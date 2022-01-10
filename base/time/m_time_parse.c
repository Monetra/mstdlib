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

/*! Returns 1 on success, 0 on failure, -1 on critical failure 
 * Support 'now', 'epoch', 'yesterday', 'today', 'tomorrow', '+/-N magnitude', where magnitude is:
 *	year, month, day, hour, min, sec
 *	where long names and plural are supported
 * Ex: +6 Months  or -7 hours
 */
static int M_time_parseoffset(const char *timestr, const M_time_tz_t *tz, M_time_t *out_time)
{
	M_time_t          t;
	M_time_localtm_t  result;
	M_time_localtm_t  ltime;
	int               offset = 0;
	char             *temp   = NULL;
	char             *ptr    = NULL;

	t = M_time();

	if (timestr == NULL || !M_str_len(timestr))
		return -1;

	/* skip over spaces and tabs */
	while (timestr[0] == ' ' || timestr[0] == '\t')
		timestr++;

	if (M_str_caseeq(timestr, "now") || M_str_caseeq(timestr, "today")) {
		*out_time = t;
		return 1;
	} else if (M_str_caseeq(timestr, "epoch")) {
		*out_time = 0;
		return 1;
	} else if (M_str_caseeq(timestr, "yesterday")) {
		timestr = "-1 day";
	} else if (M_str_caseeq(timestr, "tomorrow")) {
		timestr = "+1 day";
	}

	/* Not in offset format */
	if (timestr[0] != '-' && timestr[0] != '+')
		return 0;

	temp = M_strdup(timestr);
	ptr  = M_str_chr(temp, ' ');
	if (ptr == NULL) {
		M_free(temp);
		return -1;
	}
	ptr[0] = 0;
	offset = M_str_to_int32(temp);
	ptr++;

	/* skip over spaces and tabs */
	while (ptr[0] == ' ' || ptr[0] == '\t')
		ptr++;

	M_time_tolocal(t, &ltime, tz);
	M_mem_set(&result, 0, sizeof(result));
	result.isdst = -1;
	result.year  = ltime.year;
	result.month = ltime.month;
	result.day   = ltime.day;
	result.hour  = ltime.hour;
	result.min   = ltime.min;
	result.sec   = ltime.sec;

	if (M_str_caseeq_max(ptr, "year", 4))
		result.year += offset;
	else if (M_str_caseeq_max(ptr, "month", 5))
		result.month += offset;
	else if (M_str_caseeq_max(ptr, "week", 4))
		result.day += 7*offset;
	else if (M_str_caseeq_max(ptr, "day", 3))
		result.day += offset;
	else if (M_str_caseeq_max(ptr, "hour", 4))
		result.hour += offset;
	else if (M_str_caseeq_max(ptr, "min", 3))
		result.min += offset;
	else if (M_str_caseeq_max(ptr, "sec", 3))
		result.sec += offset;
	else {
		M_free(temp);
		return -1;
	}

	M_free(temp);
	(*out_time) = M_time_fromlocal(&result, tz);

	return 1;
}

/**
 * returns numerical value for the buffer position
 * consuming at most 'len' but up to next delimiter
 * returns -1 if not a #
 */
static M_int64 M_time_getnum(const char **s, int len)
{
	M_int64 x;

	if(!M_chr_isdigit(**s))
		return -1;

	for(x=0; len && **s != 0 && M_chr_isdigit(**s); (*s)++)
	{
		x *= 10;
		x += **s - '0';
		len--;
	}

	return x;
}

static M_int64 M_time_get_ampm_hour(const char **s, M_int64 hour)
{
	M_bool is_pm = M_FALSE;

	if (hour > 23 || hour < 0)
		return -1;

	if (M_str_caseeq_max(*s, "p.m.", 4)) {
		is_pm = M_TRUE;
	} else if (M_str_caseeq_max(*s, "a.m.", 4)) {
		/* Pass */
	} else if (M_str_caseeq_max(*s, "pm", 2)) {
		is_pm = M_TRUE;
		*s += 2;
	} else if (M_str_caseeq_max(*s, "am", 2)) {
		/* Pass */
		*s += 2;
	} else if (M_str_caseeq_max(*s, "p", 1)) {
		is_pm = M_TRUE;
		*s += 1;
	} else if (M_str_caseeq_max(*s, "a", 1)) {
		/* Pass */
		*s += 1;
	} else {
		return -1;
	}


	/* AM with a PM hour is invalid. */
	if (!is_pm && hour > 12) 
		return -1;

	if (is_pm) {
		if (hour < 12 && hour != 0) {
			hour += 12;
		}
	} else if (hour == 12) {
		hour = 0;
	}

	return hour;
}

/* RFC-822/ISO 8601 standard timezone specification parser.
 *
 * RFC-822
 * zone =  "UT"  / "GMT"         ; Universal Time
 *                               ; North American : UT
 *      /  "EST" / "EDT"         ;  Eastern:  - 5/ - 4
 *      /  "CST" / "CDT"         ;  Central:  - 6/ - 5
 *      /  "MST" / "MDT"         ;  Mountain: - 7/ - 6
 *      /  "PST" / "PDT"         ;  Pacific:  - 8/ - 7
 *      /  1ALPHA                ; Military: Z = UT;
 *                               ;  A:-1; (J not used)
 *                               ;  M:-12; N:+1; Y:+12
 *      / ( ("+" / "-") 4DIGIT ) ; Local differential
 *                               ;  hours+min. (HHMM)
 *
 *     Time zone may be indicated in several ways.  "UT" is Univer-
 * sal  Time  (formerly called "Greenwich Mean Time"); "GMT" is per-
 * mitted as a reference to Universal Time.  The  military  standard
 * uses  a  single  character for each zone.  "Z" is Universal Time.
 * "A" indicates one hour earlier, and "M" indicates 12  hours  ear-
 * lier;  "N"  is  one  hour  later, and "Y" is 12 hours later.  The
 * letter "J" is not used.  The other remaining two forms are  taken
 * from ANSI standard X3.51-1975.  One allows explicit indication of
 * the amount of offset from UT; the other uses  common  3-character
 * strings for indicating time zones in North America.
 *
 * ISO 8601
 * [+-]hh:mm
 * [+-]hhmm
 * [+-]hh
 */
