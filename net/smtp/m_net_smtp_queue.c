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

#include "m_net_smtp_int.h"

typedef struct {
	const M_net_smtp_t    *sp;
	char                  *msg;
	size_t                 number_of_tries;
	M_event_timer_t       *timer;
} retry_msg_t;

typedef enum {
	DISPATCH_MSG_SUCCESS = 1,
	DISPATCH_MSG_FAILURE,
	DISPATCH_MSG_NO_ATTEMPT_NO_ENDPOINT,
	DISPATCH_MSG_NO_ATTEMPT_NO_MSG,
} dispatch_msg_error_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static retry_msg_t *retry_msg_alloc(const M_net_smtp_t *sp, char *msg, size_t number_of_tries,
		M_event_timer_t *event_timer)
{
	retry_msg_t *retry     = M_malloc_zero(sizeof(*retry));
	retry->sp              = sp;
	retry->msg             = msg;
	retry->number_of_tries = number_of_tries;
	retry->timer           = event_timer;
	return retry;
}

static void retry_msg_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	retry_msg_t         *retry = thunk;
	M_net_smtp_queue_t  *q     = retry->sp->queue;
	(void)el;
	(void)io;
	(void)etype;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_event_timer_remove(retry->timer);
	retry->timer = NULL;
	M_list_remove_val(q->retry_timeout_queue, retry, M_LIST_MATCH_PTR);
	M_list_insert(q->retry_queue, retry);
	M_thread_rwlock_unlock(q->retry_queue_rwlock);
	if (M_net_smtp_status(q->sp) == M_NET_SMTP_STATUS_IDLE)
		M_net_smtp_queue_advance(q);
}

static char *email_address_domain_cpy(const char* address)
{
	const char *domain = M_str_chr(address, '@');

	if (domain == NULL)
		return NULL;

	return M_strdup(domain + 1);
}

static M_bool dispatch_msg(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t *ep, char *msg, size_t num_tries)
{
	M_net_smtp_dispatch_msg_args_t        dispatch_args = { sp, msg, num_tries, NULL, NULL, M_FALSE, NULL };
	M_email_error_t                       email_error;
	const char                           *address;
	size_t                                num_rcpt_to_addresses;

	email_error = M_email_simple_read(&dispatch_args.email, msg, M_str_len(msg), M_EMAIL_SIMPLE_READ_NONE, NULL);
	if (email_error != M_EMAIL_ERROR_SUCCESS) {
		M_bool is_retrying = M_FALSE;
		num_tries = sp->queue->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(NULL, msg, num_tries, is_retrying, sp->thunk);
		goto fail;
	}

	M_email_simple_split_header_body(msg, &dispatch_args.headers, NULL);

	if (!M_email_from(dispatch_args.email, NULL, NULL, &address) || address == NULL) {
		M_bool is_retrying = M_FALSE;
		num_tries = sp->queue->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(dispatch_args.headers, "No from address found", num_tries, is_retrying, sp->thunk);
		goto fail;
	}

	dispatch_args.domain = email_address_domain_cpy(address);
	if (dispatch_args.domain == NULL) {
		M_bool is_retrying = M_FALSE;
		num_tries = sp->queue->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(dispatch_args.headers, "No domain found in email address", num_tries, is_retrying, sp->thunk);
		goto fail;
	}

	num_rcpt_to_addresses = M_email_to_len(dispatch_args.email) + M_email_cc_len(dispatch_args.email) +
			M_email_bcc_len(dispatch_args.email);

	if (num_rcpt_to_addresses == 0) {
		M_bool is_retrying = M_FALSE;
		num_tries = sp->queue->is_external_queue_enabled ? 0 : 1;
		sp->cbs.send_failed_cb(dispatch_args.headers, "No send addresses found", num_tries, is_retrying, sp->thunk);
		goto fail;
	}

	if (sp->queue->max_number_of_attempts == 0) {
		M_net_smtp_queue_reschedule_msg_args_t reschedule_args = { sp, msg, dispatch_args.headers, M_FALSE, num_tries,
			"Max number attempts set to 0", sp->queue->retry_default_ms };
		M_net_smtp_queue_reschedule_msg(&reschedule_args);
		goto fail;
	}

	if (!M_net_smtp_endpoint_dispatch_msg(M_CAST_OFF_CONST(M_net_smtp_endpoint_t*,ep), &dispatch_args)) {
		M_net_smtp_queue_reschedule_msg_args_t reschedule_args = { sp, msg, dispatch_args.headers, M_TRUE, num_tries + 1,
				"Failure creating session", sp->queue->retry_default_ms };
		M_net_smtp_queue_reschedule_msg(&reschedule_args);
		goto fail;
	}
	return M_TRUE;
fail:
	M_free(dispatch_args.domain);
	M_email_destroy(dispatch_args.email);
	M_hash_dict_destroy(dispatch_args.headers);
	M_free(msg);
	return M_FALSE;
}

