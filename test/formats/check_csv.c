#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Helpers for tests. */

#define CSV_DATA "" \
	"header\"\"1,header2,\"hea,der3\",\"header4\t\"\r\n" \
	"row1-h1,row1-h2,row1-h3,\r\n" \
	"row2-h1,,row1-h3,row1-h4\r\n" \
	"row3-h1,row3-h2,row3-h3,row3-h4\r\n"

#define CSV_DATA_SIMPLE "" \
	"h01,h02,h03,h04,h05\r\n" \
	"c11,c12,c13,c54,c15\r\n" \
	"c21,c22,c23,c64,c25\r\n" \
	"c31,c32,c33,c74,c35\r\n"

typedef struct  {
	char to_keep[2];
} thunk_t;

static M_bool simple_row_filter(const M_csv_t *csv, size_t row, void *thunk)
{
	const char     *val;
	thunk_t *data = (thunk_t *)thunk;

	val = M_csv_get_cell(csv, row, "h04");

	if (M_str_len(val) == 3 && (val[1] == data->to_keep[0] || val[1] == data->to_keep[1])) {
		return M_TRUE;
	}
	return M_FALSE;
}

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_parse)
{
	M_csv_t *csv = M_csv_parse(CSV_DATA, M_str_len(CSV_DATA), ',', '"', M_CSV_FLAG_NONE);

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

START_TEST(check_write_basic)
{
	M_csv_t *csv = M_csv_parse(CSV_DATA, M_str_len(CSV_DATA), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t *buf = M_buf_create();

	ck_assert_ptr_ne(csv, NULL);

	M_csv_output_headers_buf(buf, csv, NULL);
	M_csv_output_rows_buf(buf, csv, NULL, NULL, NULL);

	/*M_printf("\nExpected:\n--[%s]--\n\nGot:\n--[%s]--\n", CSV_DATA, M_buf_peek(buf));*/
	ck_assert_msg(M_str_eq(CSV_DATA, M_buf_peek(buf)), "Output data doesn't match input data");

	M_csv_destroy(csv);
	M_buf_cancel(buf);
}
END_TEST

START_TEST(check_write_change_headers)
{
	M_csv_t      *csv      = M_csv_parse(CSV_DATA_SIMPLE, M_str_len(CSV_DATA_SIMPLE), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t      *buf      = M_buf_create();
	M_list_str_t *headers  = M_list_str_create(M_LIST_STR_NONE);
	const char   *expected =
		"h03,h01\r\n"
		"c13,c11\r\n"
		"c23,c21\r\n"
		"c33,c31\r\n";

	ck_assert_ptr_ne(csv, NULL);

	M_list_str_insert(headers, "h03");
	M_list_str_insert(headers, "h01");

	M_csv_output_headers_buf(buf, csv, headers);
	M_csv_output_rows_buf(buf, csv, headers, NULL, NULL);

	/*M_printf("\nExpected:\n--[%s]--\n\nGot:\n--[%s]--\n", expected, M_buf_peek(buf));*/
	ck_assert_msg(M_str_eq(expected, M_buf_peek(buf)), "Output data doesn't match expected result");

	M_csv_destroy(csv);
	M_buf_cancel(buf);
	M_list_str_destroy(headers);
}
END_TEST

START_TEST(check_write_filter)
{
	M_csv_t      *csv      = M_csv_parse(CSV_DATA_SIMPLE, M_str_len(CSV_DATA_SIMPLE), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t      *buf      = M_buf_create();
	M_list_str_t *headers  = M_list_str_create(M_LIST_STR_NONE);
	thunk_t       fthunk;
	const char   *expected =
		"h03,h01\r\n"
		"c23,c21\r\n"
		"c33,c31\r\n";

	fthunk.to_keep[0] = '7';
	fthunk.to_keep[1] = '6';

	ck_assert_ptr_ne(csv, NULL);

	M_list_str_insert(headers, "h03");
	M_list_str_insert(headers, "h01");

	M_csv_output_headers_buf(buf, csv, headers);
	M_csv_output_rows_buf(buf, csv, headers, simple_row_filter, &fthunk);

	/*M_printf("\nExpected:\n--[%s]--\n\nGot:\n--[%s]--\n", expected, M_buf_peek(buf));*/
	ck_assert_msg(M_str_eq(expected, M_buf_peek(buf)), "Output data doesn't match expected result");

	M_csv_destroy(csv);
	M_buf_cancel(buf);
	M_list_str_destroy(headers);
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("csv");

	add_test(suite, check_parse);
	add_test(suite, check_parse_inplace);
	add_test(suite, check_write_basic);
	add_test(suite, check_write_change_headers);
	add_test(suite, check_write_filter);

	sr = srunner_create(suite);
	srunner_set_log(sr, "check_csv.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
