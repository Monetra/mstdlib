/* The MIT License (MIT)
 *
 * Copyright (c) 2022 Monetra Technologies, LLC.
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

#include <mstdlib/mstdlib.h>
#include <mstdlib/formats/m_http2.h>
#include "../http/m_http_reader_int.h"

typedef struct {
	M_http_reader_t *hr;
	struct {
		char *scheme;
		char *authority;
		char *path;
		char *method;
	} request;
} M_http2_http_args_t;

static M_http2_http_args_t *M_http2_http_args_create(M_http_reader_t *hr)
{
	M_http2_http_args_t *args;

	args     = M_malloc_zero(sizeof(*args));
	args->hr = hr;

	return args;
}

static void M_http2_http_args_destroy(M_http2_http_args_t *args)
{
	M_free(args->request.scheme);
	M_free(args->request.authority);
	M_free(args->request.path);
	M_free(args->request.method);
	M_free(args);
}

static M_http_error_t M_http2_http_reader_frame_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_frame_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_goaway_func(M_http2_goaway_t *goaway, void *thunk)
{
	(void)goaway;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_data_func(M_http2_data_t *data, void *thunk)
{
	M_http2_http_args_t *args    = thunk;
	M_http_reader_t     *hr      = args->hr;
	M_parser_t          *parser  = M_parser_create_const(data->data, data->data_len, M_PARSER_FLAG_NONE);
	M_http_error_t       res     = M_http_reader_body(hr, parser);
	M_parser_destroy(parser);
	return res;
}

static M_http_error_t M_http2_http_reader_settings_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_settings_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_setting_func(M_http2_setting_t *setting, void *thunk)
{
	(void)setting;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static void M_http2_http_reader_error_func(M_http_error_t errcode, const char *errmsg)
{
	(void)errcode;
	(void)errmsg;
}

static M_http_error_t M_http2_http_reader_headers_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_http_reader_headers_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	M_http2_http_args_t *args    = thunk;
	M_http_reader_t     *hr      = args->hr;
	M_http_error_t       h_error;
	(void)framehdr;
	h_error = hr->cbs.header_done_func(hr->data_type, hr->thunk);
	hr->rstep = M_HTTP_READER_STEP_BODY;

	if (hr->data_type == M_HTTP_DATA_FORMAT_MULTIPART)
		hr->rstep = M_HTTP_READER_STEP_MULTIPART_PREAMBLE;

	if (hr->data_type == M_HTTP_DATA_FORMAT_CHUNKED)
		hr->rstep = M_HTTP_READER_STEP_CHUNK_START;

	return h_error;
}

static M_http_error_t M_http2_http_reader_header_priority_func(M_http2_header_priority_t *priority, void *thunk)
{
	(void)priority;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}
static M_http_error_t M_http2_http_reader_header_func(M_http2_header_t *header, void *thunk)
{
	M_http2_http_args_t *args              = thunk;
	M_http_reader_t     *hr                = args->hr;
	M_bool               is_request_header = M_FALSE;
	M_http_error_t       h_error;

	hr->rstep = M_HTTP_READER_STEP_HEADER;
	if (hr->data_type == M_HTTP_DATA_FORMAT_MULTIPART) {
		hr->rstep = M_HTTP_READER_STEP_MULTIPART_HEADER;
	}
	hr->body_len_seen = 0;

	if (M_str_eq(header->key, ":status")) {
		M_uint32 code = M_str_to_uint32(header->value);
		h_error = hr->cbs.start_func(M_HTTP_MESSAGE_TYPE_RESPONSE, M_HTTP_VERSION_2, M_HTTP_METHOD_UNKNOWN, NULL, code, "OK", hr->thunk);
	} else if (M_str_eq(header->key, ":authority")) {
		args->request.authority = M_strdup(header->value);
		is_request_header = M_TRUE;
	} else if (M_str_eq(header->key, ":scheme")) {
		args->request.scheme = M_strdup(header->value);
		is_request_header = M_TRUE;
	} else if (M_str_eq(header->key, ":method")) {
		args->request.method = M_strdup(header->value);
		is_request_header = M_TRUE;
	} else if (M_str_eq(header->key, ":path")) {
		args->request.path = M_strdup(header->value);
		is_request_header = M_TRUE;
	} else {
		h_error = M_http_reader_header_entry(hr, header->key, header->value);
	}

	if (is_request_header) {
		if (
			args->request.authority != NULL &&
			args->request.scheme    != NULL &&
			args->request.method    != NULL &&
			args->request.path      != NULL
		) {
			M_http_method_t  method  = M_http_method_from_str(args->request.method);
			M_buf_t         *uri_buf = M_buf_create();
			char            *uri_str = NULL;
			M_buf_add_str(uri_buf, args->request.scheme);
			M_buf_add_str(uri_buf, "://");
			M_buf_add_str(uri_buf, args->request.authority);
			M_buf_add_str(uri_buf, args->request.path);
			uri_str = M_buf_finish_str(uri_buf, NULL);
			h_error = hr->cbs.start_func(M_HTTP_MESSAGE_TYPE_REQUEST, M_HTTP_VERSION_2, method, uri_str, 0, NULL, hr->thunk);
			M_free(uri_str);
		}
	}

	return h_error;
}

static M_http_error_t M_http2_http_reader_pri_str_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}


M_http_error_t M_http2_http_reader_read(M_http_reader_t *httpr, const unsigned char *data, size_t data_len, size_t *len_read)
{
	static const struct M_http2_reader_callbacks cbs = {
		M_http2_http_reader_frame_begin_func,
		M_http2_http_reader_frame_end_func,
		M_http2_http_reader_goaway_func,
		M_http2_http_reader_data_func,
		M_http2_http_reader_settings_begin_func,
		M_http2_http_reader_settings_end_func,
		M_http2_http_reader_setting_func,
		M_http2_http_reader_error_func,
		M_http2_http_reader_headers_begin_func,
		M_http2_http_reader_headers_end_func,
		M_http2_http_reader_header_priority_func,
		M_http2_http_reader_header_func,
		M_http2_http_reader_pri_str_func,
	};
	M_http_error_t        h_error;
	M_http2_reader_t     *h2r;
	M_http2_http_args_t  *args;

	args    = M_http2_http_args_create(httpr);
	h2r     = M_http2_reader_create(&cbs, M_HTTP2_READER_NONE, args);
	h_error = M_http2_reader_read(h2r, data, data_len, len_read);
	M_http2_reader_destroy(h2r);
	M_http2_http_args_destroy(args);
	return h_error;
}
