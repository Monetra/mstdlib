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

/* In order to detect a problem with a process over-eagerly consuming a message containing a
 * "\r\n.\r\n", messages will be split into two parts at the first occurance.
    *
 * sendmail will if -i isn't specified for example
 */

typedef enum {
    STATE_WRITE_FIRST_PART = 1,
    STATE_WRITE_WAIT,
    STATE_WRITE_SECOND_PART,
    STATE_WRITE_FINISH,
    STATE_DISCONNECTING,
} m_state_ids;

static M_state_machine_status_t M_state_write_first_part(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session    = data;
    size_t                len        = 0;
    M_parser_t           *parser     = NULL;

    session->process.len = M_str_len(session->msg);
    parser = M_parser_create_const((unsigned char *)session->msg, session->process.len, M_PARSER_SPLIT_FLAG_NONE);


    len = M_parser_consume_until(parser, (unsigned char *)"\r\n.\r\n", 5, M_TRUE);
    M_parser_destroy(parser);

    if (len == 0) {
        /* There is no second part. Send as is. */
        session->process.len = 0;
        M_buf_add_str(session->out_buf, session->msg);
        *next = STATE_WRITE_FINISH;
        return M_STATE_MACHINE_STATUS_NEXT;
    }

    M_buf_add_str_max(session->out_buf, session->msg, len);
    session->process.msg_second_part = session->msg + len;
    session->process.len -= len;
    session->process.is_done_waiting = M_FALSE;
    *next = STATE_WRITE_WAIT;
    return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_write_wait(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session = data;
    if (!session->process.is_done_waiting) {
        return M_STATE_MACHINE_STATUS_WAIT;
    }

    *next = STATE_WRITE_SECOND_PART;
    return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_write_second_part(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session = data;
    if (M_buf_len(session->out_buf) > 0) {
        return M_STATE_MACHINE_STATUS_WAIT;
    }

    /* If we made it this far then the process has proven itself capable of consuming
     * a full-stop.  Send the rest of the message regardless of additional full-stops.
     */

    M_buf_add_str(session->out_buf, session->process.msg_second_part);
    session->process.len = 0;
    *next = STATE_WRITE_FINISH;
    return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_write_finish(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session = data;
    if (M_buf_len(session->out_buf) > 0) {
        return M_STATE_MACHINE_STATUS_WAIT;
    }
    session->is_successfully_sent = M_TRUE;
    *next = STATE_DISCONNECTING;
    return M_STATE_MACHINE_STATUS_NEXT;

}

static M_state_machine_status_t M_state_disconnecting(void *data, M_uint64 *next)
{
    M_net_smtp_session_t *session = data;
    (void)next;

    if (session->connection_mask != M_NET_SMTP_CONNECTION_MASK_NONE)
        return M_STATE_MACHINE_STATUS_WAIT;

    return M_STATE_MACHINE_STATUS_DONE;
}

M_state_machine_t * M_net_smtp_flow_process(void)
{
    M_state_machine_t *m;
    m = M_state_machine_create(0, "M-net-smtp-flow-process", M_STATE_MACHINE_NONE);
    M_state_machine_insert_state(m, STATE_WRITE_FIRST_PART, 0, NULL, M_state_write_first_part, NULL, NULL);
    M_state_machine_insert_state(m, STATE_WRITE_WAIT, 0, NULL, M_state_write_wait, NULL, NULL);
    M_state_machine_insert_state(m, STATE_WRITE_SECOND_PART, 0, NULL, M_state_write_second_part, NULL, NULL);
    M_state_machine_insert_state(m, STATE_WRITE_FINISH, 0, NULL, M_state_write_finish, NULL, NULL);
    M_state_machine_insert_state(m, STATE_DISCONNECTING, 0, NULL, M_state_disconnecting, NULL, NULL);
    return m;
}
