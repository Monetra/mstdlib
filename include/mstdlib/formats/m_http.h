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
	M_HTTP_ERROR_USER_FAILURE,
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
 * \param[in] format The format data was sent using.
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

M_API void M_http_simple_destroy(M_http_simple_t *http);

M_API M_http_message_type_t M_http_simple_message_type(const M_http_simple_t *simple);
M_API M_http_version_t M_http_simple_version(const M_http_simple_t *simple);
M_API M_uint32 M_http_simple_status_code(const M_http_simple_t *simple);
M_API const char *M_http_simple_reason_phrase(const M_http_simple_t *simple);
M_API M_http_method_t M_http_simple_method(const M_http_simple_t *simple);
M_API const char *M_http_simple_uri(const M_http_simple_t *simple);
M_API M_bool M_http_simple_port(const M_http_simple_t *simple, M_uint16 *port);
M_API const char *M_http_simple_path(const M_http_simple_t *simple);
M_API const char *M_http_simple_query_string(const M_http_simple_t *simple);
M_API const M_hash_dict_t *M_http_simple_query_args(const M_http_simple_t *simple);
M_API const M_hash_dict_t *M_http_simple_headers(const M_http_simple_t *simple);
M_API char *M_http_simple_header(const M_http_simple_t *simple, const char *key);
M_API const M_list_str_t *M_http_simple_get_set_cookie(const M_http_simple_t *simple);
M_API const char *M_http_simple_body(const M_http_simple_t *simple, size_t *len);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API M_http_error_t M_http_simple_read(M_http_simple_t **simple, const unsigned char *data, size_t data_len, M_uint32 flags, size_t *len_read);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version, const M_hash_dict_t *headers, const char *data, size_t datea_len, size_t *len);
M_API unsigned char *M_http_simple_write_respone(M_http_version_t version, M_uint32 code, const char *reason, const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len);

//M_API unsigned char *M_http_simple_write_request_multipart(M_http_method_t method, const char *uri, M_http_version_t version, const M_hash_dict_t *headers, M_http_multipart_t *parts, size_t *len);
//M_API unsigned char *M_http_simple_write_respone_multipart(M_http_version_t version, M_uint32 code, const char *reason, const M_hash_dict_t *headers, M_http_multipart_t *parts, size_t *len);

/*! @} */

__END_DECLS

#endif /* __M_HTTP_H__ */
