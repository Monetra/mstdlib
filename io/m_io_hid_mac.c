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

#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_hid.h>
#include "m_io_int.h"
#include "m_io_hid_int.h"
#include "m_io_mac_common.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDManager.h>

struct M_io_handle {
	IOHIDDeviceRef    device;     /*!< Device handle. */
	M_io_t           *io;         /*!< io object handle is associated with. */
	M_buf_t          *readbuf;    /*!< Reads are transferred via a buffer. */
	M_buf_t          *writebuf;   /*!< Writes are transferred via a buffer. */
	unsigned char    *report;     /*!< Buffer for storing report data that will be read from thh device. */
	size_t            report_len; /*!< Size of the report buffer. */
	M_bool            uses_reportid;
	M_bool            run;
	M_threadid_t      write_tid;
	M_thread_mutex_t *write_lock;
	M_thread_cond_t  *write_cond;
	char              error[256]; /*!< Error buffer for description of last system error. */
	M_bool            in_write;
	char             *path;
	char              manufacturer[256];
	char              product[256];
	char              serial[256];
	M_uint16          vendorid;
	M_uint16          productid;
	size_t            max_input_report_size;
	size_t            max_output_report_size;
};

static M_io_handle_t *M_io_hid_get_io_handle(M_io_t *io)
{
	M_io_layer_t  *layer;
	M_io_handle_t *handle = NULL;
	size_t         len;
	size_t         i;

	len = M_io_layer_count(io);
	for (i=len; i-->0; ) {
		layer = M_io_layer_acquire(io, i, M_IO_USB_HID_NAME);
		if (layer != NULL) {
			handle = M_io_layer_get_handle(layer);
			M_io_layer_release(layer);
			break;
		}
	}
	return handle;
}

static void M_io_hid_disassociate_runloop(M_io_handle_t *handle)
{
	if (handle->device == NULL)
		return;
	IOHIDDeviceUnscheduleFromRunLoop(handle->device, M_io_mac_runloop, kCFRunLoopDefaultMode);
}

static void M_io_hid_close_device(M_io_handle_t *handle)
{
	if (handle == NULL || handle->device == NULL)
		return;

	M_io_hid_disassociate_runloop(handle);

	handle->run = M_FALSE;
	M_thread_mutex_lock(handle->write_lock);
	M_thread_cond_broadcast(handle->write_cond);
	M_thread_mutex_unlock(handle->write_lock);
	M_thread_join(handle->write_tid, NULL);

	IOHIDDeviceClose(handle->device, kIOHIDOptionsTypeNone);
	handle->device = NULL;
}

/* DO NOT use IOHIDDeviceSetReportWithCallback. As of macOS 10.12 it returns
 * a not implemented error. As of 10.13 it will cause a kernel panic.
 * To make writes non-blocking we have a write thread that uses the
 * blocking write function. */
