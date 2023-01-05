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

#ifndef __M_TIME_H__
#define __M_TIME_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_time Time
 *  \ingroup mstdlib_base
 *
 * Time handling functions.
 *
 * Features
 * ========
 *
 * Covers:
 * - Local
 * - GMT
 * - Normalization
 * - Conversion
 * - Diff
 * - Elapsed
 * - Time zone
 * - string reading
 * - string writing
 *
 *
 * Key data types
 * ==============
 *
 * M_time_t is provided as a platform agnostic replacement for time_t. M_time_t a signed 64 bit data type.
 * This allows systems which provide a 32 bit time_t to handle times above the 32 bit boundary. However,
 * any functions (such as M_time) that use underlying system time functions will only operate using the
 * bit max/min provided by the system time_t.
 *
 * M_timeval_t (struct M_timeval) is also provided for the same reasons as M_time_t. In addition, not all
 * platforms support struct timval in an obvious way. Windows in particular can have header conflict issues when
 * dealing with struct timeval. Specifically, struct timeval is defined in Winsock2.h which much be included
 * before Windows.h. Either this header would have to include, which can lead to problems is this header is
 * included after Windows.h is declared. Or an application using mstdlib would have to include Winsock2.h,
 * which is nonobvious.
 *
 *
 * Timezone
 * ========
 *
 * Time zone data is stored in a timezone database object. Data can be loaded in two ways.
 * - Loading a timezone database (Olson files, Windows registry).
 * - Loading individual timezone data.
 *
 * Lazy loading is available when using a timezone database. Lazy loading has the data read into
 * the db on demand instead of reading the data immediately. Only one timezone data source
 * can be used for lazy loading.
 *
 * When using lazy loading in a multi threaded environment all calls to M_time_tzs_get_tz
 * need to be protected by a mutex or other access broker.
 *
 * The tz (timezone) object should not be used directly. Instead it should be passed to
 * M_time_tolocal or M_time_fromlocal.
 *
 *
 * Examples
 * ========
 *
 * Timezone
 * --------
 *
 * \code{.c}
 *     M_time_tzs_t      *tzs;
 *     const M_time_tz_t *tz;
 *     M_time_t           ts = 1375277153;
 *     M_time_t           cs;
 *     M_time_localtm_t   ltime;
 *
 *     M_mem_set(&ltime, 0, sizeof(ltime));
 *
 *     tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_AMERICA, M_TIME_TZ_ALIAS_OLSON_MAIN, M_TIME_TZ_LOAD_LAZY);
 *     tz  = M_time_tzs_get_tz(tzs, "America/New_York");
 *
 *     M_time_tolocal(ts, &ltime, tz);
 *
 *     M_printf("isdst='%s'\n", ltime.isdst?"YES":"NO");
 *
 *     cs = M_time_fromlocal(&ltime, tz);
 *     if (ts != cs) {
 *         M_printf("time conversion failed\n");
 *     } else {
 *         M_printf("time conversion success\n");
 *     }
 *
 *     M_time_tzs_destroy(tzs);
 * \endcode
 *
 * System
 * ------
 *
 * \code{.c}
 *     M_time_localtm_t ltime;
 *     M_time_t         ts;
 *
 *     M_mem_set(&ltime, 0, sizeof(ltime));
 *
 *     M_time_tolocal(0, &ltime, NULL);
 *     t = M_time_fromlocal(&ltime, NULL);
 *
 *     if (t != 0) {
 *         M_printf("time conversion failed\n");
 *     } else {
 *         M_printf("time conversion success\n");
 *     }
 * \endcode
 *
 * Time Strings
 * ------------
 *
 * \code{.c}
 *     M_time_t          ts;
 *     char             *out;
 *     M_time_localtm_t  ltime;
 *
 *     M_mem_set(&ltime, 0, sizeof(ltime));
 *
 *     ts  = M_time_from_str("1998/11/31 10:02:50", NULL, M_FALSE);
 *     M_time_tolocal(ts, &ltime, NULL);
 *     out = M_time_to_str("%Y-%m-%d %H:%M:%S %p", &tm);
 *     M_printf("out='%s'\n", out);
 *     M_free(out);
 * \endcode
 *
 * @{
 */

typedef M_int64 M_time_t;
typedef M_int64 M_suseconds_t;

/*! Broken down time stored as individual components. */
struct M_time_tm {
	M_int64  month;    /*!< Month. 1-12*/
	M_int64  day;      /*!< Day of month. 1-X */
	M_int64  year;     /*!< Year. Full year. E.g. 2013. */
	M_int64  year2;    /*!< 2digit Year. E.g. 13. */
	M_int64  hour;     /*!< hour. 0=Midnight ... 23=11PM. */
	M_int64  min;      /*!< minute. 0-59. */
	M_int64  sec;      /*!< second. 0-59. */
	M_int64  wday;     /*!< day of week. 0=Sun ... 6=Sat */
	M_int64  yday;     /*!< day of year. 0-364 (or 365 on leap years) */
	/* Local time data */
	M_int64  isdst;    /*!< -1=DST unknown, 0=not DST, 1=is DST */
	M_time_t gmtoff;   /*!< Seconds west of Greenwich. */
	char     abbr[32]; /*!< Abbreviation for use with printing. This will only be filled if a M_time_tz_t is passed
	                       in with the time. If abbr is filled by a M_time_tz_t then the M_time_tz_t must remain
	                       valid for the life of the struct. */
};
typedef struct M_time_tm M_time_localtm_t;
typedef struct M_time_tm M_time_gmtm_t;


/*! Number of seconds and microseconds since the Epoch. */
typedef struct M_timeval {
	M_time_t      tv_sec;  /*!< Seconds. */
	M_suseconds_t tv_usec; /*!< Microseconds. */
} M_timeval_t;


/*! Timezone data. */
struct M_time_tz;
typedef struct M_time_tz M_time_tz_t;


/*! Timezone database. */
struct M_time_tzs;
typedef struct M_time_tzs M_time_tzs_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Olson/TZ/Zoneinfo locations that can be loaded. */
typedef enum {
	M_TIME_TZ_ZONE_ALL        = 0,       /*!< Load all zones. This cannot be combined with individual zones. */
	M_TIME_TZ_ZONE_AFRICA     = 1 << 1,  /*!< Load data form Africa. */
	M_TIME_TZ_ZONE_AMERICA    = 1 << 2,  /*!< Load data form the Americas. */
	M_TIME_TZ_ZONE_ANTARCTICA = 1 << 3,  /*!< Load data form Antarctica. */
	M_TIME_TZ_ZONE_ARCTIC     = 1 << 4,  /*!< Load data form the artic. */
	M_TIME_TZ_ZONE_ASIA       = 1 << 5,  /*!< Load data form Asia. */
	M_TIME_TZ_ZONE_ATLANTIC   = 1 << 6,  /*!< Load data form the Atlantic. */
	M_TIME_TZ_ZONE_AUSTRALIA  = 1 << 7,  /*!< Load data form Australia. */
	M_TIME_TZ_ZONE_EUROPE     = 1 << 8,  /*!< Load data form Europe. */
	M_TIME_TZ_ZONE_INDIAN     = 1 << 9,  /*!< Load data form the Indian ocean region. */
	M_TIME_TZ_ZONE_PACIFIC    = 1 << 10, /*!< Load data form the Pacific. */
	M_TIME_TZ_ZONE_ETC        = 1 << 11  /*!< Load data form Etc (fixed offset) zones. */
} M_time_tz_zones_t;


/*! Flags to control loading behavior of Olson/TZ/Zoneinfo data. */
typedef enum {
	M_TIME_TZ_LOAD_NORMAL    = 0,      /*!< Load all data. */
	M_TIME_TZ_LOAD_LAZY      = 1 << 1  /*!< Lazy load data. This is really only useful for memory constrained
	                                        environments where only a few zones will be in use but the overhead
	                                        of loading all zones may be too much for the system. */
} M_time_tz_load_t;


/*! Handle alias loading.
 * Not all alias options will be avalaible for all zone data sources.
 */
typedef enum {
	M_TIME_TZ_ALIAS_ALL          = 0,      /*!< Include all names and aliases. */
	M_TIME_TZ_ALIAS_OLSON_MAIN   = 1 << 1, /*!< Include main Olson alias. */
	M_TIME_TZ_ALIAS_OLSON_ALL    = 1 << 2, /*!< Include all Olson aliases. */
	M_TIME_TZ_ALIAS_WINDOWS_MAIN = 1 << 3, /*!< Include Windows zone names. */
	M_TIME_TZ_ALIAS_WINDOWS_ALL  = 1 << 4  /*!< Include Windows zone names. */
} M_time_tz_alias_t;


/*! Result codes specific to time operations. */
typedef enum {
	M_TIME_RESULT_SUCCESS = 0, /*!< Success. */
	M_TIME_RESULT_INVALID,     /*!< Invalid argument. */
	M_TIME_RESULT_ERROR,       /*!< General error. */
	M_TIME_RESULT_DUP,         /*!< Duplicate. */
	M_TIME_RESULT_INI,         /*!< ini failed to parse. */
	M_TIME_RESULT_ABBR,        /*!< Std abbreviation failed to parse. */
	M_TIME_RESULT_OFFSET,      /*!< Std offset failed to parse. */
	M_TIME_RESULT_DATE,        /*!< Date failed to parse. */
	M_TIME_RESULT_TIME,        /*!< Time failed to parse. */
	M_TIME_RESULT_DATETIME,    /*!< Date/time failed to parse. */
	M_TIME_RESULT_YEAR,        /*!< Year failed to parse. */
	M_TIME_RESULT_DSTABBR,     /*!< DST abbreviation failed to parse. */
	M_TIME_RESULT_DSTOFFSET    /*!< DST offset failed to parse. */
} M_time_result_t;


/*! Source timezone data was loaded form.
 * \see M_time_tzs_load */
typedef enum {
	M_TIME_LOAD_SOURCE_FAIL   = 0, /*!< Timezone data failed to load. This
	                                    can happen if no timezone data was
	                                    loaded. For example, a specific
	                                    M_time_tz_zones_t was requested but
	                                    not available. */
	M_TIME_LOAD_SOURCE_SYSTEM,     /*!< The system timezone data was loaded. */
	M_TIME_LOAD_SOURCE_FALLBACK    /*!< Main four US timezones were loaded as
	                                    a fallback because system data could
	                                    not be loaded. */
} M_time_load_source_t;

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_time_time Date Time
 *  \ingroup m_time
 *
 * @{
 */

/*! Get the system time.
 *
 * While M_time_t is guaranteed to be 64 bit the time returned is not.
 * Time is dependent on the platform and some only support 32 bit time
 * values. On these systems M_time will only return a value up to
 * Jan 19, 2038 03:14:07 UTC.
 *
 * \return Number of seconds since Epoch (Jan 1, 1970 00:00:00 UTC).
 */
M_API M_time_t M_time(void);


/*! Get the number of seconds and milliseconds since Epoch.
 *
 * \param[out] tv The time as seconds and milliseconds since Epoch.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_time_gettimeofday(M_timeval_t *tv) M_WARN_NONNULL(1);


/*! Get the number of days in a given month for a given year.
 *
 * Accounts for leap years.
 *
 * \param[in] year  The year.
 * \param[in] month The month. 1-12.
 *
 * \return The number of days in the month.
 */
M_API int M_time_days_in_month(M_int64 year, M_int64 month);


/*! Determine if a give day of month valid for the given month for a given year.
 *
 * \param[in] year  The year.
 * \param[in] month The month.
 * \param[in] day   The day of month.
 *
 * \return M_TRUE if the day is valid. Otherwise M_FALSE.
 */
M_API M_bool M_time_is_valid_day(M_int64 year, M_int64 month, M_int64 day);


/*! Normalize a struct tm.
 *
 * If adjustments are made to a struct tm this will bring the adjustments back
 * to a real date/time.
 *
 * This does not modify the isdst, gmtoff or abbr fields of the struct. These may be
 * wrong if the adjust time crosses a DST boundary for example. Use M_time_fromlocal
 * with the appropriate time zone data (or NULL if using the systems current info)
 * to normalize a time taking into account these fields. Or use M_time_fromgm if dealing
 * with a gm time (isdst, gmtoff and abbr will be cleared).
 *
 * \param[in,out] tm The tm to normalize.
 */
M_API void M_time_normalize_tm(struct M_time_tm *tm);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a local time to a UTC time.
 *
 * \param[in,out] ltime The local time structure to convert. This will be normalized.
 * \param[in]     tz    The time zone the local time is in.
 *
 * \return UTC time.
 */
M_API M_time_t M_time_fromlocal(M_time_localtm_t *ltime, const M_time_tz_t *tz) M_WARN_NONNULL(1);


/*! Convert a UTC time to a local time struct.
 *
 * \param[in]  t     The UTC time.
 * \param[out] ltime The local time struct.
 * \param[in]  tz    The time zone the local time is in.
 */
M_API void M_time_tolocal(M_time_t t, M_time_localtm_t *ltime, const M_time_tz_t *tz) M_WARN_NONNULL(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a UTC time to a broken out time struct.
 *
 * \param[in]  t  UTC time.
 * \param[out] tm Time struct.
 */
M_API void M_time_togm(M_time_t t, M_time_gmtm_t *tm) M_WARN_NONNULL(2);


/*! Convert a broken out time struct to a unix timestamp.
 *
 * \param[in,out] tm The time struct. This will be normalized.
 *
 * \return Time stamp.
 */
M_API M_time_t M_time_fromgm(M_time_gmtm_t *tm) M_WARN_NONNULL(1);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_time_calc Time Calculations
 *  \ingroup m_time
 *
 * @{
 */

/*! Calculate the number of milliseconds between two timevals.
 *
 * \param[in] start_time The start time.
 * \param[in] end_time The end time.
 *
 * \return The number of milliseconds between the two times.
 */
M_API M_int64 M_time_timeval_diff(const M_timeval_t *start_time, const M_timeval_t *end_time) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/*! Start time to use for elapsed time operations.
 *
 * \param[out] start_tv The current time to use as the start time.
 */
M_API void M_time_elapsed_start(M_timeval_t *start_tv) M_WARN_NONNULL(1);


/*! The amount of time that has elapsed since start in milliseconds.
 *
 * \param[in] start_tv The time to calculate from. This should be the value from M_time_elapsed_start.
 *
 * \return The number of milliseconds that have elapsed since start until now.
 */
M_API M_uint64 M_time_elapsed(const M_timeval_t *start_tv) M_WARN_NONNULL(1);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_time_parse Parsing
 *  \ingroup m_time
 *
 * @{
 */

/*! Parse a time string.
 *
 * Supports offsets and fixed formats.
 *
 * Offsets:
 *
 * - Support:
 *   - 'now'
 *   - 'epoch'
 *   - 'yesterday' (at same time as current day)
 *   - 'today' (same as 'now'),
 *   - 'tomorrow' (at same time as current day),
 *   - 'BOD' (beginning of current day)
 *   - 'EOD' (end of current day)
 *   - +/-N magnitude', where magnitude is:
 *     - year, month, day, hour, min, sec
 *     - where long names and plural are supported
 *
 * Ex: +6 Months  or -7 hours
 *
 * Offsets use the current time.
 *
 * Fixed:
 *
 * - \%m/\%d/\%Y \%H
 * - \%m/\%d/\%Y \%H \%P
 * - \%m/\%d/\%Y \%H \%p
 * - \%m/\%d/\%Y \%I \%P
 * - \%m/\%d/\%Y \%I \%p
 * - \%m/\%d/\%Y \%H\%M
 * - \%m/\%d/\%Y \%H\%M \%P
 * - \%m/\%d/\%Y \%H\%M \%p
 * - \%m/\%d/\%Y \%I\%M \%P
 * - \%m/\%d/\%Y \%I\%M \%p
 * - \%m/\%d/\%Y \%H\%M\%S
 * - \%m/\%d/\%Y \%H\%M\%S \%P
 * - \%m/\%d/\%Y \%H\%M\%S \%p
 * - \%m/\%d/\%Y \%H\%M\%S \%z
 * - \%m/\%d/\%Y \%I\%M\%S \%P
 * - \%m/\%d/\%Y \%I\%M\%S \%p
 * - \%m/\%d/\%Y \%H\%M\%S \%z
 * - \%m/\%d/\%Y \%H:\%M
 * - \%m/\%d/\%Y \%H:\%M \%P
 * - \%m/\%d/\%Y \%H:\%M \%p
 * - \%m/\%d/\%Y \%I:\%M \%P
 * - \%m/\%d/\%Y \%I:\%M \%p
 * - \%m/\%d/\%Y \%H:\%M:\%S
 * - \%m/\%d/\%Y \%H:\%M:\%S \%P
 * - \%m/\%d/\%Y \%H:\%M:\%S \%p
 * - \%m/\%d/\%Y \%I:\%M:\%S \%P
 * - \%m/\%d/\%Y \%I:\%M:\%S \%p
 * - \%m/\%d/\%Y \%H:\%M:\%S \%z
 * - \%m/\%d/\%Y \%H-\%M
 * - \%m/\%d/\%Y \%H-\%M \%P
 * - \%m/\%d/\%Y \%H-\%M \%p
 * - \%m/\%d/\%Y \%I-\%M \%P
 * - \%m/\%d/\%Y \%I-\%M \%p
 * - \%m/\%d/\%Y \%H-\%M-\%S
 * - \%m/\%d/\%Y \%H-\%M-\%S \%P
 * - \%m/\%d/\%Y \%H-\%M-\%S \%p
 * - \%m/\%d/\%Y \%I-\%M-\%S \%P
 * - \%m/\%d/\%Y \%I-\%M-\%S \%p
 * - \%m/\%d/\%Y \%H-\%M-\%S \%z
 * - \%m/\%d/\%Y T \%H
 * - \%m/\%d/\%Y T \%H \%P
 * - \%m/\%d/\%Y T \%H \%p
 * - \%m/\%d/\%Y T \%I \%P
 * - \%m/\%d/\%Y T \%I \%p
 * - \%m/\%d/\%Y T \%H\%M
 * - \%m/\%d/\%Y T \%H\%M \%P
 * - \%m/\%d/\%Y T \%H\%M \%p
 * - \%m/\%d/\%Y T \%I\%M \%P
 * - \%m/\%d/\%Y T \%I\%M \%p
 * - \%m/\%d/\%Y T \%H\%M\%S
 * - \%m/\%d/\%Y T \%H\%M\%S \%P
 * - \%m/\%d/\%Y T \%H\%M\%S \%p
 * - \%m/\%d/\%Y T \%I\%M\%S \%P
 * - \%m/\%d/\%Y T \%I\%M\%S \%p
 * - \%m/\%d/\%Y T \%H\%M\%S \%z
 * - \%m/\%d/\%Y T \%H:\%M
 * - \%m/\%d/\%Y T \%H:\%M \%P
 * - \%m/\%d/\%Y T \%H:\%M \%p
 * - \%m/\%d/\%Y T \%I:\%M \%P
 * - \%m/\%d/\%Y T \%I:\%M \%p
 * - \%m/\%d/\%Y T \%H:\%M:\%S
 * - \%m/\%d/\%Y T \%H:\%M:\%S \%P
 * - \%m/\%d/\%Y T \%H:\%M:\%S \%p
 * - \%m/\%d/\%Y T \%I:\%M:\%S \%P
 * - \%m/\%d/\%Y T \%I:\%M:\%S \%p
 * - \%m/\%d/\%Y T \%H:\%M:\%S \%z
 * - \%m/\%d/\%Y T \%H-\%M
 * - \%m/\%d/\%Y T \%H-\%M \%P
 * - \%m/\%d/\%Y T \%H-\%M \%p
 * - \%m/\%d/\%Y T \%I-\%M \%P
 * - \%m/\%d/\%Y T \%I-\%M \%p
 * - \%m/\%d/\%Y T \%H-\%M-\%S
 * - \%m/\%d/\%Y T \%H-\%M-\%S \%P
 * - \%m/\%d/\%Y T \%H-\%M-\%S \%p
 * - \%m/\%d/\%Y T \%I-\%M-\%S \%P
 * - \%m/\%d/\%Y T \%I-\%M-\%S \%p
 * - \%m/\%d/\%Y T \%H-\%M-\%S \%z
 * - \%m-\%d-\%Y \%H
 * - \%m-\%d-\%Y \%H \%P
 * - \%m-\%d-\%Y \%H \%p
 * - \%m-\%d-\%Y \%I \%P
 * - \%m-\%d-\%Y \%I \%p
 * - \%m-\%d-\%Y \%H\%M
 * - \%m-\%d-\%Y \%H\%M \%P
 * - \%m-\%d-\%Y \%H\%M \%p
 * - \%m-\%d-\%Y \%I\%M \%P
 * - \%m-\%d-\%Y \%I\%M \%p
 * - \%m-\%d-\%Y \%H\%M\%S
 * - \%m-\%d-\%Y \%H\%M\%S \%P
 * - \%m-\%d-\%Y \%H\%M\%S \%p
 * - \%m-\%d-\%Y \%I\%M\%S \%P
 * - \%m-\%d-\%Y \%I\%M\%S \%p
 * - \%m-\%d-\%Y \%H\%M\%S \%z
 * - \%m-\%d-\%Y \%H:\%M
 * - \%m-\%d-\%Y \%H:\%M \%P
 * - \%m-\%d-\%Y \%H:\%M \%p
 * - \%m-\%d-\%Y \%I:\%M \%P
 * - \%m-\%d-\%Y \%I:\%M \%p
 * - \%m-\%d-\%Y \%H:\%M:\%S
 * - \%m-\%d-\%Y \%H:\%M:\%S \%P
 * - \%m-\%d-\%Y \%H:\%M:\%S \%p
 * - \%m-\%d-\%Y \%I:\%M:\%S \%P
 * - \%m-\%d-\%Y \%I:\%M:\%S \%p
 * - \%m-\%d-\%Y \%H:\%M:\%S \%z
 * - \%m-\%d-\%Y \%H-\%M
 * - \%m-\%d-\%Y \%H-\%M \%P
 * - \%m-\%d-\%Y \%H-\%M \%p
 * - \%m-\%d-\%Y \%I-\%M \%P
 * - \%m-\%d-\%Y \%I-\%M \%p
 * - \%m-\%d-\%Y \%H-\%M-\%S
 * - \%m-\%d-\%Y \%H-\%M-\%S \%P
 * - \%m-\%d-\%Y \%H-\%M-\%S \%p
 * - \%m-\%d-\%Y \%I-\%M-\%S \%P
 * - \%m-\%d-\%Y \%I-\%M-\%S \%p
 * - \%m-\%d-\%Y \%H-\%M-\%S \%z
 * - \%m-\%d-\%Y T \%H
 * - \%m-\%d-\%Y T \%H \%P
 * - \%m-\%d-\%Y T \%H \%p
 * - \%m-\%d-\%Y T \%I \%P
 * - \%m-\%d-\%Y T \%I \%p
 * - \%m-\%d-\%Y T \%H\%M
 * - \%m-\%d-\%Y T \%H\%M \%P
 * - \%m-\%d-\%Y T \%H\%M \%p
 * - \%m-\%d-\%Y T \%I\%M \%P
 * - \%m-\%d-\%Y T \%I\%M \%p
 * - \%m-\%d-\%Y T \%H\%M\%S
 * - \%m-\%d-\%Y T \%H\%M\%S \%P
 * - \%m-\%d-\%Y T \%H\%M\%S \%p
 * - \%m-\%d-\%Y T \%H\%M\%S \%z
 * - \%m-\%d-\%Y T \%I\%M\%S \%P
 * - \%m-\%d-\%Y T \%I\%M\%S \%p
 * - \%m-\%d-\%Y T \%H:\%M
 * - \%m-\%d-\%Y T \%H:\%M \%P
 * - \%m-\%d-\%Y T \%H:\%M \%p
 * - \%m-\%d-\%Y T \%I:\%M \%P
 * - \%m-\%d-\%Y T \%I:\%M \%p
 * - \%m-\%d-\%Y T \%H:\%M:\%S
 * - \%m-\%d-\%Y T \%H:\%M:\%S \%P
 * - \%m-\%d-\%Y T \%H:\%M:\%S \%p
 * - \%m-\%d-\%Y T \%I:\%M:\%S \%P
 * - \%m-\%d-\%Y T \%I:\%M:\%S \%p
 * - \%m-\%d-\%Y T \%H:\%M:\%S \%z
 * - \%m-\%d-\%Y T \%H-\%M
 * - \%m-\%d-\%Y T \%H-\%M \%P
 * - \%m-\%d-\%Y T \%H-\%M \%p
 * - \%m-\%d-\%Y T \%I-\%M \%P
 * - \%m-\%d-\%Y T \%I-\%M \%p
 * - \%m-\%d-\%Y T \%H-\%M-\%S
 * - \%m-\%d-\%Y T \%H-\%M-\%S \%P
 * - \%m-\%d-\%Y T \%H-\%M-\%S \%p
 * - \%m-\%d-\%Y T \%I-\%M-\%S \%P
 * - \%m-\%d-\%Y T \%I-\%M-\%S \%p
 * - \%m-\%d-\%Y T \%H-\%M-\%S \%z
 * - \%m/\%d/\%y \%H
 * - \%m/\%d/\%y \%H \%P
 * - \%m/\%d/\%y \%H \%p
 * - \%m/\%d/\%y \%I \%P
 * - \%m/\%d/\%y \%I \%p
 * - \%m/\%d/\%y \%H\%M
 * - \%m/\%d/\%y \%H\%M \%P
 * - \%m/\%d/\%y \%H\%M \%p
 * - \%m/\%d/\%y \%I\%M \%P
 * - \%m/\%d/\%y \%I\%M \%p
 * - \%m/\%d/\%y \%H\%M\%S
 * - \%m/\%d/\%y \%H\%M\%S \%P
 * - \%m/\%d/\%y \%H\%M\%S \%p
 * - \%m/\%d/\%y \%I\%M\%S \%P
 * - \%m/\%d/\%y \%I\%M\%S \%p
 * - \%m/\%d/\%y \%H\%M\%S \%z
 * - \%m/\%d/\%y \%H:\%M
 * - \%m/\%d/\%y \%H:\%M \%P
 * - \%m/\%d/\%y \%H:\%M \%p
 * - \%m/\%d/\%y \%I:\%M \%P
 * - \%m/\%d/\%y \%I:\%M \%p
 * - \%m/\%d/\%y \%H:\%M:\%S
 * - \%m/\%d/\%y \%H:\%M:\%S \%P
 * - \%m/\%d/\%y \%H:\%M:\%S \%p
 * - \%m/\%d/\%y \%I:\%M:\%S \%P
 * - \%m/\%d/\%y \%I:\%M:\%S \%p
 * - \%m/\%d/\%y \%H:\%M:\%S \%z
 * - \%m/\%d/\%y \%H-\%M
 * - \%m/\%d/\%y \%H-\%M \%P
 * - \%m/\%d/\%y \%H-\%M \%p
 * - \%m/\%d/\%y \%I-\%M \%P
 * - \%m/\%d/\%y \%I-\%M \%p
 * - \%m/\%d/\%y \%H-\%M-\%S
 * - \%m/\%d/\%y \%H-\%M-\%S \%P
 * - \%m/\%d/\%y \%H-\%M-\%S \%p
 * - \%m/\%d/\%y \%I-\%M-\%S \%P
 * - \%m/\%d/\%y \%I-\%M-\%S \%p
 * - \%m/\%d/\%y \%H-\%M-\%S \%z
 * - \%m/\%d/\%y T \%H
 * - \%m/\%d/\%y T \%H \%P
 * - \%m/\%d/\%y T \%H \%p
 * - \%m/\%d/\%y T \%I \%P
 * - \%m/\%d/\%y T \%I \%p
 * - \%m/\%d/\%y T \%H\%M
 * - \%m/\%d/\%y T \%H\%M \%P
 * - \%m/\%d/\%y T \%H\%M \%p
 * - \%m/\%d/\%y T \%I\%M \%P
 * - \%m/\%d/\%y T \%I\%M \%p
 * - \%m/\%d/\%y T \%H\%M\%S
 * - \%m/\%d/\%y T \%H\%M\%S \%P
 * - \%m/\%d/\%y T \%H\%M\%S \%p
 * - \%m/\%d/\%y T \%I\%M\%S \%P
 * - \%m/\%d/\%y T \%I\%M\%S \%p
 * - \%m/\%d/\%y T \%H\%M\%S \%z
 * - \%m/\%d/\%y T \%H:\%M
 * - \%m/\%d/\%y T \%H:\%M \%P
 * - \%m/\%d/\%y T \%H:\%M \%p
 * - \%m/\%d/\%y T \%I:\%M \%P
 * - \%m/\%d/\%y T \%I:\%M \%p
 * - \%m/\%d/\%y T \%H:\%M:\%S
 * - \%m/\%d/\%y T \%H:\%M:\%S \%P
 * - \%m/\%d/\%y T \%H:\%M:\%S \%p
 * - \%m/\%d/\%y T \%I:\%M:\%S \%P
 * - \%m/\%d/\%y T \%I:\%M:\%S \%p
 * - \%m/\%d/\%y T \%H:\%M:\%S \%z
 * - \%m/\%d/\%y T \%H-\%M
 * - \%m/\%d/\%y T \%H-\%M \%P
 * - \%m/\%d/\%y T \%H-\%M \%p
 * - \%m/\%d/\%y T \%I-\%M \%P
 * - \%m/\%d/\%y T \%I-\%M \%p
 * - \%m/\%d/\%y T \%H-\%M-\%S
 * - \%m/\%d/\%y T \%H-\%M-\%S \%P
 * - \%m/\%d/\%y T \%H-\%M-\%S \%p
 * - \%m/\%d/\%y T \%I-\%M-\%S \%P
 * - \%m/\%d/\%y T \%I-\%M-\%S \%p
 * - \%m/\%d/\%y T \%H-\%M-\%S \%z
 * - \%Y/\%m/\%d \%H
 * - \%Y/\%m/\%d \%H \%P
 * - \%Y/\%m/\%d \%H \%p
 * - \%Y/\%m/\%d \%I \%P
 * - \%Y/\%m/\%d \%I \%p
 * - \%Y/\%m/\%d \%H\%M
 * - \%Y/\%m/\%d \%H\%M \%P
 * - \%Y/\%m/\%d \%H\%M \%p
 * - \%Y/\%m/\%d \%I\%M \%P
 * - \%Y/\%m/\%d \%I\%M \%p
 * - \%Y/\%m/\%d \%H\%M\%S
 * - \%Y/\%m/\%d \%H\%M\%S \%P
 * - \%Y/\%m/\%d \%H\%M\%S \%p
 * - \%Y/\%m/\%d \%I\%M\%S \%P
 * - \%Y/\%m/\%d \%I\%M\%S \%p
 * - \%Y/\%m/\%d \%H\%M\%S \%z
 * - \%Y/\%m/\%d \%H:\%M
 * - \%Y/\%m/\%d \%H:\%M \%P
 * - \%Y/\%m/\%d \%H:\%M \%p
 * - \%Y/\%m/\%d \%I:\%M \%P
 * - \%Y/\%m/\%d \%I:\%M \%p
 * - \%Y/\%m/\%d \%H:\%M:\%S
 * - \%Y/\%m/\%d \%H:\%M:\%S \%P
 * - \%Y/\%m/\%d \%H:\%M:\%S \%p
 * - \%Y/\%m/\%d \%I:\%M:\%S \%P
 * - \%Y/\%m/\%d \%I:\%M:\%S \%p
 * - \%Y/\%m/\%d \%H:\%M:\%S \%z
 * - \%Y/\%m/\%d \%H-\%M
 * - \%Y/\%m/\%d \%H-\%M \%P
 * - \%Y/\%m/\%d \%H-\%M \%p
 * - \%Y/\%m/\%d \%I-\%M \%P
 * - \%Y/\%m/\%d \%I-\%M \%p
 * - \%Y/\%m/\%d \%H-\%M-\%S
 * - \%Y/\%m/\%d \%H-\%M-\%S \%P
 * - \%Y/\%m/\%d \%H-\%M-\%S \%p
 * - \%Y/\%m/\%d \%I-\%M-\%S \%P
 * - \%Y/\%m/\%d \%I-\%M-\%S \%p
 * - \%Y/\%m/\%d \%H-\%M-\%S \%z
 * - \%Y/\%m/\%d T \%H
 * - \%Y/\%m/\%d T \%H \%P
 * - \%Y/\%m/\%d T \%H \%p
 * - \%Y/\%m/\%d T \%I \%P
 * - \%Y/\%m/\%d T \%I \%p
 * - \%Y/\%m/\%d T \%H\%M
 * - \%Y/\%m/\%d T \%H\%M \%P
 * - \%Y/\%m/\%d T \%H\%M \%p
 * - \%Y/\%m/\%d T \%I\%M \%P
 * - \%Y/\%m/\%d T \%I\%M \%p
 * - \%Y/\%m/\%d T \%H\%M\%S
 * - \%Y/\%m/\%d T \%H\%M\%S \%P
 * - \%Y/\%m/\%d T \%H\%M\%S \%p
 * - \%Y/\%m/\%d T \%I\%M\%S \%P
 * - \%Y/\%m/\%d T \%I\%M\%S \%p
 * - \%Y/\%m/\%d T \%H\%M\%S \%z
 * - \%Y/\%m/\%d T \%H:\%M
 * - \%Y/\%m/\%d T \%H:\%M \%P
 * - \%Y/\%m/\%d T \%H:\%M \%p
 * - \%Y/\%m/\%d T \%I:\%M \%P
 * - \%Y/\%m/\%d T \%I:\%M \%p
 * - \%Y/\%m/\%d T \%H:\%M:\%S
 * - \%Y/\%m/\%d T \%H:\%M:\%S \%P
 * - \%Y/\%m/\%d T \%H:\%M:\%S \%p
 * - \%Y/\%m/\%d T \%I:\%M:\%S \%P
 * - \%Y/\%m/\%d T \%I:\%M:\%S \%p
 * - \%Y/\%m/\%d T \%H:\%M:\%S \%z
 * - \%Y/\%m/\%d T \%H-\%M
 * - \%Y/\%m/\%d T \%H-\%M \%P
 * - \%Y/\%m/\%d T \%H-\%M \%p
 * - \%Y/\%m/\%d T \%I-\%M \%P
 * - \%Y/\%m/\%d T \%I-\%M \%p
 * - \%Y/\%m/\%d T \%H-\%M-\%S
 * - \%Y/\%m/\%d T \%H-\%M-\%S \%P
 * - \%Y/\%m/\%d T \%H-\%M-\%S \%p
 * - \%Y/\%m/\%d T \%I-\%M-\%S \%P
 * - \%Y/\%m/\%d T \%I-\%M-\%S \%p
 * - \%Y/\%m/\%d T \%H-\%M-\%S \%z
 * - \%Y-\%m-\%d \%H
 * - \%Y-\%m-\%d \%H \%P
 * - \%Y-\%m-\%d \%H \%p
 * - \%Y-\%m-\%d \%I \%P
 * - \%Y-\%m-\%d \%I \%p
 * - \%Y-\%m-\%d \%H\%M
 * - \%Y-\%m-\%d \%H\%M \%P
 * - \%Y-\%m-\%d \%H\%M \%p
 * - \%Y-\%m-\%d \%I\%M \%P
 * - \%Y-\%m-\%d \%I\%M \%p
 * - \%Y-\%m-\%d \%H\%M\%S
 * - \%Y-\%m-\%d \%H\%M\%S \%P
 * - \%Y-\%m-\%d \%H\%M\%S \%p
 * - \%Y-\%m-\%d \%I\%M\%S \%P
 * - \%Y-\%m-\%d \%I\%M\%S \%p
 * - \%Y-\%m-\%d \%H\%M\%S \%z
 * - \%Y-\%m-\%d \%H:\%M
 * - \%Y-\%m-\%d \%H:\%M \%P
 * - \%Y-\%m-\%d \%H:\%M \%p
 * - \%Y-\%m-\%d \%I:\%M \%P
 * - \%Y-\%m-\%d \%I:\%M \%p
 * - \%Y-\%m-\%d \%H:\%M:\%S
 * - \%Y-\%m-\%d \%H:\%M:\%S \%P
 * - \%Y-\%m-\%d \%H:\%M:\%S \%p
 * - \%Y-\%m-\%d \%I:\%M:\%S \%P
 * - \%Y-\%m-\%d \%I:\%M:\%S \%p
 * - \%Y-\%m-\%d \%H:\%M:\%S \%z
 * - \%Y-\%m-\%d \%H-\%M
 * - \%Y-\%m-\%d \%H-\%M \%P
 * - \%Y-\%m-\%d \%H-\%M \%p
 * - \%Y-\%m-\%d \%I-\%M \%P
 * - \%Y-\%m-\%d \%I-\%M \%p
 * - \%Y-\%m-\%d \%H-\%M-\%S
 * - \%Y-\%m-\%d \%H-\%M-\%S \%P
 * - \%Y-\%m-\%d \%H-\%M-\%S \%p
 * - \%Y-\%m-\%d \%I-\%M-\%S \%P
 * - \%Y-\%m-\%d \%I-\%M-\%S \%p
 * - \%Y-\%m-\%d \%H-\%M-\%S \%z
 * - \%Y-\%m-\%d T \%H
 * - \%Y-\%m-\%d T \%H \%P
 * - \%Y-\%m-\%d T \%H \%p
 * - \%Y-\%m-\%d T \%I \%P
 * - \%Y-\%m-\%d T \%I \%p
 * - \%Y-\%m-\%d T \%H\%M
 * - \%Y-\%m-\%d T \%H\%M \%P
 * - \%Y-\%m-\%d T \%H\%M \%p
 * - \%Y-\%m-\%d T \%I\%M \%P
 * - \%Y-\%m-\%d T \%I\%M \%p
 * - \%Y-\%m-\%d T \%H\%M\%S
 * - \%Y-\%m-\%d T \%H\%M\%S \%P
 * - \%Y-\%m-\%d T \%H\%M\%S \%p
 * - \%Y-\%m-\%d T \%I\%M\%S \%P
 * - \%Y-\%m-\%d T \%I\%M\%S \%p
 * - \%Y-\%m-\%d T \%H\%M\%S \%z
 * - \%Y-\%m-\%d T \%H:\%M
 * - \%Y-\%m-\%d T \%H:\%M \%P
 * - \%Y-\%m-\%d T \%H:\%M \%p
 * - \%Y-\%m-\%d T \%I:\%M \%P
 * - \%Y-\%m-\%d T \%I:\%M \%p
 * - \%Y-\%m-\%d T \%H:\%M:\%S
 * - \%Y-\%m-\%d T \%H:\%M:\%S \%P
 * - \%Y-\%m-\%d T \%H:\%M:\%S \%p
 * - \%Y-\%m-\%d T \%I:\%M:\%S \%P
 * - \%Y-\%m-\%d T \%I:\%M:\%S \%p
 * - \%Y-\%m-\%d T \%H:\%M:\%S \%z
 * - \%Y-\%m-\%d T \%H-\%M
 * - \%Y-\%m-\%d T \%H-\%M \%P
 * - \%Y-\%m-\%d T \%H-\%M \%p
 * - \%Y-\%m-\%d T \%I-\%M \%P
 * - \%Y-\%m-\%d T \%I-\%M \%p
 * - \%Y-\%m-\%d T \%H-\%M-\%S
 * - \%Y-\%m-\%d T \%H-\%M-\%S \%P
 * - \%Y-\%m-\%d T \%H-\%M-\%S \%p
 * - \%Y-\%m-\%d T \%I-\%M-\%S \%P
 * - \%Y-\%m-\%d T \%I-\%M-\%S \%p
 * - \%Y-\%m-\%d T \%H-\%M-\%S \%z
 * - \%m/\%d/\%Y
 * - \%m-\%d-\%Y
 * - \%m-\%d-\%y
 * - \%m/\%d/\%y
 * - \%m\%d\%Y
 * - \%m\%d\%y
 * - \%Y/\%m/\%d
 * - \%Y-\%m-\%d
 *
 * \param[in] timestr            The time string to parse.
 * \param[in] tz                 The time zone to use. If NULL the local system time zone will be used.
 *                               This will only be used if the parsed time does not include a fixed
 *                               time zone offset.
 * \param[in] default_end_of_day M_TRUE when the returned time be at the end of the day if a time is not explictly
 *                               present in the string.
 *
 * \return M_time_t of the parsed time. -1 on error.
 *
 * \see M_time_parsefmt
 */
M_API M_time_t M_time_from_str(const char *timestr, const M_time_tz_t *tz, M_bool default_end_of_day);


/*! Format a date and time as a string.
 *
 * \param[in] fmt The format of the string.
 * \param[in] tm  The tm strcut to read from.
 *
 * \return NULL if failed to format string. Otherwise returns a NULL terminated string.
 *
 * \see M_time_parsefmt
 */
M_API char *M_time_to_str(const char *fmt, const M_time_localtm_t *tm) M_WARN_NONNULL(1) M_WARN_NONNULL(2);

/*! Parse a formatted time string into a tm structure.
 *
 * Supports the following input descriptors:
 *
 * - %% - The % character.
 * - \%m - month in 2 digit format.
 * - \%d - day in 2 digit format.
 * - \%y - year in 2 digit format.
 * - \%Y - year in 4 digit format.
 * - \%H - hour in 2 digit (24 hour) format.
 * - \%I - hour in 2 digit (12 hour) format. Should be paired with am/pm descriptors.
 * - \%M - minutes in 2 digit format.
 * - \%S - seconds in 2 digit format.
 * - \%z - offset from gmt. RFC-822 Character(s) identifier or ISO 8601 [+-]hh[[:]mm] numeric offset.
 * - \%P, \%p - AM/PM, am/pm (can parse A.M./P.M./a.m./p.m.).
 *
 * Notes on format:
 *
 * - Whitespace between descriptors is ignored but non-whitespace characters are not.
 *   - E.G: "%m%d" is equivalent to "%m %d" but not equivalent to "%m 7 %d".
 * - YY is calculated as current year -80 to +20.
 * - E.g. YY = 30. Current year is 2013. Parsed year will be 2030.
 *        YY = 50. Current year is 2013. Parsed year will be 1950.
 * - z supports multiple representations of the zone information based on RFC-822 and ISO 8601:
 *   - +/- Digit:
 *   - [+-]hh[[:]mm]
 *      Named:
 *      - UTC/GMT/UT/Z  = 0
 *      - EST/EDT       = -5/-4
 *      - CST/CDT       = -6/-5
 *      - MST/MDT       = -7/-6
 *      - PST/PDT       = -8/-7
 *      1ALPHA:
 *       - A: -1 (J not used) to M: -12
 *       - N: +1 to Y: +12
 *
 * \param[in]  s   The string to parse.
 * \param[in]  fmt The format of the string.
 * \param[out] tm  The tm strcut to fill.
 *
 * \return NULL if failed to match format otherwise returns pointer
 *         to where it stopped processing in the buffer. If successfully
 *         processed entire buffer then returns pointer to NULL byte in buffer.
 *
 * \see M_time_from_str
 */
M_API char *M_time_parsefmt(const char *s, const char *fmt, M_time_localtm_t *tm) M_WARN_NONNULL(2) M_WARN_NONNULL(3);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/*! \addtogroup m_time_zone Timezone
 *  \ingroup m_time
 *
 * @{
 */

/*! Create an empty timezone db.
 *
 * \return Time zone db.
 */
M_API M_time_tzs_t *M_time_tzs_create(void);


/*! Destroy a time zone db.
 *
 * \param[in] tzs Time zone db.
 */
M_API void M_time_tzs_destroy(M_time_tzs_t *tzs);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Load default timezones from system available source.
 *
 * This will attempt to load system timezone data. If that fails, it will fall
 * back to loading the four main US timezones with DST times and without
 * historic data.
 *
 * - ET: Eastern
 * - CT: Central
 * - MT: Mountain
 * - PT: Pacific
 *
 * \param[out] tzs Time zone db.
 * \param[in]  zones   M_time_tz_zones_t zones. What zones from the db should be loaded.
 * \param[in]  alias_f M_time_tz_alias_t to handle alias loading.
 * \param[in]  flags   M_time_tz_load_t flags. How should the data be loaded. Specifically should lazy loading be used.
 *                    When lazying loading data is read from disk (and cached) on
 *                    demand. If lazy loading is not used all day is read from disk immediately.
 *
 * \return Source timezone data was loaded from.
 */
M_API M_time_load_source_t M_time_tzs_load(M_time_tzs_t **tzs, M_uint32 zones, M_uint32 alias_f, M_uint32 flags);


/*! Load a tzs with data from a precomputed Olson/TZ/Zoneinfo db.
 *
 * To prevent possible issues the zoneinfo path cannot be a symlink. If it is a symlink the symlink needs to
 * be resolved before passing into this function. Further, symlinks within the zoneinfo base dir cannot
 * point to locations outside of the base dir.
 *
 * \param[in] path    The path to the data to load. On Unix systems this is typically "/usr/share/zoneinfo" or
 *                    "/usr/lib/zoneinfo". If NULL those two default locations will be checked for zoneinfo.
 * \param[in] zones   M_time_tz_zones_t zones. What zones from the db should be loaded.
 * \param[in] alias_f M_time_tz_alias_t to handle alias loading.
 * \param[in] flags   M_time_tz_load_t flags. How should the data be loaded. Specifically should lazy loading be used.
 *                    When lazying loading data is read from disk (and cached) on
 *                    demand. If lazy loading is not used all day is read from disk immediately.
 *
 * \return Time zone db.
 */
M_API M_time_tzs_t *M_time_tzs_load_zoneinfo(const char *path, M_uint32 zones, M_uint32 alias_f, M_uint32 flags);


/*! Load a tzs with data from the Windows time zone database.
 *
 * Windows only.
 *
 * \param[in] zones   M_time_tz_zones_t zones. What zones from the db should be loaded.
 * \param[in] alias_f M_time_tz_alias_t to handle alias loading.
 * \param[in] flags   M_time_tz_load_t flags. How should the data be loaded. Specifically should lazy loading be used.
 *                    When lazying loading data is read from disk (and cached) on
 *                    demand. If lazy loading is not used all day is read from disk immediately.
 *
 * \return Time zone db.
 */
M_API M_time_tzs_t *M_time_tzs_load_win_zones(M_uint32 zones, M_uint32 alias_f, M_uint32 flags);


/*! Add data from the Windows time zone database.
 *
 * Windows only.
 *
 * \param[in,out] tzs  The tz db.
 * \param[in]     name The info to load. Only the Windows name is supported.
 *
 * \return Success on success. Otherwise error condition.
 */
M_API M_time_result_t M_time_tzs_add_win_zone(M_time_tzs_t *tzs, const char *name);


/*! Add the timezone data from a Posix TZ string.
 *
 * Only the M day of week format is supported for specifying transition day.
 * Example: EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00
 *
 * \param[in,out] tzs The tz db.
 * \param[in]     str The string to parse.
 *
 * \return Success on success. Otherwise error condition.
 */
M_API M_time_result_t M_time_tzs_add_posix_str(M_time_tzs_t *tzs, const char *str);


/*! Add data from a specific TZif file.
 *
 * \param[in,out] tzs  The tz db.
 * \param[in]     path The file to load.
 * \param[in]     name The name to associate with the file.
 *
 * \return Success on success. Otherwise error condition.
 */
M_API M_time_result_t M_time_tzs_add_tzfile(M_time_tzs_t *tzs, const char *path, const char *name);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a list of loaded timezones.
 *
 * The names are stored case preserving but a lookup is case insensitive.
 *
 * \param[in] tzs The tz db.
 *
 * \return A list of names in the db.
 */
M_API M_list_str_t *M_time_tzs_get_loaded_zones(const M_time_tzs_t *tzs);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a specific tz from the db.
 *
 * The time zone will be loaded if lazy loading is in use.
 *
 * \param[in,out] tzs  The tz db.
 * \param[in] name The name of the tz. The names are looked up case insensitive.
 *
 * \return The tz on success. Otherwise NULL if a tz for the given name was not found.
 */
M_API const M_time_tz_t *M_time_tzs_get_tz(M_time_tzs_t *tzs, const char *name);

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_TIME_H__ */
