#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define CHECK_INI_READ_WRITE_SINGLE "" \
"#comment start\n" \
"key1=val1\n" \
"\n" \
"[Section1]\n" \
"s1_key1=s1_val0\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"s1_key1=s1_val1\n" \
"\n" \
"[Section2]\n" \
"s2_key1=\"s2_val1 quoted\n" \
"across multiple\n" \
"lines\"\n" \
"s2_key2=s2_val2\n" \
"s2_key3=\"quoted with \"\"quotes\"\" within\"\n" \
"s2_key4=\"quoted with \"\"quo\n" \
"tes\"\" within\"\n" \
"\n" \
"[Section3]\n" \
"s3_key1\n" \
"s3_key2=\n" \
"s3_key3 # I am a comment\n" \
"s3_key4= # I'm also a comment.\n" \
"s3_key5= Value # and a comment\n" \
"\n" \
"\n" \
"\n" \
"[Section 4]\n" \
"# This section has a space in the name\n" \
"s4_key1 = s4_val1\n"

#define CHECK_INI_READ_WRITE_SINGLE_RESULT "" \
"# comment start\n" \
"key1=val1\n" \
"\n" \
"s1_key1=yes\n" \
"loadmodule=new.so\n" \
"[Section1]\n" \
"s1_key1=s1_val1\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"[Section2]\n" \
"s2_key1=\"s2_val1 quoted\n" \
"across multiple\n" \
"lines\"\n" \
"s2_key2=s2_val2\n" \
"s2_key3=\"quoted with \"\"quotes\"\" within\"\n" \
"s2_key4=\"quoted with \"\"quo\n" \
"tes\"\" within\"\n" \
"\n" \
"[Section3]\n" \
"s3_key1\n" \
"s3_key2=\n" \
"s3_key3# I am a comment\n" \
"s3_key4=# I'm also a comment.\n" \
"s3_key5=Value# and a comment\n" \
"\n" \
"\n" \
"\n" \
"[Section 4]\n" \
"# This section has a space in the name\n" \
"s4_key1=s4_val1\n"

#define CHECK_INI_READ_WRITE_MULTI "" \
"#comment start\n" \
"key1=val1\n" \
"\n" \
"[Section1]\n" \
"s1_key1=s1_val1\n" \
"s1_key1=s1_val2\n" \
"s1_key1=s1_val3\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"[Section 1]\n" \
"s1_key1=s1_val1_new\n"

#define CHECK_INI_READ_WRITE_MULTI_RESULT_MAINTAIN_ORDER "" \
"# comment start\n" \
"key1=val1\n" \
"\n" \
"[Section1]\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"s1_key1=s1_val1\n" \
"s1_key1=s1_val2\n" \
"s1_key1=s1_val1_new\n" \
"s1_key1=yes\n" \
"s1_key1=new.so\n"

#define CHECK_INI_READ_WRITE_MULTI_RESULT_KEEP_EXISTING "" \
"# comment start\n" \
"key1=val1\n" \
"\n" \
"[Section1]\n" \
"s1_key1=s1_val1\n" \
"s1_key1=s1_val2\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"s1_key1=s1_val1_new\n" \
"s1_key1=yes\n" \
"s1_key1=new.so\n"

#define CHECK_INI_READ_WRITE_INVALID "" \
"#comment start\n" \
"key1=\"val1\n" \
"2\n" \
"3\"\n" \
"\n" \
"[Section1]\n" \
"s2_key/invald=123\n" \
"s2_key2=val2\n"

#define CHECK_INI_READ_WRITE_MERGE_CUR "" \
"#comment start\n" \
"key1=val1\n" \
"key2=val_new\n" \
"\n" \
"[Section1]\n" \
"s1_key1=s1_val1\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"[Section 2]\n" \
"s2_key1=s2_val1\n" \
"\n" \
"[Section multi]\n" \
"loadmodule=a\n" \
"loadmodule=b\n" \
"loadmodule=c\n" \
"loadmodule=d\n"

#define CHECK_INI_READ_WRITE_MERGE_NEW "" \
"#comment start\n" \
"key1=val_old\n" \
"key2=val_new\n" \
"key3=\n" \
"key4\n" \
"\n" \
"[Section1]\n" \
"s1_key1=different\n" \
"# comment in section\n" \
"s1_key3=333\n" \
"\n" \
"[Section 2]\n" \
"s2_key1=s2_val1\n" \
"\n" \
"[Section 3]\n" \
"s3_key1=s3_val1\n" \
"\n" \
"[section_multi]\n" \
"loadmodule=b\n" \
"loadmodule=c\n" \
"#loadmodule=f\n" \
"loadmodule=g\n"

