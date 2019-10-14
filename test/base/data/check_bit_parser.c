#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */

/* Windows fix. VS2010 and newer have the stdint.h header, though libcheck doesn't realize this. */
#if !defined(HAVE_STDINT_H) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#	define HAVE_STDINT_H
#elif defined(_MSC_VER)
    typedef unsigned __int64 uintmax_t;
#endif
#include <check.h>

#include <mstdlib/mstdlib.h>


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Helper macros for tests. */

#define init_test(BITSTR)\
	const char     *bitstr  = BITSTR;\
	M_bit_parser_t *bparser;\
	M_bit_buf_t    *builder = M_bit_buf_create();\
	M_bit_buf_t    *bbuf    = M_bit_buf_create();\
	M_buf_t        *buf     = M_buf_create();\
	M_uint8         bit     = 0;\
	size_t          nbits   = 0;\
	char           *str     = NULL;\
	M_uint64        num     = 0;\
	M_int64         snum    = 0;\
	(void)buf;\
	(void)bit;\
	(void)nbits;\
	(void)str;\
	(void)num;\
	(void)snum;\
	M_bit_buf_add_bitstr(builder, bitstr, M_BIT_BUF_PAD_NONE);\
	bparser = M_bit_parser_create(M_bit_buf_peek(builder), M_bit_buf_len(builder));\
	M_bit_parser_mark(bparser)


#define reset_test(BITSTR)\
	bitstr  = BITSTR;\
	M_bit_parser_destroy(bparser);\
	M_bit_buf_truncate(builder, 0);\
	M_bit_buf_truncate(bbuf, 0);\
	M_bit_buf_add_bitstr(builder, bitstr, M_BIT_BUF_PAD_NONE);\
	M_free(str);\
	M_buf_truncate(buf, 0);\
	str     = NULL;\
	bit     = 0;\
	nbits   = 0;\
	num     = 0;\
	snum    = 0;\
	bparser = M_bit_parser_create(M_bit_buf_peek(builder), M_bit_buf_len(builder))


#define cleanup_test\
	M_bit_parser_destroy(bparser);\
	M_bit_buf_destroy(builder);\
	M_bit_buf_destroy(bbuf);\
	M_buf_cancel(buf);\
	M_free(str);\
	builder = NULL;\
	bparser = NULL


#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


/* BPARSER:  bit parser
 * EXP_BITS: expected length of buffer, in bits
 */
#define check_len(BPARSER, EXP_BITS)\
do {\
	size_t len_bits  = M_bit_parser_len(BPARSER);\
	ck_assert_msg(len_bits == EXP_BITS, "len is %lld bits, expected %lld", (long long)len_bits, EXP_BITS);\
} while (0)


#define check_bitstr_eq(TESTSTR, EXPECTEDSTR)\
do {\
	const char *test     = TESTSTR;\
	const char *expected = EXPECTEDSTR;\
	while (*test && *expected) {\
		if (M_chr_isspace(*test)) {\
			test++;\
		} else if(M_chr_isspace(*expected)) {\
			expected++;\
		} else if (*test == *expected) {\
			test++;\
			expected++;\
		} else {\
			break;\
		}\
	}\
	ck_assert_msg(*test == *expected, "%s does not match expected bitstr %s", TESTSTR, EXPECTEDSTR);\
} while (0)


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Individual tests. */

