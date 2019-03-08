/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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


M_bool M_table_load_json(M_table_t *table, const char *data, size_t len)
{
	M_table_t     *jtable;
	M_json_node_t *json;
	M_json_node_t *node;
	M_list_str_t  *keys;
	const char    *colname;
	const char    *val;
	M_json_type_t  type;
	M_bool         ret  = M_FALSE;
	size_t         numelms;
	size_t         numkeys;
	size_t         i;
	size_t         j;
	size_t         rowidx;

	if (table == NULL)
		return M_FALSE;

	/* Read the json string. */
	json = M_json_read(data, len, M_JSON_READER_OBJECT_UNIQUE_KEYS, NULL, NULL, NULL, NULL);
	if (json == NULL)
		return M_FALSE;

	jtable = M_table_create(M_TABLE_NONE);
	
	type = M_json_node_type(json);
	if (type != M_JSON_TYPE_ARRAY)
		goto done;

	numelms = M_json_array_len(json);
	for (i=0; i<numelms; i++) {
		rowidx = M_table_row_insert(jtable);
		node   = M_json_array_at(json, i);
		type   = M_json_node_type(node);
		if (type != M_JSON_TYPE_OBJECT) {
			goto done;
		}

		keys    = M_json_object_keys(node);
		numkeys = M_list_str_len(keys);
		for (j=0; j<numkeys; j++) {
			colname = M_list_str_at(keys, j);
			val     = M_json_object_value_string(node, colname);
			if (val == NULL) {
				M_list_str_destroy(keys);
				goto done;
			}
			if (!M_table_cell_set(jtable, rowidx, colname, val, M_TABLE_INSERT_COLADD)) {
				M_list_str_destroy(keys);
				goto done;
			}
		}

		M_list_str_destroy(keys);
	}

	ret = M_table_merge(&table, jtable);

done:
	M_json_node_destroy(json);
	if (!ret)
		M_table_destroy(jtable);
	return ret;
}

char *M_table_write_json(const M_table_t *table, M_uint32 flags)
{
	M_json_node_t *json;
	M_json_node_t *node;
	char          *out;
	const char    *colname;
	const char    *val;
	size_t         numrows;
	size_t         numcols;
	size_t         i;
	size_t         j;

	/* Validate all columns are named. */
	numcols = M_table_column_count(table);
	for (i=0; i<numcols; i++) {
		if (M_str_isempty(M_table_column_name(table, i))) {
			return NULL;
		}
	}

	json = M_json_node_create(M_JSON_TYPE_ARRAY);

	numrows = M_table_row_count(table);
	for (i=0; i<numrows; i++) {
		node = M_json_node_create(M_JSON_TYPE_OBJECT);

		for (j=0; j<numcols; j++) {
			colname = M_table_column_name(table, j);
			val     = M_table_cell_at(table, i, j);
			/* Don't add empty values. */
			if (val == NULL) {
				continue;
			}
			M_json_object_insert_string(node, colname, val);
		}

		M_json_array_insert(json, node);
	}

	out = M_json_write(json, flags, NULL);
	M_json_node_destroy(json);
	return out;
}
