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

typedef enum {
	M_HTTP_READ_STEP_UNKNONW = 0,
	M_HTTP_READ_STEP_START_LINE,
	M_HTTP_READ_STEP_START_LINE_DONE,
	M_HTTP_READ_STEP_HEADER,
	M_HTTP_READ_STEP_HEADER_DONE,
	M_HTTP_READ_STEP_BODY,
	M_HTTP_READ_STEP_BODY_DONE,

	M_HTTP_READ_STEP_CHUNK_START,
	M_HTTP_READ_STEP_CHUNK_START_DONE,
	M_HTTP_READ_STEP_CHUNK_DATA,
	M_HTTP_READ_STEP_CHUNK_DATA_DONE,
	M_HTTP_READ_STEP_TRAILER,
	M_HTTP_READ_STEP_TRAILER_DONE,

	M_HTTP_READ_STEP_DONE
} M_http_read_step_t;


typedef enum {
	M_HTTP_READ_NONE = 0,
	M_HTTP_READ_LEN_REQUIRED,
	M_HTTP_READ_STOP_ON_DONE
} M_http_read_flags_t;


/*! Result of parsing. */
typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,

	M_HTTP_ERROR_INVALIDUSE,
	M_HTTP_ERROR_STARTLINE_LENGTH, /* 414 (6k limit) */
	M_HTTP_ERROR_STARTLINE_MALFORMED, /* 400 */
	M_HTTP_ERROR_UNKNOWN_VERSION,
	M_HTTP_ERROR_REQUEST_METHOD, /* 501 */
	M_HTTP_ERROR_REQUEST_URI,
	M_HTTP_ERROR_HEADER_LENGTH, /* 413 (8k limit) */
	M_HTTP_ERROR_HEADER_NODATA,
	M_HTTP_ERROR_HEADER_FOLD, /* 400/502 */
	M_HTTP_ERROR_HEADER_NOTALLOWED,
	M_HTTP_ERROR_HEADER_INVLD,
	M_HTTP_ERROR_HEADER_MALFORMEDVAL, /* 400 */
	M_HTTP_ERROR_HEADER_DUPLICATE, /* 400 */
	M_HTTP_ERROR_LENGTH_REQUIRED, /* 411 */
	M_HTTP_ERROR_UPGRADE,
	M_HTTP_ERROR_CHUNK_LENGTH,
	M_HTTP_ERROR_CHUNK_EXTENSION,
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
} M_http_version_t;


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
 * stream. M_HTTP_PARSE_RESULT_SUCCESS_CHUNK_END will be returned when a
 * single chunk is fully processed. More calls to read are necessary starting
 * from the return read_len position in the data to continuing reading
 * the next chunks.
 *
 * \param[in]  http     HTTP object.
 * \param[in]  data     Raw data.
 * \param[in]  data_len Length of data.
 * \param[in]  flags    M_http_read_flags_t flags controlling read behavior.
 * \param[out] step     Read step currently being process.
 * \param[out] len_read Length of the data read. Can be less than data_len if the content length was reached.
 * 
 * \return M_HTTP_PARSE_RESULT_SUCCESS when a message has been read without error. There may be more data, check
 *         the complete functions and len_read to determine if another call is needed.
 *         M_HTTP_PARSE_RESULT_SUCCESS_END when the message body has been read as defined by the content length.
 *         This can be returned from a since call or after multiple calls.
 *         Otherwise an error condition.
 */
M_http_error_t M_http_read(M_http_t *http, const unsigned char *data, size_t data_len, M_uint32 flags, M_http_read_step_t *step, size_t *len_read);


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


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
