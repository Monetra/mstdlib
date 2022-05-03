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
	STARTUP = 1,
	WRITING,
	WRITE_DONE,
} state_ids;

typedef enum {
	IO_PROC,
	IO_STDOUT,
	IO_STDIN,
	IO_STDERR,
	IO_NULL,
} io_types_t;

static M_bool connect_io(endpoint_slot_t *slot, io_types_t io_type)
{
	switch (io_type) {
		case IO_PROC:
			slot->is_io_connected = M_TRUE;
			break;
		case IO_STDOUT:
			slot->is_io_stdout_connected = M_TRUE;
			break;
		case IO_STDIN:
			slot->is_io_stdin_connected = M_TRUE;
			break;
		case IO_STDERR:
			slot->is_io_stderr_connected = M_TRUE;
			break;
		case IO_NULL:
			break;
	}
	return (
		slot->is_io_connected        &&
		slot->is_io_stderr_connected &&
		slot->is_io_stdout_connected &&
		slot->is_io_stdin_connected
	);
}

static io_types_t which_io(endpoint_slot_t *slot)
{
	if (slot->event_io == slot->io)
		return IO_PROC;
	if (slot->event_io == slot->io_stdout)
		return IO_STDOUT;
	if (slot->event_io == slot->io_stdin)
		return IO_STDIN;
	if (slot->event_io == slot->io_stderr)
		return IO_STDERR;

	return IO_NULL;
}

static const char *io_type_str(io_types_t io_type)
{
	switch(io_type) {
		case IO_PROC:
			return "IO_PROC";
			break;
		case IO_STDIN:
			return "IO_STDIN";
			break;
		case IO_STDOUT:
			return "IO_STDOUT";
			break;
		case IO_STDERR:
			return "IO_STDERR";
			break;
		case IO_NULL:
			return "IO_NULL";
			break;
	}
	return "";
}

static void debug_print(endpoint_slot_t *slot)
{
	io_types_t io_type = which_io(slot);

	switch(slot->event_type) {
		case M_EVENT_TYPE_CONNECTED:
			M_printf("M_EVENT_TYPE_CONNECTED: ");
			break;
		case M_EVENT_TYPE_ACCEPT:
			M_printf("M_EVENT_TYPE_ACCEPT: ");
			break;
		case M_EVENT_TYPE_READ:
			M_printf("M_EVENT_TYPE_READ: ");
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			M_printf("M_EVENT_TYPE_DISCONNECTED: ");
			break;
		case M_EVENT_TYPE_ERROR:
			M_printf("M_EVENT_TYPE_ERROR: ");
			break;
		case M_EVENT_TYPE_WRITE:
			M_printf("M_EVENT_TYPE_WRITE: ");
			break;
		case M_EVENT_TYPE_OTHER:
			M_printf("M_EVENT_TYPE_OTHER: ");
			break;
	}

	M_printf("%s: ", io_type_str(io_type));

	M_printf("\n");
}

static void destroy_io(endpoint_slot_t *slot, io_types_t io_type, M_io_t *io)
{
	switch (io_type) {
		case IO_STDOUT:
			slot->is_io_stdout_connected = M_FALSE;
			slot->io_stdout = NULL;
			break;
		case IO_STDIN:
			slot->is_io_stdin_connected = M_FALSE;
			slot->io_stdin = NULL;
			break;
		case IO_STDERR:
			slot->is_io_stderr_connected = M_FALSE;
			slot->io_stderr = NULL;
			break;
		case IO_PROC:
			slot->is_io_connected = M_FALSE;
			slot->io = NULL;
			break;
		case IO_NULL:
			break;
	}
	M_io_destroy(io);
	if (slot->io == NULL && slot->io_stderr == NULL && slot->io_stdout == NULL && slot->io_stdin == NULL) {
		M_printf("stdout: %s, stderr: %s\n", slot->out_str, slot->proc_stderror);
		slot->is_alive = M_FALSE;
	}
}

