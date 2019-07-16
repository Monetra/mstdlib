/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include "http/m_http_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_str_t *M_http_split_header_vals(const char *val)
{
	M_list_str_t  *split_header = NULL;
	char         **parts;
	char          *temp;
	size_t         num_parts    = 0;
	size_t         i;

	if (M_str_isempty(val))
		return NULL;

	split_header = M_list_str_create(M_LIST_STR_NONE);

	/* Some headers use a ; instead of a , as the separator. The spec says , is the
	 * separator but the special headers could have a , as part of their value so they
	 * will use ; instead. This behavior isn't part of the spec but this is how it's done. */
	if (M_str_chr(val, ';') != NULL) {
		parts = M_str_explode_str(';', val, &num_parts);
	} else {
		parts = M_str_explode_str(',', val, &num_parts);
	}

	if (parts == NULL || num_parts == 0) {
		M_list_str_destroy(split_header);
		return NULL;
	}

	/* We're going to ignore duplicate values in the header. They shouldn't have
	 * been sent in the first place but since we don't really know what to do with
	 * them we'll put that on whoever is receiving the data. */
	for (i=0; i<num_parts; i++) {
		/* After splitting we'll most likely have a space preceding the data. */
		temp = M_strdup_trim(parts[i]);
		M_list_str_insert(split_header, temp);
		M_free(temp);
	}

	M_str_explode_free(parts, num_parts);
	return split_header;
}

static void M_http_set_headers_int(M_hash_dict_t **cur_headers, const M_hash_dict_t *new_headers, M_bool merge)
{
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *oval;

	if (new_headers == NULL && merge)
		return;

	/* If we're not merging clear the current headers. We're still going
	 * to go though the merge process to copy everything into this empty
	 * hash table. We don't want to duplicate because the new headers might
	 * not be constructed properly (multi). */
	if (!merge) {
		M_hash_dict_destroy(*cur_headers);
		*cur_headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	}

	if (new_headers == NULL || M_hash_dict_num_keys(new_headers) == 0)
		return;

	/* We can't do a straight merge because we need to deal with header fields that
	 * use the = syntax to define sub key value pairs. We need to go through each
	 * sub key and remove it from current headers it's being replaced. */
	M_hash_dict_enumerate(new_headers, &he);
	while (M_hash_dict_enumerate_next(new_headers, he, &key, &oval)) {
		M_list_str_t *split_header = NULL;
		size_t        nlen         = 0;
		size_t        i;

		if ((!M_hash_dict_multi_len(new_headers, key, &nlen) || nlen == 0) && M_str_isempty(oval))
			continue;

		/* No multi length means this isn't a multi hashtable. */
		if (nlen == 0) {
			split_header = M_http_split_header_vals(oval);
			nlen         = M_list_str_len(split_header);
		}

		/* Go though the new header values. */
		for (i=0; i<nlen; i++) {
			const char  *nval;
			char       **nparts;
			size_t       num_nparts;
			size_t       clen;
			size_t       j;

			/* No split header means this is a multi-value hashtable. */
			if (split_header == NULL) {
				nval = M_hash_dict_multi_get_direct(new_headers, key, i);
			} else {
				nval = M_list_str_at(split_header, i);
			}

			/* First split the value in case it's a sub key val. */
			nparts = M_str_explode_str('=', nval, &num_nparts);
			/* Nothing shouldn't happen but we want to be sure we
			 * don't operate on anything invalid. */
			if (nparts == NULL || num_nparts == 0)
				continue;

			/* Now we need to go though the current headers and see if we need
			 * to replace or add the value from new the headers */

			/* If we don't have this header yet add this value. */
			if (!M_hash_dict_multi_len(*cur_headers, key, &clen) || clen == 0) {
				M_hash_dict_insert(*cur_headers, key, nval);
				M_str_explode_free(nparts, num_nparts);
				continue;
			}

			/* Go backwards because we can remove from the multi value
			 * list backwards without the list reshuffling. */
			for (j=clen; j-->0; ) {
				const char  *cval;
				char       **cparts;
				size_t       num_cparts;
				M_bool       found = M_FALSE;

				cval   = M_hash_dict_multi_get_direct(*cur_headers, key, j);
				cparts = M_str_explode_str('=', cval, &num_cparts);
				if (cparts == NULL || num_cparts == 0) {
					M_hash_dict_insert(*cur_headers, key, nval);
					continue;
				}

				if (num_nparts == 2 && num_cparts == 2 && M_str_caseeq(nparts[0], cparts[0])) {
					found = M_TRUE;
				} else if (num_nparts == 1 && num_cparts == 1 && M_str_caseeq(nval, cval)) {
					found = M_TRUE;
				}

				M_str_explode_free(cparts, num_cparts);

				if (found) {
					M_hash_dict_multi_remove(*cur_headers, key, j);
					break;
				}
			}

			M_hash_dict_insert(*cur_headers, key, nval);
			M_str_explode_free(nparts, num_nparts);
		}
		M_list_str_destroy(split_header);
	}
	M_hash_dict_enumerate_free(he);
}

