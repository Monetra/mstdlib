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
#include "m_io_int.h"
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
 * we're listening for them. */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	/* Events processing status. */
	M_bool                   run;
	M_thread_mutex_t        *running_lock;

	/* Read status. */
	M_bool                   in_read;
	M_thread_mutex_t        *read_lock;
	unsigned char           *read_buf;

	/* Write status. */
	M_bool                   in_write;
	M_thread_mutex_t        *write_lock;
	M_buf_t                 *write_buf;

	/* Metadata. */
	size_t                   iface_num;
	size_t                   ep_num;
	M_io_usb_ep_type_t       type;
	M_io_usb_ep_direction_t  direction;
	size_t                   poll_interval; /* milliseconds */
	size_t                   max_packet_size;

	/* References */
	IOUSBInterfaceInterface **iface;
	M_io_handle_t            *handle;
} M_io_usb_ep_t;

typedef struct {
	IOUSBInterfaceInterface **iface;
	size_t                    iface_num;

	M_hash_u64vp_t           *eps; /* key = ep num, val = M_io_usb_ep_t */
	size_t                    num_eps; /* eps var has usable end points (ones that are types we support). This is the total including types we don't support yet. */
} M_io_usb_interface_t;

struct M_io_handle {
	/* Device. */
	IOUSBDeviceInterface **dev;
	M_io_t                *io;
	M_bool                 shutdown;
	M_bool                 started;    /*!< Has the handle run through the init process and had processing threads started. */
	M_event_timer_t       *disconnect_timer;
	CFRunLoopSourceRef     run_source;

	/* Metadata. */
	char                  *manufacturer;
	char                  *product;
	char                  *serial;
	M_uint16               vendorid;
	M_uint16               productid;
	M_io_usb_speed_t       speed;
	char                  *path;

	/* Event data. */
	char                   error[256];

	/* Control data. */
	IOUSBDevRequest        control_req;
	M_thread_mutex_t      *control_lock;
	M_buf_t               *control_wbuf;
	unsigned char          control_rbuf[1024];
	M_bool                 in_control;

	/* Interfaces (lazy opening). */
	M_hash_u64vp_t        *interfaces; /* key = iface num, val = M_io_usb_interface_t */

	/* Read data waiting to be read by higher layers. */
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

