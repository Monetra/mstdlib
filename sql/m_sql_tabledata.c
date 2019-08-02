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

/* Returns M_TRUE on change, M_FALSE on no change */
static M_bool M_sql_tabledata_edit_fetch_int(char **field_data, size_t *field_data_len, const char *field_name, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strbin_t *prev_fields)
{
	const unsigned char *prior_data     = NULL;
	size_t               prior_data_len = 0;

	/* If the field wasn't specified, then we use the old value, so no change */
	if (!fetch_cb(field_data, field_data_len, field_name, thunk))
		return M_FALSE;

	/* If didn't exist previously, but does exist now, its a change. */
	if (!M_hash_strbin_get(prev_fields, field_name, &prior_data, &prior_data_len))
		return M_TRUE;

	/* If it doesn't match length or data, its a change */
	if (prior_data_len != *field_data_len)
		return M_TRUE;

	if (!M_mem_eq(*field_data, prior_data, *field_data_len))
		return M_TRUE;

	/* No change */
	return M_FALSE;
}


/* Returns M_FALSE if field was not specified, or if it matches the previous value.
 * If output_always is M_TRUE, then field_data and field_data_len will be filled in even if no change is occurring...
 * this is used for tagged fields otherwise we'd lose data. */
static M_bool M_sql_tabledata_edit_fetch(char **field_data, size_t *field_data_len, const char *field_name, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strbin_t *prev_fields, M_bool output_identical)
{
	M_bool rv = M_sql_tabledata_edit_fetch_int(field_data, field_data_len, field_name, fetch_cb, thunk, prev_fields);

	if (output_identical && rv == M_FALSE) {
		const unsigned char *prior_data     = NULL;
		size_t               prior_data_len = 0;

		if (M_hash_strbin_get(prev_fields, field_name, &prior_data, &prior_data_len)) {
			*field_data     = (char *)M_memdup(prior_data, prior_data_len);
			*field_data_len = prior_data_len;
		}
	}

	return rv;
}

/* NOTE: if prev_fields is specified, it is an edit, otherwise its an add.  If NO fields have changed on edit, will return NULL. */
static char *M_sql_tabledata_row_gather_tagged(M_sql_tabledata_t *fields, size_t num_fields, size_t curr_idx, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, M_hash_strbin_t *prev_fields)
{
	const char    *column_name   = fields[curr_idx].table_column;
	size_t         i;
	M_hash_dict_t *dict          = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_LOWER);
	char          *out           = NULL;
	M_bool         field_updated = M_FALSE;

	for (i=curr_idx; i<num_fields; i++) {
		char   *data      = NULL;
		size_t  data_len  = 0;

		if (!M_str_caseeq(fields[i].table_column, column_name))
			continue;

		if (prev_fields) {
			if (M_sql_tabledata_edit_fetch(&data, &data_len, fields[i].field_name, fetch_cb, thunk, prev_fields, M_TRUE))
				field_updated = M_TRUE;
		} else {
			if (!fetch_cb(&data, &data_len, fields[i].field_name, thunk)) {
				if (!fields[i].default_val)
					continue;
				data     = M_strdup(fields[i].default_val);
				data_len = M_str_len(data);
			}
		}

		/* Null data is the same as the key not existing */
		if (data != NULL)
			M_hash_dict_insert(dict, fields[i].field_name, data);

		M_free(data);
	}

	/* Only serialize if there are updated fields or during an add */
	if (!prev_fields || field_updated) {
		out = M_hash_dict_serialize(dict, '|', '=', '"', '"', M_HASH_DICT_SER_FLAG_NONE);
	}

	M_hash_dict_destroy(dict); dict = NULL;
	return out;
}


