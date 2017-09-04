#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */

/* Windows fix. VS2010 and newer have the stdint.h header, though libcheck doesn't realize this. */
#if !defined(HAVE_STDINT_H) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#	define HAVE_STDINT_H
#elif defined(_MSC_VER)
#   typedef unsigned __int64 uintmax_t;
#endif
#include <check.h>

#include <mstdlib/mstdlib.h>



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Helper macros for tests. */

#define init_test\
	M_bit_buf_t *bbuf   = M_bit_buf_create();\
	M_uint8     *bytes  = NULL;\
	size_t       nbytes = 0


#define reset_test\
	M_bit_buf_destroy(bbuf);\
	M_free(bytes);\
	bbuf   = M_bit_buf_create();\
	bytes  = NULL;\
	nbytes = 0


#define to_bytes\
	bytes = M_bit_buf_finish(bbuf, &nbytes);\
	bbuf  = NULL


#define cleanup_test\
	M_bit_buf_destroy(bbuf);\
	M_free(bytes);\
	nbytes = 0


#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


/* BBUF:      bit buffer
 * EXP_BITS:  expected length of buffer, in bits
 * EXP_BYTES: expected length of buffer, in bytes
 */
#define check_lens(BBUF, EXP_BITS, EXP_BYTES)\
do {\
	size_t len_bits  = M_bit_buf_len(BBUF);\
	size_t len_bytes = M_bit_buf_len_bytes(BBUF);\
	ck_assert_msg(len_bits == EXP_BITS, "len is %lld bits, expected %lld", (long long)len_bits, EXP_BITS);\
	ck_assert_msg(len_bytes == EXP_BYTES, "len is %lld bytes, expected %lld", (long long)len_bytes, EXP_BYTES);\
} while (0)



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Individual tests. */

START_TEST(check_bbuf_add_bit)
{
	size_t i;
	init_test;

	check_lens(NULL, 0, 0);
	check_lens(bbuf, 0, 0);
	M_bit_buf_add_bit(bbuf, 1); check_lens(bbuf, 1, 1);
	M_bit_buf_add_bit(bbuf, 0); check_lens(bbuf, 2, 1);
	M_bit_buf_add_bit(bbuf, 1); check_lens(bbuf, 3, 1);
	to_bytes;
	ck_assert_ptr_ne(bytes, NULL);
	ck_assert_uint_eq(nbytes, 1);
	ck_assert_uint_eq(bytes[0] & 0xE0, 0xA0);

	reset_test;
	/* Add following binary: 0110 1101 10
	 *                  hex:   6    D    8
     */
	for (i=0; i<10; i++) {
		check_lens(bbuf, i, (i + 7) / 8);
		M_bit_buf_add_bit(bbuf, (i % 3 == 0)? 0 : 1);
	}
	check_lens(bbuf, 10, 2);
	to_bytes;
	ck_assert_uint_eq(nbytes, 2);
	ck_assert_uint_eq(bytes[0], 0x6D);
	ck_assert_uint_eq(bytes[1] & 0xC0, 0x80);

	cleanup_test;
}
END_TEST


START_TEST(check_bbuf_fill)
{
	init_test;

	M_bit_buf_fill(bbuf, 1, 15);
	/* 1111 1111 1111 1110
	 *   F    F    F    E
	 */
	check_lens(bbuf, 15, 2);
	to_bytes;
	ck_assert_uint_eq(nbytes, 2);
	ck_assert_uint_eq(bytes[0], 0xFF);
	ck_assert_uint_eq(bytes[1] & 0xFE, 0xFE);

	cleanup_test;
}
END_TEST


START_TEST(check_bbuf_add)
{
	init_test;

	M_bit_buf_add(bbuf, 0x0123456789ABCDEF, 21, M_BIT_BUF_PAD_NONE);
	/*                              0BCDEF
	 *                     0 1011 1100 1101 1110 1111
	 *                     0101 1110 0110 1111 0111 1
	 *                       5    E    6    F    7    8
	 */
	check_lens(bbuf, 21, 3);
	to_bytes;
	ck_assert_uint_eq(nbytes, 3);
	ck_assert_uint_eq(bytes[0], 0x5E);
	ck_assert_uint_eq(bytes[1], 0x6F);
	ck_assert_uint_eq(bytes[2], 0x78);

	reset_test;
	M_bit_buf_add(bbuf, 0x0123456789ABCDEF, 21, M_BIT_BUF_PAD_BEFORE);
	/* With padding: 000 0101 1110 0110 1111 0111 1
	 *               0000 1011 1100 1101 1110 1111
	 *                 0    B    C    D    E    F
	 */
	check_lens(bbuf, 24, 3);
	to_bytes;
	ck_assert_uint_eq(nbytes, 3);
	ck_assert_uint_eq(bytes[0], 0x0B);
	ck_assert_uint_eq(bytes[1], 0xCD);
	ck_assert_uint_eq(bytes[2], 0xEF);

	reset_test;
	M_bit_buf_add(bbuf, 0x0123456789ABCDEF, 21, M_BIT_BUF_PAD_AFTER);
	/* With padding: 0101 1110 0110 1111 0111 1 000
	 *               0101 1110 0110 1111 0111 1000
	 *                 5    E    6    F    7    8
	 */
	check_lens(bbuf, 24, 3);
	to_bytes;
	ck_assert_uint_eq(nbytes, 3);
	ck_assert_uint_eq(bytes[0], 0x5E);
	ck_assert_uint_eq(bytes[1], 0x6F);
	ck_assert_uint_eq(bytes[2], 0x78);

	cleanup_test;
}
END_TEST


