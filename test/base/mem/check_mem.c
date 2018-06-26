#include "m_config.h"
#include <check.h>
#include <stdlib.h>
#include <string.h> /* memcmp */

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define ALPHA_CHARS "abcdefghijklmnopqrstuvwxyz"
#define STR_EMPTY ""
#define STR_TEST "test"
#define MEM_TEST (M_uint8 *)STR_TEST

static const M_uint8 *cmem1;
static const M_uint8 *cmem2;
static M_uint8 *mem1;
static M_uint8 *mem2;
static M_uint8 *test;
static M_uint8 temp[1024];
static const M_uint8 b0 = '0';
static const M_uint8 b1 = '1';
static const char str[] = ALPHA_CHARS;
static const M_uint8 mem[] = ALPHA_CHARS;
static const size_t mem_size = sizeof(mem);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_malloc_NULL)
{
	ck_assert_msg(M_malloc(0) == NULL);
	M_printf("**THIS OUT OF MEMORY ERROR IS EXPECTED**\n");
	ck_assert_msg(M_malloc((size_t)-1) == NULL);
}
END_TEST

START_TEST(check_malloc_M_mem_set)
{
	size_t size = (size_t)_i;
	mem1 = M_malloc(size);
	M_mem_set(mem1, 1, size);
	mem2 = M_malloc(size);
	M_mem_set(mem2, 1, size);
	/* both mem areas are the same */
	ck_assert_msg(M_mem_eq(mem1,mem2,size));

	/* clear out mem2 */
	M_mem_set(mem2, 0, size);
	ck_assert(!M_mem_eq(mem1,mem2,size));

	/* should free/zero the memory */
	M_free(mem1);
	M_free(mem2);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_free_allocated)
{
	size_t size = (size_t)_i;
	mem1 = M_malloc(size);
	ck_assert_msg(mem1 != NULL);
	/* try to free non-null allocation */
	M_free(mem1);
}
END_TEST

START_TEST(check_free_NULL)
{
	M_free(NULL);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_realloc_NULL)
{
	ck_assert_msg(M_realloc(NULL, 0) == NULL);
}
END_TEST

START_TEST(check_realloc_alloc_and_free)
{
	size_t size = (size_t)_i;
	ck_assert_msg(size > 0);
	mem1 = M_realloc(NULL,size);
	ck_assert_msg(mem1 != NULL);
	/* should return NULL when used as free() */
	mem1 = M_realloc(mem1,0);
	ck_assert_msg(mem1 == NULL);
}
END_TEST

START_TEST(check_realloc_resize_growing)
{
	size_t size = (size_t)_i;
	ck_assert_msg(size >= 1);
	mem1 = M_realloc(NULL,size);
	ck_assert_msg(mem1 != NULL);
	if (mem1 == NULL)
		return;
	M_mem_set(mem1,b0,size);
	M_mem_set(temp,b1,size);
	/* ensure mem1 and temp aren't the same */
	ck_assert_msg(!M_mem_eq(mem1,temp,size));
	/* preserve the contents of mem1 in temp */
	M_mem_copy(temp,mem1,size);

	size++;
	mem2 = M_realloc(mem1,size);
	ck_assert_msg(mem2 != NULL);
	/* ensure different pointers */
	ck_assert(mem1 != mem2);
	/* ensure expected content */
	ck_assert_msg(M_mem_eq(mem2,temp,size-1));
}
END_TEST

START_TEST(check_realloc_resize_shrinking)
{
	size_t size = (size_t)_i;

	ck_assert_msg(_i >= 2);
	mem1 = M_realloc(NULL,size);
	ck_assert_msg(mem1 != NULL);
	if (mem1 == NULL)
		return;
	M_mem_set(mem1, b0, size);

	M_mem_set(temp, b1, size);
	/* ensure mem1 and temp aren't the same */
	ck_assert_msg(!M_mem_eq(mem1,temp,size));
	/* preserve the contents of mem1 in temp */
	M_mem_copy(temp, mem1, size);

	/* shrink */
	size--;

	mem2 = M_realloc(mem1,size);
	ck_assert_msg(mem2 != NULL);
	/* ensure different pointers */
	ck_assert(mem1 != mem2);
	/* ensure expected content */
	ck_assert_msg(M_mem_eq(mem2,temp,size));
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_memdup_NULL)
{
	size_t size = (size_t)_i;
	ck_assert_msg(M_memdup(NULL,size) == NULL);
}
END_TEST

