/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Since there is a global cache shared by both our event loop(s) and the macOS
 * run loop we need to worry about locks. We don't want to run anything from these
 * callbacks directly. The callbacks have an implict layer lock which could cause
 * a dead lock if we call cache functions directly.
 *
 * For example:
 * 1. OS read event is triggered.
 * 2. M_io_ble_device_read_data is called.
 * 3. M_io_ble_device_read_data locks the ble lock.
 * 4. M_io_ble_destroy_cb is called. This has an implict io layer lock.
 * 5. M_io_ble_destroy_cb calls M_io_ble_close.
 * 6. M_io_ble_close blocks while it waits for the ble lock.
 * 7. M_io_ble_device_read_data attempts to lock the io layer.
 * 8. We have a dead lock because M_io_ble_device_read_data has
 *    the ble lock which M_io_ble_close needs in order to release
 *    the io layer lock but that can't happen until M_io_ble_device_read_data 
 *    release the ble lock but it can't until ...
 *
 * To deal with this we have a destroy and close runner event functions
 * which will run after the functions with the implicit layer locks have
 * exited.
 *
 * Write functions are an exception to this pattern because there isn't a good
 * way to buffer the write data. Also, we want the write function to give us
 * an error condition. Instead we use a try lock in the cache functions. If
 * the try lock fails they'll return M_IO_ERROR_WOULDBLOCK letting the caller
 * know they need to try again.
 */

static void M_io_ble_connect_runner(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;

	(void)event;
	(void)type;
	(void)dummy_io;

	if (arg == NULL)
		return;

	handle->initalized_timer = NULL;
	if (!M_io_ble_initalized()) {
		handle->initalized_timer = M_event_timer_oneshot(event, 50, M_TRUE, M_io_ble_connect_runner, handle);
		return;
	}

	M_io_ble_connect(handle);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_handle_t *M_io_ble_open(const char *uuid, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	M_io_handle_t *handle = NULL;
	struct M_llist_callbacks llcbs = {
		NULL,
		NULL,
		NULL,
		(M_llist_free_func)M_io_ble_rdata_destroy
	};

	*ioerr = M_IO_ERROR_SUCCESS;

	if (M_str_isempty(uuid)) {
		*ioerr = M_IO_ERROR_INVALID;
		return NULL;
	}

	handle             = M_malloc_zero(sizeof(*handle));
	M_str_cpy(handle->uuid, sizeof(handle->uuid), uuid);
	handle->timeout_ms = M_io_ble_validate_timeout(timeout_ms);
	handle->read_queue = M_llist_create(&llcbs, M_LLIST_NONE);

	return handle;
}

M_io_handle_t *M_io_ble_open_with_service(const char *service_uuid, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	M_io_handle_t *handle = NULL;
	struct M_llist_callbacks llcbs = {
		NULL,
		NULL,
		NULL,
		(M_llist_free_func)M_io_ble_rdata_destroy
	};

	*ioerr = M_IO_ERROR_SUCCESS;

	if (M_str_isempty(service_uuid)) {
		*ioerr = M_IO_ERROR_INVALID;
		return NULL;
	}

	handle             = M_malloc_zero(sizeof(*handle));
	M_str_cpy(handle->service_uuid, sizeof(handle->service_uuid), service_uuid);
	handle->timeout_ms = M_io_ble_validate_timeout(timeout_ms);
	handle->read_queue = M_llist_create(&llcbs, M_LLIST_NONE);

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

	handle->io = NULL;
	M_io_ble_close(handle);
	M_llist_destroy(handle->read_queue, M_TRUE);
	M_free(handle);
}

M_bool M_io_ble_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (*type == M_EVENT_TYPE_CONNECTED || *type == M_EVENT_TYPE_ERROR || *type == M_EVENT_TYPE_DISCONNECTED) {
		/* Disable timer */
		if (handle->timer != NULL) {
			M_event_timer_remove(handle->timer);
			handle->timer = NULL;
		}
	}
	/* Do nothing, all events are generated as soft events and we don't have anything to process */
	return M_FALSE;
}

