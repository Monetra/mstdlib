#include "m_net_smtp_int.h"

typedef struct {
	const M_net_smtp_t    *sp;
	char                  *msg;
	M_hash_dict_t         *headers;
	size_t                 number_of_tries;
	M_event_timer_t       *event_timer;
} retry_msg_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void reschedule_event_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	retry_msg_t         *retry = thunk;
	const M_net_smtp_t  *sp    = retry->sp;
	M_net_smtp_queue_t  *q     = sp->queue;
	M_net_smtp_status_t  status;
	(void)el;
	(void)io;
	(void)etype;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_event_timer_remove(retry->event_timer);
	M_list_remove_val(q->retry_timers, retry->event_timer, M_LIST_MATCH_PTR);
	retry->event_timer = NULL;
	M_list_remove_val(q->retry_timeout_queue, retry, M_LIST_MATCH_PTR);
	M_list_insert(q->retry_queue, retry);
	M_thread_rwlock_unlock(q->retry_queue_rwlock);
	status = M_net_smtp_status(sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		M_net_smtp_queue_delegate_msgs(q);
}

void M_net_smtp_queue_reschedule_msg(const M_net_smtp_t *sp, const char *msg, const M_hash_dict_t *headers, M_bool is_backout, size_t num_tries, const char* errmsg, size_t retry_ms)
{
	retry_msg_t               *retry          = NULL;
	M_bool                     is_requeue     = M_FALSE;
	M_net_smtp_send_failed_cb  send_failed_cb = sp->cbs.send_failed_cb;
	M_net_smtp_reschedule_cb   reschedule_cb  = sp->cbs.reschedule_cb;
	M_net_smtp_queue_t        *q              = sp->queue;

	if (q->is_external_queue_enabled) {
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
			M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
			M_list_str_insert(q->internal_queue, msg);
			M_thread_rwlock_unlock(q->internal_queue_rwlock);
		} else {
			/* need to keep track of num_tries */
			retry = M_malloc_zero(sizeof(*retry));
			retry->sp = sp;
			retry->msg = M_strdup(msg);
			retry->number_of_tries = num_tries - 1;
			retry->headers = M_hash_dict_duplicate(headers);
			retry->event_timer = NULL;
			M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
			M_list_insert(q->retry_queue, retry);
			M_thread_rwlock_unlock(q->retry_queue_rwlock);
		}
		return;
	}

	if (num_tries < q->max_number_of_attempts) {
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
		M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		retry->event_timer = M_event_timer_oneshot(sp->el, retry_ms, M_FALSE,
				reschedule_event_cb, retry);
		M_list_insert(q->retry_timeout_queue, retry);
		M_list_insert(q->retry_timers, retry->event_timer);
		M_thread_rwlock_unlock(q->retry_queue_rwlock);
	}
}

