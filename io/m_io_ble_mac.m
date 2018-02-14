/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"

M_io_handle_t *M_io_ble_open(const char *uuid, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	M_io_handle_t *handle = NULL;
	struct M_list_callbacks lcbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_io_ble_data_destory
	};

	*ioerr = M_IO_ERROR_SUCCESS;

	if (M_str_isempty(uuid)) {
		*ioerr = M_IO_ERROR_INVALID;
		return NULL;
	}

	handle              = M_malloc_zero(sizeof(*handle));
	M_str_cpy(handle->uuid, sizeof(handle->uuid), uuid);
	handle->read_queue  = M_list_create(&lcbs, M_LIST_NONE);
	handle->write_queue = M_list_create(&lcbs, M_LIST_NONE);
	handle->timeout_ms  = M_io_ble_validate_timeout(timeout_ms);

	return handle;
}


M_bool M_io_ble_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

M_io_state_t M_io_ble_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return handle->state;
}

void M_io_ble_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	M_io_ble_close(handle);
	M_list_destroy(handle->read_queue, M_TRUE);
	M_list_destroy(handle->write_queue, M_TRUE);
	M_free(handle);
}

M_bool M_io_ble_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	return M_FALSE;
}

M_io_error_t M_io_ble_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)write_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_ble_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)read_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

static void M_io_ble_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	(void)dummy_io;
	(void)type;
	(void)event;

	/* Lock! */
	layer = M_io_layer_acquire(handle->io, 0, NULL);

	handle->timer = NULL;

	if (handle->state == M_IO_STATE_CONNECTING) {
		M_io_ble_close(handle);
		M_snprintf(handle->error, sizeof(handle->error), "Timeout waiting on connect");
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
	} else if (handle->state == M_IO_STATE_DISCONNECTING) {
		M_io_ble_close(handle);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	} else {
		/* Shouldn't ever happen */
	}

	M_io_layer_release(layer);
}

M_bool M_io_ble_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_IO_STATE_CONNECTED && handle->state != M_IO_STATE_DISCONNECTING)
		return M_TRUE;

	M_io_ble_close(handle);
	return M_TRUE;
}

void M_io_ble_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_ble_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	switch (handle->state) {
		case M_IO_STATE_INIT:
			handle->state = M_IO_STATE_CONNECTING;
			handle->io    = io;
			M_io_ble_connect(handle);
			/* Fall-thru */
		case M_IO_STATE_CONNECTING:
			/* start timer to time out operation */
			handle->timer = M_event_timer_oneshot(event, handle->timeout_ms, M_TRUE, M_io_ble_timer_cb, handle);
			break;
		case M_IO_STATE_CONNECTED:
			/* Trigger connected soft event when registered with event handle */
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

			/* If there is data in the read buffer, signal there is data to be read as well */
			if (M_list_len(handle->read_queue) != 0) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}
			break;
		default:
			/* Any other state is an error */
			return M_FALSE;
	}

	return M_TRUE;
}
