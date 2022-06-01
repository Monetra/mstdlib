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

#include "m_net_int.h"
#include <mstdlib/base/m_defs.h> /* M_CAST_OFF_CONST */
#include <mstdlib/io/m_io_layer.h> /* M_io_layer_softevent_add (STARTTLS) */
#include "smtp/m_net_smtp_int.h" /* State machines & internal structure definitions */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_smtp {
	M_event_t                         *el;
	struct M_net_smtp_callbacks        cbs;
	void                              *thunk;
	M_list_t                          *endpoints;
	M_net_smtp_status_t                status;
	M_thread_rwlock_t                 *status_rwlock;
	size_t                             retry_default_ms;
	M_dns_t                           *tcp_dns;
	M_tls_clientctx_t                 *tcp_tls_ctx;
	M_uint64                           tcp_connect_ms;
	M_uint64                           tcp_stall_ms;
	M_uint64                           tcp_idle_ms;
	M_net_smtp_load_balance_t          load_balance_mode;
	size_t                             round_robin_idx;
	size_t                             max_number_of_attempts;
	M_list_t                          *retry_timers;
	M_list_t                          *retry_queue;
	M_list_t                          *retry_timeout_queue;
	M_thread_rwlock_t                 *retry_queue_rwlock;
	M_list_str_t                      *internal_queue;
	M_thread_rwlock_t                 *internal_queue_rwlock;
	M_bool                             is_external_queue_enabled;
	M_bool                             is_external_queue_pending;
	M_event_timer_t                   *restart_processing_timer;
	M_thread_mutex_t                  *endpoints_mutex;
	char *                           (*external_queue_get_cb)(void);
};

typedef struct {
	M_net_smtp_t    *sp;
	char            *msg;
	M_hash_dict_t   *headers;
	size_t           number_of_tries;
	M_event_timer_t *event_timer;
} retry_msg_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* forward declarations */
static void process_queue_queue(M_net_smtp_t *sp);
static void processing_halted(M_net_smtp_t *sp);
static void proc_io_stdin_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static int  process_external_queue_num(M_net_smtp_t *sp);
static void tcp_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static void session_destroy(M_net_smtp_endpoint_session_t *session);

static M_bool is_running(M_net_smtp_status_t status)
{
	return status == M_NET_SMTP_STATUS_PROCESSING || status == M_NET_SMTP_STATUS_IDLE;
}

static M_bool is_pending(M_net_smtp_t *sp)
{
	M_bool is_pending_internal;
	M_bool is_pending_retry;

	if (sp->is_external_queue_enabled)
		return sp->is_external_queue_pending;

	M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_pending_internal = M_list_str_len(sp->internal_queue) > 0;
	M_thread_rwlock_unlock(sp->internal_queue_rwlock);

	M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_pending_retry = M_list_len(sp->retry_queue) > 0;
	M_thread_rwlock_unlock(sp->retry_queue_rwlock);

	return is_pending_retry || is_pending_internal;
}

static M_bool is_endpoint_available(const M_net_smtp_endpoint_t *ep)
{
	return (ep->max_sessions - M_list_len(ep->send_sessions)) > 0;
}

static const M_net_smtp_endpoint_t *active_endpoint(M_net_smtp_t *sp)
{
	const M_net_smtp_endpoint_t *ep;
	size_t                       i;
	size_t                       idx;
	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			for (i = 0; i < M_list_len(sp->endpoints); i++) {
				ep = M_list_at(sp->endpoints, i);
				if (ep->is_removed)
					continue;
				if (is_endpoint_available(ep)) {
					return ep;
				} else {
					return NULL;
				}
			}
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			for (i = 0; i < M_list_len(sp->endpoints); i++) {
				idx = (sp->round_robin_idx + i) % M_list_len(sp->endpoints);
				ep = M_list_at(sp->endpoints, idx);
				if (!ep->is_removed && is_endpoint_available(ep))
					return ep;
			}
			break;
	}
	return NULL;
}

