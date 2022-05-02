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

#ifndef __SMTP_FLOW_H__
#define __SMTP_FLOW_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

typedef enum {
	PROCESS_ENDPOINT = 1,
	TCP_ENDPOINT,
} endpoint_type_t;

typedef struct {
	endpoint_type_t    endpoint_type;
	M_bool             is_io_connected;
	M_io_t            *io;
	M_state_machine_t *state_machine;
	char              *msg;
	size_t             msg_pos;
	size_t             msg_len;
	M_event_t         *event;
	M_event_type_t     event_type;
	M_io_t            *event_io;
	M_bool             is_alive;
	M_net_smtp_t      *sp;
	const void        *endpoint_manager;
	size_t             number_of_tries;
	M_bool             is_failure;
	M_bool             is_queue_task;
	int                result_code;
	char               errmsg[128];
	char               proc_stdout[128];
	size_t             proc_stdout_len;
	char               proc_stderror[128];
	size_t             proc_stderror_len;

	/* Only used for proc endpoints */
	M_bool             is_io_stdin_connected;
	M_bool             is_io_stdout_connected;
	M_bool             is_io_stderr_connected;
	M_io_t            *io_stdin;
	M_io_t            *io_stdout;
	M_io_t            *io_stderr;
} endpoint_slot_t;

M_state_machine_t *smtp_flow_process(void);
M_state_machine_t *smtp_flow_tcp(void);

#endif
