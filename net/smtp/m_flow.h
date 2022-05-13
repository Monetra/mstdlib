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

#ifndef __SMTP_FLOW_H__
#define __SMTP_FLOW_H__

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
} M_net_smtp_authtype_t;

#define M_NET_SMTP_CONNECTION_MASK_NONE      (0u)
#define M_NET_SMTP_CONNECTION_MASK_IO        (1u << 0u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDIN  (1u << 1u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDOUT (1u << 2u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDERR (1u << 3u)

typedef struct {
	M_net_smtp_t               *sp;
	M_net_smtp_endpoint_type_t  endpoint_type;
	M_state_machine_t          *state_machine;
	M_bool                      is_alive;
	unsigned int                connection_mask;
	M_io_t                     *io;
	char                       *msg;
	M_hash_dict_t              *headers;
	M_email_t                  *email;
	const char                 *address;
	size_t                      str_len_address;
	M_int16                     smtp_response_code;
	M_list_str_t               *smtp_response;
	M_net_smtp_authtype_t       smtp_authtype;
	M_bool                      is_starttls_capable;
	const char                 *username;
	size_t                      str_len_username;
	const char                 *password;
	size_t                      str_len_password;
	const char                 *str_auth_plain_base64;
	const char                 *str_auth_login_username_base64;
	const char                 *str_auth_login_password_base64;
	size_t                      auth_login_response_count;
	char                       *ehlo_domain;
	size_t                      tls_ctx_layer_idx;
	size_t                      rcpt_n;
	size_t                      rcpt_i;
	size_t                      number_of_tries;
	const void                 *endpoint_manager;
	M_net_smtp_tls_state_t      tls_state;
	M_bool                      is_failure;
	M_bool                      is_backout;
	M_bool                      is_connect_fail;
	M_bool                      is_QUIT_enabled;
	M_net_error_t               net_error;
	int                         result_code;
	char                        errmsg[128];
	M_buf_t                    *out_buf;
	M_parser_t                 *in_parser;
	M_event_timer_t            *event_timer;

	/* Only used for proc endpoints */
	M_io_t                     *io_stdin;
	M_io_t                     *io_stdout;
	M_io_t                     *io_stderr;
} M_net_smtp_endpoint_slot_t;

M_bool M_net_smtp_flow_tcp_smtp_response_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next);
M_state_machine_status_t M_net_smtp_flow_tcp_smtp_response_post_cb(
		void *data, M_state_machine_status_t sub_status, M_uint64 *next);

M_state_machine_t *M_net_smtp_flow_process(void);
M_state_machine_t *M_net_smtp_flow_tcp_smtp_response(void);
M_state_machine_t *M_net_smtp_flow_tcp_starttls(void);
M_state_machine_t *M_net_smtp_flow_tcp_sendmsg(void);
M_state_machine_t *M_net_smtp_flow_tcp_auth(void);
M_state_machine_t *M_net_smtp_flow_tcp_ehlo(void);
M_state_machine_t *M_net_smtp_flow_tcp(void);

#endif
