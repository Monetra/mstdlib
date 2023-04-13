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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_http_simple {
	M_event_t         *el;

	M_dns_t           *dns;
	M_tls_clientctx_t *ctx;

	M_uint64           redirect_max;
	M_uint64           redirect_cnt;

	M_uint64           receive_max;

	M_uint64           timeout_connect_ms;
	M_uint64           timeout_stall_ms;
	M_uint64           timeout_overall_ms;
	M_event_timer_t   *timer_stall;
	M_event_timer_t   *timer_overall;

	M_io_t            *io;
	M_parser_t        *read_parser;
	M_buf_t           *header_buf;

	char              *proxy_server;
	char              *proxy_auth;

	M_http_simple_read_t *simple;

	M_http_method_t  method;
	char            *user_agent;
	char            *content_type;
	char            *charset;
	M_hash_dict_t   *headers;
	unsigned char   *message;
	size_t           message_len;
	size_t           message_pos;

	M_http_error_t   httperr;
	M_net_error_t    neterr;
	char             error[256];

	void            *thunk;

	M_net_http_simple_done_cb      done_cb;
	M_net_http_simple_iocreate_cb  iocreate_cb;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const char *DEFAULT_USER_AGENT = "MSTDLIB/Net HTTP Simple " NET_HTTP_VERSION;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void io_destroy_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)thunk;
	M_io_destroy(io);
}

static void io_disconnect_and_destroy(M_net_http_simple_t *hs)
{
	M_io_state_t state;

	if (hs == NULL || hs->io == NULL)
		return;

	state = M_io_get_state(hs->io);
	if (state == M_IO_STATE_CONNECTED || state == M_IO_STATE_DISCONNECTING) {
		M_event_edit_io_cb(hs->io, io_destroy_cb, NULL);
	} else {
		M_io_destroy(hs->io);
	}
	hs->io = NULL;
}

