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
#include <mstdlib/base/m_fs.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "m_io_hid_int.h"
#include "m_io_int.h"
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "m_io_posix_common.h"

/* May need to expand to include /sys/subsystem once it's implemented */
static const char * const hid_paths[] = {
#if defined(__linux__)
	"/sys/class/hidraw/hidraw*",
#endif
	NULL
};


static char *hid_get_property(const char *basepath, const char *property)
{
	unsigned char *result;
	char           path[1024];
	size_t         len;
	M_fs_error_t   error;

	/* Make sure we have a basepath */
	if (M_str_isempty(basepath) || M_str_isempty(property))
		return NULL;

	/* Properties are split up in their own files. Simply read the appropraite file */
	M_snprintf(path, sizeof(path), "%s/%s", basepath, property);

	/* If file doesn't exist, bail */
	if (M_fs_perms_can_access(path, 0) != M_FS_ERROR_SUCCESS)
		return NULL;

	error = M_fs_file_read_bytes(path, 0, &result, &len);
	if (error != M_FS_ERROR_SUCCESS)
		return NULL;

	/* Trim read data */
	M_str_delete_newlines((char *)result);
	return (char *)result;
}

static char *hid_get_manufacturer(const char *basepath)
{
	const char *filename = "manufacturer";

	return hid_get_property(basepath, filename);
}

static char *hid_get_product(const char *basepath)
{
	const char *filename = "product";

	return hid_get_property(basepath, filename);
}

static char *hid_get_serial(const char *basepath)
{
	const char *filename = "serial";

	return hid_get_property(basepath, filename);
}

static M_uint16 hid_get_vendorid(const char *basepath)
{
	const char *filename = "idVendor";
	char       *id_str;
	M_uint32    id;

	id_str = hid_get_property(basepath, filename);
	if (M_str_isempty(id_str))
		return 0;

	if (M_str_to_uint32_ex(id_str, M_str_len(id_str), 16, &id, NULL) != M_STR_INT_SUCCESS)
		return 0;
	M_free(id_str);
	return (M_uint16)id;
}

static M_uint16 hid_get_productid(const char *basepath)
{
	const char *filename = "idProduct";
	char       *id_str;
	M_uint32    id;

	id_str = hid_get_property(basepath, filename);
	if (M_str_isempty(id_str))
		return 0;

	if (M_str_to_uint32_ex(id_str, M_str_len(id_str), 16, &id, NULL) != M_STR_INT_SUCCESS)
		return 0;

	M_free(id_str);
	return (M_uint16)id;
}

static void hid_enum_device(M_io_hid_enum_t *hidenum, const char *classpath, const char *devpath,
		M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serialnum)
{
	char     *manufacturer;
	char     *product;
	char     *serial;
	M_uint16  vendorid;
	M_uint16  productid;

	/* If vendorid is 0, then this device is not present */
	vendorid     = hid_get_vendorid(classpath);
	if (vendorid == 0)
		return;

	productid    = hid_get_productid(classpath);
	serial       = hid_get_serial(classpath);
	manufacturer = hid_get_manufacturer(classpath);
	product      = hid_get_product(classpath);

	M_io_hid_enum_add(hidenum, devpath, manufacturer, product, serial, vendorid, productid, 
	                  s_vendor_id, s_product_ids, s_num_product_ids, s_serialnum);

	M_free(manufacturer);
	M_free(product);
	M_free(serial);
}

