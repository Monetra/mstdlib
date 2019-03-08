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

#ifndef __M_DEFS_INT_H__
#define __M_DEFS_INT_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

#ifdef HAVE_STDDEF_H
#  include <stddef.h>
#endif
#ifdef HAVE_STDALIGN_H
#  include <stdalign.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* There is not a portable way to determine alignment at compile or run time.
 * It can be cacluated using alignof(max_align_t)); with C11 (we support back
 * to C89).
 *
 * The typicall recomendation is to use __WORDSIZE from limits.h but there
 * are two issues.
 *
 * 1. limits.h is a C99 addition.
 * 2. word size on Solaris SPARC is 32 (4 bytes) but alignment is 64 (8 bytes).
 * 3. float can be 16 bytes on some platforms while pointers are 8.
 * 
 * 16 is a known safe alignment value that works across all CPUs we use/test.
 */
#if defined(HAVE_ALIGNOF) && defined(HAVE_MAX_ALIGN_T)
#  define M_SAFE_ALIGNMENT alignof(max_align_t)
#else
#  define M_SAFE_ALIGNMENT 16
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_DEFS_INT_H__ */
