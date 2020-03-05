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

#define test_data "b"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_testing)
{
	M_email_t        *email;
	char             *out;
	const char       *group;
	const char       *name;
	const char       *address;
	M_email_error_t   res;
	size_t            len_read = 0;
	size_t            len;
	size_t            i;

	res = M_email_simple_read(&email, test_data, M_str_len(test_data), M_EMAIL_SIMPLE_READ_NONE, &len_read);
	M_printf("res = %d\n", res);
	M_printf("len = %zu, len_read = %zu\n", M_str_len(test_data), len_read);

	out = M_hash_dict_serialize(M_email_headers(email), '\n', '=', '\"', '\\', M_HASH_DICT_SER_FLAG_NONE);
	M_printf("HEADERS:\n%s\n\n", out);
	M_free(out);


	len = M_email_to_len(email);
	M_printf("TO (%zu):\n", len);
	for (i=0; i<len; i++) {
		if (M_email_to(email, i, &group, &name, &address)) {
			M_printf("Group: '%s', Name: '%s', Address: '%s'\n", M_str_safe(group), M_str_safe(name), M_str_safe(address));
		}
	}
	M_printf("\n");

	len = M_email_parts_len(email);
	M_printf("num parts = %zu\n", len);
	for (i=0; i<len; i++) {
		out = M_hash_dict_serialize(M_email_part_headers(email, i), '\n', '=', '\"', '\\', M_HASH_DICT_SER_FLAG_NONE);
		M_printf("HEADERS:\n%s\n\n", out);
		M_free(out);

		M_printf("PART:\n'%s'\n\n", M_email_part_data(email, i));
	}

	M_email_destroy(email);
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

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_email.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

