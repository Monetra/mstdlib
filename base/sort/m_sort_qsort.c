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

/* This Qsort implementation is based off of the implementation found in
 * Jon L. Bentley and M. Douglas McIlroy's "Engineering a Sort Function". (Nov 1993)
 * It has been modified to be more readable, portable, and reusable, but otherwise
 * the algorithm itself is unchanged. 
 */

#include "m_config.h"

#include <mstdlib/mstdlib.h>

enum QSORT_SWAPTYPE {
	QSORT_SWAPTYPE_MULTIPLEOF_UINTPTR,
	QSORT_SWAPTYPE_SIZEOF_UINTPTR,
	QSORT_SWAPTYPE_UNALIGNED
};

#define sort_swap_bytype(type, elem1, elem2, esize)  \
	do {                                             \
		size_t i;                                    \
		for (i = 0; i < esize/sizeof(type); i++) {   \
			type save          = ((type *)elem1)[i]; \
			((type *)elem1)[i] = ((type *)elem2)[i]; \
			((type *)elem2)[i] = save;               \
		}                                            \
	} while(0);

static __inline__ void sort_swap(void *elem1, void *elem2, size_t esize, enum QSORT_SWAPTYPE swaptype)
{
	/* Why would we try to swap the same pointer? */
	if (elem1 == elem2)
		return;

	switch (swaptype) {
		case QSORT_SWAPTYPE_SIZEOF_UINTPTR:
			/* Element is exactly sizeof(M_uintptr), use that to swap */
			do {
				M_uintptr t           = *((M_uintptr *)elem1);
				*((M_uintptr *)elem1) = *((M_uintptr *)elem2);
				*((M_uintptr *)elem2) = t;
			} while(0);
			break;
		case QSORT_SWAPTYPE_MULTIPLEOF_UINTPTR:
			/* Element is aligned and is a multiple of sizeof(M_uintptr) */
			sort_swap_bytype(M_uintptr, elem1, elem2, esize);
			break;
		case QSORT_SWAPTYPE_UNALIGNED:
			/* Element is not aligned or not a multiple of sizeof(M_uintptr) */
			sort_swap_bytype(unsigned char, elem1, elem2, esize);
			break;
	}
}


static __inline__ void sort_swap_multiple(void *pos1, void *pos2, size_t size, enum QSORT_SWAPTYPE swaptype)
{
	if (size != 0)
		sort_swap(pos1, pos2, size, swaptype == QSORT_SWAPTYPE_SIZEOF_UINTPTR?QSORT_SWAPTYPE_MULTIPLEOF_UINTPTR:swaptype);
}


static enum QSORT_SWAPTYPE M_sort_swaptype(void *base, size_t esize)
{
	/* If address not aligned to sizeof(M_uintptr) or element size not a multiple
	 * of sizeof(M_uintptr), means we have to use unaligned access */
	if ((M_uintptr)base % sizeof(M_uintptr) || esize % sizeof(M_uintptr)) {
		return QSORT_SWAPTYPE_UNALIGNED;
	} else if (esize == sizeof(M_uintptr)) {
		/* Aligned and is exactly a M_uintptr */
		return QSORT_SWAPTYPE_SIZEOF_UINTPTR;
	}
	
	/* Aligned and is a multiple of M_uintptr */
	return QSORT_SWAPTYPE_MULTIPLEOF_UINTPTR;
}


static void M_sort_insertion(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk)
{
	enum QSORT_SWAPTYPE swaptype;
	unsigned char      *ptr;
	size_t              i;

	swaptype = M_sort_swaptype(base, esize);

	for (i=1; i<nmemb; i++) {
		for (ptr=(unsigned char *)base + (i * esize); ptr > (unsigned char *)base && compar(ptr-esize, ptr, thunk) > 0; ptr -= esize) {
			sort_swap(ptr, ptr-esize, esize, swaptype);
		}
	}
}


static __inline__ void *qsort_median(void *a, void *b, void *c, M_sort_compar_t compar, void *thunk)
{
	if (compar(a, b, thunk) < 0) {
		if (compar(b, c, thunk) < 0)
			return b;
		if (compar(a, c, thunk) < 0)
			return c;
		return a;
	}

	if (compar(b, c, thunk) > 0)
		return b;
	if (compar(a, c, thunk) < 0)
		return a;

	return c;
}


