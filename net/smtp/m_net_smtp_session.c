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
#include <mstdlib/io/m_io_layer.h> /* M_io_layer_softevent_add (STARTTLS) */

typedef enum {
	SESSION_PROCESSING = 1,
	SESSION_IDLE,
	SESSION_FINISHED,
	SESSION_STALE,
} session_status_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void trigger_softevent(M_io_t *io, M_event_type_t etype)
{
	M_io_layer_t *layer = M_io_layer_acquire(io, 0, NULL);
	M_io_layer_softevent_add(layer, M_FALSE, etype, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);
}

static void trigger_write_softevent(M_io_t *io)
{
	trigger_softevent(io, M_EVENT_TYPE_WRITE);
}

/* forward declarations */
static void session_tcp_advance_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static void session_proc_advance_stdin_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static void session_proc_advance_stdout_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static void session_proc_advance_stderr_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);

static session_status_t session_tcp_advance(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_session_t        *session            = thunk;
	const M_net_smtp_t          *sp                 = session->sp;
	const M_net_smtp_endpoint_t *const_ep           = session->ep;
	M_net_smtp_connect_cb        connect_cb         = sp->cbs.connect_cb;
	M_net_smtp_iocreate_cb       iocreate_cb        = sp->cbs.iocreate_cb;
	M_io_error_t                 io_error           = M_IO_ERROR_SUCCESS;

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
				return SESSION_PROCESSING;
			}

			break;
		case M_EVENT_TYPE_DISCONNECTED:
			goto destroy;
		case M_EVENT_TYPE_READ:
			io_error = M_io_read_into_parser(io, session->in_parser);
			switch (io_error) {
				case M_IO_ERROR_SUCCESS:
					break;
				case M_IO_ERROR_WOULDBLOCK:
					return SESSION_PROCESSING;
				case M_IO_ERROR_DISCONNECT:
					goto destroy;
				default:
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Read failed: %s", M_io_error_string(io_error));
					goto destroy;
			}
			break;
		case M_EVENT_TYPE_WRITE:
			if (session->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_ADDED) {
				/* Need to wait for M_EVENT_TYPE_CONNECT */
				return SESSION_PROCESSING;
			}
			io_error = M_io_write_from_buf(io, session->out_buf);
			switch (io_error) {
				case M_IO_ERROR_SUCCESS:
					break;
				case M_IO_ERROR_WOULDBLOCK:
					return SESSION_PROCESSING;
				case M_IO_ERROR_DISCONNECT:
					goto destroy;
				default:
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Write failed: %s", M_io_error_string(io_error));
					goto destroy;
			}
			break;
		case M_EVENT_TYPE_ACCEPT:
			/* should be impossible */
			session->tcp.net_error = M_NET_ERROR_PROTONOTSUPPORTED;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unsupported ACCEPT event");
			goto backout;
			break;
		case M_EVENT_TYPE_OTHER:
		case M_EVENT_TYPE_ERROR:
			if (etype == M_EVENT_TYPE_OTHER && session->is_successfully_sent) {
				/* Idle timeout */
				session->tcp.is_QUIT_enabled = M_TRUE;
				M_event_timer_stop(session->event_timer);
				break;
			}
			if (session->tcp.tls_state == M_NET_SMTP_TLS_IMPLICIT && session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
				/* Implict TLS failed.  Follwup with with STARTTLS */
				session->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS;
				M_io_destroy(session->io);
				io_error = M_io_net_client_create(&session->io, sp->tcp_dns, const_ep->tcp.address,
						const_ep->tcp.port, M_IO_NET_ANY);
				if (io_error != M_IO_ERROR_SUCCESS) {
					goto destroy;
				}
				M_event_add(el, session->io, session_tcp_advance_task, session);
				M_event_timer_reset(session->event_timer, sp->tcp_connect_ms);
				return SESSION_PROCESSING;
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
	}

	if (M_net_smtp_status(sp) == M_NET_SMTP_STATUS_STOPPING || session->ep->is_removed)
		session->tcp.is_QUIT_enabled = M_TRUE;

	switch (M_state_machine_run(session->state_machine, session)) {
		case M_STATE_MACHINE_STATUS_WAIT:
			break;
		case M_STATE_MACHINE_STATUS_DONE:
			goto destroy;
		default: /* M_STATE_MACHINE_STATUS_ERROR_STATE is the only real other option */
			if (M_str_eq(session->errmsg, "")) {
				M_snprintf(session->errmsg, sizeof(session->errmsg), "State machine error");
			}
			if (session->tcp.is_connect_fail) {
				goto backout;
			}
			goto destroy;
	}


	if (session->is_successfully_sent && session->msg != NULL && !session->tcp.is_QUIT_enabled) {
		/* get ready to accept another. */
		M_net_smtp_session_clean(session);
		M_net_smtp_endpoint_idle_session(session->ep, session);
		M_event_timer_reset(session->event_timer, sp->tcp_idle_ms);
		return SESSION_IDLE;
	}

	if (session->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_READY) {
		size_t        layer_id;
		M_io_layer_t *layer;
		M_io_tls_client_add(io, session->sp->tcp_tls_ctx, NULL, &layer_id);
		layer = M_io_layer_acquire(io, layer_id, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
		session->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS_ADDED;
		return SESSION_PROCESSING; /* short circuit out */
	}

	if (M_buf_len(session->out_buf) > 0)
		trigger_write_softevent(session->io);

	return SESSION_PROCESSING;
backout:
	M_net_smtp_connect_fail(session);
	session->is_backout = M_TRUE;
destroy:
	if (session->io != NULL) {
		M_io_destroy(session->io);
		session->connection_mask &= ~M_NET_SMTP_CONNECTION_MASK_IO;
		if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			if (session->is_backout == M_FALSE) {
				sp->cbs.disconnect_cb(const_ep->tcp.address, const_ep->tcp.port, sp->thunk);
			}
		}
		session->io = NULL;
		return SESSION_FINISHED;
	}
	return SESSION_STALE;
}

