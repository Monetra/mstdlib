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
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_bluetooth.h>
#include "m_io_bluetooth_int.h"


M_io_bluetooth_enum_t *M_io_bluetooth_enum(void)
{
	return NULL;
}

M_io_handle_t *M_io_bluetooth_open(const char *mac, const char *uuid, M_io_error_t *ioerr)
{
	(void)mac;
	(void)uuid;
	*ioerr = M_IO_ERROR_NOTIMPL;
	return NULL;
}


M_bool M_io_bluetooth_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	(void)layer;
	(void)error;
	(void)err_len;
	return M_FALSE;
}

M_io_state_t M_io_bluetooth_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_ERROR;
}

void M_io_bluetooth_destroy_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_bluetooth_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	return M_FALSE;
}

M_io_error_t M_io_bluetooth_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)write_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_bluetooth_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)read_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_bool M_io_bluetooth_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}

void M_io_bluetooth_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_bluetooth_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_FALSE;
}

