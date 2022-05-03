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
	START = 1,
	SENDMSG,
	SENDMSG_MAIL_FROM_WRITE,
	SENDMSG_MAIL_FROM_READ,
	SENDMSG_RCPT_TO_WRITE,
	SENDMSG_RCPT_TO_READ,
	SENDMSG_DATA_PRE_WRITE,
	SENDMSG_DATA_PRE_READ,
	SENDMSG_DATA,
	SENDMSG_DATA_POST_WRITE,
	SENDMSG_DATA_POST_READ,
	SENDMSG_FULL_STOP,
	QUIT_WRITE,
	QUIT_READ,
} state_id;

static void debug_print(endpoint_slot_t *slot)
{
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
	M_printf("\n");
}

static M_io_error_t write_all(M_io_t *io, const unsigned char* msg, size_t len)
{
	size_t       i      = 0;
	size_t       n      = 0;
	M_io_error_t io_err = M_IO_ERROR_SUCCESS;
	while (n < len && io_err == M_IO_ERROR_SUCCESS) {
		io_err = M_io_write(io, &msg[n], len - n, &i);
		n += i;
	}
	return io_err;
}

static M_bool read_line_into_out_str(endpoint_slot_t* slot)
{
	M_io_t          *io         = slot->event_io;
	char            *str        = NULL;
	size_t           remaining  = 0;
	size_t           len        = 0;

	str = &slot->out_str[slot->out_str_len];
	remaining = sizeof(slot->out_str) - slot->out_str_len;
	M_io_read(io, (unsigned char*)str, remaining - 1, &len);
	str[len] = '\0';
	slot->out_str_len += len;
	if (slot->out_str[slot->out_str_len - 2] == '\r' && slot->out_str[slot->out_str_len - 1] == '\n') {
		return M_TRUE;
	}
	return M_FALSE;
}

static M_bool sendmsg_pre(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	M_email_error_t  email_err;
	(void)status;
	(void)next;

	slot->msg_len = M_str_len(slot->msg);
	email_err = M_email_simple_read(&slot->email, slot->msg, slot->msg_len, M_EMAIL_SIMPLE_READ_NONE, NULL);
	if (email_err != M_EMAIL_ERROR_SUCCESS) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "M_email_simple_read(): %d", email_err);
		return M_FALSE;
	}

	email_err = M_email_simple_split_header_body(slot->msg, &slot->email_hash_dict, &slot->email_body);
	if (email_err != M_EMAIL_ERROR_SUCCESS) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "M_email_simple_read(): %d", email_err);
		return M_FALSE;
	}

	return M_TRUE;
}

