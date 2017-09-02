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

#include <mstdlib/mstdlib.h>

/*! Default hash algorithm. FNV1a */
static M_uint32 M_hash_func_hash_FNV1a(const void *key, size_t key_len, M_uint32 seed)
{
	const unsigned char *bp = key;
	const unsigned char *be = bp + key_len;
	M_uint32             hv = seed; /*2166136261U;*/

	while (bp < be) {
		hv ^= (M_uint32)*bp++;
		/* hv *= 0x01000193 */
		hv += (hv<<1) + (hv<<4) + (hv<<7) + (hv<<8) + (hv<<24);
	}

	return hv;
}

static M_uint32 M_hash_func_hash_FNV1a_casecmp(const void *key, size_t key_len, M_uint32 seed)
{
	/* FNV1a */
	const char *bp = key;
	const char *be = bp + key_len;
	M_uint32    hv = seed; /*2166136261U*/;

	while (bp < be) {
		hv ^= (unsigned char)M_chr_tolower(*bp++);
		/* hv *=  16777619 */
		hv += (hv<<1) + (hv<<4) + (hv<<7) + (hv<<8) + (hv<<24);
	}
	return hv;
}

M_uint32 M_hash_func_hash_str(const void *key, M_uint32 seed)
{
	return M_hash_func_hash_FNV1a(key, M_str_len(key), seed);
}

M_uint32 M_hash_func_hash_str_casecmp(const void *key, M_uint32 seed)
{
	return M_hash_func_hash_FNV1a_casecmp(key, M_str_len(key), seed);
}

M_uint32 M_hash_func_hash_vp(const void *key, M_uint32 seed)
{
	return M_hash_func_hash_FNV1a(&key, sizeof(key), seed);
}

M_uint32 M_hash_func_hash_u64(const void *key, M_uint32 seed)
{
	return M_hash_func_hash_FNV1a(key, 8, seed);
}

void *M_hash_func_u64dup(const void *arg)
{
	return M_memdup(arg, 8);
}

void *M_hash_void_strdup(const void *arg)
{
	return M_strdup(arg);
}
