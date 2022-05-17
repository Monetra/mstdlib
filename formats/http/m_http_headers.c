/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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
#include "http/m_http_header.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_hash_dict_t *M_http_headers_dict_int(M_hash_strvp_t *headers)
{
	M_hash_dict_t       *headers_dict;
	M_hash_strvp_enum_t *he;
	const char          *key;
	M_http_header_t     *hval;
	char                *out;

	if (headers == NULL)
		return NULL;

	headers_dict = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);

	/* Multi value, keys will be called multiple times, once for each value.
	 * Insert into multi will append to multi. */
	M_hash_strvp_enumerate(headers, &he);
	while (M_hash_strvp_enumerate_next(headers, he, &key, (void **)&hval)) {
		out = M_http_header_value(hval);
		M_hash_dict_insert(headers_dict, key, out);
		M_free(out);
	}
	M_hash_strvp_enumerate_free(he);

	return headers_dict;
}

static void M_http_set_headers_int(M_hash_strvp_t **cur_headers, const M_hash_dict_t *new_headers, M_bool merge)
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
		M_hash_strvp_destroy(*cur_headers, M_TRUE);
		*cur_headers = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void(*)(void *))M_http_header_destroy);
	}

	if (new_headers == NULL || M_hash_dict_num_keys(new_headers) == 0)
		return;

	M_hash_dict_enumerate(new_headers, &he);
	while (M_hash_dict_enumerate_next(new_headers, he, &key, &oval)) {
		M_http_header_t *hval;

		hval = M_hash_strvp_get_direct(*cur_headers, key);
		if (hval == NULL) {
			hval = M_http_header_create(key, oval);
			M_hash_strvp_insert(*cur_headers, key, hval);
			continue;
		}

		M_http_header_update(hval, oval);
	}
	M_hash_dict_enumerate_free(he);
}

static M_bool M_http_set_header_int(M_hash_strvp_t **d, const char *key, const char *val, M_bool append)
{
	M_hash_dict_t *new_headers;

	if (d == NULL || M_str_isempty(key))
		return M_FALSE;

	if (!append)
		M_hash_strvp_remove(*d, key, M_TRUE);

	if (M_str_isempty(val))
		return M_TRUE;

	new_headers = M_hash_dict_create(8, 16, M_HASH_DICT_CASECMP);
	M_hash_dict_insert(new_headers, key, val);
	/* Merge and d exists so we don't have to worry about d
	 * changing within this function. */
	M_http_set_headers_int(d, new_headers, M_TRUE);
	M_hash_dict_destroy(new_headers);
	return M_TRUE;
}

static char *M_http_header_int(const M_hash_strvp_t *d, const char *key)
{
	M_http_header_t *hval;

	/* Check if key exists. */
	if (!M_hash_strvp_get(d, key, (void **)&hval))
		return NULL;

	return M_http_header_value(hval);
}

static M_list_str_t *M_http_headers_int(const M_hash_strvp_t *d)
{
	M_list_str_t        *hkeys;
	M_hash_strvp_enum_t *he;
	const char          *key;

	hkeys = M_list_str_create(M_LIST_STR_CASECMP);

	M_hash_strvp_enumerate(d, &he);
	while (M_hash_strvp_enumerate_next(d, he, &key, NULL)) {
		M_list_str_insert(hkeys, key);
	}
	M_hash_strvp_enumerate_free(he);

	return hkeys;
}

