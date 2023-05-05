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

#include <time.h>

#include <mstdlib/mstdlib.h>

#define IS_LEAPYEAR(year) (((year) % 400 == 0) || (((year) % 100 != 0) && ((year) % 4 == 0)))

static const int mdays[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static M_int64 calc_yday(const M_time_gmtm_t *tm)
{
	int     is_leapyear = IS_LEAPYEAR(tm->year);
	M_int64 yday        = 0;
	int     i;

	for (i=0; i<tm->month-1; i++) {
		yday += mdays[is_leapyear][i];
	}
	yday += tm->day;

	/* yday is 0-based, subtract 1 */
	yday--;

	return yday;
}

static void normalize_range(M_int64 *overflow, M_int64 *num, int min, int max)
{
	M_int64 tmp;

	if (*num < min) {
		tmp        = M_ABS(*num);
		*overflow -= (tmp + max) / (max + 1);
		*num       = max - ((tmp + max) % (max + 1));
	}

	if (*num > max) {
		(*overflow) += (*num) / (max + 1);
		*num        %= (max + 1); 
	}
}

M_time_t M_time(void)
{
	return time(NULL);
}

void M_time_normalize_tm(struct M_time_tm *tm)
{
	int is_leapyear;
	int num_month_days;
	int num_year_days;

	/* Make month and day of month 0-based */
	tm->month--;
	tm->day--;

	/* Normalize secs, minutes, hours */
	normalize_range(&tm->min, &tm->sec, 0, 59);
	normalize_range(&tm->hour, &tm->min, 0, 59);
	normalize_range(&tm->day, &tm->hour, 0, 23);

	/* Optimization: normalize days to years. Not necesary
	 * to the calc, but helps */
	while (1) {
		if (tm->day > 0) {
			is_leapyear = IS_LEAPYEAR(tm->year);
		} else {
			is_leapyear = IS_LEAPYEAR(tm->year-1);
		}
		num_year_days = is_leapyear?366:365;

		if (M_ABS(tm->day) < num_year_days)
			break;

		if (tm->day < 0) {
			tm->day += num_year_days;
			tm->year--;
		} else {
			tm->day -= num_year_days;
			tm->year++;
		}
		
	}

	/* Normalize days to months */
	while (1) {
		/* Normalize months first */
		normalize_range(&tm->year, &tm->month, 0, 11);

		/* Normalize days a month at a time.  Need to do this because
		 * months have a different number of days, and on leap years
		 * could change days in Feb from year to year.  If we change
		 * the month though, we need to loop again to re-normalize the
		 * month, hence the while loop */
		is_leapyear    = IS_LEAPYEAR(tm->year);
		num_month_days = mdays[is_leapyear][tm->month];

		/* Already in normal range */
		if (tm->day < num_month_days && tm->day >= 0)
			break;

		if (tm->day < 0) {
			/* Add the prior month's number of days */
			tm->day += mdays[is_leapyear][tm->month-1 < 0?11:tm->month-1];
			tm->month--;
		} else if (tm->day >= num_month_days) {
			tm->day -= num_month_days;
			tm->month++;
		}
	}

	/* month and day of month is currently 0-based, increment by 1 */
	tm->day++;
	tm->month++;

	/* Ensure year2 is set properly. Normaizing moths and days could have
	 * changed the year. */
	tm->year2 = tm->year % 100;
}

int M_time_days_in_month(M_int64 year, M_int64 month)
{
	int is_leapyear = IS_LEAPYEAR(year);

	if (month < 1 || month > 12) {
		return 0;
	}

	return mdays[is_leapyear][month-1];
}

M_bool M_time_is_valid_day(M_int64 year, M_int64 month, M_int64 day)
{
	int days;

	days = M_time_days_in_month(year, month);
	if (days == 0 || day < 1) {
		return M_FALSE;
	}

	if (day > days) {
		return M_FALSE;
	}

	return M_TRUE;
}

static const int wdaymap[]  = { 4, 5, 6, 0, 1, 2, 3 };
static const int rwdaymap[] = { 4, 3, 2, 1, 0, 6, 5 };

void M_time_togm(M_time_t t, M_time_gmtm_t *tm)
{
	M_mem_set(tm, 0, sizeof(*tm));

	/* Calculate total number of days since epoch, plus remaining seconds */
	tm->yday = t / 86400;
	tm->sec  = t;

	if (t < 0 && t % 86400 != 0)
		tm->yday--;

	/* Calculate the day of the week.  Epoch was a Thursday,
	 * so we need to adjust appropriately */
	if (tm->yday < 0) {
		tm->wday = rwdaymap[M_ABS(tm->yday) % 7];
	} else {
		tm->wday = wdaymap[tm->yday % 7];
	}

	/* Epoch is Jan 1, 1970 00:00:00 UTC */
	tm->year = 1970;

	/* month and day of month is currently 0-based, increment by 1 */
	tm->day++;
	tm->month++;

	/* Normalize the time */
	M_time_normalize_tm(tm);

	/* Calculate julian day */
	tm->yday = calc_yday(tm);
}

M_time_t M_time_fromgm(M_time_gmtm_t *tm)
{
	M_time_t t;
	int      i;

	M_time_normalize_tm(tm);

	/* Make month and day of month 0-based */
	tm->month--;
	tm->day--;

	/* Bring month to 0 */
	for (i=0; i<tm->month; i++) {
		int is_leapyear = IS_LEAPYEAR(tm->year);
		tm->day += mdays[is_leapyear][i];
	}
	tm->month = 0;

	/* Bring days to 0 */
	tm->hour += tm->day * 24;
	tm->day  = 0;

	/* Bring hours to 0 */
	tm->min += tm->hour * 60;
	tm->hour = 0;

	/* Bring minutes to 0 */
	tm->sec += tm->min * 60;
	tm->min  = 0;

	/* Bring years EPOCH ... do this last */
	while (tm->year != 1970) {
		if (tm->year > 1970) {
			int is_leapyear = IS_LEAPYEAR(tm->year-1);
			tm->sec += 86400 * (is_leapyear?366:365);
			tm->year--;
		} else {
			int is_leapyear = IS_LEAPYEAR(tm->year);
			tm->sec -= 86400 * (is_leapyear?366:365);
			tm->year++;
		}
	}

	/* At this point, all should be zero'd out except for tm.year which
	 * should be 1970, tm.sec should be number of seconds since epoch,
	 * and month/day/hour/min should all be zero'd */
	t = (M_time_t)tm->sec;

	/* Normalize the tm if a non-empty one was passed in */
	M_time_togm(t, tm);
	return t;
}
