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

#ifndef __M_TABLE_H__
#define __M_TABLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_table Table
 *  \ingroup m_formats
 *
 * Generic table construction and manipulation.
 *
 * @{
 */

struct M_table;
typedef struct M_table M_table_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Flags controlling behavior of insert by column name operations. */
typedef enum {
	M_TABLE_INSERT_NONE      = 0,      /*!< Fail the insert if the header, or index does not exist. */
	M_TABLE_INSERT_COLADD    = 1 << 0, /*!< Add a named column if it does not exist. */
	M_TABLE_INSERT_COLIGNORE = 1 << 1  /*!< Ignore names if a corresponding named header does not exist. */
} M_table_insert_flags_t;


/*! Flags controlling table construction. */
typedef enum {
	M_TABLE_NONE            = 0,     /*!< Default operation. */
	M_TABLE_COLNAME_CASECMP = 1 << 0 /*!< Compare column names case insensitive. */
} M_table_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a table.
 *
 * \param[in] flags M_table_flags_t controlling behavior of the table.
 *
 * return Table.
 */
M_API M_table_t *M_table_create(M_uint32 flags) M_MALLOC;


/*! Destroy a table.
 *
 * \param[in] table Table.
 */
M_API void M_table_destroy(M_table_t *table) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert a new column into the table.
 *
 * \param[in] table   Table.
 * \param[in] colname Optional name associated with the column.
 *
 * \return M_TRUE when the column is successfully added. Otherwise, M_FALSE.
 */
M_API M_bool M_table_column_insert(M_table_t *table, const char *colname);


/*! Insert a new column into the table at a specified index.
 *
 * \param[in] table   Table.
 * \param[in] idx     Index to insert at. Cannot be larger than the number of columns (last idx+1).
 * \param[in] colname Optional name associated with the column.
 *
 * \return M_TRUE when the column is successfully added. Otherwise, M_FALSE.
 */
M_API M_bool M_table_column_insert_at(M_table_t *table, size_t idx, const char *colname);


/*! Get the name associated with a column.
 *
 * \param[in] table Table.
 * \param[in] idx   Column index.
 *
 * \return NULL if no name associated. Otherwise, name. Name can be an empty string is
 *         it was set to an empty string.
 */
M_API const char *M_table_column_name(const M_table_t *table, size_t idx);


/*! Associate a name with a column.
 *
 * \param[in] table   Table.
 * \param[in] idx     Column index.
 * \param[in] colname Name.
 *
 * \return M_TRUE on success. Otherwise, M_FALSE. Can fail if a column with the given name
 *         already exists.
 */
M_API M_bool M_table_column_set_name(M_table_t *table, size_t idx, const char *colname);


/*! Get the index for a column with a given name.
 *
 * \param[in]  table   Table.
 * \param[in]  colname Column name.
 * \param[out] idx     Index of column
 *
 * \return M_TRUE if the column exists. Otherwise, M_FALSE.
 */
M_API M_bool M_table_column_idx(const M_table_t *table, const char *colname, size_t *idx);


/*! Sort rows based on data in a given column name.
 *
 * Supports secondary column sorting when values in the primary column are equivalent.
 *
 * \param[in] table             Table.
 * \param[in] colname           Column name for primary sorting.
 * \param[in] primary_sort      Sort comparison function for `colname`.
 * \param[in] secondary_colname Column name for secondary sorting. Only used when values from
 *                              primary sort are equivalent.
 * \param[in] secondary_sort    Sort comparison function for `secondary_colname`.
 * \param[in] thunk             Thunk passed to comparison functions.
 */
M_API void M_table_column_sort_data(M_table_t *table, const char *colname, M_sort_compar_t primary_sort, const char *secondary_colname, M_sort_compar_t secondary_sort, void *thunk);


/*! Sort rows based on data in a given column index.
 *
 * \param[in] table          Table.
 * \param[in] idx            Column index used for sorting.
 * \param[in] primary_sort   Sort comparison function for `colname`.
 * \param[in] secondary_idx  Column index for secondary sorting. Only used when values from
 *                           primary sort are equivalent.
 * \param[in] secondary_sort Sort comparison function for `secondary_colname`.
 * \param[in] thunk          Thunk passed to comparison functions.
 */
M_API void M_table_column_sort_data_at(M_table_t *table, size_t idx, M_sort_compar_t primary_sort, size_t secondary_idx, M_sort_compar_t secondary_sort, void *thunk);


/*! Sort column based on names.
 *
 * It is not required for all columns to be named. Unnamed columns will
 * be passed to the sort function as an empty string ("").
 *
 * \param[in] table Table.
 * \param[in] sort  Sort comparison function.
 * \param[in] thunk Thunk passed to comparison function.
 */
M_API void M_table_column_order(M_table_t *table, M_sort_compar_t sort, void *thunk);


/*! Remove a column with a given name.
 *
 * \param[in] table   Table.
 * \param[in] colname Column name.
 */
M_API void M_table_column_remove(M_table_t *table, const char *colname);


/*! Remove a column at a given index.
 *
 * \param[in] table Table.
 * \param[in] idx   Column index.
 */
M_API void M_table_column_remove_at(M_table_t *table, size_t idx);


/*! Remove empty columns.
 *
 * A column is empty when no rows have data for that column.
 *
 * param[in] table Table.
 *
 * \return Number of columns removed.
 */
M_API size_t M_table_column_remove_empty_columns(M_table_t *table);


/*! Get the number of columns in the table.
 *
 * \param[in] table Table.
 *
 * \return Column count.
 */
M_API size_t M_table_column_count(const M_table_t *table);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Inset a row into the table.
 *
 * \param[in] table Table.
 *
 * \return Index the row was inserted at.
 */
M_API size_t M_table_row_insert(M_table_t *table);


/*! Insert a row into the table at a given index.
 *
 * \param[in] table Table.
 * \param[in] idx   Index to insert at. Cannot be larger than the number of rows (last idx+1).
 * 
 * \return M_TRUE if the row was inserted. Otherwise, M_FALSE.
 */
M_API M_bool M_table_row_insert_at(M_table_t *table, size_t idx);


/*! Insert data from a dict into the table.
 *
 * Dictionary key is the column name and the value is the cell value.
 *
 * \param[in] table Table.
 * \param[in] data  Data to insert.
 * \param[in] flags M_table_insert_flags_t flags controlling insert behavior. Specifically
 *                  handling of situations where the key in data is not a current column.
 *
 * \return M_TRUE if the row was inserted. Otherwise, M_FALSE.
 */
M_API M_bool M_table_row_insert_dict(M_table_t *table, const M_hash_dict_t *data, M_uint32 flags);


/*! Insert data from a dict into the table at a given idex.
 *
 * Dictionary key is the column name and the value is the cell value.
 *
 * \param[in] table Table.
 * \param[in] idx   Index to insert at. Cannot be larger than the number of rows (last idx+1).
 * \param[in] data  Data to insert.
 * \param[in] flags M_table_insert_flags_t flags controlling insert behavior. Specifically
 *                  handling of situations where the key in data is not a current column.
 *
 * \return M_TRUE if the row was inserted. Otherwise, M_FALSE.
 */
M_API M_bool M_table_row_insert_dict_at(M_table_t *table, size_t idx, const M_hash_dict_t *data, M_uint32 flags);


/*! Remove a row.
 *
 * \param[in] table Table.
 * \param[in] idx   Row index.
 */
M_API void M_table_row_remove(M_table_t *table, size_t idx);


/*! Remove all empty rows from the table.
 *
 * A row is considered empty if there is no data in any column.
 * An empty string is considered data and the row will not be removed.
 *
 * \param[in] table Table.
 *
 * \return Number of rows removed.
 */
M_API size_t M_table_row_remove_empty_rows(M_table_t *table);


/*! Get the number of rows in the table.
 *
 * \param[in] table Table.
 *
 * \return Number of rows.
 */
M_API size_t M_table_row_count(const M_table_t *table);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set data in a given cell by column name.
 *
 * \param[in] table   Table.
 * \param[in] row     Row index.
 * \param[in] colname Column name.
 * \param[in] val     Value to set. NULL will clear.
 * \param[in] flags   M_table_insert_flags_t flags controlling insert behavior. Specifically
 *                    handling of situations where the `colname` is not a current column.
 *
 * \return M_TRUE if the value was set. Otherwise, M_FALSE.
 */
M_API M_bool M_table_cell_set(M_table_t *table, size_t row, const char *colname, const char *val, M_uint32 flags);


/*! Set data in a given cell by index.
 *
 * \param[in] table Table.
 * \param[in] row   Row index.
 * \param[in] col   Column index.
 * \param[in] val   Value to set. NULL will clear.
 *
 * \return M_TRUE if the value was set. Otherwise, M_FALSE.
 */
M_API M_bool M_table_cell_set_at(M_table_t *table, size_t row, size_t col, const char *val);


/*! Clear the data from a cell by column name.
 *
 * This is the equivalent to calling `M_table_cell_set` with a NULL value.
 *
 * \param[in] table   Table.
 * \param[in] row     Row index.
 * \param[in] colname Column name.
 *
 * \return M_TRUE if the cell was cleared. Otherwise, M_FALSE. Clearing a column that does not
 *         exist is considered success.
 */
M_API M_bool M_table_cell_clear(M_table_t *table, size_t row, const char *colname);


/*! Clear the data from a cell by column index.
 *
 * This is the equivalent to calling `M_table_cell_set` with a NULL value.
 *
 * \param[in] table Table.
 * \param[in] row   Row index.
 * \param[in] col   Column index.
 *
 * \return M_TRUE if the cell was cleared. Otherwise, M_FALSE.
 */
M_API M_bool M_table_cell_clear_at(M_table_t *table, size_t row, size_t col);


/*! Get the data for a cell by column name.
 *
 * \param[in] table   Table.
 * \param[in] row     Row index.
 * \param[in] colname Column name.
 *
 * \return Cell value. NULL is returned if there is no cell value and on error.
 */
M_API const char *M_table_cell(const M_table_t *table, size_t row, const char *colname);


/*! Get the data for a cell by column index.
 *
 * \param[in] table Table.
 * \param[in] row   Row index.
 * \param[in] col   Column index.
 *
 * \return Cell value. NULL is returned if there is no cell value and on error.
 */
M_API const char *M_table_cell_at(const M_table_t *table, size_t row, size_t col);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate a table.
 *
 * \param[in] table Table.
 * 
 * \return Table.
 */
M_API M_table_t *M_table_duplicate(const M_table_t *table);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Load CSV formatted data into the table.
 *
 * \param[in] table       Table.
 * \param[in] data        CSV data.
 * \param[in] len         Length of data to load.
 * \param[in] delim       CSV delimiter character. Typically comma (",").
 * \param[in] quote       CSV quote character. Typically double quote (""").
 * \param[in] flags       M_CSV_FLAGS flags controlling parse behavior.
 * \param[in] have_header Whether the CSV data has a header.
 *
 * \return M_TRUE if the data was loaded. Otherwise, M_FALSE.
 */
M_API M_bool M_table_load_csv(M_table_t *table, const char *data, size_t len, char delim, char quote, M_uint32 flags, M_bool have_header);


/*! Write the table as CSV.
 *
 * \param[in] table        Table.
 * \param[in] delim        CSV delimiter character. Typically comma (",").
 * \param[in] quote        CSV quote character. Typically double quote (""").
 * \param[in] write_header Whether the column names should be written as the CSV header.
 *                         All columns should be named if writing a header. However, it is not an
 *                         error if there are unnamed columns.
 *
 * \return CSV data.
 */
M_API char *M_table_write_csv(const M_table_t *table, char delim, char quote, M_bool write_header);

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_TABLE_H__ */
