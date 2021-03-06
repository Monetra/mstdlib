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

#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_hid.h>
#include "m_io_hid_int.h"

M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	(void)vendorid;
	(void)productids;
	(void)num_productids;
	(void)serial;
	return NULL;
}

char *M_io_hid_get_manufacturer(M_io_t *io)
{
	(void)io;
	return NULL;
}

char *M_io_hid_get_path(M_io_t *io)
{
	(void)io;
	return NULL;
}

char *M_io_hid_get_product(M_io_t *io)
{
	(void)io;
	return NULL;
}

M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	(void)io;
	return 0;
}

M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	(void)io;
	return 0;
}

char *M_io_hid_get_serial(M_io_t *io)
{
	(void)io;
	return NULL;
}

void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	(void)io;
	if (max_input_size != NULL)
		*max_input_size = 0;
	if (max_output_size != NULL)
		*max_output_size = 0;
}

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	(void)devpath;
	*ioerr = M_IO_ERROR_NOTIMPL;
	return NULL;
}

M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	(void)layer;
	(void)error;
	(void)err_len;
	return M_FALSE;
}

M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_ERROR;
}

void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	return M_FALSE;
}

M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)write_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)read_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}

void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_FALSE;
}

