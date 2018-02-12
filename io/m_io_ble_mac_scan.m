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

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_ble.h>
#include "m_io_int.h"
#include "m_io_ble_int.h"
#include "m_io_ble_mac.h"
#include "m_io_ble_mac_scanner.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * CBCentralManager will only trigger events on the main run loop. If it's not
 * running there won't be any events!
 *
 * The scanner object is only going to be run on on the main queue and this
 * need to ensure that happens. This is to prevent any cross thread access and
 * event delivery. Always disptach these async if it's sync and the main run
 * loop is not running we'll get a segfault due to bad access.
 *
 * All access to the ble_devices cache needs to be locked.
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_event_trigger_t  *trigger;
	M_event_callback_t  cb;
	void               *cb_arg;
} trigger_wrapper_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_ble_mac_scanner *scanner     = nil;
static CBCentralManager     *cbc_manager = nil;
static M_hash_strvp_t       *ble_devices = NULL; /* key = mac, val = M_io_ble_device_t. */
static M_hash_strvp_t       *ble_waiting = NULL; /* key = mac, val = M_io_handle_t. */
static M_thread_mutex_t     *lock        = NULL;
static M_thread_cond_t      *cond        = NULL;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_io_ble_cleanup(void *arg)
{
	(void)arg;

	if (cbc_manager == nil)
		return;

	/* Kill the references so ARC sees the object
 	 * isn't used and will clean it up. */
	dispatch_sync(dispatch_get_main_queue(), ^{
		[scanner setManager:nil];
	});
	scanner     = nil;
	cbc_manager = nil;

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

	cbc = (__bridge_transfer CBCharacteristic *)c;
	cbc = nil;
}

static M_io_ble_device_t *M_io_ble_device_create(CFTypeRef peripheral, const char *name, const char *mac)
{
	M_io_ble_device_t *dev;

	if (peripheral == nil || M_str_isempty(mac))
		return NULL;

	dev             = M_malloc_zero(sizeof(*dev));
	dev->peripheral = peripheral;
	dev->name       = M_strdup(name);
	dev->mac        = M_strdup(mac);
	dev->services   = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_services_destroy);
	dev->last_seen  = M_time();

	return dev;
}

static void M_io_ble_device_destroy(M_io_ble_device_t *dev)
{
	CBPeripheral *peripheral;

	if (dev == NULL)
		return;

	M_free(dev->name);
	M_free(dev->mac);
	M_hash_strvp_destroy(dev->services, M_TRUE);

	/* Decrement the reference count and set to nil so ARC can
 	 * clean up peripheral */
	peripheral = (__bridge_transfer CBPeripheral *)dev->peripheral;
	peripheral = nil;

	M_free(dev);
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

static void M_io_ble_manager_init(void)
{
	static dispatch_once_t  d = 0;

	/* Setup the scanning objects. */
	dispatch_once(&d, ^{
		ble_devices = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
		ble_waiting = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, NULL);
		lock        = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
		cond        = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
		scanner     = [M_io_ble_mac_scanner m_io_ble_mac_scanner];
		cbc_manager = [[CBCentralManager alloc] initWithDelegate:scanner queue:nil];
		[scanner setManager:cbc_manager];

		M_library_cleanup_register(M_io_ble_cleanup, NULL);
	});
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_io_ble_cbc_event_reset(void)
{
	M_io_layer_t        *layer;
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;

	M_thread_mutex_lock(lock);
	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
		if (dev->handle == NULL) {
			continue;
		}
		layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
		M_io_layer_release(layer);
		dev->handle = NULL;
	}

	M_hash_strvp_destroy(ble_devices, M_TRUE);
	ble_devices = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
	M_hash_strvp_destroy(ble_waiting, M_TRUE);
	ble_waiting = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, NULL);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_cache_device(CFTypeRef peripheral)
{
	CBPeripheral      *p;
	M_io_ble_device_t *dev;
	const char        *mac;

	M_thread_mutex_lock(lock);

	p   = (__bridge CBPeripheral *)peripheral;
	mac = [[[p identifier] UUIDString] UTF8String];
	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		dev = M_io_ble_device_create(peripheral, [p.name UTF8String], mac);
		if (dev == NULL) {
			M_thread_mutex_unlock(lock);
			return;
		}
		M_hash_strvp_insert(ble_devices, mac, dev);
	} else {
		if (peripheral != dev->peripheral) {
			/* Let ARC know we don't need this anymore.
			 * Can't just overwrite because we need to
			 * do the bridge transfer for ARC. */
			p               = (__bridge_transfer CBPeripheral *)dev->peripheral;
			p               = nil;
			dev->peripheral = peripheral;
		}
		dev->last_seen = M_time();
	}

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_add_serivce(const char *mac, const char *uuid)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *characteristics;

	if (M_str_isempty(mac) || M_str_isempty(uuid))
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	characteristics = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_characteristics_destroy);
	M_hash_strvp_insert(dev->services, uuid, characteristics);

	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_add_characteristic(const char *mac, const char *service_uuid, const char *characteristic_uuid, CFTypeRef cbc)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *characteristics;

	if (M_str_isempty(mac))
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev) || dev->services == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&characteristics)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_hash_strvp_insert(dev->services, characteristic_uuid, cbc);
	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);

}

