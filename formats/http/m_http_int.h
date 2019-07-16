/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib_formats.h>
#include "m_defs_int.h"

#include "http/m_http_reader_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Internal object used by simple and other internal functionality to track
 * aspects of the http object. Not for public use.
 */

struct M_http;
typedef struct M_http M_http_t;

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

	M_bool                 is_chunked;

	M_hash_dict_t         *headers;
	char                  *content_type;
	char                  *origcontent_type;
	char                  *charset;
	M_textcodec_codec_t    codec;
	M_list_str_t          *set_cookies;
	M_hash_dict_t         *trailers;

	M_buf_t               *body;
	M_bool                 have_body_len;
	size_t                 body_len;
	size_t                 body_len_seen;

	M_list_t              *chunks; /* M_http_chunk_t */
};

struct M_http_simple_read {
	M_http_t                   *http;
	M_http_simple_read_flags_t  rflags;
	M_bool                      rdone;
} ;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_http_chunk_destory(M_http_chunk_t *chunk);
M_http_chunk_t *M_http_chunk_get(const M_http_t *http, size_t num);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * In progress internal http object that needs to be reworked. */

/*! Create an http object.
 *
 * The http object holds a parsed or compo unitized http request or
 * response.
 *
 * \return Object.
 */
M_http_t *M_http_create(void);


/*! Destroy an http object.
 *
 * \param[in] http HTTP object.
 */
void M_http_destroy(M_http_t *http);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Reset the http object for resuse.
 *
 * \param[in] http HTTP object.
 */
void M_http_reset(M_http_t *http);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The type (request/response) of message.
 *
 * \param[in] http HTTP object.
 *
 * \return Message type.
 */
M_http_message_type_t M_http_message_type(const M_http_t *http);


/*! Set the type (request/response) of message.
 *
 * \param[in] http HTTP object.
 * \param[in] type Message type.
 */
void M_http_set_message_type(M_http_t *http, M_http_message_type_t type);


/*! HTTP version.
 *
 * \param[in] http HTTP object.
 *
 * \return Version.
 */
M_http_version_t M_http_version(const M_http_t *http);


/*! Set the HTTP version.
 *
 * \param[in] http    HTTP object.
 * \param[in] version Version.
 */
void M_http_set_version(M_http_t *http, M_http_version_t version);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Response status code.
 *
 * \param[in] http HTTP object.
 *
 * \return Status code.
 */
M_uint32 M_http_status_code(const M_http_t *http);


/*! Set the response status code.
 *
 * \param[in] http HTTP object.
 * \param[in] code Status code.
 */
void M_http_set_status_code(M_http_t *http, M_uint32 code);


/*! Response textual status reason.
 *
 * \param[in] http HTTP object.
 *
 * \return String.
 */
const char *M_http_reason_phrase(const M_http_t *http);


/*! Set the response textual status reason.
 *
 * \param[in] http   HTTP object.
 * \param[in] phrase Textual reason phrase.
 */
void M_http_set_reason_phrase(M_http_t *http, const char *phrase);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Request method (get, post...)
 *
 * \param[in] http HTTP object.
 *
 * \return Method.
 */
M_http_method_t M_http_method(const M_http_t *http);


/*! Set the request method.
 *
 * \param[in] http   HTTP object.
 * \param[in] method Method.
 */
void M_http_set_method(M_http_t *http, M_http_method_t method);


/* Request URI.
 *
 * The full URI in the request.
 *
 * \param[in] http HTTP Object.
 *
 * \return String.
 */
const char *M_http_uri(const M_http_t *http);

/*! Set the request URI.
 *
 * \param[in] http HTTP object.
 * \param[in] uri  The URI.
 *
 * \return M_TRUE if URI was successfully set (parsed). Otherwise, M_FALSE.
 */
M_bool M_http_set_uri(M_http_t *http, const char *uri);


/*! Host part of request URI.
 *
 * Only present when URI is absolute.
 *
 * This can be used to determine if a URI is absolute or relative.
 *
 * \param[in] http HTTP object.
 *
 * \return String.
 */
const char *M_http_host(const M_http_t *http);


/*! Port part of request URI.
 *
 * Only present when URI is absolute and port is present.
 *
 * \param[in]  http HTTP object.
 * \param[out] port The port.
 *
 * \return M_TRUE if port is present. Otherwise, M_FALSE.
 */
M_bool M_http_port(const M_http_t *http, M_uint16 *port);


/*! Path from the request URI
 *
 * \param[in] http HTTP object.
 *
 * \return String.
 */
const char *M_http_path(const M_http_t *http);


