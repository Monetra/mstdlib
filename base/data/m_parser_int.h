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

#ifndef __M_PARSER_INT_H__
#define __M_PARSER_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

struct M_parser {
    const unsigned char *data_const;    /*!< If constant, base data pointer here */
    unsigned char       *data_dyn;      /*!< If dynamic (appendable), base data pointer here */

    const unsigned char *data;          /*!< Pointer containing current offset of either data_const
                                             or data_dyn */
    enum M_PARSER_FLAGS  flags;         /*!< Flags controlling behavior */
    size_t               data_dyn_size; /*!< For dynamic buffers, this is the current allocated
                                             size */
    size_t               data_len;      /*!< Length of data remaining in buffer */
    size_t               consumed;      /*!< Number of bytes consumed */
    ssize_t              mark_user;     /*!< Position marked by user for future reference */
    ssize_t              mark_int;      /*!< Internal marked position by parser functions (so it doesn't clobber user marks) */

    size_t               curr_col;      /*!< Current column number */
    size_t               curr_line;     /*!< Current line number */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_parser_init(M_parser_t *parser, const unsigned char *buf, size_t len, M_uint32 flags);

__END_DECLS

#endif /* __M_PARSER_INT_H__ */
