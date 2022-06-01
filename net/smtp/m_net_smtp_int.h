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

#ifndef __NET_SMTP_INT_H__
#define __NET_SMTP_INT_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

typedef enum {
	M_NET_SMTP_EPTYPE_PROCESS = 1,
	M_NET_SMTP_EPTYPE_TCP
} M_net_smtp_endpoint_type_t;

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

#define M_NET_SMTP_CONNECTION_MASK_NONE      (0u)
#define M_NET_SMTP_CONNECTION_MASK_IO        (1u << 0u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDIN  (1u << 1u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDOUT (1u << 2u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDERR (1u << 3u)

typedef struct endpoint_struct {
	M_net_smtp_endpoint_type_t  type;
	M_bool                      is_removed;
	size_t                      max_sessions;
	M_thread_rwlock_t          *sessions_rwlock;
	M_list_t                   *send_sessions;
	M_list_t                   *idle_sessions;
	union {
		struct {
			char          *address;
			M_uint16       port;
			M_bool         connect_tls;
			char          *username;
			char          *password;
		} tcp;
		struct {
			char          *command;
			M_list_str_t  *args;
			M_hash_dict_t *env;
			M_uint64       timeout_ms;
		} process;
	};
} M_net_smtp_endpoint_t;

typedef struct {
	M_bool                       is_alive;
	M_bool                       is_successfully_sent;
	M_bool                       is_backout;
	size_t                       retry_ms;
	M_net_smtp_t                *sp;
	M_state_machine_t           *state_machine;
	unsigned int                 connection_mask;
	M_thread_mutex_t            *mutex;
	char                        *msg;
	M_io_t                      *io;
	const M_hash_dict_t         *headers;
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
			const char             *next_write_chunk;
		} process;
	};
} M_net_smtp_endpoint_session_t;

M_bool M_net_smtp_flow_tcp_check_smtp_response_code(M_net_smtp_endpoint_session_t *session, M_uint64 expected_code);
M_bool M_net_smtp_flow_tcp_smtp_response_pre_cb_helper(void *data, M_state_machine_status_t *status, M_uint64 *next);
M_state_machine_status_t M_net_smtp_flow_tcp_smtp_response_post_cb_helper(
		void *data, M_state_machine_status_t sub_status, M_uint64 *next);

M_state_machine_t *M_net_smtp_flow_process(void);
M_state_machine_t *M_net_smtp_flow_tcp_smtp_response(void);
M_state_machine_t *M_net_smtp_flow_tcp_starttls(void);
M_state_machine_t *M_net_smtp_flow_tcp_sendmsg(void);
M_state_machine_t *M_net_smtp_flow_tcp_auth(void);
M_state_machine_t *M_net_smtp_flow_tcp_ehlo(void);
M_state_machine_t *M_net_smtp_flow_tcp(void);

#endif
