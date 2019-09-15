/* The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Monetra Technologies, LLC.
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

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_int.h"
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"
#include "m_io_ble_mac_manager.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * CBCentralManager will only trigger events on the main run loop. If it's not
 * running there won't be any events!
 *
 * The manager object is only going to be run on on the main queue and this
 * need to ensure that happens. This is to prevent any cross thread access and
 * event delivery. Always dispatch these async if it's sync and the main run
 * loop is not running we'll get a segfault due to bad access.
 *
 *
 * All access to the ble_devices, and ble_waiting caches need to be locked.
 *
 *
 * Devices need to be seen from a scan before they can be used. We cannot
 * cache the CBPeripheral/CBService/CBCharacteristic objects in a cache
 * because once cancelPeripheralConnection is called they are invalidated.
 * This prevents us from reusing already seen devices.
 *
 * The OS caches devices internally based on the device (UUID) identifier.
 * We'll query the OS using retrievePeripheralsWithIdentifiers to try and
 * skip scanning.
 *
 * The OS also can be queried for currently connected devices by the service
 * id. We'll use retrieveConnectedPeripheralsWithServices to try and connect
 * to a matching device if it's present. Multiple connection to a BLE device
 * is permitted and works. However, some devices aren't able to handle
 * this and could be a bit wonky. For example, if the device was designed
 * for serial communication (it really shouldn't have been).
 *
 * Using retrieve[Connected]PeripheralsWith* will speed up the connection process
 * because scanning isn't necessary.
 *
 *
 * Apple provides guidance on reconnecting to devices at
 * https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/BestPracticesForInteractingWithARemotePeripheralDevice/BestPracticesForInteractingWithARemotePeripheralDevice.html#//apple_ref/doc/uid/TP40013257-CH6-SW9
 * It is described as:
 *
 * Find Known Peripherals (retrievePeripheralsWithIdentifiers)
 * -> Found
 *    -> Try connect
 *       -> Connected
 *          -> Done
 *       -> Fail connect
 *          -> Goto scan flow
 * -> Not found
 *    -> Goto Service flow
 *
 * Service Flow (retrieveConnectedPeripheralsWithServices)
 * -> Found
 *    -> Connect
 *       -> Done
 * -> Not found
 *    -> Goto scan flow
 *
 * Scan Flow (See M_io_ble_mac_manager.m for steps)
 * -> Scan, discover, connect
 *
 * We do not follow the "Find Known Peripherals" flow exactly as described.
 * because we do not have a M_io_ble_create type function that takes both
 * a device and service id. When the device is not found in this flow
 * we skip the Service Flow (we don't have a service id) and go directly to
 * the Scan Flow.
 *
 * Also, if connect fails in the "Find Known Peripherals" flow we do not
 * auto retry by scanning. Instead we let the caller retry. A connection
 * error will call the didFailToConnectPeripheral callback when there is
 * a transient error and it's safe to try again. Per the docs,
 * "connection attempts do not time out" so it's not guaranteed this callback
 * will be triggered. If it's not the caller will most likely try again anyway
 * so we don't double the work.
 *
 * Our modified flow looks like this:
 *
 * Find Known Peripherals (retrievePeripheralsWithIdentifiers)
 * -> Found
 *    -> Try connect
 *       -> Connected
 *          -> Done
 * -> Not found
 *    -> Goto Scan flow
 *
 *
 * The ble_waiting cache holds device handles we want to associate to a device.
 * The ble_waiting_service cache holds device handles we want to associate with
 * any device that provides a specific service. On connect a waiting device is
 * added to the ble_devices cache which holds devices currently open / in use.
 *
 * The ble_peripherals cache holds CBPeripheral's that are currently in the
 * process of connecting but have not been associated with a M_io_ble_device_t
 * and added to the ble_devices cache. CBPeripheral are ARC counted devices.
 * Meaning, we need to retain the object otherwise it will be cleaned up once
 * it goes out of scope. If didDiscoverPeripheral from the CBCentralManager
 * is called without retaining the CBPeripheral the didConnectPeripheral delegate
 * function would never be called because the CBPeripheral will have been
 * destroyed when the function ended.
 *
 * Devices in the ble_devices cache will have an M_io_handle_t associated with
 * them when connected. Read and write events will marshal data into and out
 * of the handle. Event's will also be triggered on the handle. If a handle
 * is not associated with a device then events will be ignored.
 *
 * Any access to dev->handle will be within a M_io_layer_acquire. This is to
 * prevent the cbc_manager from trying to add data to the handle's write queue
 * while it's trying to be read from a read_cb.
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_event_trigger_t  *trigger;
	M_event_callback_t  cb;
	void               *cb_arg;
} trigger_wrapper_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Last seen can't be more than 15 minutes old. */
const M_int64 LAST_SEEN_EXPIRE = 15*60;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_ble_mac_manager *manager             = nil;
static CBCentralManager     *cbc_manager         = nil;

