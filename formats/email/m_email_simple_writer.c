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


#include "m_config.h"

#include "email/m_email_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const size_t LINE_LEN = 78;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_email_has_attachment(const M_email_t *email)
{
    size_t i;

    if (M_email_is_mixed_multipart(email))
        return M_TRUE;

    for (i=0; i<M_email_parts_len(email); i++) {
        if (M_email_part_is_attachmenet(email, i))
            return M_TRUE;
    }
    return M_FALSE;
}


static char *M_email_write_gen_boundary(void)
{
    M_rand_t *r;
    M_buf_t  *buf;
    size_t    num;
    size_t    i;

    r   = M_rand_create(0);
    buf = M_buf_create();

    M_buf_add_fill(buf, '-', 12);
    for (i=0; i<28; i++) {
        do {
            num = 48 + M_rand(r) % 74;
        } while ((num >= 58 && num <= 63) || (num >= 92 && num <= 96));

        M_buf_add_byte(buf, (unsigned char)num);
    }

    M_rand_destroy(r);
    return M_buf_finish_str(buf, NULL);
}

static void M_email_add_header_entry(M_buf_t *buf, const char *key, const char *val)
{
    M_parser_t *parser;
    size_t      len    = 0;
    size_t      plen   = 0;

    if (M_str_isempty(val))
        return;

    /* 78 character recommended line length limit (true max if 998). */
    if (M_str_len(key) + 2 + M_str_len(val) <= LINE_LEN) {
        M_buf_add_str(buf, key);
        M_buf_add_str(buf, ": ");
        M_buf_add_str(buf, val);
        M_buf_add_str(buf, "\r\n");
        return;
    }

    /* Over recommend length so we need to do folding. */
    parser = M_parser_create(M_PARSER_FLAG_NONE);
    M_parser_append(parser, (const unsigned char *)key, M_str_len(key));
    M_parser_append(parser, (const unsigned char *)": ", 2);
    M_parser_append(parser, (const unsigned char *)val, M_str_len(val));

    /* Eat any starting whitepace because it's not necessary. */
    M_parser_consume_whitespace(parser, M_PARSER_WHITESPACE_NONE);

    /* Lines are only broken on whitespace. This could cause some lines
     * to be longer than the max length but we can't break in the middle
     * of a continous string. */
    M_parser_mark(parser);
    while (M_parser_consume_until(parser, (const unsigned char *)" \t", 2, M_FALSE) > 0) {
        plen = M_parser_mark_len(parser);
        /* Eat the whitespace so the next iteration will start after.
         * We don't want it included in our length because if we have
         * to back up and split we want the space to start the next line. */
        M_parser_consume(parser, 1);

        /* Store the last length shorter than lenth. */
        if (plen < LINE_LEN) {
            len = plen;
            continue;
        } else if (len == 0) {
            /* We never found a space to break on shorter thean the max line length.
             * We have to split on plen. */
            len = plen;
        }

        /* The parser exceeded the line len. */
        M_parser_mark_rewind(parser);
        M_parser_read_buf(parser, buf, len);
        M_buf_add_str(buf, "\r\n");

        M_parser_mark(parser);
        len = 0;
        /* Consume any starting whitespace. It will start our next line
         * but we don't want it to stop our iteration. */
        M_parser_consume_whitespace(parser, M_PARSER_WHITESPACE_NONE);
    }

    M_parser_mark_rewind(parser);
    len = M_parser_len(parser);
    if (len > 0) {
        M_parser_read_buf(parser, buf, len);
        M_buf_add_str(buf, "\r\n");
    }

    M_parser_destroy(parser);
    return;
}

