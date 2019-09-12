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
#include "m_defs_int.h"

struct M_hashtable_bucket;

/*! This is where we store the actual key and value pairs for the
 *  h entries.  It is formatted as a linked list, but we
 *  also use it as an array for the main bucket list in the h,
 *  so we store the first hash match within the bucket list itself
 *  rather than needing to allocate memory for it */
struct M_hashtable_bucket {
	void                      *key;         /*!< Pointer to the key. May or may not have allocated memory.
	                                         *   May NOT be NULL. */
	union {
		void                  *value;       /*!< Value stored. May be NULL. */
		M_list_t              *multi_value; /*!< A list of values. */
	} value;
	struct M_hashtable_bucket *next;        /*!< Chained entry.  May be NULL if no hash collisions */
};


/*! This is our main h structure.  It contains implementation-specific
 *  callbacks that control behavior.  The h implementation uses
 *  chaining for hash collisions, and stores the first hash match in the
 *  bucket list itself to avoid additional memory allocations, though does
 *  waste some memory. */
struct M_hashtable {
	M_sort_compar_t            key_equality;           /*!< Callback for key equality check */
	M_hashtable_hash_func      key_hash;               /*!< Callback for key hash */
	M_hashtable_duplicate_func key_duplicate_insert;   /*!< Callback to duplicate a key */
	M_hashtable_duplicate_func key_duplicate_copy;     /*!< Callback to duplicate a key */
	M_hashtable_free_func      key_free;               /*!< Callback to free a key */
	M_hashtable_duplicate_func value_duplicate_insert; /*!< Callback to duplicate a value */
	M_hashtable_duplicate_func value_duplicate_copy;   /*!< Callback to duplicate a value */
	M_sort_compar_t            value_equality;         /*!< Callback for value equality check
	                                                        (used for sorting of multi_value) */
	M_hashtable_free_func      value_free;             /*!< Callback to free a value */

	struct M_hashtable_bucket *buckets;                /*!< Bucket list */

	M_llist_t                 *keys;                   /*!< List of keys in the h used for ordering. */

	M_uint32                   key_hash_seed;          /*!< Used when computing hashes to prevent collision attacks. */
	M_uint32                   size;                   /*!< Number of buckets. Power of 2 */
	size_t                     num_keys;               /*!< Number of keys in the hash table */
	size_t                     num_values;             /*!< Number of values in the hash table */
	size_t                     num_collisions;         /*!< Number of collisions in the hash table */
	size_t                     num_expansions;         /*!< Number of times the hash table has been expanded/rehashed */

	M_uint8                    fillpct;                /*!< Percentage full before expansion/rehash. 0=no rehash */

	M_hashtable_flags_t        flags;                  /*!< Flags controlling behavior. */
};



/*! Default duplication callback.  Pass-thru pointer */
static void *M_hashtable_duplicate_func_default(const void *arg)
{
	return M_CAST_OFF_CONST(void *, arg);
}


/*! Default free callback. No-Op */
static void M_hashtable_free_func_default(void *arg)
{
	(void)arg;
	/* No-op */
}

static int M_hashtable_equality_func_default(const void *arg1, const void *arg2, void *thunk)
{
	(void)arg1;
	(void)arg2;
	(void)thunk;
	return 0;
}

