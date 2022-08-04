/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#ifndef _WIN32
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#    ifdef __sun__
#        include <xti.h>
#    endif
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#  include <unistd.h>
#endif
#include "m_io_net_int.h"

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#ifdef _WIN32
/* NOTE: Included just for M_io_win32_err_to_ioerr() */
#  include "m_io_win32_common.h"
#else
#  include "m_io_posix_common.h"
#endif

/* XXX: currently needed for M_io_setnonblock() which should be moved */
#include "m_io_int.h"

/* For some reason this is defined on OS X but we get a compile error. We are
 * setting _DARWIN_C_SOURCE which should allow the define to be used but it's not
 * so we just check if it's defined and if not define it ourselves.
 * It should also be noted that NI_MAXHOST seems to be implemented on all systems
 * where as HOST_NAME_MAX is Linux only. */
#ifndef NI_MAXHOST
#  if defined(HOST_NAME_MAX)
#    define NI_MAXHOST HOST_NAME_MAX
#  else
/* 1025 is what is defined on OS X. */
#    define NI_MAXHOST 1025
#  endif
#endif


#ifdef _WIN32
static M_io_error_t M_io_net_resolve_error_sys(DWORD err)
{
	return M_io_win32_err_to_ioerr(err);
}
#else
static M_io_error_t M_io_net_resolve_error_sys(int err)
{
	return M_io_posix_err_to_ioerr(err);
}
#endif

static void M_io_net_resolve_error(M_io_handle_t *handle)
{
#ifdef _WIN32
	handle->data.net.last_error_sys = (DWORD)WSAGetLastError();
#else
	handle->data.net.last_error_sys = errno;
	errno                           = 0;
#endif
	handle->data.net.last_error     = M_io_net_resolve_error_sys(handle->data.net.last_error_sys);
}

#ifdef _WIN32
static M_thread_once_t M_io_net_once = M_THREAD_ONCE_STATIC_INITIALIZER;
static void M_io_net_init_destroy(void *arg)
{
	(void)arg;
	if (!M_thread_once_reset(&M_io_net_once))
		return;

	WSACleanup();
}

static void M_io_net_init_system_run(M_uint64 flags)
{
	WSADATA       wsaData;
	(void)flags;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	M_library_cleanup_register(M_io_net_init_destroy, NULL);
}
#endif

void M_io_net_init_system(void)
{
#ifdef _WIN32
	M_thread_once(&M_io_net_once, M_io_net_init_system_run, 0);
#endif
}


static void M_io_net_handle_close(M_io_t *comm, M_io_handle_t *handle)
{
	M_event_t *event = M_io_get_event(comm);

	if (handle->data.net.evhandle == M_EVENT_INVALID_HANDLE && handle->data.net.sock == M_EVENT_INVALID_SOCKET)
		return;

	if (handle->state == M_IO_NET_STATE_CONNECTED || handle->state == M_IO_NET_STATE_CONNECTING || handle->state == M_IO_NET_STATE_DISCONNECTING)
		handle->state = M_IO_NET_STATE_DISCONNECTED;

	if (event)
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, comm, handle->data.net.evhandle, handle->data.net.sock, 0, 0);
#ifdef _WIN32
	/* It should not be necessary to detach from the socket, but lets do so anyhow */
	if (handle->data.net.sock != M_EVENT_INVALID_SOCKET && handle->data.net.evhandle != M_EVENT_INVALID_HANDLE)
		WSAEventSelect(handle->data.net.sock, handle->data.net.evhandle, 0);
	if (handle->data.net.sock != M_EVENT_INVALID_SOCKET)
		closesocket(handle->data.net.sock);
	if (handle->data.net.evhandle != M_EVENT_INVALID_HANDLE)
		WSACloseEvent(handle->data.net.evhandle);
#else
	if (handle->data.net.sock != M_EVENT_INVALID_SOCKET)
		close(handle->data.net.sock);
#endif
	handle->data.net.evhandle = M_EVENT_INVALID_HANDLE;
	handle->data.net.sock     = M_EVENT_INVALID_SOCKET;
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}
}

static void M_io_net_timeout_cb(M_event_t *event, M_event_type_t type, M_io_t *comm_bogus, void *data)
{
	M_io_layer_t  *layer  = data;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	(void)event;
	(void)type;
	(void)comm_bogus;

	if (handle->state != M_IO_NET_STATE_CONNECTING) {
		return;
	}

	handle->state = M_IO_NET_STATE_ERROR;

	/* Don't store an error buffer just for this message, go ahead and map it
	 * to one of the system's standard messages for a timed out operation */
#ifdef _WIN32
	handle->data.net.last_error_sys = WSAETIMEDOUT;
#else
	handle->data.net.last_error_sys = ETIMEDOUT;
#endif

	M_io_net_handle_close(io, handle);

	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, M_io_net_resolve_error_sys(handle->data.net.last_error_sys));
	M_event_timer_stop(handle->timer);
}


#ifdef _WIN32
#  define RECV_TYPE     char *
#  define RECV_LEN_TYPE int
#else
#  define RECV_TYPE     unsigned char *
#  define RECV_LEN_TYPE size_t
#endif

static M_io_error_t M_io_net_read_cb_int(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	ssize_t        retval;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)meta;

	errno  = 0;
	retval = (ssize_t)recv(handle->data.net.sock, (RECV_TYPE)buf, (RECV_LEN_TYPE)*read_len, 0);
	if (retval == 0) {
		handle->data.net.last_error_sys = 0;
		handle->data.net.last_error     = M_IO_ERROR_DISCONNECT;
		return M_IO_ERROR_DISCONNECT;
	} else if (retval < 0) {
		M_io_net_resolve_error(handle);
		return handle->data.net.last_error;
	}

	*read_len = (size_t)retval;
	return M_IO_ERROR_SUCCESS;
}

#ifdef _WIN32
#  define SEND_TYPE     const char *
#  define SEND_LEN_TYPE int
#else
#  define SEND_TYPE     const unsigned char *
#  define SEND_LEN_TYPE size_t
#endif

