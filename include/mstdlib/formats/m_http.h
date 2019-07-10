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

#ifndef __M_HTTP_H__
#define __M_HTTP_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_hash_multi.h>
#include <mstdlib/base/m_parser.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_http HTTP
 *  \ingroup m_formats
 *
 * HTTP 1.0/1.1 message reading and writing.
 *
 * Conforms to:
 *
 * - RFC 7230 Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
 * - RFC 7231 Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content
 *
 * There are two types of message parsing supported.
 * - Stream based callback
 * - Simple reader (memory buffered)
 *
 * Currently supported Read:
 * - Callback
 * - Simple
 *
 * Currently support Write:
 * - Simple (simple can generate head only and data can be sent separately)
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,                /*!< Success. */
	M_HTTP_ERROR_INVALIDUSE,                 /*!< Invalid use. */
	M_HTTP_ERROR_STOP,                       /*!< Stop processing (Used by callback functions to indicate non-error but stop processing). */
	M_HTTP_ERROR_MOREDATA,                   /*!< Incomplete message, more data required. */
	M_HTTP_ERROR_LENGTH_REQUIRED,            /*!< Content-Length is required but not provided. 411 code. */
	M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED, /*!< Chunk extensions are present but not allowed. */
	M_HTTP_ERROR_TRAILER_NOTALLOWED,         /*!< Chunk trailer present but not allowed. */
	M_HTTP_ERROR_URI,                        /*!< Invalid URI. 400 code. */
	M_HTTP_ERROR_STARTLINE_LENGTH,           /*!< Start line exceed maximum length (6k limit). 414 code. */
	M_HTTP_ERROR_STARTLINE_MALFORMED,        /*!< Start line is malformed. 400 code. */
	M_HTTP_ERROR_UNKNOWN_VERSION,            /*!< Unknown or unsupported HTTP version. */
	M_HTTP_ERROR_REQUEST_METHOD,             /*!< Invalid request method. 501 code. */
	M_HTTP_ERROR_HEADER_LENGTH,              /*!< Header exceeds maximum length (8k limit). 413 code. */
	M_HTTP_ERROR_HEADER_FOLD,                /*!< Header folded. Folding is deprecated and should not be used. 400/502 code. */
	M_HTTP_ERROR_HEADER_INVALID,             /*!< Header is malformed. 400 code. */
	M_HTTP_ERROR_HEADER_DUPLICATE,           /*!< Duplicate header present. 400 code. */
	M_HTTP_ERROR_CHUNK_STARTLINE_LENGTH,     /*!< Chunk start line exceed maximum length (6k limit). 414 code. */
	M_HTTP_ERROR_CHUNK_LENGTH,               /*!< Failed to parse chunk length. */
	M_HTTP_ERROR_CHUNK_MALFORMED,            /*!< Chunk is malformed. */ 
	M_HTTP_ERROR_CHUNK_EXTENSION,            /*!< Chunk extensions present but malformed. */
	M_HTTP_ERROR_CHUNK_DATA_MALFORMED,       /*!< Chunk data malformed. */
	M_HTTP_ERROR_CONTENT_LENGTH_MALFORMED,   /*!< Content-Length present but malformed. */
	M_HTTP_ERROR_NOT_HTTP,                   /*!< Not an HTTP message. */
	M_HTTP_ERROR_MULTIPART_NOBOUNDARY,       /*!< Multipart message missing boundary. */
	M_HTTP_ERROR_MULTIPART_MISSING,          /*!< Multipart message but multipart missing. */
	M_HTTP_ERROR_MULTIPART_MISSING_DATA,     /*!< Multipart data missing. */
	M_HTTP_ERROR_MULTIPART_INVALID,          /*!< Multipart is invalid. */
	M_HTTP_ERROR_UNSUPPORTED_DATA,           /*!< Data received is unsupported. */
	M_HTTP_ERROR_TEXTCODEC_FAILURE,          /*!< Text decode failure. */
	M_HTTP_ERROR_USER_FAILURE                /*!< Generic callback generated failure. */
} M_http_error_t;


/*! Message type. */
typedef enum {
	M_HTTP_MESSAGE_TYPE_UNKNOWN = 0, /*!< Unknown message type. */
	M_HTTP_MESSAGE_TYPE_REQUEST,     /*!< Request message. */
	M_HTTP_MESSAGE_TYPE_RESPONSE     /*!< Response message. */
} M_http_message_type_t;


