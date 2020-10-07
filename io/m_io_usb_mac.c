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
#include "m_io_mac_common.h"
#include "m_io_meta.h"

#include <mach/mach_port.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Interfaces are lazy opened when the end points are first accessed.
 * We open them because we can get more info about the end points that
 * way. Having the interface open is fine. Events won't happen unless
 * we've attached them here so we start listending. */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_bool                   run;
	M_thread_mutex_t        *write_lock;
	M_thread_cond_t         *write_cond;
	M_bool                   in_write;
	M_buf_t                 *buf;

	size_t                   iface_num;
	size_t                   ep_num;

	M_io_usb_ep_type_t       type;
	M_io_usb_ep_direction_t  direction;
	size_t                   poll_interval; /* milliseconds */
	size_t                   max_packet_size;
} M_io_usb_interface_ep_t;

typedef struct {
	IOUSBInterfaceInterface **iface;
	M_hash_u64vp_t           *eps; /* key = ep num, val = M_io_usb_interface_ep_t */
	M_hash_u64u64_t          *eps_tids_read; /* key = ep num, val = M_threadid_t */
	M_hash_u64u64_t          *eps_tids_write; /* key = ep num, val = M_threadid_t */
	size_t                    iface_num;
	size_t                    num_eps;
} M_io_usb_interface_t;

typedef struct {
	M_io_handle_t           *handle;
	M_io_usb_interface_t    *usb_iface;
	M_io_usb_interface_ep_t *ep;
} M_io_usb_interface_ep_info_t;

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

	char                   error[256];

	M_hash_u64vp_t        *interfaces; /* key = iface num, val = M_io_usb_interface_t */

	M_llist_t             *read_queue;
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

static M_io_usb_interface_ep_t *M_io_usb_interface_ep_create(size_t iface_num, size_t ep_num, M_io_usb_ep_type_t type, M_io_usb_ep_direction_t direction, size_t poll_interval, size_t max_packet_size)
{
	M_io_usb_interface_ep_t *ep;

	ep                  = M_malloc_zero(sizeof(*ep));
	ep->run             = M_TRUE;
	ep->write_lock      = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	ep->write_cond      = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	ep->iface_num       = iface_num;
	ep->ep_num          = ep_num;
	ep->type            = type;
	ep->direction       = direction;
	ep->poll_interval   = poll_interval;
	ep->max_packet_size = max_packet_size;

	return ep;
}

static void M_io_usb_interface_ep_destroy(M_io_usb_interface_ep_t *ep)
{
	if (ep == NULL)
		return;

	M_thread_mutex_destroy(ep->write_lock);
	M_thread_cond_destroy(ep->write_cond);

	M_free(ep);
}

static M_io_usb_interface_t *M_io_usb_interface_create(IOUSBInterfaceInterface **iface, size_t iface_num)
{
	M_io_usb_interface_t *usb_iface;
	UInt8                 cnt = 0;
 
 	if (iface == NULL)
 		return NULL;
 
 	usb_iface                 = M_malloc_zero(sizeof(*usb_iface));
 	usb_iface->iface          = iface;
 	usb_iface->iface_num      = iface_num;
	(*iface)->GetNumEndpoints(iface, &cnt);
	if (cnt > 0)
		cnt--; /* -1 because we don't want to include the 0 control interface. */
	usb_iface->num_eps        = cnt;
	usb_iface->eps            = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void (*)(void *))M_io_usb_interface_ep_destroy);
	usb_iface->eps_tids_read  = M_hash_u64u64_create(8, 75, M_HASH_U64U64_NONE);
	usb_iface->eps_tids_write = M_hash_u64u64_create(8, 75, M_HASH_U64U64_NONE);

	return usb_iface;
}

