/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

static M_bool M_http_simple_read_parse_host(const char *full_host, char **host, M_uint16 *port)
{
	M_parser_t *parser = NULL;
	M_uint64    myport = 0;

	*host = NULL;
	*port = 0;

	if (M_str_isempty(full_host))
		return M_FALSE;

	parser = M_parser_create_const((const unsigned char *)full_host, M_str_len(full_host), M_PARSER_FLAG_NONE);

	/* Move past any prefix. */
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
		if (M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 0, 10, &myport)) {
			*port = (M_uint16)myport;
		}
	} else {
		M_parser_mark_clear(parser);
		*host = M_parser_read_strdup(parser, M_parser_len(parser));
	}

	M_parser_destroy(parser);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t M_http_simple_read_start_cb(M_http_message_type_t type, M_http_version_t version,
	M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	M_http_simple_read_t *simple = thunk;
	M_http_error_t        res    = M_HTTP_ERROR_SUCCESS;

	M_http_set_message_type(simple->http, type);
	M_http_set_version(simple->http, version);

	if (type == M_HTTP_MESSAGE_TYPE_REQUEST) {
		M_http_set_method(simple->http, method);
		if (!M_http_set_uri(simple->http, uri)) {
			res = M_HTTP_ERROR_URI;
		}
	} else {
		M_http_set_status_code(simple->http, code);
		M_http_set_reason_phrase(simple->http, reason);
	}

	return res;
}

