#include "m_config.h"
#include <check.h>
#include <stdlib.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *round_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint64_round_up_to_nearest_multiple)
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(0,2) == 2);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(1,2) == 2);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(2,2) == 2);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(3,2) == 4);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(4,2) == 4);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(5,2) == 6);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(6,2) == 6);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(7,2) == 8);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(8,2) == 8);
	ck_assert_msg(M_uint64_round_up_to_nearest_multiple(9,2) ==10);
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint32_round_up_to_power_of_two)
{
	M_uint32 n;
	ck_assert_msg(M_uint32_round_up_to_power_of_two(0) == 0);
	ck_assert_msg(M_uint32_round_up_to_power_of_two(1) == 1);
	ck_assert_msg(M_uint32_round_up_to_power_of_two(2) == 2);
	ck_assert_msg(M_uint32_round_up_to_power_of_two(3) == 4);
	for (n=4; n<M_INT32_MAX; n<<=1) {
		ck_assert_msg(M_uint32_round_up_to_power_of_two(n-1) == n);
		ck_assert_msg(M_uint32_round_up_to_power_of_two(n)   == n);
		ck_assert_msg(M_uint32_round_up_to_power_of_two(n+1) == (n<<1));
	}
}
END_TEST

START_TEST(check_size_t_round_up_to_power_of_two)
{
	size_t n;
	ck_assert_msg(M_size_t_round_up_to_power_of_two(0) == 0);
	ck_assert_msg(M_size_t_round_up_to_power_of_two(1) == 1);
	ck_assert_msg(M_size_t_round_up_to_power_of_two(2) == 2);
	ck_assert_msg(M_size_t_round_up_to_power_of_two(3) == 4);
	for (n=4; n<M_INT32_MAX; n<<=1) {
		ck_assert_msg(M_size_t_round_up_to_power_of_two(n-1) == n);
		ck_assert_msg(M_size_t_round_up_to_power_of_two(n)   == n);
		ck_assert_msg(M_size_t_round_up_to_power_of_two(n+1) == (n<<1));
	}
	/* Check for size_t that's larger than an uint32. */
	if (sizeof(size_t) >= sizeof(M_uint64)) {
		for (n=(size_t)M_UINT32_MAX+1; n<M_INT64_MAX; n<<=1) {
			ck_assert_msg(M_size_t_round_up_to_power_of_two(n-1) == n);
			ck_assert_msg(M_size_t_round_up_to_power_of_two(n)   == n);
			ck_assert_msg(M_size_t_round_up_to_power_of_two(n+1) == (n<<1));
		}
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *round_suite(void)
{
	Suite *suite;
	TCase *tc_uint32_round_up_to_power_of_two;
	TCase *tc_size_t_round_up_to_power_of_two;
	TCase *tc_uint64_round_up_to_nearest_multiple;
	   
	suite = suite_create("round");

	tc_uint32_round_up_to_power_of_two = tcase_create("uint32_round_up_to_power_of_two");
	tcase_add_test(tc_uint32_round_up_to_power_of_two, check_uint32_round_up_to_power_of_two);
	suite_add_tcase(suite, tc_uint32_round_up_to_power_of_two);

	tc_size_t_round_up_to_power_of_two = tcase_create("size_t_round_up_to_power_of_two");
	tcase_add_test(tc_size_t_round_up_to_power_of_two, check_size_t_round_up_to_power_of_two);
	suite_add_tcase(suite, tc_size_t_round_up_to_power_of_two);

	tc_uint64_round_up_to_nearest_multiple = tcase_create("uint64_round_up_to_nearest_multiple");
	tcase_add_test(tc_uint64_round_up_to_nearest_multiple, check_uint64_round_up_to_nearest_multiple);
	suite_add_tcase(suite, tc_uint64_round_up_to_nearest_multiple);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(round_suite());
	srunner_set_log(sr, "check_round.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
