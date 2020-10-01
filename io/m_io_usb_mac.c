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

#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_usb.h>
#include "m_io_usb_int.h"

#include <mach/mach_port.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_io_handle {
	IOUSBDeviceInterface **dev;
	M_io_t                *io;

	char                  *manufacturer;
	char                  *product;
	char                  *serial;
	M_uint16               vendorid;
	M_uint16               productid;
	M_io_usb_speed_t       speed;
	char                  *path;

	M_list_t              *interfaces; /* List of interfaces, IOUSBInterfaceInterface */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool get_string_from_descriptor_idx(IOUSBDeviceInterface **dev, UInt8 idx, char **str)
{
	IOUSBDevRequest request;
	IOReturn        ioret;
	char            buffer[4086] = { 0 };
	CFStringRef     cfstr;
	CFIndex         len;

	if (str != NULL)
		*str = NULL;

	request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
	request.bRequest      = kUSBRqGetDescriptor;
	request.wValue        = (kUSBStringDesc << 8) | idx;
	request.wIndex        = 0x409;
	request.wLength       = sizeof(buffer);
	request.pData         = buffer;

	ioret = (*dev)->DeviceRequest(dev, &request);
	if (ioret != kIOReturnSuccess)
		return M_FALSE;

	if (str == NULL || request.wLength <= 2)
		return M_TRUE;

	/* Now we need to parse out the actual data.
 	 * Byte 1 - Length of packet (same as request.wLenDone)
	 * Byte 2 - Type
	 * Byte 3+ - Data
	 *
	 * Data is a little endian UTF16 string which we need to convert to a utf-8 string.
	 * There are a few ways we can do it but we'll use CFString method right now.
	 * Once we have a text conversion form utf-16 to 8 in mstdlib we'll want to change
	 * to using that instead.
	 */
	cfstr   = CFStringCreateWithBytes(NULL, (const UInt8 *)buffer+2, request.wLenDone-2, kCFStringEncodingUTF16LE, 0);
	len     = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
	if (len < 0) {
		CFRelease(cfstr);
		return M_TRUE;
	}
	*str    = M_malloc_zero((size_t)len);
	CFStringGetCString(cfstr, *str, len, kCFStringEncodingUTF8); 
	M_str_trim(*str);

	CFRelease(cfstr);
	return M_TRUE;
}

