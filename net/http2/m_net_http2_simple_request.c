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

#include "m_net_http2_simple_request.h"

M_net_http2_simple_request_t *M_net_http2_simple_request_create(M_uint64 stream_id, M_net_http2_simple_response_cb response_cb, void *thunk, const char *url_str)
{
	M_net_http2_simple_request_t *request;
	request = M_malloc_zero(sizeof(*request));
	request->stream_id   = stream_id;
	request->response_cb = response_cb;
	request->thunk       = thunk;
	request->url_str     = M_strdup(url_str);
	request->data        = M_buf_create();
	request->headers     = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	return request;
}

void M_net_http2_simple_request_destroy(M_net_http2_simple_request_t *request)
{
	if (request == NULL)
		return;
	M_hash_dict_destroy(request->headers);
	M_buf_cancel(request->data);
	M_free(request->url_str);
	M_free(request);
}

void M_net_http2_simple_request_add_header(M_net_http2_simple_request_t *request, M_http2_header_t *header)
{
	if (request == NULL || header == NULL)
		return;
	M_hash_dict_insert(request->headers, header->key, header->value);
}

void M_net_http2_simple_request_add_data(M_net_http2_simple_request_t *request, M_http2_data_t *data)
{
	if (request == NULL || data == NULL)
		return;
	M_buf_add_bytes(request->data, data->data, data->data_len);
}

void M_net_http2_simple_request_finish(M_net_http2_simple_request_t *request)
{
	if (request == NULL)
		return;
	request->response_cb(request->url_str, request->headers, M_buf_peek(request->data), M_buf_len(request->data), request->thunk);
}