static M_hash_strvp_t       *ble_seen            = NULL; /* key = UUID (device), val = M_io_ble_enum_device_t. */
static M_hash_strvp_t       *ble_peripherals     = NULL; /* key = UUID (device), val = M_io_ble_device_t. */
static M_hash_strvp_t       *ble_devices         = NULL; /* key = UUID (device), val = M_io_ble_device_t. */
static M_hash_strvp_t       *ble_waiting         = NULL; /* key = UUID (device), val = M_io_handle_t. */
static M_hash_strvp_t       *ble_waiting_service = NULL; /* key = UUID (service), val = M_io_handle_t. */
static M_thread_mutex_t     *lock                = NULL;
static M_thread_cond_t      *cond                = NULL;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_ble_cleanup(void *arg)
{
	(void)arg;

	if (cbc_manager == nil)
		return;

	/* Kill the references so ARC sees the object
 	 * isn't used and will clean it up. */
	manager     = nil;
	cbc_manager = nil;

	M_hash_strvp_destroy(ble_seen, M_TRUE);
	M_hash_strvp_destroy(ble_devices, M_TRUE);
	M_hash_strvp_destroy(ble_peripherals, M_TRUE);
	M_hash_strvp_destroy(ble_waiting, M_TRUE);
	M_hash_strvp_destroy(ble_waiting_service, M_TRUE);

	M_thread_mutex_destroy(lock);
	M_thread_cond_destroy(cond);
}

static void M_io_ble_services_destroy(M_hash_strvp_t *characteristics)
{
	if (characteristics == NULL)
		return;
	M_hash_strvp_destroy(characteristics, M_TRUE);
}

static void M_io_ble_characteristics_destroy(CFTypeRef c)
{
	CBCharacteristic *cbc;

	if (c == NULL)
		return;

	/* Decrement the reference count so ARC can
 	 * clean up peripheral. Ignore clang warnings about value
	 * set but not used. */
	cbc = (__bridge_transfer CBCharacteristic *)c;
	(void)cbc;
}

static void M_io_ble_device_remove_cache(CFTypeRef p)
{
	CBPeripheral *peripheral;

	if (p == NULL)
		return;

	/* Decrement the reference count so ARC can
 	 * clean up peripheral. Ignore clang warnings about value
	 * set but not used. */
	peripheral = (__bridge_transfer CBPeripheral *)p;
	(void)peripheral;
}

static M_io_ble_device_t *M_io_ble_device_create(CBPeripheral *peripheral)
{
	//CBPeripheral      *p;
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *characteristics;
	const char        *service_uuid;
	const char        *characteristic_uuid;

	dev             = M_malloc_zero(sizeof(*dev));
	dev->peripheral = (__bridge_retained CFTypeRef)peripheral;
	dev->services   = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_services_destroy);

	/* Go though all of the services and store their characteristic objects. */
	for (CBService *s in peripheral.services) {
		service_uuid    = [[s.UUID UUIDString] UTF8String];
		characteristics = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_characteristics_destroy);
		M_hash_strvp_insert(dev->services, service_uuid, characteristics);

		for (CBCharacteristic *c in s.characteristics) {
			characteristic_uuid = [[c.UUID UUIDString] UTF8String];
			M_hash_strvp_insert(characteristics, characteristic_uuid, M_CAST_OFF_CONST(void *, (__bridge_retained CFTypeRef)c));
		}
	}

	return dev;
}

static void M_io_ble_device_destroy(M_io_ble_device_t *dev)
{
	CBPeripheral *p;

	if (dev == NULL)
		return;

	M_hash_strvp_destroy(dev->services, M_TRUE);

	/* Decrement the reference count so ARC can
 	 * clean up peripheral. Ignore clang warnings about value
	 * set but not used. */
	p = (__bridge_transfer CBPeripheral *)dev->peripheral;
	(void)p;

	M_free(dev);
}

static void M_io_ble_waiting_destroy(M_io_handle_t *handle)
{
	M_io_layer_t *layer;

	if (handle == NULL || handle->io == NULL)
		return;

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	M_snprintf(handle->error, sizeof(handle->error), "Timeout");
	handle->state = M_IO_STATE_ERROR;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR);
	M_io_layer_release(layer);
}