static void destroy_endpoint(M_net_smtp_endpoint_t *ep)
{
	M_net_smtp_endpoint_session_t *session;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	while (
		(session = M_list_take_last(ep->send_sessions)) != NULL ||
		(session = M_list_take_last(ep->idle_sessions)) != NULL
	) {
		session_destroy(session);
	}
	M_list_destroy(ep->send_sessions, M_TRUE);
	M_list_destroy(ep->idle_sessions, M_TRUE);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
	M_thread_rwlock_destroy(ep->sessions_rwlock);
	if (ep->type == M_NET_SMTP_EPTYPE_PROCESS) {
		M_free(ep->process.command);
		M_list_str_destroy(ep->process.args);
		M_hash_dict_destroy(ep->process.env);
	} else {
		/* TCP endpoint */
		M_free(ep->tcp.address);
		M_free(ep->tcp.username);
		M_free(ep->tcp.password);
	}
	M_free(ep);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* NOP callbacks */
static void nop_connect_cb(const char *a, M_uint16 b, void *c) { (void)a; (void)b; (void)c; }
static void nop_disconnect_cb(const char *a, M_uint16 b, void *c) { (void)a; (void)b; (void)c; }
static void nop_sent_cb(const M_hash_dict_t *a, void *b) { (void)a; (void)b; }
static void nop_reschedule_cb(const char *a, M_uint64 b, void *c) { (void)a; (void)b; (void)c; }
static M_bool nop_connect_fail_cb(const char *a, M_uint16 b, M_net_error_t c, const char *d, void *e)
{
	(void)a; (void)b; (void)c; (void)d; (void)e;
	return M_FALSE; /* Default is stop on tcp connection failure */
}
static M_bool nop_process_fail_cb(const char *a, int b, const char *c, const char *d, void *e)
{
	(void)a; (void)b; (void)c; (void)d; (void)e;
	return M_FALSE; /* Default is stop on process failure */
}
static M_uint64 nop_processing_halted_cb(M_bool a, void *b)
{
	(void)a; (void)b;
	return 0; /* Default is to stop on total failure */
}
static M_bool nop_send_failed_cb(const M_hash_dict_t *a, const char *b, size_t c, M_bool d, void *e)
{
	(void)a; (void)b; (void)c; (void)d; (void)e;
	return M_TRUE; /* Default is to retry msg */
}
static M_bool nop_iocreate_cb(M_io_t *a, char *b, size_t c, void *d)
{
	(void)a; (void)b; (void)c; (void)d;
	return M_TRUE; /* Goal was achieved! */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_smtp_t *M_net_smtp_create(M_event_t *el, const struct M_net_smtp_callbacks *cbs, void *thunk)
{
	M_net_smtp_t *sp;

	if (el == NULL)
		return NULL;

	sp                        = M_malloc_zero(sizeof(*sp));
	sp->internal_queue        = M_list_str_create(M_LIST_STR_NONE);
	sp->internal_queue_rwlock = M_thread_rwlock_create();
	sp->endpoints             = M_list_create(NULL, M_LIST_NONE);
	sp->retry_queue           = M_list_create(NULL, M_LIST_NONE);
	sp->retry_timeout_queue   = M_list_create(NULL, M_LIST_NONE);
	sp->retry_timers          = M_list_create(NULL, M_LIST_NONE);
	sp->retry_queue_rwlock    = M_thread_rwlock_create();
	sp->status_rwlock         = M_thread_rwlock_create();
	sp->endpoints_mutex       = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

#define ASSIGN_CB(x) sp->cbs.x = cbs->x ? cbs->x : nop_##x;
	ASSIGN_CB(connect_cb);
	ASSIGN_CB(connect_fail_cb);
	ASSIGN_CB(disconnect_cb);
	ASSIGN_CB(process_fail_cb);
	ASSIGN_CB(processing_halted_cb);
	ASSIGN_CB(sent_cb);
	ASSIGN_CB(send_failed_cb);
	ASSIGN_CB(reschedule_cb);
	ASSIGN_CB(iocreate_cb);
#undef ASSIGN_CB

	sp->el                     = el;
	sp->thunk                  = thunk;
	sp->status                 = M_NET_SMTP_STATUS_NOENDPOINTS;
	/* defaults */
	sp->max_number_of_attempts = 3;
	sp->retry_default_ms       = 300000;
	sp->tcp_connect_ms         = 5000;
	sp->tcp_stall_ms           = 5000;
	sp->tcp_idle_ms            = 1000;

	return sp;
}

void M_net_smtp_destroy(M_net_smtp_t *sp)
{
	M_net_smtp_endpoint_t *ep    = NULL;
	M_event_timer_t       *timer = NULL;
	retry_msg_t           *retry = NULL;

	if (sp == NULL)
		return;

	while ((ep = M_list_take_last(sp->endpoints)) != NULL) {
		destroy_endpoint(ep);
	}
	M_list_destroy(sp->endpoints, M_TRUE);
	M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	while ((timer = M_list_take_last(sp->retry_timers)) != NULL) {
		M_event_timer_remove(timer);
	}
	M_list_destroy(sp->retry_timers, M_TRUE);
	M_event_timer_remove(sp->restart_processing_timer);
	while (
		(retry = M_list_take_last(sp->retry_queue))         != NULL ||
		(retry = M_list_take_last(sp->retry_timeout_queue)) != NULL
	) {
		M_free(retry->msg);
		M_free(retry);
	}
	M_thread_rwlock_unlock(sp->retry_queue_rwlock);
	M_list_destroy(sp->retry_queue, M_TRUE);
	M_list_destroy(sp->retry_timeout_queue, M_TRUE);
	M_thread_rwlock_destroy(sp->retry_queue_rwlock);
	M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_str_destroy(sp->internal_queue);
	M_thread_rwlock_unlock(sp->internal_queue_rwlock);
	M_thread_rwlock_destroy(sp->internal_queue_rwlock);
	M_thread_rwlock_destroy(sp->status_rwlock);
	M_tls_clientctx_destroy(sp->tcp_tls_ctx);
	M_free(sp);
}

void M_net_smtp_pause(M_net_smtp_t *sp)
{

	if (sp == NULL)
		return;


	if (!is_running(M_net_smtp_status(sp)))
		return;

	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (sp->status == M_NET_SMTP_STATUS_IDLE) {
		processing_halted(sp);
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPING;
	}
	M_thread_rwlock_unlock(sp->status_rwlock);
}

static void reschedule_event_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	retry_msg_t         *retry = thunk;
	M_net_smtp_t        *sp    = retry->sp;
	M_net_smtp_status_t  status;
	(void)el;
	(void)io;
	(void)etype;

	M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_event_timer_remove(retry->event_timer);
	M_list_remove_val(sp->retry_timers, retry->event_timer, M_LIST_MATCH_PTR);
	retry->event_timer = NULL;
	M_list_remove_val(sp->retry_timeout_queue, retry, M_LIST_MATCH_PTR);
	M_list_insert(sp->retry_queue, retry);
	M_thread_rwlock_unlock(sp->retry_queue_rwlock);
	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		process_queue_queue(sp);
}

static void reschedule_msg(M_net_smtp_t *sp, const char *msg, const M_hash_dict_t *headers, M_bool is_backout, size_t num_tries, const char* errmsg, size_t retry_ms)
{
	retry_msg_t               *retry          = NULL;
	M_bool                     is_requeue     = M_FALSE;
	M_net_smtp_send_failed_cb  send_failed_cb = sp->cbs.send_failed_cb;
	M_net_smtp_reschedule_cb   reschedule_cb  = sp->cbs.reschedule_cb;

	if (sp->is_external_queue_enabled) {
		if (is_backout) {
			/* Can retry immediately - the failure was with the endpoint */
			reschedule_cb(msg, 0, sp->thunk);
			return;
		}
		reschedule_cb(msg, (retry_ms / 1000), sp->thunk); /* reschedule_cb arg is wait_sec */
		send_failed_cb(headers, errmsg, 0, M_FALSE, sp->thunk);
		return;
	}

	if (is_backout) {
		if (num_tries == 1) {
			M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
			M_list_str_insert(sp->internal_queue, msg);
			M_thread_rwlock_unlock(sp->internal_queue_rwlock);
		} else {
			/* need to keep track of num_tries */
			retry = M_malloc_zero(sizeof(*retry));
			retry->sp = sp;
			retry->msg = M_strdup(msg);
			retry->number_of_tries = num_tries - 1;
			retry->headers = M_hash_dict_duplicate(headers);
			retry->event_timer = NULL;
			M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
			M_list_insert(sp->retry_queue, retry);
			M_thread_rwlock_unlock(sp->retry_queue_rwlock);
		}
		return;
	}

	if (num_tries < sp->max_number_of_attempts) {
		is_requeue = send_failed_cb(headers, errmsg, num_tries, M_TRUE, sp->thunk);
	} else {
		send_failed_cb(headers, errmsg, num_tries, M_FALSE, sp->thunk);
	}

	if (is_requeue) {
		retry = M_malloc_zero(sizeof(*retry));
		retry->sp = sp;
		retry->msg = M_strdup(msg);
		retry->number_of_tries = num_tries;
		retry->headers = M_hash_dict_duplicate(headers);
		M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		retry->event_timer = M_event_timer_oneshot(sp->el, retry_ms, M_FALSE,
				reschedule_event_cb, retry);
		M_list_insert(sp->retry_timeout_queue, retry);
		M_list_insert(sp->retry_timers, retry->event_timer);
		M_thread_rwlock_unlock(sp->retry_queue_rwlock);
	}
}

static M_bool run_state_machine(M_net_smtp_endpoint_session_t *session, M_bool *is_done)
{
	M_state_machine_status_t result;

	result = M_state_machine_run(session->state_machine, session);
	if (result == M_STATE_MACHINE_STATUS_WAIT) {
		if (is_done != NULL) {
			*is_done = M_FALSE;
		}
		return M_TRUE;
	}
	if (result == M_STATE_MACHINE_STATUS_DONE) {
		if (is_done != NULL) {
			*is_done = M_TRUE;
		}
		return M_TRUE;
	}

	if (session->errmsg[0] == 0) {
		M_snprintf(session->errmsg, sizeof(session->errmsg), "State machine failure: %d", result);
	}
	return M_FALSE;
}

static void round_robin_skip_removed(M_net_smtp_t *sp)
{
	size_t i;
	size_t len = M_list_len(sp->endpoints);

	for (i = 0; i < len; i++) {
		size_t                       idx = (sp->round_robin_idx + i) % len;
		const M_net_smtp_endpoint_t *ep  = M_list_at(sp->endpoints, idx);
		if (ep->is_removed)
			continue;
		sp->round_robin_idx = idx;
		break;
	}
}

static void remove_endpoint(M_net_smtp_t *sp, M_net_smtp_endpoint_t *ep)
{
	size_t                       i;
	M_bool                       is_all_endpoints_removed = M_TRUE;
	const M_net_smtp_endpoint_t *const_ep                 = NULL;

	ep->is_removed = M_TRUE;
	for (i = 0; i < M_list_len(sp->endpoints); i++) {
		const_ep = M_list_at(sp->endpoints, i);
		if (const_ep->is_removed == M_FALSE) {
			is_all_endpoints_removed = M_FALSE;
			break;
		}
	}

	if (is_all_endpoints_removed) {
		M_net_smtp_status_t status;
		M_net_smtp_pause(sp);
		status = M_net_smtp_status(sp);
		if (status == M_NET_SMTP_STATUS_STOPPING) {
			/* need to check if idle then process_halted() */
			process_queue_queue(sp);
		}
		return;
	}

	if (sp->load_balance_mode == M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN) {
		round_robin_skip_removed(sp);
	}

}

static void rotate_endpoints(M_net_smtp_t *sp)
{
	M_net_smtp_endpoint_t *ep;

	if (sp->load_balance_mode != M_NET_SMTP_LOAD_BALANCE_FAILOVER)
		return;

	if (M_list_len(sp->endpoints) <= 1)
		return;

	ep = M_list_take_first(sp->endpoints);
	M_list_insert(sp->endpoints, ep);
}

static void process_fail(M_net_smtp_endpoint_session_t *session, const char *stdout_str)
{
	const M_net_smtp_endpoint_t *const_ep = session->ep;

	if (session->sp->cbs.process_fail_cb(
		const_ep->process.command,
		session->process.result_code,
		stdout_str,
		session->errmsg,
		session->sp->thunk)
	) {
		remove_endpoint(session->sp, M_CAST_OFF_CONST(M_net_smtp_endpoint_t*, const_ep));
	} else {
		/* Had a failure, but they want to keep the endpoint. */
		rotate_endpoints(session->sp);
	}
}

static void clean_session(M_net_smtp_endpoint_session_t *session)
{
	M_net_smtp_endpoint_t *ep = M_CAST_OFF_CONST(M_net_smtp_endpoint_t*, session->ep);

	if (session->msg == NULL)
		return;

	if (session->is_backout || !session->is_successfully_sent) {
		reschedule_msg(session->sp, session->msg, session->headers, session->is_backout, session->number_of_tries + 1, session->errmsg, session->retry_ms);
	} else {
		session->sp->cbs.sent_cb(session->headers, session->sp->thunk);
	}
	M_email_destroy(session->email);
	session->email = NULL;
	M_free(session->msg);
	session->msg = NULL;
	session->headers = NULL;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_remove_val(ep->send_sessions, session, M_LIST_MATCH_PTR);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

static void continue_processing_after_finish(M_net_smtp_t *sp)
{
	if (!sp->is_external_queue_enabled) {
		process_queue_queue(sp);
		return;
	}
	/* eager eval external queue to determine IDLE */
	if (process_external_queue_num(sp) == 0)
		process_queue_queue(sp);
}

static void session_destroy(M_net_smtp_endpoint_session_t *session)
{
	clean_session(session);
	M_event_timer_remove(session->event_timer);
	session->event_timer = NULL;
	M_buf_cancel(session->out_buf);
	session->out_buf = NULL;
	M_parser_destroy(session->in_parser);
	session->in_parser = NULL;
	M_state_machine_destroy(session->state_machine);
	session->state_machine = NULL;
	session->is_alive = M_FALSE;
	M_thread_mutex_destroy(session->mutex);
	M_thread_rwlock_lock(session->ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_remove_val(session->ep->idle_sessions, session, M_LIST_MATCH_PTR);
	M_thread_rwlock_unlock(session->ep->sessions_rwlock);
	M_free(session);
}

/* Return value is whether to run continue processing another
	* message.  process_queue_queue() can potentially destroy
	* the endpoint + sessions + session locks, which will seg fault the unlock
	* wrapped around this function. */
static M_bool proc_io_cb_sub(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask, M_bool *is_destroy_session)
{
	M_net_smtp_endpoint_session_t  *session    = thunk;
	M_io_error_t                    io_error   = M_IO_ERROR_SUCCESS;
	M_io_t                        **session_io = NULL;
	size_t                          len;
	M_bool                          is_done;
	(void)el;

	switch(etype) {
		case M_EVENT_TYPE_CONNECTED:
			session->connection_mask |= connection_mask;
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			if (io == session->io) {
				if (!M_io_process_get_result_code(io, &session->process.result_code)) {
					session->is_successfully_sent = M_FALSE;
					if (session->errmsg[0] == 0) {
						M_snprintf(session->errmsg, sizeof(session->errmsg), "Error getting result code");
					}
				}
				if (session->process.result_code != 0) {
					char *stdout_str = M_buf_finish_str(session->out_buf, NULL);
					session->out_buf = NULL;
					process_fail(session, stdout_str);
					M_free(stdout_str);
					session->is_successfully_sent = M_FALSE;
					if (session->errmsg[0] == 0) {
						M_snprintf(session->errmsg, sizeof(session->errmsg), "Bad result code %d", session->process.result_code);
					}
				}
			}
			goto destroy;
			break;
		case M_EVENT_TYPE_READ:
			if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDERR) {
				io_error = M_io_read(io, (unsigned char *)session->errmsg, sizeof(session->errmsg) - 1, &len);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Read failure: %s", M_io_error_string(io_error));
					goto destroy;
				}
				session->errmsg[len] = '\0';
				goto destroy;
			}
			if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) {
				io_error = M_io_read_into_parser(io, session->in_parser);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Read failure: %s", M_io_error_string(io_error));
				}
				goto destroy; /* shouldn't receive anything on stdout */
			}
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_WRITE:
			if (connection_mask != M_NET_SMTP_CONNECTION_MASK_IO_STDIN) {
				M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
				goto destroy;
			}
			if (M_buf_len(session->out_buf) > 0) {
				io_error = M_io_write_from_buf(io, session->out_buf);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (!session->is_successfully_sent) {
					/* Give process a chance to parse and react to input */
					session->event_timer = M_event_timer_oneshot(session->sp->el, 100, M_TRUE, proc_io_stdin_cb, session);
				} else {
					session->event_timer = NULL;
				}
			}
			if (session->is_successfully_sent) {
				M_io_disconnect(io);
			}
			return M_FALSE;
		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_ACCEPT:
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_OTHER:
			break;
	}

	if (!run_state_machine(session, &is_done) || is_done)
		goto destroy;

	if (M_buf_len(session->out_buf) > 0) {
		M_io_layer_t *layer;
		layer = M_io_layer_acquire(session->process.io_stdin, 0, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	}

	return M_FALSE;
destroy:
	switch (connection_mask) {
		case M_NET_SMTP_CONNECTION_MASK_IO:        session_io = &session->io;                break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDIN:  session_io = &session->process.io_stdin;  break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDOUT: session_io = &session->process.io_stdout; break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDERR: session_io = &session->process.io_stderr; break;
	}
	if (*session_io != NULL) {
		M_io_destroy(io);
		*session_io = NULL;
		session->connection_mask &= ~connection_mask;
		if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			*is_destroy_session = M_TRUE;
			return M_TRUE;
		}
	}
	return M_FALSE;
}

