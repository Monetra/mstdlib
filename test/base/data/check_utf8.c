#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>

/* Soft tests are from
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *utf8_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("utf8");

	tc = tcase_create("utf8_correct");
	tcase_add_test(tc, check_utf8_correct);
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