static void session_tcp_advance_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_session_t *session = thunk;
	M_net_smtp_queue_t   *q       = session->sp->queue;
	session_status_t      status;

	M_thread_mutex_lock(session->mutex);
	status = session_tcp_advance(el, etype, io, thunk);
	M_thread_mutex_unlock(session->mutex);

	switch (status) {
		case SESSION_FINISHED:
			M_event_timer_stop(session->event_timer);
			M_net_smtp_endpoint_cull_session(session->ep, session);
			M_net_smtp_session_clean(session);
			M_event_queue_task(session->sp->el, M_net_smtp_session_destroy_task, session);
			M_net_smtp_queue_advance(q);
			break;
		case SESSION_IDLE:
			M_net_smtp_queue_advance(q);
			break;
		case SESSION_STALE:
		case SESSION_PROCESSING:
			break;
	}
}

static session_status_t session_proc_advance(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask)
{
	M_net_smtp_session_t  *session    = thunk;
	M_io_error_t           io_error   = M_IO_ERROR_SUCCESS;
	M_io_t               **session_io = NULL;
	size_t                 len;
	(void)el;

	switch(etype) {
		case M_EVENT_TYPE_CONNECTED:
			session->connection_mask |= connection_mask;
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			session->connection_mask &= ~connection_mask;
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
					M_net_smtp_process_fail(session, stdout_str);
					M_free(stdout_str);
					session->is_successfully_sent = M_FALSE;
					if (session->errmsg[0] == 0) {
						M_snprintf(session->errmsg, sizeof(session->errmsg), "Bad result code %d", session->process.result_code);
					}
				}
				if (session->process.io_stdin != NULL) {
					trigger_softevent(session->process.io_stdin, M_EVENT_TYPE_DISCONNECTED);
				}
			}
			break;
		case M_EVENT_TYPE_READ:
			if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDERR) {
				io_error = M_io_read(io, (unsigned char *)session->errmsg, sizeof(session->errmsg) - 1, &len);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					session->connection_mask &= ~connection_mask;
					break;
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
					session->connection_mask &= ~connection_mask;
					break;
				}
				if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
					M_snprintf(session->errmsg, sizeof(session->errmsg), "Read failure: %s", M_io_error_string(io_error));
				}
				goto destroy; /* shouldn't receive anything on stdout */
			}
			session->is_successfully_sent = M_FALSE;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_WRITE:
			if (connection_mask != M_NET_SMTP_CONNECTION_MASK_IO_STDIN) {
				session->is_successfully_sent = M_FALSE;
				M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
				goto destroy;
			}
			if (M_buf_len(session->out_buf) > 0) {
				io_error = M_io_write_from_buf(io, session->out_buf);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					session->connection_mask &= ~connection_mask;
					break;
				}
				if (session->process.len > 0) {
					/* Give process a chance to parse and react to input */
					M_uint64 timeout_ms = 5000;
					if (session->ep->process.timeout_ms > 0 && session->ep->process.timeout_ms < (timeout_ms * 10) / 9) {
						/* If we have less than 90% of the timeout before a process timeout, use 90% of the time available
							* to try to detect a problem */
						timeout_ms = (session->ep->process.timeout_ms * 9) / 10;
					}
					session->event_timer = M_event_timer_oneshot(session->sp->el, timeout_ms, M_FALSE, session_proc_advance_stdin_task, session);
				} else {
					session->event_timer = NULL;
				}
			}
			if (session->process.len == 0) {
				session->connection_mask &= ~connection_mask;
				break;
			}
			return SESSION_PROCESSING;
		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_ACCEPT:
			if (etype == M_EVENT_TYPE_ERROR && connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDIN) {
				session->connection_mask &= ~connection_mask;
				break;
			}
			if (etype == M_EVENT_TYPE_ERROR && io == session->io && M_io_get_error(io) == M_IO_ERROR_TIMEDOUT) {
				session->connection_mask &= ~connection_mask;
				break;
			}
			session->is_successfully_sent = M_FALSE;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_OTHER:
			if (session->event_timer != NULL) {
				session->process.is_done_waiting = M_TRUE;
				M_event_timer_stop(session->event_timer);
				M_event_timer_remove(session->event_timer);
				session->event_timer = NULL;
			}
			break;
	}

	switch (M_state_machine_run(session->state_machine, session)) {
		case M_STATE_MACHINE_STATUS_WAIT:
			break;
		case M_STATE_MACHINE_STATUS_DONE:
			goto destroy;
		default: /* M_STATE_MACHINE_STATUS_ERROR_STATE is the only real other option */
			if (M_str_eq(session->errmsg, "")) {
				M_snprintf(session->errmsg, sizeof(session->errmsg), "State machine error");
			}
			goto destroy;
	}

	if ((session->connection_mask & connection_mask) == 0)
		goto destroy;

	if (M_buf_len(session->out_buf) > 0)
		trigger_write_softevent(session->process.io_stdin);

	return SESSION_PROCESSING;