M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_hid_enum_t *hidenum = M_io_hid_enum_init();
	size_t           i;

	for (i=0; hid_paths[i] != NULL; i++) {
		char *udirname  = M_fs_path_dirname(hid_paths[i], M_FS_SYSTEM_AUTO);
		char *ubasename = M_fs_path_basename(hid_paths[i], M_FS_SYSTEM_AUTO);
		if (!M_str_isempty(udirname) && !M_str_isempty(ubasename)) {
			size_t        j;
			M_list_str_t *matches = M_fs_dir_walk_strs(udirname, ubasename, M_FS_DIR_WALK_FILTER_SYMLINK);
			if (matches) {
				M_list_str_change_sorting(matches, M_LIST_STR_SORTASC);
				for (j=0; j<M_list_str_len(matches); j++) {
					const char *devname = M_list_str_at(matches, j);
					if (devname != NULL) {
						char classpath[1024];
						char devpath[256];
						M_snprintf(devpath, sizeof(devpath), "/dev/%s", devname);
						M_snprintf(classpath, sizeof(classpath), "%s/%s/device/../..", udirname, devname);
						hid_enum_device(hidenum, classpath, devpath, vendorid, productids, num_productids, serial);
					}
				}
				M_list_str_destroy(matches);
			}
		}
		M_free(udirname);
		M_free(ubasename);
	}
	return hidenum;
}


static char *get_classpath(const char *devpath)
{
	char *udirname  = M_fs_path_dirname(hid_paths[0], M_FS_SYSTEM_AUTO);
	char *devname   = M_fs_path_basename(devpath, M_FS_SYSTEM_AUTO);
	char *classpath;

	M_asprintf(&classpath, "%s/%s/device/../..", udirname, devname);
	M_free(udirname);
	M_free(devname);

	return classpath;
}


struct M_io_handle {
	M_EVENT_HANDLE  handle;
	int             last_error_sys;
	M_bool          uses_report_descriptors;
	unsigned char   descriptor[4096]; /* 4096 is the max descriptor size */
	size_t          descriptor_size;

	char           *path;
	char           *manufacturer;
	char           *product;
	char           *serial;
	M_uint16        productid;
	M_uint16        vendorid;
	size_t          max_input_report_size;
	size_t          max_output_report_size;
};


static void hid_linux_close_handle(M_io_handle_t *handle)
{
	if (handle->handle != -1) {
		close(handle->handle);
	}
	handle->handle = -1;
}


static void hid_linux_close(M_io_layer_t *layer)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (event && handle->handle != -1)
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);

	hid_linux_close_handle(handle);
}


M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (handle->handle == -1)
		return M_FALSE;

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

	/* Register fd to event subsystem */
	M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, M_EVENT_CAPS_WRITE|M_EVENT_CAPS_READ);
	return M_TRUE;
}


void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->handle, M_EVENT_INVALID_SOCKET, 0, 0);
}


M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return M_io_posix_process_cb(layer, handle->handle, handle->handle, type);
}


M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_error_t   err;
	size_t         offset;
	size_t         len    = *read_len;

	if (layer == NULL || handle == NULL)
		return M_IO_ERROR_INVALID;

	/* If we don't use report descriptors, we must prefix the read buffer with a zero */
	offset = handle->uses_report_descriptors?0:1;
	len   -= offset;

	err = M_io_posix_read(io, handle->handle, buf+offset, &len, &handle->last_error_sys);
	if (M_io_error_is_critical(err))
		hid_linux_close(layer);

	if (err == M_IO_ERROR_SUCCESS) {
		if (offset) {
			len++;
			buf[0] = 0;
		}
		*read_len = len;
	}

/*M_printf("%s(): err = %s\n", __FUNCTION__, M_io_error_string(err));*/
	return err;
}


M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_error_t   err;
	size_t         offset;
	size_t         len    = *write_len;

	if (layer == NULL || handle == NULL)
		return M_IO_ERROR_INVALID;

	/* If we don't use report descriptors, we must skip over the 0 prefix */
	offset = handle->uses_report_descriptors?0:1;
	len   -= offset;

	err = M_io_posix_write(io, handle->handle, buf + offset, &len, &handle->last_error_sys);
	if (M_io_error_is_critical(err))
		hid_linux_close(layer);

	if (err == M_IO_ERROR_SUCCESS) {
		if (offset)
			len++;
		*write_len = len;
	}
		
/*if (err != M_IO_ERROR_SUCCESS) {
	M_printf("%s(): err = %s, %s\n", __FUNCTION__, M_io_error_string(err), strerror(handle->last_error_sys));
}*/
	return err;
}

