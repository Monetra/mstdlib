#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


START_TEST(check_hotness)
{
	M_cache_strvp_t *cache;
	const char      *val;
	size_t           size;

	cache = M_cache_strvp_create(4, M_CACHE_STRVP_CASECMP, M_free);

	M_cache_strvp_insert(cache, "key1", M_strdup("val1"));
	M_cache_strvp_insert(cache, "key2", M_strdup("val2"));
	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 2, "Cache size invalid, got '%zu' expected '%d'", size, 2);

	M_cache_strvp_insert(cache, "key3", M_strdup("val3"));
	M_cache_strvp_insert(cache, "key4", M_strdup("val4"));
	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 4, "Cache size invalid, got '%zu' expected '%d'", size, 4);

	val = M_cache_strvp_get_direct(cache, "KEY1");
	ck_assert_msg(M_str_eq(val, "val1"), "key1 expected '%s' got '%s'", "val1", val);

	val = M_cache_strvp_get_direct(cache, "KeY3");
	ck_assert_msg(M_str_eq(val, "val3"), "key3 expected '%s' got '%s'", "val3", val);

	M_cache_strvp_insert(cache, "key5", M_strdup("val5"));
	M_cache_strvp_insert(cache, "key6", M_strdup("val6"));

	val = M_cache_strvp_get_direct(cache, "key4");
	ck_assert_msg(val == NULL, "key4 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "key2");
	ck_assert_msg(val == NULL, "key4 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "Key5");
	ck_assert_msg(M_str_eq(val, "val5"), "key5 expected '%s' got '%s'", "val5", val);

	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 4, "Cache size invalid, got '%zu' expected '%d'", size, 4);

	M_cache_strvp_destroy(cache);
}
END_TEST

START_TEST(check_change_size_shrink)
{
	M_cache_strvp_t *cache;
	const char      *val;
	size_t           size;

	cache = M_cache_strvp_create(4, M_CACHE_STRVP_CASECMP, M_free);
	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 0, "Cache size invalid, got '%zu' expected '%d'", size, 0);
	size = M_cache_strvp_max_size(cache);
	ck_assert_msg(size == 4, "Cache max size invalid, got '%zu' expected '%d'", size, 4);

	M_cache_strvp_insert(cache, "key1", M_strdup("val1"));
	M_cache_strvp_insert(cache, "key2", M_strdup("val2"));
	M_cache_strvp_insert(cache, "key3", M_strdup("val3"));
	M_cache_strvp_insert(cache, "key4", M_strdup("val4"));
	M_cache_strvp_insert(cache, "key5", M_strdup("val5"));
	M_cache_strvp_insert(cache, "key6", M_strdup("val6"));

	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 4, "Cache size invalid, got '%zu' expected '%d'", size, 4);
	size = M_cache_strvp_max_size(cache);
	ck_assert_msg(size == 4, "Cache max size invalid, got '%zu' expected '%d'", size, 4);

	M_cache_strvp_set_max_size(cache, 2);
	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 2, "Cache size invalid, got '%zu' expected '%d'", size, 2);

	val = M_cache_strvp_get_direct(cache, "key1");
	ck_assert_msg(val == NULL, "key1 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "key2");
	ck_assert_msg(val == NULL, "key2 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "key3");
	ck_assert_msg(val == NULL, "key3 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "key4");
	ck_assert_msg(val == NULL, "key4 should not have been found got '%s'", val);

	val = M_cache_strvp_get_direct(cache, "key5");
	ck_assert_msg(M_str_eq(val, "val5"), "key5 expected '%s' got '%s'", "val5", val);

	val = M_cache_strvp_get_direct(cache, "key6");
	ck_assert_msg(M_str_eq(val, "val6"), "key6 expected '%s' got '%s'", "val6", val);

	M_cache_strvp_get(cache, "key5", NULL);

	M_cache_strvp_insert(cache, "key7", M_strdup("val7"));
	val = M_cache_strvp_get_direct(cache, "key7");
	ck_assert_msg(M_str_eq(val, "val7"), "key7 expected '%s' got '%s'", "val7", val);

	val = M_cache_strvp_get_direct(cache, "key6");
	ck_assert_msg(val == NULL, "key6 should not have been found got '%s'", val);

	size  = M_cache_strvp_size(cache);
	ck_assert_msg(size == 2, "Cache size invalid, got '%zu' expected '%d'", size, 2);

	M_cache_strvp_destroy(cache);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_cache_strvp_suite(void)
{
	Suite *suite = suite_create("cache_strvp");
	TCase *tc;

	tc = tcase_create("check_hotness");
	tcase_add_test(tc, check_hotness);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_change_size_shrink");
	tcase_add_test(tc, check_change_size_shrink);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_cache_strvp_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_cache_strvp.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
