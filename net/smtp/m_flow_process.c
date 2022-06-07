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
	STATE_WRITE_START,
	STATE_WRITE_CHUNK,
	STATE_WRITE_CHUNK_WAIT,
	STATE_WRITE_FINISH,
	STATE_DISCONNECTING,
} m_state_ids;

static M_state_machine_status_t M_state_connecting(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	if (
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO)        != 0u) &&
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDIN)  != 0u) &&
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) != 0u) &&
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		*next = STATE_WRITE_START;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t M_state_write_start(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	session->process.next_write_chunk = session->msg;

	*next = STATE_WRITE_CHUNK;
	return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_write_chunk(void *data, M_uint64 *next)
{
	const char           *next_chunk = NULL;
	M_net_smtp_session_t *session    = data;
	size_t                chunk_len  = 0;

	/* This is used to detect if the command quits early.
		* sendmail will if -i isn't specified */
	next_chunk = M_str_str(session->process.next_write_chunk, "\r\n.\r\n");
	if (next_chunk == NULL) {
		M_buf_add_str(session->out_buf, session->process.next_write_chunk);
		*next = STATE_WRITE_FINISH;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	next_chunk = &next_chunk[5];
	chunk_len = (size_t)(next_chunk - session->process.next_write_chunk);
	M_buf_add_str_max(session->out_buf, session->process.next_write_chunk, chunk_len);
	session->process.next_write_chunk = next_chunk;
	*next = STATE_WRITE_CHUNK_WAIT;
	return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_write_chunk_wait(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	if (M_buf_len(session->out_buf) > 0) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	*next = STATE_WRITE_CHUNK;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_write_finish(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	session->is_successfully_sent = M_TRUE;

	*next = STATE_DISCONNECTING;
	return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_disconnecting(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	(void)next;
	if (
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO)        != 0u) ||
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDIN)  != 0u) ||
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDOUT) != 0u) ||
		((session->connection_mask & M_NET_SMTP_CONNECTION_MASK_IO_STDERR) != 0u)
	) {
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	if (M_buf_len(session->out_buf) > 0) {
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * M_net_smtp_flow_process(void)
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "M-net-smtp-flow-process", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_CONNECTING, 0, NULL, M_state_connecting, NULL, NULL);
	M_state_machine_insert_state(m, STATE_WRITE_START, 0, NULL, M_state_write_start, NULL, NULL);
	M_state_machine_insert_state(m, STATE_WRITE_CHUNK, 0, NULL, M_state_write_chunk, NULL, NULL);
	M_state_machine_insert_state(m, STATE_WRITE_CHUNK_WAIT, 0, NULL, M_state_write_chunk_wait, NULL, NULL);
	M_state_machine_insert_state(m, STATE_WRITE_FINISH, 0, NULL, M_state_write_finish, NULL, NULL);
	M_state_machine_insert_state(m, STATE_DISCONNECTING, 0, NULL, M_state_disconnecting, NULL, NULL);
	return m;
}