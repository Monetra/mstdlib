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

static M_http_error_t M_http_simple_start_cb(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	M_http_simple_t *simple = thunk;
	M_http_error_t   res    = M_HTTP_ERROR_SUCCESS;

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

static M_http_error_t M_http_simple_header_cb(const char *key, const char *val, void *thunk)
{
	M_http_simple_t *simple = thunk;

	M_http_set_header(simple->http, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_header_done_cb(void *thunk)
{
	M_http_simple_t     *simple = thunk;
	const M_hash_dict_t *headers;
	const char          *val;
	M_int64              i64v;

	headers = M_http_headers(simple->http);
	val     = M_hash_dict_get_direct(headers, "content-length");

	if (M_str_isempty(val) && simple->rflags & M_HTTP_SIMPLE_READ_LEN_REQUIRED) {
		return M_HTTP_ERROR_LENGTH_REQUIRED;
	} else if (!M_str_isempty(val)) {
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS || i64v < 0) {
			return M_HTTP_ERROR_MALFORMED;
		}

		/* No body so we're all done. */
		if (i64v == 0) {
			simple->rdone = M_TRUE;
		}

		simple->http->body_len = (size_t)i64v;
	}

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_body_cb(const unsigned char *data, size_t len, void *thunk)
{
	M_http_simple_t     *simple = thunk;
	const M_hash_dict_t *headers;

	M_http_body_append(simple->http, data, len);

	/* If we don't have a content length and we have a body we can only assume all
 	 * the data has been sent in. */
	headers = M_http_headers(simple->http);
	if (!M_hash_dict_multi_len(headers, "content-length", &len))
		simple->rdone = M_TRUE;

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_body_done_cb(void *thunk)
{
	M_http_simple_t *simple = thunk;

	if (simple->http->body_len != simple->http->body_len_seen)
		return M_HTTP_ERROR_MOREDATA;

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_chunk_extensions_cb(const char *key, const char *val, void *thunk)
{
	M_http_simple_t *simple = thunk;

	(void)key;
	(void)val;

	if (simple->rflags & M_HTTP_SIMPLE_READ_FAIL_EXTENSION)
		return M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_chunk_extensions_done_cb(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_chunk_data_cb(const unsigned char *data, size_t len, void *thunk)
{
	M_http_simple_t *simple = thunk;

	M_http_body_append(simple->http, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_chunk_data_done_cb(void *thunk)
{
	M_http_simple_t *simple = thunk;

	simple->rdone = M_TRUE;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_trailer_cb(const char *key, const char *val, void *thunk)
{
	M_http_simple_t *simple = thunk;

	(void)key;
	(void)val;

	if (simple->rflags & M_HTTP_SIMPLE_READ_FAIL_TRAILERS)
		return M_HTTP_ERROR_TRAILER_NOTALLOWED;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_trailer_done_cb(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http_simple_decode_body(M_http_simple_t *simple)
{
#if 0
	const M_hash_dict_t *headers;
	const char          *const_temp;
	char                *dec;
	M_textcodec_codec_t  codec;
	size_t               len;
	size_t               i;

	headers = M_http_headers(simple->http);
	len     = M_hash_dict_multi_len(headers, "content-type");
	for (i=0; i<len; i++) {
		const_temp = M_hash_dict_multi_get_direct(headers, "content-type");
		1. check charset
		2. check for form encoding
	}

	if (codec == unknown)
		return M_HTTP_ERROR_SUCCESS;

	1. Split multi part now or later?

	1. decode charset.
	2. decode form encoding
	tcerr = M_textcodec_decode(&dec, M_http_body(simple->http), M_TEXTCODEC_EHANDLER_FAIL, codec);
	if (M_textcodec_error_is_error(tcerr))
		return M_HTTP_ERROR_TEXTCODEC_FAILURE;

	1. Kill current body data.
	2. Set newly decoded body data.
	3. set charset to utf-8
	4. change form encoding to ?, just remove?
	M_http_body_drop(simple->http, M_http_body_length_buffered(simple->http));
	M_http_body_append(simple->http, dec);
	M_free(dec);

#endif
	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Adds headers and body. */
static M_bool M_http_simple_write_int(M_buf_t *buf, const M_hash_dict_t *headers, const char *data, size_t data_len)
{
	M_http_t           *http = NULL;
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;
	char                tempa[32];
	M_int64             i64v;

	/* We want to push the headers into the http to ensure they're in a
 	 * properly configured hashtable. We need to ensure flags like casecomp
	 * are enabled. */
	http = M_http_create();
	if (headers != NULL) {
		M_http_set_headers(http, headers, M_FALSE);
		headers = M_http_headers(http);
	}

	/* Validate some headers. */
	if (data != NULL && data_len != 0) {
		/* Can't have transfer-encoding AND data. */
		if (M_hash_dict_get(headers, "transfer-encoding", NULL)) {
			goto err;
		}

		if (M_hash_dict_get(headers, "content-length", &val)) {
			/* If content-length is already set we need to ensure it matches data
 			 * since this is considered a complete message. */
			if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS || i64v < 0) {
				goto err;
			}

			if ((size_t)i64v != data_len) {
				goto err;
			}
		} else {
			M_snprintf(tempa, sizeof(tempa), "%zu", data_len);
			M_http_set_header(http, "content-length", tempa);
		}
	}

	/* We're not going to convert duplicates into a list.
 	 * We'll write them as individual ones. */
	M_hash_dict_enumerate(headers, &he);
	while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
		M_buf_add_str(buf, key);
		M_buf_add_byte(buf, ':');
		M_buf_add_str(buf, val);
		M_buf_add_str(buf, "\r\n");
	}
	M_hash_dict_enumerate_free(he);

	/* End of start/headers. */
	M_buf_add_str(buf, "\r\n");

	/* Add the body data. */
	M_buf_add_bytes(buf, data, data_len);

	M_http_destroy(http);
	return M_TRUE;

err:
	M_http_destroy(http);
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_simple_t *M_http_simple_create(M_http_simple_read_flags_t flags)
{
	M_http_simple_t *simple;

	simple         = M_malloc_zero(sizeof(*simple));
	simple->http   = M_http_create();
	simple->rflags = flags;

	return simple;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_simple_destroy(M_http_simple_t *simple)
{
	if (simple == NULL)
		return;

	M_http_destroy(simple->http);
	M_free(simple);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_message_type_t M_http_simple_message_type(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return M_HTTP_MESSAGE_TYPE_UNKNOWN;
	return M_http_message_type(simple->http);
}

M_http_version_t M_http_simple_version(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return M_HTTP_VERSION_UNKNOWN;
	return M_http_version(simple->http);
}

M_uint32 M_http_simple_status_code(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return 0;
	return M_http_status_code(simple->http);
}

const char *M_http_simple_reason_phrase(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_reason_phrase(simple->http);
}

M_http_method_t M_http_simple_method(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return M_HTTP_METHOD_UNKNOWN;
	return M_http_method(simple->http);
}

const char *M_http_simple_uri(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_uri(simple->http);
}

M_bool M_http_simple_port(const M_http_simple_t *simple, M_uint16 *port)
{
	M_uint16 myport;

	if (port == NULL)
		port = &myport;
	*port = 0;

	if (simple == NULL)
		return M_FALSE;
	return M_http_port(simple->http, port);
}

const char *M_http_simple_path(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_path(simple->http);
}

const char *M_http_simple_query_string(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_query_string(simple->http);
}

const M_hash_dict_t *M_http_simple_query_args(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_query_args(simple->http);
}

const M_hash_dict_t *M_http_simple_headers(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_headers(simple->http);
}

char *M_http_simple_header(const M_http_simple_t *simple, const char *key)
{
	if (simple == NULL)
		return NULL;
	return M_http_header(simple->http, key);
}

const M_list_str_t *M_http_simple_get_set_cookie(const M_http_simple_t *simple)
{
	if (simple == NULL)
		return NULL;
	return M_http_get_set_cookie(simple->http);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_simple_read(M_http_simple_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags, size_t *len_read)
{
	M_http_reader_t                *reader;
	M_http_error_t                  res;
	size_t                          mylen_read;
	struct M_http_reader_callbacks  cbs = {
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

	if (len_read == NULL)
		len_read = &mylen_read;
	*len_read = 0;

	if (simple == NULL)
		return M_HTTP_ERROR_INVALIDUSE;

	*simple = M_http_simple_create(flags);

	if (data == NULL || data_len == 0)
		return M_HTTP_ERROR_SUCCESS;

	reader = M_http_reader_create(&cbs, *simple);
	res    = M_http_reader_read(reader, data, data_len, len_read);
	M_http_reader_destroy(reader);

	if (!(*simple)->rdone)
		res = M_HTTP_ERROR_MOREDATA;

	if (res != M_HTTP_ERROR_SUCCESS) {
		M_http_simple_destroy(*simple);
		*simple = NULL;
	}

	res = M_http_simple_decode_body(*simple);
	if (res != M_HTTP_ERROR_SUCCESS) {
		M_http_simple_destroy(*simple);
		*simple = NULL;
	}

	return res;
}

unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version, const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len)
{
	M_buf_t *buf;

	if (method == M_HTTP_METHOD_UNKNOWN || M_str_isempty(uri) || version == M_HTTP_VERSION_UNKNOWN || (headers == NULL && (data == NULL || len == 0)))
		return NULL;

	buf = M_buf_create();

	/* request-line = method SP request-target SP HTTP-version CRLF */
	M_buf_add_str(buf, M_http_method_to_str(method));
	M_buf_add_byte(buf, ' ');

	/* XXX: Convert spaces to ? */
	M_buf_add_str(buf, uri);
	M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, M_http_version_to_str(version));
	M_buf_add_str(buf, "\r\n");

	if (!M_http_simple_write_int(buf, headers, data, data_len)) {
		M_buf_cancel(buf);
		return NULL;
	}
	return M_buf_finish(buf, len);
}

unsigned char *M_http_simple_write_respone(M_http_version_t version, M_uint32 code, const char *reason, const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len)
{
	M_buf_t *buf;

	if (version == M_HTTP_VERSION_UNKNOWN || M_str_isempty(reason))
		return NULL;

	buf = M_buf_create();

	/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */
	M_buf_add_str(buf, M_http_version_to_str(version));
	M_buf_add_byte(buf, ' ');

	M_buf_add_int(buf, code);
	M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, reason);
	M_buf_add_str(buf, "\r\n");

	if (!M_http_simple_write_int(buf, headers, data, data_len)) {
		M_buf_cancel(buf);
		return NULL;
	}
	return M_buf_finish(buf, len);
}
