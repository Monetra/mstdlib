#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define CHECK_GETOPT_HELP "" \
"  -i, --i1 <val> (integer) DESCR 1\n" \
"  --i2 [val] (integer)\n" \
"  -d <val> (decimal) DDESCR\n" \
"  -b B DESC 1\n" \
"  --c2 <val> (boolean)\n" \
"  -s, --ssss <val> (string) SSSSSSSS\n"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_getopt_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool check_getopt_nonopt_thunk_list_cb(size_t idx, const char *option, void *thunk)
{
	M_list_str_t *t;
	(void)idx;
	t = thunk;
	M_list_str_insert(t, option);
	return M_TRUE;
}

static M_bool check_getopt_nonopt_thunk_char_cb(size_t idx, const char *option, void *thunk)
{
	char **t;
	(void)idx;
	t = (char **)thunk;
	*t = M_strdup(option);
	return M_TRUE;
}

static M_bool check_getopt_nonopt_cb(size_t idx, const char *option, void *thunk)
{
	(void)idx;
	(void)thunk;
	if (M_str_eq_max(option, "arg", 3) || M_str_eq_max(option, "--arg", 5) || M_str_eq(option, "-a"))
		return M_TRUE;
	return M_FALSE;
}

static M_bool check_getopt_int_cb(char short_opt, const char *long_opt, M_int64 *integer, void *thunk)
{
	(void)thunk;

	if (integer == NULL || (short_opt != 0 && short_opt != 'i') || (long_opt != NULL && !M_str_caseeq(long_opt, "i2") && !M_str_caseeq(long_opt, "i1")))
		return M_FALSE;

	if (*integer != 123 && *integer != 456)
		return M_FALSE;

	return M_TRUE;
}

static M_bool check_getopt_dec_cb(char short_opt, const char *long_opt, M_decimal_t *decimal, void *thunk)
{
	M_decimal_t dec_cmp1;
	M_decimal_t dec_cmp2;

	(void)thunk;

	if (decimal == NULL || (short_opt != 0 && short_opt != 'd') || (long_opt != NULL && !M_str_caseeq(long_opt, "d2")))
		return M_FALSE;

	M_decimal_create(&dec_cmp1);
	M_decimal_from_int(&dec_cmp1, 12, 1);

	M_decimal_create(&dec_cmp2);
	M_decimal_from_int(&dec_cmp2, 34, 1);

	if (M_decimal_cmp(decimal, &dec_cmp1) != 0 && M_decimal_cmp(decimal, &dec_cmp2) != 0)
		return M_FALSE;

	return M_TRUE;
}

static M_bool check_getopt_string_cb(char short_opt, const char *long_opt, const char *string, void *thunk)
{
	(void)thunk;

	if (string == NULL || *string == '\0' || (short_opt != 0 && short_opt != 's') || (long_opt != NULL && !M_str_caseeq(long_opt, "s2")))
		return M_FALSE;

	if (!M_str_caseeq(string, "abc") && !M_str_caseeq(string, "xyz"))
		return M_FALSE;

	return M_TRUE;
}

