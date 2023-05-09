#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

typedef unsigned long long llu;
typedef          long long lld;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define EST5EDT "EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00"
#define PST8PDT "PST8PDT,M3.2.0/02:00:00,M11.1.0/02:00:00"

#define POSIXEX_INI "[EST5EDT]\n" \
	"alias=America/New_York\n" \
	"offset=5\n" \
	"offset_dst=4\n" \
	"abbr=EST\n" \
	"abbr_dst=EDT\n" \
	"dst=2007;M3.2.0/02:00:00,M11.1.0/02:00:00\n" \
	"[PST8PDT]\n" \
	"alias=America/Los_Angeles\n" \
	"offset=8\n" \
	"offset_dst=7\n" \
	"abbr=PST\n" \
	"abbr_dst=PDT\n" \
	"dst=2007;M3.2.0/02:00:00,M11.1.0/02:00:00"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_time_tz_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	time_t  utc;
	time_t  gmtoff;
	M_bool  isdst;
	M_int64 lyear;
	M_int64 lmon;
	M_int64 lday;
	M_int64 lhour;
	M_int64 lmin;
	M_int64 lsec;
} check_tz_time_t;

static check_tz_time_t check_tz_times_ny[] = {
	/* DST on/off. */
	{ 1464900596, -14400, M_TRUE,  2016,  6, 2,  16, 49, 56 }, 
	{ 1375277153, -14400, M_TRUE,  2013,  7, 31,  9, 25, 53 }, 
	{ 1359638780, -18000, M_FALSE, 2013,  1, 31,  8, 26, 20 },
	{ 1362900611, -14400, M_TRUE,  2013,  3, 10,  3, 30, 11 },
	/* DST Fallback checks. */
	{ 1383451211, -14400, M_TRUE,  2013, 11,  3,  0,  0, 11 },
	{ 1383453011, -14400, M_TRUE,  2013, 11,  3,  0, 30, 11 },
	{ 1383454811, -14400, M_TRUE,  2013, 11,  3,  1,  0, 11 },
	{ 1383456611, -14400, M_TRUE,  2013, 11,  3,  1, 30, 11 },
#ifndef _WIN32
	/* Unix converts these to 1 AM EST. Windows converts these
 	 * to 1 AM DST. Due to the overlap there are two 1 AMs.
	 * Unix is doing the right thing because it is differentiating
	 * between EST and DST 1AMs but Windows doesn't make that
	 * distinction using their from local function calls. */
	{ 1383458411, -18000, M_FALSE, 2013, 11,  3,  1,  0, 11 },
	{ 1383460211, -18000, M_FALSE, 2013, 11,  3,  1, 30, 11 },
#endif
	{ 1383462011, -18000, M_FALSE, 2013, 11,  3,  2,  0, 11 },
	{ 1383463811, -18000, M_FALSE, 2013, 11,  3,  2, 30, 11 },
	{ 0,               0, M_FALSE,    0,  0,  0,  0,  0,  0 }
};

