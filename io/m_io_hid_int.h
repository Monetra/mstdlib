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

#ifndef __M_IO_USB_HID_INT_H__
#define __M_IO_USB_HID_INT_H__

#include <mstdlib/io/m_io_layer.h>

#define M_IO_USB_HID_NAME "HID"

struct M_io_hid_enum_device {
    char    *path;
    char    *manufacturer;
    char    *product;
    char    *serial;
    M_uint16 vendor_id;
    M_uint16 product_id;
};

typedef struct M_io_hid_enum_device M_io_hid_enum_device_t;

struct M_io_hid_enum {
    M_list_t *devices;
};

M_io_hid_enum_t *M_io_hid_enum_init(void);


void M_io_hid_enum_add(M_io_hid_enum_t *hidenum,
                       /* Info about this enumerated device */
                       const char *d_path, const char *d_manufacturer, const char *d_product,
                       const char *d_serialnum, M_uint16 d_vendor_id, M_uint16 d_product_id,
                       /* Search/Match criteria */
                       M_uint16 s_vendor_id, const M_uint16 *s_product_ids, size_t s_num_product_ids,
                       const char *s_serialnum);

M_io_handle_t *M_io_hid_open(const char *devpath, M_io_error_t *ioerr);
M_bool M_io_hid_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len);
M_io_state_t M_io_hid_state_cb(M_io_layer_t *layer);
void M_io_hid_destroy_cb(M_io_layer_t *layer);
M_bool M_io_hid_process_cb(M_io_layer_t *layer, M_event_type_t *type);
M_io_error_t M_io_hid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);
M_io_error_t M_io_hid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);
void M_io_hid_unregister_cb(M_io_layer_t *layer);
M_bool M_io_hid_disconnect_cb(M_io_layer_t *layer);
M_bool M_io_hid_init_cb(M_io_layer_t *layer);

M_io_layer_t *M_io_hid_get_top_hid_layer(M_io_t *io);
M_bool hid_uses_report_descriptors(const unsigned char *desc, size_t len);
M_bool hid_get_max_report_sizes(const M_uint8 *desc, size_t desc_len, size_t *max_input, size_t *max_output);

#endif