static void hid_linux_destroy_handle(M_io_handle_t *handle)
{
	if (handle == NULL)
		return;

	hid_linux_close_handle(handle);

	M_free(handle->path);
	M_free(handle->manufacturer);
	M_free(handle->product);
	M_free(handle->serial);

	M_free(handle);
}

void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	hid_linux_destroy_handle(handle);
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* Nothing to do with linux */
	return M_TRUE;
}


M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->handle == -1)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}


M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	return M_io_posix_errormsg(handle->last_error_sys, error, err_len);
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


/* Return maximum report sizes: (1) max over all input reports, and (2) max over all output reports. */
static M_bool hid_get_max_report_sizes(const M_uint8 *desc, size_t desc_len, size_t *max_input, size_t *max_output)
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


#if 0 /* TODO: for future use, when we figure out how to do it on all platforms. */
/* Return the size (in bytes) of a particular report, or 0 if there's an error (unknown report_id, etc.).
 * Passing 0 will return the global report size, if any.
 */
static size_t hid_get_report_size(const M_uint8 *desc, size_t desc_len, M_uint8 report_id)
{
	size_t  i                   = 0;
	M_uint8 rid                 = 0; /* Current report ID: global (rid == 0) to start with. */
	size_t  report_size         = 0; /* size of each report field, in bits. */
	size_t  report_count        = 0; /* number of fields in the report. */
	size_t  global_report_count = 0;
	size_t  global_report_size  = 0;

	while (i < desc_len) {
		size_t  data_len;
		size_t  key_len;
		M_uint8 key_no_size;

		if (!read_hid_key(desc + i, desc_len - i, &key_len, &data_len)) {
			return 0;
		}

		/* Mask off the size fields in the key. */
		key_no_size = desc[i] & 0xFC;

		if (desc[i] == 0x85) {
			/* Report ID (always has size 1) */
			rid = desc[i + key_len];
		} else if(key_no_size == 0x74) {
			/* Report Size */
			report_size = (size_t)read_hid_field_le(desc + key_len, data_len);
			if (rid == 0) {
				global_report_size = report_size;
			}
		} else if(key_no_size == 0x94 && (rid == 0 || rid == report_id)) {
			/* Report Count */
			/* Only update report_count if we're on the global RID, or the specific RID we're looking for. */
			report_count = (size_t)read_hid_field_le(desc + key_len, data_len);
			if (rid == report_id && report_size > 0) {
				return ((report_size * report_count) + 7) / 8;
			}
			if (rid == 0) {
				global_report_count = report_count;
			}
		}

		/* Move past this key and its data. */
		i += data_len + key_len;
	}

	return ((global_report_size * global_report_count) + 7) / 8;
}
#endif


static M_bool hid_uses_report_descriptors(const unsigned char *desc, size_t len)
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


M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	int                             fd;
	struct hidraw_report_descriptor rpt_desc;
	M_io_handle_t                  *handle;
	int                             flags     = O_RDWR;
	char                           *classpath = NULL;

#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif

	fd = open(devpath, flags);
	if (fd < 0) {
		int err = errno;
/*M_printf("%s(): open failed %d - %s\n", __FUNCTION__, err, strerror(err));*/
		*ioerr = M_io_posix_err_to_ioerr(err);
		return NULL;
	}

#ifndef O_CLOEXEC
	M_io_posix_fd_set_closeonexec(fd);
#endif

	if (!M_io_setnonblock(fd)) {
/*M_printf("%s(): set nonblock failed\n", __FUNCTION__);*/
		close(fd);
		*ioerr = M_IO_ERROR_ERROR;
		return NULL;
	}

	/* Get the report descriptor */
	M_mem_set(&rpt_desc, 0, sizeof(rpt_desc));

	if (ioctl(fd, HIDIOCGRDESCSIZE, &rpt_desc.size) < 0) {
		int err = errno;
/*M_printf("%s(): get reportdescriptor size failed %d - %s\n", __FUNCTION__, err, strerror(err));*/
		close(fd);
		*ioerr = M_io_posix_err_to_ioerr(err);
		return NULL;
	}

	if (ioctl(fd, HIDIOCGRDESC, &rpt_desc) < 0) {
		int err = errno;
/*M_printf("%s(): get reportdescriptor failed %d - %s\n", __FUNCTION__, err, strerror(err));*/
		close(fd);
		*ioerr = M_io_posix_err_to_ioerr(err);
		return NULL;
	}

	handle                          = M_malloc_zero(sizeof(*handle));
	handle->handle                  = fd;
	handle->descriptor_size         = rpt_desc.size;
	M_mem_copy(handle->descriptor, rpt_desc.value, rpt_desc.size);
