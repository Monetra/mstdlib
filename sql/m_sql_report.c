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

typedef struct {
	char                  *name;         /*!< Output name of column */
	char                  *sql_col_name; /*!< SQL Column name referenced (if not specified, col_idx must be) */
	ssize_t                sql_col_idx;  /*!< Column index referenced (if not specified, col_name must be) */
	M_sql_report_cell_cb_t cb;           /*!< Callback used for formatting output */
} M_sql_report_col_t;


struct M_sql_report {
	M_sql_report_flags_t    flags;           /*!< Bitmap of #M_sql_report_flags_t */
	unsigned char           field_delim[8];  /*!< Field delimiter, defaults to ',' */
	size_t                  field_delim_size;
	unsigned char           row_delim[8];    /*!< Row delimiter, defaults to '\r\n' */
	size_t                  row_delim_size;
	unsigned char           field_encaps[8]; /*!< Field Encapsulation, defaults to '"' */
	size_t                  field_encaps_size;
	unsigned char           field_escape[8]; /*!< Field Escape, defaults to '"' */
	size_t                  field_escape_size;
	M_sql_report_fetch_cb_t fetch_cb;        /*!< Callback to call after M_sql_stmt_fetch() */

	M_llist_t           *add_cols;           /*!< Ordered list of columns to output */
	M_hash_dict_t       *hide_cols_byname;   /*!< Dictionary of columns in server output to hide, only relevant if flags has M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED */
	M_hash_u64str_t     *hide_cols_byidx;    /*!< Indexes to hide, only relevant if flags has M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED */
};


static void M_sql_report_col_destroy(void *arg)
{
	M_sql_report_col_t *col = arg;
	if (col == NULL)
		return;
	M_free(col->name);
	M_free(col->sql_col_name);
	M_free(col);
}


M_sql_report_t *M_sql_report_create(M_uint32 flags)
{
	const struct M_llist_callbacks add_cols_cb = {
		NULL,
		NULL,
		NULL,
		M_sql_report_col_destroy
	};
	M_sql_report_t *report    = M_malloc_zero(sizeof(*report));
	report->flags             = flags;
	report->field_delim[0]    = ',';
	report->field_delim_size  = 1;

	report->row_delim[0]      = '\r';
	report->row_delim[1]      = '\n';
	report->row_delim_size    = 2;

	report->field_encaps[0]   = '"';
	report->field_encaps_size = 1;

	report->field_escape[0]   = '"';
	report->field_escape_size = 1;

	report->add_cols          = M_llist_create(&add_cols_cb, M_LLIST_NONE);

	/* Only create hide_cols if we're auto-importing columns from the SQL output */
	if (flags & M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED) {
		report->hide_cols_byname  = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP);
		report->hide_cols_byidx   = M_hash_u64str_create(16, 75, M_HASH_U64STR_NONE);
	}
	return report;
}


void M_sql_report_destroy(M_sql_report_t *report)
{
	if (report == NULL)
		return;

	M_llist_destroy(report->add_cols, M_TRUE);
	M_hash_dict_destroy(report->hide_cols_byname);
	M_hash_u64str_destroy(report->hide_cols_byidx);
	M_free(report);
}


M_bool M_sql_report_set_delims(M_sql_report_t *report, const unsigned char *field_delim, size_t field_delim_size, const unsigned char *row_delim, size_t row_delim_size, const unsigned char *field_encaps, size_t field_encaps_size, const unsigned char *field_escape, size_t field_escape_size)
{
	if (report == NULL)
		return M_FALSE;

	if (field_delim != NULL) {
		if (field_delim_size > sizeof(report->field_delim) || field_delim_size == 0)
			return M_FALSE;
		M_mem_copy(report->field_delim, field_delim, field_delim_size);
		report->field_delim_size = field_delim_size;
	}

	if (row_delim != NULL) {
		if (row_delim_size > sizeof(report->row_delim) || row_delim_size == 0)
			return M_FALSE;
		M_mem_copy(report->row_delim, row_delim, row_delim_size);
		report->row_delim_size = row_delim_size;
	}

	if (field_encaps != NULL) {
		if (field_encaps_size > sizeof(report->field_encaps) || field_encaps_size == 0)
			return M_FALSE;
		M_mem_copy(report->field_encaps, field_encaps, field_encaps_size);
		report->field_encaps_size = field_encaps_size;
	}

	if (field_escape != NULL) {
		if (field_escape_size > sizeof(report->field_escape) || field_escape_size == 0)
			return M_FALSE;
		M_mem_copy(report->field_escape, field_escape, field_escape_size);
		report->field_escape_size = field_escape_size;
	}

	return M_TRUE;
}


