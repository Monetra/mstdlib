/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * When a peripheral first connects we know very little about it. Before any reads
 * or write can happen we need to get a list of services and each service's
 * characteristics. This is an asyc process which follows this pattern.
 *
 * 1. Start a scan
 * 2. didDiscoverPeripheral is triggered with a peripheral object.
 * 3. Connect to peripheral.
 * 4. didConnectPeripheral is triggered.
 * 5. request services.
 * 6. didDiscoverServices is triggered and provides a list of services.
 * 7. For each service reported request its characteristics.
 * 8. didDiscoverCharacteristicsForService is triggered.
 *
 * The peripheral, and characteristic objects are all cached externally.
 * Characteristics are lazy loaded and only pulled on a connect. A scan
 * running as part of a basic enumeration will not request characteristics.
 * As such, we can end up in a few different situations when opening a device
 * for use.
 *
 * 1. We've scanned and have a peripheral and list of services cached.
 *    a. Request characteristics.
 *    b. Issue connected event.
 * 2. We haven't scanned and need to find a given device.
 *    a. Start scan
 *    b. Request services
 *    c. Request characteristics for each service.
 *    d. Issue connected event.
 *
 * In both situations the connected event is triggered after characteristics
 * for all services have been read.
 *
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"
#import "m_io_ble_mac_manager.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int compar_scan_trigger(const void *a, const void *b, void *thunk)
{
	const ScanTrigger *sta = (__bridge ScanTrigger *)(*(void * const *)a);
	const ScanTrigger *stb = (__bridge ScanTrigger *)(*(void * const *)b);

	(void)thunk;

	if (sta == NULL || stb == NULL)
		return 0;

	/* Compare based on trigger pointer */
	if (sta.trigger == stb.trigger)
		return 0;
	if (sta.trigger < stb.trigger)
		return -1;
	return 1;
}

static void del_scan_trigger(void *p)
{
	ScanTrigger *st = (__bridge_transfer ScanTrigger *)p;

	if (p == NULL)
		return;

	/* Just trigger. We don't own it and it will be cleanup externally. */
	M_event_trigger_signal(st.trigger);
	if (st.timer.valid)
		[st.timer invalidate];
	
	st.trigger = NULL;
	st.timer   = nil;
	st         = nil;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

@implementation ScanTrigger

+ (id)scanTrigger:(M_event_trigger_t *)trigger timer:(NSTimer *)timer
{
	return [[ScanTrigger alloc] init:trigger timer:timer];
}

- (id)init:(M_event_trigger_t *)trigger timer:(NSTimer *)timer
{
	self = [super init];
	if (!self)
		return nil;

	_trigger = trigger;
	_timer   = timer;

	return self;
}

@end

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

@implementation M_io_ble_mac_manager

CBCentralManager *_manager   = nil;
M_list_t         *triggers   = NULL; /* List of ScanTrigger objects */
BOOL              powered_on = NO;
NSUInteger        blind_cnt  = 0;

+ (id)m_io_ble_mac_manager
{
	return [[M_io_ble_mac_manager alloc] init];
}

- (id)init
{
	struct M_list_callbacks lcbs = {
		compar_scan_trigger,
		NULL,
		NULL,
		(M_list_free_func)del_scan_trigger
	};

	self = [super init];
	if (!self)
		return nil;

	/* All matching is based on value because the ScanTrigger object
	 * created in startScan has a different pointer than the one
	 * sent to scanTimeout. Even though they're the same object. */
	triggers = M_list_create(&lcbs, M_LIST_SET_VAL);

	return self;
}

- (void)dealloc
{
	_manager = nil;
	M_list_destroy(triggers, M_TRUE);
	M_io_ble_cbc_event_reset();
}

- (void)setManager:(CBCentralManager *)manager
{
	_manager = manager;
}

- (void)startScan:(M_event_trigger_t *)trigger timeout_ms:(M_uint64)timeout_ms
{
	NSTimer     *timer;
	ScanTrigger *st;

	if (trigger == NULL)
		return;

	if (!_manager.isScanning && powered_on)
		[_manager scanForPeripheralsWithServices:nil options:nil];

	timeout_ms = M_io_ble_validate_timeout(timeout_ms);
	st         = [ScanTrigger scanTrigger:trigger timer:nil];
	timer      = [NSTimer scheduledTimerWithTimeInterval:(double)timeout_ms/1000 target:self selector:@selector(scanTimeout:) userInfo:st repeats:NO];
	[st setTimer:timer];

	M_list_insert(triggers, (__bridge_retained CFTypeRef)st);
}

- (void)startScanBlind
{
	blind_cnt++;

	if (_manager.isScanning || !powered_on)
		return;

	[_manager scanForPeripheralsWithServices:nil options:nil];
}

- (void)stopScanBlind
{
	if (blind_cnt == 0)
		return;

	blind_cnt--;

	/* Scan requests might still be outstanding. Don't
	 * stop scanning if this is the case. This is to
	 * stop blind scans in case they were started without
	 * using a trigger. Trying to connect to a specific
	 * device uses blind scans. */
	if (!_manager.isScanning || M_list_len(triggers) != 0 || blind_cnt != 0)
		return;

	[_manager stopScan];
	M_io_ble_device_scan_finished();
}

- (void)scanTimeout:(NSTimer *)timer
{
	ScanTrigger *st = timer.userInfo;
	size_t       idx;

	if (st == nil || !M_list_index_of(triggers, (__bridge void *)st, M_LIST_MATCH_VAL, &idx))
		return;

	M_list_remove_at(triggers, idx);
	if (M_list_len(triggers) == 0 && blind_cnt == 0) {
		[_manager stopScan];
		M_io_ble_device_scan_finished();
	}
}

- (BOOL)connectToDevice:(CBPeripheral *)peripheral
{
	CBPeripheralState state;

	if (!powered_on || _manager == nil || peripheral == nil)	
		return NO;

	state = peripheral.state;
	if (state == CBPeripheralStateDisconnected) {
		[_manager connectPeripheral:peripheral options:nil];
		return YES;
	}

	/* Most likely the device is already connected. We can't connect
	 * to a connected device. Also, we want to protect a second request
	 * from another io object thinking it's connected when it isn't. */
	return NO;
}

- (void)disconnectFromDevice:(CBPeripheral *)peripheral
{
	const char        *uuid;
	CBPeripheralState  state;

	if (!powered_on || _manager == nil || peripheral == nil)	
		return;

	uuid  = [[[peripheral identifier] UUIDString] UTF8String];
	state = peripheral.state;
	if (state == CBPeripheralStateConnected || state == CBPeripheralStateConnecting) {
		M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTING, NULL);
		[_manager cancelPeripheralConnection:peripheral];
	} else {
		M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTED, NULL);
	}
	return;
}

