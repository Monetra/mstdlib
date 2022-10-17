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

#ifndef __NET_HTTP2_SIMPLE_REQUEST_H__
#define __NET_HTTP2_SIMPLE_REQUEST_H__

#include "../m_net_int.h"

typedef struct {
	M_uint64                        stream_id;
	M_net_http2_simple_response_cb  response_cb;
	M_buf_t                        *data;
	M_hash_dict_t                  *headers;
	void                           *thunk;
	char                           *url_str;
} M_net_http2_simple_request_t;

M_net_http2_simple_request_t *
     M_net_http2_simple_request_create(M_uint64 stream_id, M_net_http2_simple_response_cb response_cb, void *thunk, const char *url_str);
void M_net_http2_simple_request_destroy(M_net_http2_simple_request_t *request);
void M_net_http2_simple_request_add_header(M_net_http2_simple_request_t *request, M_http2_header_t *header);
void M_net_http2_simple_request_add_data(M_net_http2_simple_request_t *request, M_http2_data_t *data);
void M_net_http2_simple_request_finish(M_net_http2_simple_request_t *request);

#endif
