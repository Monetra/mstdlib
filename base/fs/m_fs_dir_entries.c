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

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include "fs/m_fs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_fs_dir_sort_t primary;
	M_fs_dir_sort_t secondary;
	M_bool          primary_asc;
	M_bool          secondary_asc;
} M_fs_dir_entries_thunk_t;

typedef enum {
	M_FS_DIR_ENTRIES_TIME_ATIME,
	M_FS_DIR_ENTRIES_TIME_MTIME,
	M_FS_DIR_ENTRIES_TIME_CTIME
} M_fs_dir_entries_time_type_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int M_fs_dir_entries_sort_isdir(const M_fs_dir_entry_t *arg1, const M_fs_dir_entry_t *arg2)
{
	M_bool isdir1;
	M_bool isdir2;

	isdir1 = M_fs_dir_entry_get_type(arg1)==M_FS_TYPE_DIR?M_TRUE:M_FALSE;
	isdir2 = M_fs_dir_entry_get_type(arg2)==M_FS_TYPE_DIR?M_TRUE:M_FALSE;

	if (isdir1 == isdir2)
		return 0;
	if (isdir1)
		return -1;
	return 1;
}

static int M_fs_dir_entries_sort_ishidden(const M_fs_dir_entry_t *arg1, const M_fs_dir_entry_t *arg2)
{
	M_bool ishidden1;
	M_bool ishidden2;

	ishidden1 = M_fs_dir_entry_get_ishidden(arg1);
	ishidden2 = M_fs_dir_entry_get_ishidden(arg2);

	if (ishidden1 == ishidden2)
		return 0;
	if (ishidden1)
		return -1;
	return 1;
}

static int M_fs_dir_entries_sort_size(const M_fs_dir_entry_t *arg1, const M_fs_dir_entry_t *arg2)
{
	M_uint64 i1;
	M_uint64 i2;

	i1 = M_fs_info_get_size(M_fs_dir_entry_get_info(arg1));
	i2 = M_fs_info_get_size(M_fs_dir_entry_get_info(arg2));

	if (i1 == i2)
		return 0;
	else if (i1 < i2)
		return -1;
	return 1;
}

static int M_fs_dir_entries_sort_time(const M_fs_dir_entry_t *arg1, const M_fs_dir_entry_t *arg2, M_fs_dir_entries_time_type_t type)
{
	M_time_t t1 = 0;
	M_time_t t2 = 0;

	switch (type) {
		case M_FS_DIR_ENTRIES_TIME_ATIME:
			t1 = M_fs_info_get_atime(M_fs_dir_entry_get_info(arg1));
			t2 = M_fs_info_get_atime(M_fs_dir_entry_get_info(arg2));
			break;
		case M_FS_DIR_ENTRIES_TIME_MTIME:
			t1 = M_fs_info_get_mtime(M_fs_dir_entry_get_info(arg1));
			t2 = M_fs_info_get_mtime(M_fs_dir_entry_get_info(arg2));
			break;
		case M_FS_DIR_ENTRIES_TIME_CTIME:
			t1 = M_fs_info_get_ctime(M_fs_dir_entry_get_info(arg1));
			t2 = M_fs_info_get_ctime(M_fs_dir_entry_get_info(arg2));
			break;
	}

	if (t1 == t2)
		return 0;
	else if (t1 < t2)
		return -1;
	return 1;
}

static int M_fs_dir_entries_sort_type(const M_fs_dir_entry_t *arg1, const M_fs_dir_entry_t *arg2, M_fs_dir_sort_t type, M_bool asc)
{
	const M_fs_dir_entry_t *e1;
	const M_fs_dir_entry_t *e2;

	e1 = arg1;
	e2 = arg2;
	if (!asc) {
		e1 = arg2;
		e2 = arg1;
	}

	switch (type) {
		case M_FS_DIR_SORT_NAME_CASECMP:
			return M_str_casecmpsort(M_fs_dir_entry_get_name(e1), M_fs_dir_entry_get_name(e2));
		case M_FS_DIR_SORT_NAME_CMP:
			return M_str_cmpsort(M_fs_dir_entry_get_name(e1), M_fs_dir_entry_get_name(e2));
		case M_FS_DIR_SORT_ISDIR:
			return M_fs_dir_entries_sort_isdir(e1, e2);
		case M_FS_DIR_SORT_ISHIDDEN:
			return M_fs_dir_entries_sort_ishidden(e1, e2);
		case M_FS_DIR_SORT_SIZE:
			return M_fs_dir_entries_sort_size(e1, e2);
		case M_FS_DIR_SORT_ATIME:
			return M_fs_dir_entries_sort_time(e1, e1, M_FS_DIR_ENTRIES_TIME_ATIME);
		case M_FS_DIR_SORT_MTIME:
			return M_fs_dir_entries_sort_time(e1, e1, M_FS_DIR_ENTRIES_TIME_MTIME);
		case M_FS_DIR_SORT_CTIME:
			return M_fs_dir_entries_sort_time(e1, e1, M_FS_DIR_ENTRIES_TIME_CTIME);
		case M_FS_DIR_SORT_NONE:
			return 0;
	}
	return 0;
}

