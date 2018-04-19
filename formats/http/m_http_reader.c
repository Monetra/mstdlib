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

	temp    = M_parser_read_strdup(parser, M_parser_len(parser));
	version = M_http_version_from_str(temp);
	M_free(temp);
	if (version == M_HTTP_VERSION_UNKNOWN)
		return M_HTTP_ERROR_UNKNOWN_VERSION;
	M_http_set_version(http, version);

	return M_HTTP_ERROR_SUCCESS;
}

/* status-line  = HTTP-version SP status-code SP reason-phrase CRLF */
static M_http_error_t M_http_read_start_line_response(M_http_t *http, M_parser_t **parts, size_t num_parts)
{
	char           *temp;
	M_uint64        u64v;
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
	M_http_set_status_code(http, (M_uint32)u64v);

	/* Part 3: Reason phrase */
	if (M_parser_len(parts[2]) == 0)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	temp = M_parser_read_strdup(parts[2], M_parser_len(parts[2]));
	M_http_set_reason_phrase(http, temp);
	M_free(temp);

	return M_HTTP_ERROR_SUCCESS;
}

/* request-line = method SP request-target SP HTTP-version CRLF */
static M_http_error_t M_http_read_start_line_request(M_http_t *http, M_parser_t **parts, size_t num_parts)
{
	char            *temp;
	M_http_method_t  method;
	M_http_error_t   res = M_HTTP_ERROR_SUCCESS;

	if (num_parts != 3)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 1: Method */
	temp = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
	method = M_http_method_from_str(temp);
	M_free(temp);
	if (method == M_HTTP_METHOD_UNKNOWN)
		return M_HTTP_ERROR_REQUEST_METHOD;
	M_http_set_method(http, method);

	/* Part 2: URI */
	temp = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
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
	M_parser_t      *msg       = NULL;
	M_parser_t     **parts     = NULL;
	size_t           num_parts = 0;
	size_t           start_len;
	M_http_error_t   res       = M_HTTP_ERROR_SUCCESS;

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
		*(len_read) += start_len - M_parser_len(parser);
	return res;
}

static M_http_error_t M_http_read_headers_validate_upgrade(M_http_t *http, const M_hash_dict_t *headers)
{
	const char *type    = NULL;
	const char *payload = NULL;
	const char *val     = NULL;
	M_bool      upgrade = M_FALSE;
	M_bool      secure  = M_FALSE;
	size_t      i;
	size_t      len;

	len = 0;
	M_hash_dict_multi_len(headers, "connection", &len);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(headers, "connection", i);
		if (M_str_caseeq(val, "upgrade")) {
			upgrade = M_TRUE;
		}
	}

	len = 0;
	M_hash_dict_multi_len(headers, "upgrade", &len);
	if (len > 1)
		return M_HTTP_ERROR_HEADER_DUPLICATE;
	type = M_hash_dict_get_direct(headers, "upgrade");
	if (M_str_caseeq(type, "h2"))
		secure = M_TRUE;

	len = 0;
	M_hash_dict_multi_len(headers, "HTTP2-Settings", &len);
	if (len > 1)
		return M_HTTP_ERROR_HEADER_DUPLICATE;
	payload = M_hash_dict_get_direct(headers, "HTTP2-Settings");

	if (upgrade) {
		if (M_str_isempty(type) || M_str_isempty(payload)) {
			return M_HTTP_ERROR_UPGRADE;
		}
		if (M_str_caseeq(type, "h2")) {
			secure = M_TRUE;
		} else if (!M_str_caseeq(type, "h2c")) {
			return M_HTTP_ERROR_UPGRADE;
		}
		M_http_set_want_upgrade(http, upgrade, secure, payload);
	}

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_headers_validate(M_http_t *http)
{
	const M_hash_dict_t *headers;
	const char          *val;
	M_int64              i64v;
	size_t               len;
	size_t               i;
	M_http_error_t       res;

	headers = M_http_headers(http);

	/* Content-Length. */
	len = 0;
	M_hash_dict_multi_len(headers, "content-length", &len);
	if (len > 1) {
		return M_HTTP_ERROR_HEADER_DUPLICATE;
	} else if (len == 0 && M_http_require_content_length(http)) {
		return M_HTTP_ERROR_LENGTH_REQUIRED;
	} else if (len == 1 && M_hash_dict_get(headers, "transfer-encoding", NULL)) {
		return M_HTTP_ERROR_MALFORMED;
	} else if (len == 1) {
		val = M_hash_dict_get_direct(headers, "content-length");
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS) {
			return M_HTTP_ERROR_MALFORMED;
		}
		if (i64v < 0) {
			return M_HTTP_ERROR_MALFORMED;
		}
		M_http_set_body_length(http, (size_t)i64v);
	}

	/* Transfer-Encoding. */
	len = 0;
	M_hash_dict_multi_len(headers, "transfer-encoding", &len);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(headers, "transfer-encoding", i);
		if (M_str_caseeq(val, "chunked")) {
			M_http_set_chunked(http, M_TRUE);
		}
	}

	/* Persistant connection. */
	len = 0;
	M_hash_dict_multi_len(headers, "connection", &len);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(headers, "connection", i);
		if (M_str_caseeq(val, "keep-alive")) {
			M_http_set_persistent_conn(http, M_TRUE);
		}
	}

	/* Upgrade requested. */
	res = M_http_read_headers_validate_upgrade(http, headers);
	if (M_http_error_is_error(res))
		return res;

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_headers(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	M_parser_t      *msg;
	M_parser_t     **parts     = NULL;
	M_parser_t     **kv        = NULL;
	size_t           num_parts = 0;
	size_t           num_kv    = 0;
	size_t           start_len;
	size_t           i;
	size_t           j;
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
		char   **sparts;
		size_t   num_sparts;
		char    *key;
		char    *val;

		/* Folding is deprecated and shouldn't be supported. */
		if (M_parser_consume_whitespace(parts[i], M_PARSER_WHITESPACE_NONE) != 0) {
			res = M_HTTP_ERROR_HEADER_FOLD;
			goto done;
		}

		kv = M_parser_split(parts[i], ':', 2, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &num_kv);
		if (kv == NULL || num_kv != 2) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			goto done;
		}

		/* Spaces between the separator (:) and value are not allowd. */
		if (M_parser_consume_whitespace(kv[1], M_PARSER_WHITESPACE_NONE) != 0) {
			res = M_HTTP_ERROR_HEADER_MALFORMEDVAL;
			goto done;
		}

		if (M_parser_len(kv[0]) == 0 || M_parser_len(kv[1]) == 0) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			goto done;
		}

		key = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));
		val = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));

		M_str_trim(val);
		if (M_str_isempty(key) || M_str_isempty(val)) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			goto done;
		}

		if (M_str_caseeq(key, "set-cookie")) {
			M_http_set_cookie_insert(http, val);
		} else {
			/* If multi value expode them into their parts
 			 * for storage. */
			sparts = M_str_explode_str(',', val, &num_sparts);
			if (sparts == NULL)
				num_sparts = 0;
			for (j=0; j<num_sparts; j++) {
				M_str_trim(sparts[j]);
				if (M_str_isempty(sparts[j])) {
					continue;
				}
				M_http_add_header(http, key, sparts[j]);
			}
			M_str_explode_free(sparts, num_sparts);
		}
		M_free(key);
		M_free(val);

		M_parser_split_free(kv, num_kv);
		kv     = NULL;
		num_kv = 0;
	}

	res = M_http_read_headers_validate(http);
	if (M_http_error_is_error(res))
		goto done;

	M_http_set_headers_complete(http, M_TRUE);

