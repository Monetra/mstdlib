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
	M_io_trace_cb_t      callback;
	void                *cb_arg;
	M_io_trace_cb_dup_t  callback_duplicate;
	M_io_trace_cb_free_t callback_free;
};

static M_bool M_io_trace_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
	return M_TRUE;
}


static M_bool M_io_trace_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	handle->callback(handle->cb_arg, M_IO_TRACE_TYPE_EVENT, *type, NULL, 0);
	return M_FALSE;
}


static void M_io_trace_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
}


static M_io_error_t M_io_trace_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;

	if (layer == NULL || handle == NULL)
		return M_IO_ERROR_INVALID;

	err = M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, read_len);
	if (err == M_IO_ERROR_SUCCESS) {
		handle->callback(handle->cb_arg, M_IO_TRACE_TYPE_READ, M_EVENT_TYPE_READ, buf, *read_len);
	}
	return err;
}


static M_io_error_t M_io_trace_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;

	if (layer == NULL || handle == NULL)
		return M_IO_ERROR_INVALID;

	err = M_io_layer_write(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, write_len);
	if (err == M_IO_ERROR_SUCCESS) {
		handle->callback(handle->cb_arg, M_IO_TRACE_TYPE_WRITE, M_EVENT_TYPE_WRITE, buf, *write_len);
	}
	return err;
}


static void M_io_trace_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	if (handle->callback_free)
		handle->callback_free(handle->cb_arg);

	M_free(handle);
}

static M_io_error_t M_io_trace_accept_cb(M_io_t *io, M_io_layer_t *orig_layer)
{
	size_t         layer_id;
	M_io_handle_t *orig_handle = M_io_layer_get_handle(orig_layer);
	void          *arg         = orig_handle->cb_arg;

	/* Its possible the argument is connection specific, so we might need to
	 * duplicate it */
	if (orig_handle->callback_duplicate)
		arg = orig_handle->callback_duplicate(orig_handle->cb_arg);

	/* Add a new layer into the new comm object with the same settings as we have */
	return M_io_add_trace(io, &layer_id, orig_handle->callback, arg, orig_handle->callback_duplicate, orig_handle->callback_free);
}

M_io_error_t M_io_add_trace(M_io_t *io, size_t *layer_id, M_io_trace_cb_t callback, void *cb_arg, M_io_trace_cb_dup_t cb_dup, M_io_trace_cb_free_t cb_free)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_layer_t     *layer;

	if (io == NULL || callback == NULL)
		return M_IO_ERROR_INVALID;

	handle                     = M_malloc_zero(sizeof(*handle));
	handle->callback           = callback;
	handle->cb_arg             = cb_arg;
	handle->callback_duplicate = cb_dup;
	handle->callback_free      = cb_free;

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_trace_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_trace_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_trace_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_trace_process_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_trace_accept_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_trace_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_trace_destroy_cb);
	layer = M_io_layer_add(io, "TRACE", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);
	return M_IO_ERROR_SUCCESS;
}


void *M_io_trace_get_callback_arg(M_io_t *io, size_t layer_id)
{
	M_io_layer_t  *layer = M_io_layer_acquire(io, layer_id, "TRACE");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	void          *arg;

	if (layer == NULL || handle == NULL)
		return NULL;

	arg = handle->cb_arg;

	M_io_layer_release(layer);

	return arg;
}


M_bool M_io_trace_set_callback_arg(M_io_t *io, size_t layer_id, void *cb_arg)
{
	M_io_layer_t  *layer = M_io_layer_acquire(io, layer_id, "TRACE");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	handle->cb_arg = cb_arg;

	M_io_layer_release(layer);

	return M_TRUE;
}