M_hashtable_t *M_hashtable_create(size_t size, M_uint8 fillpct,
		M_hashtable_hash_func key_hash, M_sort_compar_t key_equality,
		M_uint32 flags, const struct M_hashtable_callbacks *callbacks)
{
	M_hashtable_t            *h;
	struct M_llist_callbacks  llist_callbacks;

	if (size == 0 || fillpct >= 100)
		return NULL;

	/* Error if we have a multi-option and multi-value is not enabled. */
	if ((flags & (M_HASHTABLE_MULTI_SORTED|M_HASHTABLE_MULTI_GETLAST)) && !(flags & M_HASHTABLE_MULTI_VALUE))
		return NULL;

	/* Error if value sorting is requested for multi-values but a value equality function is not present. */
	if ((flags & M_HASHTABLE_MULTI_SORTED) && (callbacks == NULL || callbacks->value_equality == NULL))
		return NULL;

	/* Error if key sorting is requested but ordered keys is not enabled. */
	if ((flags & M_HASHTABLE_KEYS_SORTED) && !(flags & M_HASHTABLE_KEYS_ORDERED))
		return NULL;

	h = M_malloc(sizeof(*h));
	M_mem_set(h, 0, sizeof(*h));

	size = M_size_t_round_up_to_power_of_two(size);
	if (size > M_HASHTABLE_MAX_BUCKETS) {
		h->size = M_HASHTABLE_MAX_BUCKETS;
	} else {
		h->size = (M_uint32)size;
	}

	h->fillpct = fillpct;
	h->flags   = flags;

	/* Set a non-zero seed. */
	if (flags & M_HASHTABLE_STATIC_SEED) {
		/* FNV1a 32 bit prime. */
		h->key_hash_seed = 16777619;
	} else {
		h->key_hash_seed = (M_uint32)M_rand_range(NULL, 1, (M_uint64)(M_UINT32_MAX)+1);
	}

	/* Default callbacks */
	if (key_hash == NULL)
		key_hash     = M_hash_func_hash_vp;

	if (key_equality == NULL)
		key_equality = M_sort_compar_vp;

	h->key_hash               = key_hash;
	h->key_equality           = key_equality;
	h->key_duplicate_insert   = M_hashtable_duplicate_func_default;
	h->key_duplicate_copy     = M_hashtable_duplicate_func_default;
	h->key_free               = M_hashtable_free_func_default;
	h->value_duplicate_insert = M_hashtable_duplicate_func_default;
	h->value_duplicate_copy   = M_hashtable_duplicate_func_default;
	h->value_equality         = M_hashtable_equality_func_default;
	h->value_free             = M_hashtable_free_func_default;

	/* Custom callbacks */
	if (callbacks != NULL) {
		if (callbacks->key_duplicate_insert   != NULL) h->key_duplicate_insert   = callbacks->key_duplicate_insert;
		if (callbacks->key_duplicate_copy     != NULL) h->key_duplicate_copy     = callbacks->key_duplicate_copy;
		if (callbacks->key_free               != NULL) h->key_free               = callbacks->key_free;
		if (callbacks->value_duplicate_insert != NULL) h->value_duplicate_insert = callbacks->value_duplicate_insert;
		if (callbacks->value_duplicate_copy   != NULL) h->value_duplicate_copy   = callbacks->value_duplicate_copy;
		if (callbacks->value_equality         != NULL) h->value_equality         = callbacks->value_equality;
		if (callbacks->value_free             != NULL) h->value_free             = callbacks->value_free;
	}

	h->buckets = M_malloc(sizeof(*h->buckets) * h->size);
	M_mem_set(h->buckets, 0, sizeof(*h->buckets) * h->size);

	if (flags & M_HASHTABLE_KEYS_ORDERED) {
		M_mem_set(&llist_callbacks, 0, sizeof(llist_callbacks));
		llist_callbacks.equality = h->key_equality;
		/* The ordered key list uses references to the key in the h itself. It does not copy or own
 		 * the keys it holds. */
		h->keys = M_llist_create(&llist_callbacks, (h->flags & M_HASHTABLE_KEYS_SORTED)?M_LLIST_SORTED:M_LLIST_NONE);
	}

	return h;
}


/*! Searches the chained entries of a hash index for a matching key.
 *  \param h Pointer to the h
 *  \param idx       Hash index being searched
 *  \param key       key being searched for
 *  \return Pointer to h bucket containing a match, or NULL if no
 *          match found */
static struct M_hashtable_bucket *M_hashtable_get_match(const M_hashtable_t *h, size_t idx, const void *key)
{
	struct M_hashtable_bucket *entry;

	entry = &h->buckets[idx];
	if (entry->key == NULL)
		return NULL;

	while (entry != NULL) {
		if (h->key_equality(&entry->key, &key, NULL) == 0)
			return entry;
		entry = entry->next;
	}

	return NULL;
}


/*! Grabs the Hashtable index from the key and length.  The h index is
 *  the hash of the function reduced to the size of the bucket list.
 *  We are doing "hash & (size - 1)" since we are guaranteeing a power of 2 for size.
 *  This is equivalent to "hash % size", but should be more efficient */
#define HASH_IDX(h, key) h->key_hash(key, h->key_hash_seed) & (h->size - 1)

enum M_hashtable_insert_type {
	M_HASHTABLE_INSERT_NODUP   = 0,      /*!< Do not duplicate the value. Store the pointer directly. */
	M_HASHTABLE_INSERT_DUP     = 1 << 0, /*!< Duplicate the value before storing. */
	M_HASHTABLE_INSERT_INITIAL = 1 << 1, /*!< This is an initial insert (not a copy from another h). Use
	                                          The initial duplicate callback and not the copy duplicate callback. */
	M_HASHTABLE_INSERT_REHASH  = 1 << 2  /*!< The value itself is a list and should be added directly. */
};


