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
#include <mstdlib/mstdlib_formats.h>
#include "json/m_json_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_json_node_clear(M_json_node_t *node)
{
	if (node == NULL)
		return;

	switch (node->type) {
		case M_JSON_TYPE_OBJECT:
			M_hash_strvp_destroy(node->data.json_object, M_TRUE);
			node->data.json_object = NULL;
			break;
		case M_JSON_TYPE_ARRAY:
			M_list_destroy(node->data.json_array, M_TRUE);
			node->data.json_array = NULL;
			break;
		case M_JSON_TYPE_STRING:
			M_free(node->data.json_string);
			node->data.json_string = NULL;
			break;
		case M_JSON_TYPE_INTEGER:
			node->data.json_integer = 0;
			break;
		case M_JSON_TYPE_DECIMAL:
			M_decimal_from_int(&(node->data.json_decimal), 0, 0);
			break;
		default:
			break;
	}

	node->type = M_JSON_TYPE_UNKNOWN;
}

static void M_json_node_destroy_int(M_json_node_t *node)
{
	if (node == NULL)
		return;
	M_json_node_clear(node);
	node->parent = NULL;
	M_free(node);
}

/*! A wrapper around node_destroy to accomidate the argument types when used with a base type. */
static void M_json_node_destroy_int_vp(void *val)
{
	M_json_node_destroy_int(val);
}

static M_bool M_json_object_remove_node(M_json_node_t *parent_node, M_json_node_t *child_node, M_bool destroy_values)
{
	M_hash_strvp_enum_t *hashenum;
	const char          *key;
	void                *val;
	M_bool               ret = M_FALSE;

	if (parent_node == NULL || parent_node->type != M_JSON_TYPE_OBJECT || child_node == NULL)
		return M_FALSE;

	M_hash_strvp_enumerate(parent_node->data.json_object, &hashenum);
	while (M_hash_strvp_enumerate_next(parent_node->data.json_object, hashenum, &key, &val)) {
		if (child_node == (M_json_node_t *)val) {
			/* Killing the child node, we cannot continue enumerating because the enumeration is no longer valid.
 			 * Hence the break right after the remove. */
			ret = M_hash_strvp_remove(parent_node->data.json_object, key, destroy_values);
			break;
		}
	}
	M_hash_strvp_enumerate_free(hashenum);

	return ret;
}

static M_bool M_json_array_remove_node(M_json_node_t *parent_node, M_json_node_t *child_node, M_bool destroy_values)
{
	size_t len;
	size_t i;

	if (parent_node == NULL || parent_node->type != M_JSON_TYPE_ARRAY || child_node == NULL)
		return M_FALSE;

	len = M_json_array_len(parent_node);
	for (i=0; i<len; i++) {
		if (child_node == M_json_array_at(parent_node, i)) {
			if (destroy_values) {
				return M_list_remove_at(parent_node->data.json_array, i);
			}
			if (M_list_take_at(parent_node->data.json_array, i) != NULL) {
				return M_TRUE;
			}
			break;
		}
	}

	return M_FALSE;
}

