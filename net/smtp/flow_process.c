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
	WRITE,
	DISCONNECTING,
} state_ids;

static M_state_machine_status_t connecting(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;
	if (
		((slot->connection_mask & CONNECTION_MASK_IO)        != 0u) &&
		((slot->connection_mask & CONNECTION_MASK_IO_STDIN)  != 0u) &&
		((slot->connection_mask & CONNECTION_MASK_IO_STDOUT) != 0u) &&
		((slot->connection_mask & CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		*next = WRITE;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t write(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;

	M_buf_add_str(slot->out_buf, slot->msg);

	*next = DISCONNECTING;
	return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t disconnecting(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;
	(void)next;
	if (
		((slot->connection_mask & CONNECTION_MASK_IO)        != 0u) ||
		((slot->connection_mask & CONNECTION_MASK_IO_STDIN)  != 0u) ||
		((slot->connection_mask & CONNECTION_MASK_IO_STDOUT) != 0u) ||
		((slot->connection_mask & CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * smtp_flow_process()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-process", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, CONNECTING, 0, NULL, connecting, NULL, NULL);
	M_state_machine_insert_state(m, WRITE, 0, NULL, write, NULL, NULL);
	M_state_machine_insert_state(m, DISCONNECTING, 0, NULL, disconnecting, NULL, NULL);
	return m;
}
