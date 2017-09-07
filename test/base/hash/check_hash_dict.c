#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_hash_dict_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_hash_dict_t *dict = NULL;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define ARYLEN(ary) (sizeof(ary)/sizeof(*ary))

static M_uint32 create_invalid_size[]    = {0};
static M_uint32 create_valid_size[]      = {1,M_HASHTABLE_MAX_BUCKETS};
static M_uint8  create_invalid_pctfill[] = {100};
static M_uint8  create_valid_pctfill[]   = {1,99};

static void ensure_create_is_null(M_uint32 size, M_uint8 pctfill)
{
	M_hash_dict_t *d = M_hash_dict_create(size, pctfill, M_HASH_DICT_CASECMP);
	ck_assert_msg(d == NULL, "not null when size=%u pctfill=%u", size, pctfill);
}

START_TEST(check_create_invalid_pctfill)
{
	size_t i,j;
	for (i=0; i<ARYLEN(create_valid_size); i++) {
		for (j=0; j<ARYLEN(create_invalid_pctfill); j++) {
			M_uint32 size = create_valid_size[i];
			M_uint8 pctfill = create_invalid_pctfill[j];
			ensure_create_is_null(size, pctfill);
		}
	}
}
END_TEST

START_TEST(check_create_invalid_size)
{
	size_t i,j;
	for (i=0; i<ARYLEN(create_invalid_size); i++) {
		for (j=0; j<ARYLEN(create_valid_pctfill); j++) {
			M_uint32 size = create_invalid_size[i];
			M_uint8 pctfill = create_valid_pctfill[j];
			ensure_create_is_null(size, pctfill);
		}
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_create_pow2_help(size_t size)
{
	M_hash_dict_t *d = M_hash_dict_create(size, create_valid_pctfill[0], M_HASH_DICT_CASECMP);
	if (d != NULL) {
		size_t r_size = M_hash_dict_size(d);
		size_t e_size = M_size_t_round_up_to_power_of_two(size);
		if (e_size > M_HASHTABLE_MAX_BUCKETS)
			e_size = M_HASHTABLE_MAX_BUCKETS;
		ck_assert_msg(r_size == e_size, "for %zu, expected %zu, got %zu", size, e_size, r_size);
		M_hash_dict_destroy(d);
	} else {
		size_t r_size = size;
		size_t e_size = 0;
		ck_assert_msg(r_size == e_size, "for %zu, expected %zu, got %zu", size, e_size, r_size);
	}
}

START_TEST(check_create_pow2)
{
	size_t n;
	check_create_pow2_help(0);
	check_create_pow2_help(1);
	check_create_pow2_help(2);
	check_create_pow2_help(3);
	for (n=4; n<=M_HASHTABLE_MAX_BUCKETS; n<<=1) {
		check_create_pow2_help(n-1);
		check_create_pow2_help(n);
		check_create_pow2_help(n+1);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define DICT_INITIAL_SIZE    2
#define DICT_INITIAL_LOAD   50

static void setup(void)
{
	dict =
	    M_hash_dict_create(
	        DICT_INITIAL_SIZE,
	        DICT_INITIAL_LOAD,
			M_HASH_DICT_CASECMP
	    );
}

static void teardown(void)
{
	M_hash_dict_destroy(dict);
	dict = NULL;
}

/* - - - - - - - - - - - - - - - - - - - - */
/* Utility Functions - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - */

static const char *random_string(void)
{
	static const char alphanum[] =
		"0123456789"
		"abcdefghijklmnopqrstuvwxyz";
	static char s[32];
	size_t len = sizeof(s)-1;
	size_t i;

	for (i=0; i<len; i++)
		s[i] = alphanum[(size_t)rand() % (sizeof(alphanum) - 1)];
	s[len] = 0;
	return s;
}

static void random_insert(void)
{
	const char *pkey = random_string();
	size_t e_num_entries = M_hash_dict_num_keys(dict);
	M_hash_dict_insert(dict, pkey, pkey);
	ck_assert_msg(M_hash_dict_num_keys(dict) == e_num_entries+1, "unexpected number of entries");
}

/* assumes randomly generated string does not exist in dictionary */
static void insert_random_strings(size_t n)
{
	size_t i;
	/* insert entries */
	for (i=0; i<n; i++) random_insert();
}

/* assumes randomly generated string exists in dictionary */
static M_bool random_remove(void)
{
	size_t e_num_entries = M_hash_dict_num_keys(dict);
	M_bool r = M_hash_dict_remove(dict, random_string());
	ck_assert_msg(M_hash_dict_num_keys(dict)+1 == e_num_entries, "unexpected number of entries");
	return r;
}

static void ensure_num_entries(size_t e_entries)
{
	size_t r_entries = M_hash_dict_num_keys(dict);
	ck_assert_msg(r_entries == e_entries, "expected %zu, got %zu", e_entries, r_entries);
}

static void ensure_size(size_t e_size)
{
	size_t r_size = M_hash_dict_size(dict);
	ck_assert_msg(r_size == e_size, "expected %zu, got %zu", e_size, r_size);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_insert)
{
	size_t e_entries = 0;
	size_t e_size = DICT_INITIAL_SIZE;

	/* initial conditions */
	ensure_num_entries(e_entries);
	ensure_size(DICT_INITIAL_SIZE);                   /* 0, 2 */

	random_insert(); ensure_num_entries(++e_entries); /* 1, 2 */
	/* reached 50% load, should grow */
	e_size <<= 1; ensure_size(e_size);                /* 1, 4 */

	random_insert(); ensure_num_entries(++e_entries); /* 2, 4 */
	/* reached 50% load, should grow */
	e_size <<= 1; ensure_size(e_size);                /* 2, 8 */

	random_insert(); ensure_num_entries(++e_entries); /* 3, 8 */
	random_insert(); ensure_num_entries(++e_entries); /* 4, 8 */
	/* reached 50% load, should grow */
	e_size <<= 1; ensure_size(e_size);                /* 4,16 */

	random_insert(); ensure_num_entries(++e_entries); /* 5,16 */
	random_insert(); ensure_num_entries(++e_entries); /* 6,16 */
	random_insert(); ensure_num_entries(++e_entries); /* 7,16 */
	random_insert(); ensure_num_entries(++e_entries); /* 8,16 */
	/* reached 50% load, should grow */
	e_size <<= 1; ensure_size(e_size);                /* 8,32 */
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CHECK_REMOVE_NUM 1024

START_TEST(check_remove)
{
	size_t e_entries;
	size_t i;

	ensure_num_entries(0);

	srand(1);
	/* insert entries */
	for (i=0; i<CHECK_REMOVE_NUM; i++) {
		e_entries = M_hash_dict_num_keys(dict);
		random_insert();
		ensure_num_entries(e_entries+1);
	}

	ensure_num_entries(CHECK_REMOVE_NUM);

	srand(1);
	/* remove entries */
	for (i=0; i<CHECK_REMOVE_NUM; i++) {
		e_entries = M_hash_dict_num_keys(dict);
		ck_assert_msg(random_remove(), "remove failed");
		ensure_num_entries(e_entries-1);
	}

	ensure_num_entries(0);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_get)
{
	size_t i;
	const char *key;
	const char *value;
	size_t to_insert = 1024;

	ensure_num_entries(0);

	srand(1);
	insert_random_strings(to_insert);

	ensure_num_entries(to_insert);

	srand(1);
	/* get entries */
	for (i=0; i<CHECK_REMOVE_NUM; i++) {
		key = random_string();
		ck_assert_msg(M_hash_dict_get(dict, key, &value), "get failed");
		ck_assert_msg(M_str_eq(key, value), "key did not match value");
	}
}
END_TEST

START_TEST(check_get_caseless)
{
	static const char *keys[] = {
		"key",
		"keY", "kEy", "Key",
		"kEY", "KEy", "KeY",
		"KEY",
	};
	size_t i;
	ck_assert_msg(M_hash_dict_insert(dict, *keys, *keys), "insert failed");
	for (i=0; i<ARYLEN(keys); i++)
		ck_assert_msg(M_hash_dict_get(dict, keys[i], NULL), "get failed");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_get_direct)
{
	size_t i;
	const char *key;
	size_t to_insert = 1024;

	ensure_num_entries(0);

	srand(1);
	insert_random_strings(to_insert);

	ensure_num_entries(to_insert);

	srand(1);
	/* get entries */
	for (i=0; i<CHECK_REMOVE_NUM; i++) {
		key = random_string();
		ck_assert_msg(M_str_eq(key, M_hash_dict_get_direct(dict, key)), "key did not match value");
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if 0

START_TEST(check_num_collisions)
	fail("implement me");
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_num_expansions)
	fail("implement me");
END_TEST
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_enumerate)
{
	M_hash_dict_t *dict2;
	M_hash_dict_enum_t *dict2_enum;
	const char *key;
	const char *value;

	insert_random_strings(1024);
	ensure_num_entries(1024);

	dict2 = M_hash_dict_duplicate(dict);
	ensure_num_entries(M_hash_dict_num_keys(dict2));
	ensure_size(M_hash_dict_size(dict2));

	ck_assert_msg(M_hash_dict_num_keys(dict2) == M_hash_dict_enumerate(dict2, &dict2_enum), "enumerate did not return correct size");
	/* remove all keys of duplicate dictionary from original dictionary
	 * failing if any remove operation is unsuccessful */
	while (M_hash_dict_enumerate_next(dict2, dict2_enum, &key, &value))
		ck_assert_msg(M_hash_dict_remove(dict, key), "remove failed");

	/* nothing should remain */
	ensure_num_entries(0);

	M_hash_dict_enumerate_free(dict2_enum);
	M_hash_dict_destroy(dict2);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_merge)
{
	const char *dict1_unique_keys[] = {
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	};
	const char *dict1_value = "1";
	const char *dict2_unique_keys[] = {
		"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	};
	const char *dict2_clobber_keys[] = {
		dict1_unique_keys[0], dict1_unique_keys[5],
	};
	const char *dict2_value = "2";
	size_t i;

	M_hash_dict_t *dict1;
	M_hash_dict_t *dict2;
	M_hash_dict_t *dict2_backup;

	dict1 = M_hash_dict_create(128, 50, M_HASH_DICT_CASECMP);
	for (i=0; i<ARYLEN(dict1_unique_keys); i++)
		ck_assert_msg(M_hash_dict_insert(dict1, dict1_unique_keys[i], dict1_value), "insert failed");

	dict2 = M_hash_dict_create(128, 50, M_HASH_DICT_CASECMP);
	for (i=0; i<ARYLEN(dict2_unique_keys); i++) {
		ck_assert_msg(!M_hash_dict_get(dict1, dict2_unique_keys[i], NULL), "get failed");
		ck_assert_msg(M_hash_dict_insert(dict2, dict2_unique_keys[i], dict2_value), "insert failed");
	}
	for (i=0; i<ARYLEN(dict2_clobber_keys); i++) {
		ck_assert_msg(!M_hash_dict_get(dict1, dict2_unique_keys[i], NULL), "get failed");
		ck_assert_msg(M_hash_dict_insert(dict2, dict2_clobber_keys[i], dict2_value), "insert failed");
	}

	/* make a backup since merge operation destroy the src */
	dict2_backup = M_hash_dict_duplicate(dict2);

	M_hash_dict_merge(&dict1, dict2_backup);
	ck_assert_msg(M_hash_dict_num_keys(dict1) >= M_hash_dict_num_keys(dict2), "unexpected number of entries");

	/* for all keys of unique to dict1, ensure values are as expected */
	for (i=0; i<ARYLEN(dict1_unique_keys); i++) {
		if (!M_hash_dict_get(dict2, dict1_unique_keys[i], NULL))
			ck_assert_msg(M_str_eq(M_hash_dict_get_direct(dict1, dict1_unique_keys[i]), dict1_value), "value should be from dict1");
	}
	/* for all keys unique to dict2, ensure values are as expected */
	for (i=0; i<ARYLEN(dict2_unique_keys); i++) {
		ck_assert_msg(M_str_eq(M_hash_dict_get_direct(dict1, dict2_unique_keys[i]), dict2_value), "value should be from dict2");
	}
	/* for all keys that exist in both dict1 and dict2, ensure values are from dict2 */
	for (i=0; i<ARYLEN(dict2_clobber_keys); i++) {
		ck_assert_msg(M_str_eq(M_hash_dict_get_direct(dict1, dict2_unique_keys[i]), dict2_value), "value from dict2 should clobber value from dict1");
	}

	ck_assert_msg(M_hash_dict_num_keys(dict1) == ARYLEN(dict1_unique_keys)+ARYLEN(dict2_unique_keys), "");

	M_hash_dict_destroy(dict2);
	M_hash_dict_destroy(dict1);
}
END_TEST

START_TEST(check_casesensitive)
{
	static struct {
		const char *key;
		const char *val;
	} pairs[] = {
		{ "key", "a" },
		{ "keY", "b" },
		{ "kEy", "c" },
		{ "Key", "d" },
		{ "kEY", "e" },
		{ "KEy", "f" },
		{ "KeY", "g" },
		{ "KEY", "h" },
		{ NULL,  NULL}
	};
	size_t i;
	M_hash_dict_t *d = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);

	for (i=0; pairs[i].key != NULL; i++)
		ck_assert_msg(M_hash_dict_insert(d, pairs[i].key, pairs[i].val), "%zu: insert failed", i);
	for (i=0; pairs[i].key != NULL; i++)
		ck_assert_msg(M_str_eq(M_hash_dict_get_direct(d, pairs[i].key), pairs[i].val), "%zu: get failed", i);

	M_hash_dict_destroy(d);
}
END_TEST

START_TEST(check_multi)
{
	static const char *result = "hgfedcba";
	static struct {
		const char *key;
		const char *val;
	} pairs[] = {
		{ "key", "a" },
		{ "key", "b" },
		{ "key", "c" },
		{ "key", "d" },
		{ "key", "e" },
		{ "key", "f" },
		{ "key", "g" },
		{ "key", "h" },
		{ NULL,  NULL}
	};
	const char *key;
	const char *val;
	char       *out;
	size_t i;
	M_hash_dict_enum_t *d_enum;
	M_buf_t *buf     = M_buf_create();
	M_hash_dict_t *d = M_hash_dict_create(8, 75, M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_SORTDESC);

	for (i=0; pairs[i].key != NULL; i++)
		ck_assert_msg(M_hash_dict_insert(d, pairs[i].key, pairs[i].val), "%zu: insert failed", i);

	ck_assert_msg(M_hash_dict_enumerate(d, &d_enum) > 0, "enumerate failed");
	while (M_hash_dict_enumerate_next(d, d_enum, &key, &val)) {
		ck_assert_msg(M_str_eq(key, "key"), "unexpected key %s found", key);
		M_buf_add_str(buf, val);
	}
	M_hash_dict_enumerate_free(d_enum);

	out = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(out, result), "%s != %s", out, result);
	M_free(out);

	M_hash_dict_destroy(d);
}
END_TEST

START_TEST(check_ordered_insert)
{
	static const char *result = "yabczzzxx";
	const char *key;
	char       *out;
	M_hash_dict_enum_t *d_enum;
	M_buf_t *buf     = M_buf_create();
	M_hash_dict_t *d = M_hash_dict_create(8, 75, M_HASH_DICT_KEYS_ORDERED);
	size_t i;
	static struct {
		const char *key;
		const char *val;
	} pairs[] = {
		{ "y",   "b" },
		{ "abc", "a" },
		{ "zzz", "c" },
		{ "xx",  "c" },
		{ NULL,  NULL}
	};

	for (i=0; pairs[i].key != NULL; i++)
		ck_assert_msg(M_hash_dict_insert(d, pairs[i].key, pairs[i].val), "%zu: insert failed", i);

	ck_assert_msg(M_hash_dict_enumerate(d, &d_enum) > 0, "enumerate failed");
	while (M_hash_dict_enumerate_next(d, d_enum, &key, NULL)) {
		M_buf_add_str(buf, key);
	}
	M_hash_dict_enumerate_free(d_enum);

	out = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(out, result), "%s != %s", out, result);
	M_free(out);

	M_hash_dict_destroy(d);
}
END_TEST

START_TEST(check_ordered_sort)
{
	static const char *result = "zzzyxxabc";
	const char *key;
	char       *out;
	M_hash_dict_enum_t *d_enum;
	M_buf_t *buf     = M_buf_create();
	M_hash_dict_t *d = M_hash_dict_create(8, 75, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_KEYS_SORTDESC);
	size_t i;
	static struct {
		const char *key;
		const char *val;
	} pairs[] = {
		{ "y",   "b" },
		{ "abc", "a" },
		{ "zzz", "c" },
		{ "xx",  "c" },
		{ NULL,  NULL}
	};

	for (i=0; pairs[i].key != NULL; i++)
		ck_assert_msg(M_hash_dict_insert(d, pairs[i].key, pairs[i].val), "%zu: insert failed", i);

	ck_assert_msg(M_hash_dict_enumerate(d, &d_enum) > 0, "enumerate failed");
	while (M_hash_dict_enumerate_next(d, d_enum, &key, NULL)) {
		M_buf_add_str(buf, key);
	}
	M_hash_dict_enumerate_free(d_enum);

	out = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(out, result), "%s != %s", out, result);
	M_free(out);

	M_hash_dict_destroy(d);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_reuse_val)
{
	const char    *key = "abc";
	const char    *val;
	M_hash_dict_t *d   = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);
	size_t         num_entries;

	M_hash_dict_insert(d, key, "1234");

	val = M_hash_dict_get_direct(d, key);
	M_hash_dict_insert(d, key, val);

	num_entries = M_hash_dict_num_keys(d);
	ck_assert_msg(num_entries == 1, "expected 1, got %zu", num_entries);

	M_hash_dict_destroy(d);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_hash_dict_suite(void)
{
	Suite *suite = suite_create("hash_dict");
	TCase *tc_create;
	TCase *tc_insert;
	TCase *tc_remove;
	TCase *tc_get;
	TCase *tc_get_direct;
#if 0
	TCase *tc_num_collisions;
	TCase *tc_num_expansions;
#endif
	TCase *tc_enumerate;
	TCase *tc_merge;
	TCase *tc_casesensitive;
	TCase *tc_multi;
	TCase *tc_ordered_insert;
	TCase *tc_ordered_sort;
	TCase *tc_reuse_val;
	

	tc_create = tcase_create("hash_dict_create");
	tcase_add_checked_fixture(tc_create, setup, teardown);
	tcase_add_test(tc_create, check_create_invalid_size);
	tcase_add_test(tc_create, check_create_invalid_pctfill);
	tcase_add_test(tc_create, check_create_pow2);
	tcase_set_timeout(tc_create, 30);
	suite_add_tcase(suite, tc_create);

	tc_insert = tcase_create("hash_dict_insert");
	tcase_add_checked_fixture(tc_insert, setup, teardown);
	tcase_add_test(tc_insert, check_insert);
	suite_add_tcase(suite, tc_insert);

	tc_remove = tcase_create("hash_dict_remove");
	tcase_add_checked_fixture(tc_remove, setup, teardown);
	tcase_add_test(tc_remove, check_remove);
	suite_add_tcase(suite, tc_remove);

	tc_get = tcase_create("hash_dict_get");
	tcase_add_checked_fixture(tc_get, setup, teardown);
	tcase_add_test(tc_get, check_get);
	tcase_add_test(tc_get, check_get_caseless);
	suite_add_tcase(suite, tc_get);

	tc_get_direct = tcase_create("hash_dict_get_direct");
	tcase_add_checked_fixture(tc_get_direct, setup, teardown);
	tcase_add_test(tc_get_direct, check_get_direct);
	suite_add_tcase(suite, tc_get_direct);

#if 0
	tc_num_collisions = tcase_create("hash_dict_num_collisions");
	tcase_add_checked_fixture(tc_num_collisions, setup, teardown);
	tcase_add_test(tc_num_collisions, check_num_collisions);
	suite_add_tcase(suite, tc_num_collisions);

	tc_num_expansions = tcase_create("hash_dict_num_expansions");
	tcase_add_checked_fixture(tc_num_expansions, setup, teardown);
	tcase_add_test(tc_num_expansions, check_num_expansions);
	suite_add_tcase(suite, tc_num_expansions);
#endif

	tc_enumerate = tcase_create("hash_dict_enumerate");
	tcase_add_checked_fixture(tc_enumerate, setup, teardown);
	tcase_add_test(tc_enumerate, check_enumerate);
	suite_add_tcase(suite, tc_enumerate);

	tc_merge = tcase_create("hash_dict_merge");
	tcase_add_checked_fixture(tc_merge, setup, teardown);
	tcase_add_test(tc_merge, check_merge);
	suite_add_tcase(suite, tc_merge);

	tc_casesensitive = tcase_create("hash_dict_casesensitive");
	tcase_add_test(tc_casesensitive, check_casesensitive);
	suite_add_tcase(suite, tc_casesensitive);

	tc_multi = tcase_create("hash_dict_multi");
	tcase_add_test(tc_multi, check_multi);
	suite_add_tcase(suite, tc_multi);

	tc_ordered_insert = tcase_create("hash_dict_ordered_insert");
	tcase_add_test(tc_ordered_insert, check_ordered_insert);
	suite_add_tcase(suite, tc_ordered_insert);

	tc_ordered_sort = tcase_create("hash_dict_ordered_sort");
	tcase_add_test(tc_ordered_sort, check_ordered_sort);
	suite_add_tcase(suite, tc_ordered_sort);

	tc_reuse_val = tcase_create("hash_dict_reuse_val");
	tcase_add_test(tc_reuse_val, check_reuse_val);
	suite_add_tcase(suite, tc_reuse_val);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_hash_dict_suite());
	srunner_set_log(sr, "check_hash_dict.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