START_TEST(check_memdup_contents)
{
	size_t size = (size_t)_i;
	mem1 = M_memdup(mem,size);
	ck_assert_msg(M_mem_eq(mem1,mem,size));
	M_free(mem1);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_memdup_max_NULL)
{
	size_t size = (size_t)_i;
	mem1 = M_memdup_max(NULL,size,0);
	ck_assert_msg(mem1 == NULL);
}
END_TEST

START_TEST(check_memdup_max_contents)
{
	size_t size = (size_t)_i;
	mem1 = M_memdup_max(mem,size,0);
	ck_assert(mem1 != NULL);
	ck_assert_msg(M_mem_eq(mem1,mem,size));
	M_free(mem1);
}
END_TEST

START_TEST(check_memdup_max_empty_allocation)
{
	size_t size = (size_t)_i;
	mem1 = M_memdup_max(mem,0,size);
	ck_assert(mem1 != NULL);
	if (mem1 == NULL)
		return;
	M_mem_set(mem1,0,size);
	M_free(mem1);
}
END_TEST

START_TEST(check_memdup_max_contents_allocation)
{
	size_t size = (size_t)_i;
	mem1 = M_memdup_max(mem,size,size);
	ck_assert(mem1 != NULL);
	ck_assert_msg(M_mem_eq(mem1,mem,size));
	/* try for a segfault if size wasn't fully allocated */
	M_mem_set(mem1,0,size);
	M_free(mem1);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_mem_chr_NULL)
{
	ck_assert_msg(M_mem_chr(NULL, 'a',   0) == NULL);
	ck_assert_msg(M_mem_chr(NULL, 'a',  32) == NULL);
	ck_assert_msg(M_mem_chr(NULL, '\0',  0) == NULL);
	ck_assert_msg(M_mem_chr(NULL, '\0', 32) == NULL);
}
END_TEST

START_TEST(check_mem_chr_not_found)
{
	ck_assert_msg(M_mem_chr("a",  'a',   0) == NULL);
	ck_assert_msg(M_mem_chr("a",  'b',   1) == NULL);
	ck_assert_msg(M_mem_chr("a",  '\0',  1) == NULL);
}
END_TEST

START_TEST(check_mem_chr_found)
{
	ck_assert_msg(M_mem_chr(mem, mem[_i], mem_size) == mem+_i);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* expected_pos of NULL means not found */
static void check_mem_mem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len, const size_t *expected_pos)
{
	M_bool   expect_contains;
	void    *result_mem;
	size_t   result_pos;
	M_bool   result_has_mem;
	M_bool   result_has_idx;
	M_bool   result_contains;

	/* ensure our check is valid */
	if (expected_pos != NULL) ck_assert_msg(*expected_pos < haystack_len);

	/* determine expected values */
	expect_contains = expected_pos != NULL;

	/* perform queries */
	result_mem      = M_mem_mem(haystack, haystack_len, needle, needle_len);
	result_has_mem  = result_mem != NULL;
	result_has_idx  = M_mem_mempos(haystack, haystack_len, needle, needle_len, &result_pos);
	result_contains = M_mem_contains(haystack, haystack_len, needle, needle_len);

	/* ensure the match condition agrees with expected */
	ck_assert_msg(result_contains == expect_contains);
	ck_assert_msg(result_has_mem  == expect_contains);
	ck_assert_msg(result_has_idx  == expect_contains);

	/* if found, ensure pointer agrees */
	if (expect_contains) {
		size_t expect_pos = *expected_pos;
		ck_assert_msg(result_pos == expect_pos);
	}
	/* if found, ensure position agrees */
	if (expect_contains) {
		const void *expect_mem = (const M_uint8 *)haystack + *expected_pos;
		ck_assert_msg(result_mem == expect_mem);
	}
}

START_TEST(check_mem_mem_empty_haystack)
{
	/* no haystack, haystack_len is zero, other params okay */
	check_mem_mem(NULL, 0, mem, mem_size, NULL);
	/* no haystack, other params okay */
	check_mem_mem(NULL, 1, mem, mem_size, NULL);
	check_mem_mem(NULL, 2, mem, mem_size, NULL);
	check_mem_mem(NULL, 3, mem, mem_size, NULL);
	/* haystack_len is zero, other params okay */
	check_mem_mem(mem,  0, mem, mem_size, NULL);
}
END_TEST

