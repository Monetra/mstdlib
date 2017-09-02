#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

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
	time_t            timestamp;

	M_mem_set(&ltime, 0, sizeof(ltime));

	/* Convert to a local time. */
	M_time_tolocal(tz_check->utc, &ltime, tz);

	/* Check adjustment. */
	if (tz_check->gmtoff != ltime.gmtoff) {
		M_snprintf(err, err_len, "Expected offset %lld does not match offset %lld", (M_int64)tz_check->gmtoff, (M_int64)ltime.gmtoff);
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

	if (tz_check->utc != timestamp) {
		M_snprintf(err, err_len, "Expected UTC time %lld does not match calculated time of %lld", (M_int64)tz_check->utc, (M_int64)timestamp);
		return M_FALSE;
	}

	return M_TRUE;
}

static void check_tz_run_checks(const M_time_tz_t *tz, const char *prefix, const check_tz_time_t *tz_checks)
{
	char   err[256];
	size_t i;

	for (i=0; tz_checks[i].utc != 0; i++) {
		if (!check_time_tz_int(&tz_checks[i], tz, err, sizeof(err))) {
			ck_abort_msg("%s check %zu failed: %s", prefix, i, err);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_mtzfile)
{
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;
	char              *err_sect;
	char              *err_data;
	size_t             err_line = 0;

	tzs = M_time_tzs_create();
	if (M_mtzfile_tzs_add_str(tzs, POSIXEX_INI, &err_line, &err_sect, &err_data) != M_TIME_RESULT_SUCCESS) {
		ck_abort_msg("Error loading mtzfile ini data: line=%zu, sect=%s, data=%s", err_line, err_sect, err_data);
	}

	tz  = M_time_tzs_get_tz(tzs, "EST5EDT");
	ck_assert_msg(tz != NULL, "Could not get tz data");
	check_tz_run_checks(tz, "mtzfile-ny", check_tz_times_ny);

	tz  = M_time_tzs_get_tz(tzs, "America/Los_Angeles");
	ck_assert_msg(tz != NULL, "Could not get tz data");
	check_tz_run_checks(tz, "mtzfile-la", check_tz_times_la);

	M_time_tzs_destroy(tzs);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_time_tz_suite(void)
{
	Suite *suite;
	TCase *tc_mtzfile;

	suite = suite_create("mtzfile");

	tc_mtzfile = tcase_create("mtzfile");
	tcase_add_unchecked_fixture(tc_mtzfile, NULL, NULL);
	tcase_add_test(tc_mtzfile, check_mtzfile);
	suite_add_tcase(suite, tc_mtzfile);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_time_tz_suite());
	srunner_set_log(sr, "check_mtzfile.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