static M_bool M_sql_tabledata_validate_fields(M_sql_tabledata_t *fields, size_t num_fields, char *error, size_t error_len)
{
	size_t         i;
	M_hash_dict_t *seen_cols   = NULL;
	M_hash_dict_t *seen_fields = NULL;
	M_bool         rv          = M_FALSE;
	M_bool         has_id      = M_FALSE;
	M_bool         has_nonid   = M_FALSE;

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

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED && fields[i].type != M_SQL_DATA_TYPE_TEXT) {
			M_snprintf(error, error_len, "Column %s tagged field %s is only allowed to be text", fields[i].table_column, fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both editable and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "field %s cannot be both tagged and an id", fields[i].field_name);
			goto done;
		}

		if (fields[i].flags & (M_SQL_TABLEDATA_FLAG_ID_GENERATE|M_SQL_TABLEDATA_FLAG_ID_REQUIRED) &&
		    !(fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)) {
			M_snprintf(error, error_len, "field %s must be an id to specify id_generate or id_required", fields[i].field_name);
			goto done;
		}

		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			if (!(fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED)) {
				M_snprintf(error, error_len, "non-tagged column %s specified more than once", fields[i].table_column);
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

/* NOTE: Will M_free()' field_data! */
static M_bool M_sql_tabledata_bind(M_sql_stmt_t *stmt, M_sql_data_type_t type, char *field_data, size_t field_data_len, size_t max_column_len)
{
	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			if (field_data == NULL) {
				M_sql_stmt_bind_bool_null(stmt);
			} else {
				M_sql_stmt_bind_bool(stmt, M_str_istrue(field_data));
				M_free(field_data);
			}
			break;

		case M_SQL_DATA_TYPE_INT16:
			if (field_data == NULL) {
				M_sql_stmt_bind_int16_null(stmt);
			} else {
				M_int64 num = 0;
				M_str_to_int64_ex(field_data, field_data_len, 10, &num, NULL);
				M_sql_stmt_bind_int16(stmt, (M_int16)(num & 0xFFFF));
				M_free(field_data);
			}
			break;

		case M_SQL_DATA_TYPE_INT32:
			if (field_data == NULL) {
				M_sql_stmt_bind_int32_null(stmt);
			} else {
				M_int64 num = 0;
				M_str_to_int64_ex(field_data, field_data_len, 10, &num, NULL);
				M_sql_stmt_bind_int32(stmt, (M_int32)(num & 0xFFFFFFFF));
				M_free(field_data);
			}
			break;

		case M_SQL_DATA_TYPE_INT64:
			if (field_data == NULL) {
				M_sql_stmt_bind_int64_null(stmt);
			} else {
				M_int64 num = 0;
				M_str_to_int64_ex(field_data, field_data_len, 10, &num, NULL);
				M_sql_stmt_bind_int64(stmt, num);
				M_free(field_data);
			}
			break;

		case M_SQL_DATA_TYPE_TEXT:
			M_sql_stmt_bind_text_own(stmt, field_data, M_MIN(field_data_len, max_column_len));
			break;

		case M_SQL_DATA_TYPE_BINARY:
			M_sql_stmt_bind_binary_own(stmt, (unsigned char *)field_data, M_MIN(field_data_len, max_column_len));
			break;

		case M_SQL_DATA_TYPE_UNKNOWN:
		default:
			M_free(field_data);
			return M_FALSE;
	}
	return M_TRUE;
}


