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

static M_llist_match_type_t M_llist_bin_convert_match_type(M_llist_bin_match_type_t type)
{
	M_llist_match_type_t ltype = M_LLIST_MATCH_VAL;

	if (type & M_LLIST_BIN_MATCH_ALL) {
		ltype = M_LLIST_MATCH_ALL;
	}

	return ltype;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_bin_t *M_llist_bin_create(M_uint32 flags)
{
	M_llist_flags_t          lflags    = M_LLIST_NONE;
	struct M_llist_callbacks callbacks = {
		M_sort_compar_binwraped,
		NULL, /* The wrapper will manage copying for insert. */
		M_bin_wrapeddup_vp,
		M_free
	};

	if (flags & M_LLIST_BIN_CIRCULAR)
		lflags = M_LLIST_CIRCULAR;

	return (M_llist_bin_t *)M_llist_create(&callbacks, lflags);
}

void M_llist_bin_destroy(M_llist_bin_t *d)
{
	M_llist_destroy((M_llist_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_bin_node_t *M_llist_bin_insert(M_llist_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return (M_llist_bin_node_t *)M_llist_insert((M_llist_t *)d, duped_val);
}

M_llist_bin_node_t *M_llist_bin_insert_first(M_llist_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return (M_llist_bin_node_t *)M_llist_insert_first((M_llist_t *)d, duped_val);
}

M_llist_bin_node_t *M_llist_bin_insert_before(M_llist_bin_node_t *n, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return (M_llist_bin_node_t *)M_llist_insert_before((M_llist_node_t *)n, duped_val);
}

M_llist_bin_node_t *M_llist_bin_insert_after(M_llist_bin_node_t *n, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return (M_llist_bin_node_t *)M_llist_insert_after((M_llist_node_t *)n, duped_val);
}

void M_llist_bin_set_first(M_llist_bin_node_t *n)
{
	M_llist_set_first((M_llist_node_t *)n);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_bin_move_before(M_llist_bin_node_t *move, M_llist_bin_node_t *before)
{
	return M_llist_move_before((M_llist_node_t *)move, (M_llist_node_t *)before);
}

M_bool M_llist_bin_move_after(M_llist_bin_node_t *move, M_llist_bin_node_t *after)
{
	return M_llist_move_after((M_llist_node_t *)move, (M_llist_node_t *)after);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_llist_bin_len(const M_llist_bin_t *d)
{
	return M_llist_len((const M_llist_t *)d);
}

size_t M_llist_bin_count(const M_llist_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val;
	size_t   cnt;

	duped_val = M_bin_wrap(val, len);
	cnt       = M_llist_count((const M_llist_t *)d, duped_val, M_LLIST_MATCH_VAL);
	M_free(duped_val);

	return cnt;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_bin_node_t *M_llist_bin_first(const M_llist_bin_t *d)
{
	return (M_llist_bin_node_t *)M_llist_first((const M_llist_t *)d);
}

M_llist_bin_node_t *M_llist_bin_last(const M_llist_bin_t *d)
{
	return (M_llist_bin_node_t *)M_llist_last((const M_llist_t *)d);
}

M_llist_bin_node_t *M_llist_bin_find(const M_llist_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8            *duped_val;
	M_llist_bin_node_t *node;

	duped_val = M_bin_wrap(val, len);
	node      = (M_llist_bin_node_t *)M_llist_find((const M_llist_t *)d, duped_val, M_LLIST_MATCH_VAL);
	M_free(duped_val);

	return node;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint8 *M_llist_bin_take_node(M_llist_bin_node_t *n, size_t *len)
{
	M_uint8 *val;
	M_uint8 *nval;

	if (len != NULL)
		*len = 0;

	val  = M_llist_take_node((M_llist_node_t *)n);
	if (val == NULL)
		return NULL;

	nval = M_bin_unwrapdup(val, len);
	M_free(val);

	return nval;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_bin_remove_node(M_llist_bin_node_t *n)
{
	return M_llist_remove_node((M_llist_node_t *)n);
}

size_t M_llist_bin_remove_val(M_llist_bin_t *d, const M_uint8 *val, size_t len, M_uint32 type)
{
	M_uint8 *duped_val;
	size_t   cnt;

	duped_val = M_bin_wrap(val, len);
	cnt      = M_llist_remove_val((M_llist_t * )d, duped_val, M_llist_bin_convert_match_type(type));
	M_free(duped_val);

	return cnt;
}

void M_llist_bin_remove_duplicates(M_llist_bin_t *d)
{
	M_llist_remove_duplicates((M_llist_t * )d, M_LLIST_MATCH_VAL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_bin_node_t *M_llist_bin_node_next(const M_llist_bin_node_t *n)
{
	return (M_llist_bin_node_t *)M_llist_node_next((const M_llist_node_t *)n);
}

M_llist_bin_node_t *M_llist_bin_node_prev(const M_llist_bin_node_t *n)
{
	return (M_llist_bin_node_t *)M_llist_node_prev((const M_llist_node_t *)n);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const M_uint8 *M_llist_bin_node_val(const M_llist_bin_node_t *n, size_t *len)
{
	return M_bin_unwrap(M_llist_node_val((const M_llist_node_t *)n), len);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_bin_t *M_llist_bin_duplicate(const M_llist_bin_t *d)
{
	return (M_llist_bin_t *)M_llist_duplicate((const M_llist_t *)d);
}

void M_llist_bin_merge(M_llist_bin_t **dest, M_llist_bin_t *src, M_bool include_duplicates)
{
	M_llist_merge((M_llist_t **)dest, (M_llist_t *)src, include_duplicates, M_LLIST_MATCH_VAL);
}

