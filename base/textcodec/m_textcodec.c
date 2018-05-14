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

static M_bool M_textcodec_validate_params(M_textcodec_buffer_t *buf, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	M_bool fail;

	fail = M_TRUE;
	switch (codec) {
		case M_TEXTCODEC_UNKNOWN:
		case M_TEXTCODEC_UTF8:
		case M_TEXTCODEC_ASCII:
		case M_TEXTCODEC_PERCENT_URL:
		case M_TEXTCODEC_PERCENT_URLPLUS:
		case M_TEXTCODEC_PERCENT_FORM:
		case M_TEXTCODEC_CP1252:
		case M_TEXTCODEC_ISO8859_1:
		case M_TEXTCODEC_ISO8859_2:
		case M_TEXTCODEC_ISO8859_3:
		case M_TEXTCODEC_ISO8859_4:
		case M_TEXTCODEC_ISO8859_5:
		case M_TEXTCODEC_ISO8859_6:
		case M_TEXTCODEC_ISO8859_7:
		case M_TEXTCODEC_ISO8859_8:
		case M_TEXTCODEC_ISO8859_9:
			fail = M_FALSE;
			break;
	}
	if (fail)
		return M_FALSE;

	fail = M_TRUE;
	switch (buf->type) {
		case M_TEXTCODEC_BUFFER_TYPE_BUF:
			if (buf->u.buf != NULL) {
				fail = M_FALSE;
			}
			break;
		case M_TEXTCODEC_BUFFER_TYPE_PARSER:
			if (buf->u.parser != NULL) {
				fail = M_FALSE;
			}
			break;
	}
	if (fail)
		return M_FALSE;

	fail = M_TRUE;
	switch (ehandler) {
		case M_TEXTCODEC_EHANDLER_FAIL:
		case M_TEXTCODEC_EHANDLER_REPLACE:
		case M_TEXTCODEC_EHANDLER_IGNORE:
			fail = M_FALSE;
			break;
	}
	if (fail)
		return M_FALSE;

	return M_TRUE;
}

