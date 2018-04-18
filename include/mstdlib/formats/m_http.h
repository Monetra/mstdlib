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

/*! \addtogroup m_http HTTP
 *  \ingroup m_formats
 *
 * @{
 */

struct M_http;
typedef struct M_http M_http_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Result of parsing. */
typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,
	M_HTTP_ERROR_SUCCESS_END,

	M_HTTP_ERROR_INVALIDUSE,
	M_HTTP_ERROR_STARTLINE_LENGTH, /* 414 (6k limit) */
	M_HTTP_ERROR_STARTLINE_MALFORMED, /* 400 */
	M_HTTP_ERROR_UNKNOWN_VERSION,
	M_HTTP_ERROR_REQUEST_METHOD, /* 501 */
	M_HTTP_ERROR_REQUEST_URI,
	M_HTTP_ERROR_HEADER_LENGTH, /* 413 (8k limit) */
	M_HTTP_ERROR_HEADER_NODATA,
	M_HTTP_ERROR_HEADER_FOLD, /* 400/502 */
	M_HTTP_ERROR_HEADER_INVLD,
	M_HTTP_ERROR_HEADER_MALFORMEDVAL, /* 400 */
	M_HTTP_ERROR_HEADER_DUPLICATE, /* 400 */
	M_HTTP_ERROR_LENGTH_REQUIRED, /* 411 */
	M_HTTP_ERROR_UPGRADE,
	M_HTTP_ERROR_MALFORMED
} M_http_error_t;


/*! Message type. */
typedef enum {
	M_HTTP_MESSAGE_TYPE_UNKNOWN = 0,
	M_HTTP_MESSAGE_TYPE_REQUEST,
	M_HTTP_MESSAGE_TYPE_RESPONSE
} M_http_message_type_t;


/*! HTTP version in use. */
typedef enum {
	M_HTTP_VERSION_UNKNOWN, /*!< Unknown. */
	M_HTTP_VERSION_1_0,     /*!< 1.0 */
	M_HTTP_VERSION_1_1,     /*!< 1.1 */
	M_HTTP_VERSION_2        /*!< 2 */
} M_http_message_type_t;


/*! HTTP methods. */
typedef enum {
	M_HTTP_METHOD_UNKNOWN = 0, /*!< Options. */
	M_HTTP_METHOD_OPTIONS,     /*!< Options. */
	M_HTTP_METHOD_GET,         /*!< Get. */
	M_HTTP_METHOD_HEAD,        /*!< Head. */
	M_HTTP_METHOD_POST,        /*!< Post. */
	M_HTTP_METHOD_PUT,         /*!< Put. */
	M_HTTP_METHOD_DELETE,      /*!< Delete. */
	M_HTTP_METHOD_TRACE,       /*!< Trace. */
	M_HTTP_METHOD_CONNECT      /*!< Connect. */
} M_http_method_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
 * An http object can be reused with various internals cleared as needed.
 *
 * \param[in] http HTTP object.
 *
 * \see M_http_clear
 */
void M_http_destroy(M_http_t *http);


/*! Has the content length header been set to required.
 *
 * Does not allow read until close body.
 * Does not allow chunked encoding.
 *
 * \param[in] http    HTTP object.
 * \param[in] require Require content length.
 */
M_bool M_http_require_content_length(M_http_t *http);


/*! Require content length header to be present.
 *
 * Does not allow read until close body.
 * Does not allow chunked encoding.
 *
 * \param[in] http    HTTP object.
 * \param[in] require Require content length.
 */
void M_http_set_require_content_length(M_http_t *http, M_bool require);


/*! Clear contents of http object.
 *
 * This is the equivalent of calling all clear functions.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear(M_http_t *http);


/*! Clear headers.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_headers(M_http_t *http);


/*! Clear Set-Cookie header (Removes).
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_set_cookie(M_http_t *http);


/*! Clear body.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_body(M_http_t *http);


/*! Clear chunk body and trailer.
 *
 * Chunked body and trailers should be cleared
 * once all data from the chunk is processed.
 * This will put the HTTP object in a state
 * ready for reading in the next chunk.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_chunked(M_http_t *http);


/*! Clear chunk body.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_chunk_body(M_http_t *http);


/*! Clear chunk trailer.
 *
 * \param[in] http HTTP object.
 */
void M_http_clear_chunk_trailer(M_http_t *http);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse raw data into an http object.
 *
 * The http object will be updated with the data after parsing.
 * Multiple reads may be necessary for a single message in case
 * data is being streamed. Once M_HTTP_PARSE_RESULT_SUCCESS_END is
 * returned no new calls to read should be performed unless the
 * object is cleared with M_http_clear.
 *
 * It's not always possible to determine when a message ends.
 * For example no length is specified. In which case M_HTTP_PARSE_RESULT_SUCCESS_END 
 * will never be returned.
 *
 * When receiving chunked data there could be multiple chunks in the
 * stream. M_HTTP_PARSE_RESULT_SUCCESS_END will be returned when a
 * single chunk is processed. More calls to read are necessary starting
 * from the return read_len position in the data to continuing reading
 * the next chunks.
 *
 * \param[in]  http     HTTP object.
 * \param[in]  data     Raw data.
 * \param[in]  data_len Length of data.
 * \param[out] len_read Length of the data read. Can be less than data_len if the content length was reached.
 * 
 * \return M_HTTP_PARSE_RESULT_SUCCESS when a message has been read without error.
 *         M_HTTP_PARSE_RESULT_SUCCESS_END when the message body has been read as defined by the content length.
 *         This can be returned from a since call or after multiple calls.
 *         Otherwise an error condition.
 */
