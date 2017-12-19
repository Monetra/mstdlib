#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	M_uint64 n;
	size_t   bytes;
} check_parser_bcd_data[] = {
	{ 1,          8 },
	{ 1,          1 },
	{ 100,        2 },
	{ 100,        4 },
	{ 222,        3 },
	{ 9999,       2 },
	{ 9999,       6 },
	{ 43245189,   7 },
	{ 1234567890, 5 },
	{ 0, 0 }
};

static const M_uint8  check_parser_hex_in[]     = {0x12u, 0x34u, 0x56u, 0x78u, 0x9Au, 0xBCu, 0xDEu, 0xF0u};
static const char    *check_parser_hex_out      = "123456789ABCDEF0";
static const char    *check_parser_buf_hex_out  = "hello 123456789ABCDEF0";

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_parser_read_strdup_hex)
{
	M_parser_t *parser;
	char       *hex;

	parser = M_parser_create_const(check_parser_hex_in, sizeof(check_parser_hex_in), M_PARSER_FLAG_NONE);
	hex    = M_parser_read_strdup_hex(parser, sizeof(check_parser_hex_in));

	ck_assert_msg(hex != NULL, "couldn't read bytes");
	ck_assert_msg(M_str_caseeq(hex, check_parser_hex_out), "output doesn't match");

	M_parser_destroy(parser);
	M_free(hex);
}
END_TEST


START_TEST(check_parser_read_buf_hex)
{
	M_parser_t *parser;
	M_buf_t    *buf;

	parser = M_parser_create_const(check_parser_hex_in, sizeof(check_parser_hex_in), M_PARSER_FLAG_NONE);
	buf    = M_buf_create();

	M_buf_add_str(buf, "hello ");

	ck_assert_msg(M_parser_read_buf_hex(parser, buf, sizeof(check_parser_hex_in)), "couldn't read bytes");

	ck_assert_msg(M_str_caseeq(M_buf_peek(buf), check_parser_buf_hex_out), "output doesn't match");

	M_parser_destroy(parser);
	M_buf_cancel(buf);
}
END_TEST

START_TEST(check_parser_bcd)
{
	M_buf_t       *buf;
	M_parser_t    *parser;
	M_uint64       n;
	size_t         bytes;
	unsigned char *out;
	size_t         out_len;
	M_uint64       out_n;
	size_t         i;

	for (i=0; check_parser_bcd_data[i].n!=0; i++) {
		n     = check_parser_bcd_data[i].n;
		bytes = check_parser_bcd_data[i].bytes;

		buf = M_buf_create();
		ck_assert_msg(M_buf_add_uintbcd(buf, n, bytes), "%zu: Could not convert '%llu' to bcd", i, n);
		out = M_buf_finish(buf, &out_len);
		ck_assert_msg(bytes == out_len, "%zu: out_len (%zu) != bytes (%zu)", i, out_len, out_len);

		parser = M_parser_create_const(out, out_len, M_PARSER_FLAG_NONE);
		ck_assert_msg(M_parser_read_uint_bcd(parser, bytes, &out_n), "%zu: could not read bcd from parser", i);
		ck_assert_msg(out_n == n, "%zu: out_n (%llu) != n (%llu)", i, out_n, n);

		M_parser_destroy(parser);
		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_parser_suite(void)
{
	Suite *suite;
	TCase *tc_parser_read_strdup_hex;
	TCase *tc_parser_read_buf_hex;
	TCase *tc_parser_bcd;

	suite = suite_create("parser");

	tc_parser_read_strdup_hex = tcase_create("check_parser_read_strdup_hex");
	tcase_add_test(tc_parser_read_strdup_hex, check_parser_read_strdup_hex);
	suite_add_tcase(suite, tc_parser_read_strdup_hex);

	tc_parser_read_buf_hex = tcase_create("check_parser_read_buf_hex");
	tcase_add_test(tc_parser_read_buf_hex, check_parser_read_buf_hex);
	suite_add_tcase(suite, tc_parser_read_buf_hex);

	tc_parser_bcd = tcase_create("check_parser_bcd");
	tcase_add_test(tc_parser_bcd, check_parser_bcd);
	suite_add_tcase(suite, tc_parser_bcd);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_parser_suite());
	srunner_set_log(sr, "check_parser.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

