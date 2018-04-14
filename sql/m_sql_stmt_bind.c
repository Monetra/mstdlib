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


void M_sql_stmt_bind_clear(M_sql_stmt_t *stmt)
{
	size_t i;
	size_t j;

	if (stmt == NULL)
		return;

	for (i=0; i<stmt->bind_row_cnt; i++) {
		for (j=0; j<stmt->bind_rows[i].col_cnt; j++) {
			M_sql_stmt_bind_col_t *col = &stmt->bind_rows[i].cols[j];
			if (col->type == M_SQL_DATA_TYPE_TEXT && !col->v.text.is_const)
				M_free(col->v.text.data);
			if (col->type == M_SQL_DATA_TYPE_BINARY && !col->v.binary.is_const)
				M_free(col->v.binary.data);
		}
		M_free(stmt->bind_rows[i].cols);
	}
	M_free(stmt->bind_rows);
	stmt->bind_rows    = NULL;
	stmt->bind_row_cnt = 0;
}


void M_sql_stmt_bind_new_row(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return;

	/* If the last row is a blank row, ignore. */
	if (stmt->bind_row_cnt && stmt->bind_rows[stmt->bind_row_cnt-1].col_cnt == 0)
		return;

	/* Since we allocate by powers of two, we know we have room if the row count isn't equal to the next power of 2 */
	if (stmt->bind_row_cnt && stmt->bind_row_cnt != M_size_t_round_up_to_power_of_two(stmt->bind_row_cnt)) {
		stmt->bind_row_cnt++;
		return;
	}

	/* Resize */
	stmt->bind_row_cnt++;
	stmt->bind_rows = M_realloc_zero(stmt->bind_rows, sizeof(*(stmt->bind_rows)) * M_size_t_round_up_to_power_of_two(stmt->bind_row_cnt));
}


static M_sql_stmt_bind_col_t *M_sql_stmt_bind_new_col(M_sql_stmt_t *stmt)
{
	size_t row;

	if (stmt == NULL)
		return NULL;

	/* If there is no row yet, allocate one */
	if (stmt->bind_row_cnt == 0)
		M_sql_stmt_bind_new_row(stmt);

	row = stmt->bind_row_cnt - 1;

	/* Since we allocate by powers of two, we know we have room if the col count isn't equal to the next power of 2 */
	if (stmt->bind_rows[row].col_cnt && stmt->bind_rows[row].col_cnt != M_size_t_round_up_to_power_of_two(stmt->bind_rows[row].col_cnt)) {
		stmt->bind_rows[row].col_cnt++;
		
	} else {
		/* Resize */
		stmt->bind_rows[row].col_cnt++;
		stmt->bind_rows[row].cols = M_realloc_zero(stmt->bind_rows[row].cols, sizeof(*(stmt->bind_rows[row].cols)) * M_size_t_round_up_to_power_of_two(stmt->bind_rows[row].col_cnt));
	}

	return &stmt->bind_rows[row].cols[stmt->bind_rows[row].col_cnt-1];
}

