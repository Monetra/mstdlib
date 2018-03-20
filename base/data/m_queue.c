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

#include <mstdlib/mstdlib.h>


struct M_queue {
	M_llist_t     *list;
	M_hashtable_t *hash;
};


M_queue_t *M_queue_create(M_sort_compar_t sort_cb, void (*free_cb)(void *))
{
	struct M_llist_callbacks callbacks = {
		sort_cb, /* quality          */
		NULL,    /* duplicate_insert */
		NULL,    /* duplicate_copy   */
		free_cb  /* value_free       */
	};
	M_queue_t *queue = M_malloc_zero(sizeof(*queue));

	queue->list      = M_llist_create((sort_cb || free_cb)?&callbacks:NULL, (sort_cb)?M_LLIST_SORTED:M_LLIST_NONE);
	queue->hash      = M_hashtable_create(16, 75, M_hash_func_hash_vp, M_sort_compar_vp, M_HASHTABLE_NONE, NULL);
	return queue;
}


void M_queue_destroy(M_queue_t *queue)
{
	if (queue == NULL)
		return;

	M_llist_destroy(queue->list, M_TRUE);
	M_hashtable_destroy(queue->hash, M_TRUE);
	M_free(queue);
}


M_bool M_queue_insert(M_queue_t *queue, void *member)
{
	M_llist_node_t *node;

	if (queue == NULL || member == NULL)
		return M_FALSE;

	/* Make sure entry doesn't already exist */
	if (M_hashtable_get(queue->hash, member, NULL))
		return M_FALSE;

	node = M_llist_insert(queue->list, member);
	M_hashtable_insert(queue->hash, member, node);
	return M_TRUE;
}


M_bool M_queue_remove(M_queue_t *queue, void *member)
{
	M_llist_node_t *node;

	if (queue == NULL || member == NULL)
		return M_FALSE;

	/* Make sure entry doesn't already exist */
	if (!M_hashtable_get(queue->hash, member, (void **)&node))
		return M_FALSE;

	M_llist_remove_node(node);
	M_hashtable_remove(queue->hash, member, M_TRUE /* Doesn't actually matter, no cb registered */);
	return M_TRUE;
}


M_bool M_queue_take(M_queue_t *queue, void *member)
{
	M_llist_node_t *node;

	if (queue == NULL || member == NULL)
		return M_FALSE;

	/* Make sure entry doesn't already exist */
	if (!M_hashtable_get(queue->hash, member, (void **)&node))
		return M_FALSE;

	(void)M_llist_take_node(node);
	M_hashtable_remove(queue->hash, member, M_TRUE /* Doesn't actually matter, no cb registered */);
	return M_TRUE;
}


M_bool M_queue_exists(M_queue_t *queue, void *member)
{
	M_llist_node_t *node;

	if (queue == NULL || member == NULL)
		return M_FALSE;

	if (!M_hashtable_get(queue->hash, member, (void **)&node))
		return M_FALSE;

	return M_TRUE;
}


size_t M_queue_len(M_queue_t *queue)
{
	if (queue == NULL)
		return M_FALSE;

	return M_llist_len(queue->list);
}


void *M_queue_first(M_queue_t *queue)
{
	M_llist_node_t *node;

	if (queue == NULL)
		return NULL;

	node = M_llist_first(queue->list);
	if (node == NULL)
		return NULL;

	return M_llist_node_val(node);
}


void *M_queue_take_first(M_queue_t *queue)
{
	M_llist_node_t *node;
	void           *member;

	if (queue == NULL)
		return NULL;

	/* Grab first entry */
	node = M_llist_first(queue->list);
	if (node == NULL)
		return NULL;

	/* Remove it from linked list and grab the member */
	member = M_llist_take_node(node);

	/* Remove it from hashtable */
	M_hashtable_remove(queue->hash, member, M_TRUE /* Doesn't actually matter, no cb registered */);

	return member;
}


void *M_queue_last(M_queue_t *queue)
{
	M_llist_node_t *node;

	if (queue == NULL)
		return NULL;

	node = M_llist_last(queue->list);
	if (node == NULL)
		return NULL;

	return M_llist_node_val(node);
}


M_bool M_queue_foreach(M_queue_t *queue, M_queue_foreach_t **q_foreach, void **member)
{
	if (queue == NULL || q_foreach == NULL || member == NULL)
		return M_FALSE;

	*member = NULL;

	if (*q_foreach == NULL) {
		*q_foreach = (M_queue_foreach_t *)M_llist_first(queue->list);

		/* No elements in list */
		if (*q_foreach == NULL)
			return M_FALSE;
	}

	/* Check for end of list by checking for invalid pointer */
	if ((M_uintptr)(*q_foreach) == M_UINTPTR_MAX) {
		*q_foreach = NULL;
		return M_FALSE;
	}

	*member    = M_llist_node_val((M_llist_node_t *)(*q_foreach));
	*q_foreach = (M_queue_foreach_t *)M_llist_node_next((M_llist_node_t *)(*q_foreach));
	/* If we're at the end, set the pointer to an invalid address as an indicator */
	if (*q_foreach == NULL) {
		*q_foreach = (M_queue_foreach_t *)M_UINTPTR_MAX;
	}

	return M_TRUE;
}


void M_queue_foreach_free(M_queue_foreach_t *q_foreach)
{
	/* Current implementation is a no-op since we alias M_llist_node_t * */
	(void)q_foreach;
}
