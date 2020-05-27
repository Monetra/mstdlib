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
#include "bincodec/m_hex.h"

#define hex2dec(c) ((c) >= 'a'?((c)-'a'+10):((c) >= 'A'?((c)-'A'+10):((c)-'0')))

static __inline__ M_bool hex_isvalidchar(char c)
{
	if (c >= '0' && c <= '9')
		return M_TRUE;
	if (c >= 'A' && c <= 'F')
		return M_TRUE;
	if (c >= 'a' && c <= 'f')
		return M_TRUE;
	return M_FALSE;
}

static __inline__ void add_byte(char *out, size_t *pos, size_t *linelen, size_t wrap, char c)
{
	out[(*pos)++] = c;
	(*linelen)++;
	if (wrap > 0 && *linelen >= wrap) {
		out[(*pos)++] = '\n';
		*linelen = 0;
	}
}

static __inline__ M_bool get_byte(const char *in, size_t inLen, size_t *i, char *val, M_bool *fail)
{
	while (*i<inLen && M_chr_isspace(in[*i]))
		(*i)++;

	if (*i >= inLen)
		return M_FALSE;

	if (!hex_isvalidchar(in[*i])) {
		*fail = M_TRUE;
		return M_FALSE;
	}

	*val = in[(*i)++];
	return M_TRUE;
}

size_t M_hex_target_size(size_t srcsize, size_t wrap)
{
	if (srcsize == 0)
		return 0;

	if (wrap == 0)
		return srcsize*2;

	return (srcsize*2)+(((srcsize*2)/wrap)+1);
}

size_t M_hex_encode(const M_uint8 *in, size_t inLen, char *out, size_t outLen, size_t wrap)
{
	static const char *hex_digits = "0123456789ABCDEF";
	size_t i;
	size_t pos     = 0;
	size_t linelen = 0;

	if (in == NULL || inLen == 0 || out == NULL || outLen < M_bincodec_encode_size(inLen, wrap, M_BINCODEC_HEX))
		return 0;

	for (i=0; i<inLen; i++) {
		add_byte(out, &pos, &linelen, wrap, hex_digits[(0xf0 & in[i]) >> 4]);
		/* We'll specify the wrap as 0 if we're on the last character in the input so we don't output a '\n' because
 		 * there is nothing after to wrap. */
		add_byte(out, &pos, &linelen, (i==inLen-1)?0:wrap, hex_digits[(0x0f & in[i]) >> 0]);
	}

	/* Null terminate if we have enough bytes */
	if (pos < outLen)
		out[pos] = 0;
	return pos;
}

size_t M_hex_decode(const char *in, size_t inLen, M_uint8 *out, size_t outLen)
{
	char   c1;
	char   c2;
	size_t i    = 0;
	size_t pos  = 0;
	M_bool fail = M_FALSE;

	if (in == NULL || inLen == 0 || inLen%2 != 0 || out == NULL || outLen < inLen/2)
		return 0;

	while (get_byte(in, inLen, &i, &c1, &fail) && get_byte(in, inLen, &i, &c2, &fail)) {
		out[pos++] = (M_uint8)(hex2dec(c1) << 4 | hex2dec(c2));
	}

	if (fail)
		return 0;
	return pos;
}

