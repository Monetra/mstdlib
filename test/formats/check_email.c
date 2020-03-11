#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define test_data "a"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_testing)
{
	M_email_t        *email;
	char             *out;
	M_email_error_t   res;
	size_t            len;
	size_t            len_read = 0;

	len = M_str_len(test_data);
	res = M_email_simple_read(&email, test_data, len, M_EMAIL_SIMPLE_READ_NONE, &len_read);
	M_printf("res = %d\n", res);
	M_printf("len = %zu, len_read = %zu\n", M_str_len(test_data), len_read);

	out = M_email_simple_write(email);
	M_printf("WRITE:\n'%s'\n", out);
	M_free(out);

	M_email_destroy(email);
}
END_TEST

START_TEST(check_splitting)
{
	M_hash_dict_t    *headers = NULL;
	char             *body    = NULL;
	char             *out     = NULL;
	M_email_error_t   res;


	res = M_email_simple_split_header_body(test_data, &headers, &body);
	M_printf("res: %d\n", res);

	out = M_hash_dict_serialize(headers, ';', '=', '\"', '\\', M_HASH_DICT_SER_FLAG_NONE);
	M_printf("HEADERS:\n'''\n%s\n'''\n", out);
	M_printf("BODY:\n'''\n%s\n'''\n", body);

	M_free(body);
	M_hash_dict_destroy(headers);

}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("email");

	add_test(suite, check_testing);
	add_test(suite, check_splitting);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_email.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

