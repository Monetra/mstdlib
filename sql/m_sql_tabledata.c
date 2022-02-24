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
			size_t               len;        /*!< Length of binary data */
		} bin;           /*!< Binary */
	} d;
};

static M_sql_tabledata_field_t *M_sql_tabledata_field_duplicate(const M_sql_tabledata_field_t *field)
{
	M_sql_tabledata_field_t *out = M_malloc(sizeof(*out));
	M_mem_copy(out, field, sizeof(*out));

	if (field->type == M_SQL_DATA_TYPE_TEXT && field->d.t.data_alloc) {
		out->d.t.data_alloc   = M_strdup(field->d.t.data_alloc);
		out->d.t.data         = out->d.t.data_alloc;
	} else if (field->type == M_SQL_DATA_TYPE_BINARY && field->d.bin.data_alloc) {
		out->d.bin.data_alloc = M_memdup(field->d.bin.data_alloc, field->d.bin.len);
		out->d.bin.data       = out->d.bin.data_alloc;
	}
	return out;
}


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

M_sql_tabledata_field_t *M_sql_tabledata_field_create_ext(void)
{
	M_sql_tabledata_field_t *field = M_malloc_zero(sizeof(*field));
	M_sql_tabledata_field_clear(field);
	return field;
}

void M_sql_tabledata_field_destroy_ext(M_sql_tabledata_field_t *field)
{
	M_sql_tabledata_field_clear(field);
	M_free(field);
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
	if (field == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_BOOL) {
		if (val)
			*val = field->d.b;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_BOOL;
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
				if (M_str_caseeq(field->d.t.data, "yes")   || M_str_caseeq(field->d.t.data, "y") ||
				    M_str_caseeq(field->d.t.data, "true")  || M_str_caseeq(field->d.t.data, "1") ||
				    M_str_caseeq(field->d.t.data, "on")    ||
				    M_str_caseeq(field->d.t.data, "no")    || M_str_caseeq(field->d.t.data, "n") ||
				    M_str_caseeq(field->d.t.data, "false") || M_str_caseeq(field->d.t.data, "0") ||
				    M_str_caseeq(field->d.t.data, "off")) {
					M_sql_tabledata_field_set_bool(field, M_str_istrue(field->d.t.data));
					break;
				}
				return M_FALSE;
			default:
				return M_FALSE;
		}
	}
	return M_sql_tabledata_field_get_bool(field, val);
}

