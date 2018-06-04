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

static M_bool read_strip_framing_eol(M_parser_t *parser)
{
	unsigned char byte;
	size_t        len;

	if (!M_parser_peek_byte(parser, &byte))
		return M_FALSE;

	/* Remove starting pipe. */
	if (byte == '|')
		M_parser_consume(parser, 1);

	if (M_parser_len(parser) == 0)
		return M_FALSE;

	/* Get the length so when we check the end characters we know
 	 * how much to truncate. */
	len = M_parser_len(parser);

	/* Check for \r */
	M_parser_mark(parser);
	M_parser_consume(parser, len - 1);
	if (!M_parser_peek_byte(parser, &byte))
		return M_FALSE;

	if (byte == '\r')
		len--;

	M_parser_mark_rewind(parser);

	/* Check for pipe. */
	M_parser_mark(parser);
	M_parser_consume(parser, len - 1);
	if (!M_parser_peek_byte(parser, &byte))
		return M_FALSE;

	if (byte == '\r')
		len--;

	M_parser_mark_rewind(parser);

	/* Truncate off pipe and \r if they are present. */
	if (len != M_parser_len(parser))
		M_parser_truncate(parser, len);

	if (M_parser_len(parser) == 0)
		return M_FALSE;

	return M_TRUE;
}

static M_parser_t **read_cols(M_parser_t *parser, size_t *num_cols)
{
	if (!read_strip_framing_eol(parser))
		return NULL;

	return M_parser_split(parser, '|', 0, M_PARSER_SPLIT_FLAG_NONE, num_cols);
}

static M_bool read_header(M_table_t *table, M_parser_t *parser)
{
	M_parser_t **cols     = NULL;
	char        *colname;
	size_t       num_cols = 0;
	size_t       i;

	cols = read_cols(parser, &num_cols);
	if (cols == NULL || num_cols == 0) {
		M_parser_split_free(cols, num_cols);
		return M_FALSE;
	}

	for (i=0; i<num_cols; i++) {
		/* Clear whitespace from the start and end since it's used for pretty printing
 		 * and not part of the data. */
		M_parser_consume_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);
		M_parser_truncate_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);

		colname = M_parser_read_strdup(cols[i], M_parser_len(cols[i]));
		M_table_column_insert(table, colname);
		M_free(colname);
	}

	M_parser_split_free(cols, num_cols);
	return M_TRUE;
}

static M_bool read_header_sep_line(M_parser_t *parser)
{
	M_parser_t    **cols     = NULL;
	unsigned char   byte;
	size_t          num_cols = 0;
	size_t          len;
	size_t          i;

 	/* We're not going to validate we the correct number of columns.
	 * We're only going to check that the format is correct.*/
	cols = read_cols(parser, &num_cols);
	if (cols == NULL || num_cols == 0) {
		M_parser_split_free(cols, num_cols);
		return M_FALSE;
	}

	for (i=0; i<num_cols; i++) {
		/* Eat whitespace starting and ending the cell. */
		M_parser_consume_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);
		M_parser_truncate_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);

		if (M_parser_len(cols[i]) == 0)
			goto err;

		/* Strip off the justify marker (:) if present. */
		if (!M_parser_peek_byte(cols[i], &byte))
			goto err;

		if (byte == ':')
			M_parser_consume(cols[i], 1);

		/* Check for three dashes (-). */
		len = M_parser_consume_str_charset(cols[i], "-");
		if (len < 3)
			goto err;

		/* Remove the justify marker (:) if present. */
		len = M_parser_consume_str_charset(cols[i], ":");
		if (len > 1)
			goto err;

		if (M_parser_len(cols[i]) != 0)
			goto err;
	}

	M_parser_split_free(cols, num_cols);
	return M_TRUE;

err:
	M_parser_split_free(cols, num_cols);
	return M_FALSE;
}

