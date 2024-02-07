/* The MIT License (MIT)
 *
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

M_textcodec_error_t M_textcodec_encode_quoted_printable(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
    size_t cnt = 0;
    size_t len;
    size_t i;

    (void)ehandler;

    len = M_str_len(in);
    for (i=0; i<len; i++) {
        char c = in[i];

        /* Tab, \r, \n, and space are allowed to be left alone.
         * RFC says safe characters are 33-60 and 62-126 inclusive but
         * we want a few more in this list. */
        if (c != '\t' && c != '\r' && c != '\n' && (c < 32 || c == '=' || c > 126)) {
            M_textcodec_buffer_add_byte(buf, '=');
            M_textcodec_buffer_add_byte(buf, (unsigned char)("0123456789ABCDEF"[c >> 4]));
            M_textcodec_buffer_add_byte(buf, (unsigned char)("0123456789ABCDEF"[c & 0x0F]));
            cnt += 3;
        } else {
            M_textcodec_buffer_add_byte(buf, (unsigned char)c);
            cnt++;
        }

        if (c == '\n')
            cnt = 0;

        /* Max line lenght is 76. We're going to ensure we never exceed this.
         * Some lines might break earlier due to us not using a look ahead. */
        if (cnt > 72) {
            M_textcodec_buffer_add_byte(buf, '=');
            M_textcodec_buffer_add_bytes(buf, (const unsigned char *)"\r\n", 2);
            cnt = 0;
        }
    }

    return M_TEXTCODEC_ERROR_SUCCESS;
}

M_textcodec_error_t M_textcodec_decode_quoted_printable(M_textcodec_buffer_t *buf, const char *in, M_textcodec_ehandler_t ehandler)
{
    M_parser_t          *parser;
    M_textcodec_error_t  res = M_TEXTCODEC_ERROR_SUCCESS;

    parser = M_parser_create_const((const unsigned char *)in, M_str_len(in), M_PARSER_FLAG_NONE);
    if (parser == NULL)
        return M_TEXTCODEC_ERROR_FAIL;

    do {
        size_t        len;
        unsigned char byte = 0;
        M_int64       i64v;

        M_parser_mark(parser);

        len = M_parser_consume_until(parser, (const unsigned char *)"=", 1, M_FALSE);
        /* Stop precessing if this doesn't start with an = or there is no more escapes. */
        if (!M_parser_peek_byte(parser, &byte) || (byte != '=' && len == 0)) {
            break;
        }

        /* Put everything before the = in the buffer.
         * and advance the parser. */
        M_parser_mark_rewind(parser);
        M_textcodec_buffer_add_bytes(buf, M_parser_peek(parser), len);
        M_parser_consume(parser, len);

        /* Eat the = */
        M_parser_consume(parser, 1);

        if (M_parser_len(parser) < 2) {
            switch (ehandler) {
                case M_TEXTCODEC_EHANDLER_FAIL:
                    res = M_TEXTCODEC_ERROR_FAIL;
                    break;
                case M_TEXTCODEC_EHANDLER_REPLACE:
                    M_textcodec_buffer_add_byte(buf, 0xFF);
                    M_textcodec_buffer_add_byte(buf, 0xFD);
                    /* Fall through. */
                case M_TEXTCODEC_EHANDLER_IGNORE:
                    res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
                    break;
            }
            break;
        }

        if (M_parser_compare_str(parser, "\r\n", 2, M_FALSE)) {
            /* =\r\n is a soft line break. Kill it because we'll put the line back together
             * on the next pass. */
            M_parser_consume(parser, 2);
        } else if (M_parser_read_int(parser, M_PARSER_INTEGER_ASCII, 2, 16, &i64v)) {
            /* Two character hex code converted into a character.
             *
             * The RFC says the characters must be upper case but
             * we're not going to be that strict.
             */
            M_textcodec_buffer_add_byte(buf, (unsigned char)i64v);
        } else {
            /* Not \r\n, and not HH. So... it's a bad sequence.
             * We've removed the = and we'll keep going from this point
             * because it might have been an errant = */
            switch (ehandler) {
                case M_TEXTCODEC_EHANDLER_FAIL:
                    res = M_TEXTCODEC_ERROR_FAIL;
                    break;
                case M_TEXTCODEC_EHANDLER_REPLACE:
                    M_textcodec_buffer_add_byte(buf, 0xFF);
                    M_textcodec_buffer_add_byte(buf, 0xFD);
                    /* Fall through. */
                case M_TEXTCODEC_EHANDLER_IGNORE:
                    res = M_TEXTCODEC_ERROR_SUCCESS_EHANDLER;
                    break;
            }
            if (res == M_TEXTCODEC_ERROR_FAIL) {
                break;
            }
        }
    } while (M_parser_len(parser) > 0);

    /* Add anything remaining to our output. */
    if ((res == M_TEXTCODEC_ERROR_SUCCESS_EHANDLER || res == M_TEXTCODEC_ERROR_SUCCESS) && M_parser_len(parser) > 0)
        M_textcodec_buffer_add_bytes(buf, M_parser_peek(parser), M_parser_len(parser));

    M_parser_destroy(parser);
    return res;
}
