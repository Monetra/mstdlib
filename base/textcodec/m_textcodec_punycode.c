/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
#include "textcodec/m_textcodec_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const M_uint32 base         = 36;
static const M_uint32 tmin         = 1;
static const M_uint32 tmax         = 26;
static const M_uint32 skew         = 38;
static const M_uint32 damp         = 700;
static const M_uint32 initial_bias = 72;
static const M_uint32 initial_n    = 128;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_uint32 adapt(M_uint32 delta, M_uint32 numpoints, M_bool firsttime)
{
	const M_uint32 v = 455; /* ((base - tmin) * tmax) / 2 */
	const M_uint32 w = 35;  /* base - tmin */
	const M_uint32 x = 36;  /* base - tmin + 1 */
	M_uint32       k = 0;

	if (firsttime) {
		delta /= damp;
	} else {
		delta /= 2;
	}
	delta += (delta / numpoints);

	while (delta > v) {
		delta /= w;
		k     += base;
	}
	return k + ((x * delta) / (delta + skew));
}

static char encode_digit(M_uint32 d)
{
	if (d <= 25) {
		return (char)(d + 0x61);
	} else {
		return (char)(d + 0x16);
	}
	return 0;
}

static M_uint32 decode_digit(M_uint32 d)
{
	if (d >= 0x41 && d <= 0x5A)
		return d - 0x41;
	if (d >= 0x61 && d <= 0x7A)
		return d - 0x61;
	if (d >= 0x30 && d <= 0x39)
		return d - 0x16;
	return base;
}

