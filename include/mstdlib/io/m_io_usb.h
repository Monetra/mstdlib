/* The MIT License (MIT)
 * 
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

#ifndef __M_IO_USB_H__
#define __M_IO_USB_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_usb USB (Raw)
 *  \ingroup m_eventio_base
 * 
 * ## USB (raw) IO functions.
 *
 * Typically used with USB devices that do not have a system driver interface.
 * Such as non-HID and non Serial Emulated devices.
 *
 * ## Supported OS
 *
 * XXX: List of proposed supported OS
 * - Windows
 * - Linux
 * - macOS
 * - Android
 *
 * @{
 */

struct M_io_usb_enum;
typedef struct M_io_usb_enum M_io_usb_enum_t;


M_API M_io_usb_enum_t *M_io_usb_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial);
M_API void M_io_usb_enum_destroy(M_io_usb_enum_t *usbenum);
M_API size_t M_io_usb_enum_count(const M_io_usb_enum_t *usbenum);
M_API M_uint16 M_io_usb_enum_vendorid(const M_io_usb_enum_t *usbenum, size_t idx);
M_API M_uint16 M_io_usb_enum_productid(const M_io_usb_enum_t *usbenum, size_t idx);

M_API size_t M_io_usb_enum_num_endpoints(const M_io_usb_enum_t *usbenum, size_t idx);
M_API const char *M_io_usb_enum_manufacturer(const M_io_usb_enum_t *usbenum, size_t idx);
M_API const char *M_io_usb_enum_product(const M_io_usb_enum_t *usbenum, size_t idx);
M_API const char *M_io_usb_enum_serial(const M_io_usb_enum_t *usbenum, size_t idx);


M_API M_io_error_t M_io_usb_create(M_io_t **io_out, M_uint16 vendorid, M_uint16 productid, const char *serial);
M_API M_io_error_t M_io_usb_create_one(M_io_t **io_out, M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial);

/* Device metadata */
M_API M_uint16 M_io_usb_get_vendorid(M_io_t *io);
M_API M_uint16 M_io_usb_get_productid(M_io_t *io);

/* XXX: Need to enumerate each interface offered by the device.
 * Need to report if it's control, bulk, interrupt and provide index.
 * Also direction if it's read or write. */


M_API M_io_error_t M_io_usb_create_control_interface(M_io_t **io_out, M_io_t *io_usb_device, M_uint16 index);
/* Use -1 if not read or write index is not wanted. */
M_API M_io_error_t M_io_usb_create_bulk_interface(M_io_t **io_out, M_io_t *io_usb_device, M_int32 index_read, M_int32 index_write);
/* Use -1 if not read or write index is not wanted. */
M_API M_io_error_t M_io_usb_create_interrupt_interface(M_io_t **io_out, M_io_t *io_usb_device, M_int32 index_read, M_int32 index_write);

/* XXX: Interface meta data accessors, such as type and indexes */

/*! @} */

__END_DECLS

#endif /* __M_IO_USB_H__ */
