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
#include "ini/m_ini_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define M_INI_KVS_FILLPCT 75

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The kv store is essentially an order hash table. */
struct M_ini_kvs {
	M_hash_strvp_t *dict;        /*!< Hashtable of string = M_ini_elements_t */
	M_list_str_t   *keys;        /*!< Ordered list of dict keys. Kept in insertion order. */
	M_uint64        entry_count; /*!< Track the number of values (keys can have multiple values)
	                                  contained in the store. */
};

/*! Track where we are when enumerating. */
struct M_ini_kvs_enum {
	size_t next_idx;     /*!< The index in key which is what key were are on. */
	size_t next_sub_idx; /*!< The index in the list of values for the current key we are on. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Ensure that the key exists in the dictionary.
 * \param dict The kv store.
 * \param key The key
 */
static void M_ini_kvs_ensure_key(M_ini_kvs_t *dict, const char *key)
{
	if (!M_ini_kvs_has_key(dict, key)) {
		M_hash_strvp_insert(dict->dict, key, M_list_str_create(M_LIST_STR_CASECMP));
		M_list_str_insert(dict->keys, key);
	}
}

/*! Thin wrapper to conform to prototype for M_hash_strvp free val callback. */
static void M_ini_kvs_value_free(void *arg)
{
	M_list_str_destroy(arg);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_kvs_t *M_ini_kvs_create(void)
{
	M_ini_kvs_t *out;

	out               = M_malloc(sizeof(*out));
	out->dict         = M_hash_strvp_create(8, M_INI_KVS_FILLPCT, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_CASECMP, M_ini_kvs_value_free);
	out->keys         = M_list_str_create(M_LIST_STR_CASECMP);
	out->entry_count  = 0;

	return out;
}

void M_ini_kvs_destroy(M_ini_kvs_t *dict)
{
	if (dict == NULL)
		return;
	M_hash_strvp_destroy(dict->dict, M_TRUE);
	dict->dict = NULL;
	M_list_str_destroy(dict->keys);
	dict->keys = NULL;
	dict->entry_count = 0;
	M_free(dict);
}

M_list_str_t *M_ini_kvs_keys(M_ini_kvs_t *dict)
{
	return M_list_str_duplicate(dict->keys);
}

M_bool M_ini_kvs_has_key(M_ini_kvs_t *dict, const char *key)
{
	if (dict == NULL || dict->dict == NULL)
		return M_FALSE;
	return M_hash_strvp_get(dict->dict, key, NULL);
}

M_bool M_ini_kvs_rename(M_ini_kvs_t *dict, const char *key, const char *new_key)
{
	void   *vals;

	if (dict == NULL || dict->dict == NULL || M_str_isempty(key) || M_str_isempty(new_key) ||
	    key[M_str_len(key)-1] == '/' || new_key[M_str_len(new_key)-1] == '/')
	{
		return M_FALSE;
	}
	if (M_str_caseeq(key, new_key))
		return M_TRUE;

	if (!M_hash_strvp_get(dict->dict, key, &vals)) {
		return M_FALSE;
	}
	M_hash_strvp_remove(dict->dict, key, M_FALSE);
	M_hash_strvp_insert(dict->dict, new_key, vals);
	M_list_str_replace_val(dict->keys, key, new_key, M_LIST_STR_MATCH_VAL);

	return M_TRUE;
}

M_bool M_ini_kvs_val_add_key(M_ini_kvs_t *dict, const char *key)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL)
		return M_FALSE;
	M_ini_kvs_ensure_key(dict, key);
	return M_TRUE;
}

M_bool M_ini_kvs_val_set(M_ini_kvs_t *dict, const char *key, const char *value)
{
	if (!M_ini_kvs_val_remove_all(dict, key)) {
		return M_FALSE;
	}
	return M_ini_kvs_val_insert(dict, key, value);
}

M_bool M_ini_kvs_val_insert(M_ini_kvs_t *dict, const char *key, const char *value)
{
	M_list_str_t *vals;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return M_FALSE;

	M_ini_kvs_ensure_key(dict, key);
	vals = M_hash_strvp_get_direct(dict->dict, key);

	if (M_list_str_insert(vals, value)) {
		dict->entry_count++;
		return M_TRUE;
	}
	return M_FALSE;
}

M_bool M_ini_kvs_val_remove_at(M_ini_kvs_t *dict, const char *key, size_t idx)
{
	M_list_str_t *vals;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(dict->dict, key, NULL)) {
		return M_FALSE;
	}

	if (!M_ini_kvs_has_key(dict, key))
		return M_TRUE;

	vals = M_hash_strvp_get_direct(dict->dict, key);
	if (M_list_str_remove_at(vals, idx)) {
		dict->entry_count--;
		return M_TRUE;
	}
	return M_FALSE;
}

M_bool M_ini_kvs_val_remove_all(M_ini_kvs_t *dict, const char *key)
{
	M_list_str_t *vals;
	size_t        len;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return M_FALSE;

	if (!M_ini_kvs_has_key(dict, key))
		return M_TRUE;

	vals = M_hash_strvp_get_direct(dict->dict, key);
	len  = M_list_str_len(vals);
	if (len == 0) {
		return M_TRUE;
	}
	if (M_list_str_remove_range(vals, 0, len)) {
		dict->entry_count -= len;
		return M_TRUE;
	}
	return M_FALSE;
}

