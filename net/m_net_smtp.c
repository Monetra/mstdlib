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
#include "smtp/m_flow.h" /* State machines */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_net_smtp {
	M_event_t                         *el;
	struct M_net_smtp_callbacks        cbs;
	void                              *thunk;
	M_list_t                          *endpoints;
	M_net_smtp_status_t                status;
	size_t                             retry_default_ms;
	M_dns_t                           *tcp_dns;
	M_tls_clientctx_t                 *tcp_tls_ctx;
	M_uint64                           tcp_connect_ms;
	M_uint64                           tcp_stall_ms;
	M_uint64                           tcp_idle_ms;
	M_net_smtp_load_balance_t          load_balance_mode;
	size_t                             round_robin_idx;
	size_t                             max_number_of_attempts;
	M_list_t                          *timers;
	M_list_t                          *retry_queue;
	M_list_t                          *retry_timeout_queue;
	M_list_str_t                      *internal_queue;
	M_bool                             is_external_queue_enabled;
	M_bool                             is_external_queue_pending;
	M_event_timer_t                   *restart_processing_timer;
	char *                           (*external_queue_get_cb)(void);
};

typedef struct {
	M_net_smtp_t    *sp;
	char            *msg;
	M_hash_dict_t   *headers;
	size_t           number_of_tries;
	M_event_timer_t *event_timer;
} retry_msg_t;

typedef struct {
	M_net_smtp_endpoint_type_t  type;
	size_t                      slot_count;
	size_t                      slot_available;
	M_bool                      is_alive;
	M_bool                      is_removed;
	union {
		struct {
			char          *address;
			M_uint16       port;
			M_bool         connect_tls;
			char          *username;
			char          *password;
			size_t         address_len;
			size_t         password_len;
			size_t         username_len;
			char          *auth_plain;
			char          *auth_login_user;
			char          *auth_login_pass;
			size_t         max_conns;
		} tcp;
		struct {
			char          *command;
			M_list_str_t  *args;
			M_hash_dict_t *env;
			M_uint64       timeout_ms;
			size_t         max_processes;
		} process;
	};
	M_net_smtp_endpoint_slot_t  slots[];
} endpoint_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* forward declarations */
static void process_queue_queue(M_net_smtp_t *sp);
static void processing_halted(M_net_smtp_t *sp);
static void proc_io_stdin_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk);
static int  process_external_queue_num(M_net_smtp_t *sp);

static M_bool is_running(M_net_smtp_t *sp)
{
	return (
		sp->status == M_NET_SMTP_STATUS_PROCESSING ||
		sp->status == M_NET_SMTP_STATUS_IDLE
	);
}

static M_bool is_pending(M_net_smtp_t *sp)
{
	if (M_list_len(sp->retry_queue) > 0)
		return M_TRUE;

	if (sp->is_external_queue_enabled) {
		return sp->is_external_queue_pending;
	}
	return (M_list_str_len(sp->internal_queue) > 0);
}

static M_bool is_available_failover(M_net_smtp_t *sp)
{
	const endpoint_t *ep = NULL;
	for (size_t i = 0; i < M_list_len(sp->endpoints); i++) {
		ep = M_list_at(sp->endpoints, i);

		if (!ep->is_removed)
			break;

		ep = NULL;
	}

	return ep != NULL && ep->slot_available > 0;
}

