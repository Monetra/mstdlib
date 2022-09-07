#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_url)
{
	M_url_t *url        = M_url_create("schema://userinfo@host:8080/path?query#fragment");
	ck_assert_msg(M_str_eq(url->schema, "schema"), "Should have parsed schema");
	ck_assert_msg(M_str_eq(url->host, "host"), "Should have parsed host");
	ck_assert_msg(M_str_eq(url->userinfo, "userinfo"), "Should have parsed userinfo");
	ck_assert_msg(M_str_eq(url->port, "8080"), "Should have parsed port");
	ck_assert_msg(M_str_eq(url->path, "/path"), "Should have parsed path");
	ck_assert_msg(M_str_eq(url->query, "query"), "Should have parsed query");
	ck_assert_msg(M_str_eq(url->fragment, "fragment"), "Should have parsed fragment");
	ck_assert_msg(url->port_u16 == 8080, "Should have parsed port as uint16");
	M_url_destroy(url);
}
END_TEST



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("url");

	add_test(suite, check_url);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_url.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