static trigger_wrapper_t *trigger_wrapper_create(M_event_trigger_t *trigger, M_event_callback_t callback, void *cb_arg)
{
	trigger_wrapper_t *tw;

	tw          = M_malloc_zero(sizeof(*tw));
	tw->trigger = trigger;
	tw->cb      = callback;
	tw->cb_arg  = cb_arg;

	return tw;
}

static void trigger_wrapper_destroy(trigger_wrapper_t *tw)
{
	if (tw == NULL)
		return;

	M_event_trigger_remove(tw->trigger);
	M_free(tw);
}

static void trigger_wrapper_set_trigger(trigger_wrapper_t *tw, M_event_trigger_t *trigger)
{
	if (tw == NULL)
		return;
	tw->trigger = trigger;
}

static void scan_done_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	trigger_wrapper_t *tw = cb_arg;

	tw->cb(event, type, io, tw->cb_arg);
	trigger_wrapper_destroy(tw);
}

static void M_io_ble_reset_caches(void)
{
	M_hash_strvp_destroy(ble_seen, M_TRUE);
	M_hash_strvp_destroy(ble_devices, M_TRUE);
	M_hash_strvp_destroy(ble_peripherals, M_TRUE);
	/* Will notify all waiting io objects with an error. */
	M_hash_strvp_destroy(ble_waiting, M_TRUE);
	M_hash_strvp_destroy(ble_waiting_service , M_TRUE);

	ble_seen            = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void (*)(void *))M_io_ble_enum_free_device);
	ble_devices         = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
	ble_peripherals     = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_remove_cache);
	ble_waiting         = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);
	ble_waiting_service = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);
}

static void M_io_ble_manager_init(void)
{
	static dispatch_once_t  d = 0;

	/* Setup the scanning objects. */
	dispatch_once(&d, ^{
		M_io_ble_reset_caches();
		lock        = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
		cond        = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
		manager     = [M_io_ble_mac_manager m_io_ble_mac_manager];
		cbc_manager = [[CBCentralManager alloc] initWithDelegate:manager queue:nil];
		[manager setManager:cbc_manager];

		M_library_cleanup_register(M_io_ble_cleanup, NULL);
	});
}

static void start_blind_scan(void)
{
	/* Don't start a scan if there aren't any
	 * devices that need to be scanned for. */
	if (M_hash_strvp_num_keys(ble_waiting) == 0 && M_hash_strvp_num_keys(ble_waiting_service) == 0)
		return;

	dispatch_async(dispatch_get_main_queue(), ^{
		[manager startScanBlind];
	});
}

static void stop_blind_scan(void)
{
	/* Don't stop the scan if there are handles waiting
	 * for a device to be found. */
	if (M_hash_strvp_num_keys(ble_waiting) != 0 && M_hash_strvp_num_keys(ble_waiting_service) != 0)
		return;

	dispatch_async(dispatch_get_main_queue(), ^{
		[manager stopScanBlind];
	});
}

/* MUST BE IN A LOCK TO CALL THIS FUNCTION! */
static void update_seen(const char *uuid)
{
	M_io_ble_enum_device_t *edev;

	if (!M_hash_strvp_get(ble_seen, uuid, (void **)&edev))
		return;
	edev->last_seen = M_time();
}

/* MUST BE IN A LOCK TO CALL THIS FUNCTION! */
static void M_io_ble_device_cache_peripherial_int(CBPeripheral *peripheral)
{
	CFTypeRef   p;
	const char *uuid;

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	if (M_hash_strvp_get(ble_peripherals, uuid, NULL))
		return;

	/* Transfer the reference of the peripheral into our cache so
 	 * it isn't delete by ARC. */
	p = (__bridge_retained CFTypeRef)peripheral;
	M_hash_strvp_insert(ble_peripherals, uuid, M_CAST_OFF_CONST(void *, p));
}

static M_bool get_peripheral_by_int(NSArray<CBPeripheral *> *peripherals)
{
	CBPeripheral *peripheral  = nil;

	if (peripherals == nil || [peripherals count] <= 0)
		return M_FALSE;

	/* Setup and try to connect to the first peripheral in the list. */
	peripheral = peripherals[0];
	if (peripheral.delegate != manager)
		peripheral.delegate = manager;

	M_io_ble_device_cache_peripherial_int(peripheral);
	[cbc_manager connectPeripheral:peripheral options:nil];

	return M_TRUE;
}

static M_bool get_peripheral_by_identifier(const char *identifier)
{
	NSUUID                  *uuid;
	NSArray<NSUUID *>       *uarr;
	NSArray<CBPeripheral *> *peripherals;

	uuid = [[NSUUID alloc] initWithUUIDString:[NSString stringWithUTF8String:identifier]];
	/* Ensure we actually have a NSUUID object because
	 * passing nil into the array will cause an exception.
	 * Creating the NSUUID will fail if the string isn't valid. */
	if (uuid == nil)
		return M_FALSE;

	uarr = @[uuid];
	peripherals = [cbc_manager retrievePeripheralsWithIdentifiers:uarr];
	return get_peripheral_by_int(peripherals);
}

