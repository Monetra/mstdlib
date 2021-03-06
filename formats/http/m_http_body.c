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


#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include "http/m_http_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_body_length(M_http_t *http, size_t *len)
{
	size_t mylen;

	if (len == NULL)
		len = &mylen;
	*len = 0;

	if (http == NULL)
		return M_FALSE;

	if (http->have_body_len) {
		*len = http->body_len;
		return M_TRUE;
	}

	return M_FALSE;
}

size_t M_http_body_length_seen(M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->body_len_seen;
}

size_t M_http_body_length_buffered(M_http_t *http)
{
	if (http == NULL)
		return 0;
	return M_buf_len(http->body);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const unsigned char *M_http_body(const M_http_t *http, size_t *len)
{
	if (http == NULL || len == NULL)
		return NULL;

	*len = M_buf_len(http->body);
	return (const unsigned char *)M_buf_peek(http->body);
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

void M_http_body_drop(M_http_t *http, size_t len)
{
	if (http == NULL || len == 0)
		return;
	M_buf_drop(http->body, len);
}