M_bool M_sql_report_set_fetch_cb(M_sql_report_t *report, M_sql_report_fetch_cb_t fetch_cb)
{
	if (report == NULL || fetch_cb == NULL)
		return M_FALSE;

	report->fetch_cb = fetch_cb;
	return M_TRUE;
}


M_sql_report_cberror_t M_sql_report_cell_cb_passthru(M_sql_stmt_t *stmt, void *arg, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null)
{
	const char *text    = NULL;

	(void)arg;

	*is_null = M_TRUE;

	if (col < 0)
		return M_SQL_REPORT_SUCCESS;

	M_sql_stmt_result_isnull(stmt, row, (size_t)col, is_null);
	if (*is_null)
		return M_SQL_REPORT_SUCCESS;

	/* Handle binary data */
	if (M_sql_stmt_result_col_type(stmt, (size_t)col, NULL) == M_SQL_DATA_TYPE_BINARY) {
		const M_uint8 *bin      = NULL;
		size_t         bin_size = 0;
		size_t         enc_size = 0;
		M_uint8       *wbuf     = NULL;

		M_sql_stmt_result_binary(stmt, row, (size_t)col, &bin, &bin_size);

		/* Base64 encode it */
		enc_size = M_bincodec_encode_size(bin_size, 0, M_BINCODEC_BASE64);
		wbuf     = M_buf_direct_write_start(buf, &enc_size);
		enc_size = M_bincodec_encode((char *)wbuf, enc_size, bin, bin_size, 0, M_BINCODEC_BASE64);
		M_buf_direct_write_end(buf, enc_size);
		return M_SQL_REPORT_SUCCESS;
	}

	/* Normal text */
	M_sql_stmt_result_text(stmt, row, (size_t)col, &text);
	M_buf_add_str(buf, text);
	return M_SQL_REPORT_SUCCESS;
}


M_sql_report_cberror_t M_sql_report_cell_cb_int5dec(M_sql_stmt_t *stmt, void *arg, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null)
{
	M_int64     i64;
	M_decimal_t dec;
	M_uint8     num_dec = 5;

	(void)arg;

	*is_null = M_TRUE;

	if (col < 0)
		return M_SQL_REPORT_ERROR;

	M_sql_stmt_result_isnull(stmt, row, (size_t)col, is_null);
	if (*is_null)
		return M_SQL_REPORT_SUCCESS;

	M_sql_stmt_result_int64(stmt, row, (size_t)col, &i64);

	/* Convert to Decimal with 5 implied decimal places */
	M_decimal_from_int(&dec, i64, num_dec);

	/* Output in string form with real decimal and 5 guaranteed decimal places */
	if (!M_buf_add_decimal(buf, &dec, M_FALSE /* We want the decimal point */, (M_int8)num_dec, 0 /* no max width */))
		return M_SQL_REPORT_ERROR;
	return M_SQL_REPORT_SUCCESS;
}


M_sql_report_cberror_t M_sql_report_cell_cb_int2dec(M_sql_stmt_t *stmt, void *arg, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null)
{
	M_int64     i64;
	M_decimal_t dec;
	M_uint8     num_dec = 2;

	(void)arg;

	*is_null = M_TRUE;

	if (col < 0)
		return M_SQL_REPORT_ERROR;

	M_sql_stmt_result_isnull(stmt, row, (size_t)col, is_null);
	if (*is_null)
		return M_SQL_REPORT_SUCCESS;

	M_sql_stmt_result_int64(stmt, row, (size_t)col, &i64);

	/* Convert to Decimal with 2 implied decimal places */
	M_decimal_from_int(&dec, i64, num_dec);

	/* Output in string form with real decimal and 2 guaranteed decimal places */
	if (!M_buf_add_decimal(buf, &dec, M_FALSE /* We want the decimal point */, (M_int8)num_dec, 0 /* no max width */))
		return M_SQL_REPORT_ERROR;
	return M_SQL_REPORT_SUCCESS;
}


