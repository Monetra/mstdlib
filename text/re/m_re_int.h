/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

#ifndef __M_RE_INT_H__
#define __M_RE_INT_H__

#include "m_config.h"

#include <mstdlib/mstdlib_text.h>
#include "mtre/mregex.h"

/*! Create a match object
 *
 * \return Match object.
 */
M_re_match_t *M_re_match_create(void);


/*! Insert a capture.
 *
 * \param[in] match Match object.
 * \param[in] idx   Index of the capture.
 * \param[in] start Start of the match.
 * \param[in] len   Length of the match.
 */
void M_re_match_insert(M_re_match_t *match, size_t idx, size_t start, size_t len);


/*! Adjust an offset for a match object.
 *
 * Used for chaining match objects together so they start indexing
 * from the beginning of a string instead of from the search start
 * point.
 *
 * \param[in] match  Match object.
 * \param[in] adjust Adjustment offset from the start of the evaluated
 *                   string.
 */
void M_re_match_adjust_offset(M_re_match_t *match, size_t adjust);

#endif /* __M_RE_INT_H__ */
