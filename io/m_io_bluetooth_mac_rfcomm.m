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

#include <mstdlib/mstdlib_thread.h>
#include "m_io_mac_common.h"
#include "m_io_bluetooth_int.h"
#include "m_io_bluetooth_mac.h"
#include "m_io_bluetooth_mac_rfcomm.h"
#include "m_io_int.h" /* For M_io_layer_at() */

@implementation M_io_bluetooth_mac_rfcomm

M_io_handle_t            *_handle  = NULL;
IOBluetoothDevice        *_dev     = nil;
IOBluetoothRFCOMMChannel *_channel = nil;
BluetoothRFCOMMChannelID  _cid;
M_uint16                  _mtu     = 0;

+ (id)m_io_bluetooth_mac_rfcomm:(NSString *)mac uuid:(NSString *)uuid handle:(M_io_handle_t *)handle
{
	return [[M_io_bluetooth_mac_rfcomm alloc] init:mac uuid:uuid handle:handle];
}

- (id)init: (NSString *)mac uuid:(NSString *)uuid handle:(M_io_handle_t *)handle
{
	const char *cuuid;
	M_bool      found = M_FALSE;

	if (mac == nil || handle == nil)
		return nil;

	self = [super init];
	if (!self)
		return nil;

	_handle = handle;

	if (uuid == nil)
		uuid = [NSString stringWithUTF8String:M_IO_BLUETOOTH_RFCOMM_UUID];
	cuuid = [uuid UTF8String];
	
	/* Create the device. */
	_dev = [IOBluetoothDevice deviceWithAddressString:mac];
	if (_dev == nil)
		return nil;

	/* Get the rfcomm channel id for the uuid.
	 * We need to go through all of the services and match against
	 * the uuid until we find the right one, or run out. */
	NSArray *srs = _dev.services;
	for (IOBluetoothSDPServiceRecord *sr in srs) {
		/* Skip anything that's not an rfcomm service. */
		if ([sr getRFCOMMChannelID:&_cid] != kIOReturnSuccess) {
			continue;

		}

		if ([sr getRFCOMMChannelID:&_cid] != kIOReturnSuccess) {
			continue;
		}

		NSDictionary *di = sr.attributes;
		for (NSString *k in di) {
			IOBluetoothSDPDataElement *e = [di objectForKey:k];
			NSArray *iea = [e getArrayValue];

			for (IOBluetoothSDPDataElement *ie in iea) {
				if ([ie getTypeDescriptor] != kBluetoothSDPDataElementTypeUUID) {
					continue;
				}
				char suuid[64];
				M_io_bluetooth_mac_uuid_to_str([ie getUUIDValue], suuid, sizeof(suuid));

				if (M_str_caseeq(cuuid, suuid)) {
					found = M_TRUE;
					break;
				}
			}

			if (found) {
				break;
			}
		}
	}

	if (!found)
		return nil;

	return self;
}

- (void)dealloc
{
	[self close];
}

- (BOOL)connect
{
	/* We need to get the result of the connect here to know if the whole process is being started
	 * or if the caller needs to fail and clean up. The connect call needs to happen in our global
	 * macOS io runloop because the bluetooth interface needs to be on a runloop. We can't assign
	 * the object to a runloop like we can with HID; instead it's bound to which ever thread connect
	 * was called on.
	 *
	 * We use CFRunLoopPerformBlock to run the connect function (within a block) on the global runloop.
	 * ioret is tagged as a __block variable so the block can modify the value for use outside of the
	 * block.
	 *
	 * A semaphore is used to wait for the connect call to set the return value.
	 *
	 * CFRunLoopPerformBlock queues the block but it won't run until an event triggers the runloop
	 * to wake up. We use CFRunLoopWakeUp to force it to run pending blocks. Such as our connect
	 * block.
	 */

	__block IOReturn     ioret = kIOReturnInvalid;
	dispatch_semaphore_t sem   = dispatch_semaphore_create(0);

	M_io_mac_runloop_start();

	CFRunLoopPerformBlock(M_io_mac_runloop, kCFRunLoopCommonModes, ^{
		/* Use a temporary channel for storage because openRFCOMMChannelAsync takes an __auto_release paraemter.
		 * We are going to assign it to a __strong parameter to prevent it from being cleaned up from under
		 * us but we can't do that directly. */
		IOBluetoothRFCOMMChannel *channel = nil;
		ioret    = [_dev openRFCOMMChannelAsync:&channel withChannelID:_cid delegate:self];
		_channel = channel;
		dispatch_semaphore_signal(sem);
	});
	CFRunLoopWakeUp(M_io_mac_runloop);
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

	if (ioret != kIOReturnSuccess)
		return NO;

	return YES;
}

