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
#include <mstdlib/text/m_textcodec.h>

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

/*! Error codes.
 *
 * M_HTTP_ERROR_SUCCESS, and M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE are both
 * Success conditions. All data is valid and has been parsed.
 *
 * M_HTTP_ERROR_MOREDATA indicates valid data and isn't always considered an
 * error condition. It typically indicates a retry once more data is received
 * condition. For example more headers could follow or the content length or
 * chucked data was not complete.  There is more data needed to complete the
 * message. It is only an error if end of data has been reached.
 *
 * M_HTTP_ERROR_STOP is not considered an error and means no more processing
 * will/should take place. A callback should generate this if all data the caller
 * wants has been processed if partial processing is taking place. For example,
 * a proxy looking for X-Forwarded-For header in order to blacklist an abusive
 * IP before forwarding the message.
 */
typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,                /*!< Success. Data fully parsed and all data is present. */
	M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE,      /*!< Success but more data possible. No content length was sent or chunking was used. The only way to know all data was received is by a disconnect. */
	M_HTTP_ERROR_MOREDATA,                   /*!< Incomplete message, more data required. Not necessarily an error if parsing as data is streaming. */
	M_HTTP_ERROR_STOP,                       /*!< Stop processing (Used by callback functions to indicate non-error but stop processing). */
	M_HTTP_ERROR_INVALIDUSE,                 /*!< Invalid use. */
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
	M_HTTP_METHOD_CONNECT,     /*!< Connect. */
	M_HTTP_METHOD_PATCH        /*!< Patch. */
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
 * Query string are defined as application/x-www-form-urlencoded data so url form encoding is
 * used.
 *
 * \see M_http_generate_query_string_buf()
 *
 * \param[in] uri    Uri string (e.g., /cgi-bin/payment/start, or %http://google.com/whatever).
 * \param[in] params Key-value pairs to encode in query string.
 *
 * \return New string with URI + query string, or \c NULL if there was an encoding error.
 */
M_API char *M_http_generate_query_string(const char *uri, const M_hash_dict_t *params);


/*! Create query string, append URI + query string to buffer.
 *
 * Empty values are not permitted - keys whose values are set to the empty string will be left out of
 * the query string.
 *
 * Query string are defined as application/x-www-form-urlencoded data so url form encoding is
 * used.
 *
 * \see M_http_generate_query_string()
 *
 * \param[out] buf    Buffer to add URI + query string to, contents remain unchanged if there was an error.
 * \param[in]  uri    Uri string (e.g., /cgi-bin/payment/start, or %http://google.com/whatever).
 * \param[in]  params Key-value pairs to encode in query string.
 *
 * \return M_TRUE if successful, or \c M_FALSE if there was an encoding error.
 */
M_API M_bool M_http_generate_query_string_buf(M_buf_t *buf, const char *uri, const M_hash_dict_t *params);


/*! Parse a query string.
 *
 * Components are expected to be application/x-www-form-urlencoded and will be decoded.
 *
 * \see M_http_generate_query_string()
 *
 * \param[in] data  The formatted query arguments.
 * \param[in] codec Additional encodings before the data was form encoded.
 *                  The form decoded data will be decoded to utf-8 from this encoding.
 *                  Use M_TEXTCODEC_UNKNOWN to skip additional decoding.
 *
 * \return Multi value dict of key value pairs. Keys are considered case insensitive.
 */
M_API M_hash_dict_t *M_http_parse_query_string(const char *data, M_textcodec_codec_t codec);


/*! Create form data string.
 *
 * Data is defined as application/x-www-form-urlencoded data so url form encoding is
 * used.
 *
 * \see M_http_generate_form_data_string_buf()
 *
 * \param[in] params Key-value pairs to encode in form data string.
 *
 * \return New string.
 */
M_API char *M_http_generate_form_data_string(const M_hash_dict_t *params);


/*! Create form data string.
 *
 * \see M_http_generate_query_string()
 *
 * \param[out] buf    Buffer to add URI + query string to, contents remain unchanged if there was an error.
 * \param[in]  params Key-value pairs to encode in query string.
 *
 * \return M_TRUE if successful, or \c M_FALSE if there was an encoding error.
 */
M_API M_bool M_http_generate_form_data_string_buf(M_buf_t *buf, const M_hash_dict_t *params);


/*! Parse a application/x-www-form-urlencoded paramter string.
 *
 * Components are expected to be application/x-www-form-urlencoded and will be decoded.
 * This is similar to decoding a query string but this is intended for form data from
 * the body of a message.
 *
 * \see M_http_parse_query_string()
 *
 * \param[in] data  The formatted query arguments.
 * \param[in] codec Additional encodings before the data was form encoded.
 *                  The form decoded data will be decoded to utf-8 from this encoding.
 *                  Use M_TEXTCODEC_UNKNOWN to skip additional decoding.
 *
 * \return Multi value dict of key value pairs. Keys are considered case insensitive.
 */
M_API M_hash_dict_t *M_http_parse_form_data_string(const char *data, M_textcodec_codec_t codec);

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

/*! Function definition for reading full headers.
 *
 * This will provide the full unparsed header.
 * This is always called for every header.
 * It may be called multiple times if a header appears multiple times.
 * This is intended for informational use or if passing along data and
 * not altering any headers in the process.
 *
 * A header appearing multiple times here means it was present multiple times.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_http_reader_header_func
 */
typedef M_http_error_t (*M_http_reader_header_full_func)(const char *key, const char *val, void *thunk);

/*! Function definition for reading headers as component parts.
 *
 * This is the main header reading callback that should be used when parsing a message.
 *
 * Headers are split if a header list. Keys will appear multiple times if values were
 * in a list or if the header appears multiple times. The standard uses ',' as a list
 * separator but some headers will use a semicolon ';'. Values with semicolon ';' separated
 * parameters are split as well.
 *
 * Will be called for headers that have a single part and are not split.
 *
 * It is not possible to determine if a header was present multiple times vs being
 * in list form.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_http_reader_header_full_func
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

/*! Function definition for reading full multi part headers.
 *
 * This will provide the full unparsed header.
 * This is always called for every header.
 * It may be called multiple times if a header appears multiple times.
 * This is intended for informational use or if passing along data and
 * not altering any headers in the process.
 *
 * A header appearing multiple times here means it was present multiple times.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] idx   Part number the header belongs to.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_http_reader_header_func
 */
typedef M_http_error_t (*M_http_reader_multipart_header_full_func)(const char *key, const char *val, size_t idx, void *thunk);

/*! Function definition for reading multipart part headers.
 *
 * This is the main multi part header reading callback that should be used when parsing a message.
 *
 * Headers are split if a header list. Keys will appear multiple times if values were
 * in a list or if the header appears multiple times. The standard uses ',' as a list
 * separator but some headers will use a semicolon ';'. Values with semicolon ';' separated
 * parameters are split as well.
 *
 * Will be called for headers that have a single part and are not split.
 *
 * It is not possible to determine if a header was present multiple times vs being
 * in list form.
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

/*! Function definition for reading full trailer headers.
 *
 * This will provide the full unparsed header.
 * This is always called for every trailer header.
 * It may be called multiple times if a header appears multiple times.
 * This is intended for informational use or if passing along data and
 * not altering any headers in the process.
 *
 * A header appearing multiple times here means it was present multiple times.
 *
 * \param[in] key   Header key.
 * \param[in] val   Header value.
 * \param[in] thunk Thunk.
 *
 * \return Result
 *
 * \see M_http_reader_header_func
 */
typedef M_http_error_t (*M_http_reader_trailer_full_func)(const char *key, const char *val, void *thunk);

/*! Function definition for reading trailing headers.
 *
 * This is the main trailer header reading callback that should be used when parsing a message.
 *
 * Headers are split if a header list. Keys will appear multiple times if values were
 * in a list or if the header appears multiple times. The standard uses ',' as a list
 * separator but some headers will use a semicolon ';'. Values with semicolon ';' separated
 * parameters are split as well.
 *
 * Will be called for headers that have a single part and are not split.
 *
 * It is not possible to determine if a header was present multiple times vs being
 * in list form.
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
	M_http_reader_header_full_func             header_full_func;
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
	M_http_reader_multipart_header_full_func   multipart_header_full_func;
	M_http_reader_multipart_header_func        multipart_header_func;
	M_http_reader_multipart_header_done_func   multipart_header_done_func;
	M_http_reader_multipart_data_func          multipart_data_func;
	M_http_reader_multipart_data_done_func     multipart_data_done_func;
	M_http_reader_multipart_data_finished_func multipart_data_finished_func;
	M_http_reader_multipart_epilouge_func      multipart_epilouge_func;
	M_http_reader_multipart_epilouge_done_func multipart_epilouge_done_func;
	M_http_reader_trailer_full_func            trailer_full_func;
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

/*! Parse http message from given data.
 *
 * M_HTTP_ERROR_SUCCESS, and M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE are both
 * Success conditions. Data is valid and has been parsed. Remaining unread data
 * in the buffer on M_HTTP_ERROR_SUCCESS indicates a possible additional
 * message.
 *
 * M_HTTP_ERROR_MOREDATA indicates valid data but an incomplete message.  The
 * parse should be run again starting where the last parse stopped. Until
 * a known or possible full parse has completed.
 *
 * The reader can only be used once per complete message.
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
 * Will return M_HTTP_ERROR_MOREDATA if we need to wait for more data to get a complete message.
 *
 * \param[out] simple   Place to store new M_http_simple_read_t object. Can be NULL to check for valid message.
 *                      Will only be set on M_HTTP_ERROR_SUCCESS, and M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE.
 * \param[in]  data     Buffer containing HTTP messages to read.
 * \param[in]  data_len Length of \a data.
 * \param[in]  flags    Read options (OR'd combo of M_http_simple_read_flags_t).
 * \param[in]  len_read Num bytes consumed from \a data (may be NULL, if caller doesn't need this info).
 *                      Will be set on error indicating the location in the message that generated the error.
 *
 * \return Response code.
 *
 * \see M_http_reader_read
 * \see M_http_simple_read_parser
 * \see M_http_simple_read_destroy
 */
M_API M_http_error_t M_http_simple_read(M_http_simple_read_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags, size_t *len_read);


/*! Read the next HTTP message from the given parser.
 *
 * Will return M_HTTP_ERROR_MOREDATA if we need to wait for more data to get a complete message.
 * No data will be dropped from the parser, in this case.
 *
 * On all other return values the parser will advance and data consumed. On a hard ERROR condition
 * the parser will start at the point of the error. If this is undesirable, the parser should be
 * marked and rewound after this function is called.
 *
 * \param[out] simple Place to store new M_http_simple_read_t object. Can be NULL to check for valid message.
 * \param[in]  parser Buffer containing HTTP messages to read.
 * \param[in]  flags  Read options (OR'd combo of M_http_simple_read_flags_t).
 *
 * \return Response code.
 *
 * \see M_http_reader_read
 * \see M_http_simple_read
 * \see M_http_simple_destroy
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


/*! Return the host.
 *
 * Only request messages have a host. If the parsed message wasn't a request, the
 * function will return NULL.
 *
 * The host will be read from the URI and fall back to the Host header.
 * If neither are present the host will be NULL.
 *
 * \param[in]  simple Parsed HTTP message.
 *
 * \return Host.
 */
M_API const char *M_http_simple_read_host(const M_http_simple_read_t *simple);


/*! Return the port number.
 *
 * Only request messages have a port. If the parsed message wasn't a request, the
 * function will return M_FALSE and set \a port to 0.
 *
 * The port will be read from the URI and fall back to the Host header.
 * If neither are present the port will be returned as 0. In this case it
 * should be assumed to be 80.
 *
 * \param[in]  simple Parsed HTTP message.
 * \param[out] port   Place to store port number. May be \c NULL, if you're just checking to see if a port is present.
 *
 * \return M_TRUE if a port was set, M_FALSE if there was no port in the message.
 */
M_API M_bool M_http_simple_read_port(const M_http_simple_read_t *simple, M_uint16 *port);



/*! Get headers from parsed message as key-multivalue pairs.
 *
 * Note that some headers may contain a list of multiple values, so the returned
 * M_hash_dict_t is a multimap (one key may map to a list of values).
 * The first entry in the list is the first item in the value list if there are
 * multiple values.
 *
 * Modfiers that are part of the value are included with the value.
 *
 * Header names are not case-sensitive, when doing lookups into the returned dictionary.
 *
 * \warning
 * M_http_simple_read_header should be used in order to get a full header. This is
 * a parsed list and is provided to make searching for specific values easier.
 * Use M_http_simple_read_headers to get a list of headers.
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
 * \see M_http_simple_read_headers
 * \see M_http_simple_read_get_set_cookie
 */
M_API M_hash_dict_t *M_http_simple_read_headers_dict(const M_http_simple_read_t *simple);


/*! Get a list of headers.
 *
 * Header names are not case-sensitive.
 *
 * \warning
 * The returned list does not include "Set-Cookie" headers, because they can
 * be sent multiple times with different attributes, and their values cannot be
 * merged into a list.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return List of headers
 *
 * \see M_http_simple_read_header
 * \see M_http_simple_read_get_set_cookie
 */
M_API M_list_str_t *M_http_simple_read_headers(const M_http_simple_read_t *simple);


/*! Get value of the named header from the parsed message.
 *
 * The key is not case-sensitive - it will match header names that only differ because
 * of capitalization.
 *
 * Note that some headers may contain a list of multiple values. For these headers,
 * this function will return a comma-delimited list of values. Some extra white space
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
 * \return Delimited list of values (if necessary) for this header, or \c NULL if no data found.
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
 * If the body is application/x-www-form-urlencoded this will return
 * the raw body and it will not be decoded. Even if decoding the
 * body content is enabled. The decoded data can be accessed as a
 * dict of key value pairs.
 *
 * \see M_http_simple_read_body_form_data()
 *
 * \param[in]  simple Parsed HTTP message.
 * \param[out] len    Place to store length of body (may be \c NULL).
 *
 * \return Bytes from body of message.
 */
M_API const unsigned char *M_http_simple_read_body(const M_http_simple_read_t *simple, size_t *len);


/*! Return the body of the parsed message (if any).
 *
 * This will only be filled when the content-type is application/x-www-form-urlencoded 
 * and there is key value pair body data.
 *
 * \param[in]  simple Parsed HTTP message.
 *
 * \return Bytes from body of message.
 */
M_API const M_hash_dict_t *M_http_simple_read_body_form_data(const M_http_simple_read_t *simple);


/*! Get the content type.
 *
 * May be empty if the content type was not set. Or if it was
 * application/x-www-form-urlencoded and the body was auto decoded.
 * It's not known what the content type when this encoding is used.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return The content type.
 */
M_API const char *M_http_simple_read_content_type(const M_http_simple_read_t *simple);


/*! Get the codec of the data.
 *
 * Codec is detected by charset. May be unkown if charset was not present
 * or if the charset is not supported by mstdlib's text encoding functionality.
 * If body decoding takes place and the data can be decoded this will be utf-8.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Encoded data codec.
 *
 * \see M_http_simple_read_charset
 */
M_API M_textcodec_codec_t M_http_simple_read_codec(const M_http_simple_read_t *simple);


/*! Get the text charset of the data.
 *
 * May be empty if charset was not present.
 * If the data encoded codec is unknown this can be used to determine if
 * it was not present or if the encoding is a type not supported by
 * mstdlib's text codec. If not suppored it is up to the caller to
 * handle decoding.
 *
 * \param[in] simple Parsed HTTP message.
 *
 * \return Character set.
 *
 * \see M_http_simple_read_codec
 */
M_API const char *M_http_simple_read_charset(const M_http_simple_read_t *simple);

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

/*! Create an HTTP/1.1 request message, return as a new string.
 *
 * Caller is responsible for freeing the returned string.
 *
 * If the Content-Length header is not provided in \a headers, it will be added
 * automatically for you, using \a data_len as the length. Data can be NULL
 * If either the header or data_len is provided. Data can be sent later.
 * This allows generating the header and sending large messages without
 * buffering the data in memory. In this case, this function will generate
 * the necessary HTTP header part of the message.
 *
 * If set host, port, uri, user_agent, and content_type will all override
 * any that were previoiusly set in the headers.
 *
 * \see M_http_simple_write_request_buf
 *
 * \param[in]  method       HTTP verb to use (GET, POST, etc).
 * \param[in]  host         Host the request will be sent to. Required if 'Host' header is not set.
 * \param[in]  port         Port the request will be sent on. 0 or 80 will not set the port part of the 'Host' Header.
 *                          However, if the port is 0 it is assumed to be 80. Will be ignored if 'Host' header is set.
 * \param[in]  uri          URI (may be absolute or relative, may include query string). Typically, this will be relative.
 *                          if absolute, the host and port should match the host and port parameters.
 * \param[in]  user_agent   Optional user agent to include in the request.
 * \param[in]  content_type Optional content type. If not set and not in headers this will default to 'plain/text'.
 * \param[in]  headers      Additional headers to include in request.
 * \param[in]  data         String to place in body of message (may be empty).
 * \param[in]  data_len     Number of chars to use from \c data (may be 0). Cannot be 0 if data is NULL.
 * \param[in]  charset      Encoding of the data. M_TEXTCODEC_UNKNOWN should be used if charset should not be added
 *                          to the Content-Type header. This will overwrite the charset if already present in
 *                          Content-Type. Also use M_TEXTCODEC_UNKNOWN for non-text data.
 * \param[out] len          Place to store length of returned HTTP request message (may be NULL).
 *
 * \return Allocated string containing HTTP request message. The string will be NULL terminated.
 */
M_API unsigned char *M_http_simple_write_request(M_http_method_t method,
	const char *host, unsigned short port, const char *uri,
	const char *user_agent, const char *content_type, const M_hash_dict_t *headers,
	const unsigned char *data, size_t data_len, const char *charset, size_t *len);


/*! Create an HTTP/1.1 request message, add it to the given buffer.
 *
 * Same as M_http_simple_write_request(), except that it adds the new message to the given buffer instead
 * of returning it in a newly-allocated string.
 *
 * \param[out] buf          Buffer to add the message to.
 * \param[in]  method       HTTP verb to use (GET, POST, etc).
 * \param[in]  host         Host the request will be sent to. Required if 'Host' header is not set.
 * \param[in]  port         Port the request will be sent on. 0 or 80 will not set the port part of the 'Host' Header.
 *                          However, if the port is 0 it is assumed to be 80. Will be ignored if 'Host' header is set.
 * \param[in]  uri          URI (may be absolute or relative, may include query string). Typically, this will be relative.
 *                          if absolute, the host and port should match the host and port parameters.
 * \param[in]  user_agent   Optional user agent to include in the request.
 * \param[in]  content_type Optional content type. If not set and not in headers this will default to 'plain/text'.
 * \param[in]  headers      Additional headers to include in request.
 * \param[in]  data         String to place in body of message (may be empty).
 * \param[in]  data_len     Number of chars to use from \c data (may be 0). Cannot be 0 if data is NULL.
 * \param[in]  charset      Encoding of the data. Set NULL or empty to not add a charset to the Content-Type header.
 *                          If set, this will overwrite the charset if already present in Content-Type.
 *
 * \return M_TRUE if add was successful, M_FALSE if message creation failed.
 *
 * \see M_http_simple_write_request
 */
M_API M_bool M_http_simple_write_request_buf(M_buf_t *buf, M_http_method_t method,
	const char *host, unsigned short port, const char *uri,
	const char *user_agent, const char *content_type, const M_hash_dict_t *headers,
	const unsigned char *data, size_t data_len, const char *charset);


/*! Create an HTTP/1.1 response message, return as new string.
 *
 * Caller is responsible for freeing the returned string.
 *
 * If the Content-Length header is not provided in \a headers, it will be added
 * automatically for you, using \a data_len as the length. Data can be NULL
 * If either the header or data_len is provided. Data can be sent later.
 * This allows generating the header and sending large messages without
 * buffering the data in memory. In this case, this function will generate
 * the necessary HTTP header part of the message.
 *
 * \param[in]  code         HTTP status code to use (200, 404, etc).
 * \param[in]  reason       HTTP status reason string. If NULL, will attempt to pick one automatically based on code.
 * \param[in]  content_type Optional content type. If not set and not in headers this will default to 'plain/text'.
 * \param[in]  headers      Headers to include in response.
 * \param[in]  data         String to place in body of message (may be empty).
 * \param[in]  data_len     Number of chars to use from \c data (may be 0).
 * \param[in]  charset      Encoding of the data. Set NULL or empty to not add a charset to the Content-Type header.
 *                          If set, this will overwrite the charset if already present in Content-Type.
 * \param[out] len          Place to store length of returned HTTP response message (may be NULL).
 *
 * \return New string containing HTTP response message.
 *
 * \see M_http_simple_write_response_buf
 */
M_API unsigned char *M_http_simple_write_response(M_uint32 code, const char *reason,
	const char *content_type, const M_hash_dict_t *headers, const unsigned char *data, size_t data_len,
	const char *charset, size_t *len);


/*! Create an HTTP/1.1 response message, add it to the given buffer.
 *
 * Same as M_http_simple_write_response(), except that it adds the new message to the given buffer instead
 * of returning it in a newly-allocated string.
 *
 * \param[out] buf      Buffer to add the message to.
 * \param[in]  code         HTTP status code to use (200, 404, etc).
 * \param[in]  reason       HTTP status reason string. If NULL, will attempt to pick one automatically based on code.
 * \param[in]  content_type Optional content type. If not set and not in headers this will default to 'plain/text'.
 * \param[in]  headers      Headers to include in response.
 * \param[in]  data         String to place in body of message (may be empty).
 * \param[in]  data_len     Number of chars to use from \c data (may be 0).
 * \param[in]  charset      Encoding of the data. Set NULL or empty to not add a charset to the Content-Type header.
 *                          If set, this will overwrite the charset if already present in Content-Type.
 *
 * \return M_TRUE if add was successful, M_FALSE if message creation failed.
 *
 * \see M_http_simple_write_response
 */
M_API M_bool M_http_simple_write_response_buf(M_buf_t *buf, M_uint32 code, const char *reason,
	const char *content_type, const M_hash_dict_t *headers, const unsigned char *data, size_t data_len,
	const char *charset);

/*! @} */

/*! @} */

__END_DECLS

#endif /* __M_HTTP_H__ */
