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

#define CONNECTION_MASK_NONE      (0u)
#define CONNECTION_MASK_IO        (1u << 0u)
#define CONNECTION_MASK_IO_STDIN  (1u << 1u)
#define CONNECTION_MASK_IO_STDOUT (1u << 2u)
#define CONNECTION_MASK_IO_STDERR (1u << 3u)

typedef struct {
	endpoint_type_t    endpoint_type;
	M_bool             is_alive;
	unsigned int       connection_mask;
	M_io_t            *io;
	M_state_machine_t *state_machine;
	char              *msg;
	size_t             msg_pos;
	size_t             msg_len;
	size_t             email_position;
	M_email_t         *email;
	char              *email_body;
	M_hash_dict_t     *email_hash_dict;
	M_net_smtp_t      *sp;
	const void        *endpoint_manager;
	size_t             number_of_tries;
	M_bool             is_failure;
	int                result_code;
	char               errmsg[128];
	M_buf_t           *out_buf;
	M_parser_t        *in_parser;

	/* Only used for proc endpoints */
	M_io_t            *io_stdin;
	M_io_t            *io_stdout;
	M_io_t            *io_stderr;
} endpoint_slot_t;

M_state_machine_t *smtp_flow_process(void);
M_state_machine_t *smtp_flow_tcp_sendmsg(void);
M_state_machine_t *smtp_flow_tcp(void);

#endif
