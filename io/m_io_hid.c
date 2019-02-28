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
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_hid.h>
#include "m_io_hid_int.h"

static M_bool read_hid_key(const M_uint8 *data, size_t data_len, size_t *key_len, size_t *payload_len)
{
	if (data == NULL || data_len == 0 || key_len == NULL || payload_len == NULL) {
		return M_FALSE;
	}

	*key_len     = 0;
	*payload_len = 0;

	/* Long Item. Next byte contains length of data for this key. */
	if ((data[0] & 0xF0) == 0xF0) {
		*key_len = 3;

		/* Malformed */
		if (data_len < 2) {
			return M_FALSE;
		}
		*payload_len = data[1];
	} else {
		*key_len = 1;

		/* Short Item. The bottom two bits of the key contain the
		 * size code for the data section for this key. */
		if ((data[0] & 0x3) < 3) {
			*payload_len = data[0] & 0x3;
		} else {
			*payload_len = 4;
		}
	}

	/* Make sure this key hasn't been truncated. */
	if (*key_len + *payload_len > data_len) {
		return M_FALSE;
	}

	return M_TRUE;
}

/* Read a HID field value (little-endian). */
static M_uint64 read_hid_field_le(const M_uint8 *data, size_t data_len)
{
	M_uint64 ret;
	size_t   i;

	if (data == NULL || data_len == 0) {
		return 0;
	}

	if (data_len == 1) {
		return *data;
	}

	ret = 0;
	for (i=0; i<data_len; i++) {
		size_t val = data[i];
		val <<= (8*i);
		ret |= val;
	}

	return ret;
}

static void M_io_hid_enum_free_device(void *arg)
{
	M_io_hid_enum_device_t *device = arg;
	M_free(device->path);
	M_free(device->manufacturer);
	M_free(device->product);
	M_free(device->serial);
	M_free(device);
}

void M_io_hid_enum_destroy(M_io_hid_enum_t *hidenum)
{
	if (hidenum == NULL)
		return;
	M_list_destroy(hidenum->devices, M_TRUE);
	M_free(hidenum);
}

M_io_hid_enum_t *M_io_hid_enum_init(void)
{
	M_io_hid_enum_t *hidenum        = M_malloc_zero(sizeof(*hidenum));
	struct M_list_callbacks listcbs = {
		NULL,
		NULL,
		NULL,
		M_io_hid_enum_free_device
	};
	hidenum->devices = M_list_create(&listcbs, M_LIST_NONE);
	return hidenum;
}

size_t M_io_hid_enum_count(const M_io_hid_enum_t *hidenum)
{
	if (hidenum == NULL)
		return 0;
	return M_list_len(hidenum->devices);
}


void M_io_hid_enum_add(M_io_hid_enum_t *hidenum,
                       /* Info about this enumerated device */
                       const char *d_path, const char *d_manufacturer, const char *d_product,
                       const char *d_serialnum, M_uint16 d_vendor_id, M_uint16 d_product_id,
                       /* Search/Match criteria */
                       M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids,
                       const char *s_serialnum)
{
	M_io_hid_enum_device_t *device;

	if (hidenum == NULL || M_str_isempty(d_path) || d_vendor_id == 0)
		return;

	/* Filter by vendor id */
	if (s_vendor_id != 0 && s_vendor_id != d_vendor_id)
		return;

	/* Filter by product id */
	if (s_product_ids != NULL && s_num_product_ids > 0) {
		size_t i;
		for (i=0; i<s_num_product_ids; i++) {
			if (s_product_ids[i] == d_product_id)
				break;
		}

		/* Not found */
		if (i == s_num_product_ids)
			return;
	}

	/* Filter by serial number */
	if (s_serialnum != NULL && !M_str_caseeq(s_serialnum, d_serialnum)) {
		return;
	}

	device               = M_malloc_zero(sizeof(*device));
	device->path         = M_strdup(d_path);

	device->manufacturer = M_strdup(d_manufacturer);
	device->product      = M_strdup(d_product);
	device->serial       = M_strdup(d_serialnum);

	device->vendor_id    = d_vendor_id;
	device->product_id   = d_product_id;
	M_list_insert(hidenum->devices, device);
}


const char *M_io_hid_enum_path(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return NULL;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->path;
}

const char *M_io_hid_enum_manufacturer(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return NULL;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->manufacturer;
}

const char *M_io_hid_enum_product(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return NULL;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->product;
}

M_uint16 M_io_hid_enum_vendorid(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return 0;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->vendor_id;
}

M_uint16 M_io_hid_enum_productid(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return 0;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return 0;
	return device->product_id;
}

