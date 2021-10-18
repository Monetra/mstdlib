#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
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
		ck_assert_msg(gmt.yday == i, "Normal time (%"PRId64") yday (%"PRIu64") != expected yday (%"PRId64")", normal, gmt.yday, i);
		normal += sec_in_day;

		M_mem_set(&gmt, 0, sizeof(gmt));
		M_time_togm(leap, &gmt);
		ck_assert_msg(gmt.yday == i, "Leap time (%"PRId64") yday (%"PRId64") != expected yday (%"PRId64")", normal, gmt.yday, i);
		leap += sec_in_day;
	}
	/* Coming out of the loop we have normal = Jan 1 2002, leap = Dec 31 2004 */

	/* Ensure yday wraps when we go into the first day of the next year. */
	ck_assert_msg(normal == 1009861200, "Normal time (%"PRId64") != Jan 1, 2002 05:00:00 (1009861200)", normal);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(normal, &gmt);
	ck_assert_msg(gmt.yday == 0, "Normal time (%"PRId64") yday (%"PRId64") != expected yday (%d)", normal, gmt.yday, 0);

	/* For the leap year ensure we have an additional yday for the last day of year. */
	ck_assert_msg(leap == 1104469200, "Leap time (%"PRId64") != Dec 31, 2004 05:00:00 (1104469200)", leap);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(leap, &gmt);
	ck_assert_msg(gmt.yday == 365, "Leap time (%"PRId64") yday (%"PRId64") != expected yday (%d)", normal, gmt.yday, 365);

	/* For the leap year ensure it also wraps going into the next year */
	leap += sec_in_day;
	ck_assert_msg(leap == 1104555600, "Leap time (%"PRId64") != Jan 1, 2005 05:00:00 (1104555600)", leap);
	M_mem_set(&gmt, 0, sizeof(gmt));
	M_time_togm(leap, &gmt);
	ck_assert_msg(gmt.yday == 0, "Leap time (%"PRId64") yday (%"PRId64") != expected yday (%d)", normal, gmt.yday, 0);
}
END_TEST

START_TEST(check_time_n1)
{
	M_time_localtm_t   ltime;
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;

	tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_ETC, M_TIME_TZ_ALIAS_ALL, M_TIME_TZ_LOAD_LAZY);
	tz  = M_time_tzs_get_tz(tzs, "Etc/GMT");

	M_mem_set(&ltime, 0, sizeof(ltime));
	M_time_tolocal(-1, &ltime, tz);
	ck_assert_msg(ltime.year == 1969, "Year (%"PRId64") != expected year (%d)", ltime.year, 1969);
	ck_assert_msg(ltime.year2 == 69, "Year2 (%"PRId64") != expected year2 (%d)", ltime.year2, 69);

	M_time_tzs_destroy(tzs);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("time_tm");

	tc = tcase_create("time_tm_yday");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_tm_yday);
	suite_add_tcase(suite, tc);

	tc = tcase_create("time_n1");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_n1);
	suite_add_tcase(suite, tc);

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
