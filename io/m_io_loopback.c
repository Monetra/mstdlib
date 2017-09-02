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

struct M_io_handle {
	M_io_t *reader;
	M_io_t *writer;
};

static void M_io_loopback_close(M_io_handle_t *handle)
{
	if (handle->reader)
		M_io_destroy(handle->reader);

	if (handle->writer)
		M_io_destroy(handle->writer);

	handle->reader = NULL;
	handle->writer = NULL;
}


static void M_io_loopback_handle_event(M_event_t *event, M_event_type_t type, M_io_t *io, void *arg)
{
	M_io_layer_t  *layer  = arg;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)event;

	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			/* Deliver only the connected event for the reader, ignore the writer */
			if (io == handle->reader)
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
			break;
		case M_EVENT_TYPE_READ:
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			break;
		case M_EVENT_TYPE_WRITE:
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
			M_io_loopback_close(handle);
			break;
		case M_EVENT_TYPE_ERROR:
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
			M_io_loopback_close(handle);
			break;
		default:
			/* Ignore anything else */
			break;
	}
}


static M_bool M_io_loopback_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (handle->reader == NULL || handle->writer == NULL)
		return M_FALSE;

	/* Register each end of the pipe */
	if (!M_event_add(event, handle->reader, M_io_loopback_handle_event, layer))
		return M_FALSE;

	if (!M_event_add(event, handle->writer, M_io_loopback_handle_event, layer))
		return M_FALSE;

	return M_TRUE;
}


static M_bool M_io_loopback_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	/* No reason to interpret events, they're all internally-triggered soft-events, just pass them on */
	(void)layer;
	(void)type;
	return M_FALSE;
}


static void M_io_loopback_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->reader == NULL || handle->writer == NULL)
		return;

	M_event_remove(handle->reader);
	M_event_remove(handle->writer);
}


static M_io_error_t M_io_loopback_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (layer == NULL || handle == NULL || handle->reader == NULL)
		return M_IO_ERROR_INVALID;
	return M_io_read(handle->reader, buf, *read_len, read_len);
}


static M_io_error_t M_io_loopback_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (layer == NULL || handle == NULL || handle->writer == NULL)
		return M_IO_ERROR_INVALID;
	return M_io_write(handle->writer, buf, *write_len, write_len);
}


static void M_io_loopback_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	M_io_loopback_close(handle);

	M_free(handle);
}


static M_io_state_t M_io_loopback_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->writer == NULL || handle->reader == NULL)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}


M_io_error_t M_io_loopback_create(M_io_t **io_out)
{
	M_io_handle_t    *handle;
	M_io_error_t      err;
	M_io_callbacks_t *callbacks;

	if (io_out == NULL)
		return M_IO_ERROR_INVALID;

	handle = M_malloc_zero(sizeof(*handle));
	err    = M_io_pipe_create(&handle->reader, &handle->writer);
	if (err != M_IO_ERROR_SUCCESS) {
		M_free(handle);
		return err;
	}

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_loopback_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_loopback_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_loopback_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_loopback_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_loopback_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_loopback_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_loopback_state_cb);
	M_io_layer_add(*io_out, "LOOPBACK", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}