M_http_result_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len, size_t *len_read);


/*! Structure an http object into a message stubble for sending.
 *
 * Output is guaranteed to be NULL terminated but the contents
 * are not guaranteed to be free of NULL bytes.
 *
 * No encoding is performed on the data before writing. It is up
 * to the caller to have encoded the data before adding to the http
 * object. The caller also need to have previously set the proper
 * headers for length or chunking.
 *
 * In some cases it may be beneficial to use the http object to
 * create the headers and request line but write the body data
 * separately.
 *
 * When using chunked encoding the data must be set. Because
 * chunked data is used for multiple writes it is not necessary
 * to stream the data manually. Streaming multiple chunks is
 * already taking place. The chunked length will be determined
 * by the size of the chunked data within the object. Also, due
 * to chunked trailers it is not possible to stream chunked data.
 *
 * \param[in]  http HTTP object.
 * \param[out] len  Length of the generated data.
 *
 * \return Data.
 */
unsigned char *M_http_write(const M_http_t *http, size_t *len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/*! Has the start line been loaded.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool.
 */
M_bool M_http_start_line_complete(const M_http_t *http);


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
 * \return Dict.
 *
 * \see M_http_query_string
 */
const M_hash_dict_t *M_http_query_args(const M_http_t *http);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Have all headers been loaded.
 *
 * This will be set to M_TRUE once all headers have been
 * read. If streamed read this indicates whether there are
 * more headers that have not been parsed and any processing
 * using headers should wait.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool.
 */ 
M_bool M_http_headers_complete(const M_http_t *http);


/*! Set whether all headers have been loaded.
 *
 * \param[in] http     HTTP object.
 * \param[in] complete Whether loading is complete.
 */
void M_http_set_headers_complete(M_http_t *http, M_bool complete);


/*! Currently loaded headers.
 *
 * Does not included the "Set-Cookie" header which can be sent multiple
 * times with different attributes.
 *
 * This is a multi value dict to perceive the individual values when
 * the header is present multiple times.
 *
 * \param[in] http HTTP object.
 *
 * \return Dict.
 */
const M_hash_dict_t *M_http_headers(const M_http_t *http);


/*! Get all values for a header combined into a single
 *
 * Get the value of the header as a comma (,) separated list
 * if multiple values were specified.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 *
 * \return String.
 */
char *M_http_header(const M_http_t *http, const char key);


/*! Set the http headers.
 *
 * If the dict is multi value the values will be concatenated into a comma (,) separated list.
 *
 * \param[in] http    HTTP object.
 * \param[in] headers Headers.
 * \param[in] merge   Merge into or replace the existing headers.
 *
 * \see M_http_clear_headers
 */
void M_http_set_headers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge);


/*! Set a single http header.
 *
 * Replacing any existing values.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_set_header(M_http_t *http, const char *key, const char *val);


/*! Add a value to headers preserving existing values.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_add_header(M_http_t *http, const char *key, const char *val);


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


/*! Is upgrading to http 2 requested.
 *
 * This can only be present when the version if 1.1.
 * This can be ignored if http 2 is not being supported.
 *
 * This reads the Upgrade header and is a convince function.
 *
 * \param[in]  http             HTTP object.
 * \param[out] secure           Whether upgrading to TLS is requested.
 * \param[out] settings_payload Payload of upgrade settings.
 *
 * \return M_TRUE if upgrade is requested. Otherwise, M_FALSE.
 */
M_bool M_http_want_upgrade(const M_http_t *http, M_bool *secure, const char **settings_payload);


/*! Set whether upgrade should be requested.
 *
 * Only valid when version is set to 1.1.
 * Can be ignored by the client.
 *
 * Sets the Upgrade header. Will overwrite the existing header data if already set.
 *
 * \param[in] http             HTTP object.
 * \param[in] want             Whether upgrade should be requested.
 * \param[in] secure           Whether a secure upgrade should be requested.
 * \param[in] settings_payload Payload of upgrade settings.
 */
void M_http_set_want_upgrade(M_http_t *http, M_bool want, M_bool secure, const char *settings_payload);


/*! Is keep alive connection type set to indicate the connection is persistent.
 *
 * Reads the Connection header to determine if "keep-alive" is requested.
 *
 * \param[in] http HTTP object.
 *
 * \return Whether the connection should remain open.
 */
M_bool M_http_persistent_conn(const M_http_t *http);


/*! Sets whether the connection should remain open for subsequent messages.
 *
 * \param[in] http    HTTP object.
 * \param[in] persist Whether to request a persistent connection.
 */