static void M_io_usb_interface_destroy(M_io_usb_interface_t *usb_iface)
{
	if (usb_iface == NULL)
		return;

	M_hash_u64vp_destroy(usb_iface->eps, M_TRUE);
	M_hash_u64u64_destroy(usb_iface->eps_tids_read);
	M_hash_u64u64_destroy(usb_iface->eps_tids_write);

	M_free(usb_iface);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_io_usb_open_interface(M_io_handle_t *handle, size_t iface_num)
{
	M_io_usb_interface_t       *usb_iface;
	M_io_usb_interface_ep_t    *ep;
	IOUSBInterfaceInterface   **iface   = NULL;
	io_iterator_t               iter    = 0;
	io_service_t                service = 0;
	IOUSBFindInterfaceRequest   req;
	IOReturn                    ioret;
	size_t                      idx     = 0;
	M_bool                      ret     = M_TRUE;

	if (handle == NULL || handle->dev == NULL)
		return M_FALSE;

	/* Check if we've already opened this interface. */
	if (M_hash_u64vp_get(handle->interfaces, iface_num, NULL))
		return M_TRUE;

	/* Iterate our interfaces looking for the one we want. */
	M_mem_set(&req, 0, sizeof(req));
	req.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

	ioret = (*handle->dev)->CreateInterfaceIterator(handle->dev, &req, &iter);
	if (ioret != kIOReturnSuccess || iter == 0) {
		ret = M_FALSE;
		goto done;
	}

	while ((service = IOIteratorNext(iter))) {
		IOCFPlugInInterface **plug  = NULL;
		SInt32                score = 0;

		if (idx != iface_num) {
			idx++;
			continue;
		}

		/* Create the interface. */
		IOCreatePlugInInterfaceForService(service, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
		IOObjectRelease(service);
		if (plug == NULL) {
			ret = M_FALSE;
			goto done;
		}

		(*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&iface);
		(*plug)->Release(plug);
		if (iface == NULL) {
			ret = M_FALSE;
			goto done;
		}

		ioret = (*iface)->USBInterfaceOpen(iface);
		if (ioret != kIOReturnSuccess) {
			(*iface)->Release(iface);
			ret = M_FALSE;
			goto done;
		}

		/* We should have the interface opened at this point. */
		break;
	}

	/* Verify we found the interface. If iface_num
 	 * was greater than the number of device interfaces,
	 * we'll be here without an iface. */
	if (iface == NULL) {
		ret = M_FALSE;
		goto done;
	}

	usb_iface = M_io_usb_interface_create(iface, iface_num);
	M_hash_u64vp_insert(handle->interfaces, iface_num, usb_iface);

	/* Now that we have the interface, let's get all the info about
 	 * the end points. */
	for (idx=0; idx<usb_iface->num_eps; idx++) {
		IOUSBEndpointProperties properties;
		M_io_usb_ep_type_t      type;
		M_io_usb_ep_direction_t direction = M_IO_USB_EP_DIRECTION_UNKNOWN;

		M_mem_set(&properties, 0, sizeof(properties));
		properties.bVersion = kUSBEndpointPropertiesVersion3;

		/* +1 when because the range is 1 to num
		 * endpoints. EP 0 is special and is the device control end point. */
    	ioret = (*iface)->GetPipePropertiesV3(iface, (UInt8)idx+1, &properties);
		if (ioret != kIOReturnSuccess) {
			/* Bad end point? */
			continue;
		}

		switch (properties.bTransferType) {
			case kUSBControl:
				type = M_IO_USB_EP_TYPE_CONTROL;
				break;
			case kUSBIsoc:
				type = M_IO_USB_EP_TYPE_ISOC;
				break;
			case kUSBBulk:
				type = M_IO_USB_EP_TYPE_BULK;
				break;
			case kUSBInterrupt:
				type = M_IO_USB_EP_TYPE_INTERRUPT;
				break;
			case kUSBAnyType:
				/* Bad endpoint? */
				continue;
		}

		if (properties.bDirection & kUSBIn)
			direction |= M_IO_USB_EP_DIRECTION_IN;
		if (properties.bDirection & kUSBOut)
			direction |= M_IO_USB_EP_DIRECTION_OUT;

		ep = M_io_usb_interface_ep_create(usb_iface->iface_num, idx, type, direction, properties.bInterval, properties.wMaxPacketSize);
		M_hash_u64vp_insert(usb_iface->eps, idx, ep);
	}

done:
	if (iter != 0)
		IOObjectRelease(iter);
	if (!ret && iface != NULL)
		(*iface)->Release(iface);

	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Note: macOS provides callback based functions for
 * read and write events and it uses the macOS event
 * loop. We're not using them right now because thread
 * loops with the timeout versions of the functions are
 * easier to implement. We will want to switch to the
 * event based callbacks in the future. We'd call the
 * wait event function within the callback to wait for
 * more data to be avaliable.
 *
 * When we use the aync event basec callback functions,
 * the AbortPipe will stop waiting and trigger the callback.
 * We'll want to use this when we're closing the device to
 * break out of the loop we'll be in.
 *
 * Breaking out is the hard part. With a thread loop we
 * have a run variable, call set that to false, join
 * the thread and wait for it to exit. With the callback
 * system, we'll need some way to signal when all of the
 * callbacks have exited.
 */
static void *M_io_usb_bulkirpt_read_loop(void *arg)
{
	M_io_usb_interface_ep_info_t *info = arg;
	M_io_layer_t                 *layer;
	unsigned char                *buf;
	IOReturn                      ioret;
	M_io_error_t                  ioerr;
	UInt32                        size;
	M_bool                        stall;

	buf = M_malloc_zero(info->ep->max_packet_size);
	/* XXX: Processing LOOP! */
	while (info->ep->run) {

		size  = (UInt32)info->ep->max_packet_size;
	    ioret = (*info->usb_iface->iface)->ReadPipe(info->usb_iface->iface, (UInt8)info->ep->ep_num, buf, &size);

		stall = M_FALSE;
		if (ioret != kIOReturnSuccess) {
			if (ioret == kIOUSBPipeStalled) {
				stall = M_TRUE;
			} else if ((*info->usb_iface->iface)->GetPipeStatus(info->usb_iface->iface, (UInt8)info->ep->ep_num+1) == kIOUSBPipeStalled) {
				stall = M_TRUE;
			}
			if (stall) {
				ioret = (*info->usb_iface->iface)->ClearPipeStall(info->usb_iface->iface, (UInt8)info->ep->ep_num+1);
			}
		}

		ioerr = M_io_mac_ioreturn_to_err(ioret);
		if (M_io_error_is_critical(ioerr)) {
			layer = M_io_layer_acquire(info->handle->io, 0, NULL);
			M_snprintf(info->handle->error, sizeof(info->handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
			// XXX: ??? M_io_usb_close_device(info->handle);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, ioerr);
			M_io_layer_release(layer);
			break;
		}

		if (size > 0) {
			layer = M_io_layer_acquire(info->handle->io, 0, NULL);
			M_io_usb_rdata_queue_add_read_bulkirpt(info->handle->read_queue, info->ep->type, info->ep->iface_num, info->ep->ep_num, buf, size);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
			M_io_layer_release(layer);
		}
	}

	M_free(buf);
	/* Don't free info data. Handled elsewhere. */
	M_free(info);
	return NULL;
}

static void *M_io_usb_bulkirpt_write_loop(void *arg)
{
	M_io_usb_interface_ep_info_t *info = arg;
	M_io_layer_t                 *layer;
	IOReturn                      ioret;
	M_io_error_t                  ioerr;
	UInt32                        len;
	M_bool                        stall;

	while (info->ep->run) {
		M_thread_mutex_lock(info->ep->write_lock);
		if (M_buf_len(info->ep->buf) == 0) {
			M_thread_cond_wait(info->ep->write_cond, info->ep->write_lock);
		}
		if (!info->ep->run) {
			M_thread_mutex_unlock(info->ep->write_lock);
			break;
		}
		if (M_buf_len(info->ep->buf) == 0) {
			M_thread_mutex_unlock(info->ep->write_lock);
			continue;
		}
		info->ep->in_write = M_TRUE;
		M_thread_mutex_unlock(info->ep->write_lock);

		len   = (UInt32)M_MIN(M_buf_len(info->ep->buf), info->ep->max_packet_size);
		ioret = (*info->usb_iface->iface)->WritePipe(info->usb_iface->iface, (UInt8)info->ep->ep_num+1, (void *)M_buf_peek(info->ep->buf), len);

		/* Check for stall and clear pipe. */
		stall = M_FALSE;
		if (ioret != kIOReturnSuccess) {
			if (ioret == kIOUSBPipeStalled) {
				stall = M_TRUE;
			} else if ((*info->usb_iface->iface)->GetPipeStatus(info->usb_iface->iface, (UInt8)info->ep->ep_num+1) == kIOUSBPipeStalled) {
				stall = M_TRUE;
			}
			if (stall) {
				ioret = (*info->usb_iface->iface)->ClearPipeStall(info->usb_iface->iface, (UInt8)info->ep->ep_num+1);
			}
		}

		ioerr = M_io_mac_ioreturn_to_err(ioret);
		if (M_io_error_is_critical(ioerr)) {
			M_thread_mutex_lock(info->ep->write_lock);
			info->ep->in_write = M_FALSE;
			M_thread_mutex_unlock(info->ep->write_lock);

			layer = M_io_layer_acquire(info->handle->io, 0, NULL);
			M_snprintf(info->handle->error, sizeof(info->handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
			// XXX: ??? M_io_usb_close_device(info->handle);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, ioerr);
			M_io_layer_release(layer);
			break;
		}

		if (!stall && ioerr == M_IO_ERROR_SUCCESS) {
			M_buf_drop(info->ep->buf, len);
		}
		if (M_buf_len(info->ep->buf) == 0) {
			M_thread_mutex_lock(info->ep->write_lock);
			info->ep->in_write = M_FALSE;
			M_thread_mutex_unlock(info->ep->write_lock);
		}

		if (ioerr == M_IO_ERROR_SUCCESS && M_buf_len(info->ep->buf) == 0) {
			layer = M_io_layer_acquire(info->handle->io, 0, NULL);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
			M_io_layer_release(layer);
		}
	}

	/* Don't free info data. Handled elsewhere. */
	M_free(info);
	return NULL;
}

static M_bool M_io_usb_attach_interface_endpoint_int(M_io_handle_t *handle, size_t iface_num, size_t ep_num)
{
	M_io_usb_interface_t         *usb_iface;
	M_io_usb_interface_ep_t      *ep;
	M_io_usb_interface_ep_info_t *info;
	M_threadid_t                  tid;
	M_thread_attr_t              *tattr;

	if (!M_io_usb_open_interface(handle, iface_num))
		return M_FALSE;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	/* Bad iface_num most likley. */
	if (usb_iface == NULL)
		return M_FALSE;

	/* Check if we've already attached. If we have, then we're going
 	 * to return success because we don't have anything that we need to do. */
	if (M_hash_u64u64_get(usb_iface->eps_tids_read, ep_num, NULL) || M_hash_u64u64_get(usb_iface->eps_tids_write, ep_num, NULL))
		return M_TRUE;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);
	/* Must have been an invalid ep_num or a bad ep. */
	if (ep == NULL)
		return M_FALSE;

	switch (ep->type) {
		case M_IO_USB_EP_TYPE_CONTROL:
			/* Must be used directly with the device not an interface. */
		case M_IO_USB_EP_TYPE_ISOC:
		case M_IO_USB_EP_TYPE_UNKNOWN:
			/* Not currently supported. */
			return M_FALSE;
		case M_IO_USB_EP_TYPE_BULK:
		case M_IO_USB_EP_TYPE_INTERRUPT:
			break;
	}

	/* Uhhh... What? */
	if (ep->direction == M_IO_USB_EP_DIRECTION_UNKNOWN)
		return M_FALSE;

	/* Start our loops. */
	if (ep->direction & M_IO_USB_EP_DIRECTION_IN) {
		info         = M_malloc_zero(sizeof(*info));
		info->handle = handle;
		info->ep     = ep;

		tattr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(tattr, M_TRUE);
		tid   = M_thread_create(tattr, M_io_usb_bulkirpt_read_loop, info);
		M_thread_attr_destroy(tattr);
		M_hash_u64u64_insert(usb_iface->eps_tids_read, ep_num, tid);
		/* Thread owns info and the thread will destroy it. */
	}
	if (ep->direction & M_IO_USB_EP_DIRECTION_OUT) {
		info         = M_malloc_zero(sizeof(*info));
		info->handle = handle;
		info->ep     = ep;

		tattr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(tattr, M_TRUE);
		tid   = M_thread_create(tattr, M_io_usb_bulkirpt_write_loop, info);
		M_thread_attr_destroy(tattr);
		M_hash_u64u64_insert(usb_iface->eps_tids_write, ep_num, tid);
		/* Thread owns info and the thread will destroy it. */
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_error_t M_io_usb_write_control(M_io_handle_t *handle, const unsigned char *buf, size_t *write_len, M_hash_multi_t *mdata)
{
	(void)handle;
	(void)buf;
	(void)write_len;
	(void)mdata;
	return M_IO_ERROR_NOTIMPL;
}

static M_io_error_t M_io_usb_write_bulkirpt(M_io_handle_t *handle, const unsigned char *buf, size_t *write_len, M_hash_multi_t *mdata)
{
	M_io_usb_interface_t    *usb_iface;
	M_io_usb_interface_ep_t *ep;
	M_uint64                 iface_num;
	M_uint64                 ep_num;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_IFACE_NUM, &iface_num))
		return M_IO_ERROR_INVALID;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_EP_NUM, &ep_num))
		return M_IO_ERROR_INVALID;

	if (!M_io_usb_attach_interface_endpoint_int(handle, iface_num, ep_num))
		return M_IO_ERROR_INVALID;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);	
	if (usb_iface == NULL)
		return M_IO_ERROR_INVALID;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);	
	if (ep == NULL)
		return M_IO_ERROR_INVALID;

	M_thread_mutex_lock(ep->write_lock);

	if (ep->in_write || M_buf_len(ep->buf) > 0) {
		M_thread_mutex_unlock(ep->write_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	if (buf != NULL && *write_len != 0)
		M_buf_add_bytes(ep->buf, buf, *write_len);

	if (M_buf_len(ep->buf) == 0) {
		M_thread_mutex_unlock(ep->write_lock);
		return M_IO_ERROR_SUCCESS;
	}

	M_thread_cond_signal(ep->write_cond);
	M_thread_mutex_unlock(ep->write_lock);
	return M_IO_ERROR_SUCCESS;
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
	struct M_llist_callbacks llcbs = {
		NULL,
		NULL,
		NULL,
		(M_llist_free_func)M_io_usb_rdata_destroy
	};

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

	/* XXX: This needs to be done in a thread(?) with a
 	 * timeout because it's a blocking function?. */
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

	handle->interfaces   = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void (*)(void *))M_io_usb_interface_destroy);

	handle->read_queue   = M_llist_create(&llcbs, M_LLIST_NONE);

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
	M_io_handle_t      *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t     *mdata;
	M_io_usb_ep_type_t  ep_type;
	M_uint64            u64v;

	if (meta == NULL)
		return M_IO_ERROR_INVALID;

	mdata = M_io_meta_get_layer_data(meta, layer);
	if (mdata == NULL)
		return M_IO_ERROR_INVALID;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_EP_TYPE, &u64v))
		return M_IO_ERROR_INVALID;
	ep_type = (M_io_usb_ep_type_t)u64v;

	switch (ep_type) {
		case M_IO_USB_EP_TYPE_CONTROL:
			return M_io_usb_write_control(handle, buf, write_len, mdata);
		case M_IO_USB_EP_TYPE_BULK:
		case M_IO_USB_EP_TYPE_INTERRUPT:
		case M_IO_USB_EP_TYPE_ISOC:
		case M_IO_USB_EP_TYPE_UNKNOWN:
			return M_io_usb_write_bulkirpt(handle, buf, write_len, mdata);
	}
}

