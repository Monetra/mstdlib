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

#ifndef __M_HASH_INT_H__
#define __M_HASH_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS
/*! \addtogroup m_hash_func Hashtable - Callback default implementations
 *  \ingroup m_hashtable
 * @{
 */

/*! Implementation will compute a hash using FNV1a from a string. */
M_API M_uint32 M_hash_func_hash_str(const void *key, M_uint32 seed);

/*! Implementation will compute a hash using FNV1a from a string in a case-insensitive manner. */
M_API M_uint32 M_hash_func_hash_str_casecmp(const void *key, M_uint32 seed);

/*! Implementation will compute a hash using FNV1a from a u64 (pointer). */
M_API M_uint32 M_hash_func_hash_u64(const void *key, M_uint32 seed);

/*! Implemntation will compute a hash using FNV1a from a pointer address */
M_API M_uint32 M_hash_func_hash_vp(const void *key, M_uint32 seed);

/*! This function duplicates a M_uint64 (pointer). */
M_API void *M_hash_func_u64dup(const void *arg);

/* Void wrapper for strdup to silence incompatible pointer warnings when using
 * strdup with hashtable callbacks. */
M_API void *M_hash_void_strdup(const void *arg);

/*! @} */

__END_DECLS

#endif /* __M_HASH_INT_H__ */
