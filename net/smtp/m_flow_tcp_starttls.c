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
    STATE_STARTTLS = 1,
    STATE_STARTTLS_RESPONSE,
} m_state_ids;

static M_state_machine_status_t M_state_starttls(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session = data;

    M_bprintf(session->out_buf, "STARTTLS\r\n");
    *next = STATE_STARTTLS_RESPONSE;
    return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_starttls_response_post_cb(void *data,
        M_state_machine_status_t sub_status, M_uint64 *next)
{
    M_net_smtp_session_t     *session        = data;
    (void)next;

    if (sub_status != M_STATE_MACHINE_STATUS_DONE)
        return M_STATE_MACHINE_STATUS_ERROR_STATE;

    if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 220))
        return M_STATE_MACHINE_STATUS_ERROR_STATE;

    session->tcp.tls_state = M_NET_SMTP_TLS_STARTTLS_READY;
    return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * M_net_smtp_flow_tcp_starttls(void)
{
    M_state_machine_t *m = NULL;

    m = M_state_machine_create(0, "SMTP-flow-tcp-sendmsg", M_STATE_MACHINE_NONE);
    M_state_machine_insert_state(m, STATE_STARTTLS, 0, "Start TLS", M_state_starttls, NULL, NULL);
    M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_STARTTLS_RESPONSE, M_starttls_response_post_cb, "Start TLS response");

    return m;
}