static M_bool read_data_line(M_table_t *table, M_parser_t *parser)
{
	M_parser_t **cols     = NULL;
	char        *data;
	size_t       num_cols = 0;
	size_t       rowidx;
	size_t       i;

	cols = read_cols(parser, &num_cols);
	if (cols == NULL || num_cols == 0) {
		M_parser_split_free(cols, num_cols);
		return M_FALSE;
	}

	/* Validate we don't have too many columns.
 	 * We're going to allow less. */
	if (num_cols > M_table_column_count(table)) {
		M_parser_split_free(cols, num_cols);
		return M_FALSE;
	}

	/* add the row. */
	rowidx = M_table_row_insert(table);

	/* Add the cell data to the row. */
	for (i=0; i<num_cols; i++) {
		/* Clear whitespace from the start and end since it's used for pretty printing
 		 * and not part of the data. */
		M_parser_consume_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);
		M_parser_truncate_whitespace(cols[i], M_PARSER_WHITESPACE_NONE);

		if (M_parser_len(cols[i]) == 0)
			continue;

		data = M_parser_read_strdup(cols[i], M_parser_len(cols[i]));
		M_table_cell_set_at(table, rowidx, i, data);
		M_free(data);
	}

	M_parser_split_free(cols, num_cols);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_u64_t *write_determine_cell_widths(const M_table_t *table)
{
	M_list_u64_t *cell_widths;
	const char   *const_temp;
	size_t        width;
	size_t        num_rows;
	size_t        num_cols;
	size_t        len;
	size_t        i;
	size_t        j;

	cell_widths = M_list_u64_create(M_LIST_U64_NONE);
	num_rows    = M_table_row_count(table);
	num_cols    = M_table_column_count(table);

	/* Go through the column names and get the longest name. */
	for (i=0; i<num_cols; i++) {
		/* Header sep line has a minimum of 3 characters so we can't be smaller. */
		width = 3;

		const_temp = M_table_column_name(table, i);
		len        = M_str_len(const_temp);
		if (len > width) {
			width = len;
		}

		M_list_u64_insert(cell_widths, (M_uint64)width);
	}

	/* Go though all cells and see which is largest. */
	for (i=0; i<num_rows; i++) {
		for (j=0; j<num_cols; j++) {
			const_temp = M_table_cell_at(table, i, j);
			width      = (size_t)M_list_u64_at(cell_widths, j);
			len        = M_str_len(const_temp);
			if (len > width) {
				M_list_u64_replace_at(cell_widths, (M_uint64)len, j);
			}
		}
	}

	return cell_widths;
}

static void write_line_start(M_buf_t *buf, M_uint32 flags)
{
	if (flags & M_TABLE_MARKDOWN_OUTERPIPE)
		M_buf_add_str(buf, "| ");
}

static void write_line_end(M_buf_t *buf, M_uint32 flags)
{
	/* Remove trailing separator (" | "). If we're adding outer
	 * pipes we'll leave the " |" because it's the outer pipe. */
	M_buf_truncate(buf, M_buf_len(buf)-1);
	if (!(flags & M_TABLE_MARKDOWN_OUTERPIPE))
		M_buf_truncate(buf, M_buf_len(buf)-2);

	if (flags & M_TABLE_MARKDOWN_LINEEND_WIN)
		M_buf_add_byte(buf, '\r');
	M_buf_add_byte(buf, '\n');
}

static void write_cell_padding(M_buf_t *buf, const char *data, const M_list_u64_t *cell_widths, size_t idx, M_uint32 flags)
{
	size_t width;
	size_t len;

	if (!(flags & M_TABLE_MARKDOWN_PRETTYPRINT))
		return;

	width = (size_t)M_list_u64_at(cell_widths, idx);
	len   = M_str_len(data);
	if (len < width) {
		M_buf_add_fill(buf, ' ', width-len);
	}
}


static void write_header(const M_table_t *table, M_buf_t *buf, const M_list_u64_t *cell_widths, M_uint32 flags)
{
	const char *const_temp;
	size_t      num_cols;
	size_t      i;

	write_line_start(buf, flags);

	num_cols = M_table_column_count(table);
	for (i=0; i<num_cols; i++) {
		/* Add the cell data. */
		const_temp = M_table_column_name(table, i);
		M_buf_add_str(buf, const_temp);

		/* Add cell padding if needed. */
		write_cell_padding(buf, const_temp, cell_widths, i, flags);

		/* Add the next separator. */
		M_buf_add_str(buf, " | ");
	}

	write_line_end(buf, flags);
}

