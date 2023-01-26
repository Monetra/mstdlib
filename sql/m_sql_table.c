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

typedef struct {
	char                   *name;
	M_sql_table_col_flags_t flags;
	M_sql_data_type_t       datatype;
	size_t                  max_len;
	char                   *default_value;
} M_sql_table_col_t;

struct M_sql_index {
	char                     *name;
	M_sql_table_index_flags_t flags;
	M_list_str_t             *cols;   /*!< List of columns that make up the index */
	M_sql_table_t            *table;
};

struct M_sql_table {
	char         *name;    /*!< Table Name */
	M_list_t     *cols;    /*!< List of M_sql_table_col_t * */
	M_list_str_t *pk_cols; /*!< Columns that make up the primary key */
	M_list_t     *idx;     /*!< List of M_sql_index_t * */
};


M_bool M_sql_table_exists(M_sql_connpool_t *pool, const char *name)
{
	M_sql_stmt_t *stmt;
	char          query[256];
	M_sql_error_t err;

	if (M_str_isempty(name))
		return M_FALSE;

	stmt = M_sql_stmt_create();
	M_sql_trace_ignore_tranfail(stmt);
	M_snprintf(query, sizeof(query), "SELECT 1 FROM \"%s\" WHERE 1 = ?", name);
	M_sql_stmt_bind_int32(stmt, 0);

	err = M_sql_stmt_prepare(stmt, query);
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

	err = M_sql_stmt_execute(pool, stmt);

done:
	M_sql_stmt_destroy(stmt);

	if (err != M_SQL_ERROR_SUCCESS)
		return M_FALSE;
	return M_TRUE;
}

#define M_SQL_MAX_IDENTIFIER_LEN 63

static M_bool M_sql_identifier_validate(const char *name)
{
	size_t len;
	size_t i;

	if (M_str_isempty(name))
		return M_FALSE;

	/* Must begin with alpha or underscore */
	if (!M_chr_isalpha(name[0]) && name[0] != '_')
		return M_FALSE;

	len = M_str_len(name);

	/* Max is 63 bytes */
	if (len > M_SQL_MAX_IDENTIFIER_LEN)
		return M_FALSE;

	for (i=0; i<len; i++) {
		if (!M_chr_isalnum(name[i]) && name[i] != '_')
			return M_FALSE;
	}

	return M_TRUE;
}


static void M_sql_table_col_destroy(void *arg)
{
	M_sql_table_col_t *col = arg;
	M_free(col->name);
	M_free(col->default_value);
	M_free(col);
}


static void M_sql_index_destroy(void *arg)
{
	M_sql_index_t *idx = arg;
	if (idx == NULL)
		return;
	M_free(idx->name);
	M_list_str_destroy(idx->cols);
	M_free(idx);
}


M_sql_table_t *M_sql_table_create(const char *name)
{
	M_sql_table_t *tbl;
	struct M_list_callbacks col_cb = {
		NULL,
		NULL,
		NULL,
		M_sql_table_col_destroy
	};
	struct M_list_callbacks idx_cb = {
		NULL,
		NULL,
		NULL,
		M_sql_index_destroy
	};

	if (!M_sql_identifier_validate(name))
		return NULL;

	tbl          = M_malloc_zero(sizeof(*tbl));
	tbl->name    = M_strdup(name);
	tbl->cols    = M_list_create(&col_cb, M_LIST_NONE);
	tbl->pk_cols = M_list_str_create(M_LIST_STR_CASECMP|M_LIST_STR_SET);
	tbl->idx     = M_list_create(&idx_cb, M_LIST_NONE);

	return tbl;
}


void M_sql_table_destroy(M_sql_table_t *table)
{
	if (table == NULL)
		return;

	M_free(table->name);
	M_list_destroy(table->cols, M_TRUE);
	M_list_str_destroy(table->pk_cols);
	M_list_destroy(table->idx, M_TRUE);
	M_free(table);
}


static M_bool M_sql_table_col_exists(M_sql_table_t *table, const char *col_name)
{
	size_t                   i;
	size_t                   len = M_list_len(table->cols);

	for (i=0; i < len; i++) {
		const M_sql_table_col_t *col = M_list_at(table->cols, i);
		/* NOTE: Some databases like PostgreSQL are case sensitive */
		if (M_str_eq(col_name, col->name)) {
			break;
		}
	}

	if (i == len)
		return M_FALSE;

	return M_TRUE;
}


M_bool M_sql_table_add_col(M_sql_table_t *table, M_uint32 flags, const char *col_name, M_sql_data_type_t datatype, size_t max_len, const char *default_value)
{
	M_sql_table_col_t *col = NULL;

	if (table == NULL || !M_sql_identifier_validate(col_name) || M_sql_table_col_exists(table, col_name))
		return M_FALSE;

	col                = M_malloc_zero(sizeof(*col));
	col->name          = M_strdup(col_name);
	col->flags         = flags;
	col->datatype      = datatype;
	col->max_len       = max_len;
	col->default_value = M_strdup(default_value);
	M_list_insert(table->cols, col);
	return M_TRUE;
}



