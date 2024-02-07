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

#ifndef __M_DECIMAL_H__
#define __M_DECIMAL_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_decimal Decimal
 *  \ingroup mstdlib_base
 *
 * Floating point number type. Used instead of double or float to elimiate rounding errors.
 *
 * Example:
 *
 * \code{.c}
 *     const char  *s1 = "1.01";
 *     const char  *s2 = "0.001";
 *     M_decimal_t  d1;
 *     M_decimal_t  d2;
 *     char         out[16];
 *
 *     M_mem_set(out, 0, sizeof(out));
 *
 *     M_decimal_from_str(s1, M_str_len(s1), &d1, NULL);
 *     M_decimal_from_str(s2, M_str_len(s2), &d2, NULL);
 *     M_decimal_add(&d1, &d1, &d2);
 *     M_decimal_reduce(&d1);
 *
 *     if (M_decimal_to_str(&d1, out, sizeof(out)) != M_DECIMAL_SUCCESS) {
 *         M_printf("failure\n");
 *     } else {
 *         M_printf("out='%s'\n", out);
 *     }
 * \endcode
 *
 * Example output:
 *
 * \code
 *     out='1.011'
 * \endcode
 *
 * @{
 */


/*! Structure defining storage for decimal numbers.
 *
 * This structure should never be touched directly. It is only made public to reduce the overhead of using this
 * datatype (no malloc needed). */
typedef struct {
    M_int64 num;     /*!< Number represented. */
    M_uint8 num_dec; /*!< How many implied decimal places. */
} M_decimal_t;


/*! Result/Error codes for M_decimal functions. */
enum M_DECIMAL_RETVAL {
    M_DECIMAL_SUCCESS    = 0, /*!< Operation successful. */
    M_DECIMAL_OVERFLOW   = 1, /*!< An overflow occurred in the operation. */
    M_DECIMAL_TRUNCATION = 2, /*!< The result was truncated/rounded in order
                                   to approximate the best result. This is
                                   true on most divide operations. */
    M_DECIMAL_INVALID    = 3  /*!< Invalid data. */
};

/*! Rounding formula */
typedef enum {
    M_DECIMAL_ROUND_NONE        = 0, /*!< Truncate */
    M_DECIMAL_ROUND_TRADITIONAL = 1, /*!< Traditional, aka Round Half away from Zero. */
    M_DECIMAL_ROUND_BANKERS     = 2, /*!< Bankers, aka Round Half to Even */
} M_decimal_round_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create new zero'd out decimal number.
 *
 * \param[out] dec New decimal output.
 */
M_API void M_decimal_create(M_decimal_t *dec);


/*! Convert to a decimal representation from an integer
 *
 * \param[out] dec         New decimal output.
 * \param[in]  integer     Integer to convert to decimal.
 * \param[in]  implied_dec Number of implied decimals in integer input.
 */
M_API void M_decimal_from_int(M_decimal_t *dec, M_int64 integer, M_uint8 implied_dec);


/*! Convert from a decimal representation to an integer with implied decimal places.
 *  If the conversion causes truncation, the number will be rounded.
 *  For example a decimal of 123.456, with implied decimals 2, will return
 *  12346.
 *
 * \param[in] dec         Decimal type
 * \param[in] implied_dec Number of implied decimal positions.
 *
 * \return Rounded integer representation of number.
 */
M_API M_int64 M_decimal_to_int(const M_decimal_t *dec, M_uint8 implied_dec);


/*! Convert to a decimal representation from a string.
 *
 * \param[in]  string Buffer with decimal representation.
 * \param[in]  len    Length of bytes to evaluate from string.
 * \param[out] val    New decimal output.
 * \param[out] endptr Pointer to end of evaluated decimal.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_from_str(const char *string, size_t len, M_decimal_t *val, const char **endptr);


/*! Convert from a decimal representation to a string.
 *
 * \param[in]  dec     Decimal type.
 * \param[out] buf     Buffer to output string representation.
 * \param[in]  buf_len Length of output buffer.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_to_str(const M_decimal_t *dec, char *buf, size_t buf_len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Compare 2 decimals.
 *
 * \param[in] dec1 Decimal 1.
 * \param[in] dec2 Decimal 2.
 *
 * \return -1 if dec1 < dec2, 0 if dec1 == dec2, 1 if dec1 > dec2.
 */
M_API M_int8 M_decimal_cmp(const M_decimal_t *dec1, const M_decimal_t *dec2);


/*! Transform decimal number representation to have the specified number
 * of decimal places (rounding if needed).
 *
 * \param[in,out] dec     Decimal type.
 * \param[in]     num_dec Number of decimal places number should be transformed to.
 * \param[in]     round   Round method to use when rounding.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_transform(M_decimal_t *dec, M_uint8 num_dec, M_decimal_round_t round);


/*! Reduce the decimal representation to the smallest number of decimal places
 * possible without reducing precision (remove trailing zeros).
 *
 * \param[in,out] dec Decimal type.
 */
M_API void M_decimal_reduce(M_decimal_t *dec);


/*! Number of decimal places present in the M_decimal_t representation.
 *
 * \param[in] dec Decimal type.
 *
 * \return Number of decimals currently represented by type.
 */
M_API M_uint8 M_decimal_num_decimals(const M_decimal_t *dec);


/*! Copy the decimal object from the source into the destination
 *
 * \param[out] dest New decimal duplicated from src.
 * \param[in]  src  Decimal type to copy.
 */
M_API void M_decimal_duplicate(M_decimal_t *dest, const M_decimal_t *src);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Multiply the two decimals together putting the result in dest.
 *
 * The destination and one of the sources may be the same. The number of resulting decimal places will be the same as
 * the largest input.
 *
 * \param[out] dest New decimal with result.
 * \param[in]  dec1 First decimal to multiply.
 * \param[in]  dec2 Second decimal to multiply.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_multiply(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2);


/*! Divide the two decimals, putting the result in dest.
 *
 * The destination and one of the sources may be the same. The maximum number of decimal places able to be represented
 * will be.
 *
 * \param[out] dest  New decimal with result.
 * \param[in]  dec1  First decimal (numerator).
 * \param[in]  dec2  Second decimal (denominator).
 * \param[in]  round The result may not be able to be represented fully, select the rounding method if truncated.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_divide(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2, M_decimal_round_t round);


/*! Subtract two decimals, putting the result in dest.
 *
 * The destination and one of the sources may be the same. The number of resulting decimal places will be the same as
 * the largest input.
 *
 * \param[out] dest New decimal with result.
 * \param[in]  dec1 First decimal.
 * \param[in]  dec2 Second decimal.
 *
 * \return One of the enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_decimal_subtract(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2);


/*! Add two decimals, putting the result in dest.
 *
 * The destination and one of the sources may be the same.  The number of resulting decimal places will be the same as
 * the largest input.
 *
 * \param[out] dest New decimal with result.
 * \param[in]  dec1 First decimal.
 * \param[in]  dec2 Second decimal.
 *
 * \return One of the enum M_DECIMAL_RETVAL values
 */
M_API enum M_DECIMAL_RETVAL M_decimal_add(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2);

/*! @} */

__END_DECLS

#endif /* __M_DECIMAL_H__ */
