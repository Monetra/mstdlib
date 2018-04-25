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
#include "http/m_http_reader_int.h"

#define READ_BUF_SIZE (8*1024)

static size_t MAX_START_LEN    = 6*1024;
static size_t MAX_HEADERS_SIZE = 8*1024;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_reader_start_func_default(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	(void)type;
	(void)version;
	(void)method;
	(void)uri;
	(void)code;
	(void)reason;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_header_func_default(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_header_done_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_body_func_default(const unsigned char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_body_done_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_chunk_extensions_func_default(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_chunk_extensions_done_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_chunk_data_func_default(const unsigned char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_chunk_data_done_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_trailer_func_default(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_reader_trailer_done_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_read_version(M_parser_t *parser, M_http_version_t *version)
{
	char *temp;

	if (!M_parser_compare_str(parser, "HTTP/", 5, M_FALSE))
		return M_HTTP_ERROR_MALFORMED;
	M_parser_consume(parser, 5);

	temp     = M_parser_read_strdup(parser, M_parser_len(parser));
	*version = M_http_version_from_str(temp);
	M_free(temp);
	if (version == M_HTTP_VERSION_UNKNOWN)
		return M_HTTP_ERROR_UNKNOWN_VERSION;

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_header_validate(M_http_reader_t *httpr, const char *key, const char *val)
{
	M_int64 i64v;

	if (M_str_caseeq(key, "content-length")) {
		if (httpr->have_body_len) {
			return M_HTTP_ERROR_HEADER_DUPLICATE;
		}
		if (httpr->is_chunked) {
			return M_HTTP_ERROR_MALFORMED;
		}
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS) {
			return M_HTTP_ERROR_MALFORMED;
		}
		if (i64v < 0) {
			return M_HTTP_ERROR_MALFORMED;
		}
		httpr->have_body_len = M_TRUE;
		httpr->body_len      = (size_t)i64v;
	}

	if (M_str_caseeq(key, "transfer-encoding") && M_str_caseeq(val, "chunked")) {
		if (httpr->is_chunked) {
			return M_HTTP_ERROR_MALFORMED;
		}
		if (httpr->have_body_len) {
			return M_HTTP_ERROR_HEADER_MALFORMEDVAL;
		}
		httpr->is_chunked = M_TRUE;
	}

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_header_int(M_http_reader_t *httpr, M_parser_t *parser, M_bool is_header, M_bool *full_read)
{
	M_parser_t      *header    = NULL;
	M_parser_t     **kv        = NULL;
	char            *key       = NULL;
	char            *val       = NULL;
	size_t           num_kv    = 0;
	M_http_error_t   res       = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	do {
		header = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
		/* Not enough data so nothing to do. */
		if (header == NULL)
			break;
		/* Eat the \r\n */
		M_parser_consume(parser, 2);

		/* An empty line means the end of the header. */
		if (M_parser_len(header) == 0) {
			*full_read = M_TRUE;
			break;
		}

		httpr->header_len += M_parser_len(header);
		if (httpr->header_len > MAX_HEADERS_SIZE) {
			res = M_HTTP_ERROR_HEADER_LENGTH;
			break;
		}

		/* Folding is deprecated and shouldn't be supported. */
		if (M_parser_consume_whitespace(header, M_PARSER_WHITESPACE_NONE) != 0) {
			res = M_HTTP_ERROR_HEADER_FOLD;
			break;
		}

		kv = M_parser_split(header, ':', 2, M_PARSER_SPLIT_FLAG_NODELIM_ERROR, &num_kv);
		if (kv == NULL || num_kv != 2) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			break;
		}

		/* Spaces between the separator (:) and value are not allowed. */
		if (M_parser_consume_whitespace(kv[1], M_PARSER_WHITESPACE_NONE) != 0) {
			res = M_HTTP_ERROR_HEADER_MALFORMEDVAL;
			break;
		}

		if (M_parser_len(kv[0]) == 0 || M_parser_len(kv[1]) == 0) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			break;
		}

		key = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));
		val = M_parser_read_strdup(kv[0], M_parser_len(kv[0]));

		M_str_trim(val);
		if (M_str_isempty(key) || M_str_isempty(val)) {
			res = M_HTTP_ERROR_HEADER_INVLD;
			break;
		}

		if (is_header) {
			res = M_http_read_header_validate(httpr, key, val);
			if (res != M_HTTP_ERROR_SUCCESS) {
				break;
			}
		}

		if (is_header) {
			res = httpr->cbs.header_func(key, val, httpr->thunk);
		} else {
			res = httpr->cbs.trailer_func(key, val, httpr->thunk);
		}
		if (res != M_HTTP_ERROR_SUCCESS) {
			break;
		}

		M_free(key);
		M_free(val);
		key = NULL;
		val = NULL;
		M_parser_split_free(kv, num_kv);
		kv     = NULL;
		num_kv = 0;
		M_parser_destroy(header);
		header = NULL;
	} while (res == M_HTTP_ERROR_SUCCESS && !(*full_read));

	M_free(key);
	M_free(val);
	M_parser_split_free(kv, num_kv);
	M_parser_destroy(header);
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */
static M_http_error_t M_http_read_start_line_response(M_http_reader_t *httpr, M_parser_t **parts, size_t num_parts)
{
	char             *reason;
	M_uint64          code    = 0;
	M_http_version_t  version = M_HTTP_VERSION_UNKNOWN;
	M_http_error_t    res     = M_HTTP_ERROR_SUCCESS;

	if (num_parts != 3)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 1: HTTP version */
	res = M_http_read_version(parts[0], &version);
	if (res != M_HTTP_ERROR_SUCCESS)
		return res;

	/* Part 2: Status code */
	if (!M_parser_read_uint(parts[1], M_PARSER_INTEGER_ASCII, 0, 10, &code))
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 3: Reason phrase */
	if (M_parser_len(parts[2]) == 0)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;
	reason = M_parser_read_strdup(parts[2], M_parser_len(parts[2]));

	/* Send along the data. */
	res = httpr->cbs.start_func(M_HTTP_MESSAGE_TYPE_RESPONSE, version, M_HTTP_METHOD_UNKNOWN, NULL, (M_uint32)code, reason, httpr->thunk);
	M_free(reason);

	return res;
}

/* request-line = method SP request-target SP HTTP-version CRLF */
static M_http_error_t M_http_read_start_line_request(M_http_reader_t *httpr, M_parser_t **parts, size_t num_parts)
{
	char             *temp    = NULL;
	char             *uri     = NULL;
	M_http_method_t   method  = M_HTTP_METHOD_UNKNOWN;
	M_http_version_t  version = M_HTTP_VERSION_UNKNOWN;
	M_http_error_t    res     = M_HTTP_ERROR_SUCCESS;

	if (num_parts != 3)
		return M_HTTP_ERROR_STARTLINE_MALFORMED;

	/* Part 1: Method */
	temp   = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
	method = M_http_method_from_str(temp);
	M_free(temp);
	if (method == M_HTTP_METHOD_UNKNOWN)
		return M_HTTP_ERROR_REQUEST_METHOD;

	/* Part 2: URI */
	uri = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
	/* XXX: Validate URI .*/
#if 0
	if (!M_http_set_uri(httpr, temp))
		return M_HTTP_ERROR_REQUEST_URI;
#endif

	/* Part 3: Version */
	res = M_http_read_version(parts[0], &version);
	if (res != M_HTTP_ERROR_SUCCESS)
		goto done;

	/* Send along the data. */
	res = httpr->cbs.start_func(M_HTTP_MESSAGE_TYPE_REQUEST, version, method, uri, 0, NULL, httpr->thunk);

done:
	M_free(uri);
	return res;
}

static M_http_error_t M_http_read_start_line(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	M_parser_t      *msg       = NULL;
	M_parser_t     **parts     = NULL;
	size_t           num_parts = 0;
	M_http_error_t   res       = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	/* Check if we have a full line and pull it off. */
	msg = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
	if (msg == NULL)
		return M_HTTP_ERROR_SUCCESS;
	/* Eat the \r\n */
	M_parser_consume(parser, 2);

	if (M_parser_len(msg) > MAX_START_LEN) {
		res = M_HTTP_ERROR_STARTLINE_LENGTH;
		goto done;
	}

	parts = M_parser_split(msg, ' ', 3, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	if (parts == NULL || num_parts != 3) {
		res = M_HTTP_ERROR_STARTLINE_MALFORMED;
		goto done;
	}

	if (M_parser_compare_str(parser, "HTTP/", 5, M_FALSE)) {
		res = M_http_read_start_line_response(httpr, parts, num_parts);
	} else {
		res = M_http_read_start_line_request(httpr, parts, num_parts);
	}

	*full_read = M_TRUE;

done:
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_read_header(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	return M_http_read_header_int(httpr, parser, M_TRUE, full_read);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_read_body(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	unsigned char  buf[READ_BUF_SIZE];
	size_t         len;
	M_http_error_t res = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (httpr->have_body_len && (httpr->body_len == 0 || httpr->body_len == httpr->body_len_seen)) {
		*full_read = M_TRUE;
		return M_HTTP_ERROR_SUCCESS;
	}
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	do {
		if (httpr->have_body_len) {
			len = M_parser_read_bytes_max(parser, M_MIN(sizeof(buf), httpr->body_len-httpr->body_len_seen), buf, sizeof(buf));
		} else {
			len = M_parser_read_bytes_max(parser, sizeof(buf), buf, sizeof(buf));
		}
		httpr->body_len_seen += len;

		res = httpr->cbs.body_func(buf, len, httpr->thunk);
		if (res != M_HTTP_ERROR_SUCCESS) {
			break;
		}
	} while ((!httpr->have_body_len && len > 0) || (httpr->have_body_len && len > 0 && httpr->body_len != httpr->body_len_seen));

	if (httpr->have_body_len && httpr->body_len == httpr->body_len_seen)
		*full_read = M_TRUE;
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_read_chunk_start(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	M_parser_t      *msg        = NULL;
	char            *extensions = NULL;
	char           **parts      = NULL;
	size_t           num_parts  = 0;
	char           **kv         = NULL;
	size_t           num_kv     = 0;
	const char      *key;
	const char      *val;
	size_t           len        = 0;
	size_t           i;
	M_int64          i64v       = 0;
	unsigned char    byte;
	M_http_error_t   res        = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	/* Check if we have a full line and pull it off. */
	msg = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
	if (msg == NULL)
		return M_HTTP_ERROR_SUCCESS;
	/* Eat the \r\n */
	M_parser_consume(parser, 2);

	if (M_parser_len(msg) > MAX_START_LEN) {
		res = M_HTTP_ERROR_CHUNK_LENGTH;
		goto done;
	}

	/* Get the lengh. It's either before any extensions or before the end of the line. */
	len = M_parser_len(msg);
	M_parser_mark(msg);
	if (M_parser_consume_until(msg, (const unsigned char *)";", 1, M_FALSE) > 0)
		len = M_parser_mark_len(msg);
	M_parser_mark_rewind(msg);

	/* No length!?. */
	if (len == 0) {
		res = M_HTTP_ERROR_MALFORMED;
		goto done;
	}

	/* Read the length. */
	if (!M_parser_read_int(msg, M_PARSER_INTEGER_ASCII, len, 16, &i64v) || i64v < 0) {
		res = M_HTTP_ERROR_CHUNK_LENGTH;
		goto done;
	}
	if (i64v < 0) {
		res = M_HTTP_ERROR_MALFORMED;
		goto done;
	}
	httpr->body_len      = (size_t)i64v; 
	httpr->body_len_seen = 0;

	/* Parse off extensions if they're present. */
	if (M_parser_len(msg) > 0) {
		if (!M_parser_peek_byte(msg, &byte) || byte != ';') {
			res = M_HTTP_ERROR_CHUNK_EXTENSION;
			goto done;
		}

		extensions = M_parser_read_strdup(msg, M_parser_len(msg));
		parts      = M_str_explode_str(';', extensions, &num_parts);
		if (parts == NULL || num_parts == 0) {
			res = M_HTTP_ERROR_CHUNK_EXTENSION;
			goto done;
		}
		for (i=0; i<num_parts; i++) {
			kv = M_str_explode_str('=', parts[i], &num_kv);
			if (kv == NULL || (num_kv != 1 && num_kv != 2)) {
				res = M_HTTP_ERROR_CHUNK_EXTENSION;
				goto done;
			}

			key = kv[0];
			val = NULL;
			if (num_kv== 2) {
				val = kv[1];
			}

			res = httpr->cbs.chunk_extensions_func(key, val, httpr->thunk);
			if (res != M_HTTP_ERROR_SUCCESS) {
				goto done;
			}

			M_str_explode_free(kv, num_kv);
			kv     = NULL;
			num_kv = 0;
		}
		M_str_explode_free(parts, num_parts);
		parts     = NULL;
		num_parts = 0;
		M_free(extensions);
		extensions = NULL;

		res = httpr->cbs.chunk_extensions_done_func(httpr->thunk);
		if (res != M_HTTP_ERROR_SUCCESS)
			goto done;
	}

	*full_read = M_TRUE;

done:
	M_str_explode_free(kv, num_kv);
	M_str_explode_free(parts, num_parts);
	M_free(extensions);
	M_parser_destroy(msg);
	return res;
}

static M_http_error_t M_http_read_chunk_data(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	unsigned char  buf[READ_BUF_SIZE];
	size_t         len;
	M_http_error_t res = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (httpr->body_len == 0) {
		*full_read = M_TRUE;
		return M_HTTP_ERROR_SUCCESS;
	}

	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	/* Set len to something so the loop will run if we're waiting
 	 * for body data. */
	len = 1;
	while (len > 0 && httpr->body_len != httpr->body_len_seen) {
		len = M_parser_read_bytes_max(parser, M_MIN(sizeof(buf), httpr->body_len-httpr->body_len_seen), buf, sizeof(buf));
		httpr->body_len_seen += len;

		res = httpr->cbs.chunk_data_func(buf, len, httpr->thunk);
		if (res != M_HTTP_ERROR_SUCCESS) {
			break;
		}
	} 

	if (httpr->body_len == httpr->body_len_seen) {
		if (M_parser_len(parser) >= 2) {
			if (M_parser_compare_str(parser, "\r\n", 2, M_FALSE)) {
				M_parser_consume(parser, 2);
				*full_read = M_TRUE;
			} else {
				res = M_HTTP_ERROR_CHUNK_MALFORMED;
			}
		}
	}
	return res;
}

static M_http_error_t M_http_read_trailer(M_http_reader_t *httpr, M_parser_t *parser, M_bool *full_read)
{
	return M_http_read_header_int(httpr, parser, M_TRUE, full_read);
}

static M_http_error_t M_http_read_chunked(M_http_reader_t *httpr, M_parser_t *parser)
{
	M_http_error_t res       = M_HTTP_ERROR_SUCCESS;
	M_bool         full_read = M_FALSE;

	/* Read the start of the chunk. */
	if (httpr->rstep == M_HTTP_READER_STEP_CHUNK_START) {
		res = M_http_read_chunk_start(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		httpr->rstep = M_HTTP_READER_STEP_CHUNK_DATA;
	}

	if (httpr->rstep == M_HTTP_READER_STEP_CHUNK_DATA) {
		res = M_http_read_chunk_data(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		if (httpr->body_len == 0) {
			httpr->rstep  = M_HTTP_READER_STEP_TRAILER;
			/* Reset the header len because we'll use it for the trailer.
 			 * We're reading trailing header after all. */
			httpr->header_len = 0;
		} else {
			/* If this isn't the last chunk start reading the next one. */
			httpr->rstep = M_HTTP_READER_STEP_CHUNK_START;

			res = httpr->cbs.chunk_data_done_func(httpr->thunk);
			if (res != M_HTTP_ERROR_SUCCESS) {
				goto done;
			}

			return M_http_read_chunked(httpr, parser);
		}
	}

	if (httpr->rstep == M_HTTP_READER_STEP_TRAILER) {
		res = M_http_read_trailer(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		httpr->rstep = M_HTTP_READER_STEP_DONE;
		res = httpr->cbs.trailer_done_func(httpr->thunk);
		if (res != M_HTTP_ERROR_SUCCESS) {
			goto done;
		}
	}

done:
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_reader_read(M_http_reader_t *httpr, const unsigned char *data, size_t data_len, size_t *len_read)
{
	M_parser_t     *parser;
	M_http_error_t  res       = M_HTTP_ERROR_SUCCESS;
	size_t          start_len = 0;
	size_t          mylen_read;
	M_bool          full_read;

	if (len_read == NULL)
		len_read = &mylen_read;
	*len_read = 0;

	if (httpr == NULL || data == NULL || data_len == 0)
		return M_HTTP_ERROR_INVALIDUSE;

	if (httpr->rstep == M_HTTP_READER_STEP_DONE) {
		return M_HTTP_ERROR_SUCCESS;
	}

	parser    = M_parser_create_const(data, data_len, M_PARSER_FLAG_NONE);
	start_len = M_parser_len(parser);

	if (httpr->rstep == M_HTTP_READER_STEP_UNKNONW)
		httpr->rstep = M_HTTP_READER_STEP_START_LINE;

	/* Read the start line. */
	if (httpr->rstep == M_HTTP_READER_STEP_START_LINE) {
		res = M_http_read_start_line(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		httpr->rstep = M_HTTP_READER_STEP_HEADER;
	}

	/* Read the header. */
	if (httpr->rstep == M_HTTP_READER_STEP_HEADER) {
		res = M_http_read_header(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		if (httpr->is_chunked) {
			httpr->rstep = M_HTTP_READER_STEP_CHUNK_START;
		} else {
			httpr->rstep = M_HTTP_READER_STEP_BODY;
		}

		res = httpr->cbs.header_done_func(httpr->thunk);
		if (res != M_HTTP_ERROR_SUCCESS) {
			goto done;
		}
	}

	/* Read the body (not chunked message). */
	if (httpr->rstep == M_HTTP_READER_STEP_BODY) {
		res = M_http_read_body(httpr, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		/* We may never know if this is done if the content length wasn't set. */
		httpr->rstep = M_HTTP_READER_STEP_DONE;
		res = httpr->cbs.body_done_func(httpr->thunk);
		goto done;
	}

	/* If we're chunked then read the chunks. */
	if (httpr->rstep == M_HTTP_READER_STEP_CHUNK_START ||
		httpr->rstep == M_HTTP_READER_STEP_CHUNK_DATA  ||
		httpr->rstep == M_HTTP_READER_STEP_TRAILER)
	{
		res = M_http_read_chunked(httpr, parser);
	}

done:
	*len_read = start_len - M_parser_len(parser);
	M_parser_destroy(parser);
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_reader_t *M_http_reader_create(struct M_http_reader_callbacks *cbs, void *thunk)
{
	M_http_reader_t *httpr;

	httpr        = M_malloc_zero(sizeof(*httpr));
	httpr->thunk = thunk;

	httpr->cbs.start_func                 =  M_http_reader_start_func_default;
	httpr->cbs.header_func                =  M_http_reader_header_func_default;
	httpr->cbs.header_done_func           =  M_http_reader_header_done_func_default;
	httpr->cbs.body_func                  =  M_http_reader_body_func_default;
	httpr->cbs.body_done_func             =  M_http_reader_body_done_func_default;
	httpr->cbs.chunk_extensions_func      =  M_http_reader_chunk_extensions_func_default;
	httpr->cbs.chunk_extensions_done_func =  M_http_reader_chunk_extensions_done_func_default;
	httpr->cbs.chunk_data_func            =  M_http_reader_chunk_data_func_default;
	httpr->cbs.chunk_data_done_func       =  M_http_reader_chunk_data_done_func_default;
	httpr->cbs.trailer_func               =  M_http_reader_trailer_func_default;
	httpr->cbs.trailer_done_func          =  M_http_reader_trailer_done_func_default;
											 
	if (cbs != NULL) {
		if (cbs->start_func                  !=  NULL) httpr->cbs.start_func                 =  cbs->start_func;
		if (cbs->header_func                 !=  NULL) httpr->cbs.header_func                =  cbs->header_func;
		if (cbs->header_done_func            !=  NULL) httpr->cbs.header_done_func           =  cbs->header_done_func;
		if (cbs->body_func                   !=  NULL) httpr->cbs.body_func                  =  cbs->body_func;
		if (cbs->body_done_func              !=  NULL) httpr->cbs.body_done_func             =  cbs->body_done_func;
		if (cbs->chunk_extensions_func       !=  NULL) httpr->cbs.chunk_extensions_func      =  cbs->chunk_extensions_func;
		if (cbs->chunk_extensions_done_func  !=  NULL) httpr->cbs.chunk_extensions_done_func =  cbs->chunk_extensions_done_func;
		if (cbs->chunk_data_func             !=  NULL) httpr->cbs.chunk_data_func            =  cbs->chunk_data_func;
		if (cbs->chunk_data_done_func        !=  NULL) httpr->cbs.chunk_data_done_func       =  cbs->chunk_data_done_func;
		if (cbs->trailer_func                !=  NULL) httpr->cbs.trailer_func               =  cbs->trailer_func;
		if (cbs->trailer_done_func           !=  NULL) httpr->cbs.trailer_done_func          =  cbs->trailer_done_func;
	}

	return httpr;
}

void M_http_reader_destroy(M_http_reader_t *httpr)
{
	if (httpr == NULL)
		return;
	M_free(httpr);
}