/*! Internal function to insert into a h.  It only differs from the normal
 *  h insert function by the fact that it contains a 'duplicate' boolean
 *  flag.  This function may be used during re-hashing where it is not desirable
 *  to re-duplicate the key/value again, so the flag indicates to just pass-thru
 *  the pointers regardless of the registered callbacks.
 *  \param h Pointer to the h
 *  \param insert_type Whether or not the key_duplicate and value_duplicate callbacks
 *                     should be used. Typically this is set but during
 *                     a rehash, this will be omitted to prevent unnecessary memory
 *                     allocations and frees.
 *
 *                     Also whether this is an insert operation or an internal copy.
 *                     Used to determine which duplicate callback to use.
 *  \param key Key being inserted
 *  \param value Value associated with the key
 *  \return M_TRUE on success. M_FALSE on failure.  Currently this function will
 *          only return failure on misuse. */
static M_bool M_hashtable_insert_direct(M_hashtable_t *h, enum M_hashtable_insert_type insert_type, const void *key, const void *value)
{
	size_t                     idx;
	struct M_hashtable_bucket *entry;
	void                      *myvalue;
	struct M_list_callbacks    list_callbacks;
	M_bool                     key_added       = M_FALSE;

	if (h == NULL || key == NULL)
		return M_FALSE;

	/* Duplicate the value (before possibly freeing the old one in case the
	 * new value references the old value as a pointer in some way) */
	if (insert_type & M_HASHTABLE_INSERT_DUP) {
		if (insert_type & M_HASHTABLE_INSERT_INITIAL) {
			myvalue = h->value_duplicate_insert(value);
		} else {
			myvalue = h->value_duplicate_copy(value);
		}
	} else {
		/* Must be in a rehash, don't duplicate! */
		myvalue = M_CAST_OFF_CONST(void *, value);
	}

	idx   = HASH_IDX(h, key);
	entry = M_hashtable_get_match(h, idx, key);

	if (entry == NULL) {
		/* No matching entry */
		if (!(insert_type & M_HASHTABLE_INSERT_REHASH))
			h->num_keys++;
		key_added = M_TRUE;

		if (h->buckets[idx].key == NULL) {
			/* No collision */
			entry = &h->buckets[idx];
		} else {
			/* Collision, chain it */
			h->num_collisions++;
			entry                       = M_malloc(sizeof(*entry));
			M_mem_set(entry, 0, sizeof(*entry));
			entry->next                 = h->buckets[idx].next;
			h->buckets[idx].next = entry;
		}

		/* Store the key */
		if (insert_type & M_HASHTABLE_INSERT_DUP) {
			if (insert_type & M_HASHTABLE_INSERT_INITIAL) {
				entry->key = h->key_duplicate_insert(key);
			} else {
				entry->key = h->key_duplicate_copy(key);
			}
		} else {
			/* Must be in a rehash, don't duplicate! */
			entry->key     = M_CAST_OFF_CONST(void *, key);
		}
		/* Add the key to the ordered list of keys. */
		if (h->flags & M_HASHTABLE_KEYS_ORDERED && !(insert_type & M_HASHTABLE_INSERT_REHASH))
			M_llist_insert(h->keys, entry->key);

		/* Create a place to store values if using muli-value. */
		if ((h->flags & M_HASHTABLE_MULTI_VALUE) && !(insert_type & M_HASHTABLE_INSERT_REHASH)) {
			M_mem_set(&list_callbacks, 0, sizeof(list_callbacks));
			/* Note: The h will handle duplicating values for the list */
			list_callbacks.equality   = h->value_equality;
			list_callbacks.value_free = h->value_free;
			entry->value.multi_value  = M_list_create(&list_callbacks, (h->flags & M_HASHTABLE_MULTI_SORTED)?M_LIST_SORTED:M_LIST_NONE);
		}
	} else {
		if (!(h->flags & M_HASHTABLE_MULTI_VALUE)) {
			/* Check that the stored value isn't the same as the value we're trying to store.
			 *
 			 * E.g:
 			 * val = M_hash_get(..., key);
			 * M_hash_set(..., key, val);
			 *
			 * This will only happen when using a passthrough value. If
			 * this is a insert with duplication, for example, we'll have
			 * already duplicated and will store the dup.
			 */
			if (entry->value.value == myvalue) {
				return M_TRUE;
			}

			/* Kill existing value so we can replace it */
			if (h->value_free != NULL)
				h->value_free(entry->value.value);
		}
	}

	/* Store the value */
	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		if (insert_type & M_HASHTABLE_INSERT_REHASH) {
			entry->value.multi_value = myvalue;
		} else {
			M_list_insert(entry->value.multi_value, myvalue);
			h->num_values++;
		}
	} else {
		entry->value.value = myvalue;
		if (key_added && !(insert_type & M_HASHTABLE_INSERT_REHASH))
			h->num_values++;
	}

	return M_TRUE;
}


