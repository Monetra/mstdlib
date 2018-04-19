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

static M_http_chunk_t *M_http_chunk_create(void)
{
	M_http_chunk_t *chunk;

	chunk = M_malloc_zero(sizeof(*chunk));
	
	chunk->data       = M_buf_create();
	chunk->trailers   = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	chunk->extensions = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);

	return chunk;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_chunk_destory(M_http_chunk_t *chunk)
{
	if (chunk == NULL)
		return;

	M_buf_cancel(chunk->data);
	M_hash_dict_destroy(chunk->trailers);
	M_hash_dict_destroy(chunk->extensions);

	M_free(chunk);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_clear_chunked(M_http_t *http)
{
	size_t len;
	size_t i;

	if (http == NULL)
		return;

	len = M_list_len(http->chunks);
	for (i=len; i-->0; ) {
		M_list_remove_at(http->chunks, i);
	}

	M_http_set_chunked(http, M_FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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

size_t M_http_chunk_count(const M_http_t *http)
{
}

M_bool M_http_chunk_complete(const M_http_t *http, size_t num)
{
}

void M_http_set_chunk_complete(M_http_t *http, size_t num, M_bool complete)
{
}

M_bool M_http_chunk_len_complete(const M_http_t *http, size_t num)
{
}

void M_http_set_chunk_len_complete(M_http_t *http, size_t num, M_bool complete)
{
}

M_bool M_http_chunk_extensions_complete(const M_http_t *http, size_t num)
{
}

void M_http_set_chunk_extensions_complete(M_http_t *http, size_t num, M_bool complete)
{
}

M_bool M_http_chunk_trailers_complete(const M_http_t *http, size_t num)
{
}

void M_http_set_chunk_trailers_complete(M_http_t *http, size_t num, M_bool complete)
{
}

size_t M_http_chunk_insert(M_http_t *http)
{
	M_http_chunk_t *chunk;

	if (http == NULL)
		return 0;

	chunk = M_http_chunk_create();
	M_list_insert(http->chunks, chunk);

	return M_list_len(http->chunks)-1;
}

void M_http_chunk_remove(M_http_t *http, size_t num)
{
}

size_t M_http_chunk_data_len(const M_http_t *http, size_t num)
{
}

const unsigned char *M_http_chunk_data(const M_http_t *http, size_t num, size_t *len)
{
}

void M_http_set_chunk_data(M_http_t *http, size_t num, const unsigned char *data, size_t len)
{
}

void M_http_chunk_data_append(M_http_t *http, size_t num, const unsigned char *data, size_t len)
{
}

const M_hash_dict_t *M_http_chunk_trailers(const M_http_t *http, size_t num)
{
}

char *M_http_chunk_trailer(const M_http_t *http, size_t num, const char *key)
{
}

void M_http_set_chunk_trailers(M_http_t *http, size_t num, const M_hash_dict_t *headers, M_bool merge)
{
}

void M_http_set_chunk_trailer(M_http_t *http, size_t num, const char *key, const char *val)
{
}

void M_http_add_chunk_trailer(M_http_t *http, size_t num, const char *key, const char *val)
{
}

const M_hash_dict_t *M_http_chunk_extensions(const M_http_t *http, size_t num)
{
}

const char *M_http_chunk_extension_string(const M_http_t *http)
{
}

void M_http_set_chunk_extensions(M_http_t *http, size_t num, const M_hash_dict_t *extensions)
{
}

void M_http_set_chunk_extension(M_http_t *http, size_t num, const char *key, const char *val)
{
}
