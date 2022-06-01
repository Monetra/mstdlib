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

#include "../m_net_int.h"
#include "m_net_smtp_session.h"
#include "m_net_smtp_endpoint.h"
#include "m_net_smtp_queue.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

#define M_NET_SMTP_CONNECTION_MASK_NONE      (0u)
#define M_NET_SMTP_CONNECTION_MASK_IO        (1u << 0u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDIN  (1u << 1u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDOUT (1u << 2u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDERR (1u << 3u)

struct M_net_smtp {
	M_event_t                         *el;
	struct M_net_smtp_callbacks        cbs;
	void                              *thunk;
	M_list_t                          *endpoints;
	M_thread_mutex_t                  *endpoints_mutex;
	M_net_smtp_status_t                status;
	M_thread_rwlock_t                 *status_rwlock;
	M_dns_t                           *tcp_dns;
	M_tls_clientctx_t                 *tcp_tls_ctx;
	M_uint64                           tcp_connect_ms;
	M_uint64                           tcp_stall_ms;
	M_uint64                           tcp_idle_ms;
	M_net_smtp_load_balance_t          load_balance_mode;
	size_t                             round_robin_idx;
	M_event_timer_t                   *restart_processing_timer;
	M_net_smtp_queue_t                *queue;
};

void M_net_smtp_prune_endpoints(M_net_smtp_t *sp);
M_bool M_net_smtp_is_running(M_net_smtp_status_t status);
void M_net_smtp_round_robin_advance(M_net_smtp_t *sp);
const M_net_smtp_endpoint_t * M_net_smtp_endpoint(M_net_smtp_t *sp);
M_bool M_net_smtp_flow_tcp_check_smtp_response_code(M_net_smtp_session_t *session, M_uint64 expected_code);
M_bool M_net_smtp_flow_tcp_smtp_response_pre_cb_helper(void *data, M_state_machine_status_t *status, M_uint64 *next);
M_state_machine_status_t M_net_smtp_flow_tcp_smtp_response_post_cb_helper(
		void *data, M_state_machine_status_t sub_status, M_uint64 *next);
void M_net_smtp_processing_halted(M_net_smtp_t *sp);

M_state_machine_t *M_net_smtp_flow_process(void);
M_state_machine_t *M_net_smtp_flow_tcp_smtp_response(void);
M_state_machine_t *M_net_smtp_flow_tcp_starttls(void);
M_state_machine_t *M_net_smtp_flow_tcp_sendmsg(void);
M_state_machine_t *M_net_smtp_flow_tcp_auth(void);
M_state_machine_t *M_net_smtp_flow_tcp_ehlo(void);
M_state_machine_t *M_net_smtp_flow_tcp(void);

#endif
