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

#ifndef __M_BIN_H__
#define __M_BIN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_bin Binary data
 *  \ingroup mstdlib_base
 *
 * Allows for wrapping binary data into a data type that includes both length and data.
 * This is to allow binary data to be passed around as a single pointer instead of having
 * to manage the data separate from the length.
 *
 * Binary data of form data, len will be wrapped into len+data.
 * The length prefix is always fixed as 8. 8 was chosen instead of sizeof(size_t) because
 * 32bit Solaris Sparc sizeof(size_t) is 4 but alignment is 8. 8 is also sizeof(size_t) on
 * 64bit platforms.
 *
 * Example:
 *
 * \code{.c}
 *     M_uint8  data[3] = { 1, 2, 3 };
 *     M_uint8 *wd;
 *     M_uint8 *wd_dup;
 *     size_t   len;
 *
 *     wd     = M_bin_wrap(data, 3);
 *     wd_dup = M_bin_wrapeddup(wd);
 *
 *     M_free(wd);
 *     wd = M_bin_unwrapdup(wd_dup, &len);
 *
 *     M_free(wd);
 *     M_free(wd_dup);
 *
 *     M_printf(len='%zu'\n", len);
 * \endcode
 *
 * @{
 */


/*! Duplicate a binary value with the length prepended.
 *
 * \param[in] value The binary data.
 * \param[in] len   The length of value.
 *
 * \return Allocated memory of len+data of length len+sizeof(len). Where sizeof(size_t) is typically 8.
 */
M_API M_uint8 *M_bin_wrap(const M_uint8 *value, size_t len);


/*! Duplicates binary data that has the length prepended.
 *
 * The data would have to have been created using M_bin_wrap.
 * The prepended length is used to determine how much memory to duplicate.
 *
 * \param[in] value Wrapped binary data to duplicate.
 *
 * \return Copy of wrapped data (len+data).
 */
M_API M_uint8 *M_bin_wrapeddup(const M_uint8 *value);


/*! Duplicates data that has the length prepended.
 *
 * The data would have to have been created using M_bin_wrap.
 * The prepended length is used to determine how much memory to duplicate.
 * This function should be considered unsafe and is only provided
 * as a convience to avoid casting if the data is not an M_uint8.
 *
 * \param[in] value Wrapped binary data to duplicate.
 *
 * \return Copy of wrapped data (len+data).
 *
 * \see M_bin_wrapeddup
 */
M_API void *M_bin_wrapeddup_vp(const void *value);


/*! Unwrap wrapped binary data.
 *
 * \param[in]  value Wrapped binary data.
 * \param[out] len   The length of the binary data.
 *
 * \return Pointer to start of data within value.
 */
M_API const M_uint8 *M_bin_unwrap(const M_uint8 *value, size_t *len);


/*! Unwrap wrapped binary data and return a copy of the data.
 *
 * \param[in]  value Wrapped binary data.
 * \param[out] len   The length of the binary data.
 *
 * \return Allocated memory with a copy of data from value. The prepended length will not be duplicated.
 */
M_API M_uint8 *M_bin_unwrapdup(const M_uint8 *value, size_t *len);

/*! @} */

__END_DECLS

#endif /* __M_BIN_H__ */
