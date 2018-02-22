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

#ifndef __M_IO_BLE_H__
#define __M_IO_BLE_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_time.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_ble Bluetooth LE (Low Energy) IO functions
 *  \ingroup m_eventio_base
 *
 * Bluetooth LE (Low Energy) IO functions.
 *
 * Supported OS:
 * - iOS
 * - macOS
 *
 * \note iOS also supports an Apple proprietary system known as Made for iPhone/iPod/iPad (MFi).
 * MFi uses ble but is handled differently. It's supposed though the m_io_mfi layer.
 * MFi is also known as the External Accessory / EAAccessory protocol.
 *
 *
 * ## Overview
 *
 * BLE was designed to keep energy consumption to a minimum and allow for seamless device
 * access. Unlike Bluetooth classic, devices are not paired to the system. Typical use
 * is to scan for available devices, inspect their services, and connect to a device that
 * provides services the application wants to use. A good example is a heart rate monitor.
 *
 * A health app doesn't care which heart rate monitor is being used it only cares about
 * getting heart rate data. Typically, the user will be presented with a list of suitable
 * devices in case multiple devices are detected (for example, multiple people going on
 * a bike ride together).
 *
 * Since there is no pairing the device must be found by scanning for available devices.
 * All devices that have been found during a scan (excluding ones that have been pruned)
 * will be listed as part of device enumeration. This means devices may no longer be present.
 * Such as an iPhone being seen during scanning and later the owner of the phone leaving
 * the room. There are no OS level events to notify that this has happened.
 *
 * When necessary, a scan can be initiated is by trying to connect to a device.
 * Opening a device requires specifying a device identifier or service UUID and if
 * not found a scan will be started internally for either the duration of the timeout
 * or until the device has been found. This can cause a delay between trying to open
 * a device and receiving CONNECT or ERROR events.
 *
 * Device identifiers will vary between OS's. macOS for example assigns a device specific
 * UUID to devices it sees. Android returns the device's MAC address. There is no way
 * to read a device's MAC address on macOS. Identifiers are subject to change periodically.
 * iOS will randomly change the device's MAC address every few hours to prevent tracking.
 *
 * BLE devices provide services and there can be multiple services. Services provide
 * characteristics and there can be multiple characteristics per service. Both
 * services and characteristics can be defined using standardized profiles. See
 * the Bluetooth GATT specifications.
 *
 * Since there are multiple, potentially, read and write end points it is required
 * to specify the service and characteristic UUIDs. A write event must have them
 * specified using the M_io_meta_t and associated BLE meta functions. A read will
 * fill a provided meta object with the service and characteristic the data came from.
 * This means only the read and write meta functions can be use with BLE. The non-meta
 * functions will return an error.
 *
 * Characteristics can have multiple properties.
 * - Read
 * - Notify
 * - Indicate
 * - Write
 * - Write without response
 *
 * BLE by default is not a stream-based protocol like serial, HID, or Bluetooth classic.
 * Characteristics with the read property can be requested to read data. This is an async
 * request. M_io_ble facilitates this by using M_io_write_meta with a property indicator
 * that specifies data is not being written but a request for read is being sent.
 *
 * Characteristics with the notify or indicate property can be subscribed to which will
 * have them issue read events similar to a stream protocol. Reads will still require a meta
 * object to specify which service and characteristic the data is from. Manual read requests
 * may still be necessary. Notify and Indicate events are left to the device to imitate.
 * The device may have internal rules which limit how often events are triggered. For example
 * a heart rate monitor could notify every 2 seconds even though it's reading every 100 ms. A
 * time service might send an event every second or it might send an event every minute.
 *
 * Characteristics won't receive read events by default. They need to be subscribed to first.
 * Subscripts will not service a disconnect or destroy of an io object. Also, not all characteristics
 * support this property even if it supports read. Conversely some support notify/indicate but
 * not read.
 *
 * Write will write data to the device and the OS will issue an event whether the write
 * succeeded or failed. Mstdlib uses this to determine if there was a write error and will
 * block subsequent writes (returns WOULDBLOCK) until an outstanding write has completed.
 *
 * Write without response is a blind write. No result is requested from the OS. The state
 * of the write is not known after it is sent.
 *
 *
 * ## macOS requirements
 *
 * BLE events are only delivered to the main run loop. This is a design decision by Apple.
 * It is not possible to use an different run loop to receive events like can be done
 * with classic Bluetooth or HID. BLE events are non-blocking so there shouldn't be any
 * performance impact with the events being delivered. As little work as possible is
 * performed during event processing to limit any impact of this design requirement.
 *
 * A C application will need to manually start the macOS main runloop, otherwise no events
 * will be delivered and no BLE operations will work.
 *
 * ### Examples
 *
 * #### Application that scans for 30 seconds and enumerates all devices and their services that were seen.
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_enum.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * CFRunLoopRef  mrl = NULL;
 *
 * static void scan_done_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
 * {
 *     M_io_ble_enum_t *btenum;
 *     M_list_str_t    *service_uuids;
 *     size_t           len;
 *     size_t           len2;
 *     size_t           i;
 *     size_t           j;
 *
 *     (void)event;
 *     (void)type;
 *     (void)io;
 *     (void)cb_arg;
 *
 *     btenum = M_io_ble_enum();
 *
 *     len = M_io_ble_enum_count(btenum);
 *     M_printf("Num devs = %zu\n", len);
 *     for (i=0; i<len; i++) {
 *         M_printf("Device:\n");
 *         M_printf("\tName: %s\n", M_io_ble_enum_name(btenum, i));
 *         M_printf("\tIdentifier: %s\n", M_io_ble_enum_identifier(btenum, i));
 *         M_printf("\tLast Seen: %llu\n", M_io_ble_enum_last_seen(btenum, i));
 *         M_printf("\tSerivces:\n");
 *         service_uuids = M_io_ble_enum_service_uuids(btenum, i);
 *         len2 = M_list_str_len(service_uuids);
 *         for (j=0; j<len2; j++) {
 *             M_printf("\t\t: %s\n", M_list_str_at(service_uuids, j));
 *         }
 *         M_list_str_destroy(service_uuids);
 *     }
 *
 *     M_io_ble_enum_destroy(btenum);
 *
 *     if (mrl != NULL)
 *         CFRunLoopStop(mrl);
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     M_io_ble_scan(el, scan_done_cb, NULL, 30000);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     // 5 sec timeout.
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that scans for 30 seconds and connects to a specified device which has been seen and cached (hopefully).
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_connect.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             M_printf("DISCONNECTED!!!\n");
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *         case M_EVENT_TYPE_WRITE:
 *         case M_EVENT_TYPE_ACCEPT:
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void scan_done_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
 * {
 *     M_io_ble_enum_t *btenum;
 *     size_t           len;
 *     size_t           i;
 *
 *     (void)event;
 *     (void)type;
 *     (void)io;
 *     (void)cb_arg;
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     M_event_add(el, dio, events, NULL);
 *
 *     M_printf("SCAN DONE\n");
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     M_io_ble_scan(el, scan_done_cb, NULL, 5000);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that implicitly scans and connects to a specified device which has not been seen and cached.
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_connect_noscan.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             M_io_disconnect(dio);
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             M_printf("DISCONNECTED!!!\n");
 *         case M_EVENT_TYPE_READ:
 *         case M_EVENT_TYPE_WRITE:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     M_event_add(el, dio, events, NULL);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that lists services and their characteristics for a specific device
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_interrogate.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 * 
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 * 
 * #include <CoreFoundation/CoreFoundation.h>
 * 
 * M_event_t    *el;
 * M_io_t       *dio;
 * CFRunLoopRef  mrl = NULL;
 * 
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_list_str_t *services;
 *     M_list_str_t *characteristics;
 *     size_t        len;
 *     size_t        len2;
 *     size_t        i;
 *     size_t        j;
 * 
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 * 
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 * 
 *             services = M_io_ble_get_services(dio);
 *             len      = M_list_str_len(services);
 *             for (i=0; i<len; i++) {
 *                 M_printf("service = %s:\n", M_list_str_at(services, i));
 *                 characteristics = M_io_ble_get_service_characteristics(dio, M_list_str_at(services, i));
 *                 len2            = M_list_str_len(characteristics);
 *                 for (j=0; j<len2; j++) {
 *                     M_printf("\t%s\n", M_list_str_at(characteristics, j));
 *                 }
 *                 M_list_str_destroy(characteristics);
 *             }
 *             M_list_str_destroy(services);
 * 
 *             M_io_disconnect(dio);
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             M_printf("DISCONNECTED!!!\n");
 *         case M_EVENT_TYPE_READ:
 *         case M_EVENT_TYPE_WRITE:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 * 
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 * 
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 * 
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 * 
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 * 
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     M_event_add(el, dio, events, NULL);
 * 
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 * 
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 * 
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that reads by polling the device for a read using a write
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_readp.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * M_io_meta_t  *wmeta;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_int64      rssi  = M_INT64_MIN;
 *     M_io_meta_t *rmeta = NULL;
 *     const char  *service_uuid;
 *     const char  *characteristic_uuid;
 *     char         msg[256];
 *     size_t       len;
 *
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             M_io_write_meta(dio, NULL, 0, NULL, wmeta);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             rmeta = M_io_meta_create();
 *             M_io_read_meta(dio, msg, sizeof(msg)-1, &len, rmeta);
 *             msg[len]            = '\0';
 *             service_uuid        = M_io_ble_meta_get_service(dio, rmeta);
 *             characteristic_uuid = M_io_ble_meta_get_characteristic(dio, rmeta);
 *
 *             M_printf("%s - %s: %s\n", service_uuid, characteristic_uuid, msg);
 *
 *             M_io_meta_destroy(rmeta);
 *
 *             M_thread_sleep(100000);
 *             M_io_write_meta(dio, NULL, 0, NULL, wmeta);
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(dio, msg, sizeof(msg));
 *             M_printf("ERROR: %s\n", msg);
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED)
 *                 M_printf("DISCONNECTED!!!\n");
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     wmeta = M_io_meta_create();
 *     M_io_ble_meta_set_write_type(dio, wmeta, M_IO_BLE_WTYPE_REQVAL);
 *     M_io_ble_meta_set_service(dio, wmeta, "1111");
 *     M_io_ble_meta_set_characteristic(dio, wmeta, "2222");
 *     M_event_add(el, dio, events, NULL);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     M_io_meta_destroy(wmeta);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that reads by requesting notification when value changes
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_readn.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_int64      rssi  = M_INT64_MIN;
 *     M_io_meta_t *meta  = NULL;
 *     const char  *service_uuid;
 *     const char  *characteristic_uuid;
 *     char         msg[256];
 *     size_t       len;
 *
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             // XXX: Set notify service and characteristic.
 *             meta = M_io_meta_create();
 *             M_io_ble_meta_set_service(dio, meta, "1111");
 *             M_io_ble_meta_set_characteristic(dio, meta, "2222");
 *             M_io_ble_meta_set_notify(dio, M_TRUE);
 *             M_io_write_meta(dio, NULL, 0, NULL, meta);
 *             M_io_meta_destroy(meta);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             meta = M_io_meta_create();
 *             M_io_read_meta(dio, msg, sizeof(msg), &len, meta);
 *             if (M_io_ble_meta_get_read_type(io, meta) == M_IO_BLE_RTYPE_READ) {
 *                 msg[len]            = '\0';
 *                 service_uuid        = M_io_ble_meta_get_service(dio, meta);
 *                 characteristic_uuid = M_io_ble_meta_get_characteristic(dio, meta);
 *                 M_printf("%s - %s: %s\n", service_uuid, characteristic_uuid, msg);
 *             }
 *             M_io_meta_destroy(meta);
 *             M_thread_sleep(100000);
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(dio, msg, sizeof(msg));
 *             M_printf("ERROR: %s\n", msg);
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED)
 *                 M_printf("DISCONNECTED!!!\n");
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     M_event_add(el, dio, events, NULL);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that uses writes to request current RSSI value.
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_rssi.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * M_io_meta_t  *wmeta;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_int64      rssi  = M_INT64_MIN;
 *     M_io_meta_t *rmeta = NULL;
 *     char         msg[256];
 *
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             M_io_write_meta(dio, NULL, 0, NULL, wmeta);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             rmeta = M_io_meta_create();
 *             M_io_read_meta(dio, NULL, 0, NULL, rmeta);
 *             M_io_ble_meta_get_rssi(dio, rmeta, &rssi);
 *             M_io_meta_destroy(rmeta);
 *
 *             M_printf("RSSI = %lld\n", rssi);
 *
 *             M_thread_sleep(100000);
 *             M_io_write_meta(dio, NULL, 0, NULL, wmeta);
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(dio, msg, sizeof(msg));
 *             M_printf("ERROR: %s\n", msg);
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED)
 *                 M_printf("DISCONNECTED!!!\n");
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     wmeta = M_io_meta_create();
 *     M_io_ble_meta_set_write_type(dio, wmeta, M_IO_BLE_WTYPE_REQRSSI);
 *     M_event_add(el, dio, events, NULL);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     M_io_meta_destroy(wmeta);
 *
 *     return 0;
 * }
 * \endcode
 *
 * #### Application that writes
 *
 * \code{.c}
 * // Build:
 * // clang -g -fobjc-arc -framework CoreFoundation test_ble_write.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 * //
 * // Run:
 * // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_thread.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/io/m_io_ble.h>
 *
 * #include <CoreFoundation/CoreFoundation.h>
 *
 * M_event_t    *el;
 * M_io_t       *dio;
 * M_io_meta_t  *meta;
 * CFRunLoopRef  mrl = NULL;
 *
 * void events(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     size_t        len = 0;
 *     static size_t num = 1;
 *     char          msg[256];
 *     M_io_error_t  ret;
 *
 *     (void)el;
 *     (void)io;
 *     (void)thunk;
 *
 *     M_snprintf(msg, sizeof(msg), "%zu", num++);
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED!!!\n");
 *             M_io_write_meta(dio, (const unsigned char *)msg, M_str_len(msg), &len, meta);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             M_printf("WRITE\n");
 *             M_io_write_meta(dio, (const unsigned char *)msg, M_str_len(msg), &len, meta);
 *             M_thread_sleep(100000);
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(dio, msg, sizeof(msg));
 *             M_printf("ERROR: %s\n", msg);
 *         case M_EVENT_TYPE_DISCONNECTED:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED)
 *                 M_printf("DISCONNECTED!!!\n");
 *             M_io_destroy(dio);
 *             if (mrl != NULL)
 *                 CFRunLoopStop(mrl);
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 * }
 *
 * static void *run_el(void *arg)
 * {
 *     (void)arg;
 *     M_event_loop(el, M_TIMEOUT_INF);
 *     return NULL;
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_threadid_t     el_thread;
 *     M_thread_attr_t *tattr;
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     tattr = M_thread_attr_create();
 *     M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *     el_thread = M_thread_create(tattr, run_el, NULL);
 *     M_thread_attr_destroy(tattr);
 *
 *     // XXX: Set the id to the device you want to connect to.
 *     M_io_ble_create(&dio, "92BD9AC6-3BC8-4B24-8BF8-AE583AFE3ED4", 5000);
 *     meta = M_io_meta_create();
 *     M_io_ble_meta_set_service(dio, meta, "1111");
 *     M_io_ble_meta_set_characteristic(dio, meta, "2222");
 *     M_event_add(el, dio, events, NULL);
 *
 *     mrl = CFRunLoopGetCurrent();
 *     CFRunLoopRun();
 *
 *     M_event_done_with_disconnect(el, 5*1000);
 *     M_thread_join(el_thread, NULL);
 *
 *     M_io_meta_destroy(meta);
 *
 *     return 0;
 * }
 * \endcode
 * @{
 */

/*! Meta property types used by M_io_write_meta.
 *
 * Specifies how the write should function. */
typedef enum {
	M_IO_BLE_WTYPE_WRITE = 0,   /*!< Normal write. Waits for confirmation data was
	                                 written before writes can take place again. */
	M_IO_BLE_WTYPE_WRITENORESP, /*!< Write without confirmation response. Blind write. */
	M_IO_BLE_WTYPE_REQVAL,      /*!< Request value for service and characteristic. Not
	                                 an actual write but a pseudo write to poll for a
	                                 read event. */
	M_IO_BLE_WTYPE_REQRSSI,     /*!< Request RSSI value. */
	M_IO_BLE_WTYPE_REQNOTIFY    /*!< Request to change notify state. */
} M_io_ble_wtype_t;


/*! Meta types used by M_io_read_meta.
 *
 * Specifies what type of read is being returned.
 */
typedef enum {
	M_IO_BLE_RTYPE_READ = 0, /*!< Regular read of data from service and characteristic. */
	M_IO_BLE_RTYPE_RSSI,     /*!< RSSI data read. Use M_io_ble_meta_get_rssi. */
	M_IO_BLE_RTYPE_NOTIFY    /*!< Notify state changed. */
} M_io_ble_rtype_t;


struct M_io_ble_enum;
typedef struct M_io_ble_enum M_io_ble_enum_t;


/*! Start a BLE scan.
 *
 * A scan needs to take place for nearby devices to be found. Once found they will
 * appear in an enumeration.
 *
 * Opening a known device does not require explicitly scanning. Scanning
 * will happen implicitly if the device has not been seen before.
 *
 * \warning On macOS the callback will never be called if the main event loop is not running!
 *
 * \param[out] event      Event handle to receive scan events.
 * \param[in] callback   User-specified callback to call when the scan finishes
 * \param[in] cb_data    Optional. User-specified data supplied to user-specified callback when
 *                        executed.
 * \param[in]  timeout_ms How long the scan should run before stopping.
 *                        0 will default to 1 minute. Scanning for devices
 *                        can take a long time. During testing of a simple
 *                        pedometer times upwards of 50 seconds were seen
 *                        before the device was detected while sitting
 *                        6 inches away. A maximum of 5 minutes is allowed.
 *                        Any amount larger will be reduced to the max.
 *
 * \return M_TRUE if the scan was started and the callback will be called.
 *         Otherwise M_FALSE, the callback will not be called.
 */
M_API M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms);


