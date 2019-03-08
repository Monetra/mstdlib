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

static M_list_match_type_t M_list_bin_convert_match_type(M_list_bin_match_type_t type)
{
	M_list_match_type_t ltype = M_LIST_MATCH_VAL;

	if (type & M_LIST_BIN_MATCH_ALL) {
		ltype = M_LIST_MATCH_ALL;
	}

	return ltype;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_bin_t *M_list_bin_create(M_uint32 flags)
{
	M_list_flags_t          lflags    = M_LIST_NONE;
	struct M_list_callbacks callbacks = {
		M_sort_compar_binwraped,
		NULL, /* The wrapper will manage copying for insert. */
		M_bin_wrapeddup_vp,
		M_free
	};

	if (flags & M_LIST_BIN_STACK)
		lflags |= M_LIST_STACK;
	if (flags & M_LIST_BIN_SET)
		lflags |= M_LIST_SET_VAL;
	if (flags & M_LIST_BIN_NEVERSHRINK)
		lflags |= M_LIST_NEVERSHRINK;

	/* We are only dealing in opaque types here, and we don't have any
	 * metadata of our own to store, so we are only casting one pointer
	 * type to another.  This is a safe operation */
	return (M_list_bin_t *)M_list_create(&callbacks, lflags);
}

void M_list_bin_destroy(M_list_bin_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_list_bin_insert(M_list_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return M_list_insert((M_list_t *)d, duped_val);
}

size_t M_list_bin_insert_idx(const M_list_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val;
	size_t   idx;

	duped_val = M_bin_wrap(val, len);
	idx       = M_list_insert_idx((const M_list_t *)d, duped_val);
	M_free(duped_val);

	return idx;
}

M_bool M_list_bin_insert_at(M_list_bin_t *d, const M_uint8 *val, size_t len, size_t idx)
{
	M_uint8 *duped_val = M_bin_wrap(val, len);
	return M_list_insert_at((M_list_t *)d, duped_val, idx);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_list_bin_len(const M_list_bin_t *d)
{
	return M_list_len((const M_list_t *)d);
}

size_t M_list_bin_count(const M_list_bin_t *d, const M_uint8 *val, size_t len)
{
	M_uint8 *duped_val;
	size_t   cnt;

	duped_val = M_bin_wrap(val, len);
	cnt       = M_list_count((const M_list_t *)d, duped_val, M_LIST_MATCH_VAL);
	M_free(duped_val);

	return cnt;
}

M_bool M_list_bin_index_of(const M_list_bin_t *d, const M_uint8 *val, size_t len, size_t *idx)
{
	M_uint8 *duped_val;
	M_bool   ret;

	duped_val = M_bin_wrap(val, len);
	ret       = M_list_index_of((const M_list_t *)d, duped_val, M_LIST_MATCH_VAL, idx);
	M_free(duped_val);

	return ret;
}

static const M_uint8 *M_list_bin_peek_int(const M_uint8 *val, size_t *len)
{
	return M_bin_unwrap(val, len);
}

const M_uint8 *M_list_bin_first(const M_list_bin_t *d, size_t *len)
{
	return M_list_bin_peek_int(M_list_first((const M_list_t *)d), len);
}

const M_uint8 *M_list_bin_last(const M_list_bin_t *d, size_t *len)
{
	return M_list_bin_peek_int(M_list_last((const M_list_t *)d), len);
}

const M_uint8 *M_list_bin_at(const M_list_bin_t *d, size_t idx, size_t *len)
{
	return M_list_bin_peek_int(M_list_at((const M_list_t *)d, idx), len);
}

static M_uint8 *M_list_bin_take_int(M_uint8 *val, size_t *len)
{
	M_uint8 *nval;

	if (len != NULL)
		*len = 0;
	if (val == NULL)
		return NULL;

	nval = M_bin_unwrapdup(val, len);
	M_free(val);
	return nval;
}

M_uint8 *M_list_bin_take_first(M_list_bin_t *d, size_t *len)
{
	return M_list_bin_take_int(M_list_take_first((M_list_t *)d), len);
}

M_uint8 *M_list_bin_take_last(M_list_bin_t *d, size_t *len)
{
	return M_list_bin_take_int(M_list_take_last((M_list_t *)d), len);
}

M_uint8 *M_list_bin_take_at(M_list_bin_t *d, size_t idx, size_t *len)
{
	return M_list_bin_take_int(M_list_take_at((M_list_t *)d, idx), len);
}

M_bool M_list_bin_remove_first(M_list_bin_t *d)
{
	return M_list_remove_first((M_list_t *)d);
}

M_bool M_list_bin_remove_last(M_list_bin_t *d)
{
	return M_list_remove_last((M_list_t *)d);
}

M_bool M_list_bin_remove_at(M_list_bin_t *d, size_t idx)
{
	return M_list_remove_at((M_list_t *)d, idx);
}

size_t M_list_bin_remove_val(M_list_bin_t *d, const M_uint8 *val, size_t len, M_uint32 type)
{
	M_uint8 *duped_val;
	size_t   ret;

	duped_val = M_bin_wrap(val, len);
	ret       = M_list_remove_val((M_list_t *)d, duped_val, M_list_bin_convert_match_type(type));
	M_free(duped_val);
	return ret;
}

M_bool M_list_bin_remove_range(M_list_bin_t *d, size_t start, size_t end)
{
	return M_list_remove_range((M_list_t *)d, start, end);
}

void M_list_bin_remove_duplicates(M_list_bin_t *d)
{
	M_list_remove_duplicates((M_list_t *)d, M_LIST_MATCH_VAL);
}

size_t M_list_bin_replace_val(M_list_bin_t *d, const M_uint8 *val, size_t len, const M_uint8 *new_val, size_t new_len, M_uint32 type)
{
	M_uint8 *duped_val;
	M_uint8 *new_duped_val;
	size_t   cnt;

	/* Wrap val as the comparison function within M_list_replace_val is going
	 * to compare against an already wrapped stored value */
	duped_val     = M_bin_wrap(val, len);
	new_duped_val = M_bin_wrap(new_val, new_len);

	cnt = M_list_replace_val((M_list_t *)d, duped_val, new_duped_val, M_list_bin_convert_match_type(type));
	M_free(duped_val);

	if (cnt == 0)
		M_free(new_duped_val);
	return cnt;
}

M_bool M_list_bin_replace_at(M_list_bin_t *d, const M_uint8 *val, size_t len, size_t idx)
{
	M_uint8 *duped_val;
	M_bool   ret;

	duped_val = M_bin_wrap(val, len);
	ret       = M_list_replace_at((M_list_t *)d, duped_val, idx);
	if (!ret)
		M_free(duped_val);

	return ret;
}

M_bool M_list_bin_swap(M_list_bin_t *d, size_t idx1, size_t idx2)
{
	return M_list_swap((M_list_t *)d, idx1, idx2);
}

M_list_bin_t *M_list_bin_duplicate(const M_list_bin_t *d)
{
	return (M_list_bin_t *)M_list_duplicate((const M_list_t *)d);
}

void M_list_bin_merge(M_list_bin_t **dest, M_list_bin_t *src, M_bool include_duplicates)
{
	M_list_merge((M_list_t **)dest, (M_list_t *)src, include_duplicates, M_LIST_MATCH_VAL);
}
