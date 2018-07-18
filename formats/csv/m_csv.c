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
	char            ***data_arr;
	size_t             num_rows;
	size_t             num_cols;
	char              *data_ptr;
	char               delim;
	char               quote;
	M_hash_stridx_t   *headers;
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


/* Helper function for M_csv_output_headers_buf() and M_csv_output_rows_buf().
 *
 * Adds required quotes, escapes and trailing delimiter to the given cell value, then writes it to output buffer.
 *
 * If cell value is empty, still adds the trailing delimiter.
 */
static void add_cell(M_buf_t *buf, char delim, char quote, const char *cell)
{
	if (!M_str_isempty(cell)) {
		char        chars_to_quote[5] = {'\0'/*set to delim below*/, '\0'/*set to quote below*/, '\n', '\r', '\0'};
		M_bool      needs_quotes      = M_FALSE;
		const char *next_quote;
		size_t      len               = M_str_len(cell);
		size_t      chars_to_add;

		chars_to_quote[0] = delim;
		chars_to_quote[1] = quote;

		/* If cell value starts/ends with whitespace, or if it contains delimiter, newline chars, or quotes,
		 * it needs to be wrapped in quotes.
		 */
		if (M_chr_isspace(*cell) || M_chr_isspace(cell[len - 1])
			|| M_str_find_first_from_charset(cell, chars_to_quote) != NULL) {
			needs_quotes = M_TRUE;

			M_buf_add_char(buf, quote);
		}

		/* If cell value contains any quote chars, we'll need to escape them by adding a second quote character
		 * right after it.
		 */
		do {
			next_quote   = M_mem_chr(cell, (M_uint8)quote, len);

			chars_to_add = (next_quote == NULL)? len : (size_t)(next_quote - cell) + 1;

			M_buf_add_bytes(buf, cell, chars_to_add);
			len  -= chars_to_add;
			cell += chars_to_add;

			if (next_quote != NULL) {
				M_buf_add_char(buf, quote);
			}
		} while (len > 0);

		if (needs_quotes) {
			M_buf_add_char(buf, quote);
		}
	}

	/* Always add delimiter character at end, even if cell is empty. */
	M_buf_add_char(buf, delim);
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
	csv->delim    = delim;
	csv->quote    = quote;

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


M_csv_t *M_csv_parse_add_headers(const char *data, size_t len, char delim, char quote, M_uint32 flags,
	M_list_str_t *headers)
{
	M_buf_t *buf;
	size_t   i;
	char    *out;
	size_t   out_len;

	if (data == NULL || len == 0) {
		return NULL;
	}

	if (M_list_str_len(headers) == 0) {
		return M_csv_parse(data, len, delim, quote, flags);
	}

	buf = M_buf_create();

	/* Add header line to start of buffer. */
	for (i=0; i<M_list_str_len(headers); i++) {
		add_cell(buf, delim, quote, M_list_str_at(headers, i));
	}
	M_buf_truncate(buf, M_buf_len(buf) - 1); /* trim off last delim char */
	M_buf_add_str(buf, "\r\n");

	/* Add the rest of the table to the buffer. */
	M_buf_add_bytes(buf, data, len);

	/* Parse in-place. The returned CSV object takes ownership of 'out'. */
	out = M_buf_finish_str(buf, &out_len);
	return M_csv_parse_inplace(out, out_len, delim, quote, flags);
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


void M_csv_output_set_control_chars(M_csv_t *csv, char delim, char quote)
{
	if (csv == NULL) {
		return;
	}

	csv->quote = quote;
	csv->delim = delim;
}


void M_csv_output_headers_buf(M_buf_t *buf, const M_csv_t *csv, M_list_str_t *headers)
{
	size_t i;
	size_t ncols = M_csv_get_numcols(csv);
	size_t nhdrs = M_list_str_len(headers);

	if (nhdrs > 0) {
		for (i=0; i<nhdrs; i++) {
			add_cell(buf, csv->delim, csv->quote, M_list_str_at(headers, i));
		}
	} else {
		for (i=0; i<ncols; i++) {
			add_cell(buf, csv->delim, csv->quote, M_csv_get_header(csv, i));
		}
	}

	/* add_cell() always adds a trailing delimiter character, so we need to remove the
	 * trailing delimiter from the last cell in the row.
	 */
	M_buf_truncate(buf, M_buf_len(buf) - 1);

	/* CSV spec REQUIRES \r\n at end of each row, can't just use \n here. */
	M_buf_add_str(buf, "\r\n");
}


static const char *rewrite_cell(const char *cell, const char *header, M_buf_t **cellbuf,
	M_csv_cell_writer_cb writer_cb, void *thunk)
{
	if (writer_cb == NULL) {
		return cell;
	}

	if (*cellbuf == NULL) {
		*cellbuf = M_buf_create();
	}
	M_buf_truncate(*cellbuf, 0);

	if (writer_cb(*cellbuf, cell, header, thunk)) {
		return M_buf_peek(*cellbuf);
	}
	return cell;
}


void M_csv_output_rows_buf(M_buf_t *buf, const M_csv_t *csv, M_list_str_t *headers,
	M_csv_row_filter_cb filter_cb, void *filter_thunk, M_csv_cell_writer_cb writer_cb, void *writer_thunk)
{
	size_t      nrows   = M_csv_get_numrows(csv);
	size_t      ncols   = M_csv_get_numcols(csv);
	size_t      nhdrs   = M_list_str_len(headers);
	size_t      rowidx;
	size_t      i;
	const char *header;
	const char *cellval;
	M_buf_t    *cellbuf = NULL;

	for (rowidx=0; rowidx<nrows; rowidx++) {
		if (filter_cb != NULL && !filter_cb(csv, rowidx, filter_thunk)) {
			/* Skip this row, if the filter callback wants us to omit it. */
			continue;
		}

		if (nhdrs > 0) {
			/* If user passed in a list of headers, output only the columns they requested, in the
			 * same order that they listed them in.
			 */
			for (i=0; i<nhdrs; i++) {
				header  = M_list_str_at(headers, i);
				cellval = M_csv_get_cell(csv, rowidx, header);
				cellval = rewrite_cell(cellval, header, &cellbuf, writer_cb, writer_thunk);
				add_cell(buf, csv->delim, csv->quote, cellval);
			}
		} else {
			/* Otherwise, just use the headers as we originally parsed them from the CSV data. */
			for (i=0; i<ncols; i++) {
				header  = M_csv_get_header(csv, i);
				cellval = M_csv_get_cellbynum(csv, rowidx, i);
				cellval = rewrite_cell(cellval, header, &cellbuf, writer_cb, writer_thunk);
				add_cell(buf, csv->delim, csv->quote, cellval);
			}
		}

		/* add_cell() always adds a trailing delimiter character, so we need to remove the
		 * trailing delimiter from the last cell in the row.
		 */
		M_buf_truncate(buf, M_buf_len(buf) - 1);

		/* CSV spec requires \r\n at end of each row, can't just use \n here. */
		M_buf_add_str(buf, "\r\n");
	}

	M_buf_cancel(cellbuf);
}