static void *M_io_hid_write_loop(void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	IOReturn       ioret;
	M_io_error_t   ioerr;
	const uint8_t *data;
	uint8_t        reportid;
	CFIndex        len;

	while (handle->run) {
		M_thread_mutex_lock(handle->write_lock);
		if (M_buf_len(handle->writebuf) == 0) {
			M_thread_cond_wait(handle->write_cond, handle->write_lock);
		}
		if (!handle->run || handle->device == NULL) {
			M_thread_mutex_unlock(handle->write_lock);
			break;
		}
		/* If there isn't anything to write we have nothing to
 		 * do right now. */
		if (M_buf_len(handle->writebuf) == 0) {
			M_thread_mutex_unlock(handle->write_lock);
			continue;
		}
		handle->in_write = M_TRUE;
		M_thread_mutex_unlock(handle->write_lock);

		data     = (const uint8_t *)M_buf_peek(handle->writebuf);
		len      = (CFIndex)M_buf_len(handle->writebuf);
		reportid = data[0];
		if (!handle->uses_reportid) {
			/* Don't send the report id in the data if we're not using report ids. */
			data++;
			len--;
		}
		ioret = IOHIDDeviceSetReport(handle->device, kIOHIDReportTypeOutput, reportid, data, len);
		ioerr = M_io_mac_ioreturn_to_err(ioret);
		/* Lock the layer so we can send events. */
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		if (M_io_error_is_critical(ioerr)) {
			M_snprintf(handle->error, sizeof(handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
			M_io_hid_close_device(handle);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
			M_io_layer_release(layer);

			M_thread_mutex_lock(handle->write_lock);
			handle->in_write = M_FALSE;
			M_thread_mutex_unlock(handle->write_lock);
			break;
		}
		/* clear the write buf since we've written the data. If
 		 * there was a recoverable error we don't clear the buf
		 * because the write event will trigger this to run
		 * again and the write will be retried. */
		if (ioerr == M_IO_ERROR_SUCCESS) {
			M_buf_truncate(handle->writebuf, 0);
		}

		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
		M_io_layer_release(layer);

		M_thread_mutex_lock(handle->write_lock);
		handle->in_write = M_FALSE;
		M_thread_mutex_unlock(handle->write_lock);
	}

	return NULL;
}

static M_bool M_io_hid_uses_reportid(IOHIDDeviceRef device)
{
	CFArrayRef      elements;
	IOHIDElementRef e;
	CFIndex         i;
	CFIndex         len;
	M_bool          ret = M_FALSE;

	elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
	if (elements == NULL)
		return M_FALSE;

	len = CFArrayGetCount(elements);
	for (i=0; i<len; i++) {
		e = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
		if (e == NULL) {
			continue;
		}
		if (IOHIDElementGetType(e) != kIOHIDElementTypeOutput) {
			continue;
		}
		if (IOHIDElementGetReportID(e) != 0) {
			ret = M_TRUE;
			break;
		}
	}

	CFRelease(elements);
	return ret;
}

static M_bool M_io_hid_get_prop(IOHIDDeviceRef device, char *out, size_t out_len, const char *id_s)
{
	CFStringRef  prop;
	CFStringRef  str;

	if (device == NULL || out == NULL || out_len == 0 || M_str_isempty(id_s))
		return M_FALSE;
	out[0] = '\0';

	str  = CFStringCreateWithCString(NULL, id_s, kCFStringEncodingUTF8);
	prop = IOHIDDeviceGetProperty(device, str);
	CFRelease(str);
	if (prop == NULL)
		return M_FALSE;

	CFStringGetCString(prop, out, (CFIndex)out_len, kCFStringEncodingUTF8);
	return M_TRUE;
}

static M_int32 M_io_hid_get_prop_int32(IOHIDDeviceRef device, const char *id_s)
{
	CFTypeRef   prop;
	CFStringRef str;
	M_int32     id = 0;

	if (device == NULL || M_str_isempty(id_s))
		return 0;

	str  = CFStringCreateWithCString(NULL, id_s, kCFStringEncodingUTF8);
	prop = IOHIDDeviceGetProperty(device, str);
	CFRelease(str);
	if (prop == NULL)
		return 0;

	if (CFGetTypeID(prop) != CFNumberGetTypeID())
		return 0;

	CFNumberGetValue((CFNumberRef)prop, kCFNumberSInt32Type, &id);
	return id;

}

static char *M_io_hid_get_os_path(IOHIDDeviceRef device)
{
	io_service_t  service;
	kern_return_t ret;
	io_string_t   path;

	if (device == NULL)
		return NULL;

	service = IOHIDDeviceGetService(device);
	if (service == MACH_PORT_NULL)
		return NULL;
	ret = IORegistryEntryGetPath(service, kIOServicePlane, path);
	if (ret != KERN_SUCCESS)
		return NULL;
	
	return M_strdup(path);
}

static void M_io_hid_dev_info(IOHIDDeviceRef device,
		char *manufacturer, size_t manufacturer_len,
		char *product, size_t product_len,
		char *serial, size_t serial_len,
		M_uint16 *vendorid, M_uint16 *productid,
		char **path,
		size_t *max_input_report_size, size_t *max_output_report_size)
{
	if (device == NULL)
		return;

	if (manufacturer != NULL && manufacturer_len != 0)
		M_io_hid_get_prop(device, manufacturer, manufacturer_len, kIOHIDManufacturerKey);

	if (product != NULL && product_len != 0)
		M_io_hid_get_prop(device, product, product_len, kIOHIDProductKey);

	if (serial != NULL && serial_len != 0)
		M_io_hid_get_prop(device, serial, serial_len, kIOHIDSerialNumberKey);

	if (vendorid != NULL)
		*vendorid = (M_uint16)M_io_hid_get_prop_int32(device, kIOHIDVendorIDKey);

	if (productid != NULL)
		*productid = (M_uint16)M_io_hid_get_prop_int32(device, kIOHIDProductIDKey);

	if (path != NULL)
		*path = M_io_hid_get_os_path(device);

	/* Add one to accomidate the report ID that needs to be in the buffer. */
	if (max_input_report_size != NULL)
		*max_input_report_size  = (size_t)M_io_hid_get_prop_int32(device, kIOHIDMaxInputReportSizeKey)+1;
	if (max_output_report_size != NULL)
		*max_output_report_size = (size_t)M_io_hid_get_prop_int32(device, kIOHIDMaxOutputReportSizeKey)+1;
}


static void M_io_hid_enum_device(M_io_hid_enum_t *hidenum, IOHIDDeviceRef device,
		M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serialnum)
{
	char      *path              = NULL;
	char       manufacturer[256] = { 0 };
	char       product[256]      = { 0 };
	char       serial[256]       = { 0 };
	M_uint16   vendorid          = 0;
	M_uint16   productid         = 0;

	M_io_hid_dev_info(device, manufacturer, sizeof(manufacturer), product, sizeof(product), serial, sizeof(serial),
			&vendorid, &productid, &path, NULL, NULL);

	M_io_hid_enum_add(hidenum, path, manufacturer, product, serial, vendorid, productid, 
	                  s_vendor_id, s_product_ids, s_num_product_ids, s_serialnum);

	M_free(path);
}

static void M_io_hid_disconnect_iocb(void *context, IOReturn result, void *sender)
{
	M_io_handle_t *handle = context;
	M_io_layer_t  *layer;

	(void)result;
	(void)sender;

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	M_io_layer_release(layer);
}

static void M_io_hid_read_iocb(void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength)
{
	M_io_handle_t *handle = context;
	M_io_layer_t  *layer;
	M_io_error_t   ioerr;

	(void)type;
	(void)reportID;
	(void)sender;

	layer = M_io_layer_acquire(handle->io, 0, NULL);

	ioerr = M_io_mac_ioreturn_to_err(result);
	if (M_io_error_is_critical(ioerr)) {
		M_snprintf(handle->error, sizeof(handle->error), "%s", M_io_mac_ioreturn_errormsg(result));
		M_io_hid_close_device(handle);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		return;
	}

	if (reportLength > 0)
		M_buf_add_bytes(handle->readbuf, report, (size_t)reportLength);

	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
	M_io_layer_release(layer);
}

M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial)
{
	M_io_hid_enum_t *hidenum    = M_io_hid_enum_init();
	IOHIDDeviceRef  *devices    = NULL; /* This is an arrary _not_ a single device. */
	IOHIDManagerRef  manager    = NULL;
	CFSetRef         device_set = NULL;
	IOReturn         ioret;
	size_t           len;
	size_t           i;

	manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	if (manager == NULL)
		goto done;

	/* We're not going to use the interl device matching routines.
 	 * We will determine if a device matches ourselves. */
	IOHIDManagerSetDeviceMatching(manager, NULL);

	ioret = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
	if (ioret != kIOReturnSuccess)
		goto done;

	device_set = IOHIDManagerCopyDevices(manager);
	len        = (size_t)CFSetGetCount(device_set);
	devices    = M_malloc_zero(sizeof(*devices) * len);
	CFSetGetValues(device_set, (const void **)devices);

	for (i=0; i<len; i++) {
		if (devices[i] == NULL) {
			continue;
		}
		M_io_hid_enum_device(hidenum, devices[i], vendorid, productids, num_productids, serial);
	}

done:
	if (devices != NULL)
		M_free(devices);

	if (device_set != NULL)
		CFRelease(device_set);

	if (manager != NULL)
		CFRelease(manager);

	return hidenum;
}

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr)
{
	M_io_handle_t       *handle;
	M_thread_attr_t     *tattr;
	io_registry_entry_t  entry  = MACH_PORT_NULL;
	IOHIDDeviceRef       device = NULL;
	IOReturn             ioret;
	M_bool               uses_reportid;
	size_t               report_len;

	if (M_str_isempty(devpath))
		*ioerr = M_IO_ERROR_INVALID;

	entry = IORegistryEntryFromPath(kIOMasterPortDefault, devpath);
	if (entry == MACH_PORT_NULL) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		goto err;
	}

	device = IOHIDDeviceCreate(kCFAllocatorDefault, entry);
	if (device == NULL) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	ioret = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
	if (ioret != kIOReturnSuccess) {
		*ioerr = M_IO_ERROR_NOTCONNECTED;
		goto err;
	}

	uses_reportid = M_io_hid_uses_reportid(device);
	report_len    = (size_t)M_io_hid_get_prop_int32(device, kIOHIDMaxInputReportSizeKey);
	if (report_len == 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	handle                         = M_malloc_zero(sizeof(*handle));
	handle->device                 = device;
	handle->report_len             = report_len;
	handle->report                 = M_malloc(handle->report_len);
	handle->uses_reportid          = uses_reportid;
	handle->readbuf                = M_buf_create();
	handle->writebuf               = M_buf_create();
	handle->write_lock             = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	handle->write_cond             = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	handle->run                    = M_TRUE;

	M_io_hid_dev_info(handle->device, handle->manufacturer, sizeof(handle->manufacturer),
			handle->product, sizeof(handle->product), handle->serial, sizeof(handle->serial),
			&handle->vendorid, &handle->productid, &handle->path,
			&handle->max_input_report_size, &handle->max_output_report_size);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	handle->write_tid  = M_thread_create(tattr, M_io_hid_write_loop, handle);
	M_thread_attr_destroy(tattr);

	IOObjectRelease(entry);
	return handle;

err:
	if (device != NULL)
		CFRelease(device);
	if (entry != MACH_PORT_NULL)
		IOObjectRelease(entry);

	return NULL;
}