static M_io_error_t M_io_net_write_cb_int(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	ssize_t        retval;
	int            flags  = 0;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err    = M_IO_ERROR_ERROR;

#if !defined(MSG_NOSIGNAL) && !defined(_WIN32) /* && !defined(SO_NOSIGPIPE) */
	M_io_posix_sigpipe_state_t sigpipe_state;
#endif

	(void)meta;

	if (handle->state != M_IO_NET_STATE_CONNECTED) {
		if (handle->state == M_IO_NET_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return M_IO_ERROR_ERROR;
	}

#if !defined(MSG_NOSIGNAL) && !defined(_WIN32) /* && !defined(SO_NOSIGPIPE) */
	M_io_posix_sigpipe_block(&sigpipe_state);
#endif

#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif

	errno  = 0;
	retval = (ssize_t)send(handle->data.net.sock, (SEND_TYPE)buf, (SEND_LEN_TYPE)*write_len, flags);
	if (retval == 0) {
		handle->data.net.last_error = M_IO_ERROR_DISCONNECT;
		err = M_IO_ERROR_DISCONNECT;
	} else if (retval < 0) {
		M_io_net_resolve_error(handle);
		err = handle->data.net.last_error;
	}

#if !defined(MSG_NOSIGNAL) && !defined(_WIN32) /* && !defined(SO_NOSIGPIPE) */
	M_io_posix_sigpipe_unblock(&sigpipe_state);
#endif

	if (retval <= 0)
		return err;

	*write_len = (size_t)retval;
	return M_IO_ERROR_SUCCESS;
}


static void M_io_net_readwrite_err(M_io_t *comm, M_io_layer_t *layer, M_bool is_read, M_io_error_t err, size_t request_len, size_t out_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_event_t     *event  = M_io_get_event(comm);

	if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
		/* Error condition, Stop waiting on all events */
		M_io_net_handle_close(comm, handle);
		handle->state     = (err == M_IO_ERROR_DISCONNECT)?M_IO_NET_STATE_DISCONNECTED:M_IO_NET_STATE_ERROR;
		handle->hard_down = M_TRUE;
		return;
	}

	if (err == M_IO_ERROR_WOULDBLOCK) {
		/* Must wait on respectiv read or write event */
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, is_read?M_EVENT_WAIT_READ:M_EVENT_WAIT_WRITE, 0);
		return;
	}

	/* Success case here */

	/* If we read something, go ahead and allow us to be signaled again.  This works around some issues
	 * where the socket is closed but bytes are still buffered, if we don't listen for
	 * reads, we never get the disconnect event. Normally we'd only want to re-enable reads if
	 * we got a partial read. */
	if (is_read && out_len) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, 0);
		return;
	}

	/* If we got a partial write, we've got to wait to be signaled again */
	if (!is_read && request_len > out_len) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_WRITE, 0);
		return;
	}

	/* Otherwise, disable waiting on events */
	M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, is_read?M_EVENT_WAIT_READ:M_EVENT_WAIT_WRITE, 0);
}


static M_io_error_t M_io_net_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	size_t         request_len;
	M_io_error_t   err;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || buf == NULL || read_len == NULL || *read_len == 0) {
		return M_IO_ERROR_INVALID;
	}

	/* Don't use sanity check here if netdns is wrapping us, that layer has its own that is
	 * more valid due to relayed signals.  There may infact be data buffered still due to
	 * the way signals are delivered */
	if (handle->hard_down && handle->state != M_IO_NET_STATE_CONNECTED && handle->state != M_IO_NET_STATE_DISCONNECTING)
		return M_IO_ERROR_NOTCONNECTED;

	request_len = *read_len;
	err         = M_io_net_read_cb_int(layer, buf, read_len, meta);
	M_io_net_readwrite_err(M_io_layer_get_io(layer), layer, M_TRUE, err, request_len, *read_len);

	return err;
}


static M_io_error_t M_io_net_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	size_t         request_len;
	M_io_error_t   err;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || buf == NULL || write_len == NULL || *write_len == 0)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_NET_STATE_CONNECTED)
		return M_IO_ERROR_NOTCONNECTED;

	request_len = *write_len;
	err         = M_io_net_write_cb_int(layer, buf, write_len, meta);
	M_io_net_readwrite_err(M_io_layer_get_io(layer), layer, M_FALSE, err, request_len, *write_len);

	return err;
}


static void M_io_net_set_sockopts_keepalives(M_io_handle_t *handle)
{
	size_t               num_opts = 0;
#ifdef _WIN32
	struct tcp_keepalive ka       = { 0 };
	struct tcp_keepalive kar      = { 0 };
	DWORD                dwBytes  = 0;

	ka.onoff             = 1;
	ka.keepalivetime     = (unsigned long)(handle->settings.ka_idle_time_s * 1000);
	/* The keepalive successive probes is hardcoded to 10, so we need adjust the ka_retry_time_s to better
	 * match the user request */
	ka.keepaliveinterval = (unsigned long)((handle->settings.ka_retry_time_s * 1000 * handle->settings.ka_retry_cnt ) / 10);
	if (WSAIoctl(handle->data.net.sock, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), &kar, sizeof(kar), &dwBytes, NULL, NULL) != 0) {
		M_printf("WSAIoctl(SIO_KEEPALIVE_VALS) failed: %ld", (long)WSAGetLastError());
	}
	num_opts += 3;
#else /* !_WIN32 */

	int on;

#  ifdef SO_KEEPALIVE
	on = 1;
	if (setsockopt(handle->data.net.sock, SOL_SOCKET, SO_KEEPALIVE, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(SO_KEEPALIVE) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  ifdef TCP_KEEPIDLE
	/* how long (seconds) the connection is idle before sending keepalive probes */
	on = (int)handle->settings.ka_idle_time_s;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPIDLE, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(TCP_KEEPIDLE) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  if !defined(TCP_KEEPCNT) && defined(TCP_NKEEP)
#    define TCP_KEEPCNT TCP_NKEEP  /* Alias for SCO */
#  endif

#  ifdef TCP_KEEPCNT
	/* max number of probes until connection is considered dead */
	on = (int)handle->settings.ka_retry_cnt;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPCNT, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(TCP_KEEPCNT) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  ifdef TCP_KEEPINTVL
	/* time in seconds between individual keepalive probes */
	on = (int)handle->settings.ka_retry_time_s;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPINTVL, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(TCP_KEEPINTVL) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  if defined(TCP_KEEPALIVE) && defined(__APPLE__)
	/* Sun has TCP_KEEPALIVE but doesn't work, returns errors */

	/* time in seconds idle before keepalive probes */
	on = (int)handle->settings.ka_idle_time_s;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPALIVE, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(TCP_KEEPALIVE) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  if defined(TCP_KEEPALIVE_THRESHOLD)
	/* Solaris */
	/* time in milliseconds between individual keepalive probes */
	on = (int)handle->settings.ka_retry_time_s * 1000;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPALIVE_THRESHOLD, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(IPPROTO_TCP, TCP_KEEPALIVE_THRESHOLD) failed: %s", strerror(errno));
	}
	num_opts++;
#  endif

#  if defined(TCP_KEEPALIVE_ABORT_THRESHOLD)
	/* Solaris */
	/* default time (in milliseconds) threshold to abort a TCP connection after the keepalive probing mechanism has failed */
	on = (int)handle->settings.ka_retry_time_s * handle->settings.ka_retry_cnt * 1000;
	if (setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_KEEPALIVE_ABORT_THRESHOLD, (const void *)&on, sizeof(on)) == -1) {
		M_printf("setsockopt(IPPROTO_TCP, TCP_KEEPALIVE_ABORT_THRESHOLD) failed: %s", strerror(errno));
	}
	num_opts+=2;
#  endif
#endif /* !_WIN32 */

	if (num_opts == 0)
		M_fprintf(stderr, "%s(): keepalive not supported on this system\n", __FUNCTION__);
}


