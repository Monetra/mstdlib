#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_hash_u64str_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	M_uint64    key;
	const char *val;
} kv_pairs[] = {
	{ 2, "b"  },
	{ 1, "a"  },
	{ 4, "d"  },
	{ 3, "c"  },
	{ 0,  NULL}
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_ordered(const char *key_result, const char *val_result, M_uint32 flags)
{
	M_uint64              key;
	const char           *val;
	M_buf_t              *key_buf;
	M_buf_t              *val_buf;
	M_hash_u64str_enum_t *d_enum;
	M_hash_u64str_t      *d;
	size_t                i;
	char                 *out;

	key_buf = M_buf_create();
	val_buf = M_buf_create();
	d       = M_hash_u64str_create(8, 75, flags);

	/* Load the data into the hashtable. */
	for (i=0; kv_pairs[i].key != 0; i++)
		ck_assert_msg(M_hash_u64str_insert(d, kv_pairs[i].key, kv_pairs[i].val), "%zu: insert failed", i);

	/* Check the data was set correctly. */
	for (i=0; kv_pairs[i].key != 0; i++)
		ck_assert_msg(M_str_eq(M_hash_u64str_get_direct(d, kv_pairs[i].key), kv_pairs[i].val), "%zu: get failed", i);

	/* Enumerate the data and fill in our result data buffers. */
	ck_assert_msg(M_hash_u64str_enumerate(d, &d_enum) > 0, "enumerate failed");
	while (M_hash_u64str_enumerate_next(d, d_enum, &key, &val)) {
		M_buf_add_uint(key_buf, key);
		M_buf_add_str(val_buf, val);
	}
	M_hash_u64str_enumerate_free(d_enum);

	/* Check the keys are in correct order. */
	out = M_buf_finish_str(key_buf, NULL);
	ck_assert_msg(M_str_eq(out, key_result), "%s != %s", out, key_result);
	M_free(out);

	/* Check the values are in correct order. */
	out = M_buf_finish_str(val_buf, NULL);
	ck_assert_msg(M_str_eq(out, val_result), "%s != %s", out, val_result);
	M_free(out);

	M_hash_u64str_destroy(d);
}

START_TEST(check_ordered_insert)
{
	check_ordered("2143", "badc", M_HASH_U64STR_KEYS_ORDERED);
}
END_TEST

START_TEST(check_ordered_sort)
{
	check_ordered("1234", "abcd", M_HASH_U64STR_KEYS_ORDERED|M_HASH_U64STR_KEYS_SORTASC);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_hash_u64str_suite(void)
{
	Suite *suite = suite_create("hash_u64str");
	TCase *tc_ordered_insert;
	TCase *tc_ordered_sort;

	tc_ordered_insert = tcase_create("hash_u64str_ordered_insert");
	tcase_add_unchecked_fixture(tc_ordered_insert, NULL, NULL);
	tcase_add_test(tc_ordered_insert, check_ordered_insert);
	suite_add_tcase(suite, tc_ordered_insert);

	tc_ordered_sort = tcase_create("hash_u64str_ordered_sort");
	tcase_add_unchecked_fixture(tc_ordered_sort, NULL, NULL);
	tcase_add_test(tc_ordered_sort, check_ordered_sort);
	suite_add_tcase(suite, tc_ordered_sort);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_hash_u64str_suite());
	srunner_set_log(sr, "check_hash_u64str.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
