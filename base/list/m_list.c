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
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define INITIAL_SIZE 4

struct M_list {
	M_sort_compar_t          equality;         /*!< Callback for equality function. */
	M_list_duplicate_func    duplicate_insert; /*!< Callback for duplicate function */
	M_list_duplicate_func    duplicate_copy;   /*!< Callback for duplicate function */
	M_list_free_func         value_free;       /*!< Callback for free function */

	M_list_flags_t           flags;            /*!< Flags controlling behavior. */

	void                   **base;             /*!< Storage for data. */
	void                   **start;            /*!< The start of the acctual data in base. */
	size_t                   elements;         /*!< Number of elements in list. */
	size_t                   allocated;        /*!< Number of allocated spaces in list. Size of base. */

	M_bool                   multi_insert;     /*!< Are we in a multi-insert operation? */
	void                    *thunk;            /*!< Variable passed to equality function. */
};

typedef enum {
	M_LIST_INSERT_NODUP      = 1 << 0, /*!< Do not duplicate the value. Store the pointer directly. */
	M_LIST_INSERT_DUP        = 1 << 1, /*!< Duplicate the value before storing. */
	M_LIST_INSERT_INITIAL    = 1 << 2, /*!< This is an initial insert (not a copy from another list). Use
	                                        The insert duplicate callback and not the copy duplicate callback. */
	M_LIST_INSERT_NOSETCHECK = 1 << 3  /*!< The value is guarenteed not to be in the list already. Skip set checks
	                                        because they've already been run. */
} M_list_insert_type_t;

typedef enum {
	M_LIST_MATCH_OP_COUNT = 0,
	M_LIST_MATCH_OP_REMOVE,
	M_LIST_MATCH_OP_REPLACE
} M_list_match_op_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Default duplication callback. Pass-thru pointer */
static void *M_list_duplicate_func_default(const void *arg)
{
	return M_CAST_OFF_CONST(void *, arg);
}

/*! Default free callback. No-Op. */
static void M_list_free_func_default(void *arg)
{
	(void)arg;
	/* No-op */
}

/*! Convert an index for a STACK (starts at the end not the front) to the internal
 * index that elements are stored at. */
static __inline__ size_t M_list_convert_idx_at(const M_list_t *d, size_t idx)
{
	if (d == NULL || d->elements == 0)
		return 0;
	if (idx > d->elements)
		idx = d->elements-1;
	if (!(d->flags & M_LIST_STACK))
		return idx;
	if (d->elements-1 == idx)
		return 0;
	return d->elements-idx-1;
}

static __inline__ size_t M_list_convert_idx_insert(const M_list_t *d, size_t idx)
{
	size_t new_idx;

	if (d == NULL || d->elements == 0)
		return 0;
	new_idx = M_list_convert_idx_at(d, idx);
	if (!(d->flags & M_LIST_STACK))
		return new_idx;
	return new_idx+1;
}

static void M_list_shift_data(M_list_t *d, M_bool force)
{
	if (d == NULL)
		return;

	/* Reset the offset if we don't have any items. It doesn't make sense to put them in at an offset
 	 * if there is nothing to offset. */
	if (d->elements == 0)
		d->start = d->base;

	/* If we're going to resize move the elements to the beginning of base if the data doesn't already
 	 * start at the beginning of base. */
	if (d->start != d->base) {
		/* Move the elements to the beginning of base if the we don't have any room at the end. */
		if (force || (size_t)(d->start-d->base)+d->elements == d->allocated) {
			M_mem_move(d->base, d->start, d->elements*sizeof(*d->base));
			d->start = d->base;
		}
	}
}

/*! Grows the array if necessary. */
static void M_list_grow(M_list_t *d)
{
	if (d == NULL) {
		return;
	}

	M_list_shift_data(d, M_FALSE);
	if (d->elements == d->allocated) {
		d->allocated <<= 1;
		d->base        = M_realloc(d->base, sizeof(*d->base)*d->allocated);
		d->start       = d->base;
	}
}

