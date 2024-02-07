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

#include "textcodec/m_textcodec_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


M_textcodec_error_t M_textcodec_utf8_to_utf8(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_bool is_encode)
{
    const char          *endptr;
    M_textcodec_error_t  res = M_TEXTCODEC_ERROR_SUCCESS;

    /* If we're ignoring there is no need to validate anything. */
    if (ehandler == M_TEXTCODEC_EHANDLER_IGNORE) {
        M_textcodec_buffer_add_str(buf, in);
        if (!M_utf8_is_valid(in, NULL)) {
            return M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
        }
        return M_TEXTCODEC_ERROR_SUCCESS;
    }

    while (*in != '\0') {
        if (M_utf8_is_valid(in, &endptr)) {
            M_textcodec_buffer_add_bytes(buf, (const unsigned char *)in, (size_t)(endptr-in));
        } else {
            if (ehandler == M_TEXTCODEC_EHANDLER_FAIL) {
                return M_TEXTCODEC_ERROR_FAIL;
            }

            /* Add the valid bytes. */
            M_textcodec_buffer_add_bytes(buf, (const unsigned char *)in, (size_t)(endptr-in));

            /* Error handler can only be replace at this point. */
            res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;

            /* Add replace character. */
            if (is_encode) {
                M_textcodec_buffer_add_byte(buf, M_CP_REPLACE);
            } else {
                M_textcodec_buffer_add_str(buf, M_UTF8_REPLACE);
            }

            /* Find the next valid character because we only want to add one replace. */
            endptr = M_utf8_next_chr(endptr);
        }

        in = endptr;
    }

    return res;
}
