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


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_HTTP_ERROR_SUCCESS = 0,

	M_HTTP_ERROR_INVALIDUSE,
	M_HTTP_ERROR_STOP,

	M_HTTP_ERROR_MOREDATA,
	M_HTTP_ERROR_LEN_NOTFOUND,
	M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED,
	M_HTTP_ERROR_TRAILER_NOTALLOWED,

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
	M_HTTP_ERROR_CHUNK_MALFORMED,
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
