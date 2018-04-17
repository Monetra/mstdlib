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

M_bool M_http_headers_complete(M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->headers_complete;
}

void M_http_set_headers_complete(M_http_t *http, M_bool complete)
{
	if (http == NULL)
		return;
	http->headers_complete = complete;
}

const M_hash_dict_t *M_http_headers(M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->headers;
}

const char *M_http_header(M_http_t *http, const char key)
{
	M_list_str_t *l;
	char         *out;
	size_t        len;
	size_t        i;

	if (!M_hash_dict_multi_len(http->headers, key, &len) || len == 0)
		return NULL;

	if (len == 1)
		return M_strdup(M_hash_dict_multi_get_direct(http->headers, key, 0));

	l = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<len; i++) {
		M_list_str_insert(l, M_hash_dict_multi_get_direct(http->headers, key, i));
	}

	out = M_list_str_join_str(l, ", ");
	M_list_str_destroy(l);
	return out;
}

void M_http_set_headers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge)
{
	M_hash_dict_t      *d;
	M_list_str_t       *l;
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;
	size_t              len;
	size_t              i;

	if (http == NULL)
		return;

	if (headers == NULL && merge)
		return;

	if (!merge) {
		M_hash_dict_destroy(http->headers);
		http->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
		M_hash_dict_merge(&http->headers, M_hash_dict_duplicate(headers));
		return;
	}

	/* We're going to iterate over every item in new header for each
 	 * key. We'll do the same for the current headers and push them
	 * all into a set to remove duplicates. Then we'll put them all
	 * back into the headers. */
	M_hash_dict_enumerate(headers, &he);
	while (M_hash_dict_enumerate_next(headers, he, &key, NULL)) {
		if (M_hash_dict_multi_len(headers, key, &len) && len > 0) {
			/* keep unsorted because we want all header values in
 			 * the order they were set. */
			l = M_list_str_create(M_LIST_STR_CASECMP|M_LIST_STR_SET);

			len = M_hash_dict_multi_len(http->headers, key);
			for (i=0; i<len; i++) {
				M_list_str_insert(l, M_hash_dict_multi_get_direct(http->headers, key, i));
			}

			len = M_hash_dict_multi_len(headers, key);
			for (i=0; i<len; i++) {
				M_list_str_insert(l, M_hash_dict_multi_get_direct(headers, key, i));
			}

			M_hash_dict_remove(http->headers, key);
			len = M_list_str_len(l);
			for (i=0; i<len; i++) {
				M_hash_dict_insert(http->headers, key, M_list_str_at(l, i));
			}

			M_list_str_destroy(l);
		}
	}
	M_hash_dict_enumerate_free(he);
}

void M_http_set_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return;

	M_hash_dict_remove(http->headers, key);
	M_hash_dict_insert(htpp->headers, key, val);
}

void M_http_set_header_append(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return;

	M_hash_dict_insert(htpp->headers, key, val);
}

const M_list_str_t *M_http_get_set_cookie(M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->set_cookies;
}

void M_http_set_cookie_remove(M_http_t *http, size_t idx)
{
	if (http == NULL)
		return;
	M_list_str_remove(http->set_cookies, idx);
}

void M_http_set_cookie_insert(M_http_t *http, const char *val)
{
	if (http == NULL)
		return;
	M_list_str_insert(http->set_cookies, val);
}

M_bool M_http_want_upgrade(M_http_t *http, M_bool *secure, const char **settings_payload)
{
	if (http == NULL)
		return M_FALSE;

	if (secure != NULL)
		*secure = http->want_upgrade_secure;
	if (settings_payload != NULL)
		*settings_payload = http->settings_payload;
	return http->want_upgrade;
}

void M_http_set_want_upgrade(M_http_t *http, M_bool want, M_bool secure, const char *settings_payload)
{
	if (http == NULL || (want && M_str_isempty(settings_payload)))
		return;

	http->want_upgrade        = want;
	http->want_upgrade_secure = M_FALSE;
	M_free(http->settings_payload);
	http->settings_payload    = NULL;

	M_hash_dict_remove(http->headers, "Connection");
	M_hash_dict_remove(http->headers, "Upgrade");
	M_hash_dict_remove(http->headers, "HTTP2-Settings");

	if (want) {
		M_hash_dict_insert(http->headers, "Connection", "Upgrade, HTTP2-Settings");
		M_hash_dict_insert(http->headers, "Upgrade", secure?"h2":"h2c");
		M_hash_dict_insert(http->headers, "HTTP2-Settings", settings_payload);

		http->want_upgrade_secure = secure;
		http->settings_payload    = M_strdup(settings_payload);
	}
}

M_bool M_http_persistent_conn(M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->persist_conn;
}

void M_http_set_persistent_conn(M_http_t *http, M_bool persist)
{
	/* Upgrading to http 2 isn't compatible. */
	if (http == NULL || http->want_upgrade)
		return;

	http->persist = persist;

	M_hash_dict_remove(http->headers, "Connection");
	if (persist)
		M_hash_dict_insert(http->headers, "Connection", "keep-alive");
}