M_bool M_sql_table_add_pk_col(M_sql_table_t *table, const char *col_name)
{
	if (table == NULL || !M_sql_table_col_exists(table, col_name))
		return M_FALSE;

	return M_list_str_insert(table->pk_cols, col_name);
}


static M_sql_index_t *M_sql_table_add_index_int(M_sql_table_t *table, M_uint32 flags, const char *idx_name, M_bool insert_to_table)
{
	M_sql_index_t *idx;
	char           fullname[256];
	size_t         i;

	if (table == NULL || M_str_isempty(idx_name))
		return NULL;

	M_snprintf(fullname, sizeof(fullname), "i_%s_%s", table->name, idx_name);
	if (!M_sql_identifier_validate(fullname))
		return NULL;

	/* Make sure index name doesn't already exist */
	for (i=0; i<M_list_len(table->idx); i++) {
		const M_sql_index_t *myidx = M_list_at(table->idx, i);
		if (M_str_caseeq(myidx->name, fullname))
			break;
	}
	if (i != M_list_len(table->idx))
		return NULL;

	idx        = M_malloc_zero(sizeof(*idx));
	idx->name  = M_strdup(fullname);
	idx->flags = flags;
	idx->cols  = M_list_str_create(M_LIST_STR_CASECMP|M_LIST_STR_SET);
	idx->table = table;

	if (insert_to_table)
		M_list_insert(table->idx, idx);

	return idx;
}


M_sql_index_t *M_sql_table_add_index(M_sql_table_t *table, M_uint32 flags, const char *idx_name)
{
	return M_sql_table_add_index_int(table, flags, idx_name, M_TRUE);
}


M_bool M_sql_index_add_col(M_sql_index_t *idx, const char *col_name)
{
	if (idx == NULL || !M_sql_identifier_validate(col_name) || !M_sql_table_col_exists(idx->table, col_name))
		return M_FALSE;

	return M_list_str_insert(idx->cols, col_name);
}


M_bool M_sql_table_add_index_str(M_sql_table_t *table, M_uint32 flags, const char *idx_name, const char *idx_cols_csv)
{
	char         **cols = NULL;
	size_t         cnt  = 0;
	M_bool         rv   = M_FALSE;
	M_sql_index_t *idx  = NULL;
	size_t         i;

	if (table == NULL || M_str_isempty(idx_name) || M_str_isempty(idx_cols_csv))
		goto done;

	cols = M_str_explode_str(',', idx_cols_csv, &cnt);
	if (cols == NULL || cnt == 0)
		goto done;


	idx = M_sql_table_add_index_int(table, flags, idx_name, M_FALSE);
	if (idx == NULL)
		goto done;

	for (i=0; i<cnt; i++) {
		M_str_trim(cols[i]);
		if (M_str_isempty(cols[i]))
			continue;

		if (!M_sql_index_add_col(idx, cols[i]))
			goto done;
	}

	M_list_insert(table->idx, idx);
	rv = M_TRUE;

done:
	if (!rv)
		M_sql_index_destroy(idx);

	M_str_explode_free(cols, cnt);

	return rv;
}