M_io_error_t M_io_usb_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t    *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t   *mdata;
	M_llist_node_t   *node;
	M_io_usb_rdata_t *rdata;

	/* Validate we have a meta object. If not we don't know what to do. */
	if (meta == NULL)
		return M_IO_ERROR_INVALID;

	mdata = M_io_meta_get_layer_data(meta, layer);
	if (mdata == NULL) {
		mdata = M_hash_multi_create(M_HASH_MULTI_NONE);
		M_io_meta_insert_layer_data(meta, layer, mdata, (void (*)(void *))M_hash_multi_destroy);
	}

	/* Get the first record in the read queue. If no
 	 * records we're all done. */
	node = M_llist_first(handle->read_queue);
	if (node == NULL)
		return M_IO_ERROR_WOULDBLOCK;
	rdata = M_llist_node_val(node);

	/* Get the type of read the data is for. */
	M_hash_multi_u64_insert_int(mdata, M_IO_USB_META_KEY_EP_TYPE, rdata->ep_type);
	switch (rdata->ep_type) {
		case M_IO_USB_EP_TYPE_BULK:
		case M_IO_USB_EP_TYPE_INTERRUPT:
		case M_IO_USB_EP_TYPE_ISOC:
			M_hash_multi_u64_insert_uint(mdata, M_IO_USB_META_KEY_IFACE_NUM, rdata->iface_num);
			M_hash_multi_u64_insert_uint(mdata, M_IO_USB_META_KEY_EP_NUM, rdata->ep_num);
			break;
		case M_IO_USB_EP_TYPE_CONTROL:
		case M_IO_USB_EP_TYPE_UNKNOWN:
			break;
	}


	if (buf != NULL && read_len != NULL) {
		if (*read_len > M_buf_len(rdata->data)) {
			*read_len = M_buf_len(rdata->data);
		}

		M_mem_copy(buf, M_buf_peek(rdata->data), *read_len);
		M_buf_drop(rdata->data, *read_len);
	}

	/* If we've read everything we will drop this record. */
	if (M_buf_len(rdata->data) == 0)
		M_llist_remove_node(node);

	return M_IO_ERROR_SUCCESS;
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_usb_attach_interface_endpoint(M_io_t *io, size_t iface_num, size_t ep_num)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_bool         ret;

	ret = M_io_usb_attach_interface_endpoint_int(handle, iface_num, ep_num);

	M_io_layer_release(layer);
	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
	M_io_layer_t               *layer   = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t              *handle  = M_io_layer_get_handle(layer);
	size_t                      cnt     = 0;
	IOReturn                    ioret;
	IOUSBFindInterfaceRequest   req;
	io_iterator_t               iter    = 0;
	io_service_t                service = 0;

	if (handle == NULL)
		goto done;

	M_mem_set(&req, 0, sizeof(req));
	req.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

	ioret = (*handle->dev)->CreateInterfaceIterator(handle->dev, &req, &iter);
	if (ioret != kIOReturnSuccess || iter == 0)
		goto done;

	while ((service = IOIteratorNext(iter))) {
		IOObjectRelease(service);
		cnt++;
	}

