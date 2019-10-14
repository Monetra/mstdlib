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

M_uint64 M_uint64_prec_round(M_uint64 num, int currentDecimals, int wantedDecimals)
{
	M_uint64 roundDivisor;
	M_uint64 ret;

	if (wantedDecimals >= currentDecimals)
		return num;

	/* coverity[return_constant : FALSE] */
	roundDivisor = M_uint64_exp(10, currentDecimals-wantedDecimals);

	ret = num / roundDivisor;

	if ((num / (roundDivisor/10)) % 10 >= 5) {
		ret++;
	}

	return ret;
}

/*
 * Calling this function with mul 0 is a programming error
 *   5,2 -> 6
 *   8,8 -> 8
 *   9,8 -> 16
 */
M_uint64 M_uint64_round_up_to_nearest_multiple(M_uint64 n, M_uint64 mul)
{
	if (mul == 0)
		return 0;

	if (n < mul)
		return mul;

	if (n % mul == 0)
		return n;

	return (n / mul + 1) * mul;
}
