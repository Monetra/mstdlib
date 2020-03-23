/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_IO_POSIX_COMMON_H__
#define __M_IO_POSIX_COMMON_H__

#include "m_io_meta.h"

M_io_error_t M_io_posix_err_to_ioerr(int err);
M_bool M_io_posix_errormsg(int err, char *error, size_t err_len);
M_io_error_t M_io_posix_read(M_io_t *comm, int fd, unsigned char *buf, size_t *read_len, int *sys_error, M_io_meta_t *meta);
M_io_error_t M_io_posix_write(M_io_t *io, int fd, const unsigned char *buf, size_t *write_len, int *sys_error, M_io_meta_t *meta);
M_bool M_io_posix_process_cb(M_io_layer_t *layer, M_EVENT_HANDLE rhandle, M_EVENT_HANDLE whandle, M_event_type_t *type);

struct M_io_posix_sigpipe_state {
	M_bool already_pending;
	M_bool blocked;
};
typedef struct M_io_posix_sigpipe_state M_io_posix_sigpipe_state_t;
void M_io_posix_sigpipe_block(M_io_posix_sigpipe_state_t *state);
void M_io_posix_sigpipe_unblock(M_io_posix_sigpipe_state_t *state);
void M_io_posix_fd_set_closeonexec(int fd, M_bool tf);

#endif
