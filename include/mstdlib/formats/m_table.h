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
	M_TABLE_INSERT_NONE         = 0,      /*!< Fail the insert if the header, or index does not exist. */
	M_TABLE_INSERT_EXPAND       = 1 << 0, /*!< Add the header if it does not exist. If by index, add cell and
	                                           any empty cells before the index as needed. */
	M_TABLE_INSERT_IGNOREHEADER = 1 << 1  /*!< Ignore the header if it does not exist. */
} M_table_insert_flags_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_table_t *M_table_create(void) M_MALLOC;
void M_table_destroy(M_table_t *table) M_FREE;

/* Always expands. */
void M_table_header_insert(M_table_t *table, const char *name);
void M_table_header_insert_at(M_table_t *table, const char *name, size_t idx);
const char *M_table_header_at(M_table_t *table, size_t idx);
M_bool M_table_header_remove_at(M_table_t *table, size_t idx);
size_t M_table_header_count(M_table_t *table);

void M_table_row_insert(M_table_t *table);
M_bool M_table_row_insert_at(M_table_t *table, size_t idx, M_uint32 flags);
M_bool M_table_row_insert_dict(M_table_t *table, const M_hash_dict_t *d, M_uint32 flags);
M_hash_dict_t *M_table_row_dict(M_table_t *table, size_t idx);
size_t M_table_row_count(M_table_t *table);
void M_table_row_set_count(M_table_t *table, size_t count);

void M_table_column_insert(M_table_t *table);
M_bool M_table_column_insert_at(M_table_t *table, size_t idx, M_uint32 flags);
size_t M_table_column_count(M_table_t *table);
void M_table_column_set_count(M_table_t *table, size_t num);

M_bool M_table_cell_insert(M_table_t *table, size_t row, const char *header, const char *val, M_uint32 flags);
M_bool M_table_cell_insert_idx(M_table_t *table, size_t row, size_t col, const char *val, M_uint32 flags);

M_bool M_table_cell_remove(M_table_t *table, size_t row, const char *header);
M_bool M_table_cell_remove_idx(M_table_t *table, size_t row, size_t col);

const char *M_table_cell_at(M_table_t *table, size_t row, const char *header);
const char *M_table_cell_at_idx(M_table_t *table, size_t row, size_t col);

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_TABLE_H__ */
