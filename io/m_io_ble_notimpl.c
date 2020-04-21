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

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"

M_io_ble_enum_t *M_io_ble_enum(void)
{
	return NULL;
}

M_io_error_t M_io_ble_set_device_notify(const char *uuid, const char *service_uuid, const char *characteristic_uuid, M_bool enable)
{
	(void)uuid;
	(void)service_uuid;
	(void)characteristic_uuid;
	(void)enable;
	return M_IO_ERROR_NOTIMPL;
}

char *M_io_ble_get_device_identifier(const char *uuid)
{
	(void)uuid;
	return NULL;
}

char *M_io_ble_get_device_name(const char *uuid)
{
	(void)uuid;
	return NULL;
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

M_io_ble_property_t M_io_ble_get_device_service_characteristic_properties(const char *uuid, const char *service_uuid, const char *characteristic_uuid)
{
	(void)uuid;
	(void)service_uuid;
	(void)characteristic_uuid;
	return M_IO_BLE_PROPERTY_NONE;
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

M_io_handle_t *M_io_ble_open(const char *mac, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	(void)mac;
	(void)timeout_ms;
	*ioerr = M_IO_ERROR_NOTIMPL;
	return NULL;
}

M_io_handle_t *M_io_ble_open_with_service(const char *service_uuid, M_io_error_t *ioerr, M_uint64 timeout_ms)
{
	(void)service_uuid;
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

M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms)
{
	(void)event; (void)callback; (void)cb_data; (void)timeout_ms;
	return M_FALSE;
}