static void M_io_net_set_sockopts(M_io_handle_t *handle)
{
	int ival;
	int rv;
	struct linger so_linger;

	/* Set Nagle, if enable TCP_NODELAY is 0 (off), otherwise it is 1 (on) */
	ival = (handle->settings.nagle_enable)?0:1;
	rv   = setsockopt(handle->data.net.sock, IPPROTO_TCP, TCP_NODELAY, (const void *)&ival, sizeof(ival));
	(void)rv; /* Not the end of the world if this fails, coverity complained */

	/* Prevent SIGPIPE */
#ifdef SO_NOSIGPIPE
	ival = 1;
	rv = setsockopt(handle->data.net.sock, SOL_SOCKET, SO_NOSIGPIPE, (const void *)&ival, sizeof(ival));
	(void)rv;
#endif

	/* We have seen windows closesocket() hang for 200ms to 1s for unknown reasons.  Setting
	 * abortive close resolves this issue.  This should be fine as we rely on shutdown()
	 * for closing gracefully.  Default is l_onoff=zero ... but of course ... Windows...
	 * |---------|----------|----------------------------------------------------------------|----------------|
	 * | l_onoff | l_linger | Type of close                                                  | Wait for close |
	 * |=========|==========|================================================================|================|
	 * | zero    | N/A      | Graceful close                                                 | No             |
	 * |---------|----------|----------------------------------------------------------------|----------------|
	 * | nonzero | zero     | Hard                                                           | No             |
	 * |---------|----------|----------------------------------------------------------------|----------------|
	 * | nonzero | nonzero  | Graceful if all data is sent within timeout value specified in | Yes            |
	 * |         |          | the l_linger member.                                           |                |
	 * |         |          | Hard if all data could not be sent within timeout value        |                |
	 * |         |          | specified in the l_linger member.                              |                |
	 * |---------|----------|----------------------------------------------------------------|----------------|
	 */
#ifdef _WIN32
	so_linger.l_onoff  = 1;
#else
	/* 20220720 -- Cirrus CI's FreeBSD needs SO_LINGER to be off.  When it is on, a client will initiate a
	* disconnect sequence with shutdown() via M_io_disconnect() returning 0 errors.  The system will then
	* flag an EV_EOF event indicating that a graceful shutdown sequence completed.  Then socket will close()
	* via M_io_destroy().  If this happens when there is pending data (activating the SO_LINGER logic)
	* the server will never receive an EV_EOF flagged event. I have many logs available if you want more
	* information -- AK */
	so_linger.l_onoff  = 0;
#endif
	so_linger.l_linger = 0;
	rv = setsockopt(handle->data.net.sock, SOL_SOCKET, SO_LINGER, (const void *)&so_linger, sizeof(so_linger));
	(void)rv; /* silence coverity */

	if (handle->settings.ka_enable)
		M_io_net_set_sockopts_keepalives(handle);

#ifdef _WIN32

#endif
}


/* Set Windows SIO_LOOPBACK_FAST_PATH which must be enabled BEFORE connecting, and on
 * the listening socket as well.  Windows 8/2012+ only. */
static void M_io_net_set_fastpath(M_io_handle_t *handle)
{
#ifdef _WIN32
	int   OptionValue           = 1;
	DWORD NumberOfBytesReturned = 0;

#  ifndef SIO_LOOPBACK_FAST_PATH
#    define SIO_LOOPBACK_FAST_PATH 0x98000010
#  endif
	WSAIoctl(handle->data.net.sock,
	         SIO_LOOPBACK_FAST_PATH,
	         &OptionValue,
	         sizeof(OptionValue),
	         NULL,
	         0,
	         &NumberOfBytesReturned,
	         0,
	         0);
#else
	(void)handle;
#endif
}

static M_bool M_io_net_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *comm   = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(comm);
	M_io_type_t    ctype  = M_io_get_type(comm);

	/* If we are disconnected already, we should pass thru DISCONNECT or ERROR events and drop
	 * any others (DISCONNECT and ERROR events might otherwise not have yet been delivered) */
	if (handle->state == M_IO_NET_STATE_DISCONNECTED || handle->state == M_IO_NET_STATE_ERROR) {
		/* Remove any registered event from the OS if necessary */
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, 0);
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_WRITE, 0);
		if (*type == M_EVENT_TYPE_DISCONNECTED || *type == M_EVENT_TYPE_ERROR) {
			return M_FALSE;
		}
		return M_TRUE;
	}

	/* While disconnecting, don't send WRITE events */
	if (handle->state == M_IO_NET_STATE_DISCONNECTING && *type == M_EVENT_TYPE_WRITE)
		return M_TRUE;

	/* While disconnecting, just treat an ERROR event as if it was a DISCONNECT event */
	if (handle->state == M_IO_NET_STATE_DISCONNECTING && *type == M_EVENT_TYPE_ERROR) {
		*type = M_EVENT_TYPE_DISCONNECTED;
	}

	/* Attempting to connect, we should only get a read event or a disconnected/error
	 * event possibly (ever?) */
	if (ctype == M_IO_TYPE_STREAM && handle->state == M_IO_NET_STATE_CONNECTING) {
		socklen_t        arglen;

		switch (*type) {
			case M_EVENT_TYPE_WRITE:
			case M_EVENT_TYPE_READ:         /* This might be emitted on error */
			case M_EVENT_TYPE_DISCONNECTED: /* This might be emitted on error */
			case M_EVENT_TYPE_ERROR:        /* This might be emitted on error */
				arglen = (socklen_t)sizeof(handle->data.net.last_error_sys);
#ifdef _WIN32
				if (getsockopt(handle->data.net.sock, SOL_SOCKET, SO_ERROR, (char *)&handle->data.net.last_error_sys, &arglen) == 0) {
#else
				if (getsockopt(handle->data.net.sock, SOL_SOCKET, SO_ERROR, &handle->data.net.last_error_sys, &arglen) == 0) {
#endif
					handle->data.net.last_error = M_io_net_resolve_error_sys(handle->data.net.last_error_sys);
				}

				if ((*type == M_EVENT_TYPE_WRITE || *type == M_EVENT_TYPE_READ) && handle->data.net.last_error_sys == 0) {
					handle->data.net.last_error = M_IO_ERROR_SUCCESS;
					/* Remove write waiter, add read waiter */
					M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_WRITE, 0);
					M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, 0);

					if (*type == M_EVENT_TYPE_READ) {
						/* Add another event for the read because this one is transformed to connect */
						M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
					}
					/* Rewrite event to say connected */
					*type         = M_EVENT_TYPE_CONNECTED;
					handle->state = M_IO_NET_STATE_CONNECTED;
					M_event_timer_stop(handle->timer);
					break;
				} else {
#ifndef _WIN32
					/* MacOSX has a weird bug where they return EV_EOF under high load for establishing a
					 * connection and do not indicate an error condition.  */
					if (handle->data.net.last_error_sys == 0) {
						handle->data.net.last_error_sys = ECONNABORTED;
					}
#endif
					handle->data.net.last_error = M_io_net_resolve_error_sys(handle->data.net.last_error_sys);
				}
				/* Fall-Through to error condition */

				/* Error condition */
				*type         = M_EVENT_TYPE_ERROR;
				handle->state = M_IO_NET_STATE_ERROR;
				M_io_net_handle_close(comm, handle);
				M_io_set_error(comm, handle->data.net.last_error);
				return M_FALSE;

			default:
				/* When attempting to connect, these events should never occur,
				 * if they do, ignore and consume */
				return M_TRUE;
		}
	}

	/* Attempting to listen */
	if (ctype == M_IO_TYPE_LISTENER) {
		if (*type == M_EVENT_TYPE_READ || *type == M_EVENT_TYPE_ACCEPT) {
			*type = M_EVENT_TYPE_ACCEPT;
			return M_FALSE;
		}
		/* any other events are bogus, ignore */
		return M_TRUE;
	}

	/* When disconnecting, consume data on read events, the user doesn't want it, we need
	 * to do this because we may receive a READ event, not a disconnect event */
	if (handle->state == M_IO_NET_STATE_DISCONNECTING) {
		/* Don't deliver write events */
		if (*type == M_EVENT_TYPE_WRITE)
			return M_TRUE;

		if (*type == M_EVENT_TYPE_READ) {
			unsigned char buf[1024];
			size_t        buf_len = sizeof(buf);
			M_io_error_t  ioerr;

			while ((ioerr = M_io_net_read_cb(layer, buf, &buf_len, NULL)) == M_IO_ERROR_SUCCESS && buf_len == sizeof(buf)) {
				/* Reset buf size */
				buf_len = sizeof(buf);
			}

			/* If an error occurred, rewrite the event type */
			if (ioerr == M_IO_ERROR_DISCONNECT) {
				*type = M_EVENT_TYPE_DISCONNECTED;
			} else if (ioerr != M_IO_ERROR_SUCCESS && ioerr != M_IO_ERROR_WOULDBLOCK) {
				*type = M_EVENT_TYPE_ERROR;
			} else {
				/* Consume the event since we ate it */
				return M_TRUE;
			}
		}
	}


	switch (*type) {
		case M_EVENT_TYPE_CONNECTED:
			M_io_net_set_sockopts(handle);
			break;
		case M_EVENT_TYPE_ERROR:
			if (handle->state == M_IO_NET_STATE_CONNECTED && handle->data.net.last_error_sys == 0) {
				/* No way to *really* know the error, use the reset by peer error */
				/* NOTE: really? couldn't we getsockopt(handle->data.net.sock, SOL_SOCKET, SO_ERROR, ...) like we do for connect errors? */
#ifdef _WIN32
				handle->data.net.last_error_sys = WSAECONNRESET;
#else
				handle->data.net.last_error_sys = ECONNRESET;
#endif
			}
			handle->state               = M_IO_NET_STATE_ERROR;
			handle->data.net.last_error = M_io_net_resolve_error_sys(handle->data.net.last_error_sys);
			M_io_set_error(comm, handle->data.net.last_error);
			/* DO NOT close handle automatically, user will do so. */
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			handle->state = M_IO_NET_STATE_DISCONNECTED;
			/* DO NOT close handle automatically, user will do so. */
			break;
		case M_EVENT_TYPE_READ:
			/* We got a read, we need to wait on an op to re-activate */
			if (handle->state == M_IO_NET_STATE_CONNECTED) {
				M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, 0);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			/* We got a write, we need to wait on an op to re-activate */
			if (handle->state == M_IO_NET_STATE_CONNECTED) {
				M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_WRITE, 0);
			}
			break;
		default:
			break;
	}


	/* Pass event on to next layer */
	return M_FALSE;
}


