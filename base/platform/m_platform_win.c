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
#include "platform/m_platform.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ERRNO_T M_platform_errno(void)
{
	return GetLastError();
}

char *M_win32_wchar_to_char(const wchar_t *in)
{
	char *out;
	int   len;

	len = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, NULL);
	if (len == -1)
		return NULL;

	out = M_malloc_zero((size_t)len+1);

	if (WideCharToMultiByte(CP_UTF8, 0, in, -1, out, len, NULL, NULL) == -1) {
		M_free(out);
		return NULL;
	}

	return out;
}

M_bool M_win32_size_t_to_dword(size_t in, DWORD *out)
{
	if (sizeof(size_t) == sizeof(DWORD) || in <= M_UINT32_MAX) {
		if (out != NULL) {
			*out = (DWORD)in;
		}
		return M_TRUE;
	}

	if (out != NULL)
		*out = 0;
	return M_FALSE;
}

M_bool M_win32_size_t_to_int(size_t in, int *out)
{
	if (in <= M_INT32_MAX) {
		if (out != NULL) {
			*out = (int)in;
		}
		return M_TRUE;
	}

	if (out != NULL)
		*out = 0;
	return M_FALSE;
}
