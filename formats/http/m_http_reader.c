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

static size_t max_start_len    = 6*1024;
static size_t max_headers_size = 8*1024;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_read_version(M_http_t *http, M_parser_t *parser)
{
	char             *temp;
	M_http_version_t  version;

	if (!M_parser_compare_str(parser, "HTTP/", 5, M_FALSE))
		return M_HTTP_ERROR_MALFORMED;
	M_parser_consume(parser, 5);

	temp    = M_parser_read_strdup(msg, M_parser_len(parser));
	version = M_http_version_from_str(temp);
	M_free(temp);
	if (version == M_HTTP_VERSION_UNKNOWN)
		return M_HTTP_ERROR_UNKNOWN_VERSION;
	M_http_set_version(http, version);

	return M_HTTP_PARSE_RESULT_SUCCESS;
}

/* status-line  = HTTP-version SP status-code SP reason-phrase CRLF */
static M_http_error_t M_http_read_start_line_response(M_http_t *http, M_parser_t **parts, size_t *num_parts)
{
	char           *temp;
	M_http_error_t  res = M_HTTP_ERROR_SUCCESS;

	if (num_parts != 3)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 1: HTTP version */
	res = M_http_read_version(http, parts[0]);
	if (M_http_error_is_error(res))
		return res;

	/* Part 2: Status code */
	if (!M_parser_read_uint(parts[1], M_PARSER_INTEGER_ASCII, 0, 10, &u64v))
		return M_HTTP_ERROR_STARTLINE_MALFORMED;
	M_http_set_status_code(http, u64v);

	/* Part 3: Reason phrase */
	if (M_parser_len(parts[2]) == 0)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	temp = M_parser_read_strdup(msg, M_parser_len(msg));
	M_http_set_reason_phrase(http, temp);
	M_free(temp);

	return M_HTTP_PARSE_RESULT_SUCCESS;
}

