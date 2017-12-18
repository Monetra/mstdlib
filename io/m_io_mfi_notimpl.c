/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
#include <mstdlib/io/m_io_mfi.h>

M_io_mfi_enum_t *M_io_mfi_enum(void)
{
	return NULL;
}

void M_io_mfi_enum_destroy(M_io_mfi_enum_t *mfienum)
{
	(void)mfienum;
}

size_t M_io_mfi_enum_count(const M_io_mfi_enum_t *mfienum)
{
	(void)mfienum;
	return 0;
}

const char *M_io_mfi_enum_name(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	(void)mfienum;
	(void)idx;
	return NULL;
}

const char *M_io_mfi_enum_protocol(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	(void)mfienum;
	(void)idx;
	return NULL;
}

const char *M_io_mfi_enum_serialnum(const M_io_mfi_enum_t *mfienum, size_t idx)
{
	(void)mfienum;
	(void)idx;
	return NULL;
}

M_io_error_t M_io_mfi_create(M_io_t **io_out, const char *protocol, const char *serialnum)
{
	(void)io_out;
	(void)protocol;
	(void)serialnum;
	return M_IO_ERROR_NOTIMPL;
}