START_TEST(check_bparser_read_peek_bit)
{
	init_test("");
	check_len(bparser, 0);
	ck_assert(!M_bit_parser_peek_bit(bparser, &bit));
	ck_assert(!M_bit_parser_read_bit(bparser, &bit));
	ck_assert(!M_bit_parser_peek_bit(bparser, &bit));

	reset_test("10  110");
	check_len(bparser, 5);
	ck_assert(M_bit_parser_read_bit(bparser, &bit) && bit == 1); check_len(bparser, 4);

	ck_assert(M_bit_parser_peek_bit(bparser, &bit) && bit == 0); check_len(bparser, 4);
	ck_assert(M_bit_parser_read_bit(bparser, &bit) && bit == 0); check_len(bparser, 3);

	ck_assert(M_bit_parser_read_bit(bparser, &bit) && bit == 1); check_len(bparser, 2);

	ck_assert(M_bit_parser_peek_bit(bparser, &bit) && bit == 1); check_len(bparser, 2);
	ck_assert(M_bit_parser_read_bit(bparser, &bit) && bit == 1); check_len(bparser, 1);

	ck_assert(M_bit_parser_read_bit(bparser, &bit) && bit == 0); check_len(bparser, 0);

	ck_assert(!M_bit_parser_read_bit(bparser, &bit));
	ck_assert(!M_bit_parser_peek_bit(bparser, &bit));

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_bytes)
{
	M_uint8 bin[5] = {0};
	size_t  len;

	init_test("1011 0100 01"); /* Expected results: 0xB4 40 */
	check_len(bparser, 10);

	len = sizeof(bin);
	ck_assert(!M_bit_parser_read_bytes(bparser, bin, &len, 11)); /* verify that requests for too many bits are rejected. */
	ck_assert(len == 0);
	len = sizeof(bin);
	ck_assert(M_bit_parser_read_bytes(bparser, bin, &len, 10));
	ck_assert(len == 2);
	check_len(bparser, 0);
	len = sizeof(bin);
	ck_assert(!M_bit_parser_read_bytes(bparser, bin, &len, 2)); /* verify that requests on an empty parser are rejected. */
	ck_assert(len == 0);
	ck_assert(bin[0] == 0xB4);
	ck_assert(bin[1] == 0x40);

	reset_test("1011 0100 0101 1101"); /* Expected results: 0xB4 5D */
	check_len(bparser, 16);
	len = sizeof(bin);
	ck_assert(M_bit_parser_read_bytes(bparser, bin, &len, 16));
	ck_assert(len == 2);
	check_len(bparser, 0);
	ck_assert(bin[0] == 0xB4);
	ck_assert(bin[1] == 0x5D);

	reset_test("1 1011 0100 0101 1101 1"); /* Expected results (after consuming first bit): 0xB4 5D 80 */
	check_len(bparser, 18);
	M_bit_parser_consume(bparser, 1);
	check_len(bparser, 17);
	len = sizeof(bin);
	ck_assert(M_bit_parser_read_bytes(bparser, bin, &len, 17));
	ck_assert(len == 3);
	check_len(bparser, 0);
	ck_assert(bin[0] == 0xB4);
	ck_assert(bin[1] == 0x5D);
	ck_assert(bin[2] == 0x80);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_bit_buf)
{
	init_test("1011 0100 011");
	check_len(bparser, 11);

	ck_assert(M_bit_parser_read_bit_buf(bparser, bbuf, 1));
	check_len(bparser, 10);
	ck_assert(M_bit_buf_len(bbuf) == 1);
	ck_assert((M_bit_buf_peek(bbuf)[0] & 0x80) == 0x80);
	M_bit_buf_truncate(bbuf, 0);

	/* expected results: 011 0100 011
	 *                   0110 1000 11
	 *                     6    8    C
	 */
	ck_assert(M_bit_parser_read_bit_buf(bparser, bbuf, M_bit_parser_len(bparser)));
	check_len(bparser, 0);
	ck_assert(M_bit_buf_len(bbuf) == 10);
	ck_assert(M_bit_buf_peek(bbuf)[0] == 0x68);
	ck_assert((M_bit_buf_peek(bbuf)[1] & 0xC0) == 0xC0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_buf)
{
	init_test("1011 0100 01"); /* Expected results: 0xB4 40 */
	check_len(bparser, 10);

	ck_assert(!M_bit_parser_read_buf(bparser, buf, 11)); /* verify that requests for too many bits are rejected. */
	ck_assert(M_buf_len(buf) == 0);
	ck_assert(M_bit_parser_read_buf(bparser, buf, 10));
	check_len(bparser, 0);
	ck_assert(!M_bit_parser_read_buf(bparser, buf, 2)); /* verify that requests on an empty parser are rejected. */
	ck_assert(M_buf_len(buf) == 2);
	ck_assert((M_uint8)(M_buf_peek(buf)[0]) == 0xB4);
	ck_assert((M_uint8)(M_buf_peek(buf)[1]) == 0x40);

	reset_test("1011 0100 0101 1101"); /* Expected results: 0xB4 5D */
	check_len(bparser, 16);
	M_buf_add_byte(buf, 0xDE); /* Add an extra byte first, to make sure that read_buf() isn't wiping previous contents.*/
	ck_assert(M_bit_parser_read_buf(bparser, buf, 16));
	check_len(bparser, 0);
	ck_assert(M_buf_len(buf) == 3);
	ck_assert((M_uint8)(M_buf_peek(buf)[0]) == 0xDE); /* extra byte we added before calling M_bit_parser_read_buf(). */
	ck_assert((M_uint8)(M_buf_peek(buf)[1]) == 0xB4);
	ck_assert((M_uint8)(M_buf_peek(buf)[2]) == 0x5D);

	reset_test("1 1011 0100 0101 1101 1"); /* Expected results (after consuming first bit): 0xB4 5D 80 */
	check_len(bparser, 18);
	M_bit_parser_consume(bparser, 1);
	check_len(bparser, 17);
	ck_assert(M_bit_parser_read_buf(bparser, buf, 17));
	check_len(bparser, 0);
	ck_assert(M_buf_len(buf) == 3);
	ck_assert((M_uint8)(M_buf_peek(buf)[0]) == 0xB4);
	ck_assert((M_uint8)(M_buf_peek(buf)[1]) == 0x5D);
	ck_assert((M_uint8)(M_buf_peek(buf)[2]) == 0x80);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_strdup)
{
	init_test("1001 0011 01");
	check_len(bparser, 10);

	str = M_bit_parser_read_strdup(bparser, 5);
	check_bitstr_eq(str, "1001 0");

	M_free(str);
	str = M_bit_parser_read_strdup(bparser, 4);
	check_bitstr_eq(str, "011 0");

	M_free(str);
	str = M_bit_parser_read_strdup(bparser, 2);
	ck_assert(str == NULL);

	M_free(str);
	str = M_bit_parser_read_strdup(bparser, 1);
	check_bitstr_eq(str, "1");

	check_len(bparser, 0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_range)
{
	init_test("1 0 111 0000 00000");
	check_len(bparser, 14);

	ck_assert(!M_bit_parser_read_range(bparser, &bit, &nbits, 0));

	ck_assert(M_bit_parser_read_range(bparser, &bit, &nbits, M_bit_parser_len(bparser)));
	check_len(bparser, 13);
	ck_assert(bit == 1);
	ck_assert(nbits == 1);

	ck_assert(M_bit_parser_read_range(bparser, &bit, &nbits, M_bit_parser_len(bparser)));
	check_len(bparser, 12);
	ck_assert(bit == 0);
	ck_assert(nbits == 1);

	ck_assert(M_bit_parser_read_range(bparser, &bit, &nbits, M_bit_parser_len(bparser)));
	check_len(bparser, 9);
	ck_assert(bit == 1);
	ck_assert(nbits == 3);

	ck_assert(M_bit_parser_read_range(bparser, &bit, &nbits, 4));
	check_len(bparser, 5);
	ck_assert(bit == 0);
	ck_assert(nbits == 4);

	ck_assert(M_bit_parser_read_range(bparser, &bit, &nbits, M_bit_parser_len(bparser) + 1));
	check_len(bparser, 0);
	ck_assert(bit == 0);
	ck_assert(nbits == 5);

	ck_assert(!M_bit_parser_read_range(bparser, &bit, &nbits, 1));
	check_len(bparser, 0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_consume)
{
	init_test("1 1001 0011 0111");
	check_len(bparser, 13);

	ck_assert(M_bit_parser_consume(bparser, 1));
	check_len(bparser, 12);

	ck_assert(M_bit_parser_consume(bparser, 4));
	check_len(bparser, 8);

	ck_assert(M_bit_parser_consume(bparser, 2));
	check_len(bparser, 6);

	str = M_bit_parser_read_strdup(bparser, 6);
	check_bitstr_eq(str, "11 0111");
	check_len(bparser, 0);

	ck_assert(!M_bit_parser_consume(bparser, 2));
	check_len(bparser, 0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_consume_range)
{
	init_test("1 0 111 0000 00000");
	check_len(bparser, 14);

	ck_assert(!M_bit_parser_consume_range(bparser, 0));

	ck_assert(M_bit_parser_consume_range(bparser, M_bit_parser_len(bparser))); /* consume "1" */
	check_len(bparser, 13);

	ck_assert(M_bit_parser_consume_range(bparser, M_bit_parser_len(bparser))); /* consume "0" */
	check_len(bparser, 12);

	ck_assert(M_bit_parser_consume_range(bparser, M_bit_parser_len(bparser))); /* consume "111" */
	check_len(bparser, 9);

	ck_assert(M_bit_parser_consume_range(bparser, 4)); /* consume "0000" */
	check_len(bparser, 5);

	ck_assert(M_bit_parser_consume_range(bparser, M_bit_parser_len(bparser) + 1)); /* consume "00000" */
	check_len(bparser, 0);

	ck_assert(!M_bit_parser_consume_range(bparser, 1));
	check_len(bparser, 0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_consume_to_next)
{
	init_test("1000011000");
	check_len(bparser, 10);

	ck_assert(!M_bit_parser_consume_to_next(bparser, 1, 0));

	ck_assert(M_bit_parser_consume_to_next(bparser, 1, M_bit_parser_len(bparser))); /* consume "1" */
	check_len(bparser, 9);

	ck_assert(!M_bit_parser_consume_to_next(bparser, 1, 3)); /* consume "000" */
	check_len(bparser, 6);

	ck_assert(M_bit_parser_consume_to_next(bparser, 1, M_bit_parser_len(bparser))); /* consume "01" */
	check_len(bparser, 4);

	ck_assert(M_bit_parser_consume_to_next(bparser, 1, 1)); /* consume "1" */
	check_len(bparser, 3);

	ck_assert(!M_bit_parser_consume_to_next(bparser, 1, M_bit_parser_len(bparser) + 10)); /* consume "000" */
	check_len(bparser, 0);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_rewind_mark)
{
	init_test("1011 0010 0001 0101");
	check_len(bparser, 16);

	/* Check rewind to start. */
	M_bit_parser_rewind_to_start(bparser);
	check_len(bparser, 16);
	M_bit_parser_consume(bparser, 9);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "001 0101");

	M_bit_parser_rewind_to_start(bparser);
	check_len(bparser, 16);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "1011 0010 0001 0101");

	M_bit_parser_rewind_to_start(bparser);
	check_len(bparser, 16);
	M_bit_parser_consume(bparser, 9);
	M_bit_parser_rewind_to_start(bparser);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "1011 0010 0001 0101");

	/* Check mark_rewind and mark_len. */
	reset_test("1011 0010 0001 0101");
	M_bit_parser_consume(bparser, 7);
	M_bit_parser_mark(bparser);
	ck_assert(M_bit_parser_mark_len(bparser) == 0);
	M_bit_parser_consume(bparser, 4);
	check_len(bparser, 5);
	ck_assert(M_bit_parser_mark_len(bparser) == 4);

	M_bit_parser_mark_rewind(bparser);
	check_len(bparser, 9);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "0 0001 0101");

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_append)
{
	/* Test 1: existing data ends on byte boundary. */
	init_test("0101 1011");
	bit = 0x9D; /* 1001 1101 */
	check_len(bparser, 8);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "0101 1011");
	M_bit_parser_append(bparser, &bit, 3); /* append 100 */
	M_bit_parser_rewind_to_start(bparser);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "0101 1011 100");

	/* Test 2: existing data does not end on byte boundary. */
	reset_test("1001 11");
	bit = 0x9D; /* 1001 1101 */
	M_bit_parser_append(bparser, &bit, 5); /* append 10011 */
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "1001 1110 011");

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_reset)
{
	init_test("0101 1011 0011 101");
	check_len(bparser, 15);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "0101 1011 0011 101");
	M_bit_parser_rewind_to_start(bparser);
	M_bit_parser_consume(bparser, 5);
	M_bit_parser_mark(bparser);

	/* Reset the parser to a new bit string. */
	M_bit_buf_add_bitstr(bbuf, "1101 0010 0", M_BIT_BUF_PAD_NONE);
	M_bit_parser_reset(bparser, M_bit_buf_peek(bbuf), M_bit_buf_len(bbuf));
	M_bit_buf_truncate(bbuf, 0);

	/* Validate that everything got reset correctly. */
	check_len(bparser, 9);
	ck_assert(M_bit_parser_current_offset(bparser) == 0);
	ck_assert(M_bit_parser_mark_len(bparser) == 0);
	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_current_offset(bparser) == 0);
	ck_assert(M_bit_parser_mark_len(bparser) == 0);
	M_free(str); str = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_bitstr_eq(str, "1101 0010 0");

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_uint)
{
	init_test("01011");
	check_len(bparser, 5);

	ck_assert(M_bit_parser_read_uint(bparser, 3, &num));
	ck_assert(num == 2);

	num = 0;
	ck_assert(M_bit_parser_read_uint(bparser, 2, &num));
	ck_assert(num == 3);

	check_len(bparser, 0);

	/* Maximum representable value. */
	reset_test("11111111" "11111111" "11111111" "11111111" "11111111" "11111111" "11111111" "11111111");
	check_len(bparser, 64);
	ck_assert(M_bit_parser_read_uint(bparser, 64, &num));
	ck_assert(num == M_UINT64_MAX);

	cleanup_test;
}
END_TEST