static __inline__ void *qsort_choose_pivot(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk)
{
	unsigned char *middle;
	unsigned char *left;
	unsigned char *right;
	size_t         srange;

	middle = (unsigned char *)base + (nmemb / 2) * esize;
	if (nmemb > 7) {
		left  = base;
		right = (unsigned char *)base + (nmemb - 1) * esize;
		if (nmemb > 40) {
			/* Choose smaller pivot to reduce recursion because the range less than
			 * the pivot point chosen is sorted recursively, but right is iterative */
			srange  = (nmemb / 8) * esize; /* Search range */
			left    = qsort_median(left,               left + srange,  left + 2 * srange, compar, thunk);
			middle  = qsort_median(middle - srange,    middle,         middle + srange,   compar, thunk);
			right   = qsort_median(right - 2 * srange, right - srange, right,             compar, thunk);
		}
		middle = qsort_median(left, middle, right, compar, thunk);
	}

	return middle;
}


void M_sort_qsort(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk)
{
	/* Guarantees: base < left1 <= left2 <= right1 <= right2 <= end */
	unsigned char      *left1;
	unsigned char      *left2;
	unsigned char      *right1;
	unsigned char      *right2;
	unsigned char      *pivot;
	unsigned char      *end;
	size_t              s;
	int                 rv;
	enum QSORT_SWAPTYPE swaptype;
	M_bool              sort_right = M_TRUE;

	/* determine most efficient swap algorithm */
	swaptype = M_sort_swaptype(base, esize);

	/* We are doing a while loop around this to make the function iterative
	 * on the right-most partition, which is guaranteed to be the _larger_
	 * partition by our qsort_choose_pivot.  We only recurse on the left
	 * partition which is smaller to save stack space */
	do {
		/* Insertion sort is faster than qsort with small sets */
		if (nmemb < 7) {
			M_sort_insertion(base, nmemb, esize, compar, thunk);
			return;
		}

		pivot = qsort_choose_pivot(base, nmemb, esize, compar, thunk);

		sort_swap(base, pivot, esize, swaptype);
		/* NOTE: The original implementation sets left1 = left2 = a (base) ... but
		*       the first comparison is done against 'pv' which is also 'a' (base)
		*       when not using the word-size optimization, so it makes sense to
		*       start at the next element.  We are also skipping the word-size
		*       optimization as it makes the code more complex.  BSD
		*       implementation does this as well */
		left1  = left2  = (unsigned char *)base + 1*esize; 
		right1 = right2 = (unsigned char *)base + (nmemb - 1) * esize;
		while (1) {
			while (left2 <= right1 && (rv = compar(left2, base, thunk)) <= 0) {
				if (rv == 0) {
					sort_swap(left1, left2, esize, swaptype);
					left1 += esize;
				}
				left2 += esize;
			}
			while (left2 <= right1 && (rv = compar(right1, base, thunk)) >= 0) {
				if (rv == 0) {
					sort_swap(right1, right2, esize, swaptype);
					right2 -= esize;
				}
				right1 -= esize;
			}
			if (left2 > right1)
				break;
			sort_swap(left2, right1, esize, swaptype);
			left2  += esize;
			right1 -= esize;
		}

		/* NOTE: the BSD implementation appears to have an optimization that if
		 *       in the above while(1) loop, no swaps were performed, it sorts
		 *       by insertion sort instead like
		 * M_sort_insertion(base, nmemb, esize, compar, thunk);
		 * return;
		 *       Does this optimization make sense?
		 */

		/* Cast to size_t is ok here because 'left1' is always > 'base' (as 'left1' is
		 * never decremented and starts out greater than base), and 'left2' is
		 * always >= 'left1' because they are initialized to the same value and every
		 * time 'left1' is incremented, 'left2' is also incremented */
		s  = (size_t)M_MIN(left1 - (unsigned char *)base, left2 - left1);
		sort_swap_multiple(base, left2 - s, s, swaptype);

		end = (unsigned char *)base + (nmemb * esize);
		/* Cast to size_t is ok here because 'right2' and 'right1' are initialized to the
		 * same value, and every time 'right2' is decremented, 'right1' is guaranteed to
		 * be decremented as well.  Also 'right2' is initialized to be 1 less than end
		 * and only decremented so calc won't go negative */
		s   = (size_t)M_MIN(right2 - right1, end - (right2 + esize));
		sort_swap_multiple(left2, end - s, s, swaptype);

		/* Sort the left (smaller) section recursively */
		if ((s = (size_t)(left2 - left1)) > esize)
			M_sort_qsort(base, s / esize, esize, compar, thunk);

		if ((s = (size_t)(right2 - right1)) > esize) {
			/* M_sor_qsort(end - s, s / esize, esize, compar); */
			/* Iterate rather than recurse to save stack space in case compiler
			 * can't do tail recursion optimization */
			base  = end - s;
			nmemb = s / esize;
		} else {
			/* No more sorting/looping */
			sort_right = M_FALSE;
		}
	} while(sort_right);
}
