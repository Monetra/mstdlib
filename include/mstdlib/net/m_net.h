/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

#ifndef __M_NET_H__
#define __M_NET_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_net_common Common Net functions
 *  \ingroup m_net
 * 
 * Common net functions
 *
 * @{
 */

/*! Error codes. */
typedef enum {
	M_NET_ERROR_SUCCESS = 0,         /*!< Success. */
	M_NET_ERROR_ERROR,               /*!< Generic error. */
	M_NET_ERROR_INTERNAL,            /*!< Internal error. */
	M_NET_ERROR_CREATE,              /*!< Error setting up I/O objects. */
	M_NET_ERROR_PROTOFORMAT,         /*!< Protocol format error. */
	M_NET_ERROR_REDIRECT,            /*!< Invalid redirect encountered. */
	M_NET_ERROR_REDIRECT_LIMIT,      /*!< Maximum number of redirects reached. */
	M_NET_ERROR_DISCONNET,           /*!< Unexpected disconnect. */
	M_NET_ERROR_TLS_REQUIRED,        /*!< TLS required but TLS client context was not provided. */
	M_NET_ERROR_TLS_SETUP_FAILURE,   /*!< Failed to add TLS context to I/O object. */
	M_NET_ERROR_TLS_BAD_CERTIFICATE, /*!< TLS certificate verification failed. */
	M_NET_ERROR_NOT_FOUND,           /*!< Host or location not found. */
	M_NET_ERROR_TIMEOUT,             /*!< Operation timed out. Could be during connect or overall. */
	M_NET_ERROR_TIMEOUT_STALL,       /*!< Operation timed out due to stall. */
	M_NET_ERROR_OVER_LIMIT,          /*!< Maximum data size limit exceeded. */
	M_NET_ERROR_NOTPERM,             /*!< Operation not permitted. */
	M_NET_ERROR_CONNRESET,           /*!< Connection reset by peer. */
	M_NET_ERROR_CONNABORTED,         /*!< Connection aborted. */
	M_NET_ERROR_PROTONOTSUPPORTED,   /*!< Protocol not supported. */
	M_NET_ERROR_CONNREFUSED,         /*!< Connection refused. */
	M_NET_ERROR_UNREACHABLE,         /*!< Host or location unreachable. */
} M_net_error_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a Net error code to a string.
 *
 * \param[in] err Error code
 *
 * \return Name of error code (not a description, just the enum name, like M_NET_ERROR_SUCCESS)
 */
M_API const char *M_net_errcode_to_str(M_net_error_t err);

/*! @} */

__END_DECLS

#endif /* __M_NET_H__ */
