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

static void *M_list_u64_duplicate_func(const void *arg)
{
	return M_memdup(arg, sizeof(M_uint64));
}

static void M_list_u64_get_sorting(M_list_u64_flags_t flags, M_sort_compar_t *equality, M_list_flags_t *sorted)
{
	*equality = M_sort_compar_u64;

	if (sorted == NULL)
		return;

	if (flags & (M_LIST_U64_SORTASC|M_LIST_U64_SORTDESC)) {
		*sorted |= M_LIST_SORTED;
		if (flags & M_LIST_U64_SORTDESC) {
			*equality = M_sort_compar_u64_desc;
		}
	}
	if (flags & M_LIST_U64_STABLE)
		*sorted |= M_LIST_SORTED;
}

static M_list_match_type_t M_list_u64_convert_match_type(M_list_u64_match_type_t type)
{
	M_list_match_type_t ltype = M_LIST_MATCH_VAL;

	if (type & M_LIST_U64_MATCH_ALL) {
		ltype = M_LIST_MATCH_ALL;
	}

	return ltype;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_u64_t *M_list_u64_create(M_uint32 flags)
{
	M_list_flags_t          lflags    = M_LIST_NONE;
	struct M_list_callbacks callbacks = {
		NULL,
		M_list_u64_duplicate_func,
		M_list_u64_duplicate_func,
		M_free
	};

	M_list_u64_get_sorting(flags, &callbacks.equality, &lflags);
	if (flags & M_LIST_U64_STACK)
		lflags |= M_LIST_STACK;
	if (flags & M_LIST_U64_SET)
		lflags |= M_LIST_SET_VAL;
	if (flags & M_LIST_U64_NEVERSHRINK)
		lflags |= M_LIST_NEVERSHRINK;

	return (M_list_u64_t *)M_list_create(&callbacks, lflags);
}

void M_list_u64_destroy(M_list_u64_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_list_u64_change_sorting(M_list_u64_t *d, M_uint32 flags)
{
	M_sort_compar_t equality = M_sort_compar_u64;
	M_list_flags_t  sorted   = M_LIST_NONE;

	M_list_u64_get_sorting(flags, &equality, &sorted);
	M_list_change_sorting((M_list_t *)d, equality, sorted, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_list_u64_insert(M_list_u64_t *d, M_uint64 val)
{
	return M_list_insert((M_list_t *)d, &val);
}

size_t M_list_u64_insert_idx(const M_list_u64_t *d, M_uint64 *val)
{
	return M_list_insert_idx((const M_list_t *)d, &val);
}

M_bool M_list_u64_insert_at(M_list_u64_t *d, M_uint64 val, size_t idx)
{
	return M_list_insert_at((M_list_t *)d, &val, idx);
}

void M_list_u64_insert_begin(M_list_u64_t *d)
{
	M_list_insert_begin((M_list_t *)d);
}

void M_list_u64_insert_end(M_list_u64_t *d)
{
	M_list_insert_end((M_list_t *)d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_list_u64_len(const M_list_u64_t *d)
{
	return M_list_len((const M_list_t *)d);
}

size_t M_list_u64_count(const M_list_u64_t *d, M_uint64 val)
{
	return M_list_count((const M_list_t *)d, &val, M_LIST_MATCH_VAL);
}

M_bool M_list_u64_index_of(const M_list_u64_t *d, M_uint64 val, size_t *idx)
{
	return M_list_index_of((const M_list_t *)d, &val, M_LIST_MATCH_VAL, idx);
}

static M_uint64 M_list_u64_peek_int(const M_uint64 *n)
{
	if (n == NULL) {
		return 0;
	}
	return *n;
}

M_uint64 M_list_u64_first(const M_list_u64_t *d)
{
	return M_list_u64_peek_int(M_list_first((const M_list_t *)d));
}

M_uint64 M_list_u64_last(const M_list_u64_t *d)
{
	return M_list_u64_peek_int(M_list_last((const M_list_t *)d));
}

M_uint64 M_list_u64_at(const M_list_u64_t *d, size_t idx)
{
	return M_list_u64_peek_int(M_list_at((const M_list_t *)d, idx));
}

static M_uint64 M_list_u64_take_int(M_uint64 *n)
{
	M_uint64 m;

	if (n == NULL) {
		m = 0;
	} else {
		m = *n;
	}
	M_free(n);

	return m;
}

M_uint64 M_list_u64_take_first(M_list_u64_t *d)
{
	return M_list_u64_take_int(M_list_take_first((M_list_t *)d));
}

M_uint64 M_list_u64_take_last(M_list_u64_t *d)
{
	return M_list_u64_take_int(M_list_take_last((M_list_t *)d));
}

M_uint64 M_list_u64_take_at(M_list_u64_t *d, size_t idx)
{
	return M_list_u64_take_int(M_list_take_at((M_list_t *)d, idx));
}

M_bool M_list_u64_remove_first(M_list_u64_t *d)
{
	return M_list_remove_first((M_list_t *)d);
}

M_bool M_list_u64_remove_last(M_list_u64_t *d)
{
	return M_list_remove_last((M_list_t *)d);
}

M_bool M_list_u64_remove_at(M_list_u64_t *d, size_t idx)
{
	return M_list_remove_at((M_list_t *)d, idx);
}

size_t M_list_u64_remove_val(M_list_u64_t *d, M_uint64 val, M_uint32 type)
{
	return M_list_remove_val((M_list_t *)d, &val, M_list_u64_convert_match_type(type));
}

M_bool M_list_u64_remove_range(M_list_u64_t *d, size_t start, size_t end)
{
	return M_list_remove_range((M_list_t *)d, start, end);
}

void M_list_u64_remove_duplicates(M_list_u64_t *d)
{
	M_list_remove_duplicates((M_list_t *)d, M_LIST_MATCH_VAL);
}

size_t M_list_u64_replace_val(M_list_u64_t *d, M_uint64 val, M_uint64 new_val, M_uint32 type)
{
	return M_list_replace_val((M_list_t *)d, &val, &new_val, M_list_u64_convert_match_type(type)); 
}

M_bool M_list_u64_replace_at(M_list_u64_t *d, M_uint64 val, size_t idx)
{
	return M_list_replace_at((M_list_t *)d, &val, idx);
}

M_bool M_list_u64_swap(M_list_u64_t *d, size_t idx1, size_t idx2)
{
	return M_list_swap((M_list_t *)d, idx1, idx2);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_u64_t *M_list_u64_duplicate(const M_list_u64_t *d)
{
	return (M_list_u64_t *)M_list_duplicate((const M_list_t *)d);
}

void M_list_u64_merge(M_list_u64_t **dest, M_list_u64_t *src, M_bool include_duplicates)
{
	M_list_merge((M_list_t **)dest, (M_list_t *)src, include_duplicates, M_LIST_MATCH_VAL);
}
