#include "m_config.h"
#include <check.h>
#include <stdlib.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *types_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_bool)
{
	ck_assert_msg(M_false == 0,       "M_false is wrong");
	ck_assert_msg(M_false == M_FALSE, "M_FALSE is wrong");
	ck_assert_msg(M_true  != M_false, "M_true is wrong");
	ck_assert_msg(M_true == M_TRUE,   "M_TRUE is wrong");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint8)
{
	ck_assert_msg(sizeof(M_uint8) == 1, "unexpected sizeof");
	ck_assert_msg(M_UINT8_MAX  ==  ( 255 ), "unexpected max");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_int8)
{
	ck_assert_msg(sizeof(M_int8) == 1, "unexpected sizeof");
	ck_assert_msg(M_INT8_MAX   ==  ( 127 ), "unexpected max");
	ck_assert_msg(M_INT8_MIN   == -( 128 ), "unexpected min");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint16)
{
	ck_assert_msg(sizeof(M_uint16) == 2, "unexpected sizeof");
	ck_assert_msg(M_UINT16_MAX ==  ( 65535 ), "unexpected max");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_int16)
{
	ck_assert_msg(sizeof(M_int16) == 2, "unexpected sizeof");
	ck_assert_msg(M_INT16_MAX  ==  ( 32767 ), "unexpected max");
	ck_assert_msg(M_INT16_MIN  == -( 32768 ), "unexpected min");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint32)
{
	ck_assert_msg(sizeof(M_uint32) == 4, "unexpected sizeof");
	ck_assert_msg(M_UINT32_MAX ==  ( 4294967295 ), "unexpected max");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_int32)
{
	ck_assert_msg(sizeof(M_int32) == 4, "unexpected sizeof");
	ck_assert_msg(M_INT32_MAX  ==  ( 2147483647 ), "unexpected max");
	ck_assert_msg(M_INT32_MIN  == -( 2147483648 ), "unexpected min");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_uint64)
{
	ck_assert_msg(sizeof(M_uint64) == 8, "unexpected sizeof");
	ck_assert_msg(M_UINT64_MAX ==  ( 18446744073709551615ULL ), "unexpected max");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_int64)
{
	ck_assert_msg(sizeof(M_int64) == 8, "unexpected sizeof");
	ck_assert_msg(M_INT64_MAX  == (M_uint64)  9223372036854775807UL, "unexpected max");
	ck_assert_msg(M_INT64_MIN  == (M_int64 ) -9223372036854775808UL, "unexpected min");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_intptr)
{
	ck_assert_msg(sizeof(M_intptr) == sizeof(void *), "unexpected sizeof");
	ck_assert_msg(M_INTPTR_MAX == SSIZE_MAX, "unexpected max");
	ck_assert_msg(M_INTPTR_MIN == SSIZE_MIN, "unexpected min");

	switch (sizeof(void *)) {
	case 4:
		ck_assert_msg(M_INTPTR_MAX == M_INT32_MAX, "unexpected max");
		ck_assert_msg(M_INTPTR_MIN == M_INT32_MIN, "unexpected min");
		break;
	case 8:
		ck_assert_msg(M_INTPTR_MAX == M_INT64_MAX, "unexpected max");
		ck_assert_msg(M_INTPTR_MIN == M_INT64_MIN, "unexpected min");
		break;
	default:
		ck_abort_msg("unexpected sizeof");
		break;
	}
}
END_TEST

START_TEST(check_uintptr)
{
	ck_assert_msg(sizeof(M_uintptr) == sizeof(void *), "unexpected sizeof");
	ck_assert_msg(M_UINTPTR_MAX == ((size_t)-1), "unexpected max");

	switch (sizeof(void *)) {
	case sizeof(M_uint32):
		ck_assert_msg(M_UINTPTR_MAX == M_UINT32_MAX, "unexpected max");
		break;
	case sizeof(M_uint64):
		ck_assert_msg(M_UINTPTR_MAX == M_UINT64_MAX, "unexpected max");
		break;
	default:
		ck_abort_msg("unexpected sizeof");
		break;
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *types_suite(void)
{
	Suite *suite;
	TCase *tc_bool;
	TCase *tc_int8;
	TCase *tc_int16;
	TCase *tc_int32;
	TCase *tc_int64;
	TCase *tc_uint8;
	TCase *tc_uint16;
	TCase *tc_uint32;
	TCase *tc_uint64;
	TCase *tc_intptr;
	TCase *tc_uintptr;

	suite = suite_create("types");

	tc_bool = tcase_create("bool");
	tcase_add_test(tc_bool , check_bool);
	suite_add_tcase(suite, tc_bool);

	tc_int8 = tcase_create("int8");
	tcase_add_test(tc_int8 , check_int8);
	suite_add_tcase(suite, tc_int8);

	tc_int16 = tcase_create("int16");
	tcase_add_test(tc_int16 , check_int16);
	suite_add_tcase(suite, tc_int16);

	tc_int32 = tcase_create("int32");
	tcase_add_test(tc_int32 , check_int32);
	suite_add_tcase(suite, tc_int32);

	tc_int64 = tcase_create("int64");
	tcase_add_test(tc_int64 , check_int64);
	suite_add_tcase(suite, tc_int64);

	tc_uint8 = tcase_create("uint8");
	tcase_add_test(tc_uint8 , check_uint8);
	suite_add_tcase(suite, tc_uint8);

	tc_uint16 = tcase_create("uint16");
	tcase_add_test(tc_uint16 , check_uint16);
	suite_add_tcase(suite, tc_uint16);

	tc_uint32 = tcase_create("uint32");
	tcase_add_test(tc_uint32 , check_uint32);
	suite_add_tcase(suite, tc_uint32);

	tc_uint64 = tcase_create("uint64");
	tcase_add_test(tc_uint64 , check_uint64);
	suite_add_tcase(suite, tc_uint64);

	tc_intptr = tcase_create("intptr");
	tcase_add_test(tc_intptr , check_intptr);
	suite_add_tcase(suite, tc_intptr);

	tc_uintptr = tcase_create("uintptr");
	tcase_add_test(tc_uintptr , check_uintptr);
	suite_add_tcase(suite, tc_uintptr);

	return suite;
}

int main(void)
{
	SRunner *sr;
	int      nf;

	sr = srunner_create(types_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_types.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