/*! Function to cleanup key/value when removing an entry or destroying the
 *  h. */
static void M_hashtable_destroy_entry(M_hashtable_t *h, struct M_hashtable_bucket *entry, M_bool destroy_vals)
{
	/* Remove the key from the list of keys. We need to do this before we destroy the key becuase
	 * this is a reference to the key. */
	if (h->flags & M_HASHTABLE_KEYS_ORDERED)
		M_llist_remove_val(h->keys, entry->key, M_LLIST_MATCH_VAL);

	h->key_free(entry->key);
	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		M_list_destroy(entry->value.multi_value, destroy_vals);
	} else if (destroy_vals) {
		h->value_free(entry->value.value);
	}
}


/*! This function is used to either rehash a h or destroy a h.
 *  Though it seems odd that they'd be the same function, they both iterate
 *  over the h the same exact way.  So in order to reduce this error-prone
 *  logic, they were combined into a single function.
 *  \param h  Pointer to the h
 *  \param is_destroy Whether or not this is a destroy call or a rehash call.
 *                    M_TRUE for destroy, M_FALSE for rehash. */
static void M_hashtable_rehash_or_destroy(M_hashtable_t *h, M_bool is_destroy, M_bool destroy_vals)
{
	M_uint32                   i;
	M_uint32                   old_size;
	struct M_hashtable_bucket *old;
	struct M_hashtable_bucket *ptr;
	struct M_hashtable_bucket *next;

	if (h == NULL)
		return;

	/* If we are rehashing, we are going to create a new bucket list and
	 * re-insert (and thus re-hash) each item in the h one by one.
	 * We will NOT call the key_duplicate() or value_duplicate() callbacks
	 * though, we will use the existing memory pointers for those */
	old      = h->buckets;
	old_size = h->size;

	if (!is_destroy) {
		/* No-op if we grow too large.  Do not need to rehash, just return */
		if (h->size << 1 > M_HASHTABLE_MAX_BUCKETS)
			return;

		h->size      <<= 1;
		h->num_expansions++;
		h->buckets     = M_malloc(sizeof(*h->buckets) * h->size);
		M_mem_set(h->buckets, 0, sizeof(*h->buckets) * h->size);
	}

	for (i=0; i<old_size; i++) {
		if (old[i].key != NULL) {
			if (is_destroy) {
				/* Free base entry */
				M_hashtable_destroy_entry(h, &old[i], destroy_vals);
			} else {
				/* Copy over base entry */
				if (h->flags & M_HASHTABLE_MULTI_VALUE) {
					M_hashtable_insert_direct(h, M_HASHTABLE_INSERT_NODUP|M_HASHTABLE_INSERT_REHASH, old[i].key, old[i].value.multi_value);
				} else {
					M_hashtable_insert_direct(h, M_HASHTABLE_INSERT_NODUP|M_HASHTABLE_INSERT_REHASH, old[i].key, old[i].value.value);
				}
			}
			/* Copy then free any chained entries */
			next = NULL;
			ptr  = old[i].next;
			while (ptr != NULL) {
				next = ptr->next;
				if (is_destroy) {
					/* Destroy */
					M_hashtable_destroy_entry(h, ptr, destroy_vals);
				} else {
					/* Copy */
					if (h->flags & M_HASHTABLE_MULTI_VALUE) {
						M_hashtable_insert_direct(h, M_HASHTABLE_INSERT_NODUP|M_HASHTABLE_INSERT_REHASH, ptr->key, ptr->value.multi_value);
					} else {
						M_hashtable_insert_direct(h, M_HASHTABLE_INSERT_NODUP|M_HASHTABLE_INSERT_REHASH, ptr->key, ptr->value.value);
					}
				}
				M_free(ptr);
				ptr = next;
			}
		}
	}

	/* Kill the bucket list */
	M_free(old);

	if (is_destroy) {
		if (h->flags & M_HASHTABLE_KEYS_ORDERED) {
			M_llist_destroy(h->keys, M_FALSE);
		}
		M_free(h);
	}
}


