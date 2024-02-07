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

#include "textcodec/m_textcodec_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_textcodec_validate_params(M_textcodec_buffer_t *buf, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
    M_bool fail = M_TRUE;

    switch (codec) {
        case M_TEXTCODEC_UNKNOWN:
        case M_TEXTCODEC_UTF8:
        case M_TEXTCODEC_ASCII:
        case M_TEXTCODEC_CP037:
        case M_TEXTCODEC_CP500:
        case M_TEXTCODEC_CP874:
        case M_TEXTCODEC_CP1250:
        case M_TEXTCODEC_CP1251:
        case M_TEXTCODEC_CP1252:
        case M_TEXTCODEC_CP1253:
        case M_TEXTCODEC_CP1254:
        case M_TEXTCODEC_CP1255:
        case M_TEXTCODEC_CP1256:
        case M_TEXTCODEC_CP1257:
        case M_TEXTCODEC_CP1258:
        case M_TEXTCODEC_ISO8859_1:
        case M_TEXTCODEC_ISO8859_2:
        case M_TEXTCODEC_ISO8859_3:
        case M_TEXTCODEC_ISO8859_4:
        case M_TEXTCODEC_ISO8859_5:
        case M_TEXTCODEC_ISO8859_6:
        case M_TEXTCODEC_ISO8859_7:
        case M_TEXTCODEC_ISO8859_8:
        case M_TEXTCODEC_ISO8859_9:
        case M_TEXTCODEC_ISO8859_10:
        case M_TEXTCODEC_ISO8859_11:
        case M_TEXTCODEC_ISO8859_13:
        case M_TEXTCODEC_ISO8859_14:
        case M_TEXTCODEC_ISO8859_15:
        case M_TEXTCODEC_ISO8859_16:
        case M_TEXTCODEC_PERCENT_URL:
        case M_TEXTCODEC_PERCENT_FORM:
        case M_TEXTCODEC_PERCENT_URLMIN:
        case M_TEXTCODEC_PERCENT_FORMMIN:
        case M_TEXTCODEC_PUNYCODE:
        case M_TEXTCODEC_QUOTED_PRINTABLE:
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
    (void)fail; /* Silence compiler, this is a check to make sure all enum values are checked */
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

    if (codec == M_TEXTCODEC_UTF8)
        return M_textcodec_utf8_to_utf8(buf, in, ehandler, M_TRUE);

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
        case M_TEXTCODEC_CP037:
            return M_textcodec_encode_cp037(buf, in, ehandler);
        case M_TEXTCODEC_CP500:
            return M_textcodec_encode_cp500(buf, in, ehandler);
        case M_TEXTCODEC_CP874:
            return M_textcodec_encode_cp874(buf, in, ehandler);
        case M_TEXTCODEC_CP1250:
            return M_textcodec_encode_cp1250(buf, in, ehandler);
        case M_TEXTCODEC_CP1251:
            return M_textcodec_encode_cp1251(buf, in, ehandler);
        case M_TEXTCODEC_CP1252:
            return M_textcodec_encode_cp1252(buf, in, ehandler);
        case M_TEXTCODEC_CP1253:
            return M_textcodec_encode_cp1253(buf, in, ehandler);
        case M_TEXTCODEC_CP1254:
            return M_textcodec_encode_cp1254(buf, in, ehandler);
        case M_TEXTCODEC_CP1255:
            return M_textcodec_encode_cp1255(buf, in, ehandler);
        case M_TEXTCODEC_CP1256:
            return M_textcodec_encode_cp1256(buf, in, ehandler);
        case M_TEXTCODEC_CP1257:
            return M_textcodec_encode_cp1257(buf, in, ehandler);
        case M_TEXTCODEC_CP1258:
            return M_textcodec_encode_cp1258(buf, in, ehandler);
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
        case M_TEXTCODEC_ISO8859_10:
            return M_textcodec_encode_iso8859_10(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_11:
            return M_textcodec_encode_iso8859_11(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_13:
            return M_textcodec_encode_iso8859_13(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_14:
            return M_textcodec_encode_iso8859_14(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_15:
            return M_textcodec_encode_iso8859_15(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_16:
            return M_textcodec_encode_iso8859_16(buf, in, ehandler);
        case M_TEXTCODEC_PERCENT_URL:
        case M_TEXTCODEC_PERCENT_FORM:
        case M_TEXTCODEC_PERCENT_URLMIN:
        case M_TEXTCODEC_PERCENT_FORMMIN:
            return M_textcodec_encode_percent(buf, in, ehandler, codec);
        case M_TEXTCODEC_PUNYCODE:
            return M_textcodec_encode_punycode(buf, in, ehandler);
        case M_TEXTCODEC_QUOTED_PRINTABLE:
            return M_textcodec_encode_quoted_printable(buf, in, ehandler);
    }

    return M_TEXTCODEC_ERROR_FAIL;
}

static M_textcodec_error_t M_textcodec_decode_int(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler, M_textcodec_codec_t codec)
{
    if (!M_textcodec_validate_params(buf, ehandler, codec))
        return M_TEXTCODEC_ERROR_INVALID_PARAM;

    if (M_str_isempty(in))
        return M_TEXTCODEC_ERROR_SUCCESS;

    if (codec == M_TEXTCODEC_UTF8)
        return M_textcodec_utf8_to_utf8(buf, in, ehandler, M_FALSE);

    switch (codec) {
        case M_TEXTCODEC_UNKNOWN:
        case M_TEXTCODEC_UTF8:
            break;
        case M_TEXTCODEC_ASCII:
            return M_textcodec_decode_ascii(buf, in, ehandler);
        case M_TEXTCODEC_CP037:
            return M_textcodec_decode_cp037(buf, in, ehandler);
        case M_TEXTCODEC_CP500:
            return M_textcodec_decode_cp500(buf, in, ehandler);
        case M_TEXTCODEC_CP874:
            return M_textcodec_decode_cp874(buf, in, ehandler);
        case M_TEXTCODEC_CP1250:
            return M_textcodec_decode_cp1250(buf, in, ehandler);
        case M_TEXTCODEC_CP1251:
            return M_textcodec_decode_cp1251(buf, in, ehandler);
        case M_TEXTCODEC_CP1252:
            return M_textcodec_decode_cp1252(buf, in, ehandler);
        case M_TEXTCODEC_CP1253:
            return M_textcodec_decode_cp1253(buf, in, ehandler);
        case M_TEXTCODEC_CP1254:
            return M_textcodec_decode_cp1254(buf, in, ehandler);
        case M_TEXTCODEC_CP1255:
            return M_textcodec_decode_cp1255(buf, in, ehandler);
        case M_TEXTCODEC_CP1256:
            return M_textcodec_decode_cp1256(buf, in, ehandler);
        case M_TEXTCODEC_CP1257:
            return M_textcodec_decode_cp1257(buf, in, ehandler);
        case M_TEXTCODEC_CP1258:
            return M_textcodec_decode_cp1258(buf, in, ehandler);
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
        case M_TEXTCODEC_ISO8859_10:
            return M_textcodec_decode_iso8859_10(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_11:
            return M_textcodec_decode_iso8859_11(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_13:
            return M_textcodec_decode_iso8859_13(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_14:
            return M_textcodec_decode_iso8859_14(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_15:
            return M_textcodec_decode_iso8859_15(buf, in, ehandler);
        case M_TEXTCODEC_ISO8859_16:
            return M_textcodec_decode_iso8859_16(buf, in, ehandler);
        case M_TEXTCODEC_PERCENT_URL:
        case M_TEXTCODEC_PERCENT_FORM:
        case M_TEXTCODEC_PERCENT_URLMIN:
        case M_TEXTCODEC_PERCENT_FORMMIN:
            return M_textcodec_decode_percent(buf, in, ehandler, codec);
        case M_TEXTCODEC_PUNYCODE:
            return M_textcodec_decode_punycode(buf, in, ehandler);
        case M_TEXTCODEC_QUOTED_PRINTABLE:
            return M_textcodec_decode_quoted_printable(buf, in, ehandler);
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
    res = M_textcodec_decode_buf(buf, in, ehandler, codec);
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

    if (M_str_caseeq(s, "cp037") ||
            M_str_caseeq(s, "ibm037") || M_str_caseeq(s, "ibm-037") ||
            M_str_caseeq(s, "ibm039") || M_str_caseeq(s, "ibm-039"))
    {
        return M_TEXTCODEC_CP037;
    }

    if (M_str_caseeq(s, "cp500") || M_str_caseeq(s, "ibm500") || M_str_caseeq(s, "ibm-500") ||
            M_str_caseeq(s, "ebcdic-cp-be") || M_str_caseeq(s, "ebcdic-cp-ch"))
    {
        return M_TEXTCODEC_CP500;
    }

    if (M_str_caseeq(s, "cp874") || M_str_caseeq(s, "windows-874"))
        return M_TEXTCODEC_CP874;

    if (M_str_caseeq(s, "cp1250") || M_str_caseeq(s, "windows-1250"))
        return M_TEXTCODEC_CP1250;

    if (M_str_caseeq(s, "cp1251") || M_str_caseeq(s, "windows-1251"))
        return M_TEXTCODEC_CP1251;

    if (M_str_caseeq(s, "cp1252") || M_str_caseeq(s, "windows-1252"))
        return M_TEXTCODEC_CP1252;

    if (M_str_caseeq(s, "cp1253") || M_str_caseeq(s, "windows-1253"))
        return M_TEXTCODEC_CP1253;

    if (M_str_caseeq(s, "cp1254") || M_str_caseeq(s, "windows-1254"))
        return M_TEXTCODEC_CP1254;

    if (M_str_caseeq(s, "cp1255") || M_str_caseeq(s, "windows-1255"))
        return M_TEXTCODEC_CP1255;

    if (M_str_caseeq(s, "cp1256") || M_str_caseeq(s, "windows-1256"))
        return M_TEXTCODEC_CP1256;

    if (M_str_caseeq(s, "cp1257") || M_str_caseeq(s, "windows-1257"))
        return M_TEXTCODEC_CP1257;

    if (M_str_caseeq(s, "cp1258") || M_str_caseeq(s, "windows-1258"))
        return M_TEXTCODEC_CP1258;

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

    if (M_str_caseeq(s, "latin_5")        ||
            M_str_caseeq(s, "latin-5")    ||
            M_str_caseeq(s, "latin5")     ||
            M_str_caseeq(s, "latin 5")    ||
            M_str_caseeq(s, "l5")         ||
            M_str_caseeq(s, "iso-8859-9") ||
            M_str_caseeq(s, "iso8859-9")  ||
            M_str_caseeq(s, "iso8859_9")  ||
            M_str_caseeq(s, "iso88599")   ||
            M_str_caseeq(s, "88599"))
    {
        return M_TEXTCODEC_ISO8859_9;
    }

    if (M_str_caseeq(s, "latin_6")         ||
            M_str_caseeq(s, "latin-6")     ||
            M_str_caseeq(s, "latin6")      ||
            M_str_caseeq(s, "latin 6")     ||
            M_str_caseeq(s, "l6")          ||
            M_str_caseeq(s, "iso-8859-10") ||
            M_str_caseeq(s, "iso8859-10")  ||
            M_str_caseeq(s, "iso8859_10")  ||
            M_str_caseeq(s, "iso885910")   ||
            M_str_caseeq(s, "885910"))
    {
        return M_TEXTCODEC_ISO8859_10;
    }

    if (M_str_caseeq(s, "thai")         ||
            M_str_caseeq(s, "iso-8859-11") ||
            M_str_caseeq(s, "iso8859-11")  ||
            M_str_caseeq(s, "iso8859_11")  ||
            M_str_caseeq(s, "iso885911")   ||
            M_str_caseeq(s, "885911"))
    {
        return M_TEXTCODEC_ISO8859_11;
    }

    if (M_str_caseeq(s, "latin_7")         ||
            M_str_caseeq(s, "latin-7")     ||
            M_str_caseeq(s, "latin7")      ||
            M_str_caseeq(s, "latin 7")     ||
            M_str_caseeq(s, "l7")          ||
            M_str_caseeq(s, "iso-8859-13") ||
            M_str_caseeq(s, "iso8859-13")  ||
            M_str_caseeq(s, "iso8859_13")  ||
            M_str_caseeq(s, "iso885913")   ||
            M_str_caseeq(s, "885913"))
    {
        return M_TEXTCODEC_ISO8859_13;
    }

    if (M_str_caseeq(s, "latin_8")         ||
            M_str_caseeq(s, "latin-8")     ||
            M_str_caseeq(s, "latin8")      ||
            M_str_caseeq(s, "latin 8")     ||
            M_str_caseeq(s, "l8")          ||
            M_str_caseeq(s, "iso-8859-14") ||
            M_str_caseeq(s, "iso8859-14")  ||
            M_str_caseeq(s, "iso8859_14")  ||
            M_str_caseeq(s, "iso885914")   ||
            M_str_caseeq(s, "885914"))
    {
        return M_TEXTCODEC_ISO8859_14;
    }

    if (M_str_caseeq(s, "latin_9")         ||
            M_str_caseeq(s, "latin-9")     ||
            M_str_caseeq(s, "latin9")      ||
            M_str_caseeq(s, "latin 9")     ||
            M_str_caseeq(s, "l9")          ||
            M_str_caseeq(s, "iso-8859-15") ||
            M_str_caseeq(s, "iso8859-15")  ||
            M_str_caseeq(s, "iso8859_15")  ||
            M_str_caseeq(s, "iso885915")   ||
            M_str_caseeq(s, "885915"))
    {
        return M_TEXTCODEC_ISO8859_15;
    }

    if (M_str_caseeq(s, "latin_10")         ||
            M_str_caseeq(s, "latin-10")     ||
            M_str_caseeq(s, "latin10")      ||
            M_str_caseeq(s, "latin 10")     ||
            M_str_caseeq(s, "l10")          ||
            M_str_caseeq(s, "iso-8859-16") ||
            M_str_caseeq(s, "iso8859-16")  ||
            M_str_caseeq(s, "iso8859_17")  ||
            M_str_caseeq(s, "iso885916")   ||
            M_str_caseeq(s, "885916"))
    {
        return M_TEXTCODEC_ISO8859_16;
    }

    if (M_str_caseeq(s, "percent") || M_str_caseeq(s, "url"))
        return M_TEXTCODEC_PERCENT_URL;

    if (M_str_caseeq(s, "application/x-www-form-urlencoded") ||
            M_str_caseeq(s, "x-www-form-urlencoded") ||
            M_str_caseeq(s, "www-form-urlencoded") ||
            M_str_caseeq(s, "form-urlencoded") ||
            M_str_caseeq(s, "percent_plus") || M_str_caseeq(s, "url_plus") ||
            M_str_caseeq(s, "percent-plus") || M_str_caseeq(s, "url-plus") ||
            M_str_caseeq(s, "percentplus") || M_str_caseeq(s, "urlplus"))
    {
        return M_TEXTCODEC_PERCENT_FORM;
    }

    if (M_str_caseeq(s, "percent_min") || M_str_caseeq(s, "url_min"))
        return M_TEXTCODEC_PERCENT_URLMIN;

    if (M_str_caseeq(s, "form_min") || M_str_caseeq(s, "form-urlencoded-min"))
        return M_TEXTCODEC_PERCENT_FORMMIN;

    if (M_str_caseeq(s, "punycode") || M_str_caseeq(s, "puny"))
        return M_TEXTCODEC_PUNYCODE;

    if (M_str_caseeq(s, "quoted-printable") || M_str_caseeq(s, "qp"))
        return M_TEXTCODEC_QUOTED_PRINTABLE;

    return M_TEXTCODEC_UNKNOWN;
}

const char *M_textcodec_codec_to_str(M_textcodec_codec_t codec)
{
    switch (codec) {
        case M_TEXTCODEC_UNKNOWN:
            break;
        case M_TEXTCODEC_UTF8:
            return "utf-8";
        case M_TEXTCODEC_ASCII:
            return "ascii";
        case M_TEXTCODEC_CP037:
            return "cp037";
        case M_TEXTCODEC_CP500:
            return "cp500";
        case M_TEXTCODEC_CP874:
            return "cp874";
        case M_TEXTCODEC_CP1250:
            return "cp1250";
        case M_TEXTCODEC_CP1251:
            return "cp1251";
        case M_TEXTCODEC_CP1252:
            return "cp1252";
        case M_TEXTCODEC_CP1253:
            return "cp1253";
        case M_TEXTCODEC_CP1254:
            return "cp1254";
        case M_TEXTCODEC_CP1255:
            return "cp1255";
        case M_TEXTCODEC_CP1256:
            return "cp1256";
        case M_TEXTCODEC_CP1257:
            return "cp1257";
        case M_TEXTCODEC_CP1258:
            return "cp1258";
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
            return "latin_5";
        case M_TEXTCODEC_ISO8859_10:
            return "latin_6";
        case M_TEXTCODEC_ISO8859_11:
            return "thai";
        case M_TEXTCODEC_ISO8859_13:
            return "latin_7";
        case M_TEXTCODEC_ISO8859_14:
            return "latin_8";
        case M_TEXTCODEC_ISO8859_15:
            return "latin_9";
        case M_TEXTCODEC_ISO8859_16:
            return "latin_10";
        case M_TEXTCODEC_PERCENT_URL:
            return "percent";
        case M_TEXTCODEC_PERCENT_FORM:
            return "application/x-www-form-urlencoded";
        case M_TEXTCODEC_PERCENT_URLMIN:
            return "percent_min";
        case M_TEXTCODEC_PERCENT_FORMMIN:
            return "form_min";
        case M_TEXTCODEC_PUNYCODE:
            return "punycode";
        case M_TEXTCODEC_QUOTED_PRINTABLE:
            return "quoted-printable";
    }

    return "unknown";
}
