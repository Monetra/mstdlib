/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"
#include "ini/m_ini_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Wrapper to conform to the prototype for the list value free callback. */
static void M_ini_elements_destroy_element(void *elem)
{
    M_ini_element_destroy((M_ini_element_t *)elem);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_elements_t *M_ini_elements_create(void)
{
    struct M_list_callbacks callbacks = {
        NULL,
        NULL,
        NULL,
        M_ini_elements_destroy_element
    };

    return (M_ini_elements_t *)M_list_create(&callbacks, M_LIST_NONE);
}

void M_ini_elements_destroy(M_ini_elements_t *d)
{
    M_list_destroy((M_list_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_ini_elements_insert(M_ini_elements_t *d, M_ini_element_t *val)
{
    return M_list_insert((M_list_t *)d, val);
}

M_bool M_ini_elements_insert_at(M_ini_elements_t *d, M_ini_element_t *val, size_t idx)
{
    return M_list_insert_at((M_list_t *)d, val, idx);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_ini_elements_len(const M_ini_elements_t *d)
{
    return M_list_len((const M_list_t *)d);
}

M_ini_element_t *M_ini_elements_at(const M_ini_elements_t *d, size_t idx)
{
    const void *tmp_val;
    tmp_val = M_list_at((const M_list_t *)d, idx);
    return M_CAST_OFF_CONST(M_ini_element_t *, tmp_val);
}

M_ini_element_t *M_ini_elements_take_at(M_ini_elements_t *d, size_t idx)
{
    return M_list_take_at((M_list_t *)d, idx);
}

M_bool M_ini_elements_remove_at(M_ini_elements_t *d, size_t idx)
{
    return M_list_remove_at((M_list_t *)d, idx);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_elements_t *M_ini_elements_duplicate(const M_ini_elements_t *d)
{
    M_ini_elements_t *dup_elems;
    size_t            len;
    size_t            i;

    if (d == NULL)
        return NULL;

    /* We don't use the duplicate from the underlying M_list_t because that requires the
     * duplicate function to be set. We want to use pass through pointers for storing values
     * but setting a duplicate function will cause anything put into the list to be duplicated.
     * The duplicate function (which we cannot set) is requried for the M_list_t duplicate to
     * be used. */
    dup_elems = M_ini_elements_create();
    len       = M_ini_elements_len(d);
    for (i=0; i<len; i++) {
        M_ini_elements_insert(dup_elems, M_ini_element_duplicate(M_ini_elements_at(d, i)));
    }

    return dup_elems;
}
