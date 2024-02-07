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

typedef struct {
    M_buf_t                     *collector;
    M_hash_dict_t               *headers;
    M_email_t                   *email;  /* Managed outside of the object create/destroy. */
    M_bool                       is_attachment;
    M_email_simple_read_flags_t  rflags;
} M_email_simple_read_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t M_email_simple_read_header_cb(const char *key, const char *val, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    M_email_headers_insert(simple->email, key, val);
    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_body_cb(const char *data, size_t len, void *thunk)
{
    M_email_simple_read_t *simple      = thunk;
    const char            *const_temp;
    M_hash_dict_t         *headers     = NULL;
    size_t                 idx         = 0;

    /* Move the content type header to the part we're going to create because
     * reader always create a multi part even when there is only one. The
     * main header will need to change to be multi part when reassembled. */
    if (M_hash_dict_get(M_email_headers(simple->email), "Content-Type", &const_temp)) {
        headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_STRVP_KEYS_ORDERED);
        M_hash_dict_insert(headers, "Content-Type", const_temp);
        M_email_headers_remove(simple->email, "Content-Type");
    }

    M_email_part_append(simple->email, data, len, headers, &idx);
    M_hash_dict_destroy(headers);

    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_preamble_cb(const char *data, size_t len, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    M_buf_add_bytes(simple->collector, data, len);
    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_preamble_done_cb(void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    M_email_set_preamble(simple->email, M_buf_peek(simple->collector), M_buf_len(simple->collector));
    M_buf_truncate(simple->collector, 0);

    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_header_cb(const char *key, const char *val, size_t idx, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    (void)idx;

    /* Collect the headers. */
    M_hash_dict_insert(simple->headers, key, val);

    if (M_str_caseeq(key, "Content-Type") && M_str_str(val, "oundary"))
        M_email_set_mixed_multipart(simple->email, M_TRUE);

    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_header_done_cb(size_t idx, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    (void)idx;

    if (!simple->is_attachment) {
        if (!M_email_part_append(simple->email, NULL, 0, simple->headers, NULL)) {
            return M_EMAIL_ERROR_MULTIPART_HEADER_INVALID;
        }
    }

    M_hash_dict_destroy(simple->headers);
    simple->headers       = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);
    simple->is_attachment = M_FALSE;

    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_data_cb(const char *data, size_t len, size_t idx, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    (void)idx;

    M_buf_add_bytes(simple->collector, data, len);
    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_data_done_cb(size_t idx, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    M_email_part_set_data(simple->email, idx, M_buf_peek(simple->collector), M_buf_len(simple->collector));
    M_buf_truncate(simple->collector, 0);

    return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_simple_read_multipart_epilouge_cb(const char *data, size_t len, void *thunk)
{
    M_email_simple_read_t *simple = thunk;

    M_email_set_epilouge(simple->email, data, len);
    return M_EMAIL_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_simple_read_t *M_email_simple_read_create(M_email_simple_read_flags_t rflags)
{
    M_email_simple_read_t *simple;

    simple            = M_malloc_zero(sizeof(*simple));
    simple->collector = M_buf_create();
    simple->headers   = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);
    simple->rflags    = rflags;

    return simple;
}

static void M_email_simple_read_destroy(M_email_simple_read_t *simple)
{
    if (simple == NULL)
        return;

    M_buf_cancel(simple->collector);
    M_hash_dict_destroy(simple->headers);

    M_free(simple);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_error_t M_email_simple_read(M_email_t **email, const char *data, size_t data_len, M_uint32 flags, size_t *len_read)
{
    M_email_simple_read_t           *simple;
    M_email_reader_t                *reader;
    M_email_t                       *em         = NULL;
    M_email_error_t                  res;
    size_t                           mylen_read;
    M_bool                           have_email = M_TRUE;
    struct M_email_reader_callbacks  cbs = {
        M_email_simple_read_header_cb,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        M_email_simple_read_body_cb,
        M_email_simple_read_multipart_preamble_cb,
        M_email_simple_read_multipart_preamble_done_cb,
        M_email_simple_read_multipart_header_cb,
        NULL,
        M_email_simple_read_multipart_header_done_cb,
        M_email_simple_read_multipart_data_cb,
        M_email_simple_read_multipart_data_done_cb,
        NULL,
        M_email_simple_read_multipart_epilouge_cb
    };

    if (len_read == NULL)
        len_read = &mylen_read;
    *len_read = 0;

    if (email == NULL) {
        have_email = M_FALSE;
        email      = &em;
    }

    if (data == NULL || data_len == 0)
        return M_EMAIL_ERROR_MOREDATA;

    *email        = M_email_create();
    simple        = M_email_simple_read_create(flags);
    simple->email = *email;
    reader        = M_email_reader_create(&cbs, M_EMAIL_READER_NONE, simple);
    if (reader == NULL) {
        res = M_EMAIL_ERROR_INVALIDUSE;
        goto done;
    }

    res = M_email_reader_read(reader, data, data_len, len_read);

done:
    M_email_reader_destroy(reader);
    M_email_simple_read_destroy(simple);
    if (res != M_EMAIL_ERROR_SUCCESS || !have_email) {
        M_email_destroy(*email);
        *email = NULL;
    }

    return res;
}

M_email_error_t M_email_simple_read_parser(M_email_t **email, M_parser_t *parser, M_uint32 flags)
{
    M_email_error_t res;
    size_t          len_read = 0;

    res = M_email_simple_read(email, (const char *)M_parser_peek(parser), M_parser_len(parser), flags, &len_read);

    if (res != M_EMAIL_ERROR_MOREDATA)
        M_parser_consume(parser, len_read);

    return res;
}