static M_bool M_io_net_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	if (handle == NULL)
		return M_FALSE;

	/* Cleanup for re-init */
	handle->state                    = M_IO_NET_STATE_INIT;
	handle->hard_down                = M_FALSE;
	handle->notify_down              = M_FALSE;
	handle->data.net.last_error_sys  = 0;
	handle->data.net.last_error      = M_IO_ERROR_SUCCESS;

	M_io_net_handle_close(io, handle);
	return M_TRUE;
}


static void M_io_net_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;

	/* reset_cb() ensures handle is closed */

	M_free(handle->host);
	M_free(handle->server_ipaddr);
	M_free(handle);
}


static struct sockaddr *M_io_net_addr2peer(const char *addr, M_uint16 port, socklen_t *peer_size, int *type)
{
	struct in_addr  addr4;
#ifdef AF_INET6
	struct in6_addr addr6;
#endif

	if (M_dns_pton(AF_INET, addr, &addr4) > 0) {
		struct sockaddr_in *peer = M_malloc_zero(sizeof(*peer));
		*peer_size               = sizeof(*peer);
		*type                    = AF_INET;
		peer->sin_family         = AF_INET;
		peer->sin_port           = M_hton16(port);
		M_mem_copy(&peer->sin_addr.s_addr, &addr4, sizeof(addr4));
		return (struct sockaddr *)peer;
	}
#ifdef AF_INET6
	if (M_dns_pton(AF_INET6, addr, &addr6) > 0) {
		struct sockaddr_in6 *peer = M_malloc_zero(sizeof(*peer));
		*peer_size                = sizeof(*peer);
		*type                     = AF_INET6;
		peer->sin6_family         = AF_INET6;
		peer->sin6_port           = M_hton16(port);
		M_mem_copy(&peer->sin6_addr, &addr6, sizeof(addr6));
		return (struct sockaddr *)peer;
	}
#endif
	return NULL;
}

#ifdef _WIN32
static M_bool M_io_net_IsWindowsVistaOrGreater(void)
{
	OSVERSIONINFO vinfo;
	memset(&vinfo, 0, sizeof(vinfo));
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
	if (!GetVersionEx(&vinfo) || vinfo.dwMajorVersion < 6)
		return M_FALSE;
	return M_TRUE;
}
#endif


static M_bool M_io_net_set_ephemeral_port(M_io_handle_t *handle)
{
#ifdef HAVE_SOCKADDR_STORAGE
	struct sockaddr_storage sockaddr;
#else
	struct sockaddr         sockaddr;
#endif
	struct sockaddr        *sockaddr_ptr  = (struct sockaddr *)&sockaddr;
	socklen_t               sockaddr_size = sizeof(sockaddr);

	M_mem_set(sockaddr_ptr, 0, (size_t)sockaddr_size);

	if (getsockname(handle->data.net.sock, sockaddr_ptr, &sockaddr_size) != 0)
		return M_FALSE;

	if (sockaddr_ptr->sa_family == AF_INET) {
		struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)((void *)sockaddr_ptr);
		handle->data.net.eport = M_ntoh16(sockaddr_in->sin_port);
#ifdef AF_INET6
	} else if (sockaddr_ptr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sockaddr_in6 = (struct sockaddr_in6 *)((void *)sockaddr_ptr);
		handle->data.net.eport = M_ntoh16(sockaddr_in6->sin6_port);
#endif
	} else {
		return M_FALSE;
	}

	return M_TRUE;
}