M_bool M_sql_tabledata_field_get_int16(M_sql_tabledata_field_t *field, M_int16 *val)
{
	M_int32 i32;

	if (field == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT16) {
		if (val)
			*val = field->d.i16;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT16;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_int16(field, (M_int16)field->d.b);
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

	if (field == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT32) {
		if (val)
			*val = field->d.i32;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_INT32;
	} else {
		switch  (field->type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_sql_tabledata_field_set_int32(field, (M_int16)field->d.b);
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

	if (field == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_INT64) {
		if (val)
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

	if (field == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_TEXT) {
		if (val)
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
	if (field == NULL)
		return M_FALSE;

	/* You can get only the length, but if you request val you must also get the length */
	if (val != NULL && len == NULL)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_BINARY) {
		if (val)
			*val = field->d.bin.data;
		if (len)
			*len = field->d.bin.len;
		return M_TRUE;
	}

	if (field->is_null) {
		field->type = M_SQL_DATA_TYPE_BINARY;
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

M_bool M_sql_tabledata_field_is_alloc(const M_sql_tabledata_field_t *field)
{
	if (field == NULL)
		return M_FALSE;

	if (field->type != M_SQL_DATA_TYPE_TEXT && field->type != M_SQL_DATA_TYPE_BINARY)
		return M_FALSE;

	if (field->type == M_SQL_DATA_TYPE_BINARY && field->d.bin.data_alloc)
		return M_TRUE;

	if (field->type == M_SQL_DATA_TYPE_TEXT && field->d.t.data_alloc)
		return M_TRUE;

	return M_FALSE;
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

	if (M_sql_tabledata_field_is_null(field1) || field1 == NULL) {
		if (M_sql_tabledata_field_is_null(field2) || field2 == NULL)
			return M_TRUE;
		return M_FALSE;
	}

	if (M_sql_tabledata_field_is_null(field2) || field2 == NULL)
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


M_bool M_sql_tabledata_filter_int2dec_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char       *const_temp = NULL;
	M_decimal_t       dec;

	(void)txn;
	(void)field_name;

	/* blank is fine */
	if (!M_sql_tabledata_field_get_text(field, &const_temp) || M_str_isempty(const_temp)) {
		M_sql_tabledata_field_set_null(field);
		return M_TRUE;
	}

	if (M_decimal_from_str(const_temp, M_str_len(const_temp), &dec, NULL) != M_DECIMAL_SUCCESS) {
		M_snprintf(error, error_len, "invalid");
		return M_FALSE;
	}

	if (M_decimal_num_decimals(&dec) > 2) {
		M_snprintf(error, error_len, "too many decimal places");
		return M_FALSE;
	}

	M_sql_tabledata_field_set_int64(field, M_decimal_to_int(&dec, 2));

	return M_TRUE;
}

M_bool M_sql_tabledata_filter_int5dec_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char       *const_temp = NULL;
	M_decimal_t       dec;

	(void)txn;
	(void)field_name;

	/* blank is fine */
	if (!M_sql_tabledata_field_get_text(field, &const_temp) || M_str_isempty(const_temp)) {
		M_sql_tabledata_field_set_null(field);
		return M_TRUE;
	}

	if (M_decimal_from_str(const_temp, M_str_len(const_temp), &dec, NULL) != M_DECIMAL_SUCCESS) {
		M_snprintf(error, error_len, "invalid");
		return M_FALSE;
	}

	if (M_decimal_num_decimals(&dec) > 5) {
		M_snprintf(error, error_len, "too many decimal places");
		return M_FALSE;
	}

	M_sql_tabledata_field_set_int64(field, M_decimal_to_int(&dec, 5));

	return M_TRUE;
}

M_bool M_sql_tabledata_filter_alnum_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char *const_temp = NULL;
	(void)txn;
	(void)field_name;

	/* Blank should be fine */
	if (M_sql_tabledata_field_is_null(field))
		return M_TRUE;

	if (!M_sql_tabledata_field_get_text(field, &const_temp)) {
		M_snprintf(error, error_len, "field is not textual");
		return M_FALSE;
	}

	if (!M_str_isempty(const_temp) && !M_str_isalnum(const_temp)) {
		M_snprintf(error, error_len, "field is not alphanumeric");
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_sql_tabledata_filter_alnumsp_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char *const_temp = NULL;
	(void)txn;
	(void)field_name;

	/* Blank should be fine */
	if (M_sql_tabledata_field_is_null(field))
		return M_TRUE;

	if (!M_sql_tabledata_field_get_text(field, &const_temp)) {
		M_snprintf(error, error_len, "field is not textual");
		return M_FALSE;
	}

	if (!M_str_isempty(const_temp) && !M_str_isalnumsp(const_temp)) {
		M_snprintf(error, error_len, "field is not alphanumeric with spaces");
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_sql_tabledata_filter_alpha_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char *const_temp = NULL;
	(void)txn;
	(void)field_name;

	/* Blank should be fine */
	if (M_sql_tabledata_field_is_null(field))
		return M_TRUE;

	if (!M_sql_tabledata_field_get_text(field, &const_temp)) {
		M_snprintf(error, error_len, "field is not textual");
		return M_FALSE;
	}

	if (!M_str_isempty(const_temp) && !M_str_isalpha(const_temp)) {
		M_snprintf(error, error_len, "field is not alphabetic");
		return M_FALSE;
	}
	return M_TRUE;
}

M_bool M_sql_tabledata_filter_graph_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len)
{
	const char *const_temp = NULL;
	(void)txn;
	(void)field_name;

	/* Blank should be fine */
	if (M_sql_tabledata_field_is_null(field))
		return M_TRUE;

	if (!M_sql_tabledata_field_get_text(field, &const_temp)) {
		M_snprintf(error, error_len, "field is not textual");
		return M_FALSE;
	}

	if (!M_str_isempty(const_temp) && !M_str_isgraph(const_temp)) {
		M_snprintf(error, error_len, "field is not graphical characters");
		return M_FALSE;
	}
	return M_TRUE;
}

static M_bool M_sql_tabledata_field_validate(M_sql_tabledata_field_t *field, const M_sql_tabledata_t *fielddef, M_bool is_add, char *error, size_t error_len)
{
	/* On add, verify field is not null if flag is set */
	if (is_add && fielddef->flags & M_SQL_TABLEDATA_FLAG_NOTNULL && M_sql_tabledata_field_is_null(field)) {
		M_snprintf(error, error_len, "required to not be null");
		return M_FALSE;
	}

	if (M_sql_tabledata_field_is_null(field))
		return M_TRUE;

	switch (fielddef->type) {
		case M_SQL_DATA_TYPE_BOOL:
			if (!M_sql_tabledata_field_get_bool(field, NULL)) {
				M_snprintf(error, error_len, "not boolean");
				return M_FALSE;
			}
			break;
		case M_SQL_DATA_TYPE_INT16:
			if (!M_sql_tabledata_field_get_int16(field, NULL)) {
				M_snprintf(error, error_len, "not a 16bit integer");
				return M_FALSE;
			}
			break;
		case M_SQL_DATA_TYPE_INT32:
			if (!M_sql_tabledata_field_get_int32(field, NULL)) {
				M_snprintf(error, error_len, "not a 32bit integer");
				return M_FALSE;
			}
			break;
		case M_SQL_DATA_TYPE_INT64:
			if (!M_sql_tabledata_field_get_int64(field, NULL)) {
				M_snprintf(error, error_len, "not a 64bit integer");
				return M_FALSE;
			}
			break;
		case M_SQL_DATA_TYPE_TEXT: {
			const char *const_temp = NULL;
			if (!M_sql_tabledata_field_get_text(field, &const_temp)) {
				M_snprintf(error, error_len, "cannot be represented as text");
				return M_FALSE;
			}
			if (!M_str_isprint(const_temp)) {
				M_snprintf(error, error_len, "not printable");
				return M_FALSE;
			}
			break;
		}
		case M_SQL_DATA_TYPE_BINARY:
			if (!M_sql_tabledata_field_get_binary(field, NULL, NULL)) {
				M_snprintf(error, error_len, "cannot be represented as binary");
				return M_FALSE;
			}
			break;
		default:
			M_snprintf(error, error_len, "Invalid data type in field definition");
			return M_FALSE;
	}

	return M_TRUE;
}

static M_sql_error_t M_sql_tabledata_fetch(M_sql_tabledata_txn_t *txn, M_sql_tabledata_field_t *field, const M_sql_tabledata_t *fielddef, M_sql_tabledata_fetch_cb fetch_cb, M_bool is_add, void *thunk, char *error, size_t error_len)
{
	if (fielddef == NULL || fetch_cb == NULL) {
		M_snprintf(error, error_len, "invalid use");
		return M_SQL_ERROR_USER_FAILURE;
	}

	if (M_str_isempty(fielddef->field_name))
		return M_SQL_ERROR_USER_BYPASS;

	/* If the field wasn't specified, then we can skip as long as it isn't a NOT NULL field on add */
	if (!fetch_cb(field, fielddef->field_name, is_add, thunk)) {

		return M_SQL_ERROR_USER_BYPASS;
	}

	/* Run user-supplied field filter */
	if (field && fielddef->filter_cb) {
		char myerror[256] = { 0 };

		if (!fielddef->filter_cb(txn, fielddef->field_name, field, myerror, sizeof(myerror))) {
			M_snprintf(error, error_len, "field %s filter: %s", fielddef->field_name, myerror);
			return M_SQL_ERROR_USER_FAILURE;
		}
	}

	return M_SQL_ERROR_USER_SUCCESS;
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
			/* No validation can be performed if the field name is blank */
			continue;
		}

		if (M_hash_dict_get(seen_fields, fields[i].field_name, NULL)) {
			M_snprintf(error, error_len, "Duplicate field name %s specified", fields[i].field_name);
			goto done;
		}
		M_hash_dict_insert(seen_fields, fields[i].field_name, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both editable and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both virtual and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL && fields[i].type == M_SQL_DATA_TYPE_BINARY) {
			M_snprintf(error, error_len, "field %s cannot be both virtual and binary", fields[i].field_name);
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

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TIMESTAMP) {
			if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
				M_snprintf(error, error_len, "field %s cannot be both an ID and a Timestamp", fields[i].field_name);
				goto done;
			}
			if (fields[i].type != M_SQL_DATA_TYPE_INT64) {
				M_snprintf(error, error_len, "field %s is a Timestamp field, must be INT64", fields[i].field_name);
				goto done;
			}
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
					M_sql_stmt_bind_text_dup(stmt, field->d.t.data_alloc, M_MIN(len, max_column_len));
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
					M_sql_stmt_bind_binary_dup(stmt, field->d.bin.data_alloc, M_MIN(len, max_column_len));
				} else {
					M_sql_stmt_bind_binary_const(stmt, bin, M_MIN(len, max_column_len));
				}
			}
			break;

		case M_SQL_DATA_TYPE_UNKNOWN:
		default:
			return M_FALSE;
	}

	return M_TRUE;
}



struct M_sql_tabledata_txn {
	M_bool                         is_add;      /*!< Add vs Edit operation*/
	M_bool                         edit_insert_not_found; /*<! If the record is not found on edit, insert it */
	const char                    *table_name;  /*!< Table Name */
	const M_sql_tabledata_t       *fields;      /*!< Table definition, per field */
	size_t                         num_fields;  /*!< Number of fields in the table definition */

	M_sql_tabledata_fetch_cb       fetch_cb;    /*!< Callback to fetch a requested field. Should not perform any validation. */
	M_sql_tabledata_notify_cb      notify_cb;   /*!< Callback that is called at completion of an add/edit. */

	M_hash_strvp_t                *curr_fields; /*!< Current fields fetched */

	M_hash_strvp_t                *prev_fields; /*!< Previous fields (edit only) */

	M_int64                        generated_id; /*!< Unique record id generated during add. Add only */
	void                          *thunk;        /*!< User-specified argument passed to callbacks */
};


void *M_sql_tabledata_txn_get_thunk(M_sql_tabledata_txn_t *txn)
{
	if (txn == NULL)
		return NULL;
	return txn->thunk;
}

const char *M_sql_tabledata_txn_get_tablename(M_sql_tabledata_txn_t *txn)
{
	if (txn == NULL)
		return NULL;
	return txn->table_name;
}

M_int64 M_sql_tabledata_txn_get_generated_id(M_sql_tabledata_txn_t *txn)
{
	if (txn == NULL || !txn->is_add)
		return 0;
	return txn->generated_id;
}

M_bool M_sql_tabledata_txn_is_add(M_sql_tabledata_txn_t *txn)
{
	if (txn == NULL || !txn->is_add)
		return M_FALSE;
	return M_TRUE;
}

static void M_sql_tabledata_txn_destroy(M_sql_tabledata_txn_t *txn)
{
	if (txn == NULL)
		return;

	M_hash_strvp_destroy(txn->curr_fields, M_TRUE);
	M_hash_strvp_destroy(txn->prev_fields, M_TRUE);
}

static void M_sql_tabledata_field_free_cb(void *arg)
{
	M_sql_tabledata_field_t *field = arg;
	if (field == NULL)
		return;
	M_sql_tabledata_field_clear(field);
	M_free(field);
}

static void M_sql_tabledata_txn_reset(M_sql_tabledata_txn_t *txn)
{
	M_hash_strvp_destroy(txn->curr_fields, M_TRUE);
	txn->curr_fields = NULL;

	M_hash_strvp_destroy(txn->prev_fields, M_TRUE);
	txn->prev_fields = NULL;

	txn->curr_fields = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_sql_tabledata_field_free_cb);

	if (!txn->is_add) {
		txn->prev_fields = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_sql_tabledata_field_free_cb);
	}
}

