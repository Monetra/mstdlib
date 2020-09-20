#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *decimal_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_decimal_cmp)
{
	M_decimal_t d1, d2, d3, d4, d5, d6;
	M_decimal_from_int(&d1, 100000, 3);
	M_decimal_from_int(&d2, 100, 0);
	M_decimal_from_int(&d3, 5, 1);
	M_decimal_from_int(&d4, -512, 1);
	M_decimal_from_int(&d5, 0, 0);
	M_decimal_from_int(&d6, 2, 2);

	ck_assert(M_decimal_cmp(&d1, &d2) == 0);
	ck_assert(M_decimal_cmp(&d1, &d3) == 1);
	ck_assert(M_decimal_cmp(&d3, &d1) == -1);
	ck_assert(M_decimal_cmp(&d4, &d3) == -1);
	ck_assert(M_decimal_cmp(&d1, &d3) == 1);
	ck_assert(M_decimal_cmp(&d1, &d5) == 1);
	ck_assert(M_decimal_cmp(&d5, &d6) == -1);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct math_test {
	const char             *d1;
	const char             *d2;
	enum M_DECIMAL_RETVAL (*op)(M_decimal_t *, const M_decimal_t *, const M_decimal_t *);
	enum M_DECIMAL_RETVAL   rv;
	const char             *r;
};

static enum M_DECIMAL_RETVAL M_decimal_divide_trad(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	return M_decimal_divide(dest, dec1, dec2, M_DECIMAL_ROUND_TRADITIONAL);
}

static enum M_DECIMAL_RETVAL M_decimal_transform_trad(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_duplicate(dest, dec1);

	return M_decimal_transform(dest, (M_uint8)M_decimal_to_int(dec2, 0), M_DECIMAL_ROUND_TRADITIONAL);
}

static enum M_DECIMAL_RETVAL M_decimal_transform_bank(M_decimal_t *dest, const M_decimal_t *dec1, const M_decimal_t *dec2)
{
	M_decimal_duplicate(dest, dec1);

	return M_decimal_transform(dest, (M_uint8)M_decimal_to_int(dec2, 0), M_DECIMAL_ROUND_BANKERS);
}

static const struct math_test math_tests[] = {
	/* Basic tests, no decimal places */
	{ "1", "1", M_decimal_add,      M_DECIMAL_SUCCESS, "2" },
	{ "1", "1", M_decimal_subtract, M_DECIMAL_SUCCESS, "0" },
	{ "2", "2", M_decimal_multiply, M_DECIMAL_SUCCESS, "4" },
	{ "9", "3", M_decimal_divide_trad, M_DECIMAL_SUCCESS, "3" },
	{ "9", "0", M_decimal_divide_trad, M_DECIMAL_INVALID, "0" },

	/* Simple tests */
	{ "1.1",        "1.1",       M_decimal_add,         M_DECIMAL_SUCCESS,  "2.2"   },
 	{ "2.2",        "1.1",       M_decimal_subtract,    M_DECIMAL_SUCCESS,  "1.1"   },
	{ "2.2",        "2.2",       M_decimal_multiply,    M_DECIMAL_SUCCESS,  "4.84"  },
	{ "1.23",       "5",         M_decimal_divide_trad, M_DECIMAL_SUCCESS,  "0.246" },
	{ "1.01",       "0.001",     M_decimal_add,         M_DECIMAL_SUCCESS,  "1.011" },

	/* Range */
	{ "9223372036854775807",  "0",     M_decimal_add,      M_DECIMAL_SUCCESS, "9223372036854775807"  },
	{ "9223372036854775807",  "-1",    M_decimal_multiply, M_DECIMAL_SUCCESS, "-9223372036854775807" },
	{ "-9223372036854775808", "1",     M_decimal_add,      M_DECIMAL_SUCCESS, "-9223372036854775807" },
	{ "-9223372036854775807", "1",     M_decimal_subtract, M_DECIMAL_SUCCESS, "-9223372036854775808" },
	{ "9223372036854775.807", "0.807", M_decimal_subtract, M_DECIMAL_SUCCESS, "9223372036854775"     },

	/* Overflow */
	{ "922337203685477580",   "11", M_decimal_multiply,    M_DECIMAL_OVERFLOW, "0"  },
	{ "9223372036854775807",   "1", M_decimal_add,         M_DECIMAL_OVERFLOW, "0"  },
	{ "-9223372036854775808",  "1", M_decimal_subtract,    M_DECIMAL_OVERFLOW, "0"  },
	{ "-9223372036854775808", "-1", M_decimal_divide_trad, M_DECIMAL_OVERFLOW, "0"  },

	/* Truncation */
	{ "9999.123456",          "9999.123456", M_decimal_multiply, M_DECIMAL_TRUNCATION, "99982469.9683223716"  },
	{ "9223372036854111.111", "0.12345",     M_decimal_add,      M_DECIMAL_TRUNCATION, "9223372036854111.234" },
	{ "9223372036854775.807", "0.11111",     M_decimal_subtract, M_DECIMAL_TRUNCATION, "9223372036854775.696" },

	/* Truncation during reading */
	{ "9.999999999999999999999", "0",        M_decimal_add,      M_DECIMAL_SUCCESS,    "9.99999999999999999"  },

	/* Reading of exponents */
	{ "1.00e2",                  "0",        M_decimal_add,      M_DECIMAL_SUCCESS,    "100"                  },
	{ "1.00e-2",                 "0",        M_decimal_add,      M_DECIMAL_SUCCESS,    "0.01"                 },

	/* Rounding */
	{ "1.2344",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "1.234" },
	{ "1.2345",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "1.235" },
	{ "1.2346",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "1.235" },
	{ "1.2344",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "1.234" },
	{ "1.2345",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "1.234" },
	{ "1.2346",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "1.235" },

	{ "-1.2344",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "-1.234" },
	{ "-1.2345",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "-1.235" },
	{ "-1.2346",    "3", M_decimal_transform_trad, M_DECIMAL_TRUNCATION, "-1.235" },
	{ "-1.2344",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "-1.234" },
	{ "-1.2345",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "-1.234" },
	{ "-1.2346",    "3", M_decimal_transform_bank, M_DECIMAL_TRUNCATION, "-1.235" },
};

START_TEST(check_decimal_math)
{
	M_decimal_t           d1;
	M_decimal_t           d2;
	M_decimal_t           exp;
	char                  exp_out[256];
	M_decimal_t           r;
	char                  r_out[256];
	enum M_DECIMAL_RETVAL rv;
	size_t                i;

	for (i=0; i < sizeof(math_tests) / sizeof(*math_tests); i++) {
		M_decimal_from_str(math_tests[i].d1, M_str_len(math_tests[i].d1), &d1, NULL);
		M_decimal_from_str(math_tests[i].d2, M_str_len(math_tests[i].d2), &d2, NULL);
		M_decimal_from_str(math_tests[i].r, M_str_len(math_tests[i].r), &exp, NULL);

		rv = math_tests[i].op(&r, &d1, &d2);
		M_decimal_to_str(&r, r_out, sizeof(r_out));
		M_decimal_to_str(&exp, exp_out, sizeof(exp_out));
		//M_printf("test %zu exp %s == %s\n", i, r_out, exp_out);
		ck_assert_msg(rv == math_tests[i].rv, "test %zu returned %u (%s vs exp %s)", i, (unsigned int)rv, r_out, exp_out);
		if (rv == M_DECIMAL_SUCCESS || rv == M_DECIMAL_TRUNCATION) {
			ck_assert_msg(M_decimal_cmp(&r, &exp) == 0, "test %zu expected result: %s returned %s", i, exp_out, r_out);
		}
	}
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *decimal_suite(void)
{
	Suite *suite;
	TCase *tc_decimal_cmp;
	TCase *tc_decimal_math;

	suite = suite_create("decimal");

	tc_decimal_cmp = tcase_create("decimal_cmp");
	tcase_add_test(tc_decimal_cmp, check_decimal_cmp);
	suite_add_tcase(suite, tc_decimal_cmp);

	tc_decimal_math = tcase_create("decimal_math");
	tcase_add_test(tc_decimal_math, check_decimal_math);
	suite_add_tcase(suite, tc_decimal_math);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(decimal_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_decimal.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
