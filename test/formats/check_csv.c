#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CSV_DATA "" \
	"header1,header2,header3,header4\n" \
	"row1-h1,row1-h2,row1-h3,\n" \
	"row2-h1,,row1-h3,row1-h4\n" \
	"row3-h1,row3-h2,row3-h3,row3-h4"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_parse)
{
	M_csv_t *csv;

	csv = M_csv_parse(CSV_DATA, M_str_len(CSV_DATA), ',', '"', M_CSV_FLAG_NONE);
	ck_assert_msg(csv != NULL, "Unable to parse csv data");

	M_csv_destroy(csv);
}
END_TEST

START_TEST(check_parse_inplace)
{
	M_csv_t *csv;
	char    *data;

	data = M_strdup(CSV_DATA);
	csv  = M_csv_parse(data, M_str_len(data), ',', '"', M_CSV_FLAG_NONE);
	ck_assert_msg(csv != NULL, "Unable to parse csv data");

	M_csv_destroy(csv);
	M_free(data);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_suite(void)
{
	Suite *suite;
	TCase *tc_csv_parse;
	TCase *tc_csv_parse_inplace;

	suite = suite_create("csv");

	tc_csv_parse = tcase_create("check_parse");
	tcase_add_test(tc_csv_parse, check_parse);
	suite_add_tcase(suite, tc_csv_parse);

	tc_csv_parse_inplace = tcase_create("check_parse_inplace");
	tcase_add_test(tc_csv_parse_inplace, check_parse_inplace);
	suite_add_tcase(suite, tc_csv_parse_inplace);

	return suite;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_suite());
	srunner_set_log(sr, "check_csv.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
