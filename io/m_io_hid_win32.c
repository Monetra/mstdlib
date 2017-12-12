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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/io/m_io_hid.h>
#include "m_io_hid_int.h"
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_io_w32overlap.h"
#include "m_io_win32_common.h"
#include <objbase.h>
#include <initguid.h>
#include <Setupapi.h>
#include <setupapi.h>
#include <winioctl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include "base/platform/m_platform_win.h"

struct M_io_handle_w32 {
	M_bool    uses_report_descriptors;
	char     *path;
	char     *manufacturer;
	char     *product;
	char     *serial;
	M_uint16  productid;
	M_uint16  vendorid;
	size_t    max_input_report_size;
	size_t    max_output_report_size;
};

typedef BOOLEAN (__stdcall *hidstring_cb_t)(HANDLE, PVOID, ULONG);

static char *hid_get_string(HANDLE handle, hidstring_cb_t func)
{
	wchar_t wstr[512];
	char   *ret;

	M_mem_set(wstr, 0, sizeof(wstr));

	if (!func(handle, wstr, sizeof(wstr) - sizeof(*wstr) /* NULL Term */))
		return NULL;

	ret = M_win32_wchar_to_char(wstr);
	if (!M_str_isprint(ret)) {
		M_free(ret);
		return NULL;
	}

	return ret;
}


static char *hid_get_serial(HANDLE handle)
{
	return hid_get_string(handle, HidD_GetSerialNumberString);
}


static char *hid_get_product(HANDLE handle)
{
	return hid_get_string(handle, HidD_GetProductString);
}


static char *hid_get_manufacturer(HANDLE handle)
{
	return hid_get_string(handle, HidD_GetManufacturerString);
}



static void hid_enum_device(M_io_hid_enum_t *hidenum, const char *devpath, M_uint16 s_vendor_id,
                            const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serialnum)
{
	char     *manufacturer = NULL;
	char     *product      = NULL;
	char     *serial       = NULL;
	M_uint16  vendorid;
	M_uint16  productid;
	HANDLE    handle       = INVALID_HANDLE_VALUE;
	HIDD_ATTRIBUTES attrib;

	handle = CreateFileA(devpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED /* ?? */, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		goto cleanup;

	attrib.Size = sizeof(attrib);
	if (!HidD_GetAttributes(handle, &attrib))
		goto cleanup;

	if (attrib.VendorID == 0)
		goto cleanup;

	vendorid     = attrib.VendorID;
	productid    = attrib.ProductID;

	manufacturer = hid_get_manufacturer(handle);
	product      = hid_get_product(handle);
	serial       = hid_get_serial(handle);
	M_io_hid_enum_add(hidenum, devpath, manufacturer, product, serial, vendorid, productid,
	                  s_vendor_id, s_product_ids, s_num_product_ids, s_serialnum);

cleanup:
	M_free(manufacturer);
	M_free(product);
	M_free(serial);
	if (handle != INVALID_HANDLE_VALUE)
		CloseHandle(handle);
}


static M_bool hid_enum_has_driver(HDEVINFO hDevInfo, SP_DEVINFO_DATA *devinfo)
{
	char               classname[256];
	char               drivername[256];
	static const char *hidclass = "HIDClass";

	M_mem_set(classname, 0, sizeof(classname));
	M_mem_set(drivername, 0, sizeof(drivername));
	if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, devinfo, SPDRP_CLASS, NULL, (PBYTE)classname, sizeof(classname)-1, NULL))
		return M_FALSE;

	/* Validate the class is "HIDClass", otherwise its an error (we don't want keyboards and mice) */
	if (!M_str_eq(hidclass, classname))
		return M_FALSE;

	if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, devinfo, SPDRP_DRIVER, NULL, (PBYTE)drivername, sizeof(drivername)-1, NULL))
		return M_FALSE;

	return M_TRUE;
}


