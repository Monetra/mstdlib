/* The MIT License (MIT)
 * 
 * Copyright (c) 2016 Monetra Technologies, LLC.
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

/* Data format type_id - size - data */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_hash_multi
{
	M_hash_u64vp_t *table_int;
	M_hash_u64vp_t *table_int_destroy;

	M_hash_strvp_t *table_str;
	M_hash_strvp_t *table_str_destroy;
};

struct M_hash_multi_object;
typedef struct M_hash_multi_object M_hash_multi_object_t;

typedef enum {
	M_HASH_MULTI_KEY_TYPE_INT = 0,
	M_HASH_MULTI_KEY_TYPE_STR
} M_hash_multi_key_type_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_hash_multi_val_type_t M_hash_multi_val_type(M_hash_multi_object_t *o)
{
	M_hash_multi_val_type_t val_type;

	if (o == NULL)
		return M_HASH_MULTI_VAL_TYPE_UNKNOWN;

	M_mem_copy(&val_type, o, sizeof(val_type));

	switch (val_type) {
		case M_HASH_MULTI_VAL_TYPE_UNKNOWN:
		case M_HASH_MULTI_VAL_TYPE_BOOL:
		case M_HASH_MULTI_VAL_TYPE_INT:
		case M_HASH_MULTI_VAL_TYPE_STR:
		case M_HASH_MULTI_VAL_TYPE_BIN:
		case M_HASH_MULTI_VAL_TYPE_VP:
			break;
		default:
			val_type = M_HASH_MULTI_VAL_TYPE_UNKNOWN;
			break;
	}

	return val_type;
}

static M_hash_multi_object_t *M_hash_multi_create_object(M_hash_multi_val_type_t val_type, const unsigned char *val, size_t num_bytes)
{
	unsigned char *o;

	if (val == NULL)
		return NULL;

	o = M_malloc_zero(M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT+num_bytes);
	M_mem_copy(o, &val_type, sizeof(val_type));
	M_mem_copy(o+M_SAFE_ALIGNMENT, &num_bytes, sizeof(num_bytes));
	/* Only bin allows a NULL value because it also has a length.
 	 * Someone might want to store there was no data for a field. */
	if (val != NULL && num_bytes != 0)
		M_mem_copy(o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT, val, num_bytes);

	return (M_hash_multi_object_t *)o;
}

static M_hash_multi_object_t *M_hash_multi_create_object_bool(M_bool val)
{
	return M_hash_multi_create_object(M_HASH_MULTI_VAL_TYPE_BOOL, (const unsigned char *)&val, sizeof(val));
}

static M_hash_multi_object_t *M_hash_multi_create_object_int(M_uint64 val)
{
	return M_hash_multi_create_object(M_HASH_MULTI_VAL_TYPE_INT, (const unsigned char *)&val, sizeof(val));
}

static M_hash_multi_object_t *M_hash_multi_create_object_str(const char *val)
{
	return M_hash_multi_create_object(M_HASH_MULTI_VAL_TYPE_STR, (const unsigned char *)val, M_str_len(val)+1);
}

static M_hash_multi_object_t *M_hash_multi_create_object_bin(const unsigned char *val, size_t len)
{
	return M_hash_multi_create_object(M_HASH_MULTI_VAL_TYPE_BIN, val, len);
}

static M_hash_multi_object_t *M_hash_multi_create_object_vp(void *val)
{
	return M_hash_multi_create_object(M_HASH_MULTI_VAL_TYPE_VP, (const unsigned char *)&val, sizeof(val));
}

static M_bool M_hash_multi_get_object_bool(M_hash_multi_object_t *o, M_bool *val)
{
	if (o == NULL)
		return M_FALSE;

	if (M_hash_multi_val_type(o) != M_HASH_MULTI_VAL_TYPE_BOOL)
		return M_FALSE;

	if (val != NULL)
		M_mem_copy(val, (const unsigned char *)o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT, sizeof(*val));

	return M_TRUE;
}

static M_bool M_hash_multi_get_object_int(M_hash_multi_object_t *o, M_uint64 *val)
{
	if (o == NULL)
		return M_FALSE;

	if (M_hash_multi_val_type(o) != M_HASH_MULTI_VAL_TYPE_INT)
		return M_FALSE;

	if (val != NULL)
		M_mem_copy(val, (const unsigned char *)o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT, sizeof(*val));

	return M_TRUE;
}

