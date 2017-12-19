#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

static const char    *test_hex   = "9F33036020C8";
static const M_uint8  test_bin[] = {0x9F, 0x33, 0x03, 0x60, 0x20, 0xC8};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_buf_add_bytes_hex)
{
	M_buf_t *buf = M_buf_create();

	M_buf_add_bytes_hex(buf, test_hex);

	ck_assert_msg(M_buf_len(buf) == sizeof(test_bin), "size doesn't match");
	ck_assert_msg(M_mem_eq(test_bin, M_buf_peek(buf), sizeof(test_bin)), "output doesn't match");

	M_buf_cancel(buf);
}
END_TEST

START_TEST(check_buf_add_str_hex)
{
	M_buf_t *buf = M_buf_create();

	M_buf_add_str_hex(buf, test_bin, sizeof(test_bin));

	ck_assert_msg(M_str_caseeq(test_hex, M_buf_peek(buf)), "output doesn't match");

	M_buf_cancel(buf);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	M_uint64    n;
	size_t      bytes;
	M_endian_t  endianness;
	const char *hex;
} check_buf_uintbin_data[] = {
	{ 1,          8, M_ENDIAN_BIG,    "0000000000000001" },
	{ 1,          8, M_ENDIAN_LITTLE, "0100000000000000" },
	{ 1,          1, M_ENDIAN_BIG,    "01"               },
	{ 100,        4, M_ENDIAN_BIG,    "00000064"         },
	{ 100,        4, M_ENDIAN_LITTLE, "64000000"         },
	{ 100,        1, M_ENDIAN_LITTLE, "64"               },
	{ 222,        3, M_ENDIAN_BIG,    "0000DE"           },
	{ 222,        3, M_ENDIAN_LITTLE, "DE0000"           },
	{ 222,        1, M_ENDIAN_LITTLE, "DE"               },
	{ 9999,       2, M_ENDIAN_BIG,    "270F"             },
	{ 9999,       2, M_ENDIAN_LITTLE, "0F27"             },
	{ 43245189,   7, M_ENDIAN_BIG,    "0000000293DE85"   },
	{ 43245189,   7, M_ENDIAN_LITTLE, "85DE9302000000"   },
	{ 1234567890, 5, M_ENDIAN_BIG,    "00499602D2"       },
	{ 1234567890, 5, M_ENDIAN_LITTLE, "D202964900"       },
	{ 0, M_ENDIAN_BIG, 0, NULL }
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_buf_uintbin)
{
	M_buf_t       *buf;
	char          *out_hex;
	unsigned char *out;
	size_t         out_len;
	size_t         i;
	M_uint64       n;
	M_endian_t     endianness;
	const char    *hex;
	size_t         num;

	for (i=0; check_buf_uintbin_data[i].n!=0; i++) {
		n          = check_buf_uintbin_data[i].n;
		num        = check_buf_uintbin_data[i].bytes;
		endianness = check_buf_uintbin_data[i].endianness;
		hex        = check_buf_uintbin_data[i].hex;

		buf = M_buf_create();
		ck_assert_msg(M_buf_add_uintbin(buf, n, num, endianness), "%zu: Could not convert '%llu' to bin, with %s", i, n, endianness==M_ENDIAN_BIG?"BIG":"LITTLE");
		out = M_buf_finish(buf, &out_len);

		out_hex = M_bincodec_encode_alloc((M_uint8 *)out, out_len, 0, M_BINCODEC_HEX);
		M_free(out);
		ck_assert_msg(M_str_caseeq(out_hex, hex), "%zu: '%s' does not match expected '%s' with %s", i, out_hex, hex, endianness==M_ENDIAN_BIG?"BIG":"LITTLE");

		M_free(out_hex);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char        *in;
	unsigned char      base;
	size_t             bytes;
	const char        *hex;
} check_buf_strbin_data[] = {
	{ "0000000000000001", 16, 8, "0000000000000001" },
	{ "00000064",         16, 4, "00000064"         },
	{ "0000DE",           16, 3, "0000DE"           },
	{ "270F",             16, 2, "270F"             },
	{ "0000000293DE85",   16, 7, "0000000293DE85"   },
	{ "00499602D2",       16, 5, "00499602D2"       },
	{ "1",                10, 8, "0000000000000001" },
	{ "1",                10, 1, "01"               },
	{ "100",              10, 4, "00000064"         },
	{ "100",              10, 1, "64"               },
	{ "222",              10, 3, "0000DE"           },
	{ "9999",             10, 2, "270F"             },
	{ "43245189",         10, 7, "0000000293DE85"   },
	{ "1234567890",       10, 5, "00499602D2"       },
	{ NULL, 0, 0, NULL }
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_buf_strbin)
{
	M_buf_t           *buf;
	const char        *in;
	size_t             bytes;
	const char        *hex;
	unsigned char      base;
	unsigned char     *out;
	size_t             out_len = 0;
	char              *out_hex;
	size_t             i;

	for (i=0; check_buf_strbin_data[i].in!=NULL; i++) {
		in    = check_buf_strbin_data[i].in;
		base  = check_buf_strbin_data[i].base;
		bytes = check_buf_strbin_data[i].bytes;
		hex   = check_buf_strbin_data[i].hex;

		buf = M_buf_create();
		ck_assert_msg(M_buf_add_uintstrbin(buf, in, base, bytes, M_ENDIAN_BIG), "%zu: Could not convert '%s' to bin", i, in);
		out = M_buf_finish(buf, &out_len);

		out_hex = M_bincodec_encode_alloc((M_uint8 *)out, out_len, 0, M_BINCODEC_HEX);
		M_free(out);
		ck_assert_msg(M_str_caseeq(out_hex, hex), "%zu: '%s' does not match expected '%s'", i, out_hex, hex);

		M_free(out_hex);
	}
}
END_TEST

static struct {
	M_uint64    n;
	size_t      just;
	const char *hex;
} check_buf_uintbcd_data[] = {
	{ 0,          1, "00"                 },
	{ 0,          2, "0000"               },
	{ 1,          6, "000000000001"       },
	{ 64,         1, "64"                 },
	{ 100,        2, "0100"               },
	{ 100,        3, "000100"             },
	{ 222,        4, "00000222"           },
	{ 9999,       2, "9999"               },
	{ 10001,      2, NULL                 },
	{ 43245189,   1, NULL                 },
	{ 1234567890, 9, "000000001234567890" },
	{ 0, 0, NULL }
};

START_TEST(check_buf_uintbcd)
{
	M_buf_t       *buf;
	M_uint64       n;
	size_t         just;
	const char    *hex;
	unsigned char *out;
	char          *out_hex;
	size_t         out_len;
	size_t         i;
	M_bool         ret;

	for (i=0; check_buf_uintbcd_data[i].n!=0; i++) {
		n    = check_buf_uintbcd_data[i].n;
		just = check_buf_uintbcd_data[i].just;
		hex  = check_buf_uintbcd_data[i].hex;

		buf = M_buf_create();
		ret = M_buf_add_uintbcd(buf, n, just);
		if (ret == M_FALSE) {
			ck_assert_msg(hex == NULL, "%zu: Failed to convert %d to bcd", i, n);
			continue;
		}

		out = M_buf_finish(buf, &out_len);

		out_hex = M_bincodec_encode_alloc((M_uint8 *)out, out_len, 0, M_BINCODEC_HEX);
		M_free(out);
		ck_assert_msg(M_str_caseeq(out_hex, hex), "%zu: '%s' does not match expected '%s'", i, out_hex, hex);

		M_free(out_hex);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_buf_suite(void)
{
	Suite *suite;
	TCase *tc_buf_add_bytes_hex;
	TCase *tc_buf_add_str_hex;
	TCase *tc_buf_uintbin;
	TCase *tc_buf_strbin;
	TCase *tc_buf_uintbcd;

	suite = suite_create("buf");

	tc_buf_add_bytes_hex = tcase_create("check_buf_add_bytes_hex");
	tcase_add_test(tc_buf_add_bytes_hex, check_buf_add_bytes_hex);
	suite_add_tcase(suite, tc_buf_add_bytes_hex);

	tc_buf_add_str_hex = tcase_create("check_buf_add_str_hex");
	tcase_add_test(tc_buf_add_str_hex, check_buf_add_str_hex);
	suite_add_tcase(suite, tc_buf_add_str_hex);

	tc_buf_uintbin = tcase_create("check_buf_uintbin");
	tcase_add_test(tc_buf_uintbin, check_buf_uintbin);
	suite_add_tcase(suite, tc_buf_uintbin);

	tc_buf_strbin = tcase_create("check_buf_strbin");
	tcase_add_test(tc_buf_strbin, check_buf_strbin);
	suite_add_tcase(suite, tc_buf_strbin);

	tc_buf_uintbcd = tcase_create("check_buf_uintbcd");
	tcase_add_test(tc_buf_uintbcd, check_buf_uintbcd);
	suite_add_tcase(suite, tc_buf_uintbcd);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_buf_suite());
	srunner_set_log(sr, "check_buf.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

