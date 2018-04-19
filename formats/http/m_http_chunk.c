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
	
	chunk->body       = M_buf_create();
	chunk->extensions = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);

	return chunk;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_chunk_destory(M_http_chunk_t *chunk)
{
	if (chunk == NULL)
		return;

	M_buf_cancel(chunk->body);
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
	if (http == NULL)
		return 0;
	return M_list_len(http->chunks);
}

M_bool M_http_chunk_complete(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return M_FALSE;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return M_FALSE;

	if (chunk->extensions_complete   &&
			chunk->have_body_len     &&
			((chunk->body != 0 && chunk->body_len == chunk->body_len_seen) ||
			 (chunk->body_len == 0 && http->trailers_complete)))
	{
		return M_TRUE;
	}

	return M_FALSE;
}

M_bool M_http_chunk_len_complete(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return M_FALSE;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return M_FALSE;

	return chunk->have_body_len;
}

void M_http_set_chunk_len_complete(M_http_t *http, size_t num, M_bool complete)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	chunk->have_body_len = complete;
}

M_bool M_http_chunk_extensions_complete(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return M_FALSE;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return M_FALSE;

	return chunk->extensions_complete;
}

void M_http_set_chunk_extensions_complete(M_http_t *http, size_t num, M_bool complete)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	chunk->extensions_complete = complete;
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
	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	M_list_remove_at(http->chunks, num);
}

size_t M_http_chunk_data_len(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return 0;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return 0;

	return chunk->body_len;
}

size_t M_http_chunk_data_len_buffered(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return 0;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return 0;

	return chunk->body_len_seen;
}

size_t M_http_chunk_data_len_seen(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return 0;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return 0;

	return M_buf_len(chunk->body);
}

const unsigned char *M_http_chunk_data(const M_http_t *http, size_t num, size_t *len)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return NULL;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return NULL;

	*len = M_buf_len(chunk->body);
	return (const unsigned char *)M_buf_peek(chunk->body);
}

void M_http_set_chunk_data(M_http_t *http, size_t num, const unsigned char *data, size_t len)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num || data == NULL || len == 0)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	M_buf_truncate(chunk->body, 0);
	M_buf_add_bytes(chunk->body, data, len);
	chunk->body_len      = M_buf_len(chunk->body);
	chunk->body_len_seen = M_buf_len(chunk->body);
}

void M_http_chunk_data_append(M_http_t *http, size_t num, const unsigned char *data, size_t len)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num || data == NULL || len == 0)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	M_buf_add_bytes(chunk->body, data, len);
	chunk->body_len      = M_buf_len(chunk->body);
	chunk->body_len_seen = M_buf_len(chunk->body);
}

void M_http_chunk_data_drop(M_http_t *http, size_t num, size_t len)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	M_buf_drop(chunk->body, len);
}

const M_hash_dict_t *M_http_chunk_extensions(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return NULL;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return NULL;

	return chunk->extensions;
}

char *M_http_chunk_extension_string(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;
	M_hash_dict_enum_t   *he;
	const char           *key;
	const char           *val;
	M_list_str_t         *l;
	M_buf_t              *buf;
	char                 *out;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return NULL;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return NULL;

	l = M_list_str_create(M_LIST_STR_NONE);
	buf = M_buf_create();

	M_hash_dict_enumerate(chunk->extensions, &he);
	while (M_hash_dict_enumerate_next(chunk->extensions, he, &key, &val)) {
		M_buf_truncate(buf, 0);
		M_buf_add_str(buf, key);
		if (!M_str_isempty(val)) {
			M_buf_add_byte(buf, '=');
			M_buf_add_str(buf, val);
		}
		M_list_str_insert(l, M_buf_peek(buf));
	}
	M_hash_dict_enumerate_free(he);
	M_buf_cancel(buf);

	out = M_list_str_join(l, ';');
	M_list_str_destroy(l);
	return out;
}

void M_http_set_chunk_extensions(M_http_t *http, size_t num, const M_hash_dict_t *extensions)
{
	M_http_chunk_t *chunk;
	M_hash_dict_enum_t   *he;
	const char           *key;
	const char           *val;

	if (http == NULL || M_list_len(http->chunks) >= num || extensions == NULL)
		return;

	chunk = M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
	if (chunk == NULL)
		return;

	M_hash_dict_destroy(chunk->extensions);
	chunk->extensions = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);

	M_hash_dict_enumerate(chunk->extensions, &he);
	while (M_hash_dict_enumerate_next(extensions, he, &key, &val)) {
		M_hash_dict_insert(chunk->extensions, key, val);
	}
	M_hash_dict_enumerate_free(he);
}

void M_http_set_chunk_extension(M_http_t *http, size_t num, const char *key, const char *val)
{
	const M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	chunk = M_list_at(http->chunks, num);
	if (chunk == NULL)
		return;

	if (val == NULL)
		val = "";

	M_hash_dict_insert(chunk->extensions, key, val);
}
