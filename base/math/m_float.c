/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * floating point functions from musl libc.
 *
 * Note: Assumes double's are in IEEE 754 format
 */

/* Copyright © 2005-2014 Rich Felker, et al.
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

double M_math_modf(double x, double *iptr)
{
	union {double f; M_uint64 i;} u = {x};
	M_uint64 mask;
	int e = (int)(u.i>>52 & 0x7ff) - 0x3ff;

	/* no fractional part */
	if (e >= 52) {
		*iptr = x;
		if (e == 0x400 && u.i<<12 != 0) /* nan */
			return x;
		u.i &= 1ULL<<63;
		return u.f;
	}

	/* no integral part*/
	if (e < 0) {
		u.i &= 1ULL<<63;
		*iptr = u.f;
		return x;
	}

	mask = ((M_uint64)(-1LL)) /* Was -1ULL, caused warning */ >> 12 >> e;
	if ((u.i & mask) == 0) {
		*iptr = x;
		u.i &= 1ULL<<63;
		return u.f;
	}
	u.i &= ~mask;
	*iptr = u.f;
	return x - u.f;
}


double M_math_round(double x)
{
	/* using FTL_EVAL_METHOD = 0 */
	double toint = 1/2.22044604925031308085e-16;
	union {double f; M_uint64 i;} u = {x};
	int e = u.i >> 52 & 0x7ff;
	double y;

	if (e >= 0x3ff+52)
		return x;
	if (u.i >> 63)
		x = -x;
	if (e < 0x3ff-1) {
		/* raise inexact if x!=0 */
		return 0*u.f;
	}
	y = x + toint - toint - x;
	if (y > 0.5)
		y = y + x - 1;
	else if (y <= -0.5)
		y = y + x + 1;
	else
		y = y + x;
	if (u.i >> 63)
		y = -y;
	return y;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