#define CHECK_INI_READ_WRITE_MERGE_ORIG "" \
"#comment start\n" \
"key1=val_old\n" \
"\n" \
"[Section1]\n" \
"s1_key1=s1_val1\n" \
"# comment in section\n" \
"s1_key2=s1_val2\n" \
"\n" \
"[Section 2]\n" \
"s2_key1=s2_val1\n" \
"\n" \
"[Section multi]\n" \
"loadmodule=b\n" \
"loadmodule=d\n" \
"loadmodule=e\n" \
"loadmodule=g\n"

#define CHECK_INI_READ_WRITE_MERGE_RESULT "" \
"# comment start\n" \
"key1=val1\n" \
"key2=val_new\n" \
"key3=\n" \
"key4\n" \
"\n" \
"[Section1]\n" \
"s1_key1=different\n" \
"# comment in section\n" \
"s1_key3=333\n" \
"\n" \
"[Section 2]\n" \
"s2_key1=s2_val1\n" \
"\n" \
"[Section 3]\n" \
"s3_key1=s3_val1\n" \
"\n" \
"[Section multi]\n" \
"loadmodule=b\n" \
"loadmodule=c\n" \
"# loadmodule=f\n" \
"loadmodule=a\n"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_ini_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_read_write_single)
{
	M_ini_t          *ini;
	M_ini_settings_t *info;
	char             *out;
	size_t            errln;

	info = M_ini_settings_create();
	M_ini_settings_set_quote_char(info, '"');
	M_ini_settings_set_escape_char(info, '"');
	M_ini_settings_set_padding(info, M_INI_PADDING_AFTER_COMMENT_CHAR);
	M_ini_settings_reader_set_dupkvs_handling(info, M_INI_DUPKVS_REMOVE);
	M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_USE_LAST);

	ini = M_ini_read(CHECK_INI_READ_WRITE_SINGLE, info, M_TRUE, &errln);
	ck_assert_msg(ini != NULL, "ini could not be parsed");

	/* Make some changes */
	M_ini_kv_set(ini, "s1_key1", "yes");
	M_ini_kv_insert(ini, "loadmodule", "new.so");

	out = M_ini_write(ini, info);
	ck_assert_msg(M_str_eq(out, CHECK_INI_READ_WRITE_SINGLE_RESULT), "input does not match expected output");
	M_free(out);

	M_ini_destroy(ini);
	M_ini_settings_destroy(info);
}
END_TEST

START_TEST(check_read_write_multi)
{
	M_ini_t          *ini;
	M_ini_t          *ini2;
	M_ini_settings_t *info;
	char             *out;
	size_t            errln;

	info = M_ini_settings_create();
	M_ini_settings_set_quote_char(info, '"');
	M_ini_settings_set_escape_char(info, '"');
	M_ini_settings_set_padding(info, M_INI_PADDING_AFTER_COMMENT_CHAR);
	M_ini_settings_reader_set_dupkvs_handling(info, M_INI_DUPKVS_COLLECT);

	ini = M_ini_read(CHECK_INI_READ_WRITE_MULTI, info, M_TRUE, &errln);
	ck_assert_msg(ini != NULL, "ini could not be parsed");

	/* Make some changes */
	M_ini_kv_remove_val_at(ini, "section1/s1_key1", M_ini_kv_len(ini, "section1/s1_key1")-2);
	M_ini_kv_insert(ini, "section1/s1_key1", "yes");
	M_ini_kv_insert(ini, "section1/s1_key1", "new.so");

	/* Duplicate the ini so we can write it twice with different options. */
	ini2 = M_ini_duplicate(ini);
	ck_assert_msg(ini2 != NULL, "ini could not be duplicated");

	/* Maintain order */
	M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_MAINTAIN_ORDER);
	out = M_ini_write(ini, info);
	ck_assert_msg(M_str_eq(out, CHECK_INI_READ_WRITE_MULTI_RESULT_MAINTAIN_ORDER), "input does not match expected output while maintaining order");
	M_free(out);

	/* Keep existing */
	M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_KEEP_EXISTING);
	out = M_ini_write(ini2, info);
	ck_assert_msg(M_str_eq(out, CHECK_INI_READ_WRITE_MULTI_RESULT_KEEP_EXISTING), "input does not match expected output while keeping existing");
	M_free(out);

	M_ini_destroy(ini);
	M_ini_destroy(ini2);
	M_ini_settings_destroy(info);
}
END_TEST