static M_bool get_peripheral_by_service(const char *service_uuid)
{
	CBUUID                  *uuid;
	NSUUID                  *ns_uuid;
	NSArray<CBUUID *>       *cbarr;
	NSArray<CBPeripheral *> *peripherals;

	@try {
		/* Validate the UUID is a valid UUID.
		 * Creating the NSUUID will fail if the string isn't valid. */
		ns_uuid = [[NSUUID alloc] initWithUUIDString:[NSString stringWithUTF8String:service_uuid]];
		if (ns_uuid == nil)
			return M_FALSE;

		/* Pass the UUID along to become a CBUUID. This function will
		 * throw an exception if the UUID is bad so we're not using UUIDWithString here.
		 * We're catching exceptions but a bad UUID will generate an assertion warning
		 * which we'd rather not see. */
		uuid = [CBUUID UUIDWithNSUUID:ns_uuid];
	}
	@catch (NSException *e) {
		return M_FALSE;
	}
	/* Ensure we actually have a CBUUID object because
	 * passing nil into the array will cause an exception. */
	if (uuid == nil)
		return M_FALSE;

	cbarr = @[uuid];
	peripherals = [cbc_manager retrieveConnectedPeripheralsWithServices:cbarr];
	return get_peripheral_by_int(peripherals);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_io_ble_cbc_event_reset(void)
{
	M_io_layer_t        *layer;
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;

	M_thread_mutex_lock(lock);

	/* We need to issue a disconnect for each device individually because destroying
 	 * the device in the cache won't generate one. We can't have destroy generate
	 * because we could be removing from the cache for a disconnect or an error event. */
	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
		if (dev->handle == NULL) {
			continue;
		}
		layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
		M_io_layer_release(layer);
		dev->handle = NULL;
	}
	M_hash_strvp_enumerate_free(he);

	/* Reset the caches because everything is cleared. */
	M_io_ble_reset_caches();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_saw_device(const char *uuid, const char *name, const M_list_str_t *service_uuids)
{
	M_io_ble_enum_device_t *edev;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_seen, uuid, (void **)&edev)) {
		edev = M_malloc_zero(sizeof(*edev));
		M_str_cpy(edev->identifier, sizeof(edev->identifier), uuid);
		M_hash_strvp_insert(ble_seen, uuid, edev);
	}

	if (!M_str_isempty(name))
		M_str_cpy(edev->name, sizeof(edev->name), name);

	M_list_str_merge(&edev->service_uuids, M_list_str_duplicate(service_uuids), M_FALSE);
	edev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_update_name(const char *uuid, const char *name)
{
	M_io_ble_enum_device_t *edev;

	if (M_str_isempty(uuid))
		return;

	M_thread_mutex_lock(lock);

	/* Get the seen device, it should be there but if  not we'll ignore it. */
	if (!M_hash_strvp_get(ble_seen, uuid, (void **)&edev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_str_cpy(edev->name, sizeof(edev->name), name);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_reap_seen(void)
{
	M_io_ble_enum_device_t *edev;
	M_hash_strvp_enum_t    *he;
	const char             *uuid;
	M_list_str_t           *uuids;
	M_time_t                expire;
	size_t                  len;
	size_t                  i;

	M_thread_mutex_lock(lock);

	/* Generate a list of UUIDs that have expired. */
	uuids  = M_list_str_create(M_LIST_STR_NONE);
	expire = M_time()-LAST_SEEN_EXPIRE;
	M_hash_strvp_enumerate(ble_seen, &he);
	while (M_hash_strvp_enumerate_next(ble_seen, he, &uuid, (void **)&edev)) {
		if (edev->last_seen < expire) {
			M_list_str_insert(uuids, uuid);
		}
	}
	M_hash_strvp_enumerate_free(he);

	/* Go though the list of expired UUIDs and remove them. */
	len = M_list_str_len(uuids);
	for (i=0; i<len; i++) {
		M_hash_strvp_remove(ble_seen, M_list_str_at(uuids, i), M_TRUE);
	}
	M_list_str_destroy(uuids);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_cache_peripherial(CBPeripheral *peripheral)
{
	M_thread_mutex_lock(lock);
	M_io_ble_device_cache_peripherial_int(peripheral);
	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_set_connected(CBPeripheral *peripheral)
{
	M_io_ble_device_t   *dev;
	const char          *uuid;
	const char          *service_uuid;
	M_io_handle_t       *handle;
	M_io_layer_t        *layer;
	M_hash_strvp_enum_t *he;

	M_thread_mutex_lock(lock);

	stop_blind_scan();

	uuid = [[[peripheral identifier] UUIDString] UTF8String];
	if (M_hash_strvp_get(ble_devices, uuid, NULL)) {
		/* There is already an association. We have seen multiple
		 * connect events generated. What happens is we get duplicate
		 * events for discovering services and characteristics which
		 * in turn causes multiple connected events to be sent.
		 * We'll ignore this event when the device is already connected. */
		M_thread_mutex_unlock(lock);
		return;
	}

	dev = M_io_ble_device_create(peripheral);

	/* Remove from our cache since we have it held in the device. */
	M_hash_strvp_remove(ble_peripherals, uuid, M_TRUE);

	if (dev == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	/* Try checking for a device uuid. */
	dev->handle = M_hash_strvp_get_direct(ble_waiting, uuid);
	M_hash_strvp_remove(ble_waiting, uuid, M_FALSE);

	/* Couldn't find it by device uuid maybe we're opening using a service uuid. */
	if (dev->handle == NULL && M_hash_strvp_num_keys(ble_waiting_service) != 0) {
		const char *remove_uuid = NULL;

		/* Go through all of this devices services and see if anything is waiting for one. */
		M_hash_strvp_enumerate(dev->services, &he);
		while (M_hash_strvp_enumerate_next(dev->services, he, &service_uuid, NULL)) {
			if (M_hash_strvp_get(ble_waiting_service, service_uuid, (void **)&handle)) {
				dev->handle = handle;
				remove_uuid = service_uuid;
				break;
			}
		}
		M_hash_strvp_enumerate_free(he);

		if (dev->handle != NULL) {
			M_hash_strvp_remove(ble_waiting_service, remove_uuid, M_FALSE);
		}
	}

	if (dev->handle != NULL) {
		/* We have successfully associated! */
		M_hash_strvp_insert(ble_devices, uuid, dev);
		layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
		M_str_cpy(dev->handle->uuid, sizeof(dev->handle->uuid), uuid);
		dev->handle->state     = M_IO_STATE_CONNECTED;
		dev->handle->can_write = M_TRUE;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_io_layer_release(layer);
	} else {
		/* We have a connect event but no io objects are attached.
		 * Close the device since it won't be used by anything.
		 * This could happen due to timing.
		 *
		 * 1. Manager sees device is associated because it has a
		 *    handle in the waiting queue.
		 * 2. The device does not have characteristics so Manager
		 *    requests them.
		 * 3. Handle times out while device is getting characteristics.
		 * 4. Handle is gone so the device will not longer be associated. */
		dispatch_async(dispatch_get_main_queue(), ^{
			[manager disconnectFromDevice:peripheral];
		});
		M_io_ble_device_destroy(dev);
	}

	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

static void M_io_ble_device_set_disconnected(const char *uuid)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (dev->handle == NULL) {
		M_hash_strvp_remove(ble_devices, uuid, M_TRUE);
		M_hash_strvp_remove(ble_peripherals, uuid, M_TRUE);
		M_thread_mutex_unlock(lock);
		return;
	}

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	dev->handle->state = M_IO_STATE_DISCONNECTED;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
	M_io_layer_release(layer);

	M_hash_strvp_remove(ble_devices, uuid, M_TRUE);
	M_hash_strvp_remove(ble_peripherals, uuid, M_TRUE);
	M_thread_mutex_unlock(lock);
}

static void M_io_ble_device_set_error(const char *uuid, const char *error)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	/* A connection failure will trigger this event. A read/write error will also trigger this event.
	 * It's okay to try to stop the blind scan here. If there are waiting devices the scan
	 * won't be stopped. */
	stop_blind_scan();

	if (dev->handle == NULL) {
		M_hash_strvp_remove(ble_devices, uuid, M_TRUE);
		M_hash_strvp_remove(ble_peripherals, uuid, M_TRUE);
		M_thread_mutex_unlock(lock);
		return;
	}

	if (M_str_isempty(error))
		error = "Generic error";

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	dev->handle->state = M_IO_STATE_ERROR;
	M_snprintf(dev->handle->error, sizeof(dev->handle->error), "%s", error);
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO : better error? */);
	M_io_layer_release(layer);

	M_hash_strvp_remove(ble_devices, uuid, M_TRUE);
	M_hash_strvp_remove(ble_peripherals, uuid, M_TRUE);
	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_set_state(const char *uuid, M_io_state_t state, const char *error)
{
	switch (state) {
		case M_IO_STATE_CONNECTED:
			/* There is a function for this state because it needs to do more. */
			break;
		case M_IO_STATE_DISCONNECTED:
			M_io_ble_device_set_disconnected(uuid);
			break;
		case M_IO_STATE_ERROR:
			M_io_ble_device_set_error(uuid, error);
			break;
		case M_IO_STATE_INIT:
		case M_IO_STATE_CONNECTING:
		case M_IO_STATE_DISCONNECTING:
		case M_IO_STATE_LISTENING:
			break;
	}

	M_thread_mutex_lock(lock);
	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_waiting_connect(const char *uuid, const M_list_str_t *service_uuids)
{
	size_t len;
	size_t i;

	M_thread_mutex_lock(lock);

	/* If we've already cached the peripheral then we're in the process
	 * of connecting. We'll return false so we don't try to connect
	 * again while we're connecting. */
	if (M_hash_strvp_get(ble_peripherals, uuid, NULL)) {
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}

	if (M_hash_strvp_get(ble_waiting, uuid, NULL)) {
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	len = M_list_str_len(service_uuids);
	for (i=0; i<len; i++) {
		if (M_hash_strvp_get(ble_waiting_service, M_list_str_at(service_uuids, i), NULL)) {
			M_thread_mutex_unlock(lock);
			return M_TRUE;
		}
	}

	M_thread_mutex_unlock(lock);
	return M_FALSE;
}


M_io_error_t M_io_ble_device_write(const char *uuid, const char *service_uuid, const char *characteristic_uuid, const unsigned char *data, size_t data_len, M_bool blind)
{
	NSData            *ndata;
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *service;
	M_io_layer_t      *layer;
	void              *v;
	CBCharacteristic  *c;
	CBPeripheral      *p;

	if (M_str_isempty(uuid) || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid) || data == NULL || data_len == 0)
		return M_IO_ERROR_INVALID;

	if (!M_thread_mutex_trylock(lock))
		return M_IO_ERROR_WOULDBLOCK;

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTFOUND;
	}

	/* We need the handle. */
	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);

	/* Verify if we can write. */
	if (!dev->handle->can_write) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_WOULDBLOCK;
	}

	/* Check if the service is valid.  */
	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&service)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	/* Check if the characteristic is valid and get it. */
	if (!M_hash_strvp_get(service, characteristic_uuid, &v)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	/* Get the BLE objects we want to operate on. */
	c     = (__bridge CBCharacteristic *)v;
	p     = (__bridge CBPeripheral *)dev->peripheral;
	ndata = [NSData dataWithBytes:data length:data_len];

	dispatch_async(dispatch_get_main_queue(), ^{
		/* Pass the data off for writing. */
		[manager writeDataToPeripherial:p characteristic:c data:ndata blind:blind?YES:NO];
	});

	dev->handle->can_write = blind;

	M_io_layer_release(layer);
	update_seen(uuid);
	M_thread_mutex_unlock(lock);

	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_ble_device_req_val(const char *uuid, const char *service_uuid, const char *characteristic_uuid)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *service;
	M_io_layer_t      *layer;
	void              *v;
	CBCharacteristic  *c;
	CBPeripheral      *p;

	if (M_str_isempty(uuid) || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid))
		return M_IO_ERROR_INVALID;

	if (!M_thread_mutex_trylock(lock))
		return M_IO_ERROR_WOULDBLOCK;

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTFOUND;
	}

	/* We need the handle. */
	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);

	/* Check if the service is valid.  */
	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&service)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	/* Check if the characteristic is valid and get it. */
	if (!M_hash_strvp_get(service, characteristic_uuid, &v)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	c = (__bridge CBCharacteristic *)v;
	p = (__bridge CBPeripheral *)dev->peripheral;

	dispatch_async(dispatch_get_main_queue(), ^{
		/* Pass the data off for writing. */
		[manager requestDataFromPeripherial:p characteristic:c];
	});

	M_io_layer_release(layer);

	update_seen(uuid);
	M_thread_mutex_unlock(lock);

	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_ble_device_req_rssi(const char *uuid)
{
	M_io_ble_device_t *dev;
	CBPeripheral      *p;

	if (M_str_isempty(uuid))
		return M_IO_ERROR_INVALID;

	if (!M_thread_mutex_trylock(lock))
		return M_IO_ERROR_WOULDBLOCK;

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTFOUND;
	}

	/* We need the handle. */
	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	p = (__bridge CBPeripheral *)dev->peripheral;
	dispatch_async(dispatch_get_main_queue(), ^{
		/* Pass the data off for writing. */
		[manager requestRSSIFromPeripheral:p];
	});

	update_seen(uuid);
	M_thread_mutex_unlock(lock);

	return M_IO_ERROR_SUCCESS;
}

