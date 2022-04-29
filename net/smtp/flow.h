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

typedef struct {
	M_io_t            *io;
	M_state_machine_t *state_machine;
	char              *msg;
	M_event_t         *event;
	M_event_type_t     event_type;
	M_io_t            *event_io;
	M_bool             is_alive;
	M_net_smtp_t      *sp;

	/* Only used for proc endpoints */
	M_io_t            *io_stdin;
	M_io_t            *io_stdout;
	M_io_t            *io_stderr;
} endpoint_slot_t;

M_state_machine_t *smtp_flow_process(void);
M_state_machine_t *smtp_flow_tcp(void);

#endif
