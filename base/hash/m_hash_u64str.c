/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

M_hash_u64str_t *M_hash_u64str_create(size_t size, M_uint8 fillpct, M_uint32 flags)
{
	M_hashtable_hash_func        key_hash     = M_hash_func_hash_u64;
	M_sort_compar_t              key_equality = M_sort_compar_u64;
	M_hashtable_flags_t          hash_flags   = M_HASHTABLE_NONE;
	struct M_hashtable_callbacks callbacks    = {
		M_hash_func_u64dup,
		M_hash_func_u64dup,
		M_free,
		M_hash_void_strdup,
		M_hash_void_strdup,
		NULL,
		M_free	
	};

	/* Key options. */
	if (flags & M_HASH_U64STR_KEYS_ORDERED) {
		hash_flags |= M_HASHTABLE_KEYS_ORDERED;
		if (flags & M_HASH_U64STR_KEYS_SORTASC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
		}
		if (flags & M_HASH_U64STR_KEYS_SORTDESC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
			key_equality = M_sort_compar_u64_desc;
		}
	}

	/* Multi-value options. */
	if (flags & M_HASH_U64STR_MULTI_VALUE) {
		hash_flags |= M_HASHTABLE_MULTI_VALUE;
	}
	if (flags & M_HASH_U64STR_MULTI_SORTASC) {
		hash_flags |= M_HASHTABLE_MULTI_SORTED;
		if (flags & M_HASH_U64STR_MULTI_CASECMP) {
			callbacks.value_equality = M_sort_compar_str_casecmp;
		} else {
			callbacks.value_equality = M_sort_compar_str;
		}
	}
	if (flags & M_HASH_U64STR_MULTI_SORTDESC) {
		hash_flags |= M_HASHTABLE_MULTI_SORTED;
		if (flags & M_HASH_U64STR_MULTI_CASECMP) {
			callbacks.value_equality = M_sort_compar_str_casecmp_desc;
		} else {
			callbacks.value_equality = M_sort_compar_str_desc;
		}
	}
	if (flags & M_HASH_U64STR_MULTI_GETLAST) {
		hash_flags |= M_HASHTABLE_MULTI_GETLAST;
	}

	/* We are only dealing in opaque types here, and we don't have any
	 * metadata of our own to store, so we are only casting one pointer
	 * type to another.  This is a safe operation */
	return (M_hash_u64str_t *)M_hashtable_create(size, fillpct, key_hash, key_equality, hash_flags, &callbacks);
}


void M_hash_u64str_destroy(M_hash_u64str_t *h)
{
	M_hashtable_destroy((M_hashtable_t *)h, M_TRUE);
}


M_bool M_hash_u64str_insert(M_hash_u64str_t *h, M_uint64 key, const char *value)
{
	/* We are hacking it a bit here.  We are using the fact that
	 * sizeof(size_t) == sizeof(void *) so we are storing the value as
	 * a pointer address to avoid allocating memory */
	return M_hashtable_insert((M_hashtable_t *)h, &key, value);
}


M_bool M_hash_u64str_remove(M_hash_u64str_t *h, M_uint64 key)
{
	return M_hashtable_remove((M_hashtable_t *)h, &key, M_TRUE);
}


M_bool M_hash_u64str_get(const M_hash_u64str_t *h, M_uint64 key, const char **value)
{
	void  *outval = NULL;
	M_bool retval;

	retval = M_hashtable_get((const M_hashtable_t *)h, &key, &outval);

	if (value != NULL)
		*value = (const char *)outval;

	return retval;
}

const char *M_hash_u64str_get_direct(const M_hash_u64str_t *h, M_uint64 key)
{
	const char *val = NULL;

	if (!M_hash_u64str_get(h, key, &val))
		return NULL;

	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_u64str_multi_len(const M_hash_u64str_t *h, M_uint64 key, size_t *len)
{
	return M_hashtable_multi_len((const M_hashtable_t *)h, &key, len);
}


M_bool M_hash_u64str_multi_get(const M_hash_u64str_t *h, M_uint64 key, size_t idx, const char **value)
{
	void   *outval = NULL;
	M_bool  retval;

	retval = M_hashtable_multi_get((const M_hashtable_t *)h, &key, idx, &outval);

	if (value != NULL)
		*value = outval;

	return retval;
}


const char *M_hash_u64str_multi_get_direct(const M_hash_u64str_t *h, M_uint64 key, size_t idx)
{
	const char *val = NULL;
	M_hash_u64str_multi_get(h, key, idx, &val);
	return val;
}


M_bool M_hash_u64str_multi_remove(M_hash_u64str_t *h, M_uint64 key, size_t idx)
{
	return M_hashtable_multi_remove((M_hashtable_t *)h, &key, idx, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint32 M_hash_u64str_size(const M_hash_u64str_t *h)
{
	return M_hashtable_size((const M_hashtable_t *)h);
}


size_t M_hash_u64str_num_collisions(const M_hash_u64str_t *h)
{
	return M_hashtable_num_collisions((const M_hashtable_t *)h);
}


size_t M_hash_u64str_num_expansions(const M_hash_u64str_t *h)
{
	return M_hashtable_num_expansions((const M_hashtable_t *)h);
}


size_t M_hash_u64str_num_keys(const M_hash_u64str_t *h)
{
	return M_hashtable_num_keys((const M_hashtable_t *)h);
}


size_t M_hash_u64str_enumerate(const M_hash_u64str_t *h, M_hash_u64str_enum_t **hashenum)
{
	M_hashtable_enum_t *myhashenum = M_malloc(sizeof(*myhashenum));
	size_t              rv;

	*hashenum = (M_hash_u64str_enum_t *)myhashenum;
	rv        = M_hashtable_enumerate((const M_hashtable_t *)h, myhashenum);
	if (rv == 0) {
		M_free(*hashenum);
		*hashenum = NULL;
	}
	return rv;
}


M_bool M_hash_u64str_enumerate_next(const M_hash_u64str_t *h, M_hash_u64str_enum_t *hashenum, M_uint64 *key, const char **value)
{
	M_hashtable_enum_t *myhashenum = (M_hashtable_enum_t *)hashenum;
	const M_uint64     *tmp_key;
	const void         *tmp_val;

	if (!M_hashtable_enumerate_next((const M_hashtable_t *)h, myhashenum, (const void **)&tmp_key, (const void **)&tmp_val))
		return M_FALSE;

	if (key != NULL)
		*key = *tmp_key;

	if (value != NULL)
		*value = tmp_val;

	return M_TRUE;
}


void M_hash_u64str_enumerate_free(M_hash_u64str_enum_t *hashenum)
{
	M_free(hashenum);
}


void M_hash_u64str_merge(M_hash_u64str_t **dest, M_hash_u64str_t *src)
{
	M_hashtable_merge((M_hashtable_t **)dest, (M_hashtable_t *)src);
}


M_hash_u64str_t *M_hash_u64str_duplicate(const M_hash_u64str_t *h)
{
	return (M_hash_u64str_t *)M_hashtable_duplicate((const M_hashtable_t *)h);
}