/*! Create a ble enumeration object.
 *
 * You must call M_io_ble_scan to populate the enumeration.
 * Failing to do so will result in an empty enumeration.
 *
 * Use to determine what ble devices are connected and
 * what services are being offered. This is a
 * list of associated devices not necessarily what's actively connected.
 *
 * The enumeration is based on available services. Meaning a device may
 * be listed multiple times if it exposes multiple services.
 *
 * \return Bluetooth enumeration object.
 */
M_API M_io_ble_enum_t *M_io_ble_enum(void);


/*! Destroy a ble enumeration object.
 *
 * \param[in] btenum Bluetooth enumeration object.
 */
M_API void M_io_ble_enum_destroy(M_io_ble_enum_t *btenum);


/*! Number of ble objects in the enumeration.
 *
 * \param[in] btenum Bluetooth enumeration object.
 *
 * \return Count of ble devices.
 */
M_API size_t M_io_ble_enum_count(const M_io_ble_enum_t *btenum);


/*! UUID of ble device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_identifier(const M_io_ble_enum_t *btenum, size_t idx);


/*! Name of ble device as reported by the device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_name(const M_io_ble_enum_t *btenum, size_t idx);


/*! UUIDs of services reported by device.
 *
 * This could be empty if the device has not been opened. Some devices
 * do not advertise this unless they are opened and interrogated.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in ble enumeration.
 *
 * \return String list of UUIDs.
 */
