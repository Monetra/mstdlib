#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_text.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_valid)
{
	const char          *in  = "ăѣ𝔠ծềſģȟ";
	char                *out = NULL;
	M_textcodec_error_t  res;

	res = M_textcodec_encode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "Encode: Failed to read valid input");
	ck_assert_msg(M_str_eq(in, out), "Encode: Input does not match output");
	M_free(out);

	res = M_textcodec_decode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "Decode: Failed to read valid input");
	ck_assert_msg(M_str_eq(in, out), "Decode: Input does not match output");
	M_free(out);
}
END_TEST

START_TEST(check_efail)
{
	const char          *in  = "ăѣ𝔠" "\xe2\x28\xa1" "ծề" "\xc3\xb1" "ſģȟ";
	char                *out = NULL;
	M_textcodec_error_t  res;

	res = M_textcodec_encode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_FAIL, "Encode: Valid input, should have be treated as invalid");
	M_free(out);

	res = M_textcodec_decode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_FAIL, "Decode: Valid input, should be treated as invalid");
	M_free(out);
}
END_TEST

START_TEST(check_eignore)
{
	const char          *in  = "ăѣ𝔠" "\xF0\xA4\xAD" "ծề" "\xF0\xA4" "ſģȟ";
	char                *out = NULL;
	M_textcodec_error_t  res;

	res = M_textcodec_encode(&out, in, M_TEXTCODEC_EHANDLER_IGNORE, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS_EHANDLER, "Encode: Failed to read input");
	ck_assert_msg(M_str_eq(in, out), "Encode: Input does not match output");
	M_free(out);

	res = M_textcodec_decode(&out, in, M_TEXTCODEC_EHANDLER_IGNORE, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS_EHANDLER, "Decode: Failed to read input");
	ck_assert_msg(M_str_eq(in, out), "Decode: Input does not match output");
	M_free(out);
}
END_TEST

START_TEST(check_ereplace)
{
	const char          *in  = "ăѣ𝔠" "\xF0\xA4\xAD" "ծề" "\xF0\xA4" "ſģȟ";
	const char          *enc  = "ăѣ𝔠" "?" "ծề" "?" "ſģȟ";
	const char          *dec  = "ăѣ𝔠" "\xFF\xFD" "ծề" "\xFF\xFD" "ſģȟ";
	char                *out = NULL;
	M_textcodec_error_t  res;

	res = M_textcodec_encode(&out, in, M_TEXTCODEC_EHANDLER_REPLACE, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS_EHANDLER, "Encode: Failed to read input");
	ck_assert_msg(M_str_eq(enc, out), "Encode: in '%s': enc '%s', does not match out '%s'", in, enc, out);
	M_free(out);

	res = M_textcodec_decode(&out, in, M_TEXTCODEC_EHANDLER_REPLACE, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS_EHANDLER, "Decode: Failed to read input");
	ck_assert_msg(M_str_eq(dec, out), "Decode: in '%s': dec '%s', does not match out '%s'", in, dec, out);
	M_free(out);
}
END_TEST

START_TEST(check_control)
{
	const char          *in  = "\x1Căѣ𝔠\x1Dծềſģȟ";
	char                *out = NULL;
	M_textcodec_error_t  res;

	res = M_textcodec_encode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "Encode: Failed to read valid input");
	ck_assert_msg(M_str_eq(in, out), "Encode: Input does not match output");
	M_free(out);

	res = M_textcodec_decode(&out, in, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_UTF8);
	ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "Decode: Failed to read valid input");
	ck_assert_msg(M_str_eq(in, out), "Decode: Input does not match output");
	M_free(out);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *test_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("textcodec_utf8");

	tc = tcase_create("valid");
	tcase_add_test(tc, check_valid);
	suite_add_tcase(suite, tc);

	tc = tcase_create("efail");
	tcase_add_test(tc, check_efail);
	suite_add_tcase(suite, tc);

	tc = tcase_create("ereplace");
	tcase_add_test(tc, check_ereplace);
	suite_add_tcase(suite, tc);

	tc = tcase_create("eignore");
	tcase_add_test(tc, check_eignore);
	suite_add_tcase(suite, tc);

	tc = tcase_create("control");
	tcase_add_test(tc, check_control);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(test_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_puny.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
