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

#ifndef __M_QUEUE_H__
#define __M_QUEUE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_sort.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_queue Queue
 *  \ingroup m_datastructures
 *
 * Queue, meant for storing a list of pointers in either insertion order or
 * sorted as per a user-defined callback.  The usage is very similar to that of
 * an M_llist_t, and infact an M_llist_t is used as one of the backing data types,
 * but it also uses an M_hashtable_t in order to make lookups and removals of
 * queue members by pointer O(1) rather than O(log n) for sorted lists
 * or O(n) for unsorted lists.
 *
 * Usage Example:
 *
 * \code{.c}
 *     M_queue_t         *queue;
 *     void              *member    = NULL;
 *     M_queue_foreach_t *q_foreach = NULL;
 *
 *     queue = M_queue_create(M_sort_compar_str, M_free);
 *     M_queue_insert(queue, M_strdup("b. hello world"));
 *     M_queue_insert(queue, M_strdup("c. goodbye"));
 *     M_queue_insert(queue, M_strdup("a! -- I should end up enumerated first"));
 *     M_printf("queue members: %zu\n", M_queue_len(queue));
 *
 *     while (M_queue_foreach(queue, &q_foreach, &member)) {
 *         M_printf("removing member: %s\n", (const char *)member);
 *         M_queue_remove(queue, member);
 *     }
 *
 *     M_printf("queue members: %zu\n", M_queue_len(queue));
 *     M_queue_destroy(queue);
 * \endcode
 *
 * @{
 */

__BEGIN_DECLS

struct M_queue;
/*! Data type used as the main queue object */
typedef struct M_queue M_queue_t;

struct M_queue_foreach;
/*! Data type used for enumeration of a queue */
typedef struct M_queue_foreach M_queue_foreach_t;

/*! Create a queue (list of objects) that stores user-provided pointers.  The pointers
 *  stored may be kept in insertion order or sorted, depending on how the queue is
 *  initialized.
 * \param sort_cb  If the pointers should be stored in a sorted order, register this
 *                 callback with the routine for sorting.  If insertion order is
 *                 desired, pass NULL.
 * \param free_cb  Upon removal of a pointer from the queue, this callback will be
 *                 called.  This applies to M_queue_destroy() and M_queue_remove().
 *                 If no free callback desired, pass NULL.
 * \return Allocated M_queue_t * on success that should be free'd with M_queue_destroy(),
 *         otherwise NULL.
 */
M_API M_queue_t *M_queue_create(M_sort_compar_t sort_cb, void (*free_cb)(void *));


/*! Destroy's an initialized M_queue_t * object.
 * \param queue Initialized queue object returned by M_queue_create()
 */
M_API void M_queue_destroy(M_queue_t *queue);


/*! Insert a user-supplied queue object (pointer) into the queue.  The object specified
 *  should not already be in the queue.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \param member User-supplied queue object (pointer) to insert, must not be NULL.
 * \return M_TRUE on success, M_FALSE on failure such as if the user-supplied object
 *         already exists in the queue */
M_API M_bool M_queue_insert(M_queue_t *queue, void *member);


/*! Remove a user-supplied queue object (pointer) from the queue.  If M_queue_create()
 *  registered the free_cb, the free_cb will be called upon removal.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \param member User-supplied queue object (pointer) to remove.
 * \return M_TRUE on success, M_FALSE on failure such as object not found.
 */
M_API M_bool M_queue_remove(M_queue_t *queue, const void *member);


/*! See if queue member still exists.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \param member User-supplied queue object (pointer) to find.
 * \return M_TRUE if member exists, M_FALSE otherwise.
 */
M_API M_bool M_queue_exists(const M_queue_t *queue, const void *member);


/*! Take control of a user-supplied queue object (pointer).  This will remove the
 *  object from the list without freeing the object (assuming free_cb was registered).
 *  If no free_cb was registered in M_queue_create(), this has the same behavior
 *  as M_queue_remove().
 * \param queue  Initialized queue object returned by M_queue_create()
 * \param member User-supplied queue object (pointer) to take ownership of.
 * \return M_TRUE on success, M_FALSE on failure such as object not found.
 */
M_API M_bool M_queue_take(M_queue_t *queue, const void *member);


/*! Take control of the first queue member.  This will remove the object
 *  from the list without freeing the object (assuming free_cb was registered).
 *
 * \param queue Initialized queue object returned by M_queue_create()
 * \return pointer to first queue object, NULL if none
 */
M_API void *M_queue_take_first(M_queue_t *queue);


/*! Retrieve the number of items in the specified queue.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \return count of items in the queue.
 */
M_API size_t M_queue_len(const M_queue_t *queue);


/*! Retrieve the first queue entry in the specified queue.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \return First queue entry, NULL if no entries
 */
M_API void *M_queue_first(M_queue_t *queue);


/*! Retrieve the last queue entry in the specified queue.
 * \param queue  Initialized queue object returned by M_queue_create()
 * \return Last queue entry, NULL if no entries
 */
M_API void *M_queue_last(M_queue_t *queue);


/*! Enumerate across all members in the queue.  This function is designed to be run
 *  in a while() loop until it returns M_FALSE.  The q_foreach parameter will be
 *  automatically deallocated if this returns M_FALSE, otherwise if breaking out of
 *  the loop early, M_queue_foreach_free must be called.
 *
 *  During an enumeration, it is allowable to remove the *current* member from the
 *  queue.  It is undefined behavior to remove any other member during an enumeration.
 *  Addition to the queue is also allowed during an enumeration, however it is not
 *  defined if the new value will end up in the enumerated set.
 * \param queue     Initialized queue object returned by M_queue_create()
 * \param q_foreach M_queue_foreach_t * passed By Reference, and initialized to NULL
 *                  before the first call to M_queue_foreach().  The value returned
 *                  is not meant to be interpreted and must be passed back in, unmodified
 *                  to the next call of M_queue_foreach().  If breaking out of the loop
 *                  prior to M_queue_foreach() returning M_FALSE, this object should be
 *                  free'd with M_queue_foreach_free().
 * \param member    Pointer to member to be filled in, passed By Reference.  This value
 *                  will be populated with the current member in the enumeration.  Must
 *                  not be NULL.
 * \return M_TRUE if there are more members to enumerate, M_FALSE if no more members and no
 *         result was returned.
 */
M_API M_bool M_queue_foreach(const M_queue_t *queue, M_queue_foreach_t **q_foreach, void **member);


/*! Free's the M_queue_foreach_t * filled in by M_queue_foreach.  This only needs to be
 *  called if the enumeration was ended early and not allowed to run to completion.
 *  NOTE: This may currently be a no-op if q_foreach just references an internal pointer.
 * \param q_foreach M_queue_foreach_t * initialized by M_queue_foreach.
 */
M_API void M_queue_foreach_free(M_queue_foreach_t *q_foreach);

__END_DECLS

/*! @} */

#endif /* __M_QUEUE_H__ */