/*! Query string from the request URI.
 *
 * \param[in] http HTTP object.
 *
 * \return String.
 *
 * \see M_http_query_args
 */
const char *M_http_query_string(const M_http_t *http);


/*! Query arguments from the request URI.
 *
 * \param[in] http HTTP object.
 *
 * \return Multi value dict.
 *
 * \see M_http_query_string
 */
const M_hash_dict_t *M_http_query_args(const M_http_t *http);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Currently loaded headers.
 *
 * Does not included the "Set-Cookie" header which can be sent multiple
 * times with different attributes.
 *
 * \param[in] http HTTP object.
 *
 * \return Multi value dict.
 */
const M_hash_dict_t *M_http_headers(const M_http_t *http);


/*! Get all values for a header combined into a string.
 *
 * Get the value of the header as a comma (,) separated list
 * if multiple values were specified.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 *
 * \return String.
 */
char *M_http_header(const M_http_t *http, const char *key);


/*! Set the http headers.
 *
 * \param[in] http    HTTP object.
 * \param[in] headers Headers. Can be multi value dict. NULL to clear.
 * \param[in] merge   Merge into or replace the existing headers.
 */
void M_http_set_headers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge);


/*! Set a single http header.
 *
 * Replaces existing values.
 * Can be a comma (,) separated list.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value. NULL to clear header.
 */
M_bool M_http_set_header(M_http_t *http, const char *key, const char *val);


/*! Set a single http header adding additional values.
 *
 * Adds a new value to the header list of values. Can be a comma (,) separated list.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
M_bool M_http_set_header_append(M_http_t *http, const char *key, const char *val);


/*! Add a value to a header.
 *
 * Preserves existing values.
 * Cannot not be a comma (,) separated list.
 * This adds to any existing values
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_add_header(M_http_t *http, const char *key, const char *val);


/*! Remove a header.
 *
 * Removes all values.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 */
void M_http_remove_header(M_http_t *http, const char *key);


/*! Get the Set-Cookie headers.
 *
 * \param[in] http HTTP object.
 *
 * \return String list
 */
const M_list_str_t *M_http_get_set_cookie(const M_http_t *http);


/*! Remove a value from the Set-Cookie header value list.
 *
 * \param[in] http HTTP object.
 * \param[in] idx  Index to remove.
 */
void M_http_set_cookie_remove(M_http_t *http, size_t idx);


/*! Append a value from the Set-Cookie header value list.
 *
 * \param[in] http HTTP object.
 * \param[in] val  Value to append.
 */
void M_http_set_cookie_insert(M_http_t *http, const char *val);


/*! Update the content type of it has changed.
 * 
 * Typically it's changed due to decoding.
 *
 * \param[in] http HTTP object.
 * \param[in] val  New content type.
 *
 */
void M_http_update_content_type(M_http_t *http, const char *val);


/*! Update the character encoding of it has changed.
 * 
 * \param[in] http  HTTP object.
 * \param[in] codec Text encoding.
 */
void M_http_update_charset(M_http_t *http, M_textcodec_codec_t codec);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the trailing headers.
 *
 * \param[in] http HTTP object.
 *
 * \return Multi value dict.
 */
const M_hash_dict_t *M_http_trailers(const M_http_t *http);


/*! Get all values for a trailer combined into a single
 *
 * Get the value of the header as a comma (,) separated list
 * if multiple values were specified.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 *
 * \return String.
 */
char *M_http_trailer(const M_http_t *http, const char *key);


/*! Set the trailing headers.
 *
 * \param[in] http    HTTP object.
 * \param[in] headers Headers. NULL to clear
 * \param[in] merge   Merge into or replace the existing headers.
 */
void M_http_set_trailers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge);


/*! Set a single http trailer.
 *
 * Replaces existing values.
 * Can be a comma (,) separated list.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value. NULL to clear.
 */
M_bool M_http_set_trailer(M_http_t *http, const char *key, const char *val);


/*! Add a value to a trailer.
 *
 * Preserves existing values.
 * Cannot not be a comma (,) separated list.
 * This adds a single value to any existing values
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_add_trailer(M_http_t *http, const char *key, const char *val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the body length.
 *
 * This is not the amount of data in the object.
 * This is the total length as defined by the
 * content-length header.
 *
 * \param[in]  http HTTP object.
 * \param[out] len  They body length.
 *
 * \return M_TRUE if the body length is known.
 */
M_bool M_http_body_length(M_http_t *http, size_t *len);


/*! Amount of body data that has been read.
 *
 * This is not the amount of data currently buffered
 * in the object. This the amount of data that has
 * passed through the object.
 *
 * \param[in] http HTTP object.
 *
 * \return Length. 
 *
 * \see M_http_body_length_buffered
 */
