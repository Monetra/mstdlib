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

#ifndef __M_TEXTCODEC_INT_H__
#define __M_TEXTCODEC_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_TEXTCODEC_BUFFER_TYPE_BUF,
	M_TEXTCODEC_BUFFER_TYPE_PARSER
} M_textcodec_buffer_type_t;

typedef struct {
	M_textcodec_buffer_type_t type;
	union {
		M_buf_t    *buf;
		M_parser_t *parser;
	} u;
} M_textcodec_buffer_t;

#define M_UTF8_REPLACE "\xFF\xFD"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_textcodec_buffer_add_byte(M_textcodec_buffer_t *buf, unsigned char b);
M_bool M_textcodec_buffer_add_bytes(M_textcodec_buffer_t *buf, const unsigned char *bs, size_t len);
M_bool M_textcodec_buffer_add_str(M_textcodec_buffer_t *buf, const char *s);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_encode_ascii(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler);
M_textcodec_error_t M_textcodec_decode_ascii(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_encode_cp1252(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler);
M_textcodec_error_t M_textcodec_decode_cp1252(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_encode_percent(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec);
M_textcodec_error_t M_textcodec_decode_percent(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec);

#endif /* __M_TEXTCODEC_INT_H__ */