void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size)
{
	M_io_handle_t *handle;
	size_t         my_max_input;
	size_t         my_max_output;

	if (max_input_size == NULL)
		max_input_size = &my_max_input;
	if (max_output_size == NULL)
		max_output_size = &my_max_output;

	*max_input_size  = 0;
	*max_output_size = 0;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		*max_input_size  = handle->max_input_report_size;
		*max_output_size = handle->max_output_report_size;
	}
}

const char *M_io_hid_get_manufacturer(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->manufacturer;
	}
	return NULL;
}

const char *M_io_hid_get_path(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->path;
	}
	return NULL;
}

const char *M_io_hid_get_product(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->product;
	}
	return NULL;
}

M_uint16 M_io_hid_get_productid(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->productid;
	}
	return 0;
}

M_uint16 M_io_hid_get_vendorid(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->vendorid;
	}
	return 0;
}

const char *M_io_hid_get_serial(M_io_t *io)
{
	M_io_handle_t *handle;

	handle = M_io_hid_get_io_handle(io);
	if (handle != NULL) {
		return handle->serial;
	}
	return NULL;
}

M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->device == NULL)
		return M_IO_STATE_ERROR;
	return M_IO_STATE_CONNECTED;
}

void M_io_hid_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	M_io_hid_close_device(handle);

	M_thread_mutex_destroy(handle->write_lock);
	M_thread_cond_destroy(handle->write_cond);
	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);
	M_free(handle->report);
	M_free(handle->path);

	M_free(handle);
}

