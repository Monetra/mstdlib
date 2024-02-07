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

#ifndef __M_LLIST_BIN_H__
#define __M_LLIST_BIN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_llist_bin Linked List - Binary
 *  \ingroup m_llist
 *
 * Linked list for storing values.
 *
 * The list can be used in multiple ways:
 * - Unsorted.
 * - Sorted.
 * - Queue (FIFO) (really just unsorted).
 * - Priority Queue (really just sorted).
 *
 * A linked list is not indexable. Iteration and find are supported.
 *
 * Sorted notes:
 * - Sorting is implemented as a skip list. This should provide near O(long(n)) performance.
 *   Performance nearing a sorted M_list_t.
 * - Sorting is stable. If an element with a matching value is already in the list then it
 *   will be inserted after. Find will always find the first matching element in the list.
 *
 * @{
 */

struct M_llist_bin;
/* Currently a direct map to M_list private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary */
typedef struct M_llist_bin M_llist_bin_t;

struct M_llist_bin_node;
/* Currently a direct map to M_list private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary */
typedef struct M_llist_bin_node M_llist_bin_node_t;


/*! Flags for controlling the behavior of the list. */
typedef enum {
    M_LLIST_BIN_NONE     = 0,     /*!< List mode. */
    M_LLIST_BIN_CIRCULAR = 1 << 0 /*!< Circular list. */
} M_llist_bin_flags_t;


/*! Type of matching that should be used when searching/modifying a value in the list. */
typedef enum {
    M_LLIST_BIN_MATCH_VAL = 0,      /*!< Match based on the value. */
    M_LLIST_BIN_MATCH_ALL = 1 << 0  /*!< Include all instances. */
} M_llist_bin_match_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new list.
 *
 * A list is a linked list. The list can be, optionally, kept in sorted
 * order. The sorted order is determined by the flags.
 *
 *  \param[in] flags M_llist_bin_flags_t flags controlling behavior.
 *
 * \return Allocated linked list.
 *
 * \see M_llist_bin_destroy
 */
M_API M_llist_bin_t *M_llist_bin_create(M_uint32 flags) M_MALLOC;


/*! Destroy the list.
 *
 * \param[in] d The llist to destory.
 */
M_API void M_llist_bin_destroy(M_llist_bin_t *d) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert a value into the list.
 *
 * If sorted the value will be inserted in sorted order. Otherwise it will be
 * appended to the end of the list.
 *
 * \param[in,out] d   The list.
 * \param[in]     val The value to insert.
 * \param[in]     len The length of val.
 *
 * \return Pointer to M_llist_bin_node_t container object of new node on success,
 *         otherwise NULL.
 *
 * \see m_llist_bin_insert_first
 */
M_API M_llist_bin_node_t *M_llist_bin_insert(M_llist_bin_t *d, const M_uint8 *val, size_t len);


/*! Insert a value into the list as the first node.
 *
 * Only applies to unsorted lists.
 *
 * \param[in,out] d   The list.
 * \param[in]     val The value to insert.
 * \param[in]     len The length of val.
 *
 * \return Pointer to M_llist_bin_node_t container object of new node on success,
 *         otherwise NULL.
 *
 * \see M_llist_bin_insert
 */
M_API M_llist_bin_node_t *M_llist_bin_insert_first(M_llist_bin_t *d, const M_uint8 *val, size_t len);


/*! Insert a value into the list before a given node.
 *
 * Only applies to unsorted lists.
 *
 * \param[in,out] n   The node to insert before. Cannot be NULL.
 * \param[in]     val The value to insert.
 * \param[in]     len The length of val.
 *
 * \return Pointer to M_llist_bin_node_t container object of new node on success,
 *         otherwise NULL.
 *
 * \see M_llist_bin_insert_after
 */
M_API M_llist_bin_node_t *M_llist_bin_insert_before(M_llist_bin_node_t *n, const M_uint8 *val, size_t len);


/*! Insert a value into the list after a given node.
 *
 * Only applies to unsorted lists.
 *
 * \param[in,out] n   The node to insert after. Cannot be NULL.
 * \param[in]     val The value to insert.
 * \param[in]     len The length of val.
 *
 * \return Pointer to M_llist_bin_node_t container object of new node on success,
 *         otherwise NULL.
 *
 * \see M_llist_bin_insert_before
 */
M_API M_llist_bin_node_t *M_llist_bin_insert_after(M_llist_bin_node_t *n, const M_uint8 *val, size_t len);


/*! Set the node as the first node in the circular list.
 *
 * Only applies to circular lists.
 *
 * \param[in] n The node that should be considered first.
 */
M_API void M_llist_bin_set_first(M_llist_bin_node_t *n);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Move a node before another node in the list.
 *
 * \param[in] move   The node to move.
 * \param[in] before The node that move should be placed before.
 *
 * \return M_TRUE on sucess, otherwise M_FALSE.
 */
M_API M_bool M_llist_bin_move_before(M_llist_bin_node_t *move, M_llist_bin_node_t *before);


/*! Move a node after another node in the list.
 *
 * \param[in] move  The node to move.
 * \param[in] after The node that move should be placed after.
 *
 * \return M_TRUE on sucess, otherwise M_FALSE.
 */
M_API M_bool M_llist_bin_move_after(M_llist_bin_node_t *move, M_llist_bin_node_t *after);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The length of the list.
 *
 * \param[in] d The list.
 *
 * \return the length of the list.
 */
M_API size_t M_llist_bin_len(const M_llist_bin_t *d);


/*! Count the number of times a value occurs in the list.
 *
 * \param[in] d   The list.
 * \param[in] val The value to search for.
 * \param[in]     len The length of val.
 *
 *
 * \return The number of times val appears in the list.
 */
M_API size_t M_llist_bin_count(const M_llist_bin_t *d, const M_uint8 *val, size_t len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the first node in the list.
 *
 * \param[in] d The list.
 *
 * \return Node or NULL.
 *
 * \see M_llist_bin_last
 * \see M_llist_bin_find
 */
M_API M_llist_bin_node_t *M_llist_bin_first(const M_llist_bin_t *d);


/*! Get the last node in the list.
 *
 * \param[in] d The list.
 *
 * \return Node or NULL.
 *
 * \see M_llist_bin_first
 * \see M_llist_bin_find
 */
M_API M_llist_bin_node_t *M_llist_bin_last(const M_llist_bin_t *d);


/*! Find a node for the given value in the list.
 *
 * \param[in] d   The list.
 * \param[in] val The value to search for.
 * \param[in] len The length of val.
 *
 *
 * \return Node or NULL.
 *
 * \see M_llist_bin_first
 * \see M_llist_bin_last
 */
M_API M_llist_bin_node_t *M_llist_bin_find(const M_llist_bin_t *d, const M_uint8 *val, size_t len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Take the node from the list and return its value.
 *
 * The element will be removed from the list and its value returned. The caller is
 * responsible for freeing the value.
 *
 * \param[in]  n   The node.
 * \param[out] len The length of val.
 *
 * \return The node's value.
 *
 * \see M_llist_bin_node_val
 */
M_API M_uint8 *M_llist_bin_take_node(M_llist_bin_node_t *n, size_t *len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Remove a node from the list.
 *
 * The value will be free'd using the value_free callback.
 *
 * \param[in] n The node.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 *
 * \see M_llist_bin_remove_val
 */
M_API M_bool M_llist_bin_remove_node(M_llist_bin_node_t *n);


/*! Remove node(s) from the list matching a given value.
 *
 * The value will be free'd using the value_free callback.
 *
 * \param[in,out] d    The list.
 * \param[in]     val  The value to search for.
 * \param[in]     len  The length of val.
 * \param[in]     type M_llist_bin_match_type_t type of how the val should be matched.
 *                     valid values are:
 *                     - M_LLIST_BIN_MATCH_VAL (removes one/first)
 *                     - M_LLIST_BIN_MATCH_ALL
 *
 * \return M_TRUE on success otherwise M_FALSE.
 *
 * \see M_llist_bin_remove_node
 */
M_API size_t M_llist_bin_remove_val(M_llist_bin_t *d, const M_uint8 *val, size_t len, M_uint32 type);


/*! Remove duplicate values from the list.
 *
 * \param[in] d The list.
 */
M_API void M_llist_bin_remove_duplicates(M_llist_bin_t *d);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the next node, the one after a given node.
 *
 * \param[in] n The node.
 *
 * \return Node or NULL.
 *
 * \see M_llist_bin_node_prev
 */
M_API M_llist_bin_node_t *M_llist_bin_node_next(const M_llist_bin_node_t *n);


/*! Get the previous node, the one before a given node.
 *
 * \param[in] n The node.
 *
 * \return Node or NULL.
 *
 * \see M_llist_bin_node_next
 */
M_API M_llist_bin_node_t *M_llist_bin_node_prev(const M_llist_bin_node_t *n);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the value for a node.
 *
 * \param[in]  n   The node.
 * \param[out] len The length of val.
 *
 *
 * \return The node's value.
 *
 * \see M_llist_bin_take_node
 */
M_API const M_uint8 *M_llist_bin_node_val(const M_llist_bin_node_t *n, size_t *len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate an existing list. Will copy all elements of the list.
 *
 * \param[in] d list to duplicate.
 *
 * \return New list.
 */
M_API M_llist_bin_t *M_llist_bin_duplicate(const M_llist_bin_t *d) M_MALLOC;


/*! Merge two lists together.
 *
 * The second (src) list will be destroyed automatically upon completion of this function. Any value pointers for the
 * list will be directly copied over to the destination list, they will not be duplicated.
 *
 * \param[in,out] dest               Pointer by reference to the list receiving the values.
 *                                   if this is NULL, the pointer will simply be switched out for src.
 * \param[in,out] src                Pointer to the list giving up its values.
 * \param[in]     include_duplicates When M_TRUE any values in 'dest' that also exist in
 *                                   'src' will be included in 'dest'. When M_FALSE any
 *                                   duplicate values will not be added to 'dest'.
 */
M_API void M_llist_bin_merge(M_llist_bin_t **dest, M_llist_bin_t *src, M_bool include_duplicates) M_FREE(2);

/*! @} */

__END_DECLS

#endif /* __M_LLIST_BIN_H__ */
