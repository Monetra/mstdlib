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

/*! \addtogroup m_io_mfi Made for iPod/iPhone/iPad IO functions
 *  \ingroup m_eventio_base
 * 
 * Made for iPod/iPhone/iPad IO functions
 *
 * Supported OS:
 * - iOS
 *
 * This is the External Accessory EAAccessory protocol. Devices that
 * are part of the Made for iPhone/iPod/iPad (MFi) program. BLE is not supported
 * by this interface use m_io_mfi for BLE on iOS.
 *
 * @{
 */


struct M_io_mfi_enum;
typedef struct M_io_mfi_enum M_io_mfi_enum_t;


/*! Create a mfi enumeration object.
 *
 * Use to determine what mfi devices are connected. On some OS's this may
 * be a list of associated devices not necessarily what's actively connected.
 *
 * A device can expose multiple protocols.
 *
 * \return mfi enumeration object.
 */
M_API M_io_mfi_enum_t *M_io_mfi_enum(void);


/*! Destroy a mfi enumeration object.
 *
 * \param[in] mfienum mfi enumeration object.
 */
M_API void M_io_mfi_enum_destroy(M_io_mfi_enum_t *mfienum);


/*! Number of mfi objects in the enumeration.
 * 
 * \param[in] mfienum mfi enumeration object.
 *
 * \return Count of mfi devices.
 */
M_API size_t M_io_mfi_enum_count(const M_io_mfi_enum_t *mfienum);


/*! Name of mfi device as reported by the device.
 *
 * \param[in] mfienum mfi enumeration object.
 * \param[in] idx    Index in mfi enumeration.
 *
 * \return String.
 */
M_API const char *M_io_mfi_enum_name(const M_io_mfi_enum_t *mfienum, size_t idx);


/*! Protocol exposed by of mfi device.
 *
 * \param[in] mfienum mfi enumeration object.
 * \param[in] idx    Index in mfi enumeration.
 *
 * \return String.
 */
M_API const char *M_io_mfi_enum_protocol(const M_io_mfi_enum_t *mfienum, size_t idx);


/*! Serial number.
 *
 * \param[in]  mfienum mfi enumeration object.
 * \param[in]  idx    Index in mfi enumeration.
 *
 * \return String
 */
M_API const char *M_io_mfi_enum_serialnum(const M_io_mfi_enum_t *mfienum, size_t idx, size_t sidx, const char **name, const char **uuid);


/*! Create a mfi connection.
 *
 * \param[out] io_out    io object for communication.
 * \param[in]  protocol  Protocol to use. Required. 
 * \param[in]  serialnum Serial number of device to use.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_mfi_create(M_io_t **io_out, const char *protocol, const char *serialnum);

/*! @} */

__END_DECLS

#endif /* __M_IO_BLUETOOTH_H__ */