size_t M_http_body_length_seen(M_http_t *http);


/*! Amount of body data is currently buffered.
 *
 * \param[in] http HTTP object.
 *
 * \return Length. 
 */
size_t M_http_body_length_buffered(M_http_t *http);


/*! Get the body data.
 *
 * Data is returned raw and not decoded. It is up to the
 * caller to perform any decoded specified in the header.
 *
 * \param[in] http HTTP object.
 * \param[in] len  Length of data.
 *
 * \return Data.
 */
const unsigned char *M_http_body(const M_http_t *http, size_t *len);


/*! Add to existing body data.
 *
 * Increases seen length, and buffered length. If seen is greater than length
 * will also increase length.
 *
 * \param[in] http HTTP object.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_body_append(M_http_t *http, const unsigned char *data, size_t len);


/*! Drop the specified number of bytes from the beginning of the body data.
 *
 * Useful when doing partial reads of body data.
 * Only changes buffered length.
 *
 * \param[in] http HTTP object.
 * \param[in] len  Length of data.
 */
void M_http_body_drop(M_http_t *http, size_t len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Is this a chunked message.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool
 */
M_bool M_http_is_chunked(const M_http_t *http);


/*! Queue a new chunk.
 *
 * \param[in] http HTTP object.
 *
 * \return Chunk number.
 */
size_t M_http_chunk_insert(M_http_t *http);


/*! Remove a chunk.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 */
void M_http_chunk_remove(M_http_t *http, size_t num);


/*! Number of available data chunks.
 *
 * \param[in] http HTTP object.
 *
 * \return Number of chunks. 
 */
size_t M_http_chunk_count(const M_http_t *http);


/*! The length of the chunked data.
 *
 * \note When the length is 0 this indicates it is the
 *       final chunk in the sequence and all data has
 *       been sent.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 *
 * \return Length.
 */
size_t M_http_chunk_data_length(const M_http_t *http, size_t num);


/*! Amount of chunk data that has been read.
 *
 * This is not the amount of data currently buffered
 * in the object. This the amount of data that has
 * passed through the object.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 *
 * \return Length. 
 *
 * \see M_http_chunk_data_length_buffered
 */
size_t M_http_chunk_data_length_seen(const M_http_t *http, size_t num);


/*! Amount of chunk data is currently buffered.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 *
 * \return Length. 
 */
size_t M_http_chunk_data_length_buffered(const M_http_t *http, size_t num);


/*! Get the chunk data.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 * \param[in] len  Length of data.
 *
 * Data is returned raw and not decoded. It is up to the
 * caller to perform any decoded specified in the header.
 *
 * \return Data.
 */
const unsigned char *M_http_chunk_data(const M_http_t *http, size_t num, size_t *len);


/*! Add to existing chunked data.
 *
 * Increases seen length, and buffered length. If seen is greater than length
 * will also increase length.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_chunk_data_append(M_http_t *http, size_t num, const unsigned char *data, size_t len);


/*! Drop the specified number of bytes from the beginning of the chunk data.
 *
 * Useful when doing partial reads of chunk data.
 * Only changes buffered length.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 * \param[in] len  Length of data.
 */
void M_http_chunk_data_drop(M_http_t *http, size_t num, size_t len);


/*! Get the chunk's extensions.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 *
 * \return Dict.
 */
const M_hash_dict_t *M_http_chunk_extensions(const M_http_t *http, size_t num);


/*! Get all extensions combined into a single string.
 *
 * Get the value of of all extensions as a semicolon (;) separated list.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 *
 * \return String.
 */
char *M_http_chunk_extension_string(const M_http_t *http, size_t num);


/*! Set the chunk extensions.
 *
 * \param[in] http       HTTP object.
 * \param[in] num        Chunk number.
 * \param[in] extensions Extensions.
 */
void M_http_set_chunk_extensions(M_http_t *http, size_t num, const M_hash_dict_t *extensions);


/*! Set the extensions from a string.
 *
 * Can be a semicolon (;) separated list.
 * If not a list this is the equivalent of calling
 * M_http_set_chunk_extension without a value.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 * \param[in] str  String.
 */ 
M_bool M_http_set_chunk_extensions_string(M_http_t *http, size_t num, const char *str);


/*! Set a single chunk extension.
 *
 * Replaces existing values.
 * Cannot not be a semicolon (;) separated list.
 *
 * \param[in] http HTTP object.
 * \param[in] num  Chunk number.
 * \param[in] key  Name.
 * \param[in] val  Value. Can be NULL.
 */
void M_http_set_chunk_extension(M_http_t *http, size_t num, const char *key, const char *val);

#endif
