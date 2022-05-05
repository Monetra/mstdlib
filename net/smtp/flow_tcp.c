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

#include "flow.h"

typedef enum {
	CONNECTING = 1,
	OPENING_ACK,
	SENDMSG,
	QUIT,
	QUIT_ACK,
	DISCONNECTING,
} state_id;

static size_t rcpt_count(M_email_t *email)
{
	return M_email_to_len(email) + M_email_cc_len(email) + M_email_bcc_len(email);
}

static M_bool sendmsg_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	endpoint_slot_t *slot = data;
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
	slot->rcpt_n = rcpt_count(slot->email);
	return M_TRUE;
}

static M_state_machine_status_t sendmsg_post_cb(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	(void)sub_status;
	(void)next;

	M_email_destroy(slot->email);
	slot->email = NULL;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t connecting(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot      = data;

	if ((slot->connection_mask & CONNECTION_MASK_IO) != 0u) {
		*next = OPENING_ACK;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t opening_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = SENDMSG;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t quit(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "QUIT\r\n");
	*next = QUIT_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t quit_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = DISCONNECTING;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t disconnecting(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot      = data;
	(void)next;

	if ((slot->connection_mask & CONNECTION_MASK_IO) != 0u) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * smtp_flow_tcp()
{
	M_state_machine_t *m         = NULL;
	M_state_machine_t *sendmsg_m = NULL;
	m = M_state_machine_create(0, "SMTP-flow-tcp", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, CONNECTING, 0, NULL, connecting, NULL, NULL);
	M_state_machine_insert_state(m, OPENING_ACK, 0, NULL, opening_ack, NULL, NULL);
	sendmsg_m = smtp_flow_tcp_sendmsg();
	M_state_machine_insert_sub_state_machine(m, SENDMSG, 0, NULL, sendmsg_m, sendmsg_pre_cb, sendmsg_post_cb, NULL, NULL);
	M_state_machine_destroy(sendmsg_m);
	M_state_machine_insert_state(m, QUIT, 0, NULL, quit, NULL, NULL);
	M_state_machine_insert_state(m, QUIT_ACK, 0, NULL, quit_ack, NULL, NULL);
	M_state_machine_insert_state(m, DISCONNECTING, 0, NULL, disconnecting, NULL, NULL);
	return m;
}
