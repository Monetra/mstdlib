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

#include "m_config.h"

#include <mstdlib/mstdlib.h>

#ifndef M_NOINLINE
#  if defined(__GNUC__)
#    define M_NOINLINE  __attribute__((noinline))
#  elif defined(_MSC_VER)
#    define M_NOINLINE  __declspec(noinline)
#  else
#    define M_NOINLINE
#  endif
#endif

void M_decimal_create(M_decimal_t *dec)
{
	if (dec == NULL)
		return;
	M_mem_set(dec, 0, sizeof(*dec));
}


void M_decimal_from_int(M_decimal_t *dec, M_int64 integer, M_uint8 implied_dec)
{
	if (dec == NULL)
		return;
	M_decimal_create(dec);
	dec->num     = integer;
	dec->num_dec = implied_dec;
	M_decimal_reduce(dec);
}


M_int64 M_decimal_to_int(const M_decimal_t *dec, M_uint8 implied_dec)
{
	M_decimal_t           dupl;
	enum M_DECIMAL_RETVAL rv;
	if (dec == NULL)
		return 0;

	M_decimal_duplicate(&dupl, dec);
	rv = M_decimal_transform(&dupl, implied_dec);
	if (rv != M_DECIMAL_SUCCESS && rv != M_DECIMAL_TRUNCATION)
		return 0;

	return dupl.num;
}


/* Don't inline this function - if you do, triggers a strict-overflow warning due to the compiler assuming
 * that in1 is always greater than 0 when called below in some tests - in these cases, the "if (in1 > 0)" check
 * was being optimized out by the compiler.
 *
 * If this function isn't inlined, the compiler has no way of guessing whether or not in1 is > 0 or not, so
 * it doesn't remove the if statement, and we don't see any warnings.
 */
static M_NOINLINE enum M_DECIMAL_RETVAL M_decimal_mult_int64(M_int64 *out, M_int64 in1, M_int64 in2)
{
	*out = in1 * in2;

	/* test for overflow -- From CERT */
	if (in1 > 0) {  /* in1 is positive */
		if (in2 > 0) {  /* in1 and in2 are positive */
			if (in1 > (M_INT64_MAX / in2)) {
				/* Handle error */
				return M_DECIMAL_OVERFLOW;
			}
		} else { /* in1 positive, in2 nonpositive */
			if (in2 < (M_INT64_MIN / in1)) {
				/* Handle error */
				return M_DECIMAL_OVERFLOW;
			}
		} /* in1 positive, in2 nonpositive */
	} else { /* in1 is nonpositive */
		if (in2 > 0) { /* in1 is nonpositive, in2 is positive */
			if (in1 < (M_INT64_MIN / in2)) {
				/* Handle error */
				return M_DECIMAL_OVERFLOW;
			}
		} else { /* in1 and in2 are nonpositive */
			if ( (in1 != 0) && (in2 < (M_INT64_MAX / in1))) {
				/* Handle error */
				return M_DECIMAL_OVERFLOW;
			}
		} /* End if in1 and in2 are nonpositive */
	} /* End if in1 is nonpositive */


	return M_DECIMAL_SUCCESS;
}


static enum M_DECIMAL_RETVAL M_decimal_div_int64(M_int64 *out, M_int64 num, M_int64 denom, M_bool round)
{
	enum M_DECIMAL_RETVAL rv;
	M_int64               moddenom;

	if (denom == 0)
		return M_DECIMAL_INVALID;

	/* Integer overflow on two's complement signed integer division when the divident is equal
	 * to the minimum negative value for the signed integer type and the divisor is -1 */
	if (num == M_INT64_MIN && denom == -1) {
		*out = M_INT64_MAX;
		return M_DECIMAL_OVERFLOW;
	}

	*out = num / denom;

	/* Round */
	if (round) {
		moddenom = denom/10;
		if (moddenom && (num / moddenom) % 10 >= 5)
			(*out)++;
	}

