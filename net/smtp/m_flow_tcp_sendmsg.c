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
	STATE_MAIL_FROM = 1,
	STATE_MAIL_FROM_RESPONSE,
	STATE_RCPT_TO,
	STATE_RCPT_TO_RESPONSE,
	STATE_DATA,
	STATE_DATA_RESPONSE,
	STATE_DATA_PAYLOAD_AND_STOP,
	STATE_DATA_STOP_RESPONSE,
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
	const char                 *address = NULL;

	if (!M_email_from(slot->email, NULL, NULL, &address)) {
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	M_bprintf(slot->out_buf, "MAIL FROM:<%s>\r\n", address);
	*next = STATE_MAIL_FROM_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_mail_from_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->smtp_response_code != 250) {
		const char *line = M_list_str_last(slot->smtp_response);
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 250 mail-from response, got: %s", line);
		goto done;
	}
	*next = STATE_RCPT_TO;
	machine_status = M_STATE_MACHINE_STATUS_NEXT;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_state_rcpt_to(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot    = data;
	const char                 *address = NULL;

	if (!M_rcpt_at(slot->email, slot->rcpt_i, NULL, NULL, &address)) {
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	M_bprintf(slot->out_buf, "RCPT TO:<%s>\r\n", address);

	*next = STATE_RCPT_TO_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_rcpt_to_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->smtp_response_code != 250) {
		const char *line = M_list_str_last(slot->smtp_response);
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 250 rcpt-to response, got: %s", line);
		goto done;
	}

	slot->rcpt_i++;
	if (slot->rcpt_i < slot->rcpt_n) {
		*next = STATE_RCPT_TO;
	} else {
		*next = STATE_DATA;
	}
		machine_status = M_STATE_MACHINE_STATUS_NEXT;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_state_data(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "DATA\r\n");

	*next = STATE_DATA_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_data_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->smtp_response_code != 354) {
		const char *line = M_list_str_last(slot->smtp_response);
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 354 data response, got: %s", line);
		goto done;
	}

	*next = STATE_DATA_PAYLOAD_AND_STOP;
	machine_status = M_STATE_MACHINE_STATUS_NEXT;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_state_data_payload_and_stop(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "%s\r\n.\r\n", slot->msg);

	*next = STATE_DATA_STOP_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_data_stop_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->smtp_response_code != 250) {
		const char *line = M_list_str_last(slot->smtp_response);
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 250 data response, got: %s", line);
		goto done;
	}

	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

M_state_machine_t * M_net_smtp_flow_tcp_sendmsg()
{
	M_state_machine_t *m     = NULL;
	M_state_machine_t *sub_m = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(m, STATE_MAIL_FROM, 0, NULL, M_state_mail_from, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_MAIL_FROM_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_mail_from_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_RCPT_TO, 0, NULL, M_state_rcpt_to, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_RCPT_TO_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_rcpt_to_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_DATA, 0, NULL, M_state_data, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_DATA_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_data_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_DATA_PAYLOAD_AND_STOP, 0, NULL, M_state_data_payload_and_stop, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_DATA_STOP_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_data_stop_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	return m;
}
