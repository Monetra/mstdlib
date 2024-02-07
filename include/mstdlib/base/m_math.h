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

#ifndef __M_MATH_H__
#define __M_MATH_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_math Math
 *  \ingroup mstdlib_base
 *
 * Mathmatic calculations and conversions.
 *
 * @{
 */

#define M_MIN(a,b)     ((a)<(b)?(a):(b))
#define M_MAX(a,b)     ((a)>(b)?(a):(b))
#define M_ABS(a)       ((a)<0?((a)*-1):(a))
#define M_CLAMP(x,l,h) M_MIN(h,M_MAX(l,x))


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Calculate the exponent of an integer x^y (num^exp).
 *
 * \param[in] num      Base number.
 * \param[in] exponent Exponent.
 *
 * \return num ^ exp
 */
M_API M_uint64 M_uint64_exp(M_uint64 num, int exponent);


/*! Round an integer with implied decimals.
 *
 * \param[in] num             Number to round.
 * \param[in] currentDecimals Current number of implied decimal places.
 * \param[in] wantedDecimals  Desired number of decimal places in output. Must be <= currentDecimals.
 *
 * \return num rounded to wantedDecimals (output will have wantedDecimals implied decimals).
 */
M_API M_uint64 M_uint64_prec_round(M_uint64 num, int currentDecimals, int wantedDecimals);


/*! Round an integer value up to a given multiple.
 *
 * \param[in] n   integer value from which to determine the next multiple.
 * \param[in] mul integer multiple to use.
 *
 * \return the next value greater than or equal to n that is evenly divisible by mul.
 */
M_API M_uint64 M_uint64_round_up_to_nearest_multiple(M_uint64 n, M_uint64 mul);


/*! Determine if 32bit integer is a power of two.
 *
 *  \param[in] n integer to check
 *
 *  \return M_TRUE if power of 2, M_FALSE otherwise
 */
M_API M_bool M_uint32_is_power_of_two(M_uint32 n);


/*! Determine if 64bit integer is a power of two.
 *
 *  \param[in] n integer to check
 *
 *  \return M_TRUE if power of 2, M_FALSE otherwise
 */
M_API M_bool M_uint64_is_power_of_two(M_uint64 n);


/*! Determine if size_t is a power of two.
 *
 *  \param[in] n integer to check
 *
 *  \return M_TRUE if power of 2, M_FALSE otherwise
 */
M_API M_bool M_size_t_is_power_of_two(size_t n);


/*! Round a 32bit integer value up to the next power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API M_uint32 M_uint32_round_up_to_power_of_two(M_uint32 n);


/*! Round a 64bit integer value up to the next power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API M_uint64 M_uint64_round_up_to_power_of_two(M_uint64 n);


/*! Round a size_t value up to the next power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API size_t M_size_t_round_up_to_power_of_two(size_t n);


/*! Round a 32bit integer value down to the last power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API M_uint32 M_uint32_round_down_to_power_of_two(M_uint32 n);


/*! Round a 64bit integer value down to the last power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API M_uint64 M_uint64_round_down_to_power_of_two(M_uint64 n);


/*! Round a size_t value down to the last power of two.
 *
 * \param[in] n integer value from which to determine the next power of two.
 *
 * \return the next value greater than or equal to n that is a power of two.
 */
M_API size_t M_size_t_round_down_to_power_of_two(size_t n);


/*! Get the log2 of a 32bit integer.  n is rounded down if not power of 2, can
 *  use M_uint32_round_up_to_power_of_two() to round n up first.
 *  \param[in] n integer value to get the log2 for
 *  \return log2 of n
 */
M_API M_uint8 M_uint32_log2(M_uint32 n);


/*! Get the log2 of a 64bit integer.  n is rounded down if not power of 2, can
 *  use M_uint64_round_up_to_power_of_two() to round n up first.
 *  \param[in] n integer value to get the log2 for
 *  \return log2 of n
 */
M_API M_uint8 M_uint64_log2(M_uint64 n);


/*! Increase the number of bits while keeping the sign.
 *
 * \param[in] x        Value to sign extend
 * \param[in] num_bits The number of bits to extend from
 * \return sign extended value
 *
 */
M_API M_int64 M_sign_extend(M_uint64 x, size_t num_bits);


/*! Count number of decimal digits in an integer.
 *
 * \param[in] num Number to count digits.
 *
 * \return number of digits in integer.
 */
M_API int M_uint64_count_digits(M_uint64 num);


/*! Count number of set bits in a single byte.
 *
 * \param[in] x value to count bits in
 * \return      number of set (1) bits in \a x
 */
M_API M_uint8 M_uint8_popcount(M_uint8 x);

/*! Count number of set bits in a 64bit integer.
 *
 * \param[in] num value to count bits in
 * \return        number of set (1) bits in \a num
 */
M_API M_uint8 M_uint64_popcount(M_uint64 num);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Floating point modulus.
 *
 * Splits floating point number into integer and fractional parts.
 *
 * \param[in]  x    Number to split.
 * \param[out] iptr Integer part of number.
 *
 * \return Fractional part of number.
 */
M_API double M_math_modf(double x, double *iptr);


/*! Floating point rounding.
 *
 * \param[in] x Number to round.
 *
 * \return Number rounded.
 */
M_API double M_math_round(double x);

/*! @} */

__END_DECLS

#endif /* __M_MATH_H__ */
