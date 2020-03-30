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

typedef enum {
	M_EMAIL_PART_TYPE_UNKNOWN,
	M_EMAIL_PART_TYPE_ATTACHMENT
} M_email_part_type_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_email_reader {
	struct M_email_reader_callbacks  cbs;
	M_email_reader_flags_t           flags;
	void                            *thunk;
	M_state_machine_t               *sm;
	char                            *boundary;
	size_t                           boundary_len;
	M_email_data_format_t            data_format;
	size_t                           part_idx;
	M_email_part_type_t              part_type;
	char                            *part_content_type;
	char                            *part_transfer_encoding;
	char                            *part_filename;
	M_email_error_t                  res;
	M_parser_t                      *parser;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t M_email_reader_header_func_default(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_to_func_default(const char *group, const char *name, const char *address, void *thunk)
{
	(void)group;
	(void)name;
	(void)address;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_from_func_default(const char *group, const char *name, const char *address, void *thunk)
{
	(void)group;
	(void)name;
	(void)address;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_cc_func_default(const char *group, const char *name, const char *address, void *thunk)
{
	(void)group;
	(void)name;
	(void)address;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_bcc_func_default(const char *group, const char *name, const char *address, void *thunk)
{
	(void)group;
	(void)name;
	(void)address;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_reply_to_func_default(const char *group, const char *name, const char *address, void *thunk)
{
	(void)group;
	(void)name;
	(void)address;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_subject_func_default(const char *subject, void *thunk)
{
	(void)subject;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_header_done_func_default(M_email_data_format_t format, void *thunk)
{
	(void)format;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_body_func_default(const char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_preamble_func_default(const char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_preamble_done_func_default(void *thunk)
{
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_header_func_default(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)key;
	(void)val;
	(void)idx;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_header_done_func_default(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_header_attachment_func_default(const char *mime, const char *transfer_encoding, const char *filename, size_t idx, void *thunk)
{
	(void)mime;
	(void)transfer_encoding;
	(void)filename;
	(void)idx;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_data_func_default(const char *data, size_t len, size_t idx, void *thunk)
{
	(void)data;
	(void)len;
	(void)idx;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_data_done_func_default(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_data_finished_func_default(void *thunk)
{
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t M_email_reader_multipart_epilouge_func_default(const char *data, size_t len, void *thunk)
{
	(void)data;
	(void)len;
	(void)thunk;
	return M_EMAIL_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t M_email_header_process_content_type(M_email_reader_t *emailr, const char *val)
{
	M_parser_t    *parser;
	M_buf_t       *buf;
	unsigned char  byte;

	/* Format defaults to BODY.
 	 * We only care about mulipart because data is handled differently. */
	if (M_str_casestr(val, "multipart") == NULL)
		return M_EMAIL_ERROR_SUCCESS;

	emailr->data_format = M_EMAIL_DATA_FORMAT_MULTIPART;

	parser = M_parser_create_const((const unsigned char *)val, M_str_len(val), M_PARSER_FLAG_NONE);
	if (M_parser_consume_str_until(parser, "boundary=", M_FALSE) == 0) {
		M_parser_destroy(parser);
		return M_EMAIL_ERROR_MULTIPART_NOBOUNDARY;
	}
	M_parser_consume(parser, 9 /* "boundary=" */);

	if (M_parser_peek_byte(parser, &byte) && byte == '"')
		M_parser_consume(parser, 1);

	/* Mulipart boundaries are prefixed with -- to signify the start
 	 * of the given boundary. */
	buf = M_buf_create();
	M_buf_add_str(buf, "--");
	M_parser_read_buf_not_charset(parser, buf, (const unsigned char *)";\r\n\"", 4);
	emailr->boundary = M_buf_finish_str(buf, &emailr->boundary_len);
	M_parser_destroy(parser);

	if (M_str_isempty(emailr->boundary))
		return M_EMAIL_ERROR_MULTIPART_NOBOUNDARY;
	return M_EMAIL_ERROR_SUCCESS;
}

static M_bool M_email_header_process(M_email_reader_t *emailr, const char *key, const char *val)
{
	M_email_error_t res;

	res = emailr->cbs.header_func(key, val, emailr->thunk);
	if (res != M_EMAIL_ERROR_SUCCESS) {
		emailr->res = res;
		return M_FALSE;
	}

	if (M_str_caseeq(key, "To")) {
		res = M_email_process_address(val, emailr->cbs.to_func, emailr->thunk);
	} else if (M_str_caseeq(key, "From")) {
		res = M_email_process_address(val, emailr->cbs.from_func, emailr->thunk);
	} else if (M_str_caseeq(key, "CC")) {
		res = M_email_process_address(val, emailr->cbs.cc_func, emailr->thunk);
	} else if (M_str_caseeq(key, "BCC")) {
		res = M_email_process_address(val, emailr->cbs.bcc_func, emailr->thunk);
	} else if (M_str_caseeq(key, "Reply-To")) {
		res = M_email_process_address(val, emailr->cbs.reply_to_func, emailr->thunk);
	} else if (M_str_caseeq(key, "Subject")) {
		res = emailr->cbs.subject_func(val, emailr->thunk);
	} else if (M_str_caseeq(key, "Content-Type")) {
		res = M_email_header_process_content_type(emailr, val);
	}

	if (res != M_EMAIL_ERROR_SUCCESS) {
		emailr->res = res;
		return M_FALSE;
	}

	return M_TRUE;
}

static M_bool M_email_header_process_multipart(M_email_reader_t *emailr, const char *key, const char *val)
{
	char            *myfilename = NULL;
	M_email_error_t  res;

	res = emailr->cbs.multipart_header_func(key, val, emailr->part_idx, emailr->thunk);
	if (res != M_EMAIL_ERROR_SUCCESS) {
		emailr->res = res;
		return M_FALSE;
	}

	/* We only care about additional processing of these headers. */
	if (M_str_caseeq(key, "Content-Transfer-Encoding")) {
		/* Content-Transfer-Encoding: base64 */
		M_free(emailr->part_transfer_encoding);
		emailr->part_transfer_encoding = M_strdup(val);
	} else if (M_str_caseeq(key, "Content-Disposition")) {
		if (M_email_attachment_parse_info_attachment(val, &myfilename)) {
			emailr->part_type = M_EMAIL_PART_TYPE_ATTACHMENT;

			if (!M_str_isempty(myfilename)) {
				M_free(emailr->part_filename);
				emailr->part_filename = myfilename;
				myfilename            = NULL;
			}
		}
	} else if (M_str_caseeq(key, "Content-Type")) {
		M_free(emailr->part_content_type);
		emailr->part_content_type = M_email_attachment_parse_info_content_type(val, &myfilename);

		if (M_str_isempty(emailr->part_filename)) {
			emailr->part_filename = myfilename;
			myfilename            = NULL;
		}
	}

	M_free(myfilename);
	return M_TRUE;
}

static M_email_error_t M_email_header_process_header_done(M_email_reader_t *emailr, M_bool is_multipart)
{
	M_email_error_t res = M_EMAIL_ERROR_SUCCESS;

	if (!is_multipart)
		return emailr->cbs.header_done_func(emailr->data_format, emailr->thunk);

	if (emailr->part_type == M_EMAIL_PART_TYPE_ATTACHMENT)
		res = emailr->cbs.multipart_header_attachment_func(emailr->part_content_type, emailr->part_transfer_encoding, emailr->part_filename, emailr->part_idx, emailr->thunk);

	if (res == M_EMAIL_ERROR_SUCCESS)
		res = emailr->cbs.multipart_header_done_func(emailr->part_idx, emailr->thunk);

	M_free(emailr->part_content_type);
	M_free(emailr->part_transfer_encoding);
	M_free(emailr->part_filename);
	emailr->part_content_type      = NULL;
	emailr->part_transfer_encoding = NULL;
	emailr->part_filename          = NULL;
	emailr->part_type              = M_EMAIL_PART_TYPE_UNKNOWN;

	return res;
}

static M_state_machine_status_t M_email_header_process_headers(M_email_reader_t *emailr, M_bool is_multipart)
{
	M_email_error_t res;
	header_state_t  hsres;
	M_bool          process_success = M_TRUE;

	if (M_parser_len(emailr->parser) == 0)
		return M_STATE_MACHINE_STATUS_WAIT;

	do {
		char *key = NULL;
		char *val = NULL;

		hsres = M_email_header_get_next(emailr->parser, &key, &val);
		switch (hsres) {
			case HEADER_STATE_FAIL:
				emailr->res = M_EMAIL_ERROR_HEADER_INVALID;
			case HEADER_STATE_END:
			case HEADER_STATE_MOREDATA:
				goto end_of_header;
			case HEADER_STATE_SUCCESS:
				break;
		}

		if (is_multipart) {
			process_success = M_email_header_process_multipart(emailr, key, val);
		} else {
			process_success = M_email_header_process(emailr, key, val);
		}

		M_free(key);
		M_free(val);
	} while (M_parser_len(emailr->parser) != 0 && process_success);

end_of_header:
	if (hsres != HEADER_STATE_END)
		return M_STATE_MACHINE_STATUS_WAIT;

	res = M_email_header_process_header_done(emailr, is_multipart);
	if (res != M_EMAIL_ERROR_SUCCESS)
		emailr->res = res;

	return M_STATE_MACHINE_STATUS_NEXT;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	STATE_START = 1,
	STATE_HEADER,
	STATE_BODY,
	STATE_MULTIPART_PREAMBLE,
	STATE_MULTIPART_HEADER,
	STATE_MULTIPART_DATA,
	STATE_MULTIPART_CHECK_END,
	STATE_MULTIPART_EPILOUGE
} state_ids;

static M_state_machine_status_t state_start(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr = data;

	(void)next;

	/* We want to consume any and all new lines that might start the data. */
	M_parser_consume_whitespace(emailr->parser, M_PARSER_WHITESPACE_NONE);

	/* No data following, we need more. Maybe there is more whitespace
	 * following we need to eat. */
	if (M_parser_len(emailr->parser) == 0)
		return M_STATE_MACHINE_STATUS_WAIT;

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_header(void *data, M_uint64 *next)
{
	(void)next;
	return M_email_header_process_headers((M_email_reader_t *)data, M_FALSE);
}

static M_state_machine_status_t state_body(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr = data;

	if (emailr->data_format != M_EMAIL_DATA_FORMAT_BODY) {
		*next = STATE_MULTIPART_PREAMBLE;
		return M_STATE_MACHINE_STATUS_NEXT;
	}

	if (M_parser_len(emailr->parser) == 0)
		return M_STATE_MACHINE_STATUS_WAIT;

	emailr->res = emailr->cbs.body_func((const char *)M_parser_peek(emailr->parser), M_parser_len(emailr->parser), emailr->thunk);
	if (emailr->res == M_EMAIL_ERROR_SUCCESS)
		M_parser_consume(emailr->parser, M_parser_len(emailr->parser));
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t state_multipart_preamble(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr    = data;
	M_email_error_t   res;
	size_t            consume_len;
	size_t            data_len;
	M_bool            found     = M_FALSE;
	M_bool            full_read = M_FALSE;

	(void)next;

	if (M_parser_len(emailr->parser) == 0)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* Pull off all data before the first boundary. */
	M_parser_mark(emailr->parser);
	data_len    = M_parser_consume_boundary(emailr->parser, (const unsigned char *)emailr->boundary, emailr->boundary_len, M_FALSE, &found);
	consume_len = data_len;
	if (found && M_parser_len(emailr->parser) >= emailr->boundary_len + 2) {
		/* Eat the boundary. */
		M_parser_consume(emailr->parser, emailr->boundary_len);

		if (M_parser_compare_str(emailr->parser, "--", 2, M_FALSE)) {
			/* Check for an ending boundary to check which shouldn't be here. */
			M_parser_mark_rewind(emailr->parser);
			emailr->res = M_EMAIL_ERROR_MULTIPART_MISSING_DATA;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		} else if (M_parser_compare_str(emailr->parser, "\r\n", 2, M_FALSE)) {
			/* End the line end. */
			M_parser_consume(emailr->parser, 2);
		} else {
			/* We have a boundary existing in data. */
			M_parser_mark_rewind(emailr->parser);
			emailr->res = M_EMAIL_ERROR_MULTIPART_INVALID;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}

		full_read   = M_TRUE;
		consume_len = M_parser_mark_len(emailr->parser);
	}
	M_parser_mark_rewind(emailr->parser);

	/* The data before the boundary should end with a \r\n. The only time it
 	 * doesn't is if there is no preamble. The \r\n is not part of the data. */
	if (data_len == 1) {
		M_parser_mark_rewind(emailr->parser);
		emailr->res = M_EMAIL_ERROR_MULTIPART_INVALID;
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	} else if (data_len >= 2) {
		M_parser_mark(emailr->parser);
		M_parser_consume(emailr->parser, data_len-2);
		if (!M_parser_compare_str(emailr->parser, "\r\n", 2, M_FALSE)) {
			emailr->res = M_EMAIL_ERROR_MULTIPART_INVALID;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		data_len -= 2;
		M_parser_mark_rewind(emailr->parser);
	}

	if (data_len != 0) {
		res = emailr->cbs.multipart_preamble_func((const char *)M_parser_peek(emailr->parser), data_len, emailr->thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			emailr->res = res;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
	}
	M_parser_consume(emailr->parser, consume_len);

	if (full_read) {
		res = emailr->cbs.multipart_preamble_done_func(emailr->thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			emailr->res = res;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t state_multipart_header(void *data, M_uint64 *next)
{
	(void)next;
	return M_email_header_process_headers((M_email_reader_t *)data, M_TRUE);
}

static M_state_machine_status_t state_multipart_data(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr  = data;
	size_t            consume_len;
	size_t            data_len;
	M_email_error_t   res     = M_EMAIL_ERROR_SUCCESS;
	M_bool            found   = M_FALSE;

	(void)next;

	if (M_parser_len(emailr->parser) == 0)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* Find all the data before the boundary. */
	M_parser_mark(emailr->parser);
	consume_len = M_parser_consume_boundary(emailr->parser, (unsigned char *)emailr->boundary, emailr->boundary_len, M_FALSE, &found); 
	data_len    = consume_len;
	M_parser_mark_rewind(emailr->parser);

	/* The data and boundary are separated by a \r\n which is not part of the data.
	 *
	 * While we should treat a missing \r\n as an error, we're going to be lenient and allow it. */
	if (consume_len >= 2) {
		M_parser_mark(emailr->parser);
		M_parser_consume(emailr->parser, consume_len-2);
		if (M_parser_compare_str(emailr->parser, "\r\n", 2, M_FALSE)) {
			data_len -= 2;
		}
		M_parser_mark_rewind(emailr->parser);
	}

	if (data_len != 0) {
		res = emailr->cbs.multipart_data_func((const char *)M_parser_peek(emailr->parser), data_len, emailr->part_idx, emailr->thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			emailr->res = res;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
	}
	M_parser_consume(emailr->parser, consume_len);

	if (found) {
		/* Eat the boundary. */
		M_parser_consume(emailr->parser, emailr->boundary_len);

		res = emailr->cbs.multipart_data_done_func(emailr->part_idx, emailr->thunk);
		if (res != M_EMAIL_ERROR_SUCCESS) {
			emailr->res = res;
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}

		emailr->part_idx++;
		return M_STATE_MACHINE_STATUS_NEXT;
	}
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_status_t state_multipart_check_end(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr = data;
	M_email_error_t   res    = M_EMAIL_ERROR_SUCCESS;

	if (M_parser_len(emailr->parser) < 2)
		return M_STATE_MACHINE_STATUS_WAIT;

	if (M_parser_compare_str(emailr->parser, "--", 2, M_FALSE)) {
		M_parser_consume(emailr->parser, 2);
		*next = STATE_MULTIPART_EPILOUGE;
	} else if (!M_parser_compare_str(emailr->parser, "\r\n", 2, M_FALSE)) {
		emailr->res = M_EMAIL_ERROR_MULTIPART_INVALID;
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	} else {
		*next = STATE_MULTIPART_HEADER;
	}
	M_parser_consume(emailr->parser, 2);

	res = emailr->cbs.multipart_data_finished_func(emailr->thunk);
	if (res != M_EMAIL_ERROR_SUCCESS) {
		emailr->res = res;
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_multipart_epilouge(void *data, M_uint64 *next)
{
	M_email_reader_t *emailr = data;

	(void)next;

	if (M_parser_len(emailr->parser) == 0) {
		emailr->res = M_EMAIL_ERROR_SUCCESS;
		return M_STATE_MACHINE_STATUS_NEXT;
	}

	emailr->res = emailr->cbs.multipart_epilouge_func((const char *)M_parser_peek(emailr->parser), M_parser_len(emailr->parser), emailr->thunk);
	if (emailr->res == M_EMAIL_ERROR_SUCCESS)
		M_parser_consume(emailr->parser, M_parser_len(emailr->parser));
	return M_STATE_MACHINE_STATUS_WAIT;
}

static M_state_machine_t *M_email_reader_create_sm(void)
{
	M_state_machine_t *sm;

	sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);

	M_state_machine_insert_state(sm, STATE_START, 0, NULL, state_start, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_HEADER, 0, NULL, state_header, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_BODY, 0, NULL, state_body, NULL, NULL);

	M_state_machine_insert_state(sm, STATE_MULTIPART_PREAMBLE, 0, NULL, state_multipart_preamble, NULL, NULL);

	M_state_machine_insert_state(sm, STATE_MULTIPART_HEADER, 0, NULL, state_multipart_header, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_MULTIPART_DATA, 0, NULL, state_multipart_data, NULL, NULL);
	M_state_machine_insert_state(sm, STATE_MULTIPART_CHECK_END, 0, NULL, state_multipart_check_end, NULL, NULL);

	M_state_machine_insert_state(sm, STATE_MULTIPART_EPILOUGE, 0, NULL, state_multipart_epilouge, NULL, NULL);

	return sm;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_error_t M_email_reader_read(M_email_reader_t *emailr, const char *data, size_t data_len, size_t *len_read)
{
	size_t mylen_read;

	if (len_read == NULL)
		len_read = &mylen_read;
	*len_read = 0;

	if (emailr == NULL || data == NULL || data_len == 0)
		return M_EMAIL_ERROR_INVALIDUSE;

	emailr->parser = M_parser_create_const((const unsigned char *)data, data_len, M_PARSER_FLAG_NONE);
	emailr->res    = M_EMAIL_ERROR_MOREDATA;

	M_state_machine_run(emailr->sm, emailr);

	*len_read = data_len - M_parser_len(emailr->parser);
	M_parser_destroy(emailr->parser);
	emailr->parser = NULL;
	return emailr->res;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_email_reader_t *M_email_reader_create(struct M_email_reader_callbacks *cbs, M_uint32 flags, void *thunk)
{
	M_email_reader_t *emailr;

	emailr              = M_malloc_zero(sizeof(*emailr));
	emailr->flags       = flags;
	emailr->thunk       = thunk;
	emailr->data_format = M_EMAIL_DATA_FORMAT_BODY;
	emailr->sm          = M_email_reader_create_sm();

	emailr->cbs.header_func                      = M_email_reader_header_func_default;
	emailr->cbs.to_func                          = M_email_reader_to_func_default;
	emailr->cbs.from_func                        = M_email_reader_from_func_default;
	emailr->cbs.cc_func                          = M_email_reader_cc_func_default;
	emailr->cbs.bcc_func                         = M_email_reader_bcc_func_default;
	emailr->cbs.reply_to_func                    = M_email_reader_reply_to_func_default;
	emailr->cbs.subject_func                     = M_email_reader_subject_func_default;
	emailr->cbs.header_done_func                 = M_email_reader_header_done_func_default;
	emailr->cbs.body_func                        = M_email_reader_body_func_default;
	emailr->cbs.multipart_preamble_func          = M_email_reader_multipart_preamble_func_default;
	emailr->cbs.multipart_preamble_done_func     = M_email_reader_multipart_preamble_done_func_default;
	emailr->cbs.multipart_header_func            = M_email_reader_multipart_header_func_default;
	emailr->cbs.multipart_header_attachment_func = M_email_reader_multipart_header_attachment_func_default;
	emailr->cbs.multipart_header_done_func       = M_email_reader_multipart_header_done_func_default;
	emailr->cbs.multipart_data_func              = M_email_reader_multipart_data_func_default;
	emailr->cbs.multipart_data_done_func         = M_email_reader_multipart_data_done_func_default;
	emailr->cbs.multipart_data_finished_func     = M_email_reader_multipart_data_finished_func_default;
	emailr->cbs.multipart_epilouge_func          = M_email_reader_multipart_epilouge_func_default;

	if (cbs != NULL) {
		if (cbs->header_func                      != NULL) emailr->cbs.header_func                      = cbs->header_func;
		if (cbs->to_func                          != NULL) emailr->cbs.to_func                          = cbs->to_func;
		if (cbs->from_func                        != NULL) emailr->cbs.from_func                        = cbs->from_func;
		if (cbs->cc_func                          != NULL) emailr->cbs.cc_func                          = cbs->cc_func;
		if (cbs->bcc_func                         != NULL) emailr->cbs.bcc_func                         = cbs->bcc_func;
		if (cbs->reply_to_func                    != NULL) emailr->cbs.reply_to_func                    = cbs->reply_to_func;
		if (cbs->subject_func                     != NULL) emailr->cbs.subject_func                     = cbs->subject_func;
		if (cbs->header_done_func                 != NULL) emailr->cbs.header_done_func                 = cbs->header_done_func;
		if (cbs->body_func                        != NULL) emailr->cbs.body_func                        = cbs->body_func;
		if (cbs->multipart_preamble_func          != NULL) emailr->cbs.multipart_preamble_func          = cbs->multipart_preamble_func;
		if (cbs->multipart_preamble_done_func     != NULL) emailr->cbs.multipart_preamble_done_func     = cbs->multipart_preamble_done_func;
		if (cbs->multipart_header_func            != NULL) emailr->cbs.multipart_header_func            = cbs->multipart_header_func;
		if (cbs->multipart_header_attachment_func != NULL) emailr->cbs.multipart_header_attachment_func = cbs->multipart_header_attachment_func;
		if (cbs->multipart_header_done_func       != NULL) emailr->cbs.multipart_header_done_func       = cbs->multipart_header_done_func;
		if (cbs->multipart_data_func              != NULL) emailr->cbs.multipart_data_func              = cbs->multipart_data_func;
		if (cbs->multipart_data_done_func         != NULL) emailr->cbs.multipart_data_done_func         = cbs->multipart_data_done_func;
		if (cbs->multipart_data_finished_func     != NULL) emailr->cbs.multipart_data_finished_func     = cbs->multipart_data_finished_func;
		if (cbs->multipart_epilouge_func          != NULL) emailr->cbs.multipart_epilouge_func          = cbs->multipart_epilouge_func;
	}

	return emailr;
}

void M_email_reader_destroy(M_email_reader_t *emailr)
{
	if (emailr == NULL)
		return;
	M_state_machine_destroy(emailr->sm);
	M_free(emailr->boundary);
	M_free(emailr->part_content_type);
	M_free(emailr->part_transfer_encoding);
	M_free(emailr->part_filename);
	M_free(emailr);
}