static M_bool is_available_round_robin(M_net_smtp_t *sp)
{
	size_t            len = M_list_len(sp->endpoints);
	const endpoint_t *ep  = NULL;

	for (size_t i = 0; i < len; i++) {
		size_t idx = (sp->round_robin_idx + i) % len;
		ep = M_list_at(sp->endpoints, idx);
		if (ep->is_removed)
			continue;
		if (ep->slot_available > 0)
			return M_TRUE;
	}

	return M_FALSE;
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

static void destroy_endpoint(endpoint_t *ep)
{
	if (ep->type == M_NET_SMTP_EPTYPE_PROCESS) {
		M_free(ep->process.command);
		M_list_str_destroy(ep->process.args);
		M_hash_dict_destroy(ep->process.env);
	} else {
		/* TCP endpoint */
		M_free(ep->tcp.address);
		M_free(ep->tcp.username);
		M_free(ep->tcp.password);
		M_free(ep->tcp.auth_plain);
		M_free(ep->tcp.auth_login_user);
		M_free(ep->tcp.auth_login_pass);
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

M_net_smtp_t *M_net_smtp_create(M_event_t *el, const struct M_net_smtp_callbacks *cbs, void *thunk)
{
	M_net_smtp_t *sp;

	if (el == NULL)
		return NULL;

	sp                      = M_malloc_zero(sizeof(*sp));
	sp->internal_queue      = M_list_str_create(M_LIST_STR_NONE);
	sp->endpoints           = M_list_create(NULL, M_LIST_NONE);
	sp->retry_queue         = M_list_create(NULL, M_LIST_NONE);
	sp->retry_timeout_queue = M_list_create(NULL, M_LIST_NONE);
	sp->timers              = M_list_create(NULL, M_LIST_NONE);

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
	endpoint_t      *ep    = NULL;
	M_event_timer_t *timer = NULL;
	retry_msg_t     *retry = NULL;

	if (sp == NULL)
		return;

	while ((ep = M_list_take_last(sp->endpoints)) != NULL) {
		destroy_endpoint(ep);
	}
	M_list_destroy(sp->endpoints, M_TRUE);
	while ((timer = M_list_take_last(sp->timers)) != NULL) {
		M_event_timer_remove(timer);
	}
	M_list_destroy(sp->timers, M_TRUE);
	while (
		(retry = M_list_take_last(sp->retry_queue))         != NULL ||
		(retry = M_list_take_last(sp->retry_timeout_queue)) != NULL
	) {
		M_free(retry->msg);
		M_free(retry);
	}
	M_list_destroy(sp->retry_queue, M_TRUE);
	M_list_destroy(sp->retry_timeout_queue, M_TRUE);
	M_list_str_destroy(sp->internal_queue);
	M_tls_clientctx_destroy(sp->tcp_tls_ctx);
	M_free(sp);
}

void M_net_smtp_pause(M_net_smtp_t *sp)
{
	if (sp == NULL || !is_running(sp))
		return;

	if (sp->status == M_NET_SMTP_STATUS_IDLE) {
		processing_halted(sp);
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPING;
	}
}

static void reschedule_event_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	retry_msg_t  *retry = thunk;
	M_net_smtp_t *sp    = retry->sp;
	(void)el;
	(void)io;
	(void)etype;

	M_list_remove_val(sp->timers, retry->event_timer, M_LIST_MATCH_PTR);
	retry->event_timer = NULL;

	M_list_remove_val(sp->retry_timeout_queue, retry, M_LIST_MATCH_PTR);
	M_list_insert(sp->retry_queue, retry);
	process_queue_queue(sp);
}

static void reschedule_msg(M_net_smtp_t *sp, const char *msg, M_hash_dict_t *headers, M_bool is_backout, size_t num_tries, const char* errmsg)
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
		reschedule_cb(msg, (sp->retry_default_ms / 1000), sp->thunk); /* reschedule_cb arg is wait_sec */
		send_failed_cb(headers, errmsg, 0, M_FALSE, sp->thunk);
		return;
	}

	if (is_backout) {
		M_list_str_insert(sp->internal_queue, msg);
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
		retry->event_timer = M_event_timer_oneshot(sp->el, sp->retry_default_ms, M_TRUE,
				reschedule_event_cb, retry);
		M_list_insert(sp->retry_timeout_queue, retry);
		M_list_insert(sp->timers, retry->event_timer);
	}
}

static void reschedule_slot(const M_net_smtp_endpoint_slot_t *slot)
{
	reschedule_msg(slot->sp, slot->msg, slot->headers, slot->is_backout, slot->number_of_tries + 1, slot->errmsg);
}

static M_bool run_state_machine(M_net_smtp_endpoint_slot_t *slot, M_bool *is_done)
{
	M_state_machine_status_t result;

	result = M_state_machine_run(slot->state_machine, slot);
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

	if (slot->errmsg[0] == 0) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "State machine failure: %d", result);
	}
	return M_FALSE;
}

static void round_robin_skip_removed(M_net_smtp_t *sp)
{
	size_t            len = M_list_len(sp->endpoints);;
	const endpoint_t *ep;
	size_t            idx;
	for (size_t i = 0; i < len; i++) {
		idx = (sp->round_robin_idx + i) % len;
		ep = M_list_at(sp->endpoints, idx);
		if (!ep->is_removed) {
			sp->round_robin_idx = idx;
			return;
		}
	}
}

