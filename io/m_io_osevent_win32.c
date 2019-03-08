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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"

#define M_IO_OSEVENT_NAME "WIN32EVENT"

struct M_io_handle {
	M_EVENT_HANDLE     handle;
};


static M_bool M_io_osevent_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	return M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_ADD_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);
}


static M_bool M_io_osevent_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;

	/* Unknown event, consume */
	if (*type != M_EVENT_TYPE_OTHER)
		return M_TRUE;

	/* Pass on */
	return M_FALSE;
}


static void M_io_osevent_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_DEL_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);
}


static void M_io_osevent_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;

	if (handle->handle != NULL) {
		CloseHandle(handle->handle);
		handle->handle = NULL;
	}
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

	if (event == NULL)
		return NULL;

	handle                     = M_malloc_zero(sizeof(*handle));
	handle->handle             = CreateEvent(NULL, FALSE, FALSE, NULL);

	io                         = M_io_init(M_IO_TYPE_EVENT);
	callbacks                  = M_io_callbacks_create();
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

	if (io == NULL || M_io_get_type(io) != M_IO_TYPE_EVENT)
		return;

	layer = M_io_layer_acquire(io, 0, M_IO_OSEVENT_NAME);
	if (layer == NULL)
		return;
	handle = M_io_layer_get_handle(layer);

	SetEvent(handle->handle);
	M_io_layer_release(layer);
}

