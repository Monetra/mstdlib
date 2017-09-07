/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_STR_H__
#define __M_STR_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/base/m_chr.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

M_BEGIN_IGNORE_REDECLARATIONS
#if M_BLACKLIST_FUNC == 1
#  ifdef strlen
#    undef strlen
#  else
     M_DEPRECATED_FOR(M_str_len, size_t strlen(const char *))
#  endif
#  ifdef strcat
#    undef strcat
#  else
     M_DEPRECATED_FOR(M_str_cat, char  *strcat(char  *, const char *))
#  endif
#  ifdef strcpy
#    undef strcpy
#  else
     M_DEPRECATED_FOR(M_str_cpy, char  *strcpy(char  *, const char *))
#  endif
#  ifdef strncat
#    undef strncat
#  else
     M_DEPRECATED_FOR(M_str_cat, char  *strncat(char *, const char *, size_t))
#  endif
#  ifdef strlcat
#    undef strlcat
#  else
     M_DEPRECATED_FOR(M_str_cat, size_t strlcat(char *, const char *, size_t))
#  endif
#  ifdef strncpy
#    undef strncpy
#  else
     M_DEPRECATED_FOR(M_str_cpy, char  *strncpy(char *, const char *, size_t))
#  endif
#  ifdef strlcpy
#    undef strlcpy
#  else
     M_DEPRECATED_FOR(M_str_cpy, size_t strlcpy(char *, const char *, size_t))
#  endif

#  ifdef strcmp
#    undef strcmp
#  else
     M_DEPRECATED_FOR(M_str_eq, int strcmp(const char *, const char *))
#  endif

#  ifdef strncmp
#    undef strncmp
#  else
     M_DEPRECATED_FOR(M_str_eq_max, int strncmp(const char *, const char *, size_t))
#  endif

#  ifdef strcasecmp
#    undef strcasecmp
#  else
     M_DEPRECATED_FOR(M_str_caseeq, int strcasecmp(const char *, const char *))
#  endif

#  ifdef strncasecmp
#    undef strncasecmp
#  else
     M_DEPRECATED_FOR(M_str_caseeq_max, int strncasecmp(const char *, const char *, size_t))
#  endif

#  ifdef atoi
#    undef atoi
#  else
     M_DEPRECATED_FOR(M_str_to_int32, int atoi(const char *))
#  endif

#  ifdef atoll
#    undef atoll
#  else
     M_DEPRECATED_FOR(M_str_to_int64, long long atoll(const char *))
#  endif

#endif
M_END_IGNORE_REDECLARATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \defgroup m_string String Functions
 *  \ingroup mstdlib_base
 *   String Validation and Manipulation Functions
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_str_check String Checking/Validation
 *  \ingroup m_string
 *
 *  String Checking and Validation
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Safety Wrappers
 */

/*! Ensure a NULL will not be used as a string.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return "" if s is NULL or s.
 */
M_API const char *M_str_safe(const char *s);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Query
 */

/*! Determines if the string is considered empty.
 *
 * A string is considered empty if it is NULL or has a 0 length.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if it is empty. Otherwise M_FALSE.
 */
M_API M_bool M_str_isempty(const char *s) M_WARN_UNUSED_RESULT;


/*! Check if a string is considered true.
 *
 * A string is considered true when it equals any of the following (case intensive):
 * - t
 * - true
 * - y
 * - yes
 * - 1
 * - on
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if it is considered true. Otherwise M_FALSE.
 */
M_API M_bool M_str_istrue(const char *s) M_WARN_UNUSED_RESULT;


/*! A wrapper around strlen that treats NULL as a string with length 0.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return Length of string.
 */
M_API size_t M_str_len(const char *s) M_WARN_UNUSED_RESULT;


/*! A wrapper around strlen that treats NULL as a string with length 0, but returns at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes to return.
 *
 * \return Length of string up to max bytes.
 */
M_API size_t M_str_len_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;

/*! Determines if all characters of string \p s satisfy predicate \p pred.
 *
 * \param[in] s    NULL-terminated string.
 * \param[in] pred Predicate to apply to each character.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_ispredicate(const char *s, M_chr_predicate_func pred);


/*! Check whether each character of a string s are alphanumeric.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalnum(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s ar alphanumeric or contains spaces.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalnumsp(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is alpha.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalpha(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is alpha or contains spaces.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalphasp(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a space.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isspace(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is printable except space.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isgraph(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is printable.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isprint(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a hexadecimal digit.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 *         otherwise M_TRUE
 */
