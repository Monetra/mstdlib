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

/* XXX: currently needed for M_io_setnonblock() which should be moved */
#include "m_io_int.h"
#include "m_io_posix_common.h"

#define M_IO_OSEVENT_NAME "PIPEEVENT"

struct M_io_handle {
	M_EVENT_HANDLE    *handles;
	size_t             handles_cnt;
};


static M_bool M_io_osevent_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	return M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_ADD_HANDLE, io, handle->handles[0], M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ);
}


static M_bool M_io_osevent_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	unsigned char  tmp[32];
	ssize_t        bytes;
	size_t         total_read = 0;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	switch (*type) {
		case M_EVENT_TYPE_READ:
			/* Read until either error, EWOULDBLOCK/EAGAIN, or partial read occurs.  If any
			 * bytes were read, success */
			while ((bytes = read(handle->handles[0], tmp, sizeof(tmp))) > 0) {
				total_read += (size_t)bytes;
			}

			/* If error condition and no bytes were read, consume the event and wait
			 * on a new one */
			if (bytes <= 0 && total_read == 0)
				return M_TRUE;

			/* Do not consume the event, pass it on, but rewrite it as M_EVENT_TYPE_OTHER */
			*type = M_EVENT_TYPE_OTHER;
			return M_FALSE;
		default:
			/* Hmm, what other legitimate event could there be for this?  I'd think none */
			break;
	}

	/* Consume this unknown event, I can't see what purpose passing it on could have */
	return M_TRUE;
}


static void M_io_osevent_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_DEL_HANDLE, io, handle->handles[0], M_EVENT_INVALID_SOCKET, 0, 0);
}


static void M_io_osevent_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;

	if (handle->handles_cnt == 2) {
		if (handle->handles[0] != -1) {
			close(handle->handles[0]);
		}
		if (handle->handles[1] != -1)
			close(handle->handles[1]);
	}
	M_free(handle->handles);
	M_free(handle);
}


static M_io_state_t M_io_osevent_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_CONNECTED;
}


M_io_t *M_io_osevent_create(M_event_t *event)
{
	M_io_t           *io;
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;

	handle                     = M_malloc_zero(sizeof(*handle));
	handle->handles_cnt        = 2;
	handle->handles            = M_malloc_zero(sizeof(*handle->handles) * handle->handles_cnt);

#if defined(HAVE_PIPE2) && defined(O_CLOEXEC)
	if (pipe2(handle->handles, O_CLOEXEC) == 0) {
#else
	if (pipe(handle->handles) == 0) {
		M_io_posix_fd_set_closeonexec(handle->handles[0]);
		M_io_posix_fd_set_closeonexec(handle->handles[1]);
#endif
	} else {
		M_free(handle->handles);
		M_free(handle);
		return NULL;
	}

	if (!M_io_setnonblock(handle->handles[0]) || !M_io_setnonblock(handle->handles[1])) {
		close(handle->handles[0]);
		close(handle->handles[1]);
		M_free(handle->handles);
		M_free(handle);
		return NULL;
	}

	io        = M_io_init(M_IO_TYPE_EVENT);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_osevent_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_osevent_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_osevent_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_osevent_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_osevent_state_cb);
	M_io_layer_add(io, M_IO_OSEVENT_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	M_event_add(event, io, NULL, NULL);
	return io;
}


void M_io_osevent_trigger(M_io_t *io)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	unsigned char data[] = { 0x01 };
	ssize_t retval;

	if (io == NULL || M_io_get_type(io) != M_IO_TYPE_EVENT)
		return;

	layer  = M_io_layer_acquire(io, 0, M_IO_OSEVENT_NAME);
	if (layer == NULL)
		return;

	handle = M_io_layer_get_handle(layer);

	/* Ignore errors, if it can't write, that means the pipe is already full of
	 * events and we really only actually deliver one anyhow */
	retval = (ssize_t)write(handle->handles[1], data, sizeof(data));

	M_io_layer_release(layer);

	/* We have to do this to avoid some compiler warnings about unused results, even
	 * though this actually _is_ valid in this circumstance */
	(void)retval;
}


