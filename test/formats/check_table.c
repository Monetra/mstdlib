#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_table_colname_sort)
{
	M_table_t  *table;
	const char *const_temp;
	size_t      len;
	size_t      lena;
	size_t      i;
	const char *unordered[] = { "zeta", "alpha", "beta", "gama", "epsilon" };
	const char *ordered[]   = { "alpha", "beta", "epsilon", "gama", "zeta" };

	table = M_table_create(M_TABLE_COLNAME_CASECMP);

	lena  = sizeof(unordered)/sizeof(*unordered);
	for (i=0; i<lena; i++) {
		M_table_column_insert(table, unordered[i]);
	}
	len = M_table_column_count(table);
	ck_assert_msg(len == lena, "Wrong number of columns: got %zu, expected %zu", len, lena);

	for (i=0; i<lena; i++) {
		const_temp = M_table_column_name(table, i);
		ck_assert_msg(M_str_caseeq(const_temp, unordered[i]), "%zu: Unordered column name does not match: got '%s', expected '%s'", i, const_temp, unordered[i]);
	}

	M_table_column_order(table, M_sort_compar_str, NULL);

	lena = sizeof(ordered)/sizeof(*ordered);
	len  = M_table_column_count(table);
	ck_assert_msg(len == lena, "Wrong number of columns after order, got %zu, expected %zu", len, lena);

	for (i=0; i<lena; i++) {
		const_temp = M_table_column_name(table, i);
		ck_assert_msg(M_str_caseeq(const_temp, ordered[i]), "%zu: Ordered column name does not match: got '%s', expected '%s'", i, const_temp, ordered[i]);
	}

	M_table_destroy(table);
}
END_TEST

START_TEST(check_table_coldata_sort)
{
	M_table_t  *table;
	const char *const_temp;
	size_t      len;
	size_t      lena;
	size_t      i;
	const char *unordered[]  = { "zeta", "alpha", "beta", "gama", "epsilon" };
	const char *ordered[]    = { "alpha", "beta", "epsilon", "gama", "zeta" };
	const char *afterorder[] = { "gama", "beta", "zeta", "alpha", "epsilon" };

	table = M_table_create(M_TABLE_COLNAME_CASECMP);

	M_table_column_insert(table, "beta");
	M_table_column_insert_at(table, 0, "alpha");
	lena = 2;
	len  = M_table_column_count(table);
	ck_assert_msg(len == lena, "Wrong number of columns: got %zu, expected %zu", len, lena);

	lena = sizeof(unordered)/sizeof(unordered[0]);
	for (i=0; i<lena; i++) {
		M_table_row_insert(table);
		M_table_cell_set(table, i, "alpha", unordered[i], M_TABLE_INSERT_NONE);
		/* second column will be reversed. */
		M_table_cell_set_at(table, i, 1, unordered[lena-i-1]);
	}
	len = M_table_row_count(table);
	ck_assert_msg(len == lena, "Wrong number of rows: got %zu, expected %zu", len, lena);

	M_table_column_sort_data(table, "alpha", NULL, NULL, NULL, NULL);

	lena = sizeof(afterorder)/sizeof(afterorder[0]);
	for (i=0; i<lena; i++) {
		const_temp = M_table_cell_at(table, i, 1);
		ck_assert_msg(M_str_caseeq(const_temp, afterorder[i]), "%zu: Unordered 'beta' does not match: got '%s', expected '%s'", i, const_temp, afterorder[i]);
	}

	lena = sizeof(ordered)/sizeof(ordered[0]);
	for (i=0; i<lena; i++) {
		const_temp = M_table_cell(table, i, "alpha");
		ck_assert_msg(M_str_caseeq(const_temp, ordered[i]), "%zu: Ordered 'alpha' does not match: got '%s', expected '%s'", i, const_temp, ordered[i]);
	}

	M_table_destroy(table);
}
END_TEST

