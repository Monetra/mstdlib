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
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_hash_multi.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_http HTTP
 *  \ingroup m_formats
 *
 * \warning IN PROGRESS AND API UNSTABLE
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,
	M_HTTP_ERROR_INVALIDUSE,
	M_HTTP_ERROR_STOP,
	M_HTTP_ERROR_SKIP,
	M_HTTP_ERROR_MOREDATA,
	M_HTTP_ERROR_LENGTH_REQUIRED, /* 411 */
	M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED,
	M_HTTP_ERROR_TRAILER_NOTALLOWED,
	M_HTTP_ERROR_URI, /* 400 */
	M_HTTP_ERROR_STARTLINE_LENGTH, /* 414 (6k limit) */
	M_HTTP_ERROR_STARTLINE_MALFORMED, /* 400 */
	M_HTTP_ERROR_UNKNOWN_VERSION,
	M_HTTP_ERROR_REQUEST_METHOD, /* 501 */
	M_HTTP_ERROR_REQUEST_URI,
	M_HTTP_ERROR_HEADER_LENGTH, /* 413 (8k limit) */
	M_HTTP_ERROR_HEADER_FOLD, /* 400/502 */
	M_HTTP_ERROR_HEADER_NOTALLOWED,
	M_HTTP_ERROR_HEADER_INVALID,
	M_HTTP_ERROR_HEADER_MALFORMEDVAL, /* 400 */
	M_HTTP_ERROR_HEADER_DUPLICATE, /* 400 */
	M_HTTP_ERROR_CHUNK_LENGTH,
	M_HTTP_ERROR_CHUNK_MALFORMED,
	M_HTTP_ERROR_CHUNK_EXTENSION,
	M_HTTP_ERROR_CHUNK_DATA_MALFORMED,
	M_HTTP_ERROR_MALFORMED,
	M_HTTP_ERROR_BODYLEN_REQUIRED,
	M_HTTP_ERROR_MULTIPART_NOBOUNDARY,
	M_HTTP_ERROR_MULTIPART_MISSING,
	M_HTTP_ERROR_MULTIPART_MISSING_DATA,
	M_HTTP_ERROR_MULTIPART_INVALID,
	M_HTTP_ERROR_UNSUPPORTED_DATA,
	M_HTTP_ERROR_TEXTCODEC_FAILURE,
	M_HTTP_ERROR_USER_FAILURE
} M_http_error_t;


/*! Message type. */
typedef enum {
	M_HTTP_MESSAGE_TYPE_UNKNOWN = 0,
	M_HTTP_MESSAGE_TYPE_REQUEST,
	M_HTTP_MESSAGE_TYPE_RESPONSE
} M_http_message_type_t;


