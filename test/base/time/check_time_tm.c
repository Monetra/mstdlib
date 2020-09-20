#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const M_time_t sec_in_day = 86400;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_time_tm_yday)
{
	M_int64 i;

	M_time_t      normal = 978325200;  /* Jan 1, 2001 05:00:00 GMT */
	M_time_t      leap   = 1072933200; /* Jan 1, 2004 05:00:00 GMT */
	M_time_gmtm_t gmt;

	for (i=0; i<365; i++) {
		M_mem_set(&gmt, 0, sizeof(gmt));
		M_time_togm(normal, &gmt);
		ck_assert_msg(gmt.yday == i, "Normal time (%lld) yday (%llu) != expected yday (%lld)", normal, gmt.yday, i);
		normal += sec_in_day;

		M_mem_set(&gmt, 0, sizeof(gmt));
		M_time_togm(leap, &gmt);
		ck_assert_msg(gmt.yday == i, "Leap time (%lld) yday (%lld) != expected yday (%lld)", normal, gmt.yday, i);
		leap += sec_in_day;
	}
	/* Coming out of the loop we have normal = Jan 1 2002, leap = Dec 31 2004 */

	/* Ensure yday wraps when we go into the first day of the next year. */
	ck_assert_msg(normal == 1009861200, "Normal time (%lld) != Jan 1, 2002 05:00:00 (1009861200)", normal);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(normal, &gmt);
	ck_assert_msg(gmt.yday == 0, "Normal time (%lld) yday (%lld) != expected yday (%d)", normal, gmt.yday, 0);

	/* For the leap year ensure we have an additional yday for the last day of year. */
	ck_assert_msg(leap == 1104469200, "Leap time (%lld) != Dec 31, 2004 05:00:00 (1104469200)", leap);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(leap, &gmt);
	ck_assert_msg(gmt.yday == 365, "Leap time (%lld) yday (%lld) != expected yday (%d)", normal, gmt.yday, 365);

	/* For the leap year ensure it also wraps going into the next year */
	leap += sec_in_day;
	ck_assert_msg(leap == 1104555600, "Leap time (%lld) != Jan 1, 2005 05:00:00 (1104555600)", leap);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(leap, &gmt);
	ck_assert_msg(gmt.yday == 0, "Leap time (%lld) yday (%lld) != expected yday (%d)", normal, gmt.yday, 0);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *suite(void)
{
	Suite *suite;
	TCase *tc_time_tm_yday;

	suite = suite_create("time_tm");

	tc_time_tm_yday = tcase_create("time_tm_yday");
	tcase_add_unchecked_fixture(tc_time_tm_yday, NULL, NULL);
	tcase_add_test(tc_time_tm_yday, check_time_tm_yday);
	suite_add_tcase(suite, tc_time_tm_yday);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_time_tm.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
