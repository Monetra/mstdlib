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

struct M_table {
	M_list_u64_t    *col_order;            /* List of column ids. */
	M_hash_u64str_t *col_id_name;          /* Column ids -> column names. */
	M_hash_stru64_t *col_name_id;          /* Column name -> column id. */

	M_list_u64_t    *row_order;            /* List of row ids. */
	M_hash_u64vp_t  *rows;                 /* Row id -> M_hash_u64str_t (column id -> value) */

	M_rand_t        *rand;                 /* Used for generating ids. */
	M_uint32         flags;                /* Flags from creation. */

	M_sort_compar_t  primary_sort;         /* Primary sorting function when sorting. */
	M_sort_compar_t  secondary_sort;       /* Secondary sorting function when sorting. */
	void            *sort_thunk;           /* Thunk passed to sort functions. */
	M_uint64         sort_colid;           /* Column id that row data should be sorted on. */
	M_uint64         secondary_sort_colid; /* Column id that row data should be secondarily sorted on. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int table_colname_compar(const void *arg1, const void *arg2, void *thunk)
{
	M_table_t  *table = thunk;
	const char *v1    = NULL;
	const char *v2    = NULL;
	M_uint64    id1   = 0;
	M_uint64    id2   = 0;

	/* Get the column ids. */
	if (arg1 != NULL)
		id1 = *((M_uint64 const *)arg1);
	if (arg2 != NULL)
		id2 = *(M_uint64 const *)arg2;

	/* Get the values that are actually going to be compared from the
 	 * ids that have been sent in. */
	if (!M_hash_u64str_get(table->col_id_name, id1, &v1))
		v1 = "";
	if (!M_hash_u64str_get(table->col_id_name, id2, &v2))
		v2 = "";

	/* Sort based on the column name. */
	return table->primary_sort(&v1, &v2, table->sort_thunk);
}

static int table_coldata_compar(const void *arg1, const void *arg2, void *thunk)
{
	M_table_t       *table = thunk;
	M_hash_u64str_t *d1;
	M_hash_u64str_t *d2;
	const char      *v1    = NULL;
	const char      *v2    = NULL;
	M_uint64         id1   = 0;
	M_uint64         id2   = 0;
	int              ret;

	/* Get the rowids that have been sent in. */
	if (arg1 != NULL)
		id1 = *((M_uint64 const *)arg1);
	if (arg2 != NULL)
		id2 = *(M_uint64 const *)arg2;

	/* Get the row data for each rowid. */
	d1 = M_hash_u64vp_get_direct(table->rows, id1);
	d2 = M_hash_u64vp_get_direct(table->rows, id2);

	/* Get the values that are actually going to be compared from the
 	 * ids that have been sent in. */
	if (!M_hash_u64str_get(d1, table->sort_colid, &v1))
		v1 = "";
	if (!M_hash_u64str_get(d2, table->sort_colid, &v2))
		v2 = "";

	/* Sort based on the column name. */
	ret = table->primary_sort(&v1, &v2, table->sort_thunk);

	/* If they're the same run a secondary sort if present. */
	if (ret == 0 && table->secondary_sort != NULL) {
		/* Get the value for the secondary column. */
		if (!M_hash_u64str_get(d1, table->secondary_sort_colid, &v1))
			v1 = "";
		if (!M_hash_u64str_get(d2, table->secondary_sort_colid, &v2))
			v2 = "";

		/* Sort the secondary column. */
		ret = table->secondary_sort(&v1, &v2, table->sort_thunk);
	}
	return ret;
}

static M_uint64 generate_id(M_table_t *table, M_bool is_cols)
{
	M_uint64 id;

	do {
		id = M_rand(table->rand);
	} while (id != 0 && ((is_cols && M_hash_u64str_get(table->col_id_name, id, NULL)) || (!is_cols && M_hash_u64vp_get(table->rows, id, NULL))));

	return id;
}

