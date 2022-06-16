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

#include "smtp/m_net_smtp_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void endpoint_failure(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t *ep, M_bool is_remove_ep)
{
	M_thread_mutex_lock(sp->endpoints_mutex);
	if (is_remove_ep) {
		M_CAST_OFF_CONST(M_net_smtp_endpoint_t*,ep)->is_removed = M_TRUE;
		/* Reactivate any idle sessions to quit out */
		M_event_queue_task(sp->el, M_net_smtp_endpoint_reactivate_idle_task, M_CAST_OFF_CONST(M_net_smtp_endpoint_t*,ep));
		if (M_net_smtp_is_all_endpoints_removed(sp)) {
			M_net_smtp_pause(M_CAST_OFF_CONST(M_net_smtp_t*,sp));
		}
	} else {
		if (sp->load_balance_mode == M_NET_SMTP_LOAD_BALANCE_FAILOVER && M_list_len(sp->endpoints) > 1) {
			/* Had a failure, but they want to keep the endpoint.
			 * Fail this endpoint over to the back of the list.
			 */
			M_list_remove_val(sp->endpoints, ep, M_LIST_MATCH_PTR);
			M_list_insert(sp->endpoints, ep);
		}
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
}

static void add_endpoint(M_net_smtp_t *sp, const M_net_smtp_endpoint_t *ep)
{
	M_thread_mutex_lock(sp->endpoints_mutex);
	M_list_insert(sp->endpoints, ep);
	M_thread_mutex_unlock(sp->endpoints_mutex);

	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (sp->status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
	}
	M_thread_rwlock_unlock(sp->status_rwlock);

	M_net_smtp_resume(sp);
}

static void restart_processing_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_t *sp = thunk;
	(void)el;
	(void)etype;
	(void)io;

	M_event_timer_remove(sp->restart_processing_timer);
	sp->restart_processing_timer = NULL;
	M_net_smtp_resume(sp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_net_smtp_is_running(M_net_smtp_status_t status)
{
	return status == M_NET_SMTP_STATUS_PROCESSING || status == M_NET_SMTP_STATUS_IDLE;
}

M_bool M_net_smtp_is_all_endpoints_removed(const M_net_smtp_t *sp)
{
	M_bool is_all_endpoints_removed = M_TRUE;
	size_t i;
	M_thread_mutex_lock(sp->endpoints_mutex);
	for (i=0; i<M_list_len(sp->endpoints); i++) {
		const M_net_smtp_endpoint_t *ep = M_list_at(sp->endpoints, i);
		if (!ep->is_removed) {
			is_all_endpoints_removed = M_FALSE;
			break;
		}
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
	return is_all_endpoints_removed;
}

M_bool M_net_smtp_is_all_endpoints_idle(M_net_smtp_t *sp)
{
	M_bool is_all_endpoints_idle = M_TRUE;
	size_t i;
	M_thread_mutex_lock(sp->endpoints_mutex);
	for (i=0; i<M_list_len(sp->endpoints); i++) {
		if (M_net_smtp_endpoint_is_idle(M_list_at(sp->endpoints, i)) == M_FALSE) {
			is_all_endpoints_idle =  M_FALSE;
			break;
		}
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
	return is_all_endpoints_idle;
}

const M_net_smtp_endpoint_t *M_net_smtp_endpoint_acquire(M_net_smtp_t *sp)
{
	const M_net_smtp_endpoint_t *ep;
	size_t                       i;
	size_t                       idx;
	M_thread_mutex_lock(sp->endpoints_mutex);
	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			for (i=0; i<M_list_len(sp->endpoints); i++) {
				ep = M_list_at(sp->endpoints, i);
				if (ep->is_removed)
					continue;
				if (M_net_smtp_endpoint_is_available(ep)) {
					return ep;
				}
				break;
			}
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			for (i=0; i<M_list_len(sp->endpoints); i++) {
				idx = (sp->round_robin_idx + i + 1) % M_list_len(sp->endpoints);
				ep = M_list_at(sp->endpoints, idx);
				if (ep->is_removed)
					continue;
				if (M_net_smtp_endpoint_is_available(ep)) {
					if (i == 0) {
						sp->round_robin_idx = idx;
					}
					return ep;
				}
			}
			break;
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
	return NULL;
}

void M_net_smtp_endpoint_release(M_net_smtp_t *sp)
{
	M_thread_mutex_unlock(sp->endpoints_mutex);
}

void M_net_smtp_prune_endpoints(M_net_smtp_t *sp)
{
	size_t i;
	M_bool is_requeue = M_FALSE;
	for (i=M_list_len(sp->endpoints); i-->0; ) {
		const M_net_smtp_endpoint_t *ep = M_list_at(sp->endpoints, i);
		if (ep->is_removed) {
			if (M_net_smtp_endpoint_destroy_is_ready(ep)) {
				M_net_smtp_endpoint_destroy(M_list_take_at(sp->endpoints, i));
			} else {
				is_requeue = M_TRUE;
			}
		}
	}
	if (is_requeue) {
		M_event_queue_task(sp->el, M_net_smtp_prune_endpoints_task, sp);
	}
}

void M_net_smtp_prune_endpoints_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	M_net_smtp_prune_endpoints(thunk);
}

void M_net_smtp_processing_halted(M_net_smtp_t *sp)
{
	M_bool                       is_no_endpoints = M_net_smtp_is_all_endpoints_removed(sp);
	M_uint64                     delay_ms;

	if (is_no_endpoints) {
		sp->status = M_NET_SMTP_STATUS_NOENDPOINTS;
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
	}

	delay_ms = sp->cbs.processing_halted_cb(is_no_endpoints, sp->thunk);
	if (delay_ms == 0 || is_no_endpoints)
		return;

	sp->restart_processing_timer = M_event_timer_oneshot(sp->el, delay_ms, M_FALSE, restart_processing_task, sp);
}

void M_net_smtp_connect_fail(M_net_smtp_session_t *session)
{
	const M_net_smtp_t          *sp        = session->sp;
	const M_net_smtp_endpoint_t *ep        = session->ep;
	M_net_error_t                net_error = session->tcp.net_error;
	const char                  *errmsg    = session->errmsg;

	M_bool is_remove_ep = sp->cbs.connect_fail_cb(ep->tcp.address, ep->tcp.port, net_error, errmsg, sp->thunk);
	endpoint_failure(sp, ep, is_remove_ep);
}

void M_net_smtp_process_fail(M_net_smtp_session_t *session, const char *stdout_str)
{
	const M_net_smtp_t          *sp          = session->sp;
	const M_net_smtp_endpoint_t *ep          = session->ep;
	int                          result_code = session->process.result_code;
	const char                  *errmsg      = session->errmsg;

	M_bool is_remove_ep = sp->cbs.process_fail_cb(ep->process.command, result_code, stdout_str, errmsg, sp->thunk);
	endpoint_failure(sp, ep, is_remove_ep);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* These NOP callbacks are used to avoid NULL checking for functions. If they don't
 * supply a callback we will just use these.
 */

static void nop_sent_cb(const M_hash_dict_t *a, void * b)
{
	(void)a;
	(void)b;
}

static void nop_connect_cb(const char *a, M_uint16 b, void *c)
{
	(void)a;
	(void)b;
	(void)c;
}

static void nop_disconnect_cb(const char *a, M_uint16 b, void *c)
{
	(void)a;
	(void)b;
	(void)c;
}

static void nop_reschedule_cb(const char *a, M_uint64 b, void *c)
{
	(void)a;
	(void)b;
	(void)c;
}

static M_bool nop_connect_fail_cb(const char *a, M_uint16 b, M_net_error_t c, const char *d, void *e)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
	return M_FALSE; /* Don't remove endpoint */
}

static M_bool nop_process_fail_cb(const char *a, int b, const char *c, const char *d, void *e)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
	return M_FALSE; /* Don't remove endpoint */
}

static M_bool nop_send_failed_cb(const M_hash_dict_t *a, const char *b, size_t c, M_bool d, void *e)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
	return M_TRUE; /* Requeue message */
}

static M_uint64 nop_processing_halted_cb(M_bool a, void *b)
{
	(void)a;
	(void)b;
	return 0; /* Don't restart processing */
}

static M_bool nop_iocreate_cb(M_io_t *a, char *b, size_t c, void *d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return M_TRUE; /* Continue */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_smtp_t *M_net_smtp_create(M_event_t *el, const struct M_net_smtp_callbacks *cbs, void *thunk)
{
	M_net_smtp_t *sp;
	const struct M_net_smtp_callbacks nop_cbs = { nop_connect_cb, nop_connect_fail_cb, nop_disconnect_cb, nop_process_fail_cb, 
		nop_processing_halted_cb, nop_sent_cb, nop_send_failed_cb, nop_reschedule_cb, nop_iocreate_cb };

	if (el == NULL)
		return NULL;

	sp                           = M_malloc_zero(sizeof(*sp));
	sp->endpoints                = M_list_create(NULL, M_LIST_NONE);
	sp->queue                    = M_net_smtp_queue_create(sp, 3, 900000 /* 15min */);
	sp->status_rwlock            = M_thread_rwlock_create();
	sp->endpoints_mutex          = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	if (cbs == NULL) {
		M_mem_copy(&sp->cbs, &nop_cbs, sizeof(sp->cbs));
	} else {
		sp->cbs.connect_cb           = cbs->connect_cb           ? cbs->connect_cb           : nop_connect_cb;
		sp->cbs.connect_fail_cb      = cbs->connect_fail_cb      ? cbs->connect_fail_cb      : nop_connect_fail_cb;
		sp->cbs.disconnect_cb        = cbs->disconnect_cb        ? cbs->disconnect_cb        : nop_disconnect_cb;
		sp->cbs.process_fail_cb      = cbs->process_fail_cb      ? cbs->process_fail_cb      : nop_process_fail_cb;
		sp->cbs.processing_halted_cb = cbs->processing_halted_cb ? cbs->processing_halted_cb : nop_processing_halted_cb;
		sp->cbs.sent_cb              = cbs->sent_cb              ? cbs->sent_cb              : nop_sent_cb;
		sp->cbs.send_failed_cb       = cbs->send_failed_cb       ? cbs->send_failed_cb       : nop_send_failed_cb;
		sp->cbs.reschedule_cb        = cbs->reschedule_cb        ? cbs->reschedule_cb        : nop_reschedule_cb;
		sp->cbs.iocreate_cb          = cbs->iocreate_cb          ? cbs->iocreate_cb          : nop_iocreate_cb;
	}
	sp->el             = el;
	sp->thunk          = thunk;
	sp->status         = M_NET_SMTP_STATUS_NOENDPOINTS;
	sp->tcp_connect_ms = 5000;
	sp->tcp_stall_ms   = 5000;
	sp->tcp_idle_ms    = 1000;

	return sp;
}

void M_net_smtp_destroy(M_net_smtp_t *sp)
{
	M_net_smtp_endpoint_t *ep    = NULL;

	if (sp == NULL)
		return;

	M_tls_clientctx_destroy(sp->tcp_tls_ctx);
	M_thread_mutex_lock(sp->endpoints_mutex);
	ep = M_list_take_last(sp->endpoints);
	while (ep != NULL) {
		M_net_smtp_endpoint_destroy(ep);
		ep = M_list_take_last(sp->endpoints);
	}
	M_list_destroy(sp->endpoints, M_TRUE);
	M_thread_mutex_unlock(sp->endpoints_mutex);
	M_thread_mutex_destroy(sp->endpoints_mutex);
	M_event_timer_remove(sp->restart_processing_timer);
	M_thread_rwlock_destroy(sp->status_rwlock);
	M_net_smtp_queue_destroy(sp->queue);
	M_free(sp);
}

void M_net_smtp_pause(M_net_smtp_t *sp)
{
	if (sp == NULL)
		return;

	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (!M_net_smtp_is_running(sp->status)) {
		M_thread_rwlock_unlock(sp->status_rwlock);
		return;
	}

	if (sp->status == M_NET_SMTP_STATUS_IDLE) {
		M_net_smtp_processing_halted(sp);
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPING;
	}
	M_thread_rwlock_unlock(sp->status_rwlock);
}

M_bool M_net_smtp_resume(M_net_smtp_t *sp)
{
	M_net_smtp_status_t status;

	if (sp == NULL)
		return M_FALSE;

	status = M_net_smtp_status(sp);

	switch (status) {
		case M_NET_SMTP_STATUS_NOENDPOINTS:
			sp->cbs.processing_halted_cb(M_TRUE, sp->thunk);
			return M_FALSE;
		case M_NET_SMTP_STATUS_PROCESSING:
		case M_NET_SMTP_STATUS_IDLE:
			return M_TRUE;
		case M_NET_SMTP_STATUS_STOPPED:
		case M_NET_SMTP_STATUS_STOPPING:
			if (status == M_NET_SMTP_STATUS_STOPPED) {
				/* Prune any removed endpoints before starting again, but after any pending reactivate idle */
				M_event_queue_task(sp->el, M_net_smtp_prune_endpoints_task, sp);
				M_net_smtp_prune_endpoints(sp);
			}
			if (M_net_smtp_queue_is_pending(sp->queue)) {
				sp->status = M_NET_SMTP_STATUS_PROCESSING;
				M_event_queue_task(sp->el, M_net_smtp_queue_advance_task, sp->queue);
			} else {
				sp->status = M_NET_SMTP_STATUS_IDLE;
			}
			return M_TRUE;
	}
	return M_FALSE; /* impossible */
}

M_net_smtp_status_t M_net_smtp_status(const M_net_smtp_t *sp)
{
	M_net_smtp_status_t status;

	if (sp == NULL)
		return M_NET_SMTP_STATUS_NOENDPOINTS;

	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	status = sp->status;
	M_thread_rwlock_unlock(sp->status_rwlock);

	return status;
}

void M_net_smtp_setup_tcp(M_net_smtp_t *sp, M_dns_t *dns, M_tls_clientctx_t *ctx)
{
	if (sp == NULL)
		return;

	sp->tcp_dns = dns;
	if (sp->tcp_tls_ctx != NULL)
		M_tls_clientctx_destroy(sp->tcp_tls_ctx);

	M_tls_clientctx_upref(ctx);
	sp->tcp_tls_ctx = ctx;
}

void M_net_smtp_setup_tcp_timeouts(M_net_smtp_t *sp, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 idle_ms)
{

	if (sp == NULL)
		return;

	sp->tcp_connect_ms = connect_ms;
	sp->tcp_stall_ms   = stall_ms;
	sp->tcp_idle_ms    = idle_ms;
}

M_bool M_net_smtp_add_endpoint_tcp(
	M_net_smtp_t *sp,
	const char   *address,
	M_uint16      port,
	M_bool        connect_tls,
	const char   *username,
	const char   *password,
	size_t        max_conns
)
{
	M_net_smtp_endpoint_t *ep = NULL;
	M_net_smtp_endpoint_tcp_args_t args = { address, port, connect_tls, username, password, max_conns };

	if (sp == NULL || max_conns == 0 || address == NULL || username == NULL || password == NULL || sp->tcp_dns == NULL)
		return M_FALSE;

	if (connect_tls && sp->tcp_tls_ctx == NULL)
		return M_FALSE;

	if (port == 0)
		port = 25;

	ep = M_net_smtp_endpoint_create_tcp(&args);
	add_endpoint(sp, ep);

	return M_TRUE;
}

M_bool M_net_smtp_add_endpoint_process(
	M_net_smtp_t        *sp,
	const char          *command,
	const M_list_str_t  *args,
	const M_hash_dict_t *env,
	M_uint64             timeout_ms,
	size_t               max_processes
)
{
	M_net_smtp_endpoint_t *ep = NULL;
	M_net_smtp_endpoint_proc_args_t proc_args = { command, args, env, timeout_ms, max_processes };

	if (sp == NULL || command == NULL)
		return M_FALSE;

	if (max_processes == 0)
		proc_args.max_processes = 1;

	ep = M_net_smtp_endpoint_create_proc(&proc_args);
	add_endpoint(sp, ep);

	return M_TRUE;
}

M_bool M_net_smtp_load_balance(M_net_smtp_t *sp, M_net_smtp_load_balance_t mode)
{
	if (sp == NULL)
		return M_FALSE;

	sp->load_balance_mode = mode;
	return M_TRUE;
}

void M_net_smtp_set_num_attempts(M_net_smtp_t *sp, size_t num)
{
	if (sp == NULL)
		return;
	M_net_smtp_queue_set_num_attempts(sp->queue, num);
}

M_bool M_net_smtp_queue_smtp(M_net_smtp_t *sp, const M_email_t *e)
{

	if (sp == NULL || e == NULL)
		return M_FALSE;
	return M_net_smtp_queue_smtp_int(sp->queue, e);
}

M_list_str_t *M_net_smtp_dump_queue(M_net_smtp_t *sp)
{
	if (sp == NULL)
		return NULL;
	return M_net_smtp_queue_dump(sp->queue);
}

M_bool M_net_smtp_queue_message(M_net_smtp_t *sp, const char *msg)
{
	if (sp == NULL || msg == NULL)
		return M_FALSE;
	return M_net_smtp_queue_message_int(sp->queue, msg);
}

M_bool M_net_smtp_use_external_queue(M_net_smtp_t *sp, char *(*get_cb)(void))
{
	if (sp == NULL || get_cb == NULL)
		return M_FALSE;
	return M_net_smtp_queue_use_external_queue(sp->queue, get_cb);
}

void M_net_smtp_external_queue_have_messages(M_net_smtp_t *sp)
{
	if (sp == NULL)
		return;
	M_net_smtp_queue_external_have_messages(sp->queue);
}
