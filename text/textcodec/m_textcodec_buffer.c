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

M_bool M_textcodec_buffer_add_byte(M_textcodec_buffer_t *buf, unsigned char b)
{
    switch (buf->type) {
        case M_TEXTCODEC_BUFFER_TYPE_BUF:
            M_buf_add_byte(buf->u.buf, b);
            return M_TRUE;
        case M_TEXTCODEC_BUFFER_TYPE_PARSER:
            return M_parser_append(buf->u.parser, &b, 1);
    }
    return M_FALSE;
}

M_bool M_textcodec_buffer_add_bytes(M_textcodec_buffer_t *buf, const unsigned char *bs, size_t len)
{
    switch (buf->type) {
        case M_TEXTCODEC_BUFFER_TYPE_BUF:
            M_buf_add_bytes(buf->u.buf, bs, len);
            return M_TRUE;
        case M_TEXTCODEC_BUFFER_TYPE_PARSER:
            return M_parser_append(buf->u.parser, bs, len);
    }
    return M_FALSE;
}

M_bool M_textcodec_buffer_add_str(M_textcodec_buffer_t *buf, const char *s)
{
    switch (buf->type) {
        case M_TEXTCODEC_BUFFER_TYPE_BUF:
            M_buf_add_str(buf->u.buf, s);
            return M_TRUE;
        case M_TEXTCODEC_BUFFER_TYPE_PARSER:
            return M_parser_append(buf->u.parser, (const unsigned char *)s, M_str_len(s));
    }
    return M_FALSE;
}

size_t M_textcodec_buffer_len(M_textcodec_buffer_t *buf)
{
    switch (buf->type) {
        case M_TEXTCODEC_BUFFER_TYPE_BUF:
            return M_buf_len(buf->u.buf);
        case M_TEXTCODEC_BUFFER_TYPE_PARSER:
            return M_parser_len(buf->u.parser);
    }
    return 0;
}
