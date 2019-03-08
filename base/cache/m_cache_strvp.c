/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_cache_strvp_t *M_cache_strvp_create(size_t max_size, M_uint32 flags, void (*destroy_func)(void *))
{
	M_hashtable_hash_func    key_hash     = M_hash_func_hash_str;
	M_sort_compar_t          key_equality = M_sort_compar_str;
	struct M_cache_callbacks callbacks = {
		M_hash_void_strdup,
		M_free,
		NULL,
		destroy_func
	};

	/* Key options. */
	if (flags & M_CACHE_STRVP_CASECMP) {
		key_hash     = M_hash_func_hash_str_casecmp;
		key_equality = M_sort_compar_str_casecmp;
	}

	return (M_cache_strvp_t *)M_cache_create(max_size, key_hash, key_equality, M_CACHE_NONE, &callbacks);
}

void M_cache_strvp_destroy(M_cache_strvp_t *c)
{
	M_cache_destroy((M_cache_t *)c);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_cache_strvp_insert(M_cache_strvp_t *c, const char *key, const void *value)
{
	if (M_str_isempty(key))
		return M_FALSE;
	return M_cache_insert((M_cache_t *)c, key, value);
}

M_bool M_cache_strvp_remove(M_cache_strvp_t *c, const char *key)
{
	return M_cache_remove((M_cache_t *)c, key);
}

M_bool M_cache_strvp_get(const M_cache_strvp_t *c, const char *key, void **value)
{
	return M_cache_get((const M_cache_t *)c, key, value);
}

void *M_cache_strvp_get_direct(const M_cache_strvp_t *c, const char *key)
{
	void *val = NULL;
	M_cache_strvp_get(c, key, &val);
	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_cache_strvp_size(const M_cache_strvp_t *c)
{
	return M_cache_size((const M_cache_t *)c);
}

size_t M_cache_strvp_max_size(const M_cache_strvp_t *c)
{
	return M_cache_max_size((const M_cache_t *)c);
}

M_bool M_cache_strvp_set_max_size(M_cache_strvp_t *c, size_t max_size)
{
	return M_cache_set_max_size((M_cache_t *)c, max_size);
}