START_TEST(check_mem_mem_empty_needle)
{
	size_t pos = 0;
	/* empty string exists at the beginning of haystack */
	check_mem_mem(mem, mem_size, NULL, 0, &pos);
	check_mem_mem(mem, mem_size, NULL, 1, &pos);
	check_mem_mem(mem, mem_size, mem,  0, &pos);
}
END_TEST

START_TEST(check_mem_mem_not_found)
{
	cmem1 = (const M_uint8 *)"test";
	cmem2 = (const M_uint8 *)"this";
	check_mem_mem(
	   cmem1, M_str_len((const char *)cmem1),
	   cmem2, M_str_len((const char *)cmem2),
	   NULL
       );
}
END_TEST

START_TEST(check_mem_mem_found)
{
	size_t pos = (size_t)_i;
	ck_assert_msg(mem_size > 0);
	ck_assert_msg(mem_size-pos > 0);
	mem2 = M_memdup(mem, mem_size);

	check_mem_mem(mem, mem_size, mem2+pos, mem_size-pos, &pos);

	M_free(mem2);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_mem_str_empty_haystack)
{
	/* no haystack, haystack_len is zero, other params okay */
	ck_assert_msg(M_mem_str(NULL, 0, str) == NULL);
	/* no haystack, other params okay */
	ck_assert_msg(M_mem_str(NULL, 1, str) == NULL);
	ck_assert_msg(M_mem_str(NULL, 2, str) == NULL);
	ck_assert_msg(M_mem_str(NULL, 3, str) == NULL);
	/* haystack_len is zero, other params okay */
	ck_assert_msg(M_mem_str(mem,  0, str) == NULL);
}
END_TEST

START_TEST(check_mem_str_empty_needle)
{
	/* empty string exists at the beginning of haystack */
	ck_assert_msg(M_mem_str(mem, mem_size, NULL)  == mem);
	ck_assert_msg(M_mem_str(mem, mem_size, NULL)  == mem);
}
END_TEST

START_TEST(check_mem_str_not_found)
{
	ck_assert_msg(M_mem_str("0123456789", 10, "011") == NULL);
	ck_assert_msg(M_mem_str("0123456789", 10, "321") == NULL);
}
END_TEST

START_TEST(check_mem_str_found)
{
	size_t pos = (size_t)_i;
	ck_assert_msg(pos < mem_size);
	test = M_mem_str(mem, mem_size, str+pos);
	ck_assert_msg(test == mem+pos);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_mem_copy_empty_dst)
{
	ck_assert_msg(M_mem_copy(NULL, NULL,        0) == NULL);
	ck_assert_msg(M_mem_copy(NULL, NULL, mem_size) == NULL);
	ck_assert_msg(M_mem_copy(NULL,  mem,        0) == NULL);
	ck_assert_msg(M_mem_copy(NULL,  mem, mem_size) == NULL);
}
END_TEST

START_TEST(check_mem_copy_empty_src)
{
	ck_assert_msg(mem_size > 0);

	mem1 = M_malloc(mem_size);
	mem2 = M_memdup(mem, mem_size);

	ck_assert_msg(M_mem_copy(mem1, NULL,        0) == mem1);
	ck_assert_msg(M_mem_copy(mem1, NULL, mem_size) == mem1);
	ck_assert_msg(M_mem_copy(mem1, mem2, mem_size) == mem1);

	M_free(mem1);
	M_free(mem2);
}
END_TEST

START_TEST(check_mem_copy_success)
{
	mem1 = M_malloc(mem_size);
	M_mem_set(mem1, 0, mem_size);

	/* ensure test is valid */
	ck_assert_msg(mem_size > 0);
	ck_assert(!M_mem_eq(mem1, mem, mem_size));

	mem2 = M_mem_copy(mem1, mem, mem_size);
	/* result should be identical to dest */
	ck_assert_msg(mem1 == mem2);
	/* ensure contents match */
	ck_assert_msg(M_mem_eq(mem1, mem, mem_size));

	M_free(mem1);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_mem_count_size_as_count)
{
	M_uint8 b = 0;

	size_t size = 16;
	/* allocate an array of b valued bytes */
	mem1 = M_malloc(size);
	M_mem_set(mem1,b,size);

	/* all bytes are b, so the count of b should equal the size
	 */
	while (size--) {
		ck_assert_msg(
		    M_mem_count(mem1,size,b) == size
		);
	}

	M_free(mem1);
}
END_TEST