M_sql_report_cberror_t M_sql_report_cell_cb_boolyesno(M_sql_stmt_t *stmt, void *arg, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null)
{
	M_bool   b;

	(void)arg;

	*is_null = M_TRUE;

	if (col < 0)
		return M_SQL_REPORT_ERROR;

	M_sql_stmt_result_isnull(stmt, row, (size_t)col, is_null);
	if (*is_null)
		return M_SQL_REPORT_SUCCESS;

	if (M_sql_stmt_result_bool(stmt, row, (size_t)col, &b) != M_SQL_ERROR_SUCCESS)
		return M_SQL_REPORT_ERROR;

	M_buf_add_str(buf, b?"yes":"no");

	return M_SQL_REPORT_SUCCESS;
}


M_bool M_sql_report_add_column(M_sql_report_t *report, const char *name, M_sql_report_cell_cb_t cb, const char *sql_col_name, ssize_t sql_col_idx)
{
	M_sql_report_col_t *col;

	if (report == NULL || name == NULL || cb == NULL)
		return M_FALSE;

	/* Can't specify both sql_col_name and sql_col_idx, but you're allowed to specify neither */
	if (sql_col_name != NULL && sql_col_idx >= 0)
		return M_FALSE;

	col               = M_malloc_zero(sizeof(*col));
	col->name         = M_strdup(name);
	col->cb           = cb;
	col->sql_col_name = M_strdup(sql_col_name);
	col->sql_col_idx  = sql_col_idx;

	M_llist_insert(report->add_cols, col);
	return M_TRUE;
}


M_bool M_sql_report_hide_column(M_sql_report_t *report, const char *sql_col_name, ssize_t sql_col_idx)
{
	if (report == NULL || (sql_col_name == NULL && sql_col_idx < 0) || (sql_col_name != NULL && sql_col_idx >= 0))
		return M_FALSE;

	if (sql_col_name)
		M_hash_dict_insert(report->hide_cols_byname, sql_col_name, NULL);
	if (sql_col_idx >= 0)
		M_hash_u64str_insert(report->hide_cols_byidx, (M_uint64)sql_col_idx, NULL);
	return M_TRUE;
}


static M_sql_report_col_t *M_sql_report_addcol_get(const M_sql_report_t *report, const char *name, ssize_t idx)
{
	M_llist_node_t *node;

	for (node = M_llist_first(report->add_cols); node != NULL; node = M_llist_node_next(node)) {
		M_sql_report_col_t *col = M_llist_node_val(node);
		if (col->sql_col_name != NULL && M_str_caseeq(name, col->sql_col_name))
			return col;
		if (col->sql_col_idx >= 0 && idx == col->sql_col_idx)
			return col;
	}
	return NULL;
}


static M_sql_report_col_t *M_sql_report_create_cols_passthru(const M_sql_report_t *report, M_sql_stmt_t *stmt, size_t *out_cnt, char *error, size_t error_size)
{
	M_sql_report_col_t *cols;
	size_t              max_cols;
	size_t              num_cols = 0;
	size_t              i;
	M_llist_node_t     *node;

	max_cols = M_sql_stmt_result_num_cols(stmt) + M_llist_len(report->add_cols);
	cols     = M_malloc_zero(sizeof(*cols) * max_cols);

	/* Since its pass-thru, we start by preferring the order of the columns from the SQL server */
	for (i=0; i<M_sql_stmt_result_num_cols(stmt); i++) {
		M_sql_report_col_t *col;
		const char *name = M_sql_stmt_result_col_name(stmt, i);
		/* First see if this column is excluded */
		if (M_hash_u64str_get(report->hide_cols_byidx, i, NULL) ||
		    M_hash_dict_get(report->hide_cols_byname, name, NULL)) {
			continue;
		}

		/* Next see if this column has an override */
		col = M_sql_report_addcol_get(report, name, (ssize_t)i);
		if (col != NULL) {
			/* Duplicate data and set appropriate index */
			M_mem_copy(&cols[num_cols], col, sizeof(*col));
			cols[num_cols].sql_col_idx = (ssize_t)i;
		} else {
			/* Create new column */
			cols[num_cols].name        = M_CAST_OFF_CONST(char *, name); /* Won't ever free, this is safe */
			cols[num_cols].sql_col_idx = (ssize_t)i;
			cols[num_cols].cb          = M_sql_report_cell_cb_passthru;
		}
		num_cols++;
	}

	/* Add any columns that don't match a name/index */
	for (node = M_llist_first(report->add_cols); node != NULL; node = M_llist_node_next(node)) {
		M_sql_report_col_t *col = M_llist_node_val(node);

		/* Check for invalidation */
		if ((col->sql_col_name != NULL && !M_sql_stmt_result_col_idx(stmt, col->sql_col_name, NULL)) ||
		    col->sql_col_idx >= (ssize_t)M_sql_stmt_result_num_cols(stmt)) {
			M_snprintf(error, error_size, "column %s references an unknown sql column %s:%zd", col->name, col->sql_col_name, col->sql_col_idx);
			M_free(cols);
			return NULL;
		}

		/* Skip one that has already been added */
		if (col->sql_col_name != NULL || col->sql_col_idx >= 0)
			continue;

		M_mem_copy(&cols[num_cols], col, sizeof(*col));
		num_cols++;
	}

	*out_cnt = num_cols;
	return cols;
}


