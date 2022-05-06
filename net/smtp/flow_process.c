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
	STATE_CONNECTING = 1,
	STATE_WRITE,
	STATE_DISCONNECTING,
} m_state_ids;

static M_state_machine_status_t M_state_connecting(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	if (
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO)        != 0u) &&
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDIN)  != 0u) &&
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) != 0u) &&
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		*next = STATE_WRITE;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_write(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_buf_add_str(slot->out_buf, slot->msg);

	*next = STATE_DISCONNECTING;
	return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_disconnecting(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)next;
	if (
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO)        != 0u) ||
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDIN)  != 0u) ||
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) != 0u) ||
		((slot->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * M_net_smtp_flow_process()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "M-net-smtp-flow-process", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_CONNECTING, 0, NULL, M_state_connecting, NULL, NULL);
	M_state_machine_insert_state(m, STATE_WRITE, 0, NULL, M_state_write, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DISCONNECTING, 0, NULL, M_state_disconnecting, NULL, NULL);
	return m;
}
