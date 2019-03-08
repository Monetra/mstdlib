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
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Sort with later dates first.
 * E.g: 2013, 2012, 1990.
 */
static int M_time_tz_dst_rules_compar(const void *arg1, const void *arg2, void *thunk)
{
	M_time_tz_dst_rule_t *i1;
	M_time_tz_dst_rule_t *i2;

	(void)thunk;

	i1 = *((M_time_tz_dst_rule_t * const *)arg1);
	i2 = *((M_time_tz_dst_rule_t * const *)arg2);

	if (i1->year == i2->year)
		return 0;
	else if (i1->year > i2->year)
		return -1;
	return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_tz_dst_rules_t *M_time_tz_dst_rules_create(void)
{
	struct M_list_callbacks callbacks = {
		M_time_tz_dst_rules_compar,
		NULL,
		NULL,
		M_free
	};
	return (M_time_tz_dst_rules_t *)M_list_create(&callbacks, M_LIST_SORTED);
}

void M_time_tz_dst_rules_destroy(M_time_tz_dst_rules_t *d)
{
	M_list_destroy((M_list_t *)d, M_TRUE);
}

size_t M_time_tz_dst_rules_len(M_time_tz_dst_rules_t *d)
{
	return M_list_len((const M_list_t *)d);
}

const M_time_tz_dst_rule_t *M_time_tz_dst_rules_at(M_time_tz_dst_rules_t *d, size_t idx)
{
	return M_list_at((const M_list_t *)d, idx);
}

M_bool M_time_tz_dst_rules_insert(M_time_tz_dst_rules_t *d, M_time_tz_dst_rule_t *val)
{
	return M_list_insert((M_list_t *)d, val);
}

M_bool M_time_tz_dst_rules_contains(M_time_tz_dst_rules_t *d, M_int64 year)
{
	M_time_tz_dst_rule_t val;

	M_mem_set(&val, 0, sizeof(val));
	val.year = year;

	return M_list_index_of((M_list_t *)d, &val, M_LIST_MATCH_VAL, NULL);
}

const M_time_tz_dst_rule_t *M_time_tz_dst_rules_get_rule(M_time_tz_dst_rules_t *d, M_int64 year)
{
	const M_time_tz_dst_rule_t *rule;
	M_time_tz_dst_rule_t        srule;
	size_t                      idx;
	size_t                      len;

	if (d == NULL)
		return NULL;

	M_mem_set(&srule, 0, sizeof(srule));
	srule.year = year;

	len  = M_time_tz_dst_rules_len(d);
	/* We want to find where this time would be inserted so we can get the rule before it.
 	 * We can't use M_list_index_of because that looks for an exact match while we want to know
	 * what rule is before this time. We have no intenion of inserting. */
	idx  = M_list_insert_idx((M_list_t *)d, &srule);
	if (idx == len)
		idx--;
	rule = M_time_tz_dst_rules_at(d, idx);
	/* We don't check that year is >= rule->year because we'll use the earliest rule as the rule for
 	 * all times before the first rule. */
	if (rule != NULL) {
		return rule;
	}
	return NULL;
}
