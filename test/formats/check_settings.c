#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_hash_dict_t *create_dict(void)
{
    M_hash_dict_t *d;

    d = M_hash_dict_create(16, 75, M_HASH_DICT_KEYS_ORDERED);

    M_hash_dict_insert(d, "k1",            "v1");
    M_hash_dict_insert(d, "k1.1",          "v1.1");
    M_hash_dict_insert(d, "k1.2",          "v1.2");
    M_hash_dict_insert(d, "g1/k2",         "v2");
    M_hash_dict_insert(d, "g1/k2.1",       "v2.1");
    M_hash_dict_insert(d, "g1/k2.2",       "v2.2");
    M_hash_dict_insert(d, "g1/g2/k3",      "v3");
    M_hash_dict_insert(d, "g1/g2/g3/k4",   "v4");
    M_hash_dict_insert(d, "g1/g2/g3.1/k5", "v5");

    return d;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_groups)
{
    M_hash_dict_t *d;
    M_list_str_t  *lstr;
    size_t         len;

    d = create_dict();

    lstr = M_settings_groups(d, NULL);
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "NULL should only have one group");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "g1"), "NULL should only have group 'g1'");
    M_list_str_destroy(lstr);

    lstr = M_settings_groups(d, "g1");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "NULL should only have one group");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "g2"), "'g1' should only have group 'g2'");
    M_list_str_destroy(lstr);

    lstr = M_settings_groups(d, "g1/");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "NULL should only have one group");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "g2"), "'g1/' should only have group 'g2'");
    M_list_str_destroy(lstr);

    lstr = M_settings_groups(d, "g1/g2");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 2, "NULL should only have two groups");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "g3"), "'g1/g2' should have group 'g3' first");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 1), "g3.1"), "'g1/g2' should have group 'g3.1' second");
    M_list_str_destroy(lstr);

    lstr = M_settings_groups(d, "g2");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 0, "'g2' should only have zero groups (not top level)");
    M_list_str_destroy(lstr);

    M_hash_dict_destroy(d);
}
END_TEST

START_TEST(check_keys)
{
    M_hash_dict_t *d;
    M_list_str_t  *lstr;
    size_t         len;

    d = create_dict();

    lstr = M_settings_group_keys(d, NULL);
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 3, "NULL should have three keys");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k1"), "NULL should have key 'k1' first");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 1), "k1.1"), "NULL should have key 'k1.1' second");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 2), "k1.2"), "NULL should have key 'k1.2' third");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g1");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 3, "'g1' should have three keys");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k2"), "'g1' should have key 'k2' first");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 1), "k2.1"), "'g1' should have key 'k2.1' second");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 2), "k2.2"), "'g1' should have key 'k2.2' third");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g1/");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 3, "'g1/' should have three keys");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k2"), "'g1/' should have key 'k2' first");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 1), "k2.1"), "'g1/' should have key 'k2.1' second");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 2), "k2.2"), "'g1/' should have key 'k2.2' third");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g1/g2");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "'g1/g2' should have one key");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k3"), "'g1/g2' should have only key 'k3'");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g1/g2/g3");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "'g1/g2/g3' should have one key");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k4"), "'g1/g2/g3' should have only key 'k4'");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g1/g2/g3.1");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 1, "'g1/g2/g3.1' should have one key");
    ck_assert_msg(M_str_eq(M_list_str_at(lstr, 0), "k5"), "'g1/g2/g3.1' should have only key 'k5'");
    M_list_str_destroy(lstr);

    lstr = M_settings_group_keys(d, "g2");
    len  = M_list_str_len(lstr);
    ck_assert_msg(len == 0, "'g2' should only have zero keys (not top level)");
    M_list_str_destroy(lstr);

    M_hash_dict_destroy(d);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_suite(void)
{
    Suite *suite;
    TCase *tc_settings_groups;
    TCase *tc_settings_keys;

    suite = suite_create("settings");

    tc_settings_groups = tcase_create("check_groups");
    tcase_add_unchecked_fixture(tc_settings_groups, NULL, NULL);
    tcase_add_test(tc_settings_groups, check_groups);
    suite_add_tcase(suite, tc_settings_groups);

    tc_settings_keys = tcase_create("check_keys");
    tcase_add_unchecked_fixture(tc_settings_keys, NULL, NULL);
    tcase_add_test(tc_settings_keys, check_keys);
    suite_add_tcase(suite, tc_settings_keys);

    return suite;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
    int nf;
    SRunner *sr = srunner_create(M_suite());
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_settings.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
