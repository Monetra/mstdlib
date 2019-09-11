/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

M_hash_stridx_t *M_hash_stridx_create(size_t size, M_uint8 fillpct, M_uint32 flags)
{
	M_hashtable_hash_func        key_hash     = M_hash_func_hash_str;
	M_sort_compar_t              key_equality = M_sort_compar_str;
	M_hashtable_flags_t          hash_flags   = M_HASHTABLE_NONE;
	struct M_hashtable_callbacks callbacks    = {
		M_hash_void_strdup,
		M_hash_void_strdup,
		M_free,
		NULL, /* Default uses pointer directly */
		NULL, /* Default uses pointer directly */
		NULL, /* Default doesn't sort. */
		NULL  /* Default doesn't free */
	};

	/* Key options. */
	if (flags & M_HASH_STRIDX_CASECMP) {
		key_hash     = M_hash_func_hash_str_casecmp;
		key_equality = M_sort_compar_str_casecmp;
	}
	if (flags & M_HASH_STRIDX_KEYS_ORDERED) {
		hash_flags |= M_HASHTABLE_KEYS_ORDERED;
		if (flags & M_HASH_STRIDX_KEYS_SORTASC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
			if (flags & M_HASH_STRIDX_CASECMP) {
				key_equality = M_sort_compar_str_casecmp;
			} else {
				key_equality = M_sort_compar_str;
			}
		}
		if (flags & M_HASH_STRIDX_KEYS_SORTDESC) {
			hash_flags |= M_HASHTABLE_KEYS_SORTED;
			if (flags & M_HASH_STRIDX_CASECMP) {
				key_equality = M_sort_compar_str_casecmp_desc;
			} else {
				key_equality = M_sort_compar_str_desc;
			}
		}
	}
	if (flags & M_HASH_STRIDX_KEYS_UPPER) {
		callbacks.key_duplicate_insert = (M_hashtable_duplicate_func)M_strdup_upper;
		callbacks.key_duplicate_copy   = (M_hashtable_duplicate_func)M_strdup_upper;
	}
	if (flags & M_HASH_STRIDX_KEYS_LOWER) {
		callbacks.key_duplicate_insert = (M_hashtable_duplicate_func)M_strdup_lower;
		callbacks.key_duplicate_copy   = (M_hashtable_duplicate_func)M_strdup_lower;
	}

	/* Multi-value options. */
	if (flags & M_HASH_STRIDX_MULTI_VALUE) {
		hash_flags |= M_HASHTABLE_MULTI_VALUE;
	}
	if (flags & M_HASH_STRIDX_MULTI_GETLAST) {
		hash_flags |= M_HASHTABLE_MULTI_GETLAST;
	}

	/* Initialization options. */
	if (flags & M_HASH_STRIDX_STATIC_SEED) {
		hash_flags |= M_HASHTABLE_STATIC_SEED;
	}

	/* We are only dealing in opaque types here, and we don't have any
	 * metadata of our own to store, so we are only casting one pointer
	 * type to another.  This is a safe operation */
	return (M_hash_stridx_t *)M_hashtable_create(size, fillpct, key_hash, key_equality, hash_flags, &callbacks);
}


void M_hash_stridx_destroy(M_hash_stridx_t *h)
{
	M_hashtable_destroy((M_hashtable_t *)h, M_TRUE);
}


M_bool M_hash_stridx_insert(M_hash_stridx_t *h, const char *key, size_t value)
{
	/* Can't insert empty keys. Hashtable base will check for NULL but it won't check for \0 because it
 	 * doesn't know the type. */
	if (key == NULL || *key == '\0')
		return M_FALSE;
	/* We are hacking it a bit here.  We are using the fact that
	 * sizeof(size_t) == sizeof(void *) so we are storing the value as
	 * a pointer address to avoid allocating memory */
	return M_hashtable_insert((M_hashtable_t *)h, key, (void *)value);
}


