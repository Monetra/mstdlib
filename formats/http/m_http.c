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

static void M_http_create_init(M_http_t *http)
{
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_http_chunk_destory
	};

	if (http == NULL)
		return;

	/* Note: we don't create the query args dict here
 	 * because it's created when the URI is set. There is
	 * no other way to manipulate it so we don't need
	 * it before hand. */
	http->headers     = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	http->set_cookies = M_list_str_create(M_LIST_STR_STABLE);
	http->body        = M_buf_create();
	http->chunks      = M_list_create(&cbs, M_LIST_NONE);
}

static void M_http_clear_int(M_http_t *http)
{
	if (http == NULL)
		return;

	M_free(http->reason_phrase);
	M_free(http->uri);
	M_free(http->host);
	M_free(http->path);
	M_free(http->query_string);
	M_hash_dict_destroy(http->query_args);
	M_hash_dict_destroy(http->headers);
	M_list_str_destroy(http->set_cookies);
	M_buf_cancel(http->body);
	M_free(http->settings_payload);
	M_list_destroy(http->chunks, M_TRUE);

	M_mem_set(http, 0, sizeof(*http));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_t *M_http_create(void)
{
	M_http_t *http;

	http = M_malloc_zero(sizeof(*http));
	M_http_create_init(http);

	return http;
}

void M_http_destroy(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_clear_int(http);
	M_free(http);
}

M_bool M_http_require_content_length(M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->require_content_len;
}

void M_http_set_require_content_length(M_http_t *http, M_bool require)
{
	if (http == NULL)
		return;
	http->require_content_len = require;
}

void M_http_clear(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_clear_int(http);
	M_http_create_init(http);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_start_line_complete(const M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;

	switch (http->type) {
		case M_HTTP_MESSAGE_TYPE_UNKNOWN:
			return M_FALSE;
		case M_HTTP_MESSAGE_TYPE_REQUEST:
			if (http->method != M_HTTP_METHOD_UNKNOWN &&
					!M_str_isempty(http->uri) &&
					http->version != M_HTTP_VERSION_UNKNOWN)
			{
				return M_TRUE;
			}
		case M_HTTP_MESSAGE_TYPE_RESPONSE:
			if (http->version != M_HTTP_VERSION_UNKNOWN &&
					http->status_code != 0 &&
					!M_str_isempty(http->reason_phrase))
			{
				return M_TRUE;
			}
	}

	return M_FALSE;
}

M_http_message_type_t M_http_message_type(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_MESSAGE_TYPE_UNKNOWN;
	return http->type;
}

void M_http_set_message_type(M_http_t *http, M_http_message_type_t type)
{
	if (http == NULL)
		return;
	http->type = type;
}

M_http_version_t M_http_version(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_VERSION_UNKNOWN;
	return http->version;
}

void M_http_set_version(M_http_t *http, M_http_version_t version)
{
	if (http == NULL)
		return;
	http->version = version;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint32 M_http_status_code(const M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->status_code;
}

void M_http_set_status_code(M_http_t *http, M_uint32 code)
{
	if (http == NULL)
		return;
	http->status_code = code;
}

const char *M_http_reason_phrase(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->reason_phrase;
}

void M_http_set_reason_phrase(M_http_t *http, const char *phrase)
{
	if (http == NULL)
		return;
	M_free(http->reason_phrase);
	http->reason_phrase = M_strdup(phrase);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_method_t M_http_method(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_METHOD_UNKNOWN;
	return http->method;
}

void M_http_set_method(M_http_t *http, M_http_method_t method)
{
	if (http == NULL)
		return;
	http->method = method;
}

M_bool M_http_error_is_error(M_http_error_t res)
{
	if (res == M_HTTP_ERROR_SUCCESS ||
			res == M_HTTP_ERROR_SUCCESS_END)
	{
		return M_FALSE;
	}
	return M_TRUE;
}

M_http_version_t M_http_version_from_str(const char *version)
{
	if (M_str_eq_max(version, "HTTP/", 5))
		version += 5;

	if (M_str_eq(version, "1.0"))
		return M_HTTP_VERSION_1_0;

	if (M_str_eq(version, "1.0"))
		return M_HTTP_VERSION_1_1;

	if (M_str_eq(version, "2"))
		return M_HTTP_VERSION_2;

	return M_HTTP_VERSION_UNKNOWN;
}

M_http_method_t M_http_method_from_str(const char *method)
{
	if (M_str_caseeq(method, "OPTIONS"))
		return M_HTTP_METHOD_OPTIONS;

	if (M_str_caseeq(method, "GET"))
		return M_HTTP_METHOD_GET;

	if (M_str_caseeq(method, "HEAD"))
		return M_HTTP_METHOD_HEAD;

	if (M_str_caseeq(method, "POST"))
		return M_HTTP_METHOD_POST;

	if (M_str_caseeq(method, "PUT"))
		return M_HTTP_METHOD_PUT;

	if (M_str_caseeq(method, "DELETE"))
		return M_HTTP_METHOD_DELETE;

	if (M_str_caseeq(method, "TRACE"))
		return M_HTTP_METHOD_TRACE;

	if (M_str_caseeq(method, "CONNECT"))
		return M_HTTP_METHOD_CONNECT;

	return M_HTTP_METHOD_UNKNOWN;
}
