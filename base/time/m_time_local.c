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

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <mstdlib/mstdlib.h>
#include "time/m_time_int.h"
#include "platform/m_platform.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_time_gettimeofday(M_timeval_t *tv)
{
#ifdef _WIN32
	FILETIME systime;

	if (tv == NULL)
		return M_FALSE;

	M_mem_set(tv, 0, sizeof(*tv));
	M_mem_set(&systime, 0, sizeof(systime));

#  if _WIN32_WINNT >= 0x0602 /* Windows 8 */
	/* NOTE: Windows8/Server2012 introduced GetSystemTimePreciseAsFileTime() which is a
	 * more accurate version of the same thing.  When the default build target changes, it will
	 * automatically be used.  */
	GetSystemTimePreciseAsFileTime(&systime);
#  else
	GetSystemTimeAsFileTime(&systime);
#  endif

	M_time_timeval_from_filetime(&systime, tv);

	return M_TRUE;
#else
	struct timeval rtv;

	if (tv == NULL)
		return M_FALSE;

	if (gettimeofday(&rtv, NULL) != 0)
		return M_FALSE;

	tv->tv_sec  = rtv.tv_sec;
	tv->tv_usec = rtv.tv_usec;
	return M_TRUE;
#endif
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN32
static M_time_t M_time_fromlocal_sys(M_time_localtm_t *ltime)
{
	M_int64          t = 0;
	FILETIME         ft;
	FILETIME         fto;
	SYSTEMTIME       st;
	SYSTEMTIME       lt;
	M_time_localtm_t myltime;
	
	M_mem_set(&ft, 0, sizeof(ft));
	M_mem_set(&fto, 0, sizeof(fto));
	M_mem_set(&st, 0, sizeof(st));
	M_mem_set(&lt, 0, sizeof(lt));
	M_mem_set(&myltime, 0, sizeof(myltime));
	
	M_mem_copy(&myltime, ltime, sizeof(*ltime));
	
	/* Normalize. Don't want a month of 13. */
	//M_time_normalize_tm(myltime);
	t = M_time_fromgm(&myltime);
	M_time_to_filetime(t, &ft);

	/* It would be great if we could use LocalFileTimeToFileTime
 	 * but it intentionally doesn't account for DST. Instead
	 * call the opposite of the following 3 functions per MSDN
	 * function doc for FileTimeToLocalFileTime */
	if (!FileTimeToSystemTime(&ft, &lt))
		return 0;
	if (!TzSpecificLocalTimeToSystemTime(NULL, &lt, &st))
		return 0;
	if (!SystemTimeToFileTime(&st, &fto))
		return 0;
	
	return M_time_from_filetime(&fto);
}
#else
static M_time_t M_time_fromlocal_sys(const M_time_localtm_t *ltime)
{
	struct tm tmtime;
	M_time_t  t;

	M_mem_set(&tmtime, 0, sizeof(tmtime));
	tmtime.tm_sec   = (int)ltime->sec;
	tmtime.tm_min   = (int)ltime->min;
	tmtime.tm_hour  = (int)ltime->hour;
	tmtime.tm_mday  = (int)ltime->day;
	tmtime.tm_mon   = (int)ltime->month-1;
	tmtime.tm_year  = (int)ltime->year-1900;
	tmtime.tm_isdst = (int)ltime->isdst;
	t = (M_time_t)mktime(&tmtime);

	return t;
}
#endif

static M_time_t M_time_fromlocal_tz(M_time_localtm_t *ltime, const M_time_tz_t *tz)
{
	M_time_t t;
	int      isdst;
	
	isdst = (int)ltime->isdst;
	t  = M_time_fromgm(ltime);
	ltime->isdst = isdst;
	t += tz->adjust_fromlocal(tz->data, ltime);

	return t;
}

M_time_t M_time_fromlocal(M_time_localtm_t *ltime, const M_time_tz_t *tz)
{
	M_time_t t;

	if (ltime == NULL) {
		return 0;
	}

	if (tz == NULL) {
		t = M_time_fromlocal_sys(ltime);
	} else {
		t = M_time_fromlocal_tz(ltime, tz);
	}

	M_time_tolocal(t, ltime, tz);
	return t;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_time_tolocal_tz(M_time_t t, M_time_localtm_t *ltime, const M_time_tz_t *tz)
{
	const char *abbr = NULL;
	M_bool      isdst = M_FALSE;
	M_time_t    offset = 0;

	offset  = tz->adjust_tolocal(tz->data, t, &isdst, &abbr);
	t      += offset;

	M_time_togm(t, ltime);

	/* Set these because they will be wiped by M_time_togm. */
	ltime->isdst  = isdst;
	ltime->gmtoff = offset;
	if (abbr != NULL) {
		M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", abbr);
	} else {
		M_mem_set(ltime->abbr, 0, sizeof(ltime->abbr));
	}
}

#ifdef _WIN32

static M_uint64 M_time_win_to_int(M_uint64 month, M_uint64 day, M_uint64 hour, M_uint64 minute)
{
	return (month * 1000000) + (day * 10000) + (hour * 100) + minute;
}


/* Transform wDay member from a week multiplier to an actual day of month */
static void M_time_win_SYSTEMTIME_normalize(SYSTEMTIME *st, int year)
{
	M_time_gmtm_t gmt;
	int           days_in_month = M_time_days_in_month(year, st->wMonth);
	int           day;

	/* We need to get the day of the week the first day falls on */
	M_mem_set(&gmt, 0, sizeof(gmt));
	gmt->year = year;
	gmt->month = st->wMonth;
	gmt->day = 1;
	M_time_fromgm(&gmt); /* This updates gmt->tm_wday */

	/* use wDayOfWeek (0-6), and wDay (week of month) to calculate the day of the
	 * month */
	day = st->wDayOfWeek * st->wDay;
	if (gmt->tm_wday > st->wDayOfWeek) {
		day += 7 - gmt->tm_wday + st->wDayOfWeek;
	} else if (gmt->tm_wday < st->wDayOfWeek) {
		day += st->wDayOfWeek - gmt->tm_wday;
	}

	while (day > days_in_month)
		day -= 7;

	st->wDay = day;
}

static M_bool M_time_win_is_dst(SYSTEMTIME *StandardDate, SYSTEMTIME *DaylightDate, M_time_localtm_t *currdate)
{
	M_uint64 stdtime;
	M_uint64 dsttime;
	M_uint64 curtime;

	/* https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-time_zone_information
	 * If the time zone does not support daylight saving time or if the caller needs to disable daylight saving time,
	 * the wMonth member in the SYSTEMTIME structure must be zero. If this date is specified, the DaylightDate member
	 * of this structure must also be specified.
	 *
	 * NOTE: This is a funky interface, while wDay is typically the day of the month for the SYSTEMTIME structure,
	 *       this is NOT the case for this function.  Instead its an indicator of the *week* of the month, which
	 *       must be used in conjunction with wDayOfWeek
	 *
	 * To select the correct day in the month, set the wYear member to zero, the wHour and wMinute members to the
	 * transition time, the wDayOfWeek member to the appropriate weekday, and the wDay member to indicate the
	 * occurrence of the day of the week within the month (1 to 5, where 5 indicates the final occurrence during
	 * the month if that day of the week does not occur 5 times).
	 * Using this notation, specify 02:00 on the first Sunday in April as follows: wHour = 2, wMonth = 4,
	 * wDayOfWeek = 0, wDay = 1. Specify 02:00 on the last Thursday in October as follows:
	 *   wHour = 2, wMonth = 10, wDayOfWeek = 4, wDay = 5.
	 */
	if (StandardDate->wMonth == 0)
		return M_FALSE;

	M_time_win_SYSTEMTIME_normalize(StandardDate, currdate->year);
	M_time_win_SYSTEMTIME_normalize(DaylightDate, currdate->year);
	stdtime = M_time_win_to_int(StandardDate->wMonth, StandardDate->wDay, StandardDate->wHour, StandardDate->wMinute);
	dsttime = M_time_win_to_int(DaylightDate->wMonth, DaylightDate->wDay, DaylightDate->wHour, DaylightDate->wMinute);
	curtime = M_time_win_to_int(currdate->month, currdate->day, currdate->hour, currdate->min);
M_printf("%s(): stdtime %08llu, dsttime %08llu, curtime %08llu\r\n", __FUNCTION__, stdtime, dsttime, curtime);

	if (stdtime > dsttime) {
		/* switch to standard time at end of year, daylight at beginning */
		if (curtime >= stdtime)
			return M_FALSE;

		if (curtime < dsttime)
			return M_FALSE;

		return M_TRUE;
	}

	/* switch to daylight time at end of year, standard at end */
	if (curtime >= dsttime)
		return M_TRUE;

	if (curtime < stdtime)
		return M_TRUE;

	return M_FALSE;
}

static void M_time_tolocal_sys(M_time_t t, M_time_localtm_t *ltime)
{
	char                  *abbr;
	FILETIME               ft;
	FILETIME               fto;
	SYSTEMTIME             st;
	SYSTEMTIME             lt;
	TIME_ZONE_INFORMATION  info;

	M_mem_set(&info, 0, sizeof(info));
	M_mem_set(&ft, 0, sizeof(ft));
	M_mem_set(&fto, 0, sizeof(fto));
	M_mem_set(&st, 0, sizeof(st));
	M_mem_set(&lt, 0, sizeof(lt));
	M_mem_set(ltime, 0, sizeof(*ltime));

	M_time_to_filetime(t, &ft);

	/* It would be great if we could use FileTimeToLocalFileTime
 	 * but it intentionally doesn't account for DST. Instead
	 * call the following 3 functions per MSDN function doc. */
	FileTimeToSystemTime(&ft, &st);
	SystemTimeToTzSpecificLocalTime(NULL, &st, &lt);
	SystemTimeToFileTime(&lt, &fto);

	t = M_time_from_filetime(&fto);
	M_time_togm(t, ltime);

	GetTimeZoneInformationForYear((M_uint16)ltime->year, NULL, &info);

	if (M_time_win_is_dst(&info.StandardDate, &info.DaylightDate, ltime)) {
		abbr = M_win32_wchar_to_char(info.DaylightName);
M_printf("%s(): is_dst=1, %s\r\n", __FUNCTION__, abbr);
		M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", abbr);
		M_free(abbr);
		ltime->gmtoff = (info.Bias*-60)+(info.DaylightBias*-60);
		ltime->isdst = 1;
	} else {
		abbr = M_win32_wchar_to_char(info.StandardName);
M_printf("%s(): is_dst=0, %s\r\n", __FUNCTION__, abbr);
		M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", abbr);
		M_free(abbr);
		ltime->gmtoff = (info.Bias*-60)+(info.StandardBias*-60);
		ltime->isdst = 0;
	}

	M_time_normalize_tm(ltime);
}
#else
static void M_time_tolocal_sys(M_time_t t, M_time_localtm_t *ltime)
{
	struct tm *tmtime_ptr;

#ifdef HAVE_LOCALTIME_R
	struct tm tmtime;
	time_t    st    = M_time_M_time_t_to_time_t(t);

	tmtime_ptr = localtime_r(&st, &tmtime);
#else
	time_t st = M_time_M_time_t_to_time_t(t);
	tmtime_ptr = localtime(&st);
#endif

	if (tmtime_ptr == NULL)
		return;

	ltime->year  = tmtime_ptr->tm_year+1900;
	ltime->year2 = ltime->year % 100;
	ltime->month = tmtime_ptr->tm_mon+1;
	ltime->day   = tmtime_ptr->tm_mday;
	ltime->hour  = tmtime_ptr->tm_hour;
	ltime->min   = tmtime_ptr->tm_min;
	ltime->sec   = tmtime_ptr->tm_sec;
	ltime->wday  = tmtime_ptr->tm_wday;
	ltime->yday  = tmtime_ptr->tm_yday;
	ltime->isdst = tmtime_ptr->tm_isdst;

#ifdef STRUCT_TM_HAS_GMTOFF
	ltime->gmtoff = tmtime_ptr->tm_gmtoff;
#else
	ltime->gmtoff = -1 * timezone;
	if (ltime->isdst)
		ltime->gmtoff += 3600;
#endif

#ifdef STRUCT_TM_HAS_ZONE
	M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", tmtime_ptr->tm_zone);
#else
	M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", tzname[ltime->isdst==1?1:0]);
#endif
}
#endif

void M_time_tolocal(M_time_t t, M_time_localtm_t *ltime, const M_time_tz_t *tz)
{
	if (ltime == NULL) {
		return;
	}
	M_mem_set(ltime, 0, sizeof(*ltime));

	if (tz == NULL) {
		M_time_tolocal_sys(t, ltime);
	} else {
		M_time_tolocal_tz(t, ltime, tz);
	}
}