static const M_uint8 COUNT_ZERO_AS_ZERO[] = \
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f" \
    "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f" \
    "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f" \
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f" \
    "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f" \
    "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f" \
    "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f" \
    "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f" \
    "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f" \
    "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f" \
    "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf" \
    "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf" \
    "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf" \
    "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf" \
    "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef" \
    "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

START_TEST(check_mem_count_zero_as_zero)
{
	size_t size = sizeof(COUNT_ZERO_AS_ZERO)-1;
	cmem1 = (const M_uint8 *)COUNT_ZERO_AS_ZERO;
	ck_assert_msg(M_mem_count(cmem1,size,0) == 0);
}
END_TEST

#define COUNT_NONZERO_AS_ONE COUNT_ZERO_AS_ZERO
START_TEST(check_mem_count_nonzero_as_one)
{
	size_t pos;
	size_t size;

	pos = (size_t)_i;
	cmem1 = COUNT_NONZERO_AS_ONE;
	size = sizeof(COUNT_NONZERO_AS_ONE)-1;
	ck_assert_msg(pos < size); /* ensure valid index */
	ck_assert_msg(M_mem_count(cmem1,size,cmem1[pos]) == 1);
}
END_TEST

START_TEST(check_mem_calc_crc8_ccitt)
{
	M_uint8 test1_data[] = {
		0x01, 0x02, 0x03, 0xFF, 0xF2, 0xA7, 0x05
	};
	M_uint8 test2_data[] = {
		0x00, 0x00, 0x00, 0x20, 0x50, 0x01, 0x00, 0x00,
		0x1A, 0xD7, 0x0A, 0x30, 0x2E, 0x30, 0x30, 0x2E,
		0x30, 0x34, 0x2E, 0x30, 0x34, 0xD3, 0x04, 0x11,
		0x00, 0x00, 0x00, 0xD4, 0x01, 0xFF, 0xDF, 0x3A,
		0x02, 0x01, 0x00, 0xB7
	};

	size_t test1_len = sizeof(test1_data) / sizeof(*test1_data);
	size_t test2_len = sizeof(test2_data) / sizeof(*test2_data);

	ck_assert(M_mem_calc_crc8_ccitt(test1_data, test1_len) == 0x28);
	ck_assert(M_mem_calc_crc8_ccitt(test2_data, test2_len) == 0x89);
}
END_TEST

