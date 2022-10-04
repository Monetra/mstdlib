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

#include "m_net_int.h"
#include <mstdlib/io/m_io_layer.h> /* M_io_layer_softevent_add (STARTTLS) */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_http_simple {
	M_event_t            *el;
	M_dns_t              *dns;
	M_tls_clientctx_t    *ctx;
	M_uint64              redirect_max;
	M_uint64              redirect_cnt;
	M_uint64              receive_max;
	M_uint64              timeout_connect_ms;
	M_uint64              timeout_stall_ms;
	M_uint64              timeout_overall_ms;
	M_event_timer_t      *timer_stall;
	M_event_timer_t      *timer_overall;
	M_io_t               *io;
	M_parser_t           *read_parser;
	M_buf_t              *out_buf;
	M_buf_t              *header_buf;
	M_http_simple_read_t *simple;
	M_http_method_t       method;
	char                 *user_agent;
	char                 *content_type;
	char                 *charset;
	M_hash_dict_t        *headers;
	unsigned char        *message;
	size_t                message_len;
	size_t                message_pos;
	M_http_version_t      version;
	M_http_error_t        httperr;
	M_net_error_t         neterr;
	char                  error[256];
	void                  *thunk;
	struct {
		M_net_http_simple_done_cb      done_cb;
		M_net_http_simple_iocreate_cb  iocreate_cb;
	} cbs;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const char *DEFAULT_USER_AGENT = "MSTDLIB/Net HTTP Simple " NET_HTTP_VERSION;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool iocreate_cb_default(M_io_t *io, char *error, size_t errlen, void *thunk)
{
	(void)io;
	(void)error;
	(void)errlen;
	(void)thunk;
	return M_TRUE;
}

static void M_net_http_simple_destroy(M_net_http_simple_t *hs)
{
	M_io_destroy(hs->io);
	hs->io = NULL;

	M_tls_clientctx_destroy(hs->ctx);

	M_event_timer_remove(hs->timer_stall);
	M_event_timer_remove(hs->timer_overall);

	M_buf_cancel(hs->out_buf);
	M_parser_destroy(hs->read_parser);
	M_buf_cancel(hs->header_buf);
	M_http_simple_read_destroy(hs->simple);
	M_hash_dict_destroy(hs->headers);
	M_free(hs->message);
	M_free(hs->charset);
	M_free(hs->content_type);
	M_free(hs->user_agent);

	M_free(hs);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void trigger_softevent(M_io_t *io, M_event_type_t etype)
{
	M_io_layer_t *layer = M_io_layer_acquire(io, 0, NULL);
	M_io_layer_softevent_add(layer, M_FALSE, etype, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);
}

static void call_done(M_net_http_simple_t *hs)
{
	/* Got the final data. Stop our timers. */
	M_event_timer_stop(hs->timer_stall);
	M_event_timer_stop(hs->timer_overall);

	hs->cbs.done_cb(hs->neterr, hs->httperr, hs->simple, hs->error, hs->thunk);

	/* DO NOT USE hs after this point. Nothing can set
	 * hs as a thunk argument for a cb to receive!!! */
	M_net_http_simple_destroy(hs);
}

static void timeout_stall_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_http_simple_t *hs = thunk;

	(void)el;
	(void)etype;
	(void)io;

	hs->neterr = M_NET_ERROR_TIMEOUT_STALL;
	M_snprintf(hs->error, sizeof(hs->error), "Timeout: Stall");
	call_done(hs);
}

static void timeout_overall_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_http_simple_t *hs = thunk;

	(void)el;
	(void)etype;
	(void)io;

	hs->neterr = M_NET_ERROR_TIMEOUT;
	M_snprintf(hs->error, sizeof(hs->error), "Timeout");
	call_done(hs);
}

static void timer_start_connect(M_net_http_simple_t *hs)
{
	if (hs->timeout_connect_ms == 0)
		return;
	M_io_net_set_connect_timeout_ms(hs->io, hs->timeout_connect_ms);
}

static void timer_start_stall(M_net_http_simple_t *hs)
{
	if (hs->timeout_stall_ms == 0)
		return;

	if (hs->timer_stall == NULL)
		hs->timer_stall = M_event_timer_add(hs->el, timeout_stall_cb, hs);

	M_event_timer_reset(hs->timer_stall, hs->timeout_stall_ms);
}