M_bool M_hash_stridx_remove(M_hash_stridx_t *h, const char *key)
{
	return M_hashtable_remove((M_hashtable_t *)h, key, M_TRUE);
}


M_bool M_hash_stridx_get(const M_hash_stridx_t *h, const char *key, size_t *value)
{
	void  *outval = NULL;
	M_bool retval;

	retval = M_hashtable_get((const M_hashtable_t *)h, key, &outval);

	if (value != NULL)
		*value = (size_t)outval;

	return retval;
}

size_t M_hash_stridx_get_direct(const M_hash_stridx_t *h, const char *key)
{
	size_t value;
	M_hash_stridx_get(h, key, &value);
	return value;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_stridx_is_multi(const M_hash_stridx_t *h)
{
	return M_hashtable_is_multi((const M_hashtable_t *)h);
}


M_bool M_hash_stridx_multi_len(const M_hash_stridx_t *h, const char *key, size_t *len)
{
	return M_hashtable_multi_len((const M_hashtable_t *)h, key, len);
}


M_bool M_hash_stridx_multi_get(const M_hash_stridx_t *h, const char *key, size_t idx, size_t *value)
{
	size_t outval = 0;
	M_bool retval;

	retval = M_hashtable_multi_get((const M_hashtable_t *)h, key, idx, (void **)&outval);

	if (value != NULL)
		*value = outval;

	return retval;
}


size_t M_hash_stridx_multi_get_direct(const M_hash_stridx_t *h, const char *key, size_t idx)
{
	size_t val = 0;
	M_hash_stridx_multi_get(h, key, idx, &val);
	return val;
}


M_bool M_hash_stridx_multi_remove(M_hash_stridx_t *h, const char *key, size_t idx)
{
	return M_hashtable_multi_remove((M_hashtable_t *)h, key, idx, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */



M_uint32 M_hash_stridx_size(const M_hash_stridx_t *h)
{
	return M_hashtable_size((const M_hashtable_t *)h);
}


size_t M_hash_stridx_num_collisions(const M_hash_stridx_t *h)
{
	return M_hashtable_num_collisions((const M_hashtable_t *)h);
}


size_t M_hash_stridx_num_expansions(const M_hash_stridx_t *h)
{
	return M_hashtable_num_expansions((const M_hashtable_t *)h);
}


size_t M_hash_stridx_num_keys(const M_hash_stridx_t *h)
{
	return M_hashtable_num_keys((const M_hashtable_t *)h);
}


size_t M_hash_stridx_enumerate(const M_hash_stridx_t *h, M_hash_stridx_enum_t **hashenum)
{
	M_hashtable_enum_t *myhashenum = M_malloc(sizeof(*myhashenum));
	size_t              rv;

	*hashenum = (M_hash_stridx_enum_t *)myhashenum;
	rv        = M_hashtable_enumerate((const M_hashtable_t *)h, myhashenum);
	if (rv == 0) {
		M_free(*hashenum);
		*hashenum = NULL;
	}
	return rv;
}


M_bool M_hash_stridx_enumerate_next(const M_hash_stridx_t *h, M_hash_stridx_enum_t *hashenum, const char **key, size_t *value)
{
	M_hashtable_enum_t *myhashenum = (M_hashtable_enum_t *)hashenum;
	return M_hashtable_enumerate_next((const M_hashtable_t *)h, myhashenum, (const void **)key, (const void **)value);
}


void M_hash_stridx_enumerate_free(M_hash_stridx_enum_t *hashenum)
{
	M_free(hashenum);
}


void M_hash_stridx_merge(M_hash_stridx_t **dest, M_hash_stridx_t *src)
{
	M_hashtable_merge((M_hashtable_t **)dest, (M_hashtable_t *)src);
}


M_hash_stridx_t *M_hash_stridx_duplicate(const M_hash_stridx_t *h)
{
	return (M_hash_stridx_t *)M_hashtable_duplicate((const M_hashtable_t *)h);
}