void M_hashtable_destroy(M_hashtable_t *h, M_bool destroy_vals)
{
	M_hashtable_rehash_or_destroy(h, M_TRUE, destroy_vals);
}


/*! Check to see if the fillpct of the h has been exceeded
 *  \param h Hashtable being checked
 *  \return M_TRUE if exceeded, M_FALSE if not */
static M_bool M_hashtable_exceeds_load(const M_hashtable_t *h)
{
	return h->fillpct && h->num_keys * 100 / h->size >= h->fillpct;
}


static M_bool M_hashtable_insert_int(M_hashtable_t *h, M_bool initial_insert, const void *key, const void *value)
{
	enum M_hashtable_insert_type insert_type = M_HASHTABLE_INSERT_DUP;

	if (initial_insert)
		insert_type |= M_HASHTABLE_INSERT_INITIAL;

	if (!M_hashtable_insert_direct(h, insert_type, key, value))
		return M_FALSE;

	/* Check if we need to rehash */
	if (M_hashtable_exceeds_load(h))
		M_hashtable_rehash_or_destroy(h, M_FALSE, M_FALSE);

	return M_TRUE;
}


M_bool M_hashtable_insert(M_hashtable_t *h, const void *key, const void *value)
{
	if (h == NULL)
		return M_FALSE;
	return M_hashtable_insert_int(h, M_TRUE, key, value);
}

static M_bool M_hashtable_get_int(const M_hashtable_t *h, struct M_hashtable_bucket *entry, size_t idx, void **value)
{
	const void *myvalue;
	size_t      value_len = 1;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		value_len = M_list_len(entry->value.multi_value);
	}
	if (idx >= value_len) {
		return M_FALSE;
	}

	if (value != NULL) {
		if (h->flags & M_HASHTABLE_MULTI_VALUE) {
			myvalue = M_list_at(entry->value.multi_value, idx);
			*value = M_CAST_OFF_CONST(void *, myvalue);
		} else {
			*value = entry->value.value;
		}
	}

	return M_TRUE;
}

M_bool M_hashtable_get(const M_hashtable_t *h, const void *key, void **value)
{
	struct M_hashtable_bucket *entry;
	size_t                     hash_idx;
	size_t                     idx     = 0;

	if (h == NULL || key == NULL)
		return M_FALSE;

	hash_idx = HASH_IDX(h, key);
	entry    = M_hashtable_get_match(h, hash_idx, key);

	if (entry == NULL)
		return M_FALSE;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		idx = 0;
		if (h->flags & M_HASHTABLE_MULTI_GETLAST) {
			idx = M_list_len(entry->value.multi_value);
			if (idx > 0)
				idx--;
		}
	}

	return M_hashtable_get_int(h, entry, idx, value);
}


M_bool M_hashtable_remove(M_hashtable_t *h, const void *key, M_bool destroy_vals)
{
	size_t                     idx;
	struct M_hashtable_bucket *entry;
	struct M_hashtable_bucket *next;
	size_t                     value_cnt;

	if (h == NULL || key == NULL)
		return M_FALSE;

	idx   = HASH_IDX(h, key);
	entry = M_hashtable_get_match(h, idx, key);

	if (entry == NULL)
		return M_FALSE;

	next  = entry->next;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		value_cnt = M_list_len(entry->value.multi_value);
	} else {
		value_cnt = 1;
	}
	M_hashtable_destroy_entry(h, entry, destroy_vals);

	if (next != NULL) {
		/* If there is a chained entry following ours, then just copy
		 * its contents over ours and free its chaining ptr memory */
		M_mem_copy(entry, next, sizeof(*entry));
		M_free(next);
	} else if (entry == &h->buckets[idx]) {
		/* If we are a non-chained entry, just zero out the
		 * memory as we freed the bucket */
		M_mem_set(entry, 0, sizeof(*entry));
	} else {
		/* We are the last in a chained entry ... crap, gotta iterate so we
		 * can terminate the chain ... most expensive case */
		struct M_hashtable_bucket *ptr;

		ptr = &h->buckets[idx];
		while (ptr->next != entry)
			ptr = ptr->next;

		ptr->next = NULL;
		M_free(entry);
	}

	h->num_keys--;
	h->num_values -= value_cnt;

	return M_TRUE;
}


