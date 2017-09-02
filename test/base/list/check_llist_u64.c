#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_llist_u64_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char          *prefix;
	M_llist_u64_flags_t  flags;
	int                  vals_ordered_idx;
} check_llist_u64_generic_data[] = {
	{ "Unordered", M_LLIST_U64_NONE,     0 },
	{ "SortAsc",   M_LLIST_U64_SORTASC,  1 },
	{ "SortDesc",  M_LLIST_U64_SORTDESC, 2 },
	{ NULL, 0, 0 }
};
#define VALS_LEN 24
static const M_uint64 generic_vals[VALS_LEN] = {
      1, 7, 2, 9, 8, 10, 22, 3, 4, 3, 9, 8, 99, 2, 200, 100, 50, 82, 19, 101, 107, 41, 11, 88 };
static const M_uint64 generic_vals_ordered[3][VALS_LEN] = {
	{ 1,   7,   2,   9,   8,  10, 22, 3,  4,  3,  9,  8,  99, 2,  200, 100, 50, 82, 19, 101, 107, 41,  11,  88  },
	{ 1,   2,   2,   3,   3,  4,  7,  8,  8,  9,  9,  10, 11, 19, 22,  41,  50, 82, 88, 99,  100, 101, 107, 200 },
	{ 200, 107, 101, 100, 99, 88, 82, 50, 41, 22, 19, 11, 10, 9,  9,   8,   8,  7,  4,  3,   3,   2,   2,   1   }
};

START_TEST(check_llist_u64_insert)
{
	M_llist_u64_t      *d;
	M_llist_u64_node_t *n;
	const char         *p;
	const M_uint64     *vals_result;
	M_uint64            v;
	size_t              i;
	size_t              j;

	for (i=0; check_llist_u64_generic_data[i].prefix!=NULL; i++) {
		p           = check_llist_u64_generic_data[i].prefix;
		vals_result = generic_vals_ordered[check_llist_u64_generic_data[i].vals_ordered_idx];
		d           = M_llist_u64_create(check_llist_u64_generic_data[i].flags);

		for (j=0; j<VALS_LEN; j++) {
			ck_assert_msg(M_llist_u64_insert(d, generic_vals[j])!=NULL, "%s: Could not insert (%zu) value %lld", p, j, generic_vals[j]);
		}

		n = M_llist_u64_first(d);
		for (j=0; j<VALS_LEN; j++) {
			ck_assert_msg(n !=  NULL, "%s: Premature end of list (%zu)", p, j);
			if (n == NULL) {
				break;
			}

			v = M_llist_u64_node_val(n);
			ck_assert_msg(v == vals_result[j], "%s: Order mismatch (%zu), got=%lld, expected=%lld", p, j, v, vals_result[j]);

			n = M_llist_u64_node_next(n);
		}
		ck_assert_msg(M_llist_u64_node_next(n) == NULL, "%s: End of list expected", p);

		M_llist_u64_destroy(d);
	}
}
END_TEST

START_TEST(check_llist_u64_insert_before_after)
{
	M_llist_u64_t      *d;
	M_llist_u64_node_t *n;
	M_uint64            vals[]        = { 1, 7, 2, 9 };
	M_uint64            vals_result[] = { 4, 1, 7, 8, 2, 9 };
	M_uint64            v;
	size_t              i;

	d = M_llist_u64_create(M_LLIST_U64_NONE);

	for (i=0; i<sizeof(vals)/sizeof(*vals); i++) {
		M_llist_u64_insert(d, vals[i]);
	}

	n = M_llist_u64_find(d, 1);
	ck_assert_msg(n != NULL, "Could not find node with value 1");
	M_llist_u64_insert_before(n, 4);

	n = M_llist_u64_find(d, 7);
	ck_assert_msg(n != NULL, "Could not find node with value 7");
	M_llist_u64_insert_after(n, 8);

	n = M_llist_u64_first(d);
	for (i=0; i<sizeof(vals_result)/sizeof(*vals_result); i++) {
		ck_assert_msg(n != NULL, "Premature end of list");
		if (n == NULL) {
			break;
		}

		v = M_llist_u64_node_val(n);
		ck_assert_msg(v == vals_result[i], "Order mismatch (%zu), got=%lld, expected=%lld", i, v, vals_result[i]);

		n = M_llist_u64_node_next(n);
	}

	M_llist_u64_destroy(d);
}
END_TEST