M_API M_list_str_t *M_io_ble_enum_service_uuids(const M_io_ble_enum_t *btenum, size_t idx);


/*! Last time the device was seen.
 *
 * Run a scan to ensure this is up to date. Opening a device will also
 * update the last seen time.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in ble enumeration.
 *
 * \return time.
 */
M_API M_time_t M_io_ble_enum_last_seen(const M_io_ble_enum_t *btenum, size_t idx);


/*! Create a ble connection.
 *
 * \param[out] io_out     io object for communication.
 * \param[in]  identifier Required identifier of the device.
 * \param[in]  timeout_ms If the device has not already been seen a scan will
 *                        be performed. This time out is how long we should
 *                        wait to search for the device.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_ble_create(M_io_t **io_out, const char *identifier, M_uint64 timeout_ms);


/*! Create a ble connection to a device that provides a given service.
 *
 * This connects to the first device found exposing the requested service.
 *
 * \param[out] io_out      io object for communication.
 * \param[in] service_uuid UUID of service.
 * \param[in]  timeout_ms  If the device has not already been seen a scan will
 *                         be performed. This time out is how long we should
 *                         wait to search for the device.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_ble_create_with_service(M_io_t **io_out, const char *service_uuid, M_uint64 timeout_ms);


/*! Get a list of service UUIDs provided by the device.
 *
 * \param[in] io io object.
 *
 * \return List of strings.
 */
