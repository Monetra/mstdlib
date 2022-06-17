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

#ifndef __NET_SMTP_SESSION_H__
#define __NET_SMTP_SESSION_H__

#include "m_net_smtp_endpoint.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

typedef enum {
	M_NET_SMTP_TLS_NONE,
	M_NET_SMTP_TLS_IMPLICIT,
	M_NET_SMTP_TLS_STARTTLS,
	M_NET_SMTP_TLS_STARTTLS_READY,
	M_NET_SMTP_TLS_STARTTLS_ADDED,
	M_NET_SMTP_TLS_CONNECTED,
} M_net_smtp_tls_state_t;

typedef enum {
	M_NET_SMTP_AUTHTYPE_NONE,
	M_NET_SMTP_AUTHTYPE_LOGIN,
	M_NET_SMTP_AUTHTYPE_PLAIN,
	M_NET_SMTP_AUTHTYPE_CRAM_MD5,
	M_NET_SMTP_AUTHTYPE_DIGEST_MD5,
} M_net_smtp_authtype_t;

typedef struct {
	M_bool                       is_alive;
	M_bool                       is_successfully_sent;
	M_bool                       is_backout;
	size_t                       retry_ms;
	const M_net_smtp_t          *sp;
	M_state_machine_t           *state_machine;
	unsigned int                 connection_mask;
	M_thread_mutex_t            *mutex;
	char                        *msg;
	M_io_t                      *io;
	M_hash_dict_t               *headers;
	M_email_t                   *email;
	size_t                       number_of_tries;
	const M_net_smtp_endpoint_t *ep;
	M_buf_t                     *out_buf;
	M_parser_t                  *in_parser;
	M_event_timer_t             *event_timer;
	char                         errmsg[128];
	union {
		struct {
			M_bool                  is_starttls_capable;
			M_bool                  is_connect_fail;
			M_bool                  is_QUIT_enabled;
			M_net_smtp_tls_state_t  tls_state;
			M_net_error_t           net_error;
			M_uint64                smtp_response_code;
			M_list_str_t           *smtp_response;
			M_net_smtp_authtype_t   smtp_authtype;
			size_t                  auth_login_response_count;
			char                   *ehlo_domain;
			M_list_str_t           *rcpt_to;
		} tcp;
		struct {
			M_io_t                 *io_stdin;
			M_io_t                 *io_stdout;
			M_io_t                 *io_stderr;
			int                     result_code;
			size_t                  len;
			const char             *msg_second_part;
		} process;
	};
} M_net_smtp_session_t;

M_net_smtp_session_t * M_net_smtp_session_create(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t* ep);

void M_net_smtp_session_reactivate_tcp     (M_net_smtp_session_t *session);
void M_net_smtp_session_reactivate_tcp_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
void M_net_smtp_session_clean              (M_net_smtp_session_t *session);
void M_net_smtp_session_destroy            (M_net_smtp_session_t *session, M_bool is_remove_from_endpoint);
void M_net_smtp_session_destroy_task       (M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);

/* These are prototyped in m_net_smtp_session.h instead of m_net_smtp_endpoint.h to avoid a dependency problem */
void M_net_smtp_endpoint_cull_session      (const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);
void M_net_smtp_endpoint_remove_session    (const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);
void M_net_smtp_endpoint_idle_session      (const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);

/* This is prototyped in m_net_smtp_int.h instead of m_net_smtp_session.h to avoid a dependency problem */
/*
void M_net_smtp_session_dispatch_msg  (M_net_smtp_session_t *session, M_net_smtp_dispatch_msg_args_t *args);
*/

#endif