START_TEST(check_llist_u64_first_last_find)
{
	M_llist_u64_t      *d;
	M_llist_u64_node_t *n;
	M_uint64            vals[] = { 7, 1, 9, 2 };
	size_t              i;

	/* Sorted. */
	d = M_llist_u64_create(M_LLIST_U64_SORTASC);

	for (i=0; i<sizeof(vals)/sizeof(*vals); i++) {
		M_llist_u64_insert(d, vals[i]);
	}

	n = M_llist_u64_first(d);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 1, "Sorted first is not 1");

	n = M_llist_u64_last(d);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 9, "Sorted last is not 9");

	n = M_llist_u64_find(d, 7);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 7, "Sorted find is not 7");

	n = M_llist_u64_find(d, 99);
	ck_assert_msg(n == NULL, "Sorted find found 99 which doesn't exist");

	M_llist_u64_destroy(d);


	/* Unsorted. */
	d = M_llist_u64_create(M_LLIST_U64_NONE);

	for (i=0; i<sizeof(vals)/sizeof(*vals); i++) {
		M_llist_u64_insert(d, vals[i]);
	}

	n = M_llist_u64_first(d);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 7, "Unsorted first is not 7");

	n = M_llist_u64_last(d);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 2, "Unsorted last is not 2");

	n = M_llist_u64_find(d, 9);
	ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 9, "Unsorted find is not 9");

	n = M_llist_u64_find(d, 99);
	ck_assert_msg(n == NULL, "Sorted find found 99 which doesn't exist");

	M_llist_u64_destroy(d);
}
END_TEST

static struct {
	const char          *prefix;
	M_llist_u64_flags_t  flags;
} check_llist_u64_take_remove_count_data[] = {
	{ "Unsorted", M_LLIST_U64_NONE     },
	{ "SortAsc",  M_LLIST_U64_SORTASC  },
	{ "SortDesc", M_LLIST_U64_SORTDESC },
	{ NULL, 0 }
};

START_TEST(check_llist_u64_take_remove_count)
{
	M_llist_u64_t      *d;
	M_llist_u64_node_t *n;
	const char         *p;
	M_uint64            vals[] = { 7, 1, 4, 3, 9, 4, 3, 2, 8, 3, 1, 15 };
	size_t              i;
	size_t              j;
	size_t              len;

	for (i=0; check_llist_u64_take_remove_count_data[i].prefix!=NULL; i++) {
		p = check_llist_u64_take_remove_count_data[i].prefix;
		d = M_llist_u64_create(check_llist_u64_take_remove_count_data[i].flags);

		for (j=0; j<sizeof(vals)/sizeof(*vals); j++) {
			M_llist_u64_insert(d, vals[j]);
		}

		len = M_llist_u64_len(d);
		ck_assert_msg(len == 12, "%s: list len %zu != 12", p, len);

		n = M_llist_u64_find(d, 7);
		ck_assert_msg(n != NULL && M_llist_u64_take_node(n) == 7, "%s: 7 not found", p);
		len = M_llist_u64_len(d);
		ck_assert_msg(len == 11, "%s: list len %zu != 11", p, len);

		n = M_llist_u64_find(d, 2);
		ck_assert_msg(n != NULL && M_llist_u64_node_val(n) == 2, "%s: 2 not found", p);
		M_llist_u64_remove_node(n);
		len = M_llist_u64_len(d);
		ck_assert_msg(len == 10, "%s: list len %zu != 10", p, len);

		M_llist_u64_remove_val(d, 9, M_LLIST_U64_MATCH_VAL);
		len = M_llist_u64_len(d);
		ck_assert_msg(len == 9, "%s: list len %zu != 9", p, len);

		len = M_llist_u64_count(d, 3);
		ck_assert_msg(len == 3, "%s: 3 not found %zu times != 3", p, len);

		M_llist_u64_remove_val(d, 3, M_LLIST_U64_MATCH_ALL);
		len = M_llist_u64_len(d);
		ck_assert_msg(len == 6, "%s: list len %zu != 6", p, len);

		len = M_llist_u64_count(d, 3);
		ck_assert_msg(len == 0, "%s: 3 not found %zu times != 0", p, len);

		len = M_llist_u64_count(d, 4);
		ck_assert_msg(len == 2, "%s: 4 not found %zu times != 2", p, len);

		M_llist_u64_remove_duplicates(d);
		len = M_llist_u64_len(d);
		ck_assert_msg(len == 4, "%s: list len %zu != 4", p, len);

		len = M_llist_u64_count(d, 4);
		ck_assert_msg(len == 1, "%s: 4 not found %zu times != 1", p, len);

		len = M_llist_u64_count(d, 1);
		ck_assert_msg(len == 1, "%s: 1 not found %zu times != 1", p, len);

		M_llist_u64_destroy(d);
	}
}
END_TEST

