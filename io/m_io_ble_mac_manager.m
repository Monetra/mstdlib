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
 * characteristics. This is an async process which follows this pattern.
 *
 * 1. Start a scan
 * 2. didDiscoverPeripheral is triggered with a peripheral object.
 * 3. Connect to peripheral.
 * 4. didConnectPeripheral is triggered.
 * 5. request services.
 * 6. didDiscoverServices is triggered and provides a list of services.
 * 7. For each service reported request its characteristics.
 * 8. didDiscoverCharacteristicsForService is triggered.
 * 9. Associate peripheral with M_io_ble_device_t.
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"
#import "m_io_ble_mac_manager.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
		NULL,
		NULL,
		NULL,
		(M_list_free_func)del_scan_trigger
	};

	self = [super init];
	if (!self)
		return nil;

	triggers = M_list_create(&lcbs, M_LIST_NONE);

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

	if (st == nil || !M_list_index_of(triggers, (__bridge CFTypeRef)st, M_LIST_MATCH_PTR, &idx))
		return;

	M_list_remove_at(triggers, idx);
	if (M_list_len(triggers) == 0 && blind_cnt == 0) {
		[_manager stopScan];
		M_io_ble_device_scan_finished();
	}
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

- (BOOL)requestNotifyFromPeripheral:(CBPeripheral *)peripheral forCharacteristic:(CBCharacteristic *)characteristic fromServiceUUID:(NSString *)service_uuid enabled:(BOOL)enabled
{
	/* We're passing in the service UUID instead of getting it from the characteristic
	 * because for some unknown reason the characteristic.service.UUID causes a bad
	 * memory read and crash. */
	const char *uuid;
	const char *characteristic_uuid;

	if (characteristic == nil)
		return NO;

	if ((enabled && characteristic.isNotifying) || (!enabled && !characteristic.isNotifying)) {
		uuid                = [[[peripheral identifier] UUIDString] UTF8String];
		characteristic_uuid = [[characteristic.UUID UUIDString] UTF8String];
		M_io_ble_device_notify_done(uuid, [service_uuid UTF8String], characteristic_uuid);
		return YES;
	}

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
	const char   *uuid;
	const char   *name;
	M_list_str_t *service_uuids;

	(void)central;
	(void)RSSI;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	name = [[advertisementData objectForKey:CBAdvertisementDataLocalNameKey] UTF8String];

	service_uuids = M_list_str_create(M_LIST_STR_SORTASC);
	for (CBUUID *nuuid in [advertisementData objectForKey:CBAdvertisementDataServiceUUIDsKey]) {
		M_list_str_insert(service_uuids, [[nuuid UUIDString] UTF8String]);
	}

	M_io_ble_saw_device(uuid, name, service_uuids);

	/* Ensure the peripheral has the proper delegate set. */
	if (peripheral.delegate != self)
		peripheral.delegate = self;

	if (M_io_ble_device_waiting_connect(uuid, service_uuids) && peripheral.state == CBPeripheralStateDisconnected) {
		M_io_ble_device_cache_peripherial(peripheral);
		[_manager connectPeripheral:peripheral options:nil];
	}

	M_list_str_destroy(service_uuids);
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
	(void)central;
	[peripheral discoverServices:nil];
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
	char        msg[256];

	if (peripheral == nil)
		return;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Service discovery failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	if ([peripheral.services count] == 0) {
		M_io_ble_device_set_state(uuid, M_IO_STATE_DISCONNECTING, NULL);
		[_manager cancelPeripheralConnection:peripheral];
		return;
	}

	for (CBService *service in peripheral.services)
		[peripheral discoverCharacteristics:nil forService:service];
}

- (void)peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices
{
	/* Ignore the list of invalidted. This list is only services that have
	* been "removed" by the device and doens't include new ones. We'll clear
	* all and pull again so we can have a full listing of services. */
	(void)invalidatedServices;

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
	BOOL        have_all = YES;

	if (peripheral == nil || service == nil)
		return;

	uuid         = [[[peripheral identifier] UUIDString] UTF8String];
	service_uuid = [[service.UUID UUIDString] UTF8String];

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Characteristic discovery failed: %s", [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	/* The manager is running on a single thread (the main one) so
	 * this event will never be processed in parallel and cause two
	 * connected states to be set. Discovering characteristics only
	 * happens once so we also don't need to worry about that either. */
	for (CBService *s in peripheral.services) {
		if ([s.characteristics count] == 0) {
			have_all = NO;
			break;
		}
	}

	if (have_all)
		M_io_ble_device_set_connected(peripheral);
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

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
	const char *uuid;
	const char *service_uuid;
	const char *characteristic_uuid;
	char        msg[256];

	if (peripheral == nil)
		return;

	uuid                = [[[peripheral identifier] UUIDString] UTF8String];
	service_uuid        = [[characteristic.service.UUID UUIDString] UTF8String];
	characteristic_uuid = [[characteristic.UUID UUIDString] UTF8String];

	if (error != nil) {
		M_snprintf(msg, sizeof(msg), "Set notify failed for characteristic (%s): %s", characteristic_uuid, [[error localizedDescription] UTF8String]);
		M_io_ble_device_set_state(uuid, M_IO_STATE_ERROR, msg);
		return;
	}

	M_io_ble_device_notify_done(uuid, service_uuid, characteristic_uuid);
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
