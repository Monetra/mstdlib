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

#include <time.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <mstdlib/mstdlib.h>
#include "platform/m_platform.h"
#include "time/m_time_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Overflow checking
 *
 * time_t and struct timeval members can be different types and different widths depending on the platform.
 * M_time_t and M_timeval_t members are guaranteed to be 64 bit. The system version could be less than this.
 * The mstdlib types could overflow if put into the system type.
 *
 * We could do something like if (sizeof(type) == 8). The issue here is we need to know exactly what size
 * the type is in order to use the correct M_INT#_[MAX,MIN]. Instead we calculate the min and max sizes.
 * We know that sizeof returns the number of bytes and a byte has 8 bits. From this we can determine the
 * width of the type.
 *
 * Using a 32 bit integer which is 4 bytes there is a minimum of -2147483648, and a maximum of 2147483647. 
 * Don't foget that 1 bit is reserved for the sign.
 *
 * We cannot directly shift to determine the maximum or minimum because we'll overflow the signed range.
 * This won't work for calculating the max and min for a signed value.
 *
 * Base calculation:
 *
 *     1 << (4*8-1)     = 2147483648
 *
 * Maximum:
 *
 *     (1 << (4*8-1))-1 = 2147483647
 *
 * Minimum:
 *
 *     (1 << (4*8-1))*-1 = -2147483648
 *
 * The issue here is the shift by bits-1 we get a number larger than the maximum which is 2147483647. We get an
 * overflow and it wraps. Subtract the result by -1 gives us the correct maximum. However, this is still an overflow
 * and this will cause overflow warning to be produced. The same is true by using this method to determine the minimum.
 * We overflow the max and then multiply by -1 to get the correct minimum but this still causes an overflow warning.
 *
 * To prevent overflow half of the maximum is used. For max we'll subtract 1 from one half and add it to the other
 * half. We'll do the same for minimum but then use a twos complement (~) to invert the bits and give us the negative.
 *
 * Base calculation:
 *
 *     1 << (4*8-2) = 1073741824
 *
 * Maximum:
 *
 *     ((1 << (4*8-2))-1)+(1 << (4*8-2)) = 2147483647
 *     1073741824 - 1                    = 1073741823 
 *     1073741823 + 1073741824           = 2147483647
 *
 * Minimum:
 *
 *     ~(((1 << (4*8-2))-1)+(1 << (4*8-2))) = -2147483648
 *     1073741824 - 1                       = 1073741823 
 *     1073741823 + 1073741824              = 2147483647
 *     ~2147483647                          = -2147483648
 * 
 * sizeof(type) is used to determine the number of bytes (which in the above examples is 4).
 *
 * For struct timeval the type for each part is typedefed to the platform type. This allows the correct type to be
 * used in the sizeof for the platform. It means that if the underlying type changes the calculation will still be
 * valid.
 *
 * Examples of a change in type:
 * - On some 32 bit platforms time_t is 32 bit on some 64 bit platforms it is 64 bit.
 * - Windows will typedef time_t to _time32 or _time64 depending on the build flags on 32bit systems.
 * - struct timeval's members can be time_t, suseconds_t, long.
 *
 * For this situation C89 defines time_t must be a singed value. struct timeval on all platforms uses time_t or
 * long for tv_sec. struct timeval's tv_usec and be suseconds_t or long. suseconds_t must, again, be a signed
 * value. Thus we don't need to worry if these are signed or unsigned types.
 *
 * Finally, if calculating an unsigned types maximum value is to do the same as above with the halves
 * but subtracting 1 instead of 2 from the bits. The minimum will always be 0 for unsigned.
 *
 * Meaning:
 *
 *     1 << (4*8-1)              = 2147483648
 *     2147483648-1 + 2147483648 = 4294967295
 */