static void proc_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask)
{
	M_net_smtp_endpoint_session_t *session                = thunk;
	M_net_smtp_t                  *sp                     = session->sp;
	M_bool                         is_continue_processing = M_FALSE;
	M_bool                         is_destroy_session     = M_FALSE;
	M_thread_mutex_lock(session->mutex);
	is_continue_processing = proc_io_cb_sub(el, etype, io, thunk, connection_mask, &is_destroy_session);
	M_thread_mutex_unlock(session->mutex);
	if (is_destroy_session)
		session_destroy(session);
	if (is_continue_processing)
		continue_processing_after_finish(sp);
}

static void proc_io_stderr_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	return proc_io_cb(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO_STDERR);
}

static void proc_io_stdout_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	return proc_io_cb(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO_STDOUT);
}

static void proc_io_stdin_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	return proc_io_cb(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO_STDIN);
}

static void proc_io_proc_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	return proc_io_cb(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO);
}


static void connect_fail(M_net_smtp_endpoint_session_t *session)
{
	M_net_smtp_t                 *sp       = session->sp;
	const M_net_smtp_endpoint_t  *const_ep = session->ep;

	if (sp->cbs.connect_fail_cb(
		const_ep->tcp.address,
		const_ep->tcp.port,
		session->tcp.net_error,
		session->errmsg,
		sp->thunk
	)) {
		remove_endpoint(sp, M_CAST_OFF_CONST(M_net_smtp_endpoint_t *, const_ep));
	} else {
		/* Had a failure, but they want to keep the endpoint. */
		rotate_endpoints(sp);
	}
}