static void remove_endpoint(M_net_smtp_t *sp, endpoint_t *ep)
{
	M_bool            is_all_endpoints_removed = M_TRUE;
	const endpoint_t *const_ep                 = NULL;

	ep->is_removed = M_TRUE;
	for (size_t i = 0; i < M_list_len(sp->endpoints); i++) {
		const_ep = M_list_at(sp->endpoints, i);
		if (const_ep->is_removed == M_FALSE) {
			is_all_endpoints_removed = M_FALSE;
			break;
		}
	}

	if (is_all_endpoints_removed) {
		M_net_smtp_pause(sp);
		if (sp->status == M_NET_SMTP_STATUS_STOPPING) {
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
	endpoint_t *ep;

	if (sp->load_balance_mode != M_NET_SMTP_LOAD_BALANCE_FAILOVER)
		return;

	if (M_list_len(sp->endpoints) <= 1)
		return;

	ep = M_list_take_first(sp->endpoints);
	M_list_insert(sp->endpoints, ep);
}

static void process_fail(M_net_smtp_endpoint_slot_t *slot, const char *stdout_str)
{
	const endpoint_t *const_ep = slot->endpoint;

	if (slot->sp->cbs.process_fail_cb(
		const_ep->process.command,
		slot->process.result_code,
		stdout_str,
		slot->errmsg,
		slot->sp->thunk)
	) {
		remove_endpoint(slot->sp, M_CAST_OFF_CONST(endpoint_t*, const_ep));
	} else {
		/* Had a failure, but they want to keep the endpoint. */
		rotate_endpoints(slot->sp);
	}
}

static void clean_slot_no_process_queue_queue(M_net_smtp_endpoint_slot_t *slot)
{
	endpoint_t *ep = M_CAST_OFF_CONST(endpoint_t*, slot->endpoint);

	if (slot->msg == NULL)
		return;

	if (slot->is_backout || slot->is_failure) {
		reschedule_slot(slot);
	} else {
		slot->sp->cbs.sent_cb(slot->headers, slot->sp->thunk);
	}
	M_email_destroy(slot->email);
	slot->email = NULL;
	M_free(slot->msg);
	slot->msg = NULL;
	M_hash_dict_destroy(slot->headers);
	slot->headers = NULL;
	ep->slot_available++;
}

static void clean_slot(M_net_smtp_endpoint_slot_t *slot)
{
	clean_slot_no_process_queue_queue(slot);
	if (!slot->sp->is_external_queue_enabled) {
		process_queue_queue(slot->sp);
		return;
	}
	/* eager eval external queue to determine IDLE */
	if (process_external_queue_num(slot->sp) == 0)
		process_queue_queue(slot->sp);
}

static void destroy_slot(M_net_smtp_endpoint_slot_t *slot)
{
	M_net_smtp_t *sp = slot->sp;

	clean_slot_no_process_queue_queue(slot);
	M_event_timer_remove(slot->event_timer);
	slot->event_timer = NULL;
	M_buf_cancel(slot->out_buf);
	slot->out_buf = NULL;
	M_parser_destroy(slot->in_parser);
	slot->in_parser = NULL;
	M_state_machine_destroy(slot->state_machine);
	slot->state_machine = NULL;
	slot->is_alive = M_FALSE;
	if (!slot->sp->is_external_queue_enabled) {
		process_queue_queue(slot->sp);
		return;
	}
	/* eager eval external queue to determine IDLE */
	if (process_external_queue_num(slot->sp) == 0)
		process_queue_queue(sp);
}

static void proc_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk, unsigned int connection_mask)
{
	M_net_smtp_endpoint_slot_t *slot     = thunk;
	M_io_error_t                io_error = M_IO_ERROR_SUCCESS;
	M_io_t                    **slot_io  = NULL;
	size_t                      len;
	M_bool                      is_done;
	(void)el;

	switch(etype) {
		case M_EVENT_TYPE_CONNECTED:
			slot->connection_mask |= connection_mask;
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			if (io == slot->io) {
				if (!M_io_process_get_result_code(io, &slot->process.result_code)) {
					slot->is_failure = M_TRUE;
					if (slot->errmsg[0] == 0) {
						M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Error getting result code");
					}
				}
				if (slot->process.result_code != 0) {
					char *stdout_str = M_buf_finish_str(slot->out_buf, NULL);
					slot->out_buf = NULL;
					process_fail(slot, stdout_str);
					M_free(stdout_str);
					slot->is_failure = M_TRUE;
					if (slot->errmsg[0] == 0) {
						M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Bad result code %d", slot->process.result_code);
					}
				}
			}
			goto destroy;
			break;
		case M_EVENT_TYPE_READ:
			if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDERR) {
				io_error = M_io_read(io, (unsigned char *)slot->errmsg, sizeof(slot->errmsg) - 1, &len);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
					M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Read failure: %s", M_io_error_string(io_error));
					goto destroy;
				}
				slot->errmsg[len] = '\0';
				goto destroy;
			}
			if (connection_mask == M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) {
				io_error = M_io_read_into_parser(io, slot->in_parser);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
					M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Read failure: %s", M_io_error_string(io_error));
				}
				goto destroy; /* shouldn't receive anything on stdout */
			}
			M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_WRITE:
			if (connection_mask != M_NET_SMTP_CONNECTION_MASK_IO_STDIN) {
				M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Unexpected event: %s", M_event_type_string(etype));
				goto destroy;
			}
			if (M_buf_len(slot->out_buf) > 0) {
				io_error = M_io_write_from_buf(io, slot->out_buf);
				if (io_error == M_IO_ERROR_DISCONNECT) {
					goto destroy;
				}
				if (slot->is_failure) {
					/* Give process a chance to parse and react to input */
					slot->event_timer = M_event_timer_oneshot(slot->sp->el, 100, M_TRUE, proc_io_stdin_cb, slot);
				} else {
					slot->event_timer = NULL;
				}
			}
			if (slot->is_failure == M_FALSE) {
				M_io_disconnect(io);
			}
			return;
			break;
		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_ACCEPT:
			M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Unexpected event: %s", M_event_type_string(etype));
			goto destroy;
			break;
		case M_EVENT_TYPE_OTHER:
			break;
	}

	if (!run_state_machine(slot, &is_done) || is_done)
		goto destroy;

	if (M_buf_len(slot->out_buf) > 0) {
		M_io_layer_t *layer;
		layer = M_io_layer_acquire(slot->process.io_stdin, 0, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	}

	return;
destroy:
	switch (connection_mask) {
		case M_NET_SMTP_CONNECTION_MASK_IO:        slot_io = &slot->io;                break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDIN:  slot_io = &slot->process.io_stdin;  break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDOUT: slot_io = &slot->process.io_stdout; break;
		case M_NET_SMTP_CONNECTION_MASK_IO_STDERR: slot_io = &slot->process.io_stderr; break;
	}
	if (*slot_io != NULL) {
		M_io_destroy(io);
		slot->connection_mask &= ~connection_mask;
		if (slot->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			destroy_slot(slot);
		}
	}
	*slot_io = NULL;
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


static void connect_fail(M_net_smtp_endpoint_slot_t *slot)
{
	M_net_smtp_t      *sp       = slot->sp;
	const endpoint_t  *const_ep = slot->endpoint;

	if (sp->cbs.connect_fail_cb(
		const_ep->tcp.address,
		const_ep->tcp.port,
		slot->tcp.net_error,
		slot->errmsg,
		sp->thunk
	)) {
		remove_endpoint(sp, M_CAST_OFF_CONST(endpoint_t *, const_ep));
	} else {
		/* Had a failure, but they want to keep the endpoint. */
		rotate_endpoints(sp);
	}
}

static void tcp_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_endpoint_slot_t *slot            = thunk;
	M_net_smtp_t               *sp              = slot->sp;
	const endpoint_t           *const_ep        = slot->endpoint;
	M_net_smtp_connect_cb       connect_cb      = sp->cbs.connect_cb;
	M_net_smtp_iocreate_cb      iocreate_cb     = sp->cbs.iocreate_cb;
	M_io_error_t                io_error        = M_IO_ERROR_SUCCESS;
	M_bool                      is_done         = M_FALSE;
	(void)el;

	switch(etype) {
		case M_EVENT_TYPE_CONNECTED:

			if (slot->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
				/* STARTTLS has 2 CONNECTED events, only want to cb() once. */
				slot->connection_mask |= M_NET_SMTP_CONNECTION_MASK_IO;
				/* Sending iocreate here ensures we trace on the right one. */
				if (!iocreate_cb(io, slot->errmsg, sizeof(slot->errmsg), sp->thunk)) {
					slot->is_backout = M_TRUE;
					goto destroy;
				}
				connect_cb(const_ep->tcp.address, const_ep->tcp.port, sp->thunk);
				M_event_timer_reset(slot->event_timer, sp->tcp_stall_ms);
			}

			if (slot->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_ADDED || slot->tcp.tls_state == M_NET_SMTP_TLS_IMPLICIT) {
				slot->tcp.tls_state = M_NET_SMTP_TLS_CONNECTED;
				return;
			}

			break;
		case M_EVENT_TYPE_DISCONNECTED:
			goto destroy;
			break;
		case M_EVENT_TYPE_READ:
			io_error = M_io_read_into_parser(io, slot->in_parser);

			if (io_error == M_IO_ERROR_WOULDBLOCK)
				return;

			if (io_error == M_IO_ERROR_DISCONNECT) {
				goto destroy;
			}

			if (io_error != M_IO_ERROR_SUCCESS) {
				M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Read failure: %s", M_io_error_string(io_error));
				goto destroy;
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_ACCEPT:
			/* should be impossible */
			slot->tcp.net_error = M_NET_ERROR_PROTONOTSUPPORTED;
			M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Unsupported ACCEPT event");
			goto backout;
			break;
		case M_EVENT_TYPE_OTHER:
			if (slot->is_failure == M_FALSE) {
				/* Idle timeout */
				slot->tcp.is_QUIT_enabled = M_TRUE;
				break;
			}
		case M_EVENT_TYPE_ERROR:
			if (slot->tcp.tls_state == M_NET_SMTP_TLS_IMPLICIT && slot->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
				/* Implict TLS failed.  Follwup with with STARTTLS */
				slot->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS;
				M_io_destroy(io);
				io_error = M_io_net_client_create(&slot->io, sp->tcp_dns, const_ep->tcp.address,
						const_ep->tcp.port, M_IO_NET_ANY);
				if (io_error != M_IO_ERROR_SUCCESS) {
					goto destroy;
				}
				M_event_add(sp->el, slot->io, tcp_io_cb, slot);
				M_event_timer_reset(slot->event_timer, sp->tcp_connect_ms);
				return;
			}
			do {
				if (etype == M_EVENT_TYPE_OTHER) {
					if (slot->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
						slot->tcp.net_error = M_NET_ERROR_TIMEOUT;
						M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Connection timeout");
						break;
					}
					slot->tcp.net_error = M_NET_ERROR_TIMEOUT_STALL;
					M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Stall timeout");
					break;
				}
				M_io_get_error_string(io, slot->errmsg, sizeof(slot->errmsg));
				slot->tcp.net_error = M_net_io_error_to_net_error(M_io_get_error(io));
			} while(0);
			goto backout;
			return;
	}

	if (sp->status == M_NET_SMTP_STATUS_STOPPING)
		slot->tcp.is_QUIT_enabled = M_TRUE;

	if (!run_state_machine(slot, &is_done) || is_done) {
		if (slot->tcp.is_connect_fail) {
			goto backout;
		}
		goto destroy;
	}

	if (slot->is_failure == M_FALSE && slot->msg != NULL) {
		/* successfully sent message. get ready to accept another. */
		clean_slot(slot);
		M_event_timer_reset(slot->event_timer, sp->tcp_idle_ms);
	}

	if (slot->tcp.tls_state == M_NET_SMTP_TLS_STARTTLS_READY) {
		size_t        layer_id;
		M_io_layer_t *layer;
		M_io_tls_client_add(io, slot->sp->tcp_tls_ctx, NULL, &layer_id);
		layer = M_io_layer_acquire(io, layer_id, NULL);
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
		slot->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS_ADDED;
		return; /* short circuit out */
	}

	if (M_buf_len(slot->out_buf) > 0) {
		io_error = M_io_write_from_buf(io, slot->out_buf);
		if (io_error == M_IO_ERROR_DISCONNECT) {
			goto destroy;
		}
		if (io_error != M_IO_ERROR_SUCCESS && io_error != M_IO_ERROR_WOULDBLOCK) {
			M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Write failed: %s", M_io_error_string(io_error));
			goto destroy;
		}
	}

	return;
destroy:
	if (slot->io != NULL) {
		M_io_destroy(io);
		slot->connection_mask &= ~M_NET_SMTP_CONNECTION_MASK_IO;
		if (slot->connection_mask == M_NET_SMTP_CONNECTION_MASK_NONE) {
			M_bool is_backout = slot->is_backout;
			destroy_slot(slot);
			if (is_backout == M_FALSE) {
				sp->cbs.disconnect_cb(const_ep->tcp.address, const_ep->tcp.port, sp->thunk);
			}
		}
		slot->io = NULL;
	}
	return;
backout:
	connect_fail(slot);
	slot->is_backout = M_TRUE;
	goto destroy;
}

static M_bool bootstrap_proc_slot(M_net_smtp_t *sp, const endpoint_t *ep, M_net_smtp_endpoint_slot_t *slot)
{
	M_io_error_t io_error = M_IO_ERROR_SUCCESS;
	slot->endpoint_type = M_NET_SMTP_EPTYPE_PROCESS;

	io_error = M_io_process_create(
		ep->process.command,
		ep->process.args,
		ep->process.env,
		ep->process.timeout_ms,
		&slot->io,
		&slot->process.io_stdin,
		&slot->process.io_stdout,
		&slot->process.io_stderr
	);
	if (io_error != M_IO_ERROR_SUCCESS) {
		slot->process.result_code = (int)io_error;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "%s", M_io_error_string(io_error));
		process_fail(slot, "");
		return M_FALSE;
	}
	slot->state_machine = M_net_smtp_flow_process();
	M_event_add(sp->el, slot->io               , proc_io_proc_cb  , slot);
	M_event_add(sp->el, slot->process.io_stdin , proc_io_stdin_cb , slot);
	M_event_add(sp->el, slot->process.io_stdout, proc_io_stdout_cb, slot);
	M_event_add(sp->el, slot->process.io_stderr, proc_io_stderr_cb, slot);
	return M_TRUE;
}

