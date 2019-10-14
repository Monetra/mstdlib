#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#ifndef _WIN32
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include <mstdlib/mstdlib.h>

typedef unsigned long long llu; /* because I'm lazy ... */
typedef          long long lld;

/* We store format strings in variables to make testing easier, so disable the associated warning for this file. */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wformat-security"
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


static const struct {
	const char *fmt;      /* snprintf format string */
	const char *str;      /* passed in string */
	int         str_len;  /* -1 = not used, dynamic length '*' */
	ssize_t     buf_size; /* -1 = not used, otherwise buffer size */
	size_t      elen;     /* -1 = not used */
	const char *out;      /* Expected output */
} check_snprintf_string_data[] = {
	{ "%s",   "hello world",  -1, -1, 0, "hello world" },
	{ "%s",    NULL,          -1, -1, 0, "<NULL>"      },
	{ "%5s",   "1",           -1, -1, 0, "    1"       },
	{ "%-5s",  "1",           -1, -1, 0, "1    "       },
	{ "%5s",   "123456",      -1, -1, 0, "123456"      }, 
	{ "%.5s",  "123456",      -1, -1, 0, "12345"       },
	{ "%s",    "123456",      -1,  5, 6, "1234"        },
	{ "%*s",   "1",            5, -1, 0, "    1"       },
	{ "%.*s",  "123456",       5, -1, 0, "12345"       },
	{ "%5.*s", "123",          1, -1, 0, "    1"       },
	{ NULL, NULL, -1, -1, 0, NULL }
};

START_TEST(check_snprintf_string)
{
	char   buf[512];
	size_t buf_size;
	size_t elen;
	size_t ret;
	size_t i;

	for (i=0; check_snprintf_string_data[i].out != NULL; i++) {
		buf_size = sizeof(buf);
		if (check_snprintf_string_data[i].buf_size != -1)
			buf_size = (size_t)check_snprintf_string_data[i].buf_size;

		if (check_snprintf_string_data[i].str_len != -1) {
			ret = M_snprintf(buf, buf_size, check_snprintf_string_data[i].fmt, check_snprintf_string_data[i].str_len, check_snprintf_string_data[i].str);
		} else {
			ret = M_snprintf(buf, buf_size, check_snprintf_string_data[i].fmt, check_snprintf_string_data[i].str);
		}

		ck_assert_msg(M_str_eq(buf, check_snprintf_string_data[i].out), "%llu: Failed (%s), got '%s' expected '%s'", (llu)i, check_snprintf_string_data[i].fmt, buf, check_snprintf_string_data[i].out);

		if (check_snprintf_string_data[i].elen != 0) {
			elen = check_snprintf_string_data[i].elen;
		} else {
			elen = M_str_len(buf);
		}
		ck_assert_msg(ret == elen, "%llu: Output length failure, got '%llu' expected '%llu'", (llu)i, (llu)ret, (llu)elen);
	}
}
END_TEST

static const struct {
	const char *fmt;      /* snprintf format string */
	M_uint64    val;      /* passed in uint64 value */
	ssize_t     buf_size; /* -1 = not used, otherwise buffer size */
	size_t      elen;     /* -1 = not used */
	const char *out;      /* Expected output */
} check_snprintf_uint64_data[] = {
	{ "%llu",                     1ULL,  -1, 0, "1"   },
	{ "%02llu",                   1ULL,  -1, 0, "01"  },
	{ "%2llu",                    1ULL,  -1, 0, " 1"  },
	{ "%+llu",                    1ULL,  -1, 0, "1"   },
	{ "%-2llu",                    1ULL, -1, 0, "1 "  },
	{ "%02llu",                 123ULL,  -1, 0, "123" },
	{ "%-02llu",                 123ULL, -1, 0, "123" },
	{ "%llu",                 12345ULL,   3, 5, "12"  },
	{ "%05llu",                   1ULL,   3, 5, "00"  },
	{ "%llu",  18446744073709551615ULL,  -1, 0, "18446744073709551615" },
	{ "%I64ua",  18446744073709551615ULL,  -1, 0, "18446744073709551615a" },
	{ "%llX",  18446744073709551615ULL,  -1, 0, "FFFFFFFFFFFFFFFF" },
	{ "%#llX",  18446744073709551615ULL,  -1, 0, "0XFFFFFFFFFFFFFFFF" },
	{ "%llx",  18446744073709551615ULL,  -1, 0, "ffffffffffffffff" },
	{ "%#llx",  18446744073709551615ULL,  -1, 0, "0xffffffffffffffff" },
	{ "%######llx",  18446744073709551615ULL,  -1, 0, "0xffffffffffffffff" },
	{ NULL,                          0,  -1, 0, NULL  }
};

