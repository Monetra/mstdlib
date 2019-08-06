/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"

struct M_sql_tabledata_field {
	M_bool            is_null; /*!< If field is NULL or not */
	M_sql_data_type_t type;    /*!< Datatype */

	union {
		M_bool      b;   /*!< Boolean */
		M_int16     i16; /*!< int16 */
		M_int32     i32; /*!< int32 */
		M_int64     i64; /*!< int64 */
		struct {
			const char *data;       /*!< Pointer to use */
			char       *data_alloc; /*!< NULL if const, otherwise allocated pointer */
		} t;            /*!< Text */
		struct {
			const unsigned char *data;       /*!< Pointer to use */
			unsigned char       *data_alloc; /*!< NULL if const, otherwise allocated pointer */
			size_t      len;                 /*!< Length of binary data */
		} bin;           /*!< Binary */
	} d;
};

static void M_sql_tabledata_field_clear(M_sql_tabledata_field_t *field)
{
	if (field == NULL)
		return;

	if (field->type == M_SQL_DATA_TYPE_TEXT) {
		M_free(field->d.t.data_alloc);
	}
	if (field->type == M_SQL_DATA_TYPE_BINARY) {
		M_free(field->d.bin.data_alloc);
	}
	M_mem_set(field, 0, sizeof(*field));
	field->is_null = M_TRUE;
	field->type    = M_SQL_DATA_TYPE_TEXT;
}

void M_sql_tabledata_field_set_bool(M_sql_tabledata_field_t *field, M_bool val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type = M_SQL_DATA_TYPE_BOOL;
	field->d.b  = val;
	field->is_null = M_FALSE;
}

void M_sql_tabledata_field_set_int16(M_sql_tabledata_field_t *field, M_int16 val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type    = M_SQL_DATA_TYPE_INT16;
	field->d.i16   = val;
	field->is_null = M_FALSE;
}

void M_sql_tabledata_field_set_int32(M_sql_tabledata_field_t *field, M_int32 val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type   = M_SQL_DATA_TYPE_INT32;
	field->d.i32  = val;
	field->is_null = M_FALSE;
}

void M_sql_tabledata_field_set_int64(M_sql_tabledata_field_t *field, M_int64 val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type   = M_SQL_DATA_TYPE_INT64;
	field->d.i64  = val;
	field->is_null = M_FALSE;
}

void M_sql_tabledata_field_set_text_own(M_sql_tabledata_field_t *field, char *val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type            = M_SQL_DATA_TYPE_TEXT;
	field->d.t.data_alloc  = val;
	field->d.t.data        = field->d.t.data_alloc;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}

void M_sql_tabledata_field_set_text_dup(M_sql_tabledata_field_t *field, const char *val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type            = M_SQL_DATA_TYPE_TEXT;
	field->d.t.data_alloc  = M_strdup(val);
	field->d.t.data        = field->d.t.data_alloc;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}

void M_sql_tabledata_field_set_text_const(M_sql_tabledata_field_t *field, const char *val)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type            = M_SQL_DATA_TYPE_TEXT;
	field->d.t.data        = val;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}

void M_sql_tabledata_field_set_binary_own(M_sql_tabledata_field_t *field, unsigned char *val, size_t len)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type              = M_SQL_DATA_TYPE_BINARY;
	field->d.bin.data_alloc  = val;
	field->d.bin.data        = field->d.bin.data_alloc;
	field->d.bin.len         = len;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}

void M_sql_tabledata_field_set_binary_dup(M_sql_tabledata_field_t *field, const unsigned char *val, size_t len)
{
	M_sql_tabledata_field_clear(field);
	field->type              = M_SQL_DATA_TYPE_BINARY;
	field->d.bin.data_alloc  = M_memdup(val, len);
	field->d.bin.data        = field->d.bin.data_alloc;
	field->d.bin.len         = len;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}


void M_sql_tabledata_field_set_binary_const(M_sql_tabledata_field_t *field, const unsigned char *val, size_t len)
{
	if (field == NULL)
		return;

	M_sql_tabledata_field_clear(field);
	field->type              = M_SQL_DATA_TYPE_BINARY;
	field->d.bin.data        = val;
	field->d.bin.len         = len;
	if (val != NULL) {
		field->is_null = M_FALSE;
	}
}


void M_sql_tabledata_field_set_null(M_sql_tabledata_field_t *field)
{
	M_sql_data_type_t type;

	if (field == NULL)
		return;

	type = field->type;
	M_sql_tabledata_field_clear(field);
	field->type = type; /* Preserve original field type */
}