	rv = M_DECIMAL_SUCCESS;

	/* This is probably expected behavior, but it is prudent to let them
	 * know the precision was indeed truncated */
	if (num % denom)
		rv = M_DECIMAL_TRUNCATION;

	return rv;
}


static enum M_DECIMAL_RETVAL M_decimal_add_int64(M_int64 *out, M_int64 num1, M_int64 num2)
{
	/* Check for Overflow from CERT */
	if ((num2 > 0 && num1 > M_INT64_MAX - num2) ||
	    (num2 < 0 && num1 < M_INT64_MIN - num2)) {
		return M_DECIMAL_OVERFLOW;
	}
	
	*out = num1 + num2;
	return M_DECIMAL_SUCCESS;
}


static enum M_DECIMAL_RETVAL M_decimal_exp_int64(M_int64 *out, M_int64 num, M_uint8 exp)
{
	M_uint8 i;

	*out = 1;
	for (i=0; i<exp; i++) {
		if (M_decimal_mult_int64(out, *out, num) != M_DECIMAL_SUCCESS)
			return M_DECIMAL_OVERFLOW;
	}

	return M_DECIMAL_SUCCESS;
}


enum M_DECIMAL_RETVAL M_decimal_transform(M_decimal_t *dec, M_uint8 num_dec)
{
	enum M_DECIMAL_RETVAL rv    = M_DECIMAL_SUCCESS;
	M_int64               opnum;

	if (dec == NULL)
		return M_DECIMAL_INVALID;

	/* No-Op */
	if (num_dec == dec->num_dec)
		return M_DECIMAL_SUCCESS;

	if (num_dec > dec->num_dec) {
		if (dec->num != 0) {
			rv = M_decimal_exp_int64(&opnum, 10, (M_uint8)(num_dec - dec->num_dec));
			if (rv == M_DECIMAL_OVERFLOW)
				return rv;

			rv = M_decimal_mult_int64(&dec->num, dec->num, opnum);
		}
		dec->num_dec = num_dec;
		return rv;
	}

	/* num_dec < dec->num_dec */
	if (dec->num != 0) {
		rv = M_decimal_exp_int64(&opnum, 10, (M_uint8)(dec->num_dec - num_dec));
		if (rv == M_DECIMAL_OVERFLOW)
			return rv; /* Not possible, is it? */

		rv = M_decimal_div_int64(&dec->num, dec->num, opnum, M_TRUE);
	}
	dec->num_dec = num_dec;
	return rv;
}


void M_decimal_reduce(M_decimal_t *dec)
{
	if (dec == NULL)
		return;

	for ( ; dec->num_dec > 0 ; dec->num_dec--) {
		if (dec->num % 10)
			break;
		dec->num /= 10;
	}
}


M_uint8 M_decimal_num_decimals(const M_decimal_t *dec)
{
	if (dec == NULL)
		return 0;
	return dec->num_dec;
}


void M_decimal_duplicate(M_decimal_t *dest, const M_decimal_t *src)
{
	if (dest == NULL)
		return;

	M_decimal_create(dest);

	if (src == NULL)
		return;

	dest->num     = src->num;
	dest->num_dec = src->num_dec;
}


/*! Duplicate dec1 and dec2 into tdec1 and tdec2, respectively.  Reduce to lowest
 *  possible number of decimal places for each, then convert both to have the
 *  same number of decimal places ... may require truncation of decimal places
 *  if both can't be represented in the max */
static enum M_DECIMAL_RETVAL M_decimal_prepmath(M_decimal_t *tdec1, M_decimal_t *tdec2, const M_decimal_t *dec1, const M_decimal_t *dec2, M_bool reduce_dec)
{
	M_uint8 wanted_dec;
	M_uint8 num_dec;

	if (tdec1 == NULL || tdec2 == NULL || dec1 == NULL || dec2 == NULL)
		return M_DECIMAL_INVALID;

