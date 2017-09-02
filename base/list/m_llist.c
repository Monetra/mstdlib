/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#include <stdlib.h>

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define M_LLIST_START_LEVEL 4
#define M_LLIST_START_LEVEL_ELEMENTS 16

struct M_llist {
	M_sort_compar_t          equality;         /*!< Callback for equality function. */
	void                    *equality_thunk;   /*!< Thunk for equality function (owned by caller, don't delete). */
	M_llist_duplicate_func   duplicate_insert; /*!< Callback for duplicate function */
	M_llist_duplicate_func   duplicate_copy;   /*!< Callback for duplicate function */
	M_llist_free_func        value_free;       /*!< Callback for free function */

	M_llist_flags_t          flags;            /*!< Flags controlling behavior. */

	size_t                   elements;         /*!< Number of elements in the list. */

	union {
		struct {
			M_llist_node_t **head;             /*!< First element in the list. */
			size_t           levels;           /*!< How many levels are in head. */
			M_rand_t        *rand_state;       /*!< State of the random number generator for determining how many
			                                        levels a node should appear in. */ 
		} sorted;
		struct {
			M_llist_node_t  *head;             /*!< First element in the list. */
		} unsorted;
	} head;
	M_llist_node_t          *tail;             /*!< Last element in the list. */
};

struct M_llist_node {
	M_llist_t               *parent;
	void                    *val;

	union {
		struct {
			M_llist_node_t **next;
			M_llist_node_t **prev;
			size_t           levels;
		} sorted;
		struct {
			M_llist_node_t  *next;
			M_llist_node_t  *prev;
		} unsorted;
	} links;
};

typedef enum {
	M_LLIST_INSERT_NODUP   = 0,      /*!< Do not duplicate the value. Store the pointer directly. */
	M_LLIST_INSERT_DUP     = 1 << 0, /*!< Duplicate the value before storing. */
	M_LLIST_INSERT_INITIAL = 1 << 1  /*!< This is an initial insert (not a copy from another list). Use
	                                     The insert duplicate callback and not the copy duplicate callback. */
} M_llist_insert_type_t;

typedef enum {
	M_LLIST_MATCH_OP_COUNT = 0,
	M_LLIST_MATCH_OP_REMOVE
} M_llist_match_op_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Default duplication callback. Pass-thru pointer */
static void *M_llist_duplicate_func_default(const void *arg)
{
	return M_CAST_OFF_CONST(void *, arg);
}

