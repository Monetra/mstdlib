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

M_textcodec_error_t M_textcodec_encode_ascii(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	const char          *next = in;
	M_textcodec_error_t  res  = M_TEXTCODEC_ERROR_SUCCESS;

	while (next != '\0' && !M_textcodec_error_is_error(res)) {
		char           ubuf[8] = { 0 };
		size_t         len     = 0;
		M_utf8_error_t ures;

		/* read the next utf8 character. */
		ures = M_utf8_get_chr(next, ubuf, sizeof(ubuf), &len, &next);

		/* If we have an invalid we need to skip it. Since utf8 characters
 		 * can have multiple bytes we want to and replacement per cha cater
		 * not per byte. */
		if (ures != M_TEXTCODEC_ERROR_SUCCESS) {
			next = M_utf8_next_chr(next);
		}

		if (ures == M_TEXTCODEC_ERROR_SUCCESS && len == 1 && M_chr_isascii(ubuf[0])) {
			/* Success and 1 ASCII character. Then we have an ASCII character! */
			M_textcodec_buffer_add_byte(buf, (unsigned char)ubuf[0]);
		} else {
			/* Either we encountered an invalid utf8 sequence or it's
 			 * not ascii. */
			switch (ehandler) {
				case M_TEXTCODEC_EHANDLER_FAIL:
					res = M_TEXTCODEC_ERROR_FAIL;
					break;
				case M_TEXTCODEC_EHANDLER_REPLACE:
					M_textcodec_buffer_add_byte(buf, '?');
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
				case M_TEXTCODEC_EHANDLER_IGNORE:
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
			}
		}
	}

	return res;
}

M_textcodec_error_t M_textcodec_decode_ascii(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
	size_t len;
	size_t i;

	if (M_str_ispredicate(in, M_chr_isascii)) {
		/* ASCII is a subset of utf-8. */
		M_textcodec_buffer_add_str(buf, in);
		return M_TEXTCODEC_ERROR_SUCCESS;
	}

	/* Not ASCII and we fail on error. */
	if (ehandler == M_TEXTCODEC_EHANDLER_FAIL)
		return M_TEXTCODEC_ERROR_BADINPUT;

	/* Ignore or replace anything that's not ASCII. */
	len = M_str_len(in);
	for (i=0; i<len; i++) {
		char c = in[i];

		if (M_chr_isascii(c)) {
			M_textcodec_buffer_add_byte(buf, (unsigned char)c);
			continue;
		}

		switch (ehandler) {
			case M_TEXTCODEC_EHANDLER_FAIL:
				/* Handled earlier. */
				break;
			case M_TEXTCODEC_EHANDLER_REPLACE:
				M_textcodec_buffer_add_str(buf, M_UTF8_REPLACE);
				break;
			case M_TEXTCODEC_EHANDLER_IGNORE:
				break;
		}
	}

	return M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
}