M_sql_error_t M_sql_tabledata_add(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	M_buf_t       *request     = NULL;
	M_hash_dict_t *seen_cols   = NULL;
	M_sql_stmt_t  *stmt        = NULL;
	size_t         i;
	M_bool         has_col     = M_FALSE;
	M_sql_error_t  err         = M_SQL_ERROR_USER_FAILURE;

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

		/* If no default value is specified, its not an ID and not a tagged field, then we need to test to see if this column should be emitted at all. */
		if (fields[i].default_val == NULL &&
		    !(fields[i].flags & (M_SQL_TABLEDATA_FLAG_ID|M_SQL_TABLEDATA_FLAG_TAGGED)) &&
		    !fetch_cb(NULL, NULL, fields[i].field_name, thunk)) {
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
		char  *field_data = NULL;
		size_t field_data_len = 0;

		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED) {
			field_data     = M_sql_tabledata_row_gather_tagged(fields, num_fields, i, fetch_cb, thunk, NULL);
			field_data_len = M_str_len(field_data);
		} else if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			size_t max_len = fields[i].max_column_len;
			if (max_len == 0) {
				if (fields[i].type == M_SQL_DATA_TYPE_INT32) {
					max_len = 9;
				} else {
					max_len = 18;
				}
			}
			if (max_len > 18)
				max_len = 18;
			field_data_len = M_asprintf(&field_data, "%lld", M_sql_gen_timerand_id(pool, max_len));
		} else {
			if (!fetch_cb(&field_data, &field_data_len, fields[i].field_name, thunk)) {
				if (fields[i].default_val != NULL) {
					field_data = M_strdup(fields[i].default_val);
				} else {
					/* Do not emit field */
					continue;
				}
			}
		}

		/* NOTE: just because field_data is NULL doesn't mean they don't intend us to actually bind NULL */

		if (has_col) {
			M_buf_add_str(request, ", ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "?");

		if (!M_sql_tabledata_bind(stmt, fields[i].type, field_data, field_data_len, fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", fields[i].table_column);
			goto done;
		}

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


/*! Query table for matching record and return fieldname to value mappings in a hash dict */
static M_sql_error_t M_sql_tabledata_query(M_hash_strbin_t **params_out, M_sql_trans_t *sqltrans, const char *table_name, M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	M_buf_t       *request     = NULL;
	M_sql_stmt_t  *stmt        = NULL;
	M_hash_dict_t *seen_cols   = NULL;
	M_bool         has_col     = M_FALSE;
	size_t         i;
	M_sql_error_t  err         = M_SQL_ERROR_USER_FAILURE;

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
		char    *field_data     = NULL;
		size_t   field_data_len = 0;

		if (!(fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		if (!fetch_cb(&field_data, &field_data_len, fields[i].field_name, thunk)) {
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
		if (!M_sql_tabledata_bind(stmt, fields[i].type, field_data, field_data_len, fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", fields[i].table_column);
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
	*params_out = M_hash_strbin_create(16, 75, M_HASH_STRBIN_KEYS_LOWER|M_HASH_STRBIN_CASECMP);

	for (i=0; i<num_fields; i++) {
		/* Skip columns we already have (multiple tagged fields) */
		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		/* Skip any fields that are ids, we don't use that here */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		/* Need to deserialize and add to output params */
		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED) {
			M_hash_dict_t      *dict     = M_hash_dict_deserialize(M_sql_stmt_result_text_byname_direct(stmt, 0, fields[i].table_column), '|', '=', '"', '"', M_HASH_DICT_DESER_FLAG_CASECMP);
			M_hash_dict_enum_t *hashenum = NULL;
			const char         *key      = NULL;
			const char         *val      = NULL;

			M_hash_dict_enumerate(dict, &hashenum);
			while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
				M_hash_strbin_insert(*params_out, key, (const unsigned char *)val, M_str_len(val));
			}
			M_hash_dict_enumerate_free(hashenum);
			M_hash_dict_destroy(dict);
		} else {
			const unsigned char *data     = NULL;
			size_t               data_len = 0;

			if (fields[i].type == M_SQL_DATA_TYPE_BINARY) {
				data = M_sql_stmt_result_binary_byname_direct(stmt, 0, fields[i].table_column, &data_len);
			} else {
				/* All other types, get as text */
				data     = (const unsigned char *)M_sql_stmt_result_text_byname_direct(stmt, 0, fields[i].table_column);
				data_len = M_str_len((const char *)data);
			}
			M_hash_strbin_insert(*params_out, fields[i].field_name, data, data_len);
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
	M_sql_tabledata_t       *fields;
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
	M_hash_strbin_t        *prev_fields = NULL;
	M_sql_error_t           err         = M_SQL_ERROR_USER_FAILURE;
	M_sql_tabledata_edit_t *info        = arg;

	err = M_sql_tabledata_query(&prev_fields, sqltrans, info->table_name, info->fields, info->num_fields, info->fetch_cb, info->thunk, error, error_len);
	if (M_sql_error_is_error(err))
		goto done;

	err     = M_SQL_ERROR_USER_FAILURE;

	request = M_buf_create();
	stmt    = M_sql_stmt_create();
	M_buf_add_str(request, "UPDATE \"");
	M_buf_add_str(request, info->table_name);
	M_buf_add_str(request, "\" SET ");

	for (i=0; i<info->num_fields; i++) {
		char  *field_data     = NULL;
		size_t field_data_len = 0;
		M_bool is_changed;

		/* Skip ID columns */
		if (info->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID)
			continue;

		/* Skip columns we already have (multiple tagged fields) */
		if (M_hash_dict_get(seen_cols, info->fields[i].table_column, NULL)) {
			continue;
		}

		M_hash_dict_insert(seen_cols, info->fields[i].table_column, NULL);

		if (info->fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED) {
			/* Only on tagged data does NULL mean not changed.  Otherwise we are explicitly setting a field to NULL */
			field_data = M_sql_tabledata_row_gather_tagged(info->fields, info->num_fields, i, info->fetch_cb, info->thunk, prev_fields);
			if (field_data == NULL) {
				is_changed = M_FALSE;
			} else {
				is_changed = M_TRUE;
			}
		} else {
			is_changed  = M_sql_tabledata_edit_fetch(&field_data, &field_data_len, info->fields[i].field_name, info->fetch_cb, info->thunk, prev_fields, M_FALSE);
			/* TODO: see if field is allowed to be edited */
		}

		if (is_changed) {
			if (has_col)
				M_buf_add_str(request, ", ");
			has_col = M_TRUE;

			M_buf_add_str(request, "\"");
			M_buf_add_str(request, info->fields[i].table_column);
			M_buf_add_str(request, "\" = ?");

			if (!M_sql_tabledata_bind(stmt, info->fields[i].type, field_data, field_data_len, info->fields[i].max_column_len)) {
				M_snprintf(error, error_len, "column %s unsupported field type", info->fields[i].table_column);
				goto done;
			}

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
		char  *field_data     = NULL;
		size_t field_data_len = 0;

		/* Skip non-ID columns */
		if (!(info->fields[i].flags & M_SQL_TABLEDATA_FLAG_ID))
			continue;

		if (!info->fetch_cb(&field_data, &field_data_len, info->fields[i].field_name, info->thunk)) {
			continue;
		}

		if (has_col) {
			M_buf_add_str(request, " AND ");
		}
		has_col = M_TRUE;
		M_buf_add_str(request, "\"");
		M_buf_add_str(request, info->fields[i].table_column);
		M_buf_add_str(request, "\" = ?");
		if (!M_sql_tabledata_bind(stmt, info->fields[i].type, field_data, field_data_len, info->fields[i].max_column_len)) {
			M_snprintf(error, error_len, "column %s unsupported field type", info->fields[i].table_column);
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
		M_hash_strbin_destroy(prev_fields);
	if (stmt != NULL) {
		if (M_sql_error_is_error(err) && err != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}
	return err;
}



M_sql_error_t M_sql_tabledata_edit(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
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