static M_bool M_time_getoffset(const char **s, M_time_t *gmtoff)
{
	M_time_t x;
	M_bool   isneg = M_FALSE;

	if (s == NULL || *s == NULL || **s == '\0' || gmtoff == NULL)
		return M_FALSE;
	*gmtoff = 0;

	switch (**s) {
		/* UTC identifiers */
		case 'U':
		case 'G':
			if (M_str_eq_max(*s, "UTC", 3) || M_str_eq_max(*s, "GMT", 3)) {
				*s += 3;
			} else if (M_str_eq_max(*s, "UT", 2)) {
				*s += 2;
			} else if (**s == 'U') {
				(*s)++;
				*gmtoff = 8;
			} else if (**s == 'G') {
				(*s)++;
				*gmtoff = -7;
			} else {
				return M_FALSE;
			}
			break;
		case 'E':
			if (M_str_eq_max(*s, "EST", 3)) {
				*s += 3;
				*gmtoff = -5;
			} else if (M_str_eq_max(*s, "EDT", 3)) {
				*s += 3;
				*gmtoff = -4;
			} else if (**s == 'E') {
				(*s)++;
				*gmtoff = -5;
			} else {
				return M_FALSE;
			}
			break;
		case 'C':
			if (M_str_eq_max(*s, "CST", 3)) {
				*s += 3;
				*gmtoff = -6;
			} else if (M_str_caseeq_max(*s, "cdt", 3)) {
				*s += 3;
				*gmtoff = -5;
			} else if (**s == 'C') {
				(*s)++;
				*gmtoff = -3;
			} else {
				return M_FALSE;
			}
			break;
		case 'M':
			if (M_str_eq_max(*s, "MST", 3)) {
				*s += 3;
				*gmtoff = -7;
			} else if (M_str_eq_max(*s, "MDT", 3)) {
				*s += 3;
				*gmtoff = -6;
			} else if (**s == 'M') {
				(*s)++;
				*gmtoff = -12;
			} else {
				return M_FALSE;
			}
			break;
		case 'P':
			if (M_str_eq_max(*s, "PST", 3)) {
				*s += 3;
				*gmtoff = -8;
			} else if (M_str_eq_max(*s, "PDT", 3)) {
				*s += 3;
				*gmtoff = -7;
			} else if (**s == 'M') {
				(*s)++;
				*gmtoff = 3;
			} else {
				return M_FALSE;
			}
			break;
		case 'A':
		case 'B':
		case 'D':
		case 'F':
		case 'H':
		case 'I':
			*gmtoff = (**s - 'A' + 1) * -1;
			(*s)++;
			break;
		case 'K':
		case 'L':
			*gmtoff = (**s - 'A') * -1;
			(*s)++;
			break;
		case 'N':
		case 'O':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
			*gmtoff = (**s - 'N' + 1);
			(*s)++;
			break;
		case 'Z':
			/* UTC. No offset nothing to do. */
			(*s)++;
			break;
		case '-':
			isneg = M_TRUE;
			/* Fallthrough */
		case '+':
			(*s)++;
			/* Fallthrough */
		case '0':
		case '1':
		case '2':
			/* Get hours. */
			x = (M_time_t)M_time_getnum(s, 2);
			*gmtoff = x*60*60;
			/* Check for : and skip past. */
			if (**s == ':') {
				(*s)++;
			}
			/* Check if we have minutes. */
			if (M_chr_isdigit(**s)) {
				/* Get minutes. */
				x = (M_time_t)M_time_getnum(s, 2);
				*gmtoff += x*60;
			}
			/* Set direction. */
			if (isneg)
				*gmtoff *= -1;
			break;
		default:
			return M_FALSE;
	}

	return M_TRUE;
}

