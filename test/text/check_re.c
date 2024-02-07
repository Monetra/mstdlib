#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_text.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * XXX: Need to figure out why these have different behavior than Perl/Python.
 * { "(a(b{1,2}){1,2}?)", "abbab", 0, 3, 0, 3, 1, 2, 0, 0 },
 * { "(a(b{1,2}?){1,2})", "abbab", 0, 3, 0, 3, 2, 1, 0, 0 },
 */

#define add_test(SUITENAME, TESTNAME)\
do {\
    TCase *tc;\
    tc = tcase_create(#TESTNAME);\
    tcase_add_test(tc, TESTNAME);\
    tcase_set_timeout(tc, 300);\
    suite_add_tcase(SUITENAME, tc);\
} while (0)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    const char *pattern;
    const char *str;
    size_t      offset;
    size_t      len;
    size_t      moffset1;
    size_t      mlen1;
    size_t      moffset2;
    size_t      mlen2;
    size_t      moffset3;
    size_t      mlen3;
} tdata_captures_t;

typedef struct {
    const char *pattern;
    const char *str;
} tdata_match_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_with_captures(tdata_captures_t *tdata)
{
    size_t i;

    for (i=0; tdata[i].pattern!=NULL; i++) {
        M_re_t       *re = NULL;
        M_re_match_t *mo = NULL;
        M_bool        ret;
        size_t        offset;
        size_t        len;

        re = M_re_compile(tdata[i].pattern, M_RE_NONE);
        ck_assert_msg(re != NULL, "%zu: re compile failed: pattern '%s'", i, tdata[i].pattern);

        ret = M_re_search(re, tdata[i].str, &mo);
        ck_assert_msg(ret == M_TRUE, "%zu: re search failed: pattern '%s', str '%s'", i, tdata[i].pattern, tdata[i].str);
        ck_assert_msg(mo != NULL, "%zu: re search success but match object missing", i);

        if (tdata[i].len != 0) {
            ret = M_re_match_idx(mo, 0, &offset, &len);

            ck_assert_msg(ret == M_TRUE, "%zu: '%s' match 0 not present", i, tdata[i].pattern);

            ck_assert_msg(offset == tdata[i].offset, "%zu: '%s' match 0 offset incorrect: got %zu, expected %zu", i, tdata[i].pattern, offset, tdata[i].offset);
            ck_assert_msg(len == tdata[i].len, "%zu: '%s' match 0 len incorrect: got %zu, expected %zu", i, tdata[i].pattern, len, tdata[i].len);
        }

        if (tdata[i].mlen1 != 0) {
            ret = M_re_match_idx(mo, 1, &offset, &len);
            ck_assert_msg(ret == M_TRUE, "%zu: '%s' match 1 not present", i, tdata[i].pattern);

            ck_assert_msg(offset == tdata[i].moffset1, "%zu: '%s' match 1 offset incorrect: got %zu, expected %zu", i, tdata[i].pattern, offset, tdata[i].moffset1);
            ck_assert_msg(len == tdata[i].mlen1, "%zu: '%s' match 1 len incorrect: got %zu, expected %zu", i, tdata[i].pattern, len, tdata[i].mlen1);
        }

        if (tdata[i].mlen2 != 0) {
            ret = M_re_match_idx(mo, 2, &offset, &len);
            ck_assert_msg(ret == M_TRUE, "%zu: '%s' match 2 not present", i, tdata[i].pattern);

            ck_assert_msg(offset == tdata[i].moffset2, "%zu: '%s' match 2 offset incorrect: got %zu, expected %zu", i, tdata[i].pattern, offset, tdata[i].moffset2);
            ck_assert_msg(len == tdata[i].mlen2, "%zu: '%s' match 2 len incorrect: got %zu, expected %zu", i, tdata[i].pattern, len, tdata[i].mlen2);
        }

        if (tdata[i].mlen3 != 0) {
            ret = M_re_match_idx(mo, 3, &offset, &len);
            ck_assert_msg(ret == M_TRUE, "%zu: '%s' match 3 not present", i, tdata[i].pattern);

            ck_assert_msg(offset == tdata[i].moffset3, "%zu: '%s' match 3 offset incorrect: got %zu, expected %zu", i, tdata[i].pattern, offset, tdata[i].moffset3);
            ck_assert_msg(len == tdata[i].mlen3, "%zu: '%s' match 3 len incorrect: got %zu, expected %zu", i, tdata[i].pattern, len, tdata[i].mlen3);
        }

        M_re_match_destroy(mo);
        M_re_destroy(re);
    }
}