static M_bool hid_enum_devpath(HDEVINFO hDevInfo, SP_DEVICE_INTERFACE_DATA *devinterface, char *devpath, size_t devpath_size)
{
	DWORD                              size              = 0;
	SP_DEVICE_INTERFACE_DETAIL_DATA_A *devinterface_data = NULL;

	/* Request required buffer size */
	if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, devinterface, NULL, 0, &size, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return M_FALSE;

	devinterface_data         = M_malloc_zero(size);
	devinterface_data->cbSize = sizeof(*devinterface_data);

	/* Request data */
	if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, devinterface, devinterface_data, size, NULL, NULL)) {
		M_free(devinterface_data);
		return M_FALSE;
	}

	if (M_str_isempty(devinterface_data->DevicePath) || M_str_len(devinterface_data->DevicePath) >= devpath_size) {
		M_free(devinterface_data);
		return M_FALSE;
	}

	M_str_cpy(devpath, devpath_size, devinterface_data->DevicePath);
	M_free(devinterface_data);
	return M_TRUE;
}


M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	GUID                     HIDClassGuid;
	HDEVINFO                 hDevInfo     = INVALID_HANDLE_VALUE;
	SP_DEVINFO_DATA          devinfo;
	DWORD                    devidx       = 0;
	M_io_hid_enum_t         *hidenum      = NULL;

	HidD_GetHidGuid(&HIDClassGuid);
	hDevInfo = SetupDiGetClassDevsA(&HIDClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hDevInfo == NULL)
		return NULL;

	hidenum = M_io_hid_enum_init();

	/* Enumerate devices */
	M_mem_set(&devinfo, 0, sizeof(devinfo));
	devinfo.cbSize = sizeof(devinfo);
	for (devidx = 0; SetupDiEnumDeviceInfo(hDevInfo, devidx, &devinfo); devidx++) {
		DWORD                    ifaceidx;
		SP_DEVICE_INTERFACE_DATA devinterface;

		/* Validate device has a bound driver */
		if (!hid_enum_has_driver(hDevInfo, &devinfo))
			goto cleanupdevinfo;

		/* Enumerate interfaces for a device */
		M_mem_set(&devinterface, 0, sizeof(devinterface));
		devinterface.cbSize = sizeof(devinterface);
		for (ifaceidx = 0; SetupDiEnumDeviceInterfaces(hDevInfo, &devinfo, &HIDClassGuid, ifaceidx, &devinterface); ifaceidx++) {
			char devpath[1024];

			if (hid_enum_devpath(hDevInfo, &devinterface, devpath, sizeof(devpath))) {
				/* If we were able to get a device path, see if we can open it and get the info we need */
				hid_enum_device(hidenum, devpath, vendorid, productids, num_productids, serial);
			}

			M_mem_set(&devinterface, 0, sizeof(devinterface));
			devinterface.cbSize = sizeof(devinterface);
		}

cleanupdevinfo:
		M_mem_set(&devinfo, 0, sizeof(devinfo));
		devinfo.cbSize = sizeof(devinfo);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return hidenum;
}


static void hid_win32_cleanup(M_io_handle_t *handle)
{
	M_free(handle->priv->path);
	M_free(handle->priv->manufacturer);
	M_free(handle->priv->product);
	M_free(handle->priv->serial);

	M_free(handle->priv);
	handle->priv = NULL;
}