static M_textcodec_error_t M_textcodec_encode_int(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	if (!M_textcodec_validate_params(buf, ehandler, codec))
		return M_TEXTCODEC_ERROR_INVALID_PARAM;

	if (M_str_isempty(in))
		return M_TEXTCODEC_ERROR_SUCCESS;

	/* Validate input is utf-8. */
	if (!M_utf8_is_valid(in, NULL) && ehandler == M_TEXTCODEC_EHANDLER_FAIL)
		return M_TEXTCODEC_ERROR_BADINPUT;

	switch (codec) {
		case M_TEXTCODEC_UNKNOWN:
			M_textcodec_buffer_add_str(buf, in);
			return M_TEXTCODEC_ERROR_SUCCESS;
		case M_TEXTCODEC_UTF8:
			break;
		case M_TEXTCODEC_ASCII:
			return M_textcodec_encode_ascii(buf, in, ehandler);
		case M_TEXTCODEC_PERCENT_URL:
		case M_TEXTCODEC_PERCENT_URLPLUS:
		case M_TEXTCODEC_PERCENT_FORM:
			return M_textcodec_encode_percent(buf, in, ehandler, codec);
		case M_TEXTCODEC_CP1252:
			return M_textcodec_encode_cp1252(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_1:
			return M_textcodec_encode_iso8859_1(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_2:
			return M_textcodec_encode_iso8859_2(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_3:
			return M_textcodec_encode_iso8859_3(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_4:
			return M_textcodec_encode_iso8859_4(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_5:
			return M_textcodec_encode_iso8859_5(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_6:
			return M_textcodec_encode_iso8859_6(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_7:
			return M_textcodec_encode_iso8859_7(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_8:
			return M_textcodec_encode_iso8859_8(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_9:
			return M_textcodec_encode_iso8859_9(buf, in, ehandler);
	}

	return M_TEXTCODEC_ERROR_FAIL;
}

static M_textcodec_error_t M_textcodec_decode_int(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
	if (!M_textcodec_validate_params(buf, ehandler, codec))
		return M_TEXTCODEC_ERROR_INVALID_PARAM;

	if (M_str_isempty(in))
		return M_TEXTCODEC_ERROR_SUCCESS;

	switch (codec) {
		case M_TEXTCODEC_UNKNOWN:
			break;
		case M_TEXTCODEC_UTF8:
			M_textcodec_buffer_add_str(buf, in);
			return M_TEXTCODEC_ERROR_SUCCESS;
		case M_TEXTCODEC_ASCII:
			return M_textcodec_decode_ascii(buf, in, ehandler);
		case M_TEXTCODEC_PERCENT_URL:
		case M_TEXTCODEC_PERCENT_URLPLUS:
		case M_TEXTCODEC_PERCENT_FORM:
			return M_textcodec_decode_percent(buf, in, ehandler, codec);
		case M_TEXTCODEC_CP1252:
			return M_textcodec_decode_cp1252(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_1:
			return M_textcodec_decode_iso8859_1(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_2:
			return M_textcodec_decode_iso8859_2(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_3:
			return M_textcodec_decode_iso8859_3(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_4:
			return M_textcodec_decode_iso8859_4(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_5:
			return M_textcodec_decode_iso8859_5(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_6:
			return M_textcodec_decode_iso8859_6(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_7:
			return M_textcodec_decode_iso8859_7(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_8:
			return M_textcodec_decode_iso8859_8(buf, in, ehandler);
		case M_TEXTCODEC_ISO8859_9:
			return M_textcodec_decode_iso8859_9(buf, in, ehandler);
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

M_textcodec_codec_t M_textcodec_codec_from_str(const char *s)
{
	if (M_str_isempty(s))
		return M_TEXTCODEC_UNKNOWN;

	if (M_str_caseeq(s, "utf8") || M_str_caseeq(s, "utf-8") || M_str_caseeq(s, "utf_8"))
		return M_TEXTCODEC_UTF8;

	if (M_str_caseeq(s, "ascii") || M_str_caseeq(s, "us-ascii"))
		return M_TEXTCODEC_ASCII;

	if (M_str_caseeq(s, "percent") || M_str_caseeq(s, "url"))
		return M_TEXTCODEC_PERCENT_URL;

	if (M_str_caseeq(s, "percent_plus") || M_str_caseeq(s, "url_plus") ||
			M_str_caseeq(s, "percent-plus") || M_str_caseeq(s, "url-plus") ||
			M_str_caseeq(s, "percentplus") || M_str_caseeq(s, "urlplus"))
	{
		return M_TEXTCODEC_PERCENT_URLPLUS;
	}

	if (M_str_caseeq(s, "application/x-www-form-urlencoded") ||
			M_str_caseeq(s, "x-www-form-urlencoded") ||
			M_str_caseeq(s, "www-form-urlencoded") ||
			M_str_caseeq(s, "form-urlencoded"))
	{
		return M_TEXTCODEC_PERCENT_FORM;
	}

	if (M_str_caseeq(s, "cp1252") || M_str_caseeq(s, "windows-1252"))
		return M_TEXTCODEC_CP1252;

	if (M_str_caseeq(s, "latin_1")        || 
			M_str_caseeq(s, "latin-1")    || 
			M_str_caseeq(s, "latin1")     || 
			M_str_caseeq(s, "latin 1")    || 
			M_str_caseeq(s, "latin")      || 
			M_str_caseeq(s, "l1")         || 
			M_str_caseeq(s, "iso-8859-1") || 
			M_str_caseeq(s, "iso8859-1")  || 
			M_str_caseeq(s, "iso8859_1")  || 
			M_str_caseeq(s, "iso88591")   || 
			M_str_caseeq(s, "8859")       || 
			M_str_caseeq(s, "88591")      || 
			M_str_caseeq(s, "cp819"))
	{
		return M_TEXTCODEC_ISO8859_1;
	}

	if (M_str_caseeq(s, "latin_2")        || 
			M_str_caseeq(s, "latin-2")    || 
			M_str_caseeq(s, "latin2")     || 
			M_str_caseeq(s, "latin 2")    || 
			M_str_caseeq(s, "l2")         || 
			M_str_caseeq(s, "iso-8859-2") || 
			M_str_caseeq(s, "iso8859-2")  || 
			M_str_caseeq(s, "iso8859_2")  || 
			M_str_caseeq(s, "iso88592")   || 
			M_str_caseeq(s, "88592"))
	{
		return M_TEXTCODEC_ISO8859_2;
	}

	if (M_str_caseeq(s, "latin_3")        || 
			M_str_caseeq(s, "latin-3")    || 
			M_str_caseeq(s, "latin3")     || 
			M_str_caseeq(s, "latin 3")    || 
			M_str_caseeq(s, "l3")         || 
			M_str_caseeq(s, "iso-8859-3") || 
			M_str_caseeq(s, "iso8859-3")  || 
			M_str_caseeq(s, "iso8859_3")  || 
			M_str_caseeq(s, "iso88593")   || 
			M_str_caseeq(s, "88593"))
	{
		return M_TEXTCODEC_ISO8859_3;
	}

	if (M_str_caseeq(s, "latin_4")        || 
			M_str_caseeq(s, "latin-4")    || 
			M_str_caseeq(s, "latin4")     || 
			M_str_caseeq(s, "latin 4")    || 
			M_str_caseeq(s, "l4")         || 
			M_str_caseeq(s, "iso-8859-4") || 
			M_str_caseeq(s, "iso8859-4")  || 
			M_str_caseeq(s, "iso8859_4")  || 
			M_str_caseeq(s, "iso88594")   || 
			M_str_caseeq(s, "88594"))
	{
		return M_TEXTCODEC_ISO8859_4;
	}

	if (M_str_caseeq(s, "cyrillic")       || 
			M_str_caseeq(s, "iso-8859-5") || 
			M_str_caseeq(s, "iso8859-5")  || 
			M_str_caseeq(s, "iso8859_5")  || 
			M_str_caseeq(s, "iso88595")   || 
			M_str_caseeq(s, "88595"))
	{
		return M_TEXTCODEC_ISO8859_5;
	}

	if (M_str_caseeq(s, "arabic")         || 
			M_str_caseeq(s, "iso-8859-6") || 
			M_str_caseeq(s, "iso8859-6")  || 
			M_str_caseeq(s, "iso8859_6")  || 
			M_str_caseeq(s, "iso88596")   || 
			M_str_caseeq(s, "88596"))
	{
		return M_TEXTCODEC_ISO8859_6;
	}

	if (M_str_caseeq(s, "greek")          || 
			M_str_caseeq(s, "greek8")     || 
			M_str_caseeq(s, "iso-8859-7") || 
			M_str_caseeq(s, "iso8859-7")  || 
			M_str_caseeq(s, "iso8859_7")  || 
			M_str_caseeq(s, "iso88597")   || 
			M_str_caseeq(s, "88597"))
	{
		return M_TEXTCODEC_ISO8859_7;
	}

	if (M_str_caseeq(s, "hebrew")         || 
			M_str_caseeq(s, "iso-8859-8") || 
			M_str_caseeq(s, "iso8859-8")  || 
			M_str_caseeq(s, "iso8859_8")  || 
			M_str_caseeq(s, "iso88598")   || 
			M_str_caseeq(s, "88598"))
	{
		return M_TEXTCODEC_ISO8859_8;
	}

	if (M_str_caseeq(s, "turkish")        || 
			M_str_caseeq(s, "iso-8859-9") || 
			M_str_caseeq(s, "iso8859-9")  || 
			M_str_caseeq(s, "iso8859_9")  || 
			M_str_caseeq(s, "iso88599")   || 
			M_str_caseeq(s, "88599"))
	{
		return M_TEXTCODEC_ISO8859_9;
	}

	return M_TEXTCODEC_UNKNOWN;
}

const char *M_textcodec_codec_to_str(M_textcodec_codec_t codec)
{
	switch (codec) {
		case M_TEXTCODEC_UNKNOWN:
			break;
		case M_TEXTCODEC_UTF8:
			return "utf8";
		case M_TEXTCODEC_ASCII:
			return "ascii";
		case M_TEXTCODEC_PERCENT_URL:
			return "percent";
		case M_TEXTCODEC_PERCENT_URLPLUS:
			return "percent_plus";
		case M_TEXTCODEC_PERCENT_FORM:
			return "application/x-www-form-urlencoded";
		case M_TEXTCODEC_CP1252:
			return "cp1252";
		case M_TEXTCODEC_ISO8859_1:
			return "latin_1";
		case M_TEXTCODEC_ISO8859_2:
			return "latin_2";
		case M_TEXTCODEC_ISO8859_3:
			return "latin_3";
		case M_TEXTCODEC_ISO8859_4:
			return "latin_4";
		case M_TEXTCODEC_ISO8859_5:
			return "cyrillic";
		case M_TEXTCODEC_ISO8859_6:
			return "arabic";
		case M_TEXTCODEC_ISO8859_7:
			return "greek";
		case M_TEXTCODEC_ISO8859_8:
			return "hebrew";
		case M_TEXTCODEC_ISO8859_9:
			return "turkish";
	}

	return "unknown";
}
