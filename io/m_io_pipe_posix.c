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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "m_io_posix_common.h"

/* XXX: currently needed for M_io_setnonblock() which should be moved */
#include "m_io_int.h"

struct M_io_handle {
	M_EVENT_HANDLE handle;
	int            last_error_sys;
};


static M_bool M_io_pipe_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	M_io_type_t    type   = M_io_get_type(io);

	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return M_FALSE;

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

	/* Add the handles with read wait type if reader, or no waittype if writer */
	M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, (type == M_IO_TYPE_WRITER)?0:M_EVENT_WAIT_READ, (type == M_IO_TYPE_WRITER)?M_EVENT_CAPS_WRITE:M_EVENT_CAPS_READ);
	return M_TRUE;
}


static void M_io_pipe_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return;
	M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);
}


static void M_io_pipe_close_handle(M_io_t *comm, M_io_handle_t *handle)
{
	M_event_t     *event  = M_io_get_event(comm);
	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return;

	if (event)
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, comm, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);

	close(handle->handle);
	handle->handle = M_EVENT_INVALID_HANDLE;
}


static void M_io_pipe_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	if (handle == NULL)
		return;

	M_io_pipe_close_handle(io, handle);

	M_free(handle);
}


static M_io_error_t M_io_pipe_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_error_t   err;
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	M_io_t        *io      = M_io_layer_get_io(layer);

	if (io == NULL || layer == NULL || buf == NULL || read_len == NULL || *read_len == 0 || M_io_get_type(io) != M_IO_TYPE_READER)
		return M_IO_ERROR_INVALID;

	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return M_IO_ERROR_ERROR;

	err = M_io_posix_read(io, handle->handle, buf, read_len, &handle->last_error_sys);
	if (M_io_error_is_critical(err))
		M_io_pipe_close_handle(io, handle);
		
	return err;
}


static M_io_error_t M_io_pipe_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_error_t   err;
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	M_io_t        *io      = M_io_layer_get_io(layer);

	if (io == NULL || layer == NULL || buf == NULL || write_len == NULL || *write_len == 0 || M_io_get_type(io) != M_IO_TYPE_WRITER)
		return M_IO_ERROR_INVALID;

	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return M_IO_ERROR_ERROR;

	err = M_io_posix_write(io, handle->handle, buf, write_len, &handle->last_error_sys);
	if (M_io_error_is_critical(err))
		M_io_pipe_close_handle(io, handle);

	return err;
}


static M_io_state_t M_io_pipe_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	if (handle->handle == M_EVENT_INVALID_HANDLE)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}


static M_bool M_io_pipe_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	return M_io_posix_errormsg(handle->last_error_sys, error, err_len);
}


static M_bool M_io_pipe_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    iotype = M_io_get_type(io);

	(void)type;

	/* Pass on */
	return M_io_posix_process_cb(layer, (iotype == M_IO_TYPE_READER)?handle->handle:M_EVENT_INVALID_HANDLE, (iotype == M_IO_TYPE_WRITER)?handle->handle:M_EVENT_INVALID_HANDLE, type);
}


M_io_error_t M_io_pipe_create(M_io_t **reader, M_io_t **writer)
{
	M_io_handle_t    *rhandle;
	M_io_handle_t    *whandle;
	M_io_callbacks_t *callbacks;
	int            pipefds[2];

	if (reader == NULL || writer == NULL)
		return M_IO_ERROR_ERROR;

	*reader = NULL;
	*writer = NULL;

#if defined(HAVE_PIPE2) && defined(O_CLOEXEC)
	if (pipe2(pipefds, O_CLOEXEC) == 0) {
#else
	if (pipe(pipefds) == 0) {
		M_io_posix_fd_set_closeonexec(pipefds[0]);
		M_io_posix_fd_set_closeonexec(pipefds[1]);
#endif
	} else {
		return M_io_posix_err_to_ioerr(errno);
	}

	if (!M_io_setnonblock(pipefds[0]) || !M_io_setnonblock(pipefds[1])) {
		return M_IO_ERROR_ERROR;
	}

	rhandle         = M_malloc_zero(sizeof(*rhandle));
	whandle         = M_malloc_zero(sizeof(*whandle));
	rhandle->handle = pipefds[0];
	whandle->handle = pipefds[1];

	*reader   = M_io_init(M_IO_TYPE_READER);
	*writer   = M_io_init(M_IO_TYPE_WRITER);

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_pipe_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_pipe_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_pipe_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_pipe_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_pipe_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_pipe_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_pipe_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_pipe_errormsg_cb);
	M_io_layer_add(*reader, "PIPEREAD", rhandle, callbacks);
	M_io_layer_add(*writer, "PIPEWRITE", whandle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}