M_sql_error_t M_sql_table_execute(M_sql_connpool_t *pool, M_sql_table_t *table, char *error, size_t error_size)
{
	size_t                i;
	M_buf_t              *query = NULL;
	M_sql_stmt_t         *stmt  = NULL;
	M_sql_error_t         err;
	char                  state[256];
	const M_sql_driver_t *driver = M_sql_connpool_get_driver(pool);

	M_mem_set(error, 0, error_size);

	if (pool == NULL || table == NULL) {
		M_snprintf(error, error_size, "NULL function parameters provided");
		return M_SQL_ERROR_INVALID_USE;
	}

	/* Validate table has columns */
	if (M_list_len(table->cols) == 0) {
		M_snprintf(error, error_size, "No columns defined");
		return M_SQL_ERROR_INVALID_USE;
	}

	/* Validate table has a primary key */
	if (M_list_str_len(table->pk_cols) == 0) {
		M_snprintf(error, error_size, "No primary key defined, required");
		return M_SQL_ERROR_INVALID_USE;
	}

	/* Validate each index has columns defined */
	for (i=0; i<M_list_len(table->idx); i++) {
		const M_sql_index_t *idx = M_list_at(table->idx, i);
		if (M_list_str_len(idx->cols) == 0) {
			M_snprintf(error, error_size, "Index %s missing columns", idx->name);
			return M_SQL_ERROR_INVALID_USE;
		}
	}

	/* Validate table doesn't already exist */
	if (M_sql_table_exists(pool, table->name)) {
		M_snprintf(error, error_size, "Table %s already exists", table->name);
		return M_SQL_ERROR_INVALID_USE;
	}

	/* Create table - name */
	M_snprintf(state, sizeof(state), "create table");

	query = M_buf_create();
	M_buf_add_str(query, "CREATE TABLE \"");
	M_buf_add_str(query, table->name);
	M_buf_add_str(query, "\" (");

	/* Create table - columns */
	for (i=0; i<M_list_len(table->cols); i++) {
		const M_sql_table_col_t *col = M_list_at(table->cols, i);

		M_buf_add_str(query, "\"");
		M_buf_add_str(query, col->name);
		M_buf_add_str(query, "\" ");

		if (!driver->cb_datatype(pool, query, col->datatype, col->max_len, M_FALSE)) {
			M_snprintf(error, error_size, "column %s unable to convert datatype", col->name);
			err = M_SQL_ERROR_INVALID_USE;
			goto done;
		}

		if (col->flags & M_SQL_TABLE_COL_FLAG_NOTNULL)
			M_buf_add_str(query, " NOT NULL");

		if (col->default_value) {
			M_buf_add_str(query, " DEFAULT ");
			M_buf_add_str(query, col->default_value);
		}

		M_buf_add_str(query, ", ");
	}

	/* Create table - primary key */
	M_buf_add_str(query, "PRIMARY KEY(");
	for (i=0; i<M_list_str_len(table->pk_cols); i++) {
		if (i != 0)
			M_buf_add_str(query, ", ");
		M_buf_add_str(query, "\"");
		M_buf_add_str(query, M_list_str_at(table->pk_cols, i));
		M_buf_add_str(query, "\"");
	}
	M_buf_add_str(query, ") )");

	/* Some servers like MySQL append things like " ENGINE=InnoDB CHARSET=utf8" */
	if (driver->cb_createtable_suffix) {
		driver->cb_createtable_suffix(pool, query);
	}

	stmt  = M_sql_stmt_create();
	err   = M_sql_stmt_prepare_buf(stmt, query);
	query = NULL;
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

	err = M_sql_stmt_execute(pool, stmt);
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

	M_sql_stmt_destroy(stmt);
	stmt = NULL;


	/* Create Indexes */
	for (i=0; i<M_list_len(table->idx); i++) {
		const M_sql_index_t *idx     = M_list_at(table->idx, i);
		size_t               j;
		char                *idxname = NULL;

		M_snprintf(state, sizeof(state), "create index %s", idx->name);

		query = M_buf_create();
		M_buf_add_str(query, "CREATE ");
		if (idx->flags & M_SQL_INDEX_FLAG_UNIQUE)
			M_buf_add_str(query, "UNIQUE ");
		M_buf_add_str(query, "INDEX \"");

		/* Index name may need to be rewritten based on the database backend */
		if (driver->cb_rewrite_indexname != NULL)
			idxname = driver->cb_rewrite_indexname(pool, idx->name);
		if (idxname == NULL)
			idxname = M_strdup(idx->name);
		M_buf_add_str(query, idxname);
		M_free(idxname);

		M_buf_add_str(query, "\" ON \"");
		M_buf_add_str(query, table->name);
		M_buf_add_str(query, "\" (");
		for (j=0; j<M_list_str_len(idx->cols); j++) {
			if (j != 0)
				M_buf_add_str(query, ", ");
			M_buf_add_str(query, "\"");
			M_buf_add_str(query, M_list_str_at(idx->cols, j));
			M_buf_add_str(query, "\"");
		}
		M_buf_add_str(query, ")");

		/* Some SQL servers treat multiple NULL values as conflict, which is against the SQL
		 * standard on Unique indexes, but this can be solved by using a WHERE clause on the
		 * index on some servers. */
		if (idx->flags & M_SQL_INDEX_FLAG_UNIQUE &&
		    M_sql_connpool_get_driver_flags(pool) & M_SQL_DRIVER_FLAG_UNIQUEINDEX_NOTNULL_WHERE) {
			M_buf_add_str(query, " WHERE");
			for (j=0; j<M_list_str_len(idx->cols); j++) {
				if (j != 0)
					M_buf_add_str(query, " AND");
				M_buf_add_str(query, " \"");
				M_buf_add_str(query, M_list_str_at(idx->cols, j));
				M_buf_add_str(query, "\" IS NOT NULL");
			}
		}

		stmt  = M_sql_stmt_create();
		err   = M_sql_stmt_prepare_buf(stmt, query);
		query = NULL;
		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		err = M_sql_stmt_execute(pool, stmt);
		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		M_sql_stmt_destroy(stmt);
		stmt = NULL;
	}


done:
	if (err != M_SQL_ERROR_SUCCESS && M_str_isempty(error)) {
		M_snprintf(error, error_size, "%s: %s: %s", state, M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	}
	M_sql_stmt_destroy(stmt);
	M_buf_cancel(query);

	return err;
}

