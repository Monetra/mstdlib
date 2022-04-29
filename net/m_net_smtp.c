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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_smtp {
	M_event_t                         *el;
	struct M_net_smtp_callbacks        cbs;
	void                              *thunk;
	M_list_t                          *proc_endpoints;
	M_list_t                          *tcp_endpoints;
	size_t                             number_of_endpoints;
	M_list_t                          *endpoint_managers;
	M_net_smtp_status_t                status;
	M_dns_t                           *tcp_dns;
	M_tls_clientctx_t                 *tcp_tls_ctx;
	M_uint64                           tcp_connect_ms;
	M_uint64                           tcp_stall_ms;
	M_uint64                           tcp_idle_ms;
	M_net_smtp_load_balance_t          load_balance_mode;
	size_t                             round_robin_idx;
	size_t                             number_of_attempts;
	size_t                             number_of_processing_messages;
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
	M_io_t            *io;
	M_state_machine_t *state_machine;
	char              *msg;

	/* Only used for proc endpoints */
	M_io_t            *io_stdin;
	M_io_t            *io_stdout;
	M_io_t            *io_stderr;
} endpoint_slot_t;

typedef struct {
	const proc_endpoint_t *proc_endpoint;
	const tcp_endpoint_t  *tcp_endpoint;
	size_t                 slot_count;
	size_t                 slot_available;
	endpoint_slot_t        slots[];
} endpoint_manager_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool is_stopped(M_net_smtp_t *sp)
{
	return (
		sp->status == M_NET_SMTP_STATUS_NOENDPOINTS ||
		sp->status == M_NET_SMTP_STATUS_STOPPED
		);
}

static M_bool is_running(M_net_smtp_t *sp)
{
	return (
		sp->status == M_NET_SMTP_STATUS_PROCESSING ||
		sp->status == M_NET_SMTP_STATUS_IDLE
	);
}

static M_bool is_pending(M_net_smtp_t *sp)
{
	if (sp->is_external_queue_enabled) {
		return sp->is_external_queue_pending;
	}
	return (M_list_str_len(sp->internal_queue) > 0);
}

static M_bool is_available_failover(M_net_smtp_t *sp)
{
	const endpoint_manager_t *epm = NULL;
	for (size_t i = 0; i < M_list_len(sp->endpoint_managers); i++) {
		epm = M_list_at(sp->endpoint_managers, i);
		if (epm->slot_available > 0) {
			return M_TRUE;
		}
	}
	return M_FALSE;
}

static M_bool is_available_round_robin(M_net_smtp_t *sp)
{
	const endpoint_manager_t *epm = NULL;
	epm = M_list_at(sp->endpoint_managers, sp->round_robin_idx);
	return (epm->slot_available > 0);
}

static M_bool is_available(M_net_smtp_t *sp)
{
	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			return is_available_failover(sp);
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			return is_available_round_robin(sp);
			break;
	}
	return M_FALSE;
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	SM_PROC_WAIT = 1,
	SM_PROC_WRITING,
	SM_PROC_WRITE_DONE,
	SM_PROC_ERROR,
} sm_proc_states_t;