static void M_http_update_ctype(M_http_t *http)
{
	M_http_header_t  *hval;
	char             *value   = NULL;
	char             *p;
	const char       *modifiers = NULL;
	char            **parts;
	size_t            num_parts    = 0;
	size_t            i;

	if (!M_hash_strvp_get(http->headers, "Content-Type", (void **)&hval)) {
		M_free(http->content_type);
		http->content_type = NULL;
		M_free(http->charset);
		http->charset = NULL;
		http->codec = M_TEXTCODEC_UNKNOWN;
		return;
	}

	value = M_http_header_value(hval);
	if (value == NULL)
		return;

	/* If there are multiple entries for some reason, we only care about the first. */
	p = M_str_chr(value, ',');
	if (p != NULL)
		*p = '\0';

	p = M_str_chr(value, ';');
	if (p != NULL) {
		modifiers = p+1;
		*p        = '\0';
	}

	M_free(http->content_type);
	http->content_type = M_strdup_trim(value);
	if (M_str_caseeq(http->content_type, "application/x-www-form-urlencoded"))
		http->body_is_form_data = M_TRUE;

	M_free(http->charset);
	http->charset = NULL;
	http->codec   = M_TEXTCODEC_UNKNOWN;

	parts = M_str_explode_str(';', modifiers, &num_parts);
	if (parts == NULL || num_parts == 0) {
		M_free(value);
		M_str_explode_free(parts, num_parts);
		return;
	}

	for (i=0; i<num_parts; i++) {
		char **bparts;
		char  *trim;
		size_t num_bparts = 0;

		bparts = M_str_explode_str('=', parts[i], &num_bparts);
		if (bparts == NULL || num_bparts != 2) {
			M_str_explode_free(bparts, num_bparts);
			continue;
		}

		trim = M_strdup_trim(bparts[0]);
		if (!M_str_caseeq(trim, "charset")) {
			M_free(trim);
			M_str_explode_free(bparts, num_bparts);
			continue;
		}
		M_free(trim);

		http->charset = M_strdup_trim(bparts[1]);
		http->codec   = M_textcodec_codec_from_str(http->charset);
		M_str_explode_free(bparts, num_bparts);
		break;
	}

	M_str_explode_free(parts, num_parts);

	M_free(value);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_hash_dict_t *M_http_headers_dict(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return M_http_headers_dict_int(http->headers);
}

M_list_str_t *M_http_headers(const M_http_t *http)
{
	if (http == NULL)
		return NULL;

	return M_http_headers_int(http->headers);
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

	/* headers dict might not be casecmp so we can't check it
	 * directly if the content-type header is in there or not. */
	M_http_update_ctype(http);
}

M_bool M_http_set_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return M_FALSE;

	if (!M_http_set_header_int(&http->headers, key, val, M_FALSE))
		return M_FALSE;

	if (M_str_caseeq(key, "Content-Type"))
		M_http_update_ctype(http);

	return M_TRUE;
}

M_bool M_http_add_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key) || M_str_isempty(val))
		return M_FALSE;

	if (!M_http_set_header_int(&http->headers, key, val, M_TRUE))
		return M_FALSE;

	if (M_str_caseeq(key, "Content-Type"))
		M_http_update_ctype(http);

	return M_TRUE;
}

void M_http_remove_header(M_http_t *http, const char *key)
{
	if (http == NULL || M_str_isempty(key))
		return;

	M_hash_strvp_remove(http->headers, key, M_TRUE);
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_hash_dict_t *M_http_trailers_dict(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return M_http_headers_dict_int(http->trailers);
}

M_list_str_t *M_http_trailers(const M_http_t *http)
{
	if (http == NULL)
		return NULL;

	return M_http_headers_int(http->trailers);
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

	return M_http_set_header_int(&http->trailers, key, val, M_FALSE);
}

M_bool M_http_add_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key) || M_str_isempty(val))
		return M_FALSE;

	return M_http_set_header_int(&http->trailers, key, val, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_update_charset(M_http_t *http, M_textcodec_codec_t codec)
{
	M_http_header_t *hval;
	char             tempa[256];

	hval = M_hash_strvp_get_direct(http->headers, "Content-Type");
	if (hval == NULL)
		return;

	http->codec   = codec;
	M_free(http->charset);
	http->charset = M_strdup(M_textcodec_codec_to_str(codec));

	M_snprintf(tempa, sizeof(tempa), "%s; charset=%s", http->content_type, http->charset);
	M_http_header_update(hval, tempa);
}
