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

#ifndef __M_CHR_H__
#define __M_CHR_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


M_BEGIN_IGNORE_REDECLARATIONS
#  if M_BLACKLIST_FUNC == 1
#    ifdef isalnum
#      undef isalnum
#    else
       M_DEPRECATED_FOR(M_chr_isalnum, int isalnum(int c))
#    endif

#    ifdef isalpha
#      undef isalpha
#    else
       M_DEPRECATED_FOR(M_chr_isalpha, int isalpha(int c))
#    endif

#    ifdef iscntrl
#      undef iscntrl
#    else
       M_DEPRECATED_FOR(M_chr_iscntrl, int iscntrl(int c))
#    endif

#    ifdef isdigit
#      undef isdigit
#    else
       M_DEPRECATED_FOR(M_chr_isdigit, int isdigit(int c))
#    endif

#    ifdef isgraph
#      undef isgraph
#    else
       M_DEPRECATED_FOR(M_chr_isgraph, int isgraph(int c))
#    endif

#    ifdef islower
#      undef islower
#    else
       M_DEPRECATED_FOR(M_chr_islower, int islower(int c))
#    endif

#    ifdef isprint
#      undef isprint
#    else
       M_DEPRECATED_FOR(M_chr_isprint, int isprint(int c))
#    endif

#    ifdef ispunct
#      undef ispunct
#    else
       M_DEPRECATED_FOR(M_chr_ispunct, int ispunct(int c))
#    endif

#    ifdef isspace
#      undef isspace
#    else
       M_DEPRECATED_FOR(M_chr_isspace, int isspace(int c))
#    endif

#    ifdef isupper
#      undef isupper
#    else
       M_DEPRECATED_FOR(M_chr_isupper, int isupper(int c))
#    endif

#    ifdef isxdigit
#      undef isxdigit
#    else
       M_DEPRECATED_FOR(M_chr_isxdigit, int isxdigit(int c))
#    endif

#    ifdef isascii
#      undef isascii
#    else
       M_DEPRECATED_FOR(M_chr_isascii, int isascii(int c))
#    endif

#    ifdef isblank
#      undef isblank
#    else
       M_DEPRECATED_FOR(M_chr_isblank, int isblank(int c))
#    endif
#  endif
M_END_IGNORE_REDECLARATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/*! \addtogroup m_chr Character
 *  \ingroup mstdlib_base
 *
 * ASCII character checks and conversion.
 *
 * Handles checking if a caracter is a certain type. Also handles
 * converting characters to other representations. Such as to lowercase,
 * and to uppercase.
 *
 * @{
 */

typedef M_bool (*M_chr_predicate_func)(char c);
typedef char (*M_chr_map_func)(char);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Checks for an alphanumeric character.
 *
 * Equivalent regular expression: <tt>[A-Za-z0-9]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isalnum(char c);


/*! Checks for an alphanumeric character, spaces allowed.
 *
 * Equivalent regular expression: <tt>[A-Za-z0-9 ]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isalnumsp(char c);


/*! Checks for an alphabetic character.
 *
 * Equivalent regular expression: <tt>[A-Za-z]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isalpha(char c);


/*! Checks for an alphabetic character, spaces allowed.
 *
 * Equivalent regular expression: <tt>[A-Za-z ]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isalphasp(char c);


/*! Checks for 7-bit ASCII character.
 *
 * Equivalent regular expression: <tt>[\\x00-\\x7f]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isascii(char c);


/*! Checks for a blank character.
 *
 * Equivalent regular expression: <tt>[ \\t]</tt>
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isblank(char c);


/*! Checks for a control character.
 *
 * Equivalent regular expression: <tt>[ \\t]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_iscntrl(char c);


/*! Checks for a digit zero through nine.
 *
 * Equivalent regular expression: <tt>[0-9]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isdigit(char c);


/*! Checks for a digit zero through nine or a period.
 *
 * Equivalent regular expression: <tt>[0-9\.]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isdec(char c);


/*! Checks for any printable character except space.
 *
 * Equivalent regular expression: <tt>[!-~]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isgraph(char c);


/*! Checks for a lower-case character [a-z].
 *
 * Equivalent regular expression: <tt>[a-z]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_islower(char c);


/*! Checks for an uppercase letter.
 *
 * Equivalent regular expression: <tt>[A-Z]</tt>.
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isupper(char c);


/*! Checks for any printable character including space.
 *
 * Equivalent regular expression: <tt>[ -~]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isprint(char c);


/*! Checks for any printable character which is not a space or an alphanumeric character.
 *
 * Equivalent regular expression: <tt>[!-/:-@[-`{-~]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_ispunct(char c);


/*! Checks for white-space characters.
 *
 * Equivalent regular expression: <tt>[\\f\\n\\r\\t\\v]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_isspace(char c);


/*! Checks for a hexadecimal digits.
 *
 * Equivalent regular expression: <tt>[0-9a-fA-F]</tt>.
 *
 * \param[in] c The character to check.
 *
 * \return True if in character class or false if not.
 */
M_API M_bool M_chr_ishex(char c);


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert character to lower case, if possible.
 *
 * \param[in] c The character to convert.
 *
 * \return c if not uppercase, otherwise the lowercase equivalent of c.
 */
M_API char M_chr_tolower(char c);


/*! Convert character to upper case, if possible
 *
 * \param[in] c The character to convert.
 *
 * \return c if not lowercase, otherwise the uppercase equivalent of c
 */
M_API char M_chr_toupper(char c);


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a base-10 digit represented as a character to its corresponding integer representation.
 *
 * \param[in] c The decimal character to convert.
 *
 * \return 0-9 on valid input, -1 if c is not a digit.
 */
M_API int M_chr_digit(char c);


/*! Convert a base-16 (hexadecimal) digit represented as a character to its corresponding integer representation.
 *
 * \param[in] c The hexadecimal character to convert.
 *
 * \return 0-9 on valid input, -1 if c is not a digit.
 */
M_API int M_chr_xdigit(char c);


/*! @} */

__END_DECLS

#endif /* __M_CHR_H__* */
