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
 * ## HID (Human Interface Device) IO functions.
 *
 * Typically used with USB devices.
 *
 * Report IDs need to be the first byte of any data sent to a device and
 * will be the first byte of any data received from a device. All buffer
 * sizes report will include the extra byte for the report ID.
 *
 * If a device does not use report IDs 0 should be sent as the first byte
 * of any data and will be the first byte of any read data.
 *
 * ## Supported OS
 *
 * - Windows
 * - Linux
 * - macOS
 * - Android
 *
 * ## Android Requirements
 *
 * Android does not have blanket USB permissions. Access needs to be granted
 * by the user on a per device basis. Permission granting is _not_ supported
 * by mstdlib and must be handled by the app itself. Once permissions are
 * granted mstdlib can access the device. Device enumeration does not need
 * permission, it is only required to open a device.
 *
 * The manifest must include the uses-feature for USB host.
 *
 *     <uses-feature android:name="android.hardware.usb.host" />
 *
*
 * There are two methods for obtaining permissions.
 * The Android [USB Host documentation](https://developer.android.com/guide/topics/connectivity/usb/host)
 * provides a detailed overview of the permission process.
 *
 * UsbManager.hasPermission() should be used to determine if the app can
 * access the device or if it needs permission from the user to do so.
 * If the manifest method is used the request method may still be necessary
 * to implement. However, the manifest method allows the user to associate
 * the device with the app so permission only needs to be granted once.
 *
 * ### Manifest
 *
 * The device vendor and product ids can be registered by the App though the
 * manifest file. When the device is connected the user will be prompted if
 * they want to open the device with the application. There is an option to
 * always open with the given application the user can select. If they do not
 * select a default application they will be prompted every time the device
 * is connected. Once allowed the application can use the device.
 *
 * The manifest will specify an intent filter for a given activity for
 * USB device attached. A meta-data specifying supported devices is associated
 * with the intent which Android uses to determine if the application supports
 * the given device.
 *
 *     <activity ...>
 *       <intent-filter>
 *         <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
 *       </intent-filter>
 *       <meta-data android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
 *         android:resource="@xml/device_filter" />
 *     </activity>
 *
 * The device_filter xml file specifying one or more devices using vendor an product
 * ids. The ids must be a decimal number and cannot be hex.
 *
 *     <?xml version="1.0" encoding="utf-8"?>
 *     <resources>
 *       <usb-device vendor-id="1234" product-id="5678" />
 *     </resources>
 *
 * The disadvantage of this method is it work off of the device being attached.
 * If the application is running and the device is already connected the user
 * will not be prompted.
 *
 * ### Request Dialog
 *
 * This method uses the UsbManager.requestPermission() function to display a permission
 * request to the user to allow USB access. The application will use an intent to
 * make the request. A broadcast receiver will need to be registered with the intent
 * in order for the app to receive the users response to the query. If approved by
 * the user the application can use the device.
 *
 * This does not grant access to USB in general. The device in question is part of the
 * permission request. The user is only given permission for that specific device.
 *
 * For this method the app should use mstdlib (or direct API calls) to enumerate the
 * currently connected devices. Then use the path (mstdlib) to pull the device out
 * of UsbManager.getDeviceList(). This device can then be used for the permission request.
 *
 * This method works if the device is already attached.
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
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return new string containing manufacturer, or NULL if no HID layer was present/acquirable
 */
M_API char *M_io_hid_get_manufacturer(M_io_t *io);


/*! Get the HID path from an io object.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return new string containing path, or NULL if no HID layer was present/acquirable
 */
M_API char *M_io_hid_get_path(M_io_t *io);


/*! Get the HID product from an io object.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return new string containing product, or NULL if no HID layer was present/acquirable
 */
M_API char *M_io_hid_get_product(M_io_t *io);


/*! Get the HID product ID from an io object.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return product ID, or 0 if no HID layer was present/acquirable
 */
M_API M_uint16 M_io_hid_get_productid(M_io_t *io);


/*! Get the HID vendor ID from an io object.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return vendor ID, or 0 if no HID layer was present/acquirable
 */
M_API M_uint16 M_io_hid_get_vendorid(M_io_t *io);


/*! Get the HID serial number from an io object.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in] io io object.
 *
 * \return new string containing serial number, or NULL if no HID layer was present/acquirable
 */
M_API char *M_io_hid_get_serial(M_io_t *io);


/*! Get the HID maximum input and output report sizes from an io object.
 *
 * The report sizes returned may be 1 byte larger than the actual report size
 * to account for the report ID that is prepended to the data block.
 *
 * Queries the highest HID layer in the stack, if there are more than one.
 *
 * \param[in]  io              io object.
 * \param[out] max_input_size  Maximum input report size, or 0 if no HID layer was present/acquirable
 * \param[out] max_output_size Maximum output report size, or 0 if no HID layer was present/acquirable
 */
M_API void M_io_hid_get_max_report_sizes(M_io_t *io, size_t *max_input_size, size_t *max_output_size);

/*! @} */

__END_DECLS

#endif /* __M_IO_HID_H__ */
