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

#include "m_config.h"

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_BEGIN_IGNORE_DEPRECATIONS

static void eat_whitespace(const char **s, size_t *len)
{
	for (; M_chr_isspace(**s) && *len; (*s)++, (*len)--)
		;
}

static M_bool M_str_sign(const char **s, size_t *len)
{
	const char *p     = *s;
	M_bool      isneg = M_FALSE;

	if (*len == 0)
		return M_FALSE;
	switch (*p) {
		case '-':
		case '+':
			if (*p == '-')
				isneg = M_TRUE;
			p++;
			(*len)--;
			break;
	}
	*s= p;
	return isneg;
}


static M_str_int_retval_t M_str_to_uint64_int(const char *s, size_t len, unsigned char base, M_uint64 *val, const char **endptr)
{
	const char   *p;
	M_uint64      ret       = 0;
	M_uint64      v         = 0;
	M_bool        overflow  = M_FALSE;
	unsigned char digit;

	if (base < 2 || base > 36) {
		if (endptr)
			*endptr = s;
		if (val)
			*val    = 0;
		return M_STR_INT_INVALID;
	}

	for (p=s; len && *p != '\0'; p++) {

		/* Convert digit into proper base representation */
		if (*p >= '0' && *p <= '9') {
			digit = (unsigned char)(*p - '0');
		} else if (*p >= 'a' && *p <= 'z') {
			digit = (unsigned char)(*p - 'a' + 10);
		} else if (*p >= 'A' && *p <= 'Z') {
			digit = (unsigned char)(*p - 'A' + 10);
		} else {
			digit = 36;
		}

		/* Break if we hit an invalid character */
		if (digit >= base)
			break;

		v = ret * base + digit;
		if (v < ret) {
			overflow = M_true;
			/* Don't stop consuming valid characters
			 * break; */
		}
		ret = v;

		len--;
	}

	if (val)
		*val = ret;

	if (endptr)
		*endptr = p;

	if (overflow)
		return M_STR_INT_OVERFLOW;

	if (s == p)
		return M_STR_INT_INVALID;

	return M_STR_INT_SUCCESS;
}


