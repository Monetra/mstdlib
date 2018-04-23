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

#ifndef __M_HTTP_INT_H__
#define __M_HTTP_INT_H__

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"

/* XXX: Here until we add m_http.h to mstdlib_formats.h */
#include <mstdlib/formats/m_http.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_buf_t       *body;
	M_hash_dict_t *extensions;
	size_t         body_len;
	size_t         body_len_seen;
} M_http_chunk_t;

struct M_http {
	M_http_message_type_t  type;
	M_http_version_t       version;
	M_uint32               status_code;
	char                  *reason_phrase;
	M_http_method_t        method;

	char                  *uri;
	char                  *host;
	M_uint16               port;
	char                  *path;
	char                  *query_string;
	M_hash_dict_t         *query_args;

	M_http_read_step_t     read_step;
	M_bool                 is_chunked;

	size_t                 header_len;
	M_hash_dict_t         *headers;
	M_list_str_t          *set_cookies;
	M_hash_dict_t         *trailers;

	M_buf_t               *body;
	M_bool                 have_body_len;
	size_t                 body_len;
	size_t                 body_len_seen;

	M_list_t              *chunks; /* M_http_chunk_t */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_chunk_destory(M_http_chunk_t *chunk);
M_http_chunk_t *M_http_chunk_get(const M_http_t *http, size_t num);

#endif
