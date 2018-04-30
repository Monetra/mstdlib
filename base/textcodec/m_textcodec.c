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

static M_textcodec_ehandler_t M_textcodec_validate_ehandler(M_textcodec_ehandler_t ehandler)
{
	switch (ehandler) {
		case M_TEXTCODEC_EHANDLER_FAIL:
		case M_TEXTCODEC_EHANDLER_REPLACE:
		case M_TEXTCODEC_EHANDLER_IGNORE:
			return ehandler;
	}
	return M_TEXTCODEC_EHANDLER_FAIL;
}


static M_textcodec_error_t M_textcodec_encode_int(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	switch (buf->type) {
		case M_TEXTCODEC_BUFFER_TYPE_BUF:
			if (buf->u.buf == NULL) {
				return M_TEXTCODEC_ERROR_INVALID_PARAM;
			}
			break;
		case M_TEXTCODEC_BUFFER_TYPE_PARSER:
			if (buf->u.parser == NULL) {
				return M_TEXTCODEC_ERROR_INVALID_PARAM;
			}
			break;
	}

	if (M_str_isempty(in))
		return M_TEXTCODEC_ERROR_SUCCESS;

	/* XXX: Validate input is utf-8 and do something if not. */

	ehandler = M_textcodec_validate_ehandler(ehandler);
	switch (codec) {
		case M_TEXTCODEC_ASCII:
			return M_textcodec_encode_ascii(buf, in, ehandler);
		case M_TEXTCODEC_PERCENT_URL:
		case M_TEXTCODEC_PERCENT_URLPLUS:
		case M_TEXTCODEC_PERCENT_FORM:
			return M_textcodec_encode_percent(buf, in, ehandler, codec);
	}

	return M_TEXTCODEC_ERROR_FAIL;
}

static M_textcodec_error_t M_textcodec_decode_int(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	switch (buf->type) {
		case M_TEXTCODEC_BUFFER_TYPE_BUF:
			if (buf->u.buf == NULL) {
				return M_TEXTCODEC_ERROR_INVALID_PARAM;
			}
			break;
		case M_TEXTCODEC_BUFFER_TYPE_PARSER:
			if (buf->u.parser == NULL) {
				return M_TEXTCODEC_ERROR_INVALID_PARAM;
			}
			break;
	}

	if (M_str_isempty(in))
		return M_TEXTCODEC_ERROR_SUCCESS;

	ehandler = M_textcodec_validate_ehandler(ehandler);
	switch (codec) {
		case M_TEXTCODEC_ASCII:
			return M_textcodec_decode_ascii(buf, in, ehandler);
		case M_TEXTCODEC_PERCENT_URL:
		case M_TEXTCODEC_PERCENT_URLPLUS:
		case M_TEXTCODEC_PERCENT_FORM:
			return M_textcodec_decode_percent(buf, in, ehandler, codec);
	}

	return M_TEXTCODEC_ERROR_FAIL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_encode(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_buf_t             *buf;
	M_textcodec_error_t  res;

	if (out == NULL)
		return M_TEXTCODEC_ERROR_INVALID_PARAM;
	*out = NULL;

	buf = M_buf_create();
	res = M_textcodec_encode_buf(buf, in, ehandler, codec);
	if (M_textcodec_error_is_error(res)) {
		M_buf_cancel(buf);
		return res;
	}

	*out = M_buf_finish_str(buf, NULL);
	return res;
}

M_textcodec_error_t M_textcodec_encode_buf(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_textcodec_buffer_t wbuf;

	M_mem_set(&wbuf, 0, sizeof(wbuf));
	wbuf.type  = M_TEXTCODEC_BUFFER_TYPE_BUF;
	wbuf.u.buf = buf;

	return M_textcodec_encode_int(&wbuf, in, ehandler, codec);
}

M_textcodec_error_t M_textcodec_encode_parser(M_parser_t *parser, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_textcodec_buffer_t wbuf;

	M_mem_set(&wbuf, 0, sizeof(wbuf));
	wbuf.type     = M_TEXTCODEC_BUFFER_TYPE_PARSER;
	wbuf.u.parser = parser;

	return M_textcodec_encode_int(&wbuf, in, ehandler, codec);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_textcodec_error_t M_textcodec_decode(char **out, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_buf_t             *buf;
	M_textcodec_error_t  res;

	if (out == NULL)
		return M_TEXTCODEC_ERROR_INVALID_PARAM;
	*out = NULL;

	buf = M_buf_create();
	res = M_textcodec_encode_buf(buf, in, ehandler, codec);
	if (M_textcodec_error_is_error(res)) {
		M_buf_cancel(buf);
		return res;
	}

	*out = M_buf_finish_str(buf, NULL);
	return res;
}

M_textcodec_error_t M_textcodec_decode_buf(M_buf_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_textcodec_buffer_t wbuf;

	M_mem_set(&wbuf, 0, sizeof(wbuf));
	wbuf.type  = M_TEXTCODEC_BUFFER_TYPE_BUF;
	wbuf.u.buf = buf;

	return M_textcodec_decode_int(&wbuf, in, ehandler, codec);
}

M_textcodec_error_t M_textcodec_decode_parser(M_parser_t *parser, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_textcodec_buffer_t wbuf;

	M_mem_set(&wbuf, 0, sizeof(wbuf));
	wbuf.type     = M_TEXTCODEC_BUFFER_TYPE_PARSER;
	wbuf.u.parser = parser;

	return M_textcodec_decode_int(&wbuf, in, ehandler, codec);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_textcodec_error_is_error(M_textcodec_error_t err)
{
	switch (err) {
		case M_TEXTCODEC_ERROR_SUCCESS:
		case M_TEXTCODEC_ERROR_SUCCESS_EHANDLER:
			return M_FALSE;
		default:
			return M_TRUE;
	}
	return M_TRUE;
}