void M_io_ble_device_write_complete(const char *uuid)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	/* Inform the io object that we can write again. */
	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	dev->handle->can_write = M_TRUE;
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_read_data(const char *uuid, const char *service_uuid, const char *characteristic_uuid, const unsigned char *data, size_t data_len)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	/* Inform the io object that we read data. */
	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	if (M_io_ble_rdata_queue_add_read(dev->handle->read_queue, service_uuid, characteristic_uuid, data, data_len))
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_read_rssi(const char *uuid, M_int64 rssi)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	/* Inform the io object that we read data. */
	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	if (M_io_ble_rdata_queue_add_rssi(dev->handle->read_queue, rssi))
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_notify_done(const char *uuid, const char *service_uuid, const char *characteristic_uuid)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
	if (M_io_ble_rdata_queue_add_notify(dev->handle->read_queue, service_uuid, characteristic_uuid))
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	update_seen(uuid);
	M_thread_mutex_unlock(lock);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms)
{
	M_io_ble_manager_init();
	if (cbc_manager == nil)
		return M_FALSE;

	dispatch_async(dispatch_get_main_queue(), ^{
		M_event_trigger_t *trigger;
		trigger_wrapper_t *tw;

		tw      = trigger_wrapper_create(NULL, callback, cb_data);
		trigger = M_event_trigger_add(event, scan_done_cb, tw);
		trigger_wrapper_set_trigger(tw, trigger);
		[manager startScan:trigger timeout_ms:timeout_ms];
	});
	return M_TRUE;
}