	if (str == NULL || request.wLenDone <= 2)
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

static size_t M_io_usb_control_max_size(M_io_handle_t *handle)
{
	switch (handle->speed) {
		case M_IO_USB_SPEED_UNKNOWN:
		case M_IO_USB_SPEED_LOW:
			return 8;
		case M_IO_USB_SPEED_FULL:
		case M_IO_USB_SPEED_HIGH:
			return 64;
		case M_IO_USB_SPEED_SUPER:
		case M_IO_USB_SPEED_SUPERPLUS:
		case M_IO_USB_SPEED_SUPERPLUSX2:
			return 512;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_usb_close_device(M_io_handle_t *handle)
{
	M_hash_u64vp_enum_t  *iface_he = NULL;
	M_io_usb_interface_t *usb_iface;

	M_hash_u64vp_enumerate(handle->interfaces, &iface_he);
	while (M_hash_u64vp_enumerate_next(handle->interfaces, iface_he, NULL, (void **)&usb_iface)) {
		/* Close the interface since we're done with it. */
		(*usb_iface->iface)->USBInterfaceClose(usb_iface->iface);
	}
	M_hash_u64vp_enumerate_free(iface_he);

	CFRunLoopRemoveSource(M_io_mac_runloop, handle->run_source, kCFRunLoopDefaultMode);

	(*handle->dev)->USBDeviceClose(handle->dev);
	handle->dev = NULL;
}

/* Close out the device. */
static void M_io_usb_disconnect_runner_step2(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer  = M_io_layer_acquire(handle->io, 0, NULL);

	(void)event;
	(void)type;
	(void)dummy_io;

	M_event_timer_remove(handle->disconnect_timer);
	handle->disconnect_timer = NULL;

	M_io_usb_close_device(handle);
	M_io_layer_release(layer);
}

/* Wait for all end points to stop. */
static void M_io_usb_disconnect_runner_step1(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t        *handle   = arg;
	M_io_layer_t         *layer    = M_io_layer_acquire(handle->io, 0, NULL);
	M_hash_u64vp_enum_t  *iface_he = NULL;
	M_io_usb_interface_t *usb_iface;
	M_bool                all_done = M_TRUE;

	(void)event;
	(void)type;
	(void)dummy_io;

	M_event_timer_remove(handle->disconnect_timer);
	handle->disconnect_timer = NULL;

	M_hash_u64vp_enumerate(handle->interfaces, &iface_he);
	while (M_hash_u64vp_enumerate_next(handle->interfaces, iface_he, NULL, (void **)&usb_iface)) {
		M_hash_u64vp_enum_t *ep_he  = NULL;
		M_io_usb_ep_t       *ep;

		M_hash_u64vp_enumerate(usb_iface->eps, &ep_he);
		while (M_hash_u64vp_enumerate_next(usb_iface->eps, ep_he, NULL, (void **)&ep)) {
			/* Check if any eps are still going. */
			M_thread_mutex_lock(ep->running_lock);
			if (ep->run) {
				all_done = M_FALSE;
				break;
			}
			M_thread_mutex_unlock(ep->running_lock);

			M_thread_mutex_lock(ep->read_lock);
			if (ep->in_read) {
				all_done = M_FALSE;
				break;
			}
			M_thread_mutex_unlock(ep->read_lock);

			M_thread_mutex_lock(ep->write_lock);
			if (ep->in_write) {
				all_done = M_FALSE;
				break;
			}
			M_thread_mutex_unlock(ep->write_lock);
		}
		M_hash_u64vp_enumerate_free(ep_he);

		if (!all_done) {
			break;
		}
	}
	M_hash_u64vp_enumerate_free(iface_he);

	if (all_done) {
		handle->disconnect_timer = M_event_timer_oneshot(M_io_get_event(handle->io), 50, M_FALSE, M_io_usb_disconnect_runner_step2, handle);
	} else {
		handle->disconnect_timer = M_event_timer_oneshot(M_io_get_event(handle->io), 50, M_FALSE, M_io_usb_disconnect_runner_step1, handle);
	}

	M_io_layer_release(layer);
}

static void M_io_usb_signal_shutdown(M_io_handle_t *handle)
{
	M_hash_u64vp_enum_t  *iface_he = NULL;
	M_io_usb_interface_t *usb_iface;

	handle->shutdown = M_TRUE;

    (*handle->dev)->USBDeviceAbortPipeZero(handle->dev);

	M_hash_u64vp_enumerate(handle->interfaces, &iface_he);
	while (M_hash_u64vp_enumerate_next(handle->interfaces, iface_he, NULL, (void **)&usb_iface)) {
		M_hash_u64vp_enum_t *ep_he  = NULL;
		M_io_usb_ep_t       *ep;

		M_hash_u64vp_enumerate(usb_iface->eps, &ep_he);
		while (M_hash_u64vp_enumerate_next(usb_iface->eps, ep_he, NULL, (void **)&ep)) {
			/* Tell our end point we're not running any longer. */
			M_thread_mutex_lock(ep->running_lock);
			ep->run = M_FALSE;
			M_thread_mutex_unlock(ep->running_lock);

			/* Abort the pipe so if we're waiting for an event it will fire and the
 			 * callback will be called. */
			(*usb_iface->iface)->AbortPipe(usb_iface->iface, (UInt8)ep->ep_num+1);
		}
		M_hash_u64vp_enumerate_free(ep_he);
	}
	M_hash_u64vp_enumerate_free(iface_he);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_usb_ep_t *M_io_usb_interface_ep_create(M_io_handle_t *handle, IOUSBInterfaceInterface **iface, size_t iface_num, size_t ep_num, M_io_usb_ep_type_t type, M_io_usb_ep_direction_t direction, size_t poll_interval, size_t max_packet_size)
{
	M_io_usb_ep_t *ep;

	ep                  = M_malloc_zero(sizeof(*ep));

	ep->run             = M_TRUE;
	ep->running_lock    = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	ep->in_read         = M_FALSE;
	ep->read_lock       = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	ep->read_buf        = M_malloc_zero(max_packet_size);

	ep->in_write        = M_FALSE;
	ep->write_lock      = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	ep->write_buf       = M_buf_create();

	ep->iface_num       = iface_num;
	ep->ep_num          = ep_num;
	ep->type            = type;
	ep->direction       = direction;
	ep->poll_interval   = poll_interval;
	ep->max_packet_size = max_packet_size;

	ep->iface           = iface;
	ep->handle          = handle;

	return ep;
}

static void M_io_usb_interface_ep_destroy(M_io_usb_ep_t *ep)
{
	if (ep == NULL)
		return;

	M_thread_mutex_destroy(ep->running_lock);

	M_thread_mutex_destroy(ep->read_lock);
	M_thread_mutex_destroy(ep->write_lock);

	M_free(ep->read_buf);
	M_buf_cancel(ep->write_buf);

	M_free(ep);
}

static M_io_usb_interface_t *M_io_usb_interface_create(IOUSBInterfaceInterface **iface, size_t iface_num)
{
	M_io_usb_interface_t *usb_iface;
	UInt8                 cnt = 0;
 
 	if (iface == NULL)
 		return NULL;

	(*iface)->GetNumEndpoints(iface, &cnt);
	if (cnt > 0)
		cnt--; /* -1 because we don't want to include the 0 control interface. */
 
 	usb_iface            = M_malloc_zero(sizeof(*usb_iface));
 	usb_iface->iface     = iface;
 	usb_iface->iface_num = iface_num;
	usb_iface->num_eps   = cnt;
	usb_iface->eps       = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void (*)(void *))M_io_usb_interface_ep_destroy);

	return usb_iface;
}

static void M_io_usb_interface_destroy(M_io_usb_interface_t *usb_iface)
{
	if (usb_iface == NULL)
		return;

	M_hash_u64vp_destroy(usb_iface->eps, M_TRUE);
	M_free(usb_iface);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_io_usb_open_interface(M_io_handle_t *handle, size_t iface_num)
{
	M_io_usb_interface_t       *usb_iface;
	M_io_usb_ep_t    *ep;
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

		ep = M_io_usb_interface_ep_create(handle, iface, iface_num, idx, type, direction, properties.bInterval, properties.wMaxPacketSize);
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

static IOReturn M_usb_check_handle_stall(IOUSBInterfaceInterface **iface, size_t ep_num, IOReturn ioret)
{
	M_bool stall = M_FALSE;

	if (ioret == kIOReturnSuccess)
		return ioret;

	if (ioret == kIOUSBPipeStalled) {
		stall = M_TRUE;
	} else if ((*iface)->GetPipeStatus(iface, (UInt8)ep_num+1) == kIOUSBPipeStalled) {
		stall = M_TRUE;
	}

	if (stall)
		ioret = (*iface)->ClearPipeStall(iface, (UInt8)ep_num+1);

	return ioret;
}

static void M_io_usb_handle_rw_error(M_io_handle_t *handle, IOReturn ioret)
{
	M_io_layer_t *layer;
	M_io_error_t  ioerr  = M_io_mac_ioreturn_to_err(ioret);
	M_event_type_t etype = M_EVENT_TYPE_ERROR;

	/* Abort means we're closing the device. */
	if (ioret == kIOReturnSuccess || ioret == kIOReturnAborted)
		return;

	layer = M_io_layer_acquire(handle->io, 0, NULL);

	if (handle->shutdown) {
		M_io_layer_release(layer);
		return;
	}

	/* Disconnect event. */
	if (ioret == kIOReturnNotOpen) {
		etype = M_EVENT_TYPE_DISCONNECTED;
		ioerr = M_IO_ERROR_DISCONNECT;
	} else {
		M_snprintf(handle->error, sizeof(handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
	}

	M_io_layer_softevent_add(layer, M_TRUE, etype, ioerr);

	// XXX: ??? M_io_usb_close_device(info->handle);

	M_io_layer_release(layer);
}

static M_bool M_io_usb_usbevent_async_check(M_io_usb_ep_t *ep, IOReturn ioret, M_bool is_read)
{
	M_bool run;

	/* Check is we're shutting down.
 	 * We need to exit the lock and re-enter
	 * due to needing to set that event status
	 * outside of the running lock. */
	M_thread_mutex_lock(ep->running_lock);
	run = ep->run;
	M_thread_mutex_unlock(ep->running_lock);

	if (!run) {
		/* No longer reading/writing. */
		if (is_read) {
			M_thread_mutex_lock(ep->read_lock);
			ep->in_read = M_FALSE;
			M_thread_mutex_unlock(ep->read_lock);
		} else {
			M_thread_mutex_lock(ep->write_lock);
			ep->in_write = M_FALSE;
			M_thread_mutex_unlock(ep->write_lock);
		}

		return M_FALSE;
	}

	/* Handle stall if we hit one. Could convert the stall error condition
 	 * to success if we cleared it. */
	ioret = M_usb_check_handle_stall(ep->iface, ep->ep_num, ioret);

	/* Unrecoverable error. */
	if (ioret != kIOReturnSuccess) {
		M_io_usb_handle_rw_error(ep->handle, ioret);
		return M_FALSE;
	}

	return M_TRUE;
}

static void M_io_usb_read_async_cb(void *refcon, IOReturn result, void *arg0)
{
	M_io_usb_ep_t *ep       = refcon;
	UInt32         data_len = (UInt32)(uintptr_t)arg0;

	/* Could have been aborted due to destroy. ep isn't good in that case. */
	if (result == kIOReturnAborted)
		return;

	if (!M_io_usb_usbevent_async_check(ep, result, M_TRUE))
		return;

	/* Handle read data. */
	if (data_len > 0) {
		/* Queue the data and issue read event. */
		M_io_layer_t *layer = M_io_layer_acquire(ep->handle->io, 0, NULL);
		M_io_usb_rdata_queue_add_read_bulkirpt(ep->handle->read_queue, ep->type, ep->iface_num, ep->ep_num, ep->read_buf, data_len);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);

		/* Clear the cached read buf data in case it's
 		 * sensitive. */
		M_mem_set(ep->read_buf, 0, data_len);
	}

	/* Wait for more data. */
    result = (*ep->iface)->ReadPipeAsync(ep->iface, (UInt8)ep->ep_num+1, ep->read_buf, (UInt32)ep->max_packet_size, M_io_usb_read_async_cb, ep);
	if (result != kIOReturnSuccess) {
		M_io_usb_handle_rw_error(ep->handle, result);
		return;
	}
}

static void M_io_usb_write_async_cb(void *refcon, IOReturn result, void *arg0)
{
	M_io_usb_ep_t *ep       = refcon;
	UInt32         data_len = (UInt32)(uintptr_t)arg0;

	/* Could have been aborted due to destroy. ep isn't good in that case. */
	if (result == kIOReturnAborted)
		return;

	if (!M_io_usb_usbevent_async_check(ep, result, M_FALSE))
		return;

	/* Handle write data. */
	M_buf_drop(ep->write_buf, data_len);

	/* No data means we can write again. */
	if (M_buf_len(ep->write_buf) == 0) {
		M_io_layer_t *layer;

		M_thread_mutex_lock(ep->write_lock);
		ep->in_write = M_FALSE;
		M_thread_mutex_unlock(ep->write_lock);

		layer = M_io_layer_acquire(ep->handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	} else {
		/* If we have more data we want to try to write it. */
		result = (*ep->iface)->WritePipeAsync(ep->iface, (UInt8)ep->ep_num+1, (void *)M_buf_peek(ep->write_buf), (UInt32)M_MIN(M_buf_len(ep->write_buf), ep->max_packet_size), M_io_usb_write_async_cb, ep);
		if (result != kIOReturnSuccess) {
			M_io_usb_handle_rw_error(ep->handle, result);
			return;
		}
	}
}

static void M_io_usb_control_async_cb(void *refcon, IOReturn result, void *arg0)
{
	M_io_handle_t *handle   = refcon;
	UInt32         data_len = (UInt32)(uintptr_t)arg0;

	/* Could have been aborted due to destroy. ep isn't good in that case. */
	if (result == kIOReturnAborted)
		return;

	if (handle->shutdown) {
		M_thread_mutex_lock(handle->control_lock);
		handle->in_control = M_FALSE;
		M_thread_mutex_unlock(handle->control_lock);
		return;
	}

	/* Actual data starts at index 3. */
	if (handle->control_req.wLenDone > 2) {
		/* Queue the data and issue read event. */
		M_io_layer_t *layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_usb_rdata_queue_add_read_control(handle->read_queue, M_IO_USB_EP_TYPE_CONTROL, handle->control_req.bRequest, handle->control_req.wValue, handle->control_req.wIndex, handle->control_rbuf+2, handle->control_req.wLenDone-2);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);

		/* Clear the cached buf data in case it's sensitive. */
		M_mem_set(handle->control_rbuf, 0, handle->control_req.wLenDone);
	}

	/* Drop any data we wrote. */
	M_buf_drop(handle->control_wbuf, data_len);

	if (M_buf_len(handle->control_wbuf) == 0) {
		M_io_layer_t *layer;

		M_thread_mutex_lock(handle->control_lock);
		handle->in_control = M_FALSE;
		M_thread_mutex_unlock(handle->control_lock);

		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	} else {
		handle->control_req.wLength = (UInt16)M_MIN(M_io_usb_control_max_size(handle), M_buf_len(handle->control_wbuf));
		handle->control_req.pData   = (void *)M_buf_peek(handle->control_wbuf);

		result = (*handle->dev)->DeviceRequestAsync(handle->dev, &handle->control_req, M_io_usb_control_async_cb, handle);
		if (result != kIOReturnSuccess) {
			M_io_usb_handle_rw_error(handle, result);
			return;
		}
	}
}

static M_bool M_io_usb_listen_interface_endpoint_int(M_io_handle_t *handle, size_t iface_num, size_t ep_num)
{
	M_io_usb_interface_t *usb_iface;
	M_io_usb_ep_t        *ep;
	IOReturn              ioret;

	if (!M_io_usb_open_interface(handle, iface_num))
		return M_FALSE;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);
	/* Bad iface_num most likely. */
	if (usb_iface == NULL)
		return M_FALSE;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);
	/* Must have been an invalid ep_num or a bad ep. */
	if (ep == NULL)
		return M_FALSE;

	/* Check if we've already listening. If we have, then we're going
 	 * to return success because we don't have anything that we need to do. */
	M_thread_mutex_lock(ep->read_lock);
	if (ep->in_read) {
		M_thread_mutex_unlock(ep->read_lock);
		return M_TRUE;
	}
	M_thread_mutex_unlock(ep->read_lock);

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

	/* Start listening. */
	if (ep->direction & M_IO_USB_EP_DIRECTION_IN) {
		ioret = (*ep->iface)->ReadPipeAsync(ep->iface, (UInt8)ep->ep_num+1, ep->read_buf, (UInt32)ep->max_packet_size, M_io_usb_read_async_cb, ep);
		if (ioret != kIOReturnSuccess) {
			M_io_usb_handle_rw_error(ep->handle, ioret);
			return M_FALSE;
		}

		M_thread_mutex_lock(ep->read_lock);
		ep->in_read = M_TRUE;
		M_thread_mutex_unlock(ep->read_lock);
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_error_t M_io_usb_write_control(M_io_handle_t *handle, const unsigned char *buf, size_t *write_len, M_hash_multi_t *mdata)
{
	IOReturn ioret;
	M_uint64 index;
	M_uint64 value;
	M_uint64 type;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_CTRL_TYPE, &type))
		return M_IO_ERROR_INVALID;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_CTRL_INDEX, &index))
		return M_IO_ERROR_INVALID;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_CTRL_VALUE, &value))
		return M_IO_ERROR_INVALID;

	M_thread_mutex_lock(handle->control_lock);

	if (handle->in_control || M_buf_len(handle->control_wbuf) > 0) {
		M_thread_mutex_unlock(handle->control_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	M_buf_add_bytes(handle->control_wbuf, buf, *write_len);
	handle->in_control = M_TRUE;

	handle->control_req.bRequest = (UInt8)type;
	handle->control_req.wValue   = (UInt8)value;
	handle->control_req.wIndex   = (UInt16)index;
	handle->control_req.wLength  = (UInt16)M_MIN(M_io_usb_control_max_size(handle), M_buf_len(handle->control_wbuf));
	handle->control_req.pData    = (void *)M_buf_peek(handle->control_wbuf);

	ioret = (*handle->dev)->DeviceRequestAsync(handle->dev, &handle->control_req, M_io_usb_control_async_cb, handle);
	if (ioret != kIOReturnSuccess) {
		handle->in_control = M_FALSE;
		M_buf_truncate(handle->control_wbuf, M_buf_len(handle->control_wbuf)-*write_len);
		*write_len = 0;
		M_thread_mutex_unlock(handle->control_lock);
		return M_io_mac_ioreturn_to_err(ioret);
	}

	M_thread_mutex_unlock(handle->control_lock);
	return M_IO_ERROR_SUCCESS;
}

static M_io_error_t M_io_usb_write_bulkirpt(M_io_handle_t *handle, const unsigned char *buf, size_t *write_len, M_hash_multi_t *mdata)
{
	M_io_usb_interface_t *usb_iface;
	M_io_usb_ep_t        *ep;
	M_uint64              iface_num;
	M_uint64              ep_num;
	IOReturn              ioret;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_IFACE_NUM, &iface_num))
		return M_IO_ERROR_INVALID;

	if (!M_hash_multi_u64_get_uint(mdata, M_IO_USB_META_KEY_EP_NUM, &ep_num))
		return M_IO_ERROR_INVALID;

	if (!M_io_usb_open_interface(handle, iface_num))
		return M_IO_ERROR_INVALID;

	usb_iface = M_hash_u64vp_get_direct(handle->interfaces, iface_num);	
	if (usb_iface == NULL)
		return M_IO_ERROR_INVALID;

	ep = M_hash_u64vp_get_direct(usb_iface->eps, ep_num);	
	if (ep == NULL)
		return M_IO_ERROR_INVALID;

	M_thread_mutex_lock(ep->write_lock);

	if (ep->in_write || M_buf_len(ep->write_buf) > 0) {
		M_thread_mutex_unlock(ep->write_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	if (buf != NULL && *write_len != 0)
		M_buf_add_bytes(ep->write_buf, buf, *write_len);

	if (M_buf_len(ep->write_buf) == 0) {
		M_thread_mutex_unlock(ep->write_lock);
		return M_IO_ERROR_SUCCESS;
	}

	ep->in_write = M_TRUE;

	ioret = (*ep->iface)->WritePipeAsync(ep->iface, (UInt8)ep->ep_num+1, (void *)M_buf_peek(ep->write_buf), (UInt32)M_MIN(M_buf_len(ep->write_buf), ep->max_packet_size), M_io_usb_write_async_cb, ep);
	if (ioret != kIOReturnSuccess) {
		ep->in_write = M_FALSE;
		M_buf_truncate(ep->write_buf, M_buf_len(ep->write_buf)-*write_len);
		*write_len = 0;
		M_thread_mutex_unlock(ep->write_lock);
		return M_io_mac_ioreturn_to_err(ioret);
	}

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

	handle->control_wbuf = M_buf_create();
	handle->control_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	handle->control_req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);

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
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

M_io_state_t M_io_usb_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->dev == NULL)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}

