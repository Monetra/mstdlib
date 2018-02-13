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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_ble_enum_free_device(void *arg)
{
	M_io_ble_enum_device_t *device = arg;
	M_free(device->name);
	M_free(device->mac);
	M_free(device->uuid);
	M_free(device);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 M_io_ble_validate_timeout(M_uint64 timeout_ms)
{
	/* We have timeout default of 1 minute when 0 and a max of 5 minutes. */
	if (timeout_ms == 0)
		timeout_ms = 60000;
	if (timeout_ms >= 300000)
		timeout_ms = 300000;

	return timeout_ms;
}

void M_io_ble_data_destory(M_io_ble_data_t *data)
{
	if (data == NULL)
		return;

	M_free(data->data);
	M_free(data);
}

M_io_handle_t *M_io_ble_get_io_handle(M_io_t *io)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle = NULL;
	size_t         len;
	size_t         i;

	len = M_io_layer_count(io);
	for (i=len; i-->0; ) {
		layer = M_io_layer_acquire(io, i, M_IO_BLE_NAME);
		if (layer != NULL) {
			handle = M_io_layer_get_handle(layer);
			M_io_layer_release(layer);
			break;
		}
	}
	return handle;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_io_ble_enum_destroy(M_io_ble_enum_t *btenum)
{
	if (btenum == NULL)
		return;
	M_list_destroy(btenum->devices, M_TRUE);
	M_free(btenum);
}

M_io_ble_enum_t *M_io_ble_enum_init(void)
{
	M_io_ble_enum_t  *btenum  = M_malloc_zero(sizeof(*btenum));
	struct M_list_callbacks listcbs = {
		NULL,
		NULL,
		NULL,
		M_io_ble_enum_free_device
	};
	btenum->devices = M_list_create(&listcbs, M_LIST_NONE);
	return btenum;
}

size_t M_io_ble_enum_count(const M_io_ble_enum_t *btenum)
{
	if (btenum == NULL)
		return 0;
	return M_list_len(btenum->devices);
}

void M_io_ble_enum_add(M_io_ble_enum_t *btenum, const char *name, const char *mac, const char *uuid, M_time_t last_seen, M_bool connected)
{
	M_io_ble_enum_device_t *device;

	if (btenum == NULL || M_str_isempty(mac) || M_str_isempty(uuid))
		return;

	device            = M_malloc_zero(sizeof(*device));
	device->name      = M_strdup(name);
	device->mac       = M_strdup(mac);
	device->uuid      = M_strdup(uuid);
	device->last_seen = last_seen;
	device->connected = connected;

	M_list_insert(btenum->devices, device);
}

const char *M_io_ble_enum_name(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return NULL;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->name;
}

const char *M_io_ble_enum_mac(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return NULL;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->mac;
}

M_bool M_io_ble_enum_connected(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return M_FALSE;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return M_FALSE;
	return device->connected;
}

const char *M_io_ble_enum_service_uuid(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return NULL;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->uuid;
}

M_time_t M_io_ble_enum_last_seen(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return 0;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->last_seen;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_str_t *M_io_ble_get_services(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_services(handle->mac);
}

M_list_str_t *M_io_ble_get_service_characteristics(M_io_t *io, const char *service_uuid)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_service_characteristics(handle->mac, service_uuid);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_error_t M_io_ble_create(M_io_t **io_out, const char *mac, M_uint64 timeout_ms)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || M_str_isempty(mac))
		return M_IO_ERROR_INVALID;

	handle = M_io_ble_open(mac, &err, timeout_ms);
	if (handle == NULL)
		return M_IO_ERROR_INVALID;

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_ble_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_ble_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_ble_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_ble_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_ble_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_ble_destroy_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_ble_disconnect_cb);
	M_io_callbacks_reg_state(callbacks, M_io_ble_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_ble_errormsg_cb);
	M_io_layer_add(*io_out, M_IO_BLE_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}