void M_http_set_persistent_conn(M_http_t *http, M_bool persist);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Has the body been fully loaded.
 *
 * The body could be cleared by the caller when using streamed reads so
 * the data might not all be in the HTTP object. This specifies that all
 * data has been read at one point.
 *
 * \note It's not always possible to know when all body data has
 *       been read. It is valid for a server to define the body
 *       length as all bytes sent before connection close.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool.
 */
M_bool M_http_body_complete(const M_http_t *http);


/*! Set whether the body has been fully loaded.
 *
 * This does not update or change the length headers.
 * It is up to the caller to set any length headers
 * because the body could be streamed outside of this
 * object.
 *
 * \param[in] http     HTTP object.
 * \param[in] complete Whether loading is complete.
 */
void M_http_set_body_complete(M_http_t *http, M_bool complete);


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


/*! Set the body data.
 *
 * Clears existing body data.
 *
 * \param[in] http HTTP object.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_set_body(M_http_t *http, const unsigned char *data, size_t len);


/*! Add to existing body data.
 *
 * \param[in] http HTTP object.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_body_append(M_http_t *http, const unsigned char *data, size_t len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Is this a chunked message.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool
 */
M_bool M_http_is_chunked(const M_http_t *http);


/*! Specify that this is a chunked message.
 *
 * \param[in] http    HTTP object.
 * \param[in] chunked Whether the message is chunked.
 */
void M_http_set_chunked(M_http_t *http, M_bool chunked);


/*! Has the chunk data been fully loaded.
 *
 * \param[in] http HTTP object.
 *
 * \return Bool.
 */
M_bool M_http_chunk_complete(const M_http_t *http);


/*! Set whether the chunk data has been fully loaded.
 *
 * \param[in] http     HTTP object.
 * \param[in] complete Whether loading is complete.
 */
void M_http_set_chunk_complete(M_http_t *http, M_bool complete);


/*! The length of the chunked response data.
 *
 * The length of chunked data within the object cannot
 * be set. For responses being generated the length will
 * be set on write.
 *
 * \note When the length is 0 this indicates it is the
 *       final chunk in the sequence and all data has
 *       been sent.
 *
 * \param[in] http HTTP object.
 *
 * \return Length.
 */
size_t M_http_chunk_len(const M_http_t *http);


/*! Get the chunk data.
 *
 * \param[in] http HTTP object.
 * \param[in] len  Length of data.
 *
 * \return Data.
 */
const unsigned char *M_http_chunk_data(const M_http_t *http, size_t *len);


/*! Set the chunked data.
 *
 * \param[in] http HTTP object.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_set_chunk_data(M_http_t *http, const unsigned char *data, size_t len);


/*! Add to existing chunked data.
 *
 * \param[in] http HTTP object.
 * \param[in] data Data.
 * \param[in] len  Length of data.
 */
void M_http_chunk_data_append(M_http_t *http, const unsigned char *data, size_t len);


/*! Get the chunk's trailing headers.
 *
 * This is a multi value dict to perceive the individual values when
 * the header is present multiple times. When a duplicate header is
 * encountered, the first value in the dict is a common (,) separated
 * list with all values. Multi indexes will be present after for each
 * individual element.
 *
 * Headers that only appear once will only have one entry.
 *
 * \param[in] http HTTP object.
 *
 * \return Dict.
 */
const M_hash_dict_t *M_http_chunk_trailers(const M_http_t *http);


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
char *M_http_chunk_trailer(const M_http_t *http, const char key);


/*! Set the chunk trailing headers.
 *
 * If the dict is multi value the values will be concatenated into a comma (,) separated list.
 *
 * \param[in] http    HTTP object.
 * \param[in] headers Headers.
 * \param[in] merge   Merge into or replace the existing headers.
 *
 * \see M_http_clear_chunk_trailer
 */
void M_http_set_chunk_trailers(M_http_t *http, const M_hash_dict_t *headers, M_bool merge);


/*! Set a single http trailer.
 *
 * Replacing any existing values.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_set_chunk_trailer(M_http_t *http, const char *key, const char *val);


/*! Add a value to headers preserving existing values.
 *
 * \param[in] http HTTP object.
 * \param[in] key  Header name.
 * \param[in] val  Value.
 */
void M_http_add_chunk_trailer(M_http_t *http, const char *key, const char *val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Is the result considered an error.
 *
 * \param[in] res Result
 *
 * \return M_TRUE if an error. Otherwise, M_FALSE.
 */
M_bool M_http_error_is_error(M_http_error_t res);


/*! Convert a version string into a version value.
 *
 * The version can start with "HTTP/" or without.
 *
 * \param[in] version Version string.
 *
 * \return version.
 */
M_http_version_t M_http_version_from_str(const char *version);


/*! Convert a method string into a method value.
 *
 * \param[in] method Method string.
 *
 * \return method.
 */
M_http_method_t M_http_method_from_str(const char *method);

/*! @} */

__END_DECLS

#endif /* __M_HTTP_H__ */