static void start_sendmsg_task(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t* const_ep, char *msg, size_t num_tries)
{
	const M_hash_dict_t   *headers      = NULL;
	M_net_smtp_endpoint_t *ep           = M_CAST_OFF_CONST(M_net_smtp_endpoint_t *,const_ep);
	M_net_smtp_session_t  *session      = NULL;
	M_email_t             *e            = NULL;
	M_bool                 is_bootstrap = M_FALSE;
	M_net_smtp_queue_t    *q            = sp->queue;

	if (M_email_simple_read(&e, msg, M_str_len(msg), M_EMAIL_SIMPLE_READ_NONE, NULL) != M_EMAIL_ERROR_SUCCESS) {
		M_bool is_retrying = M_FALSE;
		num_tries = q->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(NULL, msg, num_tries, is_retrying, sp->thunk);
		M_email_destroy(e);
		M_free(msg);
		M_net_smtp_queue_resume(q);
		return;
	}

	headers = M_email_headers(e);

	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	session = M_list_take_first(ep->idle_sessions);
	if (session == NULL) {
		session = M_net_smtp_session_create(sp, ep);
		if (session == NULL) {
			M_net_smtp_queue_reschedule_msg(sp, msg, headers, M_TRUE, num_tries + 1, "Failure creating session", q->retry_default_ms);
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
	session->retry_ms = q->retry_default_ms;
	M_mem_set(session->errmsg, 0, sizeof(session->errmsg));
	session->email = e;
	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		session->tcp.is_QUIT_enabled = (sp->tcp_idle_ms == 0);
		if (!is_bootstrap) {
			M_net_smtp_session_reactivate_tcp(session);
		}
	}
	M_list_insert(ep->send_sessions, session);
	M_thread_mutex_unlock(session->mutex);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

static int process_external_queue_num(const M_net_smtp_t *sp)
{
	const M_net_smtp_endpoint_t *ep = NULL;
	int                          n  = 0;
	M_net_smtp_queue_t          *q  = sp->queue;

	M_thread_mutex_lock(sp->endpoints_mutex);
	while ((ep = M_net_smtp_endpoint(sp)) != NULL) {
		char *msg = q->external_queue_get_cb();
		if (msg == NULL) {
			q->is_external_queue_pending = M_FALSE;
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

void M_net_smtp_queue_resume(M_net_smtp_queue_t *q)
{
	const M_net_smtp_t* sp = q->sp;
	if (!q->is_external_queue_enabled) {
		M_net_smtp_queue_delegate_msgs(q);
		return;
	}
	/* eager eval external queue to determine IDLE */
	if (process_external_queue_num(sp) == 0)
		M_net_smtp_queue_delegate_msgs(q);
}




static void process_internal_queues(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t                *sp = cb_arg;
	const M_net_smtp_endpoint_t *ep = NULL;
	M_net_smtp_queue_t          *q  = sp->queue;

	(void)event;
	(void)io;
	(void)type;

	M_thread_mutex_lock(sp->endpoints_mutex);
	while ((ep = M_net_smtp_endpoint(sp)) != NULL) {
		retry_msg_t *retry = NULL;
		char        *msg   = NULL;
		M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		retry = M_list_take_first(q->retry_queue);
		M_thread_rwlock_unlock(q->retry_queue_rwlock);
		if (retry != NULL) {
			start_sendmsg_task(sp, ep, retry->msg, retry->number_of_tries);
			M_free(retry);
			continue;
		}
		M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		msg = M_list_str_take_first(q->internal_queue);
		M_thread_rwlock_unlock(q->internal_queue_rwlock);
		if (msg != NULL) {
			start_sendmsg_task(sp, ep, msg, 0);
			continue;
		}
		break;
	}
	M_thread_mutex_unlock(sp->endpoints_mutex);
}

static M_bool idle_check(M_net_smtp_t *sp)
{
	size_t i;
	for (i = 0; i < M_list_len(sp->endpoints); i++) {
		if (M_net_smtp_endpoint_is_idle(M_list_at(sp->endpoints, i)) == M_FALSE)
			return M_FALSE;
	}
	return M_TRUE;
}

static void process_queue_queue(M_net_smtp_t *sp)
{
	M_net_smtp_queue_t *q = sp->queue;
	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (M_net_smtp_is_running(sp->status) && M_net_smtp_queue_is_pending(q)) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		if (q->is_external_queue_enabled) {
			M_event_queue_task(sp->el, process_external_queue, sp);
		} else {
			M_event_queue_task(sp->el, process_internal_queues, sp);
		}
	} else if (idle_check(sp)) {
		if (sp->status == M_NET_SMTP_STATUS_STOPPING) {
			M_net_smtp_processing_halted(sp);
		} else {
			sp->status = M_NET_SMTP_STATUS_IDLE;
			M_net_smtp_prune_endpoints(sp);
		}
	}
	M_thread_rwlock_unlock(sp->status_rwlock);
}

void M_net_smtp_queue_delegate_msgs(M_net_smtp_queue_t *q)
{
	process_queue_queue(M_CAST_OFF_CONST(M_net_smtp_t*, q->sp));
}

void M_net_smtp_queue_destroy(M_net_smtp_queue_t *q)
{
	retry_msg_t           *retry = NULL;
	M_event_timer_t       *timer = NULL;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	while ((timer = M_list_take_last(q->retry_timers)) != NULL) {
		M_event_timer_remove(timer);
	}
	M_list_destroy(q->retry_timers, M_TRUE);
	while (
		(retry = M_list_take_last(q->retry_queue))         != NULL ||
		(retry = M_list_take_last(q->retry_timeout_queue)) != NULL
	) {
		M_free(retry->msg);
		M_free(retry);
	}
	M_thread_rwlock_unlock(q->retry_queue_rwlock);
	M_list_destroy(q->retry_queue, M_TRUE);
	M_list_destroy(q->retry_timeout_queue, M_TRUE);
	M_thread_rwlock_destroy(q->retry_queue_rwlock);
	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_str_destroy(q->internal_queue);
	M_thread_rwlock_unlock(q->internal_queue_rwlock);
	M_thread_rwlock_destroy(q->internal_queue_rwlock);
}

M_bool M_net_smtp_queue_is_pending(M_net_smtp_queue_t *q)
{
	M_bool is_pending_internal;
	M_bool is_pending_retry;

	if (q->is_external_queue_enabled)
		return q->is_external_queue_pending;

	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_pending_internal = M_list_str_len(q->internal_queue) > 0;
	M_thread_rwlock_unlock(q->internal_queue_rwlock);

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_pending_retry = M_list_len(q->retry_queue) > 0;
	M_thread_rwlock_unlock(q->retry_queue_rwlock);

	return is_pending_retry || is_pending_internal;
}

M_net_smtp_queue_t * M_net_smtp_queue_create(M_net_smtp_t *sp, size_t max_number_of_attempts, size_t retry_default_ms)
{
	M_net_smtp_queue_t *q = M_malloc_zero(sizeof(*q));

	q->sp                     = sp;
	q->internal_queue         = M_list_str_create(M_LIST_STR_NONE);
	q->internal_queue_rwlock  = M_thread_rwlock_create();
	q->retry_queue            = M_list_create(NULL, M_LIST_NONE);
	q->retry_timeout_queue    = M_list_create(NULL, M_LIST_NONE);
	q->retry_timers           = M_list_create(NULL, M_LIST_NONE);
	q->retry_queue_rwlock     = M_thread_rwlock_create();

	q->max_number_of_attempts = max_number_of_attempts;
	q->retry_default_ms       = retry_default_ms;

	return q;
}

M_list_str_t *M_net_smtp_queue_dump(M_net_smtp_queue_t *q)
{
	M_list_str_t       *list;
	char               *msg;
	retry_msg_t        *retry;
	M_event_timer_t    *timer;

	if (q->is_external_queue_enabled) {
		list = M_list_str_create(M_LIST_STR_NONE);
		while ((msg = q->external_queue_get_cb()) != NULL) {
			M_list_str_insert(list, msg);
			M_free(msg);
		}
		return list;
	}

	list = q->internal_queue;
	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	q->internal_queue = M_list_str_create(M_LIST_STR_NONE);
	M_thread_rwlock_unlock(q->internal_queue_rwlock);

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	while (
		(retry = M_list_take_first(q->retry_queue))         != NULL ||
		(retry = M_list_take_first(q->retry_timeout_queue)) != NULL
	) {
		M_list_str_insert(list, retry->msg);
		M_free(retry->msg);
		M_free(retry);
	}
	M_thread_rwlock_unlock(q->retry_queue_rwlock);

	while ((timer = M_list_take_first(q->retry_timers)) != NULL) {
		M_event_timer_remove(timer);
	}

	return list;
}

M_bool M_net_smtp_queue_message_int(M_net_smtp_queue_t *q, const char *msg)
{
	M_bool              is_success;
	M_net_smtp_status_t status;

	if (q->is_external_queue_enabled)
		return M_FALSE;

	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	is_success = M_list_str_insert(q->internal_queue, msg);
	M_thread_rwlock_unlock(q->internal_queue_rwlock);

	if (!is_success)
		return M_FALSE;

	status = M_net_smtp_status(q->sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		M_net_smtp_queue_delegate_msgs(q);

	return M_TRUE;
}

void M_net_smtp_queue_external_have_messages(M_net_smtp_queue_t *q)
{
	M_net_smtp_status_t status;
	q->is_external_queue_pending = M_TRUE;
	status = M_net_smtp_status(q->sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		M_net_smtp_queue_delegate_msgs(q);
}

M_bool M_net_smtp_queue_use_external_queue(M_net_smtp_queue_t *q, char *(*get_cb)(void))
{
	M_bool is_retry_timeout_queue_pending;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_retry_timeout_queue_pending = M_list_len(q->retry_timeout_queue) > 0;
	M_thread_rwlock_unlock(q->retry_queue_rwlock);
	if (M_net_smtp_queue_is_pending(q) || is_retry_timeout_queue_pending)
		return M_FALSE;

	q->is_external_queue_enabled = M_TRUE;
	q->external_queue_get_cb = get_cb;
	return M_TRUE;
}

M_bool M_net_smtp_queue_smtp_int(M_net_smtp_queue_t *q, const M_email_t *e)
{
	char   *msg;
	M_bool  is_success;

	if (q->is_external_queue_enabled)
		return M_FALSE;

	if (!(msg = M_email_simple_write(e))) { return M_FALSE; }

	is_success = M_net_smtp_queue_message_int(q, msg);
	M_free(msg);
	return is_success;
}

void M_net_smtp_queue_set_num_attempts(M_net_smtp_queue_t *q, size_t num)
{
	q->max_number_of_attempts = num;
}
