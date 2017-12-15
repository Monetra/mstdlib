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

#include "m_io_mfi_int.h"
#include "m_io_mfi_ea.h"
#import <ExternalAccessory/ExternalAccessory.h>

#define M_IO_MFI_NAME "MFI"

typedef struct {
	char *name;
	char *protocol;
	char *serialnum;
} M_io_mfi_enum_device_t;

struct M_io_mfi_enum {
	M_list_t *devices; /* list of M_io_mfi_enum_device_t */
};

static void M_io_mfi_enum_free_device(void *arg)
{
	M_io_mfi_enum_device_t *device = arg;
	M_free(device->name);
	M_free(device->protocol);
	M_free(device->serialnum);
	M_free(device);
}

static void M_io_mfi_enum_add(M_io_mfi_enum_t *mfienum, const char *name, const char *protocol, const char *serialnum)
{
	M_io_mfi_enum_device_t *device;

	if (mfienum == NULL || M_str_isempty(name) || M_str_isempty(protocol) || M_str_isempty(serialnum))
		return;

	device            = M_malloc_zero(sizeof(*device));
	device->name      = M_strdup(name);
	device->protocol  = M_strdup(protocol);
	device->serialnum = M_strdup(serialnum);

	M_list_insert(mfienum->devices, device);
}

static M_io_mfi_enum_t *M_io_mfi_enum_init(void)
{
	M_io_mfi_enum_t  *mfienum  = M_malloc_zero(sizeof(*mfienum));
	struct M_list_callbacks listcbs = {
		NULL,
		NULL,
		NULL,
		M_io_mfi_enum_free_device
	};
	mfienum->devices = M_list_create(&listcbs, M_LIST_NONE);
	return mfienum;
}

M_io_mfi_enum_t *M_io_mfi_enum(void)
{
	NSArray<EAAccessory *> *accs;
	EAAccessory            *acc;
	M_io_mfi_enum_t *mfienum = M_io_mfi_enum_init();

	accs = [[EAAccessoryManager sharedAccessoryManager] connectedAccessories];
	for (acc in accs) {
		for (NSString *k in acc.protocolStrings) {
			const char *name      = [acc.name UTF8String];
			const char *serialnum = [acc.serialNumber UTF8String];
			const char *protocol  = [k UTF8String];
			M_io_mfi_enum_add(mfienum, name, protocol, serialnum);
		}
	}

	return mfienum;
}

void M_io_mfi_enum_destroy(M_io_mfi_enum_t *mfienum)
{
	if (mfienum == NULL)
		return;
	M_list_destroy(mfienum->devices, M_TRUE);
	M_free(mfienum);
}

size_t M_io_mfi_enum_count(const M_io_mfi_enum_t *mfienum)
{
	if (mfienum == NULL)
		return 0;
	return M_list_len(mfienum->devices);
}


const char *M_io_mfi_enum_name(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	const M_io_mfi_enum_device_t *device;
	if (mfienum == NULL)
		return NULL;
	device = M_list_at(mfienum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->name;
}

const char *M_io_mfi_enum_protocol(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	const M_io_mfi_enum_device_t *device;
	if (mfienum == NULL)
		return NULL;
	device = M_list_at(mfienum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->protocol;
}

const char *M_io_mfi_enum_serialnum(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	const M_io_mfi_enum_device_t *device;
	if (mfienum == NULL)
		return NULL;
	device = M_list_at(mfienum->devices, idx);
	if (device == NULL)
		return NULL;
	return device->serialnum;
}

static M_io_handle_t *M_io_mfi_open(const char *protocol, const char *serialnum, M_io_error_t *ioerr)
{
	M_io_handle_t *handle      = NULL;
	M_io_mfi_ea   *ea          = nil;
	NSString      *myserialnum = nil;

	*ioerr = M_IO_ERROR_SUCCESS;

	if (protocol == NULL) {
		*ioerr = M_IO_ERROR_INVALID;
		return NULL;
	}

	if (serialnum != NULL)
		myserialnum = [NSString stringWithUTF8String:serialnum];

	/* All pre-validations are good here.  We're not going to start the actual connection yet as that
	 * is a blocking operation.  All of the above should have been non-blocking. */
	handle            = M_malloc_zero(sizeof(*handle));
	handle->readbuf   = M_buf_create();
	handle->writebuf  = M_buf_create();
	M_snprintf(handle->error, sizeof(handle->error), "Error not set");

	ea = [M_io_mfi_ea m_io_mfi_ea:[NSString stringWithUTF8String:protocol] handle:handle serialnum:myserialnum];
	if (ea == nil) {
		*ioerr = M_IO_ERROR_NOTFOUND;
		M_buf_cancel(handle->readbuf);
		M_buf_cancel(handle->writebuf);
		M_free(handle);
		return NULL;
	}

	handle->ea = (__bridge_retained CFTypeRef)ea;

	return handle;
}

static M_bool M_io_mfi_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

static M_io_state_t M_io_mfi_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return handle->state;
}

static void M_io_mfi_close(M_io_handle_t *handle, M_io_state_t state)
{
	M_io_mfi_ea *ea;

	if (handle->ea != nil) {
		handle->state = state;
		ea = (__bridge_transfer M_io_mfi_ea *)handle->ea;
		[ea close];
		ea            = nil;
		handle->ea    = nil;
	}
}

static void M_io_mfi_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	M_io_mfi_close(handle, M_IO_STATE_DISCONNECTED);

	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);
	M_free(handle);
}