static M_io_error_t M_io_net_listen_bind_int(M_io_handle_t *handle)
{
	struct sockaddr   *sa;
	socklen_t          sa_size;
	int                aftype;
	int                enable = 1;
	const char        *bindip = handle->host;
	int                type   = SOCK_STREAM;
	int                rv;

	/* If the bind address was set to NULL, that means to listen on all interfaces.  Lets
	 * set it appropriately based on the type of connection requested. */
	if (M_str_isempty(handle->host)) {
#ifdef _WIN32
		/* Do not attempt to use IPv6 for M_IO_NET_ANY before Windows Vista since prior
		 * versions are not properly dual-stack */
		if (!M_io_net_IsWindowsVistaOrGreater() && handle->type == M_IO_NET_ANY) {
			handle->type = M_IO_NET_IPV4;
		}
#endif

#ifdef AF_INET6
		if (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV6) {
			bindip = "::";
		}
#endif

		if (bindip == NULL && (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV4)) {
			bindip = "0.0.0.0";
		}
	}

	sa = M_io_net_addr2peer(bindip, handle->port, &sa_size, &aftype);
	if (sa == NULL) {
		return M_IO_ERROR_INVALID;
	}

#ifdef AF_INET6
	/* If requested IPv6 only, and bind ip provided was not ipv6, error */
	if (handle->type == M_IO_NET_IPV6 && aftype != AF_INET6) {
		M_free(sa);
		return M_IO_ERROR_INVALID;
	}
#endif

	/* If requested IPv4 only, and bind ip provided was not ipv4, error */
	if (handle->type == M_IO_NET_IPV4 && aftype != AF_INET) {
		M_free(sa);
		return M_IO_ERROR_INVALID;
	}

	/* If the handle type is ANY, and they specified a bind IP address, and it isn't
	 * set to listen on all interfaces, set our actual type to the type of the IP
	 * address */
	if (handle->type == M_IO_NET_ANY && !M_str_isempty(handle->host) && !M_str_eq(handle->host, "::")) {
#ifdef AF_INET6
		handle->type = (aftype == AF_INET6)?M_IO_NET_IPV6:M_IO_NET_IPV4;
#else
		handle->type = M_IO_NET_IPV4;
#endif
	}

#ifdef SOCK_CLOEXEC
	type |= SOCK_CLOEXEC;
#endif
	handle->data.net.sock = socket(aftype, type, IPPROTO_TCP);
	if (handle->data.net.sock == M_EVENT_INVALID_SOCKET) {
		M_io_net_resolve_error(handle);
		M_free(sa);
		return handle->data.net.last_error;
	}
#if !defined(SOCK_CLOEXEC) && !defined(_WIN32)
	M_io_posix_fd_set_closeonexec(handle->data.net.sock, M_TRUE);
#endif


#ifdef SO_EXCLUSIVEADDRUSE
	/* Windows, prevent 'stealing' of bound ports, why would this be allowed by default? */
	rv = setsockopt(handle->data.net.sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const void *)&enable, sizeof(enable));
	(void)rv; /* silence coverity */
#else
	/* MSDN suggests not using SO_REUSEADDR for winsock if possible. */
	/* NOTE: We don't ever want to set SO_REUSEPORT which would allow 'stealing' of our bind */
	rv = setsockopt(handle->data.net.sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&enable, sizeof(enable));
	(void)rv; /* silence coverity */
#endif

#ifdef AF_INET6
	/* Requested ipv6 only, need to set the socket option for this */
	if (aftype == AF_INET6) {
#  if defined(_WIN32) && !defined(IPV6_V6ONLY)
		/* NOTE: Windows only supports this on Vista or higher which requires the
		 *       _WIN32_WINNT define of  0x0600 or higher. If we're compiling for
		 *       a lower version, we need to define the value ourselves and it should
		 *       return an error if we're running on a lower OS that we ignore anyhow.
		 */
#    define IPV6_V6ONLY 27
#  endif
#  if defined(IPV6_V6ONLY)
		/* Some OS's may set IPV6_V6ONLY on by default.  So always override the flag
		 * with our intended behavior */
		if (handle->type == M_IO_NET_IPV6) {
			enable = 1; /* IPV6 only */
		} else {
			enable = 0; /* Dual socket IPv6 + IPv4 */
		}
		rv = setsockopt(handle->data.net.sock, IPPROTO_IPV6, IPV6_V6ONLY, (const void *)&enable, sizeof(enable));
		(void)rv; /* silence coverity */
#  endif
	}
#endif

	if (bind(handle->data.net.sock, sa, sa_size) == -1) {
		M_io_net_resolve_error(handle);
#ifdef _WIN32
		closesocket(handle->data.net.sock);
		if (handle->data.net.last_error == M_IO_ERROR_NOTPERM) {
			handle->data.net.last_error = M_IO_ERROR_ADDRINUSE;
		}
#else
		close(handle->data.net.sock);
#endif
		M_free(sa);
		return handle->data.net.last_error;
	}
	M_free(sa);

	/* port of 0 means let the OS assign.  In this case, we need to fill in the port metadata */
	if (handle->port == 0) {
		M_io_net_set_ephemeral_port(handle);
		handle->port = handle->data.net.eport;
	}

	M_io_net_set_fastpath(handle);

	if (listen(handle->data.net.sock, 512) == -1) {
		M_io_net_resolve_error(handle);
#ifdef _WIN32
		closesocket(handle->data.net.sock);
#else
		close(handle->data.net.sock);
#endif
		return handle->data.net.last_error;
	}

	handle->state  = M_IO_NET_STATE_LISTENING;
	M_io_setnonblock(handle->data.net.sock);
#ifdef _WIN32
	handle->data.net.evhandle = WSACreateEvent();
	WSAEventSelect(handle->data.net.sock, handle->data.net.evhandle, FD_ACCEPT);
#else
	handle->data.net.evhandle = handle->data.net.sock;
#endif

	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_net_listen_bind(M_io_handle_t *handle)
{
	M_io_error_t err;

	err = M_io_net_listen_bind_int(handle);
	if (err == M_IO_ERROR_SUCCESS)
		return M_IO_ERROR_SUCCESS;

	/* Some OS's may allow disabling of IPv6 completely, but the system supports
	 * it so we really need to re-try to bind to IPv4 to see if it works since
	 * they really requested ANY */
	if (handle->type == M_IO_NET_ANY) {
		handle->type = M_IO_NET_IPV4;
		err = M_io_net_listen_bind_int(handle);
	}

	return err;
}

static M_bool M_io_net_listen_init_cb(M_io_layer_t *layer)
{
	M_io_t            *io     = M_io_layer_get_io(layer);
	M_io_handle_t     *handle = M_io_layer_get_handle(layer);
	M_event_t         *event  = M_io_get_event(io);

	if (handle->state == M_IO_NET_STATE_LISTENING) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ);
	}

	return M_TRUE;
}