START_TEST(check_llist_u64_next_prev)
{
	M_llist_u64_t      *d;
	M_llist_u64_node_t *n;
	const char         *p;
	const M_uint64     *vals_result;
	M_uint64            v;
	size_t              i;
	size_t              j;

	for (i=0; check_llist_u64_generic_data[i].prefix!=NULL; i++) {
		p           = check_llist_u64_generic_data[i].prefix;
		vals_result = generic_vals_ordered[check_llist_u64_generic_data[i].vals_ordered_idx];
		d           = M_llist_u64_create(check_llist_u64_generic_data[i].flags);

		for (j=0; j<VALS_LEN; j++) {
			M_llist_u64_insert(d, generic_vals[j]);
		}

		/* next. */
		n = M_llist_u64_first(d);
		for (j=0; j<VALS_LEN; j++) {
			ck_assert_msg(n != NULL, "%s: Premature end of list", p);
			if (n == NULL) {
				break;
			}

			v = M_llist_u64_node_val(n);
			ck_assert_msg(v == vals_result[j], "%s: Order mismatch (%zu), got=%lld, expected=%lld", p, i, v, vals_result[j]);

			n = M_llist_u64_node_next(n);
		}
		n = M_llist_u64_last(d);
		ck_assert_msg(n != NULL, "%s: Premature end of list. Should have last node", p);
		n = M_llist_u64_node_next(n);
		ck_assert_msg(n == NULL, "%s: End of list expected", p);

		/* prev. */
		n = M_llist_u64_last(d);
		for (j=VALS_LEN; j-->0; ) {
			ck_assert_msg(n != NULL, "%s: Premature end of list", p);
			if (n == NULL) {
				break;
			}

			v = M_llist_u64_node_val(n);
			ck_assert_msg(v == vals_result[j], "%s: Order mismatch (%zu), got=%lld, expected=%lld", p, i, v, vals_result[j]);

			n = M_llist_u64_node_prev(n);
		}

		M_llist_u64_destroy(d);
	}
}
END_TEST

