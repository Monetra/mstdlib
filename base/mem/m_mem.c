/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "m_config.h"

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset, memchr, memmove */

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * TODO:
 *  size_t      M_mem_strpos(const char *haystack, size_t haystack_len, const char *needle);
 *  M_bool      M_mem_str(const char *haystack, size_t haystack_len, const char *needle);
 *  char       *M_mem_mem(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len);
 * possibly:
 *  void        M_mem_hexdump(FILE *fp, const void *ptr, unsigned int len);
 *  void       *M_memdup(void *ptr, size_t size);
 *  void        M_memv_free(void **ptr);
 *  size_t      M_memv_len(const void *const *ptr);
 */

M_BEGIN_IGNORE_DEPRECATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Perform a forced memset.
 *
 * Needs to be a separate function to help prevent compiler optimizations
 * removing this..
 * 
 * \see https://buildsecurityin.us-cert.gov/bsi/articles/knowledge/coding/771-BSI.html
 * \param[in]  ptr  memory location
 * \param[in]  n    number of bytes to set
 * \returns \p ptr with each byte set to 0xFF, or NULL if \p ptr is \p NULL
 */
static void *M_mem_secure_clear(void *ptr, size_t n)
{
	if (ptr == NULL)
		return NULL;

	ptr = M_mem_set(ptr, 0xFF, n);
	/* prevent compiler from optimizing out above memset */
	*(volatile char *)ptr = *(volatile char *)ptr;
	return ptr;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_malloc_error(void)
{
	fputs("********OUT OF MEMORY*********\n", stderr);
	abort();
	return M_FALSE;
}

/* 13 callback slots with the first one always being the internal failure callback.
 * This allows for 12 user specified chained callbacks. */
static M_malloc_error_cb error_cbs[13] =
	{ M_malloc_error, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static size_t error_cbs_cnt = 1;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_malloc_register_errorcb(M_malloc_error_cb cb)
{
	if (cb == NULL)
		return M_FALSE;

	if (error_cbs_cnt >= sizeof(error_cbs)/sizeof(*error_cbs))
		return M_FALSE;

	error_cbs[error_cbs_cnt] = cb;
	error_cbs_cnt++;

	return M_TRUE;
}

M_bool M_malloc_deregister_errorcb(M_malloc_error_cb cb)
{
	size_t idx = 0;
	size_t i;


	if (cb == NULL || error_cbs_cnt <= 1)
		return M_FALSE;

	for (i=0; i<sizeof(error_cbs)/sizeof(*error_cbs); i++) {
		if (error_cbs[i] == cb) {
			idx = i;
			break;
		}
	}

	/* idx 0 is the internal cannot be changed call and means the cb was not found. */
	if (idx == 0)
		return M_FALSE;

	if (idx != error_cbs_cnt-1)
		M_mem_move(error_cbs+idx, error_cbs+idx+1, (error_cbs_cnt-idx-1)*sizeof(*error_cbs));

	error_cbs[error_cbs_cnt-1] = NULL;
	error_cbs_cnt--;
	return M_TRUE;
}

void M_malloc_clear_errorcb(void)
{
	if (error_cbs_cnt <= 1)
		return;

	M_mem_set(error_cbs+1, 0, (sizeof(error_cbs)/sizeof(*error_cbs))-1); 
	error_cbs_cnt = 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void *M_malloc(size_t size)
{
	void   *ptr;
	size_t  ecb_num = error_cbs_cnt;
	M_bool  success = M_FALSE;

	/* Prevent size + M_SAFE_ALIGNMENT exceeding maximum amount of memory */
	if (size == 0 || size > SIZE_MAX - M_SAFE_ALIGNMENT)
		return NULL;

	while (1) {
		ptr = malloc(size + M_SAFE_ALIGNMENT);
		if (ptr != NULL) {
			break;
		} else {
			success = M_FALSE;
			while (ecb_num > 0 && !success) {
				ecb_num--;
				success = error_cbs[ecb_num]();
			}
			if (success) {
				continue;
			}
		}
		break;
	} 

	if (ptr == NULL)
		return NULL;

	/* Cache size allocated so we can free it later */
	M_mem_copy(ptr, &size, sizeof(size));
	return ((char *)ptr) + M_SAFE_ALIGNMENT;
}

void *M_malloc_zero(size_t size)
{
	void *p;

	if (size == 0)
		return NULL;

	p = M_malloc(size);
	if (p == NULL)
		return NULL;

	M_mem_set(p, 0, size);
	return p;
}

static void *M_realloc_int(void *ptr, size_t size, M_bool zero)
{
	void  *ret;
	size_t orig_size = 0;

	/* Same as M_malloc */
	if (ptr == NULL) {
		if (zero)
			return M_malloc_zero(size);
		return M_malloc(size);
	}

	/* Same as M_free */
	if (size == 0) {
		M_free(ptr);
		return NULL;
	}

	/* Get the original size */
	M_mem_copy(&orig_size, ((char *)ptr) - M_SAFE_ALIGNMENT, sizeof(orig_size));

	/* Copy all data to new memory address */
	ret = M_memdup_max(ptr, orig_size, size);

	/* Zero out the extended memory if necesary */
	if (ret != NULL && zero && size > orig_size) {
		M_mem_set(((char *)ret)+orig_size, 0, size-orig_size);
	}

	/* Free original memory pointer if realloc didn't fail */
	if (ret != NULL)
		M_free(ptr);

	return ret;
}

void *M_realloc(void *ptr, size_t size)
{
	return M_realloc_int(ptr, size, M_FALSE);
}

void *M_realloc_zero(void *ptr, size_t size)
{
	return M_realloc_int(ptr, size, M_TRUE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void *M_memdup(const void *src, size_t size)
{
	return M_memdup_max(src,size,size);
}

void *M_memdup_max(const void *src, size_t size, size_t min_alloc_size)
{
	void *ret;

	/* error condition */
	if (src == NULL && size > 0)
		return NULL;

	if (src == NULL && size == 0)
		return M_malloc(min_alloc_size);

	ret = M_malloc(M_MAX(size, min_alloc_size));
	M_mem_copy(ret,src,size);
	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_free(void *ptr)
{
	void  *actual_ptr;
	size_t size = 0;

	if (ptr == NULL)
		return;

	if (ptr == (void *)SIZE_MAX) {
		M_fprintf(stderr, "M_free(): invalid pointer address\n");
		abort();
	}

	actual_ptr = ((char *)ptr) - M_SAFE_ALIGNMENT;

	/* Grab size out of buffer */
	M_mem_copy(&size, actual_ptr, sizeof(size));

	/* Secure clear uses 0xFF, so SIZE_MAX should be 0xFFFFFFFF or 0xFFFFFFFFFFFFFFFF
	 * so if we see a size as size_max, we know we're dealing with already-free()'d
	 * memory */
	if (size == SIZE_MAX || size == 0) {
		M_fprintf(stderr, "M_free(): double-free or corrupt memory\n");
		abort();
	}

	/* Secure the user-data */
	M_mem_secure_clear(actual_ptr, size + M_SAFE_ALIGNMENT);

	free(actual_ptr);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

void *M_mem_set(void *s, int c, size_t n)
{
	if (s == NULL || n == 0)
		return NULL;

	return memset(s, c, n);
}


void *M_mem_move(void *dst, const void *src, size_t size)
{
	if (src == NULL) {
		return dst;
	}
	if (dst == NULL) {
		return NULL;
	}

	return memmove(dst, src, size);
}


void *M_mem_copy(void *dst, const void *src, size_t size)
{
	return M_mem_move(dst, src, size);
}


M_bool M_mem_eq(const void *m1, const void *m2, size_t size)
{
	unsigned char result = 0;
	size_t        i;

	if (m1 == m2)
		return M_TRUE;

	if (m1 == NULL || m2 == NULL)
		return M_FALSE;

	for (i = 0; i < size; i++) {
		result |= ((const unsigned char *)m1)[i] ^ ((const unsigned char *)m2)[i];
	}
	return result == 0;
}

int M_mem_cmpsort(const void *m1, size_t size1, const void *m2, size_t size2)
{
	if (m1 == m2 || (size1 == 0 && size2 == 0))
		return 0;
	if (m1 == NULL)
		return -1;
	if (m2 == NULL)
		return 1;
	if (size1 < size2)
		return -1;
	if (size1 > size2)
		return 1;
	/* At this point neither m1 nor m2 is NULL, size1 == size2, and neither size1 nor size2 is 0. */
	return memcmp(m1, m2, size1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Query
 */

void *M_mem_chr(const void *m, M_uint8 b, size_t n)
{
	return m == NULL ? NULL : memchr(m,b,n);
}

/* - - - - - - - - - - - - - - - - - - - - */

void *M_mem_mem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
	size_t i = 0;
	const M_uint8 *pos;
	const M_uint8 *ret = NULL;

	if (haystack == NULL || haystack_len == 0 || needle_len > haystack_len) {
		return NULL;
	}
	if (needle == NULL || needle_len == 0) {
		return M_CAST_OFF_CONST(void *, haystack);
	}

	while (i <= haystack_len - needle_len) {
		/* Lets use memchr to find the first character
		 * of needle so we don't do a for loop and cycle
		 * through the entire memory space doing a memcmp
		 * moving forward one byte at a time */
		pos = memchr((const M_uint8 *)haystack+i, *(const M_uint8 *)needle, haystack_len - i);
		if (pos == NULL) break;

		i += (size_t)(pos - ((const M_uint8 *)haystack + i));

		/* Sanity check, we need to make sure that the current
		 * position plus the length of needle don't overflow the
		 * haystack length */
		if (i > haystack_len - needle_len) break;

		/* We know the first character matches, do the rest? */
		if (M_mem_eq((const M_uint8 *)haystack+i, needle, needle_len)) {
			/* Fugly cast, but stays in line with functions
			 * like strstr, strchr, memchr, etc */
			ret = (const M_uint8 *)haystack + i;
			break;
		}

		/* Increment i so that we don't memchr the same position again */
		i++;
	}

	return M_CAST_OFF_CONST(M_uint8 *, ret);
}

void *M_mem_rmem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
	size_t         i;
	const M_uint8 *ret = NULL;
	
	if (haystack == NULL || haystack_len == 0)
		return NULL;

	if (needle == NULL || needle_len == 0)
		return M_CAST_OFF_CONST(void *, haystack);

	if (haystack_len > needle_len)
		return NULL;

	for (i=haystack_len-needle_len; i-->0; ) {
		if (M_mem_eq((const M_uint8 *)haystack+i, needle, needle_len)) {
			ret = (const M_uint8 *)haystack + i;
			break;
		}
	}

	return M_CAST_OFF_CONST(M_uint8 *, ret);
}

/* See offset where needle exists in haystack, -1 if it doesn't exist */
M_bool M_mem_mempos(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len, size_t *idx)
{
	M_uint8 *pos = M_mem_mem(haystack, haystack_len, needle, needle_len);
	if (pos != NULL && idx != NULL)
		*idx = (size_t)(pos - (const M_uint8 *)haystack);
	return pos != NULL;
}

M_bool M_mem_contains(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
	return M_mem_mempos(haystack, haystack_len, needle, needle_len, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_mem_count(const void *s, size_t s_len, M_uint8 b)
{
	const M_uint8 *p;
	size_t i;
	size_t cnt=0;

	if (s == NULL) {
		return 0;
	}
	
	p = s;
	for (i=0; i<s_len; i++) {
		if (p[i] == b) cnt++;
	}

	return cnt;
}

unsigned char M_mem_calc_lrc(const void *s, size_t s_len)
{
	int         lrc = 0;
	size_t      i;
	const char *sc;

	if (s == NULL || s_len == 0)
		return 0;

	sc = s;
	for (i=0; i<s_len; i++) {
		lrc ^= sc[i];
	}

	return (unsigned char)lrc;
}

M_uint8 M_mem_calc_crc8_ccitt(const void *s, size_t s_len)
{
	/* Based off of public domain code from here: https://www.3dbrew.org/wiki/CRC-8-CCITT */
	M_uint8        val;
	const M_uint8 *pos;
	const M_uint8 *end;

	static const M_uint8 CRC_TABLE[256] = {
		0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
		0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
		0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
		0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
		0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
		0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
		0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
		0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
		0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
		0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
		0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
		0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
		0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
		0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
		0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
		0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
		0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
		0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
		0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
		0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
		0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
		0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
		0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
		0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
		0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
		0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
		0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
		0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
		0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
		0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
		0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
		0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
	};

	if (s == NULL || s_len == 0) {
		return 0;
	}

	val = 0;
	pos = (const M_uint8 *)s;
	end = pos + s_len;

	while (pos < end) {
		val = CRC_TABLE[val ^ *pos];
		pos++;
	}

	return val;
}

M_bool M_mem_swap_bytes(M_uint8 *s, size_t s_len, size_t idx1, size_t idx2)
{
	M_uint8 b;

	if (s_len == 0 || idx1 >= s_len || idx2 >= s_len)
		return M_FALSE;

	if (idx1 == idx2)
		return M_TRUE;

	b       = s[idx1];
	s[idx1] = s[idx2];
	s[idx2] = b;

	return M_TRUE;
}

M_END_IGNORE_DEPRECATIONS
