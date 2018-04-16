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

#ifndef __M_HTTP_H__
#define __M_HTTP_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

struct M_http;
typedef struct M_http M_http_t;

typedef enum {
	M_HTTP_PARSE_RESULT_SUCCESS = 0,
	M_HTTP_PARSE_RESULT_REQUEST_LENGTH,
	M_HTTP_PARSE_RESULT_REQUEST_LINE_INVLD,
	M_HTTP_PARSE_RESULT_REQUEST_LINE_LENGTH,
	M_HTTP_PARSE_RESULT_HEADER_INVLD,
	M_HTTP_PARSE_RESULT_HEADER_LENGTH,
	M_HTTP_PARSE_RESULT_HEADER_DUPLICATE,
	M_HTTP_PARSE_RESULT_HEADER_FOLDING,
	M_HTTP_PARSE_RESULT_OPTION_UNKNOWN,
	M_HTTP_PARSE_RESULT_MALFORMED,
	M_HTTP_PARSE_RESULT_?,
} M_http_parse_result_t;

typedef enum {
	M_HTTP_MESSAGE_TYPE_REQUEST = 0,
	M_HTTP_MESSAGE_TYPE_RESPONSE
} M_http_message_type_t;

typedef enum {
	M_HTTP_VERSION_1_0 = 0,
	M_HTTP_VERSION_1_1,
	/* M_HTTP_VERSION_2 */
} M_http_message_type_t;

typedef enum {
	M_HTTP_METHOD_OPTIONS = 0,
	M_HTTP_METHOD_GET,
	M_HTTP_METHOD_HEAD,
	M_HTTP_METHOD_POST,
	M_HTTP_METHOD_PUT,
	M_HTTP_METHOD_DELETE,
	M_HTTP_METHOD_TRACE,
	M_HTTP_METHOD_CONNECT
} M_http_method_t;

M_http_t *M_http_create();
void M_http_destroy(void);

/* read data for parsing.
 * streamed reads are supported.
 * In event of error http object will not have been updated. */
M_http_parse_result_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len);

/* Convert http object to string for sending.
 * Can be used to generate header only and body data
 * can be directly appended. */
unsigned char *M_http_write(M_http_t *http, size_t &len);

/* response status code. */
M_uint32 M_http_status_code(M_http_t *http);
void M_http_set_status_code(M_http_t *http, M_uint32 code);

/* response textual status message. */
const char *M_http_status_message(M_http_t *http);
void M_http_set_status_message(M_http_t *http, const char *message);

/* message type (request, response) */
M_http_message_type_t M_http_message_type(M_http_t *http);
void M_http_set_message_type(M_http_t *http, M_http_message_type_t type);

/* http version. */
M_http_version_t M_http_version(M_http_t *http);
void M_http_version(M_http_t *http, M_http_version_t version);

/* http method (get, post...) */
M_http_method_t M_http_method(M_http_t *http);
void M_http_method(M_http_t *http, M_http_method_t method);

/* The full URI in the request. */
const char *M_http_uri(M_http_t *http);
void M_http_uri(M_http_t *http, const char *uri);

/* Host from the URI if absolute URI */
const char *M_http_host(M_http_t *http);

/* Port from the URI if absolute URI */
M_uint16 M_http_port(M_http_t *http);

/* Path from the URI */
const char *M_http_path(M_http_t *http);

/* Query strings after the path in the URI. */
const char *M_http_query_string(M_http_t *http);

/* Query strings parsed. */
const M_hash_dict_t *M_http_query_args(M_http_t *http);

/* Is upgrade to http 2 requested. */
M_bool M_http_do_upgrade(M_http_t *http);
/* Sets the header requesting upgrade to http2. */
void M_http_set_do_upgrade(M_http_t *http, M_bool do_upgrade);

/* Is keep alive connection type set to indicate the connection is persistent. */
M_bool M_http_persistent_conn(M_http_t *http);
/* Sets connection header to keep_alive or close. */
void M_http_persistent_conn(M_http_t *http, M_bool persist);

/* Heave we received or set all headers. */
M_bool M_http_headers_complete(M_http_t *http);
/* Not really useful because generating the message doesn't care.
 * Only here in case passing around the object before sending or
 * something. */
M_http_set_headers_complete(M_http_t *http, M_bool complete);

/* headers. excluding Set-Cookie */
const M_hash_dict_t *M_http_headers(M_http_t *http);
void M_http_set_headers(M_http_t *http, const M_hash_dict_t *headersr, M_bool merge);
void M_http_drop_headers(M_http_t *http);

/* Set-Cookie headers. */
const M_list_str_t *M_http_get_set_cookie(M_http_t *http);
void M_http_set_set_cookie(M_http_t *http, const M_list_str_t *vals);

/* Heave we received or set a complete body. */
M_bool M_http_body_complete(M_http_t *http);
void M_http_set_body_complete(M_http_t *http, M_bool complete);

/* Does not set the content length headers.
 * Will return whatever is in the object. Use M_http_drop_body
 * for streamed reads. */
const unsigned char *M_http_body(M_http_t *http);
void M_http_set_body(M_http_t *http, const unsigned char *data);

/* Drop body data stored in object.
 * Used for reading data as it's streamed in. */
void M_http_drop_body(M_http_t *http);

/* Is this a chunked message. */
M_bool M_http_is_chunked(M_http_t *http);
void M_http_set_chunked(M_http_t *http, M_bool chunked);

/* Have we received or set a complete chunk. */
M_bool M_http_chunk_complete(M_http_t *http);
void M_http_chunk_set_complete(M_http_t *http, complete);

/* Is this the last chunk in a chunk sequence. */
M_bool M_http_chuck_last(M_http_t *http);
void M_http_chuck_set_last(M_http_t *http, M_bool last);

/* chunk data. */
const unsigned char *M_http_chunk_body(M_http_t *http);
void M_http_chunk_set_body(M_http_t *http, const unsigned char *data);
void M_http_chunk_drop_body(M_http_t *http);

const M_hash_dict_t *M_http_chunk_trailer(M_http_t *http);
void M_http_chunk_set_trailer(M_http_t *http, const M_hash_dict_t *headers, M_bool merge);
void M_http_chunk_drop_trailer(M_http_t *http);

__END_DECLS

#endif /* __M_HTTP_H__ */