START_TEST(check_llist_u64_duplicate)
{
	M_llist_u64_t      *d;
	M_llist_u64_t      *dupd;
	M_llist_u64_node_t *n;
	const char         *p;
	const M_uint64     *vals_result;
	M_uint64            v;
	size_t              i;
	size_t              j;

	for (i=0; check_llist_u64_generic_data[i].prefix!=NULL; i++) {
		p           = check_llist_u64_generic_data[i].prefix;
		vals_result = generic_vals_ordered[check_llist_u64_generic_data[i].vals_ordered_idx];
		d           = M_llist_u64_create(check_llist_u64_generic_data[i].flags);

		for (j=0; j<VALS_LEN; j++) {
			M_llist_u64_insert(d, generic_vals[j]);
		}

		dupd = M_llist_u64_duplicate(d);

		/* Remove the first element then destroy the original list to ensure we have a real duplicate. */
		M_llist_u64_remove_node(M_llist_u64_first(d));
		M_llist_u64_destroy(d);

		/* Verify we have the same items that were in the original list. */
		n = M_llist_u64_first(dupd);
		for (j=0; j<VALS_LEN; j++) {
			ck_assert_msg(n != NULL, "%s: Premature end of list", p);
			if (n == NULL) {
				break;
			}

			v = M_llist_u64_node_val(n);
			ck_assert_msg(v == vals_result[j], "%s: Order mismatch (%zu), got=%lld, expected=%lld", p, i, v, vals_result[j]);

			n = M_llist_u64_node_next(n);
		}

		M_llist_u64_destroy(dupd);
	}
}
END_TEST

