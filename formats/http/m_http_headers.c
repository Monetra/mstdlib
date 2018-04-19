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

void M_http_clear_headers(M_http_t *http)
{
	if (http == NULL)
		return;

	M_hash_dict_destroy(http->headers);
	http->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);

	M_list_str_destroy(http->set_cookies);
	http->set_cookies = M_list_str_create(M_LIST_STR_STABLE);

	M_http_set_want_upgrade(http, M_FALSE, M_FALSE, NULL);
	M_http_set_persistent_conn(http, M_FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_headers_complete(const M_http_t *http)
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
	if (http == NULL || headers == NULL)
		return;

	M_http_set_headers_int(&http->headers, headers, merge);
}

void M_http_set_header(M_http_t *http, const char *key, const char *val)
{
	char   **parts;
	size_t   num_parts = 0;
	size_t   i;

	if (http == NULL || M_str_isempty(key))
		return;

	M_http_remove_header(http, key);

	parts = M_str_explode_str(',', val, &num_parts);
	if (parts == NULL || num_parts == 0)
		return;

	for (i=0; i<num_parts; i++) {
		M_hash_dict_insert(http->headers, key, parts[i]);
	}

	M_str_explode_free(parts, num_parts);
}

void M_http_add_header(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL || M_str_isempty(key))
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

M_bool M_http_want_upgrade(const M_http_t *http, M_bool *secure, const char **settings_payload)
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

M_bool M_http_persistent_conn(const M_http_t *http)
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

	http->persist_conn = persist;

	M_hash_dict_remove(http->headers, "Connection");
	if (persist)
		M_hash_dict_insert(http->headers, "Connection", "keep-alive");
}
