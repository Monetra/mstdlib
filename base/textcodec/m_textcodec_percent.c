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

M_textcodec_error_t M_textcodec_encode_percent(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_buf_t             *buf;
	size_t               len;
	size_t               i;
	M_textcodec_error_t  res = M_TEXTCODEC_ERROR_SUCCESS;

	buf = M_buf_create();
	len = M_str_len(in);
	for (i=0; i<len; i++) {
		M_bool process = M_FALSE;
		char   c       = in[i];
		char   hex[4]  = { 0 };
		size_t num     = 0;

		/* These have to be processed no matter what. */
		if (c < 0x21 || c > 0x7E || c == '%') {
			process = M_TRUE;
		} else {
			switch (codec) {
				case M_TEXTCODEC_PERCENT_URL:
					/* No special rules. */
					break;
				case M_TEXTCODEC_PERCENT_URLPLUS:
					/* '+' is used for space. */
					if (c == '+') {
						process = M_TRUE;
					}
					break;
				case M_TEXTCODEC_PERCENT_FORM:
					/* '+' is used for space and '~' must be encoded. */
					if (c == '+' || c == '~') {
						process = M_TRUE;
					}
					break;
				default:
					break;
			}
		}

		/* Don't encode \r, \n for forms. */
		if (codec == M_TEXTCODEC_PERCENT_FORM && (c == '\r' || c == '\n'))
			process = M_FALSE;

		/* If we don't need to do any proessing add the character as is. */
		if (!process) {
			M_buf_add_byte(buf, (unsigned char)c);
			continue;
		}
		
		if (c == ' ') {
			switch (codec) {
				case M_TEXTCODEC_PERCENT_URL:
					M_buf_add_str(buf, "%20");
					break;
				case M_TEXTCODEC_PERCENT_URLPLUS:
				case M_TEXTCODEC_PERCENT_FORM:
					M_buf_add_byte(buf, '+');
					break;
				default:
					break;
			}
			continue;
		}

		num = M_bincodec_encode(hex, sizeof(hex), (unsigned char *)&c, 1, 0, M_BINCODEC_HEX);
		if (num == 0) {
			switch (ehandler) {
				case M_TEXTCODEC_EHANDLER_FAIL:
					M_buf_cancel(buf);
					return M_TEXTCODEC_ERROR_FAIL;
				case M_TEXTCODEC_EHANDLER_REPLACE:
					M_buf_add_byte(buf, '?');
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
				case M_TEXTCODEC_EHANDLER_IGNORE:
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
			}
		} else {
			M_buf_add_byte(buf, '%');
			M_buf_add_bytes(buf, hex, num);
		}
	}

	*out = M_buf_finish_str(buf, NULL);
	return res;
}

M_textcodec_error_t M_textcodec_decode_percent(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_parser_t          *parser;
	M_buf_t             *buf;
	M_textcodec_error_t  res = M_TEXTCODEC_ERROR_SUCCESS;

	if (ehandler == M_TEXTCODEC_EHANDLER_FAIL && !M_str_ispredicate(in, M_chr_isascii))
		return M_TEXTCODEC_ERROR_BADINPUT;

	parser = M_parser_create_const((unsigned char *)in, M_str_len(in), M_PARSER_FLAG_NONE);
	if (parser == NULL)
		return M_TEXTCODEC_ERROR_FAIL;

	buf = M_buf_create();
	while (M_parser_len(parser) > 0) {
		unsigned char byte;
		char          hex[4] = { 0 };
		unsigned char dec[4] = { 0 };
		size_t        num    = 0;

		M_parser_read_byte(parser, &byte);
		if (byte == '+' && (codec == M_TEXTCODEC_PERCENT_URLPLUS || codec == M_TEXTCODEC_PERCENT_FORM)) {
			M_buf_add_byte(buf, ' ');
			continue;
		}

		if (byte != '%') {
			M_buf_add_byte(buf, byte);
			continue;
		}

		if (M_parser_len(parser) < 2) {
			switch (ehandler) {
				case M_TEXTCODEC_EHANDLER_FAIL:
					M_buf_cancel(buf);
					M_parser_destroy(parser);
					return M_TEXTCODEC_ERROR_FAIL;
				case M_TEXTCODEC_EHANDLER_REPLACE:
					M_buf_add_byte(buf, 0xFF);
					M_buf_add_byte(buf, 0xFD);
					M_parser_consume(parser, M_parser_len(parser));
					/* Fall through. */
				case M_TEXTCODEC_EHANDLER_IGNORE:
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
			}
			continue;
		}

		M_parser_read_bytes(parser, 2, (unsigned char *)hex);
		num = M_bincodec_decode(dec, sizeof(dec), hex, 2, M_BINCODEC_HEX);
		if (num == 0) {
			switch (ehandler) {
				case M_TEXTCODEC_EHANDLER_FAIL:
					M_buf_cancel(buf);
					M_parser_destroy(parser);
					return M_TEXTCODEC_ERROR_FAIL;
				case M_TEXTCODEC_EHANDLER_REPLACE:
					M_buf_add_byte(buf, 0xFF);
					M_buf_add_byte(buf, 0xFD);
					/* Fall through. */
				case M_TEXTCODEC_EHANDLER_IGNORE:
					res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
					break;
			}
			continue;
		} else {
			M_buf_add_bytes(buf, dec, 1);
		}
	}

	M_parser_destroy(parser);
	*out = M_buf_finish_str(buf, NULL);
	return res;
}
