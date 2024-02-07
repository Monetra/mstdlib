/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
#include <mstdlib/io/m_io_ble.h>
#include "m_io_ble_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


static M_io_ble_rdata_t *M_io_ble_rdata_create(M_io_ble_rtype_t prop)
{
    M_io_ble_rdata_t *rdata;

    rdata = M_malloc_zero(sizeof(*rdata));

    rdata->type = prop;
    switch (rdata->type) {
        case M_IO_BLE_RTYPE_READ:
            rdata->d.read.data = M_buf_create();
            break;
        case M_IO_BLE_RTYPE_RSSI:
        case M_IO_BLE_RTYPE_NOTIFY:
            break;
    }

    return rdata;
}

void M_io_ble_rdata_destroy(M_io_ble_rdata_t *rdata)
{
    if (rdata == NULL)
        return;

    switch (rdata->type) {
        case M_IO_BLE_RTYPE_READ:
            M_buf_cancel(rdata->d.read.data);
            break;
        case M_IO_BLE_RTYPE_RSSI:
        case M_IO_BLE_RTYPE_NOTIFY:
            break;
    }
    M_free(rdata);
}

M_bool M_io_ble_rdata_queue_add_read(M_llist_t *queue, const char *service_uuid, const char *characteristic_uuid, const unsigned char *data, size_t data_len)
{
    M_io_ble_rdata_t *rdata;
    M_llist_node_t   *n;

    if (queue == NULL || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid) || data == NULL || data_len == 0)
        return M_FALSE;

    /* Get the last node. If there are no nodes create new one. */
    n = M_llist_last(queue);
    if (n == NULL) {
        rdata = M_io_ble_rdata_create(M_IO_BLE_RTYPE_READ);
        M_str_cpy(rdata->d.read.service_uuid, sizeof(rdata->d.read.service_uuid), service_uuid);
        M_str_cpy(rdata->d.read.characteristic_uuid, sizeof(rdata->d.read.characteristic_uuid), characteristic_uuid);
        n     = M_llist_insert(queue, rdata);
    }

    /* If the last record in the queue is a read for the same service and characteristic we'll append
     * to that record's data buffer. Otherwise, we'll create a new record and insert it into the queue. */
    rdata = M_llist_node_val(n);
    if (rdata->type != M_IO_BLE_RTYPE_READ                          ||
            !M_str_caseeq(service_uuid, rdata->d.read.service_uuid) ||
            !M_str_caseeq(characteristic_uuid, rdata->d.read.characteristic_uuid))
    {
        rdata = M_io_ble_rdata_create(M_IO_BLE_RTYPE_READ);
        M_str_cpy(rdata->d.read.service_uuid, sizeof(rdata->d.read.service_uuid), service_uuid);
        M_str_cpy(rdata->d.read.characteristic_uuid, sizeof(rdata->d.read.characteristic_uuid), characteristic_uuid);
        if (M_llist_insert(queue, rdata) == NULL) {
            /* Shouldn't ever happen, but just in case: */
            M_io_ble_rdata_destroy(rdata);
            return M_FALSE;
        }
    }

    M_buf_add_bytes(rdata->d.read.data, data, data_len);
    return M_TRUE;
}

M_bool M_io_ble_rdata_queue_add_rssi(M_llist_t *queue, M_int64 rssi)
{
    M_io_ble_rdata_t *rdata;

    if (queue == NULL)
        return M_FALSE;

    rdata = M_io_ble_rdata_create(M_IO_BLE_RTYPE_RSSI);
    rdata->d.rssi.val = rssi;
    M_llist_insert(queue, rdata);

    return M_TRUE;
}

M_bool M_io_ble_rdata_queue_add_notify(M_llist_t *queue, const char *service_uuid, const char *characteristic_uuid)
{
    M_io_ble_rdata_t *rdata;

    if (queue == NULL)
        return M_FALSE;

    rdata = M_io_ble_rdata_create(M_IO_BLE_RTYPE_NOTIFY);
    M_str_cpy(rdata->d.notify.service_uuid, sizeof(rdata->d.notify.service_uuid), service_uuid);
    M_str_cpy(rdata->d.notify.characteristic_uuid, sizeof(rdata->d.notify.characteristic_uuid), characteristic_uuid);
    M_llist_insert(queue, rdata);

    return M_TRUE;
}
