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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_csv {
	char      ***data_arr;
	size_t       num_rows;
	size_t       num_cols;
	char        *data_ptr;
	M_hash_stridx_t *headers;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_csv_parse_count(const char *str, size_t len, char delim, char quote, size_t *num_rows_out, size_t *num_cols_out)
{
	size_t i, row_len = 0;
	M_bool on_quote   = M_FALSE;
	size_t num_cols;
	size_t num_rows;

	/* Count rows, and for first row, count columns */
	num_cols = num_rows = 0;
	for (i=0; i<len; i++) {
		if (str[i] == quote) {
			row_len++;
			if (str[i+1] == quote) {
			   i++; /* Double Quote, skip next char */
			} else {
				on_quote = on_quote ? M_FALSE : M_TRUE;
			}
		} else if (!on_quote && str[i] == delim && num_rows == 0) {
			row_len++;
			/* If still on first row, increment column count */
			num_cols++;
		} else if (!on_quote && str[i] == '\n') {
			if (num_rows == 0) num_cols++;
			row_len = 0;
			num_rows++;
		} else if (on_quote || str[i] != '\r') {
			row_len++;
		}
	}

	if (num_rows == 0 && row_len) num_cols++;
	if (row_len) num_rows++;

	*num_cols_out = num_cols;
	*num_rows_out = num_rows;
}


static void M_csv_remove_quotes(char *str, char quote)
{
	size_t len, i, cnt = 0;

	if (str == NULL)
		return;

	len = M_str_len(str);

	for (i=0; i<len; i++) {
		if (str[i] == quote) {
			if (str[i+1] == quote) {
				str[cnt++] = quote;
				i+=1;
			}
		} else {
			str[cnt++] = str[i];
		}
	}
	str[cnt] = 0;

	/* If this was an empty string, "", it may leave
	 * a single quote behind because it looks like a
	 * quoted quote.  Lets detect that and remove it */
	if (cnt == 1 && str[cnt-1] == quote)
		str[cnt-1] = 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_csv_destroy(M_csv_t *csv)
{
	if (csv == NULL)
		return;
	M_free(csv->data_arr);
	M_free(csv->data_ptr);
	M_hash_stridx_destroy(csv->headers);
	M_free(csv);
}


M_csv_t *M_csv_parse_inplace(char *data, size_t len, char delim, char quote, M_uint32 flags)
{
	size_t num_rows = 0, num_cols = 0;
	size_t i, outlen;
	int on_quote = 0, had_quote = 0;
	char ***out = NULL ,*buf = NULL;
	size_t row = 0, col = 0;
	M_csv_t *csv = NULL;

	if (data == NULL || len == 0)
		return NULL;

	M_csv_parse_count(data, len, delim, quote, &num_rows, &num_cols);

	/* Allocate as one large matrix of pointers */
	outlen = (num_rows * sizeof(*out)) + (num_rows * (num_cols * sizeof(**out)));
	if (outlen == 0)
		return NULL;
	out = M_malloc(outlen);
	M_mem_set(out, 0, outlen);
	buf = (char *)out;
	for (i=0; i<(size_t)num_rows; i++) {
		/* Calc offset of allocated block to assign pointers */
		out[i] = (char **)(void *)&buf[num_rows*sizeof(*out) + i*num_cols*sizeof(**out)];
	}

	/* Set pointers to positions in data for column starts */
	row       = 0;
	col       = 0;
	on_quote  = 0;
	had_quote = 0;
	out[row][col] = data;
	for (i=0; i<len; i++) {
		if (data[i] == quote) {
			had_quote = 1;
			if (on_quote && data[i+1] == quote) {
				/* Double Quote, skip next char */
				i++;
			} else if (on_quote) {
				on_quote = 0;
			} else {
				on_quote = 1;
			}
		} else if (!on_quote && data[i] == delim) {
			data[i] = 0;

			if (col < num_cols) {
				/* If we know the previous column had quotes, lets
				 * sanitize that data now */
				if (had_quote) {
					M_csv_remove_quotes(out[row][col], quote);
				} else {
					if (flags & M_CSV_FLAG_TRIM_WHITESPACE) {
						/* Trim whitespace if wasn't quoted */
						M_str_trim(out[row][col]);
					}
					/* If empty string and wasn't quoted, record as NULL to differentiate */
					if (M_str_isempty(out[row][col])) {
						out[row][col] = NULL;
					}
				}
			}
			had_quote = 0;

			col++;

			/* Ensure we don't overflow by one row containing too
			 * many columns.  We will effectively truncate every
			 * row that contains more columns than our first row */
			if (col < num_cols) {
				out[row][col] = data+(i+1);
			}
		} else if (!on_quote && data[i] == '\n') {
			data[i] = 0;

			if (col < num_cols) {
				/* If we know the previous column had quotes, lets
				 * sanitize that data now */
				if (had_quote) {
					M_csv_remove_quotes(out[row][col], quote);
				} else {
					if (flags & M_CSV_FLAG_TRIM_WHITESPACE) {
						/* Trim whitespace if wasn't quoted */
						M_str_trim(out[row][col]);
					}
					/* If empty string and wasn't quoted, record as NULL to differentiate */
					if (M_str_isempty(out[row][col])) {
						out[row][col] = NULL;
					}
				}
			}

			row++;
			col = 0;

			if (row == num_rows)
				break;

			out[row][col] = data+(i+1);

			had_quote = 0;
		} else if (!on_quote && data[i] == '\r') {
			data[i] = 0;
		}
	}

	/* If we know the previous column had quotes, lets
	 * sanitize that data now */
	if (row < num_rows && col < num_cols) {
		/* If we know the previous column had quotes, lets
		 * sanitize that data now */
		if (had_quote) {
			M_csv_remove_quotes(out[row][col], quote);
		} else {
			if (flags & M_CSV_FLAG_TRIM_WHITESPACE) {
				/* Trim whitespace if wasn't quoted */
				M_str_trim(out[row][col]);
			}
			/* If empty string and wasn't quoted, record as NULL to differentiate */
			if (M_str_isempty(out[row][col])) {
				out[row][col] = NULL;
			}
		}
	}

	csv = M_malloc(sizeof(*csv));
	csv->data_arr = out;
	csv->data_ptr = data;
	csv->num_rows = num_rows;
	csv->num_cols = num_cols;

	/* Create lookup table for column names to indexes */
	csv->headers  = M_hash_stridx_create(num_cols * 2, 75, M_HASH_STRIDX_CASECMP);
	for (i=0; i<num_cols; i++) {
		M_hash_stridx_insert(csv->headers, M_csv_get_header(csv, i), i);
	}
	return csv;
}


M_csv_t *M_csv_parse(const char *data, size_t len, char delim, char quote, M_uint32 flags)
{
	char *out;

	if (data == NULL || len == 0)
		return NULL;

	/* Duplicate data for modification */
	out = M_malloc(len+1);
	M_mem_copy(out, data, len);
	out[len] = 0;

	return M_csv_parse_inplace(out, len, delim, quote, flags);
}


size_t M_csv_raw_num_rows(const M_csv_t *csv)
{
	if (csv == NULL)
		return 0;
	return csv->num_rows;
}


size_t M_csv_raw_num_cols(const M_csv_t *csv)
{
	if (csv == NULL)
		return 0;
	return csv->num_cols;
}


const char *M_csv_raw_cell(const M_csv_t *csv, size_t row, size_t col)
{
	if (csv == NULL)
		return NULL;

	if (csv->num_rows > row && csv->num_cols > col)
		return csv->data_arr[row][col];
	return NULL;
}


size_t M_csv_get_numrows(const M_csv_t *csv)
{
	size_t rows;

	if (csv == NULL)
		return 0;

	rows = M_csv_raw_num_rows(csv);
	if (rows > 0)
		rows--;
	return rows;
}


size_t M_csv_get_numcols(const M_csv_t *csv)
{
	if (csv == NULL)
		return 0;
	return M_csv_raw_num_cols(csv);
}


const char *M_csv_get_cellbynum(const M_csv_t *csv, size_t row, size_t col)
{
	if (csv == NULL)
		return NULL;
	return M_csv_raw_cell(csv, row+1, col);
}


const char *M_csv_get_header(const M_csv_t *csv, size_t col)
{
	if (csv == NULL)
		return NULL;
	return M_csv_raw_cell(csv, 0, col);
}


ssize_t M_csv_get_cell_num(const M_csv_t *csv, const char *colname)
{
	size_t idx = 0;

	if (csv == NULL)
		return -1;

	if (!M_hash_stridx_get(csv->headers, colname, &idx))
		return -1;

	return (ssize_t)idx;
}


const char *M_csv_get_cell(const M_csv_t *csv, size_t row, const char *colname)
{
	size_t col;
	ssize_t ret;

	if (csv == NULL)
		return NULL;

	if (row >= M_csv_get_numrows(csv))
		return NULL;

	ret = M_csv_get_cell_num(csv, colname);
	if (ret == -1)
		return NULL;
	col = (size_t)ret;

	return M_csv_get_cellbynum(csv, row, col);
}