static M_bool M_io_net_start_connect(M_io_layer_t *layer, struct sockaddr *peer, socklen_t peer_size, int type)
{
	int            rc;
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	M_io_t        *io       = M_io_layer_get_io(layer);
	M_event_t     *event    = M_io_get_event(io);
	int            typeflag = SOCK_STREAM;

#ifdef SOCK_CLOEXEC
	typeflag |= SOCK_CLOEXEC;
#endif
	handle->data.net.sock = socket(type, typeflag, 0);
	if (handle->data.net.sock == M_EVENT_INVALID_SOCKET) {
		M_io_net_resolve_error(handle);
		return M_FALSE;
	}

#if !defined(SOCK_CLOEXEC) && !defined(_WIN32)
	M_io_posix_fd_set_closeonexec(handle->data.net.sock, M_TRUE);
#endif

	handle->state  = M_IO_NET_STATE_CONNECTING;

	M_io_setnonblock(handle->data.net.sock);

	M_io_net_set_fastpath(handle);

	rc = connect(handle->data.net.sock, peer, peer_size);

	if (rc >= 0) {
		M_io_net_set_ephemeral_port(handle);
		handle->state = M_IO_NET_STATE_CONNECTED;
#ifdef _WIN32
		handle->data.net.evhandle = WSACreateEvent();
		WSAEventSelect(handle->data.net.sock, handle->data.net.evhandle, FD_READ|FD_WRITE|FD_CLOSE);
#else
		handle->data.net.evhandle = handle->data.net.sock;
#endif
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);
		return M_TRUE;
	}

	M_io_net_resolve_error(handle);
	if (handle->data.net.last_error != M_IO_ERROR_WOULDBLOCK) {
		M_io_net_handle_close(io, handle);
		return M_FALSE;
	}

	M_io_net_set_ephemeral_port(handle);
#ifdef _WIN32
	handle->data.net.evhandle = WSACreateEvent();
	WSAEventSelect(handle->data.net.sock, handle->data.net.evhandle, FD_CONNECT|FD_READ|FD_WRITE|FD_CLOSE);
#else
	handle->data.net.evhandle = handle->data.net.sock;
#endif

	return M_TRUE;
}


static M_bool M_io_net_stream_init_cb(M_io_layer_t *layer)
{
	M_io_t        *comm   = M_io_layer_get_io(layer);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_event_t     *event  = M_io_get_event(comm);

	handle->timer = M_event_timer_add(event, M_io_net_timeout_cb, layer);

	if (handle->state == M_IO_NET_STATE_CONNECTED) {
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);
		return M_TRUE;
	}

	if (handle->state == M_IO_NET_STATE_INIT) {
		struct sockaddr  *peer;
		socklen_t         peer_size;
		int               type;

		peer = M_io_net_addr2peer(handle->host, handle->port, &peer_size, &type);
		if (peer == NULL) {
			/* Fake an error code.  Really, this shouldn't be possible though */
			handle->data.net.last_error = M_IO_ERROR_INVALID;
#ifdef _WIN32
			handle->data.net.last_error_sys = WSAEADDRNOTAVAIL;
#else
			handle->data.net.last_error_sys = EADDRNOTAVAIL;
#endif
			return M_FALSE;
		}
		if (!M_io_net_start_connect(layer, peer, peer_size, type)) {
			/* If we can't start to connect, trigger an error immediately */
			M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, handle->data.net.last_error);
			M_free(peer);
			return M_TRUE;
		}
		M_free(peer);
	}

	if (handle->state == M_IO_NET_STATE_CONNECTING) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_WRITE, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);

		if (handle->settings.connect_timeout_ms != 0) {
			M_event_timer_set_firecount(handle->timer, 1);
			M_event_timer_reset(handle->timer, handle->settings.connect_timeout_ms);
		}
	}

	if (handle->state == M_IO_NET_STATE_DISCONNECTING) {
		/* Really, someone is registering it just to wait on a disconnect? */
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, comm, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);
		M_event_timer_set_firecount(handle->timer, 1);
		M_event_timer_reset(handle->timer, handle->settings.disconnect_timeout_ms); /* How long to wait for a disconnect */
	}

	return M_TRUE;
}


static M_bool M_io_net_init_cb(M_io_layer_t *layer)
{
	if (M_io_get_type(M_io_layer_get_io(layer)) == M_IO_TYPE_LISTENER)
		return M_io_net_listen_init_cb(layer);
	return M_io_net_stream_init_cb(layer);
}


static M_bool M_io_net_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (handle->state != M_IO_NET_STATE_CONNECTED || M_io_get_type(io) != M_IO_TYPE_STREAM) {
		/* Already been called, tell caller to wait longer */
		if (handle->state == M_IO_NET_STATE_DISCONNECTING)
			return M_FALSE;
		return M_TRUE;
	}

	handle->state = M_IO_NET_STATE_DISCONNECTING;

	/* Tell the remote end we want to shutdown */
#ifdef _WIN32
	/* If unable to close gracefully, go ahead and say we're disconnected */
	if (shutdown(handle->data.net.sock, SD_SEND) == SOCKET_ERROR)
		return M_TRUE;
#else
	/* If unable to close gracefully, go ahead and say we're disconnected */
	if (shutdown(handle->data.net.sock, SHUT_WR) != 0)
		return M_TRUE;
#endif
	/* Make sure we re-activate waiting on a read event if it is not active as that is the only
	 * way we will receive the disconnect notification */
	M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, io, handle->data.net.evhandle, handle->data.net.sock, M_EVENT_WAIT_READ, 0);

	/* Start a timer to forcibly shutdown if too long */
	if (handle->timer != NULL) {
		M_event_timer_set_firecount(handle->timer, 1);
		M_event_timer_reset(handle->timer, handle->settings.disconnect_timeout_ms); /* How long to wait for a disconnect */
	}
	return M_FALSE;
}


static void M_io_net_unregister_cb(M_io_layer_t *layer)
{
	M_io_t        *comm   = M_io_layer_get_io(layer);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_event_t     *event  = M_io_get_event(comm);

	if (handle->data.net.evhandle != M_EVENT_INVALID_HANDLE) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, comm, handle->data.net.evhandle, handle->data.net.sock, 0, 0);
	}

	if (handle->timer != NULL) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

}

static M_io_state_t M_io_net_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	switch (handle->state) {
		case M_IO_NET_STATE_INIT:
			return M_IO_STATE_INIT;
		case M_IO_NET_STATE_CONNECTING:
			return M_IO_STATE_CONNECTING;
		case M_IO_NET_STATE_CONNECTED:
			return M_IO_STATE_CONNECTED;
		case M_IO_NET_STATE_DISCONNECTING:
			return M_IO_STATE_DISCONNECTING;
		case M_IO_NET_STATE_DISCONNECTED:
			return M_IO_STATE_DISCONNECTED;
		case M_IO_NET_STATE_ERROR:
			return M_IO_STATE_ERROR;
		case M_IO_NET_STATE_LISTENING:
			return M_IO_STATE_LISTENING;
		case M_IO_NET_STATE_RESOLVING:
			return M_IO_STATE_CONNECTING;
	}
	return M_IO_STATE_INIT;
}


static M_bool M_io_net_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_bool         rv;

	if (handle->state == M_IO_NET_STATE_DISCONNECTED) {
		M_snprintf(error, err_len, "Gracefully Closed Connection");
		rv = M_TRUE;
	} else {

#ifdef _WIN32
		rv = M_io_win32_errormsg(handle->data.net.last_error_sys, error, err_len);
#else
		rv = M_io_posix_errormsg(handle->data.net.last_error_sys, error, err_len);
#endif
	}

	return rv;
}


static M_io_error_t M_io_net_accept_cb(M_io_t *comm, M_io_layer_t *orig_layer);

static M_io_callbacks_t *M_io_net_callbacks_create(void)
{
	M_io_callbacks_t *callbacks;
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_net_init_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_net_accept_cb);
	M_io_callbacks_reg_read(callbacks, M_io_net_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_net_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_net_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_net_unregister_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_net_disconnect_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_net_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_net_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_net_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_net_errormsg_cb);
	return callbacks;
}