	M_decimal_duplicate(tdec1, dec1);
	M_decimal_duplicate(tdec2, dec2);

	if (reduce_dec) {
		M_decimal_reduce(tdec1);
		M_decimal_reduce(tdec2);
	}

	wanted_dec = M_MAX(tdec1->num_dec, tdec2->num_dec);
	num_dec    = wanted_dec;

	do {
		if (M_decimal_transform(tdec1, num_dec) != M_DECIMAL_OVERFLOW &&
		    M_decimal_transform(tdec2, num_dec) != M_DECIMAL_OVERFLOW)
			break;

		/* Shouldn't be possible */
		if (num_dec == 0)
			return M_DECIMAL_INVALID;

		num_dec--;

		M_decimal_duplicate(tdec1, dec1);
		M_decimal_duplicate(tdec2, dec2);
	} while(1);

	if (num_dec != wanted_dec)
		return M_DECIMAL_TRUNCATION;

	return M_DECIMAL_SUCCESS;
}


enum M_DECIMAL_RETVAL M_decimal_multiply(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_t           tdec1;
	M_decimal_t           tdec2;
	enum M_DECIMAL_RETVAL rv;
	enum M_DECIMAL_RETVAL preprv;
	M_int64               num;
	size_t                i;

	if (dest == NULL || (preprv = M_decimal_prepmath(&tdec1, &tdec2, dec1, dec2, M_TRUE)) == M_DECIMAL_INVALID)
		return M_DECIMAL_INVALID;

	M_decimal_create(dest);

	/* We multiply in a loop.  We're ok with losing precision, to prevent
	 * overflows */
	for (i=0; ; i++) {
		rv = M_decimal_mult_int64(&num, tdec1.num, tdec2.num);
		
		if (rv != M_DECIMAL_OVERFLOW)
			break;

		if (tdec1.num_dec == 0)
			break;

		/* reduce to avoid overflow */
		M_decimal_transform(&tdec1, (M_uint8)(tdec1.num_dec - 1));
		M_decimal_transform(&tdec2, (M_uint8)(tdec2.num_dec - 1));
	}

	if (rv != M_DECIMAL_SUCCESS)
		return rv;

	if (i != 0 || preprv == M_DECIMAL_TRUNCATION)
		rv = M_DECIMAL_TRUNCATION;

	dest->num     = num;
	dest->num_dec = (M_uint8)(tdec1.num_dec * 2); /* multiplication doubles precision */
	/* Reduce to minimum number of decimal places */
	M_decimal_reduce(dest);

	return rv;
}


