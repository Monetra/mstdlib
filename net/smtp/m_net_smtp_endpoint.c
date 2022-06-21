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

M_bool M_net_smtp_endpoint_is_available(const M_net_smtp_endpoint_t *ep)
{
	M_bool is_available;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_available = (ep->max_sessions - M_list_len(ep->send_sessions)) > 0;
	M_thread_rwlock_unlock(ep->sessions_rwlock);
	return is_available;
}

M_bool M_net_smtp_endpoint_is_idle(const M_net_smtp_endpoint_t *ep)
{
	M_bool is_idle;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_READ);
	is_idle = (M_list_len(ep->send_sessions) == 0);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
	return is_idle;
}

void M_net_smtp_endpoint_reactivate_idle(const M_net_smtp_endpoint_t *ep)
{
	M_net_smtp_session_t *session;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	session = M_list_take_last(ep->idle_sessions);
	while (session != NULL) {
		M_list_insert(ep->send_sessions, session);
		M_net_smtp_session_reactivate_tcp(session);
		session = M_list_take_last(ep->idle_sessions);
	}
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

void M_net_smtp_endpoint_reactivate_idle_task(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	M_net_smtp_endpoint_reactivate_idle(thunk);
}

void M_net_smtp_endpoint_remove_session(const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session)
{
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_remove_val(ep->send_sessions, session, M_LIST_MATCH_PTR);
	M_list_remove_val(ep->idle_sessions, session, M_LIST_MATCH_PTR);
	M_list_remove_val(ep->cull_sessions, session, M_LIST_MATCH_PTR);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

void M_net_smtp_endpoint_cull_session(const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session)
{
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_remove_val(ep->send_sessions, session, M_LIST_MATCH_PTR);
	M_list_remove_val(ep->idle_sessions, session, M_LIST_MATCH_PTR);
	M_list_insert(ep->cull_sessions, session);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

void M_net_smtp_endpoint_idle_session(const M_net_smtp_endpoint_t *ep, M_net_smtp_session_t *session)
{
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_list_remove_val(ep->send_sessions, session, M_LIST_MATCH_PTR);
	M_list_insert(ep->idle_sessions, session);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
}

M_bool M_net_smtp_endpoint_dispatch_msg(const M_net_smtp_endpoint_t *ep, M_net_smtp_dispatch_msg_args_t *args)
{
	const M_net_smtp_t   *sp      = args->sp;
	M_net_smtp_session_t *session;

	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	args->is_bootstrap = M_FALSE;
	session = M_list_take_first(ep->idle_sessions);
	if (session == NULL) {
		args->is_bootstrap = M_TRUE;
		session = M_net_smtp_session_create(sp, ep);
		if (session == NULL) {
			M_thread_rwlock_unlock(ep->sessions_rwlock);
			return M_FALSE;
		}
	}
	M_net_smtp_session_dispatch_msg(session, args);
	M_list_insert(ep->send_sessions, session);
	M_thread_rwlock_unlock(ep->sessions_rwlock);
	return M_TRUE;
}

M_net_smtp_endpoint_t * M_net_smtp_endpoint_create_tcp(M_net_smtp_endpoint_tcp_args_t *args)
{
	M_net_smtp_endpoint_t *ep = M_malloc_zero(sizeof(*ep));

	ep->type                  = M_NET_SMTP_EPTYPE_TCP;
	ep->max_sessions          = args->max_conns;
	ep->send_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->idle_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->cull_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->sessions_rwlock       = M_thread_rwlock_create();
	ep->tcp.address           = M_strdup(args->address);
	ep->tcp.username          = M_strdup(args->username);
	ep->tcp.password          = M_strdup(args->password);
	ep->tcp.port              = args->port;
	ep->tcp.connect_tls       = args->connect_tls;

	return ep;

}

M_net_smtp_endpoint_t * M_net_smtp_endpoint_create_proc(M_net_smtp_endpoint_proc_args_t *args)
{
	M_net_smtp_endpoint_t *ep = M_malloc_zero(sizeof(*ep));

	ep->type                  = M_NET_SMTP_EPTYPE_PROCESS;
	ep->send_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->idle_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->cull_sessions         = M_list_create(NULL, M_LIST_NONE);
	ep->sessions_rwlock       = M_thread_rwlock_create();
	ep->max_sessions          = args->max_processes;
	ep->process.command       = M_strdup(args->command);
	ep->process.args          = M_list_str_duplicate(args->args);
	ep->process.env           = M_hash_dict_duplicate(args->env);
	ep->process.timeout_ms    = args->timeout_ms;

	return ep;
}

M_bool M_net_smtp_endpoint_destroy_is_ready(const M_net_smtp_endpoint_t *ep)
{
	M_bool is_ready = M_FALSE;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_READ);

	is_ready =
		M_list_len(ep->send_sessions) == 0 &&
		M_list_len(ep->idle_sessions) == 0 &&
		M_list_len(ep->cull_sessions) == 0;

	M_thread_rwlock_unlock(ep->sessions_rwlock);
	return is_ready;
}

void M_net_smtp_endpoint_destroy(M_net_smtp_endpoint_t *ep)
{
	M_net_smtp_session_t *session;
	M_thread_rwlock_lock(ep->sessions_rwlock, M_THREAD_RWLOCK_TYPE_WRITE);

	session = M_list_take_last(ep->send_sessions);
	while (session != NULL) {
		M_net_smtp_session_destroy(session, M_FALSE);
		session = M_list_take_last(ep->send_sessions);
	}

	session = M_list_take_last(ep->idle_sessions);
	while (session != NULL) {
		M_net_smtp_session_destroy(session, M_FALSE);
		session = M_list_take_last(ep->idle_sessions);
	}

	session = M_list_take_last(ep->cull_sessions);
	while (session != NULL) {
		M_net_smtp_session_destroy(session, M_FALSE);
		session = M_list_take_last(ep->cull_sessions);
	}

	/* These lists have to be destroyed AFTER all sessions are destroyed.
		* They can't be mixed in. */
	M_list_destroy(ep->send_sessions, M_TRUE);
	M_list_destroy(ep->idle_sessions, M_TRUE);
	M_list_destroy(ep->cull_sessions, M_TRUE);

	M_thread_rwlock_unlock(ep->sessions_rwlock);
	M_thread_rwlock_destroy(ep->sessions_rwlock);
	switch (ep->type) {
		case M_NET_SMTP_EPTYPE_PROCESS:
			M_free(ep->process.command);
			M_list_str_destroy(ep->process.args);
			M_hash_dict_destroy(ep->process.env);
			break;
		case M_NET_SMTP_EPTYPE_TCP:
			M_free(ep->tcp.address);
			M_free(ep->tcp.username);
			M_free(ep->tcp.password);
			break;
	}
	M_free(ep);
}
