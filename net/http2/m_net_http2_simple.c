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

#include "../m_net_int.h"
#include "m_net_http2_simple_request.h"
#include <mstdlib/io/m_io_layer.h> /* M_io_layer_softevent_add */
#include <formats/http2/m_http2.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void trigger_softevent(M_io_t *io, M_event_type_t etype)
{
	if (M_io_get_state(io) == M_IO_STATE_CONNECTED) {
		M_io_layer_t *layer = M_io_layer_acquire(io, 0, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, etype, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_http2_simple {
	M_event_t                           *el;
	M_dns_t                             *dns;
	void                                *thunk;
	M_tls_verify_level_t                 level;
	M_tls_clientctx_t                   *ctx;
	M_io_t                              *io;
	M_parser_t                          *in_parser;
	M_buf_t                             *out_buf;
	M_http2_reader_t                    *h2r;
	char                                *schema;
	char                                *authority;
	M_hash_u64vp_t                      *requests;
	struct M_net_http2_simple_callbacks  cbs;
	M_uint32                             max_frame_size;
	char                                 errmsg[256];
	M_uint64                             next_stream_id;
};

static M_http_error_t M_nh2s_settings_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	M_net_http2_simple_t *h2 = thunk;
	if ((framehdr->flags & 0x01) == 0) {
		/* Acknowledge settings */
		M_http2_frame_settings_t *settings = M_http2_frame_settings_create(framehdr->stream.id.u32, 0x01);
		M_http2_frame_settings_finish_to_buf(settings, h2->out_buf);
	}
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_nh2s_header_func(M_http2_header_t *header, void *thunk)
{
	M_net_http2_simple_t         *h2      = thunk;
	M_net_http2_simple_request_t *request = M_hash_u64vp_get_direct(h2->requests, header->framehdr->stream.id.u32);

	if (request == NULL)
		return M_HTTP_ERROR_STREAM_ID;

	M_net_http2_simple_request_add_header(request, header);

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_nh2s_data_func(M_http2_data_t *data, void *thunk)
{
	M_net_http2_simple_t         *h2      = thunk;
	M_net_http2_simple_request_t *request = M_hash_u64vp_get_direct(h2->requests, data->framehdr->stream.id.u32);

	if (request == NULL)
		return M_HTTP_ERROR_STREAM_ID;

	M_net_http2_simple_request_add_data(request, data);

	if (data->framehdr->len.u32 < h2->max_frame_size)
		M_net_http2_simple_request_finish(request);

	return M_HTTP_ERROR_SUCCESS;
}

static void M_net_http2_simple_error_cb_default(M_http_error_t error, const char *errmsg)
{
	(void)error;
	(void)errmsg;
}

static M_bool M_net_http2_simple_iocreate_cb_default(M_io_t *io, char *error, size_t errlen, void *thunk)
{
	(void)io;
	(void)error;
	(void)errlen;
	(void)thunk;
		return M_TRUE;
	
}

static void M_net_http2_simple_disconnect_cb_default(void *thunk)
{
	(void)thunk;
}

static void M_assign_cbs(void **dst_cbs, void *const*src_cbs, size_t len)
{
	size_t i;
	for (i=0; i<len; i++) {
		if (src_cbs[i] != NULL) {
			dst_cbs[i] = src_cbs[i];
		}
	}
}

M_net_http2_simple_t *M_net_http2_simple_create(M_event_t *el, M_dns_t *dns, const struct M_net_http2_simple_callbacks *cbs, M_tls_verify_level_t level, void *thunk)
{
	static const struct M_net_http2_simple_callbacks default_cbs = {
		M_net_http2_simple_iocreate_cb_default,
		M_net_http2_simple_error_cb_default,
		M_net_http2_simple_disconnect_cb_default,
	};
	M_net_http2_simple_t *h2 = NULL;
	struct M_http2_reader_callbacks reader_cbs = { 0 };
	reader_cbs.settings_end_func = M_nh2s_settings_end_func;
	reader_cbs.data_func = M_nh2s_data_func;
	reader_cbs.header_func = M_nh2s_header_func;

	if (el == NULL || dns == NULL)
		return NULL;

	h2 = M_malloc_zero(sizeof(*h2));

	M_mem_copy(&h2->cbs, &default_cbs, sizeof(default_cbs));
	if (cbs != NULL) {
		M_assign_cbs((void**)&h2->cbs, (void*const*)cbs, sizeof(default_cbs) / sizeof(void*));
	}

	h2->el             = el;
	h2->dns            = dns;
	h2->thunk          = thunk;
	h2->level          = level;
	h2->out_buf        = M_buf_create();
	h2->in_parser      = M_parser_create(M_PARSER_FLAG_NONE);
	h2->h2r            = M_http2_reader_create(&reader_cbs, M_HTTP2_READER_NONE, h2);
	h2->next_stream_id = 1;
	h2->requests       = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void(*)(void*))M_net_http2_simple_request_destroy);
	return h2;
}

void M_net_http2_simple_destroy(M_net_http2_simple_t *h2)
{
	if (h2 == NULL)
		return;
	M_io_destroy(h2->io);
	M_buf_cancel(h2->out_buf);
	M_parser_destroy(h2->in_parser);
	M_free(h2->schema);
	M_free(h2->authority);
	M_free(h2);
}

static void M_net_http2_simple_init_tls(M_io_t *io, M_tls_verify_level_t level, const char *hostname)
{
	M_tls_clientctx_t *ctx     = M_tls_clientctx_create();
	M_list_str_t      *applist = M_list_str_create(M_LIST_STR_NONE);

	M_list_str_insert(applist, "h2");
	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_applications(ctx, applist);
	M_tls_clientctx_set_verify_level(ctx, level);

	M_io_tls_client_add(io, ctx, hostname, NULL);

	M_list_str_destroy(applist);
	M_tls_clientctx_destroy(ctx);
}

static void M_net_http2_simple_event_cb(M_event_t *el, M_event_type_t type, M_io_t *io, void *thunk)
{
	(void)el;
	(void)io;
	M_net_http2_simple_t *h2 = thunk;
	M_io_error_t          ioerr;
	size_t                len;

	M_printf("M_net_http2_simple_event_cb(%p,%d,%p,%p)\n", el, type, io, thunk);

	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
		case M_EVENT_TYPE_WRITE:
			ioerr = M_io_write_from_buf(h2->io, h2->out_buf);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			break;
		case M_EVENT_TYPE_READ:
			ioerr = M_io_read_into_parser(h2->io, h2->in_parser);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			M_http2_reader_read(h2->h2r, M_parser_peek(h2->in_parser), M_parser_len(h2->in_parser), &len);
			M_parser_consume(h2->in_parser, len);
			break;
		case M_EVENT_TYPE_ACCEPT:
			h2->cbs.error_cb(M_HTTP_ERROR_INTERNAL, "Unexexpected ACCEPT event");
			goto disconnect;
		case M_EVENT_TYPE_OTHER:
			break;
		case M_EVENT_TYPE_ERROR:
			h2->cbs.error_cb(M_HTTP_ERROR_INTERNAL, h2->errmsg);
			goto disconnect;
		case M_EVENT_TYPE_DISCONNECTED:
			goto disconnect;
	}
	return;
disconnect:
	if (h2->io != NULL) {
		M_io_destroy(h2->io);
		h2->io = NULL;
		h2->cbs.disconnect_cb(h2->thunk);
	}
	return;
}

static void M_net_http2_simple_init(M_net_http2_simple_t *h2, const char *schema, const char *authority, M_uint16 port)
{
	M_io_error_t              ioerr;
	M_http2_frame_settings_t *settings;

	ioerr = M_io_net_client_create(&h2->io, h2->dns, authority, port, M_IO_NET_ANY);
	if (ioerr != M_IO_ERROR_SUCCESS) {
		M_snprintf(h2->errmsg, sizeof(h2->errmsg), "Error creating IO: %s\n", M_io_error_string(ioerr));
		h2->cbs.error_cb(M_HTTP_ERROR_INTERNAL, h2->errmsg);
		return;
	}

	M_net_http2_simple_init_tls(h2->io, h2->level, authority);

	if (!h2->cbs.iocreate_cb(h2->io, h2->errmsg, sizeof(h2->errmsg), h2->thunk)) {
		h2->cbs.error_cb(M_HTTP_ERROR_INTERNAL, h2->errmsg);
		M_io_destroy(h2->io);
		h2->io = NULL;
		return;
	}

	M_event_add(h2->el, h2->io, M_net_http2_simple_event_cb, h2);

	h2->schema         = M_strdup(schema);
	h2->authority      = M_strdup(authority);
	h2->max_frame_size = 0x00FFFFFF;

	M_http2_pri_str_to_buf(h2->out_buf);

	settings = M_http2_frame_settings_create(0, 0);
	M_http2_frame_settings_add(settings, M_HTTP2_SETTING_HEADER_TABLE_SIZE, 0); /* Disable Dynamic Table */
	M_http2_frame_settings_add(settings, M_HTTP2_SETTING_ENABLE_PUSH, 0); /* Disable PUSH_PROMISE frames */
	M_http2_frame_settings_add(settings, M_HTTP2_SETTING_NO_RFC7540_PRIORITIES, 1); /* Disable PRIORITY frames */
	M_http2_frame_settings_finish_to_buf(settings, h2->out_buf); /* Configure settings */

	trigger_softevent(h2->io, M_EVENT_TYPE_WRITE);

}

M_bool M_net_http2_simple_goaway(M_net_http2_simple_t *h2)
{
	M_http2_stream_t stream = { M_FALSE, { 0 } };
	if (h2 == NULL || h2->io == NULL)
		return M_FALSE;
	M_http2_goaway_to_buf(&stream, 0, NULL, 0, h2->out_buf);
	trigger_softevent(h2->io, M_EVENT_TYPE_WRITE);
	return M_TRUE;
}

M_bool M_net_http2_simple_request(M_net_http2_simple_t *h2, const char *url_str, M_net_http2_simple_response_cb response_cb)
{
	M_url_t                      *url        = M_url_create(url_str);
	M_bool                        is_success = M_FALSE;
	M_net_http2_simple_request_t *request;
	M_http2_frame_headers_t      *headers;

	if (h2 == NULL)
		return M_FALSE;

	if (h2->io == NULL) {
		M_net_http2_simple_init(h2, M_url_schema(url), M_url_host(url), M_url_port_u16(url));
	} else {
		if (!M_str_eq(h2->authority, M_url_host(url)) || !M_str_eq(h2->schema, M_url_schema(url))) {
			/* A subsequent request was made to a different schema / url than connection previously established */
			is_success = M_FALSE;
			goto done;
		}
	}

	headers = M_http2_frame_headers_create((M_uint32)h2->next_stream_id, 0x05); /* END_STREAM | END_HEADERS */
	M_http2_frame_headers_add(headers, ":scheme", M_url_schema(url));
	M_http2_frame_headers_add(headers, ":method", "GET");
	M_http2_frame_headers_add(headers, ":authority", M_url_host(url));
	M_http2_frame_headers_add(headers, ":path", M_url_path(url));
	M_http2_frame_headers_finish_to_buf(headers, h2->out_buf);
	trigger_softevent(h2->io, M_EVENT_TYPE_WRITE);

	request = M_net_http2_simple_request_create(h2->next_stream_id, response_cb, h2->thunk, url_str);
	M_hash_u64vp_insert(h2->requests, (M_uint32)h2->next_stream_id, request);
	h2->next_stream_id += 2; /* Client requests are odd numbered */
	is_success = M_TRUE;
done:
	M_url_destroy(url);
	return is_success;
}
