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
 * getting hear rate data. Typically, the user will be presented with a list of suitable
 * devices in case multiple devices are detected (for example, multiple people going on
 * a bike ride together).
 *
 * Since there is no pairing the device much be found by scanning for available devices.
 * This happen in two ways. First M_io_ble_scan will look for and cache devices that can
 * be seen by the OS. During a scan, stale devices (over 15 minutes old) will be removed.
 *
 * All devices that have been found during a scan (excluding ones that have been pruned)
 * be listed as part of device enumeration. This means devices may no longer be present.
 * Such as an iPhone being seen during scanning and later the owner of the phone leaving
 * the room. There are no OS level events to notify that this has happened. At which
 * point the seen device cache may be stale.
 *
 * The other way a scan can be initiated is by trying to connect to a device that has
 * not been seen. Opening a device requires specifying the UUID of the device and if
 * not found a scan will be started internally for either the duration of the timeout
 * or until the device has been found. This can cause a delay between trying to open
 * a device and receiving CONNECT or ERROR events.
 *
 * BLE devices provide services and there can be multiple services. Services provide
 * characteristics and there can be multiple characteristics per service. Both
 * services and characteristics can be defined using standardized profiles. See
 * the Bluetooth GATT specifications.
 *
 * Since there are multiple, potentially, read and write end points it is required
 * to specify the service and characteristic UUIDs. A write event much have them
 * specified using the M_io_meta_t and associated BLE meta functions. A read will
 * fill a provided meta object with the service and characteristic the data came from.
 * This means only the read and write meta functions can be use with BLE. The none-meta
 * functions will return an error.
 *
 * Characteristics can have multiple properties.
 * - Read
 * - Notify
 * - Indicate
 * - Write
 * - Write without response
 *
 * BLE by default is not a stream based protocol like serial, HID, or Bluetooth classic.
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
 * Characteristics won't receive read events be default. They need to be subscribed to first.
 * Subscripts will not service a disconnect or destroy of an io object. Also, not all characteristics
 * support this property even if it supports read. Conversely some support notify/indicate but
 * not read.
 *
 * Write will write data to the device and the OS will issue an event whether the write as
 * successful or failed. Mstdlib uses this to determine if there was a write error and will
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
 * with classic Bluetooth or HID. BLE events are none blocking so there shouldn't be
 * performance impact with the events being delivered. As little work as possible is
 * performed during event processing to limit any impact of this design requirement.
 *
 * A C application will need to manually start the macOS main runloop otherwise no events
 * will be delivered and no BLE operations will work.
 *
 * Example application that scans for 30 seconds and enumerates all devices and their
 * services that were seen.
 *
 * \code{.c}
 *     // Build:
 *     // clang -g -fobjc-arc -framework CoreFoundation test_ble_enum.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
 *     //
 *     // Run:
 *     // DYLD_LIBRARY_PATH="../../build/lib/" ./a.out
 *
 *     #include <mstdlib/mstdlib.h>
 *     #include <mstdlib/mstdlib_thread.h>
 *     #include <mstdlib/mstdlib_io.h>
 *     #include <mstdlib/io/m_io_ble.h>
 *
 *     #include <CoreFoundation/CoreFoundation.h>
 *
 *     M_event_t    *el;
 *     CFRunLoopRef  mrl = NULL;
 *
 *     static void scan_done_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
 *     {
 *         M_io_ble_enum_t *btenum;
 *         size_t           len;
 *         size_t           i;
 *
 *         (void)event;
 *         (void)type;
 *         (void)io;
 *         (void)cb_arg;
 *
 *         btenum = M_io_ble_enum();
 *
 *         len = M_io_ble_enum_count(btenum);
 *         M_printf("Num devs = %zu\n", len);
 *         for (i=0; i<len; i++) {
 *             M_printf("Device:\n");
 *             M_printf("\tName: %s\n", M_io_ble_enum_name(btenum, i));
 *             M_printf("\tUUID: %s\n", M_io_ble_enum_uuid(btenum, i));
 *             M_printf("\tConnected: %s\n", M_io_ble_enum_connected(btenum, i)?"Yes":"No");
 *             M_printf("\tLast Seen: %llu\n", M_io_ble_enum_last_seen(btenum, i));
 *             M_printf("\tSerivce: %s\n", M_io_ble_enum_service_uuid(btenum, i));
 *         }
 *
 *         M_io_ble_enum_destroy(btenum);
 *
 *         if (mrl != NULL)
 *             CFRunLoopStop(mrl);
 *     }
 *
 *     static void *run_el(void *arg)
 *     {
 *         (void)arg;
 *         M_event_loop(el, M_TIMEOUT_INF);
 *         return NULL;
 *     }
 *
 *     int main(int argc, char **argv)
 *     {
 *         M_threadid_t     el_thread;
 *         M_thread_attr_t *tattr;
 *
 *         el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *         tattr = M_thread_attr_create();
 *         M_thread_attr_set_create_joinable(tattr, M_TRUE);
 *         el_thread = M_thread_create(tattr, run_el, NULL);
 *         M_thread_attr_destroy(tattr);
 *
 *         M_io_ble_scan(el, scan_done_cb, NULL, 30000);
 *
 *         mrl = CFRunLoopGetCurrent();
 *         CFRunLoopRun();
 *
 *         // 5 sec timeout.
 *         M_event_done_with_disconnect(el, 5*1000);
 *         M_thread_join(el_thread, NULL);
 *
 *         return 0;
 *     }
 * \endcode
 *
 *
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
	M_IO_BLE_WTYPE_REQRSSI      /*!< Request RSSI value. */
} M_io_ble_wtype_t;


