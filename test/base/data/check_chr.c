#include "m_config.h"
#include <stdlib.h>
#include <ctype.h>
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *chr_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CONTROL           "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x7F"
#define CONTROL_LEN       (sizeof(CONTROL)-1)

#define XDIGIT_LOWER      "abcdef"
#define XDIGIT_LOWER_LEN  (sizeof(XDIGIT_LOWER)-1)

#define XDIGIT_UPPER      "ABCDEF"
#define XDIGIT_UPPER_LEN  (sizeof(XDIGIT_UPPER)-1)

#define ALPHA_LOWER       "abcdefghijklmnopqrstuvwxyz"
#define ALPHA_LOWER_LEN   (sizeof(ALPHA_LOWER)-1)

#define ALPHA_UPPER       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALPHA_UPPER_LEN   (sizeof(ALPHA_UPPER)-1)

#define ALPHA             ALPHA_LOWER ALPHA_UPPER
#define ALPHA_LEN         (sizeof(ALPHA)-1)

#define DIGIT             "0123456789"
#define DIGIT_LEN         (sizeof(DIGIT)-1)

#define ALNUM             ALPHA DIGIT
#define ALNUM_LEN         (sizeof(ALNUM)-1)

#define XDIGIT            DIGIT XDIGIT_LOWER XDIGIT_UPPER
#define XDIGIT_LEN        (sizeof(XDIGIT)-1)

#define BLANK             " \t"
#define BLANK_LEN         (sizeof(BLANK)-1)

#define SPACE             BLANK "\f\n\r\v"
#define SPACE_LEN         (sizeof(SPACE)-1)

#define PUNCT             "!\"#$%&'()*+,-./" ":;<=>?@" "[\\]^_`" "{|}~"
#define PUNCT_LEN         (sizeof(PUNCT)-1)

#define GRAPH             DIGIT ALPHA PUNCT
#define GRAPH_LEN         (sizeof(GRAPH)-1)

#define PRINT             GRAPH SPACE
#define PRINT_LEN         (sizeof(PRINT)-1)

#define ASCII             CONTROL SPACE GRAPH
#define ASCII_LEN         (sizeof(ASCII)-1)

typedef M_bool (*isvalid_func)(char c);
typedef int    (*c_isvalid_func)(int c);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int r;
static ssize_t idx1;
static ssize_t idx2;

static ssize_t set_index_of(const void *set, size_t set_size, char v)
{
	const char *p = M_mem_mem(set, set_size, (void *)&v, sizeof(v));
	return p == NULL ? -1 : p - (const char *)set;
}

