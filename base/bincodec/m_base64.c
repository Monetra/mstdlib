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
#include "bincodec/m_base64.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const char enc_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/* Table is trimmed, 0-42 are removed (all -1), and last 4 removed (also -1)
 * range is char-43 to char-43-4*/
static const int  dec_table[] = { 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
                                 -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1,
                                 -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                                 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static __inline__ M_bool b64_isvalidchar(char c)
{
	if (c >= '0' && c <= '9')
		return M_TRUE;
	if (c >= 'A' && c <= 'Z')
		return M_TRUE;
	if (c >= 'a' && c <= 'z')
		return M_TRUE;
	if (c == '+' || c == '/' || c == '=')
		return M_TRUE;
	return M_FALSE;
}

static __inline__ void M_base64_encode_adder(char *out, size_t bidx, size_t *pos, size_t *len, size_t wrap, M_bool ispad)
{
	out[(*pos)++] = ispad ? '=' : enc_table[bidx];
	(*len)++;
	if (wrap > 0 && *len > 0 && (*len % wrap) == 0)
		out[(*pos)++] = '\n';
}

static __inline__ M_bool M_base64_get_decchar(M_int32 *val, const char *in, size_t inlen, size_t *idx)
{
	char c;

	*val = 0;

	if (*idx >= inlen)
		return M_FALSE;

	/* Skip whitespace anywhere. */
	while (*idx < inlen && M_chr_isspace(in[*idx]))
		(*idx)++;

	if (*idx >= inlen)
		return M_FALSE;

	if (!b64_isvalidchar(in[*idx]))
		return M_FALSE;

	c = in[*idx];
	(*idx)++;
	if (c == '=') {
		/* Ensure we don't have any ='s in the middle of the data. Padding is only allowed at the end. */
		if (*idx-1 < inlen-2) {
			return M_FALSE;
		}
		return M_TRUE;
	}

	/* The decode table is adjusted with the first 43 characters removed since they're all invalid. */
	if (c < 43 || (size_t)c-43 > sizeof(dec_table))
		return M_FALSE;

	/* Get the val. */
	*val = dec_table[c-43];

	/* Check if the value is valid because there are some gaps. */
	if (*val == -1)
		return M_FALSE;
	return M_TRUE;
}

static __inline__ M_bool M_base64_write_decchar(unsigned char *out, size_t outlen, unsigned char c, size_t *pos)
{
	if (*pos >= outlen)
		return M_FALSE;

	out[(*pos)++] = c;
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_base64_encode_size(size_t inlen, size_t wrap)
{
	size_t ret;

	if (inlen == 0)
		return 0;

	ret = inlen;
	/* Get the total size of encoded data. */
	ret *= 4;
	ret /= 3;
	/* Adjust for padding */
	ret += (inlen%3)?3-(inlen%3):0;
	/* Calculate the \r\n's we'll need to add for wrapping. */
	if (wrap > 0)
		ret += (ret/wrap);

	/* Assuming null termination. */
	return ret+1;
}

size_t M_base64_decode_size(size_t inlen)
{
	if (inlen == 0)
		return 0;
	/* We're not going to be exact and count whitespace that will be ingored. Instead we're
 	 * just going to give a good enough estimate that we know will hold all the data. */
	return (inlen/4)*3;
}

size_t M_base64_encode(char *out, size_t outlen, const unsigned char *in, size_t inlen, size_t wrap)
{
	M_int32 tmp;
	size_t  inlen3;
	size_t  i;
	size_t  len = 0;
	size_t  pos = 0;

	if (out == NULL || outlen == 0 || in == NULL || inlen == 0 || outlen < M_base64_encode_size(inlen, wrap))
		return 0;

	inlen3 = inlen - (inlen%3);
	/* Encode all exactly 3 byte segements. */
	for (i=0; i<inlen3; ) {
		tmp  = in[i++] << 16;
		tmp |= in[i++] << 8;
		tmp |= in[i++];

		M_base64_encode_adder(out, (tmp >> 18) & 63, &pos, &len, wrap, M_FALSE);
		M_base64_encode_adder(out, (tmp >> 12) & 63, &pos, &len, wrap, M_FALSE);
		M_base64_encode_adder(out, (tmp >>  6) & 63, &pos, &len, wrap, M_FALSE);
		M_base64_encode_adder(out,  tmp        & 63, &pos, &len, wrap, M_FALSE);
	}

	/* Encode the non-3 byte segement at the end if it exits. Will require padding. */
	if (inlen3 != inlen) {
		/* We don't have three bytes left, we only have 2 or 1. */
		switch (inlen - inlen3) {
			case 2:
				tmp = (in[inlen-2]  << 16) | (in[inlen-1] << 8);
				break;
			case 1:
				tmp = (in[inlen-1]  << 16);
				break;
			default:
				tmp = 0;
				break;
		}

		M_base64_encode_adder(out, (tmp >> 18) & 63, &pos, &len, wrap, M_FALSE);
		M_base64_encode_adder(out, (tmp >> 12) & 63, &pos, &len, wrap, M_FALSE);

		/* Add the remainder and the padding. */
		if (inlen-inlen3 == 2) {
			M_base64_encode_adder(out, (tmp >> 6) & 63, &pos, &len, wrap, M_FALSE);
		} else {
			M_base64_encode_adder(out, 0, &pos, &len, wrap, M_TRUE);
		}
		M_base64_encode_adder(out, 0, &pos, &len, wrap, M_TRUE);
	}

	/* Remove trailing \n if present. */
	if (out[pos-1] == '\n')
		pos--;

	/* Null terminate. */
	out[pos] = '\0';
	return pos;
}

size_t M_base64_decode(unsigned char *out, size_t outlen, const char *in, size_t inlen)
{
	M_int32 tmp;
	M_int32 a;
	M_int32 b;
	M_int32 c;
	M_int32 d;
	size_t  i;
	size_t  pos = 0;

	if (out == NULL || outlen == 0 || in == NULL || inlen == 0)
		return 0;

	/* Remove whitespace at the end so we can validate any trailing padding. And so we know when
 	 * we're really done decoding. */
	while (M_chr_isspace(in[inlen-1]))
		inlen--;

	if (inlen == 0)
		return 0;

	for (i=0; i<inlen; ) {
		/* Take the four ascii characters and transform them into an int. */
		if (!M_base64_get_decchar(&a, in, inlen, &i) ||
			!M_base64_get_decchar(&b, in, inlen, &i) ||
			!M_base64_get_decchar(&c, in, inlen, &i) ||
			!M_base64_get_decchar(&d, in, inlen, &i))
		{
			return 0;
		}
		tmp = (a << 18) | (b << 12) | (c << 6) | d;

		/* Break the bytes out into three chars. */
		if (!M_base64_write_decchar(out, outlen, (unsigned char)((tmp >> 16) & 0xFF), &pos)) {
			return 0;
		}
		/* We need to check for padding to know if we need to write these bytes or not. */
		if (i != inlen || (i == inlen && in[inlen-2] != '=')) {
			if (!M_base64_write_decchar(out, outlen, (unsigned char)((tmp >> 8) & 0xFF), &pos)) {
				return 0;
			}
		}
		if (i != inlen || (i == inlen && in[inlen-1] != '=')) {
			if (!M_base64_write_decchar(out, outlen, (unsigned char)(tmp & 0xFF), &pos)) {
				return 0;
			}
		}
	}

	return pos;
}