static M_bool bootstrap_tcp_slot(M_net_smtp_t *sp, const endpoint_t *ep, M_net_smtp_endpoint_slot_t *slot)
{
	M_io_error_t io_error = M_IO_ERROR_SUCCESS;

	slot->endpoint_type = M_NET_SMTP_EPTYPE_TCP;

	io_error = M_io_net_client_create(&slot->io, sp->tcp_dns, ep->tcp.address, ep->tcp.port, M_IO_NET_ANY);

	if (io_error != M_IO_ERROR_SUCCESS)
		return M_FALSE;

	if (ep->tcp.connect_tls) {
		io_error = M_io_tls_client_add(slot->io, slot->sp->tcp_tls_ctx, NULL, &slot->tcp.tls_ctx_layer_idx);
		if (io_error != M_IO_ERROR_SUCCESS) {
			M_io_destroy(slot->io);
			slot->io = NULL;
			return M_FALSE;
		}
		slot->tcp.tls_state = M_NET_SMTP_TLS_IMPLICIT;
	}

	M_event_add(sp->el, slot->io, tcp_io_cb, slot);

	slot->event_timer          = M_event_timer_add(sp->el, tcp_io_cb, slot);
	slot->state_machine        = M_net_smtp_flow_tcp();

	slot->tcp.address          = ep->tcp.address;
	slot->tcp.username         = ep->tcp.username;
	slot->tcp.password         = ep->tcp.password;
	slot->tcp.address_len      = ep->tcp.address_len;
	slot->tcp.username_len     = ep->tcp.username_len;
	slot->tcp.password_len     = ep->tcp.password_len;
	slot->tcp.auth_plain       = ep->tcp.auth_plain;
	slot->tcp.auth_login_user  = ep->tcp.auth_login_user;
	slot->tcp.auth_login_pass  = ep->tcp.auth_login_pass;

	M_event_timer_start(slot->event_timer, sp->tcp_connect_ms);

	return M_TRUE;
}