/*! Meta types used by M_io_read_meta.
 *
 * Specifies what type of read is being returned.
 */
typedef enum {
	M_IO_BLE_RTYPE_READ = 0, /*!< Regular read of data from service and characteristic. */
	M_IO_BLE_RTYPE_RSSI,     /*!< RSSI data read. Use M_io_ble_meta_get_rssi. */
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
 * Call m_io_destroy once finished with the scan io object.
 *
 * \param[out] event      Event handle to receive scan events.
 *  \param[in] callback   User-specified callback to call when the scan finishes
 *  \param[in] cb_data    Optional. User-specified data supplied to user-specified callback when
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


/*! Name of ble device as reported by the device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_name(const M_io_ble_enum_t *btenum, size_t idx);


/*! UUID of ble device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_uuid(const M_io_ble_enum_t *btenum, size_t idx);


/*! Whether the device is connected.
 *
 * This does not mean it is currently in use. It means the device is present
 * and connected to the OS. This is mainly a way to determine if a device
 * in the enumeration is still within range and can be used.
 *
 * Not all systems are able to report the connected status making this function
 * less useful than you would think.
 *
 * \return M_TRUE if the device is connected, otherwise M_FALSE.
 *         If it is not possible to determine the connected status this
 *         function will return M_TRUE.
 */
M_API M_bool M_io_ble_enum_connected(const M_io_ble_enum_t *btenum, size_t idx);


/*! Uuid of service reported by device.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_service_uuid(const M_io_ble_enum_t *btenum, size_t idx);


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
 * \param[in]  uuid       Required uuid of the device.
 * \param[in]  timeout_ms If the device has not already been seen a scan will
 *                        be performed. This time out is how long we should
 *                        wait to search for the device.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_ble_create(M_io_t **io_out, const char *uuid, M_uint64 timeout_ms);



/*! Request read event's when the characteristic's value changes.
 *
 * Not all characteristic's support notifications. If not supported polling with M_io_write_meta
 * using M_IO_BLE_WTYPE_REQVAL is the only way to retrieve the current value.
 *
 * \param[in] io                  io object.
 * \param[in] service_uuid        UUID of service.
 * \param[in] characteristic_uuid UUID of characteristic.
 * \param[in] enable              Receive notifications?
 *
 * \return Result
 */
M_API M_io_error_t M_io_ble_set_notify(M_io_t *io, const char *service_uuid, const char *characteristic_uuid, M_bool enable);


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
M_API const char *M_io_ble_meta_get_charateristic(M_io_t *io, M_io_meta_t *meta);


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
M_API void M_io_ble_meta_set_charateristic(M_io_t *io, M_io_meta_t *meta, const char *characteristic_uuid);


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

