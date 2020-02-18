/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

+ (id)m_io_bluetooth_mac_rfcomm:(NSString *)mac uuid:(NSString *)uuid 
{
	return [[M_io_bluetooth_mac_rfcomm alloc] init:mac uuid:uuid];
}

- (id)init:(NSString *)mac uuid:(NSString *)uuid
{
	const char *cuuid;
	M_bool      found = M_FALSE;

	if (mac == nil)
		return nil;

	self = [super init];
	if (!self)
		return nil;

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
		/* Get the channel id for the service. If we can't get the
		 * channel id, that means it's not an rfcomm service. In
		 * which case, skip it. */
		if ([sr getRFCOMMChannelID:&_cid] != kIOReturnSuccess) {
			continue;
		}

		/* Get the uuid for the service and check if it's ours. */
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

	if (!found) {
		[_dev closeConnection];
		_dev = nil;
		return nil;
	}

	return self;
}

- (void)dealloc
{
	[self close];
	_dev = nil;
}

- (void)connect:(M_io_handle_t *)handle
{
	if (handle == NULL)
		return;
	_handle = handle;

	/* The connect call needs to happen in our global macOS io runloop because
	 * the bluetooth interface needs to be on a runloop. We can't assign the
	 * object to a runloop like we can with HID; instead it's bound to which
	 * ever thread connect was called on.
	 *
	 * We use CFRunLoopPerformBlock to run the connect function (within a
	 * block) on the global runloop.
	 *
	 * CFRunLoopPerformBlock queues the block but it won't run until an event
	 * triggers the runloop to wake up. We use CFRunLoopWakeUp to force it to
	 * run pending blocks. Such as our connect block.
	 */
	M_io_mac_runloop_start();

	CFRunLoopPerformBlock(M_io_mac_runloop, kCFRunLoopCommonModes, ^{
		/* Use a temporary channel for storage because openRFCOMMChannelAsync takes an __auto_release paraemter.
		 * We are going to assign it to a __strong parameter to prevent it from being cleaned up from under
		 * us but we can't do that directly. */
		IOBluetoothRFCOMMChannel *channel = nil;
		IOReturn                  ioret;

		/* We've seen crashes trying to open the rfcomm channel due to
		 * some very bad internal issues within IOBluetooth due to an uncaught
		 * exception unrelated to opening the channel. This function is not supposed to throw
		 * exceptions as an error condition. It uses a ret return parameter.
		 *
		 * 2020-02-14 11:45:32.601930-0500 uniterm_console[85643:11048148] *** Terminating app due to uncaught exception 'NSInvalidArgumentException', reason: '*** -[__NSPlaceholderArray initWithObjects:count:]: attempt to insert nil object from objects[14]'
         * *** First throw call stack:
         * (
         * 	0   CoreFoundation                      0x00007fff3d655acd __exceptionPreprocess + 256
         * 	1   libobjc.A.dylib                     0x00007fff67d57a17 objc_exception_throw + 48
         * 	2   CoreFoundation                      0x00007fff3d694ac4 -[CFPrefsConfigurationFileSource initWithConfigurationPropertyList:containingPreferences:] + 0
         * 	3   CoreFoundation                      0x00007fff3d56836e -[__NSPlaceholderArray initWithObjects:count:] + 230
         * 	4   CoreFoundation                      0x00007fff3d5d5064 +[NSArray arrayWithObjects:count:] + 52
         * 	5   CoreFoundation                      0x00007fff3d5d5176 -[NSDictionary allValues] + 249
         * 	6   IOBluetooth                         0x00007fff3fd8e6f6 +[IOBluetoothObject getAllUniqueObjects] + 70
         * 	7   IOBluetooth                         0x00007fff3fd4f9f3 -[IOBluetoothDevice openRFCOMMChannelAsync:withChannelID:delegate:] + 115
		 * 	...
		 *
		 * This is something happening internally and there isn't anything we can do about it.
		 * We've only seen it happen when hammering the bluetooth system when repeatedly opening
		 * and closing connections.
		 *
		 * When the application starts throwing the above exception it will never stop when
		 * trying to open any bluetooth device. It will not be able to open any device and
		 * the application will need to be restarted. We're going to catch and log the exception
		 * instead of crashing from the exception.
		 */
		@try {
			ioret = [_dev openRFCOMMChannelAsync:&channel withChannelID:_cid delegate:self];
			if (ioret == kIOReturnSuccess) {
				_channel = channel;
			}
		}
		@catch (NSException *exception) {
			NSLog(@"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Bluetooth openRFCOMMChannelAsync EXCEPTION! === '%@'", exception);
		}
	});
	CFRunLoopWakeUp(M_io_mac_runloop);
}