- (void)close_int
{
	if (_channel == nil)
		return;

	
	[_channel setDelegate:nil];
	[_channel closeChannel];
	_channel = nil;
}

- (void)close
{
	M_io_layer_t *layer;

	if (_channel == nil)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	[self close_int];
	M_io_layer_release(layer);
}

- (void)_write
{
	M_io_layer_t *layer;
	M_io_error_t  ioerr;
	IOReturn      ioret;
	size_t        len;
	M_uint16      send_len;

	if (_channel == nil)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);

	len = M_buf_len(_handle->writebuf);
	if (len > _mtu) {
		send_len = _mtu;
	} else {
		send_len = (M_uint16)len;
	}
	
	ioret = [_channel writeAsync:M_buf_peek(_handle->writebuf) length:send_len refcon:NULL];
	ioerr = M_io_mac_ioreturn_to_err(ioret);
	if (ioerr == M_IO_ERROR_SUCCESS) {
		_handle->wrote_len = send_len;
		_handle->can_write = M_FALSE;
	} else if (M_io_error_is_critical(ioerr)) {
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		[self close_int];
	}

	M_io_layer_release(layer);
}

- (void)write_data_buffered
{
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		[self _write];
	});
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
	M_io_layer_t *layer;

	(void)rfcommChannel;
	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	_handle->state = M_IO_STATE_DISCONNECTED;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);

	M_io_layer_release(layer);
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	if (dataLength == 0)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	M_buf_add_bytes(_handle->readbuf, (unsigned char *)dataPointer, dataLength);
	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_READ);
	M_io_layer_release(layer);
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)error
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	if (error == kIOReturnSuccess) {
		/* need to know how much data the device is capable of transferring in one request. */
		_mtu               = [_channel getMTU];
		/* Let everyone know we've connected. */
		_handle->state     = M_IO_STATE_CONNECTED;
		_handle->can_write = M_TRUE;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED);
	} else {
		_handle->state = M_IO_STATE_ERROR;
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(error));
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR);
		[self close_int];
	}
	M_io_layer_release(layer);
}

- (void)rfcommChannelQueueSpaceAvailable:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	if (_handle->state == M_IO_STATE_CONNECTED)
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE);
	M_io_layer_release(layer);
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel refcon:(void *)refcon status:(IOReturn)error
{
	M_io_layer_t *layer;

	(void)rfcommChannel;
	(void)refcon;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	if (error == kIOReturnSuccess) {
		/* Clear the data from the buffer that was written. */
		M_buf_drop(_handle->writebuf, _handle->wrote_len);

		if (M_buf_len(_handle->writebuf) > 0) {
			/* We have more data avaliable to write because more data was
			 * buffered than could be written. Write the next block. */
			[self _write];
		} else {
			/* All pending data has been written. Let everyone know they can write again. */
			_handle->can_write = M_TRUE;
			M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE);
		}
	} else {
		_handle->state = M_IO_STATE_ERROR;
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(error));
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR);
		[self close_int];
	}
	M_io_layer_release(layer);
}

@end
