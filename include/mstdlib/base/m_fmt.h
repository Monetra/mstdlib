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

#ifndef __M_FMT_H__
#define __M_FMT_H__

#include <stdio.h>
#include <stdarg.h>

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_fs.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

M_BEGIN_IGNORE_REDECLARATIONS
#if M_BLACKLIST_FUNC == 1
/* normal */
#  ifdef printf
#    undef printf
#  else
     M_DEPRECATED_FOR(M_printf,    int printf(const char *format, ...))
#  endif

#  ifdef fprintf
#    undef fprintf
#  else
     M_DEPRECATED_FOR(M_fprintf,   int fprintf(FILE *stream, const char *format, ...))
#  endif

#  ifdef sprintf
#    undef sprintf
#  else
     M_DEPRECATED_FOR(M_sprintf,   int sprintf(char *str, const char *format, ...))
#  endif

#  ifdef snprintf
#    undef snprintf
#  else
     M_DEPRECATED_FOR(M_snprintf,  int snprintf(char *str, size_t size, const char *format, ...))
#  endif

/* var args */
#  ifdef vprintf
#    undef vprintf
#  else
     M_DEPRECATED_FOR(M_vprintf,   int vprintf(const char *format, va_list ap))
#  endif

#  ifdef vfprintf
#    undef vfprintf
#  else
     M_DEPRECATED_FOR(M_vfprintf,  int vfprintf(FILE *stream, const char *format, va_list ap))
#  endif

#  ifdef vsprintf
#    undef vsprintf
#  else
     M_DEPRECATED_FOR(M_vsprintf,  int vsprintf(char *str, const char *format, va_list ap))
#  endif

#  ifdef vsnprintf
#    undef vsnprintf
#  else
     M_DEPRECATED_FOR(M_vsnprintf, int vsnprintf(char *str, size_t size, const char *format, va_list ap))
#  endif

/* allocating */
#  ifdef asprintf
#    undef asprintf
#  else
     M_DEPRECATED_FOR(M_asprintf, int asprintf(char **ret, const char *fmt, ...) M_PRINTF(2,3))
#  endif

#  ifdef vasprintf
#    undef vasprintf
#  else
     M_DEPRECATED_FOR(M_vasprintf, int vasprintf(char **ret, const char *fmt, va_list ap))
#  endif
#endif
M_END_IGNORE_REDECLARATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN64
#  define M_PRIuPTR "llu"
#else
#  define M_PRIuPTR "lu"
#endif

#define  M_PRIu64 "llu"
#define  M_PRId64 "lld"

/*! \addtogroup m_fmt Format String Functions
 *  \ingroup m_string
 *
 * Formatted String output.
 *
 * %\<character\> is used to denote the data type of the function arguments. Arguments are passed
 * after the format string. flags and other modifiers are specified between the % and conversion
 * characters. E.g. %\<behavior\>\<character\>
 *
 * # Supported features
 *
 * ## Flags
 *
 * Flag | Description
 * -----|------------
 * '-'  | Left justify output. Default is to right justify. Overrides the '0' flag if both are set.
 * '+'  | Always add the sign (+-) for numeric output. Default is only to add sign for negative. Overrides the ' ' flag if both are set.
 * '#'  | Add the appropriate prefix to the output of numerics. 0x or 0X for Hex. 0 for Octal.
 * ' '  | Use a space as if it were the sign for positive numbers.
 * '0'  | Pad numerics with 0. Default padding is space (' ').
 *
 * ## Width and precision
 *
 * A decimal (.) separated value can be specified to control the width and precision of the argument.
 * \<width\>.\<precision\>.
 *
 * The width is the minimum output size. Padding will be added if the output would be smaller
 * than the width. If the output size exceeds the width, the width is ignored and the full input
 * will be output.
 *
 * Precision for strings controls the length that should be output. If the value is larger than the length
 * of the string, the string length will be used. E.g. ("%.2s", "abc") will result in "ab" for the output.
 *
 * Precision for floating point determines the number of decimal places to output. The default is 6.
 * It's recommended the maximum precision specified be no large than 14 digits. Digits over 14 can have
 * platform specific rounding differences.
 *
 * Width and precision are both optional. You can specify one, the other, or both. E.g. "%.2s", "%8.s".
 *
 * A '*' can be used instead of a decmail value and will read the size from an argument. The argument is an
 * int. The arguments are read right to left. E.g. ("%*.*s", 4, 2, "abc") will result in "  ab".
 *
 * ## Size modifiers
 *
 * Specify the data size of a given argument.
 *
 * Modifier | Description
 * ---------|------------
 * hh       | Size of char. 8 bit.
 * h        | Size of short. 16 bit.
 * l        | Size of long. 8 or 16 bit (system dependant).
 * ll       | Size of long long. 64 bit.
 * I, z     | size of size_t. Based on system size. 32 or 64 bit.
 * I64      | 64 bit.
 * I32      | 32 bit.
 *
 * ## Conversion
 *
 * Specifies the data type of the argument.
 *
 * Type             | Description
 * -----------------|------------
 * d, i             | Signed integer.
 * o, O             | Unsigned integer. Output as octal.
 * u                | Unsigned integer.
 * x, X             | Unsigned integer. Output as hex. 'x' outputs lowercase, 'X' outputs uppercase.
 * p, P             | Unsigned pointer. Output as hex. 'p' outputs lowercase, 'P' outputs uppercase.
 * e, E, f, F, g, G | Double. All will output in the form [-]ddd.ddd. Default 6 decimal digits unless otherwise precision is otherwise specified.
 * c                | Signed character.
 * s                | String (const char *).
 *
 * @{
 */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to FILE.
 *
 * \param[in] stream FILE stream.
 * \param[in] fmt    Format string.
 * \param[in] ap     arguments.
 *
 * \return Number of characters output. -1 on fatal error.
 */