done:
	if (iter != 0)
		IOObjectRelease(iter);
	M_io_layer_release(layer);
	return cnt;
}

size_t M_io_usb_interface_num_endpoint(M_io_t *io, size_t iface_num)
{
	M_io_layer_t         *layer    = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t        *handle   = M_io_layer_get_handle(layer);
	M_io_usb_interface_t *usb_iface;
	size_t                cnt      = 0;

	if (handle == NULL)
		goto done;

	/* Ensure we have the interface open so we have all the info about it. */
	if (!M_io_usb_open_interface(handle, iface_num))
		goto done;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	if (usb_iface == NULL)
		goto done;

	cnt = usb_iface->num_eps;

done:
	M_io_layer_release(layer);
	return cnt;
}

M_io_usb_ep_type_t M_io_usb_endpoint_type(M_io_t *io, size_t iface_num, size_t ep_num)
{
	M_io_layer_t            *layer     = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t           *handle    = M_io_layer_get_handle(layer);
	M_io_usb_interface_t    *usb_iface;
	M_io_usb_interface_ep_t *ep;
	M_io_usb_ep_type_t       type      = M_IO_USB_EP_TYPE_UNKNOWN;

	if (handle == NULL)
		goto done;

	/* Ensure we have the interface open so we have all the info about it. */
	if (!M_io_usb_open_interface(handle, iface_num))
		goto done;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	if (usb_iface == NULL)
		goto done;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);
	if (ep == NULL)
		goto done;

	type = ep->type;

