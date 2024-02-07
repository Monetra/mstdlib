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

/*! Read the zone abbreviation from the start of a Posix TZ string.
 * \param parser The parser to read from.
 * \param abbr The abbreviation that started the string.
 * \return True on success. Otherwise false.
 */
static M_bool M_time_tz_posix_parse_abbr(M_parser_t *parser, char **abbr)
{
    if (parser == NULL || M_parser_len(parser) == 0 || abbr == NULL)
        return M_FALSE;
    *abbr = M_parser_read_strdup_chr_predicate(parser, M_chr_isalpha);
    return M_TRUE;
}

/*! Parse the date from the start of a Posix TZ string.
 * The date must be in the Mm.w.d format.
 * The parser deviates from the standard by allows w to be negative to indicate the occurrence is backwards from the
 * end of the month.
 * \param parser The parser to read from.
 * \param month The month.
 * \param occur The number of times the day of week occurs.
 * \param wday The day of week.
 * \return True on success. Otherwise false.
 */
static M_bool M_time_tz_posix_parse_date(M_parser_t *parser, int *month, int *occur, int *wday)
{
    M_parser_t     *tparser;
    M_parser_t    **parts;
    size_t          num_parts;
    size_t          len;
    unsigned char   c;
    M_int64         m_val;
    M_int64         o_val;
    M_int64         w_val;

    if (parser == NULL || M_parser_len(parser) == 0 || month == NULL || wday == NULL || occur == NULL)
        return M_FALSE;

    *month = 0;
    *occur = 0;
    *wday  = 0;

    /* Check that we start with an M becuase we only support that part of the date format. */
    if (!M_parser_peek_byte(parser, &c) || c != 'M')
        return M_FALSE;
    M_parser_consume(parser, 1);

    /* Get the date portion */
    M_parser_mark(parser);
    while (M_parser_peek_byte(parser, &c) && (M_chr_isdigit((char)c) || c == '.' || c == '-')) {
        M_parser_consume(parser, 1);
    }
    tparser = M_parser_read_parser_mark(parser);

    len = M_parser_len(tparser);
    if (len < 5 || len > 8) {
        M_parser_destroy(tparser);
        return M_FALSE;
    }

    /* Separate the parts. */
    parts = M_parser_split(tparser, '.', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
    if (parts == NULL || num_parts != 3) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(tparser);
        return M_FALSE;
    }

    M_parser_read_int(parts[0], M_PARSER_INTEGER_ASCII, 0, 10, &m_val);
    M_parser_read_int(parts[1], M_PARSER_INTEGER_ASCII, 0, 10, &o_val);
    M_parser_read_int(parts[2], M_PARSER_INTEGER_ASCII, 0, 10, &w_val);

    M_parser_split_free(parts, num_parts);
    M_parser_destroy(tparser);

    if (m_val <= 0 || m_val > 12 ||
        o_val < -5 || o_val > 5  ||
        w_val < 0  || w_val > 6)
    {
        return M_FALSE;
    }

    /* Set each part. */
    *month = (int)m_val;
    *occur = (int)o_val;
    *wday  = (int)w_val;

    return M_TRUE;
}

/*! Parse the time from the start of Posix TZ string.
 * \param parser The parser to read.
 * \param hour The hour.
 * \param min The minute.
 * \param sec The seconds.
 * \param isoffset True when the time is an offset time. An offset time can be positive or negative. When true the
 * time is considered positive only when the our is prefixed with '+'. When negative hour, min and sec will all be
 * negative.
 * \return True on success. Otherwise false.
 */
