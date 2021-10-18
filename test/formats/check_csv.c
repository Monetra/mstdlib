#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Helpers for tests. */

#define CSV_DATA "" \
	"\"header\"\"1\",header2,\"hea,der3\",\"header4\t\"\r\n" \
	"row1-h1,row1-h2,row1-h3,\r\n" \
	"row2-h1,,row2-h3,row2-h4\r\n" \
	"row3-h1,row3-h2,row3-h3,row3-h4\r\n"

#define CSV_DATA_SIMPLE "" \
	"h01,h02,h03,h04,h05\r\n" \
	"c11,c12,c13,c54,c15\r\n" \
	"c21,c22,c23,c64,c25\r\n" \
	"c31,c32,c33,c74,c35\r\n"

typedef struct  {
	char to_keep[2];
} filter_thunk_t;

static M_bool simple_row_filter(const M_csv_t *csv, size_t row, void *thunk)
{
	filter_thunk_t *data = (filter_thunk_t *)thunk;
	const char     *val;

	val = M_csv_get_cell(csv, row, "h04");

	if (M_str_len(val) == 3 && (val[1] == data->to_keep[0] || val[1] == data->to_keep[1])) {
		return M_TRUE;
	}
	return M_FALSE;
}

typedef struct {
	const char *header;
	const char *from;
	const char *to;
} writer_thunk_t;

static M_bool simple_cell_writer(M_buf_t *buf, const char *cell, const char *header, void *thunk)
{
	writer_thunk_t *data = (writer_thunk_t *)thunk;

	if (M_str_isempty(cell) || !M_str_eq(header, data->header) || !M_str_eq(cell, data->from)) {
		return M_FALSE;
	}

	M_buf_add_str(buf, data->to);

	return M_TRUE;
}

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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

START_TEST(check_parse)
{
	M_csv_t *csv = M_csv_parse(CSV_DATA, M_str_len(CSV_DATA), ',', '"', M_CSV_FLAG_NONE);

	ck_assert_msg(csv != NULL, "Unable to parse csv data");

	M_csv_destroy(csv);
}
END_TEST

START_TEST(check_parse_add_headers)
{
	M_csv_t      *csv;
	const char   *data;
	M_list_str_t *headers;
	size_t        i;
	size_t        j;
	M_buf_t      *buf;

	headers = M_list_str_create(M_LIST_STR_NONE);
	M_list_str_insert(headers, "header\"1");
	M_list_str_insert(headers, "header2");
	M_list_str_insert(headers, "hea,der3");
	M_list_str_insert(headers, "header4\t");

	data = M_str_chr(CSV_DATA, '\n') + 1;
	csv  = M_csv_parse_add_headers(data, M_str_len(data), ',', '"', M_CSV_FLAG_NONE, headers);
	ck_assert_msg(csv != NULL, "Unable to parse csv data");

	for (j=0; j<M_csv_get_numcols(csv); j++) {
		const char *hdr      = M_csv_get_header(csv, j);
		const char *expected = M_list_str_at(headers, j);
		ck_assert_msg(M_str_eq(hdr, expected), "Header '%s' does not match expected value '%s'", hdr, expected);
	}

	buf = M_buf_create();
	for (i=0; i<M_csv_get_numrows(csv); i++) {
		for (j=0; j<M_csv_get_numcols(csv); j++) {
			const char *val = M_csv_get_cellbynum(csv, i, j);
			const char *exp;
			if ((i==0 && j== 3) || (i==1 && j==1)) {
				exp = "";
			} else {
				M_buf_truncate(buf, 0);
				M_buf_add_str(buf, "row");
				M_buf_add_uint(buf, i + 1);
				M_buf_add_str(buf, "-h");
				M_buf_add_uint(buf, j + 1);
				exp = M_buf_peek(buf);
			}
			ck_assert_msg(M_str_eq(val, exp), "(%" PRIu64 ",%" PRIu64 ") '%s' does not match expected value '%s'", (M_uint64)i, (M_uint64)j, val, exp);
		}
	}

	M_buf_cancel(buf);
	M_csv_destroy(csv);
	M_list_str_destroy(headers);
}
END_TEST

