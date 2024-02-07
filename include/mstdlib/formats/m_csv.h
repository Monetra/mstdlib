/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

#ifndef __M_CSV_H__
#define __M_CSV_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_buf.h>
#include <mstdlib/base/m_list_str.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_csv CSV
 *  \ingroup m_formats
 * CSV Parser.
 *
 * RFC 4180 compliant CSV parser.
 *
 * The first row in the CSV is assumed to be the header. If there is no
 * header the *raw* functions should be used to reterive data. If there
 * is a header the non-raw functions should be used. These functions
 * take into account the header when indexing rows automatically. The
 * first row after the header is index 0.
 *
 * Example:
 *
 * \code{.c}
 *     const char *data = "header1,header1\ncell1,cell2"
 *     M_csv_t    *csv;
 *     const char *const_temp;
 *
 *     csv        = M_csv_parse(data, M_str_len(data), ',', '"', M_CSV_FLAG_NONE);
 *     const_temp = M_csv_get_header(csv, 0);
 *     M_printf("header='%s'\n", const_temp);
 *
 *     const_temp = M_csv_get_cellbynum(csv, 0, 1);
 *     M_printf("cell='%s'\n", const_temp);
 *
 *     M_csv_destroy(csv);
 * \endcode
 *
 * Example output:
 *
 * \code
 *     header='header1'
 *     cell='cell2'
 * \endcode
 *
 * @{
 */

struct M_csv;
typedef struct M_csv M_csv_t;

/*! Flags controlling parse behavior */
enum M_CSV_FLAGS {
    M_CSV_FLAG_NONE            = 0,     /*!< No Flags */
    M_CSV_FLAG_TRIM_WHITESPACE = 1 << 0 /*!< If a cell is not quoted, trim leading and trailing whitespace */
};

/*! Callback that can be used to filter rows from data returned by M_csv_output_rows_buf().
 *
 * \param[in] csv   the csv being output.
 * \param[in] row   the idx of the current row being considered (NOT raw - 0 is the first row after the header).
 * \param[in] thunk pointer to thunk object passed into M_csv_output_rows_buf() by caller.
 * \return          M_TRUE, if the row should be included in output. M_FALSE otherwise.
 */
typedef M_bool (*M_csv_row_filter_cb)(const M_csv_t *csv, size_t row, void *thunk);


/*! Callback that can be used to edit data from certain columns as it's written out.
 *
 * \param[in] buf    buffer to write new version of cell data to.
 * \param[in] cell   original cell data (may be empty/NULL, if cell was empty)
 * \param[in] header header of column this cell came from
 * \param[in] thunk  pointer to thunk object passed into M_csv_output_rows_buf() by caller.
 * \return           M_TRUE if we added a modified value to buf. M_FALSE if value was OK as-is.
 */
typedef M_bool (*M_csv_cell_writer_cb)(M_buf_t *buf, const char *cell, const char *header, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse a string into a CSV object.
 *
 * \param[in] data  The data to parse.
 * \param[in] len   The length of the data to parse.
 * \param[in] delim CSV delimiter character. Typically comma (",").
 * \param[in] quote CSV quote character. Typically double quote (""").
 * \param[in] flags Flags controlling parse behavior.
 *
 * \return CSV object.
 *
 * \see M_csv_destroy
 */
M_API M_csv_t *M_csv_parse(const char *data, size_t len, char delim, char quote, M_uint32 flags) M_MALLOC;


/*! Parse a string into a CSV object, using given column headers.
 *
 * Same as M_csv_parse, but add the given headers as the first row before parsing the data into the table.
 *
 * \param[in] data    The data to parse.
 * \param[in] len     The length of data to parse.
 * \param[in] delim   CSV delimiter character. Typically comma (',').
 * \param[in] quote   CSV quote character. Typically double quote ('"').
 * \param[in] flags   Flags controlling parse behavior.
 * \param[in] headers List of headers to add as first row of table.
 *
 * \return            CSV object
 */
M_API M_csv_t *M_csv_parse_add_headers(const char *data, size_t len, char delim, char quote, M_uint32 flags,
    M_list_str_t *headers);


/*! Parse a string into a CSV object.
 *
 * This will take ownership of the data passed in. The data must be valid for the life of the
 * returned CSV object and will be destroyed by the CSV object when the CSV object is destroyed.
 *
 * \param[in] data  The string to parse.
 * \param[in] len   The length of the data to parse.
 * \param[in] delim CSV delimiter character. Typically comma (",").
 * \param[in] quote CSV quote character. Typically double quote (""").
 * \param[in] flags Flags controlling parse behavior.
 *
 * \return CSV object.
 *
 * \see M_csv_destroy
 */
M_API M_csv_t *M_csv_parse_inplace(char *data, size_t len, char delim, char quote, M_uint32 flags) M_MALLOC_ALIASED;


/*! Destory a CSV object.
 *
 * \param[in] csv The csv.
 */
M_API void M_csv_destroy(M_csv_t *csv) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Raw getters if no headers used */

/*! Get the raw number of csv rows.
 *
 * This should be used when the CSV data does not contain a header.
 * This count will include the header as a row in the count.
 *
 * \param[in] csv The csv.
 *
 * \return The number of rows including the header as a row.
 *
 * \see M_csv_get_numrows
 */
M_API size_t M_csv_raw_num_rows(const M_csv_t *csv);


/*! Get the raw number of csv columns.
 *
 * This should be used when the CSV data does not contain a header.
 *
 * \param[in] csv The csv.
 *
 * \return The number of columns.
 *
 * \see M_csv_get_numcols
 */
M_API size_t M_csv_raw_num_cols(const M_csv_t *csv);


/*! Get the cell at the given position.
 *
 * This should be used when the CSV data does not contain a header.
 * This assumes that the first row is data (not the header).
 *
 * \param[in] csv The csv.
 * \param[in] row The row. Indexed from 0 where 0 is the header (if there is a header).
 * \param[in] col The column. Indexed from 0.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 *
 * \see M_csv_get_cellbynum
 */
M_API const char *M_csv_raw_cell(const M_csv_t *csv, size_t row, size_t col);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Getters if headers used (default) */

/*! Get the number of csv rows.
 *
 * This should be used when the CSV data contains a header.
 * This count will not include the header as a row in the count.
 *
 * \param[in] csv The csv.
 *
 * \return The number of rows excluding the header as a row.
 *
 * \see M_csv_raw_num_rows
 */
M_API size_t M_csv_get_numrows(const M_csv_t *csv);


/*! Get the raw number of csv columns.
 *
 * This should be used when the CSV data contains a header.
 *
 * \param[in] csv The csv.
 *
 * \return The number of columns.
 *
 * \see M_csv_raw_num_cols
 */
M_API size_t M_csv_get_numcols(const M_csv_t *csv);


/*! Get the cell at the given position.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv The csv.
 * \param[in] row The row. Indexed from 0 where 0 is the first row after the header.
 * \param[in] col The column. Indexed from 0.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 *
 * \see M_csv_raw_cell
 */
M_API const char *M_csv_get_cellbynum(const M_csv_t *csv, size_t row, size_t col);


/*! Get the header for a given column
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv The csv.
 * \param[in] col The column. Indexed from 0.
 *
 * \return The header for the given column.
 */
M_API const char *M_csv_get_header(const M_csv_t *csv, size_t col);


/*! Get the cell at the for the given header.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv     The csv.
 * \param[in] row     The row. Indexed from 0 where 0 is the first row after the header.
 * \param[in] colname The column name to get the data from.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 */
M_API const char *M_csv_get_cell(const M_csv_t *csv, size_t row, const char *colname);


/*! Get the column number for a given column (header) name.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv     The csv.
 * \param[in] colname The column name to get the data from.
 *
 * \return Column number for the given name on success. Otherwise -1.
 */
M_API ssize_t M_csv_get_cell_num(const M_csv_t *csv, const char *colname);


/*! Use different delim and quote characters for output than for parsing.
 *
 * By default, M_csv_output_headers_buf() and M_csv_output_rows_buf() will use the same
 * delimiter and quote characters that were used when parsing the data.
 *
 * However, if you need to use a different delimiter and/or quote character in your
 * output, call this function first to change them.
 *
 * \param csv   The csv.
 * \param delim delimiter char to use in subsequent write operations
 * \param quote quote char to use in subsequent write operations
 */
void M_csv_output_set_control_chars(M_csv_t *csv, char delim, char quote);


/*! Write the header row, in CSV format.
 *
 * When outputting CSV data, this should be called first, with the exact same list of headers
 * that you'll be using later with M_csv_output_rows_buf().
 *
 * If \a headers is NULL, all headers defined in the CSV data will be output, in the same order
 * they were originally stored in.
 *
 * \see M_csv_output_rows_buf()
 *
 * \param[out] buf     buffer to place output in.
 * \param[in]  csv     the CSV data to output.
 * \param[in]  headers names of columns to include in header row (will be written in this exact order).
 */
M_API void M_csv_output_headers_buf(M_buf_t *buf, const M_csv_t *csv, M_list_str_t *headers);


/*! Write the parsed data to the given buffer, in CSV format.
 *
 * If \a headers is not NULL, only the columns whose names match will be output, in the same order
 * that the column headers are listed in \a headers. If there are names in \a headers which aren't
 * present in the parsed CSV file, an empty value will be added for that column in every row.
 *
 * A filter callback may be used to omit certain rows from the output. If no filter callback is
 * provided, all rows will be output.
 *
 * \see M_csv_output_headers_buf()
 *
 * \param[out] buf          buffer to place output in
 * \param[in]  csv          the CSV data to output.
 * \param[in]  headers      names of columns to include in output (also controls column order).
 * \param[in]  filter_cb    callback to control which rows are output (may be NULL).
 * \param[in]  filter_thunk pointer to pass to \a filter_cb (may be NULL).
 * \param[in]  writer_cb    callback to allow editing cell values (may be NULL).
 * \param[in]  writer_thunk pointer to pass to \a writer_cb (may be NULL).
 */
M_API void M_csv_output_rows_buf(M_buf_t *buf, const M_csv_t *csv, M_list_str_t *headers,
    M_csv_row_filter_cb filter_cb, void *filter_thunk, M_csv_cell_writer_cb writer_cb, void *writer_thunk);

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_CSV_H__ */