static M_bool M_http_set_header_ctype(M_http_t *http, const char *val)
{
	char   **parts;
	size_t   num_parts = 0;
	size_t   len       = 0;
	size_t   i;

	/* Need to parse out some data from the content type header
	 * to make processing a bit easier.
	 *
	 * We can't use M_http_update_content_type and M_http_update_charset
	 * functions here because the update the headers which we're currently
	 * working on.
	 */
	if (!M_hash_dict_multi_len(http->headers, "Content-Type", &len) || len == 0)
		return M_TRUE;

	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(http->headers, "Content-Type", i);

		/* Split to see if this is a multi part like the char set. */
		parts = M_str_explode_str('=', val, &num_parts);
		if (num_parts == 1) {
			/* Must be the actual content type. */
			M_free(http->content_type);
			http->content_type = M_strdup(parts[0]);
			M_free(http->origcontent_type);
			http->origcontent_type = M_strdup(parts[0]);
		} else if (num_parts > 1 && M_str_caseeq(parts[0], "charset")) {
			/* We have the char set. */
			M_free(http->charset);
			http->charset = M_strdup(parts[1]);
			http->codec   = M_textcodec_codec_from_str(http->charset);
		}
		M_str_explode_free(parts, num_parts);
	}

	return M_TRUE;
}

static M_bool M_http_set_header_int(M_hash_dict_t *d, const char *key, const char *val, M_bool append)
{
	M_hash_dict_t *new_headers;

	if (d == NULL || M_str_isempty(key))
		return M_FALSE;

	if (!append)
		M_hash_dict_remove(d, key);

	if (M_str_isempty(val))
		return M_TRUE;

	new_headers = M_hash_dict_create(8, 16, M_HASH_DICT_CASECMP);
	M_hash_dict_insert(new_headers, key, val);
	/* Merge and d exists so we don't have to worry about d
 	 * changing within this function. */
	M_http_set_headers_int(&d, new_headers, M_TRUE);
	M_hash_dict_destroy(new_headers);
	return M_TRUE;
}

static char *M_http_header_int(const M_hash_dict_t *d, const char *key)
{
	M_list_str_t *l;
	const char   *val;
	char         *out;
	size_t        len;
	size_t        i;

	if (!M_hash_dict_multi_len(d, key, &len) || len == 0)
		return NULL;

	if (len == 1)
		return M_strdup(M_hash_dict_multi_get_direct(d, key, 0));

	l = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(d, key, i);
		M_list_str_insert(l, val);
	}

	/* Special headers that could have a comma (',') in them
	 * or are just special. */
	if (M_str_caseeq(key, "WWW-Authenticate") || M_str_caseeq(key, "Proxy-Authorization") ||
			M_str_caseeq(key, "Content-Type"))
	{
		out = M_list_str_join_str(l, "; ");
	} else {
		out = M_list_str_join_str(l, ", ");
	}

	M_list_str_destroy(l);
	return out;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const M_hash_dict_t *M_http_headers(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->headers;
}

