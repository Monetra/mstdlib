/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
 * Targets unicode 10.0.
 *
 * \note Non-characters are considered an error conditions because
 *       they do not have a defined meaning.
 *
 * @{
 */ 

/*! Error codes. */
typedef enum {
	M_UTF8_ERROR_SUCCESS,         /*!< Success. */
	M_UTF8_ERROR_BAD_START,       /*!< Start of byte sequence is invalid. */
	M_UTF8_ERROR_TRUNCATED,       /*!< The utf-8 character length exceeds the data length. */
	M_UTF8_ERROR_EXPECT_CONTINUE, /*!< A conurbation marker was expected but not found. */
	M_UTF8_ERROR_BAD_CODE_POINT,  /*!< Code point is invalid. */
	M_UTF8_ERROR_OVERLONG,        /*!< Overlong encoding encountered. */
	M_UTF8_ERROR_INVALID_PARAM    /*!< Input parameter is invalid. */
} M_utf8_error_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Check if a given string is valid utf-8 encoded.
 *
 * \param[in]  str    utf-8 string.
 * \param[out] endptr On error, will be set to the character that caused the failure.
 *
 * \return M_TRUE if str is a valid utf-8 sequence. Otherwise, M_FALSE.
 */
M_API M_bool M_utf8_is_valid(const char *str, const char **endptr);


/*! Check if a given code point is valid for utf-8.
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if code point is valid for utf-8. Otherwise, M_FALSE.
 */
M_API M_bool M_utf8_is_valid_cp(M_uint32 cp);


/*! Ge the number of utf-8 characters in a string.
 *
 * This is the number of characters not the number of bytes in the string.
 * M_str_len will only return the same value if the string is only ascii.
 *
 * \param[in] str utf-8 string.
 *
 * \return Number of characters on success. On failure will return 0. Use
 *         M_str_isempty to determine if 0 is a failure or empty string.
 */
M_API size_t M_utf8_cnt(const char *str);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a utf-8 sequence as a code point.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] cp   Code point. Can be NULL.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_get_cp(const char *str, M_uint32 *cp, const char **next);


/*! Read a utf-8 sequence.
 *
 * Output is _not_ NULL terminated.
 *
 * \param[in]  str      utf-8 string.
 * \param[in]  buf      Buffer to put utf-8 sequence. Can be NULL.
 * \param[in]  buf_size Size of the buffer.
 * \param[out] len      Length of the sequence that was put into buffer.
 * \param[out] next     Start of next character. Will point to NULL terminator
 *                      if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_get_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next);


/*! Read a utf-8 sequence into an M_buf_t.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put utf-8 sequence.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_get_chr_buf(const char *str, M_buf_t *buf, const char **next);


/*! Get the location of the next utf-8 sequence.
 *
 * Does not validate characters. Useful when parsing an invalid string and
 * wanting to move past to ignore or replace invalid characters.
 *
 * \param[in] str utf-8 string.
 *
 * \return Pointer to next character in sequence.
 */
M_API char *M_utf8_next_chr(const char *str);


/*! Convert a code point to a utf-8 sequence.
 *
 * Output is _not_ NULL terminated.
 *
 * \param[in]  buf      Buffer to put utf-8 sequence.
 * \param[in]  buf_size Size of the buffer.
 * \param[out] len      Length of the sequence that was put into buffer.
 * \param[in]  cp       Code point to convert from.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_from_cp(char *buf, size_t buf_size, size_t *len, M_uint32 cp);


/*! Convert a code point to a utf-8 sequence writing to an M_buf_t.
 *
 * \param[in] buf Buffer to put utf-8 sequence.
 * \param[in] cp  Code point to convert from.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_from_cp_buf(M_buf_t *buf, M_uint32 cp);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the code point at a given index.
 *
 * Index is based on M_utf8_cnt _not_ the number of bytes.
 * This causes a *full* scan of the string. Iteration should use
 * M_utf8_get_cp.
 *
 * \param[in]  str utf-8 string.
 * \param[in]  idx Index.
 * \param[out] cp  Code point.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_cp_at(const char *str, size_t idx, M_uint32 *cp);


/*! Get the utf-8 sequence at a given index.
 *
 * Index is based on M_utf8_cnt _not_ the number of bytes.
 * This causes a *full* scan of the string. Iteration should use
 * M_utf8_get_chr.
 *
 * \param[in]  str      utf-8 string.
 * \param[in]  buf      Buffer to put utf-8 sequence.
 * \param[in]  buf_size Size of the buffer.
 * \param[out] len      Length of the sequence that was put into buffer.
 * \param[in]  idx      Index.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_chr_at(const char *str, char *buf, size_t buf_size, size_t *len, size_t idx);

/*! @} */

__END_DECLS

#endif /* __M_UTF8_H__ */