/* Return value is whether to run continue processing another
	* message.  process_queue_queue() can potentially destroy
	* the endpoint + sessions + session locks, which will seg fault the unlock
	* wrapped around this function. */
static M_bool tcp_io_cb_sub(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, M_bool *is_session_destroy)
{
	M_net_smtp_endpoint_session_t  *session     = thunk;
	M_net_smtp_t                   *sp          = session->sp;
	const M_net_smtp_endpoint_t    *const_ep    = session->ep;
	M_net_smtp_connect_cb           connect_cb  = sp->cbs.connect_cb;
	M_net_smtp_iocreate_cb          iocreate_cb = sp->cbs.iocreate_cb;
	M_io_error_t                    io_error    = M_IO_ERROR_SUCCESS;
	M_bool                          is_done     = M_FALSE;
	M_net_smtp_status_t             status;
	(void)el;

	switch(etype) {
		case M_EVENT_TYPE_CONNECTED:

			if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
				/* STARTTLS has 2 CONNECTED events, only want to cb() once. */
				session->connection_mask |= M_NET_SMTP_CONNECTION_MASK_IO;
				/* Sending iocreate here ensures we trace on the right one. */
				if (!iocreate_cb(io, session->errmsg, sizeof(session->errmsg), sp->thunk)) {
					session->is_backout = M_TRUE;
					goto destroy; /* Skip connect_fail call */
				}
				connect_cb(const_ep->tcp.address, const_ep->tcp.port, sp->thunk);
				M_event_timer_reset(session->event_timer, sp->tcp_stall_ms);
			}

			if (session->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_ADDED || session->tcp.tls_state == M_NET_SMTP_TLS_IMPLICIT) {
				session->tcp.tls_state = M_NET_SMTP_TLS_CONNECTED;
				return M_FALSE;
			}

			break;
		case M_EVENT_TYPE_DISCONNECTED:
			goto destroy;
			break;
		case M_EVENT_TYPE_READ:
			io_error = M_io_read_into_parser(io, session->in_parser);

			if (io_error == M_IO_ERROR_WOULDBLOCK)
				return M_FALSE;

			if (io_error == M_IO_ERROR_DISCONNECT) {
				goto destroy;
			}

			if (io_error != M_IO_ERROR_SUCCESS) {
				M_snprintf(session->errmsg, sizeof(session->errmsg), "Read failure: %s", M_io_error_string(io_error));
				goto destroy;
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_ACCEPT:
			/* should be impossible */
			session->tcp.net_error = M_NET_ERROR_PROTONOTSUPPORTED;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unsupported ACCEPT event");
			goto backout;
			break;
		case M_EVENT_TYPE_OTHER:
			if (session->is_successfully_sent) {
				/* Idle timeout */
				session->tcp.is_QUIT_enabled = M_TRUE;
				break;
			}
		case M_EVENT_TYPE_ERROR:
			if (session->tcp.tls_state == M_NET_SMTP_TLS_IMPLICIT && session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
				/* Implict TLS failed.  Follwup with with STARTTLS */
				session->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS;
				M_io_destroy(io);
				io_error = M_io_net_client_create(&session->io, sp->tcp_dns, const_ep->tcp.address,
						const_ep->tcp.port, M_IO_NET_ANY);
				if (io_error != M_IO_ERROR_SUCCESS) {
					goto destroy;
				}
				M_event_add(sp->el, session->io, tcp_io_cb, session);
				M_event_timer_reset(session->event_timer, sp->tcp_connect_ms);
				return M_FALSE;
			}
			do {
				if (etype == M_EVENT_TYPE_OTHER) {
					if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
						session->tcp.net_error = M_NET_ERROR_TIMEOUT;
						M_snprintf(session->errmsg, sizeof(session->errmsg), "Connection timeout");
						break;
					}
					session->tcp.net_error = M_NET_ERROR_TIMEOUT_STALL;
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Stall timeout");
					break;
				}
				M_io_get_error_string(io, session->errmsg, sizeof(session->errmsg));
				session->tcp.net_error = M_net_io_error_to_net_error(M_io_get_error(io));
			} while(0);
			goto backout;
			return M_FALSE;
	}

	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_STOPPING)
		session->tcp.is_QUIT_enabled = M_TRUE;

	if (!run_state_machine(session, &is_done) || is_done) {
		if (session->tcp.is_connect_fail) {
			goto backout;
		}
		goto destroy;
	}

	if (session->is_successfully_sent && session->msg != NULL) {
		/* get ready to accept another. */
		clean_session(session);
		M_thread_rwlock_lock(session->ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		M_list_insert(session->ep->idle_sessions, session);
		M_thread_rwlock_unlock(session->ep->sessions_rwlock);
		M_event_timer_reset(session->event_timer, sp->tcp_idle_ms);
		return M_TRUE;
	}

	if (session->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_READY) {
		size_t        layer_id;
		M_io_layer_t *layer;
		M_io_tls_client_add(io, session->sp->tcp_tls_ctx, NULL, &layer_id);
		layer = M_io_layer_acquire(io, layer_id, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
		session->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS_ADDED;
		return M_FALSE; /* short circuit out */
	}

	if (M_buf_len(session->out_buf) > 0) {
		io_error = M_io_write_from_buf(io, session->out_buf);
		if (io_error == M_IO_ERROR_DISCONNECT) {
			goto destroy;
		}
		if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Write failed: %s", M_io_error_string(io_error));
			goto destroy;
		}
	}

	return M_FALSE;
backout:
	connect_fail(session);
	session->is_backout = M_TRUE;
destroy:
	if (session->io != NULL) {
		M_io_destroy(io);
		session->connection_mask &= ~M_NET_SMTP_CONNECTION_MASK_IO;
		if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			M_bool is_backout = session->is_backout;
			*is_session_destroy = M_TRUE;
			if (is_backout == M_FALSE) {
				sp->cbs.disconnect_cb(const_ep->tcp.address, const_ep->tcp.port, sp->thunk);
			}
		}
		session->io = NULL;
		return M_TRUE;
	}
	return M_FALSE;
}

