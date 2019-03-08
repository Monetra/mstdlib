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

M_http_chunk_t *M_http_chunk_get(const M_http_t *http, size_t num)
{
	if (http == NULL)
		return NULL;
	return M_CAST_OFF_CONST(M_http_chunk_t *, M_list_at(http->chunks, num));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_http_is_chunked(const M_http_t *http)
{
	if (http == NULL)
		return M_FALSE;
	return http->is_chunked;
}

size_t M_http_chunk_count(const M_http_t *http)
{
	if (http == NULL)
		return 0;
	return M_list_len(http->chunks);
}

size_t M_http_chunk_insert(M_http_t *http)
{
	M_http_chunk_t *chunk;
	size_t          idx;

	if (http == NULL)
		return 0;

	chunk = M_http_chunk_create();
	M_list_insert(http->chunks, chunk);

	idx = M_list_len(http->chunks);

	/* Silence coverity, not possible for it to be == 0 */
	if (idx > 0)
		idx--;

	return idx;
}

void M_http_chunk_remove(M_http_t *http, size_t num)
{
	if (http == NULL)
		return;

	M_list_remove_at(http->chunks, num);
}

size_t M_http_chunk_data_length(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
		return 0;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return 0;
	return chunk->body_len;
}

size_t M_http_chunk_data_length_seen(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
		return 0;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return 0;
	return chunk->body_len_seen;
}

size_t M_http_chunk_data_length_buffered(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
		return 0;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return 0;
	return M_buf_len(chunk->body);
}

const unsigned char *M_http_chunk_data(const M_http_t *http, size_t num, size_t *len)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
		return NULL;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return NULL;

	*len = M_buf_len(chunk->body);
	return (const unsigned char *)M_buf_peek(chunk->body);
}

void M_http_chunk_data_append(M_http_t *http, size_t num, const unsigned char *data, size_t len)
{
	M_http_chunk_t *chunk;

	if (http == NULL || data == NULL || len == 0)
		return;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return;

	chunk->body_len_seen += len;
	if (chunk->body_len_seen > chunk->body_len)
		chunk->body_len = chunk->body_len_seen;
	M_buf_add_bytes(chunk->body, data, len);
}

void M_http_chunk_data_drop(M_http_t *http, size_t num, size_t len)
{
	M_http_chunk_t *chunk;

	if (http == NULL || M_list_len(http->chunks) >= num)
		return;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return;

	M_buf_drop(chunk->body, len);
}

const M_hash_dict_t *M_http_chunk_extensions(const M_http_t *http, size_t num)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
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

	if (http == NULL)
		return NULL;

	chunk = M_http_chunk_get(http, num);
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

	if (http == NULL || extensions == NULL)
		return;

	chunk = M_http_chunk_get(http, num);
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

M_bool M_http_set_chunk_extensions_string(M_http_t *http, size_t num, const char *str)
{
	char   **parts     = NULL;
	size_t   num_parts = 0;
	size_t   i;
	M_bool   ret = M_TRUE;

	if (http == NULL || M_str_isempty(str))
		return M_FALSE;

	parts = M_str_explode_str(';', str, &num_parts);
	if (parts == NULL || num_parts == 0) {
		M_str_explode_free(parts, num_parts);
		return M_FALSE;
	}

	for (i=0; i<num_parts; i++) {
		char       **kv     = NULL;
		size_t       num_kv = 0;
		const char  *key    = NULL;
		const char  *val    = NULL;

		kv = M_str_explode_str('=', parts[i], &num_kv);
		if (kv == NULL || num_kv == 0 || num_kv > 2) {
			M_str_explode_free(kv, num_kv);
			ret = M_FALSE;
			break;
		}

		key = kv[0];
		if (num_kv == 2) {
			val = kv[1];
		} 
		M_http_set_chunk_extension(http, num, key, val);

		M_str_explode_free(kv, num_kv);
	}

	M_str_explode_free(parts, num_parts);
	return ret;
}

void M_http_set_chunk_extension(M_http_t *http, size_t num, const char *key, const char *val)
{
	const M_http_chunk_t *chunk;

	if (http == NULL)
		return;

	chunk = M_http_chunk_get(http, num);
	if (chunk == NULL)
		return;

	if (val == NULL)
		val = "";

	M_hash_dict_insert(chunk->extensions, key, val);
}
