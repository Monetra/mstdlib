#include "m_config.h"
#include <check.h>
#include <stdlib.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static size_t       i;
static char        *test;
static char        *str1;
static char        *str2;
static size_t       num;
static int         *ints;
static char       **strs;
static const char  *cstr;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_safe)
{
	ck_assert_msg(M_str_eq(M_str_safe(NULL),       ""));
	ck_assert_msg(M_str_eq(M_str_safe(""),         ""));
	ck_assert_msg(M_str_eq(M_str_safe("test"), "test"));
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_len)
{
	ck_assert_msg(M_str_len(NULL) == 0);
	ck_assert_msg(M_str_len("") == 0);
	ck_assert_msg(M_str_len("a") == 1);
	ck_assert_msg(M_str_len("aa") == 2);
	ck_assert_msg(M_str_len("aaa") == 3);
}
END_TEST

START_TEST(check_len_max)
{
	ck_assert_msg(M_str_len(NULL) == 0);
	ck_assert_msg(M_str_len("") == 0);
	ck_assert_msg(M_str_len("a") == 1);
	ck_assert_msg(M_str_len("aa") == 2);
	ck_assert_msg(M_str_len("aaa") == 3);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_ischarset_empty)
{
	ck_assert_msg(!M_str_ischarset(NULL,NULL));
	ck_assert_msg(!M_str_ischarset("",""));
	ck_assert_msg(!M_str_ischarset("abc",NULL));
	ck_assert_msg(!M_str_ischarset(NULL,"abc"));
}
END_TEST

START_TEST(check_ischarset_single)
{
	ck_assert_msg(M_str_ischarset("a","a"));
	ck_assert_msg(M_str_ischarset("\f","\f"));
	ck_assert_msg(!M_str_ischarset("a","b"));
}
END_TEST

START_TEST(check_ischarset_multi)
{
	ck_assert_msg(M_str_ischarset("ollyoxenfree","frolenyx"));
	ck_assert_msg(!M_str_ischarset("ollyoxenfree","froleny"));
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_isnotcharset_empty)
{
	ck_assert_msg(M_str_isnotcharset(NULL,NULL));
	ck_assert_msg(M_str_isnotcharset("",""));
	ck_assert_msg(M_str_isnotcharset("abc",NULL));
	ck_assert_msg(M_str_isnotcharset(NULL,"abc"));
}
END_TEST

START_TEST(check_isnotcharset_single)
{
	ck_assert_msg(!M_str_isnotcharset("a","a"));
	ck_assert_msg(!M_str_isnotcharset("\f","\f"));
	ck_assert_msg(M_str_isnotcharset("a","b"));
}
END_TEST

START_TEST(check_isnotcharset_multi)
{
	ck_assert_msg(!M_str_isnotcharset("ollyoxenfree","frolenyx"));
	ck_assert_msg(!M_str_isnotcharset("ollyoxenfree","froleny"));
	ck_assert_msg(M_str_isnotcharset("abcdef", "ghi"));
	ck_assert_msg(M_str_isnotcharset("abcdef", "i"));
	ck_assert_msg(!M_str_isnotcharset("aaabbbcccddd", "def"));
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_find_first_from_charset)
{
	ck_assert_msg(M_str_find_first_from_charset(NULL,"") == NULL);
	ck_assert_msg(M_str_find_first_from_charset("",NULL) == NULL);
	
	cstr = "rocky balboa.";
	ck_assert_msg(M_str_find_first_from_charset(cstr, "xqz ")  == (cstr + 5));
	ck_assert_msg(M_str_find_first_from_charset(cstr, "xqz y") == (cstr + 4));
	ck_assert_msg(M_str_find_first_from_charset(cstr, "xqz r") == (cstr + 0));
	ck_assert_msg(M_str_find_first_from_charset(cstr, "xqz.")  == (cstr + 12));
	ck_assert_msg(M_str_find_first_from_charset(cstr, "xqz")   == NULL);
}
END_TEST