static void timer_start_overall(M_net_http_simple_t *hs)
{
	if (hs->timeout_overall_ms == 0)
		return;

	if (hs->timer_overall == NULL)
		hs->timer_overall = M_event_timer_add(hs->el, timeout_overall_cb, hs);

	/* Overall is the entire process. The run time shouldn't be restarted. */
	if (!M_event_timer_get_status(hs->timer_overall))
		M_event_timer_reset(hs->timer_overall, hs->timeout_overall_ms);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_net_http_simple_ready_send(M_net_http_simple_t *hs)
{
	hs->thunk = NULL;

	M_buf_cancel(hs->header_buf);
	hs->header_buf = M_buf_create();

	M_parser_destroy(hs->read_parser);
	hs->read_parser = M_parser_create(M_PARSER_FLAG_NONE);

	M_buf_cancel(hs->out_buf);
	hs->out_buf = M_buf_create();

	hs->message_pos = 0;

	M_http_simple_read_destroy(hs->simple);
	hs->simple = NULL;

	M_io_destroy(hs->io);
	hs->io = NULL;
}

static M_bool setup_io(M_net_http_simple_t *hs, const M_url_t *url_st)
{
	M_io_error_t  ioerr;
	size_t        lid;

	ioerr = M_io_net_client_create(&hs->io, hs->dns, M_url_host(url_st), M_url_port_u16(url_st), M_IO_NET_ANY);
	if (ioerr != M_IO_ERROR_SUCCESS) {
		hs->neterr = M_NET_ERROR_CREATE;
		M_snprintf(hs->error, sizeof(hs->error), "Failed to create network client: %s", M_io_error_string(ioerr));
		return M_FALSE;
	}

	/* If this is an https connection add the context. */
	if (M_str_caseeq_start(M_url_schema(url_st), "https")) {
		if (hs->ctx == NULL) {
			hs->neterr = M_NET_ERROR_TLS_REQUIRED;
			M_snprintf(hs->error, sizeof(hs->error), "HTTPS Connection required but client context no set");
			goto fail;
		}
			
		ioerr = M_io_tls_client_add(hs->io, hs->ctx, NULL, &lid);
		if (ioerr != M_IO_ERROR_SUCCESS) {
			hs->neterr = M_NET_ERROR_TLS_SETUP_FAILURE;
			M_snprintf(hs->error, sizeof(hs->error), "Failed to add client context: %s", M_io_error_string(ioerr));
			goto fail;
		}
	}

	/* Allow the user of this object to add any additional layers. Logging, bw shapping... */
	if (!hs->cbs.iocreate_cb(hs->io, hs->error, sizeof(hs->error), hs->thunk)) {
		hs->neterr = M_NET_ERROR_CREATE;
		if (M_str_isempty(hs->error)) {
			M_snprintf(hs->error, sizeof(hs->error), "iocreate generic failure");
		}
		goto fail;
	}

	return M_TRUE;
fail:
	M_io_destroy(hs->io);
	hs->io = NULL;
	return M_FALSE;
}

static void handle_redirect(M_net_http_simple_t *hs)
{
	char *location;

	/* Get the redirect location. */
	location = M_http_simple_read_header(hs->simple, "location");

	if (M_str_isempty(location)) {
		/* No location... invalid redirect. */
		hs->neterr = M_NET_ERROR_REDIRECT;
		M_snprintf(hs->error, sizeof(hs->error), "Invalid redirect: Location missing");

		M_free(location);

		call_done(hs);
		return;
	}

	hs->redirect_cnt++;
	if (hs->redirect_cnt > hs->redirect_max) {
		hs->neterr = M_NET_ERROR_REDIRECT_LIMIT;
		M_snprintf(hs->error, sizeof(hs->error), "Maximum redirects limit reached");

		M_free(location);

		call_done(hs);
		return;
	}

	if (!M_net_http_simple_send(hs, location, hs->thunk)) {
		/* Send sets error. */
		M_free(location);

		call_done(hs);
		return;
	}

	M_free(location);
}

static void process_response(M_net_http_simple_t *hs)
{
	M_uint32 status_code;

	status_code = M_http_simple_read_status_code(hs->simple);
	if (status_code >= 300 && status_code <= 399) {
		handle_redirect(hs);
		return;
	}

	call_done(hs);
}

static void init_out_buf(M_net_http_simple_t *hs)
{
	M_buf_add_bytes(hs->out_buf, M_buf_peek(hs->header_buf), M_buf_len(hs->header_buf));
	M_buf_add_bytes(hs->out_buf, hs->message, hs->message_len);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void run_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_http_simple_t *hs = thunk;
	M_http_error_t       httperr;
	M_io_error_t         ioerr;

	(void)el;

	switch (etype) {
		case M_EVENT_TYPE_CONNECTED:
			/* Kick this off by writing our headers. */
			init_out_buf(hs);
			trigger_softevent(io, M_EVENT_TYPE_WRITE);
			break;
		case M_EVENT_TYPE_READ:
			ioerr = M_io_read_into_parser(io, hs->read_parser);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			if (hs->receive_max != 0 && M_parser_len(hs->read_parser) > hs->receive_max) {
				hs->neterr = M_NET_ERROR_OVER_LIMIT;
				M_snprintf(hs->error, sizeof(hs->error), "Exceeded maximum receive data size limit");
				call_done(hs);
				break;
			}
			M_parser_mark(hs->read_parser);
			httperr = M_http_simple_read_parser(&hs->simple, hs->read_parser, M_HTTP_SIMPLE_READ_NONE);
			if (httperr == M_HTTP_ERROR_SUCCESS) {
				process_response(hs);
			} else if (httperr == M_HTTP_ERROR_MOREDATA || httperr == M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) {
				/* More possible means we didn't get a content-length so we don't
				 * know if we have all the data. We need to wait for a disconenct
				 * to find out. */
				if (httperr == M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) {
					M_http_simple_read_destroy(hs->simple);
					hs->simple = NULL;
				}
				M_parser_mark_rewind(hs->read_parser);
				timer_start_stall(hs);
			} else {
				hs->neterr  = M_NET_ERROR_PROTOFORMAT;
				hs->httperr = httperr;
				M_snprintf(hs->error, sizeof(hs->error), "Format error: %s", M_http_errcode_to_str(httperr));
				call_done(hs);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			timer_start_stall(hs);
			ioerr = M_io_write_from_buf(io, hs->out_buf);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			if (ioerr != M_IO_ERROR_SUCCESS && ioerr != M_IO_ERROR_WOULDBLOCK)
				call_done(hs);
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			goto disconnect;
			break;
		case M_EVENT_TYPE_ERROR:
			hs->neterr = M_net_io_error_to_net_error(M_io_get_error(io));
			M_io_get_error_string(io, hs->error, sizeof(hs->error));
			call_done(hs);
			break;
		case M_EVENT_TYPE_ACCEPT:
		case M_EVENT_TYPE_OTHER:
			break;
	}
	return;
disconnect:
	/* We got a disconenct from the server. Normally we're the ones
	 * to disconnect once we get the response.
	 *
	 * We'll do a final check on the data because we might not have gotten
	 * content-length and this is how we'd know when all data is sent. */
	httperr = M_http_simple_read_parser(&hs->simple, hs->read_parser, M_HTTP_SIMPLE_READ_NONE);
	if (httperr == M_HTTP_ERROR_SUCCESS || httperr == M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) {
		process_response(hs);
		return;
	}
	hs->neterr = M_NET_ERROR_DISCONNET;
	M_io_get_error_string(io, hs->error, sizeof(hs->error));
	call_done(hs);
	return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_http_simple_t *M_net_http_simple_create(M_event_t *el, M_dns_t *dns, M_net_http_simple_done_cb done_cb)
{
	M_net_http_simple_t *hs;

	if (el == NULL || dns == NULL || done_cb == NULL)
		return NULL;

	hs                  = M_malloc_zero(sizeof(*hs));
	hs->el              = el;
	hs->dns             = dns;
	hs->redirect_max    = 16;
	hs->receive_max     = 1024*1024*50; /* 50 MB */
	hs->cbs.done_cb     = done_cb;
	hs->cbs.iocreate_cb = iocreate_cb_default;
	hs->method          = M_HTTP_METHOD_GET;
	hs->user_agent      = M_strdup(DEFAULT_USER_AGENT);

	return hs;
}

void M_net_http_simple_cancel(M_net_http_simple_t *hs)
{
	if (hs == NULL)
		return;
	M_net_http_simple_destroy(hs);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_net_http_simple_set_timeouts(M_net_http_simple_t *hs, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 overall_ms)
{
	if (hs == NULL)
		return;
	hs->timeout_connect_ms = connect_ms;
	hs->timeout_stall_ms   = stall_ms;
	hs->timeout_overall_ms = overall_ms;
}

void M_net_http_simple_set_max_redirects(M_net_http_simple_t *hs, M_uint64 max)
{
	if (hs == NULL)
		return;
	hs->redirect_max = max;
}

void M_net_http_simple_set_max_receive_size(M_net_http_simple_t *hs, M_uint64 max)
{
	if (hs == NULL)
		return;
	hs->receive_max = max;
}

void M_net_http_simple_set_tlsctx(M_net_http_simple_t *hs, M_tls_clientctx_t *ctx)
{
	if (hs == NULL)
		return;

	/* unhook existing ctx */
	M_tls_clientctx_destroy(hs->ctx);

	hs->ctx = ctx;
	/* Make sure this doesn't go away while we're holding it. */
	M_tls_clientctx_upref(hs->ctx);
}

void M_net_http_simple_set_iocreate(M_net_http_simple_t *hs, M_net_http_simple_iocreate_cb iocreate_cb)
{
	if (hs == NULL)
		return;
	if (iocreate_cb != NULL) {
		hs->cbs.iocreate_cb = iocreate_cb;
	} else {
		hs->cbs.iocreate_cb = iocreate_cb_default;
	}
}

void M_net_http_simple_set_version(M_net_http_simple_t *hs, M_http_version_t version)
{
	if (hs == NULL)
		return;
	hs->version = version;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_net_http_simple_set_message(M_net_http_simple_t *hs, M_http_method_t method, const char *user_agent, const char *content_type, const char *charset, const M_hash_dict_t *headers, const unsigned char *message, size_t message_len)
{
	if (hs == NULL)
		return;

	hs->method = method;

	/* We're going to free everything just in case this was called multiple
	 * times. I shouldn't be but we'll be safe. */

	M_free(hs->user_agent);
	hs->user_agent = NULL;
	if (M_str_isempty(user_agent)) {
		hs->user_agent = M_strdup(DEFAULT_USER_AGENT);
	} else {
		/* Default if not set. */
		hs->user_agent = M_strdup(user_agent);
	}

	M_free(hs->content_type);
	hs->content_type = NULL;
	if (!M_str_isempty(content_type))
		hs->content_type = M_strdup(content_type);

	M_free(hs->charset);
	hs->charset = NULL;
	if (!M_str_isempty(charset))
		hs->charset = M_strdup(charset);

	M_hash_dict_destroy(hs->headers);
	hs->headers = NULL;
	if (headers != NULL)
		hs->headers = M_hash_dict_duplicate(headers);

	M_free(hs->message);
	hs->message     = NULL;
	hs->message_len = 0;
	if (message != NULL && message_len != 0) {
		hs->message     = M_memdup(message, message_len);
		hs->message_len = message_len;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_net_http_simple_send(M_net_http_simple_t *hs, const char *url, void *thunk)
{
	M_url_t  *url_st;

	if (hs == NULL || M_str_isempty(url) || (!M_str_caseeq_start(url, "http://") && !M_str_caseeq_start(url, "https://")))
		return M_FALSE;

	url_st = M_url_create(url);
	if (url_st == NULL)
		return M_FALSE;

	/* Setup the object for sending data. */
	M_net_http_simple_ready_send(hs);

	hs->thunk = thunk;

	/* Create our io object. */
	if (!setup_io(hs, url_st)) {
		M_url_destroy(url_st);
		return M_FALSE;
	}

	/* Setup read and write buffer. */
	/*
	hs->header_buf  = M_buf_create();
	hs->read_parser = M_parser_create(M_PARSER_FLAG_NONE);
	*/ /* Already done in M_net_http_simple_ready_send() */

	/* Add the data to the write buf. */
	M_http_simple_write_request_buf(hs->header_buf, hs->method,
		M_url_host(url_st), M_url_port_u16(url_st), M_url_path(url_st),
		hs->user_agent, hs->content_type, hs->headers,
		NULL, hs->message_len, hs->charset);

	/* Start/reset our timers. */
	timer_start_connect(hs);
	timer_start_stall(hs);
	/* Will only ever be started once. */
	timer_start_overall(hs);

	/* Add the object to the io object to the loop. */
	if (!M_event_add(hs->el, hs->io, run_cb, hs)) {
		hs->neterr = M_NET_ERROR_INTERNAL;
		M_snprintf(hs->error, sizeof(hs->error), "Event error: Failed to start");
		M_io_destroy(hs->io);
		hs->io = NULL;
		M_url_destroy(url_st);
		return M_FALSE;
	}

	M_url_destroy(url_st);
	return M_TRUE;
}
