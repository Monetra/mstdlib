#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

#define ORDER_NUM_ENTIRES 10000
#define ORDER_ONE 4
#define ORDER_TWO 2
#define ORDER_THREE 5
#define DUP_NUM_ENTIRES 50
#define MERGE_NUM_ENTIRES (DUP_NUM_ENTIRES * 2)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_list_u64_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_u64_t *list = NULL;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void setup(void)
{
	list = M_list_u64_create(M_LIST_U64_NONE);
}

static void setup_asc(void)
{
	list = M_list_u64_create(M_LIST_U64_SORTASC);
}

static void teardown(void)
{
	M_list_u64_destroy(list);
	list = NULL;
}

/* - - - - - - - - - - - - - - - - - - - - */
/* Utility Functions - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - */

static M_uint64 random_insert(void)
{
	M_uint64 r;
	r = (M_uint64)rand();
	M_list_u64_insert(list, r);
	return r;
}

static void ensure_len(size_t e_entries)
{
	size_t r_entries = M_list_u64_len(list);
	ck_assert_msg(r_entries == e_entries, "expected %zu, got %zu", e_entries, r_entries);
}

static void ensure_val(size_t idx, M_uint64 val)
{
	M_uint64 myval;
	myval = M_list_u64_at(list, idx);
	ck_assert_msg(myval == val, "value %llu does not match expected value %llu", myval, val);
}

static void ensure_order(void)
{
	size_t i;
	size_t len;

	len = M_list_u64_len(list);
	for (i=0; i<len-1; i++) {
		ck_assert_msg(M_list_u64_at(list, i) <= M_list_u64_at(list, i+1), "Order not maintained");
	}
}

