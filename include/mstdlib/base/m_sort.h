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

#ifndef __M_SORT_H__
#define __M_SORT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sort Search and Sort
 *  \ingroup mstdlib_base
 *
 * Searching and sorting operations.
 *
 * @{
 */

/*! Comparision function prototype.
 *
 * Used the same as qsort or bsearch!
 *
 * The internal array holding the data elements is a void pointer which means
 * the compar function has a reference to the index of the array (void **). That
 * means you may need to dereference more time than you think if base is an array
 * of pointer. E.g: 
 *
 *     Array: my_type_t **data;
 *     Deref: my_type_t *t1 = *((my_type_t * const *)arg1);
 *
 *     Array: M_uint64 **data;
 *     Deref: M_uint64 i1 = *(*((M_uint64 * const *)arg1));
 *
 * If base is an arrary of fixed data. E.g:
 *
 *     my_type_t *data;
 *     data = M_malloc_zero(sizeof(*data) * cnt));
 *     data[0].v1 = "a";
 *     data[0].v2 = "b";
 *     ...
 *     Deref: const my_type_t *t1 = arg1;
 * 
 *
 *     M_uint64 *data = { 1, 2, 3 };
 *     Deref: M_uint64 i1 = *((M_uint64 const *)arg1);
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 *
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 */
typedef int (*M_sort_compar_t)(const void *arg1, const void *arg2, void *thunk);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Binary Search */

/*! Find the index the key should be inserted at in a sorted array.
 *
 * \param[in] base   The array of elements to search.
 * \param[in] nmemb  The number of elements in the array.
 * \param[in] esize  The size of each element in the array.
 * \param[in] key    The element to be inserted.
 * \param[in] stable Should the insert find and use the last matching element.
 *                   This can cause performance to degrade to worst case O(n/2).
 * \param[in] compar The comparision function.
 * \param[in] thunk  Additional data to pass to the comparison function.
 *
 * \return The index the item should be inserted at.  If the the key is
 *         <= the first element, will always return 0.  If the key is >=
 *         the last element, will always return nmemb.
 */
M_API size_t M_sort_binary_insert_idx(const void *base, size_t nmemb, size_t esize, const void *key, M_bool stable, M_sort_compar_t compar, void *thunk);

/*! Find and element in a sorted array.
 *
 * \param[in]  base   The array of elements to search.
 * \param[in]  nmemb  The number of elements in the array.
 * \param[in]  esize  The size of each element in the array.
 * \param[in]  key    The element to be inserted.
 * \param[in]  stable Should the insert find and use the last matching element.
 *                    This can cause performance to degrade to worst case O(n/2).
 * \param[in]  compar The comparision function.
 * \param[in]  thunk  Additional data to pass to the comparison function.
 * \param[out] idx    The index of the items location.
 *
 * \return True if the item was found. Otherwise false.
 */
M_API M_bool M_sort_binary_search(const void *base, size_t nmemb, size_t esize, const void *key, M_bool stable, M_sort_compar_t compar, void *thunk, size_t *idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Sorting */

/*! Sort elements in array in ascending order according to comparison function.
 *
 * This is an unstable sort.
 *
 * \param[in,out] base   The array of elements to sort.
 * \param[in]     nmemb  The number of elements in the array.
 * \param[in]     esize  The size of each element in the array.
 * \param[in]     compar The comparison function.
 * \param[in]     thunk  Additional data to pass to the comparison function.
 */
M_API void M_sort_qsort(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk);


/*! Sort elements in array in ascending order according to comparison function.
 *
 * This is a stable sort.
 *
 * \param[in,out] base   The array of elements to sort.
 * \param[in]     nmemb  The number of elements in the array.
 * \param[in]     esize  The size of each element in the array.
 * \param[in]     compar The comparison function.
 * \param[in]     thunk  Additional data to pass to the comparison function.
 */
M_API void M_sort_mergesort(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * str compar */

/*! qsort style string comparison in ascending order.
 *
 * Base must be an array of pointers to values.
 * 
 *     const char **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_str(const void *arg1, const void *arg2, void *thunk);


/*! qsort style string comparison in descending order.
 *
 * Base must be an array of pointers to values.
 * 
 *     const char **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_str_desc(const void *arg1, const void *arg2, void *thunk);


/*! qsort style string comparison in ascending order case insensitive.
 *
 * Base must be an array of pointers to values.
 * 
 *     const char **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_str_casecmp(const void *arg1, const void *arg2, void *thunk);


/*! qsort style string comparison in descending order case insensitive.
 *
 * Base must be an array of pointers to values.
 * 
 *     const char **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_str_casecmp_desc(const void *arg1, const void *arg2, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * u64 compar */

/*! qsort style unsigned integer comparison in ascending order.
 *
 * Base must be an array of pointers to values.
 *
 *     M_uint64 **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_u64(const void *arg1, const void *arg2, void *thunk);


/*! qsort style unsigned integer comparison in descending order.
 *
 * Base must be an array of pointers to values.
 *
 *     M_uint64 **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_u64_desc(const void *arg1, const void *arg2, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * bin compar */

/*! qsort style wrapped binary data comparison for data that has been wrapped using M_bin_wrap.
 *
 * The binary data will be compared using M_mem_cmpsort.
 *
 * Base must be an array of pointers to values.
 *
 *     M_uint8 **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 * \see M_mem_cmpsort
 * \see M_bin_wrap
 */
M_API int M_sort_compar_binwraped(const void *arg1, const void *arg2, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * pointer compar */

/*! qsort style unsigned integer comparison in ascending order.
 *
 * The pointer themselves are compared; _not_ the value they point to.
 *
 * Base must be an array of pointers to values.
 *
 *     void **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_vp(const void *arg1, const void *arg2, void *thunk);


/*! qsort style unsigned integer comparison in descending order.
 *
 * The pointer themselves are compared; _not_ the value they point to.
 *
 * Base must be an array of pointers to values.
 *
 *     void **array;
 *
 * \param[in] arg1  The first arg to compare.
 * \param[in] arg2  The second arg to compare.
 * \param[in] thunk Additional data to use for comparison.
 * 
 * \return -1, 0, 1 for arg1 < arg2, arg1 == arg2, arg1 > arg2.
 *
 * \see M_sort_compar_t
 */
M_API int M_sort_compar_vp_desc(const void *arg1, const void *arg2, void *thunk);

/*! @} */

__END_DECLS

#endif /* __M_SORT_H__ */