- (BOOL)writeDataToPeripherial:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic data:(NSData *)data blind:(BOOL)blind
{
	CBCharacteristicWriteType type = CBCharacteristicWriteWithResponse;

	if (peripheral == nil || characteristic == nil || data == nil || data.length == 0)
		return NO;

	if (blind)
		type = CBCharacteristicWriteWithoutResponse;
	[peripheral writeValue:data forCharacteristic:characteristic type:type];
	return YES;
}

- (BOOL)requestDataFromPeripherial:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic
{
	if (peripheral == nil || characteristic == nil)
		return NO;

	[peripheral readValueForCharacteristic:characteristic];
	return YES;
}

- (BOOL)requestRSSIFromPeripheral:(CBPeripheral *)peripheral
{
	if (peripheral == nil)
		return NO;

	[peripheral readRSSI]; 
	return YES;
}

- (BOOL)requestNotifyFromPeripheral:peripheral forCharacteristic:(CBCharacteristic *)characteristic enabled:(BOOL)enabled
{
	if (characteristic == nil)
		return NO;

	[peripheral setNotifyValue:enabled forCharacteristic:characteristic];
	return YES;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
	switch (central.state) {
		case CBManagerStatePoweredOn:
			powered_on = YES;
			/* Start a scan if something is waiting to find devices. A scan or M_io_ble_create
			 * request could have come in before the manager had initalized. Or one could have
			 * been running when a reset or other event happend and now we can resume scanning. */
			if (!_manager.isScanning && (M_list_len(triggers) != 0 || blind_cnt != 0)) {
				[_manager scanForPeripheralsWithServices:nil options:nil];
			}
			break;
		case CBManagerStateResetting:
		case CBManagerStatePoweredOff:
		case CBManagerStateUnauthorized:
		case CBManagerStateUnknown:
		case CBManagerStateUnsupported:
		default:
			/* This will clear all cached devices. Any of these events
			 * will invalidate the devices. */
			M_io_ble_cbc_event_reset();
			powered_on = NO;
			break;
	}
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI
{
	(void)central;
	(void)advertisementData;
	(void)RSSI;

	/* We need to cache before passing into a C function for some reason.
	 * If we pass in the CBPeripheral * then do the bridging later
	 * it doens't work. */
	M_io_ble_cache_device((__bridge_retained CFTypeRef)peripheral);

	/* Ensure the peripheral has the proper delegate set. */
	if (peripheral.delegate != self)
		peripheral.delegate = self;

	if (M_io_ble_device_need_read_services([[[peripheral identifier] UUIDString] UTF8String])) {
		[_manager connectPeripheral:peripheral options:nil];
	}
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
	const char *uuid;
	const char *service_uuid;
	M_bool      read_characteristics = M_FALSE;

	(void)central;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];

	if (M_io_ble_device_need_read_services(uuid)) {
		/* If we need services we need to request them regardless if there
		 * is a device associated. */
		[peripheral discoverServices:nil];
	} else { /* We have services */
		if (M_io_ble_device_is_associated(uuid)) {
			/* Check if we already have characteristics for all of the services.
			 * If we're missing any we need to request it. */
			for (CBService *service in peripheral.services) {
				service_uuid = [[service.UUID UUIDString] UTF8String];
				if (M_io_ble_device_need_read_characteristics(uuid, service_uuid)) {
					[peripheral discoverCharacteristics:nil forService:service];
					read_characteristics = M_TRUE;
				}
			}
			if (!read_characteristics) {
				/* We have all the characteristics so we're good to use the device. */
				M_io_ble_device_set_state(uuid, M_IO_STATE_CONNECTED, NULL);
			}
		} else {
			/* Not associated and we have services so we don't need to do anything. */
			[_manager cancelPeripheralConnection:peripheral];
			M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTING, NULL);
		}
	}
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
	const char *uuid;
	char        msg[256];

	(void)central;

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Failed to connect: %s", [[error localizedDescription] UTF8String]);
	} else {
		M_str_cpy(msg, sizeof(msg), "Failed to connect");
	}

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
	const char *uuid;

	(void)central;
	(void)error;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTED, NULL);
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
	const char *uuid;
	M_bool      associated;
	char        msg[256];

	if (peripheral == nil)
		return;

	uuid       = [[[peripheral identifier] UUIDString] UTF8String];
	associated = M_io_ble_device_is_associated(uuid);

	if (error != nil) {
		if (!associated) {
			[_manager cancelPeripheralConnection:peripheral];
		}
		M_snprintf(msg, sizeof(msg), "Service discovery failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	for (CBService *service in peripheral.services) {
		M_io_ble_device_add_serivce(uuid, [[service.UUID UUIDString] UTF8String]);
		if (associated) {
			/* Something is waiting to use this peripheral so we need
			 * to request the characteristics for all services. */
			[peripheral discoverCharacteristics:nil forService:service];
		}
	}

	if (!associated) {
		/* Nothing is waiting to use the peripheral so we must have been opened
		 * by a enumerating scan. We don't need the device anymore because we
		 * have everything we need. */
		[_manager cancelPeripheralConnection:peripheral];
		M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTING, NULL);
	}
}