START_TEST(check_write_basic)
{
	M_csv_t *csv = M_csv_parse(CSV_DATA, M_str_len(CSV_DATA), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t *buf = M_buf_create();

	ck_assert(csv != NULL);

	M_csv_output_headers_buf(buf, csv, NULL);
	M_csv_output_rows_buf(buf, csv, NULL, NULL, NULL, NULL, NULL);

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

	ck_assert(csv != NULL);

	M_list_str_insert(headers, "h03");
	M_list_str_insert(headers, "h01");

	M_csv_output_headers_buf(buf, csv, headers);
	M_csv_output_rows_buf(buf, csv, headers, NULL, NULL, NULL, NULL);

	/*M_printf("\nExpected:\n--[%s]--\n\nGot:\n--[%s]--\n", expected, M_buf_peek(buf));*/
	ck_assert_msg(M_str_eq(expected, M_buf_peek(buf)), "Output data doesn't match expected result");

	M_csv_destroy(csv);
	M_buf_cancel(buf);
	M_list_str_destroy(headers);
}
END_TEST

START_TEST(check_write_filter)
{
	M_csv_t        *csv      = M_csv_parse(CSV_DATA_SIMPLE, M_str_len(CSV_DATA_SIMPLE), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t        *buf      = M_buf_create();
	M_list_str_t   *headers  = M_list_str_create(M_LIST_STR_NONE);
	filter_thunk_t  fthunk;
	const char     *expected =
		"h03,h01\r\n"
		"c23,c21\r\n"
		"c33,c31\r\n";

	fthunk.to_keep[0] = '7';
	fthunk.to_keep[1] = '6';

	ck_assert(csv != NULL);

	M_list_str_insert(headers, "h03");
	M_list_str_insert(headers, "h01");

	M_csv_output_headers_buf(buf, csv, headers);
	M_csv_output_rows_buf(buf, csv, headers, simple_row_filter, &fthunk, NULL, NULL);

	/*M_printf("\nExpected:\n--[%s]--\n\nGot:\n--[%s]--\n", expected, M_buf_peek(buf));*/
	ck_assert_msg(M_str_eq(expected, M_buf_peek(buf)), "Output data doesn't match expected result");

	M_csv_destroy(csv);
	M_buf_cancel(buf);
	M_list_str_destroy(headers);
}
END_TEST


START_TEST(check_write_cell_edit)
{
	M_csv_t        *csv      = M_csv_parse(CSV_DATA_SIMPLE, M_str_len(CSV_DATA_SIMPLE), ',', '"', M_CSV_FLAG_NONE);
	M_buf_t        *buf      = M_buf_create();
	M_list_str_t   *headers  = M_list_str_create(M_LIST_STR_NONE);
	filter_thunk_t  fthunk;
	writer_thunk_t  wthunk;
	const char     *expected =
		"h03,h01\r\n"
		"c23,c21\r\n"
		"\"SUB,BED!\",c31\r\n";

	fthunk.to_keep[0] = '7';
	fthunk.to_keep[1] = '6';

	wthunk.header = "h03";
	wthunk.from   = "c33";
	wthunk.to     = "SUB,BED!";

	ck_assert(csv != NULL);

	M_list_str_insert(headers, "h03");
	M_list_str_insert(headers, "h01");

	M_csv_output_headers_buf(buf, csv, headers);
	M_csv_output_rows_buf(buf, csv, headers, simple_row_filter, &fthunk, simple_cell_writer, &wthunk);

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

	add_test(suite, check_parse_inplace);
	add_test(suite, check_parse);
	add_test(suite, check_parse_add_headers);
	add_test(suite, check_write_basic);
	add_test(suite, check_write_change_headers);
	add_test(suite, check_write_filter);
	add_test(suite, check_write_cell_edit);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_csv.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