static M_io_error_t M_io_net_accept_cb(M_io_t *comm, M_io_layer_t *orig_layer)
{
	M_io_handle_t          *handle;
	M_io_handle_t          *orig_handle = M_io_layer_get_handle(orig_layer);
	M_io_callbacks_t       *callbacks;
#ifdef HAVE_SOCKADDR_STORAGE
	struct sockaddr_storage sockaddr;
#else
	struct sockaddr_in      sockaddr;
#endif
	struct sockaddr        *sockaddr_ptr    = (struct sockaddr *)&sockaddr;
	socklen_t               sockaddr_size   = sizeof(sockaddr);
	char                    addr[64]        = { 0 };
	char                    server_addr[64] = { 0 };

	M_mem_set(sockaddr_ptr, 0, (size_t)sockaddr_size);

	handle         = M_malloc_zero(sizeof(*handle));
	handle->port   = orig_handle->port;
	errno          = 0;
#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC)
	handle->data.net.sock   = accept4(orig_handle->data.net.sock, sockaddr_ptr, &sockaddr_size, SOCK_CLOEXEC);
#else
	handle->data.net.sock   = accept(orig_handle->data.net.sock, sockaddr_ptr, &sockaddr_size);
#endif
	if (handle->data.net.sock == M_EVENT_INVALID_SOCKET) {
		M_io_net_resolve_error(orig_handle);
		M_free(handle);
		return orig_handle->data.net.last_error;
	}

#ifndef _WIN32
	M_io_posix_fd_set_closeonexec(handle->data.net.sock, M_TRUE);
#endif

	handle->state  = M_IO_NET_STATE_CONNECTED;
	M_io_setnonblock(handle->data.net.sock);
#ifdef _WIN32
	handle->data.net.evhandle = WSACreateEvent();
	WSAEventSelect(handle->data.net.sock, handle->data.net.evhandle, FD_READ|FD_WRITE|FD_CLOSE);
#else
	handle->data.net.evhandle = handle->data.net.sock;
#endif

	if (sockaddr_ptr->sa_family == AF_INET) {
		struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)((void *)sockaddr_ptr);
		struct sockaddr_in  server_in;
		socklen_t           len         = sizeof(server_in);

		M_mem_set(&server_in, 0, (size_t)len);
		getsockname(handle->data.net.sock, (struct sockaddr *)&server_in, &len);
		M_dns_ntop(AF_INET, &server_in.sin_addr, server_addr, sizeof(server_addr));

		M_dns_ntop(AF_INET, &sockaddr_in->sin_addr, addr, sizeof(addr));
		handle->data.net.eport = sockaddr_in->sin_port;
		handle->type           = M_IO_NET_IPV4;
#ifdef AF_INET6
	} else if (sockaddr_ptr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sockaddr_in6 = (struct sockaddr_in6 *)((void *)sockaddr_ptr);
		struct sockaddr_in6  server_in;
		socklen_t            len         = sizeof(server_in);

		M_mem_set(&server_in, 0, (size_t)len);
		getsockname(handle->data.net.sock, (struct sockaddr *)&server_in, &len);
		M_dns_ntop(AF_INET6, &server_in.sin6_addr, server_addr, sizeof(server_addr));

		M_dns_ntop(AF_INET6, &sockaddr_in6->sin6_addr, addr, sizeof(addr));
		handle->data.net.eport = sockaddr_in6->sin6_port;
		handle->type           = M_IO_NET_IPV6;
#endif
	}
	if (!M_str_isempty(server_addr)) {
		if (handle->type == M_IO_NET_IPV6 && M_str_caseeq_max(addr, "::ffff:", 7)) {
			/* Rewrite an IPv4 connection coming in on an IPv6 listener as if it was IPv4 */
			handle->server_ipaddr = M_strdup(server_addr + 7);
		} else {
			handle->server_ipaddr = M_strdup(server_addr);
		}
	}
	if (!M_str_isempty(addr)) {
		if (handle->type == M_IO_NET_IPV6 && M_str_caseeq_max(addr, "::ffff:", 7)) {
			/* Rewrite an IPv4 connection coming in on an IPv6 listener as if it was IPv4 */
			handle->host = M_strdup(addr + 7);
			handle->type = M_IO_NET_IPV4;
		} else {
			handle->host = M_strdup(addr);
		}
	}

	/* Copy over any settings set on the server handle so they are preserved with the child */
	M_mem_copy(&handle->settings, &orig_handle->settings, sizeof(handle->settings));
	callbacks = M_io_net_callbacks_create();
	M_io_layer_add(comm, "NET", handle, callbacks);
	M_io_callbacks_destroy(callbacks);
	return M_IO_ERROR_SUCCESS;
}

void M_io_net_set_settings(M_io_t *io, M_io_net_settings_t *settings)
{
	M_io_layer_t  *layer = M_io_layer_acquire(io, 0, NULL);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_mem_copy(&handle->settings, settings, sizeof(*settings));
	M_io_layer_release(layer);
}


void M_io_net_settings_set_default(M_io_net_settings_t *settings)
{
	settings->connect_timeout_ms    = 10000;
	settings->disconnect_timeout_ms = 10000;
	settings->connect_failover_ms   = 100;

	settings->ka_enable             = M_FALSE;
	settings->nagle_enable          = M_FALSE;
}