START_TEST(check_read_write_invalid)
{
	M_ini_t          *ini;
	M_ini_settings_t *info;
	size_t            errln;

	info = M_ini_settings_create();
	M_ini_settings_set_quote_char(info, '"');
	M_ini_settings_set_escape_char(info, '"');

	ini = M_ini_read(CHECK_INI_READ_WRITE_INVALID, info, M_TRUE, &errln);
	ck_assert_msg(ini == NULL && errln == 7, "ini does not contain error on line 7");
}
END_TEST

START_TEST(check_merge)
{
	M_ini_settings_t *info;
	M_ini_t          *cur_ini;
	M_ini_t          *new_ini;
	M_ini_t          *orig_ini;
	M_ini_t          *merged_ini;
	char             *out;
	size_t            errln;

	info = M_ini_settings_create();
	M_ini_settings_set_quote_char(info, '"');
	M_ini_settings_set_escape_char(info, '"');
	M_ini_settings_set_padding(info, M_INI_PADDING_AFTER_COMMENT_CHAR);
	M_ini_settings_reader_set_dupkvs_handling(info, M_INI_DUPKVS_COLLECT);
	M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_KEEP_EXISTING);

	cur_ini = M_ini_read(CHECK_INI_READ_WRITE_MERGE_CUR, info, M_TRUE, &errln);
	ck_assert_msg(cur_ini != NULL, "cur ini could not be parsed");
	new_ini = M_ini_read(CHECK_INI_READ_WRITE_MERGE_NEW, info, M_TRUE, &errln);
	ck_assert_msg(cur_ini != NULL, "new ini could not be parsed");
	orig_ini = M_ini_read(CHECK_INI_READ_WRITE_MERGE_ORIG, info, M_TRUE, &errln);
	ck_assert_msg(cur_ini != NULL, "orig ini could not be parsed");

	merged_ini = M_ini_merge(cur_ini, new_ini, orig_ini, info);
	ck_assert_msg(merged_ini != NULL, "merged ini could not be created");
	out = M_ini_write(merged_ini, info);
	ck_assert_msg(M_str_eq(out, CHECK_INI_READ_WRITE_MERGE_RESULT), "input does not match expected output");
	M_free(out);

	M_ini_destroy(orig_ini);
	M_ini_destroy(new_ini);
	M_ini_destroy(cur_ini);
	M_ini_destroy(merged_ini);
	M_ini_settings_destroy(info);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_ini_suite(void)
{
	Suite *suite;
	TCase *tc_ini_read_write_single;
	TCase *tc_ini_read_write_multi;
	TCase *tc_ini_read_write_invalid;
	TCase *tc_ini_merge;

	suite = suite_create("ini");

	tc_ini_read_write_single = tcase_create("check_read_write_single");
	tcase_add_unchecked_fixture(tc_ini_read_write_single, NULL, NULL);
	tcase_add_test(tc_ini_read_write_single, check_read_write_single);
	suite_add_tcase(suite, tc_ini_read_write_single);

	tc_ini_read_write_multi = tcase_create("check_read_write_multi");
	tcase_add_unchecked_fixture(tc_ini_read_write_multi, NULL, NULL);
	tcase_add_test(tc_ini_read_write_multi, check_read_write_multi);
	suite_add_tcase(suite, tc_ini_read_write_multi);

	tc_ini_read_write_invalid = tcase_create("check_read_write_invalid");
	tcase_add_unchecked_fixture(tc_ini_read_write_invalid, NULL, NULL);
	tcase_add_test(tc_ini_read_write_invalid, check_read_write_invalid);
	suite_add_tcase(suite, tc_ini_read_write_invalid);

	tc_ini_merge = tcase_create("check_merge");
	tcase_add_unchecked_fixture(tc_ini_merge, NULL, NULL);
	tcase_add_test(tc_ini_merge, check_merge);
	suite_add_tcase(suite, tc_ini_merge);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_ini_suite());
	srunner_set_log(sr, "check_ini.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
