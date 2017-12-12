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

#ifndef __M_IO_HID_H__
#define __M_IO_HID_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_hid HID (Human Interface Device) IO functions
 *  \ingroup m_eventio_base
 * 
 * HID (Human Interface Device) IO functions. Typically used with USB devices.
 *
 * Supported OS:
 * - Windows
 * - Linux
 *
 * @{
 */

struct M_io_hid_enum;
typedef struct M_io_hid_enum M_io_hid_enum_t;

/*! Create a HID enumeration object.
 *
 * \param[in] vendorid       Optional. Filter by vendor id. Set to 0 if no filter should be applied.
 * \param[in] productids     Optional. Filter by product ids. Set to NULL if no filter should be applied.
 * \param[in] num_productids Number of product ids in list of product ids. Should be 0 if productids is NULL.
 * \param[in] serial         Optional. Filter by serial number.
 *
 * \return HID enumeration object.
 */
M_API M_io_hid_enum_t *M_io_hid_enum(M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial);


/*! Destroy a HID enumeration object.
 *
 * \param[in] hidenum HID enumeration object.
 */
M_API void M_io_hid_enum_destroy(M_io_hid_enum_t *hidenum);


/*! Number of HID objects in the enumeration.
 * 
 * \param[in] hidenum HID enumeration object.
 *
 * \return Count of HID devices.
 */
M_API size_t M_io_hid_enum_count(const M_io_hid_enum_t *hidenum);


/* Path to HID device.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return String.
 */
M_API const char *M_io_hid_enum_path(const M_io_hid_enum_t *hidenum, size_t idx);


/* HID device manufacturer.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return String.
 */
M_API const char *M_io_hid_enum_manufacturer(const M_io_hid_enum_t *hidenum, size_t idx);


/* HID device product.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return String.
 */
M_API const char *M_io_hid_enum_product(const M_io_hid_enum_t *hidenum, size_t idx);


/* Hid device serial number.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return String.
 */
M_API const char *M_io_hid_enum_serial(const M_io_hid_enum_t *hidenum, size_t idx);


/* Hid device vendor id.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return Vendor id.
 */
M_API M_uint16 M_io_hid_enum_vendorid(const M_io_hid_enum_t *hidenum, size_t idx);


/* Hid device product id.
 *
 * \param[in] hidenum HID enumeration object.
 * \param[in] idx     Index in HID enumeration.
 *
 * \return Product id.
 */
M_API M_uint16 M_io_hid_enum_productid(const M_io_hid_enum_t *hidenum, size_t idx);


/*! Create a HID connection.
 *
 * \param[out] io_out    io object for communication.
 * \param[in]  vendorid  Vendor id.
 * \param[in]  productid Product id.
 * \param[in]  serial    Product serial number. Optional. If multiple devices with the same vendor an product id
 *                       it is undefined which will be chosen.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_hid_create(M_io_t **io_out, M_uint16 vendorid, M_uint16 productid, const char *serial /* May be NULL */);


/*! Create a HID device connection.
 *
 * Creates a connection to the first device from from a given list of ids.
 *
 * \param[out] io_out         io object for communication.
 * \param[in]  vendorid       Vendor id.
 * \param[in]  productids     A list of product ids to look for.
 * \param[in]  num_productids Number of product ids in the list of product ids. These should be in priority order.
 * \param[in]  serial         Product serial number. Optional. If multiple devices with the same vendor an product id
 *                            it is undefined which will be chosen.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_hid_create_one(M_io_t **io_out, M_uint16 vendorid, const M_uint16 *productids, size_t num_productids, const char *serial /* May be NULL */);


/*! Get the HID manufacturer from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_hid_get_manufacturer(M_io_t *io);


/*! Get the HID path from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_hid_get_path(M_io_t *io);


/*! Get the HID product from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_hid_get_product(M_io_t *io);


/*! Get the HID product ID from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API M_uint16 M_io_hid_get_productid(M_io_t *io);


/*! Get the HID vendor ID from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API M_uint16 M_io_hid_get_vendorid(M_io_t *io);


/*! Get the HID serial number from an io object.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_hid_get_serial(M_io_t *io);


/*! Get the HID maximum input and output report sizes from an io object.
 *
 * The report sizes returned may be 1 byte larger than the actual report size
 * to account for the report ID that is prepended to the data block.
 *
 * \param[in]  io              io object.
 * \param[out] max_input_size  Maximum input report size.
 * \param[out] max_output_size Maximum output report size.
 */
M_API void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size);

/*! @} */

__END_DECLS

#endif /* __M_IO_HID_H__ */
