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

#ifndef __NET_SMTP_QUEUE_H__
#define __NET_SMTP_QUEUE_H__

#include "m_net_smtp_int.h"

typedef struct {
    const M_net_smtp_t                *sp;
    size_t                             max_number_of_attempts;
    M_list_t                          *retry_queue;
    M_list_t                          *retry_timeout_queue;
    M_thread_rwlock_t                 *retry_queue_rwlock;
    M_list_str_t                      *internal_queue;
    M_thread_rwlock_t                 *internal_queue_rwlock;
    M_bool                             is_external_queue_enabled;
    M_bool                             is_external_queue_pending;
    size_t                             retry_default_ms;
    char *                           (*external_queue_get_cb)(void);
} M_net_smtp_queue_t;

typedef struct {
    const M_net_smtp_t *sp;
    const char *msg;
    const M_hash_dict_t *headers;
    M_bool is_backout;
    size_t num_tries;
    const char* errmsg;
    size_t retry_ms;
} M_net_smtp_queue_reschedule_msg_args_t;

M_net_smtp_queue_t * M_net_smtp_queue_create(M_net_smtp_t *sp, size_t max_number_of_attempts, size_t retry_default_ms);

void                 M_net_smtp_queue_destroy               (M_net_smtp_queue_t *q);
M_bool               M_net_smtp_queue_is_pending            (M_net_smtp_queue_t *q);
void                 M_net_smtp_queue_advance               (M_net_smtp_queue_t *q);
void                 M_net_smtp_queue_advance_task          (M_event_t *el, M_event_type_t type, M_io_t *io, void *q);
void                 M_net_smtp_queue_reschedule_msg        (M_net_smtp_queue_reschedule_msg_args_t *args);

/* These are pass-through functions for the API */
M_list_str_t *       M_net_smtp_queue_dump                  (M_net_smtp_queue_t *q);
void                 M_net_smtp_queue_set_num_attempts      (M_net_smtp_queue_t *q, size_t num);
M_bool               M_net_smtp_queue_smtp_int              (M_net_smtp_queue_t *q, const M_email_t *e);
M_bool               M_net_smtp_queue_message_int           (M_net_smtp_queue_t *q, const char *msg);
M_bool               M_net_smtp_queue_use_external_queue    (M_net_smtp_queue_t *q, char *(*get_cb)(void));
void                 M_net_smtp_queue_external_have_messages(M_net_smtp_queue_t *q);

#endif
