/* The MIT License (MIT)
 *
 * Copyright (c) 2020 Monetra Technologies, LLC.
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
#include <mstdlib/io/m_io_usb.h>
#include "m_io_usb_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_io_handle {
	char      *path;
	char       manufacturer[256];
	char       product[256];
	char       serial[256];
	M_uint16   vendorid;
	M_uint16   productid;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_usb_enum_free_device(void *arg)
{
	M_io_usb_enum_device_t *device = arg;
	M_free(device->path);
	M_free(device->manufacturer);
	M_free(device->product);
	M_free(device->serial);
	M_free(device);
}

void M_io_usb_enum_destroy(M_io_usb_enum_t *usbenum)
{
	if (usbenum == NULL)
		return;
	M_list_destroy(usbenum->devices, M_TRUE);
	M_free(usbenum);
}

M_io_usb_enum_t *M_io_usb_enum_init(void)
{
	M_io_usb_enum_t *usbenum        = M_malloc_zero(sizeof(*usbenum));
	struct M_list_callbacks listcbs = {
		NULL,
		NULL,
		NULL,
		M_io_usb_enum_free_device
	};
	usbenum->devices = M_list_create(&listcbs, M_LIST_NONE);
	return usbenum;
}

size_t M_io_usb_enum_count(const M_io_usb_enum_t *usbenum)
{
	if (usbenum == NULL)
		return 0;
	return M_list_len(usbenum->devices);
}

void M_io_usb_enum_add(M_io_usb_enum_t *usbenum,
                       /* Info about this enumerated device */
                       const char *path,
                       M_uint16 d_vendor_id, M_uint16 d_product_id,
					   const char *d_manufacturer, const char *d_product, const char *d_serial,
					    M_io_usb_speed_t d_speed, size_t d_curr_config,
                       /* Search/Match criteria */
                       M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serial)
{
	M_io_usb_enum_device_t *device;

	if (usbenum == NULL || M_str_isempty(path) || d_vendor_id == 0)
		return;

	/* Filter by vendor id */
	if (s_vendor_id != 0 && s_vendor_id != d_vendor_id)
		return;

	/* Filter by product id */
	if (s_product_ids != NULL && s_num_product_ids > 0) {
		size_t i;
		for (i=0; i<s_num_product_ids; i++) {
			if (s_product_ids[i] == d_product_id) {
				break;
			}
		}

		/* Not found */
		if (i == s_num_product_ids) {
			return;
		}
	}

	/* Filter by serial number */
	if (s_serial != NULL && !M_str_caseeq(s_serial, d_serial))
		return;

	device                = M_malloc_zero(sizeof(*device));
	device->path          = M_strdup(path);
	device->vendor_id     = d_vendor_id;
	device->product_id    = d_product_id;
	device->manufacturer  = M_strdup(d_manufacturer);
	device->product       = M_strdup(d_product);
	device->serial        = M_strdup(d_serial);
	device->speed         = d_speed;
	device->curr_config   = d_curr_config;

	M_list_insert(usbenum->devices, device);
}

const char *M_io_usb_enum_path(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return NULL;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->path;
}

M_uint16 M_io_usb_enum_vendorid(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return 0;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->vendor_id;
}

M_uint16 M_io_usb_enum_productid(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return 0;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->product_id;
}

const char *M_io_usb_enum_manufacturer(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return NULL;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->manufacturer;
}

const char *M_io_usb_enum_product(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return NULL;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->product;
}

const char *M_io_usb_enum_serial(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return NULL;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->serial;
}

M_io_usb_speed_t M_io_usb_enum_speed(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return 0;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->speed;
}

size_t M_io_usb_enum_current_configuration(const M_io_usb_enum_t *usbenum, size_t idx)
{
	const M_io_usb_enum_device_t *device;
	if (usbenum == NULL)
		return 0;
	device = M_list_at(usbenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->curr_config;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_error_t M_io_usb_create_one(M_io_t **io_out, M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_usb_enum_t  *usbenum;
	char              path[1024] = { 0 };
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || vendorid == 0)
		return M_IO_ERROR_INVALID;

	/* Enumerate devices and use first match */
	usbenum = M_io_usb_enum(vendorid, productids, num_productids, serial);
	if (usbenum == NULL)
		return M_IO_ERROR_NOTFOUND;

	if (M_io_usb_enum_count(usbenum) == 0) {
		M_io_usb_enum_destroy(usbenum);
		return M_IO_ERROR_NOTFOUND;
	}

	M_str_cpy(path, sizeof(path), M_io_usb_enum_path(usbenum, 0));
	M_io_usb_enum_destroy(usbenum);

	if (M_str_isempty(path))
		return M_IO_ERROR_NOTFOUND;

	handle = M_io_usb_open(path, &err);
	if (handle == NULL)
		return err;

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_usb_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_usb_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_usb_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_usb_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_usb_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_usb_destroy_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_usb_disconnect_cb);
	M_io_callbacks_reg_state(callbacks, M_io_usb_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_usb_errormsg_cb);
	M_io_layer_add(*io_out, M_IO_USB_USB_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);
	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_usb_create(M_io_t **io_out, M_uint16 vendorid, M_uint16 productid, const char *serial)
{
	M_uint16 prodarr[] = { productid };
	return M_io_usb_create_one(io_out, vendorid, prodarr, (productid > 0)?1:0, serial);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_io_usb_meta_get_interface(M_io_t *io_usb_device, M_io_meta_t *meta)
{
	(void)io_usb_device;
	(void)meta;
	return 0;
}

size_t M_io_usb_meta_get_endpoint(M_io_t *io_usb_device, M_io_meta_t *meta)
{
	(void)io_usb_device;
	(void)meta;
	return 0;
}

M_io_usb_ep_type_t M_io_usb_meta_get_endpoint_type(M_io_t *io_usb_device, M_io_meta_t *meta)
{
	(void)io_usb_device;
	(void)meta;
	return M_IO_USB_EP_TYPE_UNKNOWN;
}

void M_io_usb_meta_set_interface(M_io_t *io_usb_device, M_io_meta_t *meta, size_t iface)
{
	(void)io_usb_device;
	(void)meta;
	(void)iface;
}

void M_io_usb_meta_set_endpoint(M_io_t *io_usb_device, M_io_meta_t *meta, size_t ep)
{
	(void)io_usb_device;
	(void)meta;
	(void)ep;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_layer_t *M_io_usb_get_top_usb_layer(M_io_t *io)
{
	M_io_layer_t  *layer;
	size_t         layer_idx;
	size_t         layer_count;

	if (io == NULL) {
		return NULL;
	}

	layer       = NULL;
	layer_count = M_io_layer_count(io);
	for (layer_idx=layer_count; layer_idx-->0; ) {
		layer = M_io_layer_acquire(io, layer_idx, M_IO_USB_USB_NAME);

		if (layer != NULL) {
			break;
		}
	}

	return layer;
}
