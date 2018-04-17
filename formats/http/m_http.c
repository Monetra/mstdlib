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
	if (http == NULL)
		return;

	/* Note: we don't create the query args dict here
 	 * because it's created when the URI is set. There is
	 * no other way to manipulate it so we don't need
	 * it before hand. */
	http->headers     = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	http->trailer     = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	http->set_cookies = M_list_str_create(M_LIST_STR_STABLE);
	http->body        = M_buf_create();
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
	M_hash_dict_destroy(http->trailer);
	M_list_str_destroy(http->set_cookies);
	M_buf_cancel(http->body);
	M_free(settings_payload);

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

void M_http_clear_headers(M_http_t *http)
{
	if (http == NULL)
		return;

	M_hash_dict_destroy(http->headers);
	http->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);

	M_http_set_want_upgrade(http, M_FALSE, M_FALSE, NULL);
	M_http_set_persistent_conn(http, M_FALSE);
}

void M_http_clear_set_cookie(M_http_t *http)
{
	if (http == NULL)
		return;

	M_list_str_destroy(http->set_cookies);
	http->set_cookies = M_list_str_create(M_LIST_STR_STABLE);
}

void M_http_clear_body(M_http_t *http)
{
	if (http == NULL)
		return;

	M_buf_cancel(http->body);
	http->body = M_buf_create();
}

void M_http_clear_chunked(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_clear_chunk_body(http);
	M_http_clear_chunk_trailer(http);
}

void M_http_clear_chunk_body(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_clear_body(http);
}

void M_http_clear_chunk_trailer(M_http_t *http)
{
	if (http == NULL)
		return;

	M_hash_dict_destroy(http->trailer);
	http->trailer = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_start_line_complete(const M_http_t *http)
{
	if (http->type == )
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
	return http->type
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

const char *M_http_uri(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->uri;
}

/* XXX: In the future this needs to be replaced with
 * a standard URI parsing module. */
static M_bool M_http_uri_parser_host(M_parser_t *parser, char **host, M_uint16 *port)
{
	M_uint64 myport = 0;

	if (parser == NULL || host == NULL || port == NULL)
		return M_FALSE;

	*host = NULL;
	*port = 0;

	/* Check if an absoulte URI that contains the host. */
	if (!M_parser_compare_str(parser, "http://", 7, M_TRUE) && !M_parser_compare_str(parser, "https://", 8, M_TRUE))
		return M_TRUE;

	/* Move past the prefix. */
	M_parser_consume_str_until(parser, "://", M_TRUE);

	/* Mark the start of the host. */
	M_parser_mark(parser);

	/* Having a ":" means we have a port so everyting before is
 	 * the host. */
	if (M_parser_consume_str_until(parser, ":", M_FALSE) != 0) {
		*host = M_parser_read_strdup_mark(parser);

		/* kill the ":". */
		M_parser_consume(parser, 1);

		/* Read the port. */
		if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 0, 10, &myport)) {
			goto err;
		}
		*port = (M_uint16)myport;
	}

	/* No port was specified try to find the start of the path. */
	if (*host == NULL) {
		if (M_parser_consume_str_until(parser, "/", M_FALSE) != 0) {
			*host = M_parser_read_strdup_mark(parser);
		}
	}

	/* No port and no path, all we have is the host. */
	if (*host == NULL) {
		M_parser_mark_clear(parser);
		*host = M_parser_read_strdup(parser, M_parser_len(parser));
	}

	/* We should have host... */
	if (*host == NULL)
		goto err;

	return M_TRUE;

err:
	M_free(*host);
	*host = NULL;
	*port = 0;
	return M_FALSE;
}

static M_bool M_http_uri_parser_path(M_parser_t *parser, char **path)
{
	if (parser == NULL || path == NULL)
		return M_FALSE;

	*path = NULL;

	if (M_parser_len(parser) == 0)
		return M_TRUE;

	if (!M_parser_read_byte(parser, &byte) || byte != '/')
		goto err;

	*path = M_parser_read_strdup_until(parser, "?", M_FALSE);
	if (*path == NULL)
		*path = M_parser_read_strdup(parser, M_parser_len(parser));

	if (*path == NULL)
		goto err;

	return M_TRUE;

err:
	M_free(*path);
	*path = NULL;
	return M_FALSE;
}

