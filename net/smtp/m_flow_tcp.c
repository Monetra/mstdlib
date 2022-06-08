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

typedef enum {
	STATE_CONNECTING = 1,
	STATE_OPENING_RESPONSE,
	STATE_EHLO,
	STATE_STARTTLS,
	STATE_AUTH,
	STATE_SENDMSG,
	STATE_WAIT_FOR_NEXT_MSG,
	STATE_QUIT,
	STATE_QUIT_ACK,
	STATE_DISCONNECTING,
} m_state_ids;

M_bool M_net_smtp_flow_tcp_check_smtp_response_code(M_net_smtp_session_t *session, M_uint64 expected_code)
{
	const char *line;
	if (session->tcp.smtp_response_code != expected_code) {
		/* Classify as connect failure so endpoint can get removed */
		session->tcp.is_connect_fail = M_TRUE;
		session->tcp.net_error = M_NET_ERROR_PROTOFORMAT;
		line = M_list_str_last(session->tcp.smtp_response);
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Expected %llu response, got: %llu: %s",
				expected_code, session->tcp.smtp_response_code, line);
		return M_FALSE;
	}
	return M_TRUE;
}

static M_state_machine_status_t M_state_connecting(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	if ((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO) != 0u) {
		*next = STATE_OPENING_RESPONSE;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_opening_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	const char           *line    = NULL;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 220))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (!M_str_caseeq(session->ep->tcp.address, "localhost")) {
		line = M_list_str_first(session->tcp.smtp_response);
		if (!M_str_caseeq_max(session->ep->tcp.address, line, M_str_len(session->ep->tcp.address))) {
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Domain mismatch \"%s\" != \"%s\"",
					session->ep->tcp.address, line);
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
	}
	*next = STATE_EHLO;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_bool M_ehlo_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	const char           *address = NULL;
	const char           *domain  = NULL;
	(void)status;
	(void)next;

	if (!M_email_from(session->email, NULL, NULL, &address)) {
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Failed to parse \"From:\": %s", session->msg);
		return M_FALSE;
	}

	if (
		address == NULL                                ||
		(domain = M_str_chr(address, '@')) == NULL     ||
		(domain = &domain[1]) == NULL                  ||
		(session->tcp.ehlo_domain = M_strdup(domain)) == NULL
	) {
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Failed to parse domain from: %s\n", domain);
		return M_FALSE;
	}

	return M_TRUE;
}

static M_state_machine_status_t M_ehlo_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	M_free(session->tcp.ehlo_domain);
	session->tcp.ehlo_domain = NULL;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	switch(session->tcp.tls_state) {
		case M_NET_SMTP_TLS_NONE:
		case M_NET_SMTP_TLS_CONNECTED:
			*next = STATE_AUTH;
			break;
		case M_NET_SMTP_TLS_STARTTLS:
			if (session->tcp.is_starttls_capable) {
				*next = STATE_STARTTLS;
				break;
			}
			/* Classify as connect failure so endpoint can get removed */
			session->tcp.is_connect_fail = M_TRUE;
			session->tcp.net_error = M_NET_ERROR_NOTPERM;
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Server does not support STARTTLS");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		case M_NET_SMTP_TLS_IMPLICIT:
		case M_NET_SMTP_TLS_STARTTLS_READY:
		case M_NET_SMTP_TLS_STARTTLS_ADDED:
			M_snprintf(session->errmsg, sizeof(session->errmsg), "Invalid TLS state.");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_starttls_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	(void)data;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	*next = STATE_EHLO;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	(void)data;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	*next = STATE_SENDMSG;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_bool M_sendmsg_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	const char           *group;
	const char           *name;
	const char           *address;
	size_t                i;
	(void)status;
	(void)next;

	session->tcp.rcpt_to = M_list_str_create(M_LIST_STR_NONE);

	for (i = 0; i < M_email_to_len(session->email); i++) {
		M_email_to(session->email, i, &group, &name, &address);
		M_list_str_insert(session->tcp.rcpt_to, address);
	}

	for (i = 0; i < M_email_cc_len(session->email); i++) {
		M_email_cc(session->email, i, &group, &name, &address);
		M_list_str_insert(session->tcp.rcpt_to, address);
	}

	for (i = 0; i < M_email_bcc_len(session->email); i++) {
		M_email_bcc(session->email, i, &group, &name, &address);
		M_list_str_insert(session->tcp.rcpt_to, address);
	}

	return M_TRUE;
}

static M_state_machine_status_t M_sendmsg_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	M_list_str_destroy(session->tcp.rcpt_to);
	session->tcp.rcpt_to = NULL;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	session->is_successfully_sent = M_TRUE;

	if (session->tcp.is_QUIT_enabled) {
		*next = STATE_QUIT;
	} else {
		*next = STATE_WAIT_FOR_NEXT_MSG;
	}
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_wait_for_next_msg(void *data, M_uint64 *next)
{

	/* Initially entering this state session->is_succesfully_sent will be true from the previous
		* state.  Any state machine errors will cause the state machine to error out and the connection will
		* be closed and restarted.  An idle timeout can cause the is_QUIT_enabled to be set after first entering
		* this state.  Once the session has had the old message cleaned out and a new message inserted it will set
		* the is_successfully_sent state to FALSE.  Messages are assumed to be failures until they prove success.
		*/

	M_net_smtp_session_t *session = data;
	if (session->tcp.is_QUIT_enabled) {
		*next = STATE_QUIT;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	if (session->is_successfully_sent == M_FALSE) {
		*next = STATE_SENDMSG;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_quit(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	M_buf_add_str(session->out_buf, "QUIT\r\n");
	*next = STATE_QUIT_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_quit_ack(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

/* Although RFC 5321 calls for a 221 reply, if they don't send one we need to move on,
	* regardless of how upset John Klensin may get. */

	if (M_parser_consume_until(session->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = STATE_DISCONNECTING;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_disconnecting(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session      = data;
	(void)next;

	if ((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO) != 0u) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_state_machine_t * M_net_smtp_flow_tcp(void)
{
	M_state_machine_t *m     = NULL;
	M_state_machine_t *sub_m = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_CONNECTING, 0, NULL, M_state_connecting, NULL, NULL);

	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_OPENING_RESPONSE, M_opening_response_post_cb);

	sub_m = M_net_smtp_flow_tcp_starttls();
	M_state_machine_insert_sub_state_machine(m, STATE_STARTTLS, 0, NULL, sub_m,
			NULL, M_starttls_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_ehlo();
	M_state_machine_insert_sub_state_machine(m, STATE_EHLO, 0, NULL, sub_m,
			M_ehlo_pre_cb, M_ehlo_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_auth();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH, 0, NULL, sub_m,
			NULL, M_auth_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_sendmsg();
	M_state_machine_insert_sub_state_machine(m, STATE_SENDMSG, 0, NULL, sub_m,
			M_sendmsg_pre_cb, M_sendmsg_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_WAIT_FOR_NEXT_MSG, 0, NULL, M_state_wait_for_next_msg, NULL, NULL);
	M_state_machine_insert_state(m, STATE_QUIT, 0, NULL, M_state_quit, NULL, NULL);
	M_state_machine_insert_state(m, STATE_QUIT_ACK, 0, NULL, M_state_quit_ack, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DISCONNECTING, 0, NULL, M_state_disconnecting, NULL, NULL);
	return m;
}