static M_bool M_hash_multi_get_object_str(M_hash_multi_object_t *o, const char **val)
{
	if (o == NULL)
		return M_FALSE;

	if (M_hash_multi_val_type(o) != M_HASH_MULTI_VAL_TYPE_STR)
		return M_FALSE;

	if (val != NULL)
		*val = (const char *)o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT;

	return M_TRUE;
}

static M_bool M_hash_multi_get_object_bin(M_hash_multi_object_t *o, const unsigned char **val, size_t *len)
{
	if (o == NULL)
		return M_FALSE;

	if (M_hash_multi_val_type(o) != M_HASH_MULTI_VAL_TYPE_BIN)
		return M_FALSE;

	/* Val and len could be NULL because this is only checking the type. */
	if (len != NULL)
		M_mem_copy(len, (const unsigned char *)o+M_SAFE_ALIGNMENT, sizeof(*len));

	if (val != NULL) {
		*val = NULL;
		if (len != NULL && *len != 0) {
			*val = (const unsigned char *)o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT;
		}
	}

	return M_TRUE;
}

static M_bool M_hash_multi_get_object_vp(M_hash_multi_object_t *o, void **val)
{
	if (o == NULL)
		return M_FALSE;

	if (M_hash_multi_val_type(o) != M_HASH_MULTI_VAL_TYPE_VP)
		return M_FALSE;

	if (val != NULL)
		M_mem_copy(val, (const unsigned char *)o+M_SAFE_ALIGNMENT+M_SAFE_ALIGNMENT, sizeof(*val));

	return M_TRUE;
}