static M_bool M_io_mfi_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	(void)layer;

	if (*type == M_EVENT_TYPE_CONNECTED || *type == M_EVENT_TYPE_ERROR || *type == M_EVENT_TYPE_DISCONNECTED) {
		/* Disable timer */
		if (handle->timer != NULL) {
			M_event_timer_remove(handle->timer);
			handle->timer = NULL;
		}
	}
	/* Do nothing, all events are generated as soft events and we don't have anything to process */
	return M_FALSE;
}

static M_io_error_t M_io_mfi_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);

	if (buf == NULL || *write_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	M_buf_add_bytes(handle->writebuf, buf, *write_len);

	/* Dispatch an attempt to write if there was no previously enqueued data. */
	if (M_buf_len(handle->writebuf) == *write_len) {
		[(__bridge M_io_mfi_ea *)handle->ea write_data_buffered];
	}

	return M_IO_ERROR_SUCCESS;
}

static M_io_error_t M_io_mfi_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (buf == NULL || *read_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	if (M_buf_len(handle->readbuf) == 0)
		return M_IO_ERROR_WOULDBLOCK;

	if (*read_len > M_buf_len(handle->readbuf))
		*read_len = M_buf_len(handle->readbuf);

	M_mem_copy(buf, M_buf_peek(handle->readbuf), *read_len);
	M_buf_drop(handle->readbuf, *read_len);
	return M_IO_ERROR_SUCCESS;
}

static void M_io_mfi_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_layer_t  *layer;
	(void)dummy_io;
	(void)type;
	(void)event;

	/* Lock! */
	layer = M_io_layer_acquire(handle->io, 0, NULL);

	handle->timer = NULL;

	if (handle->state == M_IO_STATE_CONNECTING) {

		/* Tell the thread to shutdown by closing the socket on our end */
		M_io_mfi_close(handle, M_IO_STATE_ERROR);

		M_snprintf(handle->error, sizeof(handle->error), "Timeout waiting on connect");
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
	} else if (handle->state == M_IO_STATE_DISCONNECTING) {
		M_io_mfi_close(handle, M_IO_STATE_DISCONNECTED);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	} else {
		/* Shouldn't ever happen */
	}

	M_io_layer_release(layer);
}

static M_bool M_io_mfi_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_IO_STATE_CONNECTED && handle->state != M_IO_STATE_DISCONNECTING)
		return M_TRUE;

	if (M_buf_len(handle->writebuf)) {
		/* Data pending, delay 100ms */
		handle->timer = M_event_timer_oneshot(M_io_get_event(M_io_layer_get_io(layer)), 100, M_TRUE, M_io_mfi_timer_cb, handle);
		handle->state = M_IO_STATE_DISCONNECTING;
		return M_FALSE;
	}

	M_io_mfi_close(handle, M_IO_STATE_DISCONNECTED);
	return M_TRUE;
}

static void M_io_mfi_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Only thing we can do is disable a timer if there was one */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

}

static M_bool M_io_mfi_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_io_t          *io     = M_io_layer_get_io(layer);
	M_event_t       *event  = M_io_get_event(io);

	switch (handle->state) {
		case M_IO_STATE_INIT:
			handle->state = M_IO_STATE_CONNECTING;
			handle->io    = io;
			[(__bridge M_io_mfi_ea *)handle->ea connect];

			/* Fall-thru */
		case M_IO_STATE_CONNECTING:
			/* start timer to time out operation */
			handle->timer = M_event_timer_oneshot(event, 10000, M_TRUE, M_io_mfi_timer_cb, handle);
			break;
		case M_IO_STATE_CONNECTED:
			/* Trigger connected soft event when registered with event handle */
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);

			/* If there is data in the read buffer, signal there is data to be read as well */
			if (M_buf_len(handle->readbuf)) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}
			break;
		default:
			/* Any other state is an error */
			return M_FALSE;
	}

	return M_TRUE;
}

M_io_error_t M_io_mfi_create(M_io_t **io_out, const char *protocol, const char *serialnum)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL || M_str_isempty(protocol))
		return M_IO_ERROR_INVALID;

	handle    = M_io_mfi_open(protocol, serialnum, &err);
	if (handle == NULL)
		return err;

	*io_out   = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_mfi_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_mfi_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_mfi_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_mfi_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_mfi_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_mfi_destroy_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_mfi_disconnect_cb);
	M_io_callbacks_reg_state(callbacks, M_io_mfi_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_mfi_errormsg_cb);
	M_io_layer_add(*io_out, M_IO_MFI_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}
