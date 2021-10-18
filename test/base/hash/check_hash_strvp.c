#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_hash_strvp_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char *key;
} kv_pairs[] = {
	{ "A000000003"       },
	{ "A000000003"       },
	{ "A000000004"       },
	{ "A000000004"       },
	{ "A000000005"       },
	{ "A000000025"       },
	{ "A000000025"       },
	{ "A000000152"       },
	{ "A000000152"       },
	{ "A000000324"       },
	{ "A000000324"       },
	{ "A000000333"       },
	{ "A000000333"       },
	{ "A000000065"       },
	{ "A000000065"       },
	{ "A000000277"       },
	{ "A000000277"       },
	{ "A0000000031010"   },
	{ "A0000000032010"   },
	{ "A0000000032020"   },
	{ "A0000000038010"   },
	{ "A0000000041010"   },
	{ "A0000000049999"   },
	{ "A0000000043060"   },
	{ "A0000000046000"   },
	{ "A0000000050001"   },
	{ "A00000002501  "   },
	{ "A0000000651010"   },
	{ "A0000001523010"   },
	{ "A0000002771010"   },
	{ "A0000003241010"   },
	{ "A000000333010101" },
	{ "A000000333010102" },
	{ "A000000333010103" },
	{ "A000000333010106" },
	{  NULL}
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_ordered(const char *key_result, M_uint32 flags)
{
	const char          *key;
	void                *val;
	M_buf_t             *key_buf;
	M_hash_strvp_enum_t *d_enum;
	M_hash_strvp_t      *d;
	size_t               i;
	char                *out;

	key_buf = M_buf_create();
	d       = M_hash_strvp_create(8, 75, flags, NULL);

	/* Load the data into the hashtable. */
	for (i=0; kv_pairs[i].key != NULL; i++)
		ck_assert_msg(M_hash_strvp_insert(d, kv_pairs[i].key, &kv_pairs), "%"PRIu64": insert failed: %s", (M_uint64)i, kv_pairs[i].key);

	/* Check the data was set correctly. */
	for (i=0; kv_pairs[i].key != 0; i++)
		ck_assert_msg(M_hash_strvp_get_direct(d, kv_pairs[i].key) == &kv_pairs, "%"PRIu64": get failed: %s", (M_uint64)i, kv_pairs[i].key);

	/* Enumerate the data and fill in our result data buffers. */
	ck_assert_msg(M_hash_strvp_enumerate(d, &d_enum) > 0, "enumerate failed");
	while (M_hash_strvp_enumerate_next(d, d_enum, &key, &val)) {
		M_buf_add_str(key_buf, key);
	}
	M_hash_strvp_enumerate_free(d_enum);

	/* Check the keys are in correct order. */
	out = M_buf_finish_str(key_buf, NULL);
	ck_assert_msg(M_str_eq(out, key_result), "%s != %s", out, key_result);
	M_free(out);

	M_hash_strvp_destroy(d, M_FALSE);
}

START_TEST(check_ordered_insert)
{
	check_ordered("A000000003A000000004A000000005A000000025A000000152A000000324A000000333A000000065A000000277A0000000031010A0000000032010A0000000032020A0000000038010A0000000041010A0000000049999A0000000043060A0000000046000A0000000050001A00000002501  A0000000651010A0000001523010A0000002771010A0000003241010A000000333010101A000000333010102A000000333010103A000000333010106", M_HASH_STRVP_KEYS_ORDERED);
}
END_TEST

START_TEST(check_ordered_sort)
{
	check_ordered("A000000003A0000000031010A0000000032010A0000000032020A0000000038010A000000004A0000000041010A0000000043060A0000000046000A0000000049999A000000005A0000000050001A000000025A00000002501  A000000065A0000000651010A000000152A0000001523010A000000277A0000002771010A000000324A0000003241010A000000333A000000333010101A000000333010102A000000333010103A000000333010106", M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_KEYS_SORTASC);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_hash_strvp_suite(void)
{
	Suite *suite = suite_create("hash_strvp");
	TCase *tc_ordered_insert;
	TCase *tc_ordered_sort;

	tc_ordered_insert = tcase_create("hash_strvp_ordered_insert");
	tcase_add_unchecked_fixture(tc_ordered_insert, NULL, NULL);
	tcase_add_test(tc_ordered_insert, check_ordered_insert);
	suite_add_tcase(suite, tc_ordered_insert);

	tc_ordered_sort = tcase_create("hash_strvp_ordered_sort");
	tcase_add_unchecked_fixture(tc_ordered_sort, NULL, NULL);
	tcase_add_test(tc_ordered_sort, check_ordered_sort);
	suite_add_tcase(suite, tc_ordered_sort);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_hash_strvp_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_hash_strvp.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
