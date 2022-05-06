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
	STATE_STARTTLS = 1,
	STATE_STARTTLS_ACK,
} m_state_ids;

static M_state_machine_status_t M_state_starttls(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "STARTTLS\r\n");
	*next = STATE_STARTTLS_ACK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_starttls_ack(void *data, M_uint64 *next)
{
	(void)next;
	M_net_smtp_endpoint_slot_t *slot = data;

	if (M_parser_consume_until(slot->in_parser, (const unsigned char *)"\r\n", 2, M_TRUE)) {
		slot->tls_state = M_NET_SMTP_TLS_STARTTLS_READY;
		return M_STATE_MACHINE_STATUS_DONE;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

M_state_machine_t * M_net_smtp_flow_tcp_starttls()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_STARTTLS, 0, NULL, M_state_starttls, NULL, NULL);
	M_state_machine_insert_state(m, STATE_STARTTLS_ACK, 0, NULL, M_state_starttls_ack, NULL, NULL);
	return m;
}
