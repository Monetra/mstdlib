/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

#include "m_re_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static size_t NUM_PMATCH = 99;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static regex_flags_t build_flags(M_uint32 flags)
{
    regex_flags_t rflags = 0;

    if (flags & M_RE_CASECMP)
        rflags |= REG_ICASE;

    if (flags & M_RE_MULTILINE)
        rflags |= REG_MULTILINE;

    if (flags & M_RE_DOTALL)
        rflags |= REG_DOTALL;

    if (flags & M_RE_UNGREEDY)
        rflags |= REG_UNGREEDY;

    return rflags;
}

M_re_t *M_re_compile(const char *pattern, M_uint32 flags)
{
    regex_t       *re;
    reg_errcode_t  ret;
    regex_flags_t  rflags;

    rflags = build_flags(flags);
    re     = M_malloc_zero(sizeof(*re));
    ret    = mregcomp(re, pattern, rflags);
    if (ret != REG_OK) {
        M_free(re);
        return NULL;
    }

    return (M_re_t *)re;
}

void M_re_destroy(M_re_t *re)
{
    if (re == NULL)
        return;

    mregfree((regex_t *)re);
    M_free(re);
}

M_bool M_re_search(const M_re_t *re, const char *str, M_re_match_t **match)
{
    regmatch_t    *pmatch     = NULL;
    reg_errcode_t  ret;
    size_t         num_pmatch = 0;
    size_t         i;

    if (re == NULL)
        return M_FALSE;

    if (str == NULL)
        str = "";

    if (match != NULL) {
        num_pmatch = NUM_PMATCH;
        pmatch     = M_malloc_zero(num_pmatch * sizeof(*pmatch));
    }

    ret = mregexec((const regex_t *)re, str, num_pmatch, pmatch);
    if (ret != REG_OK) {
        M_free(pmatch);
        return M_FALSE;
    }

    if (match != NULL) {
        *match = M_re_match_create();
        for (i=0; i<num_pmatch; i++) {
            if ((pmatch[i].rm_so == 0 && pmatch[i].rm_eo == 0) || pmatch[i].rm_so == -1 || pmatch[i].rm_eo == -1) {
                continue;
            }
            M_re_match_insert(*match, i, (size_t)pmatch[i].rm_so, (size_t)(pmatch[i].rm_eo - pmatch[i].rm_so));
        }
    }

    M_free(pmatch);
    return M_TRUE;
}

M_bool M_re_eq_start(const M_re_t *re, const char *str)
{
    M_re_match_t *match;
    size_t        offset = 0;
    M_bool        ret;

    if (re == NULL)
        return M_FALSE;

    if (str == NULL)
        str = "";

    ret = M_re_search(re, str, &match);
    if (!ret)
        return M_FALSE;

    M_re_match_idx(match, 0, &offset, NULL);
    if (offset == 0) {
        ret = M_TRUE;
    } else {
        ret = M_FALSE;
    }

    M_re_match_destroy(match);
    return ret;
}

M_bool M_re_eq(const M_re_t *re, const char *str)
{
    M_re_match_t *match;
    size_t        offset = 0;
    size_t        mlen   = 0;
    size_t        slen;
    M_bool        ret;

    if (re == NULL)
        return M_FALSE;

    if (str == NULL)
        str = "";

    ret = M_re_search(re, str, &match);
    if (!ret)
        return M_FALSE;

    slen = M_str_len(str);
    M_re_match_idx(match, 0, &offset, &mlen);

    if (offset == 0 && mlen == slen) {
        ret = M_TRUE;
    } else {
        ret = M_FALSE;
    }

    M_re_match_destroy(match);
    return ret;
}

M_list_t *M_re_matches(const M_re_t *re, const char *str)
{
    M_re_match_t            *match;
    M_list_t                *matches = NULL;
    size_t                   pos;
    struct M_list_callbacks  lcbs = {
        NULL,
        NULL,
        NULL,
        (M_list_free_func)M_re_match_destroy
    };
    size_t offset;
    size_t len;

    if (re == NULL || M_str_isempty(str))
        return NULL;

    matches = M_list_create(&lcbs, M_LIST_NONE);

    pos = 0;
    while (M_re_search(re, str+pos, &match)) {
        if (!M_re_match_idx(match, 0, &offset, &len)) {
            M_list_destroy(matches, M_TRUE);
            return NULL;
        }

        M_re_match_adjust_offset(match, pos);
        M_list_insert(matches, match);

        pos += offset + len;
    }

    return matches;
}