static void ensure_order_desc(void)
{
	size_t i;
	size_t len;

	len = M_list_u64_len(list);
	for (i=0; i<len-1; i++) {
		ck_assert_msg(M_list_u64_at(list, i) >= M_list_u64_at(list, i+1), "Order not maintained");
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check adding values to an unsorted list works properly.
 *
 * Checks:
 *   * The list expands as values are inserted and the expand boundary is
 *     passed.
 *   * Values added to the list are correct when read from the list.
 */
START_TEST(check_insert)
{
	size_t   i;
	M_uint64 r;

	/* initial conditions */
	ensure_len(0);

	for (i=0; i<ORDER_NUM_ENTIRES; i++) {
		r = random_insert();
		ensure_len(i+1);
		ensure_val(i, r);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check insertion sorting works properly.
 */
START_TEST(check_insert_order)
{
	size_t i;

	ensure_len(0);

	srand(1);
	for (i=0; i<ORDER_NUM_ENTIRES; i++) {
		random_insert();
	}
	ensure_len(ORDER_NUM_ENTIRES);

	ensure_order();
}
END_TEST

/* Check bulk insert which disables inserstion sorting and sorts after all
 * entries are added is working properly
 */
START_TEST(check_bulk_insert_order)
{
	size_t i;
	M_uint64 val1;
	M_uint64 val2;

	ensure_len(0);
	M_list_u64_insert_begin(list);

	/* Add two entires that will be out of order so
	 * we can check that insertion sorting isn't happening */
	M_list_u64_insert(list, ORDER_ONE);
	M_list_u64_insert(list, ORDER_TWO);
	val1 = M_list_u64_at(list, 0);
	val2 = M_list_u64_at(list, 1);
	ck_assert_msg(val1 != ORDER_ONE || val2 != ORDER_TWO || val1 > val2, "Bulk insertion not overriding insertion sort, val1: %llu, val2: %llu", val1, val2);

	/* Add some random values */
	srand(1);
	for (i=0; i<ORDER_NUM_ENTIRES-2; i++) {
		random_insert();
	}
	ensure_len(ORDER_NUM_ENTIRES);

	M_list_u64_insert_end(list);
	ensure_order();
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check index of can find a given value and the value can be removed.
 */
START_TEST(check_index_of_remove)
{
	size_t i;
	size_t idx;
	M_bool res;
	M_uint64 val = 20;

	list = M_list_u64_create(M_LIST_U64_NONE);
	for (i=0; i<50; i++) {
		M_list_u64_insert(list, i);
	}

	res = M_list_u64_index_of(list, val, &idx);
	ck_assert_msg(res && idx == val, "Index of did not find value at proper index: %llu. Index is: %zu", val, idx);

	i = M_list_u64_remove_val(list, val, M_LIST_U64_MATCH_VAL);
	ck_assert_msg(i == 1, "Could  not remove value: %llu", val);

	res = M_list_u64_index_of(list, val, &idx);
	ck_assert_msg(!res, "Found value: %llu that was removed", val);

	ensure_len(49);

	/* Remove a specific index */
	M_list_u64_remove_at(list, 3);
	ensure_len(48);

	/* Remove a range of values */
	M_list_u64_remove_range(list, 4, 8);
	ensure_len(43);

	M_list_u64_destroy(list);
	list = NULL;

	/* Remove value occurring multiple times unsorted. */
	list = M_list_u64_create(M_LIST_U64_NONE);
	for (i=0;i<50;i++) {
		M_list_u64_insert(list, i);
		if (i % 10 == 0) {
			M_list_u64_insert(list, val);
		}
	}
	ensure_len(55);

	i = M_list_u64_count(list, val);
	ck_assert_msg(i == 6, "Invalid count of val (%zu), got: %zu, expected: 6", val, i);

	i = M_list_u64_remove_val(list, val, M_LIST_U64_MATCH_VAL|M_LIST_U64_MATCH_ALL);
	ck_assert_msg(i == 6, "Could  not remove value: %llu", val);
	ensure_len(49);

	i = M_list_u64_count(list, val);
	ck_assert_msg(i == 0, "Invalid count of val (%zu), got: %zu, expected: 0", val, i);

	M_list_u64_destroy(list);
	list = NULL;

	/* Remove value occurring multiple times sorted. */
	list = M_list_u64_create(M_LIST_U64_SORTASC);
	for (i=0;i<50;i++) {
		M_list_u64_insert(list, i);
		if (i % 10 == 0) {
			M_list_u64_insert(list, val);
		}
	}
	ensure_len(55);

	i = M_list_u64_count(list, val);
	ck_assert_msg(i == 6, "Invalid count of val (%zu), got: %zu, expected: 6", val, i);

	i = M_list_u64_remove_val(list, val, M_LIST_U64_MATCH_VAL|M_LIST_U64_MATCH_ALL);
	ck_assert_msg(i == 6, "Could  not remove value: %llu", val);
	ensure_len(49);

	i = M_list_u64_count(list, val);
	ck_assert_msg(i == 0, "Invalid count of val (%zu), got: %zu, expected: 0", val, i);

	M_list_u64_destroy(list);
	list = NULL;
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_duplicate_merge)
{
	size_t         i;
	size_t         len;
	M_list_u64_t *d2 = NULL;
	M_list_u64_t *d3 = NULL;
	M_list_u64_t *d4 = NULL;

	for (i=0; i<DUP_NUM_ENTIRES; i++) {
		M_list_u64_insert(list, i);
	}

	d2 = M_list_u64_duplicate(list);
	len = M_list_u64_len(d2);
	ck_assert_msg(len == DUP_NUM_ENTIRES, "Dup: expected %zu, got %zu", DUP_NUM_ENTIRES, len);

	d3 = M_list_u64_duplicate(list);
	M_list_u64_merge(&d2, d3, M_FALSE);
	len = M_list_u64_len(d2);
	ck_assert_msg(len == DUP_NUM_ENTIRES, "Merge no dups: expected %zu, got %zu", DUP_NUM_ENTIRES, len);
	
	d4 = M_list_u64_duplicate(list);
	M_list_u64_merge(&d2, d4, M_TRUE);
	len = M_list_u64_len(d2);
	ck_assert_msg(len == MERGE_NUM_ENTIRES, "Merge with dups: expected %zu, got %zu", MERGE_NUM_ENTIRES, len);

	M_list_u64_destroy(d2);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_remove_dups)
{
	size_t i;

	for (i=0; i<DUP_NUM_ENTIRES; i++) {
		M_list_u64_insert(list, ORDER_ONE);
	}
	ensure_len(DUP_NUM_ENTIRES);
	M_list_u64_remove_duplicates(list);
	ensure_len(1);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_change_sorting)
{
	size_t i;
	M_uint64 val1;
	M_uint64 val2;
	M_uint64 val3;

	/* Add two entires that will be out of order so
	 * we can check that insertion sorting isn't happening */
	M_list_u64_insert(list, ORDER_ONE);
	M_list_u64_insert(list, ORDER_TWO);
	M_list_u64_insert(list, ORDER_THREE);
	val1 = M_list_u64_at(list, 0);
	val2 = M_list_u64_at(list, 1);
	val3 = M_list_u64_at(list, 3);
	ck_assert_msg(val1 != ORDER_ONE || val2 != ORDER_TWO || val3 != ORDER_THREE || val1 > val2 || val2 < val3, "insertion not unsorted, val1: %llu, val2: %llu, val3: %llu", val1, val2, val3);

	/* Add some random values. */
	srand(1);
	for (i=0; i<ORDER_NUM_ENTIRES-3; i++) {
		random_insert();
	}
	ensure_len(ORDER_NUM_ENTIRES);

	/* Set this to sorted and add check that the values were sorted. */
	M_list_u64_change_sorting(list, M_LIST_U64_SORTDESC);
	ensure_order_desc();

	/* Add some more values and ensure they are being sorted on insert still. */
	for (i=0; i<ORDER_NUM_ENTIRES; i++) {
		random_insert();
	}
	ensure_len(ORDER_NUM_ENTIRES*2);
	ensure_order_desc();

	/* Check disabling sorting works. */
	M_list_u64_change_sorting(list, M_LIST_U64_NONE);
	M_list_u64_insert(list, ORDER_ONE);
	M_list_u64_insert(list, ORDER_TWO);
	M_list_u64_insert(list, ORDER_THREE);
	ensure_len(ORDER_NUM_ENTIRES*2+3);
	val1 = M_list_u64_at(list, M_list_u64_len(list)-3);
	val2 = M_list_u64_at(list, M_list_u64_len(list)-2);
	val3 = M_list_u64_at(list, M_list_u64_len(list)-1);
	ck_assert_msg(val1 != ORDER_ONE || val2 != ORDER_TWO || val3 != ORDER_THREE || val1 > val2 || val2 < val3, "insertion not unsorted, val1: %llu, val2: %llu, val3: %llu", val1, val2, val3);
}
END_TEST

START_TEST(check_queue_stack)
{
	M_uint64 vals[]   = { 1, 7, 2, 9, 8, 10, 22, 3, 4, 3, 9, 8, 99, 2 };
	M_uint64 val;
	size_t   num_vals = 0;
	size_t   i;
	size_t   idx;
	M_bool   ret;

	num_vals = sizeof(vals)/sizeof(*vals);

	/* Queue. */
	/* take_first */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	for (i=0; i<num_vals; i++) {
		val = M_list_u64_take_first(list);
		ck_assert_msg(val == vals[i], "Queue (take_first) vals[%zu] is not correct, expected: %lld, got: %lld", i, vals[i], val);
	}
	M_list_u64_destroy(list);

	/* take_last */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	for (i=0; i<num_vals; i++) {
		idx = num_vals-i-1;
		val = M_list_u64_take_last(list);
		ck_assert_msg(val == vals[idx], "Queue (take_last) vals[%zu] is not correct, expected: %lld, got: %lld", idx, vals[idx], val);
	}
	M_list_u64_destroy(list);

	/* take_at */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	val = M_list_u64_take_at(list, 7);
	ck_assert_msg(val == 3, "Queue (take_at) vals[7] is not correct, expected: %lld, got: %lld", 3, val);

	/* remove_at */
	M_list_u64_remove_at(list, 0);
	val = M_list_u64_at(list, 0);
	ck_assert_msg(val == 7, "Queue (remove_at(0)) val is not correct, expected: %lld, got: %lld", 7, val);
	M_list_u64_remove_at(list, M_list_u64_len(list)-1);
	val = M_list_u64_at(list, M_list_u64_len(list)-1);
	ck_assert_msg(val == 99, "Queue (remove_at(len-1)) val is not correct, expected: %lld, got: %lld", 99, val);

	/* index_of */
	ret = M_list_u64_index_of(list, 8, &idx);
	ck_assert_msg(ret == M_TRUE, "Queue (index_of(8) could not get index of value");
	ck_assert_msg(idx == 3, "Queue (index_of(8)) idx is not correct, expected: %zu, got: %zu", 3, idx);

	/* remove_duplicates */
	M_list_u64_remove_range(list, 0, M_list_u64_len(list));
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	M_list_u64_remove_duplicates(list);
	ensure_len(num_vals-4);

	M_list_u64_destroy(list);
	

	/* Stack. */
	/* take_first */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK|M_LIST_U64_STACK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	for (i=0; i<num_vals; i++) {
		idx = num_vals-1-i;
		val = M_list_u64_take_first(list);
		ck_assert_msg(val == vals[idx], "Stack (take_first) vals[%zu] is not correct, expected: %lld, got: %lld", idx, vals[idx], val);
	}
	M_list_u64_destroy(list);

	/* take_last */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK|M_LIST_U64_STACK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	for (i=0; i<num_vals; i++) {
		val = M_list_u64_take_last(list);
		ck_assert_msg(val == vals[i], "Stack (take_last) vals[%zu] is not correct, expected: %lld, got: %lld", i, vals[i], val);
	}
	M_list_u64_destroy(list);

	/* take_at */
	list = M_list_u64_create(M_LIST_U64_NEVERSHRINK|M_LIST_U64_STACK);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	ensure_len(num_vals);
	val = M_list_u64_take_at(list, 7);
	ck_assert_msg(val == 22, "Stack (take_at) vals[7] is not correct, expected: %lld, got: %lld", 3, val);

	/* remove_at */
	M_list_u64_remove_at(list, 0);
	val = M_list_u64_at(list, 0);
	ck_assert_msg(val == 99, "Stack (remove_at(0)) val is not correct, expected: %lld, got: %lld", 99, val);
	M_list_u64_remove_at(list, M_list_u64_len(list)-1);
	val = M_list_u64_at(list, M_list_u64_len(list)-1);
	ck_assert_msg(val == 7, "Stack (remove_at(len-1)) val is not correct, expected: %lld, got: %lld", 7, val);

	/* index_of */
	ret = M_list_u64_index_of(list, 8, &idx);
	ck_assert_msg(ret == M_TRUE, "Stack (index_of(8) could not get index of value");
	ck_assert_msg(idx == 7, "Stack (index_of(8)) idx is not correct, expected: %zu, got: %zu", 7, idx);

	/* remove_duplicates */
	M_list_u64_remove_range(list, 0, M_list_u64_len(list));
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(list, vals[i]);
	}
	M_list_u64_remove_duplicates(list);
	ensure_len(num_vals-4);

	M_list_u64_destroy(list);
}
END_TEST

static void check_set_insert(const char *prefix, M_uint64 *vals, size_t num_vals, M_uint64 *after_vals, size_t num_after_vals, M_uint32 flags)
{
	M_list_u64_t *l;
	M_uint64      val;
	size_t        i;
	size_t        len;

	flags |= M_LIST_U64_SET;
	l      = M_list_u64_create(flags);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(l, vals[i]);
	}

	len = M_list_u64_len(l);
	ck_assert_msg(len == num_after_vals, "SET Insert %s: length not correct, expected: %zu, got: %zu", num_after_vals, len);

	for (i=0; i<num_after_vals; i++) {
		val = M_list_u64_at(l, i);
		ck_assert_msg(val == after_vals[i], "SET Insert %s: after_vals[%zu] is not correct, expected: %lld, got: %lld", prefix, i, after_vals[i], val);
	}

	M_list_u64_destroy(l);
}

static void check_set_duplicate(const char *prefix, M_uint64 *vals, size_t num_vals, M_uint64 *after_vals, size_t num_after_vals, M_uint32 flags)
{
	M_list_u64_t *l;
	M_list_u64_t *l2;
	M_uint64      val;
	size_t        i;
	size_t        len;

	flags |= M_LIST_U64_SET;
	l      = M_list_u64_create(flags);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(l, vals[i]);
	}
	l2 = M_list_u64_duplicate(l);
	M_list_u64_destroy(l);
	l  = l2;

	len = M_list_u64_len(l);
	ck_assert_msg(len == num_after_vals, "SET Duplicate %s: length not correct, expected: %zu, got: %zu", num_after_vals, len);

	for (i=0; i<num_after_vals; i++) {
		val = M_list_u64_at(l, i);
		ck_assert_msg(val == after_vals[i], "SET Duplicate %s: after_vals[%zu] is not correct, expected: %lld, got: %lld", prefix, i, after_vals[i], val);
	}

	M_list_u64_destroy(l);
}

static void check_set_merge(const char *prefix, M_uint64 *vals, size_t num_vals, M_uint64 *after_vals, size_t num_after_vals, M_uint32 flags)
{
	M_list_u64_t *l;
	M_list_u64_t *l2;
	M_uint64      val;
	size_t        i;
	size_t        len;

	flags |= M_LIST_U64_SET;
	l      = M_list_u64_create(flags);
	for (i=0; i<num_vals; i++) {
		M_list_u64_insert(l, vals[i]);
	}
	l2 = M_list_u64_duplicate(l);
	/* include_duplicates == M_TRUE should be ignored since this is a set. */
	M_list_u64_merge(&l, l2, M_TRUE);

	len = M_list_u64_len(l);
	ck_assert_msg(len == num_after_vals, "SET Merge %s: length not correct, expected: %zu, got: %zu", num_after_vals, len);

	for (i=0; i<num_after_vals; i++) {
		val = M_list_u64_at(l, i);
		ck_assert_msg(val == after_vals[i], "SET Merge %s: after_vals[%zu] is not correct, expected: %lld, got: %lld", prefix, i, after_vals[i], val);
	}

	M_list_u64_destroy(l);
}

START_TEST(check_set)
{
	M_uint64 vals[]        = { 1, 7, 2, 9, 8, 10, 22, 3, 4, 3, 9, 8, 99, 2 };
	M_uint64 after_vals[]  = { 1, 7, 2, 9, 8, 10, 22, 3, 4, 99 };
	M_uint64 svals[]       = { 1, 7, 2, 9, 8, 10, 22, 3, 4, 3, 9, 8, 99, 2 };
	M_uint64 safter_vals[] = { 1, 2, 3, 4, 7, 8, 9, 10, 22, 99 };

	check_set_insert("sorted", svals, sizeof(svals)/sizeof(*svals), safter_vals, sizeof(safter_vals)/sizeof(*safter_vals), M_LIST_U64_SORTASC);
	check_set_insert("unsorted", vals, sizeof(vals)/sizeof(*vals), after_vals, sizeof(after_vals)/sizeof(*after_vals), M_LIST_U64_NONE);

	check_set_duplicate("sorted", svals, sizeof(svals)/sizeof(*svals), safter_vals, sizeof(safter_vals)/sizeof(*safter_vals), M_LIST_U64_SORTASC);
	check_set_duplicate("unsorted", vals, sizeof(vals)/sizeof(*vals), after_vals, sizeof(after_vals)/sizeof(*after_vals), M_LIST_U64_NONE);

	check_set_merge("sorted", svals, sizeof(svals)/sizeof(*svals), safter_vals, sizeof(safter_vals)/sizeof(*safter_vals), M_LIST_U64_SORTASC);
	check_set_merge("unsorted", vals, sizeof(vals)/sizeof(*vals), after_vals, sizeof(after_vals)/sizeof(*after_vals), M_LIST_U64_NONE);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_list_u64_suite(void)
{
	Suite *suite = suite_create("list_u64");

	TCase *tc_insert;
	TCase *tc_insert_order;
	TCase *tc_bulk_insert_order;
	TCase *tc_index_of_remove;
	TCase *tc_duplicate_merge;
	TCase *tc_remove_dups;
	TCase *tc_change_sorting;
	TCase *tc_queue_stack;
	TCase *tc_set;

	tc_insert = tcase_create("list_u64_insert");
	tcase_add_unchecked_fixture(tc_insert, setup, teardown);
	tcase_add_test(tc_insert, check_insert);
	suite_add_tcase(suite, tc_insert);

	tc_insert_order = tcase_create("list_u64_insert_order");
	tcase_set_timeout(tc_insert_order, 40);
	tcase_add_unchecked_fixture(tc_insert_order, setup_asc, teardown);
	tcase_add_test(tc_insert_order, check_insert_order);
	suite_add_tcase(suite, tc_insert_order);

	tc_bulk_insert_order = tcase_create("list_u64_bulk_insert_order");
	tcase_set_timeout(tc_bulk_insert_order, 40);
	tcase_add_unchecked_fixture(tc_bulk_insert_order, setup_asc, teardown);
	tcase_add_test(tc_bulk_insert_order, check_bulk_insert_order);
	suite_add_tcase(suite, tc_bulk_insert_order);

	tc_index_of_remove = tcase_create("list_u64_index_of_remove");
	tcase_add_test(tc_index_of_remove, check_index_of_remove);
	suite_add_tcase(suite, tc_index_of_remove);

	tc_duplicate_merge = tcase_create("list_u64_duplicate_merge");
	tcase_add_unchecked_fixture(tc_duplicate_merge, setup, teardown);
	tcase_add_test(tc_duplicate_merge, check_duplicate_merge);
	suite_add_tcase(suite, tc_duplicate_merge);

	tc_remove_dups = tcase_create("list_u64_remove_dups");
	tcase_add_unchecked_fixture(tc_remove_dups, setup, teardown);
	tcase_add_test(tc_remove_dups, check_remove_dups);
	suite_add_tcase(suite, tc_remove_dups);

	tc_change_sorting = tcase_create("list_u64_change_sorting");
	tcase_add_unchecked_fixture(tc_change_sorting, setup, teardown);
	tcase_add_test(tc_change_sorting, check_change_sorting);
	suite_add_tcase(suite, tc_change_sorting);

	tc_queue_stack = tcase_create("list_u64_queue_stack");
	tcase_add_unchecked_fixture(tc_queue_stack, NULL, NULL);
	tcase_add_test(tc_queue_stack, check_queue_stack);
	suite_add_tcase(suite, tc_queue_stack);

	tc_set = tcase_create("list_u64_set");
	tcase_add_unchecked_fixture(tc_set, NULL, NULL);
	tcase_add_test(tc_set, check_set);
	suite_add_tcase(suite, tc_set);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_list_u64_suite());
	srunner_set_log(sr, "check_list_u64.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
