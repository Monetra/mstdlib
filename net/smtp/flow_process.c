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
	IDLE,
	WRITING,
	WRITE_DONE,
	ERROR,
} state_ids;

typedef enum {
	IO_PROC,
	IO_STDOUT,
	IO_STDIN,
	IO_STDERR
} io_types_t;

static io_types_t which_io(endpoint_slot_t *slot)
{
	if (slot->event_io == slot->io)
		return IO_PROC;
	if (slot->event_io == slot->io_stdout)
		return IO_STDOUT;
	if (slot->event_io == slot->io_stdin)
		return IO_STDIN;

	return IO_STDERR;
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
			slot->io_stdout = NULL;
			break;
		case IO_STDIN:
			slot->io_stdin = NULL;
			break;
		case IO_STDERR:
			slot->io_stderr = NULL;
			break;
		case IO_PROC:
			slot->io = NULL;
			break;
	}
	M_io_destroy(io);
	if (slot->io == NULL && slot->io_stderr == NULL && slot->io_stdout == NULL && slot->io_stdin == NULL) {
		if (slot->msg) {
			M_free(slot->msg);
			slot->msg = NULL;
		}
		slot->is_alive = M_FALSE;
	}
}

static M_state_machine_status_t startup(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot    = data;
	io_types_t       io_type = which_io(slot);
	M_io_t          *io      = slot->event_io;
	unsigned char    buf[256];
	size_t           len;
	(void)next;

	switch(slot->event_type) {
		case M_EVENT_TYPE_READ:
			M_io_read(io, buf, sizeof(buf) - 1, &len);
			buf[len] = '\0';
			M_printf("%s: %s\n", io_type_str(io_type), buf);
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			M_printf("Disconnected: %s\n", io_type_str(io_type));
			destroy_io(slot, io_type, io);
			break;
		default:
			debug_print(slot);
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t idle(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot = data;
	(void)next;

	M_printf("idle: ");
	debug_print(slot);

	//M_printf("%s:%d: %s(): { %p, %p, %s, %p, %p, %p }\n", __FILE__, __LINE__, __FUNCTION__, slot->io, slot->state_machine, slot->msg, slot->io_stdin, slot->io_stdout, slot->io_stderr);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t writing(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t write_done(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t error(void *data, M_uint64 *next)
{
	(void)data;
	(void)next;
	return M_STATE_MACHINE_STATUS_NEXT;
}

M_state_machine_t * smtp_flow_process()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-process", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STARTUP, 0, NULL, startup, NULL, NULL);
	M_state_machine_insert_state(m, IDLE, 0, NULL, idle, NULL, NULL);
	M_state_machine_insert_state(m, WRITING, 0, NULL, writing, NULL, NULL);
	M_state_machine_insert_state(m, WRITE_DONE, 0, NULL, write_done, NULL, NULL);
	M_state_machine_insert_state(m, ERROR, 0, NULL, error, NULL, NULL);
	return m;
}