START_TEST(check_bparser_read_int)
{
	init_test("01011");
	check_len(bparser, 5);

	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 11);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 11);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == 11);


	reset_test("11011");
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == -11);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == -4);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == -5);


	reset_test("11111");
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == -15);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == -1);


	reset_test("00000");
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 5, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == 0);


	/* Maximum width. */
	reset_test("11111111" "11111111" "11111111" "11111111" "11111111" "11111111" "11111111" "11111111");
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == -(M_int64)(M_UINT64_MAX >> 1));

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == -1);

	reset_test("10000000" "00000000" "00000000" "00000000" "00000000" "00000000" "00000000" "00000000");
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == -(M_int64)(M_UINT64_MAX >> 1));

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 64, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == M_INT64_MIN);


	/* Minimum width. */
	reset_test("00");
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == 0);

	reset_test("01");
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 1);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 1);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == 1);

	reset_test("10");
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == -1);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == -2);

	reset_test("11");
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_SIGN_MAG, &snum));
	ck_assert(snum == -1);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_ONES_COMP, &snum));
	ck_assert(snum == 0);

	M_bit_parser_mark_rewind(bparser);
	ck_assert(M_bit_parser_read_int(bparser, 2, M_BIT_PARSER_TWOS_COMP, &snum));
	ck_assert(snum == -1);


	cleanup_test;
}
END_TEST