static void bootstrap_slot(M_net_smtp_t *sp, const endpoint_t *ep, M_net_smtp_endpoint_slot_t *slot)
{
	slot->sp          = sp; /* need to be set first in case of sp->cbs.process_fail() */
	M_bool is_success = M_TRUE;

	if (ep->type == M_NET_SMTP_EPTYPE_PROCESS) {
		is_success = bootstrap_proc_slot(sp, ep, slot);
	} else {
		is_success = bootstrap_tcp_slot(sp, ep, slot);
	}

	if (!is_success)
		return;

	slot->out_buf   = M_buf_create();
	slot->in_parser = M_parser_create(M_PARSER_FLAG_NONE);
	slot->is_alive  = M_TRUE;
	return;
}

static void slate_msg_insert(M_net_smtp_t *sp, const endpoint_t *const_ep, char *msg, size_t num_tries, M_hash_dict_t *headers)
{
	endpoint_t                 *ep           = M_CAST_OFF_CONST(endpoint_t *,const_ep);
	M_net_smtp_endpoint_slot_t *slot         = NULL;
	M_email_t                  *e            = NULL;
	M_bool                      is_bootstrap = M_FALSE;

	e = M_email_create();

	if (!M_email_set_headers(e, headers))
		goto fail;

	for (size_t i = 0; i < ep->slot_count; i++) {
		slot = &ep->slots[i];
		slot->endpoint = ep;
		if (slot->msg == NULL) {
			if (slot->io == NULL) {
				bootstrap_slot(sp, ep, slot);
				is_bootstrap = M_TRUE;
			}
			if (slot->is_alive) {
				ep->is_alive = M_TRUE;
				slot->msg = msg;
				slot->number_of_tries = num_tries;
				slot->headers = headers;
				slot->is_failure = M_TRUE; /* will be unmarked as failure on success */
				M_mem_set(slot->errmsg, 0, sizeof(slot->errmsg));
				slot->email = e;
				if (slot->endpoint_type == M_NET_SMTP_EPTYPE_TCP) {
					slot->tcp.is_QUIT_enabled = (sp->tcp_idle_ms == 0);
					if (!is_bootstrap) {
						tcp_io_cb(slot->sp->el, M_EVENT_TYPE_WRITE, slot->io, slot);
					}
				}
				ep->slot_available--;
				return;
			}
		}
	}

	/* failed to bootstrap */

fail:
	M_email_destroy(e);
	reschedule_msg(sp, msg, headers, M_TRUE, num_tries, "Internal failure");
	M_free(msg);
	M_hash_dict_destroy(headers);
}

