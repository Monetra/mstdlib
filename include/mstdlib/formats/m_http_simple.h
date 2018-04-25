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

#ifndef __M_HTTP_SIMPLE_H__
#define __M_HTTP_SIMPLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_http_simple HTTP Simple
 *  \ingroup m_http
 *
 * @{
 */

struct M_http_simple;
typedef struct M_http_simple M_http_simple_t;

typedef enum {
	M_HTTP_SIMPLE_READ_NONE = 0,
	M_HTTP_SIMPLE_READ_LEN_REQUIRED,   /*!< Require content-length, cannot be chunked data. */
	M_HTTP_SIMPLE_READ_FAIL_EXTENSION, /*!< Fail if chunked extensions are specified. Otherwise, Ignore. */
	M_HTTP_SIMPLE_READ_FAIL_TRAILERS   /*!< Fail if tailers sent. Otherwise, they are ignored. */
} M_http_simple_read_flags_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_simple_destroy(M_http_simple_t *http);

M_http_message_type_t M_http_simple_message_type(const M_http_simple_t *simple);
M_http_version_t M_http_simple_version(const M_http_simple_t *simple);
M_uint32 M_http_simple_status_code(const M_http_simple_t *simple);
const char *M_http_simple_reason_phrase(const M_http_simple_t *simple);
M_http_method_t M_http_simple_method(const M_http_simple_t *simple);
const char *M_http_simple_uri(const M_http_simple_t *simple);
M_bool M_http_simple_port(const M_http_simple_t *simple, M_uint16 *port);
const char *M_http_simple_path(const M_http_simple_t *simple);
const char *M_http_simple_query_string(const M_http_simple_t *simple);
const M_hash_dict_t *M_http_simple_query_args(const M_http_simple_t *simple);
const M_hash_dict_t *M_http_simple_headers(const M_http_simple_t *simple);
char *M_http_simple_header(const M_http_simple_t *simple, const char *key);
const M_list_str_t *M_http_simple_get_set_cookie(const M_http_simple_t *simple);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_error_t M_http_simple_read(M_http_simple_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags, size_t *len_read);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version, const M_hash_dict_t *headers, const char *data, size_t datea_len, size_t *len);
unsigned char *M_http_simple_write_respone(M_http_version_t version, M_uint32 code, const char *reason, const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);

/*! @} */

__END_DECLS

#endif /* __M_HTTP_SIMPLE_H__ */