M_sql_error_t M_sql_stmt_bind_bool(M_sql_stmt_t *stmt, M_bool val)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type = M_SQL_DATA_TYPE_BOOL;
	col->v.b  = val;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_bool_null(M_sql_stmt_t *stmt)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type   = M_SQL_DATA_TYPE_BOOL;
	col->isnull = M_TRUE;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int16(M_sql_stmt_t *stmt, M_int16 val)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type  = M_SQL_DATA_TYPE_INT16;
	col->v.i16 = val;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int16_null(M_sql_stmt_t *stmt)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type   = M_SQL_DATA_TYPE_INT16;
	col->isnull = M_TRUE;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int32(M_sql_stmt_t *stmt, M_int32 val)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type  = M_SQL_DATA_TYPE_INT32;
	col->v.i32 = val;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int32_null(M_sql_stmt_t *stmt)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type   = M_SQL_DATA_TYPE_INT32;
	col->isnull = M_TRUE;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int64(M_sql_stmt_t *stmt, M_int64 val)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type  = M_SQL_DATA_TYPE_INT64;
	col->v.i64 = val;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_int64_null(M_sql_stmt_t *stmt)
{
	M_sql_stmt_bind_col_t *col = M_sql_stmt_bind_new_col(stmt);
	col->type   = M_SQL_DATA_TYPE_INT64;
	col->isnull = M_TRUE;
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_text_const(M_sql_stmt_t *stmt, const char *text, size_t max_len)
{
	M_sql_stmt_bind_col_t *col;

	col                  = M_sql_stmt_bind_new_col(stmt);
	col->type            = M_SQL_DATA_TYPE_TEXT;
	if (text == NULL) {
		col->isnull      = M_TRUE;
	} else {
		col->v.text.is_const = M_TRUE;
		col->v.text.data     = M_CAST_OFF_CONST(char *, text);
		if (max_len == 0) {
			col->v.text.max_len = M_str_len(text);
		} else {
			col->v.text.max_len = M_str_len_max(text, max_len);
		}
	}
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_text_own(M_sql_stmt_t *stmt, char *text, size_t max_len)
{
	M_sql_stmt_bind_col_t *col;

	col                  = M_sql_stmt_bind_new_col(stmt);
	col->type            = M_SQL_DATA_TYPE_TEXT;
	if (text == NULL) {
		col->isnull      = M_TRUE;
	} else {
		col->v.text.is_const = M_FALSE;
		col->v.text.data     = text;
		if (max_len == 0) {
			col->v.text.max_len = M_str_len(text);
		} else {
			col->v.text.max_len = M_str_len_max(text, max_len);
		}
	}
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_text_dup(M_sql_stmt_t *stmt, const char *text, size_t max_len)
{
	M_sql_stmt_bind_col_t *col;

	col                  = M_sql_stmt_bind_new_col(stmt);
	col->type            = M_SQL_DATA_TYPE_TEXT;
	if (text == NULL) {
		col->isnull      = M_TRUE;
	} else {
		col->v.text.is_const = M_FALSE;
		if (max_len == 0) {
			col->v.text.max_len = M_str_len(text);
		} else {
			col->v.text.max_len = M_str_len_max(text, max_len);
		}
		col->v.text.data     = M_strdup_max(text, col->v.text.max_len);
	}
	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_binary_const(M_sql_stmt_t *stmt, const M_uint8 *bin, size_t bin_len)
{
	M_sql_stmt_bind_col_t *col;

	col                    = M_sql_stmt_bind_new_col(stmt);
	col->type              = M_SQL_DATA_TYPE_BINARY;
	if (bin == NULL) {
		col->isnull        = M_TRUE;
	} else {
		col->v.binary.is_const = M_TRUE;
		col->v.binary.data     = M_CAST_OFF_CONST(M_uint8 *, bin);
		col->v.binary.len      = bin_len;
	}

	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_binary_own(M_sql_stmt_t *stmt, M_uint8 *bin, size_t bin_len)
{
	M_sql_stmt_bind_col_t *col;

	col                    = M_sql_stmt_bind_new_col(stmt);
	col->type              = M_SQL_DATA_TYPE_BINARY;
	if (bin == NULL) {
		col->isnull        = M_TRUE;
	} else {
		col->v.binary.is_const = M_FALSE;
		col->v.binary.data     = bin;
		col->v.binary.len      = bin_len;
	}

	return M_SQL_ERROR_SUCCESS;
}

M_sql_error_t M_sql_stmt_bind_binary_dup(M_sql_stmt_t *stmt, const M_uint8 *bin, size_t bin_len)
{
	M_sql_stmt_bind_col_t *col;

	col                    = M_sql_stmt_bind_new_col(stmt);
	col->type              = M_SQL_DATA_TYPE_BINARY;
	if (bin == NULL) {
		col->isnull        = M_TRUE;
	} else {
		col->v.binary.is_const = M_FALSE;
		col->v.binary.data     = M_memdup(bin, bin_len);
		col->v.binary.len      = bin_len;
	}

	return M_SQL_ERROR_SUCCESS;
}