static M_bool check_getopt_boolean_cb(char short_opt, const char *long_opt, M_bool boolean, void *thunk)
{
	(void)thunk;

	if ((short_opt != 0 && short_opt != 'b' && short_opt != 'c') || (long_opt != NULL && !M_str_caseeq(long_opt, "b2") && !M_str_caseeq(long_opt, "c2")))
		return M_FALSE;

	if ((short_opt != 0 && short_opt == 'b') || (long_opt != NULL && M_str_caseeq(long_opt, "b2")))
		return boolean;

	if ((short_opt != 0 && short_opt == 'c') || (long_opt != NULL && M_str_caseeq(long_opt, "c2")))
		return !boolean;

	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_getopt_args_success)
{
	M_getopt_t       *g;
	M_getopt_error_t  ret;

	char const *args1[21] = { "1", "-i", "123", "--i2", "456", "-d", "1.2", "--d2", "3.4", "-s", "abc", "--s2", "xyz", "-b", "--b2", "-c", "no", "--c2", "no", "arg1", "arg2" };
	char const *args2[13] = { "2", "-i=123", "--i2=456", "-d=1.2", "--d2=3.4", "-s=abc", "--s2=xyz", "-b", "--b2", "-c=no", "--c2=no", "arg1", "arg2" };
	char const *args3[22] = { "3", "-i", "123", "--i2", "456", "-d", "1.2", "--d2", "3.4", "-s", "abc", "--s2", "xyz", "-b", "--b2", "-c", "no", "--c2", "no", "--", "arg1", "arg2" };
	char const *args4[6]  = { "4", "-c", "no", "-b", "arg1", "arg2" };
	char const *args5[7]  = { "5", "--c2", "no", "--b2", "--", "arg1", "arg2" };
	char const *args6[9]  = { "6", "--c2", "no", "--b2", "--", "-a", "arg1", "arg2", "--arg4" };
	char const *args7[3]  = { "7", "-bc", "no" };
	char const *args8[2]  = { "7", "-c=no" };
	char const *args9[2]  = { "7", "-s=abc" };

	g = M_getopt_create(check_getopt_nonopt_cb);	

	M_getopt_addinteger(g, 'i', "i1", M_TRUE, "blah", check_getopt_int_cb);
	M_getopt_addinteger(g, 0, "i2", M_TRUE, NULL, check_getopt_int_cb);
	M_getopt_adddecimal(g, 'd', NULL, M_TRUE, NULL, check_getopt_dec_cb);
	M_getopt_adddecimal(g, 0, "d2", M_TRUE, NULL, check_getopt_dec_cb);
	M_getopt_addstring(g, 's', NULL, M_TRUE, NULL, check_getopt_string_cb);
	M_getopt_addstring(g, 0, "s2", M_TRUE, NULL, check_getopt_string_cb);
	M_getopt_addboolean(g, 'b', NULL, M_FALSE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 0, "b2", M_FALSE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 'c', NULL, M_TRUE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 0, "c2", M_TRUE, NULL, check_getopt_boolean_cb);

	ret = M_getopt_parse(g, args1, 21, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args1 failure");

	ret = M_getopt_parse(g, args2, 13, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args2 failure");

	ret = M_getopt_parse(g, args3, 22, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args3 failure");

	ret = M_getopt_parse(g, args4, 6, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args4 failure");

	ret = M_getopt_parse(g, args5, 7, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args5 failure");

	ret = M_getopt_parse(g, args6, 9, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args6 failure");

	ret = M_getopt_parse(g, args7, 3, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args7 failure");

	ret = M_getopt_parse(g, args8, 2, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args8 failure");

	ret = M_getopt_parse(g, args9, 2, NULL, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_SUCCESS, "args9 failure");

	M_getopt_destroy(g);
}
END_TEST

START_TEST(check_getopt_args_fail)
{
	M_getopt_t       *g;
	M_getopt_error_t  ret;
	const char       *fail;

	char const *args1[5] = { "1", "-c", "--", "arg1", "arg2" };
	char const *args2[3] = { "2", "-i", "str" };
	char const *args3[3] = { "3", "--i2", "str" };
	char const *args4[3] = { "4", "-d", "str" };
	char const *args5[3] = { "5", "--d2", "str" };
	char const *args6[2] = { "6", "-s" };
	char const *args7[3] = { "7", "-s", "str" };
	char const *args8[2] = { "8", "--s2" };
	char const *args9[2] = { "9", "-s=" };
	char const *args10[6] = { "10", "-b", "arg1", "-s", "abc", "arg2" };
	char const *args11[4] = { "11", "-i", "-s", "s" };
	char const *args12[3] = { "12", "-ib", "123" };
	char const *args13[2] = { "13", "-j" };
	char const *args14[2] = { "14", "xarg" };
	char const *args15[2] = { "15", "-s=abc=xyz" };

	g = M_getopt_create(check_getopt_nonopt_cb);	

	M_getopt_addinteger(g, 'i', NULL, M_TRUE, NULL, check_getopt_int_cb);
	M_getopt_addinteger(g, 0, "i2", M_TRUE, NULL, check_getopt_int_cb);
	M_getopt_adddecimal(g, 'd', NULL, M_TRUE, NULL, check_getopt_dec_cb);
	M_getopt_adddecimal(g, 0, "d2", M_TRUE, NULL, check_getopt_dec_cb);
	M_getopt_addstring(g, 's', NULL, M_TRUE, NULL, check_getopt_string_cb);
	M_getopt_addstring(g, 0, "s2", M_TRUE, NULL, check_getopt_string_cb);
	M_getopt_addboolean(g, 'b', NULL, M_FALSE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 0, "b2", M_FALSE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 'c', NULL, M_TRUE, NULL, check_getopt_boolean_cb);
	M_getopt_addboolean(g, 0, "c2", M_TRUE, NULL, check_getopt_boolean_cb);

	ret = M_getopt_parse(g, args1, 5, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-c"), "args1 failure");

	ret = M_getopt_parse(g, args2, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDDATATYPE && M_str_eq(fail, "-i"), "args2 failure, %d, %s", ret, fail);

	ret = M_getopt_parse(g, args3, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDDATATYPE && M_str_eq(fail, "--i2"), "args3 failure");

	ret = M_getopt_parse(g, args4, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDDATATYPE && M_str_eq(fail, "-d"), "args4 failure");

	ret = M_getopt_parse(g, args5, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDDATATYPE && M_str_eq(fail, "--d2"), "args5 failure");

	ret = M_getopt_parse(g, args6, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-s"), "args6 failure");

	ret = M_getopt_parse(g, args7, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDDATATYPE && M_str_eq(fail, "-s"), "args7 failure");

	ret = M_getopt_parse(g, args8, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "--s2"), "args8 failure");

	ret = M_getopt_parse(g, args9, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-s="), "args9 failure %d", ret);

	ret = M_getopt_parse(g, args10, 6, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDORDER && M_str_eq(fail, "-s"), "args10 failure");

	ret = M_getopt_parse(g, args11, 4, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-i"), "args11 failure");

	ret = M_getopt_parse(g, args12, 3, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-ib"), "args12 failure");

	ret = M_getopt_parse(g, args13, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_INVALIDOPT && M_str_eq(fail, "-j"), "args13 failure");

	ret = M_getopt_parse(g, args14, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_NONOPTION && M_str_eq(fail, "xarg"), "args14 failure");

	ret = M_getopt_parse(g, args15, 2, &fail, NULL);
	ck_assert_msg(ret == M_GETOPT_ERROR_MISSINGVALUE && M_str_eq(fail, "-s=abc=xyz"), "args15 failure");

	M_getopt_destroy(g);
}
END_TEST

START_TEST(check_getopt_help)
{
	M_getopt_t *g;
	char       *help;

	g = M_getopt_create(check_getopt_nonopt_cb);	

	M_getopt_addinteger(g, 'i', "i1", M_TRUE, "DESCR 1", check_getopt_int_cb);
	M_getopt_addinteger(g, 0, "i2", M_FALSE, NULL, check_getopt_int_cb);
	M_getopt_adddecimal(g, 'd', NULL, M_TRUE, "DDESCR", check_getopt_dec_cb);
	M_getopt_addboolean(g, 'b', NULL, M_FALSE, "B DESC 1", check_getopt_boolean_cb);
	M_getopt_addboolean(g, 0, "c2", M_TRUE, NULL, check_getopt_boolean_cb);
	M_getopt_addstring(g, 's', "ssss", M_TRUE, "SSSSSSSS", check_getopt_string_cb);

	help = M_getopt_help(g);
	ck_assert_msg(M_str_eq(help, CHECK_GETOPT_HELP), "got='%s'\nexpected='%s'", help, CHECK_GETOPT_HELP);

	M_free(help);
	M_getopt_destroy(g);

}
END_TEST

START_TEST(check_getopt_add)
{
	M_getopt_t *g;

	g = M_getopt_create(check_getopt_nonopt_cb);	

	ck_assert_msg(M_getopt_addinteger(g, 'i', "i1", M_TRUE, "DESCR 1", check_getopt_int_cb), "-i, --i1 add failed");
	ck_assert_msg(M_getopt_addinteger(g, 0, "i2", M_FALSE, NULL, check_getopt_int_cb), "--i2 add failed");
	ck_assert_msg(M_getopt_adddecimal(g, 'd', NULL, M_TRUE, "DDESCR", check_getopt_dec_cb), "-d add failed");
	ck_assert_msg(M_getopt_addboolean(g, 'b', NULL, M_FALSE, "B DESC 1", check_getopt_boolean_cb), "-b add failed");
	ck_assert_msg(M_getopt_addboolean(g, 0, "c2", M_TRUE, NULL, check_getopt_boolean_cb), "--c2 add failed");
	ck_assert_msg(M_getopt_addstring(g, 's', "ssss", M_TRUE, "SSSSSSSS", check_getopt_string_cb), "-s, --ssss add failed");

	ck_assert_msg(!M_getopt_addstring(g, 's', NULL, M_TRUE, "SSSSSSSS", check_getopt_string_cb), "duplicate -s add suceeded");
	ck_assert_msg(!M_getopt_addstring(g, 0, "ssss", M_TRUE, "SSSSSSSS", check_getopt_string_cb), "duplicate --ssss add suceeded");
	ck_assert_msg(!M_getopt_addinteger(g, 0, NULL, M_FALSE, NULL, check_getopt_int_cb), "'' add suceeded");
	ck_assert_msg(!M_getopt_addinteger(g, 'a', NULL, M_FALSE, NULL, NULL), "-a add suceeded");

	M_getopt_destroy(g);

}
END_TEST

START_TEST(check_getopt_thunk_list)
{
	M_getopt_t   *g;
	M_list_str_t *t;
	const char   *r;
	char const   *args[2] = { "1", "topt" };

	g = M_getopt_create(check_getopt_nonopt_thunk_list_cb);	
	t = M_list_str_create(M_LIST_STR_NONE);

	ck_assert_msg(M_getopt_parse(g, args, 2, NULL, t) == M_GETOPT_ERROR_SUCCESS, "getopt parse failed");
	ck_assert_msg(M_list_str_len(t) == 1, "list length is not 1\n");
	r = M_list_str_at(t, 0);
	ck_assert_msg(M_str_eq(r, "topt"), "got (%s) != expected (topt)", r);

	M_list_str_destroy(t);
	M_getopt_destroy(g);

}
END_TEST

START_TEST(check_getopt_thunk_char)
{
	M_getopt_t   *g;
	char         *r       = NULL;
	char const   *args[2] = { "1", "topt" };

	g = M_getopt_create(check_getopt_nonopt_thunk_char_cb);	

	ck_assert_msg(M_getopt_parse(g, args, 2, NULL, &r) == M_GETOPT_ERROR_SUCCESS, "getopt parse failed");
	ck_assert_msg(M_str_eq(r, "topt"), "got (%s) != expected (topt)", r);

	M_free(r);
	M_getopt_destroy(g);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_getopt_suite(void)
{
	Suite *suite;
	TCase *tc_getopt_args_success;
	TCase *tc_getopt_args_fail;
	TCase *tc_getopt_help;
	TCase *tc_getopt_add;
	TCase *tc_getopt_thunk_list;
	TCase *tc_getopt_thunk_char;

	suite = suite_create("getopt");

	tc_getopt_args_success = tcase_create("check_getopt_args_success");
	tcase_add_test(tc_getopt_args_success, check_getopt_args_success);
	suite_add_tcase(suite, tc_getopt_args_success);

	tc_getopt_args_fail = tcase_create("check_getopt_args_fail");
	tcase_add_test(tc_getopt_args_fail, check_getopt_args_fail);
	suite_add_tcase(suite, tc_getopt_args_fail);

	tc_getopt_help = tcase_create("check_getopt_help");
	tcase_add_test(tc_getopt_help, check_getopt_help);
	suite_add_tcase(suite, tc_getopt_help);

	tc_getopt_add = tcase_create("check_getopt_add");
	tcase_add_test(tc_getopt_add, check_getopt_add);
	suite_add_tcase(suite, tc_getopt_add);

	tc_getopt_thunk_list = tcase_create("check_getopt_thunk_list");
	tcase_add_test(tc_getopt_thunk_list, check_getopt_thunk_list);
	suite_add_tcase(suite, tc_getopt_thunk_list);

	tc_getopt_thunk_char = tcase_create("check_getopt_thunk_char");
	tcase_add_test(tc_getopt_thunk_char, check_getopt_thunk_char);
	suite_add_tcase(suite, tc_getopt_thunk_char);


	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_getopt_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_getopt.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