static M_bool M_email_simple_write_add_single_address(const M_email_t *email, M_buf_t *buf, const char *key,
        M_bool (*addr_data)(const M_email_t *, char const **, char const **, char const **))
{
    char       *full;
    const char *group;
    const char *name;
    const char *address;

    addr_data(email, &group, &name, &address);
    full = M_email_write_single_recipient(group, name, address);
    if (M_str_isempty(full)) {
        M_free(full);
        return M_FALSE;
    }

    M_email_add_header_entry(buf, key, full);
    M_free(full);
    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers_from(const M_email_t *email, M_buf_t *buf)
{
    return M_email_simple_write_add_single_address(email, buf, "From", M_email_from);
}

static M_bool M_email_simple_write_add_headers_reply_to(const M_email_t *email, M_buf_t *buf)
{
    /* Not checking return because Reply-To is optional. So, if this fails
     * because we don't have any data, that's fine. */
    M_email_simple_write_add_single_address(email, buf, "Reply-To", M_email_reply_to);
    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers_dict(const M_email_t *email, M_buf_t *buf)
{
    const M_hash_dict_t *headers;
    M_hash_dict_enum_t  *he;
    const char          *key;
    const char          *val;

    headers = M_email_headers(email);
    M_hash_dict_enumerate(headers, &he);
    while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
        if (M_str_caseeq(key, "Content-Type")) {
            /* We want to control the content type value. */
            continue;
        }
        M_email_add_header_entry(buf, key, val);
    }
    M_hash_dict_enumerate_free(he);

    return M_TRUE;
}

static M_bool M_email_simple_write_recipients(const M_email_t *email, M_buf_t *buf, const char *key,
        M_email_recp_len_func_t recp_len, M_email_recp_func_t recp)
{
    char *out;

    out = M_email_write_recipients(email, recp_len, recp);
    if (out == NULL)
        return M_TRUE;

    M_email_add_header_entry(buf, key, out);
    M_free(out);

    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers_recipients(const M_email_t *email, M_buf_t *buf)
{
    if (!M_email_simple_write_recipients(email, buf, "To", M_email_to_len, M_email_to))
        return M_FALSE;

    if (!M_email_simple_write_recipients(email, buf, "CC", M_email_cc_len, M_email_cc))
        return M_FALSE;

    if (!M_email_simple_write_recipients(email, buf, "BCC", M_email_bcc_len, M_email_bcc))
        return M_FALSE;

    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers_content_type(M_buf_t *buf, const char *boundary, M_bool is_mixed)
{
    M_buf_t *mbuf;
    char    *out;

    mbuf = M_buf_create();
    M_buf_add_str(mbuf, "multipart/");
    if (is_mixed) {
        M_buf_add_str(mbuf, "mixed");
    } else {
        M_buf_add_str(mbuf, "alternative");
    }
    M_buf_add_str(mbuf, "; boundary=\"");
    M_buf_add_str(mbuf, boundary);
    M_buf_add_byte(mbuf, '\"');

    out = M_buf_finish_str(mbuf, NULL);
    M_email_add_header_entry(buf, "Content-Type", out);
    M_free(out);

    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers_subject(const M_email_t *email, M_buf_t *buf)
{
    M_email_add_header_entry(buf, "Subject", M_email_subject(email));
    return M_TRUE;
}

static M_bool M_email_simple_write_add_headers(const M_email_t *email, M_buf_t *buf, const char *boundary)
{
    if (!M_email_simple_write_add_headers_from(email, buf))
        return M_FALSE;

    if (!M_email_simple_write_add_headers_reply_to(email, buf))
        return M_FALSE;

    if (!M_email_simple_write_add_headers_dict(email, buf))
        return M_FALSE;

    if (!M_email_simple_write_add_headers_recipients(email, buf))
        return M_FALSE;

    if (!M_email_simple_write_add_headers_content_type(buf, boundary, M_email_has_attachment(email)))
        return M_FALSE;

    if (!M_email_simple_write_add_headers_subject(email, buf))
        return M_FALSE;

    M_buf_add_str(buf, "\r\n");

    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_email_simple_write_add_preamble(const M_email_t *email, M_buf_t *buf)
{
    const char *const_temp;

    const_temp = M_email_preamble(email);
    if (M_str_isempty(const_temp))
        return M_TRUE;

    M_buf_add_str(buf, const_temp);
    M_buf_add_str(buf, "\r\n");

    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_email_simple_write_add_parts(const M_email_t *email, M_buf_t *buf, const char *boundary, const char *sub_boundary, M_bool is_processing_attachments)
{
    size_t len;
    size_t i;

    len = M_email_parts_len(email);
    for (i=0; i<len; i++) {
        const M_hash_dict_t *headers;
        M_hash_dict_enum_t  *he;
        const char          *key;
        const char          *val;

        if (M_email_part_is_attachmenet(email, i) != is_processing_attachments)
            continue;

        /* Add the boundary. */
        M_buf_add_str(buf, "--");
        if (is_processing_attachments || i == 0) {
            M_buf_add_str(buf, boundary);
        } else {
            M_buf_add_str(buf, sub_boundary);
        }
        M_buf_add_str(buf, "\r\n");

        /* Add the headers. */
        headers = M_email_part_headers(email, i);
        M_hash_dict_enumerate(headers, &he);
        while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
            if (i == 0 && M_str_caseeq(key, "Content-Type") && M_str_casestr(val, "multipart") != NULL) {
                if (boundary != sub_boundary) {
                    M_email_simple_write_add_headers_content_type(buf, sub_boundary, M_FALSE);
                } else {
                    /* This assumes that someone added an unecessary multipart entry in the first entry.
                        * We will snub it out to avoid ambiguous nested boundaries */
                    M_buf_add_str(buf, "Content-Length: 0\r\n");
                }
                continue;
            }
            M_email_add_header_entry(buf, key, val);
        }
        M_hash_dict_enumerate_free(he);

        /* Add the attachment info. */
        if (M_email_part_is_attachmenet(email, i)) {
            const char *content_type      = NULL;
            const char *transfer_encoding = NULL;
            const char *filename          = NULL;
            M_buf_t    *mbuf;
            char       *out;

            M_email_part_attachment_info(email, i, &content_type, &transfer_encoding, &filename);

            if (!M_str_isempty(content_type)) {
                if (M_str_isempty(filename)) {
                    M_email_add_header_entry(buf, "Content-Type", content_type);
                } else {
                    mbuf = M_buf_create();
                    M_buf_add_str(mbuf, content_type);
                    M_buf_add_str(mbuf, "; ");
                    M_buf_add_str(mbuf, "name=\"");
                    M_buf_add_str(mbuf, filename);
                    M_buf_add_byte(mbuf, '\"');

                    out = M_buf_finish_str(mbuf, NULL);
                    M_email_add_header_entry(buf, "Content-Type", out);
                    M_free(out);
                }
            }

            mbuf = M_buf_create();
            M_buf_add_str(mbuf, "attachment");
            if (!M_str_isempty(filename)) {
                M_buf_add_str(mbuf, "; filename=\"");
                M_buf_add_str(mbuf, filename);
                M_buf_add_byte(mbuf, '\"');
            }
            out  = M_buf_finish_str(mbuf, NULL);
            M_email_add_header_entry(buf, "Content-Disposition", out);
            M_free(out);

            if (!M_str_isempty(transfer_encoding)) {
                M_email_add_header_entry(buf, "Content-Transfer-Encoding", transfer_encoding);
            }
        }

        /* End of header marker. */
        M_buf_add_str(buf, "\r\n");

        /* Add the content. */
        M_buf_add_str(buf, M_email_part_data(email, i));
        M_buf_add_str(buf, "\r\n");

    }
    /* If there are no parts we need an empty one. */
    if (len == 0) {
        M_buf_add_str(buf, "--");
        M_buf_add_str(buf, boundary);
        M_buf_add_str(buf, "\r\n");
        M_buf_add_str(buf, "\r\n");
    }

    /* Ending boundary. */
    M_buf_add_str(buf, "--");
    if (is_processing_attachments) {
        M_buf_add_str(buf, boundary);
        M_buf_add_str(buf, "--");
    } else {
        M_buf_add_str(buf, sub_boundary);
        M_buf_add_str(buf, "--\r\n\r\n");
    }

    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_email_simple_write_add_epilouge(const M_email_t *email, M_buf_t *buf)
{
    const char *const_temp;

    const_temp = M_email_epilouge(email);
    if (M_str_isempty(const_temp))
        return M_TRUE;

    M_buf_add_str(buf, "\r\n");
    M_buf_add_str(buf, const_temp);

    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_email_simple_write(const M_email_t *email)
{
    M_buf_t *buf;

    if (email == NULL)
        return NULL;

    buf = M_buf_create();
    if (!M_email_simple_write_buf(email, buf)) {
        M_buf_cancel(buf);
        return NULL;
    }

    return M_buf_finish_str(buf, NULL);
}

M_bool M_email_simple_write_buf(const M_email_t *email, M_buf_t *buf)
{
    char   *boundary     = NULL;
    char   *sub_boundary = NULL;
    size_t  start_len;

    if (email == NULL || buf == NULL)
        return M_FALSE;

    start_len    = M_buf_len(buf);
    boundary     = M_email_write_gen_boundary();

    if (!M_email_simple_write_add_headers(email, buf, boundary))
        goto err;

    if (!M_email_simple_write_add_preamble(email, buf))
        goto err;

    if (M_email_has_attachment(email)) {
        sub_boundary = M_email_write_gen_boundary();
        if (!M_email_simple_write_add_parts(email, buf, boundary, sub_boundary, M_FALSE))
            goto err;

        if (!M_email_simple_write_add_parts(email, buf, boundary, NULL, M_TRUE))
            goto err;
    } else {
        if (!M_email_simple_write_add_parts(email, buf, boundary, boundary, M_FALSE))
            goto err;
    }

    if (!M_email_simple_write_add_epilouge(email, buf))
        goto err;

    M_free(boundary);
    M_free(sub_boundary);
    return M_TRUE;

err:
    M_free(boundary);
    M_free(sub_boundary);
    M_buf_truncate(buf, start_len);
    return M_FALSE;
}