static void write_header_sep_line(const M_table_t *table, M_buf_t *buf, const M_list_u64_t *cell_widths, M_uint32 flags)
{
	size_t num_cols;
	size_t width;
	size_t i;

	write_line_start(buf, flags);

	num_cols = M_table_column_count(table);
	for (i=0; i<num_cols; i++) {
		/* Add the line data. */
		width = (size_t)M_list_u64_at(cell_widths, i);
		if (width == 0)
			width = 3;
		M_buf_add_fill(buf, '-', width);

		/* Add the next separator. */
		M_buf_add_str(buf, " | ");
	}

	write_line_end(buf, flags);
}

static void write_data_lines(const M_table_t *table, M_buf_t *buf, const M_list_u64_t *cell_widths, M_uint32 flags)
{
	const char *const_temp;
	size_t      num_cols;
	size_t      num_rows;
	size_t      i;
	size_t      j;

	num_rows = M_table_row_count(table);
	num_cols = M_table_column_count(table);

	for (i=0; i<num_rows; i++) {
		write_line_start(buf, flags);

		for (j=0; j<num_cols; j++) {
			/* Add the cell data. */
			const_temp = M_table_cell_at(table, i, j);
			M_buf_add_str(buf, const_temp);

			/* Add cell padding if needed. */
			write_cell_padding(buf, const_temp, cell_widths, j, flags);

			/* Add the next separator. */
			M_buf_add_str(buf, " | ");
		}

		write_line_end(buf, flags);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_load_markdown(M_table_t *table, const char *data, size_t len)
{
	M_parser_t     *parser;
	M_parser_t    **rows     = NULL;
	size_t          num_rows = 0;
	size_t          i;

	parser = M_parser_create_const((unsigned char *)data, len, M_PARSER_FLAG_NONE);
	M_parser_consume_whitespace(parser, M_PARSER_WHITESPACE_NONE);
	M_parser_truncate_whitespace(parser, M_PARSER_WHITESPACE_NONE);

	rows   = M_parser_split(parser, '\n', 0, M_PARSER_SPLIT_FLAG_NONE, &num_rows);
	/* Must have 3 rows because tables cannot be empty. */
	if (rows == NULL || num_rows < 3) {
		M_parser_split_free(rows, num_rows);
		M_parser_destroy(parser);
		return M_FALSE;
	}

	/* Read the header. */
	if (!read_header(table, rows[0])) {
		M_parser_split_free(rows, num_rows);
		M_parser_destroy(parser);
		return M_FALSE;
	}

	/* Validate the line that separates the header from data. */
	if (!read_header_sep_line(rows[1])) {
		M_parser_split_free(rows, num_rows);
		M_parser_destroy(parser);
		return M_FALSE;
	}

	/* Parser the data. */
	for (i=2; i<num_rows; i++) {
		if (!read_data_line(table, rows[i])) {
			M_parser_split_free(rows, num_rows);
			M_parser_destroy(parser);
			return M_FALSE;
		}
	}

	M_parser_split_free(rows, num_rows);
	M_parser_destroy(parser);
	return M_TRUE;
}

char *M_table_write_markdown(const M_table_t *table, M_uint32 flags)
{
	M_buf_t      *buf;
	M_list_u64_t *cell_widths = NULL;

	if (M_table_column_count(table) == 0 || M_table_row_count(table) == 0)
		return NULL;

	/* When pretty printing every cell is the same width. Get the width
 	 * for the longest cell. This includes headers. */
	if (flags & M_TABLE_MARKDOWN_PRETTYPRINT)
		cell_widths = write_determine_cell_widths(table);

	buf = M_buf_create();

	write_header(table, buf, cell_widths, flags);
	write_header_sep_line(table, buf, cell_widths, flags);
	write_data_lines(table, buf, cell_widths, flags);

	/* remove the trailing newline(s). */
	M_buf_truncate(buf, M_buf_len(buf)-1);
	if (M_buf_peek(buf)[M_buf_len(buf)-1] == '\r')
		M_buf_truncate(buf, M_buf_len(buf)-1);

	M_list_u64_destroy(cell_widths);
	return M_buf_finish_str(buf, NULL);
}
