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

void M_sort_mergesort(void *base, size_t nmemb, size_t esize, M_sort_compar_t compar, void *thunk)
{
	unsigned char *left;
	unsigned char *right;
	size_t         ls;
	size_t         rs;
	size_t         mid;
	size_t         i;
	size_t         j;
	size_t         k;

	if (base == NULL || nmemb < 2 || esize == 0 || compar == NULL)
		return;

	/* Find the mid and deal with an odd number of elements. */
	mid = nmemb/2;
	ls  = mid;
	rs  = mid;

	if (nmemb > 2 && nmemb % 2 != 0)
		ls++;

	/* Copy the elements into tmp arrays. */
	left  = M_malloc(ls*esize);
	right = M_malloc(rs*esize);
	M_mem_copy(left, base, ls*esize);
	M_mem_copy(right, (const unsigned char *)base+(ls*esize), rs*esize);

	M_sort_mergesort(left, ls, esize, compar, thunk);
	M_sort_mergesort(right, rs, esize, compar, thunk);

	i = 0;
	j = 0;
	k = 0;
	/* Merge the tmp arrays back into the base array in sorted order. */
	while (i < ls && j < rs) {
		/* <= gives us a stable sort. */
		if (compar(left+(i*esize), right+(j*esize), thunk) <= 0) {
			M_mem_copy((unsigned char *)base+(k*esize), left+(i*esize), esize);
			i++;
		} else {
			M_mem_copy((unsigned char *)base+(k*esize), right+(j*esize), esize);
			j++;
		}
		k++;
	}

	/* If left is longer than right copy the remaining elements over */
	while (i < ls) {
		M_mem_copy((unsigned char *)base+(k*esize), left+(i*esize), esize);
		i++;
		k++;
	}

	/* If right is longer than right copy the remaining elements over */
	while (j < rs) {
		M_mem_copy((unsigned char *)base+(k*esize), right+(j*esize), esize);
		j++;
		k++;
	}

	M_free(right);
	M_free(left);
}
