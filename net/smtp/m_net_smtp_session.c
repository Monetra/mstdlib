#include "m_net_smtp_int.h"
#include <mstdlib/io/m_io_layer.h> /* M_io_layer_softevent_add (STARTTLS) */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* forward declarations */
static void tcp_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static void proc_io_stdin_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);

static M_bool run_state_machine(M_net_smtp_session_t *session, M_bool *is_done)
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


/* Return value is whether to run continue processing another
	* message.  process_queue_queue() can potentially destroy
	* the endpoint + sessions + session locks, which will seg fault the unlock
	* wrapped around this function. */
static M_bool tcp_io_cb_sub(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, M_bool *is_session_destroy)
{
	M_net_smtp_session_t        *session     = thunk;
	const M_net_smtp_t          *sp          = session->sp;
	const M_net_smtp_endpoint_t *const_ep    = session->ep;
	M_net_smtp_connect_cb        connect_cb  = sp->cbs.connect_cb;
	M_net_smtp_iocreate_cb       iocreate_cb = sp->cbs.iocreate_cb;
	M_io_error_t                 io_error    = M_IO_ERROR_SUCCESS;
	M_bool                       is_done     = M_FALSE;
	M_net_smtp_status_t          status;
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
		M_net_smtp_session_clean(session);
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
	M_net_smtp_connect_fail(session->sp, session->ep, session->tcp.net_error, session->errmsg);
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
	M_net_smtp_session_t *session                = thunk;
	const M_net_smtp_t   *sp                     = session->sp;
	M_bool                is_continue_processing = M_FALSE;
	M_bool                is_session_destroy     = M_FALSE;
	M_thread_mutex_lock(session->mutex);
	is_continue_processing = tcp_io_cb_sub(el, etype, io, thunk, &is_session_destroy);
	M_thread_mutex_unlock(session->mutex);

	if (is_session_destroy)
			M_net_smtp_session_destroy(session);

	if (is_continue_processing)
		M_net_smtp_queue_resume(sp->queue);
}



/* Return value is whether to run continue processing another
	* message.  process_queue_queue() can potentially destroy
	* the endpoint + sessions + session locks, which will seg fault the unlock
	* wrapped around this function. */
static M_bool proc_io_cb_sub(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask, M_bool *is_destroy_session)
{
	M_net_smtp_session_t  *session    = thunk;
	M_io_error_t           io_error   = M_IO_ERROR_SUCCESS;
	M_io_t               **session_io = NULL;
	size_t                 len;
	M_bool                 is_done;
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
					M_net_smtp_process_fail(session->sp, session->ep, session->process.result_code, stdout_str, session->errmsg);
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
	M_net_smtp_session_t *session                = thunk;
	const M_net_smtp_t   *sp                     = session->sp;
	M_bool                is_continue_processing = M_FALSE;
	M_bool                is_destroy_session     = M_FALSE;
	M_thread_mutex_lock(session->mutex);
	is_continue_processing = proc_io_cb_sub(el, etype, io, thunk, connection_mask, &is_destroy_session);
	M_thread_mutex_unlock(session->mutex);
	if (is_destroy_session)
		M_net_smtp_session_destroy(session);
	if (is_continue_processing)
		M_net_smtp_queue_resume(sp->queue);
}

#define PROC_IO_CB(type,TYPE) \
static void proc_io_##type##_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk) \
{ \
	return proc_io_cb(el, etype, io, thunk, M_NET_SMTP_CONNECTION_MASK_IO##TYPE); \
}

PROC_IO_CB(stderr, _STDERR)
PROC_IO_CB(stdout, _STDOUT)
PROC_IO_CB(stdin , _STDIN )
PROC_IO_CB(proc  ,        )

#undef PROC_IO_CB

void M_net_smtp_session_clean(M_net_smtp_session_t *session)
{
	M_net_smtp_endpoint_t *ep = M_CAST_OFF_CONST(M_net_smtp_endpoint_t*, session->ep);

	if (session->msg == NULL)
		return;

	if (session->is_backout || !session->is_successfully_sent) {
		M_net_smtp_queue_reschedule_msg(session->sp, session->msg, session->headers, session->is_backout, session->number_of_tries + 1, session->errmsg, session->retry_ms);
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

void M_net_smtp_session_reactivate_tcp(M_net_smtp_session_t *session)
{
	M_bool is_session_destroy = M_FALSE;
	/* Use sub because we already have a lock */
	tcp_io_cb_sub(session->sp->el, M_EVENT_TYPE_WRITE, session->io, session, &is_session_destroy);
}

M_net_smtp_session_t *M_net_smtp_session_create(const M_net_smtp_t *sp, const M_net_smtp_endpoint_t* ep)
{
	M_io_error_t          io_error = M_IO_ERROR_SUCCESS;
	M_net_smtp_session_t *session  = NULL;

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
			M_net_smtp_process_fail(session->sp, session->ep, session->process.result_code, "", session->errmsg);
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

void M_net_smtp_session_destroy(M_net_smtp_session_t *session)
{
	M_net_smtp_session_clean(session);
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
