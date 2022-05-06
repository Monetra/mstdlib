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
	STATE_MAIL_FROM = 1,
	STATE_MAIL_FROM_ACK,
	STATE_RCPT_TO,
	STATE_RCPT_TO_ACK,
	STATE_DATA,
	STATE_DATA_ACK,
	STATE_DATA_PAYLOAD_AND_STOP,
	STATE_DATA_STOP_ACK,
} state_id;

static M_bool M_rcpt_at(M_email_t *e, size_t idx, const char **group, const char **name, const char **address)
{
	size_t idx_offset = 0;
	size_t len        = 0;

	len = M_email_to_len(e);
	if ((idx - idx_offset) < len) {
		return M_email_to(e, idx - idx_offset, group, name, address);
	}
	idx_offset += len;

	len = M_email_cc_len(e);
	if ((idx - idx_offset) < len) {
		return M_email_cc(e, idx - idx_offset, group, name, address);
	}
	idx_offset += len;

	len = M_email_bcc_len(e);
	if ((idx - idx_offset) < len) {
		return M_email_bcc(e, idx - idx_offset, group, name, address);
	}

	return M_FALSE;
}

static M_state_machine_status_t M_state_mail_from(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot    = data;
	const char                 *group   = NULL;
	const char                 *name    = NULL;
	const char                 *address = NULL;

	if (!M_email_from(slot->email, &group, &name, &address)) {
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	M_bprintf(slot->out_buf, "MAIL FROM:<%s>\r\n", address);
	*next = STATE_MAIL_FROM_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_mail_from_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = STATE_RCPT_TO;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_rcpt_to(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot    = data;
	const char      *group   = NULL;
	const char      *name    = NULL;
	const char      *address = NULL;

	if (!M_rcpt_at(slot->email, slot->rcpt_i, &group, &name, &address)) {
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	M_bprintf(slot->out_buf, "RCPT TO:<%s>\r\n", address);

	*next = STATE_RCPT_TO_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_rcpt_to_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		slot->rcpt_i++;
		if (slot->rcpt_i < slot->rcpt_n) {
			*next = STATE_RCPT_TO;
		} else {
			*next = STATE_DATA;
		}
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_data(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "DATA\r\n");

	*next = STATE_DATA_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_data_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = STATE_DATA_PAYLOAD_AND_STOP;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_data_payload_and_stop(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_buf_add_str(slot->out_buf, slot->msg);
	M_bprintf(slot->out_buf, "\r\n.\r\n");

	*next = STATE_DATA_STOP_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_data_stop_ack(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)next;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		return M_STATE_MACHINE_STATUS_DONE;
	}

	return M_STATE_MACHINE_STATUS_WAIT;
}

M_state_machine_t * M_net_smtp_flow_tcp_sendmsg()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_MAIL_FROM, 0, NULL, M_state_mail_from, NULL, NULL);
	M_state_machine_insert_state(m, STATE_MAIL_FROM_ACK, 0, NULL, M_state_mail_from_ack, NULL, NULL);
	M_state_machine_insert_state(m, STATE_RCPT_TO, 0, NULL, M_state_rcpt_to, NULL, NULL);
	M_state_machine_insert_state(m, STATE_RCPT_TO_ACK, 0, NULL, M_state_rcpt_to_ack, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DATA, 0, NULL, M_state_data, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DATA_ACK, 0, NULL, M_state_data_ack, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DATA_PAYLOAD_AND_STOP, 0, NULL, M_state_data_payload_and_stop, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DATA_STOP_ACK, 0, NULL, M_state_data_stop_ack, NULL, NULL);
	return m;
}