M_API M_bool M_str_ishex(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a decimal digit 0-9.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isnum(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a decimal digit 0-9 or contains a decimal.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isdec(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a money amount.
 *
 * Assumes no more than 2 decimal places but does not require 2
 * decimial digits.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_ismoney(const char *s) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string is in the given character set.
 *
 * \param[in] str      string to check (NULL-terminated).
 * \param[in] charset  list of characters that are allowed in \a str (NULL-terminated).
 * \return             M_TRUE if all characters in \a str match at least one char in \a charset.
 */
M_API M_bool M_str_ischarset(const char *str, const char *charset);


/*! Check whether each character of a string is not in the given character set.
 * 
 * \param[in] str      string to check (NULL-terminated).
 * \param[in] charset  list of characters that are not allowed in \a str (NULL-terminated).
 * \return             M_TRUE if none of the characters in \a charset are present in \a str.
 */
M_API M_bool M_str_isnotcharset(const char *str, const char *charset);


/*! Check whether or not the data provided is a string.
 *   
 *  This is useful for parsing binary protocols that contain string data as a
 *  verification.  The length passed in is the size of the buffer, the last byte
 *  of the buffer must be a NULL terminator or this function will fail (This means,
 *  of course, the string length should be exactly 1 byte less than the provided
 *  buffer size).  Then the remainder of the buffer will be checked for printable
 *  data, otherwise it is not considered a string.
 *
 *  \param[in] s       Buffer to see if data is a null-terminated string
 *  \param[in] len     Size of buffer, including NULL terminator
 *  \return M_TRUE if buffer contains a string, M_FALSE otherwise
 */
M_API M_bool M_str_isstr(const unsigned char *s, size_t len);


/*! Determines if the first \p max characters of string \p s satisfy predicate \p pred up to max bytes.
 *
 * \param[in] s    NULL-terminated string.
 * \param[in] max  Max number of characters to process of \p s.
 * \param[in] pred Predicate to apply to each character.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_ispredicate_max(const char *s, size_t max, M_chr_predicate_func pred);

/*! Check whether each character of a string s are alphanumeric up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalnum_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s ar alphanumeric or contains spaces up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalnumsp_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is alpha up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalpha_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is alpha or contains spaces up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isalphasp_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a space up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isspace_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is printable except space up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isgraph_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is printable up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isprint_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a hexadecimal digit up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_ishex_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a decimal digit 0-9 up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isnum_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! Check whether each character of a string s is a decimal digit 0-9 or contains a decimal up to at most max bytes.
 *
 * \param[in] s   NULL-terminated string.
 * \param[in] max Maximum number of bytes.
 *
 * \return M_TRUE if all characters match. Otherwise M_FALSE.
 */
M_API M_bool M_str_isdec_max(const char *s, size_t max) M_WARN_UNUSED_RESULT;


/*! A wrapper around strcmp that treats NULL as an empty string.
 *
 * NOTE: this is not a constant-time comparison and thus should ONLY be used
 *       for sorting such as within qsort()!
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return An integer less than, equal to, or greater than zero if  s1 is
 *         less than, equal, or greater than s2 respectively.
 */
M_API int M_str_cmpsort(const char *s1, const char *s2) M_WARN_UNUSED_RESULT;


/*! A wrapper around strcmp that treats NULL as an empty string, but limited to max characters.
 *
 * NOTE: this is not a constant-time comparison and thus should ONLY be used
 *       for sorting such as within qsort()!
 *
 * \param[in] s1  NULL-terminated string.
 * \param[in] s2  NULL-terminated string.
 * \param[in] max Max number of characters to process of \p s.
 *
 * \return An integer less than, equal to, or greater than zero if  s1 is
 *         less than, equal, or greater than s2 respectively.
 */
M_API int M_str_cmpsort_max(const char *s1, const char *s2, size_t max) M_WARN_UNUSED_RESULT;


/*! A wrapper around strcmp that treats NULL as an empty string and compares case insensitive.
 *
 * NOTE: this is not a constant-time comparison and thus should ONLY be used
 *       for sorting such as within qsort()!
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return An integer less than, equal to, or greater than zero if  s1 is
 *         less than, equal, or greater than s2 respectively.
 */
M_API int M_str_casecmpsort(const char *s1, const char *s2) M_WARN_UNUSED_RESULT;


/*! A wrapper around strcmp that treats NULL as an empty string and compares case insensitive, but limited
 * to max characters.
 *
 * NOTE: this is not a constant-time comparison and thus should ONLY be used
 *       for sorting such as within qsort()!
 *
 * \param[in] s1  NULL-terminated string.
 * \param[in] s2  NULL-terminated string.
 * \param[in] max Max number of characters to process of \p s.
 *
 * \return An integer less than, equal to, or greater than zero if  s1 is
 *         less than, equal, or greater than s2 respectively.
 */
M_API int M_str_casecmpsort_max(const char *s1, const char *s2, size_t max) M_WARN_UNUSED_RESULT;


/*! Comparison for string equality. 
 *
 * This implementation is constant-time meaning it should not be vulnerable to timing-based attacks. 
 * Limited to first max bytes.  NULL and "" are considered equal strings.
 *
 * \param[in] s1  NULL-terminated string.
 * \param[in] s2  NULL-terminated string.
 * \param[in] max maximum length to check, or 0 for no maximum.
 *
 * \return M_TRUE if equal, M_FALSE if not equal.
 */
M_API M_bool M_str_eq_max(const char *s1, const char *s2, size_t max);


/*! Comparison for string equality.
 *
 * This implementation is constant-time meaning it should not be vulnerable to timing-based attacks.
 * NULL and "" are considered equal strings.
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return M_TRUE if equal, M_FALSE if not equal.
 */
M_API M_bool M_str_eq(const char *s1, const char *s2);


/*! Comparison for string equality in a case-insensitive manner.
 *
 * This implementation is constant-time meaning it should not be vulnerable to timing-based attacks.
 * Limited to first max bytes. NULL and "" are considered equal strings.
 *
 * \param[in] s1  NULL-terminated string.
 * \param[in] s2  NULL-terminated string.
 * \param[in] max maximum length to check, or 0 for no maximum.
 *
 * \return M_TRUE if equal, M_FALSE if not equal.
 */
M_API M_bool M_str_caseeq_max(const char *s1, const char *s2, size_t max);


/*! Comparison for string equality in a case-insensitive manner.
 *
 * This implementation is constant-time meaning it should not be vulnerable to timing-based attacks.
 * NULL and "" are considered equal strings.
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return M_TRUE if equal, M_FALSE if not equal.
 */
M_API M_bool M_str_caseeq(const char *s1, const char *s2);


/*! Determine if a string ends with a given string.
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return M_TRUE if s1 ends with s2, otherwise M_FALSE;
 */
M_API M_bool M_str_eq_end(const char *s1, const char *s2);


/*! Determine if a string ends with a given string in a case-insensitive manner.
 *
 * \param[in] s1 NULL-terminated string.
 * \param[in] s2 NULL-terminated string.
 *
 * \return M_TRUE if s1 ends with s2, otherwise M_FALSE;
 */
M_API M_bool M_str_caseeq_end(const char *s1, const char *s2);


/*! Determine if a string starts with a given string.
 *
 * \param[in] s1 NULL-terminated string, or non-terminated string that's at least as long as s2.
 * \param[in] s2 NULL-terminated string.
 *
 * \return       M_TRUE if s1 starts with s2, otherwise M_FALSE.
 */
M_API M_bool M_str_eq_start(const char *s1, const char *s2);


/*! Determine if a string starts with a given string in a case-insensitive manner.
 *
 * \param[in] s1 NULL-terminated string, or non-terminated string that's at least as long as s2.
 * \param[in] s2 NULL-terminated string.
 *
 * \return       M_TRUE if s1 starts with s2 (case insensitive), otherwise M_FALSE.
 */
M_API M_bool M_str_caseeq_start(const char *s1, const char *s2);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Matching
 */

/*! Match pattern against string as per 'man 7 glob'.
 *
 * We don't support newer POSIX functions like named character classes (e.g.
 * [:lower:]), collating symbols, or equivalence class expressions
 *
 * \param[in] pattern The pattern to match using.
 * \param[in] s       NULL-terminated string.
 *
 * \return M_TRUE if the pattern matches the string otherwise M_FALSE.
 */
M_API M_bool M_str_pattern_match(const char *pattern, const char *s) M_WARN_UNUSED_RESULT;


/*! Match pattern against string as per 'man 7 glob' case insensitive.
 *
 * We don't support newer POSIX functions like named character classes (e.g.
 * [:lower:]), collating symbols, or equivalence class expressions
 *
 * \param[in] pattern The pattern to match using.
 * \param[in] s       NULL-terminated string.
 *
 * \return M_TRUE if the pattern matches the string otherwise M_FALSE.
 *
 * \see M_str_pattern_match
 */
M_API M_bool M_str_case_pattern_match(const char *pattern, const char *s) M_WARN_UNUSED_RESULT;



/*! @} */


/*! \addtogroup m_str_dup String Manipulation (and Duplication)
 *  \ingroup m_string
 *
 *  String Manipulation (and Duplication) Functions
 *
 * @{
 */

/*! Justify Flags */
typedef enum {
	M_STR_JUSTIFY_RIGHT              = 0,  /*!< Data is right-justified (padded on left).
	                                            If src exceeds justification length, it is truncated on the left */
	M_STR_JUSTIFY_LEFT               = 1,  /*!< Data is left-justified (padded on right).
	                                            If src exceeds justification length, it is truncated on the left */
	M_STR_JUSTIFY_RIGHT_TRUNC_RIGHT  = 2,  /*!< Data is right-justified (padded on left).
	                                            If src exceeds justification length, it is truncated on the right */
	M_STR_JUSTIFY_LEFT_TRUNC_RIGHT   = 3,  /*!< Data is left-justified (padded on right).
	                                            If src exceeds justification length, it is truncated on the right */
	M_STR_JUSTIFY_RIGHT_NOTRUNC      = 4,  /*!< Data is right-justified (padded on left).
	                                            If src exceeds justification length, destination is not written, error
	                                            is returned */
	M_STR_JUSTIFY_LEFT_NOTRUNC       = 5,  /*!< Data is left-justified (padded on right).
	                                            If src exceeds justification length, destination is not written,
	                                            error is returned */
	M_STR_JUSTIFY_TRUNC_RIGHT        = 6,  /*!< Data is truncated on the right if length is exceeded.  No padding
	                                            is performed */
	M_STR_JUSTIFY_TRUNC_LEFT         = 7,  /*!< Data is truncated on the left if length is exceeded.  No padding
	                                            is performed */
	M_STR_JUSTIFY_CENTER             = 8,  /*!< Data is center-justified (padded on left and right).
	                                            If src exceeds justification length, it is truncated on the left */
	M_STR_JUSTIFY_CENTER_TRUNC_RIGHT = 9,  /*!< Data is center-justified (padded on left and right).
	                                            If src exceeds justification length, it is truncated on the right */
	M_STR_JUSTIFY_CENTER_NO_TRUNC    = 10, /*!< Data is center-justified (padded on left and right).
											    If src exceeds justification length, destination is not writtern,
												error is returned */
	M_STR_JUSTIFY_END                = 11  /*!< Non-used value that marks end of list. */
} M_str_justify_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Construction
 */

/*! Create a duplicate of the NULL-terminated string s.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated string.
 *
 * \see M_free
 */
M_API char *M_strdup(const char *s) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s and additionally applies M_str_upper to the new string.
 *
 * Later s can be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated string.
 *
 * \see M_strdup
 * \see M_str_upper
 * \see M_free
 */
M_API char *M_strdup_upper(const char *s) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s and additionally applies M_str_lower to the new string.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated string.
 *
 * \see M_strdup
 * \see M_str_lower 
 * \see M_free
 */
M_API char *M_strdup_lower(const char *s) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s and additionally applies M_str_trim to the new string.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s NULL-terminated string.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated string.
 *
 * \see M_strdup
 * \see M_str_trim
 * \see M_free
 */
M_API char *M_strdup_trim(const char *s) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s and additionally applies M_str_unquote to the new string.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s      NULL-terminated string.
 * \param[in] quote  Quote character that should be removed.
 * \param[in] escape Character that escapes a quote that is within the quoted string.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated string.
 *
 * \see M_strdup
 * \see M_str_unquote
 * \see M_free
 */
M_API char *M_strdup_unquote(const char *s, unsigned char quote, unsigned char escape) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s, but copy at most max bytes.
 *
 * If s is longer than max, only max bytes are copied. The returned string will always be NULL-terminated.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s   NULL-terminated string (or up to max bytes of s).
 * \param[in] max Maximum number of bytes to copy from s.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated substring
 *         s[0..MAX(max-1,strlen(s))].
 *
 * \see M_free
 */
M_API char *M_strdup_max(const char *s, size_t max) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s, but copy at most max bytes and additionally applies
 * M_str_lower_max to the new string.
 *
 * If s is longer than max, only max bytes are copied. The returned string will always be NULL-terminated.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s   NULL-terminated string (or up to max bytes of s).
 * \param[in] max Maximum number of bytes to copy from s.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated substring
 *         s[0..MAX(max-1,strlen(s))].
 *
 * \see M_strdup_max
 * \see M_str_upper_max
 * \see M_free
 */
M_API char *M_strdup_upper_max(const char *s, size_t max) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s, but copy at most max bytes and additionally applies
 * M_str_lower_max to the new string.
 *
 * If s is longer than max, only max bytes are copied. The returned string will always be NULL-terminated.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s   NULL-terminated string (or up to max bytes of s).
 * \param[in] max Maximum number of bytes to copy from s.
 *
 * \see M_strdup_max
 * \see M_str_lower_max
 * \see M_free
 */
M_API char *M_strdup_lower_max(const char *s, size_t max) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s, but copy at most max bytes and additionally applies
 * M_str_trim_max to the new string.
 *
 * If s is longer than max, only max bytes are copied. The returned string will always be NULL-terminated.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s   NULL-terminated string (or up to max bytes of s).
 * \param[in] max Maximum number of bytes to copy from s.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated substring
 *         s[0..MAX(max-1,strlen(s))].
 *
 * \see M_strdup_max
 * \see M_str_trim_max
 * \see M_free
 */
M_API char *M_strdup_trim_max(const char *s, size_t max) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a duplicate of the NULL-terminated string s, but copy at most max bytes and additionally applies
 * M_str_unquote_max to the new string.
 *
 * If s is longer than max, only max bytes are copied. The returned string will always be NULL-terminated.
 *
 * s must be passed to M_free to release the memory space associated with it.
 *
 * \param[in] s      NULL-terminated string (or up to max bytes of s).
 * \param[in] quote  Quote character that should be removed.
 * \param[in] escape Character that escapes a quote that is within the quoted string.
 * \param[in] max    Maximum number of bytes to copy from s.
 *
 * \return NULL when insufficient memory or s is NULL. Otherwise a NULL-terminated substring
 *         s[0..MAX(max-1,strlen(s))].
 * \see M_strdup_max
 * \see M_free
 */
M_API char *M_strdup_unquote_max(const char *s, unsigned char quote, unsigned char escape, size_t max) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Justifies the input source as specified by the parameters and writes it to a new duplicate string.
 *
 * \param[in]  src      Null-terminated input string to be justified.
 * \param[in]  justtype Type of justification to be performed.
 * \param[in]  justchar Character to use as padding/filler for justification.
 *                      (ignored if M_JUSTIFY_TRUNC_RIGHT or M_JUSTIFY_TRUNC_LEFT)
 * \param[in]  justlen  Length requested for justification (or truncation).
 *
 * \return NULL on error (such as if it would truncate when requested not to, or invalid use).
 *         New null-terminated string containing justified output on success.
 */
M_API char *M_strdup_justify(const char *src, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Replace a all characters matching a given character set with a string.
 *
 * \param[in] s       NULL-terminated string.
 * \param[in] bcs     Character set.
 * \param[in] bcs_len Number of characters in the given set.
 * \param[in] a       Replacement string for every character in the character set.
 *
 * \return NULL terminated string on success, Otherwise NULL.
 *
 * \see M_str_replace_chr
 * \see M_strdup_replace_str
 */
M_API char *M_strdup_replace_charset(const char *s, const unsigned char *bcs, size_t bcs_len, const char *a);


/*! Replace a string with another string.
 *
 * \param[in] s NULL-terminated string.
 * \param[in] b NULL-terminated string to replace.
 * \param[in] a NULL-terminated string o replace with. b is replaced with a.
 *
 * \return NULL terminated string on success, Otherwise NULL.
 *
 * \see M_str_replace_chr
 * \see M_strdup_replace_charset
 */
M_API char *M_strdup_replace_str(const char *s, const char *b, const char *a);


/*! @} */


/*! \addtogroup m_str_mutate String Manipulation (in-place)
 *  \ingroup m_string
 *
 *  String Manipulation (in-place)) Functions
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Mutation
 */

/*! Convert all characters in place to lower case.
 *
 * \param[in,out] s NULL-terminated string.
 *
 * \return pointer to string on success. Otherwise NULL.
 * */
M_API char *M_str_lower(char *s);


/*! Convert all characters in place to lower case up to a maximum length.
 *
 * \param[in,out] s   NULL-terminated string.
 * \param[in]     max Max length to convert.
 *
 * \return pointer to string on success. Otherwise NULL.
 * */
M_API char *M_str_lower_max(char *s, size_t max);


/*! Convert all characters in place to upper case.
 *
 * \param[in,out] s NULL-terminated string.
 *
 * \return Pointer to string on success. Otherwise NULL.
 * */
M_API char *M_str_upper(char *s);


/*! Convert all characters in place to upper case up to a maximum length.
 *
 * \param[in,out] s   NULL-terminated string.
 * \param[in] max max Max length to convert.
 *
 * \return Pointer to string on success. Otherwise NULL.
 * */
M_API char *M_str_upper_max(char *s, size_t max);


/*! Remove whitespace from the beginning and end of the string in place.
 *
 * \param[in,out] s NULL-terminated string.
 *
 * \return Start of the string without whitespace.
 *
 * \see M_chr_isspace
 */
M_API char *M_str_trim(char *s);


/*! Remove whitespace from the beginning and end of the string in place up to a maximum length.
 *
 * \param[in,out] s   NULL-terminated string.
 * \param[in]     max Max length to trim.
 *
 * \return the start of the string without whitespace.
 *
 * \see M_chr_isspace
 */
M_API char *M_str_trim_max(char *s, size_t max);


/*! Return a copy of the given string with bracketed expressions removed.
 * 
 * You must use different characters for \a open and \a close. If you pass the same
 * character for both, this function will return NULL.
 * 
 * For example, the string "abc (asd(e))?" becomes "abc ?" after calling this
 * function with '(' as the \a open bracket and ')' as the \a close bracket.
 * 
 * \see M_str_remove_bracketed_quoted
 * \see M_str_keep_bracketed
 * \see M_str_remove_quoted
 * 
 * \param[in] src    string to copy
 * \param[in] open   character that represents the start of a bracketed expression
 * \param[in] close  character that represents the end of a bracketed expression
 * \return           copy of input string, with bracketed expressions removed
 */
M_API char *M_str_remove_bracketed(const char *src, char open, char close) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return a copy of the given string with bracketed expressions removed.
 * 
 * Brackets inside quoted expressions are ignored.
 * 
 * You must use different characters for \a open and \a close. If you pass the same
 * character for both, this function will return NULL.
 * 
 * For example, the string "abc (asd(e))?" becomes "abc ?" after calling this
 * function with '(' as the \a open bracket and ')' as the \a close bracket.
 * 
 * \see M_str_remove_bracketed_quoted
 * \see M_str_keep_bracketed
 * \see M_str_remove_quoted
 * 
 * \param[in] src     string to copy
 * \param[in] open    character that represents the start of a bracketed expression
 * \param[in] close   character that represents the end of a bracketed expression
 * \param[in] quote   character that represents open/close of quoted string
 * \param[in] escape  character that can be used to escape a quote char
 * \return            copy of input string, with bracketed expressions removed
 */
M_API char *M_str_remove_bracketed_quoted(const char *src, char open, char close, char quote, char escape) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return a copy of the given string with everything outside bracketed expressions removed.
 * 
 * You must use different characters for \a open and \a close. If you pass the same
 * character for both, this function will return NULL.
 * 
 * For example, the string "abc (asd(e))?" becomes "asd(e)" after calling this
 * function with '(' as the \a open bracket and ')' as the \a close bracket.
 * 
 * \see M_str_keep_bracketed_quoted
 * \see M_str_remove_bracketed
 * \see M_str_keep_quoted
 * 
 * \param[in] src    string to copy
 * \param[in] open   character that represents the start of a bracketed expression
 * \param[in] close  character that represents the end of a bracketed expression
 * \return           copy of input string, containing only the contents of bracketed expressions
 */
M_API char *M_str_keep_bracketed(const char *src, char open, char close) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return a copy of the given string with everything outside bracketed expressions removed (quote aware).
 * 
 * Brackets inside quoted expressions are ignored.
 * 
 * You must use different characters for \a open and \a close. If you pass the same
 * character for both, this function will return NULL.
 * 
 * For example, the string "abc (asd(e))?" becomes "asd(e)" after calling this
 * function with '(' as the \a open bracket and ')' as the \a close bracket.
 * 
 * \see M_str_keep_bracketed
 * \see M_str_remove_bracketed
 * \see M_str_keep_quoted
 * 
 * \param[in] src     string to copy
 * \param[in] open    character that represents the start of a bracketed expression
 * \param[in] close   character that represents the end of a bracketed expression
 * \param[in] quote   character that represents open/close of quoted string
 * \param[in] escape  character that can be used to escape a quote char
 * \return            copy of input string, containing only the contents of bracketed expressions
 */
M_API char *M_str_keep_bracketed_quoted(const char *src, char open, char close, char quote, char escape) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return a copy of the given string with quoted expressions removed.
 * 
 * Quote characters that are preceded by the escape character are not processed as
 * quotes. If you don't wish to specify an escape character, pass '\0' for that argument.
 * 
 * \param[in] src          string to copy
 * \param[in] quote_char   character that represents begin/end of a quoted section
 * \param[in] escape_char  character that can be used to escape a quote char
 * \return                 copy of input string, with quoted expressions removed
 */
M_API char *M_str_remove_quoted(const char *src, char quote_char, char escape_char) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return a copy of the given string with everything outside quoted expressions removed.
 * 
 * Quote characters that are preceded by the escape character are not processed as
 * quotes. If you don't wish to specify an escape character, pass '\0' for that argument.
 * 
 * Any escape character sequences ([escape][escape] or [escape][quote]) inside the quoted
 * content are replaced by the characters they represent ([escape] or [quote], respectively).
 * 
 * \param[in] src          string to copy
 * \param[in] quote_char   character that represents begin/end of a quoted section
 * \param[in] escape_char  character that can be added 
 * \return                 copy of input string, containing only the contents of quoted expressions
 */
M_API char *M_str_keep_quoted(const char *src, char quote_char, char escape_char) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Remove quotes from a string and unescape escaped quotes in place.
 *
 * \param[in,out] s      NULL-terminated string.
 * \param[in]     quote  Quote character.
 * \param[in]     escape Escape character. Removed from other escape characters and quotes.
 *
 * \return Start of unquoted string.
 */
M_API char *M_str_unquote(char *s, unsigned char quote, unsigned char escape);


/*! Remove quotes from a string and unescape escaped quotes in place up to a maximum length.
 *
 * \param[in,out] s      NULL-terminated string.
 * \param[in]     quote  Quote character.
 * \param[in]     escape Escape character. Removed from other escape characters and quotes.
 * \param[in]     max    Max length.
 *
 * \return Start of unquoted string.
 */
M_API char *M_str_unquote_max(char *s, unsigned char quote, unsigned char escape, size_t max);


/*! Quote a string
 *
 * \param[in] s      NULL-terminated string.
 * \param[in] quote  Quote character.
 * \param[in] escape Escape character.
 *
 * \return Start of quoted string
 */
M_API char *M_str_quote(const char *s, unsigned char quote, unsigned char escape) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Quote a string up to a maximum length.
 *
 * \param[in] s      NULL-terminated string.
 * \param[in] quote  Quote character.
 * \param[in] escape Escape character.
 * \param[in] max    Max length.
 *
 * \return Start of quoted string.
 */
M_API char *M_str_quote_max(const char *s, unsigned char quote, unsigned char escape, size_t max) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Quote a string only if necessary.
 *
 * Quotes if the string starts or ends with a space. Or if the delimiter is found in the string.
 *
 * \param[out] out    Quoted string.
 * \param[in]  s      NULL-terminated string.
 * \param[in]  quote  Quote character.
 * \param[in]  escape Escape character.
 * \param[in]  delim  Delimiter.
 *
 * \return M_TRUE if the string was quoted and out was set. Otherwise M_FALSE.
 */
M_API M_bool M_str_quote_if_necessary(char **out, const char *s, unsigned char quote, unsigned char escape, unsigned char delim);


/*! Delete all whitespace characters from the string.
 *
 * \param[in,out] s NULL-terminated string.
 *
 * \return Start of string with whitespace removed.
 *
 * \see M_chr_isspace
 */
M_API char *M_str_delete_spaces(char *s);


/*! Delete all newline characters (\\r and \\n) from the string.
 *
 * \param[in,out] s NULL-terminated string.
 *
 * \return Start of string with newlines removed.
 */
M_API char *M_str_delete_newlines(char *s);


/*! Replace a character within a string with another character in place.
 *
 * \param[in] s NULL-terminated string.
 * \param[in] b Character to replace.
 * \param[in] a Character to replace with. b is replaced with a.
 *
 * \return start of string with character replaced. Does not make a duplicate.
 *
 * \see M_strdup_replace_charset
 * \see M_strdup_replace_str
 */
M_API char *M_str_replace_chr(char *s, char b, char a);


/*! Justifies the input source as specified by the parameters and writes it to the destination buffer.
 *  Source and Destination buffers may overlap.
 *
 * \param[out] dest     Destination buffer where the output is placed.
 * \param[in]  destlen  Length of destination buffer.
 * \param[in]  src      Input buffer to be justified.
 * \param[in]  justtype Type of justification to be performed.
 * \param[in]  justchar Character to use as padding/filler for justification
 *                      (ignored if M_JUSTIFY_TRUNC_RIGHT or M_JUSTIFY_TRUNC_LEFT)
 * \param[in]  justlen  Length requested for justification (or truncation).
 *
 * \return 0 on error (such as if it would truncate when requested not to, or invalid use).
 *         Length of justified output on success (typically same as justlen, unless using M_JUSTIFY_TRUNC_RIGHT
 *         or M_JUSTIFY_TRUNC_LEFT).
 */
M_API size_t M_str_justify(char *dest, size_t destlen, const char *src, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen);


/*! Justifies the input source as specified by the parameters and writes it to the destination buffer.
 *  Source and destination buffers may overlap.
 *
 * \param[out] dest     Destination buffer where the output is placed.
 * \param[in]  destlen  Length of destination buffer.
 * \param[in]  src      Input buffer to be justified.
 * \param[in]  srclen   Length of input source.
 * \param[in]  justtype Type of justification to be performed.
 * \param[in]  justchar Character to use as padding/filler for justification.
 *                      (ignored if M_JUSTIFY_TRUNC_RIGHT or M_JUSTIFY_TRUNC_LEFT)
 * \param[in]  justlen  Length requested for justification (or truncation).
 *
 * \return 0 on error (such as if it would truncate when requested not to, or invalid use).
 *         Length of justified output on success (typically same as justlen, unless using M_JUSTIFY_TRUNC_RIGHT
 *         or M_JUSTIFY_TRUNC_LEFT).
 */
M_API size_t M_str_justify_max(char *dest, size_t destlen, const char *src, size_t srclen, M_str_justify_type_t justtype, unsigned char justchar, size_t justlen);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Fill
 */

/*! Copy a string from one location to another.
 *
 * This guarantees NULL termination of dest.
 *
 * \param[out] dest     Destination buffer where the output is placed.
 * \param[in]  dest_len Length of destination buffer.
 * \param[in]  src      Input buffer to be justified.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_str_cpy(char *dest, size_t dest_len, const char *src);


/*! Copy a given length of a string from one location to another.
 *
 * This guarantees NULL termination of dest.
 *
 * \param[out] dest     Destination buffer where the output is placed.
 * \param[in]  dest_len Length of destination buffer.
 * \param[in]  src      Input buffer to be justified.
 * \param[in]  src_len  Length of source string. 
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_str_cpy_max(char *dest, size_t dest_len, const char *src, size_t src_len);


/*! Append a string on to another.
 *
 * \param[in,out] dest     String to be appended to.
 * \param[in]     dest_len The length of dest.
 * \param[in]     src      String to be appended.
 *
 * \return M_TRUE if src was appended to dest. Otherwise M_FALSE.
 */
M_API M_bool M_str_cat(char *dest, size_t dest_len, const char *src);


/*! @} */


/*! \addtogroup m_str_search String Searching
 *  \ingroup m_string
 *
 *  String Searching
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Find the first occurrence of c in s.
 *
 * \see M_str_rchr
 * \see M_str_str
 * \see M_str_find_first_from_charset
 * \see M_str_find_first_not_from_charset
 * 
 * \param[in] s NULL-terminated string.
 * \param[in] c Character to search for.
 * \return NULL if s is NULL or c is not found. Otherwise a pointer to the first occurrence of c in s.
 */
M_API char *M_str_chr(const char *s, char c) M_WARN_UNUSED_RESULT;


/*! Find the last occurrence of c in s.
 *
 * \see M_str_chr
 * 
 * \param[in] s NULL-terminated string.
 * \param[in] c Character to search for.
 *
 * \return NULL if s is NULL or c is not found. Otherwise a pointer to the last occurrence of c in s.
 */
M_API char *M_str_rchr(const char *s, char c) M_WARN_UNUSED_RESULT;


/*! Find the first occurence in \a str of any character in \a charset.
 * 
 * This is identical to C standard function strpbrk(), except that it treats NULL pointers
 * as empty strings instead of segfaulting.
 * 
 * \see M_str_find_first_not_from_charset
 * \see M_str_chr
 * \see M_str_str
 * 
 * \param[in] str     string to search in (stored as NULL-terminated C string).
 * \param[in] charset list of chars to search for (stored as NULL-terminated C string).
 * \return            pointer to first matching character from \a charset in \a str, or NULL if no matches found
 */
M_API char *M_str_find_first_from_charset(const char *str, const char *charset);


/*! Find the first occurence in \a str of any character that's not in \a charset.
 * 
 * \see M_str_find_first_from_charset
 * \see M_str_chr
 * \see M_str_str
 * 
 * \param[in] str     string to search in (stored as NULL-terminated C string).
 * \param[in] charset list of chars to skip (stored as NULL-terminated C string).
 * \return            pointer to first char in \a str that's not in \a charset, or NULL if no matches found
 */
M_API char *M_str_find_first_not_from_charset(const char *str, const char *charset);


/*! A wrapper around strstr that treats NULL as the empty string.
 *
 * \see M_str_chr
 * \see M_str_find_first_from_charset
 * \see M_str_find_first_not_from_charset
 * 
 * \param[in] haystack String to search.
 * \param[in] needle   String to search for in haystack.
 *
 * \return Pointer to the first occurrence of needle in haystack; otherwise NULL
 */
M_API char *M_str_str(const char *haystack, const char *needle) M_WARN_UNUSED_RESULT;


/*! A wrapper around strstr that ignores case and treats NULL as the empty string.
 *
 * \param[in] haystack String to search.
 * \param[in] needle   String to search for in haystack.
 *
 * \return Pointer to the first occurrence of needle in haystack; otherwise NULL
 */
M_API char *M_str_casestr(const char *haystack, const char *needle) M_WARN_UNUSED_RESULT;


/*! A wrapper around strstr that ignores case and treats NULL as the empty string.
 *
 * \param[in] haystack String to search.
 * \param[in] needle   String to search for in haystack.
 *
 * \return Integer index to the first occurrence of needle in haystack;
 *         otherwise -1 if needle is not a substring of haystack
 */
M_API ssize_t M_str_casestr_pos(const char *haystack, const char *needle) M_WARN_UNUSED_RESULT;


/*! @} */


/*! \addtogroup m_str_parse String Parsing and Joining
 *  \ingroup m_string
 *
 *  String Parsing and Joining Functions
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Substrings
 */

/*! Clobber the first occurrence of character c in string s with NULL
 *  and return a pointer to the successive character.
 *
 * \param[in] s String to break into segments.
 * \param[in] c Character at which to break into segments.
 *
 * \return NULL if s is NULL. Pointer to trailing NULL if c not found in s.
 *         Otherwise pointer to the character following the first occurrence of c in s.
 */
M_API char *M_str_split_on_char(char *s, char c);


/*! Find each substring of s delimited by delim and additionally record
 *  each substring's length.
 *
 *  The string can contain the character '\0' and will be processed up to \p s_len.
 *  All parts will be NULL terminated.
 *
 *  Empty list elements (consecutive delimiters) will produce empty strings in the
 *  output array.
 *
 *  The last length of len_array will include any trailing '\0'.
 *
 * \param[in]  delim     Delimiter.
 * \param[in]  s         String to search.
 * \param[in]  s_len     Length of string.
 * \param[out] num       The size of len_array and the returned array.
 * \param[out] len_array An array of size num containing the lengths of the substrings.
 *
 * \return an array of length num containing all substrings.
 */
M_API char **M_str_explode(unsigned char delim, const char *s, size_t s_len, size_t *num, size_t **len_array) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Find each substring of s taking into account quoting.
 *
 *  The string can contain the character '\0' and will be processed up to \p s_len.
 *  All parts are guaranteed to be NULL terminated.
 *  This takes into account if the delimiter is within a quote. Also allows the quote character
 *  to be escaped and not treated as a quote character.
 *
 *  An example of this would be CSV parsing. ',' is a delimiter but if it's in '"' it is not. "
 *  within a " is escaped with " to denote that isn't the end of the quote.
 *
 *  Empty list elements (consecutive delimiters) will produce empty strings in the
 *  output array.
 *
 * \param[in]  delim       Delimiter.
 * \param[in]  s           String to search.
 * \param[in]  s_len       Length of string.
 * \param[in]  quote_char  Character to use to denote quoted segments. Use 0 if not needed.
 * \param[in]  escape_char Character to use for escaping the quote character. Can be the same as the quote character.
 *                         use 0 if not needed.
 * \param[in]  max_sects   Maximum number of parts to return. The last part will have remaining data after last
 *                         allowed split. Use 0 to disable and return all parts.
 * \param[out] num         the size of len_array and the returned array.
 * \param[out] len_array   An array of size num containing the lengths of the substrings.
 *
 * \return an array of num string containing all substrings.
 */
M_API char **M_str_explode_quoted(unsigned char delim, const char *s, size_t s_len, unsigned char quote_char, unsigned char escape_char, size_t max_sects, size_t *num, size_t **len_array) M_WARN_UNUSED_RESULT M_MALLOC;



/*! Find each substring of s (a NULL terminated string).
 *
 * s will only be read until the first NULL. All parts are guaranteed to be NULL terminated.
 *
 *  Empty list elements (consecutive delimiters) will produce empty strings in the
 *  output array.
 *
 * \param[in]  delim Delimiter.
 * \param[in]  s     String to search.
 * \param[out] num   The size of len_array and the returned array.
 *
 * \return an array of num string containing all substrings.
 */
M_API char **M_str_explode_str(unsigned char delim, const char *s, size_t *num) M_WARN_UNUSED_RESULT M_MALLOC;



/*! Split a string among the given number of lines, while keeping words intact.
 * 
 * After you're done with the returned array, you must free it with M_str_explode_free().
 * 
 * Words in this context are defined as contiguous blocks of non-whitespace characters. For each line,
 * leading and trailing whitespace will be trimmed, but internal whitespace will be left alone.
 * 
 * An example use case is breaking up strings for display on small LCD screens.
 * 
 * \see M_str_explode_free
 * \see M_buf_add_str_lines
 * 
 * \param[in]  max_lines  Maximum number of lines to output.
 * \param[in]  max_chars  Maximum characters per line.
 * \param[in]  src_str     Source string.
 * \param[in]  truncate   If true, truncation is allowed. If false, NULL will be returned if the string won't fit.
 * \param[out] num        Number of lines displayed. Will be zero, if no output text was produced.
 * \return                Array of strings where each is a line, or NULL if no output text was produced.
 */
M_API char **M_str_explode_lines(size_t max_lines, size_t max_chars, const char *src_str,
	M_bool truncate, size_t *num) M_WARN_UNUSED_RESULT M_MALLOC;



/*! Find each substring of s (a NULL terminated string) taking into account quoting.
 *
 * s will only be read until the first NULL. All parts are guaranteed to be NULL terminated.
 * This takes into account if the delimiter is within a quote. Also allows the quote character
 * to be escaped and not treated as a quote character.
 *
 * An example of this would be CSV parsing. ',' is a delimiter but if it's in '"' it is not. "
 * within a " is escaped with " to denote that isn't the end of the quote.
 *
 * \param[in]  delim       Delimiter.
 * \param[in]  s           String to search.
 * \param[in]  quote_char  Character to use to denote quoted segments. Use 0 if not needed.
 * \param[in]  escape_char Character to use for escaping the quote character. Can be the same as the quote character.
 *                         use 0 if not needed.
 * \param[in]  max_sects   Maximum number of parts to return. The last part will have remaining data after last
 *                         allowed split. Use 0 to disable and return all parts.
 * \param[out] num         the size of len_array and the returned array.
 *
 * \return an array of num string containing all substrings.
 */
M_API char **M_str_explode_str_quoted(unsigned char delim, const char *s, unsigned char quote_char, unsigned char escape_char, size_t max_sects, size_t *num) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Given a string containing an list of integers delimited by delim, return an array containing the integer values.
 *
 *  For example, after calling:
 *    ints = M_str_explode_int(',', "-10,11,13,,,,-15", &num)
 *  then the returns values will be:
 *    num=4
 *    ints[0] = -10
 *    ints[1] = 11
 *    ints[2] = 13
 *    ints[3] = -15
 *
 * \param[in]  delim Delimiter.
 * \param[in]  s     String containing the integer list.
 * \param[out] num   The number of integers in the returned array.
 *
 * \return an array containing num integers.
 */
M_API int *M_str_explode_int(unsigned char delim, const char *s, size_t *num) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Free the substrings found by M_str_explode*.
 *
 * \param[in] strs An array of strings returned by M_str_explode*.
 * \param[in] num  The number of strings in strs.
 *
 * \see M_str_explode
 */
M_API void M_str_explode_free(char **strs, size_t num) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Join
 */

/*! Join an array of string separated by a delimiter and quoted if the delimiter is present in a string.
 *
 * \param[in] delim          Delimiter.
 * \param[in] enclose_char   Character to use for quoting.
 * \param[in] escape_char    Character used for escaping the quote character if it's found in a string.
 * \param[in] strs           array of string to join.
 * \param[in] num_strs       Number of string in the array of strings.
 * \param[in] always_enclose M_TRUE if all string should be quoted. M_FALSE if strings are only quoted when necessary.
 *
 * \return Joined string.
 */
M_API char *M_str_implode(unsigned char delim, unsigned char enclose_char, unsigned char escape_char, char **strs, size_t num_strs, M_bool always_enclose) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Convert an array of signed integers into a string representation where each
 * integer is delimited by a given character.
 *
 *  For example:
 *      M_str_implode_int('|', {1,-22,333}, 3) => "1|-22|333"
 *
 * \param[in] delim    Delimiter.
 * \param[in] ints     String containing the integer list.
 * \param[in] num_ints The number of integers in the returned array.
 *
 * \return String representation of integer list.
 */
M_API char *M_str_implode_int(unsigned char delim, const int *ints, size_t num_ints) M_WARN_UNUSED_RESULT M_MALLOC;


/*! @} */


/*! \addtogroup m_str_convert String Conversion
 *  \ingroup m_string
 *
 *  String Conversion
 *
 * @{
 */


/*! Possible return codes for integer conversion primitives */
typedef enum {
	M_STR_INT_SUCCESS  = 0, /*!< Successful conversion */
	M_STR_INT_OVERFLOW = 1, /*!< Overflow              */
	M_STR_INT_INVALID  = 2  /*!< Invalid Characters    */
} M_str_int_retval_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Conversion to number
 */

/*! Convert a string representing money (a fractional decimal amount) to an
 * integer number of cents. Fractional amounts are rounded to the nearest cent.
 *
 * \param s String containing a floating point (2 decimal places) money representation.
 *
 * \return Long integer, rounded if there are floating point errors, with amount of cents.
 */
M_API long M_atofi100(const char *s) M_WARN_UNUSED_RESULT;


/*! Convert a floating point number into a 64bit integer using arbitrary
 * precision as defined by impliedDecimals.
 *
 * For instance, if impliedDecimals is 5, and 12.34 is passed, the resulting value would be 1234000.
 *
 * \param[in] s               string to convert to decimal
 * \param[in] impliedDecimals Number of implied decimals resulting output should have
 *
 * \return 64bit integer representing number from s with implied decimals.
 */
M_API M_int64 M_atofi_prec(const char *s, int impliedDecimals) M_WARN_UNUSED_RESULT;


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.
 *
 * \param[in] s NULL-terminated string.
 *
 *  \return Determined integer. On failure will return 0 which cannot be differentiated from a legitimate 0.
 */
M_API M_int64  M_str_to_int64(const char *s) M_WARN_UNUSED_RESULT;


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in] s NULL-terminated string.
 *
 *  \return Determined integer. On failure will return 0 which cannot be differentiated from a legitimate 0.
 */
M_API M_uint64 M_str_to_uint64(const char *s) M_WARN_UNUSED_RESULT;


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in]  s      NULL-terminated string.
 * \param[in]  len    Maximum length of given string to parse.
 * \param[in]  base   Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[out] val    Integer to store result.
 * \param[out] endptr Pointer to store the end of the parsed string.
 *
 * \return One of M_str_int_retval_t return codes.
 */
M_API M_str_int_retval_t M_str_to_int64_ex(const char *s, size_t len, unsigned char base, M_int64 *val, const char **endptr);


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in] s       NULL-terminated string.
 * \param[in] len     Maximum length of given string to parse.
 * \param[in] base    Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[out] val    Integer to store result.
 * \param[out] endptr Pointer to store the end of the parsed string.
 *
 * \return One of M_str_int_retval_t return codes.
 */
M_API M_str_int_retval_t M_str_to_uint64_ex(const char *s, size_t len, unsigned char base, M_uint64 *val, const char **endptr);


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in] s       NULL-terminated string.
 * \param[in] len     Maximum length of given string to parse.
 * \param[in] base    Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[out] val    Integer to store result.
 * \param[out] endptr Pointer to store the end of the parsed string.
 *
 * \return One of M_str_int_retval_t return codes.
 */
