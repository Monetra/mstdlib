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
#include <mstdlib/mstdlib_sql.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"

M_bool M_sql_stmt_result_clear_data(M_sql_stmt_t *stmt)
{
	size_t i;

	if (stmt == NULL)
		return M_FALSE;

	stmt->affected_rows = 0;
	if (!stmt->result)
		return M_TRUE;

	/* Clear result data itself, but we're not actually going to deallocate
	 * anything since we should be able to re-use the pre-allocated data for
	 * any new rows being added */
	stmt->result->curr_col = 0;

	if (stmt->result->rows) {
		for (i=0; i<stmt->result->num_rows; i++)
			M_buf_truncate(stmt->result->rows[i], 0);
	}

	stmt->result->num_rows = 0;

	if (stmt->result->cellinfo) {
		M_mem_set(stmt->result->cellinfo, 0, sizeof(*stmt->result->cellinfo) * stmt->result->alloc_rows * stmt->result->num_cols);
	}

	return M_TRUE;
}

M_bool M_sql_stmt_result_clear(M_sql_stmt_t *stmt)
{
	size_t i;

	if (stmt == NULL)
		return M_FALSE;

	stmt->affected_rows = 0;
	if (!stmt->result)
		return M_TRUE;

	/* Free Column Definitions */
	M_free(stmt->result->col_defs);
	M_hash_stridx_destroy(stmt->result->col_name);

	/* Free Row MetaData */
	M_free(stmt->result->cellinfo);

	/* Free Row Data */
	if (stmt->result->rows) {
		for (i=0; i<stmt->result->alloc_rows; i++) {
			M_buf_cancel(stmt->result->rows[i]);
		}
	}
	M_free(stmt->result->rows);

	/* Free full result */
	M_free(stmt->result);
	stmt->result = NULL;

	return M_TRUE;
}

size_t M_sql_stmt_result_affected_rows(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return 0;
	return stmt->affected_rows;
}

size_t M_sql_stmt_result_num_rows(M_sql_stmt_t *stmt)
{
	if (stmt == NULL || stmt->result == NULL)
		return 0;
	return stmt->result->num_rows;
}


size_t M_sql_stmt_result_total_rows(M_sql_stmt_t *stmt)
{
	if (stmt == NULL || stmt->result == NULL)
		return 0;
	return stmt->result->total_rows;
}


size_t M_sql_stmt_result_num_cols(M_sql_stmt_t *stmt)
{
	if (stmt == NULL || stmt->result == NULL)
		return 0;
	return stmt->result->num_cols;
}

const char *M_sql_stmt_result_col_name(M_sql_stmt_t *stmt, size_t col)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols)
		return NULL;

	return stmt->result->col_defs[col].name;
}

M_sql_data_type_t M_sql_stmt_result_col_type(M_sql_stmt_t *stmt, size_t col, size_t *type_size)
{
	if (type_size != NULL)
		*type_size = 0;

	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols)
		return M_SQL_DATA_TYPE_UNKNOWN;

	if (type_size != NULL)
		*type_size = stmt->result->col_defs[col].max_size;

	return stmt->result->col_defs[col].type;
}

M_bool M_sql_stmt_result_col_idx(M_sql_stmt_t *stmt, const char *col, size_t *idx)
{
	size_t myidx;

	if (stmt == NULL || stmt->result == NULL)
		return M_FALSE;

	/* idx is not a required field */
	if (idx == NULL)
		idx = &myidx;

	*idx = 0;
	return M_hash_stridx_get(stmt->result->col_name, col, idx);
}


