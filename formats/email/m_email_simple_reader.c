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


#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_text.h>

#include "email/m_email_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_buf_t                     *collector;
	M_hash_dict                 *headers;
	M_email_t                   *email;
	M_email_simple_read_flags_t  rflags;
} M_email_simple_read_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_simple_read_t *M_email_simple_read_create(M_email_simple_read_flags_t rflags)
{
	M_email_simple_read_t *simple;

	simple            = M_malloc_zero(sizeof(*simple));
	simple->collector = M_buf_create();
	simple->headers   = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP);
	simple->email     = M_email_create();
	simple->rflags    = rflags;

	return simple;
}

static void M_email_simple_read_destroy(M_email_simple_read_t *simple)
{
	if (simple == NULL)
		return;

	M_buf_cancel(simple->collector);
	M_hash_dict_destroy(simple->headers);

	/* Note: simple->email is _not_ destroyed here because it could
 	 *       be a response parameter elsewhere. It must be destroyed
	 *       (if needed) outside of this function. */

	M_free(simple);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_error_t M_email_simple_read(M_email_t **email, const char *data, size_t data_len, M_uint32 flags, size_t *len_read)
{
	M_email_simple_read_t *simple;

struct M_email_reader_callbacks cbs = {
	M_email_simple_read_header_cb;
	M_email_simple_read_to_cb;
	M_email_simple_read_from_cb;
	M_email_simple_read_cc_cb;
	M_email_simple_read_bcc_cb;
	M_email_simple_read_reply_to_cb;
	M_email_simple_read_subject_cb;
	M_email_simple_read_header_done_cb;
	M_email_simple_read_body_cb;
	M_email_simple_read_multipart_preamble_cb;
	M_email_simple_read_multipart_preamble_done_cb;
	M_email_simple_read_multipart_header_cb;
	M_email_simple_read_multipart_header_attachment_cb;
	M_email_simple_read_multipart_header_done_cb;
	M_email_simple_read_multipart_data_cb;
	M_email_simple_read_multipart_data_done_cb;
	M_email_simple_read_multipart_data_finished_cb;
	M_email_simple_read_multipart_epilouge_cb;
};

	simple = M_email_simple_read_create(rflags);
}

M_email_error_t M_email_simple_read_parser(M_email_t **email, M_parser_t *parser, M_uint32 flags)
{
	M_email_error_t res;
	size_t          len_read = 0;

	res = M_email_simple_read(email, M_parser_peek(parser), M_parser_len(parser), flags, &len_read);

	if (res != M_EMAIL_ERROR_MOREDATA)
		M_parser_consume(parser, len_read);

	return res;
}