START_TEST(check_snprintf_uint64)
{
	char   buf[512];
	size_t buf_size;
	size_t elen;
	size_t ret;
	size_t i;

	for (i=0; check_snprintf_uint64_data[i].out != NULL; i++) {
		buf_size = sizeof(buf);
		if (check_snprintf_uint64_data[i].buf_size != -1)
			buf_size = (size_t)check_snprintf_uint64_data[i].buf_size;

		ret = M_snprintf(buf, buf_size, check_snprintf_uint64_data[i].fmt, check_snprintf_uint64_data[i].val);

		ck_assert_msg(M_str_eq(buf, check_snprintf_uint64_data[i].out), "%llu: Failed (%s), got '%s' expected '%s'", (llu)i, check_snprintf_uint64_data[i].fmt, buf, check_snprintf_uint64_data[i].out);

		if (check_snprintf_uint64_data[i].elen != 0) {
			elen = check_snprintf_uint64_data[i].elen;
		} else {
			elen = M_str_len(buf);
		}
		ck_assert_msg(ret == elen, "%llu: Output length failure, got '%llu' expected '%llu'", (llu)i, (llu)ret, (llu)elen);
	}
}
END_TEST

static const struct {
	const char *fmt;      /* snprintf format string */
	int         val;      /* passed in uint64 value */
	ssize_t     buf_size; /* -1 = not used, otherwise buffer size */
	size_t      elen;     /* -1 = not used */
	const char *out;      /* Expected output */
} check_snprintf_int_data[] = {
	{ "%d",           1,  -1, 0, "1"          },
	{ "%02d",         1,  -1, 0, "01"         },
	{ "%2d",          1,  -1, 0, " 1"         },
	{ "%+d",          1,  -1, 0, "+1"         },
	{ "%-2d",         1,  -1, 0, "1 "         },
	{ "%02d",       123,  -1, 0, "123"        },
	{ "%-02d",       12,  -1, 0, "12"         },
	{ "%d",       12345,   3, 5, "12"         },
	{ "%05d",         1,   3, 5, "00"         },
	{ "%d",   2147483647, -1, 0, "2147483647" },
	{ "% 06d", 12, -1, 0, " 00012" },
	{ "%- 06d", 12, -1, 0, " 12   " },
	{ "%0 6d", 12, -1, 0, " 00012" },
	{ "%0 +6d", 12, -1, 0, "+00012" },
	{ "%0+ 6d", 12, -1, 0, "+00012" },
	{ "%-0 6d", 12, -1, 0, " 12   " },
	{ "%-06d", 12, -1, 0, "12    " },
	{ "% 06d", -12, -1, 0, "-00012" },
	{ "%- 06d", -12, -1, 0, "-12   " },
	{ "%0 6d", -12, -1, 0, "-00012" },
	{ "%0 +6d", -12, -1, 0, "-00012" },
	{ "%0+ 6d", -12, -1, 0, "-00012" },
	{ "%-0 6d", -12, -1, 0, "-12   " },
	{ "%-06d", -12, -1, 0, "-12   " },
	{ "a%I32db", -12, -1, 0, "a-12b" },
	{ NULL, 0, -1,  0, NULL  }
};