static void M_sql_tabledata_txn_create(M_sql_tabledata_txn_t *txn, M_bool is_add, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk)
{
	M_mem_set(txn, 0, sizeof(*txn));
	txn->is_add      = is_add;
	txn->table_name  = table_name;
	txn->fields      = fields;
	txn->num_fields  = num_fields;
	txn->fetch_cb    = fetch_cb;
	txn->notify_cb   = notify_cb;
	txn->thunk       = thunk;

	M_sql_tabledata_txn_reset(txn);
}

#if 0
static void printf_field(M_sql_tabledata_field_t *field, const M_sql_tabledata_t *fielddef)
{

	switch (fielddef->type) {
		case M_SQL_DATA_TYPE_BOOL: {
			M_bool b;
			if (M_sql_tabledata_field_get_bool(field, &b)) {
				M_printf("bool(%s)", b?"yes":"no");
				return;
			}
			break;
		}
		case M_SQL_DATA_TYPE_INT16: {
			M_int16 i;
			if (M_sql_tabledata_field_get_int16(field, &i)) {
				M_printf("i16(%lld)", (M_int64)i);
				return;
			}
			break;
		}
		case M_SQL_DATA_TYPE_INT32: {
			M_int32 i;
			if (M_sql_tabledata_field_get_int32(field, &i)) {
				M_printf("i32(%lld)", (M_int64)i);
				return;
			}
			break;
		}

		case M_SQL_DATA_TYPE_INT64: {
			M_int64 i;
			if (M_sql_tabledata_field_get_int64(field, &i)) {
				M_printf("i64(%lld)", (M_int64)i);
				return;
			}
			break;
		}

		case M_SQL_DATA_TYPE_TEXT: {
			const char *const_temp;
			if (M_sql_tabledata_field_get_text(field, &const_temp)) {
				M_printf("text(%s)", const_temp);
				return;
			}
			break;
		}
		case M_SQL_DATA_TYPE_BINARY: {
			size_t len;
			if (M_sql_tabledata_field_get_binary(field, NULL, &len)) {
				M_printf("binary(%zu)", len);
				return;
			}
			break;
		}
		default:
			break;
	}

	M_printf("NULL");
}
#endif

