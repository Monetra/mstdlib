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

static M_bool M_time_tz_rule_change_to_time(const M_time_tz_dst_change_t *change, M_int64 year, M_time_t *out)
{
	M_time_gmtm_t atime;
	int           occur;
	M_int64       aday;
	M_int64       day;
	M_int64       start_day;

	if (change == NULL || out == NULL) {
		return M_FALSE;
	}

	*out = 0;
	M_mem_set(&atime, 0, sizeof(atime));

	/* Figure out what day of the month the rule falls on. We need to determine what day of week the first
 	 * day of the month is and from there we can determine what day of the month we're dealing with. */
	atime.year  = year;
	atime.month = change->month;
	atime.hour  = change->hour;
	atime.min   = change->min;
	atime.sec   = change->sec;
	occur       = change->occur;
	aday        = change->wday;

	/* Invalid rule. */
	if (occur == 0) {
		return M_FALSE;
	}

	/* Negative occur means we want to look backwards. */
	if (occur > 0) {
		start_day = 1;
	} else {
		start_day = M_time_days_in_month(atime.year, atime.month);
		if (atime.day == 0) {
			return M_FALSE;
		}
	}
	atime.day = start_day;

	/* Get wday. */
	M_time_fromgm(&atime);

	/* Get the day of the month the rule applies to. */
	if (occur > 0) {
		day = 1 - atime.wday + aday;
		if (day < 1)
			day += 7;
		day += 7 * (occur-1);
	} else {
		day = start_day - (atime.wday - aday);
		if (day > start_day) {
			day -= 7;
		}
		day -= 7 * (occur-1);
	}

	/* Check that the day is really valid for the month. */
	if (!M_time_is_valid_day(year, atime.month, day)) {
		return M_FALSE;
	}
	atime.day = day;

	*out = M_time_fromgm(&atime);
	return M_TRUE;
}

static M_bool M_time_tz_rule_isdst_mid(M_time_t cur_time, M_time_t dststart, M_time_t dstend, M_time_t offset_diff, M_bool isdst)
{
	/* A local time between during a fallback period can happen twice.
 	 *
	 * Once with DST and once without. For example:
	 * In EST5EDT the first sunday of the month at 2:00AM the time falls back one hour for the DST change.
	 * Due to this 1:30 will happen first due to DST then again after the 2:00AM fallback. Going forward
	 * we don't have this issue.
	 *
	 * We're going to use the isdst flag to determine whether an ambiguous time should be treated as DST or not.
	 */
	if (!isdst && cur_time <= dstend && cur_time >= dstend-offset_diff)
		return M_FALSE;

	if (cur_time < dststart || cur_time > dstend)
		return M_FALSE;
	return M_TRUE;
}

static M_bool M_time_tz_rule_isdst_ends(M_time_t cur_time, M_time_t dststart, M_time_t dstend, M_time_t offset_diff, M_bool isdst)
{
	if (!isdst && cur_time <= dststart && cur_time >= dststart-offset_diff)
		return M_FALSE;

	if (cur_time > dststart || cur_time < dstend)
		return M_FALSE;
	return M_TRUE;
}

static M_bool M_time_tz_rule_isdst(const M_time_tz_dst_rule_t *adjust, const M_time_localtm_t *ltime)
{
	M_time_t         dststart;
	M_time_t         dstend;
	M_time_t         cur_time;
	M_time_t         offset_diff;
	M_time_localtm_t myltime;
	M_bool           isdst;

	if (adjust == NULL || ltime == NULL) {
		return M_FALSE;
	}

	if (!M_time_tz_rule_change_to_time(&(adjust->start), ltime->year, &dststart) ||
		!M_time_tz_rule_change_to_time(&(adjust->end), ltime->year, &dstend))
	{
		return M_FALSE;
	}

	M_mem_copy(&myltime, ltime, sizeof(myltime));
	cur_time    = M_time_fromgm(&myltime);
	offset_diff = M_ABS(adjust->offset) - M_ABS(adjust->offset_dst);
	isdst       = ltime->isdst==1?M_TRUE:M_FALSE;

	if (dststart < dstend) {
		return M_time_tz_rule_isdst_mid(cur_time, dststart, dstend, offset_diff, isdst);
	} else if (dststart > dstend) {
		return M_time_tz_rule_isdst_ends(cur_time, dststart, dstend, offset_diff, isdst);
	} else {
		if (cur_time == dststart) {
			return M_TRUE;
		}
		return M_FALSE;
	}
}

/*! Determine if a date and time requires adjustment for DST.
 * \param tz The timezone data.
 * \param ltime The local time to check for adjustment.
 * \param offset How much the time needs to be adjusted by from UTC.
 * \return True if the date needs to be adjusted. Otherwise false.
 */
static void M_time_tz_rule_get_offset(const M_time_tz_rule_t *tz, const M_time_localtm_t *ltime, M_time_t *offset, M_bool *isdst)
{
	const M_time_tz_dst_rule_t *adjust;

	if (offset != NULL)
		*offset = 0;
	if (isdst != NULL)
		*isdst = M_FALSE;

	if (tz == NULL || ltime == NULL)
		return;

	/* Get the adjustment for the year. */
	adjust = M_time_tz_dst_rules_get_rule(tz->adjusts, ltime->year);

	/* No DST rules apply */
	if (adjust == NULL) {
		if (offset != NULL) {
			*offset = tz->offset;
		}
		return;
	}

	if (adjust->start.month == 0 || !M_time_tz_rule_isdst(adjust, ltime)) {
		if (offset != NULL) {
			*offset = adjust->offset;
		}
	} else {
		if (offset != NULL) {
			*offset = adjust->offset_dst;
		}
		if (isdst != NULL) {
			*isdst = M_TRUE;
		}
	}
}

