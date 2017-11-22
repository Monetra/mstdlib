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
#include "m_io_hid_int.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDManager.h>

struct M_io_handle {
	IOHIDDeviceRef  device;     /*!< Device handle. */
	CFRunLoopRef    runloop;    /*!< RunLoop the device is using for async events. */
	M_io_t         *io;         /*!< io object handle is associated with. */
	M_buf_t        *readbuf;    /*!< Reads are transferred via a buffer. */
	M_buf_t        *writebuf;   /*!< Writes are transferred via a buffer. */
	unsigned char  *report;     /*!< Buffer for storing report data that will be read from thh device. */
	size_t          report_len; /*!< Size of the report buffer. */
	char            error[256]; /*!< Error buffer for description of last system error. */
};

static void *M_io_hid_runloop_runner(void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	handle->runloop = CFRunLoopGetCurrent();
	IOHIDDeviceScheduleWithRunLoop(handle->device, handle->runloop, kCFRunLoopDefaultMode);
	M_io_layer_release(layer);

	CFRunLoopRun();

	return NULL;
}

static M_io_error_t M_io_hid_ioreturn_to_err(IOReturn result)
{
	switch (result) {
		case kIOReturnSuccess:
			return M_IO_ERROR_SUCCESS;
		case kIOReturnNoMemory:
		case kIOReturnNoResources:
			return M_IO_ERROR_NOSYSRESOURCES;
		case kIOReturnNoDevice:
		case kIOReturnNotFound:
			return M_IO_ERROR_NOTFOUND;
		case kIOReturnNotPrivileged:
		case kIOReturnNotPermitted:
			return M_IO_ERROR_NOTPERM;
		case kIOReturnBadArgument:
			return M_IO_ERROR_INVALID;
		case kIOReturnLockedRead:
		case kIOReturnLockedWrite:
		case kIOReturnBusy:
			return M_IO_ERROR_WOULDBLOCK;
		case kIOReturnNotOpen:
			return M_IO_ERROR_NOTCONNECTED;
		case kIOReturnTimeout:
			return M_IO_ERROR_TIMEDOUT;
		case kIOReturnAborted:
			return M_IO_ERROR_CONNABORTED;
		default:
			return M_IO_ERROR_ERROR;
	}
	return M_IO_ERROR_ERROR;
}