M_io_ble_enum_t *M_io_ble_enum(void)
{
	M_io_ble_enum_t        *btenum;
	M_hash_strvp_enum_t    *he;
	M_io_ble_enum_device_t *edev;

	M_io_ble_device_reap_seen();

	btenum = M_io_ble_enum_init();

	M_thread_mutex_lock(lock);

	M_hash_strvp_enumerate(ble_seen, &he);
	while (M_hash_strvp_enumerate_next(ble_seen, he, NULL, (void **)&edev)) {
		M_io_ble_enum_add(btenum, edev);
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return btenum;
}

M_bool M_io_ble_initalized(void)
{
	if (cbc_manager == nil || manager == nil)
		return M_FALSE;

	if (manager.initialized)
		return M_TRUE;
	return M_FALSE;
}

M_bool M_io_ble_init_int(void)
{
	M_io_ble_manager_init();
	if (cbc_manager != nil)
		return M_TRUE;
	return M_FALSE;
}

void M_io_ble_connect(M_io_handle_t *handle)
{
	M_io_layer_t *layer;
	M_bool        do_scan = M_TRUE;

	M_thread_mutex_lock(lock);

	/* Check if this device is already open and in use. */
	if (M_hash_strvp_get(ble_devices, handle->uuid, NULL)) {
		M_snprintf(handle->error, sizeof(handle->error), "Device in use");
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		handle->state = M_IO_STATE_ERROR;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO: Better error? */);
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return;
	}

	/* There is a small race condition between this check and using
 	 * the cbc_manager via the get_peripheral_* function where
	 * bluetooth could be disabled between these two. This is
	 * due to this running on a different than than the manager
	 * object which receives the status events. If bluetooth
	 * does turn off between this check and the get_peripheral_*
	 * call, an API misuse warning can be generated but as far
	 * as we've seen there are no adverse effects. */
	if (!manager.state_up) {
		if (manager.powered == M_IO_BLE_MAC_POWERED_OFF) {
			M_snprintf(handle->error, sizeof(handle->error), "Bluetooth is turned off");
		} else {
			M_snprintf(handle->error, sizeof(handle->error), "Bluetooth error");
		}
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		handle->state = M_IO_STATE_ERROR;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_ERROR /* TODO: Better error? */);
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return;
	}

	/* Cache that we want to get a device.
 	 * We'll check if a matching device is
	 * present by asking the OS if it has one
	 * cached. If so we'll use that instead of
	 * scanning. */
	if (!M_str_isempty(handle->uuid)) {
		M_hash_strvp_insert(ble_waiting, handle->uuid, handle);
		if (get_peripheral_by_identifier(handle->uuid)) {
			do_scan = M_FALSE;
		}
	} else {
		M_hash_strvp_insert(ble_waiting_service, handle->service_uuid, handle);
		if (get_peripheral_by_service(handle->service_uuid)) {
			do_scan = M_FALSE;
		}
	}

	/* We only need to do a scan if we couldn't get
 	 * a peripheral directly. Not getting a peripheral
	 * means the OS doesn't have it cached. It could be
	 * because the device hasn't connected before or
	 * got invalidated by going out of range or something.
	 *
	 * We use a blind scan and don't specify a service id
	 * because we might be matching on device uuid. Also,
	 * if another device open request comes in we don't want
	 * to block that scan while we complete this one. Blind
	 * scanning allows us to paralyze opening devices. */
	if (do_scan)
		start_blind_scan();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_close(M_io_handle_t *handle)
{
	M_io_ble_device_t *dev;
	CBPeripheral      *p;

	M_thread_mutex_lock(lock);

	M_hash_strvp_remove(ble_waiting, handle->uuid, M_FALSE);
	M_hash_strvp_remove(ble_waiting_service, handle->service_uuid, M_FALSE);
	if (M_hash_strvp_get(ble_devices, handle->uuid, (void **)&dev)) {
		dev->handle = NULL;
		p = (__bridge CBPeripheral *)dev->peripheral;
		dispatch_async(dispatch_get_main_queue(), ^{
			[manager disconnectFromDevice:p];
		});
	}

	/* We could be here from a timeout during open. We'd never get a disconnect
	 * or error event that would have stopped a blind scan. Stopping blind scans
	 * are protected so we don't need to worry about stopping a scan that should
	 * actually be running. */
	stop_blind_scan();

	M_hash_strvp_remove(ble_devices, handle->uuid, M_TRUE);
	M_hash_strvp_remove(ble_peripherals, handle->uuid, M_TRUE);
	M_thread_mutex_unlock(lock);
}

