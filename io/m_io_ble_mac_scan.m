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
	[scanner setManager:nil];
	scanner     = nil;
	cbc_manager = nil;

	M_thread_mutex_destroy(lock);
	M_thread_cond_destroy(cond);
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
	dev->services   = M_list_str_create(M_LIST_STR_SORTASC|M_LIST_STR_SET);
	dev->last_seen  = M_time();

	return dev;
}

static void M_io_ble_device_destroy(M_io_ble_device_t *dev)
{
	CBPeripheral *peripheral;

	if (dev == NULL)
		return;

	/* Decrement the reference count and set to nil so ARC can
 	 * clean up peripheral */
	peripheral = (__bridge_transfer CBPeripheral *)dev->peripheral;
	peripheral = nil;

	M_free(dev->name);
	M_free(dev->mac);
	M_list_str_destroy(dev->services);
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_io_ble_cbc_event_reset(void)
{
	M_io_ble_device_t   *dev;
	M_hash_strvp_enum_t *he;

	M_thread_mutex_lock(lock);
	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
		if (dev->layer == NULL) {
			continue;
		}
		M_io_layer_softevent_add(dev->layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
		/* XXX device needs to be deleted but it's being used by an io object.
 		 * Figure out how to handle this. */
	}

	M_hash_strvp_destroy(ble_devices, M_TRUE);
	ble_devices = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
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
	dev = M_hash_strvp_get_direct(ble_devices, mac);
	if (dev == NULL) {
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

	if (M_str_isempty(mac) || M_str_isempty(uuid))
		return;

	M_thread_mutex_lock(lock);

	dev = M_hash_strvp_get_direct(ble_devices, mac);
	if (dev == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_list_str_insert(dev->services, uuid);
	dev->last_seen = M_time();

	M_thread_mutex_unlock(lock);
}

void M_io_ble_device_clear_services(const char *mac)
{
	M_io_ble_device_t *dev;

	if (M_str_isempty(mac))
		return;

	M_thread_mutex_lock(lock);

	dev = M_hash_strvp_get_direct(ble_devices, mac);
	if (dev == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	M_list_str_destroy(dev->services);
	dev->services = M_list_str_create(M_LIST_STR_SORTASC|M_LIST_STR_SET);

	M_thread_mutex_unlock(lock);
}

M_bool M_io_ble_device_need_read_services(CBPeripheral *peripheral)
{
	M_io_ble_device_t *dev;
	const char        *mac;
	M_bool             ret = M_FALSE;

	mac = [[[peripheral identifier] UUIDString] UTF8String];

	M_thread_mutex_lock(lock);

	dev = M_hash_strvp_get_direct(ble_devices, mac);
	if (dev == NULL) {
		M_thread_mutex_unlock(lock);
		return M_TRUE;
	}

	if (M_list_str_len(dev->services) == 0)
		ret = M_TRUE;

	M_thread_mutex_unlock(lock);

	return ret;
}

#if 0
/* Connected to the cbc manager. Multiple apps can have a device connected
 * to it's cbc manager. */
M_bool M_io_ble_device_connected(const char *mac)
{
	M_io_ble_device_t         *dev;
	__block CBPeripheralState  state;
	M_bool                     ret = M_FALSE;

	if (M_str_isempty(mac))
		return;

	M_thread_mutex_lock(lock);

	dev = M_hash_strvp_get_direct(ble_devices, mac);
	if (dev == NULL) {
		M_thread_mutex_unlock(lock);
		return;
	}

	dispatch_sync(dispatch_get_main_queue(), ^{
		CBPeripheral *p = (__bridge CBPeripheral *)dev->peripheral;
		state = p.state;
	});

	if (state == CBPeripheralStateConnected || state == CBPeripheralStateConnecting)
		ret = M_TRUE;

	M_thread_mutex_unlock(lock);

	return ret;
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms)
{
	static dispatch_once_t  d = 0;

	/* Setup the scanning objects. */
	dispatch_once(&d, ^{
		ble_devices = M_hash_strvp_create(8, 75, M_HASH_STRVP_NONE, (void (*)(void *))M_io_ble_device_destroy);
		lock        = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
		cond        = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
		scanner     = [M_io_ble_mac_scanner m_io_ble_mac_scanner];
		cbc_manager = [[CBCentralManager alloc] initWithDelegate:scanner queue:nil];
		[scanner setManager:cbc_manager];

		M_library_cleanup_register(M_io_ble_cleanup, NULL);
	});

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
	M_io_ble_device_t   *dev;
	size_t               len;
	size_t               i;

	btenum = M_io_ble_enum_init();

	M_thread_mutex_lock(lock);

	M_hash_strvp_enumerate(ble_devices, &he);
	while (M_hash_strvp_enumerate_next(ble_devices, he, NULL, (void **)&dev)) {
		len = M_list_str_len(dev->services);
		for (i=0; i<len; i++) {
			M_io_ble_enum_add(btenum, dev->name, dev->mac, M_list_str_at(dev->services, i), dev->last_seen, dev->layer==NULL?M_FALSE:M_TRUE);
		}
	}

	M_thread_mutex_unlock(lock);

	return btenum;
}