static void tcp_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_endpoint_session_t *session                = thunk;
	M_net_smtp_t                  *sp                     = session->sp;
	M_bool                         is_continue_processing = M_FALSE;
	M_bool                         is_session_destroy     = M_FALSE;
	M_thread_mutex_lock(session->mutex);
	is_continue_processing = tcp_io_cb_sub(el, etype, io, thunk, &is_session_destroy);
	M_thread_mutex_unlock(session->mutex);

	if (is_session_destroy)
			session_destroy(session);

	if (is_continue_processing)
		continue_processing_after_finish(sp);
}

static M_net_smtp_endpoint_session_t *session_create(M_net_smtp_t *sp, const M_net_smtp_endpoint_t* ep)
{
	M_io_error_t                   io_error = M_IO_ERROR_SUCCESS;
	M_net_smtp_endpoint_session_t *session  = NULL;

	session = M_malloc_zero(sizeof(*session));
	session->sp = sp;
	session->ep = ep;
	session->mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	if (ep->type == M_NET_SMTP_EPTYPE_PROCESS) {
		io_error = M_io_process_create(
			ep->process.command,
			ep->process.args,
			ep->process.env,
			ep->process.timeout_ms,
			&session->io,
			&session->process.io_stdin,
			&session->process.io_stdout,
			&session->process.io_stderr
		);
		if (io_error != M_IO_ERROR_SUCCESS) {
			session->process.result_code = (int)io_error;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "%s", M_io_error_string(io_error));
			process_fail(session, "");
			goto fail;
		}
		session->state_machine = M_net_smtp_flow_process();
		M_event_add(sp->el, session->io               , proc_io_proc_cb  , session);
		M_event_add(sp->el, session->process.io_stdin , proc_io_stdin_cb , session);
		M_event_add(sp->el, session->process.io_stdout, proc_io_stdout_cb, session);
		M_event_add(sp->el, session->process.io_stderr, proc_io_stderr_cb, session);
	} else {
		io_error = M_io_net_client_create(&session->io, sp->tcp_dns, ep->tcp.address, ep->tcp.port, M_IO_NET_ANY);
		if (io_error != M_IO_ERROR_SUCCESS)
			goto fail;

		if (ep->tcp.connect_tls) {
			io_error = M_io_tls_client_add(session->io, session->sp->tcp_tls_ctx, NULL, NULL);
			if (io_error != M_IO_ERROR_SUCCESS) {
				M_io_destroy(session->io);
				session->io = NULL;
				goto fail;
			}
			session->tcp.tls_state = M_NET_SMTP_TLS_IMPLICIT;
		}

		session->state_machine        = M_net_smtp_flow_tcp();
		M_event_add(sp->el, session->io, tcp_io_cb, session);
		session->event_timer          = M_event_timer_add(sp->el, tcp_io_cb, session);
		M_event_timer_start(session->event_timer, sp->tcp_connect_ms);
	}

	session->out_buf   = M_buf_create();
	session->in_parser = M_parser_create(M_PARSER_FLAG_NONE);
	session->is_alive  = M_TRUE;
	return session;
