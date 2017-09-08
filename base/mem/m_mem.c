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

	if (haystack == NULL || haystack_len == 0) {
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