static void M_table_column_sort_data_int(M_table_t *table, M_uint64 colid, M_sort_compar_t primary_sort, M_uint64 secondary_colid, M_sort_compar_t secondary_sort, void *thunk)
{
	M_uint64 *rowids;
	size_t    len;
	size_t    i;

	if (table == NULL)
		return;

	/* Copy the ids into a sortable array. */
	len    = M_list_u64_len(table->row_order);
	rowids = M_malloc_zero(len * sizeof(rowids[0]));
	for (i=0; i<len; i++) {
		rowids[i] = M_list_u64_at(table->row_order, i);
	}

	/* Sort. */
	table->primary_sort = primary_sort;
	if (table->primary_sort == NULL) {
		if (table->flags & M_TABLE_COLNAME_CASECMP) {
			table->primary_sort = M_sort_compar_str_casecmp;
		} else {
			table->primary_sort = M_sort_compar_str;
		}
	}
	table->secondary_sort = secondary_sort;

	table->sort_thunk           = thunk;
	table->sort_colid           = colid;
	table->secondary_sort_colid = secondary_colid;

	M_sort_qsort(rowids, len, sizeof(rowids[0]), table_coldata_compar, table);

	table->primary_sort         = NULL;
	table->secondary_sort       = NULL;
	table->sort_thunk           = NULL;
	table->sort_colid           = 0;
	table->secondary_sort_colid = 0;

	/* Copy the ids in the now sorted order back into the table list. */
	M_list_u64_destroy(table->row_order);
	table->row_order = M_list_u64_create(M_LIST_U64_NONE);
	for (i=0; i<len; i++) {
		M_list_u64_insert(table->row_order, rowids[i]);
	}

	M_free(rowids);
}

static void M_table_column_remove_int(M_table_t *table, M_uint64 colid)
{
	M_hash_u64vp_enum_t *he;
	M_hash_u64str_t     *row_data;
	const char          *colname;

	if (table == NULL)
		return;

	/* Remove the column for the list of columns. */
	if (M_list_u64_remove_val(table->col_order, colid, M_LIST_U64_MATCH_VAL) == 0)
	   return;	

	/* Remove the column name id mapping. */
	colname = M_hash_u64str_get_direct(table->col_id_name, colid);
	M_hash_stru64_remove(table->col_name_id, colname);
	M_hash_u64str_remove(table->col_id_name, colid);

	/* Go though each row and remove the column data. */
	M_hash_u64vp_enumerate(table->rows, &he);
	while (M_hash_u64vp_enumerate_next(table->rows, he, NULL, (void *)&row_data)) {
		M_hash_u64str_remove(row_data, colid);
	}
	M_hash_u64vp_enumerate_free(he);
}

static M_bool M_table_column_insert_at_int(M_table_t *table, size_t idx, const char *colname, M_uint64 *colid)
{
	M_uint64 mycolid;

	if (colid == NULL)
		colid = &mycolid;

	if (table == NULL || idx > M_list_u64_len(table->col_order))
		return M_FALSE;

	if (!M_str_isempty(colname)) {
		if (M_hash_stru64_get(table->col_name_id, colname, NULL)) {
			return M_FALSE;
		}
	}

	*colid = generate_id(table, M_TRUE);
	M_list_u64_insert_at(table->col_order, *colid, idx);

	if (!M_str_isempty(colname)) {
		M_hash_u64str_insert(table->col_id_name, *colid, colname);
		M_hash_stru64_insert(table->col_name_id, colname, *colid);
	}

	return M_TRUE;
}

static M_bool M_table_row_insert_at_int(M_table_t *table, size_t idx, M_uint64 *rowid)
{
	M_uint64 myrowid;

	if (rowid == NULL)
		rowid = &myrowid;

	if (table == NULL || idx > M_list_u64_len(table->row_order))
		return M_FALSE;

	*rowid = generate_id(table, M_FALSE);
	M_list_u64_insert_at(table->row_order, *rowid, idx);

	return M_TRUE;
}

static void M_table_cell_set_int(M_table_t *table, M_uint64 rowid, M_uint64 colid, const char *val)
{
	M_hash_u64str_t *row_data;

	if (!M_hash_u64vp_get(table->rows, rowid, (void **)&row_data)) {
		row_data = M_hash_u64str_create(8, 75, M_HASH_U64STR_NONE);
		M_hash_u64vp_insert(table->rows, rowid, row_data);
	}

	if (val == NULL) {
		M_hash_u64str_remove(row_data, colid);
	} else {
		M_hash_u64str_insert(row_data, colid, val);
	}
}