- (void)peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices
{
	const char *uuid;

	/* Ignore the list of invalidted. This list is only services that have
	* been "removed" by the device and doens't include new ones. We'll clear
	* all and pull again so we can have a full listing of services. */
	(void)invalidatedServices;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	M_io_ble_device_clear_services(uuid);

	/* We issue a disconnect event because there could have been subscriptions which
	 * have now been cleared. It's better to pretend we got a disconnect and have
	 * the connection setup again by the caller. */
	[self disconnectFromDevice:peripheral];
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
	const char *uuid;
	const char *service_uuid;
	char        msg[256];

	if (peripheral == nil || service == nil)
		return;

	uuid         = [[[peripheral identifier] UUIDString] UTF8String];
	service_uuid = [[service.UUID UUIDString] UTF8String];

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Characteristic discovery failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	for (CBCharacteristic *c in service.characteristics) {
		M_io_ble_device_add_characteristic(uuid, service_uuid, [[c.UUID UUIDString] UTF8String], (__bridge_retained CFTypeRef)c);
	}

	/* The manager is running on a single thread (the main one) so
	 * this event will never be processed in parallel and cause two
	 * connected states to be set. Discovering characteristics only
	 * happens once so we also don't need to worry about that either. */
	if (M_io_ble_device_have_all_characteristics(uuid))
		M_io_ble_device_set_state(uuid, M_IO_STATE_CONNECTED, NULL);
}

- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
	const char *uuid;
	char        msg[256];

	if (peripheral == nil || characteristic == nil)
		return;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Write failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}
	M_io_ble_device_write_complete(uuid);
}

- (void)peripheralDidUpdateName:(CBPeripheral *)peripheral
{
	const char *uuid;
	const char *name;

	if (peripheral == nil)
		return;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	name = [peripheral.name UTF8String];
	M_io_ble_device_update_name(uuid, name);
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
	const char    *uuid;
	const char    *service_uuid;
	const char    *characteristic_uuid;
	NSData        *data;
	const uint8_t *rdata;
	char           msg[256];

	if (peripheral == nil || characteristic == nil)
		return;

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Read failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	uuid                = [[[peripheral identifier] UUIDString] UTF8String];
	service_uuid        = [[characteristic.service.UUID UUIDString] UTF8String];
	characteristic_uuid = [[characteristic.UUID UUIDString] UTF8String];
   	data                = characteristic.value;
   	rdata               = data.bytes;

	M_io_ble_device_read_data(uuid, service_uuid, characteristic_uuid, rdata, data.length);
}

- (void)peripheral:(CBPeripheral *)peripheral didReadRSSI:(NSNumber *)RSSI error:(NSError *)error
{
	const char *uuid;
	char        msg[256];

	if (peripheral == nil)
		return;

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "RSSI request failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	M_io_ble_device_read_rssi(uuid, [RSSI longLongValue]);
}

@end