enum M_DECIMAL_RETVAL M_decimal_divide(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_t           tdec1;
	M_decimal_t           tdec2;
	enum M_DECIMAL_RETVAL rv = M_DECIMAL_SUCCESS;
	enum M_DECIMAL_RETVAL preprv;
	M_int64               num;          /*!< Whole integer of integer calculation */
	M_int64               rem;          /*!< Remainder of integer calculation     */
	M_int64               afterdec = 0; /*!< Whole integer of integer calculation of numbers after decimal */
	M_int64               remexp   = 0; /*!< Remainder * 10^wanteddec */
	M_uint8               wanted_dec;   /*!< Number of desired decimal places in output */

	if (dest == NULL || (preprv = M_decimal_prepmath(&tdec1, &tdec2, dec1, dec2, M_FALSE)) == M_DECIMAL_INVALID)
		return M_DECIMAL_INVALID;

	M_decimal_create(dest);

	if (tdec2.num == 0)
		return M_DECIMAL_INVALID;

	if (tdec1.num == M_INT64_MIN && tdec2.num == -1)
		return M_DECIMAL_OVERFLOW;

	/* A number divided by a number, no matter how many implied decimal places
	 * results in a number with no implied decimal places.  To compensate for
	 * this, you must mod those numbers muliply to 10^num_dec then re-divide
	 * by the original divisor to get the decimal places you need */

	num        = tdec1.num / tdec2.num;
	rem        = tdec1.num % tdec2.num;

	/* Loop until we can get the most decimal places that will fit in our
	 * number which starts at one more than the input number of decimal
	 * places */
	wanted_dec = (M_uint8)(tdec1.num_dec + 1);
	while (wanted_dec) {
		M_int64 exp;

		if ((rv = M_decimal_exp_int64(&exp, 10, wanted_dec)) != M_DECIMAL_SUCCESS)
			return rv;

		rv = M_decimal_mult_int64(&remexp, rem, exp);
		if (rv == M_DECIMAL_SUCCESS)
			break;

		wanted_dec--;
	}

	/* Check for unrecoverable error, could be we can't represent any decimal
	 * places at all */
	if (rv != M_DECIMAL_SUCCESS)
		return rv;

	/* Calculate the number after the decimal */
	rv = M_decimal_div_int64(&afterdec, remexp, tdec2.num, M_TRUE);
	if (rv != M_DECIMAL_SUCCESS && rv != M_DECIMAL_TRUNCATION)
		return rv;

	/* Loop losing precision until we can represent the number appropriately */
	while (wanted_dec) {
		M_int64 exp;

		if ((rv = M_decimal_exp_int64(&exp, 10, wanted_dec)) != M_DECIMAL_SUCCESS)
			return rv;

		rv = M_decimal_mult_int64(&dest->num, num, exp);
		if (rv == M_DECIMAL_SUCCESS)
			break;

		/* Otherwise overflow occurred and we need to lose precision */
		M_decimal_div_int64(&afterdec, afterdec, 10, M_TRUE);
		wanted_dec--;
	}

	if (wanted_dec) {
		dest->num += afterdec;
	}

	dest->num_dec = wanted_dec;

	if (wanted_dec != tdec1.num_dec + 1 || preprv == M_DECIMAL_TRUNCATION) {
		rv = M_DECIMAL_TRUNCATION;
	} else {
		rv = M_DECIMAL_SUCCESS;
	}

	/* Reduce to minimum number of decimal places */
	M_decimal_reduce(dest);

	return rv;
}


enum M_DECIMAL_RETVAL M_decimal_subtract(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_t           tdec1;
	M_decimal_t           tdec2;
	enum M_DECIMAL_RETVAL preprv;

	if (dest == NULL || (preprv = M_decimal_prepmath(&tdec1, &tdec2, dec1, dec2, M_TRUE)) == M_DECIMAL_INVALID)
		return M_DECIMAL_INVALID;

	M_decimal_create(dest);

	dest->num     = tdec1.num - tdec2.num;
	dest->num_dec = tdec1.num_dec;

	/* Check for Overflow from CERT */
	if ((tdec2.num > 0 && tdec1.num < M_INT64_MIN + tdec2.num) ||
	    (tdec2.num < 0 && tdec1.num > M_INT64_MAX + tdec2.num)) {
		return M_DECIMAL_OVERFLOW;
	}

	/* Reduce to minimum number of decimal places */
	M_decimal_reduce(dest);

	return preprv;
}


enum M_DECIMAL_RETVAL M_decimal_add(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_t           tdec1;
	M_decimal_t           tdec2;
	enum M_DECIMAL_RETVAL preprv;

	if (dest == NULL || (preprv = M_decimal_prepmath(&tdec1, &tdec2, dec1, dec2, M_TRUE)) == M_DECIMAL_INVALID)
		return M_DECIMAL_INVALID;

	M_decimal_create(dest);

	if (M_decimal_add_int64(&dest->num, tdec1.num, tdec2.num) == M_DECIMAL_OVERFLOW)
		return M_DECIMAL_OVERFLOW;

	dest->num_dec = tdec1.num_dec;

	/* Reduce to minimum number of decimal places */
	M_decimal_reduce(dest);

	return preprv;
}