START_TEST(check_bbuf_add_bytes)
{
	M_uint8 data[]    = {0x72, 0xAD, 0xE2, 0x0F, 0xAB};
	size_t  data_bits = 36;
	init_test;

	M_bit_buf_add_bytes(bbuf, data, data_bits);
	check_lens(bbuf, data_bits, 5);
	to_bytes;
	ck_assert_uint_eq(nbytes, 5);
	ck_assert_uint_eq(bytes[0], data[0]);
	ck_assert_uint_eq(bytes[1], data[1]);
	ck_assert_uint_eq(bytes[2], data[2]);
	ck_assert_uint_eq(bytes[3], data[3]);
	ck_assert_uint_eq(bytes[4] & 0xF0, data[4] & 0xF0);

	cleanup_test;
}
END_TEST


START_TEST(check_bbuf_add_bitstr)
{
	init_test;

	ck_assert(M_bit_buf_add_bitstr(bbuf, "1001", M_BIT_BUF_PAD_NONE));
	check_lens(bbuf, 4, 1);
	to_bytes;
	ck_assert_uint_eq(nbytes, 1);
	ck_assert_uint_eq(bytes[0] & 0xF0, 0x90);

	reset_test;
	ck_assert(M_bit_buf_add_bitstr(bbuf, " 1 0 \n\t0   1 ", M_BIT_BUF_PAD_NONE));
	check_lens(bbuf, 4, 1);
	to_bytes;
	ck_assert_uint_eq(nbytes, 1);
	ck_assert_uint_eq(bytes[0] & 0xF0, 0x90);

	reset_test;
	ck_assert(M_bit_buf_add_bitstr(bbuf, "1011 0001 1", M_BIT_BUF_PAD_NONE));
	check_lens(bbuf, 9, 2);
	to_bytes;
	ck_assert_uint_eq(nbytes, 2);
	ck_assert_uint_eq(bytes[0], 0xB1);
	ck_assert_uint_eq(bytes[1] & 0x80, 0x80);

	reset_test;
	ck_assert(M_bit_buf_add_bitstr(bbuf, "1011 0001 1", M_BIT_BUF_PAD_BEFORE));
	/* With padding: 0000 000 1011 0001 1
	 *               0000 0001 0110 0011
	 *                 0    1    6    3
	 */
	check_lens(bbuf, 16, 2);
	to_bytes;
	ck_assert_uint_eq(nbytes, 2);
	ck_assert_uint_eq(bytes[0], 0x01);
	ck_assert_uint_eq(bytes[1], 0x63);

	reset_test;
	ck_assert(M_bit_buf_add_bitstr(bbuf, "1011 0001 1", M_BIT_BUF_PAD_AFTER));
	/* With padding: 1011 0001 1 0000 000
	 *               1011 0001 1000 0000
	 *                 B    1    8    0
	 */
	check_lens(bbuf, 16, 2);
	to_bytes;
	ck_assert_uint_eq(nbytes, 2);
	ck_assert_uint_eq(bytes[0], 0xB1);
	ck_assert_uint_eq(bytes[1], 0x80);

	cleanup_test;
}
END_TEST


START_TEST(check_bbuf_truncate)
{
	init_test;

	M_bit_buf_add_bitstr(bbuf, "1011 0001 1", M_BIT_BUF_PAD_NONE);
	check_lens(bbuf, 9, 2);
	M_bit_buf_truncate(bbuf, 6);
	check_lens(bbuf, 6, 1);
	to_bytes;
	ck_assert_uint_eq(nbytes, 1);
	ck_assert_uint_eq(bytes[0] & 0xFC, 0xB0);

	reset_test;
	M_bit_buf_add_bitstr(bbuf, "1011 0001 1001 1", M_BIT_BUF_PAD_NONE);
	check_lens(bbuf, 13, 2);
	M_bit_buf_truncate(bbuf, 9);
	check_lens(bbuf, 9, 2);
	M_bit_buf_truncate(bbuf, 3);
	check_lens(bbuf, 3, 1);
	to_bytes;
	ck_assert_uint_eq(nbytes, 1);
	ck_assert_uint_eq(bytes[0] & 0xE0, 0xA0);

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

	suite = suite_create("check_bit_buf (M_bit_buf_t)");

	add_test(suite, check_bbuf_add_bit);
	add_test(suite, check_bbuf_fill);
	add_test(suite, check_bbuf_add);
	add_test(suite, check_bbuf_add_bytes);
	add_test(suite, check_bbuf_add_bitstr);
	add_test(suite, check_bbuf_truncate);

	sr = srunner_create(suite);
	srunner_set_log(sr, "check_bit_buf.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