fail:
	M_event_timer_remove(session->event_timer);
	M_parser_destroy(session->in_parser);
	M_buf_cancel(session->out_buf);
	M_thread_mutex_destroy(session->mutex);
	M_free(session);
	return NULL;
}

static void start_sendmsg_task(M_net_smtp_t *sp, const M_net_smtp_endpoint_t* const_ep, char *msg, size_t num_tries)
{
	const M_hash_dict_t           *headers      = NULL;
	M_net_smtp_endpoint_t         *ep           = M_CAST_OFF_CONST(M_net_smtp_endpoint_t *,const_ep);
	M_net_smtp_endpoint_session_t *session      = NULL;
	M_email_t                     *e            = NULL;
	M_bool                         is_bootstrap = M_FALSE;

	if (M_email_simple_read(&e, msg, M_str_len(msg), M_EMAIL_SIMPLE_READ_NONE, NULL) != M_EMAIL_ERROR_SUCCESS) {
		M_bool is_retrying = M_FALSE;
		num_tries = sp->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(NULL, msg, num_tries, is_retrying, sp->thunk);
		M_email_destroy(e);
		M_free(msg);
		continue_processing_after_finish(sp);
		return;
	}

	headers = M_email_headers(e);

	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	session = M_list_take_first(ep->idle_sessions);
	if (session == NULL) {
		session = session_create(sp, ep);
		if (session == NULL) {
			reschedule_msg(sp, msg, headers, M_TRUE, num_tries + 1, "Failure creating session", sp->retry_default_ms);
			M_email_destroy(e);
			M_free(msg);
			M_thread_rwlock_unlock(ep->sessions_rwlock);
			return;
		}
		is_bootstrap = M_TRUE;
	}
	M_thread_mutex_lock(session->mutex);
	session->ep = ep;
	session->msg = msg;
	session->number_of_tries = num_tries;
	session->headers = headers;
	session->is_successfully_sent = M_FALSE;
	session->is_backout = M_FALSE;
	session->retry_ms = sp->retry_default_ms;
	M_mem_set(session->errmsg, 0, sizeof(session->errmsg));
	session->email = e;
	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		session->tcp.is_QUIT_enabled = (sp->tcp_idle_ms == 0);
		if (!is_bootstrap) {
			M_bool is_session_destroy = M_FALSE;
			/* Use sub because we already have a lock */
			tcp_io_cb_sub(session->sp->el, M_EVENT_TYPE_WRITE, session->io, session, &is_session_destroy);
		}
	}
	M_list_insert(ep->send_sessions, session);
	M_thread_mutex_unlock(session->mutex);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