static M_bool M_time_tz_posix_parse_time(M_parser_t *parser, int *hour, int *min, int *sec, M_bool isoffset)
{
    M_parser_t     *tparser;
    M_parser_t    **parts;
    size_t          num_parts;
    unsigned char   c;
    M_int64         val;

    if (hour == NULL || min == NULL || sec == NULL)
        return M_FALSE;

    /* default */
    *hour = 2;      /* 2AM */
    if (isoffset) {
        *hour = -1; /* One hour behind. */
    }
    *min  = 0;
    *sec  = 0;

    if (M_parser_len(parser) == 0)
        return M_TRUE;

    /* Move forward to get the ending pos. */
    M_parser_mark(parser);
    M_parser_consume_charset(parser, (const unsigned char *)"0123456789:-+", 13);
    tparser = M_parser_read_parser_mark(parser);

    /* Not set return default. */
    if (tparser == NULL || M_parser_len(tparser) == 0) {
        M_parser_destroy(tparser);
        return M_TRUE;
    }

    /* Too many digits. */
    if (M_parser_len(tparser) > 11) {
        M_parser_destroy(tparser);
        return M_FALSE;
    }

    /* Store and skip past any direction modifier. */
    if (M_parser_peek_byte(tparser, &c) && (c == '+' || c == '-')) {
        M_parser_consume(tparser, 1);
    }

    /* Separate the parts. */
    parts = M_parser_split(tparser, ':', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
    if (parts == NULL || num_parts > 3) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(tparser);
        return M_FALSE;
    }

    /* Set each part. */
    if (num_parts >= 1) {
        M_parser_read_int(parts[0], M_PARSER_INTEGER_ASCII, 0, 10, &val);
        *hour = (int)val;
    }
    if (num_parts >= 2) {
        M_parser_read_int(parts[1], M_PARSER_INTEGER_ASCII, 0, 10, &val);
        *min = (int)val;
    }
    if (num_parts >= 3) {
        M_parser_read_int(parts[2], M_PARSER_INTEGER_ASCII, 0, 10, &val);
        *sec = (int)val;
    }

    M_parser_split_free(parts, num_parts);
    M_parser_destroy(tparser);

    /* offsets without a + modifier are assumed to be negative. */
    if (isoffset && c != '+') {
        *hour *= -1;
        *min  *= -1;
        *sec  *= -1;
    }

    return M_TRUE;
}

/*! Parse the date and time from the start of Posix TZ string.
 * The time is never an offset.
 * \param parser The parser to read from.
 * \param month The month.
 * \param occur The number of times the day of week occurs.
 * \param wday The day of week.
 * \param hour The hour.
 * \param min The minute.
 * \param sec The seconds.
 * \return True on success. Otherwise false.
 */
static M_time_result_t M_time_tz_posix_parse_date_time(M_parser_t *parser, int *month, int *occur, int *wday, int *hour, int *min, int *sec)
{
    M_parser_t **parts;
    M_parser_t  *tparser  = NULL;
    size_t       num_parts;

    /* Spilt date[/time] */
    parts = M_parser_split(parser, '/', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
    if (parts == NULL || num_parts > 2) {
        M_parser_split_free(parts, num_parts);
        return M_TIME_RESULT_DATETIME;
    }

    if (num_parts >= 1) {
        if (!M_time_tz_posix_parse_date(parts[0], month, occur, wday)) {
            M_parser_split_free(parts, num_parts);
            return M_TIME_RESULT_DATE;
        }
    }

    if (num_parts >= 2) {
        tparser = parts[1];
    }

    /* If time is not set the default time will be used. */
    if (!M_time_tz_posix_parse_time(tparser, hour, min, sec, M_FALSE)) {
        M_parser_split_free(parts, num_parts);
        return M_TIME_RESULT_TIME;
    }

    M_parser_split_free(parts, num_parts);
    return M_TIME_RESULT_SUCCESS;
}

/*! Parse a Posix TZ formatted string.
 * posix string have two forms (without spaces):
 * 1. std offset
 * 2. std offset dst [offset],start[/time],end[/time]
 * \param str The string to parse.
 * \param tz The tz object created from the rule.
 * \param name The name identifier of the rule.
 * \return Success on success. Otherwise error condition.
 */
static M_time_result_t M_time_tz_posix_parse_str(const char *str, size_t len, M_time_tz_rule_t **tz, char **name)
{
    M_time_tz_dst_rule_t *adjust;
    M_parser_t           *parser;
    M_parser_t          **parts;
    size_t                num_parts;
    size_t                plen;
    M_time_t              dst_offset;
    M_time_result_t       res;

    *tz    = M_time_tz_rule_create();
    parser = M_parser_create_const((const unsigned char *)str, len, M_PARSER_FLAG_NONE);
    parts  = M_parser_split(parser, ',', 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);

    if (num_parts != 1 && num_parts != 3) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return M_TIME_RESULT_ERROR;
    }

    /* Set the name for the rule */
    M_parser_mark(parts[0]);
    *name = M_parser_read_strdup(parts[0], M_parser_len(parts[0]));
    M_parser_mark_rewind(parts[0]);

    /* std - Required */
    if (!M_time_tz_posix_parse_abbr(parts[0], &((*tz)->abbr))) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return M_TIME_RESULT_ABBR;
    }

    /* Offset - Required */
    if (!M_time_tz_posix_parse_time_offset(parts[0], &((*tz)->offset))) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return M_TIME_RESULT_OFFSET;
    }

    /* Only form 1? */
    if (num_parts == 1) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        return M_TIME_RESULT_SUCCESS;
    }

    /* dst - Requried */
    if (M_parser_len(parts[0]) == 0 || !M_time_tz_posix_parse_abbr(parts[0], &((*tz)->abbr_dst))) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return M_TIME_RESULT_DSTABBR;
    }

    /* dst offset - Optional */
    plen = M_parser_len(parts[0]);
    if (plen != 0 && !M_time_tz_posix_parse_time_offset(parts[0], &dst_offset)) {
        M_parser_split_free(parts, num_parts);
        M_parser_destroy(parser);
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return M_TIME_RESULT_DSTOFFSET;
    }
    /* No offset, default to 1 hour head of std offset. */
    if (plen == 0)
        dst_offset = (*tz)->offset + (60 * 60);

    /* start[/time],end[/time] - Requried */
    res = M_time_tz_posix_parse_dst_adjust_rule(parts[1], parts[2], &adjust, 0, (*tz)->offset, dst_offset);
    M_parser_split_free(parts, num_parts);
    M_parser_destroy(parser);
    if (res != M_TIME_RESULT_SUCCESS) {
        M_time_tz_rule_destroy(*tz);
        *tz = NULL;
        return res;
    }
    M_time_tz_rule_add_dst_adjust(*tz, adjust);

    return M_TIME_RESULT_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse the time from the start of Posix TZ string and treat it as an offset time.
 * \param parser The parser to read from.
 * \param offset The offset from the parsed string.
 * \return True on success. Otherwise false.
 */