static check_tz_time_t check_tz_times_la[] = {
	/* DST on/off. */
	{ 1375277153, -25200, M_TRUE,  2013,  7, 31,  6, 25, 53 }, 
	{ 1359638780, -28800, M_FALSE, 2013,  1, 31,  5, 26, 20 },
	{ 1362911411, -25200, M_TRUE,  2013,  3, 10,  3, 30, 11 },
	/* DST Fallback checks. */
	{ 1383462011, -25200, M_TRUE,  2013, 11,  3,  0,  0, 11 },
	{ 1383463811, -25200, M_TRUE,  2013, 11,  3,  0, 30, 11 },
	{ 1383465611, -25200, M_TRUE,  2013, 11,  3,  1,  0, 11 },
	{ 1383467411, -25200, M_TRUE,  2013, 11,  3,  1, 30, 11 },
	{ 1383469211, -28800, M_FALSE, 2013, 11,  3,  1,  0, 11 },
	{ 1383471011, -28800, M_FALSE, 2013, 11,  3,  1, 30, 11 },
	{ 1383472811, -28800, M_FALSE, 2013, 11,  3,  2,  0, 11 },
	{ 1383474611, -28800, M_FALSE, 2013, 11,  3,  2, 30, 11 },
	{ 0,               0, M_FALSE,    0,  0,  0,  0,  0,  0 }
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool check_time_tz_int(const check_tz_time_t *tz_check, const M_time_tz_t *tz, char *err, size_t err_len)
{
	M_bool            isdst;
	M_time_localtm_t  ltime;
	M_time_t          timestamp;

	M_mem_set(&ltime, 0, sizeof(ltime));

	/* Convert to a local time. */
	M_time_tolocal(tz_check->utc, &ltime, tz);

	/* Check adjustment. */
	if (tz_check->gmtoff != ltime.gmtoff) {
		M_snprintf(err, err_len, "Expected offset %lld does not match offset %lld", (lld)tz_check->gmtoff, (lld)ltime.gmtoff);
		return M_FALSE;
	}

	if (ltime.isdst == 1) {
		isdst = M_TRUE;
	} else if (ltime.isdst == 0) {
		isdst = M_FALSE;
	} else {
		M_snprintf(err, err_len, "Could not determine wheter DST is in effect");
		return M_FALSE;
	}
	if (tz_check->isdst != isdst) {
		M_snprintf(err, err_len, "Expected DST %s does not match %s", tz_check->isdst?"ON":"OFF", isdst?"ON":"OFF");
		return M_FALSE;
	}

	if (tz_check->lyear != ltime.year  ||
		tz_check->lmon  != ltime.month ||
		tz_check->lday  != ltime.day   ||
		tz_check->lhour != ltime.hour  ||
		tz_check->lmin  != ltime.min   ||
		tz_check->lsec  != ltime.sec)
	{
		M_snprintf(err, err_len, "Expected date/time y=%lld m=%lld d=%lld %lld:%lld:%lld does not match y=%lld m=%lld d=%lld %lld:%lld:%lld", 
			tz_check->lyear, tz_check->lmon, tz_check->lday, tz_check->lhour, tz_check->lmin, tz_check->lsec,
			ltime.year, ltime.month, ltime.day, ltime.hour, ltime.min, ltime.sec);
		return M_FALSE;
	}

	/* Convert back to a UTC time. */
	timestamp = M_time_fromlocal(&ltime, tz);

	if (((M_time_t)tz_check->utc) != timestamp) {
		M_snprintf(err, err_len, "Expected UTC time %lld does not match calculated time of %lld", (lld)tz_check->utc, (lld)timestamp);
		return M_FALSE;
	}

	return M_TRUE;
}

static void check_tz_run_checks(const M_time_tz_t *tz, const char *prefix, const check_tz_time_t *tz_checks)
{
	char   err[256] = {0};
	size_t i;

	for (i=0; tz_checks[i].utc != 0; i++) {
		if (!check_time_tz_int(&tz_checks[i], tz, err, sizeof(err))) {
			ck_abort_msg("%s check %llu failed: %s", prefix, (llu)i, err);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_time_tz_posix)
{
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;

	tzs = M_time_tzs_create();
	M_time_tzs_add_posix_str(tzs, EST5EDT);
	M_time_tzs_add_posix_str(tzs, PST8PDT);

	tz  = M_time_tzs_get_tz(tzs, "EST5EDT");
	ck_assert_msg(tz != NULL, "Could not get tz data EST");

	check_tz_run_checks(tz, "posix-ny", check_tz_times_ny);

	tz  = M_time_tzs_get_tz(tzs, "PST8PDT");
	ck_assert_msg(tz != NULL, "Could not get tz data PST");
	check_tz_run_checks(tz, "posix-la", check_tz_times_la);

	M_time_tzs_destroy(tzs);
}
END_TEST

#ifndef _WIN32
START_TEST(check_time_tz_olson)
{
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;

	tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_AMERICA, M_TIME_TZ_ALIAS_OLSON_MAIN, M_TIME_TZ_LOAD_LAZY);

	tz  = M_time_tzs_get_tz(tzs, "America/New_York");
	ck_assert_msg(tz != NULL, "Could not get tz data");

	check_tz_run_checks(tz, "olson-ny", check_tz_times_ny);

	tz  = M_time_tzs_get_tz(tzs, "America/Los_Angeles");
	ck_assert_msg(tz != NULL, "Could not get tz data");
	check_tz_run_checks(tz, "olson-la", check_tz_times_la);

	M_time_tzs_destroy(tzs);
}
END_TEST
#endif

START_TEST(check_time_tz_sys_convert)
{
	M_time_localtm_t ltime;
	M_time_t         t;
	size_t           i;

	for (i=0; check_tz_times_ny[i].utc != 0; i++) {
		M_mem_set(&ltime, 0, sizeof(ltime));
		M_time_tolocal(check_tz_times_ny[i].utc, &ltime, NULL);
		t = M_time_fromlocal(&ltime, NULL);
		ck_assert_msg(((M_time_t)check_tz_times_ny[i].utc) == t, "%llu: expected=%lld, got=%lld", (llu)i, (lld)check_tz_times_ny[i].utc, (lld)t);
	}

	M_mem_set(&ltime, 0, sizeof(ltime));
	M_time_tolocal(0, &ltime, NULL);
	t = M_time_fromlocal(&ltime, NULL);
	ck_assert_msg(0 == t, "expected=0, got=%lld", (lld)t);
}
END_TEST

START_TEST(check_time_tz_sys_vs_lib)
{
	const M_time_t test_times[] = { 1678510800, 1680148800, 0 };
	size_t         i;
	M_time_tzs_t  *tzs          = NULL;

	M_time_tzs_load(&tzs, M_TIME_TZ_ZONE_ALL, M_TIME_TZ_ALIAS_ALL, M_TIME_TZ_LOAD_LAZY);

	for (i=0; test_times[i] != 0; i++) {
		M_time_localtm_t   sys_ltime;
		M_time_localtm_t   lib_ltime;
		char              *sys_date;
		char              *lib_date;
		const M_time_tz_t *tz;

		/* Using system conversion */
		M_time_tolocal(test_times[i], &sys_ltime, NULL);

		/* Extract time zone from system conversion to look it up */
		tz = M_time_tzs_get_tz(tzs, sys_ltime.abbr);

		ck_assert_msg(tz == NULL, "%llu: timezone %s not found", (llu)i, sys_ltime.abbr);

		/* Transform using our own tz database */
		M_time_tolocal(test_times[i], &lib_ltime, tz);

		/* Compare system vs lib */
		sys_date = M_time_to_str("%Y-%m-%d %H:%M:%S %z", &sys_ltime);
		lib_date = M_time_to_str("%Y-%m-%d %H:%M:%S %z", &lib_ltime);
		ck_assert_msg(M_str_eq(sys_date, lib_date), "%llu: sys_date %s != lib_date %s for ts %llu TZ %s", (llu)i, sys_date, lib_date, test_times[i], sys_ltime.abbr);
		M_free(sys_date);
		M_free(lib_date);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_time_tz_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("time_tz");

	tc = tcase_create("time_tz_posix");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_tz_posix);
	suite_add_tcase(suite, tc);

#ifndef _WIN32
	tc = tcase_create("time_tz_olson");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_tz_olson);
	suite_add_tcase(suite, tc);
#endif

	tc = tcase_create("time_tz_sys_convert");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_tz_sys_convert);
	suite_add_tcase(suite, tc);

	tc = tcase_create("time_tz_sys_vs_lib");
	tcase_add_unchecked_fixture(tc, NULL, NULL);
	tcase_add_test(tc, check_time_tz_sys_vs_lib);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_time_tz_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_time_tz.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