static void slate_msg_failover(M_net_smtp_t *sp, char *msg, size_t num_tries, M_hash_dict_t *headers)
{
	const endpoint_t *ep = NULL;
	for (size_t i = 0; i < M_list_len(sp->endpoints); i++) {
		ep = M_list_at(sp->endpoints, i);

		if (!ep->is_removed)
			break;

		ep = NULL;
	}

	if (ep && ep->slot_available > 0) {
		slate_msg_insert(sp, ep, msg, num_tries, headers);
	}
}

static void slate_msg_round_robin(M_net_smtp_t *sp, char *msg, size_t num_tries, M_hash_dict_t *headers)
{
	const endpoint_t *ep  = NULL;
	size_t            idx = 0;
	size_t            len = M_list_len(sp->endpoints);

	for (size_t i = 0; i < len; i++) {
		idx = (sp->round_robin_idx + i) % len;
		ep = M_list_at(sp->endpoints, idx);
		if (ep->is_removed) {
			ep = NULL;
			continue;
		}
		if (ep->slot_available > 0)
			break;
	}

	if (ep) {
		slate_msg_insert(sp, ep, msg, num_tries, headers);
		if (idx == sp->round_robin_idx) {
			sp->round_robin_idx = (idx + 1) % len;
			round_robin_skip_removed(sp);
		}
	}
}