M_time_t M_time_from_str(const char *timestr, const M_time_tz_t *tz, M_bool default_end_of_day)
{
	struct {
		const char *fmt;
		M_bool      has_gmtoff;
	} formats[] = {
		/* All of these have spaces to break up different parts of the
 		 * format but M_time_parsefmt ignores white space. So we don't need
		 * duplicates with variations on spaces between elements.
		 *
		 * Order does matter. For example %H formats must come before %I formats
		 * that are the same but with that one difference.
		 */
		{ "%m/%d/%Y %H",            M_FALSE },
		{ "%m/%d/%Y %H %P",         M_FALSE },
		{ "%m/%d/%Y %H %p",         M_FALSE },
		{ "%m/%d/%Y %I %P",         M_FALSE },
		{ "%m/%d/%Y %I %p",         M_FALSE },
		{ "%m/%d/%Y %H%M",          M_FALSE },
		{ "%m/%d/%Y %H%M %P",       M_FALSE },
		{ "%m/%d/%Y %H%M %p",       M_FALSE },
		{ "%m/%d/%Y %I%M %P",       M_FALSE },
		{ "%m/%d/%Y %I%M %p",       M_FALSE },
		{ "%m/%d/%Y %H%M%S",        M_FALSE },
		{ "%m/%d/%Y %H%M%S %P",     M_FALSE },
		{ "%m/%d/%Y %H%M%S %p",     M_FALSE },
		{ "%m/%d/%Y %H%M%S %z",     M_TRUE, },
		{ "%m/%d/%Y %I%M%S %P",     M_FALSE },
		{ "%m/%d/%Y %I%M%S %p",     M_FALSE },
		{ "%m/%d/%Y %H%M%S %z",     M_TRUE, },
		{ "%m/%d/%Y %H:%M",         M_FALSE },
		{ "%m/%d/%Y %H:%M %P",      M_FALSE },
		{ "%m/%d/%Y %H:%M %p",      M_FALSE },
		{ "%m/%d/%Y %I:%M %P",      M_FALSE },
		{ "%m/%d/%Y %I:%M %p",      M_FALSE },
		{ "%m/%d/%Y %H:%M:%S",      M_FALSE },
		{ "%m/%d/%Y %H:%M:%S %P",   M_FALSE },
		{ "%m/%d/%Y %H:%M:%S %p",   M_FALSE },
		{ "%m/%d/%Y %I:%M:%S %P",   M_FALSE },
		{ "%m/%d/%Y %I:%M:%S %p",   M_FALSE },
		{ "%m/%d/%Y %H:%M:%S %z",   M_TRUE, },
		{ "%m/%d/%Y %H-%M",         M_FALSE },
		{ "%m/%d/%Y %H-%M %P",      M_FALSE },
		{ "%m/%d/%Y %H-%M %p",      M_FALSE },
		{ "%m/%d/%Y %I-%M %P",      M_FALSE },
		{ "%m/%d/%Y %I-%M %p",      M_FALSE },
		{ "%m/%d/%Y %H-%M-%S",      M_FALSE },
		{ "%m/%d/%Y %H-%M-%S %P",   M_FALSE },
		{ "%m/%d/%Y %H-%M-%S %p",   M_FALSE },
		{ "%m/%d/%Y %I-%M-%S %P",   M_FALSE },
		{ "%m/%d/%Y %I-%M-%S %p",   M_FALSE },
		{ "%m/%d/%Y %H-%M-%S %z",   M_TRUE, },

		{ "%m/%d/%Y T %H",          M_FALSE },
		{ "%m/%d/%Y T %H %P",       M_FALSE },
		{ "%m/%d/%Y T %H %p",       M_FALSE },
		{ "%m/%d/%Y T %I %P",       M_FALSE },
		{ "%m/%d/%Y T %I %p",       M_FALSE },
		{ "%m/%d/%Y T %H%M",        M_FALSE },
		{ "%m/%d/%Y T %H%M %P",     M_FALSE },
		{ "%m/%d/%Y T %H%M %p",     M_FALSE },
		{ "%m/%d/%Y T %I%M %P",     M_FALSE },
		{ "%m/%d/%Y T %I%M %p",     M_FALSE },
		{ "%m/%d/%Y T %H%M%S",      M_FALSE },
		{ "%m/%d/%Y T %H%M%S %P",   M_FALSE },
		{ "%m/%d/%Y T %H%M%S %p",   M_FALSE },
		{ "%m/%d/%Y T %I%M%S %P",   M_FALSE },
		{ "%m/%d/%Y T %I%M%S %p",   M_FALSE },
		{ "%m/%d/%Y T %H%M%S %z",   M_TRUE, },
		{ "%m/%d/%Y T %H:%M",       M_FALSE },
		{ "%m/%d/%Y T %H:%M %P",    M_FALSE },
		{ "%m/%d/%Y T %H:%M %p",    M_FALSE },
		{ "%m/%d/%Y T %I:%M %P",    M_FALSE },
		{ "%m/%d/%Y T %I:%M %p",    M_FALSE },
		{ "%m/%d/%Y T %H:%M:%S",    M_FALSE },
		{ "%m/%d/%Y T %H:%M:%S %P", M_FALSE },
		{ "%m/%d/%Y T %H:%M:%S %p", M_FALSE },
		{ "%m/%d/%Y T %I:%M:%S %P", M_FALSE },
		{ "%m/%d/%Y T %I:%M:%S %p", M_FALSE },
		{ "%m/%d/%Y T %H:%M:%S %z", M_TRUE, },
		{ "%m/%d/%Y T %H-%M",       M_FALSE },
		{ "%m/%d/%Y T %H-%M %P",    M_FALSE },
		{ "%m/%d/%Y T %H-%M %p",    M_FALSE },
		{ "%m/%d/%Y T %I-%M %P",    M_FALSE },
		{ "%m/%d/%Y T %I-%M %p",    M_FALSE },
		{ "%m/%d/%Y T %H-%M-%S",    M_FALSE },
		{ "%m/%d/%Y T %H-%M-%S %P", M_FALSE },
		{ "%m/%d/%Y T %H-%M-%S %p", M_FALSE },
		{ "%m/%d/%Y T %I-%M-%S %P", M_FALSE },
		{ "%m/%d/%Y T %I-%M-%S %p", M_FALSE },
		{ "%m/%d/%Y T %H-%M-%S %z", M_TRUE, },

		{ "%m-%d-%Y %H",            M_FALSE },
		{ "%m-%d-%Y %H %P",         M_FALSE },
		{ "%m-%d-%Y %H %p",         M_FALSE },
		{ "%m-%d-%Y %I %P",         M_FALSE },
		{ "%m-%d-%Y %I %p",         M_FALSE },
		{ "%m-%d-%Y %H%M",          M_FALSE },
		{ "%m-%d-%Y %H%M %P",       M_FALSE },
		{ "%m-%d-%Y %H%M %p",       M_FALSE },
		{ "%m-%d-%Y %I%M %P",       M_FALSE },
		{ "%m-%d-%Y %I%M %p",       M_FALSE },
		{ "%m-%d-%Y %H%M%S",        M_FALSE },
		{ "%m-%d-%Y %H%M%S %P",     M_FALSE },
		{ "%m-%d-%Y %H%M%S %p",     M_FALSE },
		{ "%m-%d-%Y %I%M%S %P",     M_FALSE },
		{ "%m-%d-%Y %I%M%S %p",     M_FALSE },
		{ "%m-%d-%Y %H%M%S %z",     M_FALSE },
		{ "%m-%d-%Y %H:%M",         M_FALSE },
		{ "%m-%d-%Y %H:%M %P",      M_FALSE },
		{ "%m-%d-%Y %H:%M %p",      M_FALSE },
		{ "%m-%d-%Y %I:%M %P",      M_FALSE },
		{ "%m-%d-%Y %I:%M %p",      M_FALSE },
		{ "%m-%d-%Y %H:%M:%S",      M_FALSE },
		{ "%m-%d-%Y %H:%M:%S %P",   M_FALSE },
		{ "%m-%d-%Y %H:%M:%S %p",   M_FALSE },
		{ "%m-%d-%Y %I:%M:%S %P",   M_FALSE },
		{ "%m-%d-%Y %I:%M:%S %p",   M_FALSE },
		{ "%m-%d-%Y %H:%M:%S %z",   M_TRUE, },
		{ "%m-%d-%Y %H-%M",         M_FALSE },
		{ "%m-%d-%Y %H-%M %P",      M_FALSE },
		{ "%m-%d-%Y %H-%M %p",      M_FALSE },
		{ "%m-%d-%Y %I-%M %P",      M_FALSE },
		{ "%m-%d-%Y %I-%M %p",      M_FALSE },
		{ "%m-%d-%Y %H-%M-%S",      M_FALSE },
		{ "%m-%d-%Y %H-%M-%S %P",   M_FALSE },
		{ "%m-%d-%Y %H-%M-%S %p",   M_FALSE },
		{ "%m-%d-%Y %I-%M-%S %P",   M_FALSE },
		{ "%m-%d-%Y %I-%M-%S %p",   M_FALSE },
		{ "%m-%d-%Y %H-%M-%S %z",   M_TRUE, },

		{ "%m-%d-%Y T %H",          M_FALSE },
		{ "%m-%d-%Y T %H %P",       M_FALSE },
		{ "%m-%d-%Y T %H %p",       M_FALSE },
		{ "%m-%d-%Y T %I %P",       M_FALSE },
		{ "%m-%d-%Y T %I %p",       M_FALSE },
		{ "%m-%d-%Y T %H%M",        M_FALSE },
		{ "%m-%d-%Y T %H%M %P",     M_FALSE },
		{ "%m-%d-%Y T %H%M %p",     M_FALSE },
		{ "%m-%d-%Y T %I%M %P",     M_FALSE },
		{ "%m-%d-%Y T %I%M %p",     M_FALSE },
		{ "%m-%d-%Y T %H%M%S",      M_FALSE },
		{ "%m-%d-%Y T %H%M%S %P",   M_FALSE },
		{ "%m-%d-%Y T %H%M%S %p",   M_FALSE },
		{ "%m-%d-%Y T %H%M%S %z",   M_FALSE },
		{ "%m-%d-%Y T %I%M%S %P",   M_FALSE },
		{ "%m-%d-%Y T %I%M%S %p",   M_FALSE },
		{ "%m-%d-%Y T %H:%M",       M_FALSE },
		{ "%m-%d-%Y T %H:%M %P",    M_FALSE },
		{ "%m-%d-%Y T %H:%M %p",    M_FALSE },
		{ "%m-%d-%Y T %I:%M %P",    M_FALSE },
		{ "%m-%d-%Y T %I:%M %p",    M_FALSE },
		{ "%m-%d-%Y T %H:%M:%S",    M_FALSE },
		{ "%m-%d-%Y T %H:%M:%S %P", M_FALSE },
		{ "%m-%d-%Y T %H:%M:%S %p", M_FALSE },
		{ "%m-%d-%Y T %I:%M:%S %P", M_FALSE },
		{ "%m-%d-%Y T %I:%M:%S %p", M_FALSE },
		{ "%m-%d-%Y T %H:%M:%S %z", M_TRUE, },
		{ "%m-%d-%Y T %H-%M",       M_FALSE },
		{ "%m-%d-%Y T %H-%M %P",    M_FALSE },
		{ "%m-%d-%Y T %H-%M %p",    M_FALSE },
		{ "%m-%d-%Y T %I-%M %P",    M_FALSE },
		{ "%m-%d-%Y T %I-%M %p",    M_FALSE },
		{ "%m-%d-%Y T %H-%M-%S",    M_FALSE },
		{ "%m-%d-%Y T %H-%M-%S %P", M_FALSE },
		{ "%m-%d-%Y T %H-%M-%S %p", M_FALSE },
		{ "%m-%d-%Y T %I-%M-%S %P", M_FALSE },
		{ "%m-%d-%Y T %I-%M-%S %p", M_FALSE },
		{ "%m-%d-%Y T %H-%M-%S %z", M_TRUE, },

		{ "%m/%d/%y %H",            M_FALSE },
		{ "%m/%d/%y %H %P",         M_FALSE },
		{ "%m/%d/%y %H %p",         M_FALSE },
		{ "%m/%d/%y %I %P",         M_FALSE },
		{ "%m/%d/%y %I %p",         M_FALSE },
		{ "%m/%d/%y %H%M",          M_FALSE },
		{ "%m/%d/%y %H%M %P",       M_FALSE },
		{ "%m/%d/%y %H%M %p",       M_FALSE },
		{ "%m/%d/%y %I%M %P",       M_FALSE },
		{ "%m/%d/%y %I%M %p",       M_FALSE },
		{ "%m/%d/%y %H%M%S",        M_FALSE },
		{ "%m/%d/%y %H%M%S %P",     M_FALSE },
		{ "%m/%d/%y %H%M%S %p",     M_FALSE },
		{ "%m/%d/%y %I%M%S %P",     M_FALSE },
		{ "%m/%d/%y %I%M%S %p",     M_FALSE },
		{ "%m/%d/%y %H%M%S %z",     M_TRUE, },
		{ "%m/%d/%y %H:%M",         M_FALSE },
		{ "%m/%d/%y %H:%M %P",      M_FALSE },
		{ "%m/%d/%y %H:%M %p",      M_FALSE },
		{ "%m/%d/%y %I:%M %P",      M_FALSE },
		{ "%m/%d/%y %I:%M %p",      M_FALSE },
		{ "%m/%d/%y %H:%M:%S",      M_FALSE },
		{ "%m/%d/%y %H:%M:%S %P",   M_FALSE },
		{ "%m/%d/%y %H:%M:%S %p",   M_FALSE },
		{ "%m/%d/%y %I:%M:%S %P",   M_FALSE },
		{ "%m/%d/%y %I:%M:%S %p",   M_FALSE },
		{ "%m/%d/%y %H:%M:%S %z",   M_TRUE, },
		{ "%m/%d/%y %H-%M",         M_FALSE },
		{ "%m/%d/%y %H-%M %P",      M_FALSE },
		{ "%m/%d/%y %H-%M %p",      M_FALSE },
		{ "%m/%d/%y %I-%M %P",      M_FALSE },
		{ "%m/%d/%y %I-%M %p",      M_FALSE },
		{ "%m/%d/%y %H-%M-%S",      M_FALSE },
		{ "%m/%d/%y %H-%M-%S %P",   M_FALSE },
		{ "%m/%d/%y %H-%M-%S %p",   M_FALSE },
		{ "%m/%d/%y %I-%M-%S %P",   M_FALSE },
		{ "%m/%d/%y %I-%M-%S %p",   M_FALSE },
		{ "%m/%d/%y %H-%M-%S %z",   M_TRUE, },

		{ "%m/%d/%y T %H",          M_FALSE },
		{ "%m/%d/%y T %H %P",       M_FALSE },
		{ "%m/%d/%y T %H %p",       M_FALSE },
		{ "%m/%d/%y T %I %P",       M_FALSE },
		{ "%m/%d/%y T %I %p",       M_FALSE },
		{ "%m/%d/%y T %H%M",        M_FALSE },
		{ "%m/%d/%y T %H%M %P",     M_FALSE },
		{ "%m/%d/%y T %H%M %p",     M_FALSE },
		{ "%m/%d/%y T %I%M %P",     M_FALSE },
		{ "%m/%d/%y T %I%M %p",     M_FALSE },
		{ "%m/%d/%y T %H%M%S",      M_FALSE },
		{ "%m/%d/%y T %H%M%S %P",   M_FALSE },
		{ "%m/%d/%y T %H%M%S %p",   M_FALSE },
		{ "%m/%d/%y T %I%M%S %P",   M_FALSE },
		{ "%m/%d/%y T %I%M%S %p",   M_FALSE },
		{ "%m/%d/%y T %H%M%S %z",   M_TRUE, },
		{ "%m/%d/%y T %H:%M",       M_FALSE },
		{ "%m/%d/%y T %H:%M %P",    M_FALSE },
		{ "%m/%d/%y T %H:%M %p",    M_FALSE },
		{ "%m/%d/%y T %I:%M %P",    M_FALSE },
		{ "%m/%d/%y T %I:%M %p",    M_FALSE },
		{ "%m/%d/%y T %H:%M:%S",    M_FALSE },
		{ "%m/%d/%y T %H:%M:%S %P", M_FALSE },
		{ "%m/%d/%y T %H:%M:%S %p", M_FALSE },
		{ "%m/%d/%y T %I:%M:%S %P", M_FALSE },
		{ "%m/%d/%y T %I:%M:%S %p", M_FALSE },
		{ "%m/%d/%y T %H:%M:%S %z", M_TRUE, },
		{ "%m/%d/%y T %H-%M",       M_FALSE },
		{ "%m/%d/%y T %H-%M %P",    M_FALSE },
		{ "%m/%d/%y T %H-%M %p",    M_FALSE },
		{ "%m/%d/%y T %I-%M %P",    M_FALSE },
		{ "%m/%d/%y T %I-%M %p",    M_FALSE },
		{ "%m/%d/%y T %H-%M-%S",    M_FALSE },
		{ "%m/%d/%y T %H-%M-%S %P", M_FALSE },
		{ "%m/%d/%y T %H-%M-%S %p", M_FALSE },
		{ "%m/%d/%y T %I-%M-%S %P", M_FALSE },
		{ "%m/%d/%y T %I-%M-%S %p", M_FALSE },
		{ "%m/%d/%y T %H-%M-%S %z", M_TRUE, },

		{ "%Y/%m/%d %H",            M_FALSE },
		{ "%Y/%m/%d %H %P",         M_FALSE },
		{ "%Y/%m/%d %H %p",         M_FALSE },
		{ "%Y/%m/%d %I %P",         M_FALSE },
		{ "%Y/%m/%d %I %p",         M_FALSE },
		{ "%Y/%m/%d %H%M",          M_FALSE },
		{ "%Y/%m/%d %H%M %P",       M_FALSE },
		{ "%Y/%m/%d %H%M %p",       M_FALSE },
		{ "%Y/%m/%d %I%M %P",       M_FALSE },
		{ "%Y/%m/%d %I%M %p",       M_FALSE },
		{ "%Y/%m/%d %H%M%S",        M_FALSE },
		{ "%Y/%m/%d %H%M%S %P",     M_FALSE },
		{ "%Y/%m/%d %H%M%S %p",     M_FALSE },
		{ "%Y/%m/%d %I%M%S %P",     M_FALSE },
		{ "%Y/%m/%d %I%M%S %p",     M_FALSE },
		{ "%Y/%m/%d %H%M%S %z",     M_TRUE, },
		{ "%Y/%m/%d %H:%M",         M_FALSE },
		{ "%Y/%m/%d %H:%M %P",      M_FALSE },
		{ "%Y/%m/%d %H:%M %p",      M_FALSE },
		{ "%Y/%m/%d %I:%M %P",      M_FALSE },
		{ "%Y/%m/%d %I:%M %p",      M_FALSE },
		{ "%Y/%m/%d %H:%M:%S",      M_FALSE },
		{ "%Y/%m/%d %H:%M:%S %P",   M_FALSE },
		{ "%Y/%m/%d %H:%M:%S %p",   M_FALSE },
		{ "%Y/%m/%d %I:%M:%S %P",   M_FALSE },
		{ "%Y/%m/%d %I:%M:%S %p",   M_FALSE },
		{ "%Y/%m/%d %H:%M:%S %z",   M_TRUE, },
		{ "%Y/%m/%d %H-%M",         M_FALSE },
		{ "%Y/%m/%d %H-%M %P",      M_FALSE },
		{ "%Y/%m/%d %H-%M %p",      M_FALSE },
		{ "%Y/%m/%d %I-%M %P",      M_FALSE },
		{ "%Y/%m/%d %I-%M %p",      M_FALSE },
		{ "%Y/%m/%d %H-%M-%S",      M_FALSE },
		{ "%Y/%m/%d %H-%M-%S %P",   M_FALSE },
		{ "%Y/%m/%d %H-%M-%S %p",   M_FALSE },
		{ "%Y/%m/%d %I-%M-%S %P",   M_FALSE },
		{ "%Y/%m/%d %I-%M-%S %p",   M_FALSE },
		{ "%Y/%m/%d %H-%M-%S %z",   M_TRUE, },

		{ "%Y/%m/%d T %H",          M_FALSE },
		{ "%Y/%m/%d T %H %P",       M_FALSE },
		{ "%Y/%m/%d T %H %p",       M_FALSE },
		{ "%Y/%m/%d T %I %P",       M_FALSE },
		{ "%Y/%m/%d T %I %p",       M_FALSE },
		{ "%Y/%m/%d T %H%M",        M_FALSE },
		{ "%Y/%m/%d T %H%M %P",     M_FALSE },
		{ "%Y/%m/%d T %H%M %p",     M_FALSE },
		{ "%Y/%m/%d T %I%M %P",     M_FALSE },
		{ "%Y/%m/%d T %I%M %p",     M_FALSE },
		{ "%Y/%m/%d T %H%M%S",      M_FALSE },
		{ "%Y/%m/%d T %H%M%S %P",   M_FALSE },
		{ "%Y/%m/%d T %H%M%S %p",   M_FALSE },
		{ "%Y/%m/%d T %I%M%S %P",   M_FALSE },
		{ "%Y/%m/%d T %I%M%S %p",   M_FALSE },
		{ "%Y/%m/%d T %H%M%S %z",   M_TRUE, },
		{ "%Y/%m/%d T %H:%M",       M_FALSE },
		{ "%Y/%m/%d T %H:%M %P",    M_FALSE },
		{ "%Y/%m/%d T %H:%M %p",    M_FALSE },
		{ "%Y/%m/%d T %I:%M %P",    M_FALSE },
		{ "%Y/%m/%d T %I:%M %p",    M_FALSE },
		{ "%Y/%m/%d T %H:%M:%S",    M_FALSE },
		{ "%Y/%m/%d T %H:%M:%S %P", M_FALSE },
		{ "%Y/%m/%d T %H:%M:%S %p", M_FALSE },
		{ "%Y/%m/%d T %I:%M:%S %P", M_FALSE },
		{ "%Y/%m/%d T %I:%M:%S %p", M_FALSE },
		{ "%Y/%m/%d T %H:%M:%S %z", M_TRUE, },
		{ "%Y/%m/%d T %H-%M",       M_FALSE },
		{ "%Y/%m/%d T %H-%M %P",    M_FALSE },
		{ "%Y/%m/%d T %H-%M %p",    M_FALSE },
		{ "%Y/%m/%d T %I-%M %P",    M_FALSE },
		{ "%Y/%m/%d T %I-%M %p",    M_FALSE },
		{ "%Y/%m/%d T %H-%M-%S",    M_FALSE },
		{ "%Y/%m/%d T %H-%M-%S %P", M_FALSE },
		{ "%Y/%m/%d T %H-%M-%S %p", M_FALSE },
		{ "%Y/%m/%d T %I-%M-%S %P", M_FALSE },
		{ "%Y/%m/%d T %I-%M-%S %p", M_FALSE },
		{ "%Y/%m/%d T %H-%M-%S %z", M_TRUE, },

		{ "%Y-%m-%d %H",            M_FALSE },
		{ "%Y-%m-%d %H %P",         M_FALSE },
		{ "%Y-%m-%d %H %p",         M_FALSE },
		{ "%Y-%m-%d %I %P",         M_FALSE },
		{ "%Y-%m-%d %I %p",         M_FALSE },
		{ "%Y-%m-%d %H%M",          M_FALSE },
		{ "%Y-%m-%d %H%M %P",       M_FALSE },
		{ "%Y-%m-%d %H%M %p",       M_FALSE },
		{ "%Y-%m-%d %I%M %P",       M_FALSE },
		{ "%Y-%m-%d %I%M %p",       M_FALSE },
		{ "%Y-%m-%d %H%M%S",        M_FALSE },
		{ "%Y-%m-%d %H%M%S %P",     M_FALSE },
		{ "%Y-%m-%d %H%M%S %p",     M_FALSE },
		{ "%Y-%m-%d %I%M%S %P",     M_FALSE },
		{ "%Y-%m-%d %I%M%S %p",     M_FALSE },
		{ "%Y-%m-%d %H%M%S %z",     M_TRUE, },
		{ "%Y-%m-%d %H:%M",         M_FALSE },
		{ "%Y-%m-%d %H:%M %P",      M_FALSE },
		{ "%Y-%m-%d %H:%M %p",      M_FALSE },
		{ "%Y-%m-%d %I:%M %P",      M_FALSE },
		{ "%Y-%m-%d %I:%M %p",      M_FALSE },
		{ "%Y-%m-%d %H:%M:%S",      M_FALSE },
		{ "%Y-%m-%d %H:%M:%S %P",   M_FALSE },
		{ "%Y-%m-%d %H:%M:%S %p",   M_FALSE },
		{ "%Y-%m-%d %I:%M:%S %P",   M_FALSE },
		{ "%Y-%m-%d %I:%M:%S %p",   M_FALSE },
		{ "%Y-%m-%d %H:%M:%S %z",   M_TRUE, },
		{ "%Y-%m-%d %H-%M",         M_FALSE },
		{ "%Y-%m-%d %H-%M %P",      M_FALSE },
		{ "%Y-%m-%d %H-%M %p",      M_FALSE },
		{ "%Y-%m-%d %I-%M %P",      M_FALSE },
		{ "%Y-%m-%d %I-%M %p",      M_FALSE },
		{ "%Y-%m-%d %H-%M-%S",      M_FALSE },
		{ "%Y-%m-%d %H-%M-%S %P",   M_FALSE },
		{ "%Y-%m-%d %H-%M-%S %p",   M_FALSE },
		{ "%Y-%m-%d %I-%M-%S %P",   M_FALSE },
		{ "%Y-%m-%d %I-%M-%S %p",   M_FALSE },
		{ "%Y-%m-%d %H-%M-%S %z",   M_TRUE, },

		{ "%Y-%m-%d T %H",          M_FALSE },
		{ "%Y-%m-%d T %H %P",       M_FALSE },
		{ "%Y-%m-%d T %H %p",       M_FALSE },
		{ "%Y-%m-%d T %I %P",       M_FALSE },
		{ "%Y-%m-%d T %I %p",       M_FALSE },
		{ "%Y-%m-%d T %H%M",        M_FALSE },
		{ "%Y-%m-%d T %H%M %P",     M_FALSE },
		{ "%Y-%m-%d T %H%M %p",     M_FALSE },
		{ "%Y-%m-%d T %I%M %P",     M_FALSE },
		{ "%Y-%m-%d T %I%M %p",     M_FALSE },
		{ "%Y-%m-%d T %H%M%S",      M_FALSE },
		{ "%Y-%m-%d T %H%M%S %P",   M_FALSE },
		{ "%Y-%m-%d T %H%M%S %p",   M_FALSE },
		{ "%Y-%m-%d T %I%M%S %P",   M_FALSE },
		{ "%Y-%m-%d T %I%M%S %p",   M_FALSE },
		{ "%Y-%m-%d T %H%M%S %z",   M_TRUE, },
		{ "%Y-%m-%d T %H:%M",       M_FALSE },
		{ "%Y-%m-%d T %H:%M %P",    M_FALSE },
		{ "%Y-%m-%d T %H:%M %p",    M_FALSE },
		{ "%Y-%m-%d T %I:%M %P",    M_FALSE },
		{ "%Y-%m-%d T %I:%M %p",    M_FALSE },
		{ "%Y-%m-%d T %H:%M:%S",    M_FALSE },
		{ "%Y-%m-%d T %H:%M:%S %P", M_FALSE },
		{ "%Y-%m-%d T %H:%M:%S %p", M_FALSE },
		{ "%Y-%m-%d T %I:%M:%S %P", M_FALSE },
		{ "%Y-%m-%d T %I:%M:%S %p", M_FALSE },
		{ "%Y-%m-%d T %H:%M:%S %z", M_TRUE, },
		{ "%Y-%m-%d T %H-%M",       M_FALSE },
		{ "%Y-%m-%d T %H-%M %P",    M_FALSE },
		{ "%Y-%m-%d T %H-%M %p",    M_FALSE },
		{ "%Y-%m-%d T %I-%M %P",    M_FALSE },
		{ "%Y-%m-%d T %I-%M %p",    M_FALSE },
		{ "%Y-%m-%d T %H-%M-%S",    M_FALSE },
		{ "%Y-%m-%d T %H-%M-%S %P", M_FALSE },
		{ "%Y-%m-%d T %H-%M-%S %p", M_FALSE },
		{ "%Y-%m-%d T %I-%M-%S %P", M_FALSE },
		{ "%Y-%m-%d T %I-%M-%S %p", M_FALSE },
		{ "%Y-%m-%d T %H-%M-%S %z", M_TRUE, },

		{ "%m/%d/%Y",               M_FALSE },
		{ "%m-%d-%Y",               M_FALSE },
		{ "%m-%d-%y",               M_FALSE },
		{ "%m/%d/%y",               M_FALSE },
		{ "%m%d%Y",                 M_FALSE },
		{ "%m%d%y",                 M_FALSE },
		{ "%Y/%m/%d",               M_FALSE },
		{ "%Y-%m-%d",               M_FALSE },

		{ NULL,                     M_FALSE }
	};

	M_time_t          timet_result = 0;
	M_time_t          gmtoff;
	int               ret          = 0;
	int               i            = 0;
	char             *retCheck;
	M_time_localtm_t  result;

	if (timestr == NULL)
		return -1;

	/* See if it's a time offset */
	ret = M_time_parseoffset(timestr, tz, &timet_result);
	if (ret == -1) {
		return -1;
	} else if (ret == 1) {
		return timet_result;
	}

	for (i=0; formats[i].fmt != NULL; i++) {
		M_mem_set(&result, 0, sizeof(result));
		if (default_end_of_day) {
			result.hour=23;
			result.min=59;
			result.sec=59;
		}
		result.isdst = -1;
		retCheck = M_time_parsefmt(timestr, formats[i].fmt, &result);
		if (retCheck != NULL && retCheck[0] == '\0') {
			if (formats[i].has_gmtoff) {
				gmtoff = result.gmtoff;
				timet_result  = M_time_fromgm(&result);
				timet_result -= gmtoff;
				return timet_result;
			} else {
				return M_time_fromlocal(&result, tz);
			}
		}
	}

	return -1;
}

