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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"
#import "m_io_ble_mac_scanner.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int compar_scan_trigger(const void *a, const void *b, void *thunk)
{
	const ScanTrigger *sta = (__bridge ScanTrigger *)(*(void * const *)a);
	const ScanTrigger *stb = (__bridge ScanTrigger *)(*(void * const *)b);

	(void)thunk;

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

@implementation M_io_ble_mac_scanner

CBCentralManager *_manager   = nil;
M_list_t         *triggers   = NULL; /* List of ScanTrigger objects */
BOOL              powered_on = NO;

+ (id)m_io_ble_mac_scanner
{
	return [[M_io_ble_mac_scanner alloc] init];
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

	/* If there is only one trigger than there wasn't a scan previously running.
	 * We don't need to call start a scan if it's already running. */
	if (M_list_len(triggers) == 0 && powered_on)
		[_manager scanForPeripheralsWithServices:nil options:nil];

	/* We have timeout default of 1 minute when 0 and a max of 5 minutes. */
	if (timeout_ms == 0)
		timeout_ms = 60000;
	if (timeout_ms >= 300000)
		timeout_ms = 300000;

	st    = [ScanTrigger scanTrigger:trigger timer:nil];
	timer = [NSTimer scheduledTimerWithTimeInterval:(double)timeout_ms/1000 target:self selector:@selector(scanTimeout:) userInfo:st repeats:NO];
	[st setTimer:timer];

	M_list_insert(triggers, (__bridge_retained CFTypeRef)st);
}

- (void)scanTimeout:(NSTimer *)timer
{
	ScanTrigger *st = timer.userInfo;
	size_t       idx;

	if (st == nil || !M_list_index_of(triggers, (__bridge void *)st, M_LIST_MATCH_VAL, &idx))
		return;

	M_list_remove_at(triggers, idx);
	if (M_list_len(triggers) == 0)
		[_manager stopScan];
}


- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
	switch (central.state) {
		case CBManagerStatePoweredOn:
			powered_on = YES;
			if (M_list_len(triggers) > 0) {
				[_manager scanForPeripheralsWithServices:nil options:nil];
			}
			break;
		case CBManagerStateResetting:
			M_io_ble_cbc_event_reset();
		case CBManagerStatePoweredOff:
		case CBManagerStateUnauthorized:
		case CBManagerStateUnknown:
		case CBManagerStateUnsupported:
		default:
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

	 if (M_io_ble_device_need_read_services(peripheral))
		 [_manager connectPeripheral:peripheral options:nil];
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
	(void)central;

	/* XXX: When opening a device for use need to have this set a different delegate and
	 * don't try to discover services. */
	peripheral.delegate = self;
	[peripheral discoverServices:nil];
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
	const char        *mac;
	(void)error;

	mac = [[[peripheral identifier] UUIDString] UTF8String];
	for (CBService *service in peripheral.services) {
		M_io_ble_device_add_serivce(mac, [[service.UUID UUIDString] UTF8String]);
	}
	[_manager cancelPeripheralConnection:peripheral];
}

- (void)peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices
{
	/* Ignore the list of invalidted. This list is only services that have
	* been "removed" by the device and doens't include new ones. We'll clear
	* all and pull again so we can have a full listing of services. */
	(void)invalidatedServices;

	M_io_ble_device_clear_services([[[peripheral identifier] UUIDString] UTF8String]);
	[peripheral discoverServices:nil];
}

@end