START_TEST(check_bparser_create_const)
{
	M_bit_parser_t *bparser  = NULL;
	char           *str      = NULL;

	/* Test bitstr: "0101 10"
	 *                 5    8
	 */
	M_uint8         data[]   = {0x58};
	size_t          data_len = 6;

	bparser = M_bit_parser_create_const(data, data_len);
	check_len(bparser, 6);
	str     = M_bit_parser_read_strdup(bparser, M_bit_parser_len(bparser));
	check_len(bparser, 0);
	check_bitstr_eq(str, "010110");

	M_free(str);
	M_bit_parser_destroy(bparser);
}
END_TEST


START_TEST(check_bparser_count)
{
	init_test("1 1001 0011 0111");
	check_len(bparser, 13);

	ck_assert(M_bit_parser_count(bparser, 0) == 5);
	ck_assert(M_bit_parser_count(bparser, 1) == 8);

	ck_assert(M_bit_parser_consume(bparser, 3));
	ck_assert(M_bit_parser_count(bparser, 0) == 4);
	ck_assert(M_bit_parser_count(bparser, 1) == 6);

	ck_assert(M_bit_parser_consume(bparser, M_bit_parser_len(bparser)));
	ck_assert(M_bit_parser_count(bparser, 0) == 0);
	ck_assert(M_bit_parser_count(bparser, 1) == 0);

	M_bit_parser_rewind_to_start(bparser);
	ck_assert(M_bit_parser_consume(bparser, 12)); /* parser now contains "1" */
	ck_assert(M_bit_parser_count(bparser, 0) == 0);
	ck_assert(M_bit_parser_count(bparser, 1) == 1);

	M_bit_parser_reset(bparser, NULL, 0);
	ck_assert(M_bit_parser_count(bparser, 0) == 0);
	ck_assert(M_bit_parser_count(bparser, 1) == 0);

	M_bit_parser_append_uint(bparser, 0, 1); /* parser now contains "0" */
	check_len(bparser, 1);
	ck_assert(M_bit_parser_count(bparser, 0) == 1);
	ck_assert(M_bit_parser_count(bparser, 1) == 0);

	cleanup_test;
}
END_TEST



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Code to run the individual tests as a test suite. */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int nf;

	suite = suite_create("check_bit_parser (M_bit_parser_t)");

	add_test(suite, check_bparser_read_peek_bit);
	add_test(suite, check_bparser_read_bytes);
	add_test(suite, check_bparser_read_bit_buf);
	add_test(suite, check_bparser_read_buf);
	add_test(suite, check_bparser_read_strdup);
	add_test(suite, check_bparser_read_uint);
	add_test(suite, check_bparser_read_int);
	add_test(suite, check_bparser_read_range);
	add_test(suite, check_bparser_consume);
	add_test(suite, check_bparser_consume_range);
	add_test(suite, check_bparser_consume_to_next);
	add_test(suite, check_bparser_rewind_mark);
	add_test(suite, check_bparser_append);
	add_test(suite, check_bparser_reset);
	add_test(suite, check_bparser_create_const);
	add_test(suite, check_bparser_count);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_bit_parser.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