static void M_json_node_take_or_destory(M_json_node_t *node, M_bool destroy_values)
{
	M_json_type_t parent_type = M_JSON_TYPE_UNKNOWN;

	if (node == NULL)
		return;

	if (node->parent != NULL)
		parent_type = node->parent->type;

	switch (parent_type) {
		case M_JSON_TYPE_OBJECT:
			M_json_object_remove_node(node->parent, node, destroy_values);
			break;
		case M_JSON_TYPE_ARRAY:
			M_json_array_remove_node(node->parent, node, destroy_values);
			break;
		default:
			if (destroy_values)
				M_json_node_destroy_int(node);
			break;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_json_node_t *M_json_node_create(M_json_type_t type)
{
	M_json_node_t *out;
	struct M_list_callbacks array_callbacks = {
		NULL,
		NULL,
		NULL,
		M_json_node_destroy_int_vp
	};

	out = M_malloc(sizeof(*out));
	M_mem_set(out, 0, sizeof(*out));
	out->type = type;

	switch (out->type) {
		case M_JSON_TYPE_OBJECT:
			out->data.json_object = M_hash_strvp_create(8, 75, M_HASH_STRVP_KEYS_ORDERED, M_json_node_destroy_int_vp);
			break;
		case M_JSON_TYPE_ARRAY:
			out->data.json_array = M_list_create(&array_callbacks, M_LIST_NONE);
			break;
		case M_JSON_TYPE_STRING:
		case M_JSON_TYPE_INTEGER:
		case M_JSON_TYPE_DECIMAL:
		case M_JSON_TYPE_BOOL:
		case M_JSON_TYPE_NULL:
			break;
		default:
			/* A valid type was not set for this node. */
			out->type = M_JSON_TYPE_UNKNOWN;
			M_free(out);
			return NULL;
	}

	return out;
}

void M_json_node_destroy(M_json_node_t *node)
{
	if (node == NULL)
		return;
	M_json_node_take_or_destory(node, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define ERRCASE(x) case x: ret = #x; break

const char *M_json_errcode_to_str(M_json_error_t err)
{
	const char *ret = "<unknown>";

	switch (err) {
		ERRCASE(M_JSON_ERROR_SUCCESS);
		ERRCASE(M_JSON_ERROR_GENERIC);
		ERRCASE(M_JSON_ERROR_MISUSE);
		ERRCASE(M_JSON_ERROR_INVALID_START);
		ERRCASE(M_JSON_ERROR_EXPECTED_END);
		ERRCASE(M_JSON_ERROR_MISSING_COMMENT_CLOSE);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_COMMENT_START);
		ERRCASE(M_JSON_ERROR_INVALID_PAIR_START);
		ERRCASE(M_JSON_ERROR_DUPLICATE_KEY);
		ERRCASE(M_JSON_ERROR_MISSING_PAIR_SEPARATOR);
		ERRCASE(M_JSON_ERROR_OBJECT_UNEXPECTED_CHAR);
		ERRCASE(M_JSON_ERROR_EXPECTED_VALUE);
		ERRCASE(M_JSON_ERROR_UNCLOSED_OBJECT);
		ERRCASE(M_JSON_ERROR_ARRAY_UNEXPECTED_CHAR);
		ERRCASE(M_JSON_ERROR_UNCLOSED_ARRAY);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_NEWLINE);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_CONTROL_CHAR);
		ERRCASE(M_JSON_ERROR_INVALID_UNICODE_ESACPE);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_ESCAPE);
		ERRCASE(M_JSON_ERROR_UNCLOSED_STRING);
		ERRCASE(M_JSON_ERROR_INVALID_BOOL);
		ERRCASE(M_JSON_ERROR_INVALID_NULL);
		ERRCASE(M_JSON_ERROR_INVALID_NUMBER);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_TERMINATION);
		ERRCASE(M_JSON_ERROR_INVALID_IDENTIFIER);
		ERRCASE(M_JSON_ERROR_UNEXPECTED_END);
	}

	return ret;
}

