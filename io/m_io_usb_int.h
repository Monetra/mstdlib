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

#ifndef __M_IO_USB_USB_INT_H__
#define __M_IO_USB_USB_INT_H__

#include <mstdlib/io/m_io_layer.h>

#define M_IO_USB_USB_NAME "USB"

struct M_io_usb_enum_device {
	M_uint16          vendor_id;
	M_uint16          product_id;
	char             *path;
	char             *manufacturer;
	char             *product;
	char             *serial;
	M_io_usb_speed_t  speed;
};

typedef struct M_io_usb_enum_device M_io_usb_enum_device_t;

struct M_io_usb_enum {
	M_list_t *devices;
};

M_io_usb_enum_t *M_io_usb_enum_init(void);

void M_io_usb_enum_add(M_io_usb_enum_t *usbenum,
                       /* Info about this enumerated device */
                       const char *path, M_io_usb_speed_t speed,
                       M_uint16 d_vendor_id, M_uint16 d_product_id, const char *d_serial,
					   const char *d_manufacturer, const char *d_product,
                       /* Search/Match criteria */
                       M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids, const char *s_serial);

M_io_handle_t *M_io_usb_open(const char *devpath, M_io_error_t *ioerr);
M_bool M_io_usb_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len);
M_io_state_t M_io_usb_state_cb(M_io_layer_t *layer);
void M_io_usb_destroy_cb(M_io_layer_t *layer);
M_bool M_io_usb_process_cb(M_io_layer_t *layer, M_event_type_t *type);
M_io_error_t M_io_usb_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);
M_io_error_t M_io_usb_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);
void M_io_usb_unregister_cb(M_io_layer_t *layer);
M_bool M_io_usb_disconnect_cb(M_io_layer_t *layer);
M_bool M_io_usb_init_cb(M_io_layer_t *layer);

M_io_layer_t *M_io_usb_get_top_usb_layer(M_io_t *io);

#endif


