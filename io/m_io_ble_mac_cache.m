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
 * event delivery. Always disptach these async if it's sync and the main run
 * loop is not running we'll get a segfault due to bad access.
 *
 * All access to the ble_devices, and ble_waiting caches need to be locked.
 *
 * Devices need to be seen from a scan before they can be used. As such we
 * cache them in the ble_devices cache. They are not open/connected. macOS/iOS
 * uses an object for accessing devices. The only way to get a device object is
 * by scanning. We cache the object so we can open, close, etc. the device
 * when we need it.
 *
 * It is possible to open a device (if present) that hasn't been scanned by
 * starting a scan and when found connecting to the device. This is the purpose
 * of the *_blind_scan functions. The blind scan will stop when the device is
 * connected, there is an error, or the M_io_ble_create timeout is triggered.
 * This limits scan time to the minimum necessary.
 *
 * The ble_waiting cache hold device handles we want to associate to a device
 * in the ble_devices cache. On connect a device is checked there is a waiting
 * handle and will be associated at that time.
 *
 * Devices in the ble_devices cache will have an M_io_handle_t associated with
 * them when connected. Read and write events will marshal data into and out
 * of the handle. Event's will also be triggered on the handle. If a handle
 * is not associated with a device then events will be ignored.
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
	dispatch_sync(dispatch_get_main_queue(), ^{
		[manager setManager:nil];
	});
	manager     = nil;
	cbc_manager = nil;

	M_hash_strvp_destroy(ble_devices, M_TRUE);
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

	cbc = (__bridge_transfer CBCharacteristic *)c;
	cbc = nil;
}

static M_io_ble_device_t *M_io_ble_device_create(CFTypeRef peripheral, const char *name, const char *uuid)
{
	M_io_ble_device_t *dev;

	if (peripheral == nil || M_str_isempty(uuid))
		return NULL;

	dev             = M_malloc_zero(sizeof(*dev));
	dev->peripheral = peripheral;
	dev->name       = M_strdup(name);
	dev->uuid       = M_strdup(uuid);
	dev->services   = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_services_destroy);
	dev->last_seen  = M_time();

	return dev;
}

static void M_io_ble_device_destroy(M_io_ble_device_t *dev)
{
	CBPeripheral *p;

	if (dev == NULL)
		return;

	M_free(dev->name);
	M_free(dev->uuid);
	M_hash_strvp_destroy(dev->services, M_TRUE);

	/* Decrement the reference count and set to nil so ARC can
 	 * clean up peripheral */
	p = (__bridge_transfer CBPeripheral *)dev->peripheral;
	p = nil;

	M_free(dev);
}