static time_t TIME_T_MIN = (~((((time_t)1 << (sizeof(time_t)*8-2))-1) + ((time_t)1 << (sizeof(time_t)*8-2))));
static time_t TIME_T_MAX = ((((time_t)1 << (sizeof(time_t)*8-2))-1) + ((time_t)1 << (sizeof(time_t)*8-2)));

static M_time_tv_sec_t M_TIME_TV_SEC_MIN = (~((((M_time_tv_sec_t)1 << (sizeof(M_time_tv_sec_t)*8-2))-1) + ((M_time_tv_sec_t)1 << (sizeof(M_time_tv_sec_t)*8-2))));
static M_time_tv_sec_t M_TIME_TV_SEC_MAX =((((M_time_tv_sec_t)1 << (sizeof(M_time_tv_sec_t)*8-2))-1) + ((M_time_tv_sec_t)1 << (sizeof(M_time_tv_sec_t)*8-2)));

static M_time_tv_usec_t M_TIME_TV_USEC_MIN = (~((((M_time_tv_usec_t)1 << (sizeof(M_time_tv_usec_t)*8-2))-1) + ((M_time_tv_usec_t)1 << (sizeof(M_time_tv_usec_t)*8-2))));
static M_time_tv_usec_t M_TIME_TV_USEC_MAX =((((M_time_tv_usec_t)1 << (sizeof(M_time_tv_usec_t)*8-2))-1) + ((M_time_tv_usec_t)1 << (sizeof(M_time_tv_usec_t)*8-2)));

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN32
void M_time_timeval_from_filetime(const FILETIME *ft, M_timeval_t *tv)
{
	M_int64 l;

	if (tv != NULL)
		M_mem_set(tv, 0, sizeof(*tv));
	if (ft == NULL || tv == NULL)
		return;
	
	/* Copy to 64bit signed integer */
	l = (M_int64)(((M_uint64)ft->dwHighDateTime) << 32 | (M_uint64)ft->dwLowDateTime);

	/* Bring from 1 January 1601, 00:00:00 to 1 January 1970, 00:00:00 */
	l -= 116444736000000000;
	
	/* Bring down from 100 ns representation to microseconds */
	l /= 10;

	/* Break out seconds, then remainder is microseconds */
	tv->tv_sec  = (M_time_t)(l / 1000000);
	tv->tv_usec = (M_suseconds_t)(l % 1000000);
}

M_time_t M_time_from_filetime(const FILETIME *ft)
{
	M_int64 l;

	/* Copy to 64bit signed integer */
	l = (M_int64)(((M_uint64)ft->dwHighDateTime) << 32 | (M_uint64)ft->dwLowDateTime);

	if (l == 0)
		return 0;
	return l / 10000000 - 11644473600;
}

/* http://support.microsoft.com/kb/167296 */
void M_time_to_filetime(M_time_t t, FILETIME *ft)
{
	LONGLONG l;

	l = Int32x32To64(t, 10000000) + 116444736000000000;
	ft->dwLowDateTime  = (DWORD)l;
	ft->dwHighDateTime = (DWORD)(l >> 32);
}

void M_time_to_systemtime(M_time_t t, SYSTEMTIME *st)
{
	FILETIME ft;
	M_time_to_filetime(t, &ft);
	FileTimeToSystemTime(&ft, st);
}
#endif

time_t M_time_M_time_t_to_time_t(M_time_t t)
{
	time_t st = 0;

	if (t < TIME_T_MIN) {
		st = TIME_T_MIN;
	} else if (t > TIME_T_MAX) {
		st = TIME_T_MAX;
	} else {
		st = (time_t)t;
	}

	return st;
}

