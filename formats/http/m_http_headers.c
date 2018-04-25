/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

static void M_http_set_headers_int(M_hash_dict_t **cur_headers, const M_hash_dict_t *new_headers, M_bool merge)
{
	if (new_headers == NULL && merge)
		return;

	if (!merge) {
		M_hash_dict_destroy(*cur_headers);
		*cur_headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	}

	if (new_headers == NULL)
		return;

	M_hash_dict_merge(cur_headers, M_hash_dict_duplicate(new_headers));
}

static M_bool M_http_set_header_int(M_hash_dict_t *d, const char *key, const char *val)
{
	char   **parts;
	size_t   num_parts = 0;
	size_t   i;

	if (d == NULL || M_str_isempty(key))
		return M_FALSE;

	if (M_str_isempty(val)) {
		M_hash_dict_remove(d, key);
		return M_TRUE;
	}

	M_hash_dict_remove(d, key);

	parts = M_str_explode_quoted(',', val, M_str_len(val), '"', '\\', 0, &num_parts, NULL);
	if (parts == NULL || num_parts == 0)
		return M_FALSE;

	for (i=0; i<num_parts; i++) {
		M_hash_dict_insert(d, key, parts[i]);
	}

	M_str_explode_free(parts, num_parts);
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
		out = NULL;
		val = M_hash_dict_multi_get_direct(d, key, i);
		if (M_str_quote_if_necessary(&out, val, '"', '\\', ',')) {
			val = out;
		}
		M_list_str_insert(l, val);
		M_free(out);
	}

	out = M_list_str_join_str(l, ", ");
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

M_bool M_http_set_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return M_FALSE;
	return M_http_set_header_int(http->headers, key, val);
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

	return M_http_set_header_int(http->trailers, key, val);
}

void M_http_add_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key) || M_str_isempty(val))
		return;

	M_hash_dict_insert(http->trailers, key, val);
}