static int process_external_queue_num(M_net_smtp_t *sp)
{
	const M_net_smtp_endpoint_t *ep = NULL;
	int                          n  = 0;

	M_thread_mutex_lock(sp->endpoints_mutex);
	while ((ep = active_endpoint(sp)) != NULL) {
		char *msg = sp->external_queue_get_cb();
		if (msg == NULL) {
			sp->is_external_queue_pending = M_FALSE;
			return n;
		}

		start_sendmsg_task(sp, ep, msg, 0);
		n++;
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
	return n;
}

static void process_external_queue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	(void)event;
	(void)io;
	(void)type;

	process_external_queue_num(cb_arg);
}

static void process_internal_queues(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t                *sp = cb_arg;
	const M_net_smtp_endpoint_t *ep = NULL;

	(void)event;
	(void)io;
	(void)type;

	M_thread_mutex_lock(sp->endpoints_mutex);
	while ((ep = active_endpoint(sp)) != NULL) {
		retry_msg_t *retry = NULL;
		char        *msg   = NULL;
		M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		retry = M_list_take_first(sp->retry_queue);
		M_thread_rwlock_unlock(sp->retry_queue_rwlock);
		if (retry != NULL) {
			start_sendmsg_task(sp, ep, retry->msg, retry->number_of_tries);
			M_free(retry);
			continue;
		}
		M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		msg = M_list_str_take_first(sp->internal_queue);
		M_thread_rwlock_unlock(sp->internal_queue_rwlock);
		if (msg != NULL) {
			start_sendmsg_task(sp, ep, msg, 0);
			continue;
		}
		break;
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
}

static M_bool idle_check_endpoint(const M_net_smtp_endpoint_t *ep)
{
	return (M_list_len(ep->send_sessions) == 0);
}

static M_bool idle_check(M_net_smtp_t *sp)
{
	size_t i;
	for (i = 0; i < M_list_len(sp->endpoints); i++) {
		if (idle_check_endpoint(M_list_at(sp->endpoints, i)) == M_FALSE)
			return M_FALSE;
	}
	return M_TRUE;
}

static void restart_processing_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_t *sp = thunk;
	(void)el;
	(void)etype;
	(void)io;

	M_event_timer_remove(sp->restart_processing_timer);
	sp->restart_processing_timer = NULL;
	M_net_smtp_resume(sp);
}

static void prune_removed_endpoints(M_net_smtp_t *sp)
{
	size_t i;
	for (i = M_list_len(sp->endpoints); i > 0; i--) {
		const M_net_smtp_endpoint_t *ep = M_list_at(sp->endpoints, i - 1);
		if (ep->is_removed) {
			destroy_endpoint(M_list_take_at(sp->endpoints, i - 1));
		}
	}
}

static void processing_halted(M_net_smtp_t *sp)
{
	M_bool                       is_no_endpoints = M_TRUE;
	const M_net_smtp_endpoint_t *ep              = NULL;
	M_uint64                     delay_ms;
	size_t                       i;

	for (i = 0; i < M_list_len(sp->endpoints); i++) {
		ep = M_list_at(sp->endpoints, i);
		if (!ep->is_removed) {
			is_no_endpoints = M_FALSE;
			break;
		}
	}

	if (is_no_endpoints) {
		sp->status = M_NET_SMTP_STATUS_NOENDPOINTS;
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
	}

	delay_ms = sp->cbs.processing_halted_cb(is_no_endpoints, sp->thunk);
	if (delay_ms == 0 || is_no_endpoints)
		return;

	sp->restart_processing_timer = M_event_timer_oneshot(sp->el, delay_ms, M_FALSE, restart_processing_cb, sp);
}

