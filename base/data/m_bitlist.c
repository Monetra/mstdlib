/* The MIT License (MIT)
 *
 * Copyright (c) 2021 Monetra Technologies, LLC.
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
#include "m_defs_int.h"

#include <mstdlib/mstdlib.h>


M_bool M_bitlist_list(char **out, M_bitlist_flags_t flags, const M_bitlist_t *list, M_uint64 bits, unsigned char delim, char *error, size_t error_len)
{
    M_bool   rv = M_FALSE;
    size_t   i;
    M_buf_t *buf;
    if (out == NULL || list == NULL || delim == 0) {
        M_snprintf(error, error_len, "invalid use");
        return M_FALSE;
    }

    buf = M_buf_create();

    for (i=0; list[i].name != NULL; i++) {
        if (!(flags & M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2) && list[i].id != 0 &&
            !M_uint64_is_power_of_two(list[i].id)) {
            M_snprintf(error, error_len, "'%s' is not a power of 2", list[i].name);
            goto done;
        }

        if ((bits & list[i].id) == list[i].id) {
            if (M_buf_len(buf))
                M_buf_add_byte(buf, delim);
            M_buf_add_str(buf, list[i].name);

            /* Remove consumed bits */
            bits &= ~list[i].id;
        }
    }

    if (!(flags & M_BITLIST_FLAG_IGNORE_UNKNOWN) && bits) {
        M_snprintf(error, error_len, "unknown remaining bits 0x%0llX", bits);
        goto done;
    }

    rv = M_TRUE;

done:
    if (rv) {
        *out = M_buf_finish_str(buf, NULL);
    } else {
        M_buf_cancel(buf);
    }

    return rv;
}

M_bool M_bitlist_parse(M_uint64 *out, M_bitlist_flags_t flags, const M_bitlist_t *list, const char *data, unsigned char delim, char *error, size_t error_len)
{
    char **elems     = NULL;
    size_t num_elems = 0;
    M_bool rv        = M_FALSE;
    size_t i;
    size_t j;

    if (out == NULL || list == NULL || delim == 0) {
        M_snprintf(error, error_len, "invalid use");
        return M_FALSE;
    }

    *out = 0;

    elems = M_str_explode_str(delim, data, &num_elems);
    for (i=0; i<num_elems; i++) {
        /* Trim whitespace */
        if (!(flags & M_BITLIST_FLAG_DONT_TRIM_WHITESPACE)) {
            M_str_trim(elems[i]);
        }

        /* Ignore empty strings */
        if (M_str_isempty(elems[i])) {
            continue;
        }

        for (j=0; list[j].name != NULL; j++) {
            M_bool match;

            if (flags & M_BITLIST_FLAG_CASE_SENSITIVE) {
                match = M_str_eq(list[j].name, elems[i]);
            } else {
                match = M_str_caseeq(list[j].name, elems[i]);
            }
            if (match) {
                if (list[j].id != 0 && !(flags & M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2) &&
                    !M_uint64_is_power_of_two(list[j].id)) {
                    M_snprintf(error, error_len, "'%s' is not a power of 2", list[j].name);
                    goto done;
                }
                (*out) |= list[j].id;
            }
        }
        if (list[j].name == NULL && !(flags & M_BITLIST_FLAG_IGNORE_UNKNOWN)) {
            M_snprintf(error, error_len, "unrecognized value '%s'", elems[i]);
            goto done;
        }
    }

    rv    = M_TRUE;
done:

    if (!rv)
        *out = 0;

    M_str_explode_free(elems, num_elems);
    return rv;
}

