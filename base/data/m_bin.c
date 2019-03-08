/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
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
#include "m_defs_int.h"

M_uint8 *M_bin_wrap(const M_uint8 *value, size_t len)
{
	M_uint8 *duped_value;

	if (value == NULL || len == 0)
		return NULL;

	/* Create space to put len+value. */
	duped_value = M_malloc((sizeof(*duped_value)*len)+M_SAFE_ALIGNMENT);
	/* Copy len to the start. */
	M_mem_copy(duped_value, &len, sizeof(len));
	/* Copy value after len. */
	M_mem_copy(duped_value+M_SAFE_ALIGNMENT, value, len);

	return duped_value;
}

M_uint8 *M_bin_wrapeddup(const M_uint8 *value)
{
	size_t orig_size;

	if (value == NULL)
		return NULL;

	M_mem_copy(&orig_size, value, sizeof(orig_size));
	if (orig_size == 0)
		return NULL;

	return M_memdup(value, orig_size+M_SAFE_ALIGNMENT);
}

void *M_bin_wrapeddup_vp(const void *value)
{
	return M_bin_wrapeddup(value);
}

const M_uint8 *M_bin_unwrap(const M_uint8 *value, size_t *len)
{
	if (len != NULL)
		*len = 0;

	if (value == NULL)
		return NULL;

	if (len != NULL)
		M_mem_copy(len, value, sizeof(*len));

	return value+M_SAFE_ALIGNMENT;
}

M_uint8 *M_bin_unwrapdup(const M_uint8 *value, size_t *len)
{
	const M_uint8 *uval;
	size_t         mylen;

	if (len == NULL)
		len = &mylen;

	if (value == NULL)
		return NULL;

	uval = M_bin_unwrap(value, len);
	return M_memdup(uval, *len);
}