destroy:
	switch (connection_mask) {
		case M_NET_SMTP_CONNECTION_MASK_IO:        session_io = &session->io;                break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDIN:  session_io = &session->process.io_stdin;  break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDOUT: session_io = &session->process.io_stdout; break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDERR: session_io = &session->process.io_stderr; break;
	}
	if (*session_io != NULL) {
		M_io_destroy(*session_io);
		*session_io = NULL;
		if (session->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			return SESSION_FINISHED;
		}
	}

	if (session->process.io_stdin != NULL)
		trigger_softevent(session->process.io_stdin, M_EVENT_TYPE_DISCONNECTED);

	if (session->process.io_stdout != NULL)
		trigger_softevent(session->process.io_stdout, M_EVENT_TYPE_DISCONNECTED);

	if (session->process.io_stderr != NULL)
		trigger_softevent(session->process.io_stderr, M_EVENT_TYPE_DISCONNECTED);

	return SESSION_PROCESSING;
}

static void session_proc_advance_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask)
{
	M_net_smtp_session_t *session = thunk;
	M_net_smtp_queue_t   *q       = session->sp->queue;
	session_status_t      status;

	M_thread_mutex_lock(session->mutex);
	if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO && etype == M_EVENT_TYPE_CONNECTED) {
		M_event_add(el, session->process.io_stdin , session_proc_advance_stdin_task , session);
		M_event_add(el, session->process.io_stdout, session_proc_advance_stdout_task, session);
		M_event_add(el, session->process.io_stderr, session_proc_advance_stderr_task, session);
	}
	status = session_proc_advance(el, etype, io, thunk, connection_mask);
	M_thread_mutex_unlock(session->mutex);

	switch (status) {
		case SESSION_FINISHED:
			M_net_smtp_endpoint_cull_session(session->ep, session);
			M_net_smtp_session_clean(session);
			M_event_queue_task(session->sp->el, M_net_smtp_session_destroy_task, session);
			M_net_smtp_queue_advance(q);
			break;
		case SESSION_IDLE:
			M_net_smtp_queue_advance(q);
			break;
		case SESSION_STALE:
		case SESSION_PROCESSING:
			break;
	}
}