const char *M_io_hid_enum_serial(const M_io_hid_enum_t *hidenum, size_t idx)
{
	const M_io_hid_enum_device_t *device;
	if (hidenum == NULL)
		return NULL;
	device = M_list_at(hidenum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->serial;
}


M_io_error_t M_io_hid_create_one(M_io_t **io_out, M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial /* May be NULL */)
{
	M_io_hid_enum_t  *hidenum;
	char              path[1024];
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || vendorid == 0)
		return M_IO_ERROR_INVALID;

	/* Enumerate devices and use first match */
	hidenum = M_io_hid_enum(vendorid, productids, num_productids, serial);
	if (hidenum == NULL)
		return M_IO_ERROR_NOTFOUND;

	if (M_io_hid_enum_count(hidenum) == 0) {
		M_io_hid_enum_destroy(hidenum);
		return M_IO_ERROR_NOTFOUND;
	}

	M_str_cpy(path, sizeof(path), M_io_hid_enum_path(hidenum, 0));
	M_io_hid_enum_destroy(hidenum);

	if (M_str_isempty(path))
		return M_IO_ERROR_NOTFOUND;

	handle = M_io_hid_open(path, &err);
	if (handle == NULL)
		return err;

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_hid_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_hid_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_hid_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_hid_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_hid_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_hid_destroy_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_hid_disconnect_cb);
	M_io_callbacks_reg_state(callbacks, M_io_hid_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_hid_errormsg_cb);
	M_io_layer_add(*io_out, M_IO_USB_HID_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);
	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_hid_create(M_io_t **io_out, M_uint16 vendorid, M_uint16 productid, const char *serial /* May be NULL */)
{
	M_uint16 prodarr[] = { productid };
	return M_io_hid_create_one(io_out, vendorid, prodarr, (productid > 0)?1:0, serial);
}


M_io_layer_t *M_io_hid_get_top_hid_layer(M_io_t *io)
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
		layer = M_io_layer_acquire(io, layer_idx, M_IO_USB_HID_NAME);

		if (layer != NULL) {
			break;
		}
	}

	return layer;
}

M_bool hid_uses_report_descriptors(const unsigned char *desc, size_t len)
{
	size_t i = 0;

	while (i < len) {
		size_t data_len;
		size_t key_size;

		/* Check for the Report ID key */
		if (desc[i] == 0x85) {
			/* There's a report id, therefore it uses reports */
			return M_TRUE;
		}

		if (!read_hid_key(desc + i, len - i, &key_size, &data_len)) {
			return 0;
		}

		/* Move past this key and its data */
		i += data_len + key_size;
	}

	/* Didn't find a Report ID key, must not use report descriptor numbers */
	return M_FALSE;
}

/* Return maximum report sizes: (1) max over all input reports, and (2) max over all output reports. */
M_bool hid_get_max_report_sizes(const M_uint8 *desc, size_t desc_len, size_t *max_input, size_t *max_output)
{
	size_t  i                   = 0;
	M_uint8 rid                 = 0;
	size_t  report_size         = 0;
	size_t  report_count        = 0;
	size_t  global_report_count = 0;
	size_t  global_report_size  = 0;

	if (max_input == NULL || max_output == NULL || desc == NULL) {
		return M_FALSE;
	}
	*max_input  = 0;
	*max_output = 0;

	while (i < desc_len) {
		size_t data_len;
		size_t key_len;
		size_t key_no_size;

		if (!read_hid_key(desc + i, desc_len - i, &key_len, &data_len)) {
			return M_FALSE;
		}

		/* Mask off the size fields in the key. */
		key_no_size = desc[i] & 0xFC;

		/*M_printf("desc[%d] = 0x%02X\n", (int)i, (unsigned)desc[i]); */

		if (desc[i] == 0x85) {
			/* Report ID (always has size 1) */
			rid          = desc[i + key_len];
			report_size  = global_report_size;
			report_count = global_report_count;
		} else if (key_no_size == 0x74) {
			/* Report Size */
			report_size = (size_t)read_hid_field_le(desc + i + key_len, data_len);
			if (rid == 0) {
				global_report_size = report_size;
			}
		} else if (key_no_size == 0x94) {
			/* Report Count */
			report_count = (size_t)read_hid_field_le(desc + i + key_len, data_len);
			if (rid == 0) {
				global_report_count = report_count;
			}
		} else if (key_no_size == 0x80 || key_no_size == 0x90) {
			/* (Input) or (Output) Usage Marker - should be at end of report. */
			size_t  report_bytes;
			size_t *dest = (key_no_size == 0x80)? max_input : max_output;
			report_bytes = ((report_size * report_count) + 7) / 8;
			if (*dest < report_bytes) {
				*dest = report_bytes;
			}
		}

		/* Move past this key and its data. */
		i += data_len + key_len;
	}

	return M_TRUE;
}