static M_bool M_http_uri_parser_query_args(M_parser_t *parser, char **query_string, M_hash_dict_t *query_args)
{
	M_parser_t **parser_fields;
	size_t       parser_fields_len;
	M_parser_t **parser_kv;
	size_t       parser_kv_len;
	size_t       i;

	if (parser == NULL || query_string == NULL || query_args == NULL)
		return; M_FALSE;

	*query_string = NULL;
	*query_args   = NULL;

	if (M_parser_len(parser) == 0)
		return M_TRUE;

	if (!M_parser_read_byte(parser, &byte) || byte != '?')
		goto err;

	/* Skip the separator. */
	M_parser_consume(parser, 1);
	if (M_parser_len(parser) == 0)
		return M_TRUE;

	M_parser_mark(parser);

	parser_fields = M_parser_split(parser, '&', 0, M_PARSER_SPLIT_FLAG_NONE, &parser_fields_len);
	if (parser_fields == NULL || parser_fields_len == 0)
		goto err;

	*query_args = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);

	for (i=0; i<parser_fields_len; i++) {
		char *key;
		char *val;

		parser_kv = M_parser_split(parser_fields[i], '=', 0, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &parser_kv_len);
		if (parser_kv == NULL || parser_kv_len != 2) {
			goto err;
		}

		key = M_parser_read_strdup(parser_kv[0], M_parser_len(parser_kv[0]));
		val = M_parser_read_strdup(parser_kv[1], M_parser_len(parser_kv[1]));
		M_hash_dict_insert(*query_args, key, val);
		M_free(key);
		M_free(val);

		M_parser_split_free(parser_kv, parser_kv_len);
		parser_kv     = NULL;
		parser_kv_len = 0;
	}
	M_parser_split_free(parser_fields, parser_fields_len);

	*query_string = M_parser_read_strdup_mark(parser);

	return M_TRUE;

err:
	M_parser_split_free(parser_fields, parser_fields_len);
	M_parser_split_free(parser_kv, parser_kv_len);
	M_free(*query_string);
	*query_string = NULL;
	M_hash_dict_destroy(*query_args);
	*query_args = NULL;
	return M_FALSE;
}

M_bool M_http_set_uri(M_http_t *http, const char *uri)
{
	M_parser_t    *parser       = NULL;
	char          *host         = NULL;
	char          *path         = NULL;
	char          *query_string = NULL;
	M_hash_dict_t *query_args   = NULL;
	M_uint64       port         = 0;

	if (http == NULL)
		return;

	parser = M_parser_create_const(uri, M_str_len(uri), M_PARSER_FLAG_NONE);
	if (parser == NULL)
		return M_FALSE;

	if (!M_http_uri_parser_host(parser, &host, &port) ||
			!M_http_uri_parser_path(parser, &path) ||
			!M_http_uri_parser_query_args(parser, &query_string, &query_args))
	{
		return M_FALSE;
	}

	M_free(http->uri);
	http->uri = M_strdup(uri);

	M_free(http->host);
	http->host = host;

	http->port = port;

	M_free(http->path);
	http->path = path;

	M_hash_dict_destroy(http->query_args);
	http->query_args = query_args;

	return M_TRUE;
}

const char *M_http_host(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->host;
}

M_bool M_http_port(const M_http_t *http, M_uint16 *port)
{
	if (http == NULL)
		return M_FALSE;

	if (http->port == 0)
		return M_FALSE;

	if (port != NULL)
		*port = http->port;

	return M_TRUE;
}

const char *M_http_path(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->path;
}

const char *M_http_query_string(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->query_string;
}

const M_hash_dict_t *M_http_query_args(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	reutrn http->query_args;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
}