const M_sql_tabledata_t *M_sql_tabledata_txn_fetch_fielddef(M_sql_tabledata_txn_t *txn, const char *field_name)
{
	size_t i;
	for (i=0; i<txn->num_fields; i++) {
		if (M_str_caseeq(field_name, txn->fields[i].field_name))
			return &txn->fields[i];
	}
	return NULL;
}

static M_sql_error_t M_sql_tabledata_txn_fetch_current(M_sql_trans_t *sqltrans, M_sql_tabledata_txn_t *txn, char *error, size_t error_len)
{
	size_t        i;

	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field = M_malloc_zero(sizeof(*field));
		M_sql_error_t            err;

		if (txn->is_add && txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID_GENERATE) {
			size_t  max_len = txn->fields[i].max_column_len;
			M_int64 id;
			if (max_len == 0) {
				if (txn->fields[i].type == M_SQL_DATA_TYPE_INT32) {
					max_len = 9;
				} else {
					max_len = 18;
				}
			}
			if (max_len > 18)
				max_len = 18;
			id                = M_sql_gen_timerand_id(M_sql_trans_get_pool(sqltrans), max_len);
			M_sql_tabledata_field_set_int64(field, id);
			txn->generated_id = id;
			M_hash_strvp_insert(txn->curr_fields, txn->fields[i].field_name, field);
			continue;
		}

		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_TIMESTAMP) {
			if (txn->is_add || txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE) {
				M_sql_tabledata_field_set_int64(field, M_time());
				M_hash_strvp_insert(txn->curr_fields, txn->fields[i].field_name, field);
			}
			continue;
		}

		err = M_sql_tabledata_fetch(txn, field, &txn->fields[i], txn->fetch_cb, txn->is_add, txn->thunk, error, error_len);
		if (M_sql_error_is_error(err)) {
			M_sql_tabledata_field_free_cb(field);
			return err;
		} else if (err == M_SQL_ERROR_USER_BYPASS) {
			M_sql_tabledata_field_free_cb(field);
			continue;
		}
		M_hash_strvp_insert(txn->curr_fields, txn->fields[i].field_name, field);
	}

	return M_SQL_ERROR_USER_SUCCESS;
}


M_sql_tabledata_field_t *M_sql_tabledata_txn_field_get(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_txn_field_select_t fselect)
{
	M_sql_tabledata_field_t *field = NULL;

	/* Merged is always the current on add */
	if (txn->is_add && fselect == M_SQL_TABLEDATA_TXN_FIELD_MERGED)
		fselect = M_SQL_TABLEDATA_TXN_FIELD_CURRENT;
	if (txn->is_add && fselect == M_SQL_TABLEDATA_TXN_FIELD_MERGED_NODUPLICATE)
		fselect = M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY;

	/* Not possible to get prior on add */
	if (txn->is_add && fselect == M_SQL_TABLEDATA_TXN_FIELD_PRIOR)
		return NULL;

	switch (fselect) {
		case M_SQL_TABLEDATA_TXN_FIELD_PRIOR:
			field = M_hash_strvp_get_direct(txn->prev_fields, field_name);
			break;

		case M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY:
			field = M_hash_strvp_get_direct(txn->curr_fields, field_name);
			break;

		case M_SQL_TABLEDATA_TXN_FIELD_CURRENT:
			field = M_hash_strvp_get_direct(txn->curr_fields, field_name);
			if (field == NULL && M_sql_tabledata_txn_fetch_fielddef(txn, field_name) != NULL) {
				/* Create new emtpy entry */
				field = M_malloc_zero(sizeof(*field));
				M_sql_tabledata_field_set_null(field);
				M_hash_strvp_insert(txn->curr_fields, field_name, field);
			}
			break;

		case M_SQL_TABLEDATA_TXN_FIELD_MERGED:
			field = M_hash_strvp_get_direct(txn->curr_fields, field_name);
			if (field == NULL) {
				field = M_hash_strvp_get_direct(txn->prev_fields, field_name);
				if (field != NULL) {
					/* Duplicate into own entry, might be edited */
					field = M_sql_tabledata_field_duplicate(field);
					M_hash_strvp_insert(txn->curr_fields, field_name, field);
				} else if (M_sql_tabledata_txn_fetch_fielddef(txn, field_name) != NULL) {
					/* Create new emtpy entry */
					field = M_malloc_zero(sizeof(*field));
					M_sql_tabledata_field_set_null(field);
					M_hash_strvp_insert(txn->curr_fields, field_name, field);
				}
			}
			break;

		case M_SQL_TABLEDATA_TXN_FIELD_MERGED_NODUPLICATE:
			field = M_hash_strvp_get_direct(txn->curr_fields, field_name);
			if (field == NULL) {
				field = M_hash_strvp_get_direct(txn->prev_fields, field_name);
			}
			break;
	}

	return field;
}


M_bool M_sql_tabledata_txn_field_changed(M_sql_tabledata_txn_t *txn, const char *field_name)
{
	M_sql_tabledata_field_t    *prior_field;
	M_sql_tabledata_field_t    *curr_field;

	curr_field  = M_sql_tabledata_txn_field_get(txn, field_name, M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY);

	/* If the current field wasn't specified, no change */
	if (curr_field == NULL)
		return M_FALSE;

	prior_field = M_sql_tabledata_txn_field_get(txn, field_name, M_SQL_TABLEDATA_TXN_FIELD_PRIOR);

	/* If didn't exist previously, but does exist now (and is not null), its a change. */
	if (prior_field == NULL && !M_sql_tabledata_field_is_null(curr_field))
		return M_TRUE;

	/* If its the same, its not changed */
	if (M_sql_tabledata_field_eq(prior_field, curr_field)) {
		return M_FALSE;
	}

	/* Its different, so changed */
	return M_TRUE;
}


