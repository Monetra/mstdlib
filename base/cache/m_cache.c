/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_cache {
	M_hashtable_t          *kv_table; /* key -> llist_node. Pointers only, never copies or desroys. */
	M_llist_t              *value_list; /* llist of cache_values. Never destroys. */
	size_t                  max_size;
	size_t                  size;
	M_cache_duplicate_func  key_duplicate;
	M_cache_free_func       key_free;
	M_cache_duplicate_func  value_duplicate;
	M_cache_free_func       value_free;
};

typedef struct {
	void *key; /* Key, kv_table's key points to this. */
	void *value;
} M_cache_value_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_cache_value_t *M_cache_value_create(M_cache_t *c, const void *key, const void *value)
{
	M_cache_value_t *cval;

	cval = M_malloc_zero(sizeof(*cval));
	
	if (c->key_duplicate != NULL) {
		cval->key = c->key_duplicate(key);
	} else {
		cval->key = M_CAST_OFF_CONST(void *, key);
	}

	if (c->value_duplicate != NULL) {
		cval->value = c->value_duplicate(key);
	} else {
		cval->value = M_CAST_OFF_CONST(void *, value);
	}

	return cval;
}

static void M_cache_value_destroy(M_cache_t *c, M_cache_value_t *cval, M_bool destroy_container)
{
	if (c->key_free != NULL)
		c->key_free(cval->key);

	if (c->value_free != NULL)
		c->value_free(cval->value);

	if (destroy_container) {
		M_free(cval);
	} else {
		M_mem_set(cval, 0, sizeof(*cval));
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_cache_t *M_cache_create(size_t max_size, M_hashtable_hash_func key_hash, M_sort_compar_t key_equality, M_uint32 flags, const struct M_cache_callbacks *callbacks)
{
	M_cache_t *c;

	(void)flags;

	c = M_malloc_zero(sizeof(*c));

	c->kv_table = M_hashtable_create(16, 75, key_hash, key_equality, M_HASHTABLE_NONE, NULL);
	if (c->kv_table == NULL) {
		M_free(c);
		return NULL;
	}

	c->value_list = M_llist_create(NULL, M_LLIST_NONE);
	if (c->value_list == NULL) {
		M_hashtable_destroy(c->kv_table, M_TRUE);
		M_free(c);
		return NULL;
	};

	if (callbacks != NULL) {
		c->key_duplicate   = callbacks->key_duplicate;
		c->key_free        = callbacks->key_free;
		c->value_duplicate = callbacks->value_duplicate;
		c->value_free      = callbacks->value_free;
	}

	c->max_size = max_size;
	return c;
}

void M_cache_destroy(M_cache_t *c)
{
	M_llist_node_t *bucket;

	if (c == NULL)
		return;

	M_hashtable_destroy(c->kv_table, M_FALSE);

	bucket = M_llist_first(c->value_list);
	while (bucket != NULL) {
		M_cache_value_destroy(c, M_llist_take_node(bucket), M_TRUE);
		bucket = M_llist_first(c->value_list);
	}
	M_llist_destroy(c->value_list, M_FALSE);

	M_free(c);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_cache_insert(M_cache_t *c, const void *key, const void *value)
{
	M_llist_node_t  *bucket = NULL;
	M_cache_value_t *cval   = NULL;
	M_bool           newval = M_FALSE;

	if (c == NULL || key == NULL || c->max_size == 0)
		return M_FALSE;

	/* Try to get an existing bucket if the key already exists. */
	if (!M_hashtable_get(c->kv_table, key, (void **)&bucket))
		bucket = NULL;

	if (bucket == NULL) {
		if (M_llist_len(c->value_list) == c->max_size) {
			/* Out of space, reuse the coldest bucket. */
			bucket = M_llist_last(c->value_list);
			cval   = M_llist_node_val(bucket);

			/* Remove the bucket for the cval from the hashtable. */
			M_hashtable_remove(c->kv_table, cval->key, M_FALSE);
			/* Clear the contents of the value (in bucket). */
			M_cache_value_destroy(c, cval, M_FALSE);

			/* Add the key. */
			if (c->key_duplicate != NULL) {
				cval->key = c->key_duplicate(key);
			} else {
				cval->key = M_CAST_OFF_CONST(void *, key);
			}
		} else {
			/* Still have space so we need to create a new bucket. */
			cval   = M_cache_value_create(c, key, value);
			bucket = M_llist_insert_first(c->value_list, cval);
			newval = M_TRUE;
		}
		M_hashtable_insert(c->kv_table, cval->key, bucket);
	} else {
		/* Key matches a bucket so we only need to clear the value. */
		cval = M_llist_node_val(bucket);
		if (c->value_free != NULL) {
			c->value_free(cval->value);
		}
	}

	if (!newval) {
		/* Update bucket data value. */
		if (c->value_duplicate != NULL && value != NULL) {
			cval->value = c->value_duplicate(key);
		} else {
			cval->value = M_CAST_OFF_CONST(void *, value);
		}
	}

	M_llist_set_first(bucket);
	return M_TRUE;
}

M_bool M_cache_remove(M_cache_t *c, const void *key)
{
	M_llist_node_t  *bucket = NULL;
	M_cache_value_t *cval   = NULL;

	if (c == NULL || key == NULL)
		return M_FALSE;

	if (!M_hashtable_get(c->kv_table, key, (void **)&bucket))
		return M_FALSE;

	cval = M_llist_take_node(bucket);
	M_hashtable_remove(c->kv_table, key, M_FALSE);
	M_cache_value_destroy(c, cval, M_TRUE);

	return M_TRUE;
}

M_bool M_cache_get(const M_cache_t *c, const void *key, void **value)
{
	M_llist_node_t  *bucket = NULL;
	M_cache_value_t *cval   = NULL;

	if (c == NULL || key == NULL)
		return M_FALSE;

	if (!M_hashtable_get(c->kv_table, key, (void **)&bucket))
		return M_FALSE;

	M_llist_set_first(bucket);

	if (value == NULL)
		return M_TRUE;

	cval = M_llist_node_val(bucket);
	*value = cval->value;

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_cache_size(const M_cache_t *c)
{
	if (c == NULL)
		return 0;

	return M_llist_len(c->value_list);
}

size_t M_cache_max_size(const M_cache_t *c)
{
	if (c == NULL)
		return 0;

	return c->max_size;
}

M_bool M_cache_set_max_size(M_cache_t *c, size_t max_size)
{
	M_llist_node_t  *bucket = NULL;
	M_cache_value_t *cval   = NULL;

	if (c == NULL)
		return M_FALSE;

	while (M_llist_len(c->value_list) > max_size) {
		bucket = M_llist_last(c->value_list);
		cval   = M_llist_take_node(bucket);
		M_hashtable_remove(c->kv_table, cval->key, M_FALSE);
		M_cache_value_destroy(c, cval, M_TRUE);
	}

	c->max_size = max_size;
	return M_TRUE;
}