M_API M_str_int_retval_t M_str_to_int32_ex(const char *s, size_t len, unsigned char base, M_int32 *val, const char **endptr);


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in]  s      NULL-terminated string.
 * \param[in]  len    Maximum length of given string to parse.
 * \param[in]  base   Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[out] val    Integer to store result
 * \param[out] endptr Pointer to store the end of the parsed string.
 *
 * \return One of M_str_int_retval_t return codes.
 */
M_API M_str_int_retval_t M_str_to_uint32_ex(const char *s, size_t len, unsigned char base, M_uint32 *val, const char **endptr);


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in] s NULL-terminated string.
 *
 * \return determined integer. On failure will return 0 which cannot be differentiated from a legitimate 0.
 */
M_API M_int32 M_str_to_int32(const char *s) M_WARN_UNUSED_RESULT;


/*! Interpret a string as an ascii numeric. String may begin with whitespace which
 *  will be ignored, then an optional + or - sign.  
 *
 * \param[in] s NULL-terminated string.
 *
 * \return determined integer. On failure will return 0 which cannot be differentiated from a legitimate 0.
 */
M_API M_uint32 M_str_to_uint32(const char *s) M_WARN_UNUSED_RESULT;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Money
 */

/*! Verify and convert the amount so it always has 2 decimal digits.
 *
 * Example:
 * 1 for $1 and turns it into 1.00.
 * 1.1 for $1.10 and turns it into 1.10.
 *
 * \param[in] amount Amount to verify/convert.
 *
 * \return Amount with decimal and two decimal digits. Or NULL on error.
 */