M_API ssize_t M_vfprintf(FILE *stream, const char *fmt, va_list ap);


/*! Output format string to FILE (varargs).
 *
 * \see M_vfprintf
 */
M_API ssize_t M_fprintf(FILE *stream, const char *fmt, ...) M_PRINTF(2,3) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to mstdlib file descriptor.
 *
 * \param[in] fd  mstdlib file descriptor.
 * \param[in] fmt Format string.
 * \param[in] ap  arguments.
 *
 * \return Number of characters output. -1 on fatal error.
 */
M_API ssize_t M_vmdprintf(M_fs_file_t *fd, const char *fmt, va_list ap);


/*! Output format string to mstdlib file descriptor (varargs).
 *
 * \see M_vmdprintf
 */
M_API ssize_t M_mdprintf(M_fs_file_t *fd, const char *fmt, ...) M_PRINTF(2,3) M_WARN_NONNULL(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to OS file descriptor.
 *
 * \param[in] fd  OS file descriptor.
 * \param[in] fmt Format string.
 * \param[in] ap  arguments.
 *
 * \return Number of characters output. -1 on fatal error.
 */
M_API ssize_t M_vdprintf(int fd, const char *fmt, va_list ap);


/*! Output format string to OS file descriptor (varargs).
 *
 * \see M_vdprintf
 */
M_API ssize_t M_dprintf(int fd, const char *fmt, ...) M_PRINTF(2,3) M_WARN_NONNULL(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to stdout.
 *
 * \param[in] fmt Format string.
 * \param[in] ap  arguments.
 *
 * \return Number of characters output. -1 on fatal error.
 */
M_API ssize_t M_vprintf(const char *fmt, va_list ap);


/*! Output format string to stdout (varargs).
 *
 * \see M_vprintf
 */
M_API ssize_t M_printf(const char *fmt, ...) M_PRINTF(1,2) M_WARN_NONNULL(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to pre-allocated string buffer.
 *
 * Output is NULL terminated.
 *
 * The output will not exceed size of buffer - 1. 1 byte is reserved for the NULL terminator.
 *
 * \param[in] str  Storage location for string.
 * \param[in] size Size of location.
 * \param[in] fmt  Format string.
 * \param[in] ap   arguments.
 *
 * \return The length of the fully formatted string. If the size of the buffer is smaller
 *         than the length the string is truncated but the returned length is not. To
 *         determine truncation check is this return against the str buffer.
 */
M_API size_t M_vsnprintf(char *str, size_t size, const char *fmt, va_list ap);


/*! Output format string to pre-allocated string buffer.
 *
 * \see M_vsnprintf
 */
M_API size_t M_snprintf(char *buf, size_t size, const char *fmt, ...) M_PRINTF(3,4) M_WARN_NONNULL(3);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to a newly allocated string buffer.
 *
 * Output is NULL terminated.
 *
 * \param[out] ret Allocated string.
 * \param[in]  fmt Format string.
 * \param[in]  ap  arguments.
 *
 * \return Number of characters output.
 */
M_API size_t M_vasprintf(char **ret, const char *fmt, va_list ap);


/*! Output format string to a newly allocated string buffer (varargs).
 *
 * \see M_vasprintf
 */
M_API size_t M_asprintf(char **ret, const char *fmt, ...) M_PRINTF(2,3) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output format string to an M_buf buffer.
 *
 * Output is NULL terminated.
 *
 * \param[in] buf Buffer
 * \param[in] fmt Format string.
 * \param[in] ap  arguments.
 *
 * \return Number of characters output.
 */
M_API size_t M_vbprintf(M_buf_t *buf, const char *fmt, va_list ap);


/*! Output format string to an M_buf buffer.
 *
 * \see M_vbprintf
 */
M_API size_t M_bprintf(M_buf_t *buf, const char *fmt, ...) M_PRINTF(2,3) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_FMT_H__ */
