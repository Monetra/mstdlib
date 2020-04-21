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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_hash_multi_t *M_io_ble_get_meta_data(M_io_t *io, M_io_meta_t *meta)
{
	M_hash_multi_t *d;
	M_io_layer_t   *layer  = NULL;
	M_io_handle_t  *handle = NULL;
	size_t          len;
	size_t          i;

	len = M_io_layer_count(io);
	for (i=len; i-->0; ) {
		layer = M_io_layer_acquire(io, i, M_IO_BLE_NAME);
		if (layer != NULL) {
			handle = M_io_layer_get_handle(layer);
			break;
		}
	}

	/* Couldn't find our layer. */
	if (handle == NULL)
		return NULL;

	d  = M_io_meta_get_layer_data(meta, layer);
	if (d == NULL) {
		d = M_hash_multi_create(M_HASH_MULTI_NONE);
		M_io_meta_insert_layer_data(meta, layer, d, (void (*)(void *))M_hash_multi_destroy);
	}
	M_io_layer_release(layer);

	return d;
}

static M_io_handle_t *M_io_ble_get_io_handle(M_io_t *io)
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

M_uint64 M_io_ble_validate_timeout(M_uint64 timeout_ms)
{
	/* We have timeout default of 1 minute when 0 and a max of 5 minutes. */
	if (timeout_ms == 0)
		timeout_ms = 60000;
	if (timeout_ms >= 300000)
		timeout_ms = 300000;

	return timeout_ms;
}

