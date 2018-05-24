/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_load_csv(M_table_t *table, const char *data, size_t len, char delim, char quote, M_uint32 flags, M_bool have_header)
{
	M_csv_t    *csv;
	const char *colname;
	const char *val;
	size_t      csv_numcols;
	size_t      csv_numrows;
	size_t      table_numcols;
	size_t      rowidx;
	size_t      colidx;
	size_t      i;
	size_t      j;

	if (table == NULL)
		return M_FALSE;

	/* Nothing to load. */
	if (M_str_isempty(data) || len == 0)
		return M_TRUE;

	csv = M_csv_parse(data, len, delim, quote, flags);
	if (csv == NULL)
		return M_FALSE;

	/* Ensure we have all the header or enough columns. */
	if (have_header) {
		csv_numcols = M_csv_get_numcols(csv);
		for (i=0; i<csv_numcols; i++) {
			colname = M_csv_get_header(csv, i);
			if (!M_table_column_idx(table, colname, NULL)) {
				M_table_column_insert(table, colname);
			}
		}
	} else {
		table_numcols = M_table_column_count(table);
		csv_numcols   = M_csv_raw_num_cols(csv);
		for (i=csv_numcols; i<table_numcols; i++) {
			M_table_column_insert(table, NULL);
		}
	}

	if (have_header) {
		csv_numrows = M_csv_get_numrows(csv);
	} else {
		csv_numrows = M_csv_raw_num_rows(csv);
	}
	for (i=0; i<csv_numrows; i++) {
		rowidx = M_table_row_insert(table);

		for (j=0; j<csv_numcols; j++) {
			if (have_header) {
				colname = M_csv_get_header(csv, j);
				M_table_column_idx(table, colname, &colidx);
				val     = M_csv_get_cellbynum(csv, i, j);
			} else {
				colidx = j;
				val    = M_csv_raw_cell(csv, i, j);
			}
			M_table_cell_set_at(table, rowidx, colidx, val);
		}
	}

	M_csv_destroy(csv);
	return M_TRUE;
}

char *M_table_write_csv(M_table_t *table, char delim, char quote, M_bool write_header)
{
	M_buf_t    *buf;
	const char *const_temp;
	char        quoted_chars[4] = { 0 };
	size_t      numcols;
	size_t      numrows;
	size_t      i;
	size_t      j;

	if (table == NULL)
		return NULL;

	buf = M_buf_create();

	numrows = M_table_row_count(table);
	numcols = M_table_column_count(table);

	quoted_chars[0] = delim;
	quoted_chars[1] = quote;

	if (write_header) {
		for (i=0; i<numcols; i++) {
			const_temp = M_table_column_name(table, i);
			M_buf_add_str_quoted(buf, quote, quote, quoted_chars, M_FALSE, const_temp);
			M_buf_add_byte(buf, (unsigned char)delim);
		}
		/* Strip off the last delim. */
		M_buf_truncate(buf, M_buf_len(buf)-1);
		M_buf_add_str(buf, "\r\n");
	}

	for (i=0; i<numrows; i++) {
		for (j=0; j<numcols; j++) {
			const_temp = M_table_cell_at(table, i, j);
			M_buf_add_str_quoted(buf, quote, quote, quoted_chars, M_FALSE, const_temp);
			M_buf_add_byte(buf, (unsigned char)delim);
		}
		/* Strip off the last delim. */
		M_buf_truncate(buf, M_buf_len(buf)-1);
		M_buf_add_str(buf, "\r\n");
	}

	/* strip of the last line ending. */
	M_buf_truncate(buf, M_buf_len(buf)-2);

	return M_buf_finish_str(buf, NULL);
}
