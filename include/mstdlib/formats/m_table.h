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
 * Generic table construction.
 *
 * Notes:
 *   - Headers optional.
 *   - Expand flag will allow adding data without setting explicit table size.
 *
 * @{
 */

struct M_table;
typedef struct M_table M_table_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_TABLE_INSERT_NONE      = 0,      /*!< Fail the insert if the header, or index does not exist. */
	M_TABLE_INSERT_COLADD    = 1 << 0, /*!< Add a named column if it does not exist. */
	M_TABLE_INSERT_COLIGNORE = 1 << 1  /*!< Ignore names if a corresponding named header does not exist. */
} M_table_insert_flags_t;

typedef enum {
	M_TABLE_NONE            = 0,     /*!< Default operation. */
	M_TABLE_COLNAME_CASECMP = 1 << 0 /*!< Compare column names case insensitive. */
} M_table_flags_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_table_t *M_table_create(M_uint32 flags) M_MALLOC;
void M_table_destroy(M_table_t *table) M_FREE(1);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Name is optional */
M_bool M_table_column_insert(M_table_t *table, const char *colname);
M_bool M_table_column_insert_at(M_table_t *table, size_t idx, const char *colname);

const char *M_table_column_name(M_table_t *table, size_t idx);
M_bool M_table_column_set_name(M_table_t *table, size_t idx, const char *colname);
M_bool M_table_column_idx(M_table_t *table, const char *colname, size_t *idx);
void M_table_column_sort_data(M_table_t *table, const char *colname, M_sort_compar_t primary_sort, M_sort_compar_t secondary_sort, void *thunk);
void M_table_column_sort_data_at(M_table_t *table, size_t idx, M_sort_compar_t primary_sort, M_sort_compar_t secondary_sort, void *thunk);
void M_table_column_order(M_table_t *table, M_sort_compar_t sort, void *thunk);
void M_table_column_remove(M_table_t *table, const char *colname);
void M_table_column_remove_at(M_table_t *table, size_t idx);
size_t M_table_column_count(M_table_t *table);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_table_row_insert(M_table_t *table);
M_bool M_table_row_insert_at(M_table_t *table, size_t idx);
M_bool M_table_row_insert_dict(M_table_t *table, const M_hash_dict_t *data, M_uint32 flags);
M_bool M_table_row_insert_dict_at(M_table_t *table, size_t idx, const M_hash_dict_t *data, M_uint32 flags);
void M_table_row_remove(M_table_t *table, size_t idx);
size_t M_table_row_count(M_table_t *table);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_cell_insert(M_table_t *table, size_t row, const char *colname, const char *val, M_uint32 flags);
M_bool M_table_cell_insert_at(M_table_t *table, size_t row, size_t col, const char *val);
M_bool M_table_cell_clear(M_table_t *table, size_t row, const char *colname);
M_bool M_table_cell_clear_at(M_table_t *table, size_t row, size_t col);
const char *M_table_cell(M_table_t *table, size_t row, const char *colname);
const char *M_table_cell_at(M_table_t *table, size_t row, size_t col);

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_TABLE_H__ */
