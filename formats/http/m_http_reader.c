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

static size_t max_start_line = 65536;

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
static M_http_error_t M_http_read_start_line_response(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	M_parser_t  *msg;
	M_parser_t **parts;
	size_t       num_parts;
	size_t       start_len;

	start_len = M_parser_len(parser);

	/* Check if we have a full line and pull it off. */
	msg = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
	if (msg == NULL)
		return M_HTTP_ERROR_SUCCESS;

	if (M_parser_len(msg) > max_start_line) {
		res = M_HTTP_ERROR_STARTLINE_LENGTH;
		goto done;
	}

	parts = M_parser_split(msg, ' ', 3, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	if (parts == NULL || num_parts != 3) {
		res = M_HTTP_ERROR_STARTLINE_MALFORMED;
		goto done;
	}

	/* Part 1: HTTP version */
	res = M_http_read_version(http, parts[0]);
	if (M_http_error_is_error(res))
		goto done;

	/* Part 2: Status code */
	if (!M_parser_read_uint(parts[1], M_PARSER_INTEGER_ASCII, 0, 10, &u64v)) {
		res = M_HTTP_ERROR_STARTLINE_MALFORMED;
		goto done;
	}
	M_http_set_status_code(http, u64v);

	/* Part 3: Reason phrase */
	if (M_parser_len(parts[2]) == 0) {
		res = M_HTTP_ERROR_STARTLINE_MALFORMED;
		goto done;
	}
	temp = M_parser_read_strdup(msg, M_parser_len(msg));
	M_http_set_reason_phrase(http, temp);
	M_free(temp);

done:
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);

	if (!M_http_error_is_error(res))
		*len_read += start_len - M_parser_len(parser);
	return res;
}

/* request-line = method SP request-target SP HTTP-version CRLF */
static M_http_error_t M_http_read_start_line_request(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
}

static M_http_error_t M_http_read_start_line(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	*len_read += M_parser_consume_whitespace(parser);

	if (M_parser_compare_str(parser, "HTTP/", 5, M_FALSE)) {
		http->type = M_HTTP_MESSAGE_TYPE_RESPONSE;
		res = M_http_read_start_line_response(http, parser);
	} else {
		http->type = M_HTTP_MESSAGE_TYPE_REQUEST;
		res = M_http_read_start_line_request(http, parser);
	}

	return res;
}

static M_http_error_t M_http_read_headers(M_http_t *http, M_parser_t *parser)
{
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