static M_state_machine_status_t sm_proc_wait(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;
	(void)next;

	M_printf("sm_proc_wait: { %p, %p, %s, %p, %p, %p }\n", slot->io, slot->state_machine, slot->msg, slot->io_stdin, slot->io_stdout, slot->io_stderr);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t sm_proc_writing(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t sm_proc_write_done(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t sm_proc_error(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_t * build_proc_state_machine()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-process-endpoint-slot", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, SM_PROC_WAIT, 0, NULL, sm_proc_wait, NULL, NULL);
	M_state_machine_insert_state(m, SM_PROC_WRITING, 0, NULL, sm_proc_writing, NULL, NULL);
	M_state_machine_insert_state(m, SM_PROC_WRITE_DONE, 0, NULL, sm_proc_write_done, NULL, NULL);
	M_state_machine_insert_state(m, SM_PROC_ERROR, 0, NULL, sm_proc_error, NULL, NULL);
	return m;
}

static void proc_io_callback(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	endpoint_slot_t *slot = thunk;
	M_printf("proc_io_callback(%p,%d,%p,%p)\n", el, etype, io, thunk);
	M_printf("{ io: %p, state_machine: %p, msg: %s, io_stdin: %p, io_stdout: %p, io_stderr: %p }\n", slot->io, slot->state_machine, slot->msg, slot->io_stdin, slot->io_stdout, slot->io_stderr);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	SM_TCP_ERROR = 1,
} sm_tcp_states_t;

static M_state_machine_status_t sm_tcp_error(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static M_state_machine_t * build_tcp_state_machine()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-tcp-endpoint-slot", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, SM_TCP_ERROR, 0, NULL, sm_tcp_error, NULL, NULL);
	return m;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void bootstrap_slot(M_net_smtp_t *sp, const proc_endpoint_t *proc_ep, const tcp_endpoint_t *tcp_ep,
		endpoint_slot_t *slot)
{
	M_printf("bootstrap_slot()\n");
	if (proc_ep != NULL) {
		M_io_process_create(
			proc_ep->command,
			proc_ep->args,
			proc_ep->env,
			proc_ep->timeout_ms,
			&slot->io,
			&slot->io_stdin,
			&slot->io_stdout,
			&slot->io_stderr
		);
		slot->state_machine = build_proc_state_machine();
		M_event_add(sp->el, slot->io, proc_io_callback, slot);
	} else {
		/* tcp_ip != NULL */
		M_io_net_client_create(
			&slot->io,
			sp->tcp_dns,
			tcp_ep->address,
			tcp_ep->port,
			M_IO_NET_ANY
		);
		slot->state_machine = build_tcp_state_machine();
	}
}

static void slate_msg_insert(M_net_smtp_t *sp, const endpoint_manager_t *const_epm, char *msg)
{
	endpoint_manager_t *epm  = M_CAST_OFF_CONST(endpoint_manager_t *,const_epm);
	endpoint_slot_t    *slot = NULL;
	for (size_t i = 0; i < epm->slot_count; i++) {
		slot = &epm->slots[i];
		if (slot->msg == NULL) {
			slot->msg = msg;
			if (slot->io == NULL) {
				bootstrap_slot(sp, epm->proc_endpoint, epm->tcp_endpoint, slot);
			}
			M_state_machine_run(epm->slots[i].state_machine, slot);
			epm->slot_available--;
			return;
		}
	}
}

static void slate_msg_failover(M_net_smtp_t *sp, char *msg)
{
	const endpoint_manager_t *epm = NULL;
	for (size_t i = 0; i < M_list_len(sp->endpoint_managers); i++) {
		epm = M_list_at(sp->endpoint_managers, i);
		if (epm->slot_available > 0) {
			slate_msg_insert(sp, epm, msg);
		}
	}
}

static void slate_msg_round_robin(M_net_smtp_t *sp, char *msg)
{
	const endpoint_manager_t *epm = NULL;
	epm = M_list_at(sp->endpoint_managers, sp->round_robin_idx);
	slate_msg_insert(sp, epm, msg);
	sp->round_robin_idx = (sp->round_robin_idx + 1) % sp->number_of_endpoints;
}

static void slate_msg(M_net_smtp_t *sp, char *msg)
{
	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			slate_msg_failover(sp, msg);
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			slate_msg_round_robin(sp, msg);
			break;
	}
	return;
}

static void process_internal_queue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t *sp  = cb_arg;
	char         *msg = NULL;

	/* This is an internally sent event; don't need these: */
	(void)event;
	(void)io;
	(void)type;

	if (is_available(sp)) {
		msg = M_list_str_take_first(sp->internal_queue);
		if (msg == NULL)
			return;
		slate_msg(sp, msg);
		sp->number_of_processing_messages++;
	}
}

static void process_external_queue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t *sp  = cb_arg;
	char         *msg = NULL;

	/* This is an internally sent event; don't need these: */
	(void)event;
	(void)io;
	(void)type;

	if (is_available(sp)) {
		msg = sp->external_queue_get_cb();
		if (msg == NULL)
			return;

		slate_msg(sp, msg);
		sp->number_of_processing_messages++;
	}
}

M_bool M_net_smtp_resume(M_net_smtp_t *sp)
{
	size_t                 proc_i   = 0;
	size_t                 tcp_i    = 0;
	const proc_endpoint_t *proc_ep  = NULL;
	const tcp_endpoint_t  *tcp_ep   = NULL;
	endpoint_manager_t    *epm      = NULL;

	if (sp == NULL || sp->status != M_NET_SMTP_STATUS_STOPPED)
		return M_FALSE;

	proc_ep = M_list_at(sp->proc_endpoints, proc_i);
	tcp_ep = M_list_at(sp->tcp_endpoints, tcp_i);
	for (size_t i = 0; i < sp->number_of_endpoints; i++) {
		if (proc_ep != NULL && proc_ep->idx == i) {
			size_t slot_count = proc_ep->max_processes;
			epm = M_malloc_zero(sizeof(*epm) + sizeof(epm->slots[0]) * slot_count);
			epm->proc_endpoint = proc_ep;
			epm->slot_count = slot_count;
			epm->slot_available = slot_count;
			M_list_insert(sp->endpoint_managers, epm);
			proc_i++;
			proc_ep = M_list_at(sp->proc_endpoints, proc_i);
			continue;
		}
		if (tcp_ep != NULL && tcp_ep->idx == i) {
			size_t slot_count = tcp_ep->max_conns;
			epm = M_malloc_zero(sizeof(*epm) + sizeof(epm->slots[0]) * slot_count);
			epm->tcp_endpoint = tcp_ep;
			epm->slot_count = slot_count;
			epm->slot_available = slot_count;
			M_list_insert(sp->endpoint_managers, epm);
			tcp_i++;
			tcp_ep = M_list_at(sp->tcp_endpoints, tcp_i);
			continue;
		}
	}

	if (is_pending(sp)) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		if (sp->is_external_queue_enabled) {
			M_event_queue_task(sp->el, process_external_queue, sp);
		} else {
			M_event_queue_task(sp->el, process_internal_queue, sp);
		}
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

	if (sp == NULL || !is_stopped(sp))
		return;

	sp->tcp_dns = dns;
	if (sp->tcp_tls_ctx != NULL)
		M_tls_clientctx_destroy(sp->tcp_tls_ctx);

	M_tls_clientctx_upref(ctx);
	sp->tcp_tls_ctx = ctx;
}

void M_net_smtp_setup_tcp_timeouts(M_net_smtp_t *sp, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 idle_ms)
{

	if (sp == NULL || !is_stopped(sp))
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

	if (sp == NULL || !is_stopped(sp) || max_conns == 0)
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
	M_list_insert(sp->tcp_endpoints, ep);
	sp->number_of_endpoints++;
	if (sp->status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
	}
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

	if (sp == NULL || !is_stopped(sp) || max_processes == 0)
		return M_FALSE;

	ep = M_malloc_zero(sizeof(*ep));
	ep->idx = sp->number_of_endpoints;
	ep->command = M_strdup(command);
	ep->args = M_list_str_duplicate(args);
	ep->env = M_hash_dict_duplicate(env);
	ep->timeout_ms = timeout_ms;
	ep->max_processes = max_processes;
	M_list_insert(sp->proc_endpoints, ep);
	sp->number_of_endpoints++;
	if (sp->status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
	}
	return M_TRUE;
}

M_bool M_net_smtp_load_balance(M_net_smtp_t *sp, M_net_smtp_load_balance_t mode)
{
	if (sp == NULL || !is_stopped(sp))
		return M_FALSE;

	sp->load_balance_mode = mode;
	return M_TRUE;
}

void M_net_smtp_set_num_attempts(M_net_smtp_t *sp, size_t num)
{
	if (sp == NULL || !is_stopped(sp))
		return;
	sp->number_of_attempts = num;
}

M_list_str_t *M_net_smtp_dump_queue(M_net_smtp_t *sp)
{
	M_list_str_t *list;
	char         *msg;

	if (sp == NULL)
		return NULL;

	if (sp->is_external_queue_enabled) {
		list = M_list_str_create(M_LIST_STR_NONE);
		while ((msg = sp->external_queue_get_cb()) != NULL) {
			M_list_str_insert(list, msg);
		}
		return list;
	}

	list = sp->internal_queue;
	sp->internal_queue = M_list_str_create(M_LIST_STR_NONE);
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

	if (!M_list_str_insert(sp->internal_queue, msg))
		return M_FALSE;

	if (sp->status == M_NET_SMTP_STATUS_IDLE) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		M_event_queue_task(sp->el, process_internal_queue, sp);
	}
	return M_TRUE;
}

M_bool M_net_smtp_use_external_queue(M_net_smtp_t *sp, char *(*get_cb)(void))
{
	if (sp == NULL || get_cb == NULL || M_list_str_len(sp->internal_queue) != 0)
		return M_FALSE;

	sp->is_external_queue_enabled = M_TRUE;
	sp->external_queue_get_cb = get_cb;
	return M_TRUE;
}

void M_net_smtp_external_queue_have_messages(M_net_smtp_t *sp)
{
	sp->is_external_queue_pending = M_TRUE;
	if (sp->status == M_NET_SMTP_STATUS_IDLE) {
		sp->status = M_NET_SMTP_STATUS_PROCESSING;
		M_event_queue_task(sp->el, process_external_queue, sp);
	}
}