static M_bool set_contains(const void *set, size_t set_size, char v)
{
	return set_index_of(set, set_size, v) >= 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_loop(isvalid_func isvalid, c_isvalid_func c_isvalid, const char *set, size_t set_len)
{
	M_int32 i;
	char    c;
	M_bool  ret_isvalid;
	M_bool  ret_c_isvalid;
	M_bool  ret_set_contains;

	for (i=CHAR_MIN; i<=CHAR_MAX; i++) {
		c = (char)i;
		ret_isvalid      = isvalid(c);
		ret_set_contains = set_contains(set,set_len,c);
		ck_assert_msg(ret_isvalid == ret_set_contains, "character %c (%d) is not valid: isvalid=%s, set_contains=%s", c, i, ret_isvalid?"TRUE":"FALSE", ret_set_contains?"TRUE":"FALSE");

		if (c_isvalid != NULL) {
			ret_c_isvalid = (c_isvalid(i) != 0)?M_TRUE:M_FALSE;
			ck_assert_msg(ret_isvalid == ret_c_isvalid, "character %c (%d) is not valid: isvalid=%s, c_isvalid=%s", c, i, ret_isvalid?"TRUE":"FALSE", ret_c_isvalid?"TRUE":"FALSE");
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_isalnum)
{
	check_loop(M_chr_isalnum, isalnum, ALNUM, ALNUM_LEN);
}
END_TEST

START_TEST(check_isalpha)
{
	check_loop(M_chr_isalpha, isalpha, ALPHA, ALPHA_LEN);
}
END_TEST

START_TEST(check_isascii)
{
	check_loop(M_chr_isascii, NULL, ASCII, ASCII_LEN);
}
END_TEST

START_TEST(check_iscntrl)
{
	check_loop(M_chr_iscntrl, iscntrl, CONTROL, CONTROL_LEN);
}
END_TEST

START_TEST(check_isdigit)
{
	check_loop(M_chr_isdigit, isdigit, DIGIT, DIGIT_LEN);
}
END_TEST

START_TEST(check_isgraph)
{
	check_loop(M_chr_isgraph, isgraph, GRAPH, GRAPH_LEN);
}
END_TEST

START_TEST(check_islower)
{
	check_loop(M_chr_islower, islower, ALPHA_LOWER, ALPHA_LOWER_LEN);
}
END_TEST

START_TEST(check_isupper)
{
	check_loop(M_chr_isupper, isupper, ALPHA_UPPER, ALPHA_UPPER_LEN);
}
END_TEST

START_TEST(check_isprint)
{
	/* M_chr_isprint differes from isprint. We include more characters such as \t, \n \r as printable. */
	check_loop(M_chr_isprint, NULL, PRINT, PRINT_LEN);
}
END_TEST

START_TEST(check_ispunct)
{
	check_loop(M_chr_ispunct, ispunct, PUNCT, PUNCT_LEN);
}
END_TEST

START_TEST(check_isspace)
{
	check_loop(M_chr_isspace, isspace, SPACE, SPACE_LEN);
}
END_TEST

START_TEST(check_ishex)
{
	check_loop(M_chr_ishex, NULL, XDIGIT, XDIGIT_LEN);
}
END_TEST

START_TEST(check_tolower)
{
	M_int32 i;
	char    c;

	for (i=CHAR_MIN; i<=CHAR_MAX; i++) {
		c = (char)i;
		/* tolower(upper) == lower */
		if (M_chr_isupper(c)) {
			ck_assert_msg(M_chr_islower(M_chr_tolower(c)));

			/* ensure lowercase is mapped to appropriate uppercase */
			idx1 = set_index_of(ALPHA_UPPER,ALPHA_UPPER_LEN,c);
			idx2 = set_index_of(ALPHA_LOWER,ALPHA_LOWER_LEN,M_chr_tolower(c));
			ck_assert_msg(idx1 >= 0);
			ck_assert_msg(idx2 >= 0);
			ck_assert_msg(idx1 == idx2);
		} else {
			/* tolower(*) == * */
			ck_assert_msg(c == M_chr_tolower(c));
		}
	}
}
END_TEST

START_TEST(check_toupper)
{
	M_int32 i;
	char    c;

	for (i=CHAR_MIN; i<=CHAR_MAX; i++) {
		c = (char)i;
		if (M_chr_islower(c)) {
			/* toupper(lower) == upper */
			ck_assert_msg(M_chr_isupper(M_chr_toupper(c)));
			/* ensure uppercase is mapped to appropriate lowercase */
			idx1 = set_index_of(ALPHA_LOWER,ALPHA_LOWER_LEN,c);
			idx2 = set_index_of(ALPHA_UPPER,ALPHA_UPPER_LEN,M_chr_toupper(c));
			ck_assert_msg(idx1 >= 0);
			ck_assert_msg(idx2 >= 0);
			ck_assert_msg(idx1 == idx2);
		} else {
			/* toupper(*) == * */
			ck_assert_msg(c == M_chr_toupper(c));
		}
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_digit)
{
	char c = (char)_i;
	r = M_chr_digit(c);

	if (M_chr_isdigit(c)) {
		ck_assert_msg(r >= 0);
		ck_assert_msg(r <= 9);

		ck_assert_msg(DIGIT[r] == c);
	} else {
		ck_assert_msg(M_chr_digit(c) == -1);
	}
}
END_TEST

START_TEST(check_xdigit)
{
	char c = (char)_i;
	r = M_chr_xdigit(c);

	if (M_chr_ishex(c)) {
		ck_assert_msg(r >= 0);
		ck_assert_msg(r <= 0xf);

		ck_assert_msg(M_chr_tolower(XDIGIT[r]) == M_chr_tolower(c));
	} else {
		ck_assert_msg(M_chr_xdigit(c) == -1);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *chr_suite(void)
{
	Suite *suite;
	TCase *tc_isalnum;
	TCase *tc_isalpha;
	TCase *tc_isascii;
	TCase *tc_iscntrl;
	TCase *tc_isdigit;
	TCase *tc_isgraph;
	TCase *tc_islower;
	TCase *tc_isupper;
	TCase *tc_isprint;
	TCase *tc_ispunct;
	TCase *tc_isspace;
	TCase *tc_ishex;
	TCase *tc_tolower;
	TCase *tc_toupper;
	TCase *tc_digit;
	TCase *tc_xdigit;

	suite = suite_create("chr");

	tc_isalnum = tcase_create("chr_isalnum");
	tcase_add_test(tc_isalnum, check_isalnum);
	suite_add_tcase(suite, tc_isalnum);

	tc_isalpha = tcase_create("chr_isalpha");
	tcase_add_test(tc_isalpha, check_isalpha);
	suite_add_tcase(suite, tc_isalpha);

	tc_isascii = tcase_create("chr_isascii");
	tcase_add_test(tc_isascii, check_isascii);
	suite_add_tcase(suite, tc_isascii);

	tc_iscntrl = tcase_create("chr_iscntrl");
	tcase_add_test(tc_iscntrl, check_iscntrl);
	suite_add_tcase(suite, tc_iscntrl);

	tc_isdigit = tcase_create("chr_isdigit");
	tcase_add_test(tc_isdigit, check_isdigit);
	suite_add_tcase(suite, tc_isdigit);

	tc_isgraph = tcase_create("chr_isgraph");
	tcase_add_test(tc_isgraph, check_isgraph);
	suite_add_tcase(suite, tc_isgraph);

	tc_islower = tcase_create("chr_islower");
	tcase_add_test(tc_islower, check_islower);
	suite_add_tcase(suite, tc_islower);

	tc_isupper = tcase_create("chr_isupper");
	tcase_add_test(tc_isupper, check_isupper);
	suite_add_tcase(suite, tc_isupper);

	tc_isprint = tcase_create("chr_isprint");
	tcase_add_test(tc_isprint, check_isprint);
	suite_add_tcase(suite, tc_isprint);

	tc_ispunct = tcase_create("chr_ispunct");
	tcase_add_test(tc_ispunct, check_ispunct);
	suite_add_tcase(suite, tc_ispunct);

	tc_isspace = tcase_create("chr_isspace");
	tcase_add_test(tc_isspace, check_isspace);
	suite_add_tcase(suite, tc_isspace);

	tc_ishex = tcase_create("chr_ishex");
	tcase_add_test(tc_ishex, check_ishex);
	suite_add_tcase(suite, tc_ishex);

	tc_tolower = tcase_create("chr_tolower");
	tcase_add_test(tc_tolower, check_tolower);
	suite_add_tcase(suite, tc_tolower);

	tc_toupper = tcase_create("chr_toupper");
	tcase_add_test(tc_toupper, check_toupper);
	suite_add_tcase(suite, tc_toupper);

	tc_digit = tcase_create("chr_digit");
	tcase_add_test(tc_digit, check_digit);
	suite_add_tcase(suite, tc_digit);

	tc_xdigit = tcase_create("chr_xdigit");
	tcase_add_test(tc_xdigit, check_xdigit);
	suite_add_tcase(suite, tc_xdigit);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(chr_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_chr.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
