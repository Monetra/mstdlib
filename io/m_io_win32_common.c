/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

#include "m_config.h"
#include "mstdlib/mstdlib_io.h"
#include "m_event_int.h"
#include "m_io_int.h"
#include "base/m_defs_int.h"
#include "m_io_win32_common.h"

M_io_error_t M_io_win32_err_to_ioerr(DWORD err)
{
	switch (err) {
		case 0:
			return M_IO_ERROR_SUCCESS;
		case WSAEWOULDBLOCK:
		case WSAEINPROGRESS:
		case ERROR_IO_PENDING:
			return M_IO_ERROR_WOULDBLOCK;
		case WSAEINTR:
			return M_IO_ERROR_INTERRUPTED;
		case WSAENOTCONN:
			return M_IO_ERROR_NOTCONNECTED;
		case WSAEACCES:
#if ERROR_ACCESS_DENIED != WSAEACCES
		case ERROR_ACCESS_DENIED:
#endif
			return M_IO_ERROR_NOTPERM;
		case WSAECONNRESET:
		case WSAENETRESET:
			return M_IO_ERROR_CONNRESET;
		case WSAECONNABORTED:
			return M_IO_ERROR_CONNABORTED;
		case WSAEADDRINUSE:
		case WSAEADDRNOTAVAIL:
			return M_IO_ERROR_ADDRINUSE;
		case WSAEAFNOSUPPORT:
		case WSAEPFNOSUPPORT:
		case WSAESOCKTNOSUPPORT:
		case WSAEPROTONOSUPPORT:
			return M_IO_ERROR_PROTONOTSUPPORTED;
		case WSAECONNREFUSED:
			return M_IO_ERROR_CONNREFUSED;
		case WSAENETUNREACH:
		case WSAENETDOWN:
		case WSAEHOSTDOWN:
		case WSAEHOSTUNREACH:
			return M_IO_ERROR_NETUNREACHABLE;
		case WSAETIMEDOUT:
			return M_IO_ERROR_TIMEDOUT;
		case WSAEMFILE:
		case WSAENOBUFS:
		case WSA_NOT_ENOUGH_MEMORY:
#if WSA_NOT_ENOUGH_MEMORY != ERROR_NOT_ENOUGH_MEMORY
		case ERROR_NOT_ENOUGH_MEMORY:
#endif
		case ERROR_TOO_MANY_OPEN_FILES:
			return M_IO_ERROR_NOSYSRESOURCES;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return M_IO_ERROR_NOTFOUND;
		case ERROR_BROKEN_PIPE:
			return M_IO_ERROR_DISCONNECT;
		default:
			break;
	}

	return M_IO_ERROR_ERROR;
}

M_bool M_io_win32_errormsg(DWORD err, char *error, size_t err_len)
{
	LPSTR errString = NULL;
	if (!FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	               0, 
	               err,
	               0,
	               (LPSTR)&errString,
	               0,
	               0 ))
		return M_FALSE;

	M_snprintf(error, err_len, "%s", errString);
	LocalFree(errString);

	return M_TRUE;
}