M_io_error_t M_io_ble_set_device_notify(const char *uuid, const char *service_uuid, const char *characteristic_uuid, M_bool enable)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *service;
	M_io_layer_t      *layer;
	void              *v;
	CBCharacteristic  *c;
	CBPeripheral      *p;

	if (M_str_isempty(uuid) || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid))
		return M_IO_ERROR_INVALID;

	if (!M_thread_mutex_trylock(lock))
		return M_IO_ERROR_WOULDBLOCK;

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTFOUND;
	}

	/* We don't need the handle but we can't subscribe to devices that aren't connected. */
	if (dev->handle == NULL) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTCONNECTED;
	}

	layer = M_io_layer_acquire(dev->handle->io, 0, NULL);

	/* Check if the service is valid.  */
	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&service)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	/* Check if the characteristic is valid and get it. */
	if (!M_hash_strvp_get(service, characteristic_uuid, &v)) {
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_INVALID;
	}

	c = (__bridge CBCharacteristic *)v;
	p = (__bridge CBPeripheral *)dev->peripheral;

	dispatch_async(dispatch_get_main_queue(), ^{
		/* Update notification. */
		[manager requestNotifyFromPeripheral:p forCharacteristic:c fromServiceUUID:[NSString stringWithUTF8String:service_uuid] enabled:enable?YES:NO];
	});

	M_io_layer_release(layer);
	M_thread_mutex_unlock(lock);

	return M_IO_ERROR_SUCCESS;
}

