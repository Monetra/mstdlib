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

#ifndef __NET_SMTP_FLOW_H__
#define __NET_SMTP_FLOW_H__

M_bool M_net_smtp_flow_tcp_check_smtp_response_code(M_net_smtp_session_t *session, M_uint64 expected_code);

void                       M_net_smtp_flow_tcp_smtp_response_insert_subm(
		M_state_machine_t *m, M_uint64 id, M_state_machine_post_cb post_cb);
M_state_machine_t         *M_net_smtp_flow_process(void);
M_state_machine_cleanup_t *M_net_smtp_flow_tcp_smtp_response_cleanup(void);
M_state_machine_t         *M_net_smtp_flow_tcp_smtp_response(void);
M_state_machine_t         *M_net_smtp_flow_tcp_starttls(void);
M_state_machine_t         *M_net_smtp_flow_tcp_sendmsg(void);
M_state_machine_t         *M_net_smtp_flow_tcp_auth(void);
M_state_machine_t         *M_net_smtp_flow_tcp_ehlo(void);
M_state_machine_t         *M_net_smtp_flow_tcp(void);

#endif