M_io_error_t M_io_ble_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	const char       *const_temp;
	const char       *service_uuid;
	const char       *characteristic_uuid;
	M_io_handle_t    *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t   *mdata;
	M_io_error_t      ret;
	M_io_ble_wtype_t  type;
	M_int64           i64v;
	M_bool            enable = M_TRUE;

	/* Validate we have a meta object. If not we don't know what to do. */
	if (meta == NULL)
		return M_IO_ERROR_INVALID;

	/* Are we connected to anything? */
	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	/* Get our meta data out of the meta object. */
	mdata = M_io_meta_get_layer_data(meta, layer);
	if (mdata == NULL)
		return M_IO_ERROR_INVALID;

	/* Pull the service and characteristic uuids. These might be empty but that's okay
 	 * right now. We'll validate them if we need to use them. */
	const_temp = NULL;
	M_hash_multi_u64_get_str(mdata, M_IO_BLE_META_KEY_SERVICE_UUID, &const_temp);
	service_uuid = const_temp;

	const_temp = NULL;
	M_hash_multi_u64_get_str(mdata, M_IO_BLE_META_KEY_CHARACTERISTIC_UUID, &const_temp) || M_str_isempty(const_temp);
	characteristic_uuid = const_temp;
	
	/* Get the type of write being requested. */
	if (!M_hash_multi_u64_get_int(mdata, M_IO_BLE_META_KEY_WRITE_TYPE, &i64v))
		i64v = 0;
	type = (M_io_ble_wtype_t)i64v;

	/* Validate parameter for the write and do it. */
	switch (type) {
		case M_IO_BLE_WTYPE_REQVAL:
			if (M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid)) {
				return M_IO_ERROR_INVALID;
			}

			ret = M_io_ble_device_req_val(handle->uuid, service_uuid, characteristic_uuid);
			break;
		case M_IO_BLE_WTYPE_REQRSSI:
			ret = M_io_ble_device_req_rssi(handle->uuid);
			break;
		case M_IO_BLE_WTYPE_REQNOTIFY:
			if (M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid)) {
				return M_IO_ERROR_INVALID;
			}
			/* Default to ture if not set. */
			if (!M_hash_multi_u64_get_bool(mdata, M_IO_BLE_META_KEY_NOTIFY, &enable)) {
				enable = M_TRUE;
			}
			ret = M_io_ble_set_device_notify(handle->uuid, service_uuid, characteristic_uuid, enable);
			break;
		case M_IO_BLE_WTYPE_WRITE:
		case M_IO_BLE_WTYPE_WRITENORESP:
			if (M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid) ||
					buf == NULL || write_len == NULL || *write_len == 0)
			{
				return M_IO_ERROR_INVALID;
			}

			ret = M_io_ble_device_write(handle->uuid, service_uuid, characteristic_uuid, buf, *write_len, type==M_IO_BLE_WTYPE_WRITENORESP?M_TRUE:M_FALSE);
			break;
	}

	/* A succesful write without response can write again because there is no
	 * reponse to tell us the device is ready. If we got a would block error
	 * and we can write that means we weren't able to get the ble lock and we
	 * a retry is needed. */
	if ((ret == M_IO_ERROR_SUCCESS && type == M_IO_BLE_WTYPE_WRITENORESP) || (ret == M_IO_ERROR_WOULDBLOCK && handle->can_write))
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);

	return ret;
}