static void slate_msg(M_net_smtp_t *sp, char *msg, size_t num_tries)
{
	M_hash_dict_t   *headers = NULL;

	if (M_email_simple_split_header_body(msg, &headers, NULL) != M_EMAIL_ERROR_SUCCESS) {
		sp->cbs.send_failed_cb(headers, msg, 0, M_FALSE, sp->thunk);
		return;
	}

	switch (sp->load_balance_mode) {
		case M_NET_SMTP_LOAD_BALANCE_FAILOVER:
			slate_msg_failover(sp, msg, num_tries, headers);
			break;
		case M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN:
			slate_msg_round_robin(sp, msg, num_tries, headers);
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

	while (is_available(sp)) {
		msg = M_list_str_take_first(sp->internal_queue);
		if (msg == NULL)
			return;
		slate_msg(sp, msg, 0);
	}
}

static int process_external_queue_num(M_net_smtp_t *sp)
{
	char *msg = NULL;
	int   n   = 0;

	while (is_available(sp)) {
		msg = sp->external_queue_get_cb();
		if (msg == NULL) {
			sp->is_external_queue_pending = M_FALSE;
			return n;
		}

		slate_msg(sp, msg, 0);
		n++;
	}
	return n;
}

static void process_external_queue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	(void)event;
	(void)io;
	(void)type;

	process_external_queue_num(cb_arg);
}

static void process_retry_queue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_net_smtp_t *sp    = cb_arg;
	retry_msg_t  *retry = NULL;

	/* This is an internally sent event; don't need these: */
	(void)event;
	(void)io;
	(void)type;

	while (is_available(sp)) {
		retry = M_list_take_first(sp->retry_queue);
		if (retry == NULL) {
			if (sp->is_external_queue_enabled) {
				process_external_queue(event, type, io, cb_arg);
			} else {
				process_internal_queue(event, type, io, cb_arg);
			}
			return;
		}
		slate_msg(sp, retry->msg, retry->number_of_tries);
		M_free(retry);
	}
}

static M_bool idle_check_endpoint(const endpoint_t *ep)
{
	const M_net_smtp_endpoint_slot_t *slot = NULL;
	for (size_t i = 0; i < ep->slot_count; i++) {
		slot = &ep->slots[i];
		if (slot->msg != NULL) {
			return M_FALSE;
		}
	}
	return M_TRUE;
}

static M_bool idle_check(M_net_smtp_t *sp)
{
	const endpoint_t *ep = NULL;
	for (size_t i = 0; i < M_list_len(sp->endpoints); i++) {
		ep = M_list_at(sp->endpoints, i);
		if (idle_check_endpoint(ep) == M_FALSE) {
			return M_FALSE;
		}
	}
	return M_TRUE;
}

static void restart_processing_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	M_net_smtp_t *sp = thunk;
	(void)el;
	(void)etype;
	(void)io;

	M_list_remove_val(sp->timers, sp->restart_processing_timer, M_LIST_MATCH_PTR);
	sp->restart_processing_timer = NULL;

	M_net_smtp_resume(sp);
}

static void prune_removed_endpoints(M_net_smtp_t *sp)
{
	for (size_t i = M_list_len(sp->endpoints); i > 0; i--) {
		const endpoint_t *ep = M_list_at(sp->endpoints, i - 1);
		if (ep->is_removed) {
			destroy_endpoint(M_list_take_at(sp->endpoints, i - 1));
		}
	}
}

static void processing_halted(M_net_smtp_t *sp)
{
	M_bool          is_no_endpoints;
	M_uint64        delay_ms;

	prune_removed_endpoints(sp);

	if (M_list_len(sp->endpoints) == 0) {
		sp->status = M_NET_SMTP_STATUS_NOENDPOINTS;
		is_no_endpoints = M_TRUE;
	} else {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
		is_no_endpoints = M_FALSE;
	}

	delay_ms = sp->cbs.processing_halted_cb(is_no_endpoints, sp->thunk);
	if (delay_ms == 0 || is_no_endpoints)
		return;

	sp->restart_processing_timer = M_event_timer_oneshot(sp->el, delay_ms, M_TRUE, restart_processing_cb, sp);
	M_list_insert(sp->timers, sp->restart_processing_timer);
}