M_API M_list_str_t *M_io_ble_get_services(M_io_t *io);


/*! Get a list of characteristic UUIDs provided a service provided by the device.
 *
 * \param[in] io           io object.
 * \param[in] service_uuid UUID of service.
 *
 * \return List of strings.
 */
M_API M_list_str_t *M_io_ble_get_service_characteristics(M_io_t *io, const char *service_uuid);


/*! Get the maximum write sizes from an io object.
 *
 * Queries the highest BLE layer in the stack, if there are more than one.
 *
 * \param[in]  io               io object.
 * \param[out] with_response    The maximum size that will receive a response.
 * \param[out] without_response The maximum size that will not receive a response.
 */
M_API void M_io_ble_get_max_write_sizes(M_io_t *io, size_t *with_response, size_t *without_response);


/*! Get the service associated with a meta object.
 *
 * \param[in] io   io object.
 * \param[in] meta Meta.
 *
 * \return UUID
 */
M_API const char *M_io_ble_meta_get_service(M_io_t *io, M_io_meta_t *meta);


/*! Get the characteristic associated with a meta object.
 *
 * \param[in] io   io object.
 * \param[in] meta Meta.
 *
 * \return UUID
 */
M_API const char *M_io_ble_meta_get_characteristic(M_io_t *io, M_io_meta_t *meta);


