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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_smtp {
	M_event_t                         *el;
	struct M_net_smtp_callbacks        cbs;
	void                              *thunk;
	M_list_t                          *proc_endpoints;
	M_list_t                          *tcp_endpoints;
	size_t                             number_of_endpoints;
	M_list_t                          *endpoint_threadpools;
	M_net_smtp_status_t                status;
	M_dns_t                           *tcp_dns;
	M_tls_clientctx_t                 *tcp_tls_ctx;
	M_uint64                           tcp_connect_ms;
	M_uint64                           tcp_stall_ms;
	M_uint64                           tcp_idle_ms;
	M_net_smtp_load_balance_t          load_balance_mode;
	size_t                             round_robin_idx;
	size_t                             number_of_attempts;
	M_list_str_t                      *internal_queue;
	M_bool                             is_external_queue_enabled;
	M_bool                             is_external_queue_pending;
	char *                           (*external_queue_get_cb)(void);
};


typedef struct {
	size_t                 idx;
	char                  *command;
	M_list_str_t          *args;
	M_hash_dict_t         *env;
	M_uint64               timeout_ms;
	size_t                 max_processes;
} proc_endpoint_t;

typedef struct {
	size_t                 idx;
	char                  *address;
	M_uint16               port;
	M_bool                 connect_tls;
	char                  *username;
	char                  *password;
	size_t                 max_conns;
} tcp_endpoint_t;

typedef struct {
	const proc_endpoint_t *proc_endpoint;
	const tcp_endpoint_t  *tcp_endpoint;
	M_threadpool_t        *threadpool;
	M_threadpool_parent_t *threadpool_parent;
} endpoint_threadpool_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool is_running(M_net_smtp_t *sp)
{
	return (
		sp->status == M_NET_SMTP_STATUS_PROCESSING ||
		sp->status == M_NET_SMTP_STATUS_IDLE       ||
		sp->status == M_NET_SMTP_STATUS_STOPPING
	);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_smtp_t *M_net_smtp_create(M_event_t *el, const struct M_net_smtp_callbacks *cbs, void *thunk)
{
	M_net_smtp_t *sp;

	if (el == NULL)
		return NULL;

	sp = M_malloc_zero(sizeof(*sp));
	sp->el = el;

	if (cbs != NULL) {
		M_mem_move(&sp->cbs, cbs, sizeof(sp->cbs));
	}

	sp->thunk = thunk;
	sp->status = M_NET_SMTP_STATUS_NOENDPOINTS;

	sp->proc_endpoints = M_list_create(NULL, M_LIST_NONE);
	sp->tcp_endpoints = M_list_create(NULL, M_LIST_NONE);
	sp->internal_queue = M_list_str_create(M_LIST_STR_NONE);

	return sp;
}

static void destroy_proc_endpoint(const proc_endpoint_t *ep)
{
	M_free(ep->command);
	M_list_str_destroy(ep->args);
	M_hash_dict_destroy(ep->env);
}

static void destroy_tcp_endpoint(const tcp_endpoint_t *ep)
{
	M_free(ep->address);
	M_free(ep->username);
	M_free(ep->password);
}

void M_net_smtp_destroy(M_net_smtp_t *sp)
{
	if (sp == NULL)
		return;

	for (size_t i = 0; i < M_list_len(sp->proc_endpoints); i++) {
		destroy_proc_endpoint(M_list_at(sp->proc_endpoints, i));
	}
	M_list_destroy(sp->proc_endpoints, M_TRUE);
	for (size_t i = 0; i < M_list_len(sp->tcp_endpoints); i++) {
		destroy_tcp_endpoint(M_list_at(sp->tcp_endpoints, i));
	}
	M_list_destroy(sp->tcp_endpoints, M_TRUE);
	M_tls_clientctx_destroy(sp->tcp_tls_ctx);
	M_free(sp);
}

void M_net_smtp_pause(M_net_smtp_t *sp)
{
	if (sp == NULL || !is_running(sp))
		return;

	sp->status = M_NET_SMTP_STATUS_STOPPING;
	sp->status = M_NET_SMTP_STATUS_STOPPED;
}

static M_bool is_pending(M_net_smtp_t *sp)
{
	if (sp->is_external_queue_enabled) {
		return sp->is_external_queue_pending;
	}
	return (M_list_str_len(sp->internal_queue) > 0);
}

typedef struct {
	char                  *msg;
	const proc_endpoint_t *proc_endpoint;
	const tcp_endpoint_t  *tcp_endpoint;
} task_arg_t;

static void task_finished(void * arg)
{
	M_free(arg);
}

static task_arg_t * task_arg_alloc(char *msg, const proc_endpoint_t *proc_ep, const tcp_endpoint_t *tcp_ep)
{
	task_arg_t *arg = M_malloc_zero(sizeof(*arg));
	arg->msg = msg;
	arg->proc_endpoint = proc_ep;
	arg->tcp_endpoint = tcp_ep;
	return arg;
}

static void task(void *thunk)
{
	task_arg_t *arg = thunk;
	M_printf("task(%s)\n", arg->msg);
}

static M_bool dispatch_queue_task_failover(M_net_smtp_t *sp, char *msg)
{
	const endpoint_threadpool_t *eptp = NULL;
	for (size_t i = 0; i < sp->number_of_endpoints; i++) {
		eptp = M_list_at(sp->endpoint_threadpools, i);
		size_t available = M_threadpool_available_slots(eptp->threadpool);
		if (available > 0) {
			task_arg_t *arg = task_arg_alloc(msg, eptp->proc_endpoint, eptp->tcp_endpoint);
			M_threadpool_dispatch_notify(eptp->threadpool_parent, task, (void**)&arg, 1, &task_finished);
			return M_TRUE;
		}
	}
	return M_FALSE;
}

static M_bool dispatch_queue_task_roundrobin(M_net_smtp_t *sp, char *msg)
{
	size_t                       idx       = sp->round_robin_idx;
	const endpoint_threadpool_t *eptp      = NULL;
	size_t                       available = 0;

	eptp = M_list_at(sp->endpoint_threadpools, idx);
	available = M_threadpool_available_slots(eptp->threadpool);
	if (available > 0) {
		task_arg_t *arg = task_arg_alloc(msg, eptp->proc_endpoint, eptp->tcp_endpoint);
		M_threadpool_dispatch_notify(eptp->threadpool_parent, task, (void**)&arg, 1, &task_finished);
		sp->round_robin_idx = (idx + 1) % sp->number_of_endpoints;
		return M_TRUE;
	}
	return M_FALSE;
}

static M_bool dispatch_queue_task(M_net_smtp_t *sp, char *msg)
{
	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			return dispatch_queue_task_failover(sp, msg);
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			return dispatch_queue_task_roundrobin(sp, msg);
			break;
	}
	return M_FALSE;
}