static void M_list_shrink(M_list_t *d)
{
	size_t reduced_size;
	size_t max_elements;

	if (d == NULL || (d->flags & M_LIST_NEVERSHRINK))
		return;

	/* We reduce by half but only when doing so leaves us with at least INITIAL_SIZE allocated space
 	 * and we have 25% free space left in the list. */
	reduced_size = d->allocated >> 1;
	max_elements = (size_t)((double)reduced_size / 1.25);
	if (reduced_size >= INITIAL_SIZE && d->elements <= max_elements) {
		M_list_shift_data(d, M_TRUE);
		d->allocated = reduced_size;
		d->base      = M_realloc(d->base, sizeof(*d->base)*d->allocated);
		d->start     = d->base;
	}
}

/* Insert into a given position. */
static M_bool M_list_insert_at_int(M_list_t *d, const void *val, size_t idx, M_list_insert_type_t insert_type)
{
	void *myval;

	if (d == NULL ||
		(!(insert_type & M_LIST_INSERT_NOSETCHECK) && d->flags & (M_LIST_SET_VAL|M_LIST_SET_PTR) &&
			M_list_index_of(d, val, (d->flags&M_LIST_SET_PTR)?M_LIST_MATCH_PTR:M_LIST_MATCH_VAL, NULL)))
	{
		return M_FALSE;
	}

	M_list_grow(d);

	myval = M_CAST_OFF_CONST(void *, val);
	if (insert_type & M_LIST_INSERT_DUP) {
		if (insert_type & M_LIST_INSERT_INITIAL) {
			myval = d->duplicate_insert(val);
		} else {
			myval = d->duplicate_copy(val);
		}
	}

	/* Treat any index larger than the number of elements as an append */
	if (idx > d->elements) {
		idx = d->elements;
	} else if (idx < d->elements) {
		/* If we are inserting at the front and we have unused space before the first element then use that
 		 * space instead of moving elements around. */
		if (idx == 0 && d->start > d->base) {
			d->start--;
		/* Otherwise move the existing elements out of the way to make room */
		} else {
			M_mem_move(d->start+(idx+1), d->start+(idx), (d->elements-idx)*sizeof(*d->start));
		}
	}
	d->start[idx] = myval;
	d->elements++;

	return M_TRUE;
}

/* Function for operations relating to values:
 *   - count
 *   - remove
 *   - replace
 */