M_sql_error_t M_sql_stmt_result_isnull(M_sql_stmt_t *stmt, size_t row, size_t col, M_bool *is_null)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols || row >= stmt->result->num_rows || is_null == NULL)
		return M_SQL_ERROR_INVALID_USE;
	*is_null = (stmt->result->cellinfo[row * stmt->result->num_cols + col].length == 0)?M_TRUE:M_FALSE;
	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_text(M_sql_stmt_t *stmt, size_t row, size_t col, const char **text)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols || row >= stmt->result->num_rows || text == NULL)
		return M_SQL_ERROR_INVALID_USE;

	*text = NULL;

	if (stmt->result->col_defs[col].type == M_SQL_DATA_TYPE_UNKNOWN || stmt->result->col_defs[col].type == M_SQL_DATA_TYPE_BINARY)
		return M_SQL_ERROR_INVALID_TYPE;

	if (stmt->result->cellinfo[row * stmt->result->num_cols + col].length != 0) {
		*text = M_buf_peek(stmt->result->rows[row]);
		/* Should never be NULL, but not bad to check anyhow I guess */
		if (*text != NULL) {
			(*text) += stmt->result->cellinfo[row * stmt->result->num_cols + col].offset;
		}
	}

	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_bool(M_sql_stmt_t *stmt, size_t row, size_t col, M_bool *val)
{
	const char   *text = NULL;
	M_sql_error_t err;

	if (val == NULL)
		return M_SQL_ERROR_INVALID_USE;

	*val = M_FALSE;

	err  = M_sql_stmt_result_text(stmt, row, col, &text);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	if (!M_str_caseeq(text, "1") && !M_str_caseeq(text, "0") &&
	    !M_str_caseeq(text, "y") && !M_str_caseeq(text, "n") &&
	    !M_str_caseeq(text, "yes") && !M_str_caseeq(text, "no") &&
	    !M_str_caseeq(text, "true") && !M_str_caseeq(text, "true") &&
	    !M_str_caseeq(text, "on") && !M_str_caseeq(text, "off")) {
		return M_SQL_ERROR_INVALID_TYPE;
	}

	*val = M_str_istrue(text);

	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_int16(M_sql_stmt_t *stmt, size_t row, size_t col, M_int16 *val)
{
	M_int32       i32;
	M_sql_error_t err;

	if (val == NULL)
		return M_SQL_ERROR_INVALID_USE;
	*val = 0;

	err = M_sql_stmt_result_int32(stmt, row, col, &i32);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	if (i32 > M_INT16_MAX)
		return M_SQL_ERROR_INVALID_TYPE;

	*val = (M_int16)i32;
	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_int32(M_sql_stmt_t *stmt, size_t row, size_t col, M_int32 *val)
{
	const char   *text = NULL;
	M_sql_error_t err;

	if (val == NULL)
		return M_SQL_ERROR_INVALID_USE;

	*val = 0;

	err  = M_sql_stmt_result_text(stmt, row, col, &text);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	if (text == NULL)
		return M_SQL_ERROR_SUCCESS;

	if (M_str_to_int32_ex(text, M_str_len(text), 10, val, NULL) != M_STR_INT_SUCCESS) {
		return M_SQL_ERROR_INVALID_TYPE;
	}

	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_int64(M_sql_stmt_t *stmt, size_t row, size_t col, M_int64 *val)
{
	const char   *text = NULL;
	M_sql_error_t err;

	if (val == NULL)
		return M_SQL_ERROR_INVALID_USE;

	*val = 0;

	err  = M_sql_stmt_result_text(stmt, row, col, &text);
	if (err != M_SQL_ERROR_SUCCESS)
		return err;

	if (text == NULL)
		return M_SQL_ERROR_SUCCESS;

	if (M_str_to_int64_ex(text, M_str_len(text), 10, val, NULL) != M_STR_INT_SUCCESS) {
		return M_SQL_ERROR_INVALID_TYPE;
	}

	return M_SQL_ERROR_SUCCESS;
}


M_sql_error_t M_sql_stmt_result_binary(M_sql_stmt_t *stmt, size_t row, size_t col, const M_uint8 **bin, size_t *bin_size)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols || row >= stmt->result->num_rows || bin == NULL || bin_size == NULL)
		return M_SQL_ERROR_INVALID_USE;

	*bin      = NULL;
	*bin_size = 0;

	if (stmt->result->col_defs[col].type != M_SQL_DATA_TYPE_BINARY && stmt->result->col_defs[col].type != M_SQL_DATA_TYPE_NULL)
		return M_SQL_ERROR_INVALID_TYPE;

	if (stmt->result->cellinfo[row * stmt->result->num_cols + col].length != 0) {
		*bin = (const M_uint8 *)M_buf_peek(stmt->result->rows[row]);
		/* Should never be NULL, but not bad to check anyhow I guess */
		if (*bin != NULL) {
			(*bin)   += stmt->result->cellinfo[row * stmt->result->num_cols + col].offset;
			*bin_size = stmt->result->cellinfo[row * stmt->result->num_cols + col].length - 1; /* Remove NULL term! */
		}
	}

	return M_SQL_ERROR_SUCCESS;
}


M_bool M_sql_stmt_result_isnull_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_bool rv;
	if (M_sql_stmt_result_isnull(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return M_TRUE;
	return rv;
}


const char *M_sql_stmt_result_text_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	const char *rv;
	if (M_sql_stmt_result_text(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return NULL;
	return rv;
}


M_bool M_sql_stmt_result_bool_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_bool rv;
	if (M_sql_stmt_result_bool(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return M_FALSE;
	return rv;
}

M_int16 M_sql_stmt_result_int16_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_int16 rv;
	if (M_sql_stmt_result_int16(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


M_int32 M_sql_stmt_result_int32_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_int32 rv;
	if (M_sql_stmt_result_int32(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


M_int64 M_sql_stmt_result_int64_direct(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_int64 rv;
	if (M_sql_stmt_result_int64(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


const M_uint8 *M_sql_stmt_result_binary_direct(M_sql_stmt_t *stmt, size_t row, size_t col, size_t *bin_size)
{
	const M_uint8 *rv;
	if (M_sql_stmt_result_binary(stmt, row, col, &rv, bin_size) != M_SQL_ERROR_SUCCESS)
		return NULL;
	return rv;
}


M_sql_error_t M_sql_stmt_result_isnull_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_bool *is_null)
{
	size_t idx;

	if (is_null)
		*is_null = M_TRUE;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_isnull(stmt, row, idx, is_null);
}

M_sql_error_t M_sql_stmt_result_text_byname(M_sql_stmt_t *stmt, size_t row, const char *col, const char **text)
{
	size_t idx;

	if (text)
		*text = NULL;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_text(stmt, row, idx, text);
}

M_sql_error_t M_sql_stmt_result_bool_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_bool *val)
{
	size_t idx;

	if (val)
		*val = M_FALSE;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_bool(stmt, row, idx, val);
}

M_sql_error_t M_sql_stmt_result_int16_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int16 *val)
{
	size_t idx;

	if (val)
		*val = 0;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_int16(stmt, row, idx, val);
}

M_sql_error_t M_sql_stmt_result_int32_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int32 *val)
{
	size_t idx;

	if (val)
		*val = 0;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_int32(stmt, row, idx, val);
}

M_sql_error_t M_sql_stmt_result_int64_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int64 *val)
{
	size_t idx;

	if (val)
		*val = 0;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_int64(stmt, row, idx, val);
}

M_sql_error_t M_sql_stmt_result_binary_byname(M_sql_stmt_t *stmt, size_t row, const char *col, const M_uint8 **bin, size_t *bin_size)
{
	size_t idx;

	if (bin)
		*bin = NULL;

	if (bin_size)
		*bin_size = 0;

	if (!M_sql_stmt_result_col_idx(stmt, col, &idx))
		return M_SQL_ERROR_INVALID_USE;

	return M_sql_stmt_result_binary(stmt, row, idx, bin, bin_size);
}


M_bool M_sql_stmt_result_isnull_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	M_bool rv;
	if (M_sql_stmt_result_isnull_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return M_TRUE;
	return rv;
}


const char *M_sql_stmt_result_text_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	const char *rv;
	if (M_sql_stmt_result_text_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return NULL;
	return rv;
}


M_bool M_sql_stmt_result_bool_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	M_bool rv;
	if (M_sql_stmt_result_bool_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return M_FALSE;
	return rv;
}

M_int16 M_sql_stmt_result_int16_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	M_int16 rv;
	if (M_sql_stmt_result_int16_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


M_int32 M_sql_stmt_result_int32_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	M_int32 rv;
	if (M_sql_stmt_result_int32_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


M_int64 M_sql_stmt_result_int64_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col)
{
	M_int64 rv;
	if (M_sql_stmt_result_int64_byname(stmt, row, col, &rv) != M_SQL_ERROR_SUCCESS)
		return 0;
	return rv;
}


const M_uint8 *M_sql_stmt_result_binary_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col, size_t *bin_size)
{
	const M_uint8 *rv;
	if (M_sql_stmt_result_binary_byname(stmt, row, col, &rv, bin_size) != M_SQL_ERROR_SUCCESS)
		return NULL;
	return rv;
}


M_bool M_sql_driver_stmt_result_set_affected_rows(M_sql_stmt_t *stmt, size_t cnt)
{
	if (stmt == NULL)
		return M_FALSE;

	/* Use += since a statement may be executed in a loop if there are multiple bound rows */
	stmt->affected_rows += cnt;
	return M_TRUE;
}


M_bool M_sql_driver_stmt_result_set_num_cols(M_sql_stmt_t *stmt, size_t cnt)
{
	if (stmt == NULL || cnt == 0)
		return M_FALSE;

	/* Allocate result handle if not already alloc'd */
	if (stmt->result == NULL) {
		stmt->result = M_malloc_zero(sizeof(*(stmt->result)));
	}

	/* Already been called, error */
	if (stmt->result->num_cols != 0)
		return M_FALSE;

	stmt->result->num_cols = cnt;
	stmt->result->col_defs = M_malloc_zero(sizeof(*(stmt->result->col_defs)) * cnt);
	stmt->result->col_name = M_hash_stridx_create(16, 75, M_HASH_STRIDX_CASECMP);
	return M_TRUE;
}


M_bool M_sql_driver_stmt_result_set_col_name(M_sql_stmt_t *stmt, size_t col, const char *name)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols) {
		return M_FALSE;
	}

	/* Remove existing hashtable entry */
	if (!M_str_isempty(stmt->result->col_defs[col].name)) {
		M_hash_stridx_remove(stmt->result->col_name, name);
	}

	M_str_cpy(stmt->result->col_defs[col].name, sizeof(stmt->result->col_defs[col].name), name);
	M_hash_stridx_insert(stmt->result->col_name, name, col);
	return M_TRUE;
}


M_bool M_sql_driver_stmt_result_set_col_type(M_sql_stmt_t *stmt, size_t col, M_sql_data_type_t type, size_t max_size)
{
	if (stmt == NULL || stmt->result == NULL || col >= stmt->result->num_cols) {
		return M_FALSE;
	}
	stmt->result->col_defs[col].type     = type;
	stmt->result->col_defs[col].max_size = max_size;

	return M_TRUE;
}


static void M_sql_driver_stmt_result_col_end(M_sql_stmt_t *stmt)
{
	size_t col;
	size_t row;
	size_t cell;

	if (stmt == NULL || stmt->result == NULL)
		return;

	/* Not eligible, no column started */
	if (stmt->result->curr_col == 0 || stmt->result->num_rows == 0 || stmt->result->curr_col-1 >= stmt->result->num_cols)
		return;

	col                                 = stmt->result->curr_col-1;
	row                                 = stmt->result->num_rows-1;
	cell                                = row * stmt->result->num_cols + col;
	stmt->result->cellinfo[cell].length = M_buf_len(stmt->result->rows[row]) - stmt->result->cellinfo[cell].offset;

	stmt->result->curr_col++;

}


M_buf_t *M_sql_driver_stmt_result_col_start(M_sql_stmt_t *stmt)
{
	size_t row;
	size_t col;
	size_t cell;
	size_t len;

	/* Not initialized */
	if (stmt == NULL || stmt->result == NULL || stmt->result->num_cols == 0)
		return NULL;

	/* Looks like we're starting a new row, validate all allocations */
	if (stmt->result->curr_col == 0) {
		stmt->result->num_rows++;
		stmt->result->total_rows++;
		stmt->result->curr_col = 1;

		/* Allocate space for rows using powers of 2 */
		if (stmt->result->num_rows > stmt->result->alloc_rows) {
			stmt->result->alloc_rows = M_size_t_round_up_to_power_of_two(stmt->result->num_rows);
			stmt->result->cellinfo   = M_realloc_zero(stmt->result->cellinfo, (stmt->result->alloc_rows * stmt->result->num_cols) * sizeof(*stmt->result->cellinfo));
			stmt->result->rows       = M_realloc_zero(stmt->result->rows, stmt->result->alloc_rows * sizeof(*stmt->result->rows));
		}

		row = stmt->result->num_rows-1;
		/* Allocate buffer if not yet allocated */
		if (stmt->result->rows[row] == NULL) {
			stmt->result->rows[row] = M_buf_create();
		}
	} else {
		/* Not starting a new row ... just close the prior column */
		M_sql_driver_stmt_result_col_end(stmt);
	}

	col   = stmt->result->curr_col-1;
	row   = stmt->result->num_rows-1;
	cell  = row * stmt->result->num_cols + col;
	len   = M_buf_len(stmt->result->rows[row]);

	/* Can't add more columns than we're allowed */
	if (col >= stmt->result->num_cols)
		return NULL;

	/* Align offset for safety */
	if (len % M_SAFE_ALIGNMENT != 0) {
		size_t pad_align = M_SAFE_ALIGNMENT - (len % M_SAFE_ALIGNMENT);
		M_buf_add_fill(stmt->result->rows[row], 0, pad_align);
	}

	stmt->result->cellinfo[cell].offset = M_buf_len(stmt->result->rows[row]);

	return stmt->result->rows[row];
}


M_bool M_sql_driver_stmt_result_row_finish(M_sql_stmt_t *stmt)
{
	if (stmt == NULL || stmt->result == NULL || stmt->result->curr_col != stmt->result->num_cols) {
		return M_FALSE;
	}

	M_sql_driver_stmt_result_col_end(stmt);
	stmt->result->curr_col = 0;
	return M_TRUE;
}