#define SESSION_PROC_ADVANCE_TASK(type,TYPE) \
static void session_proc_advance_##type##_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk) \
{ \
	session_proc_advance_task(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO##TYPE); \
}

SESSION_PROC_ADVANCE_TASK(stderr, _STDERR)
SESSION_PROC_ADVANCE_TASK(stdout, _STDOUT)
SESSION_PROC_ADVANCE_TASK(stdin , _STDIN )
SESSION_PROC_ADVANCE_TASK(proc  ,        )

#undef SESSION_PROC_ADVANCE_TASK

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_net_smtp_session_dispatch_msg(M_net_smtp_session_t *session, M_net_smtp_dispatch_msg_args_t *args)
{
	const M_net_smtp_t          *sp = session->sp;
	const M_net_smtp_queue_t    *q  = sp->queue;

	M_thread_mutex_lock(session->mutex);

	session->msg                    = args->msg;
	session->number_of_tries        = args->num_tries;
	session->headers                = args->headers;
	session->is_successfully_sent   = M_FALSE;
	session->is_backout             = M_FALSE;
	session->retry_ms               = q->retry_default_ms;
	session->email                  = args->email;
	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		session->tcp.ehlo_domain = args->domain;
	} else {
		M_free(args->domain);
	}
	M_mem_set(session->errmsg, 0, sizeof(session->errmsg));

	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		session->tcp.is_QUIT_enabled = (sp->tcp_idle_ms == 0);
		if (!args->is_bootstrap) {
			M_event_timer_reset(session->event_timer, sp->tcp_stall_ms);
			M_event_queue_task(session->sp->el, M_net_smtp_session_reactivate_tcp_task, session);
		} else {
			M_event_timer_start(session->event_timer, sp->tcp_connect_ms);
		}
	}
	M_thread_mutex_unlock(session->mutex);
}

void M_net_smtp_session_clean(M_net_smtp_session_t *session)
{
	if (session->msg == NULL)
		return;

	if (session->is_backout || !session->is_successfully_sent) {
		M_net_smtp_queue_reschedule_msg_args_t args = { session->sp, session->msg, session->headers, session->is_backout,
			session->number_of_tries + 1, session->errmsg, session->retry_ms };
		M_net_smtp_queue_reschedule_msg(&args);
	} else {
		session->sp->cbs.sent_cb(session->headers, session->sp->thunk);
	}

	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		M_free(session->tcp.ehlo_domain);
	}
	M_email_destroy(session->email);
	M_hash_dict_destroy(session->headers);
	M_free(session->msg);

	session->email   = NULL;
	session->msg     = NULL;
	session->headers = NULL;
}