void M_time_M_timeval_t_to_struct_timeval(const M_timeval_t *mtv, struct timeval *stv, M_bool can_neg)
{
	if (mtv == NULL || stv == NULL)
		return;

	M_mem_set(stv, 0, sizeof(*stv));

	if (!can_neg && mtv->tv_sec < 0) {
		stv->tv_sec = 0;
	} else if (can_neg && mtv->tv_sec < M_TIME_TV_SEC_MIN) {
		stv->tv_sec = M_TIME_TV_SEC_MIN;
	} else if (mtv->tv_sec > M_TIME_TV_SEC_MAX) {
		stv->tv_sec = M_TIME_TV_SEC_MAX;
	} else {
		stv->tv_sec = (M_time_tv_sec_t)mtv->tv_sec;
	}

	if (!can_neg && mtv->tv_usec < 0) {
		stv->tv_usec = 0;
	} else if (can_neg && mtv->tv_usec < M_TIME_TV_USEC_MIN) {
		stv->tv_usec = M_TIME_TV_USEC_MIN;
	} else if (mtv->tv_usec > M_TIME_TV_USEC_MAX) {
		stv->tv_usec = M_TIME_TV_USEC_MAX;
	} else {
		stv->tv_usec = (M_time_tv_usec_t)mtv->tv_usec;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Source (2013/08/26): http://unicode.org/repos/cldr/trunk/common/supplemental/windowsZones.xml */
M_time_tz_info_map_t M_time_tz_zone_map[] = {
	{ "Africa/Abidjan",                 "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Accra",                   "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Addis_Ababa",             "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Algiers",                 "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Asmera",                  "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Bamako",                  "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Bangui",                  "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Banjul",                  "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Bissau",                  "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Blantyre",                "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Brazzaville",             "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Bujumbura",               "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Cairo",                   "Egypt Standard Time",             M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Casablanca",              "Morocco Standard Time",           M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Ceuta",                   "Romance Standard Time",           M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Conakry",                 "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Dakar",                   "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Dar_es_Salaam",           "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Djibouti",                "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Douala",                  "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/El_Aaiun",                "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Freetown",                "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Gaborone",                "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Harare",                  "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Johannesburg",            "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Juba",                    "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Kampala",                 "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Khartoum",                "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Kigali",                  "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Kinshasa",                "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Lagos",                   "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Libreville",              "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Lome",                    "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Luanda",                  "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Lubumbashi",              "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Lusaka",                  "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Malabo",                  "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Maputo",                  "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Maseru",                  "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Mbabane",                 "South Africa Standard Time",      M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Mogadishu",               "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Monrovia",                "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Nairobi",                 "E. Africa Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Ndjamena",                "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Niamey",                  "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Nouakchott",              "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Ouagadougou",             "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Porto-Novo",              "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Sao_Tome",                "Greenwich Standard Time",         M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Tripoli",                 "Libya Standard Time",             M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "Africa/Tunis",                   "W. Central Africa Standard Time", M_TIME_TZ_ZONE_AFRICA,     M_FALSE },
	{ "Africa/Windhoek",                "Namibia Standard Time",           M_TIME_TZ_ZONE_AFRICA,     M_TRUE  },
	{ "America/Anchorage",              "Alaskan Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Anguilla",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Antigua",                "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Araguaina",              "E. South America Standard Time",  M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/La_Rioja",     "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/Rio_Gallegos", "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/Salta",        "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/San_Juan",     "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/San_Luis",     "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/Tucuman",      "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Argentina/Ushuaia",      "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Aruba",                  "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Asuncion",               "Paraguay Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Bahia",                  "Bahia Standard Time",             M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Bahia_Banderas",         "Central Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Barbados",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Belem",                  "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Belize",                 "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Blanc-Sablon",           "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Boa_Vista",              "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Bogota",                 "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Boise",                  "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Buenos_Aires",           "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Cambridge_Bay",          "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Campo_Grande",           "Central Brazilian Standard Time", M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Cancun",                 "Central Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Caracas",                "Venezuela Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Catamarca",              "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Cayenne",                "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Cayman",                 "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Chicago",                "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Chihuahua",              "Mountain Standard Time (Mexico)", M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Coral_Harbour",          "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Cordoba",                "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Costa_Rica",             "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Creston",                "US Mountain Standard Time",       M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Cuiaba",                 "Central Brazilian Standard Time", M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Curacao",                "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Danmarkshavn",           "UTC",                             M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Dawson",                 "Pacific Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Dawson_Creek",           "US Mountain Standard Time",       M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Denver",                 "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Detroit",                "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Dominica",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Edmonton",               "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Eirunepe",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/El_Salvador",            "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Fortaleza",              "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Glace_Bay",              "Atlantic Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Godthab",                "Greenland Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Goose_Bay",              "Atlantic Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Grand_Turk",             "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Grenada",                "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Guadeloupe",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Guatemala",              "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Guayaquil",              "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Guyana",                 "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Halifax",                "Atlantic Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Havana",                 "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Hermosillo",             "US Mountain Standard Time",       M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Knox",           "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Marengo",        "US Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Petersburg",     "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Tell_City",      "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Vevay",          "US Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Vincennes",      "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indiana/Winamac",        "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Indianapolis",           "US Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Inuvik",                 "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Iqaluit",                "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Jamaica",                "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Jujuy",                  "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Juneau",                 "Alaskan Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Kentucky/Monticello",    "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Kralendijk",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/La_Paz",                 "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Lima",                   "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Los_Angeles",            "Pacific Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Louisville",             "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Lower_Princes",          "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Maceio",                 "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Managua",                "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Manaus",                 "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Marigot",                "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Martinique",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Matamoros",              "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Mazatlan",               "Mountain Standard Time (Mexico)", M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Mendoza",                "Argentina Standard Time",         M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Menominee",              "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Merida",                 "Central Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Mexico_City",            "Central Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Moncton",                "Atlantic Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Monterrey",              "Central Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Montevideo",             "Montevideo Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Montreal",               "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Montserrat",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Nassau",                 "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/New_York",               "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Nipigon",                "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Nome",                   "Alaskan Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Noronha",                "UTC-02",                          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/North_Dakota/Beulah",    "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/North_Dakota/Center",    "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/North_Dakota/New_Salem", "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Ojinaga",                "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Panama",                 "SA Pacific Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Pangnirtung",            "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Paramaribo",             "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Phoenix",                "US Mountain Standard Time",       M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Port-au-Prince",         "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Port_of_Spain",          "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Porto_Velho",            "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Puerto_Rico",            "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Rainy_River",            "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Rankin_Inlet",           "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Recife",                 "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Regina",                 "Canada Central Standard Time",    M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Resolute",               "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Rio_Branco",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Santa_Isabel",           "Pacific Standard Time (Mexico)",  M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Santarem",               "SA Eastern Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Santiago",               "Pacific SA Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Santo_Domingo",          "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Sao_Paulo",              "E. South America Standard Time",  M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/Scoresbysund",           "Azores Standard Time",            M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Shiprock",               "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Sitka",                  "Alaskan Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/St_Barthelemy",          "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/St_Johns",               "Newfoundland Standard Time",      M_TIME_TZ_ZONE_AMERICA,    M_TRUE  },
	{ "America/St_Kitts",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/St_Lucia",               "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/St_Thomas",              "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/St_Vincent",             "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Swift_Current",          "Canada Central Standard Time",    M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Tegucigalpa",            "Central America Standard Time",   M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Thule",                  "Atlantic Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Thunder_Bay",            "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Tijuana",                "Pacific Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Toronto",                "Eastern Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Tortola",                "SA Western Standard Time",        M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Vancouver",              "Pacific Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Whitehorse",             "Pacific Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Winnipeg",               "Central Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Yakutat",                "Alaskan Standard Time",           M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "America/Yellowknife",            "Mountain Standard Time",          M_TIME_TZ_ZONE_AMERICA,    M_FALSE },
	{ "Antarctica/Casey",               "W. Australia Standard Time",      M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Davis",               "SE Asia Standard Time",           M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/DumontDUrville",      "West Pacific Standard Time",      M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Macquarie",           "Central Pacific Standard Time",   M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Mawson",              "West Asia Standard Time",         M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/McMurdo",             "New Zealand Standard Time",       M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Palmer",              "Pacific SA Standard Time",        M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Rothera",             "SA Eastern Standard Time",        M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/South_Pole",          "New Zealand Standard Time",       M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Syowa",               "E. Africa Standard Time",         M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Antarctica/Vostok",              "Central Asia Standard Time",      M_TIME_TZ_ZONE_ANTARCTICA, M_FALSE },
	{ "Arctic/Longyearbyen",            "W. Europe Standard Time",         M_TIME_TZ_ZONE_ARCTIC,     M_FALSE },
	{ "Asia/Aden",                      "Arab Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Almaty",                    "Central Asia Standard Time",      M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Amman",                     "Jordan Standard Time",            M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Anadyr",                    "Magadan Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Aqtau",                     "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Aqtobe",                    "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Ashgabat",                  "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Baghdad",                   "Arabic Standard Time",            M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Bahrain",                   "Arab Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Baku",                      "Azerbaijan Standard Time",        M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Bangkok",                   "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Beirut",                    "Middle East Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Bishkek",                   "Central Asia Standard Time",      M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Brunei",                    "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Calcutta",                  "India Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Choibalsan",                "Ulaanbaatar Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Chongqing",                 "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Colombo",                   "Sri Lanka Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Damascus",                  "Syria Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Dhaka",                     "Bangladesh Standard Time",        M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Dili",                      "Tokyo Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Dubai",                     "Arabian Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Dushanbe",                  "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Harbin",                    "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Hong_Kong",                 "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Hovd",                      "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Irkutsk",                   "North Asia East Standard Time",   M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Jakarta",                   "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Jayapura",                  "Tokyo Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Jerusalem",                 "Israel Standard Time",            M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Kabul",                     "Afghanistan Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Kamchatka",                 "Magadan Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Karachi",                   "Pakistan Standard Time",          M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Kashgar",                   "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Katmandu",                  "Nepal Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Khandyga",                  "Yakutsk Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Krasnoyarsk",               "North Asia Standard Time",        M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Kuala_Lumpur",              "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Kuching",                   "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Kuwait",                    "Arab Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Macau",                     "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Magadan",                   "Magadan Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Makassar",                  "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Manila",                    "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Muscat",                    "Arabian Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Nicosia",                   "E. Europe Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Novokuznetsk",              "N. Central Asia Standard Time",   M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Novosibirsk",               "N. Central Asia Standard Time",   M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Omsk",                      "N. Central Asia Standard Time",   M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Oral",                      "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Phnom_Penh",                "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Pontianak",                 "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Pyongyang",                 "Korea Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Qatar",                     "Arab Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Qyzylorda",                 "Central Asia Standard Time",      M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Rangoon",                   "Myanmar Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Riyadh",                    "Arab Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Saigon",                    "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Sakhalin",                  "Vladivostok Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Samarkand",                 "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Seoul",                     "Korea Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Shanghai",                  "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Singapore",                 "Singapore Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Taipei",                    "Taipei Standard Time",            M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Tashkent",                  "West Asia Standard Time",         M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Tbilisi",                   "Georgian Standard Time",          M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Tehran",                    "Iran Standard Time",              M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Thimphu",                   "Bangladesh Standard Time",        M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Tokyo",                     "Tokyo Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Ulaanbaatar",               "Ulaanbaatar Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Urumqi",                    "China Standard Time",             M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Ust-Nera",                  "Vladivostok Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Vientiane",                 "SE Asia Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_FALSE },
	{ "Asia/Vladivostok",               "Vladivostok Standard Time",       M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Yakutsk",                   "Yakutsk Standard Time",           M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Yekaterinburg",             "Ekaterinburg Standard Time",      M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Asia/Yerevan",                   "Caucasus Standard Time",          M_TIME_TZ_ZONE_ASIA,       M_TRUE  },
	{ "Atlantic/Azores",                "Azores Standard Time",            M_TIME_TZ_ZONE_ATLANTIC,   M_TRUE  },
	{ "Atlantic/Bermuda",               "Atlantic Standard Time",          M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/Canary",                "GMT Standard Time",               M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/Cape_Verde",            "Cape Verde Standard Time",        M_TIME_TZ_ZONE_ATLANTIC,   M_TRUE  },
	{ "Atlantic/Faeroe",                "GMT Standard Time",               M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/Madeira",               "GMT Standard Time",               M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/Reykjavik",             "Greenwich Standard Time",         M_TIME_TZ_ZONE_ATLANTIC,   M_TRUE  },
	{ "Atlantic/South_Georgia",         "UTC-02",                          M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/St_Helena",             "Greenwich Standard Time",         M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Atlantic/Stanley",               "SA Eastern Standard Time",        M_TIME_TZ_ZONE_ATLANTIC,   M_FALSE },
	{ "Australia/Adelaide",             "Cen. Australia Standard Time",    M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Australia/Brisbane",             "E. Australia Standard Time",      M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Australia/Broken_Hill",          "Cen. Australia Standard Time",    M_TIME_TZ_ZONE_AUSTRALIA,  M_FALSE },
	{ "Australia/Currie",               "Tasmania Standard Time",          M_TIME_TZ_ZONE_AUSTRALIA,  M_FALSE },
	{ "Australia/Darwin",               "AUS Central Standard Time",       M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Australia/Hobart",               "Tasmania Standard Time",          M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Australia/Lindeman",             "E. Australia Standard Time",      M_TIME_TZ_ZONE_AUSTRALIA,  M_FALSE },
	{ "Australia/Melbourne",            "AUS Eastern Standard Time",       M_TIME_TZ_ZONE_AUSTRALIA,  M_FALSE },
	{ "Australia/Perth",                "W. Australia Standard Time",      M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Australia/Sydney",               "AUS Eastern Standard Time",       M_TIME_TZ_ZONE_AUSTRALIA,  M_TRUE  },
	{ "Etc/GMT",                        "UTC",                             M_TIME_TZ_ZONE_ETC,        M_TRUE  },
	{ "Etc/GMT+1",                      "Cape Verde Standard Time",        M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+10",                     "Hawaiian Standard Time",          M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+11",                     "UTC-11",                          M_TIME_TZ_ZONE_ETC,        M_TRUE  },
	{ "Etc/GMT+12",                     "Dateline Standard Time",          M_TIME_TZ_ZONE_ETC,        M_TRUE  },
	{ "Etc/GMT+2",                      "UTC-02",                          M_TIME_TZ_ZONE_ETC,        M_TRUE  },
	{ "Etc/GMT+3",                      "SA Eastern Standard Time",        M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+4",                      "SA Western Standard Time",        M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+5",                      "SA Pacific Standard Time",        M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+6",                      "Central America Standard Time",   M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT+7",                      "US Mountain Standard Time",       M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-1",                      "W. Central Africa Standard Time", M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-10",                     "West Pacific Standard Time",      M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-11",                     "Central Pacific Standard Time",   M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-12",                     "UTC+12",                          M_TIME_TZ_ZONE_ETC,        M_TRUE  },
	{ "Etc/GMT-13",                     "Tonga Standard Time",             M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-2",                      "South Africa Standard Time",      M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-3",                      "E. Africa Standard Time",         M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-4",                      "Arabian Standard Time",           M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-5",                      "West Asia Standard Time",         M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-6",                      "Central Asia Standard Time",      M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-7",                      "SE Asia Standard Time",           M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-8",                      "Singapore Standard Time",         M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Etc/GMT-9",                      "Tokyo Standard Time",             M_TIME_TZ_ZONE_ETC,        M_FALSE },
	{ "Europe/Amsterdam",               "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Andorra",                 "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Athens",                  "GTB Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Belgrade",                "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Berlin",                  "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Bratislava",              "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Brussels",                "Romance Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Bucharest",               "GTB Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Budapest",                "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Busingen",                "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Chisinau",                "GTB Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Copenhagen",              "Romance Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Dublin",                  "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Gibraltar",               "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Guernsey",                "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Helsinki",                "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Isle_of_Man",             "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Istanbul",                "Turkey Standard Time",            M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Jersey",                  "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Kaliningrad",             "Kaliningrad Standard Time",       M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Kiev",                    "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Lisbon",                  "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Ljubljana",               "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/London",                  "GMT Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Luxembourg",              "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Madrid",                  "Romance Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Malta",                   "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Mariehamn",               "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Minsk",                   "Kaliningrad Standard Time",       M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Monaco",                  "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Moscow",                  "Russian Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Oslo",                    "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Paris",                   "Romance Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Podgorica",               "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Prague",                  "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Riga",                    "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Rome",                    "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Samara",                  "Russian Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/San_Marino",              "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Sarajevo",                "Central European Standard Time",  M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Simferopol",              "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Skopje",                  "Central European Standard Time",  M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Sofia",                   "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Stockholm",               "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Tallinn",                 "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Tirane",                  "Central Europe Standard Time",    M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Uzhgorod",                "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Vaduz",                   "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Vatican",                 "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Vienna",                  "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Vilnius",                 "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Volgograd",               "Russian Standard Time",           M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Warsaw",                  "Central European Standard Time",  M_TIME_TZ_ZONE_EUROPE,     M_TRUE  },
	{ "Europe/Zagreb",                  "Central European Standard Time",  M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Zaporozhye",              "FLE Standard Time",               M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Europe/Zurich",                  "W. Europe Standard Time",         M_TIME_TZ_ZONE_EUROPE,     M_FALSE },
	{ "Indian/Antananarivo",            "E. Africa Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Chagos",                  "Central Asia Standard Time",      M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Christmas",               "SE Asia Standard Time",           M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Cocos",                   "Myanmar Standard Time",           M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Comoro",                  "E. Africa Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Kerguelen",               "West Asia Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Mahe",                    "Mauritius Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Maldives",                "West Asia Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Mauritius",               "Mauritius Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_TRUE  },
	{ "Indian/Mayotte",                 "E. Africa Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Indian/Reunion",                 "Mauritius Standard Time",         M_TIME_TZ_ZONE_INDIAN,     M_FALSE },
	{ "Pacific/Apia",                   "Samoa Standard Time",             M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Auckland",               "New Zealand Standard Time",       M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Efate",                  "Central Pacific Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Enderbury",              "Tonga Standard Time",             M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Fakaofo",                "Tonga Standard Time",             M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Fiji",                   "Fiji Standard Time",              M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Funafuti",               "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Galapagos",              "Central America Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Guadalcanal",            "Central Pacific Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Guam",                   "West Pacific Standard Time",      M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Honolulu",               "Hawaiian Standard Time",          M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Johnston",               "Hawaiian Standard Time",          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Kosrae",                 "Central Pacific Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Kwajalein",              "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Majuro",                 "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Midway",                 "UTC-11",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Nauru",                  "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Niue",                   "UTC-11",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Noumea",                 "Central Pacific Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Pago_Pago",              "UTC-11",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Palau",                  "Tokyo Standard Time",             M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Ponape",                 "Central Pacific Standard Time",   M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Port_Moresby",           "West Pacific Standard Time",      M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Rarotonga",              "Hawaiian Standard Time",          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Saipan",                 "West Pacific Standard Time",      M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Tahiti",                 "Hawaiian Standard Time",          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Tarawa",                 "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Tongatapu",              "Tonga Standard Time",             M_TIME_TZ_ZONE_PACIFIC,    M_TRUE  },
	{ "Pacific/Truk",                   "West Pacific Standard Time",      M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Wake",                   "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ "Pacific/Wallis",                 "UTC+12",                          M_TIME_TZ_ZONE_PACIFIC,    M_FALSE },
	{ NULL,                             NULL,                              0,                         M_FALSE }
};
