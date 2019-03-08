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

/* Uses public domain code snipets from http://graphics.stanford.edu/~seander/bithacks.html
 */

#include "m_config.h"
#include <mstdlib/mstdlib.h>

M_bool M_uint32_is_power_of_two(M_uint32 n)
{
	if (n == 0)
		return M_FALSE;

	if (n & (n - 1))
		return M_FALSE;

	return M_TRUE;
}


M_bool M_uint64_is_power_of_two(M_uint64 n)
{
	if (n == 0)
		return M_FALSE;

	if (n & (n - 1))
		return M_FALSE;

	return M_TRUE;
}


M_bool M_size_t_is_power_of_two(size_t n)
{
	if (n == 0)
		return M_FALSE;

	if (n & (n - 1))
		return M_FALSE;

	return M_TRUE;
}


M_uint32 M_uint32_round_down_to_power_of_two(M_uint32 n)
{
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n - (n >> 1);
}


M_uint64 M_uint64_round_down_to_power_of_two(M_uint64 n)
{
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	return n - (n >> 1);
}


size_t M_size_t_round_down_to_power_of_two(size_t n)
{
	/* This is a ternary because having it as an if statement results in conversion warnings... */
	return sizeof(size_t) >= sizeof(M_uint64)?M_uint64_round_down_to_power_of_two(n):M_uint32_round_down_to_power_of_two((M_uint32)n);
}


M_uint32 M_uint32_round_up_to_power_of_two(M_uint32 n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}


M_uint64 M_uint64_round_up_to_power_of_two(M_uint64 n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;
	return n;
}


size_t M_size_t_round_up_to_power_of_two(size_t n)
{
	/* This is a ternary because having it as an if statement results in conversion warnings... */
	return sizeof(size_t) >= sizeof(M_uint64)?M_uint64_round_up_to_power_of_two(n):M_uint32_round_up_to_power_of_two((M_uint32)n);
}


M_uint8 M_uint32_log2(M_uint32 n)
{
	static const M_uint8 tab32[32] = {
		 0,  1, 28,  2, 29, 14, 24,  3,
		30, 22, 20, 15, 25, 17,  4,  8, 
		31, 27, 13, 23, 21, 19, 16,  7,
		26, 12, 18,  6, 11,  5, 10,  9
	};

	if (!M_uint32_is_power_of_two(n))
		n = M_uint32_round_down_to_power_of_two(n);

	return tab32[(M_uint32)(n*0x077CB531) >> 27];
}


M_uint8 M_uint64_log2(M_uint64 n)
{
	static const M_uint8 tab64[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5
	};

	if (!M_uint64_is_power_of_two(n))
		n = M_uint64_round_down_to_power_of_two(n);

	return tab64[((M_uint64)(n*0x07EDD5E59A4E28C2)) >> 58];
}


M_int64 M_sign_extend(M_uint64 x, size_t num_bits)
{
	const M_uint64 mask = ((M_uint64)1) << (num_bits - 1);
	return (M_int64)((x ^ mask) - mask);
}


M_uint8 M_uint8_popcount(M_uint8 x)
{
	/* Implementation obtained from: http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetTable */
#define B2(n) n,     n+1,     n+1,     n+2
#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
	static const M_uint8 lookup[256] = {
		B6(0), B6(1), B6(1), B6(2)
	};
	return lookup[x];
}