START_TEST(check_table_csv)
{
	M_table_t  *table;
	char       *out;
	M_bool      ret;
	const char *csv_data = ""
		"header1, h 2, nope, gah\r\n"
		"v1,v2, v3,\r\n"
		"1,2, 3,4\r\n"
		"1,,\"\"\"Test\"\"\",4\r\n"
		"1,\",\",,";
	const char *csv_data2 = ""
		"header1, h 2, nope, gah\r\n"
		"v1,v2, v3,\r\n"
		"1,2, 3,4\r\n"
		"1,,\"\"\"Test\"\"\",4\r\n"
		"1,\",\",,\r\n"
		"v1,v2, v3,\r\n"
		"1,2, 3,4\r\n"
		"1,,\"\"\"Test\"\"\",4\r\n"
		"1,\",\",,";
	const char *csv_data_noheader = ""
		"alpha, beta, epsilon, gama\r\n"
		"zeta, beta, gama,\r\n";
	const char *csv_data3 = ""
		"header1, h 2, nope, gah\r\n"
		"v1,v2, v3,\r\n"
		"1,2, 3,4\r\n"
		"1,,\"\"\"Test\"\"\",4\r\n"
		"1,\",\",,\r\n"
		"v1,v2, v3,\r\n"
		"1,2, 3,4\r\n"
		"1,,\"\"\"Test\"\"\",4\r\n"
		"1,\",\",,\r\n"
		"alpha, beta, epsilon, gama\r\n"
		"zeta, beta, gama,";

	/* Check csv. */
	table = M_table_create(M_TABLE_NONE);
	ret   = M_table_load_csv(table, csv_data, M_str_len(csv_data), ',', '"', M_CSV_FLAG_NONE, M_TRUE);
	ck_assert_msg(ret, "Failed to load csv");

	out = M_table_write_csv(table, ',', '"', M_TRUE);
	ck_assert_msg(M_str_eq(out, csv_data), "cvs data does not match, got:\n'%s'\nexpected:\n'%s'", out, csv_data);
	M_free(out);

	/* Load the csv again into the table so it's dobuled. */
	ret   = M_table_load_csv(table, csv_data, M_str_len(csv_data), ',', '"', M_CSV_FLAG_NONE, M_TRUE);
	ck_assert_msg(ret, "Failed to load csv second time");

	out = M_table_write_csv(table, ',', '"', M_TRUE);
	ck_assert_msg(M_str_eq(out, csv_data2), "cvs2 data does not match, got:\n'%s'\nexpected:\n'%s'", out, csv_data2);
	M_free(out);

	/* Load more csv data (without headers). */
	ret   = M_table_load_csv(table, csv_data_noheader, M_str_len(csv_data_noheader), ',', '"', M_CSV_FLAG_NONE, M_FALSE);
	ck_assert_msg(ret, "Failed to load csv no header");

	out = M_table_write_csv(table, ',', '"', M_TRUE);
	ck_assert_msg(M_str_eq(out, csv_data3), "cvs3 data does not match, got:\n'%s'\nexpected:\n'%s'", out, csv_data3);
	M_free(out);

	M_table_destroy(table);
}
END_TEST

START_TEST(check_table_json)
{
	M_table_t  *table;
	char       *out;
	M_bool      ret;
	const char *data = "["
		"{\"a\":\"a\",\"b\":\"b\",\"other\":\"val\"},"
		"{\"a\":\"q\",\"b\":\"b\"},"
		"{\"a\":\"1\",\"b\":\"b\",\"other\":\"abc\"},"
		"{\"a\":\"7\",\"other\":\"blah\"}"
		"]";

	/* Check. */
	table = M_table_create(M_TABLE_NONE);
	ret   = M_table_load_json(table, data, M_str_len(data));
	ck_assert_msg(ret, "Failed to load json");

	out = M_table_write_json(table, M_JSON_WRITER_NONE);
	ck_assert_msg(M_str_eq(out, data), "json data does not match, got:\n'%s'\nexpected:\n'%s'", out, data);
	M_free(out);
	M_table_destroy(table);
}
END_TEST

START_TEST(check_table_markdown)
{
	M_table_t  *table;
	char       *out;
	M_bool      ret;
	const char *indata = ""
		"Tables | Are | Cool\n"
		"------------- |:-------------:| -----:\n"
		"col 3 is     | right-aligned | $1600 \n"
		"col 2 is      | centered      |   $12\n"
		"zebra stripes | are neat      |    $1 \r\n";
	const char *outdata = ""
		"| Tables        | Are           | Cool  |\r\n"
		"| ------------- | ------------- | ----- |\r\n"
		"| col 3 is      | right-aligned | $1600 |\r\n"
		"| col 2 is      | centered      | $12   |\r\n"
		"| zebra stripes | are neat      | $1    |";

	/* Check. */
	table = M_table_create(M_TABLE_NONE);
	ret   = M_table_load_markdown(table, indata, M_str_len(indata));
	ck_assert_msg(ret, "Failed to load markdown");

	out = M_table_write_markdown(table, M_TABLE_MARKDOWN_PRETTYPRINT|M_TABLE_MARKDOWN_OUTERPIPE|M_TABLE_MARKDOWN_LINEEND_WIN);
	ck_assert_msg(M_str_eq(out, outdata), "markdown data does not match, got:\n'%s'\nexpected:\n'%s'", out, outdata);
	M_free(out);
	M_table_destroy(table);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *test_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("table");

	tc = tcase_create("table_colname_sort");
	tcase_add_test(tc, check_table_colname_sort);
	suite_add_tcase(suite, tc);

	tc = tcase_create("table_coldata_sort");
	tcase_add_test(tc, check_table_coldata_sort);
	suite_add_tcase(suite, tc);

	tc = tcase_create("table_csv");
	tcase_add_test(tc, check_table_csv);
	suite_add_tcase(suite, tc);

	tc = tcase_create("table_json");
	tcase_add_test(tc, check_table_json);
	suite_add_tcase(suite, tc);

	tc = tcase_create("table_markdown");
	tcase_add_test(tc, check_table_markdown);
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
	srunner_set_log(sr, "check_table.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