M_bool M_hashtable_is_multi(const M_hashtable_t *h)
{
	if (h == NULL || !(h->flags & M_HASHTABLE_MULTI_VALUE))
		return M_FALSE;
	return M_TRUE;
}


M_bool M_hashtable_multi_len(const M_hashtable_t *h, const void *key, size_t *len)
{
	struct M_hashtable_bucket *entry;
	size_t                     hash_idx;
	size_t                     mylen;

	if (len == NULL) {
		len = &mylen;
	}

	*len = 0;

	if (h == NULL || !(h->flags & M_HASHTABLE_MULTI_VALUE) || key == NULL)
		return M_FALSE;

	hash_idx = HASH_IDX(h, key);
	entry    = M_hashtable_get_match(h, hash_idx, key);

	if (entry == NULL)
		return M_FALSE;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		*len = M_list_len(entry->value.multi_value);
	} else {
		*len = 1;
	}

	return M_TRUE;
}


M_bool M_hashtable_multi_get(const M_hashtable_t *h, const void *key, size_t idx, void **value)
{
	struct M_hashtable_bucket *entry;
	size_t                     hash_idx;

	if (h == NULL || !(h->flags & M_HASHTABLE_MULTI_VALUE) || key == NULL)
		return M_FALSE;

	hash_idx = HASH_IDX(h, key);
	entry    = M_hashtable_get_match(h, hash_idx, key);

	if (entry == NULL)
		return M_FALSE;

	return M_hashtable_get_int(h, entry, idx, value);
}


M_bool M_hashtable_multi_remove(M_hashtable_t *h, const void *key, size_t idx, M_bool destroy_vals)
{
	struct M_hashtable_bucket *entry;
	void                      *value;
	size_t                     hash_idx;
	size_t                     value_len = 1;

	if (h == NULL || !(h->flags & M_HASHTABLE_MULTI_VALUE) || key == NULL)
		return M_FALSE;

	hash_idx = HASH_IDX(h, key);
	entry    = M_hashtable_get_match(h, hash_idx, key);

	if (entry == NULL)
		return M_FALSE;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		value_len = M_list_len(entry->value.multi_value);
	}

	if (idx >= value_len)
		return M_FALSE;

	if (!(h->flags & M_HASHTABLE_MULTI_VALUE) || value_len == 1)
		return M_hashtable_remove(h, key, destroy_vals);

	value = M_list_take_at(entry->value.multi_value, idx);
	if (destroy_vals)
		h->value_free(value);

	return M_TRUE;

}


M_uint32 M_hashtable_size(const M_hashtable_t *h)
{
	if (h == NULL)
		return 0;

	return h->size;
}


size_t M_hashtable_num_collisions(const M_hashtable_t *h)
{
	if (h == NULL)
		return 0;

	return h->num_collisions;
}


size_t M_hashtable_num_expansions(const M_hashtable_t *h)
{
	if (h == NULL)
		return 0;

	return h->num_expansions;
}


size_t M_hashtable_num_keys(const M_hashtable_t *h)
{
	if (h == NULL)
		return 0;

	return h->num_keys;
}


size_t M_hashtable_enumerate(const M_hashtable_t *h, M_hashtable_enum_t *hashenum)
{
	if (h == NULL || hashenum == NULL)
		return 0;

	M_mem_set(hashenum, 0, sizeof(*hashenum));
	if (h->flags & M_HASHTABLE_KEYS_ORDERED) {
		hashenum->entry.ordered.keynode = M_llist_first(h->keys);
	}
	return h->num_values;
}