static void M_str_intconv_prep(const char **s, size_t *len, unsigned char *base, M_bool *is_neg)
{
	eat_whitespace(s, len);
	*is_neg = M_str_sign(s, len);

	/* Auto-determine base */
	if (*base == 0) {
		if (*len >= 2 && M_str_caseeq_max(*s, "0x", *len)) {
			*base = 16;
		} else if (*len >= 1 && **s== '0') {
			*base = 8;
		} else {
			*base = 10;
		}
	}

	/* Hex is allowed a 0x prefix ... strip it */
	if (*base == 16 && *len >= 2 && M_str_caseeq_max(*s, "0x", *len)) {
		(*s) += 2;
		(*len)    -= 2;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

long M_atofi100(const char *s)
{
	M_int64 amount;
	long    ret;

	if (s == NULL)
		return 0;

	amount = M_atofi_prec(s, 2);
	ret    = (long)amount;

	/* Check for overflow */
	if (ret != amount)
		return -1;
	return ret;
}

M_int64 M_atofi_prec(const char *s, int impliedDecimals)
{
	int     offset   = 0;
	M_int64 value    = 0;
	int     len;
	int     i;
	int     isneg    = 0;
	int     afterdec = -1;

	if (s == NULL)
		return 0;

	/* Ignore leading whitespace */
	while (M_chr_isspace(*s))
		s++;

	if (*s == 0)
		return 0;

	if (s[0] == '-') {
		isneg  = 1;
		offset = 1;
	} else if (s[0] == '+') {
		offset = 1;
	}

	len = (int)M_str_len(s);
	for (i=offset; i<len; i++) {
		if (s[i] == '.') {
			/* Done */
			if (afterdec != -1)
				break;

			afterdec = 0;
			continue;
		}

		if (!M_chr_isdigit(s[i])) {
			/* Ignore commas in amounts, like  2,532.43  */
			if (s[i] == ',')
				continue;
			break;
		}

		if (afterdec != -1)
			afterdec++;

		/* This is the digit after the maximum number of decimal
		 * places we want to handle.  Use this for rounding purposes
		 * then break out! */
		if (afterdec > impliedDecimals) {
			if (s[i] >= '5')
				value++;
			break;
		}

		value *= 10;
		value += s[i] - '0';
	}

	/* If we didn't hit a decimal, let's account for that */
	if (afterdec < 0)
		afterdec = 0;

	/* Do implied decimal calculation */
	for (i=afterdec; i<impliedDecimals; i++)
		value *= 10;

	if (isneg)
		value *= -1;

	return value;
}


M_str_int_retval_t M_str_to_uint64_ex(const char *s, size_t len, unsigned char base, M_uint64 *val, const char **endptr)
{
	M_uint64           myval = 0;
	M_bool             is_neg;
	M_str_int_retval_t rv;

	if (s == NULL || len == 0)
		return M_STR_INT_INVALID;

	if (val == NULL)
		val = &myval;

	M_str_intconv_prep(&s, &len, &base, &is_neg);

	/* Do actual conversion */
	rv    = M_str_to_uint64_int(s, len, base, val, endptr);

	if (is_neg) {
		if (rv == M_STR_INT_OVERFLOW) {
			*val = M_UINT64_MAX;
		} else {
			*val = ~(*val) + 1ULL; /* 2s complement */
		}
	} else {
		if (rv == M_STR_INT_OVERFLOW) {
			*val = M_UINT64_MAX;
		}
	}

	return rv;
}


M_str_int_retval_t M_str_to_int64_ex(const char *s, size_t len, unsigned char base, M_int64 *val, const char **endptr)
{
	M_int64            myval = 0;
	M_bool             is_neg;
	M_str_int_retval_t rv;
	M_uint64           uval;

	if (s == NULL || len == 0)
		return M_STR_INT_INVALID;

	if (val == NULL)
		val = &myval;

	M_str_intconv_prep(&s, &len, &base, &is_neg);

	/* Do actual conversion */
	rv = M_str_to_uint64_int(s, len, base, &uval, endptr);

	if (is_neg && uval > ((M_uint64)M_INT64_MAX) + 1) {
		rv = M_STR_INT_OVERFLOW;
	} else if (!is_neg && uval > M_INT64_MAX) {
		rv = M_STR_INT_OVERFLOW;
	}

	if (rv == M_STR_INT_OVERFLOW) {
		*val = is_neg?M_INT64_MIN:M_INT64_MAX;
	} else {
		if (is_neg) {
			*val = (M_int64)(~uval + 1ULL);
		} else {
			*val = (M_int64)uval;
		}
	}

	return rv;
}


M_str_int_retval_t M_str_to_uint32_ex(const char *s, size_t len, unsigned char base, M_uint32 *val, const char **endptr)
{
	M_uint32           myval  = 0;
	M_uint64           u64val = 0;
	M_str_int_retval_t rv;

	if (s == NULL || len == 0)
		return M_STR_INT_INVALID;

	if (val == NULL)
		val = &myval;

	/* Use 64bit conversion. Yes, this is inefficient, but otherwise we'd be duplicating
	 * a bit of code */
	rv = M_str_to_uint64_ex(s, len, base, &u64val, endptr);
	if (u64val > M_UINT32_MAX) {
		*val = M_UINT32_MAX;
		rv   = M_STR_INT_OVERFLOW;
	} else {
		*val = (M_uint32)u64val;
	}
	return rv;
}


M_str_int_retval_t M_str_to_int32_ex(const char *s, size_t len, unsigned char base, M_int32 *val, const char **endptr)
{
	M_int32            myval  = 0;
	M_int64            i64val = 0;
	M_str_int_retval_t rv;

	if (s == NULL || len == 0)
		return M_STR_INT_INVALID;

	if (val == NULL)
		val = &myval;

	/* Use 64bit conversion.  Yes, this is inefficient, but otherwise we'd be duplicating
	 * a bit of code */
	rv = M_str_to_int64_ex(s, len, base, &i64val, endptr);
	if (i64val > M_INT32_MAX) {
		*val = M_INT32_MAX;
		rv   = M_STR_INT_OVERFLOW;
	} else if (i64val < M_INT32_MIN) {
		*val = M_INT32_MIN;
		rv   = M_STR_INT_OVERFLOW;
	} else {
		*val = (M_int32)i64val;
	}
	return rv;
}


M_int32 M_str_to_int32(const char *s)
{
	M_int32            val = 0;

	if (M_str_to_int32_ex(s, M_str_len(s), 10, &val, NULL) == M_STR_INT_INVALID)
		return 0;

	return val;
}


M_uint32 M_str_to_uint32(const char *s)
{
	M_uint32 val = 0;

	if (M_str_to_uint32_ex(s, M_str_len(s), 10, &val, NULL) == M_STR_INT_INVALID)
		return 0;
	return val;
}


M_uint64 M_str_to_uint64(const char *s)
{
	M_uint64 val = 0;

	if (M_str_to_uint64_ex(s, M_str_len(s), 10, &val, NULL) == M_STR_INT_INVALID)
		return 0;
	return val;
}


M_int64 M_str_to_int64(const char *s)
{
	M_int64 val = 0;

	if (M_str_to_int64_ex(s, M_str_len(s), 10, &val, NULL) == M_STR_INT_INVALID)
		return 0;
	return val;
}

char *M_str_dot_money_out(const char *amount)
{
	M_decimal_t dec_amount;
	char        temp[64];

	/* Verify and convert the amount so it always has 2 decimal digits. */
	M_decimal_create(&dec_amount);
	if (M_decimal_from_str(amount, M_str_len(amount), &dec_amount, NULL) != M_DECIMAL_SUCCESS)
		return NULL;
	if (M_decimal_transform(&dec_amount, 2, M_DECIMAL_ROUND_TRADITIONAL) != M_DECIMAL_SUCCESS)
		return NULL;
	if (M_decimal_to_str(&dec_amount, temp, sizeof(temp)) != M_DECIMAL_SUCCESS)
		return NULL;

	return M_strdup(temp);
}

M_END_IGNORE_DEPRECATIONS