static M_sql_report_col_t *M_sql_report_create_cols_normal(const M_sql_report_t *report, M_sql_stmt_t *stmt, size_t *out_cnt, char *error, size_t error_size)
{
	M_sql_report_col_t *cols;
	size_t              max_cols;
	size_t              num_cols = 0;
	M_llist_node_t     *node;

	max_cols = M_llist_len(report->add_cols);
	cols     = M_malloc_zero(sizeof(*cols) * max_cols);

	/* Add any columns that don't match a name/index */
	for (node = M_llist_first(report->add_cols); node != NULL; node = M_llist_node_next(node)) {
		M_sql_report_col_t *col = M_llist_node_val(node);

		/* Check for invalidation */
		if ((col->sql_col_name != NULL && !M_sql_stmt_result_col_idx(stmt, col->sql_col_name, NULL)) ||
		    col->sql_col_idx >= (ssize_t)M_sql_stmt_result_num_cols(stmt)) {
			M_snprintf(error, error_size, "column %s references an unknown sql column %s:%zd", col->name, col->sql_col_name, col->sql_col_idx);
			M_free(cols);
			return NULL;
		}

		M_mem_copy(&cols[num_cols], col, sizeof(*col));

		/* Get column index from name */
		if (col->sql_col_name)
			M_sql_stmt_result_col_idx(stmt, col->sql_col_name, (size_t *)&cols[num_cols].sql_col_idx);
		num_cols++;
	}

	*out_cnt = num_cols;
	return cols;
}


typedef enum {
	M_SQL_REPORT_ENCAP_NONE    = 0,
	M_SQL_REPORT_ENCAP_ONLY    = 1,
	M_SQL_REPORT_ENCAP_REWRITE = 2
} M_sql_report_encap_type_t;


static __inline__ M_bool M_sql_report_data_matches(const unsigned char *data, size_t data_len, const unsigned char *srch, size_t srch_len)
{
	if (srch_len > data_len)
		return M_FALSE;
	if (data_len == 0 || srch_len == 0)
		return M_FALSE;
	if (data[0] != srch[0])
		return M_FALSE;
	if (srch_len == 1)
		return M_TRUE;
	return M_mem_eq(data, srch, srch_len);
}


static M_sql_report_encap_type_t M_sql_report_col_needs_encap(const M_sql_report_t *report, const M_uint8 *data, size_t data_len)
{
	size_t                    i;
	M_sql_report_encap_type_t type = M_SQL_REPORT_ENCAP_NONE;

	/* If supplied data is not NULL, but has 0 length, or the cell starts or ends with whitespace, it needs encapsulation */
	if ((data != NULL && data_len == 0) ||
	    (data != NULL && data_len && (M_chr_isspace((char)data[0]) || M_chr_isspace((char)data[data_len-1])))
	   ) {
		type = M_SQL_REPORT_ENCAP_ONLY;
	}

	for (i=0; data != NULL && i<data_len; i++) {
		if (M_sql_report_data_matches(data+i, data_len - i, report->field_encaps, report->field_encaps_size))
			return M_SQL_REPORT_ENCAP_REWRITE;
		if (M_sql_report_data_matches(data+i, data_len - i, report->field_escape, report->field_escape_size))
			return M_SQL_REPORT_ENCAP_REWRITE;

		/* No need for below checks */
		if (type == M_SQL_REPORT_ENCAP_ONLY)
			continue;

		if (M_sql_report_data_matches(data+i, data_len - i, report->field_delim, report->field_delim_size)) {
			type = M_SQL_REPORT_ENCAP_ONLY;
			continue;
		}

		if (M_sql_report_data_matches(data+i, data_len - i, report->row_delim, report->row_delim_size)) {
			type = M_SQL_REPORT_ENCAP_ONLY;
			continue;
		}
	}

	return type;
}


