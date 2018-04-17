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

M_bool M_http_body_complete(const M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->body_complete;
}

void M_http_set_body_complete(M_http_t *http, M_bool complete)
{
	if (http == NULL)
		return;
	http->body_complete = complete;
}

const unsigned char *M_http_body(const M_http_t *http, size_t *len)
{
	if (http == NULL || len == NULL)
		return NULL;

	*len = M_buf_len(http->body);
	return M_buf_peek(http->body)
}

void M_http_set_body(M_http_t *http, const unsigned char *data, size_t len)
{
	if (http == NULL || data == NULL || len == 0)
		return;

	M_buf_truncate(http->body, 0);
	M_buf_add_bytes(http->body, data, len);
}

void M_http_body_append(M_http_t *http, const unsigned char *data, size_t len)
{
	if (http == NULL || data == NULL || len == 0)
		return;

	M_buf_add_bytes(http->body, data, len);
}

M_bool M_http_is_chunked(const M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->chunked;
}

void M_http_set_chunked(M_http_t *http, M_bool chunked)
{
	if (http == NULL)
		return;
	http->chunked = chunked;
}

M_bool M_http_chunk_complete(const M_http_t *http)
{
	if (http == NULL)
		return;
	return http->chunk_complete;
}

void M_http_set_chunk_complete(M_http_t *http, M_bool complete)
{
	if (http == NULL)
		return;
	http->chunk_complete = complete;
}

size_t M_http_chunk_len(const M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->body_len;
}

const unsigned char *M_http_chunk_data(const M_http_t *http, size_t *len)
{
	return M_http_body(http, len);
}

void M_http_set_chunk_data(M_http_t *http, const unsigned char *data, size_t len)
{
	M_http_set_body(http, data, len);
}

void M_http_chunk_data_append(M_http_t *http, const unsigned char *data, size_t len)
{
	M_http_set_body_append(http, data, len);
}

const M_hash_dict_t *M_http_chunk_trailers(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->trailers;
}

const char *M_http_chunk_trailer(const M_http_t *http, const char key)
{
	if (http == NULL)
		return NULL;

	return M_http_header_int(http->trailers, key);
}

void M_http_set_chunk_trailers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge)
{
	if (http == NULL)
		return;

	M_http_set_headers_int(&http->trailer, headers, merge);
}

void M_http_set_chunk_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return;

	M_hash_dict_remove(http->trailers, key);
	M_hash_dict_insert(htpp->trailers, key, val);
}

void M_http_add_chunk_trailer(M_http_t *http, const char *key, const char *val)
{
	if (http == NULL)
		return;

	M_hash_dict_insert(http->trailers, key, val);
}