static int M_fs_dir_entries_compar(const void *arg1, const void *arg2, void *thunk)
{
	const M_fs_dir_entry_t   *e1;
	const M_fs_dir_entry_t   *e2;
	M_fs_dir_entries_thunk_t *sort_data;
	int                       ret;

	if (arg1 == NULL || arg2 == NULL || thunk == NULL)
		return 0;

	e1        = *(M_fs_dir_entry_t * const *)arg1;
	e2        = *(M_fs_dir_entry_t * const *)arg2;
	sort_data = (M_fs_dir_entries_thunk_t *)thunk;

	ret = M_fs_dir_entries_sort_type(e1, e2, sort_data->primary, sort_data->primary_asc);
	if (ret == 0) {
		ret = M_fs_dir_entries_sort_type(e1, e2, sort_data->secondary, sort_data->secondary_asc);
	}

	return ret;
}

/* Prototype wrapper. */
static void M_fs_dir_entry_destroy_vp(void *d)
{
	M_fs_dir_entry_destroy(d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Internal */

M_fs_dir_entries_t *M_fs_dir_entries_create(void)
{
	struct M_list_callbacks callbacks = {
		NULL,
		NULL,
		NULL,
		M_fs_dir_entry_destroy_vp
	};

	return (M_fs_dir_entries_t *)M_list_create(&callbacks, M_LIST_NONE);
}

M_bool M_fs_dir_entries_insert(M_fs_dir_entries_t *d, M_fs_dir_entry_t *val)
{
	return M_list_insert((M_list_t *)d, val);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Public */

void M_fs_dir_entries_destroy(M_fs_dir_entries_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

void M_fs_dir_entries_sort(M_fs_dir_entries_t *d, M_fs_dir_sort_t primary_sort, M_bool primary_asc, M_fs_dir_sort_t secondary_sort, M_bool secondary_asc)
{
	M_fs_dir_entries_thunk_t thunk;

	M_mem_set(&thunk, 0, sizeof(thunk));
	thunk.primary       = primary_sort;
	thunk.primary_asc   = primary_asc;
	thunk.secondary     = secondary_sort;
	thunk.secondary_asc = secondary_asc;

	/* Sort based on the given parameters. */
	M_list_change_sorting((M_list_t *)d, M_fs_dir_entries_compar, M_LIST_SORTED|M_LIST_STABLE, &thunk);
	/* Remove sorting so we don't have to worry about keeping the thunk around. */
	M_list_change_sorting((M_list_t *)d, NULL, M_LIST_NONE, NULL);
}

size_t M_fs_dir_entries_len(const M_fs_dir_entries_t *d)
{
	return M_list_len((const M_list_t *)d);
}

const M_fs_dir_entry_t *M_fs_dir_entries_at(const M_fs_dir_entries_t *d, size_t idx)
{
	return M_list_at((const M_list_t *)d, idx);
}

M_fs_dir_entry_t *M_fs_dir_entries_take_at(M_fs_dir_entries_t *d, size_t idx)
{
	return M_list_take_at((M_list_t *)d, idx);
}

M_bool M_fs_dir_entries_remove_at(M_fs_dir_entries_t *d, size_t idx)
{
	return M_list_remove_at((M_list_t *)d, idx);
}

M_bool M_fs_dir_entries_remove_range(M_fs_dir_entries_t *d, size_t start, size_t end)
{
	return M_list_remove_range((M_list_t *)d, start, end);
}

void M_fs_dir_entries_merge(M_fs_dir_entries_t **dest, M_fs_dir_entries_t *src)
{
	M_list_merge((M_list_t **)dest, (M_list_t *)src, M_TRUE, M_LIST_MATCH_VAL);
}
