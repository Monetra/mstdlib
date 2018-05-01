/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

#ifndef __M_UTF8_H__
#define __M_UTF8_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_utf8 utf-8 Handling
 *  \ingroup mstdlib_base
 *
 * @{
 */ 

typedef enum {
	M_UTF8_ERROR_SUCESS,
	M_UTF8_ERROR_BAD_START,
	M_UTF8_ERROR_TRUNCATED,
	M_UTF8_ERROR_EXPECT_CONTINUE,
	M_UTF8_ERROR_BAD_CODE_POINT,
	M_UTF8_ERROR_OVERLONG,
	M_UTF8_ERROR_INVALID_PARAM
} M_utf8_error_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API M_bool M_utf8_is_valid(const char *str, const char **endptr);
M_API M_bool M_utf8_is_valid_cp(M_uint32 cp);
M_API size_t M_utf8_len(const char *str);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API M_utf8_error_t M_utf8_to_cp(const char *str, M_uint32 *cp, const char **next);
M_API M_utf8_error_t M_utf8_from_cp(char *buf, size_t buf_size, size_t *len, M_uint32 cp);
M_API M_utf8_error_t M_utf8_from_cp_buf(M_buf_t *buf, M_uint32 cp);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API M_utf8_error_t M_utf8_cp_at(const char *str, size_t idx, M_uint32 *cp);
M_API M_utf8_error_t M_utf8_chr_at(char *buf, size_t buf_size, size_t *len, const char *str, size_t idx);

/*! @} */

__END_DECLS

#endif /* __M_UTF8_H__ */