static void M_hash_multi_destroy_object(M_hash_multi_object_t *o, M_hash_multi_free_func free_func)
{
	void                    *vp_val;
	M_hash_multi_val_type_t  val_type;

	if (o == NULL)
		return;

	val_type = M_hash_multi_val_type(o);
	switch (val_type) {
		case M_HASH_MULTI_VAL_TYPE_UNKNOWN:
		case M_HASH_MULTI_VAL_TYPE_BOOL:
		case M_HASH_MULTI_VAL_TYPE_INT:
		case M_HASH_MULTI_VAL_TYPE_STR:
		case M_HASH_MULTI_VAL_TYPE_BIN:
			break;
		case M_HASH_MULTI_VAL_TYPE_VP:
			if (free_func != NULL && M_hash_multi_get_object_vp(o, &vp_val)) {
				free_func(vp_val);
			}
			break;
	}
	M_free(o);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_hash_multi_t *M_hash_multi_create(M_uint32 flags)
{
	M_hash_multi_t       *h;
	M_hash_strvp_flags_t  str_flags = M_HASH_STRVP_NONE;

	h = M_malloc(sizeof(*h));

	h->table_int         = M_hash_u64vp_create(16, 75, M_HASH_U64VP_NONE, NULL);
	h->table_int_destroy = M_hash_u64vp_create(16, 75, M_HASH_U64VP_NONE, NULL);

	if (flags & M_HASH_MULTI_STR_CASECMP)
		str_flags |= M_HASH_STRVP_CASECMP;
	h->table_str         = M_hash_strvp_create(16, 75, str_flags, NULL);
	h->table_str_destroy = M_hash_strvp_create(16, 75, str_flags, NULL);

	return h;
}

void M_hash_multi_destroy(M_hash_multi_t *h)
{
	M_hash_u64vp_enum_t *henum_int;
	M_hash_strvp_enum_t *henum_str;
	M_uint64             key_int;
	const char          *key_str;
	void                *val;


	if (h == NULL)
		return;

	/* Destroy vals with int key */
	M_hash_u64vp_enumerate(h->table_int, &henum_int);
	while (M_hash_u64vp_enumerate_next(h->table_int, henum_int, &key_int, &val)) {
		M_hash_multi_destroy_object(val, (M_hash_multi_free_func)M_hash_u64vp_get_direct(h->table_int_destroy, key_int));
	}
	M_hash_u64vp_enumerate_free(henum_int);

	/* Destroy vals with str key */
	M_hash_strvp_enumerate(h->table_str, &henum_str);
	while (M_hash_strvp_enumerate_next(h->table_str, henum_str, &key_str, &val)) {
		M_hash_multi_destroy_object(val, (M_hash_multi_free_func)M_hash_strvp_get_direct(h->table_str_destroy, key_str));
	}
	M_hash_strvp_enumerate_free(henum_str);

	/* Destroy containers */
	M_hash_u64vp_destroy(h->table_int, M_FALSE);
	M_hash_u64vp_destroy(h->table_int_destroy, M_FALSE);
	M_hash_strvp_destroy(h->table_str, M_FALSE);
	M_hash_strvp_destroy(h->table_str_destroy, M_FALSE);

	M_free(h);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_u64_insert_bool(M_hash_multi_t *h, M_uint64 key, M_bool val)
{
	if (h == NULL)
		return M_FALSE;

	M_hash_multi_u64_remove(h, key, M_TRUE);
	return M_hash_u64vp_insert(h->table_int, (M_uint64)key, M_hash_multi_create_object_bool(val));
}

M_bool M_hash_multi_u64_insert_int(M_hash_multi_t *h, M_uint64 key, M_int64 val)
{
	if (h == NULL)
		return M_FALSE;

	M_hash_multi_u64_remove(h, key, M_TRUE);
	return M_hash_u64vp_insert(h->table_int, (M_uint64)key, M_hash_multi_create_object_int((M_uint64)val));
}

M_bool M_hash_multi_u64_insert_uint(M_hash_multi_t *h, M_uint64 key, M_uint64 val)
{
	return M_hash_multi_u64_insert_int(h, key, (M_int64)val);
}

M_bool M_hash_multi_u64_insert_str(M_hash_multi_t *h, M_uint64 key, const char *val)
{
	if (h == NULL)
		return M_FALSE;

	M_hash_multi_u64_remove(h, key, M_TRUE);
	return M_hash_u64vp_insert(h->table_int, key, M_hash_multi_create_object_str(M_str_safe(val)));
}

M_bool M_hash_multi_u64_insert_bin(M_hash_multi_t *h, M_uint64 key, const unsigned char *val, size_t len)
{
	if (h == NULL)
		return M_FALSE;

	M_hash_multi_u64_remove(h, key, M_TRUE);
	return M_hash_u64vp_insert(h->table_int, key, M_hash_multi_create_object_bin(val, len));
}

M_bool M_hash_multi_u64_insert_vp(M_hash_multi_t *h, M_uint64 key, void *val, M_hash_multi_free_func val_free)
{
	if (h == NULL || val == NULL)
		return M_FALSE;

	M_hash_multi_u64_remove(h, key, M_TRUE);
	if (!M_hash_u64vp_insert(h->table_int, key, M_hash_multi_create_object_vp(val)))
		return M_FALSE;

	if (val_free != NULL)
		M_hash_u64vp_insert(h->table_int_destroy, key, (void *)val_free);

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_u64_get_bool(M_hash_multi_t *h, M_uint64 key, M_bool *val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_bool(o, val);
}

M_bool M_hash_multi_u64_get_int(M_hash_multi_t *h, M_uint64 key, M_int64 *val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_int(o, (M_uint64 *)val);
}

M_bool M_hash_multi_u64_get_uint(M_hash_multi_t *h, M_uint64 key, M_uint64 *val)
{
	return M_hash_multi_u64_get_int(h, key, (M_int64 *)val);
}

M_bool M_hash_multi_u64_get_str(M_hash_multi_t *h, M_uint64 key, const char **val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_str(o, val);
}

M_bool M_hash_multi_u64_get_bin(M_hash_multi_t *h, M_uint64 key, const unsigned char **val, size_t *len)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_bin(o, val, len);
}

M_bool M_hash_multi_u64_get_vp(M_hash_multi_t *h, M_uint64 key, void **val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_vp(o, val);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_u64_remove(M_hash_multi_t *h, M_uint64 key, M_bool destroy_vp)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_FALSE;

	M_hash_multi_destroy_object(o, destroy_vp?(M_hash_multi_free_func)M_hash_u64vp_get_direct(h->table_int_destroy, key):NULL);
	M_hash_u64vp_remove(h->table_int_destroy, key, M_FALSE);
	return M_hash_u64vp_remove(h->table_int, key, M_FALSE);
}

M_hash_multi_val_type_t M_hash_multi_u64_type(const M_hash_multi_t *h, M_uint64 key)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_HASH_MULTI_VAL_TYPE_UNKNOWN;

	if (!M_hash_u64vp_get(h->table_int, key, (void **)&o))
		return M_HASH_MULTI_VAL_TYPE_UNKNOWN;

	return M_hash_multi_val_type(o);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_str_insert_bool(M_hash_multi_t *h, const char *key, M_bool val)
{
	if (h == NULL || M_str_isempty(key))
		return M_FALSE;

	M_hash_multi_str_remove(h, key, M_TRUE);
	return M_hash_strvp_insert(h->table_str, key, M_hash_multi_create_object_bool(val));
}

M_bool M_hash_multi_str_insert_int(M_hash_multi_t *h, const char *key, M_int64 val)
{
	if (h == NULL || M_str_isempty(key))
		return M_FALSE;

	M_hash_multi_str_remove(h, key, M_TRUE);
	return M_hash_strvp_insert(h->table_str, key, M_hash_multi_create_object_int((M_uint64)val));
}

M_bool M_hash_multi_str_insert_uint(M_hash_multi_t *h, const char *key, M_uint64 val)
{
	return M_hash_multi_str_insert_int(h, key, (M_int64)val);
}

M_bool M_hash_multi_str_insert_str(M_hash_multi_t *h, const char *key, const char *val)
{
	if (h == NULL || M_str_isempty(key))
		return M_FALSE;

	M_hash_multi_str_remove(h, key, M_TRUE);
	return M_hash_strvp_insert(h->table_str, key, M_hash_multi_create_object_str(M_str_safe(val)));
}

M_bool M_hash_multi_str_insert_bin(M_hash_multi_t *h, const char *key, const unsigned char *val, size_t len)
{
	if (h == NULL)
		return M_FALSE;

	M_hash_multi_str_remove(h, key, M_TRUE);
	return M_hash_strvp_insert(h->table_str, key, M_hash_multi_create_object_bin(val, len));
}

M_bool M_hash_multi_str_insert_vp(M_hash_multi_t *h, const char *key, void *val, M_hash_multi_free_func val_free)
{
	if (h == NULL || val == NULL)
		return M_FALSE;

	M_hash_multi_str_remove(h, key, M_TRUE);
	if (!M_hash_strvp_insert(h->table_str, key, M_hash_multi_create_object_vp(val)))
		return M_FALSE;

	if (val_free != NULL)
		M_hash_strvp_insert(h->table_str_destroy, key, (void *)val_free);

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_str_get_bool(M_hash_multi_t *h, const char *key, M_bool *val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_bool(o, val);
}

M_bool M_hash_multi_str_get_int(M_hash_multi_t *h, const char *key, M_int64 *val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_int(o, (M_uint64 *)val);
}

M_bool M_hash_multi_str_get_uint(M_hash_multi_t *h, const char *key, M_uint64 *val)
{
	return M_hash_multi_str_get_int(h, key, (M_int64 *)val);
}

M_bool M_hash_multi_str_get_str(M_hash_multi_t *h, const char *key, const char **val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_str(o, val);
}

M_bool M_hash_multi_str_get_bin(M_hash_multi_t *h, const char *key, const unsigned char **val, size_t *len)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_bin(o, val, len);
}

M_bool M_hash_multi_str_get_vp(M_hash_multi_t *h, const char *key, void **val)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	return M_hash_multi_get_object_vp(o, val);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_hash_multi_str_remove(M_hash_multi_t *h, const char *key, M_bool destroy_vp)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_FALSE;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_FALSE;

	M_hash_multi_destroy_object(o, destroy_vp?(M_hash_multi_free_func)M_hash_strvp_get_direct(h->table_str_destroy, key):NULL);
	M_hash_strvp_remove(h->table_str_destroy, key, M_FALSE);
	return M_hash_strvp_remove(h->table_str, key, M_FALSE);
}

M_hash_multi_val_type_t M_hash_multi_str_type(const M_hash_multi_t *h, const char *key)
{
	M_hash_multi_object_t *o;

	if (h == NULL)
		return M_HASH_MULTI_VAL_TYPE_UNKNOWN;

	if (!M_hash_strvp_get(h->table_str, key, (void **)&o))
		return M_HASH_MULTI_VAL_TYPE_UNKNOWN;

	return M_hash_multi_val_type(o);
}