/*! HTTP version in use. */
typedef enum {
	M_HTTP_VERSION_UNKNOWN = 0, /*!< Unknown. */
	M_HTTP_VERSION_1_0,         /*!< 1.0 */
	M_HTTP_VERSION_1_1,         /*!< 1.1 */
	M_HTTP_VERSION_2            /*!< 2 */
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


/*! HTTP Content type. */
typedef enum {
	M_HTTP_DATA_FORMAT_UNKNOWN = 0, /*! Could not determine the format of the data. */
	M_HTTP_DATA_FORMAT_NONE,        /*!< There is no data, Content-Length = 0. */
	M_HTTP_DATA_FORMAT_BODY,        /*!< Body. */
	M_HTTP_DATA_FORMAT_CHUNKED,     /*!< Data is chunked. */
	M_HTTP_DATA_FORMAT_MULTIPART    /*!< Data is multipart. */
} M_http_data_format_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a version string into a version value.
 *
 * The version can start with "HTTP/" or without.
 *
 * \param[in] version Version string.
 *
 * \return version.
 */
M_API M_http_version_t M_http_version_from_str(const char *version);


/*! Convert an http version to a string.
 *
 * Returns in the format "HTTP/#".
 *
 * \param[in] version Version.
 *
 * \return String.
 */
M_API const char *M_http_version_to_str(M_http_version_t version);


/*! Convert a method string into a method value.
 *
 * \param[in] method Method string.
 *
 * \return method.
 */
M_API M_http_method_t M_http_method_from_str(const char *method);


/*! Convert an http method to a string.
 *
 * \param[in] method Method.
 *
 * \return String.
 */
M_API const char *M_http_method_to_str(M_http_method_t method);


/*! Convert an http code to a string.
 *
 * Not all codes can be converted to a string.
 * Codes taht cannot be converted will return "Generic".
 *
 * \param[in] code Code.
 *
 * \return String.
 */
M_API const char *M_http_code_to_reason(M_uint32 code);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_http_reader HTTP Stream Reader
 *  \ingroup m_http
 *
 * @{
 */

struct M_http_reader;
typedef struct M_http_reader M_http_reader_t;

/*! Function definition for the start line.
 *
 * \param[in] type    Type of message.
 * \param[in] version HTTP version.
 * \param[in] method  If request, method of request.
 * \param[in] uri     If request, uri requested.
 * \param[in] code    If response, numeric response code.
 * \param[in] reason  If response, response reason.
 * \param[in] thunk   Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_start_func)(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk);

/*! Function definition for reading headers.
 *
 * Headers are split if a header list. Keys will appear multiple times if values were
 * in a list or if the header appears multiple times. Values with semicolon (;) separated
 * parameters are not split.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_header_func)(const char *key, const char *val, void *thunk);

/*! Function definition for header parsing completion.
 *
 * \param[in] format The format data was sent using.
 * \param[in] thunk  Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_header_done_func)(M_http_data_format_t format, void *thunk);

/*! Function definition for reading body data.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_body_func)(const unsigned char *data, size_t len, void *thunk);

/*! Function definition for completion of body parsing.
 *
 * This will only be called if the Content-Length header was specified.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_body_done_func)(void *thunk);

/*! Function definition for reading chunk extensions.
 *
 * Extensions are not required to have values.
 *
 * \param[in] key   Key.
 * \param[in] val   Value.
 * \param[in] idx   Chunk number the extension belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_chunk_extensions_func)(const char *key, const char *val, size_t idx, void *thunk);

/*! Function definition for completion of chunk extension parsing.
 *
 * Will only be called if there were chunk extensions.
 *
 * \param[in] idx   Chunk number that had extensions.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_chunk_extensions_done_func)(size_t idx, void *thunk);

/*! Function definition for reading chunk data.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] idx   Chunk number the data belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_chunk_data_func)(const unsigned char *data, size_t len, size_t idx, void *thunk);

/*! Function definition for completion of chunk data.
 *
 * \param[in] idx   Chunk number that has been completely processed.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_chunk_data_done_func)(size_t idx, void *thunk);

/*! Function definition for completion of parsing all chunks.
 *
 * Only called when data is chunked.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_chunk_data_finished_func)(void *thunk);

/*! Function definition for reading multipart preamble.
 *
 * Typically the preamble should be ignored if present.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_preamble_func)(const unsigned char *data, size_t len, void *thunk);

/*! Function definition for completion of multipart preamble parsing.
 *
 * Only called if a preamble was present.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_preamble_done_func)(void *thunk);

/*! Function definition for reading multipart part headers.
 *
 * \param[in] key   Key.
 * \param[in] val   Value.
 * \param[in] idx   Part number the header belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_header_func)(const char *key, const char *val, size_t idx, void *thunk);

/*! Function definition for completion of multipart part header parsing.
 *
 * \param[in] idx   Part number.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_header_done_func)(size_t idx, void *thunk);

/*! Function definition for reading multipart part data.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] idx   Partnumber the data belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_data_func)(const unsigned char *data, size_t len, size_t idx, void *thunk);

/*! Function definition for completion of multipart part data.
 *
 * \param[in] idx   Chunk number that has been completely processed.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_data_done_func)(size_t idx, void *thunk);

/*! Function definition for completion of parsing all multipart parts.
 *
 * Only called when data is chunked.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_data_finished_func)(void *thunk);

/*! Function definition for reading multipart epilogue.
 *
 * Typically the epilogue should be ignored if present.
 *
 * \param[in] data  Data.
 * \param[in] len   Length of data.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_epilouge_func)(const unsigned char *data, size_t len, void *thunk);

/*! Function definition for completion of multipart epilogue parsing.
 *
 * Only called if a epilogue was present.
 *
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_multipart_epilouge_done_func)(void *thunk);

/*! Function definition for reading trailing headers.
 *
 * Headers are split if a header list. Keys will appear multiple times if values were
 * in a list or if the header appears multiple times. Values with semicolon (;) separated
 * parameters are not split.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_trailer_func)(const char *key, const char *val, void *thunk);

/*! Function definition for trailing header parsing completion.
 *
 * Only called if trailing headers were present.
 *
 * \param[in] thunk  Thunk.
 *
 * \return Result
 */
typedef M_http_error_t (*M_http_reader_trailer_done_func)(void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Flags controlling reader behavior. */
typedef enum {
	M_HTTP_READER_NONE = 0,  /*!< Default operation. */
	M_HTTP_READER_SKIP_START /*!< Skip parsing start line. Data starts with headers. */
} M_http_reader_flags_t;


/*! Callbacks for various stages of parsing. */
struct M_http_reader_callbacks {
	M_http_reader_start_func                   start_func;
	M_http_reader_header_func                  header_func;
	M_http_reader_header_done_func             header_done_func;
	M_http_reader_body_func                    body_func;
	M_http_reader_body_done_func               body_done_func;
	M_http_reader_chunk_extensions_func        chunk_extensions_func;
	M_http_reader_chunk_extensions_done_func   chunk_extensions_done_func;
	M_http_reader_chunk_data_func              chunk_data_func;
	M_http_reader_chunk_data_done_func         chunk_data_done_func;
	M_http_reader_chunk_data_finished_func     chunk_data_finished_func;
	M_http_reader_multipart_preamble_func      multipart_preamble_func;
	M_http_reader_multipart_preamble_done_func multipart_preamble_done_func;
	M_http_reader_multipart_header_func        multipart_header_func;
	M_http_reader_multipart_header_done_func   multipart_header_done_func;
	M_http_reader_multipart_data_func          multipart_data_func;
	M_http_reader_multipart_data_done_func     multipart_data_done_func;
	M_http_reader_multipart_data_finished_func multipart_data_finished_func;
	M_http_reader_multipart_epilouge_func      multipart_epilouge_func;
	M_http_reader_multipart_epilouge_done_func multipart_epilouge_done_func;
	M_http_reader_trailer_func                 trailer_func;
	M_http_reader_trailer_done_func            trailer_done_func;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an http reader object.
 *
 * \param[in] cbs   Callbacks for processing.
 * \param[in] flags Flags controlling behavior.
 * \param[in] thunk Thunk passed to callbacks.
 *
 * \return Object.
 */
M_API M_http_reader_t *M_http_reader_create(struct M_http_reader_callbacks *cbs, M_uint32 flags, void *thunk);


/*! Destroy an http object.
 *
 * \param[in] httpr Http reader object.
 */
M_API void M_http_reader_destroy(M_http_reader_t *httpr);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse http data.
 *
 * \param[in]  httpr    Http reader object.
 * \param[in]  data     Data to parser.
 * \param[in]  data_len Length of data.
 * \param[out] len_read How much data was read.
 *
 * \return Result.
 */
M_API M_http_error_t M_http_reader_read(M_http_reader_t *httpr, const unsigned char *data, size_t data_len, size_t *len_read);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_http_simple HTTP Simple
 *  \ingroup m_http
 *
 * @{
 */

struct M_http_simple;
typedef struct M_http_simple M_http_simple_t;


typedef enum {
	M_HTTP_SIMPLE_READ_NONE = 0,
	M_HTTP_SIMPLE_READ_NODECODE_BODY,  /*!< Do not attempt to decode the body data (form or charset). */
	M_HTTP_SIMPLE_READ_LEN_REQUIRED,   /*!< Require content-length, cannot be chunked data. */
	M_HTTP_SIMPLE_READ_FAIL_EXTENSION, /*!< Fail if chunked extensions are specified. Otherwise, Ignore. */
	M_HTTP_SIMPLE_READ_FAIL_TRAILERS   /*!< Fail if tailers sent. Otherwise, they are ignored. */
} M_http_simple_read_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse the given HTTP message, store results in a new M_http_simple_t object.
 *
 * \see M_http_simple_destroy
 *
 * \param[out] simple   pointer to new M_http_simple_t object will be stored here
 * \param[in]  data     buffer containing HTTP message to read
 * \param[in]  data_len length of \a data buffer
 * \param[in]  flags    read options (OR'd combo of M_http_simple_read_flags)
 * \param[in]  len_read num bytes consumed from \a data (may be NULL, if caller doesn't need this info)
 * \return              error code (M_HTTP_ERROR_SUCCESS if successful)
 */
M_API M_http_error_t M_http_simple_read(M_http_simple_t **simple, const unsigned char *data, size_t data_len,
	M_uint32 flags, size_t *len_read);


/*! Destroy the given M_http_simple_t object.
 *
 * \param[in] http object to destroy
 */
M_API void M_http_simple_destroy(M_http_simple_t *http);


/*! Return the type of the parsed message.
 *
 * \param[in] simple parsed HTTP message
 * \return           type of message (REQUEST or RESPONSE, usually)
 */
M_API M_http_message_type_t M_http_simple_message_type(const M_http_simple_t *simple);


/*! Return the HTTP protocol version of the parsed message.
 *
 * \param[in] simple parsed HTTP message
 * \return           HTTP protocol version (1.1, 2.0, etc)
 */
M_API M_http_version_t M_http_simple_version(const M_http_simple_t *simple);


/*! Return the HTTP status code of the parsed message.
 *
 * The status code is only set for response messages (type == M_HTTP_MESSAGE_TYPE_RESPONSE).
 * If the parsed message wasn't a response, the returned status code will be 0.
 *
 * \see M_http_simple_reason_phrase
 *
 * \param[in] simple parsed HTTP message
 * \return           HTTP status code (200, 404, etc.), or 0 if this isn't a response
 */
M_API M_uint32 M_http_simple_status_code(const M_http_simple_t *simple);


/*! Return the human-readable status of the parsed message.
 *
 * This is the text that goes with the HTTP status code in the message.
 *
 * The reason phrase is only set for response messages (type == M_HTTP_MESSAGE_TYPE_RESPONSE).
 * If the parsed message wasn't a response, the returned string will be \c NULL.
 *
 * \see M_http_simple_status_code
 *
 * \param[in] simple parsed HTTP message
 * \return           string describing reason for message's status code, or \c NULL if this isn't a response
 */
M_API const char *M_http_simple_reason_phrase(const M_http_simple_t *simple);


/*! Return the HTTP method (GET, POST, etc) of the parsed message.
 *
 * The method is only set for request messages (type == M_HTTP_MESSAGE_TYPE_REQUEST).
 * If the parsed message wasn't a request, M_HTTP_METHOD_UNKNOWN will be returned.
 *
 * \param[in] simple parsed HTTP message
 * \return           HTTP verb used by the parsed message, or M_HTTP_METHOD_UNKNOWN if this isn't a request
 */
M_API M_http_method_t M_http_simple_method(const M_http_simple_t *simple);


/*! Return the full URI (port, path and query) of the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * returned string will be \c NULL.
 *
 * \see M_http_simple_port
 * \see M_http_simple_path
 * \see M_http_simple_query_string
 * \see M_http_simple_query_args
 *
 * \param[in] simple parsed HTTP message
 * \return           URI of the parsed message, or \c NULL if this isn't a request
 */
M_API const char *M_http_simple_uri(const M_http_simple_t *simple);


/*! Return the port number component of the URI from the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return M_FALSE and set \a port to 0.
 *
 * The port may not be present - even absolute URI's don't have to include the port.
 *
 * \see M_http_simple_uri
 *
 * \param[in]  simple parsed HTTP message
 * \param[out] port   place to store port number. May be \c NULL, if you're just checking to see if a port is present
 * \return            M_TRUE if a port was set, M_FALSE if there was no port in the message
 */
M_API M_bool M_http_simple_port(const M_http_simple_t *simple, M_uint16 *port);


/*! Return the path component of the URI from the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return \c NULL.
 *
 * The path may be relative or absolute.
 *
 * \see M_http_simple_uri
 *
 * \param[in] simple parsed HTTP message
 * \return           path part of URI, or \c NULL if this isn't a request
 */
M_API const char *M_http_simple_path(const M_http_simple_t *simple);


/*! Return the query component of the URI from the parsed message.
 *
 * The returned query string hasn't been processed in any way. Call M_http_simple_query_args()
 * instead to process the query and return its contents as a set of key-value pairs.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return \c NULL.
 *
 * Not all requests have a query string embedded in the URI. This is normally seen
 * in GET requests, but it's not always present even there.
 *
 * \see M_http_simple_query_args
 * \see M_http_simple_uri
 *
 * \param[in] simple parsed HTTP message
 * \return           query string from URI, or \c NULL if not present
 */
M_API const char *M_http_simple_query_string(const M_http_simple_t *simple);


/*! Parse arguments from query component of URI as key-value pairs.
 *
 * Processes the query string (if any), then returns a key->value mapping of all
 * the values present in the string.
 *
 * \warning
 * Any keys in the query string that don't have values (no '='), or whose values
 * are empty ('key=') will not be present in the returned mapping. To parse empty
 * keys, you have to process the query string returned by M_http_simple_query_string()
 * yourself.
 *
 * \see M_http_simple_query_string
 * \see M_http_simple_uri
 *
 * \param[in] simple parsed HTTP message
 * \return           dictionary containing key-value mappings from query string, or \c NULL if there were none
 */
M_API const M_hash_dict_t *M_http_simple_query_args(const M_http_simple_t *simple);


/*! Get headers from parsed message as key-multivalue pairs.
 *
 * Note that some headers may contain a list of multiple values, so the returned
 * M_hash_dict_t is a multimap (one key may map to a list of values).
 *
 * Header names are not case-sensitive, when doing lookups into the returned dictionary.
 *
 * \warning
 * The returned dictionary does not include "Set-Cookie" headers, because they can
 * be sent multiple times with different attributes, and their values cannot be
 * merged into a list.
 *
 * \see M_http_simple_header
 * \see M_http_simple_get_set_cookie
 *
 * \param[in] simple parsed HTTP message
 * \return           multimap of header names and values
 */
M_API const M_hash_dict_t *M_http_simple_headers(const M_http_simple_t *simple);


/*! Get value of the named header from the parsed message.
 *
 * The key is not case-sensitive - it will match header names that only differ because
 * of capitalization.
 *
 * Note that some headers may contain a list of multiple values. For these headers,
 * this function will return a comma-delimited list of values. Some extra whitespace
 * may be added in addition to the commas.
 *
 * \warning
 * Attempts to retrieve "Set-Cookie" header values with this function will fail, because
 * those headers may be sent multiple times with different attributes, and their values
 * cannot be merged into a list.
 *
 * \see M_http_simple_headers
 * \see M_http_simple_get_set_cookie
 *
 * \param[in] simple parsed HTTP message
 * \param[in] key    name of header to retrieve values from
 * \return           comma-delimited list of values for this header, or \c NULL if no data found
 */
M_API char *M_http_simple_header(const M_http_simple_t *simple, const char *key);


/*! Return list of values from all Set-Cookie headers in the parsed message.
 *
 * The returned list of values is stable-sorted alphabetically.
 *
 * \param[in] simple parsed HTTP message
 * \return           sorted list of all cookies in the message (may be empty)
 */
M_API const M_list_str_t *M_http_simple_get_set_cookie(const M_http_simple_t *simple);


/*! Return the body of the parsed message (if any).
 *
 * \param[in]  simple parsed HTTP message
 * \param[out] len    place to store length of body (may be \c NULL)
 * \return            bytes from body of message
 */
M_API const unsigned char *M_http_simple_body(const M_http_simple_t *simple, size_t *len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an HTTP request message.
 *
 * Caller is responsible for freeing the returned string.
 *
 * \param[in]  method   HTTP verb to use (GET, POST, etc)
 * \param[in]  uri      full URI (may be absolute or relative, may include query string)
 * \param[in]  version  HTTP protocol version to use (1.1, 2.0, etc)
 * \param[in]  headers  headers to include in request
 * \param[in]  data     string to place in body of message (may be empty)
 * \param[in]  data_len number of chars to use from \c data (may be 0)
 * \param[out] len      place to store length of returned HTTP request message (may be NULL)
 * \return              allocated string containing HTTP request message
 */
M_API unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);


/*! Create an HTTP response message.
 *
 * Caller is responsible for freeing the returned string.
 *
 * \param version  HTTP protocol version to use (1.1, 2.0, etc)
 * \param code     HTTP status code to use (200, 404, etc)
 * \param reason   HTTP status reason string to use ("OK", "Not Found", etc)
 * \param headers  headers to include in response
 * \param data     string to place in body of message (may be empty)
 * \param data_len number of chars to use from \c data (may be 0)
 * \param len      place to store length of returned HTTP response message (may be NULL)
 * \return         new string containing HTTP response message
 */
M_API unsigned char *M_http_simple_write_response(M_http_version_t version, M_uint32 code, const char *reason,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);

/*
M_API unsigned char *M_http_simple_write_request_multipart(M_http_method_t method, const char *uri,
	M_http_version_t version, const M_hash_dict_t *headers, M_http_multipart_t *parts, size_t *len);

M_API unsigned char *M_http_simple_write_response_multipart(M_http_version_t version, M_uint32 code,
	const char *reason, const M_hash_dict_t *headers, M_http_multipart_t *parts, size_t *len);
*/

/*! @} */

__END_DECLS

#endif /* __M_HTTP_H__ */