void M_io_usb_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	M_io_usb_signal_shutdown(handle);
	/* XXX: Need to make this a delayed destroy to
 	 * wait for all async cbs to be triggered? Or
	 * is checking for abort good enough. */
	M_io_usb_close_device(handle);

	M_free(handle->manufacturer);
	M_free(handle->product);
	M_free(handle->serial);
	M_free(handle->path);

	M_buf_cancel(handle->control_wbuf);
	M_thread_mutex_destroy(handle->control_lock);

	M_hash_u64vp_destroy(handle->interfaces, M_TRUE);

	M_llist_destroy(handle->read_queue, M_TRUE);

	if (handle->disconnect_timer)
		M_event_timer_remove(handle->disconnect_timer);

	CFRelease(handle->run_source);

	M_free(handle);
}

M_bool M_io_usb_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	/* Do nothing, all events are generated as soft events. */
	return M_FALSE;
}

M_io_error_t M_io_usb_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t      *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t     *mdata;
	M_io_usb_ep_type_t  ep_type;
	M_uint64            u64v;

	if (handle->dev == NULL || handle->shutdown)
		return M_IO_ERROR_NOTCONNECTED;

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
			return M_io_usb_write_bulkirpt(handle, buf, write_len, mdata);
		case M_IO_USB_EP_TYPE_ISOC:
		case M_IO_USB_EP_TYPE_UNKNOWN:
			return M_IO_ERROR_INVALID;
	}

	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_usb_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t    *handle = M_io_layer_get_handle(layer);
	M_hash_multi_t   *mdata;
	M_llist_node_t   *node;
	M_io_usb_rdata_t *rdata;

	if (handle->dev == NULL)
		return M_IO_ERROR_NOTCONNECTED;

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
			M_hash_multi_u64_insert_uint(mdata, M_IO_USB_META_KEY_CTRL_TYPE, rdata->ctrl_type);
			M_hash_multi_u64_insert_uint(mdata, M_IO_USB_META_KEY_CTRL_VALUE, rdata->ctrl_value);
			M_hash_multi_u64_insert_uint(mdata, M_IO_USB_META_KEY_CTRL_INDEX, rdata->ctrl_index);
			break;
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
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Tell all of our endpoints to stop reading/writing. */
	M_io_usb_signal_shutdown(handle);

	/* Wait for end points to exit. */
	handle->disconnect_timer = M_event_timer_oneshot(M_io_get_event(handle->io), 50, M_FALSE, M_io_usb_disconnect_runner_step1, handle);

	return M_FALSE;
}

