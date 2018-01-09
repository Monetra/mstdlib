/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

#ifndef __M_IO_W32OVERLAP_H__
#define __M_IO_W32OVERLAP_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include "m_event_int.h"
#include "m_io_int.h"

struct M_io_handle_w32;
typedef struct M_io_handle_w32 M_io_handle_w32_t;

enum M_io_w32overlap_busyemu {
	M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED        = 0,
	M_IO_W32OVERLAP_BUSYEMU_STATE_RUNNING        = 1,
	M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT = 2
};

typedef enum M_io_w32overlap_busyemu M_io_w32overlap_busyemu_t;

struct M_io_handle {
	DWORD              last_error_sys;

	/* Read state */
	HANDLE             rhandle;
	M_bool             rwaiting;
	M_buf_t           *rbuf;
	M_bool             rbuffull; /*!< Whether last read was a full buffer read or not */
	OVERLAPPED         roverlapped;

	/* Write state */
	HANDLE             whandle;
	M_bool             wwaiting;
	M_buf_t           *wbuf;
	M_bool             wbuffull; /*!< Whether last write was a full buffer write or not */
	OVERLAPPED         woverlapped;

	M_io_handle_w32_t *priv;
	void             (*priv_cleanup)(M_io_handle_t *);

	M_event_timer_t   *disconnect_timer;

	/* Used by BusyEmu only */
	M_io_w32overlap_busyemu_t busyemu_state;
	M_threadid_t              busyemu_thread;
};


M_io_handle_t *M_io_w32overlap_init_handle(HANDLE rhandle, HANDLE whandle);
void M_io_w32overlap_update_handle(M_io_handle_t *handle, HANDLE rhandle, HANDLE whandle);
void M_io_w32overlap_close(M_io_layer_t *layer);
M_bool M_io_w32overlap_init_cb(M_io_layer_t *layer);
M_bool M_io_w32overlap_process_cb(M_io_layer_t *layer, M_event_type_t *type);
void M_io_w32overlap_unregister_cb(M_io_layer_t *layer);
void M_io_w32overlap_destroy_handle(M_io_handle_t *handle);
void M_io_w32overlap_destroy_cb(M_io_layer_t *layer);
M_io_error_t M_io_w32overlap_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);
M_io_error_t M_io_w32overlap_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);
M_io_state_t M_io_w32overlap_state_cb(M_io_layer_t *layer);
M_bool M_io_w32overlap_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len);
M_bool M_io_w32overlap_disconnect_cb(M_io_layer_t *layer);

M_io_handle_t *M_io_w32overlap_busyemu_init_handle(HANDLE rhandle, HANDLE whandle);
void M_io_w32overlap_busyemu_update_handle(M_io_handle_t *handle, HANDLE rhandle, HANDLE whandle);
void M_io_w32overlap_busyemu_close(M_io_layer_t *layer);
M_bool M_io_w32overlap_busyemu_init_cb(M_io_layer_t *layer);
M_bool M_io_w32overlap_busyemu_process_cb(M_io_layer_t *layer, M_event_type_t *type);
void M_io_w32overlap_busyemu_unregister_cb(M_io_layer_t *layer);
void M_io_w32overlap_busyemu_destroy_handle(M_io_handle_t *handle);
void M_io_w32overlap_busyemu_destroy_cb(M_io_layer_t *layer);
M_io_error_t M_io_w32overlap_busyemu_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);
M_io_error_t M_io_w32overlap_busyemu_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);
M_io_state_t M_io_w32overlap_busyemu_state_cb(M_io_layer_t *layer);
M_bool M_io_w32overlap_busyemu_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len);
M_bool M_io_w32overlap_busyemu_disconnect_cb(M_io_layer_t *layer);


#endif