void M_net_smtp_session_reactivate_tcp(M_net_smtp_session_t *session)
{
	session_tcp_advance_task(session->sp->el, M_EVENT_TYPE_WRITE, session->io, session);
}

void M_net_smtp_session_reactivate_tcp_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	M_net_smtp_session_reactivate_tcp(thunk);
}

static void M_net_smtp_session_destroy_int(M_net_smtp_session_t *session)
{
	if (session->ep->type == M_NET_SMTP_EPTYPE_TCP) {
		M_list_str_destroy(session->tcp.smtp_response);
		M_io_destroy(session->io);
	}

	if (session->ep->type == M_NET_SMTP_EPTYPE_PROCESS) {
		M_io_destroy(session->io);
		M_io_destroy(session->process.io_stdin);
		M_io_destroy(session->process.io_stdout);
		M_io_destroy(session->process.io_stderr);
	}

	M_event_timer_remove(session->event_timer);
	M_buf_cancel(session->out_buf);
	M_parser_destroy(session->in_parser);
	M_state_machine_destroy(session->state_machine);

	session->is_alive      = M_FALSE;
	session->state_machine = NULL;
	session->in_parser     = NULL;
	session->out_buf       = NULL;
	session->event_timer   = NULL;
}

M_net_smtp_session_t *M_net_smtp_session_create(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t* ep)
{
	M_io_error_t          io_error = M_IO_ERROR_SUCCESS;
	M_net_smtp_session_t *session  = NULL;

	session                        = M_malloc_zero(sizeof(*session));
	session->sp                    = sp;
	session->ep                    = ep;
	session->mutex                 = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

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
			M_net_smtp_process_fail(session, "");
			goto fail;
		}
		session->state_machine = M_net_smtp_flow_process();
		M_event_add(sp->el, session->io               , session_proc_advance_proc_task  , session);
		session->connection_mask = M_NET_SMTP_CONNECTION_MASK_PROC_ALL;
	} else {
		io_error = M_io_net_client_create(&session->io, sp->tcp_dns, ep->tcp.address, ep->tcp.port, M_IO_NET_ANY);
		if (io_error != M_IO_ERROR_SUCCESS) {
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Error creating session: %s", M_io_error_string(io_error));
			session->tcp.net_error = M_net_io_error_to_net_error(io_error);
			M_net_smtp_connect_fail(session);
			goto fail;
		}

		M_io_net_set_connect_timeout_ms(session->io, sp->tcp_connect_ms);

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
		session->event_timer          = M_event_timer_add(sp->el, session_tcp_advance_task, session);
		session->tcp.smtp_response    = M_list_str_create(M_LIST_STR_NONE);
		M_event_add(sp->el, session->io, session_tcp_advance_task, session);
	}

	session->out_buf   = M_buf_create();
	session->in_parser = M_parser_create(M_PARSER_FLAG_NONE);
	session->is_alive  = M_TRUE;
	return session;
fail:
	M_net_smtp_session_destroy_int(session);
	M_thread_mutex_destroy(session->mutex);
	M_free(session);
	return NULL;
}

void M_net_smtp_session_destroy(M_net_smtp_session_t *session, M_bool is_remove_from_endpoint)
{
	M_thread_mutex_lock(session->mutex);
	M_net_smtp_session_clean(session);
	M_net_smtp_session_destroy_int(session);
	if (is_remove_from_endpoint) {
		M_net_smtp_endpoint_remove_session(session->ep, session);
	}
	M_thread_mutex_unlock(session->mutex);
	M_thread_mutex_destroy(session->mutex);
	M_free(session);
}