static void M_io_ble_waiting_destroy(M_io_handle_t *handle)
{
	M_io_layer_t *layer;

	if (handle == NULL)
		return;

	layer = M_io_layer_acquire(handle->io, 0, NULL);
	if (handle->io == NULL) {
		return;
		M_io_layer_release(layer);
	}
	M_snprintf(handle->error, sizeof(handle->error), "Timeout");
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
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

static void M_io_ble_manager_init(void)
{
	static dispatch_once_t  d = 0;

	/* Setup the scanning objects. */
	dispatch_once(&d, ^{
		ble_devices         = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
		ble_waiting         = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);
		ble_waiting_service = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);
		lock                = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
		cond                = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
		manager             = [M_io_ble_mac_manager m_io_ble_mac_manager];
		cbc_manager         = [[CBCentralManager alloc] initWithDelegate:manager queue:nil];
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

static M_bool connect_to_device(M_io_ble_device_t *dev, M_io_handle_t *handle, M_bool *in_use)
{
	CBPeripheral  *p;
	__block BOOL   ret;
	M_bool         myin_use;

	if (in_use == NULL)
		in_use = &myin_use;
	*in_use = M_FALSE;

	if (dev == NULL)
		return M_FALSE;

	if (dev->handle != NULL) {
		*in_use = M_TRUE;
		return M_FALSE;
	}

	dev->handle = handle;
	p           = (__bridge CBPeripheral *)dev->peripheral;
	ret         = YES;
	dispatch_sync(dispatch_get_main_queue(), ^{
		ret = [manager connectToDevice:p];
	});

	if (!ret) {
		dev->handle = NULL;
		return M_FALSE;
	}
	return M_TRUE;
}

static void stop_blind_scan(void)
{
	/* Don't stop the scan if there are handles waiting
	 * for a device to be found. */
	if (M_hash_strvp_num_keys(ble_waiting) != 0 && M_hash_strvp_num_keys(ble_waiting_service) == 0)
		return;

	dispatch_async(dispatch_get_main_queue(), ^{
		[manager stopScanBlind];
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
	M_hash_strvp_enumerate_free(he);

	M_hash_strvp_destroy(ble_devices, M_TRUE);
	ble_devices = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
	/* Will notify all waiting io objects with an error. */
	M_hash_strvp_destroy(ble_waiting, M_TRUE);
	ble_waiting = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);
	M_hash_strvp_destroy(ble_waiting_service, M_TRUE);
	ble_waiting_service = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_waiting_destroy);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_cache_device(CFTypeRef peripheral)
{
	CBPeripheral      *p;
	M_io_ble_device_t *dev;
	const char        *uuid;

	M_thread_mutex_lock(lock);

	p    = (__bridge CBPeripheral *)peripheral;
	uuid = [[[p identifier] UUIDString] UTF8String];
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		dev = M_io_ble_device_create(peripheral, [p.name UTF8String], uuid);
		if (dev == NULL) {
			M_thread_mutex_unlock(lock);
			return;
		}
		M_hash_strvp_insert(ble_devices, uuid, dev);
	} else {
		if (peripheral != dev->peripheral) {
			/* Let ARC know we don't need this anymore.
			 * Can't just overwrite because we need to
			 * do the bridge transfer for ARC. */
			p               = (__bridge_transfer CBPeripheral *)dev->peripheral;
			p               = nil;
			dev->peripheral = peripheral;
		}
	}
	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_add_serivce(const char *uuid, const char *service_uuid)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *characteristics;

	if (M_str_isempty(uuid) || M_str_isempty(service_uuid))
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	characteristics = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_characteristics_destroy);
	M_hash_strvp_insert(dev->services, service_uuid, characteristics);

	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_add_characteristic(const char *uuid, const char *service_uuid, const char *characteristic_uuid, CFTypeRef cbc)
{
	M_io_ble_device_t *dev;
	M_hash_strvp_t    *service;

	if (M_str_isempty(uuid) || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid) || cbc == nil)
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev) || dev->services == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	if (!M_hash_strvp_get(dev->services, service_uuid, (void **)&service)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_hash_strvp_insert(service, characteristic_uuid, cbc);
	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_clear_services(const char *uuid)
{
	M_io_ble_device_t *dev;

	if (M_str_isempty(uuid))
		return;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_hash_strvp_destroy(dev->services, M_TRUE);
	dev->services = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC, (void (*)(void *))M_io_ble_services_destroy);

	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_need_read_services(const char *uuid)
{
	M_io_ble_device_t *dev;
	M_bool             ret = M_FALSE;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	if (M_hash_strvp_num_keys(dev->services) == 0)
		ret = M_TRUE;

	M_thread_mutex_unlock(lock);

	return ret;
}

M_bool M_io_ble_device_have_all_characteristics(const char *uuid)
{
	M_io_ble_device_t   *dev;
	M_hash_strvp_t      *service;
	M_hash_strvp_enum_t *he;
	M_bool               ret = M_TRUE;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}

	if (M_hash_strvp_enumerate(dev->services, &he) == 0) {
		/* No services means no characteristics. */
		M_hash_strvp_enumerate_free(he);
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}
	while (M_hash_strvp_enumerate_next(dev->services, he, NULL, (void **)&service)) {
		/* No characteristics for a service means we don't have all of them. */
		if (M_hash_strvp_num_keys(service) == 0) {
			ret = M_FALSE;
			break;
		}
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return ret;
}

void M_io_ble_device_update_name(const char *uuid, const char *name)
{
	M_io_ble_device_t *dev;

	if (M_str_isempty(uuid))
		return;

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_free(dev->name);
	dev->name = M_strdup(name);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_scan_finished(void)
{
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;
	const char          *uuid;
	M_list_str_t        *uuids;
	M_time_t             expire;
	size_t               len;
	size_t               i;

	M_thread_mutex_lock(lock);

	/* Clear out all old devices that may not
	 * be around anymore. */
	uuids = M_list_str_create(M_LIST_STR_NONE);
	expire = M_time()-LAST_SEEN_EXPIRE;
	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, &uuid, (void **)&dev)) {
		if (dev->handle == NULL && dev->last_seen < expire) {
			M_list_str_insert(uuids, uuid);
		}
	}
	M_hash_strvp_enumerate_free(he);

	len = M_list_str_len(uuids);
	for (i=0; i<len; i++) {
		M_hash_strvp_remove(ble_devices, M_list_str_at(uuids, i), M_TRUE);
	}
	M_list_str_destroy(uuids);

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_set_state(const char *uuid, M_io_state_t state, const char *error)
{
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;
	M_io_layer_t        *layer;
	M_io_handle_t       *handle;
	CBPeripheral        *p;
	const char          *service_uuid;

	M_thread_mutex_lock(lock);

	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return;
	}

	dev->last_seen = M_time();
	dev->state     = state;

	switch (state) {
		case M_IO_STATE_CONNECTED:
			/* If we found a device and it's already associated then we want try to associate it. */
			if (dev->handle == NULL) {
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
							M_str_cpy(handle->uuid, sizeof(handle->uuid), dev->uuid);
							dev->handle = handle;
							remove_uuid = service_uuid;
							break;
						}
					}
					M_hash_strvp_enumerate_free(he);

					M_hash_strvp_remove(ble_waiting_service, remove_uuid, M_FALSE);
				}
			}

			if (dev->handle != NULL) {
				/* We have successfully associated! */
				layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
				dev->handle->state     = state;
				dev->handle->can_write = M_TRUE;
				M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED);
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
				p = (__bridge CBPeripheral *)dev->peripheral;
				dispatch_async(dispatch_get_main_queue(), ^{
					[manager disconnectFromDevice:p];
				});
			}
			stop_blind_scan();
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
				if (M_str_isempty(error)) {
					error = "Generic error";
				}
				M_snprintf(dev->handle->error, sizeof(dev->handle->error), "%s", error);
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
				M_io_layer_release(layer);
			}

			/* Error events we remove the device from the cache. Assume it can't be used anymore. */
			M_hash_strvp_remove(ble_devices, uuid, M_TRUE);
			dev = NULL;

			/* A connection failure will trigger this event. A read/write error will also trigger this event.
			 * It's okay to try to stop the blind scan here. If there are waiting devices the scan
			 * won't be stopped. */
			stop_blind_scan();
			break;
		case M_IO_STATE_INIT:
		case M_IO_STATE_CONNECTING:
		case M_IO_STATE_DISCONNECTING:
		case M_IO_STATE_LISTENING:
			break;
	}

	/* DO not use dev after this point because it could have been destroyed. */

	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_is_associated(const char *uuid)
{
	M_io_ble_device_t   *dev = NULL;
	M_hash_strvp_enum_t *he;
	const char          *service_uuid;
	M_bool               ret = M_FALSE;

	M_thread_mutex_lock(lock);

	if (M_hash_strvp_get(ble_devices, uuid, (void **)&dev) && dev->handle != NULL)
		ret = M_TRUE;

	if (!ret && M_hash_strvp_get(ble_waiting, uuid, NULL))
		ret = M_TRUE;

	if (!ret && dev != NULL && M_hash_strvp_num_keys(ble_waiting_service) != 0) {
		M_hash_strvp_enumerate(dev->services, &he);
		while (M_hash_strvp_enumerate_next(dev->services, he, &service_uuid, NULL)) {
			if (M_hash_strvp_get(ble_waiting_service, service_uuid, NULL)) {
				ret = M_TRUE;
				break;
			}
		}
		M_hash_strvp_enumerate_free(he);
	}

	M_thread_mutex_unlock(lock);

	return ret;
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

	M_thread_mutex_lock(lock);

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
	dev->last_seen         = M_time();

	M_io_layer_release(layer);
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

	M_thread_mutex_lock(lock);

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

	dev->last_seen = M_time();
	M_io_layer_release(layer);
	M_thread_mutex_unlock(lock);

	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_ble_device_req_rssi(const char *uuid)
{
	M_io_ble_device_t *dev;
	M_io_layer_t      *layer;
	CBPeripheral      *p;

	if (M_str_isempty(uuid))
		return M_IO_ERROR_INVALID;

	M_thread_mutex_lock(lock);

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

	p = (__bridge CBPeripheral *)dev->peripheral;
	dispatch_async(dispatch_get_main_queue(), ^{
		/* Pass the data off for writing. */
		[manager requestRSSIFromPeripheral:p];
	});

	dev->last_seen = M_time();
	M_io_layer_release(layer);
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
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
	M_io_layer_release(layer);

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
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
	M_io_layer_release(layer);

	M_io_layer_release(layer);
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
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
	M_io_layer_release(layer);

	M_io_layer_release(layer);
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
			M_io_ble_enum_add(btenum, dev->name, dev->uuid, const_temp, dev->last_seen, dev->state==M_IO_STATE_CONNECTED?M_TRUE:M_FALSE);
		}
		M_hash_strvp_enumerate_free(hes);
	}
	M_hash_strvp_enumerate_free(he);

	M_thread_mutex_unlock(lock);

	return btenum;
}