static void process_queue_queue(M_net_smtp_t *sp)
{
	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (is_running(sp->status) && is_pending(sp)) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		if (sp->is_external_queue_enabled) {
			M_event_queue_task(sp->el, process_external_queue, sp);
		} else {
			M_event_queue_task(sp->el, process_internal_queues, sp);
		}
	} else if (idle_check(sp)) {
		if (sp->status == M_NET_SMTP_STATUS_STOPPING) {
			processing_halted(sp);
		} else {
			sp->status = M_NET_SMTP_STATUS_IDLE;
			prune_removed_endpoints(sp);
		}
	}
	M_thread_rwlock_unlock(sp->status_rwlock);
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
		case M_NET_SMTP_STATUS_STOPPING:
			/* Actually, we're not stopping */
		case M_NET_SMTP_STATUS_STOPPED:
			prune_removed_endpoints(sp); /* Prune any removed endpoints before starting again */
			sp->status = M_NET_SMTP_STATUS_PROCESSING;
			process_queue_queue(sp);
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
	M_net_smtp_endpoint_t *ep         = NULL;
	M_net_smtp_status_t    status;

	if (sp == NULL || max_conns == 0 || address == NULL || username == NULL || password == NULL || sp->tcp_dns == NULL)
		return M_FALSE;

	if (connect_tls && sp->tcp_tls_ctx == NULL)
		return M_FALSE;

	if (port == 0)
		port = 25;

	ep                        = M_malloc_zero(sizeof(*ep));
	ep->type                  = M_NET_SMTP_EPTYPE_TCP;
	ep->max_sessions          = max_conns;
	ep->send_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->idle_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->sessions_rwlock       = M_thread_rwlock_create();
	ep->tcp.address           = M_strdup(address);
	ep->tcp.username          = M_strdup(username);
	ep->tcp.password          = M_strdup(password);
	ep->tcp.port              = port;
	ep->tcp.connect_tls       = connect_tls;

	M_list_insert(sp->endpoints, ep);

	status = M_net_smtp_status(sp);

	if (status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		sp->status = M_NET_SMTP_STATUS_STOPPED;
		M_thread_rwlock_unlock(sp->status_rwlock);
	}
	M_net_smtp_resume(sp);

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
	M_net_smtp_endpoint_t *ep         = NULL;
	M_net_smtp_status_t    status;

	if (sp == NULL || command == NULL)
		return M_FALSE;

	if (max_processes == 0)
		max_processes = 1;

	ep                        = M_malloc_zero(sizeof(*ep));
	ep->type                  = M_NET_SMTP_EPTYPE_PROCESS;
	ep->send_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->idle_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->sessions_rwlock       = M_thread_rwlock_create();
	ep->max_sessions          = max_processes;
	ep->process.command       = M_strdup(command);
	ep->process.args          = M_list_str_duplicate(args);
	ep->process.env           = M_hash_dict_duplicate(env);
	ep->process.timeout_ms    = timeout_ms;

	M_list_insert(sp->endpoints, ep);

	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		sp->status = M_NET_SMTP_STATUS_STOPPED;
		M_thread_rwlock_unlock(sp->status_rwlock);
	}

	M_net_smtp_resume(sp);

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
	sp->max_number_of_attempts = num;
}

M_list_str_t *M_net_smtp_dump_queue(M_net_smtp_t *sp)
{
	M_list_str_t    *list;
	char            *msg;
	retry_msg_t     *retry;
	M_event_timer_t *timer;

	if (sp == NULL)
		return NULL;

	if (sp->is_external_queue_enabled) {
		list = M_list_str_create(M_LIST_STR_NONE);
		while ((msg = sp->external_queue_get_cb()) != NULL) {
			M_list_str_insert(list, msg);
			M_free(msg);
		}
		return list;
	}

	list = sp->internal_queue;
	M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	sp->internal_queue = M_list_str_create(M_LIST_STR_NONE);
	M_thread_rwlock_unlock(sp->internal_queue_rwlock);

	M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	while (
		(retry = M_list_take_first(sp->retry_queue))         != NULL ||
		(retry = M_list_take_first(sp->retry_timeout_queue)) != NULL
	) {
		M_list_str_insert(list, retry->msg);
		M_free(retry->msg);
		M_free(retry);
	}
	M_thread_rwlock_unlock(sp->retry_queue_rwlock);

	while ((timer = M_list_take_first(sp->retry_timers)) != NULL) {
		M_event_timer_remove(timer);
	}

	return list;
}

M_bool M_net_smtp_queue_smtp(M_net_smtp_t *sp, const M_email_t *e)
{
	char   *msg;
	M_bool  is_success;

	if (sp == NULL || sp->is_external_queue_enabled) 
		return M_FALSE;

	if (!(msg = M_email_simple_write(e))) { return M_FALSE; }
	is_success = M_net_smtp_queue_message(sp, msg);
	M_free(msg);

	return is_success;
}

M_bool M_net_smtp_queue_message(M_net_smtp_t *sp, const char *msg)
{
	M_bool              is_success;
	M_net_smtp_status_t status;
	if (sp == NULL || sp->is_external_queue_enabled)
		return M_FALSE;

	M_thread_rwlock_lock(sp->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	is_success = M_list_str_insert(sp->internal_queue, msg);
	M_thread_rwlock_unlock(sp->internal_queue_rwlock);

	if (!is_success)
		return M_FALSE;

	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		process_queue_queue(sp);

	return M_TRUE;
}

M_bool M_net_smtp_use_external_queue(M_net_smtp_t *sp, char *(*get_cb)(void))
{
	M_bool is_retry_timeout_queue_pending;

	if (sp == NULL || get_cb == NULL)
		return M_FALSE;

	M_thread_rwlock_lock(sp->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_retry_timeout_queue_pending = M_list_len(sp->retry_timeout_queue) > 0;
	M_thread_rwlock_unlock(sp->retry_queue_rwlock);
	if (is_pending(sp) || is_retry_timeout_queue_pending)
		return M_FALSE;

	sp->is_external_queue_enabled = M_TRUE;
	sp->external_queue_get_cb = get_cb;
	return M_TRUE;
}

void M_net_smtp_external_queue_have_messages(M_net_smtp_t *sp)
{
	M_net_smtp_status_t status;
	sp->is_external_queue_pending = M_TRUE;
	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		process_queue_queue(sp);
}