static M_state_machine_status_t sendmsg_post(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	(void)sub_status;
	(void)next;
	M_email_destroy(slot->email);
	slot->email = NULL;
	M_hash_dict_destroy(slot->email_hash_dict);
	slot->email_hash_dict = NULL;
	M_free(slot->email_body);
	slot->email_body = NULL;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t sendmsg_mail_from_write(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	M_io_error_t     io_errcode;
	const char      *group;
	const char      *name;
	const char      *address;
	char            *str       = NULL;
	size_t           len;

	(void)next;

	M_printf("sendmsg_mail_from_write()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			M_email_from(slot->email, &group, &name, &address);
			str = slot->out_str;
			len = sizeof(slot->out_str);
			len = M_snprintf(str, len - 1, "MAIL FROM:<%s>\r\n", address);
			str[len] = '\0';
			M_printf("%s\n", str);
			io_errcode = write_all(io, (unsigned char *)str, len);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			slot->out_str_len = 0;
			/* emulate read event */
			slot->event_type = M_EVENT_TYPE_READ;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_mail_from_read(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	(void)next;

	M_printf("sendmsg_mail_from_read()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				/* emulate write event */
				slot->event_type = M_EVENT_TYPE_WRITE;
				M_printf("%s\n", slot->out_str);
				slot->email_position = 0;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_rcpt_to_write(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	M_io_error_t     io_errcode;
	const char      *group;
	const char      *name;
	const char      *address;
	char            *str       = NULL;
	size_t           len;

	(void)next;

	M_printf("sendmsg_rcpt_to_write()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			M_email_to(slot->email, slot->email_position, &group, &name, &address);
			str = slot->out_str;
			len = sizeof(slot->out_str);
			len = M_snprintf(str, len - 1, "RCPT TO:<%s>\r\n", address);
			str[len] = '\0';
			M_printf("%s\n", str);
			io_errcode = write_all(io, (unsigned char *)str, len);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			slot->email_position++;
			slot->out_str_len = 0;
			/* emulate read event */
			slot->event_type = M_EVENT_TYPE_READ;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_rcpt_to_read(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	M_printf("sendmsg_rcpt_to()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				/* emulate write event */
				M_printf("(%zu / %zu) %s\n", slot->email_position, M_email_to_len(slot->email), slot->out_str);
				slot->event_type = M_EVENT_TYPE_WRITE;
				if (slot->email_position < M_email_to_len(slot->email)) {
					*next = SENDMSG_RCPT_TO_WRITE;
				}
				slot->event_type = M_EVENT_TYPE_WRITE;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_data_pre_write(void *data, M_uint64 *next)
{
	const char      *data_str   = "DATA\r\n";
	endpoint_slot_t *slot       = data;
	M_io_error_t     io_errcode;
	(void)next;

	M_printf("sendmsg_data_pre_write()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			M_printf("%s\n", data_str);
			io_errcode = write_all(slot->event_io, (const unsigned char *)data_str, 6);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				size_t len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			slot->out_str_len = 0;
			/* emulate read event */
			slot->event_type = M_EVENT_TYPE_READ;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_data_pre_read(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	(void)next;

	M_printf("sendmsg_data_pre_read()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				M_printf("%s\n", slot->out_str);
				/* emulate write event */
				slot->event_type = M_EVENT_TYPE_WRITE;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_data(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	M_io_error_t     io_errcode;
	size_t           len;

	(void)next;

	M_printf("sendmsg_data()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			io_errcode = write_all(io, (unsigned char *)slot->email_body, M_str_len(slot->email_body));
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_data_post_write(void *data, M_uint64 *next)
{
	const char      *fullstop   = "\r\n.\r\n";
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	M_io_error_t     io_errcode;
	size_t           len;

	(void)next;

	M_printf("sendmsg_data_post_write()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			M_printf("%s\n", fullstop);
			io_errcode = write_all(io, (unsigned char *)fullstop, 5);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			/* emulate read event */
			slot->event_type = M_EVENT_TYPE_READ;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t sendmsg_data_post_read(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	(void)next;

	M_printf("sendmsg_data_post_read()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				/* emulate write event */
				M_printf("%s\n", slot->out_str);
				slot->event_type = M_EVENT_TYPE_WRITE;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t start(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot      = data;

	M_printf("start():");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_CONNECTED:
			slot->is_io_connected = M_TRUE;
			break;
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				/* emulate write event */
				M_printf("%s\n", slot->out_str);
				slot->event_type = M_EVENT_TYPE_WRITE;
				*next = SENDMSG;
				return M_STATE_MACHINE_STATUS_NEXT;
			}
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t quit_write(void *data, M_uint64 *next)
{
	const char      *quit_str   = "QUIT\r\n";
	endpoint_slot_t *slot       = data;
	M_io_t          *io         = slot->event_io;
	M_io_error_t     io_errcode;
	size_t           len;

	(void)next;

	M_printf("quit_write()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_WRITE:
			M_printf("%s\n", quit_str);
			io_errcode = write_all(io, (unsigned char *)quit_str, 6);
			if (io_errcode != M_IO_ERROR_SUCCESS) {
				len = M_snprintf(slot->errmsg, sizeof(slot->errmsg) - 1, "write_all(): %s", M_io_error_string(io_errcode));
				M_printf("error! %s", M_io_error_string(io_errcode));
				slot->errmsg[len] = '\0';
				return M_STATE_MACHINE_STATUS_ERROR_STATE;
			}
			slot->event_type = M_EVENT_TYPE_READ;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;

}

static M_state_machine_status_t quit_read(void *data, M_uint64 *next)
{
	endpoint_slot_t *slot       = data;

	(void)next;

	M_printf("quit_read()");
	debug_print(slot);

	switch (slot->event_type) {
		case M_EVENT_TYPE_READ:
			if (read_line_into_out_str(slot)) {
				slot->out_str_len = 0;
				M_printf("%s\n", slot->out_str);
				return M_STATE_MACHINE_STATUS_WAIT;
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			slot->is_io_connected = M_FALSE;
			M_io_destroy(slot->io);
			slot->io = NULL;
			slot->is_alive = M_FALSE;
			return M_STATE_MACHINE_STATUS_DONE;
		default:
			break;
	}
	return M_STATE_MACHINE_STATUS_WAIT;

}

static M_state_machine_t * smtp_flow_tcp_sendmsg()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, SENDMSG_MAIL_FROM_WRITE, 0, NULL, sendmsg_mail_from_write, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_MAIL_FROM_READ, 0, NULL, sendmsg_mail_from_read, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_RCPT_TO_WRITE, 0, NULL, sendmsg_rcpt_to_write, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_RCPT_TO_READ, 0, NULL, sendmsg_rcpt_to_read, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_DATA_PRE_WRITE, 0, NULL, sendmsg_data_pre_write, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_DATA_PRE_READ, 0, NULL, sendmsg_data_pre_read, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_DATA, 0, NULL, sendmsg_data, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_DATA_POST_WRITE, 0, NULL, sendmsg_data_post_write, NULL, NULL);
	M_state_machine_insert_state(m, SENDMSG_DATA_POST_READ, 0, NULL, sendmsg_data_post_read, NULL, NULL);
	return m;
}

M_state_machine_t * smtp_flow_tcp()
{
	M_state_machine_t *m         = NULL;
	M_state_machine_t *sendmsg_m = NULL;
	m = M_state_machine_create(0, "SMTP-flow-tcp", M_STATE_MACHINE_NONE);
	sendmsg_m = smtp_flow_tcp_sendmsg();
	M_state_machine_insert_state(m, START, 0, NULL, start, NULL, NULL);
	M_state_machine_insert_sub_state_machine(m, SENDMSG, 0, NULL, sendmsg_m, sendmsg_pre, sendmsg_post, NULL, NULL);
	M_state_machine_insert_state(m, QUIT_WRITE, 0, NULL, quit_write, NULL, NULL);
	M_state_machine_insert_state(m, QUIT_READ, 0, NULL, quit_read, NULL, NULL);
	return m;
}
