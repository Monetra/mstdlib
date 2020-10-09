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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_usb.h>
#include "m_io_usb_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_usb_rdata_t *M_io_usb_rdata_create(M_io_usb_ep_type_t type)
{
	M_io_usb_rdata_t *rdata;

	rdata          = M_malloc_zero(sizeof(*rdata));
	rdata->ep_type = type;
	rdata->data    = M_buf_create();

	return rdata;
}

void M_io_usb_rdata_destroy(M_io_usb_rdata_t *rdata)
{
	if (rdata == NULL)
		return;

	M_buf_cancel(rdata->data);
	M_free(rdata);
}

M_bool M_io_usb_rdata_queue_add_read_bulkirpt(M_llist_t *queue, M_io_usb_ep_type_t type, size_t iface_num, size_t ep_num, const unsigned char *data, size_t data_len)
{
	M_io_usb_rdata_t *rdata;

	if (queue == NULL || data == NULL || data_len == 0)
		return M_FALSE;

	rdata = M_io_usb_rdata_create(type);
	M_llist_insert(queue, rdata);

	rdata->iface_num = iface_num;
	rdata->ep_num    = ep_num;
	M_buf_add_bytes(rdata->data, data, data_len);

	return M_TRUE;
}

M_bool M_io_usb_rdata_queue_add_read_control(M_llist_t *queue, M_io_usb_ep_type_t type, size_t ctrl_type, size_t ctrl_value, size_t ctrl_index, const unsigned char *data, size_t data_len)
{
	M_io_usb_rdata_t *rdata;

	if (queue == NULL || data == NULL || data_len == 0)
		return M_FALSE;

	rdata = M_io_usb_rdata_create(type);
	M_llist_insert(queue, rdata);

	rdata->ctrl_type  = ctrl_type;
	rdata->ctrl_value = ctrl_value;
	rdata->ctrl_index = ctrl_index;
	M_buf_add_bytes(rdata->data, data, data_len);

	return M_TRUE;
}