char *M_http_header(const M_http_t *http, const char *key)
{
	if (http == NULL)
		return NULL;

	return M_http_header_int(http->headers, key);
}

void M_http_set_headers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge)
{
	if (http == NULL)
		return;

	M_http_set_headers_int(&http->headers, headers, merge);
}

M_bool M_http_set_header_append(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return M_FALSE;

	if (!M_http_set_header_int(http->headers, key, val, M_TRUE))
		return M_FALSE;

	if (M_str_caseeq(key, "Content-Type"))
		return M_http_set_header_ctype(http, val);

	return M_TRUE;
}

M_bool M_http_set_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return M_FALSE;

	if (!M_http_set_header_int(http->headers, key, val, M_FALSE))
		return M_FALSE;

	if (M_str_caseeq(key, "Content-Type"))
		return M_http_set_header_ctype(http, val);

	return M_TRUE;
}

void M_http_add_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key) || M_str_isempty(val))
		return;

	M_hash_dict_insert(http->headers, key, val);
}

void M_http_remove_header(M_http_t *http, const char *key)
{
	if (http == NULL || M_str_isempty(key))
		return;

	M_hash_dict_remove(http->headers, key);
}

const M_list_str_t *M_http_get_set_cookie(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->set_cookies;
}

void M_http_set_cookie_remove(M_http_t *http, size_t idx)
{
	if (http == NULL)
		return;
	M_list_str_remove_at(http->set_cookies, idx);
}

void M_http_set_cookie_insert(M_http_t *http, const char *val)
{
	if (http == NULL)
		return;
	M_list_str_insert(http->set_cookies, val);
}

void M_http_update_content_type(M_http_t *http, const char *val)
{
	const char *const_temp;
	size_t      len = 0;
	size_t      i;

	M_free(http->content_type);
	if (M_str_isempty(val)) {
		http->content_type = NULL;
	} else {
		http->content_type = M_strdup(val);
	}

	M_hash_dict_multi_len(http->headers, "Content-Type", &len);
	for (i=0; i<len; i++) {
		const_temp = M_hash_dict_multi_get_direct(http->headers, "Content-Type", i);

		if (M_str_chr(const_temp, '=') == NULL) {
			M_hash_dict_multi_remove(http->headers, "Content->Type", i);
			break;
		}
	}

	if (!M_str_isempty(val))
		M_hash_dict_insert(http->headers, "Content-Type", val);
}

void M_http_update_charset(M_http_t *http, M_textcodec_codec_t codec)
{
	const char *val;
	char        tempa[256];
	size_t      len = 0;
	size_t      i;

	http->codec   = codec;
	M_free(http->charset);
	http->charset = M_strdup(M_textcodec_codec_to_str(codec));

	M_hash_dict_multi_len(http->headers, "Content-Type", &len);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(http->headers, "Content-Type", i);

		if (M_str_caseeq_start(val, "charset")) {
			M_hash_dict_multi_remove(http->headers, "Content-Type", i);
			break;
		}
	}

	M_snprintf(tempa, sizeof(tempa), "charset=%s", http->charset);
	M_hash_dict_insert(http->headers, "Content-Type", tempa);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const M_hash_dict_t *M_http_trailers(const M_http_t *http)
{
	if (http == NULL)
		return NULL;

	return http->trailers;
}

char *M_http_trailer(const M_http_t *http, const char *key)
{
	if (http == NULL || M_str_isempty(key))
		return NULL;

	return M_http_header_int(http->trailers, key);
}

void M_http_set_trailers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge)
{
	if (http == NULL)
		return;

	M_http_set_headers_int(&http->trailers, headers, merge);
}

M_bool M_http_set_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return M_FALSE;

	return M_http_set_header_int(http->trailers, key, val, M_FALSE);
}

void M_http_add_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key) || M_str_isempty(val))
		return;

	M_hash_dict_insert(http->trailers, key, val);
}