M_io_error_t M_io_ble_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t    *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t   *mdata;
	M_llist_node_t   *node;
	M_io_ble_rdata_t *rdata;

	/* Validate we have a meta object. If not we don't know what to do. */
	if (meta == NULL)
		return M_IO_ERROR_INVALID;

	/* Are we connected to anything? */
	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	/* Get our meta data out of the meta object. If we don't have any then
 	 * this meta object hasn't been used with this layer. We'll add the internal
	 * data so we can populate it. */
	mdata = M_io_meta_get_layer_data(meta, layer);
	if (mdata == NULL) {
		mdata = M_hash_multi_create(M_HASH_MULTI_NONE);
		M_io_meta_insert_layer_data(meta, layer, mdata, (void (*)(void *))M_hash_multi_destroy);
	}

	/* Get the first record in the read queue. If no
 	 * records we're all done. */
	node = M_llist_first(handle->read_queue);
	if (node == NULL)
		return M_IO_ERROR_WOULDBLOCK;
	rdata = M_llist_node_val(node);

	/* Get the type of read the data is for. */
	M_hash_multi_u64_insert_int(mdata, M_IO_BLE_META_KEY_READ_TYPE, rdata->type);
	switch (rdata->type) {
		case M_IO_BLE_RTYPE_READ:
			/* Set the service and characteristic uuids for where the data came from. */
			M_hash_multi_u64_insert_str(mdata, M_IO_BLE_META_KEY_SERVICE_UUID, rdata->d.read.service_uuid);
			M_hash_multi_u64_insert_str(mdata, M_IO_BLE_META_KEY_CHARACTERISTIC_UUID, rdata->d.read.characteristic_uuid);

			/* Read as much data as we can. */
			if (buf != NULL && read_len != NULL) {
				if (*read_len > M_buf_len(rdata->d.read.data))
					*read_len = M_buf_len(rdata->d.read.data);

				M_mem_copy(buf, M_buf_peek(rdata->d.read.data), *read_len);
				M_buf_drop(rdata->d.read.data, *read_len);
			}

			/* If we've read everything we will drop this record. */
			if (M_buf_len(rdata->d.read.data) == 0) {
				M_llist_remove_node(node);
			}
			break;
		case M_IO_BLE_RTYPE_RSSI:
			/* Get the RSSI. */
			M_hash_multi_u64_insert_int(mdata, M_IO_BLE_META_KEY_RSSI, rdata->d.rssi.val);
			M_llist_remove_node(node);
			break;
		case M_IO_BLE_RTYPE_NOTIFY:
			M_hash_multi_u64_insert_str(mdata, M_IO_BLE_META_KEY_SERVICE_UUID, rdata->d.notify.service_uuid);
			M_hash_multi_u64_insert_str(mdata, M_IO_BLE_META_KEY_CHARACTERISTIC_UUID, rdata->d.notify.characteristic_uuid);
			M_llist_remove_node(node);
			break;
	}

	/* node should not be used after this point becase it may have been removed. */

	return M_IO_ERROR_SUCCESS;
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

	M_event_timer_remove(handle->initalized_timer);
	handle->initalized_timer = NULL;
	handle->timer            = NULL;

	if (handle->state == M_IO_STATE_CONNECTING) {
		handle->state = M_IO_STATE_ERROR;
		M_snprintf(handle->error, sizeof(handle->error), "Timeout waiting on connect");
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
	} else if (handle->state == M_IO_STATE_DISCONNECTING) {
		M_io_ble_close(handle);
		handle->state = M_IO_STATE_DISCONNECTED;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	} else {
		/* Shouldn't ever happen */
	}
	M_io_layer_release(layer);

	M_io_ble_close(handle);
}

M_bool M_io_ble_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_IO_STATE_CONNECTED && handle->state != M_IO_STATE_DISCONNECTING)
		return M_TRUE;

	handle->state = M_IO_STATE_DISCONNECTING;
	/* Can't be in a layer lock when M_io_ble_close is called. */
	handle->timer = M_event_timer_oneshot(M_io_get_event(M_io_layer_get_io(layer)), 1, M_TRUE, M_io_ble_timer_cb, handle);
	return M_TRUE;
}

void M_io_ble_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Only thing we can do is disable a timer if there was one */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}
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
			if (!M_io_ble_init_int()) {
				return M_FALSE;
			}
			M_event_queue_task(M_io_get_event(M_io_layer_get_io(layer)), M_io_ble_connect_runner, handle);
			/* Fall-thru */
		case M_IO_STATE_CONNECTING:
			/* start timer to time out operation */
			handle->timer = M_event_timer_oneshot(event, handle->timeout_ms, M_TRUE, M_io_ble_timer_cb, handle);
			break;
		case M_IO_STATE_CONNECTED:
			/* Trigger connected soft event when registered with event handle */
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
			handle->can_write = M_TRUE;

			/* If there is data in the read buffer, signal there is data to be read as well */
			if (M_llist_len(handle->read_queue) != 0) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}
			break;
		default:
			/* Any other state is an error */
			return M_FALSE;
	}

	return M_TRUE;
}