- (void)close_int
{
	if (_channel != nil) {
		[_channel setDelegate:nil];
		[_channel closeChannel];
		_channel = nil;
	}

	if ([_dev isConnected])
		[_dev closeConnection];
}

- (void)close
{
	M_io_layer_t *layer;

	if (_handle == NULL) {
		[self close_int];
		return;
	}

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	[self close_int];
	_handle = NULL;
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

	if (_handle == NULL)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);

	/* We can only send up to _mtu bytes. */
	len = M_buf_len(_handle->writebuf);
	if (len > _mtu) {
		send_len = _mtu;
	} else {
		send_len = (M_uint16)len;
	}

	ioret = [_channel writeAsync:((void *)M_buf_peek(_handle->writebuf)) length:send_len refcon:NULL];
	ioerr = M_io_mac_ioreturn_to_err(ioret);
	if (ioerr == M_IO_ERROR_SUCCESS) {
		/* Save how much data we're writing so we can remove it from the buffer once the
		 * write has finished. */
		_handle->wrote_len = send_len;
		/* Block any new data from being buffered because we've started a write operation. */
		_handle->can_write = M_FALSE;
	} else {
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(ioret));
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, ioerr);
		[self close_int];
	}

	M_io_layer_release(layer);
}

- (void)write_data_buffered
{
	/* This function should be called when the layer is locked but _write will also
	 * lock the layer. We will displatch the actual write so this can return and unlock
	 * the layer. Otherwise, we'll end up in a dead lock. */
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		[self _write];
	});
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	if (_handle == NULL)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	_handle->state = M_IO_STATE_DISCONNECTED;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
	M_io_layer_release(layer);
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	if (_handle == NULL)
		return;

	if (dataLength == 0)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	M_buf_add_bytes(_handle->readbuf, (unsigned char *)dataPointer, dataLength);
	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)error
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	if (_handle == NULL)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	if (error == kIOReturnSuccess) {
		/* need to know how much data the device is capable of transferring in one request. */
		_mtu               = [_channel getMTU];
		/* Let everyone know we've connected. */
		_handle->state     = M_IO_STATE_CONNECTED;
		_handle->can_write = M_TRUE;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
	} else {
		_handle->state = M_IO_STATE_ERROR;
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(error));
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, M_io_mac_ioreturn_to_err(error));
		[self close_int];
	}
	M_io_layer_release(layer);
}

- (void)rfcommChannelQueueSpaceAvailable:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
	M_io_layer_t *layer;

	(void)rfcommChannel;

	if (_handle == NULL)
		return;

	layer = M_io_layer_acquire(_handle->io, 0, NULL);
	if (_handle->state == M_IO_STATE_CONNECTED) {
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
	}
	M_io_layer_release(layer);
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel refcon:(void *)refcon status:(IOReturn)error
{
	M_io_layer_t *layer;

	(void)rfcommChannel;
	(void)refcon;

	if (_handle == NULL)
		return;

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
			M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		}
	} else {
		_handle->state = M_IO_STATE_ERROR;
		M_snprintf(_handle->error, sizeof(_handle->error), "%s", M_io_mac_ioreturn_errormsg(error));
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, M_IO_ERROR_SUCCESS);
		[self close_int];
	}
	M_io_layer_release(layer);
}

@end
