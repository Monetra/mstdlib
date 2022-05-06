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

#include "m_flow.h"

typedef enum {
	STATE_CONNECTING = 1,
	STATE_OPENING_ACK,
	STATE_STARTTLS,
	STATE_EHLO,
	STATE_AUTH,
	STATE_SENDMSG,
	STATE_QUIT,
	STATE_QUIT_ACK,
	STATE_DISCONNECTING,
} m_state_ids;

static size_t M_rcpt_count(M_email_t *email)
{
	return M_email_to_len(email) + M_email_cc_len(email) + M_email_bcc_len(email);
}

static M_bool M_sendmsg_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)status;
	(void)next;

	slot->rcpt_i = 0;
	slot->email = M_email_create();
	if (NULL == slot->email) {
		return M_FALSE;
	}
	if (!M_email_set_headers(slot->email, slot->headers)) {
		M_email_destroy(slot->email);
		slot->email = NULL;
		return M_FALSE;
	}
	slot->rcpt_n = M_rcpt_count(slot->email);
	return M_TRUE;
}

static M_state_machine_status_t M_starttls_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	(void)data;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	*next = STATE_EHLO;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_ehlo_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	(void)data;
	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;
	*next = STATE_AUTH;
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

static M_state_machine_status_t M_sendmsg_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot       = data;

	M_email_destroy(slot->email);
	slot->email = NULL;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return sub_status;

	*next = STATE_QUIT;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_connecting(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot      = data;

	if ((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO) != 0u) {
		*next = STATE_OPENING_ACK;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_opening_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot       = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		switch(slot->tls_state) {
			case M_NET_SMTP_TLS_NONE:
			case M_NET_SMTP_TLS_CONNECTED:
				*next = STATE_EHLO;
				break;
			case M_NET_SMTP_TLS_STARTTLS:
				*next = STATE_STARTTLS;
				break;
			default:
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_quit(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "QUIT\r\n");
	*next = STATE_QUIT_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_quit_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = STATE_DISCONNECTING;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_disconnecting(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot      = data;
	(void)next;

	if ((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO) != 0u) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * M_net_smtp_flow_tcp()
{
	M_state_machine_t *m          = NULL;
	M_state_machine_t *sub_m  = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_CONNECTING, 0, NULL, M_state_connecting, NULL, NULL);
	M_state_machine_insert_state(m, STATE_OPENING_ACK, 0, NULL, M_state_opening_ack, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_starttls();
	M_state_machine_insert_sub_state_machine(m, STATE_STARTTLS, 0, NULL, sub_m,
			NULL, M_starttls_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_ehlo();
	M_state_machine_insert_sub_state_machine(m, STATE_EHLO, 0, NULL, sub_m,
			NULL, M_ehlo_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_auth();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH, 0, NULL, sub_m,
			NULL, M_auth_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_sendmsg();
	M_state_machine_insert_sub_state_machine(m, STATE_SENDMSG, 0, NULL, sub_m,
			M_sendmsg_pre_cb, M_sendmsg_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_QUIT, 0, NULL, M_state_quit, NULL, NULL);
	M_state_machine_insert_state(m, STATE_QUIT_ACK, 0, NULL, M_state_quit_ack, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DISCONNECTING, 0, NULL, M_state_disconnecting, NULL, NULL);
	return m;
}