static M_bool M_hashtable_enumerate_next_unordered(const M_hashtable_t *h, M_hashtable_enum_t *hashenum, const void **key, const void **value)
{
	M_uint32                   i;
	size_t                     idx;
	struct M_hashtable_bucket *ptr;
	const void                *myvalue;

	if (key) {
		*key = NULL;
	}
	if (value) {
		*value = NULL;
	}

	/* Go though each bucket looking for something in them. */
	for (i=hashenum->entry.unordered.hash; i<h->size; i++) {
		ptr = &h->buckets[i];
		/* having a key tell us there is something in the bucket. */
		if (ptr->key != NULL) {
			/* We're keeping track of which item in the chain we're currently processing.
			 * Since this is a linked list we have to start at the beginning and keep going
			 * until we find the one we want.
			 *
			 * We start the idx at one so we will read the bucket first. Then the chainid will
			 * be incremented and the next go around we'll start going though any chained items
			 * (if there are any).
			 */
			for (idx = 1; idx <= hashenum->entry.unordered.chainid; idx++) {
				ptr = ptr->next;
				/* No next means we've exhausted this chain and we need to move onto the
				 * next bucket. */
				if (ptr == NULL) {
					break;
				}
			}

			/* We have an item in the chain. */
			if (ptr != NULL) {
				/* Get the value */
				if (h->flags & M_HASHTABLE_MULTI_VALUE) {
					myvalue = M_list_at(ptr->value.multi_value, hashenum->valueidx);
					hashenum->valueidx++;
				} else {
					myvalue = ptr->value.value;
				}

				if (key)
					*key   = ptr->key;
				if (value)
					*value = myvalue;

				/* Advance if we've run out of values. */
				if (!(h->flags & M_HASHTABLE_MULTI_VALUE) ||
					((h->flags & M_HASHTABLE_MULTI_VALUE) &&
						hashenum->valueidx >= M_list_len(ptr->value.multi_value)))
				{
					/* The for loop exiting puts idx at chainid+1. We want to start
					 * our next run on the next item in the chain. */
					hashenum->entry.unordered.chainid = idx;
					/* New item reset the starting value. */
					hashenum->valueidx                = 0;
				}

				/* Save which bucket we're currently processing so the next call
				 * will start on it. */
				hashenum->entry.unordered.hash = i;
				return M_TRUE;
			}
		}

		/* We're moving onto a new hash. Reset the chain id since we're going to start going though all items
		 * in the chain for the next bucket we find that has a value. The valueidx has already been reset when
		 * we got to the end of those values. */
		hashenum->entry.unordered.chainid = 0;
	}

	return M_FALSE;
}

static M_bool M_hashtable_enumerate_next_ordered(const M_hashtable_t *h, M_hashtable_enum_t *hashenum, const void **key, const void **value)
{
	const void *mykey     = NULL;
	void       *myvalue   = NULL;
	size_t      multi_len = 0;

	if (key)
		*key  = NULL;
	if (value)
		*value = NULL;

	if (hashenum->entry.ordered.keynode == NULL) {
		return M_FALSE;
	}

	mykey = M_llist_node_val(hashenum->entry.ordered.keynode);
	if (key)
		*key = mykey;

	if (h->flags & M_HASHTABLE_MULTI_VALUE) {
		if (!M_hashtable_multi_get(h, mykey, hashenum->valueidx, &myvalue)) {
			return M_FALSE;
		}
		hashenum->valueidx++;
		if (!M_hashtable_multi_len(h, mykey, &multi_len)) {
			return M_FALSE;
		}
		if (hashenum->valueidx >= multi_len) {
			hashenum->entry.ordered.keynode = M_llist_node_next(hashenum->entry.ordered.keynode);
			hashenum->valueidx = 0;
		}
	} else {
		/* We know this will return a result */
		(void)M_hashtable_get(h, mykey, &myvalue);
		hashenum->entry.ordered.keynode = M_llist_node_next(hashenum->entry.ordered.keynode);
	}

	if (value) {
		*value = myvalue;
	}

	return M_TRUE;
}

M_bool M_hashtable_enumerate_next(const M_hashtable_t *h, M_hashtable_enum_t *hashenum, const void **key, const void **value)
{
	if (h == NULL || hashenum == NULL)
		return M_FALSE;

	if (h->flags & M_HASHTABLE_KEYS_ORDERED) {
		return M_hashtable_enumerate_next_ordered(h, hashenum, key, value);
	} else {
		return M_hashtable_enumerate_next_unordered(h, hashenum, key, value);
	}
}