static dispatch_msg_error_t dispatch_msg_external(M_net_smtp_t *sp)
{
	M_net_smtp_queue_t          *q          = sp->queue;
	const M_net_smtp_endpoint_t *ep         = NULL;
	char                        *msg        = NULL;
	M_bool                       is_success = M_FALSE;

	ep = M_net_smtp_endpoint_acquire(sp);

	if (ep == NULL)
		return DISPATCH_MSG_NO_ATTEMPT_NO_ENDPOINT;

	msg = q->external_queue_get_cb();

	if (msg == NULL) {
		q->is_external_queue_pending = M_FALSE;
		M_net_smtp_endpoint_release(sp);
		return DISPATCH_MSG_NO_ATTEMPT_NO_MSG;
	}

	is_success = dispatch_msg(sp, ep, msg, 0);
	M_net_smtp_endpoint_release(sp);
	if (is_success)
		return DISPATCH_MSG_SUCCESS;
	return DISPATCH_MSG_FAILURE;
}

static dispatch_msg_error_t dispatch_msg_internal(M_net_smtp_t *sp)
{
	M_net_smtp_queue_t          *q          = sp->queue;
	const M_net_smtp_endpoint_t *ep         = NULL;
	retry_msg_t                 *retry      = NULL;
	char                        *msg        = NULL;
	M_bool                       is_success = M_FALSE;

	ep = M_net_smtp_endpoint_acquire(sp);

	if (ep == NULL)
		return DISPATCH_MSG_NO_ATTEMPT_NO_ENDPOINT;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	retry = M_list_take_first(q->retry_queue);
	M_thread_rwlock_unlock(q->retry_queue_rwlock);

	if (retry != NULL) {
		is_success = dispatch_msg(sp, ep, retry->msg, retry->number_of_tries);
		M_free(retry);
		M_net_smtp_endpoint_release(sp);
		if (is_success)
			return DISPATCH_MSG_SUCCESS;
		return DISPATCH_MSG_FAILURE;
	}

	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	msg = M_list_str_take_first(q->internal_queue);
	M_thread_rwlock_unlock(q->internal_queue_rwlock);

	if (msg != NULL) {
		is_success = dispatch_msg(sp, ep, msg, 0);
		M_net_smtp_endpoint_release(sp);
		if (is_success)
			return DISPATCH_MSG_SUCCESS;
		return DISPATCH_MSG_FAILURE;
	}

	M_net_smtp_endpoint_release(sp);
	return DISPATCH_MSG_NO_ATTEMPT_NO_MSG;
}

static void dispatch_msg_external_task(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t         *sp = cb_arg;
	dispatch_msg_error_t  rc = dispatch_msg_external(sp);
	(void)io;
	(void)type;
	switch (rc) {
		case DISPATCH_MSG_NO_ATTEMPT_NO_MSG:
			/* The external messages pending flag has now been cleared so running dispatch_msg
				* will return the state to IDLE */
			M_net_smtp_queue_advance(sp->queue);
			break;
		case DISPATCH_MSG_NO_ATTEMPT_NO_ENDPOINT:
			break;
		case DISPATCH_MSG_SUCCESS:
			M_event_queue_task(event, dispatch_msg_external_task, sp);
			break;
		case DISPATCH_MSG_FAILURE:
			M_net_smtp_queue_advance(sp->queue);
			break;
	}
}