M_json_type_t M_json_node_type(const M_json_node_t *node)
{
	if (node == NULL)
		return M_JSON_TYPE_UNKNOWN;
	return node->type;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_json_node_t *M_json_get_parent(const M_json_node_t *node)
{
	if (node == NULL)
		return NULL;
	return node->parent;
}

void M_json_take_from_parent(M_json_node_t *node)
{
	if (node == NULL || node->parent == NULL)
		return;
	M_json_node_take_or_destory(node, M_FALSE);
	node->parent = NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_json_node_t *M_json_object_value(const M_json_node_t *node, const char *key)
{
	if (node == NULL || node->type != M_JSON_TYPE_OBJECT || key == NULL)
		return NULL;
	return M_hash_strvp_get_direct(node->data.json_object, key);
}

const char *M_json_object_value_string(const M_json_node_t *node, const char *key)
{
	return M_json_get_string(M_json_object_value(node, key));
}

M_int64 M_json_object_value_int(const M_json_node_t *node, const char *key)
{
	return M_json_get_int(M_json_object_value(node, key));
}

const M_decimal_t *M_json_object_value_decimal(const M_json_node_t *node, const char *key)
{
	return M_json_get_decimal(M_json_object_value(node, key));
}

M_bool M_json_object_value_bool(const M_json_node_t *node, const char *key)
{
	return M_json_get_bool(M_json_object_value(node, key));
}

M_list_str_t *M_json_object_keys(const M_json_node_t *node)
{
	M_list_str_t        *keys;
	M_hash_strvp_enum_t *hashenum;
	const char          *key;

	if (node == NULL || node->type != M_JSON_TYPE_OBJECT)
		return NULL;

	keys = M_list_str_create(M_LIST_STR_NONE);
	M_hash_strvp_enumerate(node->data.json_object, &hashenum);
	while (M_hash_strvp_enumerate_next(node->data.json_object, hashenum, &key, NULL)) {
		M_list_str_insert(keys, key);
	}
	M_hash_strvp_enumerate_free(hashenum);

	return keys;
}

M_bool M_json_object_insert(M_json_node_t *node, const char *key, M_json_node_t *value)
{
	if (node == NULL || node->type != M_JSON_TYPE_OBJECT || value->parent != NULL)
		return M_FALSE;

	if (M_hash_strvp_insert(node->data.json_object, key, (void *)value)) {
		value->parent = node;
		return M_TRUE;
	}
	return M_FALSE;
}

M_bool M_json_object_insert_string(M_json_node_t *node, const char *key, const char *value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_STRING);
	if (!M_json_set_string(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_object_insert(node, key, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_object_insert_int(M_json_node_t *node, const char *key, M_int64 value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_INTEGER);
	if (!M_json_set_int(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_object_insert(node, key, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_object_insert_decimal(M_json_node_t *node, const char *key, const M_decimal_t *value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_DECIMAL);
	if (!M_json_set_decimal(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_object_insert(node, key, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_object_insert_bool(M_json_node_t *node, const char *key, M_bool value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_BOOL);
	if (!M_json_set_bool(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_object_insert(node, key, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_json_array_len(const M_json_node_t *node)
{
	if (node == NULL || node->type != M_JSON_TYPE_ARRAY)
		return 0;
	return M_list_len(node->data.json_array);
}

M_json_node_t *M_json_array_at(const M_json_node_t *node, size_t idx)
{
	const void *value;

	if (node == NULL || node->type != M_JSON_TYPE_ARRAY)
		return NULL;

	value = M_list_at(node->data.json_array, idx);
	return M_CAST_OFF_CONST(M_json_node_t *, value);
}

const char *M_json_array_at_string(const M_json_node_t *node, size_t idx)
{
	return M_json_get_string(M_json_array_at(node, idx));
}

M_int64 M_json_array_at_int(const M_json_node_t *node, size_t idx)
{
	return M_json_get_int(M_json_array_at(node, idx));
}

const M_decimal_t *M_json_array_at_decimal(const M_json_node_t *node, size_t idx)
{
	return M_json_get_decimal(M_json_array_at(node, idx));
}

M_bool M_json_array_at_bool(const M_json_node_t *node, size_t idx)
{
	return M_json_get_bool(M_json_array_at(node, idx));
}

M_bool M_json_array_insert(M_json_node_t *node, M_json_node_t *value)
{
	if (node == NULL || node->type != M_JSON_TYPE_ARRAY || value->parent != NULL)
		return M_FALSE;

	if (M_list_insert(node->data.json_array, value)) {
		value->parent = node;
		return M_TRUE;
	}
	return M_FALSE;
}

M_bool M_json_array_insert_string(M_json_node_t *node, const char *value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_STRING);
	if (!M_json_set_string(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert(node, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_int(M_json_node_t *node, M_int64 value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_INTEGER);
	if (!M_json_set_int(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert(node, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_decimal(M_json_node_t *node, const M_decimal_t *value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_DECIMAL);
	if (!M_json_set_decimal(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert(node, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_bool(M_json_node_t *node, M_bool value)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_BOOL);
	if (!M_json_set_bool(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert(node, n)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_at(M_json_node_t *node, M_json_node_t *value, size_t idx)
{
	if (node == NULL || node->type != M_JSON_TYPE_ARRAY)
		return M_FALSE;

	if (M_list_insert_at(node->data.json_array, value, idx)) {
		value->parent = node;
		return M_TRUE;
	}
	return M_FALSE;
}

M_bool M_json_array_insert_at_string(M_json_node_t *node, const char *value, size_t idx)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_STRING);
	if (!M_json_set_string(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert_at(node, n, idx)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_at_int(M_json_node_t *node, M_int64 value, size_t idx)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_INTEGER);
	if (!M_json_set_int(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert_at(node, n, idx)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_at_decimal(M_json_node_t *node, const M_decimal_t *value, size_t idx)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_DECIMAL);
	if (!M_json_set_decimal(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert_at(node, n, idx)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool M_json_array_insert_at_bool(M_json_node_t *node, M_bool value, size_t idx)
{
	M_json_node_t *n;

	n = M_json_node_create(M_JSON_TYPE_BOOL);
	if (!M_json_set_bool(n, value)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	if (!M_json_array_insert_at(node, n, idx)) {
		M_json_node_destroy(n);
		return M_FALSE;
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_json_get_string(const M_json_node_t *node)
{
	if (node == NULL || node->type != M_JSON_TYPE_STRING)
		return NULL;
	return node->data.json_string;
}

M_bool M_json_set_string(M_json_node_t *node, const char *value)
{
	if (node == NULL)
		return M_FALSE;

	M_json_node_clear(node);
	node->data.json_string = M_strdup(value);
	node->type = M_JSON_TYPE_STRING;

	return M_TRUE;
}

M_int64 M_json_get_int(const M_json_node_t *node)
{
	if (node == NULL)
		return 0;

	switch (node->type) {
		case M_JSON_TYPE_INTEGER:
			return node->data.json_integer;
		case M_JSON_TYPE_STRING:
			return M_str_to_int64(node->data.json_string);
		case M_JSON_TYPE_BOOL:
			return node->data.json_bool?1:0;
		case M_JSON_TYPE_DECIMAL:
			return M_decimal_to_int(&(node->data.json_decimal), 0);
		case M_JSON_TYPE_ARRAY:
			return (M_int64)M_json_array_len(node);
		case M_JSON_TYPE_OBJECT:
		case M_JSON_TYPE_NULL:
		case M_JSON_TYPE_UNKNOWN:
			return M_FALSE;
	}
	return 0;
}

M_bool M_json_set_int(M_json_node_t *node, M_int64 value)
{
	if (node == NULL)
		return M_FALSE;

	M_json_node_clear(node);
	node->data.json_integer = value;
	node->type = M_JSON_TYPE_INTEGER;

	return M_TRUE;
}

const M_decimal_t *M_json_get_decimal(const M_json_node_t *node)
{
	if (node == NULL || node->type != M_JSON_TYPE_DECIMAL)
		return 0;
	return &node->data.json_decimal;
}

M_bool M_json_set_decimal(M_json_node_t *node, const M_decimal_t *value)
{
	if (node == NULL)
		return M_FALSE;

	M_json_node_clear(node);
	M_decimal_duplicate(&(node->data.json_decimal), value);
	M_decimal_reduce(&(node->data.json_decimal));
	node->type = M_JSON_TYPE_DECIMAL;

	return M_TRUE;
}

M_bool M_json_get_bool(const M_json_node_t *node)
{
	M_decimal_t d;

	if (node == NULL)
		return M_FALSE;

	switch (node->type) {
		case M_JSON_TYPE_BOOL:
			return node->data.json_bool;
		case M_JSON_TYPE_STRING:
			return M_str_istrue(node->data.json_string);
		case M_JSON_TYPE_INTEGER:
			return node->data.json_integer > 0 ? M_TRUE : M_FALSE;
		case M_JSON_TYPE_DECIMAL:
			M_decimal_from_int(&d, 0, 0);
			return M_decimal_cmp(&(node->data.json_decimal), &d) == 1 ? M_TRUE : M_FALSE;
		case M_JSON_TYPE_ARRAY:
			return M_json_array_len(node) > 0 ? M_TRUE : M_FALSE;
		case M_JSON_TYPE_OBJECT:
		case M_JSON_TYPE_NULL:
		case M_JSON_TYPE_UNKNOWN:
			return M_FALSE;

	}
	return M_FALSE;
}

M_bool M_json_set_bool(M_json_node_t *node, M_bool value)
{
	if (node == NULL)
		return M_FALSE;

	M_json_node_clear(node);
	node->data.json_bool = value;
	node->type = M_JSON_TYPE_BOOL;

	return M_TRUE;
}

M_bool M_json_set_null(M_json_node_t *node)
{
	if (node == NULL)
		return M_FALSE;

	M_json_node_clear(node);
	node->type = M_JSON_TYPE_NULL;

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_json_get_value(const M_json_node_t *node, char *buf, size_t buf_len)
{
	const char *bool_val;

	if (node == NULL || buf == NULL || buf_len == 0 || node->type == M_JSON_TYPE_UNKNOWN || node->type == M_JSON_TYPE_OBJECT || node->type == M_JSON_TYPE_ARRAY)
		return M_FALSE;

	switch (node->type) {
		case M_JSON_TYPE_STRING:
			if ((size_t)M_snprintf(buf, buf_len, "%s", node->data.json_string) >= buf_len)
				return M_FALSE;
			break;
		case M_JSON_TYPE_INTEGER:
			if ((size_t)M_snprintf(buf, buf_len, "%lld", node->data.json_integer) >= buf_len)
				return M_FALSE;
			break;
		case M_JSON_TYPE_DECIMAL:
			if (M_decimal_to_str(&(node->data.json_decimal), buf, buf_len) != M_DECIMAL_SUCCESS)
				return M_FALSE;
			break;
		case M_JSON_TYPE_BOOL:
			if (node->data.json_bool) {
				bool_val = "true";
			} else {
				bool_val = "false";
			}
			if ((size_t)M_snprintf(buf, buf_len, "%s", bool_val) >= buf_len)
				return M_FALSE;
			break;
		case M_JSON_TYPE_NULL:
			if ((size_t)M_snprintf(buf, buf_len, "null") >= buf_len)
				return M_FALSE;
			break;
		default:
			return M_FALSE;
	}

	return M_TRUE;
}

char *M_json_get_value_dup(const M_json_node_t *node)
{
	M_buf_t *buf;
	char    *out;

	if (node == NULL || node->type == M_JSON_TYPE_UNKNOWN || node->type == M_JSON_TYPE_OBJECT || node->type == M_JSON_TYPE_ARRAY)
		return NULL;

	buf = M_buf_create();
	switch (node->type) {
		case M_JSON_TYPE_STRING:
			M_buf_add_str(buf, node->data.json_string);
			break;
		case M_JSON_TYPE_INTEGER:
			M_buf_add_int(buf, node->data.json_integer);
			break;
		case M_JSON_TYPE_DECIMAL:
			if (!M_buf_add_decimal(buf, &(node->data.json_decimal), M_FALSE, -1, 0)) {
				M_buf_cancel(buf);
				return NULL;
			}
			break;
		case M_JSON_TYPE_BOOL:
			if (node->data.json_bool) {
				M_buf_add_str(buf, "true");
			} else {
				M_buf_add_str(buf, "false");
			}
			break;
		case M_JSON_TYPE_NULL:
			M_buf_add_str(buf, "null");
			break;
		default:
			M_buf_cancel(buf);
			return NULL;
	}

	out = M_buf_finish_str(buf, NULL);
	return out;
}
