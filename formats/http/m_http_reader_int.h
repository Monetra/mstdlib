/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

#ifndef __M_HTTP_READER_INT_H__
#define __M_HTTP_READER_INT_H__

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* XXX: Here until we add m_http.h to mstdlib_formats.h */
#include <mstdlib/formats/m_http.h>
#include <mstdlib/formats/m_http_reader.h>

typedef enum {
	M_HTTP_READER_STEP_UNKNONW = 0,
	M_HTTP_READER_STEP_START_LINE,
	M_HTTP_READER_STEP_HEADER,
	M_HTTP_READER_STEP_BODY,
	M_HTTP_READER_STEP_CHUNK_START,
	M_HTTP_READER_STEP_CHUNK_DATA,
	M_HTTP_READER_STEP_MULTIPART_PREAMBLE,
	M_HTTP_READER_STEP_MULTIPART_HEADER,
	M_HTTP_READER_STEP_MULTIPART_DATA,
	M_HTTP_READER_STEP_MULTIPART_CHECK_END,
	M_HTTP_READER_STEP_MULTIPART_EPILOUGE,
	M_HTTP_READER_STEP_TRAILER,
	M_HTTP_READER_STEP_DONE
} M_http_reader_step_t;

struct M_http_reader{
	struct M_http_reader_callbacks  cbs;
	M_http_reader_flags_t           flags;
	void                           *thunk;
	char                           *boundary;
	M_http_reader_step_t            rstep;
	M_http_data_format_t            data_type;
	size_t                          header_len;
	M_bool                          have_body_len;
	size_t                          body_len;
	size_t                          body_len_seen;
	size_t                          part_idx;
	M_bool                          have_end;
	M_bool                          have_part;
	M_bool                          have_epilouge;
};

#endif
