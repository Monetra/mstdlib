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
#include "bincodec/m_base64.h"
#include "bincodec/m_hex.h"
#include "bincodec/m_bincodec_conv.h"

static size_t M_bincodec_decode_size(size_t inLen, M_bincodec_codec_t codec)
{
	switch (codec) {
		case M_BINCODEC_BASE64:
		case M_BINCODEC_BASE64ORHEX:
			return M_base64_decode_size(inLen);
		case M_BINCODEC_HEX:
			return inLen/2;
	}
	return 0;
}

size_t M_bincodec_encode_size(size_t inLen, size_t wrap, M_bincodec_codec_t codec)
{
	switch (codec) {
		case M_BINCODEC_BASE64:
			return M_base64_encode_size(inLen, wrap);
		/* Hex is larger than base64 so if the caller wants to allocate a buffer that has enough
		 * room for either hex should be used. */
		case M_BINCODEC_HEX:
		case M_BINCODEC_BASE64ORHEX:
			return M_hex_target_size(inLen, wrap);
	}

	return 0;
}

char *M_bincodec_encode_alloc(const M_uint8 *in, size_t inLen, size_t wrap, M_bincodec_codec_t codec)
{
	size_t  len  = 0;
	char   *p    = NULL;
	size_t  pLen = 0;

	if (in == NULL || inLen == 0) {
		return NULL;
	}

	pLen = M_bincodec_encode_size(inLen, wrap, codec);
	if (pLen == 0) {
		return 0;
	}
	pLen += 1; /* For encode, we want enough room for a NULL-terminator */

	p     = M_malloc(pLen);
	len   = M_bincodec_encode(p, pLen, in, inLen, wrap, codec);

	if (p != NULL && len > 0) {
		p[len] = '\0';
	} else {
		M_free(p);
		p = NULL;
	}

	return p;
}

size_t M_bincodec_encode(char *out, size_t outLen, const M_uint8 *in, size_t inLen, size_t wrap, M_bincodec_codec_t codec)
{
	switch (codec) {
		case M_BINCODEC_BASE64:
			return M_base64_encode(out, outLen, in, inLen, wrap);
		case M_BINCODEC_HEX:
			return M_hex_encode(in, inLen, out, outLen, wrap);
		case M_BINCODEC_BASE64ORHEX:
			return 0;
	}

	return 0;
}

M_uint8 *M_bincodec_decode_alloc(const char *in, size_t inLen, size_t *outLen, M_bincodec_codec_t codec)
{
	size_t   len  = 0;
	M_uint8 *p    = NULL;
	size_t   pLen = 0;

	if (outLen == NULL)
		return NULL;

	*outLen = 0;

	if (in == NULL || inLen == 0) {
		return NULL;
	}

	pLen = M_bincodec_decode_size(inLen, codec);
	if (pLen == 0) {
		return 0;
	}
	p   = M_malloc(pLen + 1);
	len = M_bincodec_decode(p, pLen, in, inLen, codec);

	if (p != NULL && len > 0) {
		p[len] = '\0';
		*outLen = len;
	} else {
		M_free(p);
		p = NULL;
	}

	return p;
}


char *M_bincodec_decode_str_alloc(const char *in, M_bincodec_codec_t codec)
{
	M_uint8 *out;
	size_t   outLen;

	out = M_bincodec_decode_alloc(in, M_str_len(in), &outLen, codec);
	if (out == NULL)
		return NULL;

	if (!M_str_isstr(out, outLen+1 /* M_bincodec_decode_alloc ensures a NULL after data length */)) {
		M_free(out);
		return NULL;
	}

	return (char *)out;
}


size_t M_bincodec_decode(M_uint8 *out, size_t outLen, const char *in, size_t inLen, M_bincodec_codec_t codec)
{
	switch (codec) {
		case M_BINCODEC_BASE64:
			return M_base64_decode(out, outLen, in, inLen);
		case M_BINCODEC_HEX:
			return M_hex_decode(in, inLen, out, outLen);
		case M_BINCODEC_BASE64ORHEX:
			return M_hex_or_base64_to_bin(out, outLen, in, inLen);
	}

	return 0;
}

char *M_bincodec_convert_alloc(const char *in, size_t inLen, size_t wrap, M_bincodec_codec_t inCodec, M_bincodec_codec_t outCodec)
{
	size_t   decLen;
	M_uint8 *dec;
	char    *enc;
	
	dec = M_bincodec_decode_alloc(in, inLen, &decLen, inCodec);
	if (dec == NULL) {
		return 0;
	}

	enc = M_bincodec_encode_alloc(dec, decLen, wrap, outCodec);

	M_free(dec);
	return enc;
}

size_t M_bincodec_convert(char *out, size_t outLen, size_t wrap, M_bincodec_codec_t outCodec, const char *in, size_t inLen, M_bincodec_codec_t inCodec)
{
	size_t   decLen;
	M_uint8 *dec;
	
	dec = M_bincodec_decode_alloc(in, inLen, &decLen, inCodec);
	if (dec == NULL) {
		return 0;
	}

	outLen = M_bincodec_encode(out, outLen, dec, decLen, wrap, outCodec);

	M_free(dec);
	return outLen;
}