/* request-line = method SP request-target SP HTTP-version CRLF */
static M_http_error_t M_http_read_start_line_request(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	char            *temp;
	M_http_method_t  method;
	M_http_error_t   res = M_HTTP_ERROR_SUCCESS;

	if (num_parts != 3)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 1: Method */
	temp = M_parser_strdup(parts[0], M_parser_len(parts[0]));
	method = M_http_method_from_str(temp);
	M_free(temp);
	if (method == M_HTTP_METHOD_UNKNOWN)
		return M_HTTP_ERROR_REQUEST_METHOD;
	M_http_set_method(http, method);

	/* Part 2: URI */
	temp = M_parser_strdup(parts[0], M_parser_len(parts[0]));
	if (!M_http_set_uri(http, temp))
		return M_HTTP_ERROR_REQUEST_URI;

	/* Part 3: Version */
	res = M_http_read_version(http, parts[0]);
	if (M_http_error_is_error(res))
		return res;

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_start_line(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	M_parser_t      *msg;
	M_parser_t     **parts;
	size_t           num_parts;
	size_t           start_len;
	M_http_error_t   res = M_HTTP_ERROR_SUCCESS;

	start_len = M_parser_len(parser);

	/* Check if we have a full line and pull it off. */
	msg = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
	if (msg == NULL)
		return M_HTTP_ERROR_SUCCESS;
	/* Eat the \r\n */
	M_parser_consume(parser, 2);

	if (M_parser_len(msg) > max_start_len) {
		res = M_HTTP_ERROR_STARTLINE_LENGTH;
		goto done;
	}

	parts = M_parser_split(msg, ' ', 3, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	if (parts == NULL || num_parts != 3) {
		res = M_HTTP_ERROR_STARTLINE_MALFORMED;
		goto done;
	}

	if (M_parser_compare_str(parser, "HTTP/", 5, M_FALSE)) {
		res = M_http_read_start_line_response(http, parts, num_parts);
	} else {
		res = M_http_read_start_line_request(http, parts, num_parts);
	}

done:
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);

	if (!M_http_error_is_error(res))
		*len_read += start_len - M_parser_len(parser);
	return res;
}

static M_http_error_t M_http_read_headers(M_http_t *http, M_parser_t *parser)
{
	M_parser_t      *msg;
	M_parser_t     **parts;
	M_parser_t     **kv;
	size_t           num_parts;
	size_t           num_kv;
	size_t           start_len;
	size_t           i;
	M_http_error_t   res = M_HTTP_ERROR_SUCCESS;

	start_len = M_parser_len(parser);

	msg = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n\r\n", 4, M_FALSE);
	if (msg == NULL)
		return M_HTTP_ERROR_SUCCESS;
	/* Eat the \r\n\r\n */
	M_parser_consume(parser, 4);

	if (M_parser_len(msg) > max_headers_size) {
		res = M_HTTP_ERROR_HEADER_LENGTH;
		goto done;
	}

	parts = M_parser_split_str_pat(msg, "\r\n", 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	if (parts == NULL || num_parts == 0) {
		res = M_HTTP_ERROR_HEADER_NODATA;
		goto done;
	}
	for (i=0; i<num_parts; i++) {
		char *key;
		char *val;

		/* Folding is deprecated and shouldn't be supported. */
		if (M_parser_consume_whitespace(parts[i]) != 0) {
			res = M_HTTP_ERROR_HEADER_FOLD;
			goto done;
		}

		kv = M_parser_split(parts[i], ':', 2, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &num_kv);
		if (kv == NULL || num_kv != 2) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			goto done;
		}

		/* Spaces between the separator (:) and value are not allowd. */
		if (M_parser_consume_whitespace(kv[1]) != 0) {
			res = M_HTTP_ERROR_HEADER_MALFORMEDVAL;
			goto done;
		}

		if (M_parser_len(kv[0]) == 0 || M_parser_len(kv[1]) == 0) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			goto done;
		}

		key = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));
		val = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));

		if (M_str_caseeq(key, "set-cookie")) {
			M_http_set_cookie_insert(http, val);
		} else {
			M_http_add_header(http, key, val);
		}
		M_free(key);
		M_free(val);

		M_parser_split_free(kv, num_kv);
		kv     = NULL;
		num_kv = 0;
	}

	/* Check
 	 *
	 * - Content length:
 	 *   - only 1
	 *   - if required
	 * - Chunked
	 * - persist con
	 * - want upgrade
	 */ 

	M_http_set_headers_complete(http, M_TRUE);

done:
	M_parser_split_free(kv, num_kv);
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);

	if (!M_http_error_is_error(res))
		*len_read += start_len - M_parser_len(parser);
	return res;
}

static M_http_error_t M_http_read_chunked(M_http_t *http, M_parser_t *parser)
{
}

static M_http_error_t M_http_read_body(M_http_t *http, M_parser_t *parser)
{
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len, size_t *len_read)
{
	M_parser_t     *parser;
	M_http_error_t  res = M_HTTP_PARSE_RESULT_SUCCESS;

	if (http == NULL || data == NULL || data_len == NULL || len_read == NULL)
		return M_HTTP_PARSE_RESULT_INVALIDUSE;

	*len_read = 0;
	parser    = M_parser_create_const(data, data_len, M_PARSER_FLAG_NONE);

	if (!M_http_start_line_complete(http))
		res = M_http_read_start_line(http, parser, len_read);
	if (M_http_error_is_error(res) || !M_http_start_line_complete(http))
		goto done;

	if (!M_http_headers_complete(http))
		res = M_http_read_headers(http, parser);
	if (M_http_error_is_error(res) || !M_http_headers_complete(http))
		goto done;

	if (M_http_is_chunked(http)) {
		res = M_http_read_chunked(http, parser);
	} else {
		res = M_http_read_body(http, parser);
	}
	if (M_http_error_is_error(res))
		goto done;

	if (res == M_HTTP_ERROR_SUCCESS && 
			M_http_headers_complete(http) &&
			((M_http_is_chunked(http) && M_http_chunk_complete(http)) ||
			M_http_body_complete(http)))
	{
		res = M_HTTP_ERROR_SUCCESS_END;
	}

done:
	M_parser_destroy(parser);

	return res;
}
