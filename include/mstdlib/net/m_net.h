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
 *
 * @{
 */

typedef enum {
	M_NET_ERROR_SUCCESS = 0,
	M_NET_ERROR_ERROR,
	M_NET_ERROR_INTERNAL,
	M_NET_ERROR_CREATE,
	M_NET_ERROR_PROTOFORMAT,
	M_NET_ERROR_REDIRECT,
	M_NET_ERROR_REDIRECT_LIMIT,
	M_NET_ERROR_DISCONNET,
	M_NET_ERROR_TLS_REQUIRED,
	M_NET_ERROR_TLS_SETUP_FAILURE,
	M_NET_ERROR_TLS_BAD_CERTIFICATE,
	M_NET_ERROR_NOT_FOUND,
	M_NET_ERROR_TIMEOUT,
	M_NET_ERROR_TIMEOUT_STALL,
	M_NET_ERROR_NOTPERM,
	M_NET_ERROR_CONNRESET,
	M_NET_ERROR_CONNABORTED,
	M_NET_ERROR_PROTONOTSUPPORTED,
	M_NET_ERROR_CONNREFUSED,
	M_NET_ERROR_UNREACHABLE,
} M_net_error_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API const char *M_net_errcode_to_str(M_net_error_t err);

/*! @} */

__END_DECLS

#endif /* __M_NET_H__ */
