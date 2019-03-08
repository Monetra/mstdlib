/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_IO_BLUETOOTH_H__
#define __M_IO_BLUETOOTH_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_bluetooth Bluetooth IO functions
 *  \ingroup m_eventio_base
 * 
 * Bluetooth IO functions.
 *
 * This is a connectivity layer and uses rfcomm. Protocols,
 * such as AVDTP, should be implemented at a layer above this one.
 * The generic rfcomm uuid that the vast majority of devices use is
 * 00001101-0000-1000-8000-00805f9b34fb.
 *
 * Supported OS:
 * - Android
 * - macOS
 *
 * @{
 */


struct M_io_bluetooth_enum;
typedef struct M_io_bluetooth_enum M_io_bluetooth_enum_t;


/*! Create a bluetooth enumeration object.
 *
 * Use to determine what bluetooth devices are connected and
 * what services are being offered. This is a
 * list of associated devices not necessarily what's actively connected.
 *
 * The enumeration is based on available services. Meaning a device may 
 * be listed multiple times if it exposes multiple services.
 *
 * \return Bluetooth enumeration object.
 */
M_API M_io_bluetooth_enum_t *M_io_bluetooth_enum(void);


/*! Destroy a bluetooth enumeration object.
 *
 * \param[in] btenum Bluetooth enumeration object.
 */
M_API void M_io_bluetooth_enum_destroy(M_io_bluetooth_enum_t *btenum);


/*! Number of bluetooth objects in the enumeration.
 * 
 * \param[in] btenum Bluetooth enumeration object.
 *
 * \return Count of bluetooth devices.
 */
M_API size_t M_io_bluetooth_enum_count(const M_io_bluetooth_enum_t *btenum);


/*! Name of bluetooth device as reported by the device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in bluetooth enumeration.
 *
 * \return String.
 */
M_API const char *M_io_bluetooth_enum_name(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! MAC of bluetooth device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in bluetooth enumeration.
 *
 * \return String.
 */
M_API const char *M_io_bluetooth_enum_mac(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! Whether the device is connected.
 *
 * Not all systems are able to report the connected status making this function
 * less useful than you would think.
 *
 * \return M_TRUE if the device is connected, otherwise M_FALSE.
 *         If it is not possible to determine the connected status this
 *         function will return M_TRUE.
 */
M_API M_bool M_io_bluetooth_enum_connected(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! Name of service reported by device.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in bluetooth enumeration.
 *
 * \return String.
 */
M_API const char *M_io_bluetooth_enum_service_name(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! Uuid of service reported by device.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in bluetooth enumeration.
 *
 * \return String.
 */
M_API const char *M_io_bluetooth_enum_service_uuid(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! Create a bluetooth connection.
 *
 * \param[out] io_out io object for communication.
 * \param[in]  mac    Required MAC of the device.
 * \param[in]  uuid   Optional UUID of the device. For rfcomm (used by this io interface) the uuid
 *                    is almost always 00001101-0000-1000-8000-00805f9b34fb unless the device is
 *                    providing multiple services. Such as a device that can do multiple things like
 *                    bar code scanner, and integrated printer. If not specified the generic
 *                    rfcomm generic uuid will be used.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_bluetooth_create(M_io_t **io_out, const char *mac, const char *uuid);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLUETOOTH_H__ */
