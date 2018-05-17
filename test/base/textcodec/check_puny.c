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
		/* E */ { "\u05DC\u05DE\u05D4\u05D4\u05DD\u05E4\u05E9\u05D5\u05D8\u05DC\u05D0\u05DE\u05D3\u05D1\u05E8\u05D9\u05DD\u05E2\u05D1\u05E8\u05D9\u05EA", "4dbcagdahymbxekheh6e0a7fei0b" },
		/* F */ { "\u092F\u0939\u0932\u094B\u0917\u0939\u093F\u0928\u094D\u0926\u0940\u0915\u094D\u092F\u094B\u0902\u0928\u0939\u0940\u0902\u092C\u094B\u0932\u0938\u0915\u0924\u0947\u0939\u0948\u0902", "i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd" },
		/* G */ { "\u306A\u305C\u307F\u3093\u306A\u65E5\u672C\u8A9E\u3092\u8A71\u3057\u3066\u304F\u308C\u306A\u3044\u306E\u304B", "n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa" },
		/* H */ { "\uC138\uACC4\uC758\uBAA8\uB4E0\uC0AC\uB78C\uB4E4\uC774\uD55C\uAD6D\uC5B4\uB97C\uC774\uD574\uD55C\uB2E4\uBA74\uC5BC\uB9C8\uB098\uC88B\uC744\uAE4C", "989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0dt30a5jpsd879ccm6fea98c" },
		/* I */ { "\u043F\u043E\u0447\u0435\u043C\u0443\u0436\u0435\u043E\u043D\u0438\u043D\u0435\u0433\u043E\u0432\u043E\u0440\u044F\u0442\u043F\u043E\u0440\u0443\u0441\u0441\u043A\u0438", "b1abfaaepdrnnbgefbaDotcwatmq2g4l" },
		/* J */ { "Porqu\u00E9nopuedensimplementehablarenEspa\u00f1ol", "PorqunopuedensimplementehablarenEspaol-fmd56a" },
		/* K */ { "TạisaohọkhôngthểchỉnóitiếngViệt", "TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g" },
		/* L */ {  "3年B組金八先生", "3B-ww4c5e180e575a65lsy2b" },
		/* M */ { "安室奈美恵-with-SUPER-MONKEYS", "-with-SUPER-MONKEYS-pc58ag80a8qai00g7n9n" },
		/* N */ { "Hello-Another-Way-それぞれの場所", "Hello-Another-Way--fc4qua05auwb3674vfr0b" },
		/* O */ { "ひとつ屋根の下2", "2-u9tlzr9756bt3uc0v" },
		/* P */ { "MajiでKoiする5秒前", "MajiKoi5-783gue6qz075azm5e" },
		/* Q */ { "パフィーdeルンバ", "de-jg4avhby1noc0d" },
		/* R */ { "そのスピードで", "d9juau41awczczp" },
		/* S */ { "-> $1.00 <-", "-> $1.00 <--" },
		{ NULL, NULL }
	};

	for (i=0; tests[i].raw!=NULL; i++) {
		res = M_textcodec_encode(&out, tests[i].raw, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PUNYCODE);
		ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "%zu: Encode result not success: got %d", i, res);
		ck_assert_msg(M_str_caseeq(out, tests[i].enc), "%zu: Encode failed: got '%s', expected '%s'", i, out, tests[i].enc);
		M_free(out);

		res = M_textcodec_decode(&out, tests[i].enc, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PUNYCODE);
		ck_assert_msg(res == M_TEXTCODEC_ERROR_SUCCESS, "%zu: Decode result not success: got %d", i, res);
		ck_assert_msg(M_str_caseeq(out, tests[i].raw), "%zu: Decode failed: got '%s', expected '%s'", i, out, tests[i].raw);
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