START_TEST(check_snprintf_int)
{
	char   buf[512];
	size_t buf_size;
	size_t elen;
	size_t ret;
	size_t i;

	for (i=0; check_snprintf_int_data[i].out != NULL; i++) {
		buf_size = sizeof(buf);
		if (check_snprintf_int_data[i].buf_size != -1)
			buf_size = (size_t)check_snprintf_int_data[i].buf_size;

		ret = M_snprintf(buf, buf_size, check_snprintf_int_data[i].fmt, check_snprintf_int_data[i].val);

		ck_assert_msg(M_str_eq(buf, check_snprintf_int_data[i].out), "%llu: Failed (%s), got '%s' expected '%s'", (llu)i, check_snprintf_int_data[i].fmt, buf, check_snprintf_int_data[i].out);

		if (check_snprintf_int_data[i].elen != 0) {
			elen = check_snprintf_int_data[i].elen;
		} else {
			elen = M_str_len(buf);
		}
		ck_assert_msg(ret == elen, "%llu: Output length failure, got '%llu' expected '%llu'", (llu)i, (llu)ret, (llu)elen);
	}
}
END_TEST

static const struct {
	const char *fmt;      /* snprintf format string */
	double      val;      /* passed in uint64 value */
	ssize_t     buf_size; /* -1 = not used, otherwise buffer size */
	const char *out;      /* Expected output */
} check_snprintf_float_data[] = {
	{ "%f",    1.0,                     -1, "1.000000"            },
#ifndef _WIN32
	/* Doing math in an initializer most compilers don't support */
	{ "%f",    0.0/0.0,                 -1, "nan"                 },
	{ "%f",    1.0/0.0,                 -1, "inf"                 },
	{ "%f",    -1.0/0.0,                -1, "-inf"                },
#endif
	{ "%f",    1.234,                   -1, "1.234000"            },
	{ "%.4f",  1.234,                   -1, "1.2340"              },
	{ "%.3f",  1.234,                   -1, "1.234"               },
	{ "%.0f",  1.234,                   -1, "1"                   },
	{ "%.13f",  1.234,                   -1, "1.2340000000000" },
	{ "%.13f", 1.7976931348623157e+308, -1, "179769313486234550000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.0000000000000"  }, /* Largest representable number */
	{ "%.13f", 2.2250738585072014e-308, -1, "0.0000000000000"  }, /* Smallest number without losing precision */
	{ "%.13f", 5e-324,                  -1, "0.0000000000000"  }, /* Smallest representable number */
	{ "%.13f", 52.0,                    -1, "52.0000000000000" }, /* Mantissa bits */
	{ "%.13f", 11.0,                    -1, "11.0000000000000" }, /* Exponent bits */
	{ "%.13f", 2.220446049250313e-16,   -1, "0.0000000000000"  }, /* Epsilon */
	{ NULL, 0, -1, NULL  }
};

START_TEST(check_snprintf_float)
{
	char   buf[512];
	size_t buf_size;
	size_t elen;
	size_t ret;
	size_t i;

	for (i=0; check_snprintf_float_data[i].out != NULL; i++) {
		buf_size = sizeof(buf);
		if (check_snprintf_float_data[i].buf_size != -1)
			buf_size = (size_t)check_snprintf_float_data[i].buf_size;

		ret = M_snprintf(buf, buf_size, check_snprintf_float_data[i].fmt, check_snprintf_float_data[i].val);

		ck_assert_msg(M_str_eq(buf, check_snprintf_float_data[i].out), "%llu: Failed (%s), got '%s' expected '%s'", (llu)i, check_snprintf_float_data[i].fmt, buf, check_snprintf_float_data[i].out);

		elen = M_str_len(buf);
		ck_assert_msg(ret == elen, "%llu: Output length failure, got '%llu' expected '%llu'", (llu)i, (llu)ret, (llu)elen);
	}
}
END_TEST

static void run_snprtinf_other_generic(const char *in, const char *out)
{
	char buf[512];
	size_t elen;
	size_t ret;

	ret = M_snprintf(buf, sizeof(buf), in);
	ck_assert_msg(M_str_eq(buf, out), "Failed: (%s) got '%s' expected '%s'", in, buf, out);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
}

