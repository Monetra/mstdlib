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

#ifndef __M_IO_BLUETOOTH_MAC_H__
#define __M_IO_BLUETOOTH_MAC_H__

#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>

#include <IOBluetooth/IOBluetooth.h>

struct M_io_handle {
	M_io_state_t     state;      /*!< Current state of connection */

	CFTypeRef        conn;       /*!< Rfcomm interface (__bridge_retained) */
	M_buf_t         *readbuf;    /*!< Reads are transferred via a buffer */
	M_buf_t         *writebuf;   /*!< Write data is buffered because only uint16 max bytes can be sent at a time. */
	size_t           wrote_len;  /*!< Amount of data buffered for writing. */
	M_io_t          *io;         /*!< Pointer to IO object */
	M_event_timer_t *timer;      /*!< Timer to handle connection timeouts */
	char             error[256]; /*!< Error string */
	M_bool           can_write;  /*!< Wether data can be written. Will be false if a write operation is processing. */
};

M_bool M_io_bluetooth_mac_uuid_to_str(IOBluetoothSDPUUID *u, char *uuid, size_t uuid_len);

#endif
