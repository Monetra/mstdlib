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

#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>

M_io_bluetooth_enum_t *M_io_bluetooth_enum(void)
{
	M_io_bluetooth_enum_t *btenum = NULL;
	NSArray               *ds;

	btenum = M_io_bluetooth_enum_init();

	ds = [IOBluetoothDevice pairedDevices];
	for (IOBluetoothDevice *d in ds) {
		const char *name      = [d.name UTF8String];
		const char *mac       = [[d.addressString stringByReplacingOccurrencesOfString:@"-" withString:@":"] UTF8String];
		const char *sname     = NULL;
		M_bool      connected = d.isConnected?M_TRUE:M_FALSE;

		NSArray *srs = d.services;
		for (IOBluetoothSDPServiceRecord *sr in srs) {
			BluetoothRFCOMMChannelID rfid;
			/* Filter out anything that's not an rfcomm service. */
			if ([sr getRFCOMMChannelID:&rfid] != kIOReturnSuccess) {
				continue;
			}

			NSString *sn = [sr getServiceName];
			if (sn != nil) {
				sname = [sn UTF8String];
			}

			NSDictionary *di = sr.attributes;
			for (NSString *k in di) {
				IOBluetoothSDPDataElement *e = [di objectForKey:k];
				NSArray *iea = [e getArrayValue];

				for (IOBluetoothSDPDataElement *ie in iea) {
					if ([ie getTypeDescriptor] != kBluetoothSDPDataElementTypeUUID) {
						continue;
					}

					IOBluetoothSDPUUID *u  = [[ie getUUIDValue] getUUIDWithLength:16];
					const unsigned char *b = [u bytes];
					char uuid[64];

					snprintf(uuid, sizeof(uuid),
						"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
						b[0], b[1], b[2], b[3],
						b[4], b[5], b[6], b[7],
						b[8], b[9], b[10], b[11],
						b[12], b[13], b[14], b[15]);

					M_io_bluetooth_enum_add(btenum, name, mac, sname, uuid, connected);
				}
			}
		}
	}

	return btenum;
}

M_io_handle_t *M_io_bluetooth_open(const char *mac, const char *uuid, M_io_error_t *ioerr)
{
}

M_bool M_io_bluetooth_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
}

M_io_state_t M_io_bluetooth_state_cb(M_io_layer_t *layer)
{
}

static void M_io_bluetooth_close(M_io_handle_t *handle, M_io_state_t state)
{
}

void M_io_bluetooth_destroy_cb(M_io_layer_t *layer)
{
}

M_bool M_io_bluetooth_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;
	/* Do nothing, all events are generated as soft events and we don't have anything to process */
	return M_FALSE;
}

M_io_error_t M_io_bluetooth_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
}

M_io_error_t M_io_bluetooth_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
}

M_bool M_io_bluetooth_disconnect_cb(M_io_layer_t *layer)
{
}

void M_io_bluetooth_unregister_cb(M_io_layer_t *layer)
{
}

M_bool M_io_bluetooth_init_cb(M_io_layer_t *layer)
{
}