static void read(endpoint_slot_t *slot, io_types_t io_type, M_io_t *io)
{
	char            *buf       = NULL;
	size_t           remaining = 0;
	size_t           len       = 0;

	if (io_type == IO_STDERR) {
		buf = &slot->proc_stderror[slot->proc_stderror_len];
		remaining = sizeof(slot->proc_stderror) - slot->proc_stderror_len;
	} else if (io_type == IO_STDOUT) {
		buf = &slot->out_str[slot->out_str_len];
		remaining = sizeof(slot->out_str) - slot->out_str_len;
	} else {
		/* Unhandled */
		return;
	}

	M_io_read(io, (unsigned char *)buf, remaining - 1, &len);
	buf[len] = '\0';

	if (io_type == IO_STDERR) {
		slot->proc_stderror_len += len;
	} else if (io_type == IO_STDOUT) {
		slot->out_str_len += len;
	}
}

static void terminate_with_failure(endpoint_slot_t *slot, M_io_t *io, io_types_t io_type, M_bool is_failure)
{
	if (is_failure) {
		slot->is_failure = M_TRUE;
		if (io_type == IO_PROC) {
			M_io_process_get_result_code(io, &slot->result_code);
		}
	}
	destroy_io(slot, io_type, io);
}

static M_state_machine_status_t startup(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot    = data;
	io_types_t       io_type = which_io(slot);
	M_io_t          *io      = slot->event_io;

	M_printf("startup()\n");
	debug_print(slot);

	switch(slot->event_type) {
		case M_EVENT_TYPE_READ:
			read(slot, io_type, io);
			break;
		case M_EVENT_TYPE_CONNECTED:
			if (connect_io(slot, io_type)) {
				*next = WRITING;
				slot->msg_pos = 0;
				slot->msg_len = M_str_len(slot->msg);
				/* emulate queue task event */
				slot->event_type = M_EVENT_TYPE_OTHER;
				slot->event_io = NULL;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			terminate_with_failure(slot, io, io_type, M_TRUE);
			break;
		default:
			debug_print(slot);
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t writing(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	io_types_t       io_type    = which_io(slot);
	size_t           remaining  = 0;
	M_io_error_t     io_errcode = M_IO_ERROR_SUCCESS;
	const char      *buf        = NULL;
	size_t           len;

	M_printf("writing(): is_queue_task %d: ", slot->is_queue_task);

	switch(slot->event_type) {
		case M_EVENT_TYPE_READ:
			read(slot, io_type, io);
			break;
		case M_EVENT_TYPE_OTHER:
			/* self triggered event */
			buf = &slot->msg[slot->msg_pos];
			remaining = slot->msg_len - slot->msg_pos;
			io_errcode = M_io_write(slot->io_stdin, (const unsigned char*)buf, remaining, &len);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "M_io_write: %s", M_io_error_string(io_errcode));
				terminate_with_failure(slot, slot->io, IO_PROC, M_TRUE);
				return M_STATE_MACHINE_STATUS_WAIT;
			}
			slot->msg_pos += len;
			if (slot->msg_pos == slot->msg_len) {
				*next = WRITE_DONE;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			slot->is_queue_task = M_TRUE;
			break;
		default:
			debug_print(slot);
			terminate_with_failure(slot, io, io_type, M_TRUE);
			break;
	}

	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t write_done(void *data, M_uint64 *next)
{
	endpoint_slot_t     *slot       = data;
	M_io_t              *io         = slot->event_io;
	io_types_t           io_type    = which_io(slot);
	(void)next;

	M_printf("write_done(): is_queue_task %d: ", slot->is_queue_task);
	debug_print(slot);

	switch(slot->event_type) {
		case M_EVENT_TYPE_READ:
			read(slot, io_type, io);
			break;
		case M_EVENT_TYPE_OTHER:
			terminate_with_failure(slot, slot->io_stdin, IO_STDIN, M_FALSE);
			break;
		default:
			terminate_with_failure(slot, io, io_type, M_FALSE);
			if (slot->io == NULL && slot->io_stderr == NULL && slot->io_stdout == NULL && slot->io_stdin == NULL) {
				return M_STATE_MACHINE_STATUS_DONE;
			}
			break;
	}

	return M_STATE_MACHINE_STATUS_WAIT;
}

M_state_machine_t * smtp_flow_process()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-process", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STARTUP, 0, NULL, startup, NULL, NULL);
	M_state_machine_insert_state(m, WRITING, 0, NULL, writing, NULL, NULL);
	M_state_machine_insert_state(m, WRITE_DONE, 0, NULL, write_done, NULL, NULL);
	return m;
}