void M_hashtable_merge(M_hashtable_t **dest, M_hashtable_t *src)
{
	M_hashtable_t                *h3;
	M_hashtable_t                *hm;
	const void                   *key;
	const void                   *value;
	M_hashtable_enum_t            hashenum;
	struct M_hashtable_callbacks  callbacks;

	if (dest == NULL || src == NULL)
		return;

	if (*dest == NULL) {
		*dest = src;
		return;
	}

	/* Create a h for tracking keys that are already present in dest. These keys will need to be
	 * destroyed since we can't move them to dest. */
	M_mem_set(&callbacks, 0, sizeof(callbacks));
	callbacks.key_duplicate_insert   = src->key_duplicate_insert;
	callbacks.key_duplicate_copy     = src->key_duplicate_copy;
	callbacks.key_free               = src->key_free;
	h3 = M_hashtable_create(src->size, src->fillpct, src->key_hash, src->key_equality, src->flags, &callbacks);
	/* hm is for tracking keys that have been put into other hashtables. We don't want anything freed. */
	hm = M_hashtable_create(src->size, src->fillpct, src->key_hash, src->key_equality, src->flags, NULL);

	/* Since will be doing direct pointer copying of keys and values,
	 * rather than reallocing and freeing the old ones.  Make sure
	 * the free() callbacks are both no-ops. We can't destory
	 * src because it has pointers to keys and values that are
	 * in dest. */
	src->key_free   = M_hashtable_free_func_default;
	src->value_free = M_hashtable_free_func_default;

	/* Enumerate the table to be merged into our destination, and insert the
	 * direct key/value pairs WITHOUT duplicating them */
	if (M_hashtable_enumerate(src, &hashenum) != 0) {
		/* Multi-value src will have enumerate_next return key multiple times
		 * with each value. If dest is a multi-value all of the values will
		 * end up being added. Otherwise only the last value from src will
		 * make it into dest. */
		while (M_hashtable_enumerate_next(src, &hashenum, &key, &value)) {
			/* Track keys from src that are in dest and src so we can destroy them.
			 * If the key arleady exists in dest, dest's key won't be touched.
			 * The src key needs to be destoryed so we put it in a temporary to
			 * track it for destruction later. */
			if (M_hashtable_get(*dest, key, NULL)) {
				/* A multi value will hae enumerate return the key multiple times.
				 * Once for each value. We need to track if we put the key
				 * into dest once. If we did we can't add to h3 and destory
				 * the key. We need to track if the key was moved into src
				 * so we don't destory it on subsequent times it's seen. */
				if (src->flags & M_HASHTABLE_MULTI_VALUE) {
					/* In dest and wasn't moved into dest previously. */
					if (!M_hashtable_get(hm, key, NULL)) {
						M_hashtable_insert_direct(h3, M_HASHTABLE_INSERT_NODUP, key, NULL);
					}
				} else {
					M_hashtable_insert_direct(h3, M_HASHTABLE_INSERT_NODUP, key, NULL);
				}
			}
			M_hashtable_insert_direct(*dest, M_HASHTABLE_INSERT_NODUP, key, value);
			/* We've seen this key at least once. We've already moved it into dest or h3.
			 * Keep track of this so we don't try putting it into h3 multiple times. */
			if (src->flags & M_HASHTABLE_MULTI_VALUE && !M_hashtable_get(hm, key, NULL)) {
				M_hashtable_insert_direct(hm, M_HASHTABLE_INSERT_NODUP, key, NULL);
			}

			/* See if we need to rehash it because we added so many entries */
			if (M_hashtable_exceeds_load(*dest))
				M_hashtable_rehash_or_destroy(*dest, M_FALSE, M_FALSE);
		}
	}

	/* Free any pointers in use by the source h as we destroy it. Since
	 * we disallow freeing of the actualy key/value pairs earlier, this should
	 * be safe */
	M_hashtable_destroy(src, M_FALSE);
	M_hashtable_destroy(hm, M_FALSE);
	M_hashtable_destroy(h3, M_FALSE);
}


M_hashtable_t *M_hashtable_duplicate(const M_hashtable_t *h)
{
	M_hashtable_t                *dest;
	M_hashtable_enum_t            hashenum;
	const void                   *key;
	const void                   *value;
	struct M_hashtable_callbacks  callbacks;

	if (h == NULL) {
		return NULL;
	}

	/* Duplicate any callbacks */
	callbacks.key_duplicate_insert   = h->key_duplicate_insert;
	callbacks.key_duplicate_copy     = h->key_duplicate_copy;
	callbacks.key_free               = h->key_free;
	callbacks.value_duplicate_insert = h->value_duplicate_insert;
	callbacks.value_duplicate_copy   = h->value_duplicate_copy;
	callbacks.value_equality         = h->value_equality;
	callbacks.value_free             = h->value_free;

	/* Initialize new h with same parameter as original */
	dest = M_hashtable_create(h->size, h->fillpct, h->key_hash, h->key_equality, h->flags, &callbacks);

	/* Enumerate the table to be duplicated, and insert the key/value pairs */
	if (M_hashtable_enumerate(h, &hashenum) != 0) {
		while (M_hashtable_enumerate_next(h, &hashenum, &key, &value)) {
			M_hashtable_insert_int(dest, M_FALSE, key, value);
		}
	}

	return dest;
}

