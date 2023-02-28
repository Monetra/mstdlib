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

#ifndef __M_RAND_H__
#define __M_RAND_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_rand Pseudo Random Number Generator (PRNG)
 *  \ingroup mstdlib_base
 *
 * This is _NOT_ a cryptographically secure RNG. This should _NEVER_ be
 * used for cryptographic operations.
 *
 * @{
 */

#define M_RAND_MAX M_UINT64_MAX

struct M_rand;
typedef struct M_rand M_rand_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a random state for use with random number generation.
 *
 * \param[in] seed The seed state. Using the same seed will allow the sequence to be repeated.
 *                 If 0 the seed will be a combination of system time, local and heap memory
 *                 addresses. The data is meant to make choosing a random seed harder but still
 *                 is not cryptographically secure. This not being a cryptographically secure
 *                 random number generator we are using random data that is not cryptographically
 *                 secure for speed.
 * 
 * \return Random state.
 */
M_API M_rand_t *M_rand_create(M_uint64 seed);


/*! Destroy a random state.
 *
 * \param[in,out] state The state.
 */
M_API void M_rand_destroy(M_rand_t *state);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Generate a random number.
 *
 * Generates a random number from 0 to M_RAND_MAX.
 *
 * \param[in,out] state The state. Optional, can be NULL, but will incur the overhead of
 *                      M_rand_create(0); M_rand_destroy(); per iteration if not provided.
 *                      It is possible, in a tight loop on a very fast system for example,
 *                      the same number could be generated multiple times if the state is NULL.
 *
 * \return A random number.
 */
M_API M_uint64 M_rand(M_rand_t *state);


/*! Generate a random number within a given range.
 *
 * Range is [min, max). Meaning from min to max-1. 
 *
 * \param[in,out] state The state. Optional, can be NULL, but will incur the overhead of
 *                      M_rand_create(0); M_rand_destroy(); per iteration if not provided.
 *                      It is possible, in a tight loop on a very fast system for example,
 *                      the same number could be generated multiple times if the state is NULL.
 * \param[in]     min   The min.
 * \param[in]     max   The max.
 */
M_API M_uint64 M_rand_range(M_rand_t *state, M_uint64 min, M_uint64 max);


/*! Generate a random number with a given maximum.
 *
 * Range is [0, max). Meaning from 0 to max-1. 
 *
 * \param[in,out] state The state. Optional, can be NULL, but will incur the overhead of
 *                      M_rand_create(0); M_rand_destroy(); per iteration if not provided.
 *                      It is possible, in a tight loop on a very fast system for example,
 *                      the same number could be generated multiple times if the state is NULL.
 * \param[in]     max   The max.
 */
M_API M_uint64 M_rand_max(M_rand_t *state, M_uint64 max);

/*! Generate a random string based on the provided character set. 
 *
 * \param[in,out] state   The state. Optional, can be NULL, but will incur the overhead of
 *                        M_rand_create(0); M_rand_destroy(); per iteration if not provided.
 *                        It is possible, in a tight loop on a very fast system for example,
 *                        the same string could be generated multiple times if the state is NULL.
 * \param[in]     charset Character set to use to generate the random string.
 * \param[out]    out     Buffer to use to hold the resulting random string.  Must be
 *                        len+1 bytes in length or greater to handle NULL terminator.
 * \param[in]     len     Number of characters to generate.  Actual number of bytes written
 *                        will be 1 larger for the NULL terminator.
 * \return M_FALSE on usage error. M_TRUE on success.
 */
M_API M_bool M_rand_str(M_rand_t *state, const char *charset, char *out, size_t len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate the state of a random number generator.
 *
 * If state is NULL this is equivalent to M_rand_create(0).
 *
 * \param[in,out] state The state.
 *
 * \return Random state.
 *
 * \see M_rand_jump
 */
M_API M_rand_t *M_rand_duplicate(M_rand_t *state);


/*! Advance the random number generator to generate non-overlapping sequences.
 *
 * \param[in,out] state The state.
 *
 * \see M_rand_duplicate
 */
M_API void M_rand_jump(M_rand_t *state);

/*! @} */

__END_DECLS

#endif /* __M_RAND_H__ */