M_int8 M_decimal_cmp(const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_t           tdec1;
	M_decimal_t           tdec2;
	M_uint8               wanted_dec;
	M_uint8               num_dec;

	/* Invalid scenarios, but do something somewhat logical */
	if (dec1 && !dec2)
		return 1;
	if (dec2 && !dec1)
		return -1;
	if (!dec1 && !dec2)
		return 0;

	M_decimal_duplicate(&tdec1, dec1);
	M_decimal_reduce(&tdec1);

	M_decimal_duplicate(&tdec2, dec2);
	M_decimal_reduce(&tdec2);

	wanted_dec = M_MAX(tdec1.num_dec, tdec2.num_dec);

	/* Loop until both numbers can be represented by the same number of
	 * decimal places */
	num_dec    = wanted_dec;
	while (num_dec) {
		M_decimal_duplicate(&tdec1, dec1);
		M_decimal_duplicate(&tdec2, dec2);

		if (M_decimal_transform(&tdec1, num_dec) == M_DECIMAL_SUCCESS &&
		    M_decimal_transform(&tdec2, num_dec) == M_DECIMAL_SUCCESS) {
			break;
		}
		num_dec--;
	}

	if (tdec1.num > tdec2.num)
		return 1;
	if (tdec1.num < tdec2.num)
		return -1;
	return 0;
}


enum M_DECIMAL_RETVAL M_decimal_to_str(const M_decimal_t *dec, char *buf, size_t buf_len)
{
	char   *dec_pos;
	char    fmt[10];
	size_t  str_len = 0;

	if (dec == NULL)
		return M_DECIMAL_INVALID;

	/* Output long value to data, make sure its at least decimals+1 bytes */
	M_snprintf(fmt, sizeof(fmt), "%%0%dlld", dec->num_dec+1);

	str_len = M_snprintf(buf, buf_len, fmt, dec->num);

	if (str_len + 1 >= buf_len) {
		M_mem_set(buf, 0, buf_len);
		return M_DECIMAL_INVALID;
	}

	/* Move and place decimal */
	if (dec->num_dec) {
		dec_pos = buf + (str_len - dec->num_dec);
		M_mem_move(dec_pos+1, dec_pos, (size_t)dec->num_dec+1); /* decimals + NULL terminator */
		*dec_pos = '.';
	}

	return M_DECIMAL_SUCCESS;
}


enum M_DECIMAL_RETVAL M_decimal_from_str(const char *string, size_t len, M_decimal_t *val, const char **endptr)
{
	M_str_int_retval_t     intrv;
	enum M_DECIMAL_RETVAL  rv          = M_DECIMAL_SUCCESS;
	M_int64                num         = 0;
	M_int64                afterdec    = 0;
	const char            *ptr         = NULL;
	size_t                 num_read    = 0;
	size_t                 len_left    = 0;
	const char            *end         = NULL;
	const char            *temp        = NULL;
	size_t                 num_digits;

	if (string == NULL || len == 0 || val == NULL)
		return M_DECIMAL_INVALID;

	if (endptr)
		*endptr = NULL;

	M_decimal_create(val);

	/* Read characters before the decimal */
	if (*string == '.') {
		num = 0;
		ptr = string;
	} else {
		intrv = M_str_to_int64_ex(string, len, 10, &num, &ptr);
		switch (intrv) {
			case M_STR_INT_OVERFLOW:
				return M_DECIMAL_OVERFLOW;
			case M_STR_INT_INVALID:
				return M_DECIMAL_INVALID;
			case M_STR_INT_SUCCESS:
				break;
		}
	}