/*! Get the adjustment to a UTC time that needs to be applied for the given timezone (including DST).
 * \param data The tz data to use for the adjustment.
 * \param time The UTC time that needs to be adjusted.
 * \param isdst True if DST is in effect. Otherwise false.
 * \param abbr The abbreviation used for this time.
 * \return The adjustment that needs to be applied to the UTC time.
 */
static M_time_t M_time_tz_rule_adjust_tolocal(const void *data, M_time_t gmt, M_bool *isdst, const char **abbr)
{
	const M_time_tz_rule_t     *rtz;
	const M_time_tz_dst_rule_t *adjust;
	M_time_localtm_t            ltime;
	M_time_t                    offset;
	M_int64                     year;

	if (data == NULL) {
		return 0;
	}
	if (isdst) {
		*isdst = M_FALSE;
	}
	if (abbr) {
		*abbr = NULL;
	}

	rtz = (const M_time_tz_rule_t *)data;

	/* No DST rules apply */
	if (M_time_tz_dst_rules_len(rtz->adjusts) == 0) {
		return rtz->offset;
	}

	/* First get the year for the GMT time. */
	M_mem_set(&ltime, 0, sizeof(ltime));
	M_time_togm(gmt, &ltime);
	year = ltime.year;

	/* Get the adjustment for the year. */
	adjust = M_time_tz_dst_rules_get_rule(rtz->adjusts, year);

	/* Adjust the UTC time to local time so we can determine if DST applies. */
	gmt += adjust->offset;

	/* Convert the UTC time to a struct again. The year might have changed due to the UTC time and the local time
 	 * adjusting past a year boundary. */
	M_mem_set(&ltime, 0, sizeof(ltime));
	M_time_togm(gmt, &ltime);

	M_time_tz_rule_get_offset(rtz, &ltime, &offset, isdst);
	if (abbr != NULL && isdst != NULL && *isdst) {
		*abbr = rtz->abbr_dst;
	}
	if (abbr != NULL && *abbr == NULL) {
		*abbr = rtz->abbr;
	}

	return offset;
}

static M_time_t M_time_tz_rule_adjust_fromlocal(const void *data, const M_time_localtm_t *ltime)
{
	const M_time_tz_rule_t *rtz;
	M_time_t                offset;

	if (data == NULL || ltime == NULL)
		return 0;

	rtz = (const M_time_tz_rule_t *)data;
	M_time_tz_rule_get_offset(rtz, ltime, &offset, NULL);
	
	return offset * -1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a rule tz object.
 * \return A tz object for dealing with rule based tz data.
 */
M_time_tz_rule_t *M_time_tz_rule_create(void)
{
	M_time_tz_rule_t *tz;

	tz          = M_malloc_zero(sizeof(*tz));
	tz->adjusts = M_time_tz_dst_rules_create();

	return tz;
}

/*! Destroy a rule based tz object.
 * \param tz The tz to destroy.
 */
void M_time_tz_rule_destroy(void *tz)
{
	M_time_tz_rule_t *rtz;

	if (tz == NULL)
		return;

	rtz = (M_time_tz_rule_t *)tz;

	M_time_tz_dst_rules_destroy(rtz->adjusts);
	rtz->adjusts = NULL;

	M_free(rtz->name);
	rtz->name = NULL;

	M_free(rtz->abbr);
	rtz->abbr = NULL;

	M_free(rtz->abbr_dst);
	rtz->abbr_dst = NULL;

	M_free(rtz);
}

/*! Add a DST rule to the timezone's list of rules.
 * \param tz The rule tz.
 * \param adjust The adjustment rule to add to the tz. The tz takes ownership of the adjust on success.
 * \return True if the adjustment rule as added successfully.
 */
M_bool M_time_tz_rule_add_dst_adjust(M_time_tz_rule_t *tz, M_time_tz_dst_rule_t *adjust)
{
	if (tz == NULL)
		return M_FALSE;
	if (adjust == NULL)
		return M_TRUE;

	/* Only one rule per year is allowed. */
	if (M_time_tz_dst_rules_contains(tz->adjusts, adjust->year)) {
		return M_FALSE;
	}

	return M_time_tz_dst_rules_insert(tz->adjusts, adjust);
}

M_time_tz_t *M_time_tz_rule_create_tz(M_time_tz_rule_t *rtz)
{
	M_time_tz_t *tz;

	/* Create the tz to put the data in. */
	tz                   = M_malloc(sizeof(*tz));
	tz->data             = rtz;
	tz->destroy          = M_time_tz_rule_destroy;
	tz->adjust_fromlocal = M_time_tz_rule_adjust_fromlocal;
	tz->adjust_tolocal   = M_time_tz_rule_adjust_tolocal;

	return tz;
}

M_time_result_t M_time_tz_rule_load(M_time_tzs_t *tzs, M_time_tz_rule_t *rtz, const char *name, const M_list_str_t *aliases)
{
	M_time_tz_t *tz;
	size_t       len;
	size_t       i;

	if (tzs == NULL || name == NULL || M_list_str_len(aliases) == 0)
		return M_TIME_RESULT_ERROR;

	/* Create the tz to put the data in. */
	tz = M_time_tz_rule_create_tz(rtz);

	/* Add the tz and the aliases to the db. */
	if (!M_time_tzs_add_tz(tzs, tz, name)) {
		M_time_tz_destroy(tz);
		return M_TIME_RESULT_DUP;
	}
	/* Add aliases. */
	len = M_list_str_len(aliases);
	for (i=0; i<len; i++) {
		M_time_tzs_add_alias(tzs, M_list_str_at(aliases, i), name);
	}

	return M_TIME_RESULT_SUCCESS;
}