done:
	M_io_layer_release(layer);
	return type;
}

M_io_usb_ep_direction_t M_io_usb_endpoint_direction(M_io_t *io, size_t iface_num, size_t ep_num)
{
	M_io_layer_t            *layer     = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t           *handle    = M_io_layer_get_handle(layer);
	M_io_usb_interface_t    *usb_iface;
	M_io_usb_interface_ep_t *ep;
	M_io_usb_ep_direction_t  direction = M_IO_USB_EP_DIRECTION_UNKNOWN;

	if (handle == NULL)
		goto done;

	/* Ensure we have the interface open so we have all the info about it. */
	if (!M_io_usb_open_interface(handle, iface_num))
		goto done;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	if (usb_iface == NULL)
		goto done;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);
	if (ep == NULL)
		goto done;

	direction = ep->direction;

done:
	M_io_layer_release(layer);
	return direction;
}

size_t M_io_usb_endpoint_max_packet_size(M_io_t *io, size_t iface_num, size_t ep_num)
{
	M_io_layer_t            *layer     = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t           *handle    = M_io_layer_get_handle(layer);
	M_io_usb_interface_t    *usb_iface;
	M_io_usb_interface_ep_t *ep;
	size_t                   max       = 0;

	if (handle == NULL)
		goto done;

	/* Ensure we have the interface open so we have all the info about it. */
	if (!M_io_usb_open_interface(handle, iface_num))
		goto done;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	if (usb_iface == NULL)
		goto done;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);
	if (ep == NULL)
		goto done;

	max = ep->max_packet_size;

done:
	M_io_layer_release(layer);
	return max;
}