M_bool M_io_ble_connect(M_io_handle_t *handle)
{
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;
	M_io_layer_t        *layer;
	const char          *service_uuid;
	M_bool               in_use = M_FALSE;
	M_bool               ret    = M_TRUE;
	M_bool               found  = M_FALSE;

	M_io_ble_manager_init();
	if (cbc_manager == nil) {
		M_snprintf(handle->error, sizeof(handle->error), "Failed to initalize BLE manager");
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		return M_FALSE;
	}

	M_thread_mutex_lock(lock);

	if (!M_str_isempty(handle->uuid)) {
		if (M_hash_strvp_get(ble_devices, handle->uuid, (void **)&dev)) {
			ret   = connect_to_device(dev, handle, &in_use);
			found = M_TRUE;
		}
	} else {
		/* Try looking for service. */
		M_hash_strvp_enumerate(ble_devices, &he);
		while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
			if (M_hash_strvp_get(dev->services, service_uuid, NULL)) {
				in_use = M_FALSE;
				ret    = connect_to_device(dev, handle, &in_use);
				if (!ret && in_use) {
					/* Couldn't use this device because it's in use. Keep looking. */
					found = M_TRUE;
					continue;
				}
				found = M_TRUE;
				break;
			}
		}
		M_hash_strvp_enumerate_free(he);
	}

	/* We couldn't find a cached device so lets search for one. */
	if (!found) {
		if (!M_str_isempty(handle->uuid)) {
			M_hash_strvp_insert(ble_waiting, handle->uuid, handle);
		} else {
			M_hash_strvp_insert(ble_waiting_service, handle->uuid, handle);
		}
		start_blind_scan();
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	if (in_use) {
		M_snprintf(handle->error, sizeof(handle->error), "Device in use");
		layer = M_io_layer_acquire(dev->handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}

	if (!ret) {
		M_snprintf(handle->error, sizeof(handle->error), "Device connect fatal error: Already in use or BLE not avaliable");
		layer = M_io_layer_acquire(handle->io, 0, NULL);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		M_io_layer_release(layer);
		M_thread_mutex_unlock(lock);
		return M_FALSE;
	}

	/* We successfully started the device connection sequence. */
	M_thread_mutex_unlock(lock);
	return M_TRUE;
}

void M_io_ble_close(M_io_handle_t *handle, M_bool kill)
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

	stop_blind_scan();

	handle->state = M_IO_STATE_DISCONNECTED;

	if (kill)
		M_hash_strvp_remove(ble_devices, handle->uuid, M_TRUE);

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

	M_thread_mutex_lock(lock);

	/* Get the associated device. */
	if (!M_hash_strvp_get(ble_devices, uuid, (void **)&dev)) {
		M_thread_mutex_unlock(lock);
		return M_IO_ERROR_NOTFOUND;
	}

	/* We don't need the handle but we can't subscribe to devices that aren't connected. */
	if (dev->handle == NULL || dev->state != M_IO_STATE_CONNECTED) {
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
		[manager requestNotifyFromPeripheral:p forCharacteristic:c enabled:enable?YES:NO];
	});

	dev->last_seen = M_time();
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
