/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
 * Bluetooth IO functions
 *
 * Supported OS:
 * - Android
 * - iOS
 *
 * Note: iOS only supports the External Accessory EAAccessory protocol. Devices that
 * are part of the Made for iPhone/iPod/iPad (MFi) program. BLE is not supported
 * on iOS at this time.
 *
 * @{
 */


struct M_io_bluetooth_enum;
typedef struct M_io_bluetooth_enum M_io_bluetooth_enum_t;


/*! Create a bluetooth enumeration object.
 *
 * Use to determine what bluetooth devices are connected. On some OS's this may
 * be a list of associated devices not necessarily what's actively connected.
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


/*! Number of services offered by the device.
 *
 * \param[in] btenum Bluetooth enumeration object.
 * \param[in] idx    Index in bluetooth enumeration.
 *
 * \return Service count.
 */
M_API size_t M_io_bluetooth_enum_service_count(const M_io_bluetooth_enum_t *btenum, size_t idx);


/*! Service identifying information.
 *
 * \param[in]  btenum Bluetooth enumeration object.
 * \param[in]  idx    Index in bluetooth enumeration.
 * \param[in]  sidx   Service index for entry in bluetooth enumeration.
 * \param[out] name   Name of service. Optional. Can be returned as NULL if name not present.
 * \param[out] uuid   UUID of service. Optional.
 *
 * \return M_TRUE on sucess. Otherwise M_FALSE.
 */
M_API M_bool M_io_bluetooth_enum_service_id(const M_io_bluetooth_enum_t *btenum, size_t idx, size_t sidx, const char **name, const char **uuid);


/*! Create a bluetooth connection.
 *
 * \param[out] io_out io object for communication.
 * \param[in]  mac    Required MAC of the device.
 * \param[in]  uuid   Option UUID of the device. If not specified the first device matching
 *                    the mac will be used.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_bluetooth_create(M_io_t **io_out, const char *mac, const char *uuid);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLUETOOTH_H__ */