static void dispatch_queue(M_net_smtp_t *sp)
{
	char* msg;
	if (sp->is_external_queue_enabled) {
		do {
			msg = sp->external_queue_get_cb();
			if (msg == NULL) {
				sp->is_external_queue_pending = M_FALSE;
				break;
			}
		} while(dispatch_queue_task(sp, msg));
		return;
	}
	do {
		msg = M_list_str_take_first(sp->internal_queue);
		if (msg == NULL) {
			break;
		}
	} while(dispatch_queue_task(sp, msg));
}

M_bool M_net_smtp_resume(M_net_smtp_t *sp)
{
	size_t                 proc_i   = 0;
	size_t                 tcp_i    = 0;
	const proc_endpoint_t *proc_ep  = NULL;
	const tcp_endpoint_t  *tcp_ep   = NULL;
	endpoint_threadpool_t *eptp     = NULL;

	if (sp == NULL || sp->status != M_NET_SMTP_STATUS_STOPPED)
		return M_FALSE;

	proc_ep = M_list_at(sp->proc_endpoints, proc_i);
	tcp_ep = M_list_at(sp->tcp_endpoints, tcp_i);
	for (size_t i = 0; i < sp->number_of_endpoints; i++) {
		if (proc_ep != NULL && proc_ep->idx == i) {
			M_uint64 idle_time_ms = proc_ep->timeout_ms == 0 ? 1000 : 3 * proc_ep->timeout_ms;
			eptp = M_malloc_zero(sizeof(*eptp));
			eptp->proc_endpoint = proc_ep;
			eptp->threadpool = M_threadpool_create(0, proc_ep->max_processes, idle_time_ms, proc_ep->max_processes + 1);
			eptp->threadpool_parent = M_threadpool_parent_create(eptp->threadpool);
			M_list_insert(sp->endpoint_threadpools, eptp);
			proc_i++;
			proc_ep = M_list_at(sp->proc_endpoints, proc_i);
			continue;
		}
		if (tcp_ep != NULL && tcp_ep->idx == i) {
			M_uint64 idle_time_ms = 1000;
			eptp = M_malloc_zero(sizeof(*eptp));
			eptp->tcp_endpoint = tcp_ep;
			eptp->threadpool = M_threadpool_create(0, tcp_ep->max_conns, idle_time_ms, tcp_ep->max_conns + 1);
			eptp->threadpool_parent = M_threadpool_parent_create(eptp->threadpool);
			M_list_insert(sp->endpoint_threadpools, eptp);
			tcp_i++;
			tcp_ep = M_list_at(sp->tcp_endpoints, tcp_i);
		}
	}

	if (is_pending(sp)) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		dispatch_queue(sp);
	} else {
		sp->status = M_NET_SMTP_STATUS_IDLE;
	}
	return M_TRUE;
}