/*! Default free callback. No-Op. */
static void M_llist_free_func_default(void *arg)
{
	(void)arg;
	/* No-op */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Calculate the number of levels necessary for the number of elements in the list. */
static size_t M_llist_max_level(M_llist_t *d)
{
	size_t elements;

	elements = d->elements+1;
	if (elements <= M_LLIST_START_LEVEL_ELEMENTS)
		return M_LLIST_START_LEVEL;
	return M_uint64_log2(M_uint64_round_up_to_power_of_two(elements));
}

/*! The number of levels a node should appear in. */
static size_t M_llist_node_calc_level(M_llist_t *d)
{
	M_uint64 r     = 0;
	M_bool   found = M_FALSE;
	size_t   level = 1;
	size_t   new_max_level;

	new_max_level = M_MIN(d->head.sorted.levels+1, M_llist_max_level(d));
	for (level=0; !found; level++) {
		if (r == 0) {
			r = M_rand(d->head.sorted.rand_state);
		}
		found = (r%2)==0?M_FALSE:M_TRUE;
		r     = r / 2;
	}

	if (level > new_max_level)
		level = new_max_level;
	return level;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_llist_node_t *M_llist_node_create(M_llist_t *d, const void *val, M_llist_insert_type_t insert_type)
{
	M_llist_node_t *node;

	node         = M_malloc_zero(sizeof(*node));
	node->parent = d;

	node->val = M_CAST_OFF_CONST(void *, val);
	if (insert_type & M_LLIST_INSERT_DUP) {
		if (insert_type & M_LLIST_INSERT_INITIAL) {
			node->val = d->duplicate_insert(val);
		} else {
			node->val = d->duplicate_copy(val);
		}
	}

	return node;
}

static void M_llist_node_destory(M_llist_node_t *n, M_bool destroy_val) 
{ 
	if (n == NULL) 
		return; 

	if (destroy_val) { 
		if (destroy_val) 
			n->parent->value_free(n->val); 
	} 
	n->val = NULL; 

	if (n->parent->flags & M_LLIST_SORTED) { 
		n->links.sorted.levels = 0; 
		M_free(n->links.sorted.next); 
		M_free(n->links.sorted.prev); 
	} else { 
		n->links.unsorted.next = NULL; 
		n->links.unsorted.prev = NULL; 
	} 

	n->parent = NULL; 

	M_free(n); 
} 

static void M_llist_node_unlink(M_llist_node_t *n)
{
	M_llist_t      *d;
	M_llist_node_t *next;
	M_llist_node_t *prev;

	if (n == NULL || n->parent->flags & M_LLIST_SORTED)
		return;

	d = n->parent;

	/* Unlink n and relink it's neighbors. */
	next = n->links.unsorted.next;
	prev = n->links.unsorted.prev;
	if (next != NULL)
		next->links.unsorted.prev = prev;
	if (prev != NULL)
		prev->links.unsorted.next = next;

	/* Fix head and tail. */
	if (n == d->head.unsorted.head)
		d->head.unsorted.head = n->links.unsorted.next;
	if (n == d->tail)
		d->tail = n->links.unsorted.prev;

	n->links.unsorted.next = NULL;
	n->links.unsorted.prev = NULL;
}

static M_bool M_llist_insert_sorted(M_llist_node_t *node)
{
	M_llist_t      *d;
	M_llist_node_t *n1 = NULL;
	size_t          i;

	if (node == NULL || !(node->parent->flags & M_LLIST_SORTED))
		return M_FALSE;

	d = node->parent;

	/* Determine how many levels we want this node to use. */
	node->links.sorted.levels = M_llist_node_calc_level(d);

	/* Increase the head levels if necessary. */
	if (d->head.sorted.levels < node->links.sorted.levels) {
		d->head.sorted.levels = node->links.sorted.levels;
		d->head.sorted.head   = M_realloc_zero(d->head.sorted.head, sizeof(*(d->head.sorted.head))*d->head.sorted.levels);
	}

	/* Clear this nodes links in case we're moving it from another list. */
	M_free(node->links.sorted.next);
	M_free(node->links.sorted.prev);
	node->links.sorted.next   = M_malloc_zero(sizeof(*node->links.sorted.next) * node->links.sorted.levels);
	node->links.sorted.prev   = M_malloc_zero(sizeof(*node->links.sorted.prev) * node->links.sorted.levels);

	/* Scan from highest level finding the last node that is less than our current
	 * node's value.  If the chosen level is equal to the current level, chain
	 * the node in.  Continue down levels using the last found node as a starting
	 * point */
	for (i=d->head.sorted.levels; i-->0; ) {
		if (n1 == NULL) {
			/* No starting point inherited from a previous level, set to the
			 * head of the level */
			n1 = d->head.sorted.head[i];

			/* Validate the new node shouldn't come before the current head */
			if (n1 != NULL && d->equality(&(n1->val), &(node->val), d->equality_thunk) > 0)
				n1 = NULL;
		}

		if (n1 != NULL) {
			/* At the current level, scan forward to find the insertion point.  Our
			 * node will be appended to the node found */
			while (n1->links.sorted.next[i] != NULL && d->equality(&(node->val), &(n1->links.sorted.next[i]->val), d->equality_thunk) > 0) {
				n1 = n1->links.sorted.next[i];
			}
		}

		/* Not inserting at this level, just using it to search */
		if (i >= node->links.sorted.levels)
			continue;
		
		if (n1 == NULL) {
			/* Insert to head */
			node->links.sorted.next[i] = d->head.sorted.head[i];
			node->links.sorted.prev[i] = NULL;
			d->head.sorted.head[i]     = node;
		} else {
			/* Chain in */
			node->links.sorted.next[i] = n1->links.sorted.next[i];
			node->links.sorted.prev[i] = n1;
			n1->links.sorted.next[i]   = node;
		}

		if (node->links.sorted.next[i] != NULL) {
			/* Don't forget to chain in prev for the next node */
			node->links.sorted.next[i]->links.sorted.prev[i] = node;
		} else if (i == 0) {
			/* Update tail if necessary */
			d->tail = node;
		}
	}

	d->elements++;
	return M_TRUE;
}

static M_bool M_llist_insert_unsorted(M_llist_node_t *node, M_llist_node_t *after)
{
	M_llist_t *d;

	if (node == NULL || node->parent->flags & M_LLIST_SORTED || (after != NULL && after->parent != node->parent))
		return M_FALSE;

	d = node->parent;

	/* Insert at the front. */
	if (after == NULL) {
		node->links.unsorted.next = d->head.unsorted.head;
		if (node->links.unsorted.next != NULL) {
			node->links.unsorted.next->links.unsorted.prev = node;
		} else if (d->flags & M_LLIST_CIRCULAR) {
			node->links.unsorted.next = node;
		}
		d->head.unsorted.head = node;

		if (d->tail == NULL) {
			d->tail = node;
		}

		if (d->flags & M_LLIST_CIRCULAR) {
			node->links.unsorted.prev    = d->tail;
			d->tail->links.unsorted.next = node;
		}

		d->elements++;
		return M_TRUE;
	}

	/* Insert after. */
	node->links.unsorted.prev  = after;
	node->links.unsorted.next  = after->links.unsorted.next;
	after->links.unsorted.next = node;
	if (node->links.unsorted.next == NULL) {
		d->tail = node;
	} else {
		if (d->flags & M_LLIST_CIRCULAR && node->links.unsorted.next == d->head.unsorted.head) {
			d->tail = node;
		}
		node->links.unsorted.next->links.unsorted.prev = node;
	}

	d->elements++;
	return M_TRUE;
}

static M_bool M_llist_insert_node(M_llist_node_t *node)
{
	if (node == NULL)
		return M_FALSE;
	if (node->parent->flags & M_LLIST_SORTED)
		return M_llist_insert_sorted(node);
	return M_llist_insert_unsorted(node, node->parent->tail);
}

static size_t M_llist_match_op_val_int(M_llist_t *d, const void *val, M_llist_match_op_t type, M_llist_match_op_t match_op)
{
	M_llist_node_t *node;
	M_llist_node_t *first = NULL;
	M_llist_node_t *next;
	M_bool          val_match;
	M_bool          match;
	size_t          cnt = 0;

	if (d == NULL || val == NULL || (!(type & M_LLIST_MATCH_PTR) && d->equality == NULL))
		return 0;

	/* Determine which node we should start matching using. */
	if (d->flags & M_LLIST_SORTED) {
		/* Find is stable so it always returns the first value so we don't have to do any look back here. */
		node = M_llist_find(d, val, type);
	} else {
		first = d->head.unsorted.head;
		node  = first;
	}

	/* Move forward operating on every node we find that matches.
 	 * When sorted this will start with the first matching node.
	 * When unsorted this will start with head.
 	 */
	while (node != NULL) {
		/* Store the next pointer in case we're removing so we know where we're moving to. */
		next      = M_llist_node_next(node);
		val_match = M_FALSE;
		match     = M_FALSE;

		/* If we're sorted or if we're not matching pointers we want to check if the node matches the value of val. */
		if (d->flags & M_LLIST_SORTED || !(type & M_LLIST_MATCH_PTR)) {
			if (d->equality(&val, &(node->val), d->equality_thunk) == 0) {
				val_match = M_TRUE;
			}
		}

		/* If we have a value match or if we're unsorted and we're matching pointers we need to further check
		 * if we have a match. */
		if (val_match || (!(d->flags & M_LLIST_SORTED) && type & M_LLIST_MATCH_PTR)) {
			if (type & M_LLIST_MATCH_PTR) {
				if (val == node->val) {
					match = M_TRUE;
				}
			} else {
				match = M_TRUE;
			}
		}

		/* If we have a match remove if necessary and add to our count. */
		if (match) {
			if (match_op == M_LLIST_MATCH_OP_REMOVE) {
				if (M_llist_remove_node(node)) {
					cnt++;
				}
			} else {
				cnt++;
			}
		}

		/* Values are always adjacent in sorted lists so if we don't have a value match then we've found
		 * all instances of the value so stop iterating. */
		if (d->flags & M_LLIST_SORTED && !val_match) {
			break;
		}

		/* Stop after 1 if we're not counting and not matching all. */
		if (cnt == 1 && !(type & M_LLIST_MATCH_ALL) && match_op != M_LLIST_MATCH_OP_COUNT) {
			break;
		}
		node = next;

		if (node == first) {
			break;
		}
	}
	
	return cnt;
}

static M_bool M_llist_take_remove_node(M_llist_node_t *n, M_bool is_remove, void **val)
{
	M_llist_t *d;
	size_t     i;
	size_t     cnt;
	void      *myval;

	if (val == NULL)
		val = &myval;
	*val = NULL;

	if (n == NULL)
		return M_FALSE;

	d = n->parent;

	if (d->flags & M_LLIST_SORTED) {
		/* Go though each level the node is part of and relink the nodes before and after to not point to this
 		 * node. */
		for (i=n->links.sorted.levels; i-->0; ) {
			if (n->links.sorted.next[i] == NULL) {
				if (i == 0) {
					d->tail = n->links.sorted.prev[i];
				}
			} else {
				n->links.sorted.next[i]->links.sorted.prev[i] = n->links.sorted.prev[i];
			}

			if (n->links.sorted.prev[i] == NULL) {
				d->head.sorted.head[i] = n->links.sorted.next[i];
			} else {
				n->links.sorted.prev[i]->links.sorted.next[i] = n->links.sorted.next[i];
			}
		}

		/* Check head head level and if any are empty remove the level. */
		cnt = d->head.sorted.levels;
		for (i=d->head.sorted.levels; i-->1; ) {
			if (d->head.sorted.head[i] == NULL) {
				cnt--;
			} else {
				break;
			}
		}
		if (cnt < d->head.sorted.levels) {
			d->head.sorted.levels = cnt;
			d->head.sorted.head   = M_realloc(d->head.sorted.head, sizeof(*d->head.sorted.head)*d->head.sorted.levels);
		}
	} else {
		/* Relink the nodes before and after to not point to this node. */
		M_llist_node_unlink(n);
	}

	/* Destroy the node and decrease the number of elements. */
	if (!is_remove)
		*val = n->val;
	M_llist_node_destory(n, is_remove);
	d->elements--;
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_t *M_llist_create(const struct M_llist_callbacks *callbacks, M_uint32 flags)
{
	M_llist_t *d = NULL;

	if (flags & M_LLIST_SORTED && flags & M_LLIST_CIRCULAR)
		return NULL;

	d           = M_malloc_zero(sizeof(*d));
	d->flags    = flags;
	d->elements = 0;
	d->tail     = NULL;

	if (d->flags & M_LLIST_SORTED) {
		d->head.sorted.levels     = M_LLIST_START_LEVEL;
		d->head.sorted.head       = M_malloc_zero(sizeof(*(d->head.sorted.head))*d->head.sorted.levels);
		d->head.sorted.rand_state = M_rand_create(0);
	} else {
		d->head.unsorted.head     = NULL;
	}

	d->equality         = NULL;
	d->duplicate_insert = M_llist_duplicate_func_default;
	d->duplicate_copy   = M_llist_duplicate_func_default;
	d->value_free       = M_llist_free_func_default;
	if (callbacks != NULL) {
		if (callbacks->equality         != NULL) d->equality         = callbacks->equality;
		if (callbacks->duplicate_insert != NULL) d->duplicate_insert = callbacks->duplicate_insert;
		if (callbacks->duplicate_copy   != NULL) d->duplicate_copy   = callbacks->duplicate_copy;
		if (callbacks->value_free       != NULL) d->value_free       = callbacks->value_free;
	}

	return d;
}

M_bool M_llist_change_sorting(M_llist_t *d, M_sort_compar_t equality_cb, void *equality_thunk)
{
	if (d == NULL || (d->flags & M_LLIST_SORTED) == 0 || M_llist_len(d) > 0) {
		return M_FALSE;
	}

	if (equality_cb == NULL && equality_thunk == NULL) {
		return M_FALSE;
	}

	if (equality_cb != NULL) {
		d->equality = equality_cb;
	}
	d->equality_thunk = equality_thunk;

	return M_TRUE;
}

void M_llist_destroy(M_llist_t *d, M_bool destroy_vals)
{
	M_llist_node_t *node  = NULL;
	M_llist_node_t *next;
	M_llist_node_t *first = NULL;

	if (d == NULL)
		return;

	first = M_llist_first(d);
	node  = first;
	while (node != NULL) {
		next = M_llist_node_next(node);
		M_llist_node_destory(node, destroy_vals);
		node = next;
		/* Handle circular lists where th last node points to the first node instead of NULL. */
		if (node == first) {
			break;
		}
	}

	if (d->flags & M_LLIST_SORTED) {
		M_free(d->head.sorted.head);
		M_rand_destroy(d->head.sorted.rand_state);
	}

	M_free(d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_node_t *M_llist_insert(M_llist_t *d, const void *val)
{
	M_llist_node_t *node;
	if (d == NULL || val == NULL)
		return NULL;

	node = M_llist_node_create(d, val, M_LLIST_INSERT_DUP|M_LLIST_INSERT_INITIAL);

	M_llist_insert_node(node);

	return node;
}

M_llist_node_t *M_llist_insert_first(M_llist_t *d, const void *val)
{
	M_llist_node_t *node;

	if (d == NULL || val == NULL || d->flags & M_LLIST_SORTED)
		return NULL;

	node = M_llist_node_create(d, val, M_LLIST_INSERT_DUP|M_LLIST_INSERT_INITIAL);

	M_llist_insert_unsorted(node, NULL);

	return node;
}

M_llist_node_t *M_llist_insert_before(M_llist_node_t *n, const void *val)
{
	M_llist_node_t *node;
	if (n == NULL || val == NULL || n->parent->flags & M_LLIST_SORTED)
		return NULL;

	node = M_llist_node_create(n->parent, val, M_LLIST_INSERT_DUP|M_LLIST_INSERT_INITIAL);

	M_llist_insert_unsorted(node, n->links.unsorted.prev);

	return node;
}

M_llist_node_t *M_llist_insert_after(M_llist_node_t *n, const void *val)
{
	M_llist_node_t *node;
	if (n == NULL || val == NULL || n->parent->flags & M_LLIST_SORTED)
		return NULL;

	node = M_llist_node_create(n->parent, val, M_LLIST_INSERT_DUP|M_LLIST_INSERT_INITIAL);

	M_llist_insert_unsorted(node, n);

	return node;
}

void M_llist_set_first(M_llist_node_t *n)
{
	M_llist_t *d;

	if (n == NULL || n->parent->flags & M_LLIST_SORTED || M_llist_len(n->parent) <= 1)
		return;

	d = n->parent;

	if (n->parent->flags & M_LLIST_CIRCULAR) {
		d->tail               = n->links.unsorted.prev;
		d->head.unsorted.head = n;
		return;
	}

	M_llist_move_before(n, d->head.unsorted.head);
}

void M_llist_set_last(M_llist_node_t *n)
{
	M_llist_t *d;

	if (n == NULL || n->parent->flags & M_LLIST_SORTED || M_llist_len(n->parent) <= 1)
		return;

	d = n->parent;

	if (n->parent->flags & M_LLIST_CIRCULAR) {
		d->head.unsorted.head = n->links.unsorted.next;
		d->tail               = n;
		return;
	}

	M_llist_move_after(n, d->tail);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_move_before(M_llist_node_t *move, M_llist_node_t *before)
{
	M_llist_t      *d;
	M_llist_node_t *prev;

	if (before == NULL || move == NULL || before == move || before->parent != move->parent)
		return M_FALSE;

	d = move->parent;
	if (d->flags & M_LLIST_SORTED)
		return M_FALSE;

	/* Already before? */
	if (move->links.unsorted.next == before)
		return M_TRUE;

	/* Re-link the nodes around move handling head and tail. */
	M_llist_node_unlink(move);
	if (d->head.unsorted.head == before)
		d->head.unsorted.head = move;

	prev = before->links.unsorted.prev;
	/* before's prev points to move.
 	 * move's next points to before.
	 * move's prev points to before's old prev.
	 * before's old prev's next points to move. */
	before->links.unsorted.prev = move;
	move->links.unsorted.next   = before;
	move->links.unsorted.prev   = prev;
	if (prev != NULL)
		prev->links.unsorted.next = move;

	return M_TRUE;
}

M_bool M_llist_move_after(M_llist_node_t *move, M_llist_node_t *after)
{
	M_llist_t      *d;
	M_llist_node_t *next;

	if (after == NULL || move == NULL || after == move || after->parent != move->parent)
		return M_FALSE;

	d = move->parent;
	if (d->flags & M_LLIST_SORTED)
		return M_FALSE;

	/* Already after? */
	if (move->links.unsorted.prev == after)
		return M_TRUE;

	/* Re-link the nodes around move handling head and tail. */
	M_llist_node_unlink(move);
	if (d->tail == after)
		d->tail = move;

	next = after->links.unsorted.next;
	/* after's next points to move.
	 * move's prev points to after.
 	 * move's next points to after's previous next.
	 * after's old next's prev points to move. */
	after->links.unsorted.next = move;
	move->links.unsorted.prev  = after;
	move->links.unsorted.next  = next;
	if (next != NULL)
		next->links.unsorted.prev = move;

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_llist_len(const M_llist_t *d)
{
	if (d == NULL)
		return 0;
	return d->elements;
}

size_t M_llist_count(const M_llist_t *d, const void *val, M_uint32 type)
{
	return M_llist_match_op_val_int(M_CAST_OFF_CONST(M_llist_t *, d), val, type, M_LLIST_MATCH_OP_COUNT);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_node_t *M_llist_first(const M_llist_t *d)
{
	if (d == NULL)
		return NULL;
	if (d->flags & M_LLIST_SORTED) {
		return d->head.sorted.head[0];
	}
	return d->head.unsorted.head;
}

M_llist_node_t *M_llist_last(const M_llist_t *d)
{
	if (d == NULL)
		return NULL;
	return d->tail;
}

M_llist_node_t *M_llist_find(const M_llist_t *d, const void *val, M_uint32 type)
{
	M_llist_node_t *node      = NULL;
	M_llist_node_t *prev      = NULL;
	M_llist_node_t *first     = NULL;
	M_bool          val_match = M_FALSE;
	size_t          i;
	int             eq;

	if (d == NULL || val == NULL || (!(type & M_LLIST_MATCH_PTR) && d->equality == NULL))
		return NULL;

	/* Sorted. */
	if (d->flags & M_LLIST_SORTED) {
		/* Look for a matching node dropping from level to level.
		 * - Start at the highest level looking for a match.
 		 * - Look at each node in the level if the node val is less val than try the next node.
		 *   If the node val is greater than val then go to the previous node.
		 * - At the previous node again start looking forward dropping each time the next node is greater.
		 * - If there is no levels below to drop to then the value doesn't exist in the list.
		 * - When we find a matching value we stop everything and use that node.
		 */
		for (i=d->head.sorted.levels; i-->0; ) {
			if (node == NULL) {
				node = d->head.sorted.head[i];
			}
			if (node == NULL) {
				continue;
			}
			do {
				eq = d->equality(&(node->val), &val, d->equality_thunk);
				if (eq == 0) {
					val_match = M_TRUE;
				} else if (eq > 0) {
					/* We've gone to far, go back and we'll drop. */
					node = node->links.sorted.prev[i];
				} else {
					/* Try moving forward. */
					node = node->links.sorted.next[i];
				}
			} while (node != NULL && eq < 0);
			if (val_match) {
				break;
			}
		}

		/* We couldn't find a match. */
		if (!val_match) {
			return NULL;
		}

		/* Move backward until we find the first matching node. We're moving at the lowest level since that
 		 * has all items.
		 *
		 * We must move backwards at the lowest level because values may not all be at the same height. Meaning
		 * we could have something like this:
		 *
		 * X 
		 * X 1     3
		 * X 1 2 3 3 3 4 5
		 *
		 * At this point we'd have the node for the second 3 because it was found at the second level. We want
		 * to return the fist occurrence of a value in the list. Hence the backwards walking.
		 */
		while (node != NULL) {
			prev = M_llist_node_prev(node);
			if (prev == NULL) {
				break;
			}
			/* We want to match all nodes that match the value because pointer matches may not be adjacent. */
			if (d->equality(&(prev->val), &val, d->equality_thunk) == 0) {
				node = prev;
			} else {
				break;
			}
		}

		/* If we're matching values we have the first value at this point. */
		if (!(type & M_LLIST_MATCH_PTR)) {
			return node;
		}

		/* If we're matching pointers we need to move forward as long as we have matches and check if the pointer
 		 * matches. */
		while (node != NULL) {
			if (val == node->val) {
				return node;
			}

			node = M_llist_node_next(node);
			if (node == NULL) {
				break;
			}

			/* We want to match all nodes that match the value because pointer matches may not be adjacent. */
			if (d->equality(&(node->val), &val, d->equality_thunk) == 0) {
				continue;
			} else {
				break;
			}
		}

		/* Nothing matched. */
		return NULL;
	}

	/* Unsorted. */
	first = d->head.unsorted.head;
	node  = first;
	while (node != NULL) {
		if ((!(type & M_LLIST_MATCH_PTR) && d->equality(&(node->val), &val, d->equality_thunk) == 0) || (type & M_LLIST_MATCH_PTR && val == node->val)) {
			return node;
		}
		node = node->links.unsorted.next;
		if (node == first) {
			break;
		}
	}
	
	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void *M_llist_take_node(M_llist_node_t *n)
{
	void *val;

	if (M_llist_take_remove_node(n, M_FALSE, &val))
		return val;
	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_llist_remove_node(M_llist_node_t *n)
{
	return M_llist_take_remove_node(n, M_TRUE, NULL);
}

size_t M_llist_remove_val(M_llist_t *d, const void *val, M_uint32 type)
{
	return M_llist_match_op_val_int(d, val, type, M_LLIST_MATCH_OP_REMOVE);
}

void M_llist_remove_duplicates(M_llist_t *d, M_uint32 type)
{
	M_llist_node_t *n1;
	M_llist_node_t *n2;
	M_llist_node_t *first;
	M_llist_node_t *next;
	int             eq = 1;

	if (d == NULL || (!(type & M_LLIST_MATCH_PTR) && d->equality == NULL))
		return;

	first = M_llist_first(d);
	n1    = first;
	while (n1 != NULL) {
		if (d->flags & M_LLIST_SORTED) {
			n2 = M_llist_node_next(n1);
		} else {
			n2 = M_llist_first(d);
		}
		while (n2 != NULL) {
			next = M_llist_node_next(n2);
			if (n1 == n2) {
				n2 = next;
				if (n2 == first) {
					break;
				}
				continue;
			}

			/* If we're matching values or if we're sorted we'll need to check for equality. */
			if (!(type & M_LLIST_MATCH_PTR) || d->flags & M_LLIST_SORTED) {
				eq = d->equality(&(n1->val), &(n2->val), d->equality_thunk);
			}
			/* If we're matching values and eq == 0 or we're matching pointers and n2 matches then remove it. */
			if ((!(type & M_LLIST_MATCH_PTR) && eq == 0) || (type & M_LLIST_MATCH_PTR && n1->val == n2->val)) {
				M_llist_remove_node(n2);
			}
			/* If we're sorted and eq != 0 then we don't have a match so stop checking for this value because all
 			 * matching values will be adjacent. */
			if (d->flags & M_LLIST_SORTED && eq != 0) {
				break;
			}

			n2 = next;
			if (n2 == first) {
				break;
			}
		}
		n1 = M_llist_node_next(n1);

		if (n1 == first) {
			break;
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_node_t *M_llist_node_next(const M_llist_node_t *n)
{
	if (n == NULL)
		return NULL;
	if (n->parent->flags & M_LLIST_SORTED) {
		return n->links.sorted.next[0];
	}
	return n->links.unsorted.next;
}

M_llist_node_t *M_llist_node_prev(const M_llist_node_t *n)
{
	if (n == NULL)
		return NULL;
	if (n->parent->flags & M_LLIST_SORTED) {
		return n->links.sorted.prev[0];
	}
	return n->links.unsorted.prev;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void *M_llist_node_val(const M_llist_node_t *n)
{
	if (n == NULL)
		return NULL;
	return n->val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_llist_t *M_llist_duplicate(const M_llist_t *d)
{
	M_llist_t                *dupd;
	M_llist_node_t           *node;
	M_llist_node_t           *first;
	M_llist_node_t           *dupnode;
	struct M_llist_callbacks  callbacks;

	if (d == NULL) {
		return NULL;
	}

	callbacks.equality         = d->equality;
	callbacks.duplicate_insert = d->duplicate_insert;
	callbacks.duplicate_copy   = d->duplicate_copy;
	callbacks.value_free       = d->value_free;
	dupd = M_llist_create(&callbacks, d->flags);

	first = M_llist_first(d);
	node  = first;
	while (node != NULL) {
		dupnode = M_llist_node_create(dupd, node->val, M_LLIST_INSERT_DUP);
		M_llist_insert_node(dupnode);
		node = M_llist_node_next(node);
		if (node == first) {
			break;
		}
	}

	return dupd;
}

void M_llist_merge(M_llist_t **dest, M_llist_t *src, M_bool include_duplicates, M_uint32 type)
{
	M_llist_node_t *first;
	M_llist_node_t *node;

	if (src == NULL) {
		return;
	}
	if (*dest == NULL) {
		*dest = src;
		return;
	}

	first = M_llist_first(src);
	node  = first;
	while (node != NULL) {
		if (!include_duplicates && M_llist_find(*dest, node->val, type) != NULL) {
			node = M_llist_node_next(node);
			if (node == first) {
				break;
			}
			continue;
		}

		/* Move val to a new node inserted into dest. */
		M_llist_insert_node(M_llist_node_create(*dest, node->val, M_LLIST_INSERT_NODUP));
		node->val = NULL;

		node = M_llist_node_next(node);
		if (node == first) {
			break;
		}
	}

	M_llist_destroy(src, M_TRUE);
}
