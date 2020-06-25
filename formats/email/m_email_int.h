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

#ifndef __M_EMAIL_INT_H__
#define __M_EMAIL_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	HEADER_STATE_END,
	HEADER_STATE_SUCCESS,
	HEADER_STATE_MOREDATA,
	HEADER_STATE_FAIL,
} header_state_t;

typedef M_bool (*M_email_recp_func_t)(const M_email_t *, size_t, char const **, char const **, char const **);
typedef size_t (*M_email_recp_len_func_t)(const M_email_t *);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* return true if attachment. */
M_bool M_email_attachment_parse_info_attachment(const char *val, char **filename);
/* Returns content-type without name entry. */
char *M_email_attachment_parse_info_content_type(const char *val, char **filename);

header_state_t M_email_header_get_next(M_parser_t *parser, char **key, char **val);

char *M_email_write_recipients(const M_email_t *email, M_email_recp_len_func_t recp_len, M_email_recp_func_t recp);

#endif /* __M_EMAIL_INT_H__ */
