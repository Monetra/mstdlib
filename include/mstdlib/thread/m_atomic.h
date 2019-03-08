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

#ifndef __M_ATOMIC_H__
#define __M_ATOMIC_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_atomic Atomics
 *  \ingroup m_thread
 *
 * Operations which are guaranteed to be atomic.
 *
 * @{
 */

/*! Compare and swap 32bit integer.
 * 
 * \param[in,out] ptr      Pointer to var to operate on
 * \param[in]     expected Expected value of var before completing operation
 * \param[in]     newval   Value to set var to
 * \return M_TRUE on success, M_FALSE on failure
 */
M_API M_bool M_atomic_cas32(volatile M_uint32 *ptr, M_uint32 expected, M_uint32 newval);


/*! Compare and swap 64bit integer.
 * 
 * \param[in,out] ptr      Pointer to var to operate on
 * \param[in]     expected Expected value of var before completing operation
 * \param[in]     newval   Value to set var to
 * \return M_TRUE on success, M_FALSE on failure
 */
M_API M_bool M_atomic_cas64(volatile M_uint64 *ptr, M_uint64 expected, M_uint64 newval);


/*! Increment u32 by 1.
 *
 * \param[in] ptr Pointer to var to operate on.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint32 M_atomic_inc_u32(volatile M_uint32 *ptr);


/*! Increment u64 by 1.
 *
 * \param[in] ptr Pointer to var to operate on.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint64 M_atomic_inc_u64(volatile M_uint64 *ptr);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Decrement u32 by 1.
 *
 * \param[in] ptr Pointer to var to operate on.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint32 M_atomic_dec_u32(volatile M_uint32 *ptr);

/*! Decrement u64 by 1.
 *
 * \param[in] ptr Pointer to var to operate on.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint64 M_atomic_dec_u64(volatile M_uint64 *ptr);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add a given value with u32.
 *
 * \param[in] ptr Pointer to var to operate on.
 * \param[in] val Value to modify ptr with.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint32 M_atomic_add_u32(volatile M_uint32 *ptr, M_uint32 val);


/*! Add a given value with u64.
 *
 * \param[in] ptr Pointer to var to operate on.
 * \param[in] val Value to modify ptr with.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint64 M_atomic_add_u64(volatile M_uint64 *ptr, M_uint64 val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Subtract a given value with u32.
 *
 * \param[in] ptr Pointer to var to operate on.
 * \param[in] val Value to modify ptr with.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint32 M_atomic_sub_u32(volatile M_uint32 *ptr, M_uint32 val);


/*! Subtract a given value with u64.
 *
 * \param[in] ptr Pointer to var to operate on.
 * \param[in] val Value to modify ptr with.
 *
 * \return The value of pointer before operation.
 */
M_API M_uint64 M_atomic_sub_u64(volatile M_uint64 *ptr, M_uint64 val);

/*! @} */

__END_DECLS

#endif /* __M_ATOMIC_H__ */