char *M_time_to_str(const char *fmt, const M_time_localtm_t *tm)
{
	M_buf_t          *buf;
	M_time_localtm_t  mytm;
	M_time_t          gmtoff;

	if (fmt == NULL || tm == NULL)
		return NULL;

	M_mem_copy(&mytm, tm, sizeof(mytm));
	/* Normalize. The isdst, gmtoff, and abbr could be incorrect if the corrected time crosses a DST boundary.
 	 * We normalize to ensure a valid date/time is printed but this function really shouldn't be called
	 * with a non-normalized tm in the first place. */
	M_time_normalize_tm(&mytm);
	buf = M_buf_create();

	while (*fmt != '\0') {
		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
				/* The % character. */
				case '%':
					M_buf_add_byte(buf, '%');
					break;
				/* month in 2 digit format. */
				case 'm':
					M_buf_add_int_just(buf, mytm.month, 2);
					break;
				/* day in 2 digit format. */
				case 'd':
					M_buf_add_int_just(buf, mytm.day, 2);
					break;
				/* year in 2 digit format. */
				case 'y':
					M_buf_add_int_just(buf, mytm.year2, 2);
					break;
				/* year in 4 digit format. */
				case 'Y':
					M_buf_add_int_just(buf, mytm.year, 4);
					break;
				/* hour in 2 digit format. */
				case 'H':
					M_buf_add_int_just(buf, mytm.hour, 2);
					break;
				case 'I':
					M_buf_add_int_just(buf, mytm.hour>12?mytm.hour-12:mytm.hour, 2);
					break;
				/* minutes in 2 digit format. */
				case 'M':
					M_buf_add_int_just(buf, mytm.min, 2);
					break;
				/* seconds in 2 digit format. */
				case 'S':
					M_buf_add_int_just(buf, mytm.sec, 2);
					break;
				case 'P':
					M_buf_add_str(buf, mytm.hour>=12?"PM":"AM");
					break;
				case 'p':
					M_buf_add_str(buf, mytm.hour>=12?"pm":"am");
					break;
				/* offset from gmt. ISO 8601 [+-]hhmm numeric offset. */
				case 'z':
					gmtoff = mytm.gmtoff;
					if (gmtoff < 0) {
						M_buf_add_byte(buf, '-');
						gmtoff *= -1;
					} else {
						M_buf_add_byte(buf, '+');
					}
					M_buf_add_int_just(buf, gmtoff/60/60, 2);
					M_buf_add_int_just(buf, (gmtoff/60)%60, 2);
					break;
				case 'Z':
					M_buf_add_str(buf, mytm.abbr);
					break;
				default:
					M_buf_cancel(buf);
					return NULL;
			}
		} else {
			M_buf_add_byte(buf, (unsigned char)*fmt);
		}

		/* Next delim */
		fmt++;
	}

	return M_buf_finish_str(buf, NULL);
}

