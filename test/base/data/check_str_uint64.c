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

/* (-(2**64)).to_s.length => 21 */
#define MAX_ENCODED_LEN 22

extern Suite *str_uint64_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct str_to_uint64_test {
	const char string[MAX_ENCODED_LEN+1];
	const M_uint64 value;
} tests[] = {
	{                   "-1" , M_UINT64_MAX           },
	{                   "-2" , M_UINT64_MAX - 1ULL    },
	{                    "0" ,                0ULL    },
	{                    "1" ,                1ULL    },
	{                "    1" ,                1ULL    },
	{  "9223372036854775807" , M_INT64_MAX            },
	{  "9223372036854775808" , M_INT64_MAX  + 1ULL    },
	{ "-9223372036854775808" , (M_uint64)~M_INT64_MAX },
	{ "18446744073709551615" , M_UINT64_MAX           },
	{ "18446744073709551616" , M_UINT64_MAX           },
	{ "18446744073709551617" , M_UINT64_MAX           },
};
static const struct str_to_uint64_test *uint64_test;

static M_uint64 u64_r;

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_str_to_uint64_null)
{
	u64_r = M_str_to_uint64(NULL);
	ck_assert_msg(u64_r == 0, "decoding of NULL failed: expected %d, but was %"PRIu64"\n", 0, u64_r);
}
END_TEST

START_TEST(check_str_to_uint64)
{
	uint64_test = &tests[_i];

	u64_r = M_str_to_uint64(uint64_test->string);
	ck_assert_msg(u64_r == uint64_test->value, "decoding of \"%s\" failed: expected %"PRIu64", but was %"PRIu64"\n", uint64_test->string, uint64_test->value, u64_r);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *str_uint64_suite(void)
{
	Suite *suite;
	TCase *tc_str_to_uint64;

	suite = suite_create("str_uint64");

	tc_str_to_uint64 = tcase_create("str_to_uint64");
	tcase_add_test(tc_str_to_uint64, check_str_to_uint64_null);
	tcase_add_loop_test(tc_str_to_uint64, check_str_to_uint64, 0, sizeof(tests)/sizeof(*tests));
	suite_add_tcase(suite, tc_str_to_uint64);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(str_uint64_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_str_uint64.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
