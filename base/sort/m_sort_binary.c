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

static M_bool M_sort_binary_idx(const void *base, size_t nmemb, size_t esize, const void *key, M_sort_compar_t compar, void *thunk, M_bool match, M_bool stable, size_t *idx)
{
    /* ssize_t since mid - 1 could go negative */
    ssize_t  left;
    ssize_t  right;
    size_t   mid = 0;
    int      eq  = 1;

    *idx = 0;
    if (base == NULL || key == NULL || compar == NULL) {
        return M_FALSE;
    }

    /* Array is empty */
    if (nmemb == 0) {
        /* If we expect a match, no way that could happen */
        if (match)
            return M_FALSE;

        /* insertion index is 0 (or nmemb == 0, so whatever) */
        *idx = nmemb;
        return M_TRUE;
    }

    left  = 0;
    right = (ssize_t)nmemb-1;

    /* Shortcut if not needing to be stable to prepend ot head or append to tail */
    if (!stable) {
        /* Check the last value first, if we're inserting (!match), then it is probably
         * more efficient to append */
        eq = compar(key, (const unsigned char *)base+((size_t)right*esize), thunk);
        if (eq == 0) {
            /* We have a match so this is the element idx */
            *idx = (size_t)right;

            /* But for insertion (!match), it is more efficient to _append_, the
             * values, afterall, are equal */
            if (!match) {
                *idx = nmemb;
            }

            return M_TRUE;
        } else if (eq > 0) {
            if (match) {
                /* Didn't match and nothing after so not found. */
                return M_FALSE;
            }
            /* Insert at end since > last element */
            *idx = nmemb;
            return M_TRUE;
        }


        /* Check the first value. */
        eq = compar(key, (const unsigned char *)base+((size_t)left*esize), thunk);
        if (eq <= 0) {
            if (eq != 0 && match) {
                /* No match and there isn't anything before so not found. */
                return M_FALSE;
            }

            /* Either first element is the same, or we we are doing insertion (!match)
             * and the value is less than the current first element.  Either way, make
             * this the first element. */
            *idx = 0;
            return M_TRUE;
        }
    }

    /* Try to find the value by halving (binary search). */
    while (left <= right) {
        mid = ((size_t)(left + right)) >> 1;
        eq  = compar(key, (const unsigned char *)base+(mid*esize), thunk);
        if (eq == 0) {
            break;
        } else if (eq < 0) {
            right = ((ssize_t)mid) - 1;
        } else if (eq > 0) {
            left = ((ssize_t)mid) + 1;
        }
    }

    if (!stable) {
        /* If we expected a match but didn't get one, fail */
        if (match && eq != 0) {
            return M_FALSE;
        }

        /* Not matching so use the last mid value. */
        *idx = mid;

        /* current index is less than the key being checked, need to insert after */
        if (eq > 0)
            (*idx)++;

        return M_TRUE;
    }

    /* Stable matching now, extra work to do */

    if (match) {
        /* We require a match according to our parameters, but we didn't get
         * one, bail */
        if (eq != 0) {
            return M_FALSE;
        }

        /* Scan backwards to find the *first* match, this is what makes it stable */
        for ( ; mid > 0 && mid >= (size_t)left ; mid--) {
            eq = compar(key, (const unsigned char *)base+((mid-1)*esize), thunk);
            if (eq != 0)
                break;
        }
        *idx = mid;
    } else {
        /* Insert after the index. */
        if (eq > 0)
            mid++;

        /* Since we're not finding a match, that means we're probably inserting,
         * so we want to insert to the *end* of the identical matches */
        while (mid < (size_t)right && eq == 0) {
            mid++;
            eq = compar(key, (const unsigned char *)base+(mid*esize), thunk);
        }
        *idx = mid;
    }

    return M_TRUE;
}


size_t M_sort_binary_insert_idx(const void *base, size_t nmemb, size_t esize, const void *key, M_bool stable, M_sort_compar_t compar, void *thunk)
{
    M_bool ret;
    size_t idx;

    ret = M_sort_binary_idx(base, nmemb, esize, key, compar, thunk, M_FALSE, stable, &idx);
    if (ret) {
        return idx;
    }
    return nmemb;
}

M_bool M_sort_binary_search(const void *base, size_t nmemb, size_t esize, const void *key, M_bool stable, M_sort_compar_t compar, void *thunk, size_t *idx)
{
    size_t myidx;

    if (idx == NULL)
        idx = &myidx;

    return M_sort_binary_idx(base, nmemb, esize, key, compar, thunk, M_TRUE, stable, idx);
}
