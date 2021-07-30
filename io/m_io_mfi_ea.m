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
#include "m_io_mfi_int.h"
#include "m_io_posix_common.h"
#include "m_io_int.h" /* For M_io_layer_at() */

#import "m_io_mfi_ea.h"

@implementation M_io_mfi_ea

EAAccessory   *_acc      = nil;
EASession     *_session  = nil;
M_io_handle_t *_handle   = NULL;
BOOL           _ropened  = NO;
BOOL           _wopened  = NO;

+ (id)m_io_mfi_ea:(NSString *)protocol handle:(M_io_handle_t *)handle serialnum:(NSString *)serialnum
{
	return [[M_io_mfi_ea alloc] init: protocol handle:handle serialnum:serialnum];
}

- (void)_read
{
	uint8_t       temp[1024];
	NSInteger     bread;
	M_io_layer_t *layer;
	M_bool        is_empty;

	if (_session == nil)
		return;

	/* We are signaled to read, so *always* read once without checking hasBytesAvailable
	 * first */

	do {
		bread = [[_session inputStream] read:temp maxLength:sizeof(temp)];

		/* Lock io layer */
		layer = M_io_layer_acquire(_handle->io, 0, NULL);

		if (bread <= 0) {
			_handle->state = M_IO_STATE_ERROR;
			M_snprintf(_handle->error, sizeof(_handle->error), "Read error: %s", [[[[_session inputStream] streamError] localizedDescription] UTF8String]);

			/* Close out the EA beaus there was an error and it's
			 * no longer in a useable state. */
			[self close];
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO: Is there a way to get a better error? */);

			M_io_layer_release(layer);
			layer = NULL;
			return;
		} else {
			is_empty = (M_buf_len(_handle->readbuf) == 0)?M_TRUE:M_FALSE;
			M_buf_add_bytes(_handle->readbuf, temp, (size_t)bread);
			/* If _handle->readbuf was empty, send READ soft event */
			if (is_empty && _handle->state == M_IO_STATE_CONNECTED)
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		}

		/* Unlock IO layer */
		M_io_layer_release(layer);
		layer = NULL;
	} while([_session inputStream].hasBytesAvailable && bread > 0);
}

- (void)_write
{
	NSInteger                  bwritten;
	M_io_layer_t              *layer;
	M_io_posix_sigpipe_state_t sigpipe_state;
	NSStreamStatus             status;

	if (_session == nil || M_buf_len(_handle->writebuf) == 0)
		return;

	/* Lock io layer */
	layer = M_io_layer_acquire(_handle->io, 0, NULL);

	status = [[_session outputStream] streamStatus];
	if (status != NSStreamStatusOpen) {
		M_snprintf(_handle->error, sizeof(_handle->error), "Stream Status no longer open");
		_handle->state = M_IO_STATE_ERROR;

		[self close];
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO: Is there a way to get a better error ? */);

		goto cleanup;
	}

	/* Interestingly, it appears, though its not documented, that an outputStream write() can generate
	 * a SIGPIPE and crash the entire process.  Lets use our normal POSIX sigpipe handling code to
	 * prevent this situtation */
	M_io_posix_sigpipe_block(&sigpipe_state);
	bwritten = [[_session outputStream] write:(const uint8_t *)M_buf_peek(_handle->writebuf) maxLength:M_buf_len(_handle->writebuf)];
	M_io_posix_sigpipe_unblock(&sigpipe_state);
	if (bwritten < 0) {
		M_snprintf(_handle->error, sizeof(_handle->error), "Write error: %s", [[[[_session outputStream] streamError] localizedDescription] UTF8String]);
		_handle->state = M_IO_STATE_ERROR;
		[self close];
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR);
	} else {
		M_buf_drop(_handle->writebuf, (size_t)bwritten);
	}

cleanup:
	/* Unlock IO layer */
	M_io_layer_release(layer);
	layer = NULL;
}

- (void)accessoryDidDisconnect:(EAAccessory *)accessory
{
	M_io_layer_t *layer;

	(void)accessory;

	/* Close self */
	[self close];

	/* Lock IO layer */
	layer = M_io_layer_acquire(_handle->io, 0, NULL);

	/* Sent disconnected or error soft event */
	if (_handle->state == M_IO_STATE_CONNECTING || _handle->state == M_IO_STATE_CONNECTED) {
		M_snprintf(_handle->error, sizeof(_handle->error), "Device disconnected");
		_handle->state = M_IO_STATE_ERROR;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_DISCONNECT);
	}

	/* Unlock IO layer */
	M_io_layer_release(layer);
	layer = NULL;
}