M_bool M_sql_tabledata_field_get_bool(M_sql_tabledata_field_t *field, M_bool *val)
{
	if (field == NULL || val == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_BOOL) {
		*val = field->d.b;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT64;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_INT16:
				M_sql_tabledata_field_set_bool(field, field->d.i16?M_TRUE:M_FALSE);
				break;
			case M_SQL_DATA_TYPE_INT32:
				M_sql_tabledata_field_set_bool(field, field->d.i32?M_TRUE:M_FALSE);
				break;
			case M_SQL_DATA_TYPE_INT64:
				M_sql_tabledata_field_set_bool(field, field->d.i64?M_TRUE:M_FALSE);
				break;
			case M_SQL_DATA_TYPE_TEXT:
				M_sql_tabledata_field_set_bool(field, M_str_istrue(field->d.t.data));
				break;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_bool(field, val);
}

M_bool M_sql_tabledata_field_get_int16(M_sql_tabledata_field_t *field, M_int16 *val)
{
	M_int32 i32;

	if (field == NULL || val == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT16) {
		*val = field->d.i16;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT16;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_int16(field, field->d.b);
				break;
			case M_SQL_DATA_TYPE_INT32:
				if (field->d.i32 > M_INT16_MAX)
					return M_FALSE;
				M_sql_tabledata_field_set_int16(field, (M_int16)field->d.i32);
				break;
			case M_SQL_DATA_TYPE_INT64:
				if (field->d.i64 > M_INT16_MAX)
					return M_FALSE;
				M_sql_tabledata_field_set_int16(field, (M_int16)field->d.i64);
				break;
			case M_SQL_DATA_TYPE_TEXT:
				if (M_str_to_int32_ex(field->d.t.data, M_str_len(field->d.t.data), 10, &i32, NULL) != M_STR_INT_SUCCESS)
					return M_FALSE;
				if (i32 > M_INT16_MAX)
					return M_FALSE;
				M_sql_tabledata_field_set_int16(field, (M_int16)i32);
				break;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_int16(field, val);
}

M_bool M_sql_tabledata_field_get_int32(M_sql_tabledata_field_t *field, M_int32 *val)
{
	M_int32 i32;

	if (field == NULL || val == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT32) {
		*val = field->d.i32;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT32;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_int32(field, field->d.b);
				break;
			case M_SQL_DATA_TYPE_INT16:
				M_sql_tabledata_field_set_int32(field, (M_int32)field->d.i16);
				break;
			case M_SQL_DATA_TYPE_INT64:
				if (field->d.i64 > M_INT32_MAX)
					return M_FALSE;
				M_sql_tabledata_field_set_int32(field, (M_int32)field->d.i64);
				break;
			case M_SQL_DATA_TYPE_TEXT:
				if (M_str_to_int32_ex(field->d.t.data, M_str_len(field->d.t.data), 10, &i32, NULL) != M_STR_INT_SUCCESS)
					return M_FALSE;
				M_sql_tabledata_field_set_int32(field, i32);
				break;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_int32(field, val);
}


M_bool M_sql_tabledata_field_get_int64(M_sql_tabledata_field_t *field, M_int64 *val)
{
	M_int64 i64;

	if (field == NULL || val == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT64) {
		*val = field->d.i64;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT64;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_int64(field, (M_int64)field->d.b);
				break;
			case M_SQL_DATA_TYPE_INT16:
				M_sql_tabledata_field_set_int64(field, (M_int64)field->d.i16);
				break;
			case M_SQL_DATA_TYPE_INT32:
				M_sql_tabledata_field_set_int64(field, (M_int64)field->d.i32);
				break;
			case M_SQL_DATA_TYPE_TEXT:
				if (M_str_to_int64_ex(field->d.t.data, M_str_len(field->d.t.data), 10, &i64, NULL) != M_STR_INT_SUCCESS)
					return M_FALSE;
				M_sql_tabledata_field_set_int64(field, i64);
				break;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_int64(field, val);
}

M_bool M_sql_tabledata_field_get_text(M_sql_tabledata_field_t *field, const char **val)
{
	char *data = NULL;

	if (field == NULL || val == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_TEXT) {
		*val = field->d.t.data;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_TEXT;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_text_const(field, field->d.b?"yes":"no");
				break;
			case M_SQL_DATA_TYPE_INT16:
				M_asprintf(&data, "%d", field->d.i16);
				M_sql_tabledata_field_set_text_own(field, data);
				break;
			case M_SQL_DATA_TYPE_INT32:
				M_asprintf(&data, "%d", field->d.i32);
				M_sql_tabledata_field_set_text_own(field, data);
				break;
			case M_SQL_DATA_TYPE_INT64:
				M_asprintf(&data, "%lld", field->d.i64);
				M_sql_tabledata_field_set_text_own(field, data);
				break;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_text(field, val);
}

M_bool M_sql_tabledata_field_get_binary(M_sql_tabledata_field_t *field, const unsigned char **val, size_t *len)
{
	if (field == NULL || val == NULL || len == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_BINARY) {
		*val = field->d.bin.data;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_TEXT;
	} else {
		/* No conversion allowed */
		return M_FALSE;
	}
	return M_sql_tabledata_field_get_binary(field, val, len);
}

M_bool M_sql_tabledata_field_is_null(const M_sql_tabledata_field_t *field)
{
	if (field == NULL)
		return M_TRUE;
	return field->is_null;
}

M_sql_data_type_t M_sql_tabledata_field_type(const M_sql_tabledata_field_t *field)
{
	if (field == NULL)
		return M_SQL_DATA_TYPE_UNKNOWN;
	return field->type;
}


static M_bool M_sql_tabledata_field_eq(const M_sql_tabledata_field_t *field1, M_sql_tabledata_field_t *field2)
{
	M_bool               b;
	M_int16              i16;
	M_int32              i32;
	M_int64              i64;
	const unsigned char *bin;
	const char          *t;
	size_t               len;

	if (M_sql_tabledata_field_is_null(field1)) {
		if (M_sql_tabledata_field_is_null(field2))
			return M_TRUE;
		return M_FALSE;
	}

	if (M_sql_tabledata_field_is_null(field2))
		return M_FALSE;

	switch (M_sql_tabledata_field_type(field1)) {
		case M_SQL_DATA_TYPE_BOOL:
			if (!M_sql_tabledata_field_get_bool(field2, &b))
				return M_FALSE;
			return b == field1->d.b;

		case M_SQL_DATA_TYPE_INT16:
			if (!M_sql_tabledata_field_get_int16(field2, &i16))
				return M_FALSE;
			return i16 == field1->d.i16;

		case M_SQL_DATA_TYPE_INT32:
			if (!M_sql_tabledata_field_get_int32(field2, &i32))
				return M_FALSE;
			return i32 == field1->d.i32;

		case M_SQL_DATA_TYPE_INT64:
			if (!M_sql_tabledata_field_get_int64(field2, &i64))
				return M_FALSE;
			return i64 == field1->d.i64;

		case M_SQL_DATA_TYPE_TEXT:
			if (!M_sql_tabledata_field_get_text(field2, &t))
				return M_FALSE;
			return M_str_eq(t, field1->d.t.data);

		case M_SQL_DATA_TYPE_BINARY:
			if (!M_sql_tabledata_field_get_binary(field2, &bin, &len))
				return M_FALSE;
			if (len != field1->d.bin.len)
				return M_FALSE;
			return M_mem_eq(bin, field1->d.bin.data, len);
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}

	return M_FALSE;
}


static void M_sql_tabledata_field_copy(M_sql_tabledata_field_t *dest, const M_sql_tabledata_field_t *src)
{
	if (dest == NULL || src == NULL)
		return;

	M_sql_tabledata_field_clear(dest);
	M_mem_copy(dest, src, sizeof(*dest));

	/* Duplicate pointers if necessary */
	if (dest->type == M_SQL_DATA_TYPE_TEXT && dest->d.t.data_alloc) {
		dest->d.t.data_alloc = M_strdup(dest->d.t.data_alloc);
		dest->d.t.data       = dest->d.t.data_alloc;
	}
	if (dest->type == M_SQL_DATA_TYPE_BINARY && dest->d.bin.data_alloc) {
		dest->d.bin.data_alloc = M_memdup(dest->d.bin.data_alloc, dest->d.bin.len);
		dest->d.bin.data       = dest->d.bin.data_alloc;
	}
}


/* Returns M_TRUE on change, M_FALSE on no change */
static M_bool M_sql_tabledata_edit_fetch_int(M_sql_tabledata_field_t *field, const char *field_name, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strvp_t *prev_fields)
{
	M_sql_tabledata_field_t *prior_field = NULL;

	/* If the field wasn't specified, then we use the old value, so no change */
	if (!fetch_cb(field, field_name, (prev_fields == NULL)?M_TRUE:M_FALSE, thunk))
		return M_FALSE;

	/* If didn't exist previously, but does exist now, its a change. */
	if (!M_hash_strvp_get(prev_fields, field_name, (void **)&prior_field))
		return M_TRUE;

	if (M_sql_tabledata_field_eq(prior_field, field)) {
		M_sql_tabledata_field_clear(field);
		return M_FALSE;
	}
	return M_TRUE;
}


/* Returns M_FALSE if field was not specified, or if it matches the previous value.
 * If output_always is M_TRUE, then field_data and field_data_len will be filled in even if no change is occurring...
 * this is used for virtual fields otherwise we'd lose data. */
static M_bool M_sql_tabledata_edit_fetch(M_sql_tabledata_field_t *field, const char *field_name, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strvp_t *prev_fields, M_bool output_identical)
{
	M_bool rv;

	M_mem_set(field, 0, sizeof(*field));
	M_sql_tabledata_field_clear(field);

	rv = M_sql_tabledata_edit_fetch_int(field, field_name, fetch_cb, thunk, prev_fields);

	if (output_identical && rv == M_FALSE && M_sql_tabledata_field_is_null(field)) {
		M_sql_tabledata_field_t *prior_field = NULL;
		if (M_hash_strvp_get(prev_fields, field_name, (void **)&prior_field)) {
			M_sql_tabledata_field_copy(field, prior_field);
		}
	}

	return rv;
}

/* NOTE: if prev_fields is specified, it is an edit, otherwise its an add.  If NO fields have changed on edit, will return NULL. */
static M_bool M_sql_tabledata_row_gather_virtual(M_sql_tabledata_field_t *out_field, const M_sql_tabledata_t *fields, size_t num_fields, size_t curr_idx, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strvp_t *prev_fields)
{
	const char             *column_name   = fields[curr_idx].table_column;
	size_t                  i;
	M_hash_dict_t          *dict          = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_LOWER);
	M_bool                  field_updated = M_FALSE;
	M_sql_tabledata_field_t field;

	M_mem_set(&field, 0, sizeof(field));
	M_sql_tabledata_field_clear(&field);

	for (i=curr_idx; i<num_fields; i++) {
		if (!M_str_caseeq(fields[i].table_column, column_name))
			continue;

		if (prev_fields) {
			if (M_sql_tabledata_edit_fetch(&field, fields[i].field_name, fetch_cb, thunk, prev_fields, M_TRUE))
				field_updated = M_TRUE;
		} else {
			if (!fetch_cb(&field, fields[i].field_name, M_TRUE, thunk)) {
				continue;
			}
		}

		/* Null data is the same as the key not existing */
		if (!M_sql_tabledata_field_is_null(&field)) {
			const char *text = NULL;
			if (!M_sql_tabledata_field_get_text(&field, &text)) {
				/* TODO: come up with a way to return failure here */
			}
			M_hash_dict_insert(dict, fields[i].field_name, text);
		}

		M_sql_tabledata_field_clear(&field);
	}

	/* Only serialize if there are updated fields or during an add */
	if (!prev_fields || field_updated) {
		M_sql_tabledata_field_set_text_own(out_field, M_hash_dict_serialize(dict, '|', '=', '"', '"', M_HASH_DICT_SER_FLAG_NONE));
	}

	M_hash_dict_destroy(dict); dict = NULL;
	return M_TRUE;
}


static M_bool M_sql_tabledata_validate_fields(const M_sql_tabledata_t *fields, size_t num_fields, char *error, size_t error_len)
{
	size_t         i;
	M_hash_dict_t *seen_cols     = NULL;
	M_hash_dict_t *seen_fields   = NULL;
	M_bool         rv            = M_FALSE;
	M_bool         has_id        = M_FALSE;
	M_bool         has_nonid     = M_FALSE;
	M_bool         has_generated = M_FALSE;

	seen_cols   = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	seen_fields = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);

	for (i=0; i<num_fields; i++) {
		if (M_str_isempty(fields[i].table_column)) {
			M_snprintf(error, error_len, "field %zu did not specify a column name", i);
			goto done;
		}

		if (fields[i].field_name == NULL) {
			M_snprintf(error, error_len, "field %zu did not specify a field name", i);
			goto done;
		}

		if (M_hash_dict_get(seen_fields, fields[i].field_name, NULL)) {
			M_snprintf(error, error_len, "Duplicate field name %s specified", fields[i].field_name);
			goto done;
		}
		M_hash_dict_insert(seen_fields, fields[i].field_name, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL && fields[i].type != M_SQL_DATA_TYPE_TEXT) {
			M_snprintf(error, error_len, "Column %s virtual field %s is only allowed to be text", fields[i].table_column, fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both editable and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both virtual and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & (M_SQL_TABLEDATA_FLAG_ID_GENERATE|M_SQL_TABLEDATA_FLAG_ID_REQUIRED) &&
		    !(fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)) {
			M_snprintf(error, error_len, "field %s must be an id to specify id_generate or id_required", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID_GENERATE) {
			if (has_generated) {
				M_snprintf(error, error_len, "more than one field with id_generate specified");
				goto done;
			}
			has_generated = M_TRUE;
		}

		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			if (!(fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)) {
				M_snprintf(error, error_len, "non-virtual column %s specified more than once", fields[i].table_column);
				goto done;
			}
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			has_id = M_TRUE;
		} else {
			has_nonid = M_TRUE;
		}
	}


	if (!has_id) {
		M_snprintf(error, error_len, "table definition must contain at least 1 ID column");
		goto done;
	}
	if (!has_nonid) {
		M_snprintf(error, error_len, "table definition must contain at least 1 non-ID column");
		goto done;
	}

	rv = M_TRUE;

done:
	M_hash_dict_destroy(seen_fields);
	M_hash_dict_destroy(seen_cols);
	return rv;
}

/* NOTE: Will take ownership of field data internal pointers on success! */
static M_bool M_sql_tabledata_bind(M_sql_stmt_t *stmt, M_sql_data_type_t type, M_sql_tabledata_field_t *field, size_t max_column_len)
{
	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			if (field->is_null) {
				M_sql_stmt_bind_bool_null(stmt);
			} else {
				M_bool b;
				if (!M_sql_tabledata_field_get_bool(field, &b))
					return M_FALSE;
				M_sql_stmt_bind_bool(stmt, b);
			}
			break;

		case M_SQL_DATA_TYPE_INT16:
			if (field->is_null) {
				M_sql_stmt_bind_int16_null(stmt);
			} else {
				M_int16 i16;
				if (!M_sql_tabledata_field_get_int16(field, &i16))
					return M_FALSE;

				M_sql_stmt_bind_int16(stmt, i16);
			}
			break;

		case M_SQL_DATA_TYPE_INT32:
			if (field->is_null) {
				M_sql_stmt_bind_int32_null(stmt);
			} else {
				M_int32 i32;
				if (!M_sql_tabledata_field_get_int32(field, &i32))
					return M_FALSE;

				M_sql_stmt_bind_int32(stmt, i32);
			}
			break;

		case M_SQL_DATA_TYPE_INT64:
			if (field->is_null) {
				M_sql_stmt_bind_int64_null(stmt);
			} else {
				M_int64 i64;
				if (!M_sql_tabledata_field_get_int64(field, &i64))
					return M_FALSE;

				M_sql_stmt_bind_int64(stmt, i64);
			}
			break;

		case M_SQL_DATA_TYPE_TEXT:
			{
				const char *text = NULL;
				size_t      len;

				if (!M_sql_tabledata_field_get_text(field, &text))
					return M_FALSE;

				len = M_str_len(text);

				if (field->d.t.data_alloc) {
					M_sql_stmt_bind_text_own(stmt, field->d.t.data_alloc, M_MIN(len, max_column_len));
					field->d.t.data_alloc = NULL;
				} else {
					M_sql_stmt_bind_text_const(stmt, text, M_MIN(len, max_column_len));
				}
			}
			break;

		case M_SQL_DATA_TYPE_BINARY:
			{
				const unsigned char *bin = NULL;
				size_t               len = 0;

				if (!M_sql_tabledata_field_get_binary(field, &bin, &len))
					return M_FALSE;

				if (field->d.bin.data_alloc) {
					M_sql_stmt_bind_binary_own(stmt, field->d.bin.data_alloc, M_MIN(len, max_column_len));
					field->d.bin.data_alloc = NULL;
				} else {
					M_sql_stmt_bind_binary_const(stmt, bin, M_MIN(len, max_column_len));
				}
			}
			break;

		case M_SQL_DATA_TYPE_UNKNOWN:
		default:
			return M_FALSE;
	}

	M_sql_tabledata_field_clear(field);
	return M_TRUE;
}


static M_sql_error_t M_sql_tabledata_add_int(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	M_buf_t                *request     = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	size_t                  i;
	M_bool                  has_col     = M_FALSE;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_field_t field;

	M_mem_set(&field, 0, sizeof(field));

	if (pool == NULL && sqltrans == NULL) {
		M_snprintf(error, error_len, "must specify pool or sqltrans");
		goto done;
	}

	if (pool == NULL) {
		pool = M_sql_trans_get_pool(sqltrans);
	}

	if (M_str_isempty(table_name)) {
		M_snprintf(error, error_len, "missing table name");
		goto done;
	}
	if (fields == NULL || num_fields == 0) {
		M_snprintf(error, error_len, "fields specified invalid");
		goto done;
	}

	if (!M_sql_tabledata_validate_fields(fields, num_fields, error, error_len))
		goto done;

	/* TODO: Loop up to 10x if key conflict AND table has ID_GENERATE flag on at least 1 key */

	request     = M_buf_create();
	seen_cols   = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	stmt        = M_sql_stmt_create();

	M_buf_add_str(request, "INSERT INTO \"");
	M_buf_add_str(request, table_name);
	M_buf_add_str(request, "\" (");

	/* Specify each column name we will be outputting (in case the table as more columns than this) */
	for (i=0; i<num_fields; i++) {
		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		/* If its not an ID and not a virtual field, then we need to test to see if this column should be emitted at all. */
		if (!(fields[i].flags & (M_SQL_TABLEDATA_FLAG_ID|M_SQL_TABLEDATA_FLAG_VIRTUAL)) &&
		    !fetch_cb(NULL, fields[i].field_name, M_TRUE, thunk)) {
			/* Skip! */
			continue;
		}

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, fields[i].table_column);
		M_buf_add_str(request, "\"");
	}

	if (!has_col) {
		M_snprintf(error, error_len, "No columns were eligible to be emitted");
		goto done;
	}

	/* Ok, we'll need to clear seen_cols so we can iterate again ... */
	M_hash_dict_destroy(seen_cols);
	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	has_col   = M_FALSE;

	/* Bind values */
	M_buf_add_str(request, ") VALUES (");
	for (i=0; i<num_fields; i++) {
		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			if (!M_sql_tabledata_row_gather_virtual(&field, fields, num_fields, i, fetch_cb, thunk, NULL))
				continue;
		} else if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			size_t  max_len = fields[i].max_column_len;
			M_int64 id;
			if (max_len == 0) {
				if (fields[i].type == M_SQL_DATA_TYPE_INT32) {
					max_len = 9;
				} else {
					max_len = 18;
				}
			}
			if (max_len > 18)
				max_len = 18;
			id             = M_sql_gen_timerand_id(pool, max_len);
			M_sql_tabledata_field_set_int64(&field, id);

			if (generated_id != NULL)
				*generated_id = id;
		} else {
			if (!fetch_cb(&field, fields[i].field_name, M_TRUE, thunk)) {
				/* Do not emit field */
				continue;
			}
		}

		/* NOTE: just because field_data is NULL doesn't mean they don't intend us to actually bind NULL */

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "?");

		if (!M_sql_tabledata_bind(stmt, fields[i].type, &field, fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", fields[i].table_column);
			M_sql_tabledata_field_clear(&field);
			goto done;
		}

		M_sql_tabledata_field_clear(&field);
	}

	M_buf_add_str(request, ")");
	err = M_sql_stmt_prepare_buf(stmt, request);
	request = NULL;
	if (M_sql_error_is_error(err))
		goto done;

	if (sqltrans != NULL) {
		err = M_sql_trans_execute(sqltrans, stmt);
	} else {
		err = M_sql_stmt_execute(pool, stmt);
	}

done:
	if (request)
		M_buf_cancel(request);
	if (seen_cols)
		M_hash_dict_destroy(seen_cols);
	if (stmt != NULL) {
		if (M_sql_error_is_error(err) && err != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}
	return err;
}

M_sql_error_t M_sql_tabledata_add(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	return M_sql_tabledata_add_int(pool, NULL, table_name, fields, num_fields, fetch_cb, thunk, generated_id, error, error_len);
}

M_sql_error_t M_sql_tabledata_trans_add(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	return M_sql_tabledata_add_int(NULL, sqltrans, table_name, fields, num_fields, fetch_cb, thunk, generated_id, error, error_len);
}

static void M_sql_tabledata_field_free_cb(void *arg)
{
	M_sql_tabledata_field_t *field = arg;
	if (field == NULL)
		return;
	M_sql_tabledata_field_clear(field);
	M_free(field);
}

/*! Query table for matching record and return fieldname to value mappings in a hash dict */
static M_sql_error_t M_sql_tabledata_query(M_hash_strvp_t **params_out, M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	M_buf_t                *request     = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_bool                  has_col     = M_FALSE;
	size_t                  i;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_field_t field;

	M_mem_set(&field, 0, sizeof(field));

	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	request   = M_buf_create();
	stmt      = M_sql_stmt_create();

	M_buf_add_str(request, "SELECT ");
	for (i=0; i<num_fields; i++) {
		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		/* ID columns are not part of the selected data */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, fields[i].table_column);
		M_buf_add_str(request, "\"");
	}
	M_buf_add_str(request, " FROM \"");
	M_buf_add_str(request, table_name);
	M_buf_add_str(request, "\"");
	M_sql_query_append_updlock(M_sql_trans_get_pool(sqltrans), request, M_SQL_QUERY_UPDLOCK_TABLE, NULL);
	M_buf_add_str(request, " WHERE ");

	has_col = M_FALSE;

	for (i=0; i<num_fields; i++) {
		if (!(fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		if (!fetch_cb(&field, fields[i].field_name, M_FALSE, thunk)) {
			if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID_REQUIRED) {
				M_snprintf(error, error_len, "required field %s not specified", fields[i].field_name);
				goto done;
			}
			continue;
		}

		if (has_col) {
			M_buf_add_str(request, " AND ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, fields[i].table_column);
		M_buf_add_str(request, "\" = ?");
		if (!M_sql_tabledata_bind(stmt, fields[i].type, &field, fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", fields[i].table_column);
			M_sql_tabledata_field_clear(&field);
			goto done;
		}
		M_sql_tabledata_field_clear(&field);
	}

	if (!has_col) {
		M_snprintf(error, error_len, "no search criteria specified");
		goto done;
	}

	M_sql_query_append_updlock(M_sql_trans_get_pool(sqltrans), request, M_SQL_QUERY_UPDLOCK_QUERYEND, NULL);

	err = M_sql_stmt_prepare_buf(stmt, request);
	request = NULL;
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

	err = M_sql_trans_execute(sqltrans, stmt);

	if (M_sql_error_is_error(err))
		goto done;

	if (M_sql_stmt_result_num_rows(stmt) == 0) {
		M_snprintf(error, error_len, "no match found");
		err = M_SQL_ERROR_USER_FAILURE;
		goto done;
	}

	if (M_sql_stmt_result_num_rows(stmt) > 1) {
		M_snprintf(error, error_len, "more than one matching row found for search criteria");
		err = M_SQL_ERROR_USER_FAILURE;
		goto done;
	}

	/* Reset seen_cols, we'll use it again. */
	M_hash_dict_destroy(seen_cols);
	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);

	/* Map data */
	*params_out = M_hash_strvp_create(16, 75, M_HASH_STRVP_KEYS_LOWER|M_HASH_STRVP_CASECMP, M_sql_tabledata_field_free_cb);

	for (i=0; i<num_fields; i++) {
		M_sql_tabledata_field_t *myfield = NULL;

		/* Skip columns we already have (multiple virtual fields) */
		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		/* Skip any fields that are ids, we don't use that here */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		/* Need to deserialize and add to output params */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			M_hash_dict_t      *dict     = M_hash_dict_deserialize(M_sql_stmt_result_text_byname_direct(stmt, 0, fields[i].table_column), '|', '=', '"', '"', M_HASH_DICT_DESER_FLAG_CASECMP);
			M_hash_dict_enum_t *hashenum = NULL;
			const char         *key      = NULL;
			const char         *val      = NULL;

			M_hash_dict_enumerate(dict, &hashenum);
			while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
				myfield = M_malloc_zero(sizeof(*myfield));
				M_sql_tabledata_field_set_text_dup(myfield, val);
				M_hash_strvp_insert(*params_out, key, myfield);
				myfield = NULL;
			}
			M_hash_dict_enumerate_free(hashenum);
			M_hash_dict_destroy(dict);
		} else {
			myfield = M_malloc_zero(sizeof(*myfield));
			M_sql_tabledata_field_clear(myfield);

			if (M_sql_stmt_result_isnull_byname_direct(stmt, 0, fields[i].table_column)) {
				M_sql_tabledata_field_set_null(myfield);
			} else {
				switch (M_sql_stmt_result_col_type_byname(stmt, fields[i].table_column, NULL)) {
					case M_SQL_DATA_TYPE_BOOL:
						M_sql_tabledata_field_set_bool(myfield, M_sql_stmt_result_bool_byname_direct(stmt, 0, fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT16:
						M_sql_tabledata_field_set_int16(myfield, M_sql_stmt_result_int16_byname_direct(stmt, 0, fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT32:
						M_sql_tabledata_field_set_int32(myfield, M_sql_stmt_result_int32_byname_direct(stmt, 0, fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT64:
						M_sql_tabledata_field_set_int64(myfield, M_sql_stmt_result_int64_byname_direct(stmt, 0, fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_TEXT:
						M_sql_tabledata_field_set_text_dup(myfield, M_sql_stmt_result_text_byname_direct(stmt, 0, fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_BINARY:
						{ 
							size_t               len = 0;
							const unsigned char *bin = M_sql_stmt_result_binary_byname_direct(stmt, 0, fields[i].table_column, &len);
							M_sql_tabledata_field_set_binary_dup(myfield, bin, len);
						}
						break;
					case M_SQL_DATA_TYPE_UNKNOWN:
						break;
				}
			}

			M_hash_strvp_insert(*params_out, fields[i].field_name, myfield);
			myfield = NULL;
		}
	}

done:
	if (request)
		M_buf_cancel(request);
	if (seen_cols)
		M_hash_dict_destroy(seen_cols);
	if (stmt != NULL) {
		if (M_sql_error_is_error(err) && err != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}
	return err;
}

typedef struct {
	const char              *table_name;
	const M_sql_tabledata_t *fields;
	size_t                   num_fields;
	M_sql_tabledata_fetch_cb fetch_cb;
	void                    *thunk;
} M_sql_tabledata_edit_t;

static M_sql_error_t M_sql_tabledata_edit_do(M_sql_trans_t *sqltrans, void *arg, char *error, size_t error_len)
{
	M_buf_t                *request     = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	size_t                  i;
	M_bool                  has_col     = M_FALSE;
	M_hash_strvp_t         *prev_fields = NULL;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_edit_t *info        = arg;
	M_sql_tabledata_field_t field;

	M_mem_set(&field, 0, sizeof(field));

	err = M_sql_tabledata_query(&prev_fields, sqltrans, info->table_name, info->fields, info->num_fields, info->fetch_cb, info->thunk, error, error_len);
	if (M_sql_error_is_error(err))
		goto done;

	err       = M_SQL_ERROR_USER_FAILURE;

	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	request   = M_buf_create();
	stmt      = M_sql_stmt_create();
	M_buf_add_str(request, "UPDATE \"");
	M_buf_add_str(request, info->table_name);
	M_buf_add_str(request, "\" SET ");

	for (i=0; i<info->num_fields; i++) {
		M_bool is_changed;

		/* Skip ID columns */
		if (info->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		/* Skip columns we already have (multiple virtual fields) */
		if (M_hash_dict_get(seen_cols, info->fields[i].table_column, NULL)) {
			continue;
		}

		M_hash_dict_insert(seen_cols, info->fields[i].table_column, NULL);

		if (info->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			/* Only on virtual data does NULL mean not changed.  Otherwise we are explicitly setting a field to NULL */
			M_sql_tabledata_row_gather_virtual(&field, info->fields, info->num_fields, i, info->fetch_cb, info->thunk, prev_fields);
			if (M_sql_tabledata_field_is_null(&field)) {
				is_changed = M_FALSE;
			} else {
				is_changed = M_TRUE;
			}
		} else {
			is_changed  = M_sql_tabledata_edit_fetch(&field, info->fields[i].field_name, info->fetch_cb, info->thunk, prev_fields, M_FALSE);
			/* TODO: see if field is allowed to be edited */
		}

		if (is_changed) {
			if (has_col)
				M_buf_add_str(request, ", ");
			has_col = M_TRUE;

			M_buf_add_str(request, "\"");
			M_buf_add_str(request, info->fields[i].table_column);
			M_buf_add_str(request, "\" = ?");

			if (!M_sql_tabledata_bind(stmt, info->fields[i].type, &field, info->fields[i].max_column_len)) {
				M_snprintf(error, error_len, "column %s unsupported field type", info->fields[i].table_column);
				M_sql_tabledata_field_clear(&field);
				goto done;
			}
		} else {
			M_sql_tabledata_field_clear(&field);
		}
	}

	if (!has_col) {
		err = M_SQL_ERROR_USER_SUCCESS;
		M_snprintf(error, error_len, "no data has changed");
		goto done;
	}

	/* Now output what row to update */
	M_buf_add_str(request, " WHERE ");
	has_col = M_FALSE;
	for (i=0; i<info->num_fields; i++) {
		/* Skip non-ID columns */
		if (!(info->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		if (!info->fetch_cb(&field, info->fields[i].field_name, M_FALSE, info->thunk)) {
			continue;
		}

		if (has_col) {
			M_buf_add_str(request, " AND ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, info->fields[i].table_column);
		M_buf_add_str(request, "\" = ?");
		if (!M_sql_tabledata_bind(stmt, info->fields[i].type, &field, info->fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", info->fields[i].table_column);
			M_sql_tabledata_field_clear(&field);
			goto done;
		}
		M_sql_tabledata_field_clear(&field);
	}

	err = M_sql_stmt_prepare_buf(stmt, request);
	request = NULL;
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

	err = M_sql_trans_execute(sqltrans, stmt);

	if (M_sql_error_is_error(err))
		goto done;

	if (M_sql_stmt_result_affected_rows(stmt) > 1) {
		err = M_SQL_ERROR_USER_FAILURE;
		M_snprintf(error, error_len, "more than one row would be updated");
		goto done;
	}

	if (M_sql_stmt_result_affected_rows(stmt) == 0) {
		err = M_SQL_ERROR_USER_SUCCESS;
		M_snprintf(error, error_len, "no data has changed as per server");
		goto done;
	}


done:
	if (request)
		M_buf_cancel(request);
	if (seen_cols)
		M_hash_dict_destroy(seen_cols);
	if (prev_fields)
		M_hash_strvp_destroy(prev_fields, M_TRUE);
	if (stmt != NULL) {
		if (M_sql_error_is_error(err) && err != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}
	return err;
}


static M_sql_error_t M_sql_tabledata_edit_int(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	M_sql_error_t          err  = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_edit_t info = {
		table_name,
		fields,
		num_fields,
		fetch_cb,
		thunk
	};

	if (pool == NULL && sqltrans == NULL) {
		M_snprintf(error, error_len, "must specify pool or sqltrans");
		goto done;
	}

	if (pool == NULL) {
		pool = M_sql_trans_get_pool(sqltrans);
	}

	if (M_str_isempty(table_name)) {
		M_snprintf(error, error_len, "missing table name");
		goto done;
	}
	if (fields == NULL || num_fields == 0) {
		M_snprintf(error, error_len, "fields specified invalid");
		goto done;
	}

	if (!M_sql_tabledata_validate_fields(fields, num_fields, error, error_len))
		goto done;

	if (sqltrans != NULL) {
		err = M_sql_tabledata_edit_do(sqltrans, &info, error, error_len);
	} else {
		err = M_sql_trans_process(pool, M_SQL_ISOLATION_SERIALIZABLE, M_sql_tabledata_edit_do, &info, error, error_len);
	}

done:
	return err;
}

M_sql_error_t M_sql_tabledata_edit(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(pool, NULL, table_name, fields, num_fields, fetch_cb, thunk, error, error_len);
}

M_sql_error_t M_sql_tabledata_trans_edit(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(NULL, sqltrans, table_name, fields, num_fields, fetch_cb, thunk, error, error_len);
}


M_sql_tabledata_t *M_sql_tabledata_append_virtual_list(const M_sql_tabledata_t *fields, size_t *num_fields, const char *table_column, M_list_str_t *field_names, size_t max_len, M_sql_tabledata_flags_t flags)
{
	size_t             len;
	size_t             i;
	M_sql_tabledata_t *out_fields = NULL;

	/* Invalid Use */
	if (field_names == NULL || max_len == 0)
		return NULL;

	len        = M_list_str_len(field_names);

	/* Duplicate into expanded list */
	out_fields = M_malloc_zero(((*num_fields) + len) * sizeof(*out_fields));
	M_mem_copy(out_fields, fields, (*num_fields) * sizeof(*fields));

	for (i=0; i<len; i++) {
		const char *name = M_list_str_at(field_names, i);
		out_fields[(*num_fields)+i].table_column   = table_column;
		out_fields[(*num_fields)+i].field_name     = name;
		out_fields[(*num_fields)+i].max_column_len = max_len;
		out_fields[(*num_fields)+i].type           = M_SQL_DATA_TYPE_TEXT;
		out_fields[(*num_fields)+i].flags          = flags | M_SQL_TABLEDATA_FLAG_VIRTUAL;
	}
	(*num_fields)+=len;
	return out_fields;
}
