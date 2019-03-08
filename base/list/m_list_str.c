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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void *M_list_str_duplicate_func(const void *arg)
{
	return M_strdup(arg);
}

static void M_list_str_get_sorting(M_list_str_flags_t flags, M_sort_compar_t *equality, M_list_flags_t *sorted)
{
	*equality = M_sort_compar_str;

	if (flags & M_LIST_STR_CASECMP && flags & M_LIST_STR_SORTASC) {
		*equality = M_sort_compar_str_casecmp;
	} else if (flags & M_LIST_STR_CASECMP && flags & M_LIST_STR_SORTDESC) {
		*equality = M_sort_compar_str_casecmp_desc;
	} else if (flags & M_LIST_STR_SORTDESC) {
		*equality = M_sort_compar_str_desc;
	}

	if (flags & (M_LIST_STR_SORTASC|M_LIST_STR_SORTDESC))
		*sorted |= M_LIST_SORTED;
	if (flags & M_LIST_STR_STABLE)
		*sorted |= M_LIST_SORTED;
}

static M_list_match_type_t M_list_str_convert_match_type(M_list_str_match_type_t type)
{
	M_list_match_type_t ltype = M_LIST_MATCH_VAL;

	if (type & M_LIST_STR_MATCH_PTR) {
		ltype = M_LIST_MATCH_PTR;
	}
	if (type & M_LIST_STR_MATCH_ALL) {
		ltype |= M_LIST_MATCH_ALL;
	}

	return ltype;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_str_t *M_list_str_create(M_uint32 flags)
{
	M_list_flags_t          lflags    = M_LIST_NONE;
	struct M_list_callbacks callbacks = {
		NULL,
		M_list_str_duplicate_func,
		M_list_str_duplicate_func,
		M_free,
	};

	M_list_str_get_sorting(flags, &callbacks.equality, &lflags);
	if (flags & M_LIST_STR_STACK)
		lflags |= M_LIST_STACK;
	if (flags & M_LIST_STR_SET)
		lflags |= M_LIST_SET_VAL;
	if (flags & M_LIST_STR_NEVERSHRINK)
		lflags |= M_LIST_NEVERSHRINK;

	/* We are only dealing in opaque types here, and we don't have any
	 * metadata of our own to store, so we are only casting one pointer
	 * type to another.  This is a safe operation */
	return (M_list_str_t *)M_list_create(&callbacks, lflags);
}

void M_list_str_destroy(M_list_str_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_list_str_change_sorting(M_list_str_t *d, M_uint32 flags)
{
	M_sort_compar_t equality;
	M_list_flags_t  lflags    = M_LIST_NONE;

	M_list_str_get_sorting(flags, &equality, &lflags);
	M_list_change_sorting((M_list_t *)d, equality, lflags, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_list_str_insert(M_list_str_t *d, const char *val)
{
	return M_list_insert((M_list_t *)d, val);
}

size_t M_list_str_insert_idx(const M_list_str_t *d, const char *val)
{
	return M_list_insert_idx((const M_list_t *)d, val);
}

M_bool M_list_str_insert_at(M_list_str_t *d, const char *val, size_t idx)
{
	return M_list_insert_at((M_list_t *)d, val, idx);
}

void M_list_str_insert_begin(M_list_str_t *d)
{
	M_list_insert_begin((M_list_t *)d);
}

void M_list_str_insert_end(M_list_str_t *d)
{
	M_list_insert_end((M_list_t *)d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_list_str_len(const M_list_str_t *d)
{
	return M_list_len((const M_list_t *)d);
}

size_t M_list_str_count(const M_list_str_t *d, const char *val, M_uint32 type)
{
	return M_list_count((const M_list_t *)d, val, M_list_str_convert_match_type(type));
}

M_bool M_list_str_index_of(const M_list_str_t *d, const char *val, M_uint32 type, size_t *idx)
{
	return M_list_index_of((const M_list_t *)d, val, M_list_str_convert_match_type(type), idx);
}

const char *M_list_str_first(const M_list_str_t *d)
{
	return M_list_first((const M_list_t *)d);
}

const char *M_list_str_last(const M_list_str_t *d)
{
	return M_list_last((const M_list_t *)d);
}

const char *M_list_str_at(const M_list_str_t *d, size_t idx)
{
	return M_list_at((const M_list_t *)d, idx);
}

char *M_list_str_take_first(M_list_str_t *d)
{
	return M_list_take_first((M_list_t *)d);
}

char *M_list_str_take_last(M_list_str_t *d)
{
	return M_list_take_last((M_list_t *)d);
}

char *M_list_str_take_at(M_list_str_t *d, size_t idx)
{
	return M_list_take_at((M_list_t *)d, idx);
}

M_bool M_list_str_remove_first(M_list_str_t *d)
{
	return M_list_remove_first((M_list_t *)d);
}

M_bool M_list_str_remove_last(M_list_str_t *d)
{
	return M_list_remove_last((M_list_t *)d);
}

M_bool M_list_str_remove_at(M_list_str_t *d, size_t idx)
{
	return M_list_remove_at((M_list_t *)d, idx);
}

size_t M_list_str_remove_val(M_list_str_t *d, const char *val, M_uint32 type)
{
	return M_list_remove_val((M_list_t *)d, val, M_list_str_convert_match_type(type));
}

M_bool M_list_str_remove_range(M_list_str_t *d, size_t start, size_t end)
{
	return M_list_remove_range((M_list_t *)d, start, end);
}

void M_list_str_remove_duplicates(M_list_str_t *d)
{
	M_list_remove_duplicates((M_list_t *)d, M_LIST_MATCH_VAL);
}

size_t M_list_str_replace_val(M_list_str_t *d, const char *val, const char *new_val, M_uint32 type)
{
	return M_list_replace_val((M_list_t *)d, val, new_val, M_list_str_convert_match_type(type));
}

M_bool M_list_str_replace_at(M_list_str_t *d, const char *val, size_t idx)
{
	return M_list_replace_at((M_list_t *)d, val, idx);
}

M_bool M_list_str_swap(M_list_str_t *d, size_t idx1, size_t idx2)
{
	return M_list_swap((M_list_t *)d, idx1, idx2);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_str_t *M_list_str_duplicate(const M_list_str_t *d)
{
	return (M_list_str_t *)M_list_duplicate((const M_list_t *)d);
}

void M_list_str_merge(M_list_str_t **dest, M_list_str_t *src, M_bool include_duplicates)
{
	M_list_merge((M_list_t **)dest, (M_list_t *)src, include_duplicates, M_LIST_MATCH_VAL);
}

M_list_str_t *M_list_str_split(unsigned char delim, const char *s, M_uint32 flags, M_bool keep_empty_parts)
{
	char         **parts;
	size_t         num_parts;
	size_t         i;
	M_list_str_t  *d; 

	d = M_list_str_create(flags);

	if (s == NULL) {
		return d;
	}

	parts = M_str_explode_str((unsigned char)delim, s, &num_parts);
	for (i=0; i<num_parts; i++) {
		if (keep_empty_parts || (parts[i] != NULL && *parts[i] != '\0')) {
			M_list_str_insert(d, parts[i]);
		}
	}
	M_str_explode_free(parts, num_parts);

	return d;
}

char *M_list_str_join(const M_list_str_t *d, unsigned char sep)
{
	return M_list_str_join_range(d, sep, 0, M_list_str_len(d));
}

char *M_list_str_join_str(const M_list_str_t *d, const char *sep)
{
	return M_list_str_join_range_str(d, sep, 0, M_list_str_len(d));
}

char *M_list_str_join_range(const M_list_str_t *d, unsigned char sep, size_t start, size_t end)
{
	M_buf_t *buf;
	size_t   i;
	size_t   len;

	if (d == NULL || start > end) {
		return NULL;
	}

	len = M_list_str_len(d);
	if (start >= len) {
		return NULL;
	}
	if (end >= len) {
		end = len-1;
	}

	buf = M_buf_create();
	/* should be i<=end but gcc has a bug that warns that this will cause an
	 * infinite loop. Using i<end+1 (which is equivalent) does not produce this
	 * warning. */
	for (i=start; i<end+1; i++) {
		M_buf_add_str(buf, M_list_str_at(d, i));
		if (i != end) {
			M_buf_add_byte(buf, sep);
		}
	}

	return M_buf_finish_str(buf, NULL);
}

char *M_list_str_join_range_str(const M_list_str_t *d, const char *sep, size_t start, size_t end)
{
	M_buf_t *buf;
	size_t   i;
	size_t   len;

	if (d == NULL || start > end) {
		return NULL;
	}

	len = M_list_str_len(d);
	if (start >= len) {
		return NULL;
	}
	if (end >= len) {
		end = len-1;
	}
	if (sep == NULL) {
		sep = "";
	}

	buf = M_buf_create();
	/* should be i<=end but gcc has a bug that warns that this will cause an
	 * infinite loop. Using i<end+1 (which is equivalent) does not produce this
	 * warning. */
	for (i=start; i<end+1; i++) {
		M_buf_add_str(buf, M_list_str_at(d, i));
		if (i != end) {
			M_buf_add_str(buf, sep);
		}
	}

	return M_buf_finish_str(buf, NULL);
}