static const char *M_io_hid_sys_errormsg(IOReturn result)
{
	switch (result) {
		case kIOReturnSuccess:
			return "OK";
		case kIOReturnError:
			return "general error";
		case kIOReturnNoMemory:
			return "can't allocate memory";
		case kIOReturnNoResources:
			return "resource shortage";
		case kIOReturnIPCError:
			return "error during IPC";
		case kIOReturnNoDevice:
			return "no such device";
		case kIOReturnNotPrivileged:
			return "privilege violation ";
		case kIOReturnBadArgument:
			return "invalid argument";
		case kIOReturnLockedRead:
			return "device read locked";
		case kIOReturnLockedWrite:
			return "device write locked";
		case kIOReturnExclusiveAccess:
			return "exclusive access and";
		case kIOReturnBadMessageID:
			return "sent/received messages";
		case kIOReturnUnsupported:
			return "unsupported function ";
		case kIOReturnVMError:
			return "misc. VM failure";
		case kIOReturnInternalError:
			return "internal error";
		case kIOReturnIOError:
			return "General I/O error";
		case kIOReturnCannotLock:
			return "can't acquire lock";
		case kIOReturnNotOpen:
			return "device not open ";
		case kIOReturnNotReadable:
			return "read not supported";
		case kIOReturnNotWritable:
			return "write not supported";
		case kIOReturnNotAligned:
			return "alignment error";
		case kIOReturnBadMedia:
			return "Media Error";
		case kIOReturnStillOpen:
			return "device(s) still open ";
		case kIOReturnRLDError:
			return "rld failure";
		case kIOReturnDMAError:
			return "DMA failure";
		case kIOReturnBusy:
			return "Device Busy";
		case kIOReturnTimeout:
			return "I/O Timeout";
		case kIOReturnOffline:
			return "device offline";
		case kIOReturnNotReady:
			return "not ready";
		case kIOReturnNotAttached:
			return "device not attached";
		case kIOReturnNoChannels:
			return "no DMA channels left";
		case kIOReturnNoSpace:
			return "no space for data";
		case kIOReturnPortExists:
			return "port already exists";
		case kIOReturnCannotWire:
			return "can't wire down ";
		case kIOReturnNoInterrupt:
			return "no interrupt attached";
		case kIOReturnNoFrames:
			return "no DMA frames enqueued";
		case kIOReturnMessageTooLarge:
			return "oversized msg received";
		case kIOReturnNotPermitted:
			return "not permitted";
		case kIOReturnNoPower:
			return "no power to device";
		case kIOReturnNoMedia:
			return "media not present";
		case kIOReturnUnformattedMedia:
			return "media not formatted";
		case kIOReturnUnsupportedMode:
			return "no such mode";
		case kIOReturnUnderrun:
			return "data underrun ";
		case kIOReturnOverrun:
			return "data overrun ";
		case kIOReturnDeviceError:
			return "the device is not working properly!";
		case kIOReturnNoCompletion:
			return "a completion routine is required";
		case kIOReturnAborted:
			return "operation aborted";
		case kIOReturnNoBandwidth:
			return "bus bandwidth would be exceeded";
		case kIOReturnNotResponding:
			return "device not responding";
		case kIOReturnIsoTooOld:
			return "isochronous I/O request for distant past!";
		case kIOReturnIsoTooNew:
			return "isochronous I/O request for distant future";
		case kIOReturnNotFound:
			return "data was not found";
		case kIOReturnInvalid:
			return "should never be seen ";
	}

	return "Error";
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

static char *M_io_hid_get_path(IOHIDDeviceRef device)
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

static void M_io_hid_enum_device(M_io_hid_enum_t *hidenum, IOHIDDeviceRef device,
		M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serialnum)
{
	char      *path;
	char       manufacturer[256] = { 0 };
	char       product[256]      = { 0 };
	char       serial[256]       = { 0 };
	M_uint16   vendorid;
	M_uint16   productid;

	M_io_hid_get_prop(device, manufacturer, sizeof(manufacturer), kIOHIDManufacturerKey);
	M_io_hid_get_prop(device, product, sizeof(product), kIOHIDProductKey);
	M_io_hid_get_prop(device, serial, sizeof(serial), kIOHIDSerialNumberKey);
	vendorid  = (M_uint16)M_io_hid_get_prop_int32(device, kIOHIDVendorIDKey);
	productid = (M_uint16)M_io_hid_get_prop_int32(device, kIOHIDProductIDKey);
	path      = M_io_hid_get_path(device);

	M_io_hid_enum_add(hidenum, path, manufacturer, product, serial, vendorid, productid, 
	                  s_vendor_id, s_product_ids, s_num_product_ids, s_serialnum);

	M_free(path);
}

static void M_io_hid_stop_runloop(M_io_handle_t *handle)
{
	if (handle->runloop == NULL)
		return;

	IOHIDDeviceUnscheduleFromRunLoop(handle->device, handle->runloop, kCFRunLoopDefaultMode);
	CFRunLoopStop(handle->runloop);

	handle->runloop = NULL;
}

static void M_io_hid_close_device(M_io_handle_t *handle)
{
	if (handle == NULL)
		return;

	if (handle->device == NULL)
		return;

	M_io_hid_stop_runloop(handle);
	IOHIDDeviceClose(handle->device, kIOHIDOptionsTypeNone);
	handle->device = NULL;
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

	ioerr = M_io_hid_ioreturn_to_err(result);
	if (M_io_error_is_critical(ioerr)) {
		M_snprintf(handle->error, sizeof(handle->error), "%s", M_io_hid_sys_errormsg(result));
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
	io_registry_entry_t  entry  = MACH_PORT_NULL;
	IOHIDDeviceRef       device = NULL;
	IOReturn             ioret;
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

	report_len = (size_t)M_io_hid_get_prop_int32(device, kIOHIDMaxInputReportSizeKey);
	if (report_len == 0) {
		*ioerr = M_IO_ERROR_ERROR;
		goto err;
	}

	handle             = M_malloc_zero(sizeof(*handle));
	handle->device     = device;
	handle->report_len = report_len;
	handle->report     = M_malloc(handle->report_len);
	handle->readbuf    = M_buf_create();
	handle->writebuf   = M_buf_create();

	IOObjectRelease(entry);
	return handle;

err:
	if (device != NULL)
		CFRelease(device);
	if (entry != MACH_PORT_NULL)
		IOObjectRelease(entry);

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
	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);
	M_free(handle->report);

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
	IOReturn       ioret;
	M_io_error_t   ioerr;

	if (buf != NULL && *write_len != 0)
		M_buf_add_bytes(handle->writebuf, buf, *write_len);

	if (M_buf_len(handle->writebuf) == 0)
		return M_IO_ERROR_SUCCESS;

	ioret = IOHIDDeviceSetReport(handle->device, kIOHIDReportTypeOutput, 0, (const uint8_t *)M_buf_peek(handle->writebuf), (CFIndex)M_buf_len(handle->writebuf));
	ioerr = M_io_hid_ioreturn_to_err(ioret);
	if (M_io_error_is_critical(ioerr)) {
		M_snprintf(handle->error, sizeof(handle->error), "%s", M_io_hid_sys_errormsg(ioret));
		return ioerr;
	}
	if (ioerr == M_IO_ERROR_SUCCESS) {
		*write_len = M_buf_len(handle->writebuf);
		M_buf_truncate(handle->writebuf, 0);
	}
	
	return ioerr;
}

M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (buf == NULL || *read_len == 0)
		return M_IO_ERROR_INVALID;

	if (M_buf_len(handle->readbuf) == 0)
		return M_IO_ERROR_WOULDBLOCK;

	if (*read_len > M_buf_len(handle->readbuf))
		*read_len = M_buf_len(handle->readbuf);

	M_mem_copy(buf, M_buf_peek(handle->readbuf), *read_len);
	M_buf_drop(handle->readbuf, *read_len);
	return M_IO_ERROR_SUCCESS;
}

M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	/* Remove the device from the run loop so additional events won't come in
 	 * They shouldn't be letes be safe. */
	M_io_hid_stop_runloop(handle);
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

	if (handle->device == NULL)
		return M_FALSE;

	handle->io = io;

	/* Add the device to the device loop and register callbacks. */
	IOHIDDeviceRegisterRemovalCallback(handle->device, M_io_hid_disconnect_iocb, handle);
	IOHIDDeviceRegisterInputReportCallback(handle->device, handle->report, (CFIndex)handle->report_len, M_io_hid_read_iocb, handle);

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
	M_thread_create(NULL, M_io_hid_runloop_runner, handle);
	return M_TRUE;
}
