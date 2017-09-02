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
#include "base/platform/m_platform_win.h"

struct M_io_handle_w32 {
	M_bool         uses_report_descriptors;
	size_t         report_size;
};

typedef BOOLEAN (__stdcall *hidstring_cb_t)(HANDLE, PVOID, ULONG);

static char *M_io_hid_get_string(HANDLE handle, hidstring_cb_t func)
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


static char *M_io_hid_get_serial(HANDLE handle)
{
	return M_io_hid_get_string(handle, HidD_GetSerialNumberString);
}


static char *M_io_hid_get_product(HANDLE handle)
{
	return M_io_hid_get_string(handle, HidD_GetProductString);
}


static char *M_io_hid_get_manufacturer(HANDLE handle)
{
	return M_io_hid_get_string(handle, HidD_GetManufacturerString);
}



static void M_io_hid_enum_device(M_io_hid_enum_t *hidenum, const char *devpath, M_uint16 s_vendor_id,
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

	manufacturer = M_io_hid_get_manufacturer(handle);
	product      = M_io_hid_get_product(handle);
	serial       = M_io_hid_get_serial(handle);
	M_io_hid_enum_add(hidenum, devpath, manufacturer, product, serial, vendorid, productid, 
	                  s_vendor_id, s_product_ids, s_num_product_ids, s_serialnum);

cleanup:
	M_free(manufacturer);
	M_free(product);
	M_free(serial);
	if (handle != INVALID_HANDLE_VALUE)
		CloseHandle(handle);
}


static M_bool M_io_hid_enum_has_driver(HDEVINFO hDevInfo, SP_DEVINFO_DATA *devinfo)
{
	char               classname[256];
	char               drivername[256];
	static const char *hidclass = "HIDClass";
	
	M_mem_set(classname, 0, sizeof(classname));
	M_mem_set(drivername, 0, sizeof(drivername));
	if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, devinfo, SPDRP_CLASS, NULL, classname, sizeof(classname)-1, NULL))
		return M_FALSE;

	/* Validate the class is "HIDClass", otherwise its an error (we don't want keyboards and mice) */
	if (!M_str_eq(hidclass, classname))
		return M_FALSE;

	if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, devinfo, SPDRP_DRIVER, NULL, drivername, sizeof(drivername)-1, NULL))
		return M_FALSE;

	return M_TRUE;
}


static M_bool M_io_hid_enum_devpath(HDEVINFO hDevInfo, SP_DEVICE_INTERFACE_DATA *devinterface, char *devpath, size_t devpath_size)
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
		if (!M_io_hid_enum_has_driver(hDevInfo, &devinfo))
			goto cleanupdevinfo;

		/* Enumerate interfaces for a device */
		M_mem_set(&devinterface, 0, sizeof(devinterface));
		devinterface.cbSize = sizeof(devinterface);
		for (ifaceidx = 0; SetupDiEnumDeviceInterfaces(hDevInfo, &devinfo, &HIDClassGuid, ifaceidx, &devinterface); ifaceidx++) {
			char devpath[1024];

			if (M_io_hid_enum_devpath(hDevInfo, &devinterface, devpath, sizeof(devpath))) {
				/* If we were able to get a device path, see if we can open it and get the info we need */
				M_io_hid_enum_device(hidenum, devpath, vendorid, productids, num_productids, serial);
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


static void M_io_hid_win32_cleanup(M_io_handle_t *handle)
{
	M_free(handle->priv);
	handle->priv = NULL;
}


M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	M_io_handle_t *handle;
	HANDLE         shandle;

	shandle = CreateFile(devpath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (shandle == M_EVENT_INVALID_HANDLE) {
		*ioerr = M_io_win32_err_to_ioerr(GetLastError());
		return NULL;
	}

	handle               = M_io_w32overlap_init_handle(shandle, shandle);
	handle->priv         = M_malloc_zero(sizeof(*handle->priv));
	handle->priv_cleanup = M_io_hid_win32_cleanup;

	/* XXX: Set report descriptor info here */

	return handle;
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