char *M_time_parsefmt(const char *s, const char *fmt, M_time_localtm_t *tm)
{
	M_int64 x;                 /* internal tmp var */
	int     len;               /* internal len var */
	M_time_gmtm_t temp_tm; /* Temporary tm for getting current year when dealing with 2 digit year. */

	if (tm == NULL)
		return NULL;

	/* Loop until we process the entire format */
	while (*fmt != 0) {
		/* End of input string reached */
		if (*s == 0)
			break;

		/* eat whitespace and not formatting characters */
		if (*fmt != '%') {
			/* white space eater */
			if (M_chr_isspace(*fmt)) {
				while (*s != 0 && M_chr_isspace(*s)) {
					s++;
				}
				fmt++;
			} else if (*fmt != *s) {
				/* delimiter characters check */
				return NULL;
			} else {
				/* same delim next position */
				fmt++;
				s++;
			}
			continue;
		}

		/* jump past the % */
		fmt++;

		switch (*fmt) {
			/* end of fmt */
			case 0:
			/* chew %% chars */
			case '%':
				if (*s++ != '%')
					return NULL;
				break;
			/* month in 2 digit format */
			case 'm':
				x = M_time_getnum(&s, 2);
				if (x > 12 ||  x < 1)
					return NULL;

				tm->month = x;
				break;
			/* day in 2 digit format */
			case 'd':
				x = M_time_getnum(&s, 2);
				if (x > 31 || x < 1)
					return NULL;

				tm->day = x;
				break;
			/* year in 2 digit format */
			case 'y':
			/* year in 4 digit format */
			case 'Y':
				len = (*fmt == 'Y') ? 4 : 2;
				for (x = 0; x < len; x++)
				{
					if (*(s+x) == 0 || !M_chr_isdigit(*(s+x)))
						return NULL;
				}
				x = M_time_getnum(&s, len);
				if (x < 0)
					return NULL;
				if (*fmt == 'y') {
					/* Use a sliding scale of 80 years behind and 20 years ahead. */
					M_time_togm(M_time(), &temp_tm);
					x += (temp_tm.year/100)*100;
					if (x > temp_tm.year+20) {
						x -= 100;
					}
				}
				tm->year  = x;
				tm->year2 = x % 100;
				break;
			case 'P':
			case 'p':
				tm->hour = M_time_get_ampm_hour(&s, tm->hour);
				if (tm->hour == -1)
					return NULL;
				break;
			/* hour in 2 digit format */
			case 'H':
				x = M_time_getnum(&s, 2); 
				if (x > 23 || x < 0)
					return NULL;
				tm->hour = x;
				break;
			case 'I':
				x = M_time_getnum(&s, 2); 
				if (x > 12 || x < 1)
					return NULL;
				tm->hour = x;
				break;
			/* minutes in 2 digit format */
			case 'M':
				x = M_time_getnum(&s, 2);
				if (x > 59 || x < 0)
					return NULL;

				tm->min = x;
				break;
			/* seconds in 2 digit format */
			case 'S':
				x = M_time_getnum(&s, 2);
				if (x > 60 || x < 0)
					return NULL;

				tm->sec = x;
				break;
			/* RFC 0822 zone: +/-HHMM or +/- HH:MM*/
			case 'z':
				if (!M_time_getoffset(&s, &(tm->gmtoff)))
					return NULL;
				break;
			/* Not handled delim */
			default: 
				return NULL;
		}

		/* Next delim */
		fmt++;
	}

	/* Hackish, but as per spec */
	return (char *)((M_uintptr)s);
}
