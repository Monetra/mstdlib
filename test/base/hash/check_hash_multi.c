#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	M_uint64    int_key;
	const char *str_key;
	M_uint64    val;
} int_vals[] = {
	{ 2, "b12", 2   },
	{ 1, "a44", 9   },
	{ 4, "daa", 2   },
	{ 3, "a",   8   },
	{ 3, "a",   121 }, /* OVERWRITE VALUE */
	{ 0,  NULL, 0 }
};

static struct {
	M_uint64    int_key;
	const char *str_key;
	const char *val;
} str_vals[] = {
	{ 12, "cb12", "I am a"         },
	{ 11, "ca44", "test for"       },
	{ 14, "cdaa", "handing string" },
	{ 13, "ca",   "values"         },
	{ 13, "ca",   "data"           }, /* OVERWRITE VALUE */
	{ 0,  NULL, NULL }
};

static struct {
	M_uint64    int_key;
	const char *str_key;
	const char *val;
} bin_vals[] = {
	{ 22, "yb12", "I am a"         },
	{ 21, "ya44", "test for"       },
	{ 24, "ydaa", "handing string" },
	{ 23, "ya",   "values"         },
	{ 23, "ya",   "data"           }, /* OVERWRITE VALUE */
	{ 0,  NULL, NULL }
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void check_hash_destroy_vp(void *d)
{
	int *a = d;
	*a += 4;
}

START_TEST(check_insert)
{
	M_hash_multi_t      *hm;
	M_uint64             int_val;
	const char          *str_val;
	const unsigned char *bin_val;
	size_t               bin_len;
	size_t               i;
	int                  vp1 = 0;
	int                  vp2 = 0;
	void                *vpt;

	hm = M_hash_multi_create(M_HASH_MULTI_NONE);
	ck_assert_msg(hm != NULL, "Failed to create multi hash, object is NULL");

	/* Insert */

	/* int */
	for (i=0; int_vals[i].int_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_insert_uint(hm, int_vals[i].int_key, int_vals[i].val), "%lu: Failed to insert int key (%"PRIu64") with int val (%"PRIu64")", i, int_vals[i].int_key, int_vals[i].val);
		ck_assert_msg(M_hash_multi_str_insert_uint(hm, int_vals[i].str_key, int_vals[i].val), "%lu: Failed to insert str key (%s) with int val (%"PRIu64")", i, int_vals[i].str_key, int_vals[i].val);
	}

	/* str */
	for (i=0; str_vals[i].int_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_insert_str(hm, str_vals[i].int_key, str_vals[i].val), "%lu: Failed to insert int key (%"PRIu64") with str val (%s)", i, str_vals[i].int_key, str_vals[i].val);
		ck_assert_msg(M_hash_multi_str_insert_str(hm, str_vals[i].str_key, str_vals[i].val), "%lu: Failed to insert str key (%s) with str val (%s)", i, str_vals[i].str_key, str_vals[i].val);
	}

	/* bin */
	for (i=0; bin_vals[i].int_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_insert_bin(hm, bin_vals[i].int_key, (const unsigned char *)bin_vals[i].val, M_str_len(bin_vals[i].val)), "%lu: Failed to insert int key (%"PRIu64") with bin val (%s)", i, bin_vals[i].int_key, bin_vals[i].val);
		ck_assert_msg(M_hash_multi_str_insert_bin(hm, bin_vals[i].str_key, (const unsigned char *)bin_vals[i].val, M_str_len(bin_vals[i].val)), "%lu: Failed to insert str key (%s) with bin val (%s)", i, bin_vals[i].str_key, bin_vals[i].val);
	}

	/* vp */
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 200, int_vals, NULL), "Failed to insert int key 200 with vp int_vals");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "200", str_vals, NULL), "Failed to insert str key 200 with vp str_vals");
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 401, &vp1, check_hash_destroy_vp), "Failed to insert int key 401 with vp vp1");
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 402, &vp1, check_hash_destroy_vp), "Failed to insert int key 402 with vp vp1");
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 403, &vp1, check_hash_destroy_vp), "Failed to insert int key 402 with vp vp1");
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 403, &vp1, NULL), "Failed to insert int key 403 with vp vp1");
	ck_assert_msg(M_hash_multi_u64_insert_vp(hm, 404, &vp1, check_hash_destroy_vp), "Failed to insert int key 404 with vp vp1");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "401", &vp2, check_hash_destroy_vp), "Failed to insert str key 401 with vp vp2");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "402", &vp2, check_hash_destroy_vp), "Failed to insert str key 402 with vp vp2");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "403", &vp2, check_hash_destroy_vp), "Failed to insert str key 403 with vp vp2");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "403", &vp2, NULL), "Failed to insert str key 403 with vp vp2 and no free func");
	ck_assert_msg(M_hash_multi_str_insert_vp(hm, "404", &vp2, check_hash_destroy_vp), "Failed to insert str key 404 with vp vp2");


	/* Check vals */

	/* int */
	for (i=0; int_vals[i].int_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_get_uint(hm, int_vals[i].int_key, &int_val), "%lu: Failed to get int val for int key (%"PRIu64")", i, int_vals[i].int_key);
		if (int_vals[i].int_key == 3) {
			ck_assert_msg(int_val == 121, "%lu: int key (%"PRIu64") int val (%"PRIu64") != expected val (%d)", i, int_vals[i].int_key, int_val, 121);
		} else {
			ck_assert_msg(int_val == int_vals[i].val, "%lu: int key (%"PRIu64") int val (%"PRIu64") != expected val (%"PRIu64")", i, int_vals[i].int_key, int_val, int_vals[i].val);
		}

		ck_assert_msg(M_hash_multi_str_get_uint(hm, int_vals[i].str_key, &int_val), "%lu: Failed to get int val for str key (%s)", i, int_vals[i].str_key);
		if (int_vals[i].int_key == 3) {
			ck_assert_msg(int_val == 121, "%lu: str key (%s) int val (%"PRIu64") != expected val (%d)", i, int_vals[i].str_key, int_val, 121);
		} else {
			ck_assert_msg(int_val == int_vals[i].val, "%lu: str key (%s) int val (%"PRIu64") != expected val (%"PRIu64")", i, int_vals[i].str_key, int_val, int_vals[i].val);
		}
	}

	/* str */
	for (i=0; str_vals[i].str_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_get_str(hm, str_vals[i].int_key, &str_val), "%lu: Failed to get str val for int key (%"PRIu64")", i, str_vals[i].int_key);
		if (str_vals[i].int_key == 13) {
			ck_assert_msg(M_str_eq(str_val, "data"), "%lu: int key (%"PRIu64") str val (%s) != expected val (%s)", i, str_vals[i].int_key, str_val, "data");
		} else {
			ck_assert_msg(M_str_eq(str_val, str_vals[i].val), "%lu: int key (%"PRIu64") str val (%s) != expected val (%s)", i, str_vals[i].int_key, str_val, str_vals[i].val);
		}

		ck_assert_msg(M_hash_multi_str_get_str(hm, str_vals[i].str_key, &str_val), "%lu: Failed to get str val for str key (%s)", i, str_vals[i].str_key);
		if (M_str_eq(str_vals[i].str_key, "ca")) {
			ck_assert_msg(M_str_eq(str_val, "data"), "%lu: str key (%s) str val (%s) != expected val (%s)", i, str_vals[i].str_key, str_val, "ca");
		} else {
			ck_assert_msg(M_str_eq(str_val, str_vals[i].val), "%lu: str key (%s) str val (%s) != expected val (%s)", i, str_vals[i].str_key, str_val, str_vals[i].val);
		}
	}

	/* bin */
	for (i=0; bin_vals[i].int_key!=0; i++) {
		ck_assert_msg(M_hash_multi_u64_get_bin(hm, bin_vals[i].int_key, &bin_val, &bin_len), "%lu: Failed to get bin val for int key (%"PRIu64")", i, bin_vals[i].int_key);
		if (bin_vals[i].int_key == 23) {
			ck_assert_msg(bin_len == 4 && M_mem_eq(bin_val, "data", bin_len), "%lu: int key (%"PRIu64") bin val (%.*s) != expected val (%s)", i, bin_vals[i].int_key, (int)bin_len, bin_val, "data");
		} else {
			ck_assert_msg(bin_len == M_str_len(bin_vals[i].val) && M_mem_eq(bin_val, bin_vals[i].val, bin_len), "%lu: int key (%"PRIu64") bin val (%.*s) != expected val (%s)", i, bin_vals[i].int_key, (int)bin_len, bin_val, bin_vals[i].val);
		}

		ck_assert_msg(M_hash_multi_str_get_bin(hm, bin_vals[i].str_key, &bin_val, &bin_len), "%lu: Failed to get bin val for str key (%s)", i, bin_vals[i].str_key);
		if (M_str_eq(bin_vals[i].str_key, "ya")) {
			ck_assert_msg(bin_len == 4 && M_mem_eq(bin_val, "data", bin_len), "%lu: str key (%s) bin val (%.*s) != expected val (%s)", i, bin_vals[i].str_key, (int)bin_len, bin_val, "data");
		} else {
			ck_assert_msg(bin_len == M_str_len(bin_vals[i].val) && M_mem_eq(bin_val, bin_vals[i].val, bin_len), "%lu: str key (%s) bin val (%.*s) != expected val (%s)", i, bin_vals[i].str_key, (int)bin_len, bin_val, bin_vals[i].val);
		}
	}

	/* vp */
	ck_assert_msg(M_hash_multi_u64_get_vp(hm, 200, &vpt), "Failed to get vp for int key 200");
	ck_assert_msg(vpt == int_vals, "key int 200 vpt (%p) != int_vals (%p)", vpt, int_vals);

	ck_assert_msg(M_hash_multi_str_get_vp(hm, "200", &vpt), "Failed to get vp for str key 200");
	ck_assert_msg(vpt == str_vals, "key str 200 vpt (%p) != str_vals (%p)", vpt, str_vals);

	ck_assert_msg(M_hash_multi_str_get_vp(hm, "401", &vpt), "Failed to get vp2 for str key 401");
	ck_assert_msg(vpt == &vp2, "key str 401 vpt (%p) != vp2 (%p)", vpt, &vp2);
	ck_assert_msg(M_hash_multi_str_get_vp(hm, "403", &vpt), "Failed to get vp2 for str key 403");
	ck_assert_msg(vpt == &vp2, "key str 403 vpt (%p) != vp2 (%p)", vpt, &vp2);


	ck_assert_msg(M_hash_multi_u64_remove(hm, 401, M_TRUE), "Could not remove vp with int key 401");
	ck_assert_msg(M_hash_multi_u64_remove(hm, 402, M_FALSE), "Could not remove vp with int key 402");

	ck_assert_msg(M_hash_multi_str_remove(hm, "401", M_TRUE), "Could not remove vp with str key 401");
	ck_assert_msg(M_hash_multi_str_remove(hm, "402", M_FALSE), "Could not remove vp with str key 402");

	M_hash_multi_destroy(hm);

	/* 3 free calls, insert as overwrite, remove true (false will not increment), destroy. */
	ck_assert_msg(vp1 == 12, "vp1 (%d) != 12", vp1);
	ck_assert_msg(vp2 == 12, "vp2 (%d) != 12", vp2);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_hash_multi_suite(void)
{
	Suite *suite = suite_create("hash_multi");
	TCase *tc_insert;

	tc_insert = tcase_create("hash_multi_insert");
	tcase_add_test(tc_insert, check_insert);
	suite_add_tcase(suite, tc_insert);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(M_hash_multi_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_hash_multi.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