/*! Get the write type.
 *
 * \param[in] io   io object.
 * \param[in] meta Meta.
 *
 * \return type.
 */
M_API M_io_ble_wtype_t M_io_ble_meta_get_write_type(M_io_t *io, M_io_meta_t *meta);


/*! Get the read type.
 *
 * \param[in] io   io object.
 * \param[in] meta Meta.
 *
 * \return type.
 */
M_API M_io_ble_rtype_t M_io_ble_meta_get_read_type(M_io_t *io, M_io_meta_t *meta);


/*! Get the RSSI value from an RSSI read.
 *
 * RSSI value is in decibels.
 *
 * \param[in]  io   io object.
 * \param[in]  meta Meta.
 * \param[out] rssi RSSI value.
 *
 * \return M_TRUE if RSSI read. Otherwise, M_FALSE.
 */
M_API M_bool M_io_ble_meta_get_rssi(M_io_t *io, M_io_meta_t *meta, M_int64 *rssi);


/*! Set the service associated with a meta object.
 *
 * \param[in] io           io object.
 * \param[in] meta         Meta.
 * \param[in] service_uuid UUID of service.
 */
M_API void M_io_ble_meta_set_service(M_io_t *io, M_io_meta_t *meta, const char *service_uuid);


/*! Set the characteristic associated with a meta object.
 *
 * \param[in] io                  io object.
 * \param[in] meta                Meta.
 * \param[in] characteristic_uuid UUID of characteristic.
 */
M_API void M_io_ble_meta_set_characteristic(M_io_t *io, M_io_meta_t *meta, const char *characteristic_uuid);


/*! Set whether to receive notifications for characterisic data changes
 *
 * If not called the default is to enable notifications.
 *
 * Not all characteristic's support notifications. If not supported polling with M_io_write_meta
 * using M_IO_BLE_WTYPE_REQVAL is the only way to retrieve the current value.
 * \param[in] io     io object.
 * \param[in] meta   Meta.
 * \param[in] enable Enable or disable receiving notifications.
 */
M_API void M_io_ble_meta_set_notify(M_io_t *io, M_io_meta_t *meta, M_bool enable);


/*! Set whether a write should be blind.
 *
 * If the type is not set, the default is to have writes
 * wait for confirmation response before subsequent writes will be allowed.
 *
 * \param[in] io   io object.
 * \param[in] meta Meta.
 * \param[in] type Property controlling
 */
M_API void M_io_ble_meta_set_write_type(M_io_t *io, M_io_meta_t *meta, M_io_ble_wtype_t type);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLE_H__ */

