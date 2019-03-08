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

#ifndef __M_TYPES_H__
#define __M_TYPES_H__

#include <mstdlib/base/m_defs.h>

/* C89 */
#include <limits.h>    /* U?LONG_MAX */
#include <stddef.h>    /* NULL */
#include <sys/types.h> /* size_t */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if defined(__GNUC__)
#  pragma GCC system_header
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN32
	 typedef  unsigned __int64     M_uint64;
	 typedef           __int64     M_int64;
#  ifdef _WIN64
	 typedef  unsigned __int64     M_uintptr;
	 typedef           __int64     M_intptr;
#  else
	 typedef  unsigned long        M_uintptr;
	 typedef           long        M_intptr;
#  endif
#else /* ! _WIN32 */
	typedef  unsigned long        M_uintptr;
	typedef           long        M_intptr;
	typedef  unsigned long long   M_uint64;
	typedef           long long   M_int64;
#endif

typedef  unsigned int         M_uint32;
typedef  signed   int         M_int32;
typedef  unsigned short       M_uint16;
typedef  signed   short       M_int16;
typedef  unsigned char        M_uint8;
typedef  signed   char        M_int8;

#ifdef _MSC_VER
	typedef  M_intptr ssize_t;
#endif

#define  M_INT8_MAX    127
#define  M_INT16_MAX   32767
#define  M_INT32_MAX   2147483647L
#define  M_INT64_MAX   9223372036854775807LL

#define  M_INT8_MIN    (-M_INT8_MAX  - 1)
#define  M_INT16_MIN   (-M_INT16_MAX - 1)
#define  M_INT32_MIN   (-M_INT32_MAX - 1L)
#define  M_INT64_MIN   (-M_INT64_MAX - 1LL)

#define  M_UINT8_MAX   (M_INT8_MAX *2    + 1)
#define  M_UINT16_MAX  (M_INT16_MAX*2U   + 1)
#define  M_UINT32_MAX  (M_INT32_MAX*2UL  + 1)
#define  M_UINT64_MAX  (M_INT64_MAX*2ULL + 1)

#ifdef _WIN64
#  define  M_UINTPTR_MAX  M_UINT64_MAX
#  define  M_INTPTR_MAX   M_INT64_MAX
#  define  M_INTPTR_MIN   M_INT64_MIN
#else
#  define  M_UINTPTR_MAX  ULONG_MAX
#  define  M_INTPTR_MAX   LONG_MAX
#  define  M_INTPTR_MIN   LONG_MIN
#endif

#ifndef SSIZE_MAX
#  define  SSIZE_MAX  M_INTPTR_MAX
#endif
#ifndef SSIZE_MIN
#  define  SSIZE_MIN  M_INTPTR_MIN
#endif
#ifndef SIZE_MAX
#  define  SIZE_MAX   ((size_t)M_UINTPTR_MAX)
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum { M_false = 0, M_true = 1, M_FALSE = 0, M_TRUE = 1 } M_bool;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define M_TIMEOUT_INF M_UINT64_MAX

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_TYPES_H__ */