M_bool M_time_tz_posix_parse_time_offset(M_parser_t *parser, M_time_t *offset)
{
    M_bool ret;
    int    hour;
    int    min;
    int    sec;

    if (offset == NULL)
        return M_FALSE;
    *offset = 0;

    ret = M_time_tz_posix_parse_time(parser, &hour, &min, &sec, M_TRUE);
    if (ret)
        *offset = (((M_time_t)hour) * 60 * 60) + (((M_time_t)min) * 60) + ((M_time_t)sec);

    return ret;
}

/*! Parse a Posix TZ dst rule.
 * The rule is the "start[/time],end[/time]" DST portion of the TZ rule.
 * \param parser_start The parser to read from.
 * \param parser_end The parser to read from.
 * \param adjust The adjustment object for the DST rule.
 * \param The year the rule applies to.
 * \param offset The offset that applies when DST is in effect.
 * \param return Success on success. Otherwise error condition.
 */
M_time_result_t M_time_tz_posix_parse_dst_adjust_rule(M_parser_t *parser_start, M_parser_t *parser_end, M_time_tz_dst_rule_t **adjust, M_int64 year, M_time_t offset, M_time_t offset_dst)
{
    M_time_result_t res;

    if (adjust == NULL)
        return M_TIME_RESULT_INVALID;
    *adjust = NULL;

    if (parser_start == NULL || M_parser_len(parser_start) == 0 || parser_end == NULL || M_parser_len(parser_end) == 0)
        return M_TIME_RESULT_SUCCESS;

    *adjust               = M_malloc_zero(sizeof(**adjust));
    (*adjust)->year       = year;
    (*adjust)->offset     = offset;
    (*adjust)->offset_dst = offset_dst;

    /* start, end */
    if ((res = M_time_tz_posix_parse_date_time(parser_start, &((*adjust)->start.month), &((*adjust)->start.occur),
            &((*adjust)->start.wday), &((*adjust)->start.hour), &((*adjust)->start.min), &((*adjust)->start.sec))
            != M_TIME_RESULT_SUCCESS) ||
        (res = M_time_tz_posix_parse_date_time(parser_end, &((*adjust)->end.month), &((*adjust)->end.occur),
            &((*adjust)->end.wday), &((*adjust)->end.hour), &((*adjust)->end.min), &((*adjust)->end.sec))
            != M_TIME_RESULT_SUCCESS))
    {
        M_free(*adjust);
        *adjust = NULL;
        return res;
    }

    return M_TIME_RESULT_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_time_result_t M_time_tzs_add_posix_str(M_time_tzs_t *tzs, const char *str)
{
    M_time_tz_rule_t *rtz;
    char             *name = NULL;
    M_list_str_t     *alias;
    M_time_result_t   res;

    if (tzs == NULL || str == NULL || *str == '\0') {
        return M_TIME_RESULT_INVALID;
    }

    res = M_time_tz_posix_parse_str(str, M_str_len(str), &rtz, &name);
    if (res != M_TIME_RESULT_SUCCESS) {
        M_free(name);
        return res;
    }

    alias = M_list_str_create(M_LIST_STR_NONE);
    M_list_str_insert(alias, name);
    res = M_time_tz_rule_load(tzs, rtz, name, alias);
    M_free(name);
    M_list_str_destroy(alias);

    return res;
}
