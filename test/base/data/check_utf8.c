#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>

/* Some tests are from
 * Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> - 2015-08-28 - CC BY 4.0
 * This license is for the test data used from there only.
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_utf8_correct)
{
	const char     *str   = "κόσμε";
	const char     *next  = str;
	M_buf_t        *buf;
	char           *out;
	M_uint32        cps[]   = { 0x03BA, 0x1F79, 0x03C3, 0x03BC, 0x03B5, 0 };
	unsigned char   bytes[] = { 0xCE, 0xBA, 0xE1, 0xBD, 0xB9, 0xCF, 0x83, 0xCE, 0xBC, 0xCE, 0xB5, 0 };
	M_uint32        cp;
	size_t          len;
	size_t          out_len;
	size_t          cnt;
	size_t          i;
	M_utf8_error_t  res;

	len = M_str_len(str);
	cnt = M_utf8_cnt(str);
	ck_assert_msg(len != cnt, "Length (%zu) should not equal count (%zu)", len, cnt);
	ck_assert_msg(cnt == 5, "count != 5");

	buf = M_buf_create();
	for (i=0; i<cnt; i++) {
		res = M_utf8_get_cp(next, &cp, &next);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: get cp failed: %d", res);
		ck_assert_msg(cp == cps[i], "%zu: cp failed: expected '%x', got '%x'", i, cps[i], cp);
		M_utf8_from_cp_buf(buf, cp);
	}

	out = M_buf_finish_str(buf, &out_len);
	ck_assert_msg(len == out_len, "Length (%zu) should equal out length (%zu)", len, out_len);
	ck_assert_msg(M_str_eq(out, str), "str != out: expected '%s', got '%s'", str, out); 
	ck_assert_msg(M_str_eq(out, (const char *)bytes), "bytes != out");

	M_free(out);
}
END_TEST

START_TEST(check_utf8_case_cp)
{
	M_uint32 upper_cp[] = { 0x004B, 0x00C2, 0x0158, 0x015A, 0x017D, 0x0204, 0x0220, 0x0243, 0x040F, 0x0414, 0x0415,
		0x04D8, 0x050A, 0x13A2, 0x1C93, 0x1CAB, 0x1EA0, 0x1F6C, 0x24CD, 0x2CD0, 0xA7A4, 0x10411, 0x10427, 0x104BE,
		0x118AB, 0x118AC, 0x1E920, 0x1E921, 0x16E4F };
	M_uint32 lower_cp[] = { 0x006B, 0x00E2, 0x0159, 0x015B, 0x017E, 0x0205, 0x019E, 0x0180, 0x045F, 0x0434, 0x0435,
		0x04D9, 0x050B, 0xAB72, 0x10D3, 0x10EB, 0x1EA1, 0x1F64, 0x24E7, 0x2CD1, 0xA7A5, 0x10439, 0x1044F, 0x104E6,
		0x118CB, 0x118CC, 0x1E942, 0x1E943, 0x16E6F };
	M_uint32 same_cp[]  = { 0x0012, 0x0221, 0x1053, 0x1111, 0x207E };
	M_uint32 cp;
	size_t   i;
	M_utf8_error_t res;

	for (i=0; i<sizeof(upper_cp)/sizeof(*upper_cp); i++) {
		res = M_utf8_cp_toupper(upper_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: upper cp to upper cp failed: %d\n", i, res);
		ck_assert_msg(upper_cp[i] == cp, "%zu: upper cp != cp: expected %04X, got %04X", i, upper_cp[i], cp);

		res = M_utf8_cp_tolower(upper_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: upper cp to lower cp failed: %d\n", i, res);
		ck_assert_msg(lower_cp[i] == cp, "%zu: upper cp != cp: expected %04X, got %04X", i, lower_cp[i], cp);

		res = M_utf8_cp_tolower(lower_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: lower cp to lower cp failed: %d\n", i, res);
		ck_assert_msg(lower_cp[i] == cp, "%zu: lower cp != cp: expected %04X, got %04X", i, lower_cp[i], cp);

		res = M_utf8_cp_toupper(lower_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: lower cp to upper cp failed: %d\n", i, res);
		ck_assert_msg(upper_cp[i] == cp, "%zu: upper cp != cp: expected %04X, got %04X", i, upper_cp[i], cp);
	}

	for (i=0; i<sizeof(same_cp)/sizeof(*same_cp); i++) {
		res = M_utf8_cp_toupper(same_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: same cp to upper cp failed: %d\n", i, res);
		ck_assert_msg(same_cp[i] == cp, "%zu: same cp != to upper cp: expected %04X, got %04X", i, same_cp[i], cp);

		res = M_utf8_cp_tolower(same_cp[i], &cp);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: same cp to lower cp failed: %d\n", i, res);
		ck_assert_msg(same_cp[i] == cp, "%zu: same cp != to lower cp: expected %04X, got %04X", i, same_cp[i], cp);
	}
}
END_TEST

START_TEST(check_utf8_case)
{
	char *out;
	struct {
		const char *upper;
		const char *lower;
	} ts[] = {
		/* upper/lower cp. */
		{ "\x4B" "\xC3" "\x82" "\xC5" "\x98" "\xC5" "\x9A" "\xC5" "\xBD" "\xC8" "\x84" "\xC8" "\xA0" "\xC9" "\x83" "\xD0" "\x8F" "\xD0" "\x94" "\xD0" "\x95" "\xD3" "\x98" "\xD4" "\x8A" "\xE1" "\x8E" "\xA2" "\xE1" "\xB2" "\x93" "\xE1" "\xB2" "\xAB" "\xE1" "\xBA" "\xA0" "\xE1" "\xBD" "\xAC" "\xE2" "\x93" "\x8D" "\xE2" "\xB3" "\x90" "\xEA" "\x9E" "\xA4" "\xF0" "\x90" "\x90" "\x91" "\xF0" "\x90" "\x90" "\xA7" "\xF0" "\x90" "\x92" "\xBE" "\xF0" "\x91" "\xA2" "\xAB" "\xF0" "\x91" "\xA2" "\xAC" "\xF0" "\x9E" "\xA4" "\xA0" "\xF0" "\x9E" "\xA4" "\xA1" "\xF0" "\x96" "\xB9" "\x8F",
			"\x6B" "\xC3" "\xA2" "\xC5" "\x99" "\xC5" "\x9B" "\xC5" "\xBE" "\xC8" "\x85" "\xC6" "\x9E" "\xC6" "\x80" "\xD1" "\x9F" "\xD0" "\xB4" "\xD0" "\xB5" "\xD3" "\x99" "\xD4" "\x8B" "\xEA" "\xAD" "\xB2" "\xE1" "\x83" "\x93" "\xE1" "\x83" "\xAB" "\xE1" "\xBA" "\xA1" "\xE1" "\xBD" "\xA4" "\xE2" "\x93" "\xA7" "\xE2" "\xB3" "\x91" "\xEA" "\x9E" "\xA5" "\xF0" "\x90" "\x90" "\xB9" "\xF0" "\x90" "\x91" "\x8F" "\xF0" "\x90" "\x93" "\xA6" "\xF0" "\x91" "\xA3" "\x8B" "\xF0" "\x91" "\xA3" "\x8C" "\xF0" "\x9E" "\xA5" "\x82" "\xF0" "\x9E" "\xA5" "\x83" "\xF0" "\x96" "\xB9" "\xAF" },
		/* Same cp. No upper / lower mapping. */
		{ "\x12" "\xC8" "\xA1" "\xE1" "\x81" "\x93" "\xE1" "\x84" "\x91" "\xE2" "\x81" "\xBE",
			"\x12" "\xC8" "\xA1" "\xE1" "\x81" "\x93" "\xE1" "\x84" "\x91" "\xE2" "\x81" "\xBE" },
		/* Ascii. */
		{ "ABC",
			"abc" },
		/* Numbers. */
		{ "123",
			"123" },
		/* German. */
		{ "ẞÄÖÜ",
			"ßäöü" },
		/* Western European. */
		{ "ÀÂÈÉÊËÎÏÔÙÛÜŸÇŒ",
			"àâèéêëîïôùûüÿçœ" },
		/* With spaces */
		{ "Đ Â Ă Ê Ô Ơ Ư Ấ Ắ Ế Ố Ớ Ứ Ầ Ằ Ề Ồ Ờ Ừ Ậ Ặ Ệ Ộ Ợ Ự",
			"đ â ă ê ô ơ ư ấ ắ ế ố ớ ứ ầ ằ ề ồ ờ ừ ậ ặ ệ ộ ợ ự" },

		{ NULL, NULL }
	};
	size_t i;
	M_utf8_error_t res;

	for (i=0; ts[i].upper!=NULL; i++) {
		res = M_utf8_toupper(ts[i].upper, &out);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: upper to upper failed: %d\n", i, res);
		ck_assert_msg(M_str_eq(out, ts[i].upper), "%zu: upper to upper != out: expected '%s', got '%s'", i, ts[i].upper, out);
		M_free(out);

		res = M_utf8_tolower(ts[i].upper, &out);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: upper to lower failed: %d\n", i, res);
		ck_assert_msg(M_str_eq(out, ts[i].lower), "%zu: upper to lower != out: expected '%s', got '%s'", i, ts[i].upper, out);
		M_free(out);

		res = M_utf8_tolower(ts[i].lower, &out);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: lower to lower failed: %d\n", i, res);
		ck_assert_msg(M_str_eq(out, ts[i].lower), "%zu: lower to lower != out: expected '%s', got '%s'", i, ts[i].lower, out);
		M_free(out);

		res = M_utf8_toupper(ts[i].lower, &out);
		ck_assert_msg(res == M_UTF8_ERROR_SUCCESS, "%zu: lower to upper failed: %d\n", i, res);
		ck_assert_msg(M_str_eq(out, ts[i].upper), "%zu: lower to upper != out: expected '%s', got '%s'", i, ts[i].lower, out);
		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *utf8_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("utf8");

	tc = tcase_create("utf8_correct");
	tcase_add_test(tc, check_utf8_correct);
	suite_add_tcase(suite, tc);

	tc = tcase_create("utf8_case_cp");
	tcase_add_test(tc, check_utf8_case_cp);
	suite_add_tcase(suite, tc);

	tc = tcase_create("utf8_case");
	tcase_add_test(tc, check_utf8_case);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(utf8_suite());
	srunner_set_log(sr, "check_utf8.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
