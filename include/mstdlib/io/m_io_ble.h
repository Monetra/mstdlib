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
 * Note: iOS also supports an Apple proprietary system known as Made for iPhone/iPod/iPad (MFi).
 * MFi uses ble but is handled differently. It's supposed though the m_io_mfi layer.
 * MFi is also known as the External Accessory / EAAccessory protocol.
 *
 *
 * ## macOS requirements
 *
 * BLE events are only delivered to the main run loop. This is a design decision by Apple.
 * It is not possible to use an different run loop to receive events like can be done
 * with classic bluetooth or HID. BLE events are none blocking so there shouldn't be
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
 *             M_printf("\tMac: %s\n", M_io_ble_enum_mac(btenum, i));
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
 *         return 0;
 *     }
 * \endcode
 *
 *
 * @{
 */


struct M_io_ble_enum;
typedef struct M_io_ble_enum M_io_ble_enum_t;

/*! Start a BLE scan.
 *
 * A scan needs to take place for nearby devices to be found. Once found they will
 * appear in an enumeration.
 *
 * Opening a known device does not require explicitly scanning. Scanning
 * will happen implicity if the device has not been seen before.
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
M_bool M_io_ble_scan(M_event_t *event, M_event_callback_t callback, void *cb_data, M_uint64 timeout_ms);


/*! Create a ble enumeration object.
 *
 * You must call M_io_ble_scan_start to populate the enumeration.
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


/*! MAC of ble device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_mac(const M_io_ble_enum_t *btenum, size_t idx);


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
 * \param[in]  mac        Required MAC of the device.
 * \param[in]  timeout_ms If the device has not already been seen a scan will
 *                        be performed. This time out is how long we should
 *                        wait to search for the device.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_ble_create(M_io_t **io_out, const char *mac, M_uint64 timeout_ms);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLE_H__ */