static void dispatch_msg_internal_task(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t         *sp = cb_arg;
	dispatch_msg_error_t  rc = dispatch_msg_internal(sp);
	(void)io;
	(void)type;
	switch (rc) {
		case DISPATCH_MSG_NO_ATTEMPT_NO_ENDPOINT:
		case DISPATCH_MSG_NO_ATTEMPT_NO_MSG:
			break;
		case DISPATCH_MSG_SUCCESS:
			M_event_queue_task(event, dispatch_msg_internal_task, sp);
			break;
		case DISPATCH_MSG_FAILURE:
			M_net_smtp_queue_advance(sp->queue);
			break;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_smtp_queue_t * M_net_smtp_queue_create(M_net_smtp_t *sp, size_t max_number_of_attempts, size_t retry_default_ms)
{
	M_net_smtp_queue_t *q     = M_malloc_zero(sizeof(*q));

	q->sp                     = sp;
	q->internal_queue         = M_list_str_create(M_LIST_STR_NONE);
	q->internal_queue_rwlock  = M_thread_rwlock_create();
	q->retry_queue            = M_list_create(NULL, M_LIST_NONE);
	q->retry_timeout_queue    = M_list_create(NULL, M_LIST_NONE);
	q->retry_queue_rwlock     = M_thread_rwlock_create();

	q->max_number_of_attempts = max_number_of_attempts;
	q->retry_default_ms       = retry_default_ms;

	return q;
}

void M_net_smtp_queue_destroy(M_net_smtp_queue_t *q)
{
	retry_msg_t           *retry = NULL;

	M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);

	retry = M_list_take_last(q->retry_queue);
	while (retry != NULL) {
		M_free(retry->msg);
		M_free(retry);
		retry = M_list_take_last(q->retry_queue);
	}

	retry = M_list_take_last(q->retry_timeout_queue);
	while (retry != NULL) {
		M_event_timer_remove(retry->timer);
		M_free(retry->msg);
		M_free(retry);
		retry = M_list_take_last(q->retry_timeout_queue);
	}

	M_thread_rwlock_unlock(q->retry_queue_rwlock);
	M_list_destroy(q->retry_queue, M_TRUE);
	M_list_destroy(q->retry_timeout_queue, M_TRUE);
	M_thread_rwlock_destroy(q->retry_queue_rwlock);
	M_thread_rwlock_lock(q->internal_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_str_destroy(q->internal_queue);
	M_thread_rwlock_unlock(q->internal_queue_rwlock);
	M_thread_rwlock_destroy(q->internal_queue_rwlock);
	M_free(q);
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

void M_net_smtp_queue_advance(M_net_smtp_queue_t *q)
{
	M_net_smtp_t *sp = M_CAST_OFF_CONST(M_net_smtp_t*, q->sp);

	M_thread_rwlock_lock(sp->status_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	if (M_net_smtp_is_running(sp->status) && M_net_smtp_queue_is_pending(q)) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		M_thread_rwlock_unlock(sp->status_rwlock);
		if (q->is_external_queue_enabled) {
			dispatch_msg_external_task(sp->el, M_EVENT_TYPE_OTHER, NULL, sp);
		} else {
			dispatch_msg_internal_task(sp->el, M_EVENT_TYPE_OTHER, NULL, sp);
		}
		return;
	}
	if (M_net_smtp_is_all_endpoints_idle(sp)) {
		if (sp->status == M_NET_SMTP_STATUS_STOPPING) {
			M_net_smtp_processing_halted(sp);
		} else {
			sp->status = M_NET_SMTP_STATUS_IDLE;
			M_net_smtp_prune_endpoints(sp);
		}
	}
	M_thread_rwlock_unlock(sp->status_rwlock);
}

void M_net_smtp_queue_advance_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	M_net_smtp_queue_advance(thunk);
}

void M_net_smtp_queue_reschedule_msg(M_net_smtp_queue_reschedule_msg_args_t *args)
{
	const M_net_smtp_t        *sp             = args->sp;
	const char                *msg            = args->msg;
	const M_hash_dict_t       *headers        = args->headers;
	M_bool                     is_backout     = args->is_backout;
	size_t                     num_tries      = args->num_tries;
	const char                *errmsg         = args->errmsg;
	size_t                     retry_ms       = args->retry_ms;
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
			retry = retry_msg_alloc(sp, M_strdup(msg), num_tries - 1, NULL);
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
		retry = retry_msg_alloc(sp, M_strdup(msg), num_tries, NULL);
		M_thread_rwlock_lock(q->retry_queue_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
		retry->timer = M_event_timer_oneshot(sp->el, retry_ms, M_FALSE, retry_msg_task, retry);
		M_list_insert(q->retry_timeout_queue, retry);
		M_thread_rwlock_unlock(q->retry_queue_rwlock);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* API pass-through functions */

M_list_str_t *M_net_smtp_queue_dump(M_net_smtp_queue_t *q)
{
	M_list_str_t       *list;
	char               *msg;
	retry_msg_t        *retry;

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

	retry = M_list_take_first(q->retry_queue);
	while (retry != NULL) {
		M_list_str_insert(list, retry->msg);
		M_free(retry->msg);
		M_free(retry);
		retry = M_list_take_first(q->retry_queue);
	}

	retry = M_list_take_first(q->retry_timeout_queue);
	while (retry != NULL) {
		M_list_str_insert(list, retry->msg);
		M_event_timer_remove(retry->timer);
		M_free(retry->msg);
		M_free(retry);
		retry = M_list_take_first(q->retry_timeout_queue);
	}

	M_thread_rwlock_unlock(q->retry_queue_rwlock);

	return list;
}

void M_net_smtp_queue_set_num_attempts(M_net_smtp_queue_t *q, size_t num)
{
	q->max_number_of_attempts = num;
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
		M_event_queue_task(q->sp->el, M_net_smtp_queue_advance_task, q);

	return M_TRUE;
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

void M_net_smtp_queue_external_have_messages(M_net_smtp_queue_t *q)
{
	M_net_smtp_status_t status;
	q->is_external_queue_pending = M_TRUE;
	status = M_net_smtp_status(q->sp);
	if (status == M_NET_SMTP_STATUS_IDLE)
		M_event_queue_task(q->sp->el, M_net_smtp_queue_advance_task, q);
}

void M_net_smtp_session_destroy_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	M_net_smtp_session_destroy(thunk);
}