START_TEST(check_find_first_not_from_charset)
{
	ck_assert_msg(M_str_find_first_not_from_charset(NULL,"") == NULL);
	ck_assert_msg(M_str_find_first_not_from_charset("",NULL) == NULL);
	
	cstr = "aaabbb!cccQ";
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, NULL)    == (cstr));
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, "")      == (cstr));
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, "123")   == (cstr));
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, "ab")    == (cstr + 6));
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, "ab!c")  == (cstr + 10));
	ck_assert_msg(M_str_find_first_not_from_charset(cstr, "ab!cQ") == NULL);
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_remove_bracketed)
{
	str1 = M_str_remove_bracketed("",'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed(NULL,'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed("abcdef",'<','>');
	ck_assert_msg(M_str_eq(str1, "abcdef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed("ab<cd>ef",'<','>');
	ck_assert_msg(M_str_eq(str1, "abef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed("a<bcdf<we>fdef>",'<','>');
	ck_assert_msg(M_str_eq(str1, "a"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed("<abcdf<we>fde>f",'<','>');
	ck_assert_msg(M_str_eq(str1, "f"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed("a<bc><d<e>>f",'<','>');
	ck_assert_msg(M_str_eq(str1, "af"));
	M_free(str1);
}
END_TEST

START_TEST(check_keep_bracketed)
{
	str1 = M_str_keep_bracketed(NULL,'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("",'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("<",'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("abcdef",'<','>');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("ab<cd>ef",'<','>');
	ck_assert_msg(M_str_eq(str1, "cd"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("a<bcdf<we>fdef>",'<','>');
	ck_assert_msg(M_str_eq(str1, "bcdf<we>fdef"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("<abcdf<we>fde>f",'<','>');
	ck_assert_msg(M_str_eq(str1, "abcdf<we>fde"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed("a<bc><d<e>>f",'<','>');
	ck_assert_msg(M_str_eq(str1, "bcd<e>"));
	M_free(str1);
}
END_TEST

START_TEST(check_remove_bracketed_quoted)
{
	str1 = M_str_remove_bracketed_quoted("",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted(NULL,'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("<",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("\"<\"",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "\"<\""));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("\\\"<\\\"",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("<>",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("\"<>\"",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "\"<>\""));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("abcdef",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "abcdef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("ab<cd>ef",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "abef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("ab\"<cd>\"ef",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "ab\"<cd>\"ef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("ab\\\"<cd>\\\"ef",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "ab\\\"\\\"ef"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("a<bcdf<we>fdef>",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "a"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("<abcdf<we>fde>f",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "f"));
	M_free(str1);
	
	str1 = M_str_remove_bracketed_quoted("a<bc><d<e>>f",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "af"));
	M_free(str1);
}
END_TEST

START_TEST(check_keep_bracketed_quoted)
{
	str1 = M_str_keep_bracketed_quoted(NULL,'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("<",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("\"<\"",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("abcdef",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("ab<cd>ef",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "cd"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("a\"b<cd>\"ef",'<','>','"','\\');
	ck_assert_msg(M_str_isempty(str1));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("a<bcdf<we>fdef>",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "bcdf<we>fdef"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("a\"<bcdf>e\"f<w\\\"e>f<def>",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "w\\\"edef"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("<abcdf<we>fde>f",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "abcdf<we>fde"));
	M_free(str1);
	
	str1 = M_str_keep_bracketed_quoted("a<bc><d<e>>f",'<','>','"','\\');
	ck_assert_msg(M_str_eq(str1, "bcd<e>"));
	M_free(str1);
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_split_on_char_NULL)
{
	ck_assert_msg(M_str_split_on_char(NULL, ' ') == NULL);
}
END_TEST

START_TEST(check_split_on_char_empty)
{
	test = M_strdup("");
	ck_assert_msg(M_str_split_on_char(test, ' ') == test);
	M_free(test);
}
END_TEST

START_TEST(check_split_on_char_empty_left_empty_right)
{
	test = M_strdup(" ");
	str1 = M_str_split_on_char(test, ' ');
	ck_assert_msg(str1 == test+1);
	M_free(test);
}
END_TEST

START_TEST(check_split_on_char_left_right)
{
	test = M_strdup("foo bar");
	str1 = test;
	str2 = M_str_split_on_char(str1, ' ');
	ck_assert_msg(M_str_eq(str1,"foo"));
	ck_assert_msg(M_str_eq(str2,"bar"));
	M_free(test);
}
END_TEST

START_TEST(check_split_on_char_left)
{
	test = M_strdup(" foo");
	str1 = test;
	str2 = M_str_split_on_char(str1, ' ');
	ck_assert_msg(M_str_eq(str1,""));
	ck_assert_msg(M_str_eq(str2,"foo"));
	M_free(test);
}
END_TEST

START_TEST(check_split_on_char_right)
{
	test = M_strdup("foo ");
	str1 = test;
	str2 = M_str_split_on_char(str1, ' ');
	ck_assert_msg(M_str_eq(str1,"foo"));
	ck_assert_msg(M_str_eq(str2,""));
	M_free(test);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_explode_lines)
{
	strs = M_str_explode_lines(3, 7, "12345 12345 1 1234567 123", M_TRUE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL) /* This is a hack to suppress false warnings in clang static analyzer */
		return;
	ck_assert_msg(num == 3);
	ck_assert_msg(M_str_eq(strs[0], "12345"));
	ck_assert_msg(M_str_eq(strs[1], "12345 1"));
	ck_assert_msg(M_str_eq(strs[2], "1234567"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_no_truncate)
{
	strs = M_str_explode_lines(3, 7, "12345 12345 1 1234567 123", M_FALSE, &num);
	ck_assert_msg(strs == NULL);
	ck_assert_msg(num == 0);
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_empty)
{
	strs = M_str_explode_lines(3, 10, "", M_FALSE, &num);
	ck_assert_msg(strs == NULL);
	ck_assert_msg(num == 0);
	
	strs = M_str_explode_lines(3, 10, NULL, M_FALSE, &num);
	ck_assert_msg(strs == NULL);
	ck_assert_msg(num == 0);
}
END_TEST

START_TEST(check_explode_lines_empty_white)
{
	strs = M_str_explode_lines(1, 10, " \t\n\v\f\r ", M_FALSE, &num);
	ck_assert_msg(strs == NULL);
	ck_assert_msg(num == 0);
}
END_TEST

START_TEST(check_explode_lines_small)
{
	strs = M_str_explode_lines(1, 7, "123 56 ", M_FALSE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 1);
	ck_assert_msg(M_str_eq(strs[0], "123 56"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_str_multi_space)
{
	strs = M_str_explode_lines(3, 7, "12  56  \t\n 123\t  7 \n", M_FALSE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 2);
	ck_assert_msg(M_str_eq(strs[0], "12  56"));
	ck_assert_msg(M_str_eq(strs[1], "123\t  7"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_full)
{
	strs = M_str_explode_lines(3, 3, "123456789", M_TRUE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 3);
	ck_assert_msg(M_str_eq(strs[0], "123"));
	ck_assert_msg(M_str_eq(strs[1], "456"));
	ck_assert_msg(M_str_eq(strs[2], "789"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_full_no_truncate)
{
	strs = M_str_explode_lines(3, 3, "123456789", M_FALSE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 3);
	ck_assert_msg(M_str_eq(strs[0], "123"));
	ck_assert_msg(M_str_eq(strs[1], "456"));
	ck_assert_msg(M_str_eq(strs[2], "789"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_skip_full)
{
	strs = M_str_explode_lines(4, 3, "1 234 567890", M_FALSE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 4);
	ck_assert_msg(M_str_eq(strs[0], "1"));
	ck_assert_msg(M_str_eq(strs[1], "234"));
	ck_assert_msg(M_str_eq(strs[2], "567"));
	ck_assert_msg(M_str_eq(strs[3], "890"));
	M_str_explode_free(strs, num);
}
END_TEST

START_TEST(check_explode_lines_skip_empty)
{
	strs = M_str_explode_lines(2, 3, "1                ", M_FALSE, &num);
	ck_assert_msg(strs != NULL);
	if (strs == NULL)
		return;
	ck_assert_msg(num == 1);
	ck_assert_msg(M_str_eq(strs[0], "1"));
	M_str_explode_free(strs, num);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int str_implode_int_list_pos_ints[] = { 1, 22, 333 };
static int str_implode_int_list_neg_ints[] = {-1,-22,-333 };

START_TEST(check_implode_int_list)
{
	char *str;

	ints = str_implode_int_list_pos_ints;

	str = M_str_implode_int(',', ints, 0);
	ck_assert(str == NULL);

	str = M_str_implode_int(',', ints, 1);
	ck_assert(M_str_eq("1", str));
	M_free(str);

	str = M_str_implode_int(',', ints, 2);
	ck_assert(M_str_eq("1,22", str));
	M_free(str);

	str = M_str_implode_int(',', ints, 3);
	ck_assert(M_str_eq("1,22,333", str));
	M_free(str);


	ints = str_implode_int_list_neg_ints;

	str = M_str_implode_int('|', ints, 0);
	ck_assert(str == NULL);

	str = M_str_implode_int('|', ints, 1);
	ck_assert(M_str_eq("-1", str));
	M_free(str);

	str = M_str_implode_int('|', ints, 2);
	ck_assert(M_str_eq("-1|-22", str));
	M_free(str);

	str = M_str_implode_int('|', ints, 3);
	ck_assert(M_str_eq("-1|-22|-333", str));
	M_free(str);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_isempty_empty)
{
	ck_assert_msg(M_str_isempty(NULL));
	ck_assert_msg(M_str_isempty(""));
}
END_TEST

START_TEST(check_isempty_nonempty)
{
	ck_assert_msg(!M_str_isempty("x"));
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_str_empty_needle)
{
	ck_assert_msg(M_str_eq(M_str_str("foo",NULL),"foo"));
	ck_assert_msg(M_str_eq(M_str_str("foo",""  ),"foo"));
}
END_TEST

START_TEST(check_str_empty_haystack)
{
	ck_assert_msg(M_str_str(NULL, "bar") == NULL);
	ck_assert_msg(M_str_str(NULL, ""   ) == NULL);
}
END_TEST

START_TEST(check_str_empty_needle_and_haystack)
{
	ck_assert_msg(M_str_str(NULL,NULL) == NULL);
}
END_TEST

START_TEST(check_str_notfound)
{
	ck_assert_msg(M_str_str("foo","bar") == NULL);
}
END_TEST

START_TEST(check_str_found)
{
	const char *cstr1 = "foo bar";
	ck_assert_msg(M_str_str(cstr1,"bar") == cstr1+4);
}
END_TEST

START_TEST(check_str_ends)
{
	M_bool r;
	static struct {
		const char *s;
		const char *e;
		M_bool      cs;
		M_bool      r;
	} ends[] = {
		{ "abc",     "c",    M_TRUE,  M_TRUE  },
		{ "abc",     "C",    M_FALSE, M_TRUE  },
		{ "AbC",     "c",    M_FALSE, M_TRUE  },
		{ "AbC",     "c",    M_TRUE,  M_FALSE },
		{ "abc",     "y",    M_FALSE, M_FALSE },
		{ "abc.txt", ".txt", M_TRUE,  M_TRUE  },
		{ "abc.txt", ".txt", M_FALSE, M_TRUE  },
		{ "abc.txt", ".TXT", M_FALSE, M_TRUE  },
		{ "abc.txt", ".TXT", M_TRUE,  M_FALSE },
		{ "abc.txt", ".png", M_FALSE, M_FALSE },
		{ "abc.txt", "txt",  M_TRUE,  M_TRUE  },
		{ NULL, NULL, M_FALSE, M_FALSE }
	};

	for (i=0; ends[i].s!=NULL; i++) {
		if (ends[i].cs == M_TRUE) {
			r = M_str_eq_end(ends[i].s, ends[i].e);
		} else {
			r = M_str_caseeq_end(ends[i].s, ends[i].e);
		}
		ck_assert_msg(r == ends[i].r, "%zu: s='%s', e='%s', case-sensitive=%s, match=%s", i, ends[i].s, ends[i].e, ends[i].cs==M_TRUE?"YES":"NO", ends[i].r==M_TRUE?"TRUE":"FALSE");
	}
}
END_TEST

START_TEST(check_str_replace)
{
	char       *s_chr_s;
	char        s_chr_b  = 'a';
	char        s_chr_a  = 'Q';
	const char *s_char_r = "this is Q test of replQcement";

	const char          *s_set_s  = "this is a test of replacement";
	const unsigned char *s_set_cs = (const unsigned char *)"tar";
	const char          *s_set_a  = "zzz";
	const char          *s_set_r  = "zzzhis is zzz zzzeszzz of zzzeplzzzcemenzzz";

	const char *s_str_s = "This is a test of replacement";
	const char *s_str_b = " is ";
	const char *s_str_a = " was ";
	const char *s_str_r = "This was a test of replacement";

	const char *s_str_b2 = "is";
	const char *s_str_a2 = "was";
	const char *s_str_r2 = "Thwas was a test of replacement";

	char *ret;

	s_chr_s = M_strdup("this is a test of replacement");

	ret = M_str_replace_chr(s_chr_s, s_chr_b, s_chr_a);
	ck_assert_msg(M_str_eq(ret, s_char_r), "replace_chr failed: expected '%s' got '%s'", s_char_r, ret);
	M_free(s_chr_s);

	ret = M_strdup_replace_charset(s_set_s, s_set_cs, M_str_len((const char *)s_set_cs), s_set_a);
	ck_assert_msg(M_str_eq(ret, s_set_r), "replace_charset failed: expected '%s' got '%s'", s_set_r, ret);
	M_free(ret);

	ret = M_strdup_replace_str(s_str_s, s_str_b, s_str_a);
	ck_assert_msg(M_str_eq(ret, s_str_r), "replace_set failed: expected '%s' got '%s'", s_str_r, ret);
	M_free(ret);

	ret = M_strdup_replace_str(s_str_s, s_str_b2, s_str_a2);
	ck_assert_msg(M_str_eq(ret, s_str_r2), "replace_set failed: expected '%s' got '%s'", s_str_r2, ret);
	M_free(ret);
}
END_TEST

START_TEST(check_str_unquote)
{
	const char *sg = "\"abc\"";
	const char *sb = "\"";
	const char *sc = "\"abc";
	const char *sd = "abc\"";
	char       *ret;

	ret = M_strdup_unquote(sg, '"', '\\');
	ck_assert_msg(M_str_caseeq(ret, "abc"), "M_strdup_unquote failed: expected '%s' got '%s'", "abc", ret);
	M_free(ret);

	ret = M_strdup_unquote(sb, '"', '\\');
	ck_assert_msg(M_str_caseeq(ret, "\""), "M_strdup_unquote failed: expected '%s' got '%s'", "\"", ret);
	M_free(ret);

	ret = M_strdup_unquote(sc, '"', '\\');
	ck_assert_msg(M_str_caseeq(ret, "\"abc"), "M_strdup_unquote failed: expected '%s' got '%s'", "\"abc", ret);
	M_free(ret);

	ret = M_strdup_unquote(sd, '"', '\\');
	ck_assert_msg(M_str_caseeq(ret, "abc\""), "M_strdup_unquote failed: expected '%s' got '%s'", "abc\"", ret);
	M_free(ret);
}
END_TEST

START_TEST(check_str_justify_center)
{
	char        dest[11] = {0};
	size_t      ret      = 0;
	size_t      just_len;
	const char *expected;


	just_len = sizeof(dest) - 1;

	ret = M_str_justify(dest, sizeof(dest), "abcd", M_STR_JUSTIFY_CENTER, ' ', just_len);
	expected = "   abcd   ";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abc", M_STR_JUSTIFY_CENTER, ' ', just_len);
	expected = "    abc   ";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abcdefghi", M_STR_JUSTIFY_CENTER, ' ', just_len);
	expected = " abcdefghi";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abcdefghij", M_STR_JUSTIFY_CENTER, ' ', just_len);
	expected = "abcdefghij";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);


	just_len = 3;

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abcd", M_STR_JUSTIFY_CENTER, ' ', just_len);
	expected = "bcd";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abcd", M_STR_JUSTIFY_CENTER_TRUNC_RIGHT, ' ', just_len);
	expected = "abc";
	ck_assert_msg(ret == just_len, "M_str_justify failed: expected to return '%d', got '%d'", (int)just_len, (int)ret);
	ck_assert_msg(M_str_eq(expected, dest), "M_str_justify_failed: expected '%s', got '%s'", expected, dest);

	M_mem_set(dest, 0, sizeof(dest));
	ret = M_str_justify(dest, sizeof(dest), "abcd", M_STR_JUSTIFY_CENTER_NO_TRUNC, ' ', just_len);
	ck_assert_msg(ret == 0, "M_str_justify didn't fail, when it should have");
}
END_TEST

START_TEST(check_lower)
{
	char str[256] = {0};

	M_str_cpy(str, sizeof(str), "AbCd EfGh!@#");
	M_str_lower(str);

	ck_assert_msg(M_str_eq(str, "abcd efgh!@#"));
}
END_TEST

START_TEST(check_upper)
{
	char str[256] = {0};

	M_str_cpy(str, sizeof(str), "AbCd EfGh!@#");
	M_str_upper(str);

	ck_assert_msg(M_str_eq(str, "ABCD EFGH!@#"));
}
END_TEST

START_TEST(check_title)
{
	char str[256] = {0};

	M_str_cpy(str, sizeof(str), "AbCd EfGh!@#\tdo\nwhacka");
	M_str_title(str);

	ck_assert_msg(M_str_eq(str, "Abcd Efgh!@#\tDo\nWhacka"));
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* XXX: Add checks for explode, explode_int and strdup */
static Suite *str_suite(void)
{
	Suite *suite = suite_create("str");
	TCase *tc_safe;
	TCase *tc_len;
	TCase *tc_ischarset;
	TCase *tc_isnotcharset;
	TCase *tc_find_charset;
	TCase *tc_split_on_char;
	TCase *tc_explode_lines;
	TCase *tc_implode;
	TCase *tc_implode_int;
	TCase *tc_isempty;
	TCase *tc_str;
	TCase *tc_casestr;
	TCase *tc_cmp;
	TCase *tc_casecmp;
	TCase *tc_cat;
	TCase *tc_delete_spaces;
	TCase *tc_cpy;
	TCase *tc_justify;
	TCase *tc_ends;
	TCase *tc_replace;
	TCase *tc_bracketed;
	TCase *tc_unquote;
	TCase *tc_case_conv;

	tc_safe = tcase_create("str_safe");
	tcase_add_test(tc_safe, check_safe);
	suite_add_tcase(suite, tc_safe);

	tc_len = tcase_create("str_len");
	tcase_add_test(tc_len, check_len);
	tcase_add_test(tc_len, check_len_max);
	suite_add_tcase(suite, tc_len);

	tc_ischarset = tcase_create("str_ischarset");
	tcase_add_test(tc_ischarset, check_ischarset_empty);
	tcase_add_test(tc_ischarset, check_ischarset_single);
	tcase_add_test(tc_ischarset, check_ischarset_multi);
	suite_add_tcase(suite, tc_ischarset);
	
	tc_isnotcharset = tcase_create("str_isnotcharset");
	tcase_add_test(tc_isnotcharset, check_isnotcharset_empty);
	tcase_add_test(tc_isnotcharset, check_isnotcharset_single);
	tcase_add_test(tc_isnotcharset, check_isnotcharset_multi);
	suite_add_tcase(suite, tc_isnotcharset);
	
	tc_find_charset = tcase_create("str_find_charset");
	tcase_add_test(tc_find_charset, check_find_first_from_charset);
	tcase_add_test(tc_find_charset, check_find_first_not_from_charset);
	suite_add_tcase(suite, tc_find_charset);
	
	tc_split_on_char = tcase_create("str_split_on_char");
	tcase_add_test(tc_split_on_char, check_split_on_char_NULL);
	tcase_add_test(tc_split_on_char, check_split_on_char_empty);
	tcase_add_test(tc_split_on_char, check_split_on_char_empty_left_empty_right);
	tcase_add_test(tc_split_on_char, check_split_on_char_left_right);
	tcase_add_test(tc_split_on_char, check_split_on_char_left);
	tcase_add_test(tc_split_on_char, check_split_on_char_right);
	suite_add_tcase(suite, tc_split_on_char);

	tc_explode_lines = tcase_create("str_explode_lines");
	tcase_add_test(tc_explode_lines, check_explode_lines);
	tcase_add_test(tc_explode_lines, check_explode_lines_no_truncate);
	tcase_add_test(tc_explode_lines, check_explode_lines_empty);
	tcase_add_test(tc_explode_lines, check_explode_lines_empty_white);
	tcase_add_test(tc_explode_lines, check_explode_lines_small);
	tcase_add_test(tc_explode_lines, check_explode_str_multi_space);
	tcase_add_test(tc_explode_lines, check_explode_lines_full);
	tcase_add_test(tc_explode_lines, check_explode_lines_full_no_truncate);
	tcase_add_test(tc_explode_lines, check_explode_lines_skip_full);
	tcase_add_test(tc_explode_lines, check_explode_lines_skip_empty);
	suite_add_tcase(suite, tc_explode_lines);
	
	tc_implode = tcase_create("str_implode");
	suite_add_tcase(suite, tc_implode);

	tc_implode_int = tcase_create("str_implode_int");
	tcase_add_test(tc_implode_int, check_implode_int_list);
	suite_add_tcase(suite, tc_implode_int);

	tc_isempty = tcase_create("str_isempty");
	tcase_add_test(tc_isempty, check_isempty_empty);
	tcase_add_test(tc_isempty, check_isempty_nonempty);
	suite_add_tcase(suite, tc_isempty);

	tc_str = tcase_create("str_str");
	tcase_add_test(tc_str, check_str_empty_needle);
	tcase_add_test(tc_str, check_str_empty_haystack);
	tcase_add_test(tc_str, check_str_empty_needle_and_haystack);
	tcase_add_test(tc_str, check_str_found);
	tcase_add_test(tc_str, check_str_notfound);
	suite_add_tcase(suite, tc_str);

	tc_casestr = tcase_create("str_casestr");
	suite_add_tcase(suite, tc_casestr);

	tc_cmp = tcase_create("str_cmp");
	suite_add_tcase(suite, tc_cmp);

	tc_casecmp = tcase_create("str_casecmp");
	suite_add_tcase(suite, tc_casecmp);

	tc_cat = tcase_create("str_cat");
	suite_add_tcase(suite, tc_cat);

	tc_delete_spaces = tcase_create("str_delete_spaces");
	suite_add_tcase(suite, tc_delete_spaces);

	tc_cpy = tcase_create("str_cpy");
	suite_add_tcase(suite, tc_cpy);

	tc_justify = tcase_create("str_justify");
	tcase_add_test(tc_str, check_str_justify_center);
	suite_add_tcase(suite, tc_justify);

	tc_ends = tcase_create("str_ends");
	tcase_add_test(tc_ends, check_str_ends);
	suite_add_tcase(suite, tc_ends);

	tc_replace = tcase_create("str_replace");
	tcase_add_test(tc_replace, check_str_replace);
	suite_add_tcase(suite, tc_replace);

	tc_bracketed = tcase_create("str_bracketed");
	tcase_add_test(tc_bracketed, check_remove_bracketed);
	tcase_add_test(tc_bracketed, check_keep_bracketed);
	tcase_add_test(tc_bracketed, check_remove_bracketed_quoted);
	tcase_add_test(tc_bracketed, check_keep_bracketed_quoted);
	suite_add_tcase(suite, tc_bracketed);
	
	tc_unquote = tcase_create("str_unquote");
	tcase_add_test(tc_unquote, check_str_unquote);
	suite_add_tcase(suite, tc_unquote);

	tc_case_conv = tcase_create("str_case_conv");
	tcase_add_test(tc_case_conv, check_lower);
	tcase_add_test(tc_case_conv, check_upper);
	tcase_add_test(tc_case_conv, check_title);
	suite_add_tcase(suite, tc_case_conv);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int nf;

	(void)argc;
	(void)argv;
	
	sr = srunner_create(str_suite());
	srunner_set_log(sr, "check_str.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
