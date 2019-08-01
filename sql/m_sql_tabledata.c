/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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


static char *M_sql_tabledata_row_gather_tagged(M_sql_tabledata_t *fields, size_t num_fields, size_t curr_idx, M_sql_tabledata_fetch_cb fetch_cb, void *thunk)
{
	const char    *column_name = fields[curr_idx].table_column;
	size_t         i;
	M_hash_dict_t *dict    = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_LOWER);
	char          *out     = NULL;
	for (i=curr_idx; i<num_fields; i++) {
		char   *data      = NULL;
		size_t  data_len  = 0;

		if (!M_str_caseeq(fields[i].table_column, column_name))
			continue;

		if (!fetch_cb(&data, &data_len, fields[i].field_name, thunk)) {
			if (!fields[i].default_val)
				continue;
			data     = M_strdup(fields[i].default_val);
			data_len = M_str_len(data);
		}

		M_hash_dict_insert(dict, fields[i].field_name, data);
		M_free(data);
	}

	out = M_hash_dict_serialize(dict, '|', '=', '"', '"', M_HASH_DICT_SER_FLAG_NONE);
	M_hash_dict_destroy(dict); dict = NULL;
	return out;
}


M_sql_error_t M_sql_tabledata_add(M_sql_connpool_t *pool, M_sql_trans_t *sqltrans, const char *table_name, M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, void *thunk, char *error, size_t error_len)
{
	M_buf_t       *request     = NULL;
	M_hash_dict_t *seen_cols   = NULL;
	M_hash_dict_t *seen_fields = NULL;
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

	request     = M_buf_create();
	seen_cols   = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	seen_fields = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
	stmt        = M_sql_stmt_create();

	M_buf_add_str(request, "INSERT INTO \"");
	M_buf_add_str(request, table_name);
	M_buf_add_str(request, "\" (");

	/* Specify each column name we will be outputting (in case the table as more columns than this) */
	for (i=0; i<num_fields; i++) {
		if (M_str_isempty(fields[i].table_column)) {
			M_snprintf(error, error_len, "field %zu did not specify a column name", i);
			goto done;
		}

		if (fields[i].field_name != NULL) {
			if (M_hash_dict_get(seen_fields, fields[i].field_name, NULL)) {
				M_snprintf(error, error_len, "Duplicate field name %s specified", fields[i].field_name);
				goto done;
			}
			M_hash_dict_insert(seen_fields, fields[i].field_name, NULL);
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED && fields[i].type != M_SQL_DATA_TYPE_TEXT) {
			M_snprintf(error, error_len, "Column %s tagged field %s is only allowed to be text", fields[i].table_column, fields[i].field_name);
			goto done;
		}

		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			if (!(fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED)) {
				M_snprintf(error, error_len, "non-tagged column %s specified more than once", fields[i].table_column);
				goto done;
			}
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_EDITABLE && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "column %s cannot be both editable and an id", fields[i].table_column);
			goto done;
		}

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED && fields[i].flags & M_SQL_TABLEDATA_FLAG_ID) {
			M_snprintf(error, error_len, "column %s cannot be both tagged and an id", fields[i].table_column);
			goto done;
		}

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
	for (i=0; i<num_fields; i++) {
		char  *field_data = NULL;
		size_t field_data_len = 0;

		if (M_hash_dict_get(seen_cols, fields[i].table_column, NULL)) {
			continue;
		}
		M_hash_dict_insert(seen_cols, fields[i].table_column, NULL);

		if (fields[i].flags & M_SQL_TABLEDATA_FLAG_TAGGED) {
			field_data     = M_sql_tabledata_row_gather_tagged(fields, num_fields, i, fetch_cb, thunk);
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

		switch (fields[i].type) {
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
				M_sql_stmt_bind_text_own(stmt, field_data, M_MIN(field_data_len, fields[i].max_column_len));
				break;

			case M_SQL_DATA_TYPE_BINARY:
				M_sql_stmt_bind_binary_own(stmt, (unsigned char *)field_data, M_MIN(field_data_len, fields[i].max_column_len));
				break;

			case M_SQL_DATA_TYPE_UNKNOWN:
			default:
				M_snprintf(error, error_len, "column %s unsupported field type", fields[i].table_column);
				goto done;
		}

	}

	M_buf_add_str(request, ")");
	err = M_sql_stmt_prepare_buf(stmt, request);
	request = NULL;
	if (err != M_SQL_ERROR_SUCCESS)
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
	if (seen_fields)
		M_hash_dict_destroy(seen_fields);
	if (stmt != NULL) {
		if (err != M_SQL_ERROR_SUCCESS && err != M_SQL_ERROR_USER_FAILURE) {
			M_snprintf(error, error_len, "%s", M_sql_stmt_get_error_string(stmt));
		}
		M_sql_stmt_destroy(stmt);
	}
	return err;
}


