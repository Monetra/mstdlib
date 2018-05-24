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
		M_table_cell_insert(table, i, "alpha", unordered[i], M_TABLE_INSERT_NONE);
		/* second column will be reversed. */
		M_table_cell_insert_at(table, i, 1, unordered[lena-i-1]);
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

