#include "m_config.h"
#include <check.h>
#include <stdlib.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * TODO
 *  Check for overflow
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* (2**64).to_s.length => 20 */
#define MAX_ENCODED_LEN 20

extern Suite *str_int64_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct str_to_int64_test {
	const char string[MAX_ENCODED_LEN+1];
	const M_int64 value;
} stests[] = {
	{                    "-1" ,          -1LL },
	{                     "0" ,           0LL },
	{                     "1" ,           1LL },
	{                 "    1" ,           1LL },
	{   "9223372036854775807" ,   M_INT64_MAX },
	{   "9223372036854775808" ,   M_INT64_MAX },
	{   "9223372036854775809" ,   M_INT64_MAX },
	{  "-9223372036854775808" ,   M_INT64_MIN },
	{  "-9223372036854775809" ,   M_INT64_MIN },
	{  "-9223372036854775810" ,   M_INT64_MIN },
	{  "18446744073709551615" ,   M_INT64_MAX },
	{  "18446744073709551616" ,   M_INT64_MAX },
	{  "18446744073709551617" ,   M_INT64_MAX },
	{ "-18446744073709551615" ,   M_INT64_MIN },
	{ "-18446744073709551616" ,   M_INT64_MIN },
};

static const struct str_to_int64_test *test;

static M_int64 s64_r;

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_str_to_int64_null)
{
	s64_r = M_str_to_int64(NULL);
	ck_assert_msg(s64_r == 0, "decoding of NULL failed: expected %d, but was %" PRIu64 "\n", 0, s64_r);
}
END_TEST

START_TEST(check_str_to_int64)
{
	test = &stests[_i];

	s64_r = M_str_to_int64(test->string);
	ck_assert_msg(s64_r == test->value, "decoding of \"%s\" failed: expected %" PRId64 ", but was %" PRId64 "\n", test->string, test->value, s64_r);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *str_int64_suite(void)
{
	Suite *suite ;
	TCase *tc_str_to_int64;

	suite = suite_create("str_int64");

	tc_str_to_int64 = tcase_create("str_to_int64");
	tcase_add_test(tc_str_to_int64, check_str_to_int64_null);
	tcase_add_loop_test(tc_str_to_int64, check_str_to_int64,  0, sizeof(stests)/sizeof(*stests));
	suite_add_tcase(suite, tc_str_to_int64);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(str_int64_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_str_int64.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