START_TEST(check_snprintf_other)
{
	run_snprtinf_other_generic("%%", "%");
	run_snprtinf_other_generic("%K", "%?");
	run_snprtinf_other_generic("%0", "%?");
	run_snprtinf_other_generic("'%0a'", "'%\?\?'");
	run_snprtinf_other_generic("'%-#0a'", "'%\?\?\?\?'");
	run_snprtinf_other_generic("%#", "%?");
	run_snprtinf_other_generic("%#0", "%\?\?");
	run_snprtinf_other_generic("%", "%");
	run_snprtinf_other_generic("'%a'", "'%?'");
	run_snprtinf_other_generic("abc", "abc");
	run_snprtinf_other_generic(NULL, NULL);
}
END_TEST

START_TEST(check_snprintf_null_buf)
{
	size_t elen;
	size_t ret;

	ret  = M_snprintf(NULL, 0, NULL);
	elen = 0;
	ck_assert_msg(ret == elen, "NULL len 0: Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	ret  = M_snprintf(NULL, 4, NULL);
	elen = 0;
	ck_assert_msg(ret == elen, "NULL len 4: Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	ret  = M_snprintf(NULL, 0, "ABC");
	elen = 3;
	ck_assert_msg(ret == elen, "Static text: Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	ret  = M_snprintf(NULL, 0, "a %s, %02d", "xyz", 2);
	elen = 9;
	ck_assert_msg(ret == elen, "Static text: Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
}
END_TEST

START_TEST(check_snprintf_multi)
{
	const char *fmt;
	const char *expt;
	char        buf[512];
	size_t      elen;
	size_t      ret;

	
	fmt  = "%#09x %#09o abc %-4.1d, +%.13f -- %% %+020.13f";
	expt = "0x0000149 000000052 abc 97  , +123456.7890000000043 -- % +00032.2345578801230";
	ret = M_snprintf(buf, sizeof(buf), fmt, 329, 42, 97, 123456.789, 32.234567890123); 
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	fmt  = "% 09s, %0 9s, %-#x";
	expt = "        3,         2, 0x3806";
	ret = M_snprintf(buf, sizeof(buf), fmt, "3", "2", 14342);
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
	
	fmt  = "%*.s";
	expt = "    ";
	ret = M_snprintf(buf, sizeof(buf), fmt, 4, "abc"); 
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
	
	fmt  = "%*.s";
	expt = "";
	ret = M_snprintf(buf, sizeof(buf), fmt, 0, "abc"); 
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	fmt  = "%*s";
	expt = " abc";
	ret = M_snprintf(buf, sizeof(buf), fmt, 4, "abc"); 
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);

	fmt  = "%*.*s";
	expt = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               ";
	ret = M_snprintf(buf, sizeof(buf), fmt, 8000, 3, "abc");
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	elen = M_str_len(buf);
	ck_assert_msg(elen == sizeof(buf)-1, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
	ck_assert_msg(ret == 8000, "Return length failure, got '%llu' expected '%llu'", (llu)ret, 8000);
}
END_TEST

static const struct {
	const char *prefix;
	const char *str;
	const char *suffix;
	const char *out;
} check_snprintf_multi_string_data[] = {
	{ "",    "mstdlib_sql_sqlite", "",     "mstdlib_sql_sqlite" },
	{ "",    "mstdlib_sql_sqlite", ".dll", "mstdlib_sql_sqlite.dll" },
	{ "lib", "mstdlib_sql_sqlite", "",     "libmstdlib_sql_sqlite" },
	{ "lib", "mstdlib_sql_sqlite", ".dll", "libmstdlib_sql_sqlite.dll" },
	{ NULL, NULL, NULL, NULL }
};

START_TEST(check_snprintf_multi_string)
{
	char   buf[512];
	size_t elen;
	size_t ret;
	size_t i;

	for (i=0; check_snprintf_multi_string_data[i].out != NULL; i++) {
		ret = M_snprintf(buf, sizeof(buf), "%s%s%s", check_snprintf_multi_string_data[i].prefix, check_snprintf_multi_string_data[i].str, check_snprintf_multi_string_data[i].suffix); 
		ck_assert_msg(M_str_eq(buf, check_snprintf_multi_string_data[i].out), "%llu: Failed got '%s' expected '%s'", (llu)i, buf, check_snprintf_multi_string_data[i].out);
		elen = M_str_len(buf);
		ck_assert_msg(ret == elen, "%llu: Output length failure, got '%llu' expected '%llu'", (llu)i, (llu)ret, (llu)elen);
	}
}
END_TEST

START_TEST(check_snprintf_alloc)
{
	const char *fmt;
	const char *expt;
	char       *buf;
	size_t      elen;
	size_t      ret;

	fmt  = "%#09x %#09o abc %-4.1d, +%.13f -- %% %+020.13f abcdefgh%%ijklmnopqrstuvwxyz %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s";
	expt = "0x0000149 000000052 abc 97  , +123456.7890000000043 -- % +00032.2345578801230 abcdefgh%ijklmnopqrstuvwxyz rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire                                 874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy6548ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf7yhjklo06trf5t865fdf54rty6y+rt		28uo09ujklaiujdadad32	2	2	2	2	2	2	2	2	2	2	2	2	2	2rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy6548ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf7yhjklo06trf5t865fdf54rty6y+rt		28uo09ujklaiujdadad32	2	2	2	2	2	2	2	2	2	2	2	2	2	2rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy6548ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf7yhjklo06trf5t865fdf54rty6y+rt		28uo09ujklaiujdadad32	2	2	2	2	2	2	2	2	2	2	2	2	2	2rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy6548ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf7yhjklo06trf5t865fdf54rty6y+rt		28uo09ujklaiujdadad32	2	2	2	2	2	2	2	2	2	2	2	2	2	28ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf7yhjklo06trf5t865fdf54rty6y+rt		28uo09ujklaiujdadad32	2	2	2	2	2	2	2	2	2	2	2	2	2	2";
	elen = 1879;
	ret = M_asprintf(&buf, fmt, 329, 42, 97, 123456.789, 32.234567890123,
			"rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire",
			"                                 ",
			"874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy654",
			"8ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf",
			"7yhjklo06trf5t865fdf54rty6y+rt\t\t28uo09ujklaiujdadad32\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2",
			"rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire",
			"874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy654",
			"8ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf",
			"7yhjklo06trf5t865fdf54rty6y+rt\t\t28uo09ujklaiujdadad32\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2",
			"rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire",
			"874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy654",
			"8ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf",
			"7yhjklo06trf5t865fdf54rty6y+rt\t\t28uo09ujklaiujdadad32\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2",
			"rtyuiop-o0ytrftgyhjuiophgfghjkl]-09876rfbnm,.547125871afe431qf87458745125yryuehfjkvlgphoy0985yrehdnjmklpg[-y09685ire",
			"874red5f8t741re2fg8u5y21twrfdgy76tirwkdlfghjui887454ytwrf4748154rtgy8u875654rqe2drftwy654",
			"8ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf",
			"7yhjklo06trf5t865fdf54rty6y+rt\t\t28uo09ujklaiujdadad32\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2",
			"8ujklo987yhjklo9iku5hgf8e41562u58yhgfrdewq234567jhgfdsdfghjkl;984tuejfkr[56uthwnfri52uthnfdr[i145-urhqf",
			"7yhjklo06trf5t865fdf54rty6y+rt\t\t28uo09ujklaiujdadad32\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2\t2"
			); 
	ck_assert_msg(M_str_eq(buf, expt), "Failed (%s), got '%s' expected '%s'", fmt, buf, expt);
	ck_assert_msg(ret == elen, "Output length failure, got '%llu' expected '%llu'", (llu)ret, (llu)elen);
	M_free(buf);
}
END_TEST

START_TEST(check_snprintf_fp)
{
	M_fs_file_t   *mfd;
	const char    *mfd_filename = "check_snprintf_mfp_out.txt";
	const char    *fmt          = "%+d %s %*.*s";
	int            fds_size     = 3+1+3+1+4+190000;
	char          *output       = NULL;
	M_buf_t       *builder;
	unsigned char *buf;
	ssize_t        r;
	M_fs_error_t   fserr;
#ifndef _WIN32
	int            fd;
	const char    *fd_filename  = "check_snprintf_fp_out.txt";	
	off_t          off;
	ssize_t        cnt;
	char          *out;
	char           temp[256];
#endif

	/* Build output string. Windows MSVC doesn't like string literals as
 	 * long as the output. If we use an array of characters it takes
	 * forever to compile. After 10 minutes we gave up waiting. Instead
	 * we'll build the expected output using an M_buf. Note: the M_buf
	 * functions used here were verified not to use any M_*printf functions
	 * internally. */
	builder = M_buf_create();
	M_buf_add_str(builder, "+19 abs   54");
	M_buf_add_fill(builder, ' ', 189999);
	M_buf_add_byte(builder, 'e');
	output = M_buf_finish_str(builder, NULL);

#ifndef _WIN32
	/* OS FD */
	fd = open(fd_filename, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	ck_assert_msg(fd != -1, "Could not open fd: %s", fd_filename);

	r = M_dprintf(fd, fmt, 19, "abs", 4, 2, "5478");
	ck_assert_msg(r != -1, "fd failed to write part 1");
	r = M_dprintf(fd, "%*s", 190000, "e");
	ck_assert_msg(r != -1, "fd failed to write part 2");
	lseek(fd, 0, SEEK_SET);
	off = lseek(fd, 0, SEEK_END);
	ck_assert_msg(off == fds_size, "fd file size does not match expected, got '%lld' expected '%lld'", (lld)off, (lld)fds_size);

	lseek(fd, 0, SEEK_SET);
	out = M_malloc_zero((size_t)fds_size+1);
	cnt = 0;
	do {
		r = read(fd, temp, sizeof(temp));
		if (r > 0) {
			M_mem_copy(out+cnt, temp, (size_t)r);
			cnt += r;
		}
	} while (r > 0);
	ck_assert_msg(M_str_eq(out, output), "fd file data does not match expected");

	M_free(out);
	unlink(fd_filename);
#endif


	/* mstdlib FD */
	fserr = M_fs_file_open(&mfd, mfd_filename, 0, M_FS_FILE_MODE_WRITE|M_FS_FILE_MODE_OVERWRITE, NULL);
	ck_assert_msg(fserr == M_FS_ERROR_SUCCESS, "Could not open mfd: %s", mfd_filename);

	r = M_mdprintf(mfd, fmt, 19, "abs", 4, 2, "5478");
	ck_assert_msg(r != -1, "mfd failed to write part 1");
	r = M_mdprintf(mfd, "%*s", 190000, "e");
	ck_assert_msg(r != -1, "mfd failed to write part 2");
	M_fs_file_close(mfd);

	fserr = M_fs_file_read_bytes(mfd_filename, 0, &buf, (size_t *)&r);
	ck_assert_msg(fserr == M_FS_ERROR_SUCCESS, "mfd could not read file");
	ck_assert_msg((int)r == fds_size, "mfd file size does not match expected, got '%lld' expected '%d'", (lld)r, fds_size);
	ck_assert_msg(M_mem_eq(buf, output, (size_t)r), "mfd file data does not match expected");

	M_free(buf);
	M_free(output);
	M_fs_delete(mfd_filename, M_TRUE, NULL, M_FS_PROGRESS_NOEXTRA);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_snprintf_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("snprintf");

	tc = tcase_create("check_snprintf_string");
	tcase_add_test(tc, check_snprintf_string);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_uint64");
	tcase_add_test(tc, check_snprintf_uint64);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_int");
	tcase_add_test(tc, check_snprintf_int);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_float");
	tcase_add_test(tc, check_snprintf_float);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_other");
	tcase_add_test(tc, check_snprintf_other);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_null_buf");
	tcase_add_test(tc, check_snprintf_null_buf);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_multi");
	tcase_add_test(tc, check_snprintf_multi);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_multi_string");
	tcase_add_test(tc, check_snprintf_multi_string);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_alloc");
	tcase_add_test(tc, check_snprintf_alloc);
	suite_add_tcase(suite, tc);

	tc = tcase_create("check_snprintf_fp");
	tcase_add_test(tc, check_snprintf_fp);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_snprintf_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_snprintf.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