void M_io_usb_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_usb_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	IOReturn       ioret;

	if (handle->dev == NULL)
		return M_FALSE;

	handle->io = io;

	if (handle->started) {
		/* Trigger connected soft event when registered with event handle */
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		/* Trigger read event if there is data present. */
		if (M_llist_len(handle->read_queue) != 0) {
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		}
		return M_TRUE;
	}

	/* Start the global macOS runloop if hasn't already been
 	 * started. The USB system uses a macOS runloop for event
	 * processing and calls our callbacks which trigger events
	 * in our event system. */
	M_io_mac_runloop_start();

	/* Register the run loop so we can receive async event callbacks. */
    ioret = (*handle->dev)->CreateDeviceAsyncEventSource(handle->dev, &handle->run_source);
	if (ioret != kIOReturnSuccess)
		return M_FALSE;
	CFRunLoopAddSource(M_io_mac_runloop, handle->run_source, kCFRunLoopDefaultMode);

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);

	handle->started = M_TRUE;
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_usb_listen_interface_endpoint(M_io_t *io, size_t iface_num, size_t ep_num)
{
	M_io_layer_t  *layer  = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_bool         ret;

	ret = M_io_usb_listen_interface_endpoint_int(handle, iface_num, ep_num);

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
	M_io_usb_ep_t *ep;
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
	M_io_usb_ep_t *ep;
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
	M_io_layer_t         *layer     = M_io_usb_get_top_usb_layer(io);
	M_io_handle_t        *handle    = M_io_layer_get_handle(layer);
	M_io_usb_interface_t *usb_iface;
	M_io_usb_ep_t        *ep;
	size_t                max       = 0;

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
