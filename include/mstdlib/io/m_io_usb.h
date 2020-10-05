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

typedef enum {
    M_IO_USB_SPEED_UNKNOWN = 0, /*!< Speed not known. */
    M_IO_USB_SPEED_LOW,         /*!< USB 1.0, 1.5 Megabits per second (Mbps). */
    M_IO_USB_SPEED_FULL,        /*!< USB 1.1, 12 Megabits per second (Mbps). */
    M_IO_USB_SPEED_HIGH,        /*!< USB 2.0, 480 Megabits per second (Mbps). */
    M_IO_USB_SPEED_SUPER,       /*!< USB 3.0 (Aka 3.1 Gen 1), 5 Gigabits per second (Gbps). */
    M_IO_USB_SPEED_SUPERPLUS,   /*!< USB 3.1 (Aka 3.1 Gen 2), 10 Gigabits per second (Gbps). */
    M_IO_USB_SPEED_SUPERPLUSX2, /*!< USB 3.2 (Aka 32. Gen 2x2), 20 Gigabits per second (Gbps). */
/*    M_IO_USB_SPEED_V4 */          /*!< USB 4, 40 Gigabits per second (Gbps). */
} M_io_usb_speed_t;

typedef enum {
	M_IO_USB_EP_TYPE_UNKNOWN = 0,
	M_IO_USB_EP_TYPE_CONTROL,
	M_IO_USB_EP_TYPE_ISOC,
	M_IO_USB_EP_TYPE_BULK,
	M_IO_USB_EP_TYPE_INTERRUPT
} M_io_usb_ep_type_t;

typedef enum {
	M_IO_USB_EP_DIRECTION_UNKNOWN = 0,
	M_IO_USB_EP_DIRECTION_IN      = 1 << 0,
	M_IO_USB_EP_DIRECTION_OUT     = 1 << 1
} M_io_usb_ep_direction_t;

struct M_io_usb_enum;
typedef struct M_io_usb_enum M_io_usb_enum_t;


/* Enumeration */
M_API M_io_usb_enum_t *M_io_usb_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial);
M_API void M_io_usb_enum_destroy(M_io_usb_enum_t *usbenum);
M_API size_t M_io_usb_enum_count(const M_io_usb_enum_t *usbenum);

M_API const char *M_io_usb_enum_path(const M_io_usb_enum_t *usbenum, size_t idx);

M_API M_uint16 M_io_usb_enum_vendorid(const M_io_usb_enum_t *usbenum, size_t idx);
M_API M_uint16 M_io_usb_enum_productid(const M_io_usb_enum_t *usbenum, size_t idx);

M_API const char *M_io_usb_enum_manufacturer(const M_io_usb_enum_t *usbenum, size_t idx);
M_API const char *M_io_usb_enum_product(const M_io_usb_enum_t *usbenum, size_t idx);
M_API const char *M_io_usb_enum_serial(const M_io_usb_enum_t *usbenum, size_t idx);

M_API M_io_usb_speed_t M_io_usb_enum_speed(const M_io_usb_enum_t *usbenum, size_t idx);
M_API size_t M_io_usb_enum_current_configuration(const M_io_usb_enum_t *usbenum, size_t idx);
/* XXX: info about configurations. */


/* Device */
M_API M_io_error_t M_io_usb_create(M_io_t **io_out, M_uint16 vendorid, M_uint16 productid, const char *serial);
M_API M_io_error_t M_io_usb_create_one(M_io_t **io_out, M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial);

/* Will open the interface if not already open.
 * Must be connected before this can be called. */
M_API M_bool M_io_usb_attach_interface_endpoint(M_io_t *io, size_t iface_num, size_t ep_num);

/* Device metadata */
M_API M_uint16 M_io_usb_get_vendorid(M_io_t *io);
M_API M_uint16 M_io_usb_get_productid(M_io_t *io);

M_API char *M_io_usb_get_manufacturer(M_io_t *io);
M_API char *M_io_usb_get_product(M_io_t *io);
M_API char *M_io_usb_get_serial(M_io_t *io);

/* XXX: Need speed, get, set and info about configurations. */

M_API M_io_usb_speed_t M_io_usb_enum_speed(const M_io_usb_enum_t *usbenum, size_t idx);
M_API size_t M_io_usb_enum_current_configuration(const M_io_usb_enum_t *usbenum, size_t idx);

M_API size_t M_io_usb_num_interface(M_io_t *io);
M_API size_t M_io_usb_interface_num_endpoint(M_io_t *io, size_t iface_num);

M_API M_io_usb_ep_type_t M_io_usb_endpoint_type(M_io_t *io, size_t iface_num, size_t ep_num);
M_API M_io_usb_ep_direction_t M_io_usb_endpoint_direction(M_io_t *io, size_t iface_num, size_t ep_num);
M_API size_t M_io_usb_endpoint_max_packet_size(M_io_t *io, size_t iface_num, size_t ep_num);

/* Meta */
M_API size_t M_io_usb_meta_get_interface(M_io_t *io, M_io_meta_t *meta);
M_API size_t M_io_usb_meta_get_endpoint(M_io_t *io, M_io_meta_t *meta);
M_API M_io_usb_ep_type_t M_io_usb_meta_get_endpoint_type(M_io_t *io, M_io_meta_t *meta);

M_API void M_io_usb_meta_set_interface(M_io_t *io, M_io_meta_t *meta, size_t iface_num);
M_API void M_io_usb_meta_set_endpoint(M_io_t *io, M_io_meta_t *meta, size_t ep_num);

/*! @} */

__END_DECLS

#endif /* __M_IO_USB_H__ */