/*M_printf("%s(): opened path %s, report descriptor size %zu, fd %d\n", __FUNCTION__, path, (size_t)rpt_desc.size, fd);*/
	handle->uses_report_descriptors = hid_uses_report_descriptors(handle->descriptor, handle->descriptor_size);

	hid_get_max_report_sizes(handle->descriptor, handle->descriptor_size,
		&handle->max_input_report_size, &handle->max_output_report_size);

	/* Note: to match Windows, need to include report ID byte in reported size. So, increment both by one. */
	if (handle->max_input_report_size > 0)
		handle->max_input_report_size++;

	if (handle->max_output_report_size > 0)
		handle->max_output_report_size++;

	classpath            = get_classpath(devpath);
	handle->path         = M_strdup(devpath);
	handle->manufacturer = hid_get_manufacturer(classpath);
	handle->product      = hid_get_product(classpath);
	handle->serial       = hid_get_serial(classpath);
	handle->productid    = hid_get_productid(classpath);
	handle->vendorid     = hid_get_vendorid(classpath);
	M_free(classpath);

	/*
	M_printf("\n\n\n");
	M_printf("Path:             %s\n", handle->path);
	M_printf("Manufacturer:     %s\n", handle->manufacturer);
	M_printf("Product:          %s\n", handle->product);
	M_printf("Serial:           %s\n", handle->serial);
	M_printf("ProductID:        0x%04X\n", (unsigned)handle->productid);
	M_printf("VendorID:         0x%04X\n", (unsigned)handle->vendorid);
	M_printf("In Rpt Sz (Max):  %d bytes\n", (int)handle->max_input_report_size);
	M_printf("Out Rpt Sz (Max): %d bytes\n", (int)handle->max_output_report_size);
	M_printf("\n\n\n");
	*/

	*ioerr = M_IO_ERROR_SUCCESS;
	return handle;
}


static M_io_handle_t *get_top_hid_handle(M_io_t *io)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	size_t         layer_idx;
	size_t         layer_count;

	if (io == NULL) {
		return NULL;
	}

	handle      = NULL;
	layer_count = M_io_layer_count(io);
	for (layer_idx=layer_count; layer_idx-->0; ) {
		layer = M_io_layer_acquire(io, layer_idx, M_IO_USB_HID_NAME);

		if (layer != NULL) {
			handle = M_io_layer_get_handle(layer);
			M_io_layer_release(layer);
			break;
		}
	}

	return handle;
}


void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	M_io_handle_t *handle = get_top_hid_handle(io);
	size_t         my_in;
	size_t         my_out;

	if (max_input_size == NULL) {
		max_input_size  = &my_in;
	}
	if (max_output_size == NULL) {
		max_output_size = &my_out;
	}

	if (handle == NULL) {
		*max_input_size  = 0;
		*max_output_size = 0;
	} else {
		*max_input_size  = handle->max_input_report_size;
		*max_output_size = handle->max_output_report_size;
	}
}


const char *M_io_hid_get_path(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return NULL;
	}
	return handle->path;
}


const char *M_io_hid_get_manufacturer(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return NULL;
	}
	return handle->manufacturer;
}


const char *M_io_hid_get_product(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return NULL;
	}
	return handle->product;
}


const char *M_io_hid_get_serial(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return NULL;
	}
	return handle->serial;
}


M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return 0;
	}
	return handle->productid;
}


M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	M_io_handle_t *handle = get_top_hid_handle(io);

	if (handle == NULL) {
		return 0;
	}
	return handle->vendorid;
}