M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	/* Do nothing, all events are generated as soft events. */
	return M_FALSE;
}

M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* If we can't lock then the write thread has the lock.
 	 * We can't put anything into the buffer while it's
	 * processing so return would block. */
	if (!M_thread_mutex_trylock(handle->write_lock))
		return M_IO_ERROR_WOULDBLOCK;

	if (handle->in_write) {
		M_thread_mutex_unlock(handle->write_lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	if (handle->device == NULL) {
		M_thread_mutex_unlock(handle->write_lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	if (buf != NULL && *write_len != 0)
		M_buf_add_bytes(handle->writebuf, buf, *write_len);

	if (M_buf_len(handle->writebuf) == 0) {
		M_thread_mutex_unlock(handle->write_lock);
		return M_IO_ERROR_SUCCESS;
	}

	M_thread_cond_signal(handle->write_cond);
	M_thread_mutex_unlock(handle->write_lock);
	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         offset = 0;
	size_t         len;

	if (buf == NULL || *read_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->device == NULL)
		return M_IO_ERROR_NOTCONNECTED;

	if (M_buf_len(handle->readbuf) == 0)
		return M_IO_ERROR_WOULDBLOCK;

	len = M_buf_len(handle->readbuf);
	/* Don't try to read more than we can. */
	len = M_MIN(len, *read_len);

	if (!handle->uses_reportid) {
		/* If we don't use report ids, we must prefix the read buffer with a zero. */
		buf[0] = 0;
		offset = 1;
		/* If we're maxed on the buffer we need to make room for the offset amount. */
		if (*read_len == len) {
			len -= offset;
		}
	}

	/* Copy from the read buffer into the output buffer. */
	M_mem_copy(buf+offset, M_buf_peek(handle->readbuf), len);
	/* Drop what we read. */
	M_buf_drop(handle->readbuf, len);
	/* our read total is how much we read from the readbuf plus how much we pre-filled. */
	*read_len = len+offset;
	return M_IO_ERROR_SUCCESS;
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	/* Remove the device from the run loop so additional events won't come in
 	 * They shouldn't but let's be safe. */
	M_io_hid_disassociate_runloop(handle);
	return M_TRUE;
}

void M_io_hid_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
}

M_bool M_io_hid_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	/* Start the global macOS runloop if hasn't already been
 	 * started. The HID system uses a macOS runloop for event
	 * processing and calls our callbacks which trigger events
	 * in our event system. */
	M_io_mac_runloop_start();

	if (handle->device == NULL)
		return M_FALSE;

	handle->io = io;

	/* Register event callbacks and associate the device with the macOS runloop. */
	IOHIDDeviceRegisterRemovalCallback(handle->device, M_io_hid_disconnect_iocb, handle);
	IOHIDDeviceRegisterInputReportCallback(handle->device, handle->report, (CFIndex)handle->report_len, M_io_hid_read_iocb, handle);
	IOHIDDeviceScheduleWithRunLoop(handle->device, M_io_mac_runloop, kCFRunLoopDefaultMode);

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
	return M_TRUE;
}