static void M_io_usb_dev_info(IOUSBDeviceInterface **dev,
		M_uint16 *vendor_id, M_uint16 *product_id,
		char **manufacturer, char **product, char **serial,
		M_io_usb_speed_t *speed, size_t *curr_config)
{
	UInt8  si;
	UInt16 u16v;

	if (vendor_id != NULL) {
		*vendor_id = 0;
		if ((*dev)->GetDeviceVendor(dev, &u16v) == kIOReturnSuccess) {
			*vendor_id = u16v;
		}
	}

	if (product_id != NULL) {
		*product_id = 0;
		if ((*dev)->GetDeviceProduct(dev, &u16v) == kIOReturnSuccess) {
			*product_id = u16v;
		}
	}

	if (manufacturer != NULL) {
		*manufacturer = NULL;
		if ((*dev)->USBGetManufacturerStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, manufacturer);
		}
	}

	if (product != NULL) {
		*product = NULL;
		if ((*dev)->USBGetProductStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, product);
		}
	}

	if (serial != NULL) {
		*serial = NULL;
		if ((*dev)->USBGetSerialNumberStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, serial);
		}
	}

	if (speed != NULL) {
		*speed = M_IO_USB_SPEED_UNKNOWN;
		if ((*dev)->GetDeviceSpeed(dev, &si) == kIOReturnSuccess) {
			switch (si) {
				case kUSBDeviceSpeedLow:
					*speed = M_IO_USB_SPEED_LOW;
					break;
				case kUSBDeviceSpeedFull:
					*speed = M_IO_USB_SPEED_FULL;
					break;
				case kUSBDeviceSpeedHigh:
					*speed = M_IO_USB_SPEED_HIGH;
					break;
				case kUSBDeviceSpeedSuper:
					*speed = M_IO_USB_SPEED_SUPER;
					break;
				case kUSBDeviceSpeedSuperPlus:
					*speed = M_IO_USB_SPEED_SUPERPLUS;
					break;
				case kUSBDeviceSpeedSuperPlusBy2:
					*speed = M_IO_USB_SPEED_SUPERPLUSX2;
					break;
			}
		}
	}

	if (curr_config != NULL) {
		*curr_config = 0;
		if ((*dev)->GetConfiguration(dev, &si) == kIOReturnSuccess) {
			*curr_config = si;
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_usb_enum_t *M_io_usb_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_usb_enum_t     *usbenum       = M_io_usb_enum_init();
	io_registry_entry_t  entry         = 0;
	io_iterator_t        iter          = 0;
	io_service_t         service       = 0;
	kern_return_t        kret;

	entry = IORegistryGetRootEntry(kIOMasterPortDefault);
	if (entry == 0)
		goto done;

	kret = IORegistryEntryCreateIterator(entry, kIOUSBPlane, kIORegistryIterateRecursively, &iter);
	if (kret != KERN_SUCCESS || iter == 0)
		goto done;

	while ((service = IOIteratorNext(iter))) {
		IOCFPlugInInterface  **plug            = NULL;
		IOUSBDeviceInterface **dev             = NULL;
		M_uint16               d_vendor_id     = 0;
		M_uint16               d_product_id    = 0;
		io_string_t            path;
		char                  *d_manufacturer  = NULL;
		char                  *d_product       = NULL;
		char                  *d_serial        = NULL;
		M_io_usb_speed_t       d_speed         = M_IO_USB_SPEED_UNKNOWN;
		size_t                 d_curr_config   = 0;
		SInt32                 score           = 0;
		IOReturn               ioret;

        kret = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
        IOObjectRelease(service);
		if (kret != KERN_SUCCESS || plug == NULL) {
			continue;
		}

        ioret = (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (void *)&dev);
        (*plug)->Release(plug);
        if (ioret != kIOReturnSuccess || dev == NULL) {
            continue;
        }

		if (IORegistryEntryGetPath(service, kIOServicePlane, path) != KERN_SUCCESS) {
        	(*dev)->Release(dev);
			continue;
		}

		M_io_usb_dev_info(dev,
				&d_vendor_id, &d_product_id,
				&d_manufacturer, &d_product, &d_serial,
				&d_speed, &d_curr_config);

		M_io_usb_enum_add(usbenum,
				path,
				d_vendor_id, d_product_id,
				d_manufacturer, d_product, d_serial,
				d_speed, d_curr_config,
				vendorid, productids, num_productids, serial);

		M_free(d_manufacturer);
		M_free(d_product);
		M_free(d_serial);
        (*dev)->Release(dev);
	}
	IOObjectRelease(iter);

done:
	return usbenum;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* XXX: Read and write pipe cbs.
 * control, and bulk/interrupt. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */



/* XXX */
M_io_handle_t *M_io_usb_open(const char *devpath, M_io_error_t *ioerr)
{
	M_io_handle_t         *handle;
	io_service_t           service         = 0;
	IOCFPlugInInterface  **plug            = NULL;
	IOUSBDeviceInterface **dev             = NULL;
	IOReturn               ioret;
	kern_return_t          kret;
	M_uint16               d_vendor_id     = 0;
	M_uint16               d_product_id    = 0;
	char                  *d_manufacturer  = NULL;
	char                  *d_product       = NULL;
	char                  *d_serial        = NULL;
	M_io_usb_speed_t       d_speed         = M_IO_USB_SPEED_UNKNOWN;
	SInt32                 score           = 0;

	if (M_str_isempty(devpath)) {
		*ioerr = M_IO_ERROR_INVALID;
		goto err;
	}

	service = IORegistryEntryFromPath(kIOMasterPortDefault, devpath);
	if (service == 0) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto err;
	}

	kret = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
	IOObjectRelease(service);
	if (kret != KERN_SUCCESS || plug == NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto err;
	}

	ioret = (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (void *)&dev);
	(*plug)->Release(plug);
	if (ioret != kIOReturnSuccess || dev == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	ioret = (*dev)->USBDeviceOpen(dev);
	if (ioret != kIOReturnSuccess) {
		*ioerr = M_IO_ERROR_NOTCONNECTED;
		goto err;
	}

	handle      = M_malloc_zero(sizeof(*handle));
	handle->dev = dev;

	M_io_usb_dev_info(dev,
			&d_vendor_id, &d_product_id,
			&d_manufacturer, &d_product, &d_serial,
			&d_speed, NULL);
	handle->vendorid     = d_vendor_id;
	handle->productid    = d_product_id;
	handle->manufacturer = d_manufacturer;
	handle->product      = d_product;
	handle->serial       = d_serial;
	handle->speed        = d_speed;

	handle->path         = M_strdup(devpath);


#if 0
	io_iterator_t iter;
	IOUSBFindInterfaceRequest r;
	r.bInterfaceClass = kIOUSBFindInterfaceDontCare;
	r.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	r.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	r.bAlternateSetting = kIOUSBFindInterfaceDontCare;
	ioret = (*dev)->CreateInterfaceIterator(dev, &r, &iter);
	if (ioret != kIOReturnSuccess || iter == 0) {
		M_printf("=== it cf\n");
	}
	while ((service = IOIteratorNext(iter))) {
		IOUSBInterfaceInterface **iface;
		UInt8 cnt = 0;

		IOCreatePlugInInterfaceForService(service,
				kIOUSBInterfaceUserClientTypeID,
				kIOCFPlugInInterfaceID, &plug, &score);
		IOObjectRelease(service);
		(*plug)->QueryInterface(plug,
				CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
				(LPVOID *)&iface);
		(*plug)->Release(plug);
		(*iface)->USBInterfaceOpen(iface);
		(*iface)->GetNumEndpoints(iface, &cnt);
		M_printf("=== %u\n", cnt);
	}
	IOObjectRelease(iter);
#endif

	return handle;

err:
	if (dev != NULL)
        (*dev)->Release(dev);

	return NULL;
}

M_bool M_io_usb_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	(void)layer;
	(void)error;
	(void)err_len;
	return M_FALSE;
}

M_io_state_t M_io_usb_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_ERROR;
}