M_net_smtp_status_t M_net_smtp_status(const M_net_smtp_t *sp)
{
	if (sp == NULL)
		return M_NET_SMTP_STATUS_NOENDPOINTS;
	return sp->status;
}

void M_net_smtp_setup_tcp(M_net_smtp_t *sp, M_dns_t *dns, M_tls_clientctx_t *ctx)
{

	if (sp == NULL || is_running(sp))
		return;

	sp->tcp_dns = dns;
	if (sp->tcp_tls_ctx != NULL)
		M_tls_clientctx_destroy(sp->tcp_tls_ctx);

	M_tls_clientctx_upref(ctx);
	sp->tcp_tls_ctx = ctx;
}

void M_net_smtp_setup_tcp_timeouts(M_net_smtp_t *sp, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 idle_ms)
{

	if (sp == NULL || is_running(sp))
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
	tcp_endpoint_t *ep = NULL;

	if (sp == NULL || is_running(sp))
		return M_FALSE;

	if (sp->tcp_dns == NULL || sp->tcp_tls_ctx == NULL)
		return M_FALSE;

	ep = M_malloc_zero(sizeof(*ep));
	ep->idx = sp->number_of_endpoints;
	ep->address = M_strdup(address);
	ep->port = port;
	ep->connect_tls = connect_tls;
	ep->username = M_strdup(username);
	ep->password = M_strdup(password);
	ep->max_conns = max_conns;
	sp->number_of_endpoints++;
	sp->status = M_NET_SMTP_STATUS_STOPPED;
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
	proc_endpoint_t *ep = NULL;

	if (sp == NULL || is_running(sp))
		return M_FALSE;

	ep = M_malloc_zero(sizeof(*ep));
	ep->idx = sp->number_of_endpoints;
	ep->command = M_strdup(command);
	ep->args = M_list_str_duplicate(args);
	ep->env = M_hash_dict_duplicate(env);
	ep->timeout_ms = timeout_ms;
	ep->max_processes = max_processes;
	sp->number_of_endpoints++;
	sp->status = M_NET_SMTP_STATUS_STOPPED;
	return M_TRUE;
}

M_bool M_net_smtp_load_balance(M_net_smtp_t *sp, M_net_smtp_load_balance_t mode)
{
	if (sp == NULL || is_running(sp))
		return M_FALSE;

	sp->load_balance_mode = mode;
	return M_TRUE;
}

void M_net_smtp_set_num_attempts(M_net_smtp_t *sp, size_t num)
{
	if (sp == NULL || is_running(sp))
		return;
	sp->number_of_attempts = num;
}

M_list_str_t *M_net_smtp_dump_queue(M_net_smtp_t *sp)
{
	M_list_str_t *list;
	char         *msg;
	size_t        len;

	if (sp == NULL)
		return NULL;

	list = M_list_str_create(M_LIST_STR_NONE);

	if (sp->is_external_queue_enabled) {
		while ((msg = sp->external_queue_get_cb()) != NULL) {
			M_list_str_insert(list, msg);
		}
		return list;
	}

	len = M_list_str_len(sp->internal_queue);
	for (size_t i = 0; i < len; i++) {
		msg = M_list_str_take_first(sp->internal_queue);
		M_list_str_insert(list, msg);
		M_free(msg);
	}
	return list;
}

M_bool M_net_smtp_queue_smtp(M_net_smtp_t *sp, const M_email_t *e)
{
	char   *msg;
	M_bool  is_success;

	if (sp == NULL || sp->is_external_queue_enabled) 
		return M_FALSE;

	msg = M_email_simple_write(e);
	is_success = M_net_smtp_queue_message(sp, msg);
	M_free(msg);

	return is_success;
}


M_bool M_net_smtp_queue_message(M_net_smtp_t *sp, const char *msg)
{
	if (sp == NULL || sp->is_external_queue_enabled)
		return M_FALSE;

	return M_list_str_insert(sp->internal_queue, msg);
}

M_bool M_net_smtp_use_external_queue(M_net_smtp_t *sp, char *(*get_cb)(void))
{
	if (sp == NULL || M_list_str_len(sp->internal_queue) != 0)
		return M_FALSE;

	sp->is_external_queue_enabled = M_TRUE;
	sp->external_queue_get_cb = get_cb;
	return M_TRUE;
}

void M_net_smtp_external_queue_have_messages(M_net_smtp_t *sp)
{
	sp->is_external_queue_pending = M_TRUE;
	sp->status = M_NET_SMTP_STATUS_PROCESSING;
}
