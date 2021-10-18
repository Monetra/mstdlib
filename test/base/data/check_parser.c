#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
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
		ck_assert_msg(M_buf_add_uintbcd(buf, n, bytes), "%lu: Could not convert '%"PRIu64"' to bcd", i, n);
		out = M_buf_finish(buf, &out_len);
		ck_assert_msg(bytes == out_len, "%lu: out_len (%lu) != bytes (%lu)", i, out_len, out_len);

		parser = M_parser_create_const(out, out_len, M_PARSER_FLAG_NONE);
		ck_assert_msg(M_parser_read_uint_bcd(parser, bytes, &out_n), "%lu: could not read bcd from parser", i);
		ck_assert_msg(out_n == n, "%lu: out_n (%"PRIu64") != n (%"PRIu64")", i, out_n, n);

		M_parser_destroy(parser);
		M_free(out);
	}
}
END_TEST

START_TEST(check_parser_split)
{
	M_parser_t  *parser;
	M_parser_t **parts;
	char         buf[256];
	size_t       num_parts;
	const char  *split_text = "ABC\r\nTEST 123\r\nA\nB\rC\r\n\r\n";

	parser = M_parser_create_const((const unsigned char *)split_text, M_str_len(split_text), M_PARSER_FLAG_NONE);
	M_parser_mark(parser);

	parts  = M_parser_split_str_pat(parser, "\r\n", 0, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	ck_assert_msg(parts != NULL && num_parts == 4, "Split 1 failed");
	if (parts == NULL) { /* Silence false-positive static analyzer warning */
		return;
	}

	M_parser_read_str(parts[0], M_parser_len(parts[0]), buf, sizeof(buf));
	ck_assert_msg(M_str_caseeq(buf, "ABC"), "Split 1 [0] got '%s', expected '%s'", buf, "ABC");

	M_parser_read_str(parts[1], M_parser_len(parts[1]), buf, sizeof(buf));
	ck_assert_msg(M_str_caseeq(buf, "TEST 123"), "Split 1 [1] got '%s', expected '%s'", buf, "TEST 123");

	M_parser_read_str(parts[2], M_parser_len(parts[2]), buf, sizeof(buf));
	ck_assert_msg(M_str_caseeq(buf, "A\nB\rC"), "Split 1 [2] got '%s', expected '%s'", buf, "A\nB\rC");

	ck_assert_msg(M_parser_len(parts[3]) == 0, "Split 1 [3] has data, should be empty");

	M_parser_split_free(parts, num_parts);


	M_parser_mark_rewind(parser);
	parts  = M_parser_split_str_pat(parser, "\r\n", 2, M_PARSER_SPLIT_FLAG_NONE, &num_parts);
	ck_assert_msg(parts != NULL && num_parts == 2, "Split 2 failed");
	if (parts == NULL) { /* Silence false-positive static analyzer warning */
		return;
	}

	M_parser_read_str(parts[0], M_parser_len(parts[0]), buf, sizeof(buf));
	ck_assert_msg(M_str_caseeq(buf, "ABC"), "Split 2 [0] got '%s', expected '%s'", buf, "ABC");

	M_parser_read_str(parts[1], M_parser_len(parts[1]), buf, sizeof(buf));
	ck_assert_msg(M_str_caseeq(buf, "TEST 123\r\nA\nB\rC\r\n\r\n"), "Split 2 [1] got '%s', expected '%s'", buf, "TEST 123A\nB\rC");

	M_parser_split_free(parts, num_parts);

	M_parser_destroy(parser);
}
END_TEST

START_TEST(check_parser_boundary)
{
	M_parser_t  *parser;
	char         buf[32];
	size_t       len;
	size_t       i;
	M_bool       found;
	const char  *boundary = "AB7";
	static struct {
		const char *data;
		const char *out_data;
		M_bool      eat_pat;
		M_bool      found;
	} boundary_data[] = {
		{ "1234",              "1234",              M_FALSE, M_FALSE },
		{ "1234",              "1234",              M_TRUE,  M_FALSE },
		{ "1234A",             "1234",              M_FALSE, M_FALSE },
		{ "1234A",             "1234",              M_TRUE,  M_FALSE },
		{ "1234AB",            "1234",              M_FALSE, M_FALSE },
		{ "1234AB",            "1234",              M_TRUE,  M_FALSE },
		{ "1234AB7",           "1234",              M_FALSE, M_TRUE  },
		{ "1234AB7",           "1234AB7",           M_TRUE,  M_TRUE  },
		{ "1234AB7AB7",        "1234",              M_FALSE, M_TRUE  },
		{ "1234AB7AB7",        "1234AB7",           M_TRUE,  M_TRUE  },
		{ "1234AB7*",          "1234",              M_FALSE, M_TRUE  },
		{ "1234AB7*",          "1234AB7",           M_TRUE,  M_TRUE  },
		{ "1234AB7*AB7",       "1234",              M_FALSE, M_TRUE  },
		{ "1234AB7*AB7",       "1234AB7",           M_TRUE,  M_TRUE  },
		{ "1234AB891AB97*AB7", "1234AB891AB97*",    M_FALSE, M_TRUE  },
		{ "1234AB891AB97*AB7", "1234AB891AB97*AB7", M_TRUE,  M_TRUE  },
		{ NULL, NULL, M_FALSE, M_FALSE },
	};

	for (i=0; boundary_data[i].data!=NULL; i++) {
		parser = M_parser_create_const((const unsigned char *)boundary_data[i].data, M_str_len(boundary_data[i].data), M_PARSER_FLAG_NONE);
		len    = M_parser_read_str_boundary(parser, buf, sizeof(buf), boundary, boundary_data[i].eat_pat, &found);
		ck_assert_msg(M_str_eq(buf, boundary_data[i].out_data), "%lu: Wrong data read: got '%s', expected '%s'", i, buf, boundary_data[i].out_data);
		ck_assert_msg(len == M_str_len(boundary_data[i].out_data), "%lu: Wrong length returned: got '%lu' expected '%lu", i, len, M_str_len(boundary_data[i].out_data));
		ck_assert_msg(found == boundary_data[i].found, "%lu: boundary found not correct; got %d expected %d", i, found, boundary_data[i].found);
		M_parser_destroy(parser);
	}
}
END_TEST

static M_bool trunc_pred_func(unsigned char c)
{
	if (c == 'A' || c == 'a')
		return M_TRUE;
	return M_FALSE;
}

START_TEST(check_parser_truncate_predicate)
{
	M_parser_t  *parser;
	size_t       i;
	size_t       r;

	static struct {
		const char *data;
		const char *out_data;
		size_t      max;
		size_t      r;
	} trunc_data[] = {
		{ "abc",     "abc",     0, 0 },
		{ "abc",     "abc",     2, 0 },
		{ "aaa",     "",        0, 3 },
		{ "bbbaaa",  "bbb",     0, 3 },
		{ "bbbaaa",  "bbb",     4, 3 },
		{ "bbbaaac", "bbbaaac", 4, 0 },
		{ "bbbaaac", "bbbaaac", 0, 0 },
		{ NULL, NULL, 0, 0 }
	};

	for (i=0; trunc_data[i].data!=NULL; i++) {
		parser = M_parser_create_const((const unsigned char *)trunc_data[i].data, M_str_len(trunc_data[i].data), M_PARSER_FLAG_NONE);

		if (trunc_data[i].max == 0) {
			r = M_parser_truncate_predicate(parser, trunc_pred_func);
		} else {
			r = M_parser_truncate_predicate_max(parser, trunc_pred_func, trunc_data[i].max);
		}

		ck_assert_msg(r == trunc_data[i].r, "%lu: Wrong truncated amount: got '%lu', expected '%lu'", i, r, trunc_data[i].r);
		ck_assert_msg(M_parser_len(parser) == M_str_len(trunc_data[i].out_data), "%lu: Wrong truncated data size: got '%lu', expected '%lu'", i, M_parser_len(parser), M_str_len(trunc_data[i].out_data));
		ck_assert_msg(M_str_eq_max((const char *)M_parser_peek(parser), trunc_data[i].out_data, M_parser_len(parser)), "%lu: Wrong data read: got '%.*s', expected '%s'", i, (int)M_parser_len(parser), M_parser_peek(parser), trunc_data[i].out_data);

		M_parser_destroy(parser);
	}
}
END_TEST

START_TEST(check_parser_truncate_until)
{
	M_parser_t  *parser;
	size_t       i;
	size_t       r;

	static struct {
		const char *data;
		const char *out_data;
		const char *pat;
		M_bool      eat;
		size_t      r;
	} trunc_data[] = {
		{ "abc",                               "abc",                               "x",          M_TRUE,  3  },
		{ "abc",                               "abc",                               "x",          M_FALSE, 3  },

		{ "abc",                               "",                                  "a",          M_TRUE,  3  },
		{ "abc",                               "a",                                 "a",          M_FALSE, 2  },

		{ "aaa",                               "aaa",                               "b",          M_TRUE,  3  },
		{ "aaa",                               "aaa",                               "b",          M_FALSE, 3  },

		{ "bbbaaa",                            "bbba",                              "aa",         M_TRUE,  2  },
		{ "bbbaaa",                            "bbbaaa",                            "",           M_FALSE, 0  },

		{ "bbbaaa",                            "bb",                                "b",          M_TRUE,  4  },
		{ "bbbaaa",                            "bbb",                               "b",          M_FALSE, 3  },

		{ "bbbaaac",                           "bb",                                "ba",         M_TRUE,  5  },
		{ "bbbaaac",                           "bbba",                              "ba",         M_FALSE, 3  },

		{ "bbbaaac",                           "",                                  "bbb",        M_TRUE,  7  },
		{ "bbbaaac",                           "bbb",                               "bbb",        M_FALSE, 4  },

		{ "bbbaaac",                           "bb",                                "b",          M_TRUE,  5  },
		{ "bbbaaac",                           "bbb",                               "b",          M_FALSE, 4  },

		{ "bbbaaac",                           "b",                                 "bb",         M_TRUE,  6  },
		{ "bbbaaac",                           "bbb",                               "bb",         M_FALSE, 4  },

		{ "this is a test of some long stuff", "this is a test of some ",           "long stuff", M_TRUE,  10 },
		{ "this is a test of some long stuff", "this is a test of some long stuff", "long stuff", M_FALSE, 0  },

		{ "this is a test of some long stuff", "this is a ",                        "test ",      M_TRUE,  23 },
		{ "this is a test of some long stuff", "this is a test ",                   "test ",      M_FALSE, 18 },

		{ NULL, NULL, NULL, M_FALSE, 0 }
	};

	for (i=0; trunc_data[i].data!=NULL; i++) {
		parser = M_parser_create_const((const unsigned char *)trunc_data[i].data, M_str_len(trunc_data[i].data), M_PARSER_FLAG_NONE);

		r = M_parser_truncate_until(parser, (const unsigned char *)trunc_data[i].pat, M_str_len(trunc_data[i].pat), trunc_data[i].eat);

		ck_assert_msg(r == trunc_data[i].r, "%lu: Wrong truncated amount: got '%lu', expected '%lu'", i, r, trunc_data[i].r);
		ck_assert_msg(M_parser_len(parser) == M_str_len(trunc_data[i].data)-trunc_data[i].r, "%lu: Wrong truncated data size: got '%lu', expected '%lu'", i, M_parser_len(parser), M_str_len(trunc_data[i].out_data));
		ck_assert_msg(M_str_eq_max((const char *)M_parser_peek(parser), trunc_data[i].out_data, M_parser_len(parser)), "%lu: Wrong data read: got '%.*s', expected '%s'", i, (int)M_parser_len(parser), M_parser_peek(parser), trunc_data[i].out_data);

		M_parser_destroy(parser);
	}
}
END_TEST

START_TEST(check_parser_truncate_charset)
{
	M_parser_t  *parser;
	size_t       i;
	size_t       r;

	static struct {
		const char *data;
		const char *out_data;
		const char *set;
		size_t      r;
	} trunc_data[] = {
		{ "abc",     "abc",     "x",       0 },
		{ "abc",     "a",       "bc",      2 },
		{ "abc",     "abc",     "a",       0 },
		{ "abc",     "",        "abc",     3 },
		{ "abc",     "ab",      "c",       1 },
		{ "aaa",     "",        "a",       3 },
		{ "bbbaaa",  "bbb",     "a",       3 },
		{ "bbbaaac", "bbbaaa",  "c",       1 },
		{ "bbbaaac", "bbbaaac", "x6eafda", 0 },
		{ "bbbaaac", "bbbaaac", "x6ecfda", 4 },
		{ "bbbaaac", "bbbaaac", "x6ecfd",  1 },

		{ "this is a test of some long stuff", "this is a test of some ",   "long stuff",  11 },
		{ "this is a test of some long stuff", "this is a test of some lo", "a ishtfung",   8 },
		{ "this is a test of some long stuff", "this is a test of some l",  "a ishtfungo",  9 },

		{ NULL, NULL, NULL, 0 }
	};

	for (i=0; trunc_data[i].data!=NULL; i++) {
		parser = M_parser_create_const((const unsigned char *)trunc_data[i].data, M_str_len(trunc_data[i].data), M_PARSER_FLAG_NONE);

		r = M_parser_truncate_charset(parser, (const unsigned char *)trunc_data[i].set, M_str_len(trunc_data[i].set));

		ck_assert_msg(r == trunc_data[i].r, "%lu: Wrong truncated amount: got '%lu', expected '%lu'", i, r, trunc_data[i].r);
		ck_assert_msg(M_parser_len(parser) == M_str_len(trunc_data[i].data)-trunc_data[i].r, "%lu: Wrong truncated data size: got '%lu', expected '%lu'", i, M_parser_len(parser), M_str_len(trunc_data[i].out_data));
		ck_assert_msg(M_str_eq_max((const char *)M_parser_peek(parser), trunc_data[i].out_data, M_parser_len(parser)), "%lu: Wrong data read: got '%.*s', expected '%s'", i, (int)M_parser_len(parser), M_parser_peek(parser), trunc_data[i].out_data);

		M_parser_destroy(parser);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_parser_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("parser");

	tc = tcase_create("check_parser_read_strdup_hex");
	tcase_add_test(tc, check_parser_read_strdup_hex);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_read_buf_hex");
	tcase_add_test(tc, check_parser_read_buf_hex);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_bcd");
	tcase_add_test(tc, check_parser_bcd);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_split");
	tcase_add_test(tc, check_parser_split);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_boundary");
	tcase_add_test(tc, check_parser_boundary);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_truncate_predicate");
	tcase_add_test(tc, check_parser_truncate_predicate);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_truncate_until");
	tcase_add_test(tc, check_parser_truncate_until);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_parser_truncate_charset");
	tcase_add_test(tc, check_parser_truncate_charset);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_parser_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_parser.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

