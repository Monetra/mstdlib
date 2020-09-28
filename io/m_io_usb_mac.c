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

static M_bool get_string_from_descriptor_idx(IOUSBDeviceInterface **dev, UInt8 idx, char **str)
{
	IOUSBDevRequest request;
	IOReturn        result;
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

	result = (*dev)->DeviceRequest(dev, &request);
	if (result != kIOReturnSuccess)
		return M_FALSE;

	if (str == NULL || request.wLength <= 2)
		return M_TRUE;

	/* Now we need to parse out the actual data.
 	 * Byte 1 - Length of packet (same as request.wLenDone)
	 * Byte 2 - ?
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

M_io_usb_enum_t *M_io_usb_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_usb_enum_t     *usbenum       = M_io_usb_enum_init();
	io_registry_entry_t  entry         = 0;
	io_iterator_t        iter          = 0;
	io_service_t         usb_device    = 0;
	kern_return_t        kret;

	entry = IORegistryGetRootEntry(kIOMasterPortDefault);
	if (entry == 0)
		goto done;

	kret = IORegistryEntryCreateIterator(entry, kIOUSBPlane, kIORegistryIterateRecursively, &iter);
	if (kret != KERN_SUCCESS || iter == 0)
		goto done;

	while ((usb_device = IOIteratorNext(iter))) {
		IOCFPlugInInterface  **dev_interface   = NULL;
		IOUSBDeviceInterface **dev             = NULL;
		UInt16                 d_vendor_id     = 0;
		UInt16                 d_product_id    = 0;
		io_string_t            path;
		char                  *d_manufacturer  = NULL;
		char                  *d_product       = NULL;
		char                  *d_serial        = NULL;
		SInt32                 score           = 0;
		M_io_usb_speed_t       speed           = M_IO_USB_SPEED_UNKNOWN;
		UInt8                  si;
		IOReturn               result;

        kret = IOCreatePlugInInterfaceForService(usb_device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &dev_interface, &score);
        IOObjectRelease(usb_device);
		if (kret != KERN_SUCCESS || dev_interface == NULL) {
			continue;
		}

        result = (*dev_interface)->QueryInterface(dev_interface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (void *)&dev);
        (*dev_interface)->Release(dev_interface);
        if (result != kIOReturnSuccess || dev == NULL) {
            continue;
        }

		if (IORegistryEntryGetPath(usb_device, kIOServicePlane, path) != KERN_SUCCESS) {
        	(*dev)->Release(dev);
			continue;
		}

        (*dev)->GetDeviceVendor(dev, &d_vendor_id);
        (*dev)->GetDeviceProduct(dev, &d_product_id);

    	if ((*dev)->USBGetManufacturerStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, &d_manufacturer);		
		}
    	if ((*dev)->USBGetProductStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, &d_product);		
		}
    	if ((*dev)->USBGetSerialNumberStringIndex(dev, &si) == kIOReturnSuccess) {
			get_string_from_descriptor_idx(dev, si, &d_serial);		
		}

		if ((*dev)->GetDeviceSpeed(dev, &si) == kIOReturnSuccess) {
			switch (si) {
				case kUSBDeviceSpeedLow:
					speed = M_IO_USB_SPEED_LOW;
					break;
				case kUSBDeviceSpeedFull:
					speed = M_IO_USB_SPEED_FULL;
					break;
				case kUSBDeviceSpeedHigh:
					speed = M_IO_USB_SPEED_HIGH;
					break;
				case kUSBDeviceSpeedSuper:
					speed = M_IO_USB_SPEED_SUPER;
					break;
				case kUSBDeviceSpeedSuperPlus:
					speed = M_IO_USB_SPEED_SUPERPLUS;
					break;
				case kUSBDeviceSpeedSuperPlusBy2:
					speed = M_IO_USB_SPEED_SUPERPLUSX2;
					break;
			}
		}

		M_io_usb_enum_add(usbenum,
				path, speed,
				d_vendor_id, d_product_id, d_serial,
				d_manufacturer, d_product,
				vendorid, productids, num_productids, serial);

		M_free(d_manufacturer);
		M_free(d_product);
		M_free(d_serial);
        (*dev)->Release(dev);
	}

done:
	return usbenum;
}






/* XXX */
M_io_handle_t *M_io_usb_open(const char *devpath, M_io_error_t *ioerr)
{
	(void)devpath;
	*ioerr = M_IO_ERROR_NOTIMPL;
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
	(void)io;
	return 1;
}

M_uint16 M_io_usb_get_productid(M_io_t *io)
{
	(void)io;
	return 1;
}

M_io_error_t M_io_usb_create_control_interface(M_io_t **io_out, M_io_t *io_usb_device, M_uint16 index)
{
	(void)io_out;
	(void)io_usb_device;
	(void)index;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_usb_create_bulk_interface(M_io_t **io_out, M_io_t *io_usb_device, M_int32 index_read, M_int32 index_write)
{
	(void)io_out;
	(void)io_usb_device;
	(void)index_read;
	(void)index_write;
	return M_IO_ERROR_NOTIMPL;
}

M_io_error_t M_io_usb_create_interrupt_interface(M_io_t **io_out, M_io_t *io_usb_device, M_int32 index_read, M_int32 index_write)
{
	(void)io_out;
	(void)io_usb_device;
	(void)index_read;
	(void)index_write;
	return M_IO_ERROR_NOTIMPL;
}

