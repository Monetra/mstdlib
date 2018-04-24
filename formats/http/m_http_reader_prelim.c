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
#include "http/m_http_simple_int.h"

#define READ_BUF_SIZE (8*1024)

static size_t MAX_START_LEN    = 6*1024;
static size_t MAX_HEADERS_SIZE = 8*1024;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if 0
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

static M_http_error_t M_http_read_header_data(M_http_t *http, M_parser_t *parser, M_bool is_header, M_bool *full_read)
{
	M_parser_t      *header    = NULL;
	M_parser_t     **kv        = NULL;
	char            *key       = NULL;
	char            *val       = NULL;
	size_t           num_kv    = 0;
	M_http_error_t   res       = M_HTTP_ERROR_SUCCESS;

	do {
		header = M_parser_read_parser_until(parser, (const unsigned char *)"\r\n", 2, M_FALSE);
		/* Not enough data so nothing to do. */
		if (header == NULL)
			break;
		/* Eat the \r\n */
		M_parser_consume(parser, 2);

		/* An empty line means the end of the headers. */
		if (M_parser_len(header) == 0) {
			*full_read = M_TRUE;
			break;
		}

		http->header_len += M_parser_len(header);
		if (http->header_len > MAX_HEADERS_SIZE) {
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

		if (M_str_caseeq(key, "set-cookie")) {
			if (is_header) {
				M_http_set_cookie_insert(http, val);
			} else {
				res = M_HTTP_ERROR_HEADER_NOTALLOWED;
				break;
			}
		} else if (is_header) {
			M_http_add_header(http, key, val);
		} else {
			M_http_add_trailer(http, key, val);
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

static M_http_error_t M_http_read_headers_validate(M_http_t *http, M_http_read_flags_t flags)
{
	const M_hash_dict_t *headers;
	const char          *val;
	M_int64              i64v;
	size_t               len;
	size_t               i;

	headers = M_http_headers(http);

	/* Content-Length. */
	len = 0;
	M_hash_dict_multi_len(headers, "content-length", &len);
	if (len > 1) {
		return M_HTTP_ERROR_HEADER_DUPLICATE;
	} else if (len == 0 && flags & M_HTTP_READ_LEN_REQUIRED) {
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
		http->have_body_len = M_TRUE;
		http->body_len      = (size_t)i64v;
	}

	/* Transfer-Encoding. */
	len = 0;
	M_hash_dict_multi_len(headers, "transfer-encoding", &len);
	for (i=0; i<len; i++) {
		val = M_hash_dict_multi_get_direct(headers, "transfer-encoding", i);
		if (M_str_caseeq(val, "chunked")) {
			http->is_chunked = M_TRUE;
			break;
		}
	}

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
	if (res != M_HTTP_ERROR_SUCCESS)
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
	temp   = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
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
	return res;
}

static M_http_error_t M_http_read_start_line(M_http_t *http, M_parser_t *parser, M_bool *full_read)
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
		res = M_http_read_start_line_response(http, parts, num_parts);
	} else {
		res = M_http_read_start_line_request(http, parts, num_parts);
	}

	*full_read = M_TRUE;

done:
	M_parser_split_free(parts, num_parts);
	M_parser_destroy(msg);
	return res;
}

static M_http_error_t M_http_read_headers(M_http_t *http, M_parser_t *parser, M_http_read_flags_t flags, M_bool *full_read)
{
	M_http_error_t res = M_HTTP_ERROR_SUCCESS;

	*full_read = M_FALSE;
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	res = M_http_read_header_data(http, parser, M_TRUE, full_read);
	if (res != M_HTTP_ERROR_SUCCESS || !full_read)
		return res;

	/* Clear the full read flag because we need to validate the headers.
 	 * We we have all the data and we'll set it back if validation passes. */
	*full_read = M_FALSE;
	res        = M_http_read_headers_validate(http, flags);
	if (res != M_HTTP_ERROR_SUCCESS)
		return res;

	*full_read = M_TRUE;
	return res;
}

static M_http_error_t M_http_read_body(M_http_t *http, M_parser_t *parser, M_bool *full_read)
{
	unsigned char buf[READ_BUF_SIZE];
	size_t        len;

	*full_read = M_FALSE;
	if (http->have_body_len && (http->body_len == 0 || http->body_len == http->body_len_seen)) {
		*full_read = M_TRUE;
		return M_HTTP_ERROR_SUCCESS;
	}
	if (M_parser_len(parser) == 0)
		return M_HTTP_ERROR_SUCCESS;

	do {
		if (http->have_body_len) {
			len = M_parser_read_bytes_max(parser, M_MIN(sizeof(buf), http->body_len-http->body_len_seen), buf, sizeof(buf));
		} else {
			len = M_parser_read_bytes_max(parser, sizeof(buf), buf, sizeof(buf));
		}
		/* Updates cur internally. */
		M_http_body_append(http, buf, len);
		http->body_len_seen += len;
	} while ((!http->have_body_len && len > 0) || (http->have_body_len && len > 0 && http->body_len != http->body_len_seen));

	if (http->have_body_len && http->body_len == http->body_len_seen)
		*full_read = M_TRUE;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_read_chunk_start(M_http_t *http, M_parser_t *parser, M_bool *full_read)
{
	M_http_chunk_t *chunk      = NULL;
	M_parser_t     *msg        = NULL;
	char           *extensions = NULL;
	size_t          cnum       = 0;
	size_t          len        = 0;
	M_int64         i64v       = 0;
	unsigned char   byte;
	M_http_error_t  res        = M_HTTP_ERROR_SUCCESS;

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

	/* Parse off extensions if they're present. */
	if (M_parser_len(msg) > 0) {
		if (!M_parser_peek_byte(msg, &byte) || byte != ';') {
			res = M_HTTP_ERROR_CHUNK_EXTENSION;
			goto done;
		}

		extensions = M_parser_read_strdup(msg, M_parser_len(msg));
	}

	/* We have the length and the extension. Lets add the chunk. */
	cnum            = M_http_chunk_insert(http);
	chunk           = M_http_chunk_get(http, cnum);
	chunk->body_len = (size_t)i64v;
	if (!M_str_isempty(extensions)) {
		if (!M_http_set_chunk_extensions_string(http, cnum, extensions)) {
			/* Kill the chunk on error. */
			M_http_chunk_remove(http, cnum);
			res = M_HTTP_ERROR_CHUNK_EXTENSION;
			goto done;
		}
	}
	*full_read = M_TRUE;

done:
	M_free(extensions);
	M_parser_destroy(msg);
	return res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_read_chunked(M_http_t *http, M_parser_t *parser)
{
	M_http_chunk_t *chunk;
	M_http_error_t  res       = M_HTTP_ERROR_SUCCESS;
	M_bool          full_read = M_FALSE;
#if 0

	/* Read the start of the chunk. */
	if (http->read_step == M_HTTP_READ_STEP_CHUNK_START) {
		*step = M_HTTP_READ_STEP_CHUNK_START;
		res   = M_http_read_chunk_start(http, parser, full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		http->read_step = M_HTTP_READ_STEP_CHUNK_DATA;
		if (flags & M_HTTP_READ_STOP_ON_DONE && full_read) {
			*step = M_HTTP_READ_STEP_CHUNK_START_DONE;
			goto done;
		}
	}

	/* Check that we have a chunk to put data into. A chunk would have
 	 * been inserted by the chunk start step. If the caller remoed the
	 * chunk then we can't keep processing because we don't have all the
	 * info we need. */
	chunk_cnt = M_http_chunk_count(http);
	if (chunk_cnt == 0)
		return M_HTTP_ERROR_INVALIDUSE;

	/* Get the last chunk because we'll need to do some work on it. */
	chunk = M_http_chunk_get(http, chunk_cnt-1);
	if (chunk->body_len == 0) {
	}

	if (http->read_step == M_HTTP_READ_STEP_CHUNK_DATA) {
		if (chunk->body_len != 0 && chunk->body_len == chunk->body_len_seen)
			break;

		*step     = M_HTTP_READ_STEP_CHUNK_DATA;
		res       = M_http_read_chunk_data(http, parser, full_read);
		*len_read = start_len - M_parser_len(parser);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			return res;

		chunk = M_http_chunk_get(http, M_http_chunk_count(http)-1);


		http->read_step = M_HTTP_READ_STEP_CHUNK_START;

		if (flags & M_HTTP_READ_STOP_ON_DONE && full_read) {
			*step = M_HTTP_READ_STEP_CHUNK_DATA_DONE;
			return res;
		}
		/* Try to read the next chunk. */
		goto chunk_start;
	}

	if (http->read_step == M_HTTP_READ_STEP_TRAILER) {
	}

done:
#endif
	return res;
}

M_http_error_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len, M_http_read_flags_t flags, M_http_read_step_t *step, size_t *len_read)
{
	M_parser_t         *parser;
	M_http_read_step_t  mystep;
	M_http_error_t      res       = M_HTTP_ERROR_SUCCESS;
	size_t              start_len = 0;
	size_t              mylen_read;
	M_bool              full_read;

	if (step == NULL)
		step = &mystep;
	*step = M_HTTP_READ_STEP_UNKNONW;

	if (len_read == NULL)
		len_read = &mylen_read;
	*len_read = 0;

	if (http == NULL || data == NULL || data_len == 0)
		return M_HTTP_ERROR_INVALIDUSE;

	if (http->read_step == M_HTTP_READ_STEP_DONE) {
		*step = M_HTTP_READ_STEP_DONE;
		return M_HTTP_ERROR_SUCCESS;
	}

	parser    = M_parser_create_const(data, data_len, M_PARSER_FLAG_NONE);
	start_len = M_parser_len(parser);

	if (http->read_step == M_HTTP_READ_STEP_UNKNONW)
		http->read_step = M_HTTP_READ_STEP_START_LINE;

	/* Read the start line. */
	if (http->read_step == M_HTTP_READ_STEP_START_LINE) {
		*step = M_HTTP_READ_STEP_START_LINE;
		res   = M_http_read_start_line(http, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		http->read_step = M_HTTP_READ_STEP_HEADER;
		if (flags & M_HTTP_READ_STOP_ON_DONE && full_read) {
			*step = M_HTTP_READ_STEP_START_LINE_DONE;
			goto done;
		}
	}

	/* Read the headers. */
	if (http->read_step == M_HTTP_READ_STEP_HEADER) {
		*step = M_HTTP_READ_STEP_HEADER;
		res   = M_http_read_headers(http, parser, flags, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		if (http->is_chunked) {
			http->read_step = M_HTTP_READ_STEP_CHUNK_START;
		} else {
			http->read_step = M_HTTP_READ_STEP_BODY;
		}

		if (flags & M_HTTP_READ_STOP_ON_DONE && full_read) {
			*step = M_HTTP_READ_STEP_HEADER_DONE;
			goto done;
		}
	}

	/* Read the body (not chunked message). */
	if (http->read_step == M_HTTP_READ_STEP_BODY) {
		*step = M_HTTP_READ_STEP_BODY;
		res   = M_http_read_body(http, parser, &full_read);

		if (res != M_HTTP_ERROR_SUCCESS || !full_read)
			goto done;

		/* We may never know if this is done if the content length wasn't set. */
		http->read_step = M_HTTP_READ_STEP_DONE;
		*step           = M_HTTP_READ_STEP_DONE;
		goto done;
	}

	/* If we're chunked then read the chunks. */
	if (http->read_step == M_HTTP_READ_STEP_CHUNK_START ||
		http->read_step == M_HTTP_READ_STEP_CHUNK_DATA  ||
		http->read_step == M_HTTP_READ_STEP_TRAILER)
	{
		res = M_http_read_chunked(http, parser);
	}

done:
	*len_read = start_len - M_parser_len(parser);
	M_parser_destroy(parser);
	return res;
}
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_http_simple_start_cb(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	(void)type;
	(void)version;
	(void)method;
	(void)uri;
	(void)code;
	(void)reason;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_header_cb(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_header_done_cb(void *thunk)
{
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_body_cb(const unsigned char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_body_done_cb(void *thunk)
{
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_chunk_extensions_cb(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_chunk_extensions_done_cb(void *thunk)
{
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_chunk_data_cb(const unsigned char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_chunk_data_done_cb(void *thunk)
{
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_trailer_cb(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_TRUE;
}

static M_bool M_http_simple_trailer_done_cb(void *thunk)
{
	(void)thunk;
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_simple_read(M_http_t *http, const unsigned char *data, size_t data_len, M_http_read_flags_t flags, size_t *len_read)
{
	M_http_reader_t                *reader;
	struct M_http_reader_callbacks  cbs[] = {
		M_http_simple_start_cb,
		M_http_simple_header_cb,
		M_http_simple_header_done_cb,
		M_http_simple_body_cb,
		M_http_simple_body_done_cb,
		M_http_simple_chunk_extensions_cb,
		M_http_simple_chunk_extensions_done_cb,
		M_http_simple_chunk_data_cb,
		M_http_simple_chunk_data_done_cb,
		M_http_simple_trailer_cb,
		M_http_simple_trailer_done_cb,
	};

	http->rflags = flags;

	reader = M_http_reader_create(&cbs);
	res    = M_http_reader_read(reader, data, len, len_read);
	M_http_reader_destroy(reader);

	return res;
}