static size_t M_list_match_op_val_int(M_list_t *d, const void *val, const void *new_val, M_list_match_type_t type, M_list_match_op_t match_op)
{
	void                 *ptr = NULL;
	size_t                idx;
	size_t                start;
	size_t                end;
	size_t                i;
	size_t                cnt = 0;
	M_list_insert_type_t  insert_type;

	if (d == NULL || val == NULL || d->elements == 0 || (!(type & M_LIST_MATCH_PTR) && d->equality == NULL) ||
		(match_op == M_LIST_MATCH_OP_REPLACE &&
			(new_val == NULL || (type & M_LIST_MATCH_PTR && val == new_val) ||
			(!(type & M_LIST_MATCH_PTR) && d->equality(&val, &new_val, d->thunk) == 0))))
	{
		return 0;
	}

	/* If we're replacing in a SET and the replace value already exists in the list we can't add it so we'll turn
 	 * the replace into a remove */
	if (match_op == M_LIST_MATCH_OP_REPLACE && 
		(d->flags & (M_LIST_SET_VAL|M_LIST_SET_PTR) &&
			M_list_index_of(d, new_val, (d->flags&M_LIST_SET_PTR)?M_LIST_MATCH_PTR:M_LIST_MATCH_VAL, NULL)))
	{
		match_op = M_LIST_MATCH_OP_REMOVE;
	}

	/* Sorted.
 	 *
	 * Logic for Remove:
	 *   - Use index_of to find a matching element. This works with both values and pointer match types.
	 *   - If index_of doesn't find a match then the value doesn't exist in the list and we're done.
	 *   - index_of gives us 1 match. However, it does not guarantee where it is. Meaning if there are
	 *     three matching elements index_of could give us any of them.
	 *   - Being sorted we're guaranteed that all of the matching values are adjacent. So we can determine
	 *     a range to remove for all values when matching values only
	 *   - However, we're not guaranteed that matching pointers are adjacent. So we cannot determine
	 *     a range to remove for all values when matching pointers.
	 *   - Look at elements before and after until we hit an element that doesn't match the value.
	 *   - When matching values we'll track the first and last element we found in the list and remove the range.
	 *   - When matching pointers we'll remove them as we encounter them.
	 *
	 * Logic for Replace:
	 *   - Follow Remove logic.
	 *   - Since the elements are sorted replacing doesn't necessarily mean the new value will be in the same
	 *     place as the old.
	 *   - We know how many elements we've removed so we'll add that may to the list.
	 */
	if ((d->flags & M_LIST_SORTED) && !d->multi_insert) {
		/* We're using index_of here as a convience. It will ensure we either have a match or not. */
		if (!M_list_index_of(d, val, type, &idx)) {
			return 0;
		}
		/* Even if we're matching pointers the idx will be correct due to using index_of. */
		start = idx;
		end   = idx;
		cnt   = 1;

		if ((type & M_LIST_MATCH_ALL) || match_op == M_LIST_MATCH_OP_COUNT) {
			/* Look for matches after. */
			for (i=idx+1; i<d->elements; i++) {
				if (d->equality(&val, &d->start[i], d->thunk) != 0) {
					break;
				}
				if (type & M_LIST_MATCH_PTR) {
					if (val == d->start[i]) {
						if (match_op == M_LIST_MATCH_OP_REMOVE || match_op == M_LIST_MATCH_OP_REPLACE) {
							M_list_remove_at(d, i);
							i--;
						}
						cnt++;
					}
				} else {
					end = i;
				}
			}
			/* look for matches before. */
			for (i=idx; i-->0; ) {
				if (d->equality(&val, &d->start[i], d->thunk) != 0) {
					break;
				}
				if (type & M_LIST_MATCH_PTR) {
					if (val == d->start[i]) {
						if (match_op == M_LIST_MATCH_OP_REMOVE || match_op == M_LIST_MATCH_OP_REPLACE) {
							M_list_remove_at(d, i);
						}
						cnt++;
					}
				} else {
					start = i;
				}
			}
		}

		/* Add the matched range of elements to the count when matching values. */
		if (!(type & M_LIST_MATCH_PTR)) {
			cnt += end-start;
		}

		/* Remove the values. This will remove the range of values when not matching pointers. When matching
 		 * pointers we've already removed the matches as we found them because they may not have been adjacent. */
		if (match_op == M_LIST_MATCH_OP_REMOVE || match_op == M_LIST_MATCH_OP_REPLACE) {
			M_list_remove_range(d, start, end);
		}

		/* If we're replacing add the new value the number of times val was removed. */
		if (match_op == M_LIST_MATCH_OP_REPLACE) {
			/* Same value and sorted so the inserted values should all be together. */
			ptr         = d->duplicate_insert(new_val);
			insert_type = M_LIST_INSERT_INITIAL|M_LIST_INSERT_NODUP|M_LIST_INSERT_NOSETCHECK;
			idx         = M_list_convert_idx_insert(d, M_list_insert_idx(d, ptr));
			for (i=0; i<cnt; i++) {
				if (i > 0) {
					/* Copy the already-inserted value rather than calling duplicate_insert()
					 * again as duplicate_insert() may just be a direct pointer cast */
					ptr         = d->duplicate_copy(ptr);
					insert_type = M_LIST_INSERT_NODUP|M_LIST_INSERT_NOSETCHECK;
					/* Put the value after the last one that was inserted. This ensures if we're inserting
 					 * at the end we continue to do so and prevent M_mem_moves. */
					idx++;
				}
				if (!M_list_insert_at_int(d, ptr, idx, insert_type)) {
					d->value_free(ptr);
					break;
				}
			}
		}

		return cnt;
	}

	/* Not sorted. */
	for (i=d->elements; i-->0; ) {
		if ((!(type & M_LIST_MATCH_PTR) && d->equality(&val, &d->start[i], d->thunk) == 0) || (type & M_LIST_MATCH_PTR && val == d->start[i])) {
			if (match_op == M_LIST_MATCH_OP_REPLACE) {
				if (cnt == 0) {
					ptr = d->duplicate_insert(new_val);
				} else {
					/* Copy the already-inserted value rather than calling duplicate_insert()
					 * again as duplicate_insert() may just be a direct pointer cast */
					ptr = d->duplicate_copy(ptr);
				}
				d->value_free(d->start[i]);
				d->start[i] = ptr;
			} else if (match_op == M_LIST_MATCH_OP_REMOVE) {
				M_list_remove_at(d, i);
			}
			cnt++;
		}
		/* Stop after the first removal/replacement if not dealing with all and not counting. */
		if (cnt == 1 && !(type & M_LIST_MATCH_ALL) && match_op != M_LIST_MATCH_OP_COUNT) {
			break;
		}
	}

	return cnt;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_t *M_list_create(const struct M_list_callbacks *callbacks, M_uint32 flags)
{
	M_list_t *d = NULL;

	/* A sorted list requires an equality function and can't be used with a stack.
 	 * A set based on value needs an equality function. */
	if ((flags & M_LIST_SORTED && (callbacks == NULL || callbacks->equality == NULL || flags & M_LIST_STACK)) ||
	   (flags & M_LIST_SET_VAL && (callbacks == NULL || callbacks->equality == NULL)))
	{
		return NULL;
	}

	d                   = M_malloc(sizeof(*d));
	M_mem_set(d, 0, sizeof(*d));
	d->flags            = flags;
	d->base             = M_malloc(sizeof(*d->base)*INITIAL_SIZE);
	d->start            = d->base;
	d->elements         = 0;
	d->allocated        = INITIAL_SIZE;
	d->multi_insert     = M_FALSE;
	d->thunk            = NULL;

	d->equality         = NULL;
	d->duplicate_insert = M_list_duplicate_func_default;
	d->duplicate_copy   = M_list_duplicate_func_default;
	d->value_free       = M_list_free_func_default;
	if (callbacks != NULL) {
		if (callbacks->equality         != NULL) d->equality         = callbacks->equality;
		if (callbacks->duplicate_insert != NULL) d->duplicate_insert = callbacks->duplicate_insert;
		if (callbacks->duplicate_copy   != NULL) d->duplicate_copy   = callbacks->duplicate_copy;
		if (callbacks->value_free       != NULL) d->value_free       = callbacks->value_free;
	}

	return d;
}

void M_list_destroy(M_list_t *d, M_bool destroy_vals)
{
	size_t i;

	if (d == NULL)
		return;

	if (destroy_vals) {
		for (i=0; i<d->elements; i++) {
			d->value_free(d->start[i]);
		}
	}
	M_free(d->base);
	M_free(d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_list_change_sorting(M_list_t *d, M_sort_compar_t equality, M_uint32 sorted_flags, void *thunk)
{
	/* Can't sort NULL and can't sort stacks. */
	if (d == NULL || (sorted_flags & M_LIST_SORTED && d->flags & M_LIST_STACK))
		return;

	/* Cannot sort without an equality function. */
	if (equality == NULL && sorted_flags & M_LIST_SORTED)
		return;

	/* Nothing is changing */
	if ((sorted_flags & M_LIST_SORTED) == (d->flags & M_LIST_SORTED)     &&
			(sorted_flags & M_LIST_STABLE) == (d->flags & M_LIST_STABLE) &&
			equality == d->equality)
	{
		return;
	}

	d->equality   = equality;
	/* Set the sorting flags. We'll remove it (whether it was set or not) and add set it if necessary. */
	/* Need to cast to the M_list_flags_t when taking the complement of M_LIST_SORTED to silence a gcc warning. */
	d->flags     &= ~((M_list_flags_t)M_LIST_SORTED|M_LIST_STABLE);
	if (sorted_flags & M_LIST_SORTED)
		d->flags |= M_LIST_SORTED;
	if (sorted_flags & M_LIST_STABLE)
		d->flags |= M_LIST_STABLE;
	d->thunk      = thunk;

	/* If the list is now a sorted list, sort it. */
	if ((d->flags & M_LIST_SORTED) && d->elements != 0) {
		if (d->flags & M_LIST_STABLE) {
			M_sort_mergesort(d->start, d->elements, sizeof(*d->start), d->equality, d->thunk);
		} else {
			M_sort_qsort(d->start, d->elements, sizeof(*d->start), d->equality, d->thunk);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_list_insert(M_list_t *d, const void *val)
{
	if (d == NULL)
		return M_FALSE;

	return M_list_insert_at_int(d, val, M_list_convert_idx_insert(d, M_list_insert_idx(d, val)), M_LIST_INSERT_INITIAL|M_LIST_INSERT_DUP);
}

size_t M_list_insert_idx(const M_list_t *d, const void *val)
{
	size_t at;
	M_bool stable;

	if (d == NULL)
		return 0;

	/* Figure out where we need to insert */
	if ((d->flags & M_LIST_SORTED) && !d->multi_insert) {
		stable = (d->flags&M_LIST_STABLE)?M_TRUE:M_FALSE;
		at = M_sort_binary_insert_idx(d->start, d->elements, sizeof(*d->start), val, stable, d->equality, d->thunk);
	/* Non-sorted so insert always appends */
	} else {
		at = d->elements;
	}

	return M_list_convert_idx_insert(d, at);
}

M_bool M_list_insert_at(M_list_t *d, const void *val, size_t idx)
{
	if (d == NULL)
		return M_FALSE;

	if ((d->flags & M_LIST_SORTED) && !d->multi_insert) {
		return M_FALSE;
	}
	return M_list_insert_at_int(d, val, M_list_convert_idx_at(d, idx), M_LIST_INSERT_INITIAL|M_LIST_INSERT_DUP);
}

void M_list_insert_begin(M_list_t *d)
{
	if (d == NULL) {
		return;
	}
	d->multi_insert = M_TRUE;
}

void M_list_insert_end(M_list_t *d)
{
	if (d == NULL || !d->multi_insert) {
		return;
	}
	if ((d->flags & M_LIST_SORTED) && d->elements != 0) {
		if (d->flags & M_LIST_STABLE) {
			M_sort_mergesort(d->start, d->elements, sizeof(*d->start), d->equality, d->thunk);
		} else {
			M_sort_qsort(d->start, d->elements, sizeof(*d->start), d->equality, d->thunk);
		}
	}
	d->multi_insert = M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_list_len(const M_list_t *d)
{
	if (d == NULL) {
		return 0;
	}
	return d->elements;
}

size_t M_list_count(const M_list_t *d, const void *val, M_uint32 type)
{
	return M_list_match_op_val_int(M_CAST_OFF_CONST(M_list_t *, d), val, NULL, type, M_LIST_MATCH_OP_COUNT);
}

M_bool M_list_index_of(const M_list_t *d, const void *val, M_uint32 type, size_t *idx)
{
	size_t i;
	size_t myidx;
	M_bool stable;
	M_bool ret;

	if (d == NULL || val == NULL || d->elements == 0 || (!(type & M_LIST_MATCH_PTR) && d->equality == NULL)) {
		return M_FALSE;
	}

	if (idx == NULL) {
		idx = &myidx;
	}
	*idx = 0;

	/* Sorted. */
	if ((d->flags & M_LIST_SORTED) && !d->multi_insert) {
		stable = (d->flags&M_LIST_STABLE)?M_TRUE:M_FALSE;
		ret = M_sort_binary_search(d->start, d->elements, sizeof(*d->start), val, stable, d->equality, d->thunk, idx);

		if (ret && (type & M_LIST_MATCH_PTR)) {
			ret = M_FALSE;
			/* Check the pointer against the match. */
			if (val == d->start[*idx]) {
				ret = M_TRUE;
			}
			/* Scan up if we don't have a match for entries that also match the value. */
			if (!ret) {
				for (i=(*idx)+1; i<d->elements; i++) {
					if (d->equality(&val, &d->start[i], NULL) != 0) {
						break;
					}
					if (val == d->start[i]) {
						*idx = i;
						ret = M_TRUE;
						break;
					}
				}
			}
			/* Scan down if we don't have a match for entries that also match the value. */
			if (!ret) {
				for (i=*idx; i-->0; ) {
					if (d->equality(&val, &d->start[i], NULL) != 0) {
						break;
					}
					if (val == d->start[i]) {
						*idx = i;
						ret = M_TRUE;
						break;
					}
				}
			}
		}

		if (ret) {
			*idx = M_list_convert_idx_at(d, *idx);
		}
		return ret;
	}

	/* Not sorted. */
	for (i=0; i<d->elements; i++) {
		if (type & M_LIST_MATCH_PTR) {
			if (val == d->start[i]) {
				*idx = M_list_convert_idx_at(d, i);
				return M_TRUE;
			}
		} else {
			if (d->equality(&val, &d->start[i], NULL) == 0) {
				*idx = M_list_convert_idx_at(d, i);
				return M_TRUE;
			}
		}
	}
	return M_FALSE;
}

const void *M_list_first(const M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return NULL;
	return M_list_at(d, 0); 
}

const void *M_list_last(const M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return NULL;
	return M_list_at(d, d->elements-1); 
}

const void *M_list_at(const M_list_t *d, size_t idx)
{
	if (d == NULL || idx >= d->elements)
		return NULL;
	return d->start[M_list_convert_idx_at(d, idx)];
}

void *M_list_take_first(M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return NULL;
	return M_list_take_at(d, 0);
}

void *M_list_take_last(M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return NULL;
	return M_list_take_at(d, d->elements-1);
}

void *M_list_take_at(M_list_t *d, size_t idx)
{
	void *val;

	if (d == NULL || idx >= d->elements) {
		return NULL;
	}

	idx = M_list_convert_idx_at(d, idx);
	val = d->start[idx];
	d->start[idx] = NULL;

	/* 1. Remove from front, increases the start offset.
 	 * 2. Remove from middle, causes a hole so M_mem_move to fill it.
	 * 3. Remove from end, don't need to do anything.
 	 */
	if (idx == 0) {
		d->start++;
	} else if (idx != d->elements-1) {
		M_mem_move(d->start+(idx), d->start+(idx+1), (d->elements-idx-1)*sizeof(*d->start));
	}
	d->elements--;

	M_list_shrink(d);
	return val;
}

M_bool M_list_remove_first(M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return M_FALSE;
	return M_list_remove_at(d, 0);
}

M_bool M_list_remove_last(M_list_t *d)
{
	if (d == NULL || d->elements == 0)
		return M_FALSE;
	return M_list_remove_at(d, d->elements-1);
}

M_bool M_list_remove_at(M_list_t *d, size_t idx)
{
	if (d == NULL || idx >= d->elements) {
		return M_FALSE;
	}
	return M_list_remove_range(d, idx, idx);
}

size_t M_list_remove_val(M_list_t *d, const void *val, M_uint32 type)
{
	return M_list_match_op_val_int(d, val, NULL, type, M_LIST_MATCH_OP_REMOVE);
}

M_bool M_list_remove_range(M_list_t *d, size_t start, size_t end)
{
	size_t i;
	M_bool remove_to_end = M_FALSE;

	if (d == NULL || start > end || start >= d->elements)
		return M_FALSE;

	start = M_list_convert_idx_at(d, start);
	end   = M_list_convert_idx_at(d, end);

	if (end >= d->elements)
		end = d->elements-1;

	if (end == d->elements-1)
		remove_to_end = M_TRUE;

	/* should be i<=end but gcc has a bug that warns that this will cause an
	 * infinite loop. Using i<end+1 (which is equivalent) does not produce this
	 * warning. */
	for (i=start; i<end+1; i++) {
		d->value_free(d->start[i]);
	}
	M_mem_set(d->start+(start), 0, (end-start+1)*sizeof(*d->start));

	d->elements -= end-start+1;
	if (d->elements > 0) {
		/* 1. Remove from front, increases the start offset.
		 * 2. Remove from middle, causes a hole so M_mem_move to fill it.
		 * 3. Remove from end, don't need to do anything.
		 */
		if (start == 0) {
			d->start += end+1;
		} else if (!remove_to_end) {
			M_mem_move(d->start+(start), d->start+(end+1), (d->elements-start)*sizeof(*d->start));
		}
	}

	M_list_shrink(d);
	return M_TRUE;
}

void M_list_remove_duplicates(M_list_t *d, M_uint32 type)
{
	size_t i;
	size_t j;

	if (d == NULL || (!(type & M_LIST_MATCH_PTR) && d->equality == NULL)) {
		return;
	}

	/* O(n) performance because we're already sorted. */
	if ((d->flags & M_LIST_SORTED) && !d->multi_insert) {
		for (i=0; i<d->elements; i++) {
			if (type & M_LIST_MATCH_PTR) {
				/* Matching pointers means we have to find each individual match and remove it. */
				/* Only look forward until we run out of matches because matches will be adjacent. */
				for (j=i+1; j<d->elements && d->equality(&d->start[i], &d->start[j], NULL) == 0; j++) {
					/* Only remove if the pointer actually matches. */
					if (d->start[i] == d->start[j]) {
						M_list_remove_at(d, M_list_convert_idx_at(d, j));
						j--;
					}
				}
			} else {
				/* Matching values means we can find where the value starts and ends an only remove the range. */
				for (j=i+1; j<d->elements && d->equality(&d->start[i], &d->start[j], NULL) == 0; j++)
					;
				/* We want to do a bulk remove since the duplicates will be contiguous and one move
				 * operation will be faster than multiple. */
				if (j > i)
					M_list_remove_range(d, M_list_convert_idx_at(d, i+1), M_list_convert_idx_at(d, j));
			}
		}
	/* O(n^2) because we're unsorted so insertion order is assumed and we want to maintain the insertion order. */
	} else {
		for (i=0; i<d->elements; i++) {
			for (j=i+1; j<d->elements; j++) {
				if ((!(type & M_LIST_MATCH_PTR) && d->equality(&d->start[i], &d->start[j], NULL) == 0) || (type & M_LIST_MATCH_PTR && d->start[i] == d->start[j])) {
					M_list_remove_at(d, M_list_convert_idx_at(d, j));
					/* Need to ensure we don't skip anything. We've removed item at j and the item after
 					 * as shifted to fill j. We need to decrement j so the for loop will increment j and
					 * bring it back to what it needs to be. */
					j--;
				}
			}
		}
	}
}

size_t M_list_replace_val(M_list_t *d, const void *val, const void *new_val, M_uint32 type)
{
	return M_list_match_op_val_int(d, val, new_val, type, M_LIST_MATCH_OP_REPLACE);
}

M_bool M_list_replace_at(M_list_t *d, const void *val, size_t idx)
{
	if (d == NULL || idx >= d->elements || ((d->flags & M_LIST_SORTED) && !d->multi_insert)) {
		return M_FALSE;
	}

	idx = M_list_convert_idx_at(d, idx);

	d->value_free(d->start[idx]);
	d->start[idx] = d->duplicate_insert(val);
	return M_TRUE;
}

M_bool M_list_swap(M_list_t *d, size_t idx1, size_t idx2)
{
	void *temp;

	if (d == NULL || d->elements == 0 || (d->flags & M_LIST_SORTED) || idx1 == idx2 || idx1 > d->elements || idx2 > d->elements)
		return M_FALSE;

	temp           = d->start[idx1];
	d->start[idx1] = d->start[idx2];
	d->start[idx2] = temp;

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_t *M_list_duplicate(const M_list_t *d)
{
	size_t                    i;
	M_list_t                *dupd;
	struct M_list_callbacks  callbacks;

	if (d == NULL) {
		return NULL;
	}

	callbacks.equality         = d->equality;
	callbacks.duplicate_insert = d->duplicate_insert;
	callbacks.duplicate_copy   = d->duplicate_copy;
	callbacks.value_free       = d->value_free;
	dupd = M_list_create(&callbacks, d->flags);

	M_list_insert_begin(dupd);
	for (i=0; i<d->elements; i++) {
		M_list_insert_at_int(dupd, d->start[i], M_list_convert_idx_insert(dupd, M_list_insert_idx(dupd, d->start[i])), M_LIST_INSERT_DUP|M_LIST_INSERT_NOSETCHECK);
	}
	M_list_insert_end(dupd);

	return dupd;
}

/* dest will be created if *dest == NULL. src will be freed. Any values within
 * src that are not moved to dest will be freed.
 */
static void M_list_merge_int(M_list_t **dest, M_list_t *src, M_list_t *dups, M_bool include_duplicates, M_uint32 type)
{
	void                    *val;
	size_t                   i;
	struct M_list_callbacks  callbacks;

	if (src == NULL) {
		return;
	}

	if ((*dest == NULL || M_list_len(*dest) == 0) && dups == NULL) {
		M_list_destroy(*dest, M_TRUE);
		*dest = src;
		return;
	}

	if (*dest == NULL) {
		callbacks.equality         = src->equality;
		callbacks.duplicate_insert = src->duplicate_insert;
		callbacks.duplicate_copy   = src->duplicate_copy;
		callbacks.value_free       = src->value_free;
		*dest = M_list_create(&callbacks, src->flags);
	}

	/* If dest is a set we're never going to include duplicates. */
	if ((*dest)->flags & (M_LIST_SET_VAL|M_LIST_SET_PTR)) {
		include_duplicates = M_FALSE;
		type               = ((*dest)->flags&M_LIST_SET_PTR)?M_LIST_MATCH_PTR:M_LIST_MATCH_VAL;
	}

	M_list_insert_begin(*dest);
	for (i=0; i<src->elements; i++) {
		val = src->start[i];
		if (!include_duplicates && dups != NULL) {
			if (M_list_index_of(dups, val, type, NULL)) {
				src->value_free(val);
				continue;
			}
		}
		M_list_insert_at_int(*dest, val, (*dest)->elements, M_LIST_INSERT_NODUP|M_LIST_INSERT_NOSETCHECK);
	}
	M_list_insert_end(*dest);

	M_list_destroy(src, M_FALSE);
}

void M_list_merge(M_list_t **dest, M_list_t *src, M_bool include_duplicates, M_uint32 type)
{
	M_list_t *dtemp = NULL;

	if (src == NULL) {
		return;
	}

	if (*dest == NULL || M_list_len(*dest) == 0) {
		M_list_destroy(*dest, M_TRUE);
		*dest = src;
		return;
	}

	/* If we're sorted and we don't want duplicates we need special handing.
	 * We don't want to modify dest until we've checked all duplicates.
	 * Modifying dest will cause us to fall back to using a linear search
	 * for duplicates. We use a temporary to allow the dup checking to use
	 * the faster binary search since dest isn't modified.
	 */
	if ((src->flags & M_LIST_SORTED) && !include_duplicates) {
		/* Move src values into a temporary list checking dest for duplicates. */
		M_list_merge_int(&dtemp, src, *dest, include_duplicates, type);
		/* Move the values from the temp list into dest. This will cause dest
		 * to use a bulk insertion. But since we've already filtered duplicates
		 * we don't have to worry about the linear search. */
		M_list_merge_int(dest, dtemp, NULL, M_TRUE, type);
	} else {
		/* Otherwise it's unsorted so if we care about duplicates it's going to
		 * be a linear search because it's unsorted.
		 * Or it's sorted and we don't care about duplicates so there won't be
		 * any searching.
		 */
		M_list_merge_int(dest, src, *dest, include_duplicates, type);
	}
}
