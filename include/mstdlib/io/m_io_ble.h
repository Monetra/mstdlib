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
 * @{
 */


struct M_io_ble_enum;
typedef struct M_io_ble_enum M_io_ble_enum_t;

/*! Start a BLE scan.
 *
 * A scan needs to take place for nearby devices to be found. Once found they will
 * appear in an enumeration.
 *
 * Will trigger an OTHER event when finished scanning either due to timeout
 * or M_io_ble_scan_stop being called. This event will only be triggerd once.
 * If the scan finishes, OTHER is triggered, calling M_io_ble_scan_stop after will
 * _not_ trigger a second OTHER event.
 *
 * Opening a known device does not require explicitly scanning.
 *
 * Call m_io_destroy once finished with the scan io object.
 *
 * \param[out] io_out     io object for communication.
 * \param[in]  timeout_ms How long the scan should run before stopping.
 *                        0 will default to 1 minute. Scanning for devices
 *                        can take a long time. During testing of a simple
 *                        pedometer times upwards of 50 seconds were seen
 *                        before the device was detected while sitting
 *                        6 inches away.
 */
void M_io_ble_scan_start(M_io_t **io_out, M_int64 timeout_ms);


/* Stop a BLE scan.
 *
 * \param[out] io_out io object used for scanning for nearby devices.
 */
void M_io_ble_scan_stop(M_io_t *io);


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


/*! Name of service reported by device.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_service_name(const M_io_ble_enum_t *btenum, size_t idx);


/*! Uuid of service reported by device.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in ble enumeration.
 *
 * \return String.
 */
M_API const char *M_io_ble_enum_service_uuid(const M_io_ble_enum_t *btenum, size_t idx);


/*! Create a ble connection.
 *
 * \param[out] io_out io object for communication.
 * \param[in]  mac    Required MAC of the device.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_ble_create(M_io_t **io_out, const char *mac);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLE_H__ */