START_TEST(check_llist_u64_merge)
{
	M_llist_u64_t      *d1;
	M_llist_u64_t      *d2;
	M_llist_u64_node_t *n;
	const char         *p;
	const M_uint64     *vals_result;
	size_t              i;
	size_t              j;
	M_uint64            v;
	M_uint64            vals1[] = { 7, 9, 1, 2 };
	M_uint64            vals2[] = { 8, 1, 5, 9 };
	/* Note: 0s are     filler and will be ignored. */
	M_uint64        vals_merged[8][8] = {
		{ 7, 9, 1, 2, 8, 1, 5, 9 },
		{ 7, 9, 1, 2, 8, 5, 0, 0 },
		{ 7, 9, 1, 2, 1, 5, 8, 9 },
		{ 7, 9, 1, 2, 5, 8, 0, 0 },

		{ 1, 1, 2, 5, 7, 8, 9, 9 },
		{ 1, 2, 5, 7, 8, 9, 0, 0 },

		{ 9, 9, 8, 7, 5, 2, 1, 1 },
		{ 9, 8, 7, 5, 2, 1, 0, 0 },
	};
	static struct {
		const char          *prefix;
		M_llist_u64_flags_t  flags;
		M_llist_u64_flags_t  flags2;
		int                  vals_merged_idx;
		M_bool               include_duplicates;
	}                  data[] = {
		{ "Unordered - dups",      M_LLIST_U64_NONE,     M_LLIST_U64_NONE,     0, M_TRUE  },
		{ "Unordered - nodups",    M_LLIST_U64_NONE,     M_LLIST_U64_NONE,     1, M_FALSE },
		{ "Unordered - dups f2",   M_LLIST_U64_NONE,     M_LLIST_U64_SORTASC,  2, M_TRUE  },
		{ "Unordered - nodups f2", M_LLIST_U64_NONE,     M_LLIST_U64_SORTASC,  3, M_FALSE },

		{ "SortAsc - dups",        M_LLIST_U64_SORTASC,  M_LLIST_U64_SORTASC,  4, M_TRUE  },
		{ "SortAsc - no dups",     M_LLIST_U64_SORTASC,  M_LLIST_U64_SORTASC,  5, M_FALSE },
		{ "SortAsc - dups f2",     M_LLIST_U64_SORTASC,  M_LLIST_U64_NONE,     4, M_TRUE  },
		{ "SortAsc - no dups f2",  M_LLIST_U64_SORTASC,  M_LLIST_U64_SORTDESC, 5, M_FALSE },

		{ "SortDesc - dups",       M_LLIST_U64_SORTDESC, M_LLIST_U64_SORTDESC, 6, M_TRUE  },
		{ "SortDesc - no dups",    M_LLIST_U64_SORTDESC, M_LLIST_U64_SORTDESC, 7, M_FALSE },
		{ "SortDesc - dups f2",    M_LLIST_U64_SORTDESC, M_LLIST_U64_NONE,     6, M_TRUE  },
		{ "SortDesc - no dups f2", M_LLIST_U64_SORTDESC, M_LLIST_U64_SORTASC,  7, M_FALSE },
		{ NULL, 0, 0, 0, M_FALSE }
	};

	for (i=0; data[i].prefix!=NULL; i++) {
		p           = data[i].prefix;
		vals_result = vals_merged[data[i].vals_merged_idx];
		d1          = M_llist_u64_create(data[i].flags);
		d2          = M_llist_u64_create(data[i].flags2);

		for (j=0; j<sizeof(vals1)/sizeof(*vals1); j++) {
			M_llist_u64_insert(d1, vals1[j]);
		}
		for (j=0; j<sizeof(vals2)/sizeof(*vals2); j++) {
			M_llist_u64_insert(d2, vals2[j]);
		}

		M_llist_u64_merge(&d1, d2, data[i].include_duplicates);

		n = M_llist_u64_first(d1);
		for (j=0; j<8; j++) {
			if (vals_result[j] == 0) {
				break;
			}

			ck_assert_msg(n !=  NULL, "%s: Premature end of list (%zu)", p, j);
			if (n == NULL) {
				break;
			}

			v = M_llist_u64_node_val(n);
			ck_assert_msg(v == vals_result[j], "%s: Order mismatch (%zu), got=%lld, expected=%lld", p, j, v, vals_result[j]);

			n = M_llist_u64_node_next(n);
		}

		M_llist_u64_destroy(d1);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_llist_u64_suite(void)
{
	Suite *suite;
	TCase *tc_llist_u64_insert;
	TCase *tc_llist_u64_insert_before_after;
	TCase *tc_llist_u64_first_last_find;
	TCase *tc_llist_u64_take_remove_count;
	TCase *tc_llist_u64_next_prev;
	TCase *tc_llist_u64_duplicate;
	TCase *tc_llist_u64_merge;

	suite = suite_create("llist_u64");

	tc_llist_u64_insert = tcase_create("check_llist_u64_insert");
	tcase_add_test(tc_llist_u64_insert, check_llist_u64_insert);
	suite_add_tcase(suite, tc_llist_u64_insert);

	tc_llist_u64_insert_before_after = tcase_create("check_llist_u64_insert_before_after");
	tcase_add_test(tc_llist_u64_insert_before_after, check_llist_u64_insert_before_after);
	suite_add_tcase(suite, tc_llist_u64_insert_before_after);

	tc_llist_u64_first_last_find = tcase_create("check_llist_u64_first_last_find");
	tcase_add_test(tc_llist_u64_first_last_find, check_llist_u64_first_last_find);
	suite_add_tcase(suite, tc_llist_u64_first_last_find);

	tc_llist_u64_take_remove_count = tcase_create("check_llist_u64_take_remove_count");
	tcase_add_test(tc_llist_u64_take_remove_count, check_llist_u64_take_remove_count);
	suite_add_tcase(suite, tc_llist_u64_take_remove_count);

	tc_llist_u64_next_prev = tcase_create("check_llist_u64_next_prev");
	tcase_add_test(tc_llist_u64_next_prev, check_llist_u64_next_prev);
	suite_add_tcase(suite, tc_llist_u64_next_prev);

	tc_llist_u64_duplicate = tcase_create("check_llist_u64_duplicate");
	tcase_add_test(tc_llist_u64_duplicate, check_llist_u64_duplicate);
	suite_add_tcase(suite, tc_llist_u64_duplicate);

	tc_llist_u64_merge = tcase_create("check_llist_u64_merge");
	tcase_add_test(tc_llist_u64_merge, check_llist_u64_merge);
	suite_add_tcase(suite, tc_llist_u64_merge);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_llist_u64_suite());
	srunner_set_log(sr, "check_llist_u64.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
