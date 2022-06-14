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

#ifndef __NET_SMTP_ENDPOINT_H__
#define __NET_SMTP_ENDPOINT_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>

typedef enum {
	M_NET_SMTP_EPTYPE_PROCESS = 1,
	M_NET_SMTP_EPTYPE_TCP
} M_net_smtp_endpoint_type_t;

typedef struct {
	M_net_smtp_endpoint_type_t  type;
	M_bool                      is_removed;
	size_t                      max_sessions;
	M_thread_rwlock_t          *sessions_rwlock;
	M_list_t                   *send_sessions;
	M_list_t                   *idle_sessions;
	M_list_t                   *cull_sessions;
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
	const char   *address;
	M_uint16      port;
	M_bool        connect_tls;
	const char   *username;
	const char   *password;
	size_t        max_conns;
} M_net_smtp_endpoint_tcp_args_t;

typedef struct {
	const char *command;
	const M_list_str_t *args;
	const M_hash_dict_t *env;
	M_uint64 timeout_ms;
	size_t max_processes;
} M_net_smtp_endpoint_proc_args_t;

M_net_smtp_endpoint_t * M_net_smtp_endpoint_create_proc(M_net_smtp_endpoint_proc_args_t *args);
M_net_smtp_endpoint_t * M_net_smtp_endpoint_create_tcp(M_net_smtp_endpoint_tcp_args_t *args);

M_bool M_net_smtp_endpoint_is_available        (const M_net_smtp_endpoint_t *ep);
M_bool M_net_smtp_endpoint_is_idle             (const M_net_smtp_endpoint_t *ep);
void   M_net_smtp_endpoint_reactivate_idle     (const M_net_smtp_endpoint_t *ep);
void   M_net_smtp_endpoint_reactivate_idle_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
void   M_net_smtp_endpoint_destroy             (M_net_smtp_endpoint_t *ep);
M_bool M_net_smtp_endpoint_destroy_is_ready    (const M_net_smtp_endpoint_t *ep);

/* These are prototyped in m_net_smtp_session.h instead of m_net_smtp_endpoint.h to avoid a dependency problem
void   M_net_smtp_endpoint_cull_session  (const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);
void   M_net_smtp_endpoint_remove_session(const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);
void   M_net_smtp_endpoint_idle_session  (const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session);
*/

/* This is prototyped in m_net_smtp_int.h to avoid a dependency problem
M_bool M_net_smtp_endpoint_dispatch_msg  (const M_net_smtp_endpoint_t *ep, M_net_smtp_dispatch_msg_t *args);
*/

#endif