static void process_queue_queue(M_net_smtp_t *sp)
{
	if (is_running(sp) && is_pending(sp)) {
		if (M_list_len(sp->retry_queue) > 0) {
			M_event_queue_task(sp->el, process_retry_queue, sp);
		} else if (sp->is_external_queue_enabled) {
			M_event_queue_task(sp->el, process_external_queue, sp);
		} else {
			M_event_queue_task(sp->el, process_internal_queue, sp);
		}
	} else if (idle_check(sp)) {
		if (sp->status == M_NET_SMTP_STATUS_STOPPING) {
			processing_halted(sp);
		} else {
			sp->status = M_NET_SMTP_STATUS_IDLE;
			prune_removed_endpoints(sp);
		}
	}
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_net_smtp_resume(M_net_smtp_t *sp)
{
	if (sp == NULL)
		return M_FALSE;

	switch (sp->status) {
		case M_NET_SMTP_STATUS_NOENDPOINTS:
			sp->cbs.processing_halted_cb(M_TRUE, sp->thunk);
			return M_FALSE;
			break;
		case M_NET_SMTP_STATUS_PROCESSING:
		case M_NET_SMTP_STATUS_IDLE:
			return M_TRUE;
			break;
		case M_NET_SMTP_STATUS_STOPPING:
			/* Actually, we're not stopping */
		case M_NET_SMTP_STATUS_STOPPED:
			sp->status = M_NET_SMTP_STATUS_PROCESSING;
			process_queue_queue(sp);
			return M_TRUE;
			break;
	}
	return M_FALSE; /* impossible */
}

M_net_smtp_status_t M_net_smtp_status(const M_net_smtp_t *sp)
{
	if (sp == NULL)
		return M_NET_SMTP_STATUS_NOENDPOINTS;
	return sp->status;
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

static char * base64_alloc(const char *str, size_t len)
{
	return M_bincodec_encode_alloc((const unsigned char *)str, len, 0, M_BINCODEC_BASE64);
}

static char * create_str_auth_plain_base64(const char *username, size_t username_len,
		const char *password, size_t password_len)
{
	char           *auth_str        = NULL;
	char           *auth_str_base64 = NULL;
	size_t          len             = 0;

	len = username_len + password_len + 2;
	auth_str = M_malloc_zero(len);
	if (auth_str == NULL)
		return NULL;
	M_mem_move(&auth_str[1], username, username_len);
	M_mem_move(&auth_str[2 + username_len], password, password_len);

	auth_str_base64 = base64_alloc(auth_str, len);
	M_free(auth_str);
	return auth_str_base64;
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
	endpoint_t *ep         = NULL;
	char       *auth_plain = NULL;
	size_t      total_size = 0;

	if (sp == NULL || max_conns == 0 || address == NULL || username == NULL || password == NULL || sp->tcp_dns == NULL)
		return M_FALSE;

	if (connect_tls && sp->tcp_tls_ctx == NULL)
		return M_FALSE;

	if (port == 0)
		port = 25;

	total_size = sizeof(*ep) + sizeof(ep->slots[0]) * max_conns;

	ep                      = M_malloc_zero(total_size);
	ep->tcp.address         = M_strdup(address);
	ep->tcp.username        = M_strdup(username);
	ep->tcp.password        = M_strdup(password);
	ep->tcp.address_len     = M_str_len(address);
	ep->tcp.username_len    = M_str_len(username);
	ep->tcp.password_len    = M_str_len(password);

	auth_plain = create_str_auth_plain_base64(username, ep->tcp.username_len, password, ep->tcp.password_len);

	ep->tcp.auth_plain      = auth_plain;
	ep->tcp.auth_login_user = base64_alloc(username, ep->tcp.username_len);
	ep->tcp.auth_login_pass = base64_alloc(password, ep->tcp.password_len);
	ep->slot_count          = max_conns;
	ep->slot_available      = max_conns;
	ep->tcp.port            = port;
	ep->tcp.connect_tls     = connect_tls;
	ep->tcp.max_conns       = max_conns;

	M_list_insert(sp->endpoints, ep);

	if (sp->status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
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
	endpoint_t *ep         = NULL;
	size_t      total_size;

	if (sp == NULL || command == NULL)
		return M_FALSE;

	if (max_processes == 0)
		max_processes = 1;

	total_size = sizeof(*ep) + sizeof(ep->slots[0]) * max_processes;

	ep                        = M_malloc_zero(total_size);
	ep->process.command       = M_strdup(command);
	ep->process.args          = M_list_str_duplicate(args);
	ep->process.env           = M_hash_dict_duplicate(env);
	ep->type                  = M_NET_SMTP_EPTYPE_PROCESS;
	ep->slot_count            = max_processes;
	ep->slot_available        = max_processes;
	ep->process.timeout_ms    = timeout_ms;
	ep->process.max_processes = max_processes;

	M_list_insert(sp->endpoints, ep);

	if (sp->status == M_NET_SMTP_STATUS_NOENDPOINTS) {
		sp->status = M_NET_SMTP_STATUS_STOPPED;
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
	M_list_str_t *list;
	char         *msg;
	retry_msg_t  *retry;

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
	sp->internal_queue = M_list_str_create(M_LIST_STR_NONE);

	while (
		(retry = M_list_take_first(sp->retry_queue))         != NULL ||
		(retry = M_list_take_first(sp->retry_timeout_queue)) != NULL
	) {
		M_list_str_insert(list, retry->msg);
		M_free(retry->msg);
		M_free(retry);
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
	if (sp == NULL || get_cb == NULL)
		return M_FALSE;

	if (is_pending(sp) || M_list_len(sp->retry_timeout_queue) != 0)
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