M_list_str_t *M_io_ble_get_device_services(const char *uuid)
{
	const char          *const_temp;
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;
	M_list_str_t        *l   = NULL;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return NULL;
	}

	l = M_list_str_create(M_LIST_STR_NONE);
	M_hash_strvp_enumerate(dev->services, &he);
	while (M_hash_strvp_enumerate_next(dev->services, he, &const_temp, NULL)) {
		M_list_str_insert(l, const_temp);
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return l;
}

M_list_str_t *M_io_ble_get_device_service_characteristics(const char *uuid, const char *service_uuid)
{
	const char          *const_temp;
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;
	M_hash_strvp_t      *service;
	M_list_str_t        *l   = NULL;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return NULL;
	}

	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&service)) {
		M_thread_mutex_unlock(lock);
		return NULL;
	}

	l = M_list_str_create(M_LIST_STR_NONE);
	M_hash_strvp_enumerate(service, &he);
	while (M_hash_strvp_enumerate_next(service, he, &const_temp, NULL)) {
		M_list_str_insert(l, const_temp);
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return l;
}

void M_io_ble_get_device_max_write_sizes(const char *uuid, size_t *with_response, size_t *without_response)
{
	CBPeripheral      *p;
	M_io_ble_device_t *dev;
	NSUInteger         w;
	NSUInteger         wo;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	p  = (__bridge CBPeripheral *)dev->peripheral;
	w  = [p maximumWriteValueLengthForType:CBCharacteristicWriteWithResponse];
	wo = [p maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse];

	M_thread_mutex_unlock(lock);

	*with_response    = (size_t)w;
	*without_response = (size_t)wo;
}