M_API char *M_str_dot_money_out(const char *amount);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Conversion
 */

/*! Hex dump flags */
enum M_str_hexdump_flags {
	M_STR_HEXDUMP_NONE    = 0,      /*!< Defaults                                                                   */
	M_STR_HEXDUMP_DECLEN  = 1 << 0, /*!< Default is length in hex (address) format, print in decimal format instead */
	M_STR_HEXDUMP_NOASCII = 1 << 1, /*!< Disable dumping of ASCII representation trailing the hexdump               */
	M_STR_HEXDUMP_HEADER  = 1 << 2, /*!< Add a header above each column of output                                   */
	M_STR_HEXDUMP_NOLEN   = 1 << 3, /*!< Omit the length indicator                                                  */
	M_STR_HEXDUMP_CRLF    = 1 << 4, /*!< Use CRLF newlines (DOS style)                                              */
	M_STR_HEXDUMP_UPPER   = 1 << 5, /*!< Output hex digits as uppercase                                             */
	M_STR_HEXDUMP_NOSECTS = 1 << 6  /*!< Do not put additional emphasis on 8-byte segments                          */
}; 

/*! Generate a hex dump format of binary data meant to be human-readable, or imported via various
  *  hex-dump conversion tools such as Text2pcap.
  *  \param flags          one or more enum M_str_hexdump_flags
  *  \param bytes_per_line Number of bytes represented per line.  If zero is used, defaults to 16
  *  \param line_prefix    Prefix each line of the hex dump with the given data.
  *  \param data           Binary data to be dumped
  *  \param data_len       length of binary data to be dumped
  *  \return Allocated string representing the hex dump.  Must be M_free()'d by the caller.
  */
M_API char *M_str_hexdump(int flags, size_t bytes_per_line, const char *line_prefix, const unsigned char *data, size_t data_len); 

/*! @} */

__END_DECLS

#endif /* __M_STR_H__ */
