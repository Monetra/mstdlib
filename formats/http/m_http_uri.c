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

/* XXX: In the future this needs to be replaced with
 * a standard URI parsing module. */
static M_bool M_http_uri_parser_host(M_parser_t *parser, char **host, M_uint16 *port)
{
	M_uint64 myport = 0;

	if (parser == NULL || host == NULL || port == NULL)
		return M_FALSE;

	*host = NULL;
	*port = 0;

	/* Check if an absolute URI that contains the host. */
	if (!M_parser_compare_str(parser, "http://", 7, M_FALSE) && !M_parser_compare_str(parser, "https://", 8, M_FALSE))
		return M_TRUE;

	/* Move past the prefix. */
	M_parser_consume_str_until(parser, "://", M_TRUE);

	/* Mark the start of the host. */
	M_parser_mark(parser);

	if (M_parser_consume_str_until(parser, ":", M_FALSE) != 0) {
		/* Having a ":" means we have a port so everything before is
		 * the host. */
		*host = M_parser_read_strdup_mark(parser);

		/* kill the ":". */
		M_parser_consume(parser, 1);

		/* Read the port. */
		if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 0, 10, &myport)) {
			goto err;
		}
		*port = (M_uint16)myport;
	} else if (M_parser_consume_str_until(parser, "/", M_FALSE) != 0) {
		/* No port was specified try to find the start of the path. */
		*host = M_parser_read_strdup_mark(parser);
	}

	/* No port and no path, all we have is the host. */
	if (*host == NULL) {
		M_parser_mark_clear(parser);
		*host = M_parser_read_strdup(parser, M_parser_len(parser));
	}

	/* We should have host... */
	if (M_str_isempty(*host))
		goto err;

	return M_TRUE;

err:
	M_free(*host);
	*host = NULL;
	*port = 0;
	return M_FALSE;
}

static M_bool M_http_uri_parser_path(M_http_t *http, M_parser_t *parser, char **path)
{
	unsigned char byte;

	if (parser == NULL || path == NULL)
		return M_FALSE;

	*path = NULL;

	if (M_parser_len(parser) == 0)
		return M_TRUE;

	if (!M_parser_peek_byte(parser, &byte) || (byte != '/' && byte != '*'))
		goto err;

	/* Only the options method is allowed to apply to the server itself.
 	 * All other methods need an actual resoure. */
	if (byte == '*' && M_http_method(http) != M_HTTP_METHOD_OPTIONS)
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

static M_bool M_http_uri_parser_query_args(M_parser_t *parser, char **query_string, M_hash_dict_t **query_args)
{
	M_parser_t    **parts     = NULL;
	size_t          num_parts = 0;
	M_parser_t    **kv        = NULL;
	size_t          num_kv    = 0;
	char           *qstr      = NULL;
	M_hash_dict_t  *qargs     = NULL;
	unsigned char   byte;
	size_t          i;

	if (parser == NULL || query_string == NULL || query_args == NULL)
		return M_FALSE;

	*query_string = NULL;
	*query_args   = NULL;

	if (M_parser_len(parser) == 0)
		return M_TRUE;

	if (!M_parser_read_byte(parser, &byte) || byte != '?')
		goto err;

	if (M_parser_len(parser) == 0)
		return M_TRUE;

	M_parser_mark(parser);
	qstr = M_parser_read_strdup(parser, M_parser_len(parser));
	M_parser_mark_rewind(parser);

	parts = M_parser_split(parser, '&', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	if (parts == NULL || num_parts == 0)
		goto err;

	for (i=0; i<num_parts; i++) {
		char *key;
		char *val;

		kv = M_parser_split(parser, '=', 0, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &num_kv);
		if (kv == NULL || num_kv != 2) {
			goto err;
		}

		key = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));
		val = M_parser_read_strdup(kv[1], M_parser_len(kv[1]));
		if (M_str_isempty(key) || M_str_isempty(val)) {
			M_free(key);
			M_free(val);
			goto err;
		}

		if (qargs == NULL) {
			qargs = M_hash_dict_create(8, 75,
				M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
		}
		M_hash_dict_insert(qargs, key, val);
		M_free(key);
		M_free(val);

		M_parser_split_free(kv, num_kv);
		kv     = NULL;
		num_kv = 0;
	}

	M_parser_split_free(kv, num_kv);
	M_parser_split_free(parts, num_parts);

	*query_string = qstr;
	*query_args   = qargs;

	return M_TRUE;

err:
	M_parser_split_free(kv, num_kv);
	M_parser_split_free(parts, num_parts);
	M_hash_dict_destroy(qargs);
	M_free(qstr);
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_http_uri(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->uri;
}

M_bool M_http_set_uri(M_http_t *http, const char *uri)
{
	M_parser_t    *parser       = NULL;
	char          *host         = NULL;
	M_uint16       port;
	char          *path         = NULL;
	char          *query_string = NULL;
	M_hash_dict_t *query_args   = NULL;

	parser = M_parser_create_const((const unsigned char *)uri, M_str_len(uri), M_PARSER_FLAG_NONE);
	if (parser == NULL)
		return M_FALSE;

	if (!M_http_uri_parser_host(parser, &host, &port) ||
		!M_http_uri_parser_path(http, parser, &path) ||
		!M_http_uri_parser_query_args(parser, &query_string, &query_args))
	{
		M_parser_destroy(parser);
		M_free(host);
		M_free(path);
		M_free(query_string);
		M_hash_dict_destroy(query_args);
		return M_FALSE;
	}

	M_free(http->uri);
	http->uri = M_strdup(uri);

	M_free(http->host);
	http->host = host;

	http->port = port;

	M_free(http->path);
	http->path = path;

	M_free(http->query_string);
	http->query_string = query_string;

	M_hash_dict_destroy(http->query_args);
	http->query_args = query_args;

	M_parser_destroy(parser);
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
	return http->query_args;
}