	/* Read characters after the decimal */
	num_read = (size_t)(ptr - string);
	if (len - num_read > 0 && *ptr == '.') {
		ptr++;
		num_read++;

		len_left = len - num_read;

		/* Read in a loop.  If the read causes an overflow, read one less
		 * until it no longer overflows.  Save the original end position 
		 * though for further processing (exponents) */
		while (len_left) {
			intrv = M_str_to_int64_ex(ptr, len_left, 10, &afterdec, &temp);
			if (end == NULL)
				end = temp;

			if (intrv == M_STR_INT_INVALID) {
				return M_DECIMAL_INVALID;
			} else if (intrv == M_STR_INT_OVERFLOW) {
				rv = M_DECIMAL_TRUNCATION;
				len_left--;
			} else {
				break;
			}
		}

		/* If the number read is negative, invalid format! */
		if (afterdec < 0)
			return M_DECIMAL_INVALID;
	} else {
		end = ptr;
	}

	/* Convert into a single integer, keep reducing number of decimal places
	 * until it fits */
	if (afterdec) {
		num_digits = (size_t)(temp - ptr); /* Use temp here as that is the actual number used */
	} else {
		num_digits = 0;
	}

	/* Either the exponention of the 'before decimal' part could cause an
	 * overflow, or adding on the 'after decimal' parts could.  So the entire
	 * thing needs to be done in a loop */
	do {
		while (num_digits) {
			M_int64 exp;

			/* Not possible to overflow this calc. M_str_to_int64_ex was called previously on the decimal
 			 * portion. If we're here there was no overflow so we know that we cannot have more than 19 digits. */
			M_decimal_exp_int64(&exp, 10, (M_uint8)num_digits);

			if (M_decimal_mult_int64(&val->num, num, exp) != M_DECIMAL_OVERFLOW) {
				break;
			}

			M_decimal_div_int64(&afterdec, afterdec, 10, M_FALSE);
			num_digits--;
			rv = M_DECIMAL_TRUNCATION;
		}

		if (num_digits) {
			if (M_decimal_add_int64(&val->num, val->num, afterdec) == M_DECIMAL_SUCCESS) {
				break;
			}
			/* Overflow, need to reduce num_digits */
			M_decimal_div_int64(&afterdec, afterdec, 10, M_FALSE);
			num_digits--;
			rv = M_DECIMAL_TRUNCATION;
		} else {
			val->num = num;
			break;
		}
	} while (1);

	val->num_dec = (M_uint8)num_digits;
	M_decimal_reduce(val);

	/* Handle the exponent/sci notation, e.g.   1.24e-2 == 0.0124 */
	if (end != NULL && string + len + 1 > end && M_chr_tolower(*end) == 'e') {
		M_decimal_t           multiplier;
		enum M_DECIMAL_RETVAL exprv;

		ptr      = end+1;
		num_read = (size_t)(ptr - string);
		len_left = len - num_read;
		intrv    = M_str_to_int64_ex(ptr, len_left, 10, &num, &end);
		switch (intrv) {
			case M_STR_INT_OVERFLOW:
				return M_DECIMAL_OVERFLOW;
			case M_STR_INT_INVALID:
				return M_DECIMAL_INVALID;
			case M_STR_INT_SUCCESS:
				break;
		}

		if (num >= 0) {
			M_int64 exp;
			if (num > 255 || M_decimal_exp_int64(&exp, 10, (M_uint8)num) == M_DECIMAL_OVERFLOW)
				return M_DECIMAL_OVERFLOW;

			M_decimal_from_int(&multiplier, exp, 0);
		} else {
			num *= -1;
			if (num > 255)
				return M_DECIMAL_OVERFLOW;
			M_decimal_from_int(&multiplier, 1, (M_uint8)num);
		}

		exprv = M_decimal_multiply(val, val, &multiplier);
		switch (exprv) {
			case M_DECIMAL_OVERFLOW:
				return M_DECIMAL_OVERFLOW;
			case M_DECIMAL_TRUNCATION:
				rv = M_DECIMAL_TRUNCATION;
				break;
			default:
				/* Don't change the rv */
				break;
		}

		M_decimal_reduce(val);
	}

	if (endptr)
		*endptr = end;

	return rv;
}

