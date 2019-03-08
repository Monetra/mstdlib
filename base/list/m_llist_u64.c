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

static void *M_llist_u64_duplicate_func(const void *arg)
{
	return M_memdup(arg, sizeof(M_uint64));
}

static void M_llist_u64_get_sorting(M_llist_u64_flags_t flags, M_sort_compar_t *equality, M_bool *sorted)
{
	*equality = M_sort_compar_u64;
	*sorted   = M_FALSE;

	if (flags & (M_LLIST_U64_SORTASC|M_LLIST_U64_SORTDESC)) {
		*sorted = M_TRUE;
		if (flags & M_LLIST_U64_SORTDESC) {
			*equality = M_sort_compar_u64_desc;
		}
	}
}

static M_llist_match_type_t M_llist_u64_convert_match_type(M_llist_u64_match_type_t type)
{
	M_llist_match_type_t ltype = M_LLIST_MATCH_VAL;

	if (type & M_LLIST_U64_MATCH_ALL) {
		ltype = M_LLIST_MATCH_ALL;
	}

	return ltype;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_u64_t *M_llist_u64_create(M_uint32 flags)
{
	M_llist_flags_t          lflags    = M_LLIST_NONE;
	M_bool                   sorted;
	struct M_llist_callbacks callbacks = {
		NULL,
		M_llist_u64_duplicate_func,
		M_llist_u64_duplicate_func,
		M_free
	};

	M_llist_u64_get_sorting(flags, &callbacks.equality, &sorted);
	if (sorted)
		lflags |= M_LLIST_SORTED;

	if (flags & M_LLIST_U64_CIRCULAR)
		lflags |= M_LLIST_CIRCULAR;

	return (M_llist_u64_t *)M_llist_create(&callbacks, lflags);
}

void M_llist_u64_destroy(M_llist_u64_t *d)
{
	M_llist_destroy((M_llist_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_u64_node_t *M_llist_u64_insert(M_llist_u64_t *d, M_uint64 val)
{
	return (M_llist_u64_node_t *)M_llist_insert((M_llist_t *)d, &val);
}

M_llist_u64_node_t *M_llist_u64_insert_first(M_llist_u64_t *d, M_uint64 val)
{
	return (M_llist_u64_node_t *)M_llist_insert_first((M_llist_t *)d, &val);
}

M_llist_u64_node_t *M_llist_u64_insert_before(M_llist_u64_node_t *n, M_uint64 val)
{
	return (M_llist_u64_node_t *)M_llist_insert_before((M_llist_node_t *)n, &val);
}

M_llist_u64_node_t *M_llist_u64_insert_after(M_llist_u64_node_t *n, M_uint64 val)
{
	return (M_llist_u64_node_t *)M_llist_insert_after((M_llist_node_t *)n, &val);
}

void M_llist_u64_set_first(M_llist_u64_node_t *n)
{
	M_llist_set_first((M_llist_node_t *)n);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_u64_move_before(M_llist_u64_node_t *move, M_llist_u64_node_t *before)
{
	return M_llist_move_before((M_llist_node_t *)move, (M_llist_node_t *)before);
}

M_bool M_llist_u64_move_after(M_llist_u64_node_t *move, M_llist_u64_node_t *after)
{
	return M_llist_move_after((M_llist_node_t *)move, (M_llist_node_t *)after);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_llist_u64_len(const M_llist_u64_t *d)
{
	return M_llist_len((const M_llist_t *)d);
}

size_t M_llist_u64_count(const M_llist_u64_t *d, M_uint64 val)
{
	return M_llist_count((const M_llist_t *)d, &val, M_LLIST_MATCH_VAL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_u64_node_t *M_llist_u64_first(const M_llist_u64_t *d)
{
	return (M_llist_u64_node_t *)M_llist_first((const M_llist_t *)d);
}

M_llist_u64_node_t *M_llist_u64_last(const M_llist_u64_t *d)
{
	return (M_llist_u64_node_t *)M_llist_last((const M_llist_t *)d);
}

M_llist_u64_node_t *M_llist_u64_find(const M_llist_u64_t *d, M_uint64 val)
{
	return (M_llist_u64_node_t *)M_llist_find((const M_llist_t *)d, &val, M_LLIST_MATCH_VAL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 M_llist_u64_take_node(M_llist_u64_node_t *n)
{
	M_uint64 *val;
	M_uint64  myval = 0;

	val = M_llist_take_node((M_llist_node_t *)n);
	if (val != NULL) {
		myval = *val;
		M_free(val);
	}

	return myval;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_u64_remove_node(M_llist_u64_node_t *n)
{
	return M_llist_remove_node((M_llist_node_t *)n);
}

size_t M_llist_u64_remove_val(M_llist_u64_t *d, M_uint64 val, M_uint32 type)
{
	return M_llist_remove_val((M_llist_t * )d, &val, M_llist_u64_convert_match_type(type));
}

void M_llist_u64_remove_duplicates(M_llist_u64_t *d)
{
	M_llist_remove_duplicates((M_llist_t * )d, M_LLIST_MATCH_VAL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_u64_node_t *M_llist_u64_node_next(const M_llist_u64_node_t *n)
{
	return (M_llist_u64_node_t *)M_llist_node_next((const M_llist_node_t *)n);
}

M_llist_u64_node_t *M_llist_u64_node_prev(const M_llist_u64_node_t *n)
{
	return (M_llist_u64_node_t *)M_llist_node_prev((const M_llist_node_t *)n);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 M_llist_u64_node_val(const M_llist_u64_node_t *n)
{
	const M_uint64 *val;

	val = M_llist_node_val((const M_llist_node_t *)n);
	if (val != NULL)
		return *val;
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_u64_t *M_llist_u64_duplicate(const M_llist_u64_t *d)
{
	return (M_llist_u64_t *)M_llist_duplicate((const M_llist_t *)d);
}

void M_llist_u64_merge(M_llist_u64_t **dest, M_llist_u64_t *src, M_bool include_duplicates)
{
	M_llist_merge((M_llist_t **)dest, (M_llist_t *)src, include_duplicates, M_LLIST_MATCH_VAL);
}
