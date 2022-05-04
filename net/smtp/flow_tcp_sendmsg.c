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
	MAIL_FROM,
	MAIL_FROM_ACK,
	RCPT_TO,
	RCPT_TO_ACK,
	DATA_START, /* DATA is overloaded */
	DATA_ACK,
	DATA_PAYLOAD_AND_STOP,
	DATA_STOP_ACK,
} state_id;

static M_state_machine_status_t mail_from(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	const char      *group;
	const char      *name;
	const char      *address;

	M_email_from(slot->email, &group, &name, &address);
	M_bprintf(slot->out_buf, "MAIL FROM:<%s>\r\n", address);
	*next = MAIL_FROM_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t mail_from_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = RCPT_TO;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t rcpt_to(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	const char      *group;
	const char      *name;
	const char      *address;

	M_email_to(slot->email, slot->email_position, &group, &name, &address);
	M_bprintf(slot->out_buf, "RCPT TO:<%s>\r\n", address);

	*next = RCPT_TO_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t rcpt_to_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		slot->email_position++;
		if (slot->email_position < M_email_to_len(slot->email)) {
			*next = RCPT_TO;
		} else {
			*next = DATA_START;
		}
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t data_start(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	M_bprintf(slot->out_buf, "DATA\r\n");

	*next = DATA_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t data_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		*next = DATA_PAYLOAD_AND_STOP;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t data_payload_and_stop(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	M_buf_add_str(slot->out_buf, slot->msg);
	M_bprintf(slot->out_buf, "\r\n.\r\n");

	*next = DATA_STOP_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t data_stop_ack(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	(void)next;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		return M_STATE_MACHINE_STATUS_DONE;
	}

	return M_STATE_MACHINE_STATUS_WAIT;
}

M_state_machine_t * smtp_flow_tcp_sendmsg()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, MAIL_FROM, 0, NULL, mail_from, NULL, NULL);
	M_state_machine_insert_state(m, MAIL_FROM_ACK, 0, NULL, mail_from_ack, NULL, NULL);
	M_state_machine_insert_state(m, RCPT_TO, 0, NULL, rcpt_to, NULL, NULL);
	M_state_machine_insert_state(m, RCPT_TO_ACK, 0, NULL, rcpt_to_ack, NULL, NULL);
	M_state_machine_insert_state(m, DATA_START, 0, NULL, data_start, NULL, NULL);
	M_state_machine_insert_state(m, DATA_ACK, 0, NULL, data_ack, NULL, NULL);
	M_state_machine_insert_state(m, DATA_PAYLOAD_AND_STOP, 0, NULL, data_payload_and_stop, NULL, NULL);
	M_state_machine_insert_state(m, DATA_STOP_ACK, 0, NULL, data_stop_ack, NULL, NULL);
	return m;
}