done:
	M_parser_split_free(kv, num_kv);
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);

	if (M_http_error_is_error(res)) {
		/* Kill the headers we set when there is an error
 		 * since we can't assume they're valid. */
		M_http_clear_headers(http);
		M_http_clear_chunked(http);
		M_http_set_body_length(http, 0);
		M_http_set_want_upgrade(http, M_FALSE, M_FALSE, NULL);
		M_http_set_persistent_conn(http, M_FALSE);
	} else {
		*(len_read) += start_len - M_parser_len(parser);
	}
	return res;
}

static M_http_error_t M_http_read_chunked(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	size_t len = 0;

	M_parser_mark(parser);
	if (M_parser_consume_until(parser, ';', 1, M_FALSE) > 0) {
		len = M_parser_mark_len(parser);
	} else if (M_parser_consume_str_until(parser, "\r\n", M_FALSE) > 0) {
		len = M_parser_mark_len(parser);
	}
	M_parser_mark_rewind(parser);

	/* No length specified yet. */
	if (len == 0)
		return M_HTTP_ERROR_SUCCESS;

	/* XXX: success_end when last (0) length chunk read. */
}

static M_http_error_t M_http_read_body(M_http_t *http, M_parser_t *parser, size_t *len_read)
{
	unsigned char buf[8*1024];
	M_bool        have_total;
	size_t        total = 0;
	size_t        cur   = 0;
	size_t        len   = 0;

	/* If total is unknown then everything is body
 	 * and it ends with the connection is closed. */
	have_total = M_http_have_body_length(http);
	if (have_total) {
		total = M_http_body_length(http);
		cur   = M_http_body_length_seen(http);
		if (total == 0 || cur == total) {
			return M_HTTP_ERROR_SUCCESS_END;
		}
	}

	do {
		if (have_total) {
			len = M_parser_read_bytes_max(parser, total-cur, buf, sizeof(buf));
		} else {
			len = M_parser_read_bytes_max(parser, sizeof(buf), buf, sizeof(buf));
		}
		/* Updates cur internally. */
		M_http_body_append(http, buf, len);
		cur         += len;
		*(len_read) += len;
	} while ((!have_total && len > 0) || (have_total && len > 0 && cur != total));

	if (have_total && cur == total)
		return M_HTTP_ERROR_SUCCESS_END;
	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len, size_t *len_read)
{
	M_parser_t     *parser;
	M_http_error_t  res = M_HTTP_ERROR_SUCCESS;
	size_t          last_chunk;

	if (http == NULL || data == NULL || data_len == 0 || len_read == NULL)
		return M_HTTP_ERROR_INVALIDUSE;

	*len_read = 0;
	parser    = M_parser_create_const(data, data_len, M_PARSER_FLAG_NONE);

	if (!M_http_start_line_complete(http))
		res = M_http_read_start_line(http, parser, len_read);
	if (M_http_error_is_error(res) || !M_http_start_line_complete(http))
		goto done;

	if (!M_http_headers_complete(http))
		res = M_http_read_headers(http, parser, len_read);
	if (M_http_error_is_error(res) || !M_http_headers_complete(http))
		goto done;

	if (M_http_is_chunked(http)) {
		res = M_http_read_chunked(http, parser, len_read);
	} else {
		res = M_http_read_body(http, parser, len_read);
	}
	if (M_http_error_is_error(res))
		goto done;

	last_chunk = M_http_chunk_count(http)-1;
	if (res == M_HTTP_ERROR_SUCCESS && 
			M_http_headers_complete(http) &&
			((M_http_is_chunked(http) && M_http_chunk_complete(http, last_chunk) && M_http_chunk_data_len(http, last_chunk) == 0) ||
			M_http_body_complete(http)))
	{
		res = M_HTTP_ERROR_SUCCESS_END;
	}

done:
	M_parser_destroy(parser);

	return res;
}
