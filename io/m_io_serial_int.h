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

#ifndef __M_IO_SERIAL_INT_H__
#define __M_IO_SERIAL_INT_H__

#define M_IO_SERIAL_NAME "SERIAL"
M_io_error_t M_io_serial_handle_set_mode(M_io_handle_t *handle, M_io_serial_mode_t mode);
M_io_error_t M_io_serial_handle_set_flowcontrol(M_io_handle_t *handle, M_io_serial_flowcontrol_t flowcontrol);
M_io_error_t M_io_serial_handle_set_baud(M_io_handle_t *handle, M_io_serial_baud_t baud);
M_io_serial_flowcontrol_t M_io_serial_handle_get_flowcontrol(M_io_handle_t *handle);
M_io_serial_mode_t M_io_serial_handle_get_mode(M_io_handle_t *handle);
M_io_serial_baud_t M_io_serial_handle_get_baud(M_io_handle_t *handle);


struct M_io_serial_enum_port {
    char *path;
    char *name;
};
typedef struct M_io_serial_enum_port M_io_serial_enum_port_t;

struct M_io_serial_enum {
    M_list_t *ports;
};

M_io_serial_enum_t *M_io_serial_enum_init(void);
void M_io_serial_enum_add(M_io_serial_enum_t *serenum, const char *path, const char *name);

#endif