- (void)stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode
{
	M_io_layer_t *layer;
	switch (eventCode) {
		case NSStreamEventNone:
			break;
		case NSStreamEventOpenCompleted:
			/* Already open, nothing to do */
			if (_ropened && _wopened)
				break;

			if (aStream == [_session inputStream]) {
				_ropened = YES;
			} else if (aStream == [_session outputStream]) {
				_wopened = YES;
			}

			if (_ropened && _wopened) {
				/* Lock IO layer */
				layer = M_io_layer_acquire(_handle->io, 0, NULL);

				/* Sent connected soft event */
				M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
				_handle->state = M_IO_STATE_CONNECTED;

				/* We could have buffered bytes before both streams were open */
				if (M_buf_len(_handle->readbuf))
					M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);

				/* Unlock IO layer */
				M_io_layer_release(layer);
				layer = NULL;
			}
			break;
		case NSStreamEventHasBytesAvailable:
			[self _read];
			break;
		case NSStreamEventHasSpaceAvailable:
			[self _write];
			break;
		case NSStreamEventErrorOccurred:
		case NSStreamEventEndEncountered:
			/* Close self */
			[self close];

			/* Lock IO layer */
			layer = M_io_layer_acquire(_handle->io, 0, NULL);

			/* Sent disconnected or error soft event */
			if (_handle->state == M_IO_STATE_CONNECTING || _handle->state == M_IO_STATE_CONNECTED) {
				if (eventCode == NSStreamEventErrorOccurred) {
					M_snprintf(_handle->error, sizeof(_handle->error), "Received NSStreamEventErrorOccurred: %s", [[[[_session outputStream] streamError] localizedDescription] UTF8String]);
					_handle->state = M_IO_STATE_ERROR;
					M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO: Can we get a better error? */);
				} else {
					M_snprintf(_handle->error, sizeof(_handle->error), "Received NSStreamEventEndEncountered");
					_handle->state = M_IO_STATE_DISCONNECTED;
					M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
				}
			}

			/* Unlock IO layer */
			M_io_layer_release(layer);
			layer = NULL;

			break;
		default:
			break;
	}
}

- (id)init:(NSString *)protocol handle:(M_io_handle_t *)handle serialnum:(NSString *)serialnum
{
	NSArray<EAAccessory *> *accs;

	self = [super init];
	if (!self)
		return nil;

	_ropened = NO;
	_wopened = NO;
	_handle  = handle;

	accs = [[EAAccessoryManager sharedAccessoryManager] connectedAccessories];
	for (_acc in accs) {
		if ([_acc.protocolStrings containsObject:protocol]) {
			if ([serialnum length] == 0 || [serialnum isEqualToString:_acc.serialNumber]) {
				break;
			}
		}
		_acc = nil;
	}

	if (_acc == nil)
		return nil;

	_session = [[EASession alloc] initWithAccessory:_acc forProtocol:protocol];
	if (_session == nil)
		return nil;

	[_acc setDelegate:self];
	return self;
}


- (void)connect
{
	[[_session inputStream] setDelegate:self];
	[[_session inputStream] scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
	[[_session inputStream] open];

	[[_session outputStream] setDelegate:self];
	[[_session outputStream] scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
	[[_session outputStream] open];
}


- (void)dealloc
{
	[self close];
}


- (void)close
{
	dispatch_block_t stream_closer = ^{
		/* Try to prevent concurrent access */
		EASession *session_copy = _session;
		if (session_copy == nil)
			return;
		_session = nil;

		[[session_copy inputStream] close];
		[[session_copy inputStream] removeFromRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
		[[session_copy inputStream] setDelegate:nil];

		[[session_copy outputStream] close];
		[[session_copy outputStream] removeFromRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
		[[session_copy outputStream] setDelegate:nil];

		session_copy = nil;

		[_acc setDelegate:nil];
		_acc = nil;
	};

	if ([[NSThread currentThread] isMainThread]) {
		stream_closer();
	} else {
		/* NOTE: dispatch_sync cannot enqueue a task while a task is already
		 *       being processed.  But we are holding a lock on the IO layer
		 *       in this case so we need to release the lock and re-acquire to
		 *       prevent an odd race condition with a read or a write */
		M_io_layer_t *layer = M_io_layer_at(_handle->io, 0);
		M_io_layer_release(layer);
		dispatch_sync(dispatch_get_main_queue(), stream_closer);
		M_io_layer_acquire(_handle->io, 0, NULL);
	}
}


- (void)write_data_buffered
{
	dispatch_async(dispatch_get_main_queue(), ^ {
		[self _write];
	});
}

@end