void M_io_usb_destroy_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_usb_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	return M_FALSE;
}

M_io_error_t M_io_usb_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)write_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_usb_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	(void)layer;
	(void)buf;
	(void)read_len;
	(void)meta;
	return M_IO_ERROR_NOTIMPL;
}

M_bool M_io_usb_disconnect_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_TRUE;
}

void M_io_usb_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_usb_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_FALSE;
}

M_uint16 M_io_usb_get_vendorid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL)
		ret = handle->vendorid;

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_usb_get_productid(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret    = 0;

	if (handle != NULL)
		ret = handle->productid;

	M_io_layer_release(layer);
	return ret;
}

char *M_io_usb_get_manufacturer(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->manufacturer);

	M_io_layer_release(layer);
	return ret;
}

char *M_io_usb_get_product(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->product);

	M_io_layer_release(layer);
	return ret;
}

char *M_io_usb_get_serial(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	char          *ret    = NULL;

	if (handle != NULL)
		ret = M_strdup(handle->serial);

	M_io_layer_release(layer);
	return ret;
}

size_t M_io_usb_num_interface(M_io_t *io)
{
	M_io_layer_t              *layer   = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t             *handle  = M_io_layer_get_handle(layer);
	io_service_t               service = 0;
	io_iterator_t              iter;
	IOUSBFindInterfaceRequest  req;
	IOReturn                   ioret;
	size_t                     cnt     = 0;

	if (handle == NULL)
		goto done;

	req.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

	ioret = (*handle->dev)->CreateInterfaceIterator(handle->dev, &req, &iter);
	if (ioret != kIOReturnSuccess || iter == 0)
		goto done;

	while ((service = IOIteratorNext(iter))) {
		cnt++;
	}
	IOObjectRelease(iter);

done:
	M_io_layer_release(layer);
	return cnt;
}

size_t M_io_usb_interface_num_endpoint(M_io_t *io, size_t iface_num)
{
	M_io_layer_t              *layer   = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t             *handle  = M_io_layer_get_handle(layer);
	io_service_t               service = 0;
	io_iterator_t              iter;
	IOUSBFindInterfaceRequest  req;
	IOReturn                   ioret;
	UInt8                      cnt     = 0;
	size_t                     idx     = 0;

	if (handle == NULL)
		goto done;

	req.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

	ioret = (*handle->dev)->CreateInterfaceIterator(handle->dev, &req, &iter);
	if (ioret != kIOReturnSuccess || iter == 0)
		goto done;

	while ((service = IOIteratorNext(iter))) {
		IOCFPlugInInterface     **plug  = NULL;
		IOUSBInterfaceInterface **iface = NULL;
		SInt32                    score = 0;

		if (idx != iface_num) {
			idx++;
			continue;
		}

		IOCreatePlugInInterfaceForService(service, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
		IOObjectRelease(service);
		if (plug == NULL) {
			break;
		}

		(*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&iface);
		(*plug)->Release(plug);
		if (iface == NULL) {
			break;
		}
		(*iface)->GetNumEndpoints(iface, &cnt);
		(*iface)->Release(iface);
	}
	IOObjectRelease(iter);

done:
	M_io_layer_release(layer);
	return cnt;
}