M_io_t *M_io_netraw_client_create(const char *host, unsigned short port, M_io_net_type_t type)
{
	M_io_t           *io;
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	unsigned char     ip_bin[16];
	size_t            ip_bin_len = 0;

	if (M_str_isempty(host) || port == 0)
		return NULL;

	M_io_net_init_system();

	/* Validate host passed is a valid ip address for the type specified, and also
	 * set type to be more specific if ANY was passed */
	if (!M_io_net_ipaddr_to_bin(ip_bin, sizeof(ip_bin), host, &ip_bin_len))
		return NULL;

	switch (type) {
		case M_IO_NET_ANY:
			if (ip_bin_len == 4) {
				type = M_IO_NET_IPV4;
			} else if (ip_bin_len == 16) {
				type = M_IO_NET_IPV6;
			} else {
				return NULL;
			}
			break;
		case M_IO_NET_IPV4:
			if (ip_bin_len != 4)
				return NULL;
			break;
		case M_IO_NET_IPV6:
			if (ip_bin_len != 6)
				return NULL;
			break;
	}

	handle                                 = M_malloc_zero(sizeof(*handle));
	handle->data.net.evhandle              = M_EVENT_INVALID_HANDLE;
	handle->data.net.sock                  = M_EVENT_INVALID_SOCKET;
	handle->host                           = M_strdup(host);
	handle->port                           = port;
	handle->type                           = type;
	M_io_net_settings_set_default(&handle->settings);

	io                                     = M_io_init(M_IO_TYPE_STREAM);
	callbacks                              = M_io_net_callbacks_create();
	M_io_layer_add(io, "NET", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return io;
}


M_io_error_t M_io_net_server_create(M_io_t **io_out, unsigned short port, const char *bind_ip, M_io_net_type_t type)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_error_t      err;

	if (io_out == NULL)
		return M_IO_ERROR_INVALID;

	*io_out = NULL;

	M_io_net_init_system();

	/* XXX: Should start listening here so we can return errors instead of delaying until init_cb */
	handle                                 = M_malloc_zero(sizeof(*handle));
	handle->data.net.evhandle              = M_EVENT_INVALID_HANDLE;
	handle->data.net.sock                  = M_EVENT_INVALID_SOCKET;
	handle->host                           = M_strdup(bind_ip);
	handle->type                           = type;
	handle->port                           = port;
	M_io_net_settings_set_default(&handle->settings);

	err = M_io_net_listen_bind(handle);
	if (err != M_IO_ERROR_SUCCESS) {
		M_free(handle->host);
		M_free(handle);
		return err;
	}

	*io_out                                = M_io_init(M_IO_TYPE_LISTENER);
	callbacks                              = M_io_net_callbacks_create();
	M_io_layer_add(*io_out, "NET", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}


/* XXX: this shouldn't be here and isn't necessarily right for everything */
#ifdef _WIN32
M_bool M_io_setnonblock(SOCKET fd)
{
	unsigned long tf = 1;
	if (ioctlsocket(fd, (long)FIONBIO, &tf) == -1)
		return M_FALSE;
	return M_TRUE;
}
#else
M_bool M_io_setnonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return M_FALSE;

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return M_FALSE;

	return M_TRUE;
}
#endif
char *M_io_net_get_fqdn(void)
{
	char            hostname[NI_MAXHOST+1];
	struct hostent *h;

	if (gethostname(hostname, sizeof(hostname)) != 0)
		return NULL;

	if (*hostname == '\0')
		return NULL;

	h = gethostbyname(hostname);
	if (h == NULL || h->h_name == NULL || *h->h_name == '\0')
		return M_strdup(hostname);

	return M_strdup(h->h_name);
}


M_bool M_io_net_set_keepalives(M_io_t *io, M_uint64 idle_time_s, M_uint64 retry_time_s, M_uint64 retry_cnt)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	handle->settings.ka_enable       = M_TRUE;
	handle->settings.ka_idle_time_s  = idle_time_s;
	handle->settings.ka_retry_time_s = retry_time_s;
	handle->settings.ka_retry_cnt    = retry_cnt;
	if (handle->is_netdns && handle->data.netdns.io != NULL) {
		M_io_net_set_keepalives(handle->data.netdns.io, idle_time_s, retry_time_s, retry_cnt);
	}

	/* XXX: should we set in realtime? */

	M_io_layer_release(layer);
	return M_TRUE;
}


M_bool M_io_net_set_nagle(M_io_t *io, M_bool nagle_enabled)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	handle->settings.nagle_enable = nagle_enabled;

	if (handle->is_netdns && handle->data.netdns.io != NULL) {
		M_io_net_set_nagle(handle->data.netdns.io, nagle_enabled);
	}

	/* XXX: should we set in realtime? */
	M_io_layer_release(layer);
	return M_TRUE;

}


M_bool M_io_net_set_connect_timeout_ms(M_io_t *io, M_uint64 timeout_ms)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	if (timeout_ms == 0)
		timeout_ms = 10;

	handle->settings.connect_timeout_ms = timeout_ms;

	M_io_layer_release(layer);
	return M_TRUE;
}

M_uint64 M_io_net_get_connect_timeout_ms(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint64       ret;

	if (layer == NULL || handle == NULL)
		return 0;

	ret = handle->settings.connect_timeout_ms;

	M_io_layer_release(layer);
	return ret;
}

const char *M_io_net_get_host(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	const char    *ret    = NULL;

	if (layer == NULL || handle == NULL)
		return NULL;

	ret = handle->host;

	M_io_layer_release(layer);

	return ret;
}


const char *M_io_net_get_ipaddr(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	const char    *ret    = NULL;

	if (layer == NULL || handle == NULL)
		return NULL;

	if (handle->is_netdns) {
		ret = M_io_net_get_ipaddr(handle->data.netdns.io);
	} else {
		ret = handle->host;
	}

	M_io_layer_release(layer);
	return ret;
}


const char *M_io_net_get_server_ipaddr(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	const char    *ret    = NULL;

	if (layer == NULL || handle == NULL)
		return NULL;

	ret = handle->server_ipaddr;

	M_io_layer_release(layer);
	return ret;
}


unsigned short M_io_net_get_port(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	unsigned short port   = 0;

	if (layer == NULL || handle == NULL)
		return 0;

	port = handle->port;

	M_io_layer_release(layer);
	return port;
}


unsigned short M_io_net_get_ephemeral_port(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	unsigned short port   = 0;

	if (layer == NULL || handle == NULL)
		return 0;

	if (handle->is_netdns) {
		port = M_io_net_get_ephemeral_port(handle->data.netdns.io);
	} else {
		port = handle->data.net.eport;
	}

	M_io_layer_release(layer);
	return port;
}


enum M_io_net_type M_io_net_get_type(M_io_t *io)
{
	M_io_layer_t      *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t     *handle = M_io_layer_get_handle(layer);
	enum M_io_net_type type;

	if (layer == NULL || handle == NULL)
		return 0;

	if (handle->is_netdns && handle->data.netdns.io != NULL) {
		type = M_io_net_get_type(handle->data.netdns.io);
	} else {
		type = handle->type;
	}

	M_io_layer_release(layer);
	return type;
}


M_bool M_io_net_ipaddr_to_bin(unsigned char *ipaddr_bin, size_t ipaddr_bin_size, const char *ipaddr_str, size_t *ipaddr_bin_len)
{
	if (M_str_isempty(ipaddr_str) || ipaddr_bin == NULL || ipaddr_bin_size < 16 || ipaddr_bin_len == NULL)
		return M_FALSE;

	*ipaddr_bin_len = 0;

	if (M_dns_pton(AF_INET, ipaddr_str, ipaddr_bin)) {
		*ipaddr_bin_len = 4;
		return M_TRUE;
	}

#ifdef AF_INET6
	if (M_dns_pton(AF_INET6, ipaddr_str, ipaddr_bin)) {
		*ipaddr_bin_len = 16;
		return M_TRUE;
	}
#endif

	return M_FALSE;
}


M_bool M_io_net_bin_to_ipaddr(char *ipaddr_str, size_t ipaddr_str_size, const unsigned char *ipaddr_bin, size_t ipaddr_bin_len)
{
	int family;

	if (ipaddr_str == NULL || ipaddr_str_size == 0 || ipaddr_bin == NULL || (ipaddr_bin_len != 4 && ipaddr_bin_len != 16))
		return M_FALSE;

	if (ipaddr_bin_len == 4) {
		family = AF_INET;
		if (ipaddr_str_size < 16)
			return M_FALSE;
#ifdef AF_INET6
	} else if (ipaddr_bin_len == 16) {
		family = AF_INET6;
		if (ipaddr_str_size < 40)
			return M_FALSE;
#endif
	} else {
		return M_FALSE;
	}


	return M_dns_ntop(family, ipaddr_bin, ipaddr_str, ipaddr_str_size);
}
