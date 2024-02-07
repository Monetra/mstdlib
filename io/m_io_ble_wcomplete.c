/* The MIT License (MIT)
 *
 * Copyright (c) 2022 Monetra Technologies, LLC.
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

void M_io_ble_wcomplete_destroy(M_io_ble_wcomplete_t *wc)
{
    if (wc == NULL)
        return;
    M_free(wc);
}

void *M_io_ble_wcomplete_duplicate(M_io_ble_wcomplete_t *wc)
{
    M_io_ble_wcomplete_t *wcn;

    wcn = M_malloc_zero(sizeof(*wcn));
    M_str_cpy(wcn->service_uuid, sizeof(wcn->service_uuid), wc->service_uuid);
    M_str_cpy(wcn->characteristic_uuid, sizeof(wcn->characteristic_uuid), wc->characteristic_uuid);

    return wcn;
}

int M_io_ble_wcomplete_compar(const void *arg1, const void *arg2, void *thunk)
{
    const M_io_ble_wcomplete_t *wc1;
    const M_io_ble_wcomplete_t *wc2;
    int   c;

    (void)thunk;

    if (arg1 == arg2)
        return 0;
    if (arg1 == NULL)
        return -1;
    if (arg2 == NULL)
        return 1;

    wc1 = *(void * const *)arg1;
    wc2 = *(void * const *)arg2;

    if (M_str_caseeq(wc1->service_uuid, wc2->service_uuid) && M_str_caseeq(wc1->characteristic_uuid, wc2->characteristic_uuid))
        return 0;

    c = M_str_casecmpsort(wc1->service_uuid, wc2->service_uuid);
    if (c != 0)
        return c;

    c = M_str_casecmpsort(wc1->characteristic_uuid, wc2->characteristic_uuid);
    return c;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_io_ble_wcomplete_queue_push(M_list_t *wcomplete_queue, const char *service_uuid, const char *characteristic_uuid)
{
    M_io_ble_wcomplete_t wc;

    if (wcomplete_queue == NULL || M_str_isempty(service_uuid) || M_str_isempty(characteristic_uuid))
        return M_FALSE;

    M_str_cpy(wc.service_uuid, sizeof(wc.service_uuid), service_uuid);
    M_str_cpy(wc.characteristic_uuid, sizeof(wc.characteristic_uuid), characteristic_uuid);

    M_list_insert(wcomplete_queue, &wc);
    return M_TRUE;
}

M_bool M_io_ble_wcomplete_queue_pop(M_list_t *wcomplete_queue, char **service_uuid, char **characteristic_uuid)
{
    M_io_ble_wcomplete_t *wc;

    if (wcomplete_queue == NULL || service_uuid == NULL || characteristic_uuid == NULL)
        return M_FALSE;

    wc = M_list_take_last(wcomplete_queue);
    if (wc == NULL)
        return M_FALSE;

    *service_uuid        = M_strdup(wc->service_uuid);
    *characteristic_uuid = M_strdup(wc->characteristic_uuid);

    M_io_ble_wcomplete_destroy(wc);
    return M_TRUE;
}