static M_bool M_sql_tabledata_txn_sanitycheck_fields(M_sql_tabledata_txn_t *txn, char *error, size_t error_len)
{
	size_t i;

	/* Validate: NOTNULL, EDITABLE (!is_add), ID_REQUIRED (!is_add) */

	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field;
		char                     myerror[256] = { 0 };

		if (M_str_isempty(txn->fields[i].field_name))
			continue;

		field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_MERGED_NODUPLICATE);

		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_NOTNULL) {
			if (field == NULL || M_sql_tabledata_field_is_null(field)) {
				M_snprintf(error, error_len, "%s is required to be NOT NULL", txn->fields[i].field_name);
				return M_FALSE;
			}
		}

		if (!txn->is_add && txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID_REQUIRED) {
			if (field == NULL || M_sql_tabledata_field_is_null(field)) {
				M_snprintf(error, error_len, "The %s ID is required to be provided", txn->fields[i].field_name);
				return M_FALSE;
			}
		}

		if (!txn->is_add && !(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE)) {
			if (M_sql_tabledata_txn_field_changed(txn, txn->fields[i].field_name)) {
				M_snprintf(error, error_len, "%s is not editable", txn->fields[i].field_name);
				return M_FALSE;
			}
		}

		/* Run stock validator - after all user supplied callbacks for filter and validate */
		if (!M_sql_tabledata_field_validate(field, &txn->fields[i], M_sql_tabledata_txn_is_add(txn), myerror, sizeof(myerror))) {
			M_snprintf(error, error_len, "field %s validator: %s", txn->fields[i].field_name, myerror);
			return M_FALSE;
		}
	}

	return M_TRUE;
}

static M_sql_error_t M_sql_tabledata_txn_virtual_get(M_sql_tabledata_field_t *out_field, M_sql_tabledata_txn_t *txn, const char *table_column, char *error, size_t error_len)
{
	size_t                     i;
	M_hash_dict_t             *dict          = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_LOWER);
	M_bool                     field_updated = M_FALSE;
	M_sql_error_t              ret           = M_SQL_ERROR_USER_FAILURE;

	M_sql_tabledata_field_clear(out_field);

	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field;
		const char              *text = NULL;

		if (!M_str_caseeq(txn->fields[i].table_column, table_column))
			continue;

		/* If field has changed, make sure we mark it as such */
		if (M_sql_tabledata_txn_field_changed(txn, txn->fields[i].field_name)) {
			field_updated = M_TRUE;
		}

		field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_MERGED_NODUPLICATE);

		/* A blank/null field here is the same as not existing */
		if (field == NULL || M_sql_tabledata_field_is_null(field))
			continue;

		if (!M_sql_tabledata_field_get_text(field, &text)) {
			M_snprintf(error, error_len, "field %s cannot be converted to text", txn->fields[i].field_name);
			goto done;
		}
		M_hash_dict_insert(dict, txn->fields[i].field_name, text);
	}

	/* Only serialize if there are updated fields */
	if (!field_updated) {
		M_sql_tabledata_field_set_null(out_field);
		ret = M_SQL_ERROR_USER_BYPASS;
		goto done;
	}

	M_sql_tabledata_field_set_text_own(out_field, M_hash_dict_serialize(dict, '|', '=', '"', '"', M_HASH_DICT_SER_FLAG_NONE));

	ret = M_SQL_ERROR_USER_SUCCESS;

done:
	M_hash_dict_destroy(dict); dict = NULL;

	return ret;
}


static M_sql_error_t M_sql_tabledata_txn_uservalidate_fields(M_sql_trans_t *sqltrans, M_sql_tabledata_txn_t *txn, char *error, size_t error_len)
{
	size_t        i;
	M_sql_error_t err = M_SQL_ERROR_USER_SUCCESS;

	for (i=0; i<txn->num_fields; i++) {
		char myerror[256] = { 0 };

		/* Skip fields that are null */
		if (M_str_isempty(txn->fields[i].field_name))
			continue;

/* Should still run.  The user-callback needs to handle this */
#if 0
		/* Skip non-editable fields on edit */
		if (!txn->is_add && !(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE))
			continue;
#endif

		if (!txn->fields[i].validate_cb)
			continue;

		err = txn->fields[i].validate_cb(sqltrans, txn, txn->fields[i].field_name, myerror, sizeof(myerror));
		if (M_sql_error_is_error(err)) {
			/* Copy error if user failure */
			if (err == M_SQL_ERROR_USER_FAILURE) {
				M_snprintf(error, error_len, "field %s: %s", txn->fields[i].field_name, myerror);
			}
			break;
		}
	}

	return err;
}


