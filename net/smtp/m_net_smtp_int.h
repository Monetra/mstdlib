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
#include "m_net_smtp_flow.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

#define M_NET_SMTP_CONNECTION_MASK_NONE      (0u)
#define M_NET_SMTP_CONNECTION_MASK_IO        (1u << 0u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDIN  (1u << 1u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDOUT (1u << 2u)
#define M_NET_SMTP_CONNECTION_MASK_IO_STDERR (1u << 3u)
#define M_NET_SMTP_CONNECTION_MASK_PROC_ALL  ((unsigned)0xF)

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
    size_t                             max_stall_retries;
};

typedef struct {
    const M_net_smtp_t  *sp;
    char                *msg;
    size_t               num_tries;
    M_hash_dict_t       *headers;
    M_email_t           *email;
    M_bool               is_bootstrap;
    char                *domain;
} M_net_smtp_dispatch_msg_args_t;

const M_net_smtp_endpoint_t * M_net_smtp_endpoint_acquire (M_net_smtp_t *sp);

M_bool   M_net_smtp_is_running                (M_net_smtp_status_t status);
void     M_net_smtp_prune_endpoints           (M_net_smtp_t *sp);
void     M_net_smtp_prune_endpoints_task      (M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
void     M_net_smtp_endpoint_release          (M_net_smtp_t *sp);
void     M_net_smtp_processing_halted         (M_net_smtp_t *sp);
M_bool   M_net_smtp_is_all_endpoints_idle     (M_net_smtp_t *sp);
M_bool   M_net_smtp_is_all_endpoints_removed  (const M_net_smtp_t *sp);
M_bool   M_net_smtp_is_all_endpoints_disabled (const M_net_smtp_t *sp);
M_uint64 M_net_smtp_endpoints_min_timeout     (const M_net_smtp_t *sp);

void     M_net_smtp_connect_fail              (M_net_smtp_session_t *session);
void     M_net_smtp_process_fail              (M_net_smtp_session_t *session, const char *outstr);

/* This is prototyped in m_net_smtp_int.h instead of m_net_smtp_session.h to avoid a dependency problem */
void     M_net_smtp_session_dispatch_msg      (M_net_smtp_session_t *session, M_net_smtp_dispatch_msg_args_t *args);

/* This is prototyped in m_net_smtp_int.h instead of m_net_smtp_endpoint.h to avoid a dependency problem */
M_bool   M_net_smtp_endpoint_dispatch_msg     (const M_net_smtp_endpoint_t *ep, M_net_smtp_dispatch_msg_args_t *args);

#endif
