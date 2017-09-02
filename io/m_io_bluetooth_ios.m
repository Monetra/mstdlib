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
#include <mstdlib/io/m_io_bluetooth.h>
#include "m_io_bluetooth_int.h"
#include "m_io_bluetooth_ios.h"
#import <ExternalAccessory/ExternalAccessory.h>


M_io_bluetooth_enum_t *M_io_bluetooth_enum(void)
{
	NSArray<EAAccessory *> *accs;
	EAAccessory            *acc;
	M_io_bluetooth_enum_t *btenum = M_io_bluetooth_enum_init();

	accs = [[EAAccessoryManager sharedAccessoryManager] connectedAccessories];
	for (acc in accs) {
		for (NSString *k in acc.protocolStrings) {
			const char *name      = [acc.name UTF8String];
			const char *serialnum = [acc.serialNumber UTF8String];
			const char *protocol  = [k UTF8String];
			M_io_bluetooth_enum_add(btenum, name, protocol, serialnum);
		}
	}

	return btenum;
}


M_io_handle_t *M_io_bluetooth_open(const char *mac, const char *uuid, M_io_error_t *ioerr)
{
	M_io_handle_t         *handle     = NULL;
	M_io_bluetooth_ios_ea *ea         = nil;
	NSString              *myuuid     = nil;

	*ioerr = M_IO_ERROR_SUCCESS;

	if (mac == NULL) {
		*ioerr = M_IO_ERROR_INVALID;
		return NULL;
	}

	if (uuid != NULL)
		myuuid = [NSString stringWithUTF8String:uuid];

	/* All pre-validations are good here.  We're not going to start the actual connection yet as that
	 * is a blocking operation.  All of the above should have been non-blocking. */
	handle            = M_malloc_zero(sizeof(*handle));
	handle->readbuf   = M_buf_create();
	handle->writebuf  = M_buf_create();
	M_snprintf(handle->error, sizeof(handle->error), "Error not set");

	ea = [M_io_bluetooth_ios_ea m_io_bluetooth_ios_ea:[NSString stringWithUTF8String:mac] handle:handle serialnum:myuuid];
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


M_bool M_io_bluetooth_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}


M_io_state_t M_io_bluetooth_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return handle->state;
}


static void M_io_bluetooth_close(M_io_handle_t *handle, M_io_state_t state)
{
	M_io_bluetooth_ios_ea *ea;

	if (handle->ea != nil) {
		handle->state = state;
		ea = (__bridge_transfer M_io_bluetooth_ios_ea *)handle->ea;
		[ea close];
		ea            = nil;
		handle->ea    = nil;
	}
}


void M_io_bluetooth_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	M_io_bluetooth_close(handle, M_IO_STATE_DISCONNECTED);

	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);
	M_free(handle);
}


M_bool M_io_bluetooth_process_cb(M_io_layer_t *layer, M_event_type_t *type)
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


M_io_error_t M_io_bluetooth_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);

	if (buf == NULL || *write_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_STATE_CONNECTED)
		return M_IO_ERROR_INVALID;

	M_buf_add_bytes(handle->writebuf, buf, *write_len);

	/* Dispatch an attempt to write if there was no previously enqueued data. */
	if (M_buf_len(handle->writebuf) == *write_len) {
		[(__bridge M_io_bluetooth_ios_ea *)handle->ea write_data_buffered];
	}

	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_bluetooth_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
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



static void M_io_bluetooth_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
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
		M_io_bluetooth_close(handle, M_IO_STATE_ERROR);

		M_snprintf(handle->error, sizeof(handle->error), "Timeout waiting on connect");
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
	} else if (handle->state == M_IO_STATE_DISCONNECTING) {
		M_io_bluetooth_close(handle, M_IO_STATE_DISCONNECTED);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
	} else {
		/* Shouldn't ever happen */
	}

	M_io_layer_release(layer);
}

M_bool M_io_bluetooth_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_IO_STATE_CONNECTED && handle->state != M_IO_STATE_DISCONNECTING)
		return M_TRUE;

	if (M_buf_len(handle->writebuf)) {
		/* Data pending, delay 100ms */
		handle->timer = M_event_timer_oneshot(M_io_get_event(M_io_layer_get_io(layer)), 100, M_TRUE, M_io_bluetooth_timer_cb, handle);
		handle->state = M_IO_STATE_DISCONNECTING;
		return M_FALSE;
	}

	M_io_bluetooth_close(handle, M_IO_STATE_DISCONNECTED);
	return M_TRUE;
}


void M_io_bluetooth_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Only thing we can do is disable a timer if there was one */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

}


M_bool M_io_bluetooth_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_io_t          *io     = M_io_layer_get_io(layer);
	M_event_t       *event  = M_io_get_event(io);

	switch (handle->state) {
		case M_IO_STATE_INIT:
			handle->state = M_IO_STATE_CONNECTING;
			handle->io    = io;
			[(__bridge M_io_bluetooth_ios_ea *)handle->ea connect];

			/* Fall-thru */
		case M_IO_STATE_CONNECTING:
			/* start timer to time out operation */
			handle->timer = M_event_timer_oneshot(event, 10000, M_TRUE, M_io_bluetooth_timer_cb, handle);
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