void M_io_ble_device_clear_services(const char *mac)
{
	M_io_ble_device_t *dev;

	if (M_str_isempty(mac))
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_hash_strvp_destroy(dev->services, M_TRUE);
	dev->services = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_services_destroy);

	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_need_read_services(const char *mac)
{
	M_io_ble_device_t *dev;
	M_bool             ret = M_FALSE;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	if (M_hash_strvp_num_keys(dev->services) == 0)
		ret = M_TRUE;

	M_thread_mutex_unlock(lock);

	return ret;
}

M_bool M_io_ble_device_need_read_characteristics(const char *mac, const char *service_uuid)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *characteristics;
	M_bool             ret = M_FALSE;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	if (M_hash_strvp_num_keys(dev->services) == 0)
		ret = M_TRUE;
	
	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&characteristics)) {
		ret = M_FALSE;
	} else if (M_hash_strvp_num_keys(characteristics) == 0) {
		ret = M_TRUE;
	}

	M_thread_mutex_unlock(lock);

	return ret;
}

void M_io_ble_device_set_state(const char *mac, M_io_state_t state)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	dev->state = state;
	if (dev->handle)
		dev->handle->state = state;

	switch (state) {
		case M_IO_STATE_CONNECTED:
			/* If we found a device then we want to associate it. */
			if (dev->handle == NULL) {
				dev->handle = M_hash_strvp_get_direct(ble_waiting, mac);
				M_hash_strvp_remove(ble_waiting, mac, M_FALSE);
			}
			if (dev->handle != NULL) {
				layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED);
				M_io_layer_release(layer);
			}
			break;
		case M_IO_STATE_DISCONNECTED:
			if (dev->handle != NULL) {
				layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
				M_io_layer_release(layer);
				dev->handle = NULL;
			}
			break;
		case M_IO_STATE_ERROR:
			if (dev->handle != NULL) {
				layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
				M_snprintf(dev->handle->error, sizeof(dev->handle->error), "Error");
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
				M_io_layer_release(layer);
			}
			break;
		case M_IO_STATE_INIT:
		case M_IO_STATE_CONNECTING:
		case M_IO_STATE_DISCONNECTING:
		case M_IO_STATE_LISTENING:
			break;
	}

	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_is_associated(const char *mac)
{
	M_io_ble_device_t *dev;
	M_bool             ret = M_FALSE;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, mac, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}

	if (dev->handle != NULL)
		ret = M_TRUE;

	M_thread_mutex_unlock(lock);

	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms)
{
	M_io_ble_manager_init();
	if (cbc_manager == NULL)
		return M_FALSE;

	dispatch_async(dispatch_get_main_queue(), ^{
		M_event_trigger_t *trigger;
		trigger_wrapper_t *tw;

		tw      = trigger_wrapper_create(NULL, callback, cb_data);
		trigger = M_event_trigger_add(event, scan_done_cb, tw);
		trigger_wrapper_set_trigger(tw, trigger);
		[scanner startScan:trigger timeout_ms:timeout_ms];
	});
	return M_TRUE;
}

M_io_ble_enum_t *M_io_ble_enum(void)
{
	M_io_ble_enum_t     *btenum;
	M_hash_strvp_enum_t *he;
	M_hash_strvp_enum_t *hes;
	M_io_ble_device_t   *dev;
	const char          *const_temp;

	btenum = M_io_ble_enum_init();

	M_thread_mutex_lock(lock);

	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
		M_hash_strvp_enumerate(dev->services, &hes);
		while (M_hash_strvp_enumerate_next(dev->services, hes, &const_temp, NULL)) {
			M_io_ble_enum_add(btenum, dev->name, dev->mac, const_temp, dev->last_seen, dev->state==M_IO_STATE_CONNECTED?M_TRUE:M_FALSE);
		}
		M_hash_strvp_enumerate_free(hes);
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return btenum;
}

void M_io_ble_connect(M_io_handle_t *handle)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;
	CBPeripheral      *peripheral;
	M_bool             in_use = M_FALSE;
	__block BOOL       ret;

	M_thread_mutex_lock(lock);

	if (M_hash_strvp_get(ble_devices, handle->mac, (void **)&dev)) {
		if (dev->handle != NULL) {
			in_use = M_TRUE;
		} else {
			dev->handle = handle;
		}
		peripheral = (__bridge CBPeripheral *)dev->peripheral;
		ret        = YES;
		dispatch_sync(dispatch_get_main_queue(), ^{
			ret = [scanner connectToDevice:peripheral];
		});
		if (!ret) {
			M_snprintf(handle->error, sizeof(handle->error), "Device connect fatal error");
			layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
			M_io_layer_release(layer);
		}
	} else {
		M_hash_strvp_insert(ble_waiting, handle->mac, handle);
		/* XXX: Start scan. */
	}

	if (in_use) {
		M_snprintf(handle->error, sizeof(handle->error), "Device in use");
		layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
	}

	M_thread_mutex_unlock(lock);
}

void M_io_ble_close(M_io_handle_t *handle)
{
	M_io_ble_device_t *dev;
	CBPeripheral      *peripheral;

	M_thread_mutex_lock(lock);

	M_hash_strvp_remove(ble_waiting, handle->mac, M_FALSE);
	if (M_hash_strvp_get(ble_devices, handle->mac, (void **)&dev)) {
		dev->handle = NULL;
		peripheral  = (__bridge CBPeripheral *)dev->peripheral;
		dispatch_async(dispatch_get_main_queue(), ^{
			[scanner disconnectFromDevice:peripheral];
		});
	}

	handle->state = M_IO_STATE_DISCONNECTED;
	M_thread_mutex_unlock(lock);
}
