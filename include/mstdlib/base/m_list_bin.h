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

#ifndef __M_LIST_BIN_H__
#define __M_LIST_BIN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_list_bin List - Binary
 *  \ingroup m_list
 * 
 * Dynamic list (array) for storing binary values.
 *
 * References to the data will always be read-only.
 * All items will be duplicated by the list.
 *
 * The list can be used in multiple ways:
 * - Unsorted.
 * - Sorted.
 * - Queue (FIFO) (really just unsorted).
 * - Stack (LIFO) (which cannot be sorted).
 * - Set.
 *
 * A list is indexable. Find is also supported.
 *
 * Indexes in the list are 0 at head to len-1 at end (head ... end).
 * Functions like M_list_first will return head and M_list_last will return end.
 *
 * The index start changes in STACK mode. In STACK mode indexing is opposite.
 * Head is len-1 and end is 0 (head ... end). Entries are still added to end.
 * Functions like M_list_first will return end and M_list_last will return head.
 * This is to accommodate STACKS where entries are inserted and removed from
 * the same end.
 *
 * The list is designed for efficient head removal. A value removed from head
 * will not cause a memmove. Instead a start offset will be noted. If there is
 * space before head (due to removals) then additions at head will be efficient
 * as the empty space will be used and a memmove will be avoided. memmoves
 * will occur when the size (not necessary number of elements) of the list
 * changes (expand and shrink) and for removals in the middle of the list.
 *
 * Sorted notes:
 * - Sorting on insert and find (M_list_bin_index_of()) is done using binary insert/search.
 * - When M_list_insert_end() is called after M_list_insert_begin() qsort will be
 *   used to sort the list.
 *
 * @{
 */

struct M_list_bin;
/* Currently a direct map to M_list private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary */
typedef struct M_list_bin M_list_bin_t;


/*! Flags for controlling the behavior of the list. */
typedef enum {
	M_LIST_BIN_NONE        = 1 << 0, /*!< Not sorting, asc compare. */
	M_LIST_BIN_STACK       = 1 << 1, /*!< Last in First out mode. */
	M_LIST_BIN_SET         = 1 << 2, /*!< Don't allow duplicates in the list.
	                                      Insert is increased by an additional O(n) operation (on top of the insert
	                                      itself) in order to determine if a value is a duplicate for unsorted.
	                                      Insert is increased by an additional O(log(n)) operation (on top of the
	                                      insert itself) in order to determine if a value is a duplicate for sorted. */
	M_LIST_BIN_NEVERSHRINK = 1 << 3  /*!< Never allow the list to shrink. */
} M_list_bin_flags_t;


/*! Type of matching that should be used when searching/modifying a value in the list. */
typedef enum {
	M_LIST_BIN_MATCH_VAL = 0,      /*!< Match based on the value (equality function). */
	M_LIST_BIN_MATCH_ALL = 1 << 0  /*!< Include all instances. */
} M_list_bin_match_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new dynamic list.
 *
 * A dynamic list is a dynamically expanding array. Meaning the array will expand to accommodate new elements.
 * The list can be, optionally, kept in sorted order. 
 *
 * \param[in] flags M_list_bin_flags_t flags for controlling behavior.
 *
 * \return Allocated dynamic list for storing binary data.
 *
 * \see M_list_bin_destroy
 */
M_API M_list_bin_t *M_list_bin_create(M_uint32 flags) M_MALLOC;


/*! Destory the list.
 *
 * \param[in] d The list to destory.
 */
M_API void M_list_bin_destroy(M_list_bin_t *d) M_FREE(1);


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
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_list_bin_insert(M_list_bin_t *d, const M_uint8 *val, size_t len);


/*! Get the index a value would be insert into the list at.
 *
 * This does not actually insert the value into the list it only gets the position the value would
 * be insert into the list if/when insert is called.
 *
 * \param[in] d   The list.
 * \param[in] val The value to get the insertion index for.
 * \param[in] len The length of val.
 *
 * \return The insertion index.
 */
M_API size_t M_list_bin_insert_idx(const M_list_bin_t *d, const M_uint8 *val, size_t len);


/*! Insert a value into the list at a specific position.
 *
 * This is only supported for non-sorted lists.
 *
 * \param[in,out] d   The list.
 * \param[in]     val The value to insert.
 * \param[in]     len The length of val.
 * \param[in]     idx The position to insert at. An index larger than the number of
 *                    elements in the list will result in the item being inserted
 *                    at the end.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_list_bin_insert_at(M_list_bin_t *d, const M_uint8 *val, size_t len, size_t idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The length of the list.
 *
 * \param[in] d The list.
 *
 * \return the length of the list.
 */
M_API size_t M_list_bin_len(const M_list_bin_t *d);


/*! Count the number of times a value occurs in the list.
 *
 * \param[in] d   The list.
 * \param[in] val The value to search for.
 * \param[in] len The length of val.
 *
 * \return The number of times val appears in the list.
 */
M_API size_t M_list_bin_count(const M_list_bin_t *d, const M_uint8 *val, size_t len);


/*! Get the location of a value within the list.
 *
 * This will return a location in the list which may not be the first occurrence in the list.
 *
 * \param[in]  d   The list.
 * \param[in]  val The value to search for.
 * \param[in]  len The length of val.
 * \param[out] idx The index of the value within the list. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if the value was found within the list. Otherwise M_FALSE.
 */
M_API M_bool M_list_bin_index_of(const M_list_bin_t *d, const M_uint8 *val, size_t len, size_t *idx);


/*! Get the first element.
 *
 * The element will remain a member of the list.
 *
 * \param[in]  d   The list.
 * \param[out] len The length of val.
 *
 * \return The element or NULL if there are no elements.
 *
 * \see M_list_bin_at
 * \see M_list_bin_last
 */
M_API const M_uint8 *M_list_bin_first(const M_list_bin_t *d, size_t *len);


/*! Get the last element.
 *
 * The element will remain a member of the list.
 *
 * \param[in]  d   The list.
 * \param[out] len The length of val.
 *
 * \return The element or NULL if there are no elements.
 *
 * \see M_list_at
 * \see M_list_first
 */
M_API const M_uint8 *M_list_bin_last(const M_list_bin_t *d, size_t *len);


/*! Get the element at a given index.
 *
 * The element will remain a member of the list.
 *
 * \param[in]  d   The list.
 * \param[in]  idx The location to retrieve the element from.
 * \param[out] len The length of val.
 *
 * \return The element or NULL if index is out range.
 *
 * \see M_list_bin_first
 * \see M_list_bin_last
 */
M_API const M_uint8 *M_list_bin_at(const M_list_bin_t *d, size_t idx, size_t *len);


/*! Take the first element.
 *
 * The element will be removed from the list and returned. The caller is
 * responsible for freeing the element.
 *
 * \param[in,out] d   The list.
 * \param[out]    len The length of val.
 *
 * \return The element or NULL if there are no elements.
 *
 * \see M_list_bin_take_at
 * \see M_list_bin_last
 */
M_API M_uint8 *M_list_bin_take_first(M_list_bin_t *d, size_t *len);


/*! Take the last element.
 *
 * The element will be removed from the list and returned. The caller is
 * responsible for freeing the element.
 *
 * \param[in,out] d   The list.
 * \param[out]    len The length of val.
 *
 * \return The element or NULL if there are no elements.
 *
 * \see M_list_bin_take_at
 * \see M_list_bin_take_first
 */
M_API M_uint8 *M_list_bin_take_last(M_list_bin_t *d, size_t *len);


/*! Take the element at a given index.
 *
 * The element will be removed from the list and returned. The caller is
 * responsible for freeing the element.
 *
 * \param[in,out] d   The list.
 * \param[in]     idx The location to retrieve the element from.
 * \param[out]    len The length of val.
 *
 * \return The element or NULL if index is out range.
 *
 * \see M_list_bin_take_first
 * \see M_list_bin_take_last
 */
M_API M_uint8 *M_list_bin_take_at(M_list_bin_t *d, size_t idx, size_t *len);


/*! Remove the first element.
 *
 * \param[in,out] d The list.
 *
 * \return M_TRUE if the element was removed. Otherwise M_FALSE.
 *
 * \see M_list_bin_remove_at
 * \see M_list_bin_remove_last
 */
M_API M_bool M_list_bin_remove_first(M_list_bin_t *d);


/*! Remove the last element.
 *
 * \param[in,out] d The list.
 *
 * \return M_TRUE if the element was removed. Otherwise M_FALSE.
 *
 * \see M_list_bin_remove_at
 * \see M_list_bin_remove_first
 */
M_API M_bool M_list_bin_remove_last(M_list_bin_t *d);


/*! Remove an element at a given index from the list.
 *
 * \param[in,out] d   The list.
 * \param[in]     idx The index to remove.
 *
 * \return M_TRUE if the element was removed. Otherwise M_FALSE.
 *
 * \ see M_list_bin_remove_first
 * \ see M_list_bin_remove_last
 * \ see M_list_bin_remove_val
 * \ see M_list_bin_remove_range
 */
M_API M_bool M_list_bin_remove_at(M_list_bin_t *d, size_t idx);


/*! Remove element(s) from the list.
 *
 * Searches the list for the occurrence of val and removes it from the
 * list. The value will be free'd using the value_free callback.
 *
 * Requires the equality callback to be set.
 *
 * \param[in,out] d    The list.
 * \param[in]     val  The val to remove.
 * \param[out]    len  The length of val.
 * \param[in]     type M_list_bin_match_type_t type of how the val should be matched.
 *
 * \return The number of elements removed.
 *
 * \see M_list_bin_remove_at
 */
M_API size_t M_list_bin_remove_val(M_list_bin_t *d, const M_uint8 *val, size_t len, M_uint32 type);


/*! Remove a range of elements form the list.
 *
 * \param[in,out] d     The list.
 * \param[in]     start The start index. Inclusive.
 * \param[in]     end   The end index. Inclusive.
 *
 * \return M_TRUE if the range was removed. Otherwise M_FALSE.
 *
 * \see M_list_bin_remove_at
 */
M_API M_bool M_list_bin_remove_range(M_list_bin_t *d, size_t start, size_t end);


/*! Remove duplicate elements from the list.
 *
 * \param[in] d The list.
 */
M_API void M_list_bin_remove_duplicates(M_list_bin_t *d);


/*! Replace all matching values in the list with a different value.
 *
 * \param[in,out] d       The list.
 * \param[in]     val     The val to be replaced.
 * \param[in]     len     The length of val.
 * \param[in]     new_val The value to be replaced with.
 * \param[in]     new_len The length of new_val.
 * \param[in]     type    M_list_bin_match_type_t type of how the val should be matched.
 *
 * \return The number of elements replaced.
 */
M_API size_t M_list_bin_replace_val(M_list_bin_t *d, const M_uint8 *val, size_t len, const M_uint8 *new_val, size_t new_len, M_uint32 type);


/*! Replace a value in the list with a different value.
 *
 * \param[in,out] d   The list.
 * \param[in]     val The val to that will appear in the list at the given idx.
 * \param[in]     len The length of val.
 * \param[in]     idx The index to replace.
 *
 * \return M_TRUE if the value was replaced. Otherwise M_FALSE.
 */
M_API M_bool M_list_bin_replace_at(M_list_bin_t *d, const M_uint8 *val, size_t len, size_t idx);


/*! Exchange the elements at the given locations.
 *
 * This only applies to unsorted lists.
 *
 * \param[in,out] d    The list.
 * \param[in]     idx1 The first index.
 * \param[in]     idx2 The second index.
 *
 * \return M_TRUE if the elements were swapped.
 */
M_API M_bool M_list_bin_swap(M_list_bin_t *d, size_t idx1, size_t idx2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate an existing list.
 *
 * Will copy all elements of the list as well as any flags, etc.
 *
 * \param[in] d list to duplicate.
 *
 * \return New list.
 */
M_API M_list_bin_t *M_list_bin_duplicate(const M_list_bin_t *d) M_MALLOC;


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
M_API void M_list_bin_merge(M_list_bin_t **dest, M_list_bin_t *src, M_bool include_duplicates) M_FREE(2);

/*! @} */

__END_DECLS

#endif /* __M_LIST_BIN_H__ */