static void M_net_http_simple_destroy(M_net_http_simple_t *hs)
{
	io_disconnect_and_destroy(hs);

	M_tls_clientctx_destroy(hs->ctx);

	M_event_timer_remove(hs->timer_stall);
	M_event_timer_remove(hs->timer_overall);

	M_parser_destroy(hs->read_parser);
	M_buf_cancel(hs->header_buf);
	M_http_simple_read_destroy(hs->simple);
	M_hash_dict_destroy(hs->headers);
	M_free(hs->message);
	M_free(hs->proxy_server);
	M_free(hs->proxy_auth);

	M_free(hs);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void split_url(const char *url, char **host, M_uint16 *port, char **uri)
{
	M_parser_t *parser;
	M_uint64    port64;
	M_uint16    myport;

	if (port == NULL)
		port = &myport;
	*port = 0;

	parser = M_parser_create_const((const unsigned char *)url, M_str_len(url), M_PARSER_FLAG_NONE);

	/* Kill off the prefix. */ 
	M_parser_consume_str_until(parser, "://", M_TRUE);

	/* Mark the start of the host. */
	M_parser_mark(parser);

	if (M_parser_consume_str_until(parser, ":", M_FALSE) != 0) {
		/* Having a ":" means we have a port so everything before is
		 * the host. */
		if (host != NULL) {
			*host = M_parser_read_strdup_mark(parser);
		}

		/* kill the ":". */
		M_parser_consume(parser, 1);

		/* Read the port. */
		if (M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 0, 10, &port64)) {
			*port = (M_uint16)port64;
		}
	} else if (M_parser_consume_str_until(parser, "/", M_FALSE) != 0) {
		/* No port was specified try to find the start of the path. */
		if (host != NULL) {
			*host = M_parser_read_strdup_mark(parser);
		}
	} else {
		/* No port and no / means all we have is the host. */
		if (host != NULL) {
			*host = M_parser_read_strdup(parser, M_parser_len(parser));
		}
	}

	/* Get the uri. */
	if (M_parser_len(parser) == 0) {
		/* Nothing left, must be /. */
		if (uri != NULL) {
			*uri = M_strdup("/");
		}
	} else {
		if (uri != NULL) {
			*uri = M_parser_read_strdup(parser, M_parser_len(parser));
		}
	}

	if (*port == 0) {
		if (M_str_caseeq_start(url, "http://")) {
			*port = 80;
		} else {
			/* Prefer secure connections. */
			*port = 443;
		}
	}

	M_parser_destroy(parser);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void call_done(M_net_http_simple_t *hs)
{
	/* Got the final data. Stop our timers. */
	M_event_timer_stop(hs->timer_stall);
	M_event_timer_stop(hs->timer_overall);

	hs->done_cb(hs->neterr, hs->httperr, hs->simple, hs->error, hs->thunk);

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

	hs->message_pos = 0;

	M_http_simple_read_destroy(hs->simple);
	hs->simple = NULL;

	io_disconnect_and_destroy(hs);
}

static M_bool setup_io(M_net_http_simple_t *hs, const char *url)
{
	char         *hostname;
	M_uint16      port;
	M_io_error_t  ioerr;
	size_t        lid;

	if (hs->proxy_server == NULL) {
		split_url(url, &hostname, &port, NULL);
	} else {
		split_url(hs->proxy_server, &hostname, &port, NULL);
	}
	ioerr = M_io_net_client_create(&hs->io, hs->dns, hostname, port, M_IO_NET_ANY);
	M_free(hostname);
	if (ioerr != M_IO_ERROR_SUCCESS) {
		hs->neterr = M_NET_ERROR_CREATE;
		M_snprintf(hs->error, sizeof(hs->error), "Failed to create network client: %s", M_io_error_string(ioerr));
		return M_FALSE;
	}

	/* If this is an https connection add the context. */
	if (M_str_caseeq_start(url, "https://")) {
		if (hs->ctx == NULL) {
			hs->neterr = M_NET_ERROR_TLS_REQUIRED;
			M_snprintf(hs->error, sizeof(hs->error), "HTTPS Connection required but client context no set");
			io_disconnect_and_destroy(hs);
			return M_FALSE;
		}
			
		ioerr = M_io_tls_client_add(hs->io, hs->ctx, NULL, &lid);
		if (ioerr != M_IO_ERROR_SUCCESS) {
			hs->neterr = M_NET_ERROR_TLS_SETUP_FAILURE;
			M_snprintf(hs->error, sizeof(hs->error), "Failed to add client context: %s", M_io_error_string(ioerr));
			io_disconnect_and_destroy(hs);
			return M_FALSE;
		}
	}

	/* Allow the user of this object to add any additional layers. Logging, bw shapping... */
	if (hs->iocreate_cb) {
		if (!hs->iocreate_cb(hs->io, hs->error, sizeof(hs->error), hs->thunk)) {
			hs->neterr = M_NET_ERROR_CREATE;
			if (M_str_isempty(hs->error)) {
				M_snprintf(hs->error, sizeof(hs->error), "iocreate generic failure");
			}
			io_disconnect_and_destroy(hs);
			return M_FALSE;
		}
	}

	return M_TRUE;
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

static M_bool write_data(M_io_t *io, M_net_http_simple_t *hs)
{
	M_io_error_t ioerr;
	size_t       wrote = 0;

	/* Keep writing our headers until we've gotten them all out. */
	if (M_buf_len(hs->header_buf) > 0) {
		ioerr = M_io_write_from_buf(io, hs->header_buf);
		if (ioerr != M_IO_ERROR_SUCCESS && ioerr != M_IO_ERROR_WOULDBLOCK) {
			hs->neterr = M_net_io_error_to_net_error(ioerr);
			M_io_get_error_string(io, hs->error, sizeof(hs->error));
			return M_FALSE;
		}

		/* If we have data in the header buf we've
 		 * written everything we can and shouldn't
		 * try to write more until we've emptied it. */
		if (M_buf_len(hs->header_buf) > 0) {
			return M_TRUE;
		}
	}

	/* Write the message. */
	if (hs->message_pos < hs->message_len) {
		ioerr = M_io_write(io, hs->message+hs->message_pos, hs->message_len-hs->message_pos, &wrote);
		if (ioerr == M_IO_ERROR_SUCCESS) {
			hs->message_pos += wrote;
		} else if (ioerr != M_IO_ERROR_WOULDBLOCK) {
			hs->neterr = M_net_io_error_to_net_error(ioerr);
			M_io_get_error_string(io, hs->error, sizeof(hs->error));
			return M_FALSE;
		}
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void run_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_http_simple_t *hs = thunk;
	M_http_error_t       httperr;

	(void)el;

    switch (etype) {
        case M_EVENT_TYPE_CONNECTED:
			/* Kick this off by writing our headers. */
			if (!write_data(io, hs)) {
				call_done(hs);
			}
            break;
        case M_EVENT_TYPE_READ:
            M_io_read_into_parser(io, hs->read_parser);

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
			if (!write_data(io, hs)) {
				call_done(hs);
			}
            break;
        case M_EVENT_TYPE_DISCONNECTED:
			/* We got a disconenct from the server. Normally we're the ones
			 * to disconnect once we get the response.
			 *
			 * We'll do a final check on the data because we might not have gotten
			 * content-length and this is how we'd know when all data is sent. */
			httperr = M_http_simple_read_parser(&hs->simple, hs->read_parser, M_HTTP_SIMPLE_READ_NONE);
			if (httperr == M_HTTP_ERROR_SUCCESS || httperr == M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE) {
				process_response(hs);
				break;
			}
			hs->neterr = M_NET_ERROR_DISCONNET;
			M_io_get_error_string(io, hs->error, sizeof(hs->error));
			call_done(hs);
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
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_http_simple_t *M_net_http_simple_create(M_event_t *el, M_dns_t *dns, M_net_http_simple_done_cb done_cb)
{
	M_net_http_simple_t *hs;

	if (el == NULL || dns == NULL || done_cb == NULL)
		return NULL;

	hs               = M_malloc_zero(sizeof(*hs));
	hs->el           = el;
	hs->dns          = dns;
	hs->redirect_max = 16;
	hs->receive_max  = 1024*1024*50; /* 50 MB */
	hs->done_cb      = done_cb;
	hs->method       = M_HTTP_METHOD_GET;
	hs->user_agent   = M_strdup(DEFAULT_USER_AGENT);

	return hs;
}

void M_net_http_simple_cancel(M_net_http_simple_t *hs)
{
	if (hs == NULL)
		return;
	M_net_http_simple_destroy(hs);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_net_http_simple_set_proxy_authentication(M_net_http_simple_t *hs, const char *user, const char *pass)
{
	char *userpass;
	char *userpass_b64;

	if (hs == NULL || user == NULL || pass == NULL)
		return;

	M_free(hs->proxy_auth);

	M_asprintf(&userpass, "%s:%s", user, pass);
	userpass_b64 = M_bincodec_encode_alloc((M_uint8*)userpass, M_str_len(userpass), 0, M_BINCODEC_BASE64);
	M_asprintf(&hs->proxy_auth, "Basic %s", userpass_b64);

	M_free(userpass_b64);
	M_free(userpass);
}

void M_net_http_simple_set_proxy(M_net_http_simple_t *hs, const char *proxy_server)
{
	if (hs == NULL)
		return;
	M_free(hs->proxy_server);
	hs->proxy_server = M_strdup(proxy_server);
}

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
	hs->ctx = ctx;
	/* Make sure this doesn't go away while we're holding it. */
	M_tls_clientctx_upref(hs->ctx);
}

void M_net_http_simple_set_iocreate(M_net_http_simple_t *hs, M_net_http_simple_iocreate_cb iocreate_cb)
{
	if (hs == NULL)
		return;
	hs->iocreate_cb = iocreate_cb;
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
	const char *request_url;
	char       *host;
	char       *uri;
	M_uint16    port;

	if (hs == NULL || M_str_isempty(url) || (!M_str_caseeq_start(url, "http://") && !M_str_caseeq_start(url, "https://")))
		return M_FALSE;

	/* Setup the object for sending data. */
	M_net_http_simple_ready_send(hs);

	hs->thunk = thunk;

	/* Create our io object. */
	if (!setup_io(hs, url))
		return M_FALSE;

	/* Setup read and write buffer. */
	hs->header_buf  = M_buf_create();
	hs->read_parser = M_parser_create(M_PARSER_FLAG_NONE);

	/* Add the data to the write buf. */
	split_url(url, &host, &port, &uri);
	if (hs->proxy_server != NULL) {
		request_url = url;
		if (hs->proxy_auth != NULL) {
			if (hs->headers == NULL) {
				hs->headers = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);
		}
			M_hash_dict_insert(hs->headers, "Proxy-Authorization", hs->proxy_auth);
		}
	} else {
		request_url = uri;
	}
	M_http_simple_write_request_buf(hs->header_buf, hs->method,
		host, port, request_url,
		hs->user_agent, hs->content_type, hs->headers,
		NULL, hs->message_len, hs->charset);
	M_free(host);
	M_free(uri);

	/* Start/reset our timers. */
	timer_start_connect(hs);
	timer_start_stall(hs);
	/* Will only ever be started once. */
	timer_start_overall(hs);

	/* Add the object to the io object to the loop. */
	if (!M_event_add(hs->el, hs->io, run_cb, hs)) {
		hs->neterr = M_NET_ERROR_INTERNAL;
		M_snprintf(hs->error, sizeof(hs->error), "Event error: Failed to start");
		io_disconnect_and_destroy(hs);
		return M_FALSE;
	}

	return M_TRUE;
}
