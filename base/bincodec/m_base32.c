/* The MIT License (MIT)
 * 
 * Copyright (c) 2022 Monetra Technologies, LLC.
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
#include "bincodec/m_base32.h"

size_t M_base32_encode_size(size_t len, size_t wrap)
{
	size_t ret;

	if (len == 0)
		return 0;

	ret = ((8 * len) + 4) / 5;
	if (ret % 8 != 0)
		ret += 8 - (ret % 8);

	/* Calculate the \n's we'll need to add for wrapping. */
	if (wrap > 0) {
		if (ret % wrap) {
			ret += (ret/wrap);
		} else {
			/* If it ends on a even multiple, the last newline is stripped */
			ret += (ret/wrap) - 1;
		}
	}

	/* Assuming null termination. */
	return ret+1;
}

size_t M_base32_decode_size(size_t len)
{
	return ((len / 8) * 5);
}

static const char M_base32_charset[]     = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
/* Reverse supports both upper and lowercase letters */
static const int  M_base32_rev_charset[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1,  0, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

size_t M_base32_encode(char *out, size_t out_len, const M_uint8 *in, size_t in_len, size_t wrap)
{
	size_t i;
	size_t cnt     = 0; /* cnt includes newlines */
	size_t datalen = 0; /* datalen does not include newlines */

	if (out == NULL || out_len == 0 || in == NULL || in_len == 0 || out_len < M_base32_encode_size(in_len, wrap))
		return 0;

	for (i=0; i<in_len; i+=5) {
		M_uint64 n     = 0;
		size_t   bytes = in_len - i;
		size_t   j;

		if (bytes > 5)
			bytes = 5;

		for (j=0; j<bytes; j++) {
			n |= (M_uint64)(in[i+j]) << (32 - (j * 8));
		}

		for (j=0; j<8; j++) {
			if (wrap && datalen > 0 && (datalen % wrap) == 0) {
				out[cnt++] = '\n';
			}
			if (j * 5 > (bytes * 8)) {
				out[cnt++] = '=';
			} else {
				out[cnt++] = M_base32_charset[(n >> (35 - (j * 5))) & 0x1F];
			}
			datalen++;
		}
	}

	out[cnt] = 0;
	return cnt;
}

size_t M_base32_decode(M_uint8 *out, size_t out_len, const char *in, size_t in_len)
{
	size_t cnt = 0;

	if (out == NULL || out_len == 0 || in == NULL || in_len == 0 || out_len < M_base32_decode_size(in_len))
		return 0;

	while (*in) {
		M_uint64 n = 0;
		size_t   i;

		/* Read in 8 bytes */
		for (i=0; i<8; i++) {
			int v;

			/* Skip any whitespace */
			while (M_chr_isspace(*in))
				in++;

			/* Out of data */
			if (!*in)
				return 0;

			v = M_base32_rev_charset[(M_uint8)*(in++)];
			/* invalid character */
			if (v < 0)
				return 0;

			n |= ((M_uint64)v) << (35 - (i * 5));
		}

		/* Write out 5 bytes */
		for (i=0; i<5; i++) {
			out[cnt++] = (n >> (32 - (i*8))) & 0xFF;
		}
	}
	return cnt;
}

