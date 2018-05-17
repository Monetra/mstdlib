#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* RFC Test cases. */
START_TEST(check_correct)
{
	char                *out;
	M_textcodec_error_t  res;
	size_t               i;
	static struct {
		const char *raw;
		const char *enc;
	} tests[] = {
		/* A */ { "\u0644\u064A\u0647\u0645\u0627\u0628\u062A\u0643\u0644\u0645\u0648\u0634\u0639\u0631\u0628\u064A\u061F", "egbpdaj6bu4bxfgehfvwxn" },
		/* B */ { "\u4ED6\u4EEC\u4E3A\u4EC0\u4E48\u4E0D\u8BF4\u4E2D\u6587", "ihqwcrb4cv8a8dqg056pqjye" },
		/* C */ { "\u4ED6\u5011\u7232\u4EC0\u9EBD\u4E0D\u8AAA\u4E2D\u6587", "ihqwctvzc91f659drss3x8bo0yb" },
		/* D */ { "Pro\u010Dprost\u011Bnemluv\u00ED\u010Desky", "Proprostnemluvesky-uyb24dma41a" },

		{ NULL, NULL }
	};

	for (i=0; tests[i].raw!=NULL; i++) {
		res = M_textcodec_encode(&out, tests[i].raw, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PUNYCODE);
		ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "%zu: Encode result not success: got %d", i, res);
		ck_assert_msg(M_str_eq(out, tests[i].enc), "%zu: Encode failed: got '%s', expected '%s'", i, out, tests[i].enc);
		M_free(out);

		res = M_textcodec_decode(&out, tests[i].enc, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PUNYCODE);
		ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "%zu: Decode result not success: got %d", i, res);
		ck_assert_msg(M_str_eq(out, tests[i].raw), "%zu: Decode failed: got '%s', expected '%s'", i, out, tests[i].raw);
		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *test_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("textcodec_punycode");

	tc = tcase_create("correct");
	tcase_add_test(tc, check_correct);
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
	srunner_set_log(sr, "textcodec_punycode.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