M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	M_io_handle_t       *handle;
	HANDLE               shandle;
	PHIDP_PREPARSED_DATA preparsed_data;
	HIDP_CAPS            hid_caps;
	HIDD_ATTRIBUTES      attrib;

	shandle = CreateFile(devpath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (shandle == M_EVENT_INVALID_HANDLE) {
		*ioerr = M_io_win32_err_to_ioerr(GetLastError());
		return NULL;
	}
	if (!HidD_GetPreparsedData(shandle, &preparsed_data)) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		return NULL;
	}

	M_mem_set(&hid_caps, 0, sizeof(hid_caps));
	if (HidP_GetCaps(preparsed_data, &hid_caps) != HIDP_STATUS_SUCCESS) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		HidD_FreePreparsedData(preparsed_data);
		return NULL;
	}

	M_mem_set(&attrib, 0, sizeof(attrib));
	attrib.Size = sizeof(attrib);
	HidD_GetAttributes(shandle, &attrib);

	handle                               = M_io_w32overlap_init_handle(shandle, shandle);
	handle->priv                         = M_malloc_zero(sizeof(*handle->priv));
	handle->priv_cleanup                 = hid_win32_cleanup;
	handle->priv->max_input_report_size  = hid_caps.InputReportByteLength;  /* max size in bytes, including report ID */
	handle->priv->max_output_report_size = hid_caps.OutputReportByteLength; /* same */

	/* TODO: implement report-specific info using the HidP_GetValueCaps() function. */

	handle->priv->path         = M_strdup(devpath);
	handle->priv->manufacturer = hid_get_manufacturer(shandle);
	handle->priv->product      = hid_get_product(shandle);
	handle->priv->serial       = hid_get_serial(shandle);
	handle->priv->productid    = attrib.ProductID;
	handle->priv->vendorid     = attrib.VendorID;

	/*
	M_printf("\n\n\n");
	M_printf("Path:             %s\n", handle->priv->path);
	M_printf("Manufacturer:     %s\n", handle->priv->manufacturer);
	M_printf("Product:          %s\n", handle->priv->product);
	M_printf("Serial:           %s\n", handle->priv->serial);
	M_printf("ProductID:        0x%04X\n", (unsigned)handle->priv->productid);
	M_printf("VendorID:         0x%04X\n", (unsigned)handle->priv->vendorid);
	M_printf("In Rpt Sz (Max):  %d bytes\n", (int)handle->priv->max_input_report_size);
	M_printf("Out Rpt Sz (Max): %d bytes\n", (int)handle->priv->max_output_report_size);
	M_printf("\n\n\n");
	*/

	return handle;
}


static M_io_layer_t *acquire_top_hid_layer(M_io_t *io)
{
	M_io_layer_t *layer       = NULL;
	size_t        layer_idx;
	size_t        layer_count;

	if (io == NULL) {
		return NULL;
	}

	layer_count = M_io_layer_count(io);
	for (layer_idx=layer_count; layer == NULL && layer_idx-->0; ) {
		layer = M_io_layer_acquire(io, layer_idx, M_IO_USB_HID_NAME);
		break;
	}

	return layer;
}


void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
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
		*max_input_size  = handle->priv->max_input_report_size;
		*max_output_size = handle->priv->max_output_report_size;
	}
	M_io_layer_release(layer);
}


const char *M_io_hid_get_path(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return NULL;
	}
	M_io_layer_release(layer);
	return handle->priv->path;
}


const char *M_io_hid_get_manufacturer(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return NULL;
	}
	M_io_layer_release(layer);
	return handle->priv->manufacturer;
}


const char *M_io_hid_get_product(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return NULL;
	}
	M_io_layer_release(layer);
	return handle->priv->product;
}


const char *M_io_hid_get_serial(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return NULL;
	}
	M_io_layer_release(layer);
	return handle->priv->serial;
}


M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return 0;
	}
	M_io_layer_release(layer);
	return handle->priv->productid;
}


M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	M_io_layer_t  *layer  = acquire_top_hid_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL) {
		M_io_layer_release(layer);
		return 0;
	}
	M_io_layer_release(layer);
	return handle->priv->vendorid;
}


M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	return M_io_w32overlap_errormsg_cb(layer, error, err_len);
}

M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	return M_io_w32overlap_state_cb(layer);
}

void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	M_io_w32overlap_destroy_cb(layer);
}

M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	return M_io_w32overlap_process_cb(layer, type);
}

M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	return M_io_w32overlap_write_cb(layer, buf, write_len);
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	return M_io_w32overlap_read_cb(layer, buf, read_len);
}

void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	M_io_w32overlap_unregister_cb(layer);
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	return M_io_w32overlap_disconnect_cb(layer);
}

M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	return M_io_w32overlap_init_cb(layer);
}

