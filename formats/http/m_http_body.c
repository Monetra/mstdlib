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
#include <mstdlib/mstdlib_formats.h>
#include "http/m_http_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_set_body_length(M_http_t *http, size_t len)
{
	if (http == NULL)
		return;
	http->have_body_len = M_TRUE;
	http->body_len      = len;
}

size_t M_http_body_length(M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->body_len;
}

size_t M_http_body_length_seen(M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->body_len_seen;
}

M_bool M_http_have_body_length(M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->have_body_len;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_clear_body(M_http_t *http)
{
	if (http == NULL)
		return;

	http->have_body_len = M_FALSE;
	http->body_len      = 0;
	http->body_len_seen = 0;

	M_buf_truncate(http->body, 0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_body_complete(const M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;

	if (!http->have_body_len)
		return M_FALSE;

	if (http->body_len == http->body_len_seen)
		return M_TRUE;
}

const unsigned char *M_http_body(const M_http_t *http, size_t *len)
{
	if (http == NULL || len == NULL)
		return NULL;

	*len = M_buf_len(http->body);
	return (const unsigned char *)M_buf_peek(http->body);
}

void M_http_set_body(M_http_t *http, const unsigned char *data, size_t len)
{
	if (http == NULL || data == NULL || len == 0)
		return;

	M_buf_truncate(http->body, 0);
	M_buf_add_bytes(http->body, data, len);
	http->body_len      = len;
	http->body_len_seen = len;
}

void M_http_body_append(M_http_t *http, const unsigned char *data, size_t len)
{
	if (http == NULL || data == NULL || len == 0)
		return;

	http->body_len_seen += len;
	if (http->body_len_seen > http->body_len)
		http->body_len = http->body_len_seen;
	M_buf_add_bytes(http->body, data, len);
}