size_t M_ini_kvs_val_len(M_ini_kvs_t *dict, const char *key)
{
	M_list_str_t *vals;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return 0;

	if (!M_ini_kvs_has_key(dict, key))
		return 0;
	
	vals = M_hash_strvp_get_direct(dict->dict, key);
	return M_list_str_len(vals);
}

M_bool M_ini_kvs_val_get(M_ini_kvs_t *dict, const char *key, size_t idx, const char **value)
{
	M_list_str_t *vals;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return 0;
	if (value != NULL)
		*value = NULL;

	if (!M_ini_kvs_has_key(dict, key))
		return M_FALSE;

	vals = M_hash_strvp_get_direct(dict->dict, key);
	if (idx >= M_list_str_len(vals)) {
		return M_FALSE;
	}

	if (value != NULL)
		*value = M_list_str_at(vals, idx);
	return M_TRUE;
}

const char *M_ini_kvs_val_get_direct(M_ini_kvs_t *dict, const char *key, size_t idx)
{
	const char *value;

	if (!M_ini_kvs_val_get(dict, key, idx, &value)) {
		return NULL;
	}
	return value;
}

M_bool M_ini_kvs_remove(M_ini_kvs_t *dict, const char *key)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || key == NULL)
		return 0;
	if (M_list_str_remove_val(dict->keys, key, M_LIST_STR_MATCH_VAL) == 0)
		return M_FALSE;
	return M_hash_strvp_remove(dict->dict, key, M_TRUE);
}

M_uint64 M_ini_kvs_size(M_ini_kvs_t *dict)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL)
		return 0;
	return M_hash_strvp_size(dict->dict);
}

M_uint64 M_ini_kvs_num_collisions(M_ini_kvs_t *dict)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL)
		return 0;
	return M_hash_strvp_num_collisions(dict->dict);
}

M_uint64 M_ini_kvs_num_expansions(M_ini_kvs_t *dict)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL)
		return 0;
	return M_hash_strvp_num_expansions(dict->dict);
}

M_uint64 M_ini_kvs_num_keys(M_ini_kvs_t *dict)
{
	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL)
		return 0;
	return M_hash_strvp_num_keys(dict->dict);
}

M_uint64 M_ini_kvs_enumerate(M_ini_kvs_t *dict, M_ini_kvs_enum_t **dictenum)
{
	if (dictenum != NULL)
		*dictenum = NULL;

	if (dict == NULL || dict->dict == NULL || dict->keys ==  NULL || dictenum == NULL)
		return 0;

	*dictenum = M_malloc_zero(sizeof(**dictenum));
	return dict->entry_count;
}

M_bool M_ini_kvs_enumerate_next(M_ini_kvs_t *dict, M_ini_kvs_enum_t *dictenum, const char **key, size_t *idx, const char **value)
{
	const char     *mykey;
	M_list_str_t   *vals;

	if (dict == NULL || dict->dict == NULL || dict->keys == NULL || dictenum == NULL || dictenum->next_idx >= M_ini_kvs_num_keys(dict)) {
		if (key)
			*key  = NULL;
		if (value)
			*value = NULL;
		return M_FALSE;
	}

	mykey = M_list_str_at(dict->keys, dictenum->next_idx);
	if (key)
		*key   = mykey;

	vals = M_hash_strvp_get_direct(dict->dict, mykey);
	if (value)
		*value = M_list_str_at(vals, dictenum->next_sub_idx);
	if (idx)
		*idx = dictenum->next_sub_idx;

	dictenum->next_sub_idx++;
	if (dictenum->next_sub_idx >= M_list_str_len(vals)) {
		dictenum->next_sub_idx = 0;
		dictenum->next_idx++;
	}

	return M_TRUE;
}

void M_ini_kvs_enumerate_free(M_ini_kvs_enum_t *dictenum)
{
	M_free(dictenum);
}

void M_ini_kvs_merge(M_ini_kvs_t **dest, M_ini_kvs_t *src)
{
	M_ini_kvs_enum_t *kvsenum;
	const char       *key;
	const char       *val;

	if (dest == NULL || src == NULL)
		return;

	if (*dest == NULL) {
		*dest = src;
		return;
	}

	M_ini_kvs_enumerate(src, &kvsenum);
	while (M_ini_kvs_enumerate_next(src, kvsenum, &key, NULL, &val)) {
		M_ini_kvs_val_insert(*dest, key, val);
	}
	M_ini_kvs_enumerate_free(kvsenum);

	M_ini_kvs_destroy(src);
}

M_ini_kvs_t *M_ini_kvs_duplicate(M_ini_kvs_t *dict)
{
	M_ini_kvs_t      *out;
	M_ini_kvs_enum_t *kvsenum;
	const char       *key;
	const char       *val;

	if (dict == NULL)
		return NULL;

	out = M_ini_kvs_create();

	M_ini_kvs_enumerate(dict, &kvsenum);
	while (M_ini_kvs_enumerate_next(dict, kvsenum, &key, NULL, &val)) {
		M_ini_kvs_val_insert(out, key, val);
	}
	M_ini_kvs_enumerate_free(kvsenum);

	return out;
}
