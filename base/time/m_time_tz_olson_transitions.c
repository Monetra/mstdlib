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
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Sort with later dates first.
 * E.g: 2013, 2012, 1990.
 */
static int M_time_tz_olson_tranition_compar(const void *arg1, const void *arg2, void *thunk)
{
	M_time_tz_olson_transition_t *i1;
	M_time_tz_olson_transition_t *i2;

	(void)thunk;

	i1 = *((M_time_tz_olson_transition_t * const *)arg1);
	i2 = *((M_time_tz_olson_transition_t * const *)arg2);

	if (i1->start == i2->start)
		return 0;
	else if (i1->start > i2->start)
		return -1;
	return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_tz_olson_transitions_t *M_time_tz_olson_transitions_create(void)
{
	struct M_list_callbacks callbacks = {
		M_time_tz_olson_tranition_compar,
		NULL,
		NULL,
		M_free
	};
	return (M_time_tz_olson_transitions_t *)M_list_create(&callbacks, M_LIST_SORTED);
}

void M_time_tz_olson_transitions_destroy(M_time_tz_olson_transitions_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

size_t M_time_tz_olson_transitions_len(M_time_tz_olson_transitions_t *d)
{
	return M_list_len((const M_list_t *)d);
}

const M_time_tz_olson_transition_t *M_time_tz_olson_transitions_at(M_time_tz_olson_transitions_t *d, size_t idx)
{
	return M_list_at((const M_list_t *)d, idx);
}

M_bool M_time_tz_olson_transitions_insert(M_time_tz_olson_transitions_t *d, M_time_tz_olson_transition_t *val)
{
	return M_list_insert((M_list_t *)d, val);
}

const M_time_tz_olson_transition_t *M_time_tz_olson_transitions_get_transition(M_time_tz_olson_transitions_t *d, M_time_t gmt)
{
	const M_time_tz_olson_transition_t *transition;
	M_time_tz_olson_transition_t        stransition;
	size_t                              idx;
	size_t                              len;
	size_t                              i;

	if (d == NULL)
		return NULL;

	M_mem_set(&stransition, 0, sizeof(stransition));
	stransition.start = gmt;

	/* We want to find where this time would be inserted so we can get the transition before it.
 	 * We can't use M_list_index_of because that looks for an exact match while we want to know
	 * what transition is before this time. We have no intenion of inserting. */
	idx        = M_list_insert_idx((M_list_t *)d, &stransition);
	transition = M_time_tz_olson_transitions_at(d, idx);

	if (transition != NULL) {
		return transition;
	}

	/* If the time is before the first transition we assume non-DST. We need to find the first non-DST
 	 * transition and use that one. */
	len  = M_time_tz_olson_transitions_len(d);
	if (idx == len) {
		for (i=len; i>0; i--) {
			transition = M_time_tz_olson_transitions_at(d, i-1);
			if (transition && !transition->isdst) {
				break;
			}
		}

		/* We've gone though every transition and all are DST so use the first transition because it's closest
		 * to the requested date. */
		if (!transition || (transition && transition->isdst)) {
			transition = M_time_tz_olson_transitions_at(d, len-1);
		}

		return transition;
	}

	return NULL;
}