static M_uint32 threshold(M_uint32 k, M_uint32 bias)
{
	if (k <= bias + tmin) {
		return tmin;
	} else if (k >= bias + tmax) {
		return tmax;
	}
	return k - bias;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_encode_punycode(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	M_list_u64_t *non_basic = NULL;
	const char   *p;
	M_uint32      n         = initial_n;
	M_uint32      m         = 0;
	M_uint32      b         = 0;
	M_uint32      h         = 0;
	M_uint32      delta     = 0;
	M_uint32      bias      = initial_bias;

	(void)ehandler;

	/* All ASCII then nothing to encode. */
	if (M_str_isascii(in)) {
		M_textcodec_buffer_add_str(buf, in);
		M_textcodec_buffer_add_byte(buf, '-');
		return M_TEXTCODEC_ERROR_SUCCESS;
	}

	/* Validate this is utf-8. If an error handler other than fail was
 	 * chosen we could get here. But Punycode is fixed and can't have bad input. */
	if (!M_utf8_is_valid(in, NULL))
		return M_TEXTCODEC_ERROR_BADINPUT;

	/* When we go though the list we need to work from smallest to largest codepoint
 	 * and we only want to process them once. */
	non_basic = M_list_u64_create(M_LIST_U64_SORTASC|M_LIST_U64_SET);

	/* Separate the basic from the non-basic.
 	 * Getting the cp should always be successful since we validated the string already. */
	p = in;
	while (p != NULL && *p != '\0' && M_utf8_get_cp(p, &m, &p) == M_UTF8_ERROR_SUCCESS) {
		if (m < 0x80) {
			h++;
			M_textcodec_buffer_add_byte(buf, (unsigned char)m);
		} else {
			M_list_u64_insert(non_basic, m);
		}
	}

	b = h;
	/* Add the delim to the output. */
	if (h != 0)
		M_textcodec_buffer_add_byte(buf, '-');

	while (M_list_u64_len(non_basic) > 0) {
		M_uint32 c = 0;

		/* Get the codepoint we want to work on. */
		m = (M_uint32)M_list_u64_take_first(non_basic);

		/* Check for overflow. */
		if (m - n > (M_UINT32_MAX - delta) / (h + 1)) {
			goto fail;
		}
		delta += (m - n) * (h + 1);
		n      = m;

		/* Go though all characters in input and get the codepoint. */
		p = in;
		while (p != NULL && *p != '\0' && M_utf8_get_cp(p, &c, &p) == M_UTF8_ERROR_SUCCESS) {
			M_uint32 q;
			M_uint32 t;
			size_t   k;

			if (c < n) {
				/* Check for overflow. */
				if (delta == M_UINT32_MAX) {
					goto fail;
				}
				delta++;
				continue;
			} else if (c > n) {
				continue;
			}

			/* c == n
 			 *
			 * We've found a location for our codepoint. Start breaking it down
			 * to ascii and it to the buffer. */
			q = delta;
			for (k=base; k<SIZE_MAX; k+=base) {
				t = threshold((M_uint32)k, bias);
				if (q < t)
					break;

				M_textcodec_buffer_add_byte(buf, (unsigned char)encode_digit(t + (q - t) % (base - t)));
				q = (q - t) / (base - t);
			}

			M_textcodec_buffer_add_byte(buf, (unsigned char)encode_digit(q));
			bias = adapt(delta, h+1, h==b?M_TRUE:M_FALSE);
			delta = 0;
			h++;
		}

		delta++;
		n++;
	}

	M_list_u64_destroy(non_basic);
	return M_TEXTCODEC_ERROR_SUCCESS;

fail:
	M_list_u64_destroy(non_basic);
	return M_TEXTCODEC_ERROR_FAIL;
}

M_textcodec_error_t M_textcodec_decode_punycode(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	M_list_str_t *l        = NULL;
	const char   *p;
	char         *out;
	char          tempa[8];
	M_uint32      d;
	M_uint32      bias     = initial_bias;
	M_uint32      n        = initial_n;
	size_t        len;
	size_t        b;
	M_uint32      i;

	(void)ehandler;

	/* Punycode must be ASCII. */
	if (!M_str_isascii(in))
		return M_TEXTCODEC_ERROR_BADINPUT;

	/* Create a list to hold all of our characters. We can't use an M_buf_t
 	 * because 1 charter could be multiple bytes (going to utf-8) and we
	 * need to insert in the middle of the string. */
	l = M_list_str_create(M_LIST_STR_NONE);

	/* Find the delimiter. */
	p = M_str_rchr(in, '-');
	if (p == NULL) {
		p = in;
	} else {
		/* Put all characters before the delim into our list. */
		len = (size_t)(p - in);
		for (i=0; i<len; i++) {
			M_snprintf(tempa, sizeof(tempa), "%c", in[i]);
			M_list_str_insert(l, tempa);
		}

		/* Move past the delim. */
		p++;
	}

	/* Read all the characters after the delim which are converted to the
 	 * utf-8 characters and inserted in the proper location. */
	i   = 0;
	len = M_str_len(p);
	for (b=0; b<len; ) {
		M_uint32 w    = 1;
		size_t   oldi = i;
		size_t   len2;
		size_t   k;
		M_uint32 t;

		for (k=base; k<SIZE_MAX; k+=base) {
			/* Check for bad digit. Something like a control character. */
			d = decode_digit((unsigned char)p[b]);
			if (d >= base)
				goto fail;
			b++;

			/* Check for overflow. */
			if (d > (M_UINT32_MAX - i) / w)
				goto fail;
			i += d * w;
			t  = threshold((M_uint32)k, bias);

			if (d < t)
				break;

			/* Check for overflow. */
			if (w > M_UINT32_MAX / (base - t))
				goto fail;
			w *= base - t;
		}

		/* Update our counters. */
		len2  = M_list_str_len(l) + 1;
		bias  = adapt((M_uint32)(i - oldi), (M_uint32)len2, oldi==0?M_TRUE:M_FALSE);
		n    += (M_uint32)(i / len2);
		i    %= (M_uint32)len2;

		/* Insert the decoded character. */
		if (M_utf8_from_cp(tempa, sizeof(tempa), &len2, n) != M_UTF8_ERROR_SUCCESS)
			goto fail;
		tempa[len2] = '\0';
		M_list_str_insert_at(l, tempa, i);
		i++;
	}

	/* Join all of our chanters into a string. */
	out = M_list_str_join_str(l, "");
	M_textcodec_buffer_add_str(buf, out);
	M_free(out);

	M_list_str_destroy(l);
	return M_TEXTCODEC_ERROR_SUCCESS;

fail:
	M_list_str_destroy(l);
	return M_TEXTCODEC_ERROR_FAIL;
}