static M_http_error_t M_http_simple_read_header_cb(const char *key, const char *val, void *thunk)
{
	M_http_simple_read_t *simple = thunk;

	/* Ignore empty headers. */
	if (M_str_isempty(val))
		return M_HTTP_ERROR_SUCCESS;

	M_http_add_header(simple->http, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_header_done_cb(M_http_data_format_t format, void *thunk)
{
	M_http_simple_read_t *simple = thunk;
	char                 *val;
	M_int64               i64v;

	switch (format) {
		case M_HTTP_DATA_FORMAT_NONE:
		case M_HTTP_DATA_FORMAT_BODY:
		case M_HTTP_DATA_FORMAT_CHUNKED:
			break;
		case M_HTTP_DATA_FORMAT_MULTIPART:
		case M_HTTP_DATA_FORMAT_UNKNOWN:
			return M_HTTP_ERROR_UNSUPPORTED_DATA;
	}

	/* Set host/port if they were not part of the URI. */
	val = M_http_header(simple->http, "host");
	if ((M_str_isempty(simple->http->host) || simple->http->port == 0) && !M_str_isempty(val)) {
		char     *host   = NULL;
		M_uint16  port   = 0;

		if (M_http_simple_read_parse_host(val, &host, &port)) {
			/* Store the host if we need to update it. */
			if (M_str_isempty(simple->http->host)) {
				M_free(simple->http->host);
				simple->http->host = host;
			} else {
				M_free(host);
			}

			/* Store the port if we need to update it. */
			if (simple->http->port == 0) {
				simple->http->port = port;
			}
		}
	}
	M_free(val);

	/* We have a callback to let us know when all the data is done. */
	if (format == M_HTTP_DATA_FORMAT_CHUNKED)
		return M_HTTP_ERROR_SUCCESS;

	val = M_http_header(simple->http, "content-length");
	if (M_str_isempty(val) && simple->rflags & M_HTTP_SIMPLE_READ_LEN_REQUIRED) {
		M_free(val);
		return M_HTTP_ERROR_LENGTH_REQUIRED;
	} else if (!M_str_isempty(val)) {
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS || i64v < 0) {
			M_free(val);
			return M_HTTP_ERROR_CONTENT_LENGTH_MALFORMED;
		}
	}
	M_free(val);

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_body_cb(const unsigned char *data, size_t len, void *thunk)
{
	M_http_simple_read_t *simple = thunk;

	M_http_body_append(simple->http, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_chunk_extensions_cb(const char *key, const char *val, size_t idx, void *thunk)
{
	M_http_simple_read_t *simple = thunk;

	(void)key;
	(void)val;
	(void)idx;

	if (simple->rflags & M_HTTP_SIMPLE_READ_FAIL_EXTENSION)
		return M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_chunk_data_cb(const unsigned char *data, size_t len, size_t idx, void *thunk)
{
	M_http_simple_read_t *simple = thunk;

	(void)idx;

	M_http_body_append(simple->http, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_trailer_cb(const char *key, const char *val, void *thunk)
{
	M_http_simple_read_t *simple = thunk;

	(void)key;
	(void)val;

	if (simple->rflags & M_HTTP_SIMPLE_READ_FAIL_TRAILERS)
		return M_HTTP_ERROR_TRAILER_NOTALLOWED;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_read_decode_body(M_http_simple_read_t *simple)
{
	char                *dec         = NULL;
	char                 tempa[32];
	/* Note: Default if encoding is not set is M_TEXTCODEC_ISO8859_1 for text.
	 *       We're ignoring this and assuming anything without a charset set
	 *       is binary data. Otherwise, we'd have to detect binary vs text data. */
	M_textcodec_error_t  terr;

	if (simple == NULL)
		return M_HTTP_ERROR_INVALIDUSE;

	if (simple->rflags & M_HTTP_SIMPLE_READ_NODECODE_BODY)
		return M_HTTP_ERROR_SUCCESS;

	/* Validate we have a content type and a text encoding. */
	if (M_str_isempty(simple->http->content_type) && simple->http->codec == M_TEXTCODEC_UNKNOWN)
		return M_HTTP_ERROR_SUCCESS;

	/* Decode form data if we have it. */
	if (simple->http->body_is_form_data) {
		simple->body_form_data = M_http_parse_form_data_string(M_buf_peek(simple->http->body), simple->http->codec);
		return M_HTTP_ERROR_SUCCESS;
	}

	/* Decode the data to utf-8 if we can. */
	if (simple->http->codec == M_TEXTCODEC_UNKNOWN || simple->http->codec == M_TEXTCODEC_UTF8)
		return M_HTTP_ERROR_SUCCESS;

	dec  = NULL;
	terr = M_textcodec_decode(&dec, M_buf_peek(simple->http->body), M_TEXTCODEC_EHANDLER_REPLACE, simple->http->codec);
	if (terr != M_TEXTCODEC_ERROR_SUCCESS && terr != M_TEXTCODEC_ERROR_SUCCESS_EHANDLER) {
		M_free(dec);
		return M_HTTP_ERROR_TEXTCODEC_FAILURE;
	}

	M_buf_truncate(simple->http->body, 0);
	M_buf_add_str(simple->http->body, dec);
	M_free(dec);

	M_http_update_charset(simple->http, M_TEXTCODEC_UTF8);

	/* We've decoded the data so we need to update the content length. */
	M_snprintf(tempa, sizeof(tempa), "%zu", M_buf_len(simple->http->body));
	M_http_set_header(simple->http, "content-length", tempa);

	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_simple_read_t *M_http_simple_read_create(M_http_simple_read_flags_t flags)
{
	M_http_simple_read_t *simple;

	simple         = M_malloc_zero(sizeof(*simple));
	simple->http   = M_http_create();
	simple->rflags = flags;

	return simple;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_simple_read_destroy(M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return;

	M_http_destroy(simple->http);
	M_hash_dict_destroy(simple->body_form_data);
	M_free(simple);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_message_type_t M_http_simple_read_message_type(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return M_HTTP_MESSAGE_TYPE_UNKNOWN;
	return M_http_message_type(simple->http);
}

M_http_version_t M_http_simple_read_version(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return M_HTTP_VERSION_UNKNOWN;
	return M_http_version(simple->http);
}

M_uint32 M_http_simple_read_status_code(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return 0;
	return M_http_status_code(simple->http);
}

const char *M_http_simple_read_reason_phrase(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_reason_phrase(simple->http);
}

M_http_method_t M_http_simple_read_method(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return M_HTTP_METHOD_UNKNOWN;
	return M_http_method(simple->http);
}

const char *M_http_simple_read_uri(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_uri(simple->http);
}

const char *M_http_simple_read_path(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_path(simple->http);
}

const char *M_http_simple_read_query_string(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_query_string(simple->http);
}

const M_hash_dict_t *M_http_simple_read_query_args(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_query_args(simple->http);
}

const char *M_http_simple_read_host(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_host(simple->http);
}

M_bool M_http_simple_read_port(const M_http_simple_read_t *simple, M_uint16 *port)
{
	M_uint16 myport;

	if (port == NULL)
		port = &myport;
	*port = 0;

	if (simple == NULL)
		return M_FALSE;
	return M_http_port(simple->http, port);
}

M_hash_dict_t *M_http_simple_read_headers_dict(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_headers_dict(simple->http);
}

M_list_str_t *M_http_simple_read_headers(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_headers(simple->http);
}

char *M_http_simple_read_header(const M_http_simple_read_t *simple, const char *key)
{
	if (simple == NULL)
		return NULL;
	return M_http_header(simple->http, key);
}

const M_list_str_t *M_http_simple_read_get_set_cookie(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_get_set_cookie(simple->http);
}

M_bool M_http_simple_read_is_body_form_data(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return M_FALSE;
	return simple->http->body_is_form_data;
}

const unsigned char *M_http_simple_read_body(const M_http_simple_read_t *simple, size_t *len)
{
	size_t mylen;

	if (len == NULL)
		len = &mylen;
	*len = 0;

	if (simple == NULL)
		return NULL;

	*len = M_buf_len(simple->http->body);
	return (unsigned char *)M_buf_peek(simple->http->body);
}

const M_hash_dict_t *M_http_simple_read_body_form_data(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return simple->body_form_data;
}

const char *M_http_simple_read_content_type(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return simple->http->content_type;
}

M_textcodec_codec_t M_http_simple_read_codec(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return M_TEXTCODEC_UNKNOWN;
	return simple->http->codec;
}

const char *M_http_simple_read_charset(const M_http_simple_read_t *simple)
{
	if (simple == NULL)
		return NULL;
	return simple->http->charset;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_simple_read(M_http_simple_read_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags,
	size_t *len_read)
{
	M_http_reader_t                *reader;
	M_http_simple_read_t           *sr          = NULL;
	M_http_error_t                  res;
	M_http_error_t                  dres;
	size_t                          mylen_read;
	M_bool                          have_simple = M_TRUE;
	struct M_http_reader_callbacks  cbs = {
		M_http_simple_read_start_cb,
		NULL, /* header_full_cb */
		M_http_simple_read_header_cb,
		M_http_simple_read_header_done_cb,
		M_http_simple_read_body_cb,
		NULL, /* body_done_cb */
		M_http_simple_read_chunk_extensions_cb,
		NULL, /* chunk_extensions_done_cb */
		M_http_simple_read_chunk_data_cb,
		NULL, /* chunk_data_done_cb */
		NULL, /* chunk_data_finished_cb */
		NULL, /* multipart_preamble_cb */
		NULL, /* multipart_preamble_done_cb */
		NULL, /* multipart_header_full_cb */
		NULL, /* multipart_header_cb */
		NULL, /* multipart_header_done_cb */
		NULL, /* multipart_data_cb */
		NULL, /* multipart_data_done_cb */
		NULL, /* multipart_data_finished_cb. */
		NULL, /* multipart_epilouge_cb */
		NULL, /* multipart_epilouge_done_cb */
		NULL, /* M_http_simple_read_trailer_full_cb */
		M_http_simple_read_trailer_cb,
		NULL /* trailer_done_cb */
	};

	if (len_read == NULL)
		len_read = &mylen_read;
	*len_read = 0;

	if (simple == NULL) {
		have_simple = M_FALSE;
		simple      = &sr;
	}

	if (data == NULL || data_len == 0) {
		res = M_HTTP_ERROR_MOREDATA;
		goto done;
	}

	*simple = M_http_simple_read_create(flags);

	reader = M_http_reader_create(&cbs, M_HTTP_READER_NONE, *simple);
	res    = M_http_reader_read(reader, data, data_len, len_read);
	M_http_reader_destroy(reader);

	if (res != M_HTTP_ERROR_SUCCESS && res != M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) {
		goto done;
	}

	dres = M_http_simple_read_decode_body(*simple);
	if (dres != M_HTTP_ERROR_SUCCESS) {
		res     = dres;
		M_http_simple_read_destroy(*simple);
		*simple = NULL;
		goto done;
	}

done:
	if ((res != M_HTTP_ERROR_SUCCESS && res != M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) || !have_simple) {
		M_http_simple_read_destroy(*simple);
		if (have_simple) {
			*simple = NULL;
		}
	}
	return res;
}

M_http_error_t M_http_simple_read_parser(M_http_simple_read_t **simple, M_parser_t *parser, M_uint32 flags)
{
	M_http_error_t res;
	size_t         len_read = 0;

	res = M_http_simple_read(simple, M_parser_peek(parser), M_parser_len(parser), flags, &len_read);

	if (res != M_HTTP_ERROR_MOREDATA)
		M_parser_consume(parser, len_read);

	return res;
}
