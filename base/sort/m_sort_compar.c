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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_sort_compar_str(const void *arg1, const void *arg2, void *thunk)
{
    (void)thunk;
    return M_str_cmpsort(*(char * const *)arg1, *(char * const *)arg2);
}

int M_sort_compar_str_desc(const void *arg1, const void *arg2, void *thunk)
{
    return M_sort_compar_str(arg2, arg1, thunk);
}

int M_sort_compar_str_casecmp(const void *arg1, const void *arg2, void *thunk)
{
    (void)thunk;
    return M_str_casecmpsort(*(char * const *)arg1, *(char * const *)arg2);
}

int M_sort_compar_str_casecmp_desc(const void *arg1, const void *arg2, void *thunk)
{
    return M_sort_compar_str_casecmp(arg2, arg1, thunk);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_sort_compar_u64(const void *arg1, const void *arg2, void *thunk)
{
    M_uint64 i1 = 0;
    M_uint64 i2 = 0;

    (void)thunk;

    if (arg1 != NULL)
        i1 = *(*((M_uint64 * const *)arg1));
    if (arg2 != NULL)
        i2 = *(*((M_uint64 * const *)arg2));

    if (i1 == i2)
        return 0;
    else if (i1 < i2)
        return -1;
    return 1;
}

int M_sort_compar_u64_desc(const void *arg1, const void *arg2, void *thunk)
{
    return M_sort_compar_u64(arg2, arg1, thunk);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_sort_compar_binwraped(const void *arg1, const void *arg2, void *thunk)
{
    M_uint8 *val1;
    M_uint8 *val2;
    size_t   len1;
    size_t   len2;

    (void)thunk;

    if (arg1 == arg2)
        return 0;
    if (arg1 == NULL)
        return -1;
    if (arg2 == NULL)
        return 1;

    val1 = *(M_uint8 * const *)arg1;
    val2 = *(M_uint8 * const *)arg2;
    M_mem_copy(&len1, val1, sizeof(len1));
    M_mem_copy(&len2, val2, sizeof(len1));

    return M_mem_cmpsort(val1, len1+sizeof(len1), val2, len2+sizeof(len2));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_sort_compar_vp(const void *arg1, const void *arg2, void *thunk)
{
    const void *vp1;
    const void *vp2;

    (void)thunk;

    if (arg1 == arg2)
        return 0;
    if (arg1 == NULL)
        return -1;
    if (arg2 == NULL)
        return 1;

    vp1 = *(void * const *)arg1;
    vp2 = *(void * const *)arg2;

    if (vp1 < vp2)
        return -1;
    if (vp1 > vp2)
        return 1;
    return 0;
}

int M_sort_compar_vp_desc(const void *arg1, const void *arg2, void *thunk)
{
    return M_sort_compar_vp(arg2, arg1, thunk);
}