static void M_sql_report_col_append(const M_sql_report_t *report, M_buf_t *outbuf, M_sql_report_encap_type_t encap_type, const M_uint8 *data, size_t data_len)
{
	size_t        i;

	if (encap_type != M_SQL_REPORT_ENCAP_REWRITE) {
		M_buf_add_bytes(outbuf, data, data_len);
		return;
	}

	/* escape as necessary */
	for (i=0; i<data_len; i++) {
		if (M_sql_report_data_matches(data + i, data_len - i, report->field_encaps, report->field_encaps_size) ||
		    M_sql_report_data_matches(data + i, data_len - i, report->field_escape, report->field_escape_size)) {
			M_buf_add_bytes(outbuf, report->field_escape, report->field_escape_size);
		}

		M_buf_add_byte(outbuf, data[i]);
	}
}


struct M_sql_report_state {
	M_sql_report_col_t *cols;
	size_t              num_cols;
	M_buf_t            *colbuf;
	size_t              rowidx;
};


M_sql_error_t M_sql_report_process_partial(const M_sql_report_t *report, M_sql_stmt_t *stmt, size_t max_rows, void *arg, M_buf_t *buf, M_sql_report_state_t **state, char *error, size_t error_size)
{
	M_sql_error_t  err         = M_SQL_ERROR_SUCCESS;
	size_t         rows_output = 0;

	M_mem_set(error, 0, error_size);

	if (report == NULL || stmt == NULL || state == NULL || buf == NULL) {
		M_snprintf(error, error_size, "invalid parameters passed");
		return M_SQL_ERROR_INVALID_USE;
	}

	err = M_sql_stmt_get_error(stmt);
	if (M_sql_error_is_error(err)) {
		M_snprintf(error, error_size, "cant generate report for failed request");
		goto done;
	}

	/* First call, initialize state */
	if (*state == NULL) {
		if (M_sql_stmt_result_num_cols(stmt) == 0) {
			M_snprintf(error, error_size, "not a query, no columns");
			return M_SQL_ERROR_INVALID_USE;
		}

		*state = M_malloc_zero(sizeof(**state));

		/* We generate the column list differently depending on if M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED is set or not */
		if (report->flags & M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED) {
			(*state)->cols = M_sql_report_create_cols_passthru(report, stmt, &(*state)->num_cols, error, error_size);
		} else {
			(*state)->cols = M_sql_report_create_cols_normal(report, stmt, &(*state)->num_cols, error, error_size);
		}

		if ((*state)->cols == NULL) {
			err = M_SQL_ERROR_INVALID_USE;
			goto done;
		}

		if ((*state)->num_cols == 0) {
			M_snprintf(error, error_size, "no columns to output");
			err = M_SQL_ERROR_INVALID_USE;
			goto done;
		}

		(*state)->colbuf = M_buf_create();

		/* Output headers if desired -- only when first initializing state! */
		if (!(report->flags & M_SQL_REPORT_FLAG_OMIT_HEADERS)) {
			size_t                    i;
			M_sql_report_encap_type_t encap_type;
			for (i=0; i<(*state)->num_cols; i++) {
				if (i != 0)
					M_buf_add_bytes(buf, report->field_delim, report->field_delim_size);

				encap_type = M_sql_report_col_needs_encap(report, (const M_uint8 *)(*state)->cols[i].name, M_str_len((*state)->cols[i].name));
				if (report->flags & M_SQL_REPORT_FLAG_ALWAYS_ENCAP || encap_type != M_SQL_REPORT_ENCAP_NONE) {
					M_buf_add_bytes(buf, report->field_encaps, report->field_encaps_size);
				}

				M_sql_report_col_append(report, buf, encap_type, (const M_uint8 *)(*state)->cols[i].name, M_str_len((*state)->cols[i].name));

				if (report->flags & M_SQL_REPORT_FLAG_ALWAYS_ENCAP || encap_type != M_SQL_REPORT_ENCAP_NONE) {
					M_buf_add_bytes(buf, report->field_encaps, report->field_encaps_size);
				}
			}
			M_buf_add_bytes(buf, report->row_delim, report->row_delim_size);
		}
	}

	/* Process rows */
	do {
		size_t                    j;
		size_t                    rows = M_sql_stmt_result_num_rows(stmt);
		M_sql_report_encap_type_t encap_type;

		for (  ; (*state)->rowidx < rows; (*state)->rowidx++) {
			size_t                 start_buf_len = M_buf_len(buf);
			M_sql_report_cberror_t cberr         = M_SQL_REPORT_SUCCESS;

			for (j=0; j<(*state)->num_cols; j++) {
				M_bool                 is_null = M_FALSE;

				if (j != 0)
					M_buf_add_bytes(buf, report->field_delim, report->field_delim_size);

				M_buf_truncate((*state)->colbuf, 0);
				cberr = (*state)->cols[j].cb(stmt, arg, (*state)->rowidx, (*state)->cols[j].sql_col_idx, (*state)->colbuf, &is_null);
				if (cberr == M_SQL_REPORT_ERROR) {
					M_snprintf(error, error_size, "row %zu, col (%zu) %s callback failed", (*state)->rowidx, j, (*state)->cols[j].name);
					err = M_SQL_ERROR_INVALID_USE;
					goto done;
				} else if (cberr == M_SQL_REPORT_SKIP_ROW) {
					/* Kill anything already added to the buf, and stop processing columns for this row */
					M_buf_truncate(buf, start_buf_len);
					break;
				}

				encap_type = M_sql_report_col_needs_encap(report, (const M_uint8 *)M_buf_peek((*state)->colbuf), M_buf_len((*state)->colbuf));

				if (!is_null && (report->flags & M_SQL_REPORT_FLAG_ALWAYS_ENCAP || encap_type != M_SQL_REPORT_ENCAP_NONE)) {
					M_buf_add_bytes(buf, report->field_encaps, report->field_encaps_size);
				}

				M_sql_report_col_append(report, buf, encap_type, (const M_uint8 *)(is_null?NULL:M_buf_peek((*state)->colbuf)), is_null?0:M_buf_len((*state)->colbuf));

				if (!is_null && (report->flags & M_SQL_REPORT_FLAG_ALWAYS_ENCAP || encap_type != M_SQL_REPORT_ENCAP_NONE)) {
					M_buf_add_bytes(buf, report->field_encaps, report->field_encaps_size);
				}
			}

			if (cberr == M_SQL_REPORT_SUCCESS) {
				M_buf_add_bytes(buf, report->row_delim, report->row_delim_size);

				/* Don't output more than the requested number of rows at once */
				rows_output++;
			}

			if (max_rows != 0 && rows_output == max_rows)
				break;
		}

		/* If we've output the max rows we're allowed to, exit out */
		if (max_rows != 0 && rows_output == max_rows) {
			/* Make sure we indicate we need to loop, even if the real error is M_SQL_ERROR_SUCCESS
			 * since we're breaking out early */
			err = M_SQL_ERROR_SUCCESS_ROW;
			break;
		}

		/* Nothing more to fetch, break */
		if (err == M_SQL_ERROR_SUCCESS)
			break;

		/* Fetch more data! */
		err              = M_sql_stmt_fetch(stmt);
		(*state)->rowidx = 0;

		if (M_sql_error_is_error(err)) {
			M_snprintf(error, error_size, "fetch error: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		} else {
			if (report->fetch_cb && !report->fetch_cb(stmt, arg)) {
				M_snprintf(error, error_size, "fetch callback failed");
				err = M_SQL_ERROR_USER_FAILURE;
			}
		}
	} while (err == M_SQL_ERROR_SUCCESS_ROW);


done:
	if (*state != NULL && err != M_SQL_ERROR_SUCCESS_ROW) {
		M_free((*state)->cols);
		M_buf_cancel((*state)->colbuf);
		M_free(*state);
		*state = NULL;
	}
	return err;
}


M_sql_error_t M_sql_report_process(const M_sql_report_t *report, M_sql_stmt_t *stmt, void *arg, char **out, size_t *out_len, char *error, size_t error_size)
{
	M_buf_t              *buf   = M_buf_create();
	M_sql_report_state_t *state = NULL;
	M_sql_error_t         err   = M_sql_report_process_partial(report, stmt, 0, arg, buf, &state, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS) {
		M_buf_cancel(buf);
		return err;
	}
	*out = M_buf_finish_str(buf, out_len);
	return M_SQL_ERROR_SUCCESS;
}