START_TEST(check_mem_calc_crc16_ccitt)
{
	M_uint8 test1_data[] = {
		'1', '2', '3', '4', '5', '6', '7', '8', '9'
	};
	M_uint8 test2_data[] = {
		0x01, 0x02, 0x03, 0x04, 0x05
	};
	M_uint8 test3_data[] = {
		0x56, 0x69, 0x56, 0x4F, 0x74, 0x65, 0x63, 0x68, 0x00, 0x43, 0x18, 0x00, 0x00, 0x00
	};

	size_t test1_len = sizeof(test1_data) / sizeof(*test1_data);
	size_t test2_len = sizeof(test2_data) / sizeof(*test2_data);
	size_t test3_len = sizeof(test3_data) / sizeof(*test3_data);

	ck_assert(M_mem_calc_crc16_ccitt(test1_data, test1_len) == 0x29B1);
	ck_assert(M_mem_calc_crc16_ccitt(test2_data, test2_len) == 0x9304);
	ck_assert(M_mem_calc_crc16_ccitt(test3_data, test3_len) == 0xA1F5);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *mem_suite(void)
{
	Suite *suite;
	TCase *tc_malloc;
	TCase *tc_free;
	TCase *tc_realloc;
	TCase *tc_memdup;
	TCase *tc_memdup_max;
	TCase *tc_mem_chr;
	TCase *tc_mem_mem;
	TCase *tc_mem_str;
	TCase *tc_mem_copy;
	TCase *tc_mem_count;
	TCase *tc_mem_checksum;

	suite = suite_create("mem");

	tc_malloc = tcase_create("malloc");
	tcase_add_test(tc_malloc, check_malloc_NULL);
	tcase_add_loop_test(tc_malloc, check_malloc_M_mem_set, 1, 32);
	suite_add_tcase(suite, tc_malloc);

	tc_free = tcase_create("free");
	tcase_add_test(tc_free, check_free_NULL);
	tcase_add_loop_test(tc_free, check_free_allocated, 1, 32);
	suite_add_tcase(suite, tc_free);

	tc_realloc = tcase_create("realloc");
	tcase_add_test(     tc_realloc, check_realloc_NULL);
	tcase_add_loop_test(tc_realloc, check_realloc_alloc_and_free,   1, 32);
	tcase_add_loop_test(tc_realloc, check_realloc_resize_growing,   1, 32);
	tcase_add_loop_test(tc_realloc, check_realloc_resize_shrinking, 2, 32);
	suite_add_tcase(suite, tc_realloc);

	tc_memdup = tcase_create("memdup");
	tcase_add_loop_test(tc_memdup, check_memdup_NULL,     0, 32);
	tcase_add_loop_test(tc_memdup, check_memdup_contents, 1, 26);
	suite_add_tcase(suite, tc_memdup);

	tc_memdup_max = tcase_create("memdup_max");
	tcase_add_loop_test(tc_memdup_max, check_memdup_max_NULL,                1, 32);
	tcase_add_loop_test(tc_memdup_max, check_memdup_max_contents,            1, 26);
	tcase_add_loop_test(tc_memdup_max, check_memdup_max_empty_allocation,    1, 26);
	tcase_add_loop_test(tc_memdup_max, check_memdup_max_contents_allocation, 1, 26);
	suite_add_tcase(suite, tc_memdup_max);

	tc_mem_chr = tcase_create("mem_chr");
	tcase_add_test(     tc_mem_chr, check_mem_chr_NULL);
	tcase_add_test(     tc_mem_chr, check_mem_chr_not_found);
	tcase_add_loop_test(tc_mem_chr, check_mem_chr_found, 0, (int)mem_size);
	suite_add_tcase(suite, tc_mem_chr);

	tc_mem_mem = tcase_create("mem_mem");
	tcase_add_test(tc_mem_mem, check_mem_mem_empty_haystack);
	tcase_add_test(tc_mem_mem, check_mem_mem_empty_needle);
	tcase_add_test(tc_mem_mem, check_mem_mem_not_found);
	tcase_add_loop_test(tc_mem_mem, check_mem_mem_found, 0, (int)mem_size);
	suite_add_tcase(suite, tc_mem_mem);

	tc_mem_str = tcase_create("mem_str");
	tcase_add_test(tc_mem_str, check_mem_str_empty_haystack);
	tcase_add_test(tc_mem_str, check_mem_str_empty_needle);
	tcase_add_test(tc_mem_str, check_mem_str_not_found);
	tcase_add_loop_test(tc_mem_str, check_mem_str_found, 0, (int)M_str_len(str));
	suite_add_tcase(suite, tc_mem_str);

	tc_mem_copy = tcase_create("mem_copy");
	tcase_add_test(tc_mem_copy, check_mem_copy_empty_dst);
	tcase_add_test(tc_mem_copy, check_mem_copy_empty_src);
	tcase_add_test(tc_mem_copy, check_mem_copy_success);
	suite_add_tcase(suite, tc_mem_copy);

	tc_mem_count = tcase_create("mem_count");
	tcase_add_test(tc_mem_count, check_mem_count_size_as_count);
	tcase_add_test(tc_mem_count, check_mem_count_zero_as_zero);
	tcase_add_loop_test(tc_mem_count, check_mem_count_nonzero_as_one, 0, sizeof(COUNT_NONZERO_AS_ONE)-1);
	suite_add_tcase(suite, tc_mem_count);

	tc_mem_checksum = tcase_create("mem_checksum");
	tcase_add_test(tc_mem_checksum, check_mem_calc_crc8_ccitt);
	tcase_add_test(tc_mem_checksum, check_mem_calc_crc16_ccitt);
	suite_add_tcase(suite, tc_mem_checksum);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(mem_suite());
	srunner_set_log(sr, "check_mem.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