/*! HTTP version in use. */
typedef enum {
	M_HTTP_VERSION_UNKNOWN = 0, /*!< Unknown. */
	M_HTTP_VERSION_1_0,         /*!< 1.0 */
	M_HTTP_VERSION_1_1,         /*!< 1.1 */
} M_http_version_t;


/*! HTTP methods. */
typedef enum {
	M_HTTP_METHOD_UNKNOWN = 0, /*!< Unknown method (null value). */
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
 * \return Version.
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
 * \return Method.
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
 * Codes that cannot be converted will return "Generic".
 *
 * \param[in] code Code.
 *
 * \return String.
 */
M_API const char *M_http_code_to_reason(M_uint32 code);


/*! Convert an http error code to a string.
 *
 * \param[in] err Error code.
 *
 * \return Name of error code (not a description, just the enum name, like M_HTTP_ERROR_SUCCESS).
 */
M_API const char *M_http_errcode_to_str(M_http_error_t err);


/*! Create query string, append to given URI, return as new string.
 *
 * Empty values are not permitted - keys whose values are set to the empty string will be left out of
 * the query string.
 *
 * Web applications use two slightly-different URL encodings for query strings, one that encodes spaces
 * as '%20' (\link M_TEXTCODEC_PERCENT_URL M_TEXTCODEC_PERCENT_URL\endlink), and one that encodes spaces
 * as '+' (\link M_TEXTCODEC_PERCENT_URLPLUS M_TEXTCODEC_PERCENT_URLPLUS\endlink).  Web apps are about
 * evenly split between these two options, so the caller must pick which one to use based on their own
 * needs, by setting the \a use_plus parameter.
 *
 * \see M_http_generate_query_string_buf()
 *
 * \param[in] uri      Uri string (e.g., /cgi-bin/payment/start, or %http://google.com/whatever).
 * \param[in] params   Key-value pairs to encode in query string.
 * \param[in] use_plus If M_TRUE, sub in '+' for space character. Otherwise, use '%20'.
 *
 * \return New string with URI + query string, or \c NULL if there was an encoding error.
 */
M_API char *M_http_generate_query_string(const char *uri, M_hash_dict_t *params, M_bool use_plus);


/*! Create query string, append URI + query string to buffer.
 *
 * Empty values are not permitted - keys whose values are set to the empty string will be left out of
 * the query string.
 *
 * Web applications use two slightly-different URL encodings for query strings, one that encodes spaces
 * as '%20' (\link M_TEXTCODEC_PERCENT_URL M_TEXTCODEC_PERCENT_URL\endlink), and one that encodes spaces
 * as '+' (\link M_TEXTCODEC_PERCENT_URLPLUS M_TEXTCODEC_PERCENT_URLPLUS\endlink).  Web apps are about
 * evenly split between these two options, so the caller must pick which one to use based on their own
 * needs, by setting the \a use_plus parameter.
 *
 * \see M_http_generate_query_string()
 *
 * \param[out] buf      Buffer to add URI + query string to, contents remain unchanged if there was an error.
 * \param[in]  uri      Uri string (e.g., /cgi-bin/payment/start, or %http://google.com/whatever).
 * \param[in]  params   Key-value pairs to encode in query string.
 * \param[in]  use_plus If M_TRUE sub in '+' for space character, otherwise will use '%20'.
 *
 * \return M_TRUE if successful, or \c M_FALSE if there was an encoding error.
 */
M_API M_bool M_http_generate_query_string_buf(M_buf_t *buf, const char *uri, M_hash_dict_t *params, M_bool use_plus);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_http_reader HTTP Stream Reader
 *  \ingroup m_http
 *
 * Stream reader used for parsing using callbacks.
 * Very useful for large HTTP messages.
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

/*! Parse next http message from given array.
 *
 * \param[in]  httpr    Http reader object.
 * \param[in]  data     Data to parse.
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
 * Buffered reader and writer.
 *
 * @{
 */

/*! \addtogroup m_http_simple_write HTTP Simple Reader
 *  \ingroup m_http_simple
 *
 * Reads a full HTTP message. Useful for small messages.
 * Alls all data is contained within on object for
 * easy processing.
 *
 * @{
 */

struct M_http_simple_read;
typedef struct M_http_simple_read M_http_simple_read_t;


typedef enum {
	M_HTTP_SIMPLE_READ_NONE = 0,
	M_HTTP_SIMPLE_READ_NODECODE_BODY,  /*!< Do not attempt to decode the body data (form detected charset). */
	M_HTTP_SIMPLE_READ_LEN_REQUIRED,   /*!< Require content-length, cannot be chunked data. */
	M_HTTP_SIMPLE_READ_FAIL_EXTENSION, /*!< Fail if chunked extensions are specified. Otherwise, Ignore. */
	M_HTTP_SIMPLE_READ_FAIL_TRAILERS   /*!< Fail if trailers sent. Otherwise, they are ignored. */
} M_http_simple_read_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read the next HTTP message from the given buffer, store results in a new M_http_simple_read_t object.
 *
 * \param[out] simple   Place to store new M_http_simple_read_t object.
 * \param[in]  data     Buffer containing HTTP messages to read.
 * \param[in]  data_len Length of \a data.
 * \param[in]  flags    Read options (OR'd combo of M_http_simple_read_flags_t).
 * \param[in]  len_read Num bytes consumed from \a data (may be NULL, if caller doesn't need this info).
 *
 * \return Error code (M_HTTP_ERROR_SUCCESS if successful).
 *
 * \see M_http_simple_read_parser
 * \see M_http_simple_read_destroy
 */
M_API M_http_error_t M_http_simple_read(M_http_simple_read_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags, size_t *len_read);


/*! Read the next HTTP message from the given parser.
 *
 * Will return M_HTTP_ERROR_MOREDATA if we need to wait for more data to get a complete message.
 * No data will be dropped from the parser, in this case.
 *
 * \see M_http_simple_read
 * \see M_http_simple_destroy
 *
 * \param[out] simple Place to store new M_http_simple_read_t object.
 * \param[in]  parser Buffer containing HTTP messages to read.
 * \param[in]  flags  Read options (OR'd combo of M_http_simple_read_flags_t).
 *
 * \return Error code (M_HTTP_ERROR_SUCCESS if successful).
 */
M_API M_http_error_t M_http_simple_read_parser(M_http_simple_read_t **simple, M_parser_t *parser, M_uint32 flags);


/*! Destroy the given M_http_simple_read_t object.
 *
 * \param[in] http object to destroy
 *
 * \see M_http_simple_read
 * \see M_http_simple_read_parser
 */
M_API void M_http_simple_read_destroy(M_http_simple_read_t *http);


/*! Return the type of the parsed message.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Type of message (REQUEST or RESPONSE, usually).
 */
M_API M_http_message_type_t M_http_simple_read_message_type(const M_http_simple_read_t *simple);


/*! Return the HTTP protocol version of the parsed message.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return HTTP protocol version (1.0, 1.1).
 */
M_API M_http_version_t M_http_simple_read_version(const M_http_simple_read_t *simple);


/*! Return the HTTP status code of the parsed message.
 *
 * The status code is only set for response messages (type == M_HTTP_MESSAGE_TYPE_RESPONSE).
 * If the parsed message wasn't a response, the returned status code will be 0.
 *
 * \see M_http_simple_read_reason_phrase
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return HTTP status code (200, 404, etc.), or 0 if this isn't a response.
 */
M_API M_uint32 M_http_simple_read_status_code(const M_http_simple_read_t *simple);


/*! Return the human-readable status of the parsed message.
 *
 * This is the text that goes with the HTTP status code in the message.
 *
 * The reason phrase is only set for response messages (type == M_HTTP_MESSAGE_TYPE_RESPONSE).
 * If the parsed message wasn't a response, the returned string will be \c NULL.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return String describing reason for message's status code, or \c NULL if this isn't a response.
 *
 * \see M_http_simple_read_status_code
 */
M_API const char *M_http_simple_read_reason_phrase(const M_http_simple_read_t *simple);


/*! Return the HTTP method (GET, POST, etc) of the parsed message.
 *
 * The method is only set for request messages (type == M_HTTP_MESSAGE_TYPE_REQUEST).
 * If the parsed message wasn't a request, M_HTTP_METHOD_UNKNOWN will be returned.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return HTTP verb used by the parsed message, or M_HTTP_METHOD_UNKNOWN if this isn't a request.
 */
M_API M_http_method_t M_http_simple_read_method(const M_http_simple_read_t *simple);


/*! Return the full URI (port, path and query) of the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * returned string will be \c NULL.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return URI of the parsed message, or \c NULL if this isn't a request.
 *
 * \see M_http_simple_read_port
 * \see M_http_simple_read_path
 * \see M_http_simple_read_query_string
 * \see M_http_simple_read_query_args
 */
M_API const char *M_http_simple_read_uri(const M_http_simple_read_t *simple);


/*! Return the port number component of the URI from the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return M_FALSE and set \a port to 0.
 *
 * The port may not be present - even absolute URI's don't have to include the port.
 *
 * \param[in]  simple Parsed HTTP message.
 * \param[out] port   Place to store port number. May be \c NULL, if you're just checking to see if a port is present.
 *
 * \return M_TRUE if a port was set, M_FALSE if there was no port in the message.
 *
 * \see M_http_simple_read_uri
 */
M_API M_bool M_http_simple_read_port(const M_http_simple_read_t *simple, M_uint16 *port);


/*! Return the path component of the URI from the parsed message.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return \c NULL.
 *
 * The path may be relative or absolute.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Path part of URI, or \c NULL if this isn't a request.
 *
 * \see M_http_simple_read_uri
 */
M_API const char *M_http_simple_read_path(const M_http_simple_read_t *simple);


/*! Return the query component of the URI from the parsed message.
 *
 * The returned query string hasn't been processed in any way. Call M_http_simple_read_query_args()
 * instead to process the query and return its contents as a set of key-value pairs.
 *
 * Only request messages have a URI. If the parsed message wasn't a request, the
 * function will return \c NULL.
 *
 * Not all requests have a query string embedded in the URI. This is normally seen
 * in GET requests, but it's not always present even there.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Query string from URI, or \c NULL if not present.
 *
 * \see M_http_simple_read_query_args
 * \see M_http_simple_read_uri
 */
M_API const char *M_http_simple_read_query_string(const M_http_simple_read_t *simple);


/*! Parse arguments from query component of URI as key-value pairs.
 *
 * Processes the query string (if any), then returns a key->value mapping of all
 * the values present in the string.
 *
 * \warning
 * Any keys in the query string that don't have values (no '='), or whose values
 * are empty ('key=') will not be present in the returned mapping. To parse empty
 * keys, you have to process the query string returned by M_http_simple_read_query_string()
 * yourself.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Dictionary containing key-value mappings from query string, or \c NULL if there were none.
 *
 * \see M_http_simple_read_query_string
 * \see M_http_simple_read_uri
 */
M_API const M_hash_dict_t *M_http_simple_read_query_args(const M_http_simple_read_t *simple);


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
 * \param[in] simple Parsed HTTP message.
 *
 * \return Multimap of header names and values.
 *
 * \see M_http_simple_read_header
 * \see M_http_simple_read_get_set_cookie
 */
M_API const M_hash_dict_t *M_http_simple_read_headers(const M_http_simple_read_t *simple);


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
 * \param[in] simple Parsed HTTP message.
 * \param[in] key    Name of header to retrieve values from.
 *
 * \return Comma-delimited list of values for this header, or \c NULL if no data found.
 *
 * \see M_http_simple_read_headers
 * \see M_http_simple_read_get_set_cookie
 */
M_API char *M_http_simple_read_header(const M_http_simple_read_t *simple, const char *key);


/*! Return list of values from all Set-Cookie headers in the parsed message.
 *
 * \note
 * This does not set anything, it is an accessor to get the  "Set-Cookie"
 * header field. The header is called "Set-Cookie" and can be set
 * multiple times with different values.
 *
 * The returned list of values is stable-sorted alphabetically.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Sorted list of all cookies in the message (may be empty).
 *
 * \see M_http_simple_read_header
 * \see M_http_simple_read_headers
 */
M_API const M_list_str_t *M_http_simple_read_get_set_cookie(const M_http_simple_read_t *simple);


/*! Return the body of the parsed message (if any).
 *
 * \param[in]  simple Parsed HTTP message.
 * \param[out] len    Place to store length of body (may be \c NULL).
 *
 * \return Bytes from body of message.
 */
M_API const unsigned char *M_http_simple_read_body(const M_http_simple_read_t *simple, size_t *len);

/*! @} */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_http_simple_read HTTP Simple Writer
 *  \ingroup m_http_simple
 *
 * Generate request and response messages.
 *
 * Does not support:
 * - Multipart messages.
 * - Chunked data messages.
 *
 * @{
 */

/*! Create an HTTP request message, return as a new string.
 *
 * Caller is responsible for freeing the returned string.
 *
 * If the Content-Length header is not provided in \a headers, it will be added
 * automatically for you, using \a data_len as the length. When data will be
 * sent and Content-Length is also set data sent to this function is optional.
 * This allows generating the header and sending large messages without
 * buffering the data in memory. In this case this function will generate
 * the necessary HTTP header part of the message.
 *
 * \see M_http_simple_write_request_buf
 *
 * \param[in]  method   HTTP verb to use (GET, POST, etc).
 * \param[in]  uri      Full URI (may be absolute or relative, may include query string).
 * \param[in]  version  HTTP protocol version to use (1.0, 1.1, etc).
 * \param[in]  headers  Headers to include in request.
 * \param[in]  data     String to place in body of message (may be empty).
 * \param[in]  data_len Number of chars to use from \c data (may be 0).
 * \param[out] len      Place to store length of returned HTTP request message (may be NULL).
 *
 * \return Allocated string containing HTTP request message.
 */
M_API unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);


/*! Create an HTTP request message, add it to the given buffer.
 *
 * Same as M_http_simple_write_request(), except that it adds the new message to the given buffer instead
 * of returning it in a newly-allocated string.
 *
 * \param[out] buf      Buffer to add the message to.
 * \param[in]  method   HTTP verb to use (GET, POST, etc).
 * \param[in]  uri      Full URI (may be absolute or relative, may include query string).
 * \param[in]  version  HTTP protocol version to use (1.0, 1.1, etc).
 * \param[in]  headers  Headers to include in request.
 * \param[in]  data     String to place in body of message (may be empty).
 * \param[in]  data_len Number of chars to use from \c data for body (may be 0).
 *
 * \return M_TRUE if add was successful, M_FALSE if message creation failed.
 *
 * \see M_http_simple_write_request
 */
M_API M_bool M_http_simple_write_request_buf(M_buf_t *buf, M_http_method_t method, const char *uri,
	M_http_version_t version, const M_hash_dict_t *headers, const char *data, size_t data_len);


/*! Create an HTTP response message, return as new string.
 *
 * If the Content-Length header is not provided in \a headers, it will be added
 * automatically for you, using \a data_len as the length. When data will be
 * sent and Content-Length is also set data sent to this function is optional.
 * This allows generating the header and sending large messages without
 * buffering the data in memory. In this case this function will generate
 * the necessary HTTP header part of the message.
 *
 * Caller is responsible for freeing the returned string.
 *
 * \param[in]  version  HTTP protocol version to use (1.0, 1.1, etc).
 * \param[in]  code     HTTP status code to use (200, 404, etc).
 * \param[in]  reason   HTTP status reason string. If NULL, will attempt to pick one automatically.
 * \param[in]  headers  Headers to include in response.
 * \param[in]  data     String to place in body of message (may be empty).
 * \param[in]  data_len Number of chars to use from \c data (may be 0).
 * \param[out] len      Place to store length of returned HTTP response message (may be NULL).
 *
 * \return New string containing HTTP response message.
 *
 * \see M_http_simple_write_response_buf
 */
M_API unsigned char *M_http_simple_write_response(M_http_version_t version, M_uint32 code, const char *reason,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);


/*! Create an HTTP response message, add it to the given buffer.
 *
 * Same as M_http_simple_write_response(), except that it adds the new message to the given buffer instead
 * of returning it in a newly-allocated string.
 *
 * \param[out] buf      Buffer to add the message to.
 * \param[in]  version  HTTP protocol version to use (1.0, 1.1, etc).
 * \param[in]  code     HTTP status code to use (200, 404, etc).
 * \param[in]  reason   HTTP status reason string. If NULL, will attempt to pick one automatically.
 * \param[in]  headers  Headers to include in response.
 * \param[in]  data     String to place in body of message (may be empty).
 * \param[in]  data_len Number of chars to use from \c data (may be 0).
 *
 * \return M_TRUE if add was successful, M_FALSE if message creation failed.
 *
 * \see M_http_simple_write_response
 */
M_API M_bool M_http_simple_write_response_buf(M_buf_t *buf, M_http_version_t version, M_uint32 code,
	const char *reason, const M_hash_dict_t *headers, const char *data, size_t data_len);

/*! @} */

/*! @} */

__END_DECLS

#endif /* __M_HTTP_H__ */
