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

#include "m_net_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_net_error_t M_net_io_error_to_net_error(M_io_error_t ioerr)
{
	switch (ioerr) {
		case M_IO_ERROR_SUCCESS:
			return M_NET_ERROR_SUCCESS;
		case M_IO_ERROR_DISCONNECT:
			return M_NET_ERROR_DISCONNET;
		case M_IO_ERROR_NOTPERM:
			return M_NET_ERROR_NOTPERM;
		case M_IO_ERROR_CONNRESET:
			return M_NET_ERROR_CONNRESET;
		case M_IO_ERROR_CONNABORTED:
			return M_NET_ERROR_CONNABORTED;
		case M_IO_ERROR_PROTONOTSUPPORTED:
			return M_NET_ERROR_PROTONOTSUPPORTED;
		case M_IO_ERROR_CONNREFUSED:
			return M_NET_ERROR_CONNREFUSED;
		case M_IO_ERROR_NETUNREACHABLE:
			return M_NET_ERROR_UNREACHABLE;
		case M_IO_ERROR_TIMEDOUT:
			return M_NET_ERROR_TIMEOUT;
		case M_IO_ERROR_ERROR:
		case M_IO_ERROR_WOULDBLOCK:
		case M_IO_ERROR_NOTCONNECTED:
		case M_IO_ERROR_ADDRINUSE:
		case M_IO_ERROR_NOSYSRESOURCES:
		case M_IO_ERROR_INVALID:
		case M_IO_ERROR_NOTIMPL:
		case M_IO_ERROR_NOTFOUND:
		case M_IO_ERROR_INTERRUPTED:
		case M_IO_ERROR_BADCERTIFICATE:
			return M_NET_ERROR_ERROR;
	}
	return M_NET_ERROR_ERROR;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define ERRCASE(x) case x: ret = #x; break

const char *M_net_errcode_to_str(M_net_error_t err)
{
	const char *ret;

	switch (err) {
		ERRCASE(M_NET_ERROR_SUCCESS);
		ERRCASE(M_NET_ERROR_ERROR);
		ERRCASE(M_NET_ERROR_INTERNAL);
		ERRCASE(M_NET_ERROR_CREATE);
		ERRCASE(M_NET_ERROR_PROTOFORMAT);
		ERRCASE(M_NET_ERROR_REDIRECT);
		ERRCASE(M_NET_ERROR_REDIRECT_LIMIT);
		ERRCASE(M_NET_ERROR_DISCONNET);
		ERRCASE(M_NET_ERROR_TLS_REQUIRED);
		ERRCASE(M_NET_ERROR_TLS_SETUP_FAILURE);
		ERRCASE(M_NET_ERROR_TIMEOUT);
		ERRCASE(M_NET_ERROR_TIMEOUT_STALL);
		ERRCASE(M_NET_ERROR_NOTPERM);
		ERRCASE(M_NET_ERROR_CONNRESET);
		ERRCASE(M_NET_ERROR_CONNABORTED);
		ERRCASE(M_NET_ERROR_PROTONOTSUPPORTED);
		ERRCASE(M_NET_ERROR_CONNREFUSED);
		ERRCASE(M_NET_ERROR_UNREACHABLE);
	}

	return ret;
}
