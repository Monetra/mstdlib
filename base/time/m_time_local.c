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

	/* NOTE: Windows8/Server2012 introduced GetSystemTimePreciseAsFileTime() which is a
	 * more accurate version of the same thing.  At some point we should switch to
	 * that when we no longer care about legacy systems */
	GetSystemTimeAsFileTime(&systime);
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
static M_time_t M_time_fromlocal_sys(M_time_localtm_t *ltime)
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
static void M_time_tolocal_sys(M_time_t t, M_time_localtm_t *ltime)
{
	char                  *abbr;
	FILETIME               ft;
	FILETIME               fto;
	SYSTEMTIME             st;
	SYSTEMTIME             lt;
	TIME_ZONE_INFORMATION  info;
	
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
	
	switch (GetTimeZoneInformation(&info)) {
		case 1:
			abbr = M_win32_wchar_to_char(info.StandardName);
			M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", abbr);
			M_free(abbr);
			ltime->gmtoff = (info.Bias*-60)+(info.StandardBias*-60);
			ltime->isdst = 0;
			break;
		case 2:
			abbr = M_win32_wchar_to_char(info.DaylightName);
			M_snprintf(ltime->abbr, sizeof(ltime->abbr), "%s", abbr);
			M_free(abbr);
			ltime->gmtoff = (info.Bias*-60)+(info.DaylightBias*-60);
			ltime->isdst = 1;
			break;
		default:
			ltime->isdst = -1;
			break;
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
