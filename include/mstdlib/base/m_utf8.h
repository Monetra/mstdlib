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
 * A utf-8 sequence is defined as the variable number of bytes that represent
 * a single utf-8 display character.
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
 * \param[out] endptr On success, will be set to the NULL terminator.
 *                    On error, will be set to the character that caused the failure.
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


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_utf8_case Case Folding
 *  \ingroup m_utf8
 *
 * The case folding as defined by the official UTF-8 mapping is utalized.
 * UTF-8 does not have a one to one mapping for case folding. Multiple codes
 * can fold to the same code point. Coversion to upper, then to lower, then
 * back to upper can result in a different upper case string than the original
 * input.
 *
 * For example, 0x004B (capital K) maps to 0x006B (lower k).
 * 0x212A (kelvin sign) also maps to 0x006B. 0x006B maps to
 * 0x004B. So converting 0x212A to lower then back to upper
 * will output 0x004B.
 *
 * \note
 * Not all characters have a case equivalent. These characters
 * will return themselves when folded.
 *
 * @{
 */

/*! Convert a code point to the equivalent upper case code point.
 *
 * \param[in]  cp       Code point to convert.
 * \param[out] upper_cp Equivalent upper case code point. Or cp if
 *                      there is no equivalent.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_toupper_cp(M_uint32 cp, M_uint32 *upper_cp);


/*! Read a utf-8 sequence converting to upper case.
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
M_API M_utf8_error_t M_utf8_toupper_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next);


/*! Read a utf-8 sequence into an M_buf_t converting to upper case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put upper case utf-8 sequence.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_toupper_chr_buf(const char *str, M_buf_t *buf, const char **next);


/*! Convert a utf-8 string to an upper case equivalent string.
 *
 * \param[in]   str  utf-8 string.
 * \param[out]  out  Upper case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_toupper(const char *str, char **out);


/*! Read a utf-8 string into an M_buf_t converting to upper case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put upper case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_toupper_buf(const char *str, M_buf_t *buf);


/*! Convert a code point to the equivalent loer case code point.
 *
 * \param[in]  cp       Code point to convert.
 * \param[out] lower_cp Equivalent lower case code point. Or cp if
 *                      there is no equivalent.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_tolower_cp(M_uint32 cp, M_uint32 *lower_cp);


/*! Read a utf-8 sequence converting to lower case.
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
M_API M_utf8_error_t M_utf8_tolower_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next);


/*! Read a utf-8 sequence into an M_buf_t converting to lower case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put lower case utf-8 sequence.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_tolower_chr_buf(const char *str, M_buf_t *buf, const char **next);


/*! Convert a utf-8 string to an lower case equivalent string.
 *
 * \param[in]   str  utf-8 string.
 * \param[out]  out  Lower case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_tolower(const char *str, char **out);


/*! Read a utf-8 string into an M_buf_t converting to lower case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put lower case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_tolower_buf(const char *str, M_buf_t *buf);


/*! Convert a code point to the equivalent title case code point.
 *
 * \param[in]  cp       Code point to convert.
 * \param[out] title_cp Equivalent title case code point. Or cp if
 *                      there is no equivalent.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_totitle_cp(M_uint32 cp, M_uint32 *title_cp);


/*! Read a utf-8 sequence converting to title case.
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
M_API M_utf8_error_t M_utf8_totitle_chr(const char *str, char *buf, size_t buf_size, size_t *len, const char **next);


/*! Read a utf-8 sequence into an M_buf_t converting to title case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put title case utf-8 sequence.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_totitle_chr_buf(const char *str, M_buf_t *buf, const char **next);


/*! Convert a utf-8 string to an title case equivalent string.
 *
 * \param[in]   str  utf-8 string.
 * \param[out]  out  Lower case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_totitle(const char *str, char **out);


/*! Read a utf-8 string into an M_buf_t converting to title case.
 *
 * \param[in]  str  utf-8 string.
 * \param[in]  buf  Buffer to put title case utf-8 string.
 *
 * \return Result.
 */
M_API M_utf8_error_t M_utf8_totitle_buf(const char *str, M_buf_t *buf);

/*! @} */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_utf8_check Checking/Validation
 *  \ingroup m_utf8
 *
 * UTF-8 Checking and Validation
 *
 * Validate if a UTF-8 sequence or string is comprised
 * of a given type of characters.
 *
 * @{
 */

/*! Checks for a lower-case code point.
 *
 * Derived Core Properties: Lowercase.
 * -> General Category: Ll + Other_Lowercase
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if lowercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_islower_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is lower-case.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if lowercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_islower_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is lower-case.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if lowercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_islower(const char *str);


/*! Checks for a upper-case code point.
 *
 * Derived Core Properties: Uppercase.
 * -> General Category: Lu + Other_Uppercase
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if uppercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isupper_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is upper-case.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if uppercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isupper_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is upper-case.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if uppercase. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isupper(const char *str);


/*! Checks for an alphabetic cp.
 *
 * Derived Core Properties: Alphabetic.
 * -> Lowercase + Uppercase + Lt + Lm + Lo + Nl + Other_Alphabetic
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if alphabetic. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalpha_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is alphabetic.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if alphabetic. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalpha_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is alphabetic.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if alphabetic. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalpha(const char *str);


/*! Checks for an alphabetic or numeric cp.
 *
 * Alphabetic + Nd + Nl + No.
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if alphanumeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalnum_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is alphabetic or numeric.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if alphanumeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalnum_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is alphabetic or numeric.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if alphanumeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isalnum(const char *str);


/*! Checks for numeric code point.
 *
 * General Category: Nd, Nl, No
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if numeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isnum_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is numeric.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if numeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isnum_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is numeric.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if numeric. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isnum(const char *str);


/*! Checks for a control character code point.
 *
 * General Category: Cc
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if control. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_iscntrl_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is a control character.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if control. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_iscntrl_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is a control character.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if control. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_iscntrl(const char *str);


/*! Checks for a punctuation code point.
 *
 * General Category: Pc + Pd + Ps + Pe + Pi + Pf + Po
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if punctuation. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_ispunct_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is punctuation.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if punctuation. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_ispunct_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is punctuation.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if punctuation. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_ispunct(const char *str);


/*! Checks for a printable codepoint.
 *
 * Defined as tables L, M, N, P, S, ASCII space, and UniHan.
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if printable. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isprint_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is printable.
 *
 * Defined as tables L, M, N, P, S and ASCII space
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if printable. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isprint_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is printable.
 *
 * Defined as tables L, M, N, P, S, ASCII space, and UniHan.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if printable. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isprint(const char *str);


/*! Checks for a unihan codepoint.
 *
 * Defined as tables L, M, N, P, S, ASCII space, and UniHan.
 *
 * \param[in] cp Code point.
 *
 * \return M_TRUE if unihan. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isunihan_cp(M_uint32 cp);


/*! Checks if a utf-8 sequence is unihan.
 *
 * \param[in]  str  utf-8 string.
 * \param[out] next Start of next character. Will point to NULL terminator
 *                  if last character.
 *
 * \return M_TRUE if unihan. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isunihan_chr(const char *str, const char **next);


/*! Checks if a utf-8 string is unihan.
 *
 * \param[in] str utf-8 string.
 *
 * \return M_TRUE if unihan. Otherwise M_FALSE.
 */
M_API M_bool M_utf8_isunihan(const char *str);

/*! @} */

/*! @} */

__END_DECLS

#endif /* __M_UTF8_H__ */