static void check_with_nomatch(tdata_match_t *tdata)
{
    size_t i;

    for (i=0; tdata[i].pattern!=NULL; i++) {
        M_re_t *re = NULL;
        M_bool  ret;

        re = M_re_compile(tdata[i].pattern, M_RE_NONE);
        ck_assert_msg(re != NULL, "%zu: re compile failed: pattern '%s'", i, tdata[i].pattern);

        ret = M_re_search(re, tdata[i].str, NULL);
        ck_assert_msg(ret == M_FALSE, "%zu: re search succeeded when should have failed: pattern '%s'", i, tdata[i].pattern);

        M_re_destroy(re);
    }
}

static void check_with_compile(const char *res[], size_t num_res)
{
    size_t i;

    for (i=0; i<num_res; i++) {
        M_re_t *re = NULL;

        re = M_re_compile(res[i], M_RE_NONE);
        ck_assert_msg(re != NULL, "%zu: re compile failed: pattern '%s'", i, res[i]);

        M_re_destroy(re);
    }
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_conformance)
{
    /* Tests based on Open Group regex test cases.
     * Not all tests are included because M_re targets Perl/Python/PCRE
     * compatibility not POSIX. Tests with a difference in behavior
     * were removed. Test for features not supported
     * (collating symbols and equivalence classes) were removed.
     *
     * BRE tests are not included because BRE is not supported.
     *
     * Conformance tests use exclusive ending offsets. We use lengths. */
    tdata_captures_t tdata[] = {
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 1 */
        { "b+", "abbbc", 1, 3, 0, 0, 0, 0, 0, 0 },
        { "b+", "ababbbc", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 2 */
        { "(?i)B+", "abbbc", 1, 3, 0, 0, 0, 0, 0, 0 },
        { "(?i)b+", "aBBBc", 1, 3, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 3 */
        { "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop", "Aabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop", 1, 256, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 15 */
        { "[abc]", "abc", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "ab[abc]", "abc", 0, 3, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 16 */
        { "[abc]", "xbyz", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 17 */
        { "[^a]", "abc", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^ac]", "abcde-", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^a-bd-e]", "dec", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[^]cd]", "cd]ef", 3, 1, 0, 0, 0, 0, 0, 0 },
        { "[^ac-]", "abcde-", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^---]", "-ab", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^abc]", "axyz", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^a-b]", "abcde", 2, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 18 */
        { "[]a]", "cd]ef", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[]-a]", "a_b", 0, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 19 */
        { "[^]cd]", "cd]ef", 3, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 24 */
        { " [[:alnum:]]*", " aB18gH", 0, 7, 0, 0, 0, 0, 0, 0 },
        { "1[^[:alnum:]]*", "1 \t,\ba", 0, 5, 0, 0, 0, 0, 0, 0 },
        { " [[:alpha:]]*", " aBgH1", 0, 5, 0, 0, 0, 0, 0, 0 },
        { "[^[:alpha:]]*", "1 \t8,\ba", 0, 6, 0, 0, 0, 0, 0, 0 },
        { "[[:blank:]]*", " \t\b", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[^[:blank:]]*", "aB18gH,\b", 0, 8, 0, 0, 0, 0, 0, 0 },
        { "[[:cntrl:]]*", "\t\b", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[^[:cntrl:]]*", "aB1 8gH", 0, 7, 0, 0, 0, 0, 0, 0 },
        { "a[[:digit:]]*", "a18", 0, 3, 0, 0, 0, 0, 0, 0 },
        { "[^[:digit:]]*", "aB \tgH,\b", 0, 8, 0, 0, 0, 0, 0, 0 },
        { "[[:graph:]]*", "aB18gH", 0, 6, 0, 0, 0, 0, 0, 0 },
        { "[^[:graph:]]*", " \t\b", 0, 3, 0, 0, 0, 0, 0, 0 },
        { "[[:lower:]]*", "agB", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[^[:lower:]]*", "B1 \t8H,\ba", 0, 8, 0, 0, 0, 0, 0, 0 },
        { "[[:print:]]*", "aB1 8gH,\t", 0, 8, 0, 0, 0, 0, 0, 0 },
        { "[^[:print:]]*", "\t\b", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "a[[:punct:]]*", "a,1", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[^[:punct:]]*", "aB1 \t8gH\b", 0, 9, 0, 0, 0, 0, 0, 0 },
        { "[[:space:]]*", " \t\b", 0, 2, 0, 0, 0, 0, 0, 0 },
        { " [^[:space:]]*", " aB18gH,\b\t", 0, 9, 0, 0, 0, 0, 0, 0 },
        { "a[[:upper:]]*", "aBH1", 0, 3, 0, 0, 0, 0, 0, 0 },
        { "[^[:upper:]]*", "a1 \t8g,\bB", 0, 8, 0, 0, 0, 0, 0, 0 },
        { "g[[:xdigit:]]*", "gaB18h", 0, 5, 0, 0, 0, 0, 0, 0 },
        { "a[^[:xdigit:]]*", "a \tgH,\b1", 0, 7, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 25 */
        { "[a-c]", "bbccde", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "[a-b]", "-bc", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[a-z0-9]", "AB0", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[^a-b]", "abcde", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[^a-bd-e]", "dec", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[+--]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[--/]", "a.b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[+--c]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^---]", "-ab", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 26 */
        { "[a-cd-f]", "dbccde", 0, 1, 0, 0, 0, 0, 0, 0 },
        /* # vsx4/tset/XPG4.os/genuts/regex/T.regex 27 */
        { "[-xy]", "ac-", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[--/]", "a.b", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* # vsx4/tset/XPG4.os/genuts/regex/T.regex 28 */
        { "[^-c]*", "ab-cde", 0, 2, 0, 0, 0, 0, 0, 0 },
        /* # vsx4/tset/XPG4.os/genuts/regex/T.regex 29 */
        { "[xy-]", "zc-", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "[^ac-]", "abcde-", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 30 */
        { "[+--]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[+--]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[+--c]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[+--c]", "a,b", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^---]", "-ab", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[^---]", "-ab", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 32 */
        { "cd", "abcdeabcde", 2, 2, 0, 0, 0, 0, 0, 0 },
        { "ag*b", "abcde", 0, 2, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 46 */
        { "a$", "cba", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "(a$)", "bcaa", 3, 1, 3, 1, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 48 */
        { "^$", NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
        { "^abc$", "abc", 0, 3, 0, 0, 0, 0, 0, 0 },
        { "(^$)", NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(^abc$)", "abc", 0, 3, 0, 3, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 49 */
        { "(a)", "aaa", 0, 1, 0, 1, 0, 0, 0, 0 },
        { "([a])", "aaa", 0, 1, 0, 1, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 51 */
        { "\\.", "a.c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\[", "a[c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\\\", "a\\c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\(", "a(c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\*", "a*c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\+", "a+c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\?", "a?c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\{", "a{c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\|", "a|c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "\\^c", "a^c", 1, 2, 0, 0, 0, 0, 0, 0 },
        { "a\\$", "a$c", 0, 2, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 52 */
        { "[.]", "a.c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[[]", "a[c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[\\a]", "a\\c", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "[\\a]", "\\abc", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "[\\.]", "a\\.c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[\\.]", "a.\\c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[(]", "a(c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[*]", "a*c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[+]", "a+c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[?]", "a?c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[{]", "a{c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[|]", "a|c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[$]", "a$c", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 53 */
        { "[\\^]", "a^c", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "[b^]", "a^c", 1, 1, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 54 */
        { "(cd)", "abcdefabcdef", 2, 2, 2, 2, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 56 */
        { "(a(b(c(d(e)))))", "abcde", 0, 5, 0,5, 1,4, 2, 3}, /* More captures than we check. */
        { "(a(b(c(d(e(f(g)h(i(j))))))))", "abcdefghijk", 0, 10, 0, 10, 1,9, 2, 8 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 57 */
        { "(bb*)", "abbbc", 1, 3, 1, 3, 0, 0, 0, 0 },
        { "(bb*)", "ababbbc", 1, 1, 1, 1, 0, 0, 0, 0 },
        { "a(.*b)", "ababbbc", 0, 6, 1, 5, 0, 0, 0, 0 },
        { "a(b*)", "ababbbc", 0, 2, 1, 1, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 58 */
        { "b+(bc*)", "acabbbcde", 3, 4, 5, 2, 0, 0, 0, 0 },
        { "[ab]+", "abcdef", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[ab][ab]+", "abcdef", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "(abc)+", "acabcabcbbcde", 2, 6, 5, 3, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 59 */
        { "b*c", "cabbbcde", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "b*cd", "cabbbcdebbbbbbcdbc", 2, 5, 0, 0, 0, 0, 0, 0 },
        { "c(ab)*c", "dcabababcdeb", 1, 8, 6, 2, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 60 */
        { "b?c", "acabbbcde", 1, 1, 0, 0, 0, 0, 0, 0 },
        { "b?c", "abcabbbcde", 1, 2, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 61 */
        { "c{3}", "abababccccccd", 6, 3, 0, 0, 0, 0, 0, 0 },
        { "a{2}", "aaaa", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "a{0}", NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
        { "a{0}", "aaaa", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "a{255}", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 255, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 62 */
        { "([a-c]*){0,}", "aabcaab", 0, 7, 0, 7, 0, 0, 0, 0 },
        { "([a-c]*){2,}", "abcdefg", 0, 3, 3, 0, 0, 0, 0, 0 },
        { "(ab){2,}", "abababccccccd", 0, 6, 4, 2, 0, 0, 0, 0 },
        { "a{255,}", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 256, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 63 */
        { "a{2,3}", "aaaa", 0, 3, 0, 0, 0, 0, 0, 0 },
        { "(ab){2,3}", "abababccccccd", 0, 6, 2, 0, 0, 0, 0, 0 },
        { "([a-c]*){0,0}", "dabc", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "a{1,255}", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 255, 0, 0, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 64 */
        { "a|b|c|d", "a", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "a|b|c|d", "b", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "a|b|c|d", "c", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "a|b|c|d", "d", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "a((bc)|d)", "abc", 0, 3, 1, 2, 1, 2, 0, 0 },
        { "a((bc)|d)", "ad", 0, 2, 1, 1, 0, 0, 0, 0 },
        { "a((bc)|d)", "abcd", 0, 3, 1, 2, 1, 2, 0, 0 },
        { "(^|a)b(c|$)", "aabcc", 1, 3, 1, 1, 3, 1, 0, 0 },
        { "(^|a)b(c|$)", "bcc", 0, 2, 0, 0, 1, 1, 0, 0 },
        { "(^|a)b(c|$)", "aab", 1, 2, 1, 1, 3, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 65 */
        { "x(a|b|c|d)y", "xay", 0, 3, 1, 1, 0, 0, 0, 0 },
        { "x(a|b|c|d)y", "xby", 0, 3, 1, 1, 0, 0, 0, 0 },
        { "x(a|b|c|d)y", "xcy", 0, 3, 1, 1, 0, 0, 0, 0 },
        { "x(a|b|c|d)y", "xdy", 0, 3, 1, 1, 0, 0, 0, 0 },
        { "([a-z]|z)", "zabc", 0, 1, 0, 1, 0, 0, 0, 0 },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 66 */
        { "[\\(^*+|?{1})$]*", "^\\(*+|?{1})*$", 0, 13, 0, 0, 0, 0, 0, 0 },
        { "(a)*(b)+(c)?(d){2}", "aabbcdd", 0, 7, 1, 1, 3, 1, 4, 1 }, /* More captures than we check. */
        { "(a(b{1,2}){1,2})", "abbab", 0, 3, 0, 3, 1, 2, 0, 0 },
        { "^(^(^a$)$)$", "a", 0, 1, 0, 1, 0, 1, 0, 0 },
        { "((a|b)|(c|d))|e", "bde", 0, 1, 0, 1, 0, 1, 0, 0 },
        { "b?cd+e|f*gh{2}", "cdde", 0, 4, 0, 0, 0, 0, 0, 0 },
        { "b?cd+e|f*gh{2}", "bbdeghh", 4, 3, 0, 0, 0, 0, 0, 0 },
        { NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    check_with_captures(tdata);
}
END_TEST

START_TEST(check_conformance_nomatch)
{
    /* Tests based on Open Group regex test cases.
     * Not all tests are included because M_re targets Perl/Python/PCRE
     * compatibility not POSIX. Tests with a difference in behavior
     * were removed. Test for features not supported
     * (collating symbols and equivalence classes) were removed.
     *
     * BRE tests are not included because BRE is not supported.
     *
     * Conformance tests use exclusive ending offsets. We use lengths. */
    tdata_match_t tdata[] = {
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 16 */
        { "[abc]", "xyz" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 17 */
        { "[^abc]", "abc" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 32 */
        { "[a-c][e-f]", "abcdef" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 44 */
        { "^a", "^abc" },
        { "(^def)", "abcdef" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 46 */
        { "(a$)", "ba$" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 54 */
        { "(fg)", "abcdefabcdef" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 58 */
        { "ab+c", "ac" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 61 */
        { "a{2}", "abcd" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 62 */
        { "(ab){4,}", "abababccccccd" },
        /* vsx4/tset/XPG4.os/genuts/regex/T.regex 63 */
        { "(ab){4,5}", "abababccccccd" },
        { NULL, NULL }
    };

    check_with_nomatch(tdata);
}
END_TEST

START_TEST(check_tre_comp)
{
    const char *res[] = {
        "[A-Z]\\d\\s?\\d[A-Z]{2}|[A-Z]\\d{2}\\s?\\d[A-Z]{2}|[A-Z]{2}\\d\\s?\\d[A-Z]{2}|[A-Z]{2}\\d{2}\\s?\\d[A-Z]{2}|[A-Z]\\d[A-Z]\\s?\\d[A-Z]{2}|[A-Z]{2}\\d[A-Z]\\s?\\d[A-Z]{2}|[A-Z]{3}\\s?\\d[A-Z]{2}",
        "a{11}(b{2}c){2}",
        "a{2}{2}xb+xc*xd?x",
        "^!packet [0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3} [0-9]+",
        "^!pfast [0-9]{1,15} ([0-9]{1,3}\\.){3}[0-9]{1,3}[0-9]{1,5}$",
        "ksntoeaiksntoeaikstneoaiksnteoaiksntoeaiskntoeaiskntoekainstoeiaskntoeakisntoeksaitnokesantiksoentaikosentaiksoentaiksnoeaisknteoaksintoekasitnoeksaitkosetniaksoetnaisknoetakistoeksintokesanitksoentaisknoetaisknoetiaksotneaikstoekasitoeskatioksentaiksoenatiksoetnaiksonateiksoteaeskanotisknetaiskntoeasknitoskenatiskonetaisknoteai"
    };

    check_with_compile(res, sizeof(res)/sizeof(*res));
}
END_TEST

START_TEST(check_tre_nomatch)
{
    tdata_match_t tdata[] = {
        { "\\bx", "aax" },
        { "a{2,}", "" },
        { "a{2,}", "a" },
        { "a{3,}", "aa" },
        { "a{6,6}", "xxaaaaa" },
        { "a{6}", "xxaaaaa" },
        { "(.){2}{3}", "xxxxx" },
        { "(..){2}{3}", "xxxxxxxxxxx" },
        { "((..){2}.){3}", "xxxxxxxxxxxxxx" },
        { "((..){1,2}.){3}", "xxxxxxxx" },
        { "a{2}{2}x", "" },
        { "a{2}{2}x", "x" },
        { "a{2}{2}x", "ax" },
        { "a{2}{2}x", "aax" },
        { "a{2}{2}x", "aaax" },
        { "([a-z]+){2,5}", "a\n" },
        { "a{3}b{3}", "aabbb" },
        { "a{3}b{3}", "aaabb" },
        { "a{2}{2}xb+xc*xd?x", "aaaxbxcxdx" },
        { "a{2}{2}xb+xc*xd?x", "aabxcxdx" },
        { "a{2}{2}xb+xc*xd?x", "aaaacxdx" },
        { "a{2}{2}xb+xc*xd?x", "aaaaxbdx" },
        { NULL, NULL }
    };

    check_with_nomatch(tdata);
}
END_TEST

START_TEST(check_tre_exec)
{
    tdata_captures_t tdata[] = {
        { "foobar", "foobar", 0, 6, 0, 0, 0, 0, 0, 0 },
        { "foobar", "xxxfoobarzapzot", 3, 6, 0, 0, 0, 0, 0, 0 },
        { "aaaa", "xxaaaaaaaaaaaaaaaaa", 2, 4, 0, 0, 0, 0, 0, 0 },
        { "(a*)", "", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "((a*)*)*", "", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*bcd)*", "aaaaaaaaaaaabcxbcxbcxaabcxaabcx", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*bcd)*", "aaaaaaaaaaaabcxbcxbcxaabcxaabc", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*bcd)*", "aaaaaaaaaaaabcxbcdbcxaabcxaabc", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*bcd)*", "aaaaaaaaaaaabcdbcdbcxaabcxaabc", 0, 18, 15, 3, 0, 0, 0, 0 },
        { "(a*)+", "-", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "((a*)*b)*b", "aaaaaaaaaaaaaaaaaaaaaaaaab", 25, 1, 0, 0, 0, 0, 0, 0 },
        { "", "", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "", "foo", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*)aaaaaa", "aaaaaaaaaaaaaaax", 0, 15, 0, 9, 0, 0, 0, 0 },
        { "(a*)(a*)", "aaaa", 0, 4, 0, 4, 0, 0, 0, 0 },
        { "(abcd|abc)(d?)", "abcd", 0, 4, 0, 4, 0, 0, 0, 0 },
        { "(abc|abcd)(d?)", "abcd", 0, 4, 0, 4, 0, 0, 0, 0 },
        { "(abc|abcd)(d?)e", "abcde", 0, 5, 0, 4, 0, 0, 0, 0 },
        { "(abcd|abc)(d?)e", "abcde", 0, 5, 0, 4, 0, 0, 0, 0 },
        { "a(bc|bcd)(d?)", "abcd", 0, 4, 1, 3, 0, 0, 0, 0 },
        { "a(bcd|bc)(d?)", "abcd", 0, 4, 1, 3, 0, 0, 0, 0 },
        { "a*(a?bc|bcd)(d?)", "aaabcd", 0, 6, 3, 3, 0, 0, 0, 0 },
        { "a*(bcd|a?bc)(d?)", "aaabcd", 0, 6, 3, 3, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "a", 0, 1, 0, 1, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "aa", 0, 2, 0, 2, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "aaa", 0, 3, 0, 3, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "bbb", 0, 3, 0, 3, 0, 0, 0, 0 },
        { "(a|(a*b*))*", "aaabbb", 0, 6, 0, 6, 0, 6, 0, 0 },
        { "(a|(a*b*))*", "bbbaaa", 0, 6, 3, 3, 3, 3, 0, 0 },
        { "((a*b*)|a)*", "", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "((a*b*)|a)*", "a", 0, 1, 0, 1, 0, 0, 0, 0 },
        { "((a*b*)|a)*", "aa", 0, 2, 0, 2, 0, 0, 0, 0 },
        { "((a*b*)|a)*", "aaa", 0, 3, 0, 3, 0, 0, 0, 0 },
        { "((a*b*)|a)*", "bbb", 0, 3, 0, 3, 0, 0, 0, 0 },
        { "((a*b*)|a)*", "aaabbb", 0, 6, 0, 6, 0, 6, 0, 0 },
        { "((a*b*)|a)*", "bbbaaa", 0, 6, 3, 3, 3, 3, 0, 0 },
        { "a.*(.*b.*(.*c.*).*d.*).*e.*(.*f.*).*g", "aabbccddeeffgg", 0, 14, 3, 6, 5, 2, 11, 2 },
        { "(wee|week)(night|knights)s*", "weeknights", 0, 10, 0, 3, 3, 7, 0, 0 },
        { "(wee|week)(night|knights)s*", "weeknightss", 0, 11, 0, 3, 3, 7, 0, 0 },
        { "((a)|(b))*c", "aaabc", 0, 5, 3, 1, 0, 0, 0, 0 },
        { "((a)|(b))*c", "aaaac", 0, 5, 3, 1, 3, 1, 0, 0 },
        { "foo((bar)*)*zot", "foozot", 0, 6, 0, 0, 0, 0, 0, 0 },
        { "foo((bar)*)*zot", "foobarzot", 0, 9, 3, 3, 3, 3, 0, 0 },
        { "foo((bar)*)*zot", "foobarbarzot", 0, 12, 3, 6, 6, 3, 0, 0 },
        { "(a|ab)(blip)?", "ablip", 0, 5, 0, 1, 1, 4, 0, 0 },
        { "(a|ab)(blip)?", "ab", 0, 2, 0, 2, 0, 0, 0, 0 },
        { "(ab|a)(blip)?", "ablip", 0, 5, 0, 1, 1, 4, 0, 0 },
        { "(ab|a)(blip)?", "ab", 0, 2, 0, 2, 0, 0, 0, 0 },
        { "((a|b)*)a(a|b)*", "aaaaabaaaba", 0, 11, 0, 10, 9, 1, 0, 0 },
        { "((a|b)*)a(a|b)*", "aaaaabaaab", 0, 10, 0, 8, 7, 1, 9, 1 },
        { "((a|b)*)a(a|b)*", "caa", 1, 2, 1, 1, 1, 1, 0, 0 },
        { "((a|aba)*)(ababbaba)((a|b)*)", "aabaababbabaaababbab", 0, 20, 0, 4, 1, 3, 4, 8 },
        { "((a|aba)*)(ababbaba)((a|b)*)", "aaaaababbaba", 0, 12, 0, 4, 3, 1, 4, 8 },
        { "((a|aba|abb|bba|bab)*)(ababbababbabbbabbbbbbabbaba)((a|b)*)", "aabaabbbbabababaababbababbabbbabbbbbbabbabababbababababbabababa", 0, 63, 0, 16, 13, 3, 16, 27 },
        { "a|", "a", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "|a", "a", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "|a", "b", 0, 0, 0, 0, 0, 0, 0, 0 },
        { "(a*)b(c*)", "abc", 0, 3, 0, 1, 2, 1, 0, 0 },
        { "(a*)b(c*)", "***abc***", 3, 3, 3, 1, 5, 1, 0, 0 },
        { "((((((((((((((((((((a))))))))))))))))))))", "a", 0, 1, 0, 1, 0, 1, 0, 1 },
        { "(?i)(Ab|cD)*", "aBcD", 0, 4, 2, 2, 0, 0, 0, 0 },
        { "[--Z]+", "!ABC-./XYZ~", 1, 9, 0, 0, 0, 0, 0, 0 },
        { "[*--]", "-", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "[*--]", "*", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "[*--Z]+", "!+*,---ABC", 1, 6, 0, 0, 0, 0, 0, 0 },
        { "[a-]+", "xa-a--a-ay", 1, 8, 0, 0, 0, 0, 0, 0 },
        { "(?i)[a-c]*", "cABbage", 0, 5, 0, 0, 0, 0, 0, 0 },
        { "(?i)[^a-c]*", "tObAcCo*", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "[[:digit:]a-z#$%]+", "__abc#lmn012$x%yz789*", 2, 18, 0, 0, 0, 0, 0, 0 },
        { "[^[:digit:]#$%[:xdigit:]]+", "abc#lmn012$x%yz789--@*,abc", 4, 3, 0, 0, 0, 0, 0, 0 },
        { "[^--Z]+", "---AFD*(&,ml---", 6, 6, 0, 0, 0, 0, 0, 0 },
        { "a?", "aaaa", 0, 1, 0, 0, 0, 0, 0, 0 },
        { "a+", "aaaaa", 0, 5, 0, 0, 0, 0, 0, 0 },
        { "a+", "xaaaaa", 1, 5, 0, 0, 0, 0, 0, 0 },
        { ".*", "ab\ncd", 0, 2, 0, 0, 0, 0, 0, 0 },
        { "(?s).*", "ab\ncd", 0, 5, 0, 0, 0, 0, 0, 0 },
        { "\\<x", "aax xaa", 4, 1, 0, 0, 0, 0, 0, 0 },
        { "x\\>", "axx xaa", 2, 1, 0, 0, 0, 0, 0, 0 },
        { "\\w+", ",.(a23_Nt-öo)", 3, 6, 0, 0, 0, 0, 0, 0},
        { "\\d+", "uR120_4=v4", 2, 3, 0, 0, 0, 0, 0, 0 },
        { NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    check_with_captures(tdata);
}
END_TEST

static void check_with_catpure_holes(const M_re_t *re, const char *str, size_t idx, size_t num_caps, ...)
{
    M_re_match_t *mo;
    M_bool        ret;
    va_list       ap;
    size_t        cap_num;
    size_t        cap_offset;
    size_t        cap_len;
    size_t        offset;
    size_t        len;
    size_t        i;

    ret = M_re_search(re, str, &mo);
    ck_assert_msg(ret == M_TRUE, "%zu: re search failed", idx);
    ck_assert_msg(mo != NULL, "%zu: re search success but match object missing", idx);

    va_start(ap, num_caps);
    for (i=0; i<num_caps; i++) {
        cap_num    = (size_t)va_arg(ap, int);
        cap_offset = (size_t)va_arg(ap, int);
        cap_len    = (size_t)va_arg(ap, int);

        ret = M_re_match_idx(mo, cap_num, &offset, &len);
        ck_assert_msg(ret == M_TRUE, "%zu: Match %zu not present", idx, cap_num);

        ck_assert_msg(offset == cap_offset, "%zu: Match %zu offset incorrect: got %zu, expected %zu", idx, cap_num, offset, cap_offset);
        ck_assert_msg(len == cap_len, "%zu: Match %zu len incorrect: got %zu, expected %zu", idx, cap_num, len, cap_len);
    }
    va_end(ap);

    M_re_match_destroy(mo);
}

START_TEST(check_tre_catpure_holes)
{
    M_re_t     *re;
    const char *pat;

    pat = "foo((zup)*|(bar)*|(zap)*)*zot";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "foobarzapzot", 0 /* idx */, 3 /* captures */,
            0, 0, 12, /* capture 0, offset 0, len 12 */
            1, 6, 3, /* capture 1, offset 6, len 3 */
            4, 6, 3 /* capture 4, offset 6, len 3 */);
    check_with_catpure_holes(re, "foobarbarzapzot", 1, 3,
            0, 0, 15,
            1, 9, 3,
            4, 9, 3);
    check_with_catpure_holes(re, "foozupzot", 2, 3,
            0, 0, 9,
            1, 3, 3,
            2, 3, 3);
    check_with_catpure_holes(re, "foobarzot", 3, 3,
            0, 0, 9,
            1, 3, 3,
            3, 3, 3);
    check_with_catpure_holes(re, "foozapzot", 4, 3,
            0, 0, 9,
            1, 3, 3,
            4, 3, 3);
    check_with_catpure_holes(re, "foozot", 5, 2,
            0, 0, 6,
            1, 3, 0);
    M_re_destroy(re);


    pat = "((aab)|(aac)|(aa*))c";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "aabc", 3,
            0, 0, 4,
            1, 0, 3,
            2, 0, 3);
    check_with_catpure_holes(re, "aacc", 3,
            0, 0, 4,
            1, 0, 3,
            3, 0, 3);
    check_with_catpure_holes(re, "aaac", 3,
            0, 0, 4,
            1, 0, 3,
            4, 0, 3);
    M_re_destroy(re);

    pat = "^(([^!]+!)?([^!]+)|.+!([^!]+!)([^!]+))$";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "foo!bar!bas", 4,
            0, 0, 11,
            1, 0, 11,
            4, 4, 4,
            5, 8, 3);
    M_re_destroy(re);

    pat = "^([^!]+!)?([^!]+)$|^.+!([^!]+!)([^!]+)$";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "foo!bar!bas", 3,
            0, 0, 11,
            3, 4, 4,
            4, 8, 3);
    M_re_destroy(re);

    pat = "^(([^!]+!)?([^!]+)|.+!([^!]+!)([^!]+))$";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "foo!bar!bas", 4,
            0, 0, 11,
            1, 0, 11,
            4, 4, 4,
            5, 8, 3);
    M_re_destroy(re);

    pat = "M[ou]'?am+[ae]r .*([AEae]l[- ])?[GKQ]h?[aeu]+([dtz][dhz]?)+af[iy]";
    re  = M_re_compile(pat, M_RE_NONE);
    ck_assert_msg(re != NULL, "re compile failed: pattern '%s'", pat);
    check_with_catpure_holes(re, "Muammar Quathafi", 2,
            0, 0, 16,
            2, 11, 2);
    M_re_destroy(re);
}
END_TEST

START_TEST(check_sub)
{
    struct {
        const char *pattern;
        M_uint32    flags;
        const char *repl;
        const char *str;
        const char *out;
    } tdata[] = {
        { " ([cde])", M_RE_NONE, "\\0", "a b c d e f g", "a b c d e f g" },
        { " ([cde])", M_RE_NONE, "", "a b c d e f g", "a b f g" },
        { " ([cde])", M_RE_NONE, "\\1", "a b c d e f g", "a bcde f g" },
        { " ([cde])", M_RE_NONE, "Zi1", "a b c d e f g", "a bZi1Zi1Zi1 f g"  },
        { " ([cde])", M_RE_CASECMP, "\\g<01>", "a b C d e f g", "a bCde f g" },
        { "(?i) ([cde])", M_RE_NONE, "\\01", "a b C d e f g", "a bCde f g" },
        { "(?i) ([[:alnum:]])", M_RE_NONE, "\\1", "a b C d e f g", "abCdefg" },
        { "(?i) ([[:print:]])", M_RE_NONE, "\\1", "a b C d e f g", "abCdefg" },
        { "([[:punct:]])", M_RE_NONE, "-", "a !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ b c", "a -------------------------------- b c" },
        { "([^[:punct:]])", M_RE_NONE, "-", "a !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ b c", "--!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~----" },
        { "([^[:punct:] ])", M_RE_NONE, "-", "a b C d e f g", "- - - - - - -" },
        { "(?i) ([^[:word:]])", M_RE_NONE, "\\g<1>", "a b C d e f g", "a b C d e f g" },
        { "(?i)([^[:word:]])", M_RE_NONE, "_", "a b C d e f g", "a_b_C_d_e_f_g" },
        { "(?i) (\\W)", M_RE_NONE, "\\1", "a b C d e f g", "a b C d e f g" },
        { "(?i)(\\W)", M_RE_NONE, "_", "a b C d e f g", "a_b_C_d_e_f_g" },
        { "(?i)[^a]", M_RE_NONE, "_", "a b C d e f g", "a____________" },
        { "(?i) ([:alnum:])", M_RE_NONE, "\\1", "a b C d e f g", "a b C d e f g" },
        { "(?i) ([0-9])", M_RE_NONE, "\\1", "a b C d e f g", "a b C d e f g" },
        { "(?) (\\d)", M_RE_NONE, "\\1", "a b C d e f g", "a b C d e f g" },
        { "(?) ([^d])", M_RE_NONE, "\\g<1>", "a b C d e f g", "abC defg" },
        { "[^abc]", M_RE_NONE, "", "a b C d e f g", "ab" },
        { "[^a-c]", M_RE_NONE, "", "a b C d e f g", "ab" },
        { "[^a-b]", M_RE_NONE, "", "a b C d e f g", "ab" },
        { "(?i)[^abc]", M_RE_NONE, "", "a b C d e f g", "abC" },
        { "(?i)[^a-c]", M_RE_NONE, "", "a b C d e f g", "abC" },
        { "(?i)[^a-b]", M_RE_NONE, "", "a b C d e f g", "ab" },
        { "[^0-9]", M_RE_NONE, "", "12 / 27", "1227" },
        { "[^0-9]+", M_RE_NONE, "", "12 / 27", "1227" },
#if 0
        /* Will fail to sub.
         * Works with Python. Does not macOS POSIX regex.h
         * Does not macOS POSIX tre.h.
         * M_re is based on tre and I believe macOS is too.
         * So failing is expected. This is a somewhat ambiguous
         * expression.
         */
        { "[^0-9]*", M_RE_NONE, "", "12 / 27", "1227" },
#endif
        { NULL, 0, NULL, NULL, NULL }
    };
    char   *out;
    M_re_t *re;
    size_t  i;

    for (i=0; tdata[i].pattern!=NULL; i++) {
        re = M_re_compile(tdata[i].pattern, tdata[i].flags);
        ck_assert_msg(re != NULL, "%zu: re compile failed", i);

        out = M_re_sub(re, tdata[i].repl, tdata[i].str);
        ck_assert_msg(M_str_eq(out, tdata[i].out), "%zu: sub failed: pat '%s', expected '%s', got '%s'", i, tdata[i].pattern, tdata[i].out, out);

        M_free(out);
        M_re_destroy(re);
    }
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
    Suite   *suite;
    SRunner *sr;
    int      nf;

    suite = suite_create("re");

    add_test(suite, check_conformance);
    add_test(suite, check_conformance_nomatch);
    add_test(suite, check_tre_comp);
    add_test(suite, check_tre_nomatch);
    add_test(suite, check_tre_exec);
    add_test(suite, check_tre_catpure_holes);
    add_test(suite, check_sub);

    sr = srunner_create(suite);
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_re.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
