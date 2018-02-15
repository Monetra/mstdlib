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

M_io_ble_enum_t *M_io_ble_enum(void)
{
	return NULL;
}

M_io_error_t M_io_ble_set_notify(M_io_t *io, const char *service_uuid, const char *characteristic_uuid)
{
	(void)io;
	(void)service_uuid;
	(void)characteristic_uuid;
	return M_IO_ERROR_NOTIMPL;
}

M_list_str_t *M_io_ble_get_device_services(const char *uuid)
{
	(void)uuid;
	return NULL;
}

M_list_str_t *M_io_ble_get_device_service_characteristics(const char *uuid, const char *service_uuid)
{
	(void)uuid;
	(void)service_uuid;
	return NULL;
}

void M_io_ble_get_device_max_write_sizes(const char *uuid, size_t *with_response, size_t *without_response)
{
	(void)uuid;
	if (with_response != NULL) {
		*with_response = 0;
	}
	if (without_response != NULL) {
		*without_response = 0;
	}
}

const char *M_io_ble_meta_get_service(M_io_t *io, M_io_meta_t *meta)
{
	(void)io;
	(void)meta;
	return NULL;
}

const char *M_io_ble_meta_get_charateristic(M_io_t *io, M_io_meta_t *meta)
{
	(void)io;
	(void)meta;
	return NULL;
}

M_io_ble_write_property_t M_io_ble_meta_get_write_prop(M_io_t *io, M_io_meta_t *meta)
{
	(void)io;
	(void)meta;
	return M_IO_BLE_WRITE_PROP_WRITE;
}

M_io_ble_rtype_t M_io_ble_meta_get_read_type(M_io_t *io, M_io_meta_t *meta)
{
	(void)io;
	(void)meta;
	return M_IO_BLE_RTYPE_READ;
}

M_bool M_io_ble_meta_get_rssi(M_io_t *io, M_io_meta_t *meta, M_int64 *rssi)
{
	(void)io;
	(void)meta;
	if (rssi != NULL)
		*rssi = 0;
	return M_FALSE;
}

void M_io_ble_meta_set_service(M_io_t *io, M_io_meta_t *meta, const char *service_uuid)
{
	(void)io;
	(void)meta;
	(void)service_uuid;
}

void M_io_ble_meta_set_charateristic(M_io_t *io, M_io_meta_t *meta, const char *characteristic_uuid)
{
	(void)io;
	(void)meta;
	(void)characteristic_uuid;
}

void M_io_ble_meta_set_write_prop(M_io_t *io, M_io_meta_t *meta, M_io_ble_write_property_t prop)
{
	(void)io;
	(void)meta;
	(void)prop;
}

M_io_handle_t *M_io_ble_open(const char *mac, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	(void)mac;
	(void)timeout_ms;
	*ioerr = M_IO_ERROR_NOTIMPL;
	return NULL;
}


M_bool M_io_ble_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	(void)layer;
	(void)error;
	(void)err_len;
	return M_FALSE;
}

M_io_state_t M_io_ble_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_ERROR;
}

void M_io_ble_destroy_cb(M_io_layer_t *layer)
{
	(void)layer;
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

M_bool M_io_ble_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}

void M_io_ble_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_ble_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_FALSE;
}