M_list_str_t *M_re_find_all(const M_re_t *re, const char *str)
{
    M_list_t     *matches;
    M_list_str_t *all;
    M_buf_t      *buf;
    size_t        len;
    size_t        i;

    if (re == NULL || M_str_isempty(str))
        return NULL;

    matches = M_re_matches(re, str);
    if (matches == NULL)
        return NULL;

    all = M_list_str_create(M_LIST_STR_NONE);
    buf = M_buf_create();
    len = M_list_len(matches);
    for (i=0; i<len; i++) {
        const M_re_match_t *match;
        size_t offset;
        size_t mlen;

        M_buf_truncate(buf, 0);

        match = M_list_at(matches, i);
        if (!M_re_match_idx(match, 0, &offset, &mlen)) {
            continue;
        }

        M_buf_add_bytes(buf, str+offset, mlen);
        M_list_str_insert(all, M_buf_peek(buf));
    }

    M_buf_cancel(buf);
    M_list_destroy(matches, M_TRUE);
    return all;
}

static char *M_re_sub_build_repl(const char *repl, const char *str, const M_re_match_t *match)
{
    M_buf_t       *buf;
    M_parser_t    *parser;
    unsigned char  byte;
    size_t         offset;
    size_t         len;

    parser = M_parser_create_const((const unsigned char *)repl, M_str_len(repl), M_PARSER_FLAG_NONE);
    buf    = M_buf_create();
    while (M_parser_read_buf_until(parser, buf, (const unsigned char *)"\\", 1, M_TRUE) > 0) {
        M_uint64 refnum = 0;

        /* If there are no bytes left we're done. */
        if (M_parser_len(parser) == 0) {
            break;
        }

        /* Check if this is possibly a backref.
         * If not then add the \ back and keep going. */
        if (!M_parser_peek_byte(parser, &byte) || (!M_chr_isdigit((char)byte) && byte != 'g')) {
            continue;
        }

        /* Handle \# backref. */
        if (M_chr_isdigit((char)byte)) {
            /* The reference number can b 0-99.
             * First we'll try to read 2 digits and if that fails
             * we'll try again reading 1. */
            if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 2, 10, &refnum)) {
                if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 1, 10, &refnum)) {
                    continue;
                }
            }
        } else {
            /* Handle a \g<#> reference.
             * Right now we know we have \ (consumed) and g (not consumed).
             * We need to check for <##> and <#>. */
            M_parser_mark(parser);
            /* Consume the g. */
            M_parser_consume(parser, 1);

            /* Check for <. */
            if (!M_parser_read_byte(parser, &byte) || byte != '<') {
                M_parser_mark_rewind(parser);
                continue;
            }

            /* Try to pull off a 2 digit number and fallb ack to 1 digit number. */
            if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 2, 10, &refnum)) {
                if (!M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 1, 10, &refnum)) {
                    M_parser_mark_rewind(parser);
                    continue;
                }
            }

            /* Verify the reference close. */
            if (!M_parser_read_byte(parser, &byte) || byte != '>') {
                M_parser_mark_rewind(parser);
                continue;
            }
        }

        /* At this point we know the back reference number we need to add. */

        /* Kill the \ that was added from the read_until. */
        M_buf_truncate(buf, M_buf_len(buf)-1);

        /* Pull out the capture data and add it to our buf if there is actually data. */
        if (!M_re_match_idx(match, (size_t)refnum, &offset, &len)) {
            continue;
        }

        /* Add the back reference data from the catpure. */
        M_buf_add_bytes(buf, str+offset, len);
    }

    /* Read the remaining data into buf. */
    M_parser_read_buf(parser, buf, M_parser_len(parser));

    M_parser_destroy(parser);
    return M_buf_finish_str(buf, NULL);
}

char *M_re_sub(const M_re_t *re, const char *repl, const char *str)
{
    M_buf_t           *buf;
    char              *full_repl;
    M_list_t          *matches;
    const M_re_match_t*match;
    size_t             pos = 0;
    size_t             offset;
    size_t             mlen;
    size_t             len;
    size_t             i;

    if (re == NULL || str == NULL)
        return NULL;
    if (*str == '\0')
        return M_strdup("");

    /* Try and find all the matches. */
    matches = M_re_matches(re, str);
    if (matches == NULL)
        return M_strdup(str);

    buf = M_buf_create();

    len = M_list_len(matches);
    for (i=0; i<len; i++) {
        match = M_list_at(matches, i);

        /* Get the captured data we're going to replace. */
        M_re_match_idx(match, 0, &offset, &mlen);

        /* Add anything from our last pos to the start of this match. */
        M_buf_add_bytes(buf, str+pos, offset-pos);

        /* Add our repl. */
        full_repl = M_re_sub_build_repl(repl, str, match);
        M_buf_add_str(buf, full_repl);
        M_free(full_repl);

        /* Increase our pos to after this match. */
        pos = offset + mlen;
    }

    M_list_destroy(matches, M_TRUE);

    /* Add any trailing data. */
    M_buf_add_bytes(buf, str+pos, M_str_len(str)-pos);

    return M_buf_finish_str(buf, NULL);
}