static M_sql_error_t M_sql_tabledata_add_do_int(M_sql_trans_t *sqltrans, void *arg, char *error, size_t error_len)
{
	M_sql_tabledata_txn_t  *txn         = arg;
	M_buf_t                *request     = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	size_t                  i;
	M_bool                  has_col     = M_FALSE;
	M_sql_error_t           err;
	M_sql_error_t           rv          = M_SQL_ERROR_USER_FAILURE;

	M_sql_tabledata_txn_reset(txn);

	err         = M_sql_tabledata_txn_fetch_current(sqltrans, txn, error, error_len);
	if (M_sql_error_is_error(err))
		return err;

	err = M_sql_tabledata_txn_uservalidate_fields(sqltrans, txn, error, error_len);
	if (M_sql_error_is_error(err)) {
		rv = err;
		goto done;
	}

	if (!M_sql_tabledata_txn_sanitycheck_fields(txn, error, error_len)) {
		goto done;
	}

	request     = M_buf_create();
	seen_cols   = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	stmt        = M_sql_stmt_create();

	M_buf_add_str(request, "INSERT INTO \"");
	M_buf_add_str(request, txn->table_name);
	M_buf_add_str(request, "\" (");

	/* Specify each column name we will be outputting (in case the table as more columns than this) */
	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field = NULL;

		/* Skip empty field name entries (they aren't managed by us) */
		if (M_str_isempty(txn->fields[i].field_name))
			continue;

		/* Virtual columns can share a column name, so make sure we don't emit
		 * multiple columns for virtuals */
		if (M_hash_dict_get(seen_cols, txn->fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, txn->fields[i].table_column, NULL);


		/* Skip if field blank and not a virtual column which always gets emitted */
		if (!(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)) {
			field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY);
			if (field == NULL || M_sql_tabledata_field_is_null(field))
				continue;
		}

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, txn->fields[i].table_column);
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
	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field;
		M_sql_tabledata_field_t *alloc_field = NULL;
		M_bool                   brv;

		/* Skip empty field name entries (they aren't managed by us) */
		if (M_str_isempty(txn->fields[i].field_name))
			continue;

		if (M_hash_dict_get(seen_cols, txn->fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, txn->fields[i].table_column, NULL);

		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			alloc_field = M_malloc_zero(sizeof(*alloc_field));
			M_sql_tabledata_field_clear(alloc_field);

			field       = alloc_field;

			err = M_sql_tabledata_txn_virtual_get(field, txn, txn->fields[i].table_column, error, error_len);
			if (M_sql_error_is_error(err)) {
				rv = err;
				M_sql_tabledata_field_free_cb(alloc_field);
				goto done;
			}

			/* Virtual columns should actually bind NULL, so no skipping */
		} else {
			field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY);
			if (field == NULL || M_sql_tabledata_field_is_null(field))
				continue;
		}

		/* NOTE: just because field_data is NULL doesn't mean they don't intend us to actually bind NULL */

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "?");

		brv = M_sql_tabledata_bind(
		    stmt,
		    (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?M_SQL_DATA_TYPE_TEXT:txn->fields[i].type,
		    field,
		    (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?SIZE_MAX:txn->fields[i].max_column_len
		);

		M_sql_tabledata_field_free_cb(alloc_field);

		if (!brv) {
			M_snprintf(error, error_len, "column %s unsupported field type", txn->fields[i].table_column);
			goto done;
		}
	}

	M_buf_add_str(request, ")");
	rv = M_sql_stmt_prepare_buf(stmt, request);
	request = NULL;
	if (M_sql_error_is_error(rv))
		goto done;

	rv = M_sql_trans_execute(sqltrans, stmt);

done:
	if (request)
		M_buf_cancel(request);
	if (seen_cols)
		M_hash_dict_destroy(seen_cols);
	if (stmt != NULL) {
		if (M_sql_error_is_error(rv) && rv != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}

	/* Call notify-callback as fields have been updated (but not if USER_SUCCESS or failure) */
	if (rv == M_SQL_ERROR_SUCCESS && txn->notify_cb) {
		err = txn->notify_cb(sqltrans, txn, error, error_len);
		if (M_sql_error_is_error(err))
			rv = err;
	}

	return rv;
}


static M_sql_error_t M_sql_tabledata_add_do(M_sql_trans_t *sqltrans, void *arg, char *error, size_t error_len)
{
	size_t                 cnt = 0;
	M_sql_error_t          err = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_txn_t *txn = arg;

	/* TODO: Loop up to 10x if key conflict AND table had an auto-generated id */
	do {
		err = M_sql_tabledata_add_do_int(sqltrans, txn, error, error_len);
		cnt++;
	} while (err == M_SQL_ERROR_QUERY_CONSTRAINT && txn->generated_id != 0 && cnt < 10);

	return err;
}


static M_sql_error_t M_sql_tabledata_add_int(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	M_sql_error_t         err  = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_txn_t txn;

	M_sql_tabledata_txn_create(&txn, M_TRUE, table_name, fields, num_fields, fetch_cb, notify_cb, thunk);

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

	if (!M_sql_tabledata_validate_fields(fields, num_fields, error, error_len)) {
		err = M_SQL_ERROR_USER_FAILURE;
		goto done;
	}

	if (generated_id)
		*generated_id = 0;

	if (sqltrans != NULL) {
		err  = M_sql_tabledata_add_do(sqltrans, &txn, error, error_len);
	} else {
		err  = M_sql_trans_process(pool, M_SQL_ISOLATION_SERIALIZABLE, M_sql_tabledata_add_do, &txn, error, error_len);
	}

	if (!M_sql_error_is_error(err) && generated_id != NULL)
		*generated_id = txn.generated_id;

done:
	M_sql_tabledata_txn_destroy(&txn);
	return err;
}


M_sql_error_t M_sql_tabledata_add(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	return M_sql_tabledata_add_int(pool, NULL, table_name, fields, num_fields, fetch_cb, notify_cb, thunk, generated_id, error, error_len);
}

M_sql_error_t M_sql_tabledata_trans_add(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len)
{
	return M_sql_tabledata_add_int(NULL, sqltrans, table_name, fields, num_fields, fetch_cb, notify_cb, thunk, generated_id, error, error_len);
}



static M_sql_error_t M_sql_tabledata_txn_fetch_prior(M_sql_trans_t *sqltrans, M_sql_tabledata_txn_t *txn, char *error, size_t error_len)
{
	M_buf_t                *request     = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_bool                  has_col     = M_FALSE;
	size_t                  i;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;

	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	request   = M_buf_create();
	stmt      = M_sql_stmt_create();

	M_buf_add_str(request, "SELECT ");
	for (i=0; i<txn->num_fields; i++) {
		if (M_hash_dict_get(seen_cols, txn->fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, txn->fields[i].table_column, NULL);

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, txn->fields[i].table_column);
		M_buf_add_str(request, "\"");
	}
	M_buf_add_str(request, " FROM \"");
	M_buf_add_str(request, txn->table_name);
	M_buf_add_str(request, "\"");
	M_sql_query_append_updlock(M_sql_trans_get_pool(sqltrans), request, M_SQL_QUERY_UPDLOCK_TABLE, NULL);
	M_buf_add_str(request, " WHERE ");

	has_col = M_FALSE;

	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field;

		if (!(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY);
		if (field == NULL || M_sql_tabledata_field_is_null(field)) {
			if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID_REQUIRED) {
				M_snprintf(error, error_len, "required field %s not specified", txn->fields[i].field_name);
				goto done;
			}
			continue;
		}

		if (has_col) {
			M_buf_add_str(request, " AND ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, txn->fields[i].table_column);
		M_buf_add_str(request, "\" = ?");
		if (!M_sql_tabledata_bind(stmt, txn->fields[i].type, field, txn->fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", txn->fields[i].table_column);
			goto done;
		}
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
		if (txn->edit_insert_not_found) {
			err = M_SQL_ERROR_USER_BYPASS;
		} else {
			M_snprintf(error, error_len, "no match found");
			err = M_SQL_ERROR_USER_FAILURE;
		}
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
	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *myfield = NULL;

		/* Skip columns we already have (multiple virtual fields) */
		if (M_hash_dict_get(seen_cols, txn->fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, txn->fields[i].table_column, NULL);

		/* Need to deserialize and add to output params */
		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			const char         *col      = M_sql_stmt_result_text_byname_direct(stmt, 0, txn->fields[i].table_column);
			M_hash_dict_t      *dict     = M_hash_dict_deserialize(col, M_str_len(col), '|', '=', '"', '"', M_HASH_DICT_CASECMP);
			M_hash_dict_enum_t *hashenum = NULL;
			const char         *key      = NULL;
			const char         *val      = NULL;

			M_hash_dict_enumerate(dict, &hashenum);
			while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
				myfield = M_malloc_zero(sizeof(*myfield));
				M_sql_tabledata_field_set_text_dup(myfield, val);
				M_hash_strvp_insert(txn->prev_fields, key, myfield);
				myfield = NULL;
			}
			M_hash_dict_enumerate_free(hashenum);
			M_hash_dict_destroy(dict);
		} else {
			myfield = M_malloc_zero(sizeof(*myfield));
			M_sql_tabledata_field_clear(myfield);

			if (M_sql_stmt_result_isnull_byname_direct(stmt, 0, txn->fields[i].table_column)) {
				M_sql_tabledata_field_set_null(myfield);
			} else {
				switch (M_sql_stmt_result_col_type_byname(stmt, txn->fields[i].table_column, NULL)) {
					case M_SQL_DATA_TYPE_BOOL:
						M_sql_tabledata_field_set_bool(myfield, M_sql_stmt_result_bool_byname_direct(stmt, 0, txn->fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT16:
						M_sql_tabledata_field_set_int16(myfield, M_sql_stmt_result_int16_byname_direct(stmt, 0, txn->fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT32:
						M_sql_tabledata_field_set_int32(myfield, M_sql_stmt_result_int32_byname_direct(stmt, 0, txn->fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_INT64:
						M_sql_tabledata_field_set_int64(myfield, M_sql_stmt_result_int64_byname_direct(stmt, 0, txn->fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_TEXT:
						M_sql_tabledata_field_set_text_dup(myfield, M_sql_stmt_result_text_byname_direct(stmt, 0, txn->fields[i].table_column));
						break;

					case M_SQL_DATA_TYPE_BINARY:
						{
							size_t               len = 0;
							const unsigned char *bin = M_sql_stmt_result_binary_byname_direct(stmt, 0, txn->fields[i].table_column, &len);
							M_sql_tabledata_field_set_binary_dup(myfield, bin, len);
						}
						break;
					case M_SQL_DATA_TYPE_UNKNOWN:
						break;
				}
			}

			M_hash_strvp_insert(txn->prev_fields, txn->fields[i].field_name, myfield);
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


static M_sql_error_t M_sql_tabledata_edit_do(M_sql_trans_t *sqltrans, void *arg, char *error, size_t error_len)
{
	M_buf_t                *request     = NULL;
	M_hash_dict_t          *seen_cols   = NULL;
	M_sql_stmt_t           *stmt        = NULL;
	size_t                  i;
	M_bool                  has_col     = M_FALSE;
	M_hash_strvp_t         *prev_fields = NULL;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_txn_t  *txn         = arg;
	M_bool                  has_update  = M_FALSE;

	txn->is_add = M_FALSE;
	M_sql_tabledata_txn_reset(txn);

	err = M_sql_tabledata_txn_fetch_current(sqltrans, txn, error, error_len);
	if (M_sql_error_is_error(err))
		return err;

	err = M_sql_tabledata_txn_fetch_prior(sqltrans, txn, error, error_len);
	/* If the record is not found on an edit, and the flag is set to insert if not found, switch to an add operation */
	if (err == M_SQL_ERROR_USER_BYPASS && txn->edit_insert_not_found) {
		txn->is_add = M_TRUE;
		return M_sql_tabledata_add_do(sqltrans, arg, error, error_len);
	}

	if (M_sql_error_is_error(err)) {
		goto done;
	}

	err = M_sql_tabledata_txn_uservalidate_fields(sqltrans, txn, error, error_len);
	if (M_sql_error_is_error(err))
		goto done;

	err       = M_SQL_ERROR_USER_FAILURE;

	if (!M_sql_tabledata_txn_sanitycheck_fields(txn, error, error_len)) {
		goto done;
	}

	seen_cols = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	request   = M_buf_create();
	stmt      = M_sql_stmt_create();
	M_buf_add_str(request, "UPDATE \"");
	M_buf_add_str(request, txn->table_name);
	M_buf_add_str(request, "\" SET ");

	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field = NULL;
		M_sql_tabledata_field_t *alloc_field = NULL;

		/* Skip fields that are null */
		if (M_str_isempty(txn->fields[i].field_name))
			continue;

		/* Skip ID columns */
		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		/* Skip columns we already have (multiple virtual fields) */
		if (M_hash_dict_get(seen_cols, txn->fields[i].table_column, NULL)) {
			continue;
		}

		M_hash_dict_insert(seen_cols, txn->fields[i].table_column, NULL);

		if (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			M_sql_error_t rv;

			alloc_field = M_malloc_zero(sizeof(*alloc_field));
			M_sql_tabledata_field_clear(alloc_field);

			field       = alloc_field;

			rv = M_sql_tabledata_txn_virtual_get(field, txn, txn->fields[i].table_column, error, error_len);
			if (M_sql_error_is_error(rv)) {
				err = rv;
				M_sql_tabledata_field_free_cb(alloc_field);
				goto done;
			}

			/* Skip if no values changed */
			if (rv == M_SQL_ERROR_USER_BYPASS) {
				M_sql_tabledata_field_free_cb(alloc_field);
				continue;
			}
		} else if (!M_sql_tabledata_txn_field_changed(txn, txn->fields[i].field_name)) {
			continue;
		} else {
			field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_MERGED_NODUPLICATE);
		}

		if (has_col)
			M_buf_add_str(request, ", ");
		has_col = M_TRUE;

		/* Auto-generated timestamp fields should not be considered a real update */
		if (!(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_TIMESTAMP))
			has_update = M_TRUE;

		M_buf_add_str(request, "\"");
		M_buf_add_str(request, txn->fields[i].table_column);
		M_buf_add_str(request, "\" = ?");

		if (!M_sql_tabledata_bind(
		     stmt,
		     (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?M_SQL_DATA_TYPE_TEXT:txn->fields[i].type,
		     field,
		     (txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?SIZE_MAX:txn->fields[i].max_column_len)
		) {
			M_snprintf(error, error_len, "column %s unsupported field type (%d vs %d)", txn->fields[i].table_column, (int)txn->fields[i].type, (int)M_sql_tabledata_field_type(field));
			M_sql_tabledata_field_free_cb(alloc_field);
			goto done;
		}

		M_sql_tabledata_field_free_cb(alloc_field);
	}

	if (!has_update) {
		err = M_SQL_ERROR_USER_SUCCESS;
		M_snprintf(error, error_len, "no data has changed");
		goto done;
	}

	/* Now output what row to update */
	M_buf_add_str(request, " WHERE ");
	has_col = M_FALSE;
	for (i=0; i<txn->num_fields; i++) {
		M_sql_tabledata_field_t *field = NULL;

		/* Skip fields that are null */
		if (M_str_isempty(txn->fields[i].field_name))
			continue;

		/* Skip non-ID columns */
		if (!(txn->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		field = M_sql_tabledata_txn_field_get(txn, txn->fields[i].field_name, M_SQL_TABLEDATA_TXN_FIELD_CURRENT_READONLY);
		if (field == NULL)
			continue;

		if (has_col) {
			M_buf_add_str(request, " AND ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, txn->fields[i].table_column);
		M_buf_add_str(request, "\" = ?");
		if (!M_sql_tabledata_bind(stmt, txn->fields[i].type, field, txn->fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type (%d vs %d)", txn->fields[i].table_column, (int)txn->fields[i].type, (int)M_sql_tabledata_field_type(field));
			goto done;
		}
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

	/* Call notify-callback as fields have been updated (but not if USER_SUCCESS or failure) */
	if (err == M_SQL_ERROR_SUCCESS && txn->notify_cb) {
		M_sql_error_t myerr = txn->notify_cb(sqltrans, txn, error, error_len);
		if (M_sql_error_is_error(myerr))
			err = myerr;
	}

	return err;
}


static M_sql_error_t M_sql_tabledata_edit_int(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, M_bool insert_not_found, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len)
{
	M_sql_error_t          err  = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_txn_t  txn;

	M_sql_tabledata_txn_create(&txn, M_FALSE, table_name, fields, num_fields, fetch_cb, notify_cb, thunk);
	txn.edit_insert_not_found = insert_not_found;

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
		err = M_sql_tabledata_edit_do(sqltrans, &txn, error, error_len);
	} else {
		err = M_sql_trans_process(pool, M_SQL_ISOLATION_SERIALIZABLE, M_sql_tabledata_edit_do, &txn, error, error_len);
	}

done:
	M_sql_tabledata_txn_destroy(&txn);
	return err;
}


M_sql_error_t M_sql_tabledata_edit(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(pool, NULL, table_name, M_FALSE, fields, num_fields, fetch_cb, notify_cb, thunk, error, error_len);
}


M_sql_error_t M_sql_tabledata_trans_edit(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(NULL, sqltrans, table_name, M_FALSE, fields, num_fields, fetch_cb, notify_cb, thunk, error, error_len);
}

M_sql_error_t M_sql_tabledata_upsert(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(pool, NULL, table_name, M_TRUE, fields, num_fields, fetch_cb, notify_cb, thunk, error, error_len);
}


M_sql_error_t M_sql_tabledata_trans_upsert(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len)
{
	return M_sql_tabledata_edit_int(NULL, sqltrans, table_name, M_TRUE, fields, num_fields, fetch_cb, notify_cb, thunk, error, error_len);
}

M_sql_tabledata_t *M_sql_tabledata_append_virtual_list(const M_sql_tabledata_t *fields, size_t *num_fields, const char *table_column, const M_list_str_t *field_names, size_t max_len, M_sql_tabledata_flags_t flags)
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


M_bool M_sql_tabledata_to_table(M_sql_table_t *table, const M_sql_tabledata_t *fields, size_t num_fields)
{
	size_t         i;
	M_hash_dict_t *seen_fields = NULL;
	M_bool         rv          = M_FALSE;

	if (table == NULL || fields == NULL)
		return M_FALSE;

	seen_fields = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);

	for (i=0; i<num_fields; i++) {
		M_uint32 flags = M_SQL_TABLE_COL_FLAG_NONE;

		/* Only insert virtual columns once */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL) {
			size_t j;
			if (M_hash_dict_get(seen_fields, fields[i].table_column, NULL))
				continue;

			M_hash_dict_insert(seen_fields, fields[i].table_column, NULL);

			/* Scan other tagged fields under this name for field flags we may need to set */
			for (j=i+1; j<num_fields; j++) {
				if (M_str_caseeq(fields[i].table_column, fields[j].table_column)) {
					if (fields[j].flags & M_SQL_TABLEDATA_FLAG_NOTNULL) {
						flags |= M_SQL_TABLE_COL_FLAG_NOTNULL;
					}
				}
			}

		}

		if (fields[i].flags & (M_SQL_TABLEDATA_FLAG_NOTNULL|M_SQL_TABLEDATA_FLAG_ID_GENERATE))
			flags |= M_SQL_TABLE_COL_FLAG_NOTNULL;

		if (!M_sql_table_add_col(table, flags, fields[i].table_column,
			(fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?M_SQL_DATA_TYPE_TEXT:fields[i].type,
			(fields[i].flags & M_SQL_TABLEDATA_FLAG_VIRTUAL)?(64*1024):fields[i].max_column_len, NULL))
			goto fail;
	}

	rv = M_TRUE;

fail:
	M_hash_dict_destroy(seen_fields);
	return rv;
}