void M_io_ble_enum_free_device(M_io_ble_enum_device_t *dev)
{
	if (dev == NULL)
		return;
	M_list_str_destroy(dev->service_uuids);
	M_free(dev);
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
		(M_list_free_func)M_io_ble_enum_free_device
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

void M_io_ble_enum_add(M_io_ble_enum_t *btenum, const M_io_ble_enum_device_t *edev)
{
	M_io_ble_enum_device_t *device;

	if (btenum == NULL || edev == NULL)
		return;

	device                = M_malloc_zero(sizeof(*device));
	device->service_uuids = M_list_str_duplicate(edev->service_uuids);
	device->last_seen     = edev->last_seen;
	M_str_cpy(device->name, sizeof(device->name), edev->name);
	M_str_cpy(device->identifier, sizeof(device->identifier), edev->identifier);

	M_list_insert(btenum->devices, device);
}

const char *M_io_ble_enum_identifier(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return NULL;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->identifier;
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

M_list_str_t *M_io_ble_enum_service_uuids(const M_io_ble_enum_t *btenum, size_t idx)
{
	const M_io_ble_enum_device_t *device;
	if (btenum == NULL)
		return NULL;
	device = M_list_at(btenum->devices, idx);
	if (device == NULL)
		return NULL;
	return M_list_str_duplicate(device->service_uuids);
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

char *M_io_ble_get_identifier(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_identifier(handle->uuid);
}

char *M_io_ble_get_name(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_name(handle->uuid);
}

M_list_str_t *M_io_ble_get_services(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_services(handle->uuid);
}

M_list_str_t *M_io_ble_get_service_characteristics(M_io_t *io, const char *service_uuid)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return NULL;
	return M_io_ble_get_device_service_characteristics(handle->uuid, service_uuid);
}

M_io_ble_property_t M_io_ble_get_characteristic_properties(M_io_t *io, const char *service_uuid, const char *characteristic_uuid)
{
	M_io_handle_t *handle;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return M_IO_BLE_PROPERTY_NONE;
	return M_io_ble_get_device_service_characteristic_properties(handle->uuid, service_uuid, characteristic_uuid);
}

void M_io_ble_get_max_write_sizes(M_io_t *io, size_t *with_response, size_t *without_response)
{
	M_io_handle_t *handle;
	size_t         w;
	size_t         wo;

	if (with_response == NULL)
		with_response = &w;
	if (without_response == NULL)
		without_response = &wo;

	*with_response    = 0;
	*without_response = 0;

	handle = M_io_ble_get_io_handle(io);
	if (handle == NULL)
		return;

	if (handle->have_max_write) {
		*with_response    = handle->max_write_w_response;
		*without_response = handle->max_write_wo_response;
	} else {
		M_io_ble_get_device_max_write_sizes(handle->uuid, with_response, without_response);
		handle->have_max_write        = M_TRUE;
		handle->max_write_w_response  = *with_response;
		handle->max_write_wo_response = *without_response;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_error_t M_io_ble_create(M_io_t **io_out, const char *identifier, M_uint64 timeout_ms)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || M_str_isempty(identifier))
		return M_IO_ERROR_INVALID;

	handle = M_io_ble_open(identifier, &err, timeout_ms);
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

M_io_error_t M_io_ble_create_with_service(M_io_t **io_out, const char *service_uuid, M_uint64 timeout_ms)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || M_str_isempty(service_uuid))
		return M_IO_ERROR_INVALID;

	handle = M_io_ble_open_with_service(service_uuid, &err, timeout_ms);
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_io_ble_meta_get_service(M_io_t *io, M_io_meta_t *meta)
{
	M_hash_multi_t *d;
	const char     *const_temp = NULL;

	if (io == NULL || meta == NULL)
		return NULL;

	d = M_io_ble_get_meta_data(io, meta);
	if (!M_hash_multi_u64_get_str(d, M_IO_BLE_META_KEY_SERVICE_UUID, &const_temp))
		return NULL;
	return const_temp;
}

const char *M_io_ble_meta_get_characteristic(M_io_t *io, M_io_meta_t *meta)
{
	M_hash_multi_t *d;
	const char     *const_temp = NULL;

	if (io == NULL || meta == NULL)
		return NULL;

	d = M_io_ble_get_meta_data(io, meta);
	if (!M_hash_multi_u64_get_str(d, M_IO_BLE_META_KEY_CHARACTERISTIC_UUID, &const_temp))
		return NULL;
	return const_temp;
}

M_io_ble_wtype_t M_io_ble_meta_get_write_type(M_io_t *io, M_io_meta_t *meta)
{
	M_hash_multi_t *d;
	M_int64         i64v;

	if (io == NULL || meta == NULL)
		return M_IO_BLE_WTYPE_WRITE;

	d = M_io_ble_get_meta_data(io, meta);
	if (!M_hash_multi_u64_get_int(d, M_IO_BLE_META_KEY_WRITE_TYPE, &i64v))
		return M_IO_BLE_WTYPE_WRITE;
	return (M_io_ble_wtype_t)i64v;
}

M_io_ble_rtype_t M_io_ble_meta_get_read_type(M_io_t *io, M_io_meta_t *meta)
{
	M_hash_multi_t *d;
	M_int64         i64v;

	if (io == NULL || meta == NULL)
		return M_IO_BLE_RTYPE_READ;

	d = M_io_ble_get_meta_data(io, meta);
	if (!M_hash_multi_u64_get_int(d, M_IO_BLE_META_KEY_READ_TYPE, &i64v))
		return M_IO_BLE_RTYPE_READ;
	return (M_io_ble_rtype_t)i64v;
}

M_bool M_io_ble_meta_get_rssi(M_io_t *io, M_io_meta_t *meta, M_int64 *rssi)
{
	M_hash_multi_t *d;
	M_int64         i64v;

	if (io == NULL || meta == NULL)
		return M_FALSE;

	d = M_io_ble_get_meta_data(io, meta);
	if (!M_hash_multi_u64_get_int(d, M_IO_BLE_META_KEY_READ_TYPE, &i64v))
		return M_FALSE;

	if ((M_io_ble_rtype_t)i64v != M_IO_BLE_RTYPE_RSSI)
		return M_FALSE;

	if (!M_hash_multi_u64_get_int(d, M_IO_BLE_META_KEY_RSSI, &i64v))
		return M_FALSE;

	if (rssi != NULL)
		*rssi = i64v;

	return M_TRUE;
}

void M_io_ble_meta_set_service(M_io_t *io, M_io_meta_t *meta, const char *service_uuid)
{
	M_hash_multi_t *d;

	if (io == NULL || meta == NULL)
		return;

	d = M_io_ble_get_meta_data(io, meta);
	M_hash_multi_u64_insert_str(d, M_IO_BLE_META_KEY_SERVICE_UUID, service_uuid);
}

void M_io_ble_meta_set_characteristic(M_io_t *io, M_io_meta_t *meta, const char *characteristic_uuid)
{
	M_hash_multi_t *d;

	if (io == NULL || meta == NULL)
		return;

	d = M_io_ble_get_meta_data(io, meta);
	M_hash_multi_u64_insert_str(d, M_IO_BLE_META_KEY_CHARACTERISTIC_UUID, characteristic_uuid);
}

void M_io_ble_meta_set_notify(M_io_t *io, M_io_meta_t *meta, M_bool enable)
{
	M_hash_multi_t *d;

	if (io == NULL || meta == NULL)
		return;

	d = M_io_ble_get_meta_data(io, meta);
	M_hash_multi_u64_insert_bool(d, M_IO_BLE_META_KEY_NOTIFY, enable);
}

void M_io_ble_meta_set_write_type(M_io_t *io, M_io_meta_t *meta, M_io_ble_wtype_t type)
{
	M_hash_multi_t *d;

	if (io == NULL || meta == NULL)
		return;

	d = M_io_ble_get_meta_data(io, meta);
	M_hash_multi_u64_insert_int(d, M_IO_BLE_META_KEY_WRITE_TYPE, type);
}