static const char *M_table_cell_get_int(const M_table_t *table, M_uint64 rowid, M_uint64 colid)
{
	M_hash_u64str_t *row_data;

	if (!M_hash_u64vp_get(table->rows, rowid, (void **)&row_data))
		return NULL;
	return M_hash_u64str_get_direct(row_data, colid);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_table_t *M_table_create(M_uint32 flags)
{
	M_table_t *table;
	M_uint32   pflags;

	table = M_malloc_zero(sizeof(*table));

	/* Columns */
	table->col_order = M_list_u64_create(M_LIST_U64_NONE);
	table->col_id_name = M_hash_u64str_create(8, 75, M_HASH_U64STR_NONE);

	pflags = M_HASH_STRU64_NONE;
	if (flags & M_TABLE_COLNAME_CASECMP)
		pflags |= M_HASH_STRU64_CASECMP;
	table->col_name_id = M_hash_stru64_create(8, 75, pflags);

	/* Rows */
	table->row_order = M_list_u64_create(M_LIST_U64_NONE);
	table->rows      = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void (*)(void *))M_hash_u64str_destroy);

	/* Other. */
	table->rand  = M_rand_create(0);
	table->flags = flags;

	return table;
}

void M_table_destroy(M_table_t *table)
{
	if (table == NULL)
		return;

	M_list_u64_destroy(table->col_order);
	M_hash_u64str_destroy(table->col_id_name);
	M_hash_stru64_destroy(table->col_name_id);
	M_list_u64_destroy(table->row_order);
	M_hash_u64vp_destroy(table->rows, M_TRUE);

	M_rand_destroy(table->rand);

	M_free(table);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_column_insert(M_table_t *table, const char *colname)
{
	if (table == NULL)
		return M_FALSE;
	return M_table_column_insert_at(table, M_list_u64_len(table->col_order), colname);
}

M_bool M_table_column_insert_at(M_table_t *table, size_t idx, const char *colname)
{
	return M_table_column_insert_at_int(table, idx, colname, NULL);
}

const char *M_table_column_name(const M_table_t *table, size_t idx)
{
	M_uint64 colid;

	if (table == NULL || idx >= M_list_u64_len(table->col_order))
		return NULL;

	colid = M_list_u64_at(table->col_order, idx);
	return M_hash_u64str_get_direct(table->col_id_name, colid);
}

M_bool M_table_column_set_name(M_table_t *table, size_t idx, const char *colname)
{
	M_uint64 colid;

	if (table == NULL || idx >= M_list_u64_len(table->col_order))
		return M_FALSE;

	colid = M_list_u64_at(table->col_order, idx);

	if (M_str_isempty(colname)) {
		M_hash_u64str_remove(table->col_id_name, colid);
		M_hash_stru64_remove(table->col_name_id, colname);
	} else {
		M_hash_u64str_insert(table->col_id_name, colid, colname);
		M_hash_stru64_insert(table->col_name_id, colname, colid);
	}

	return M_TRUE;
}

M_bool M_table_column_idx(const M_table_t *table, const char *colname, size_t *idx)
{
	M_uint64 colid;

	if (table == NULL)
		return M_FALSE;

	if (!M_hash_stru64_get(table->col_name_id, colname, &colid))
		return M_FALSE;

	return M_list_u64_index_of(table->col_order, colid, idx);
}

void M_table_column_sort_data(M_table_t *table, const char *colname, M_sort_compar_t primary_sort, const char *secondary_colname, M_sort_compar_t secondary_sort, void *thunk)
{
	M_uint64 colid           = 0;
	M_uint64 secondary_colid = 0;

	if (table == NULL || M_str_isempty(colname))
		return;

	if (!M_hash_stru64_get(table->col_name_id, colname, &colid))
		return;

	if (!M_str_isempty(secondary_colname)) {
		if (!M_hash_stru64_get(table->col_name_id, secondary_colname, &secondary_colid)) {
			return;
		}
	}

	M_table_column_sort_data_int(table, colid, primary_sort, secondary_colid, secondary_sort, thunk);
}

void M_table_column_sort_data_at(M_table_t *table, size_t idx, M_sort_compar_t primary_sort, size_t secondary_idx, M_sort_compar_t secondary_sort, void *thunk)
{
	M_uint64 colid;
	M_uint64 secondary_colid;

	if (table == NULL || idx <= M_list_u64_len(table->col_order) || secondary_idx <= M_list_u64_len(table->col_order))
		return;

	colid           = M_list_u64_at(table->col_order, idx);
	secondary_colid = M_list_u64_at(table->col_order, secondary_idx);
	M_table_column_sort_data_int(table, colid, primary_sort, secondary_idx, secondary_sort, thunk);
}

void M_table_column_order(M_table_t *table, M_sort_compar_t sort, void *thunk)
{
	M_uint64 *colids;
	size_t    len;
	size_t    i;

	if (table == NULL || sort == NULL)
		return;

	/* Copy the ids into a sortable array. */
	len    = M_list_u64_len(table->col_order);
	colids = M_malloc_zero(len * sizeof(colids[0]));
	for (i=0; i<len; i++) {
		colids[i] = M_list_u64_at(table->col_order, i);
	}

	/* Sort. */
	table->primary_sort = sort;
	if (table->primary_sort == NULL)
		table->primary_sort = M_sort_compar_str;

	table->secondary_sort = NULL;
	table->sort_thunk     = thunk;
	M_sort_qsort(colids, len, sizeof(colids[0]), table_colname_compar, table);
	table->primary_sort   = NULL;
	table->secondary_sort = NULL;
	table->sort_thunk     = NULL;

	/* Copy the ids in the now sorted order back into the table list. */
	M_list_u64_destroy(table->col_order);
	table->col_order = M_list_u64_create(M_LIST_U64_NONE);
	for (i=0; i<len; i++) {
		M_list_u64_insert(table->col_order, colids[i]);
	}

	M_free(colids);
}

void M_table_column_remove(M_table_t *table, const char *colname)
{
	M_uint64 colid;

	if (table == NULL || M_str_isempty(colname))
		return;

	if (!M_hash_stru64_get(table->col_name_id, colname, &colid))
		return;

	M_table_column_remove_int(table, colid);
}

void M_table_column_remove_at(M_table_t *table, size_t idx)
{
	M_uint64 colid;

	if (table == NULL || idx >= M_list_u64_len(table->col_order))
		return;

	colid = M_list_u64_at(table->col_order, idx);
	M_table_column_remove_int(table, colid);
}

size_t M_table_column_remove_empty_columns(M_table_t *table)
{
	M_hash_u64vp_enum_t *he;
	M_hash_u64str_t     *row_data;
	const char          *colname;
	M_uint64             colid;
	size_t               len;
	size_t               i;
	size_t               cnt = 0;
	M_bool               have;

	if (table == NULL)
		return 0;

	len = M_list_u64_len(table->col_order);
	for (i=len; i-->0; ) {
		colid = M_list_u64_at(table->col_order, i);
		have  = M_FALSE;

		M_hash_u64vp_enumerate(table->rows, &he);
		while (M_hash_u64vp_enumerate_next(table->rows, he, NULL, (void **)&row_data)) {
			if (M_hash_u64str_get(row_data, colid, NULL)) {
				have = M_TRUE;
				break;
			}
		}
		M_hash_u64vp_enumerate_free(he);

		if (!have) {
			M_list_u64_remove_at(table->col_order, i);
			colname = M_hash_u64str_get_direct(table->col_id_name, colid);
			M_hash_stru64_remove(table->col_name_id, colname);
			M_hash_u64str_remove(table->col_id_name, colid);
			cnt++;
		}
	}

	return cnt;
}

size_t M_table_column_count(const M_table_t *table)
{
	if (table == NULL)
		return 0;
	return M_list_u64_len(table->col_order);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_table_row_insert(M_table_t *table)
{
	size_t len;

	if (table == NULL)
		return 0;

	len = M_list_u64_len(table->row_order);
	M_table_row_insert_at(table, len);
	return len;
}

M_bool M_table_row_insert_at(M_table_t *table, size_t idx)
{
	return M_table_row_insert_at_int(table, idx, NULL);
}

M_bool M_table_row_insert_dict(M_table_t *table, const M_hash_dict_t *data, M_uint32 flags, size_t *idx)
{
	size_t rowidx;

	if (idx == NULL)
		idx = &rowidx;

	if (table == NULL)
		return M_FALSE;

	*idx = M_list_u64_len(table->row_order);
	return M_table_row_insert_dict_at(table, *idx, data, flags);
}

M_bool M_table_row_insert_dict_at(M_table_t *table, size_t idx, const M_hash_dict_t *data, M_uint32 flags)
{
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;
	M_hash_u64str_t    *row_data;
	M_uint64            colid;
	M_uint64            rowid;

	if (table == NULL || idx > M_list_u64_len(table->row_order))
		return M_FALSE;

	/* If the dict is empty it's an empty row being inserted. */
	if (data == NULL || M_hash_dict_num_keys(data) == 0)
		return M_table_row_insert_at(table, idx);

	/* Put all the data into a row object. */
	row_data = M_hash_u64str_create(8, 75, M_HASH_U64STR_NONE);
	M_hash_dict_enumerate(data, &he);
	while (M_hash_dict_enumerate_next(data, he, &key, &val)) {
		if (!M_hash_stru64_get(table->col_name_id, key, &colid)) {
			/* handle columns that don't exist based on insert flags. */
			if (flags & M_TABLE_INSERT_COLIGNORE) {
				continue;
			} else if (flags & M_TABLE_INSERT_COLADD) {
				if (!M_table_column_insert_at_int(table, M_list_u64_len(table->col_order), key, &colid)) {
					M_hash_dict_enumerate_free(he);
					M_hash_u64str_destroy(row_data);
					return M_FALSE;
				}
			} else {
				M_hash_dict_enumerate_free(he);
				M_hash_u64str_destroy(row_data);
				return M_FALSE;
			}
		}
		/* Add the data to the row */
		M_hash_u64str_insert(row_data, colid, val);
	}
	M_hash_dict_enumerate_free(he);

	/* Add our row to the table. */
	if (!M_table_row_insert_at_int(table, idx, &rowid)) {
		M_hash_u64str_destroy(row_data);
		return M_FALSE;
	}
	M_hash_u64vp_insert(table->rows, rowid, row_data);

	return M_TRUE;
}

void M_table_row_remove(M_table_t *table, size_t idx)
{
	M_uint64 rowid;

	if (table == NULL || idx >= M_list_u64_len(table->row_order))
		return;

	rowid = M_list_u64_at(table->row_order, idx);
	M_list_u64_remove_at(table->row_order, idx);
	M_hash_u64vp_remove(table->rows, rowid, M_TRUE);
}

size_t M_table_row_remove_empty_rows(M_table_t *table)
{
	M_hash_u64str_t *row_data;
	M_uint64         rowid;
	size_t           len;
	size_t           i;
	size_t           cnt = 0;

	if (table == NULL)
		return 0;

	len = M_list_u64_len(table->row_order);
	for (i=len; i-->0; ) {
		rowid = M_list_u64_at(table->row_order, i);
		row_data = M_hash_u64vp_get_direct(table->rows, rowid);

		if (row_data == NULL || M_hash_u64str_num_keys(row_data) == 0) {
			M_list_u64_remove_at(table->row_order, i);
			M_hash_u64vp_remove(table->rows, rowid, M_TRUE);
			cnt++;
		}
	}

	return cnt;
}

size_t M_table_row_count(const M_table_t *table)
{
	if (table == NULL)
		return 0;
	return M_list_u64_len(table->row_order);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_cell_set(M_table_t *table, size_t row, const char *colname, const char *val, M_uint32 flags)
{
	M_uint64 colid;
	M_uint64 rowid;

	if (table == NULL || row >= M_list_u64_len(table->row_order) || M_str_isempty(colname))
		return M_FALSE;

	rowid = M_list_u64_at(table->row_order, row);
	if (!M_hash_stru64_get(table->col_name_id, colname, &colid)) {
		if (flags & M_TABLE_INSERT_COLIGNORE) {
			return M_TRUE;
		} else if (flags & M_TABLE_INSERT_COLADD) {
			if (!M_table_column_insert_at_int(table, M_list_u64_len(table->col_order), colname, &colid)) {
				return M_FALSE;
			}
		} else {
			return M_FALSE;
		}
	}

	M_table_cell_set_int(table, rowid, colid, val);
	return M_TRUE;
}

M_bool M_table_cell_set_at(M_table_t *table, size_t row, size_t col, const char *val)
{
	M_uint64 colid;
	M_uint64 rowid;

	if (table == NULL || row >= M_list_u64_len(table->row_order) || col >= M_list_u64_len(table->col_order))
		return M_FALSE;

	rowid = M_list_u64_at(table->row_order, row);
	colid = M_list_u64_at(table->col_order, col);

	M_table_cell_set_int(table, rowid, colid, val);
	return M_TRUE;
}

M_bool M_table_cell_set_dict(M_table_t *table, size_t row, const M_hash_dict_t *data, M_uint32 flags)
{
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;

	/* Validate the row flags. We don't want to start adding anything if
 	 * we're supposed to fail on missing column. */
	if (!(flags & (M_TABLE_INSERT_COLIGNORE|M_TABLE_INSERT_COLADD))) {
		M_hash_dict_enumerate(data, &he);
		while (M_hash_dict_enumerate_next(data, he, &key, NULL)) {
			if (!M_hash_stru64_get(table->col_name_id, key, NULL)) {
				M_hash_dict_enumerate_free(he);
				return M_FALSE;
			}
		}
		M_hash_dict_enumerate_free(he);
	}

	/* We know everythings good so let's start adding. */
	M_hash_dict_enumerate(data, &he);
	while (M_hash_dict_enumerate_next(data, he, &key, &val)) {
		M_table_cell_set(table, row, key, val, flags);
	}
	M_hash_dict_enumerate_free(he);

	return M_TRUE;
}

M_bool M_table_cell_clear(M_table_t *table, size_t row, const char *colname)
{
	return M_table_cell_set(table, row, colname, NULL, M_TABLE_INSERT_COLIGNORE);
}

M_bool M_table_cell_clear_at(M_table_t *table, size_t row, size_t col)
{
	return M_table_cell_set_at(table, row, col, NULL);
}

const char *M_table_cell(const M_table_t *table, size_t row, const char *colname)
{
	M_uint64 colid;
	M_uint64 rowid;

	if (table == NULL || row >= M_list_u64_len(table->row_order) || M_str_isempty(colname))
		return NULL;

	rowid = M_list_u64_at(table->row_order, row);
	if (!M_hash_stru64_get(table->col_name_id, colname, &colid))
		return NULL;

	return M_table_cell_get_int(table, rowid, colid);
}

const char *M_table_cell_at(const M_table_t *table, size_t row, size_t col)
{
	M_uint64 colid;
	M_uint64 rowid;

	if (table == NULL || row >= M_list_u64_len(table->row_order) || col >= M_list_u64_len(table->col_order))
		return NULL;

	rowid = M_list_u64_at(table->row_order, row);
	colid = M_list_u64_at(table->col_order, col);

	return M_table_cell_get_int(table, rowid, colid);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_table_merge(M_table_t **dest, M_table_t *src)
{
	const char *colname;
	const char *val;
	size_t      numcols;
	size_t      numcols_dest;
	size_t      numrows;
	size_t      rowidx;
	size_t      i;
	size_t      j;

	if (dest == NULL) {
		*dest = src;
		return M_TRUE;
	}

	if (src == NULL)
		return M_TRUE;

	numcols_dest = M_table_column_count(*dest);
	numrows      = M_table_row_count(src);
	numcols      = M_table_column_count(src);

	/* Validate both tables have named columns and all columns are named. */
	for (i=0; i<numcols_dest; i++) {
		if (M_str_isempty(M_table_column_name(*dest, i))) {
			return M_FALSE;
		}
	}
	for (i=0; i<numcols; i++) {
		if (M_str_isempty(M_table_column_name(src, i))) {
			return M_FALSE;
		}
	}

	/* Go though every row and cell in src and add it to dest. */
	for (i=0; i<numrows; i++) {
		rowidx = M_table_row_insert(*dest);

		for (j=0; j<numcols; j++) {
			colname = M_table_column_name(src, j);
			val     = M_table_cell_at(src, i, j);
			M_table_cell_set(*dest, rowidx, colname, val, M_TABLE_INSERT_COLADD);
		}
	}

	M_table_destroy(src);
	return M_TRUE;
}

M_table_t *M_table_duplicate(const M_table_t *table)
{
	M_table_t           *rt;
	M_hash_u64vp_enum_t *he;
	M_hash_u64str_t     *row_data;
	M_uint64             rowid;

	if (table == NULL)
		return NULL;

	rt = M_malloc_zero(sizeof(*table));

	rt->col_order   = M_list_u64_duplicate(table->col_order);
	rt->col_id_name = M_hash_u64str_duplicate(table->col_id_name);
	rt->col_name_id = M_hash_stru64_duplicate(table->col_name_id);
	rt->row_order   = M_list_u64_duplicate(table->row_order);

	rt->rows = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, (void (*)(void *))M_hash_u64str_destroy);
	M_hash_u64vp_enumerate(table->rows, &he);
	while (M_hash_u64vp_enumerate_next(table->rows, he, &rowid, (void **)&row_data)) {
		M_hash_u64vp_insert(rt->rows, rowid, M_hash_u64str_duplicate(row_data));
	}
	M_hash_u64vp_enumerate_free(he);

	rt->rand  = M_rand_create(0);
	rt->flags = table->flags;

	return rt;
}
