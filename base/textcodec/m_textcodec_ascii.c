/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

M_textcodec_error_t M_textcodec_encode_ascii(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	M_textcodec_error_t res = M_TEXTCODEC_ERROR_SUCCESS;
	size_t              len;
	size_t              i;

	len = M_str_len(in);
	for (i=0; i<len; i++) {
		char c = in[i];

		if (M_chr_isascii(c)) {
			M_buf_add_byte(buf, (unsigned char)c);
			continue;
		}

		switch (ehandler) {
			case M_TEXTCODEC_EHANDLER_FAIL:
				return M_TEXTCODEC_ERROR_FAIL;
			case M_TEXTCODEC_EHANDLER_REPLACE:
				M_buf_add_byte(buf, '?');
				res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
				break;
			case M_TEXTCODEC_EHANDLER_IGNORE:
				res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
				break;
		}
	}

	return res;
}

M_textcodec_error_t M_textcodec_decode_ascii(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	size_t len;
	size_t i;

	if (M_str_ispredicate(in, M_chr_isascii)) {
		/* Ascii is a subset of utf-8. */
		M_buf_add_str(buf, in);
		return M_TEXTCODEC_ERROR_SUCCESS;
	}

	/* Not ascii and we fail on error. */
	if (ehandler == M_TEXTCODEC_EHANDLER_FAIL)
		return M_TEXTCODEC_ERROR_BADINPUT;

	/* Ignore or replace anything that's not ascii. */
	len = M_str_len(in);
	for (i=0; i<len; i++) {
		char c = in[i];

		if (M_chr_isascii(c)) {
			M_buf_add_byte(buf, (unsigned char)c);
			continue;
		}

		switch (ehandler) {
			case M_TEXTCODEC_EHANDLER_FAIL:
				/* Handled earlier. */
				break;
			case M_TEXTCODEC_EHANDLER_REPLACE:
				M_buf_add_byte(buf, 0xFF);
				M_buf_add_byte(buf, 0xFD);
				break;
			case M_TEXTCODEC_EHANDLER_IGNORE:
				break;
		}
	}

	return M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
}
