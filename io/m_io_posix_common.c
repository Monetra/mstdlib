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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_io_posix_common.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_PTHREAD
#  include <pthread.h>
#endif
#include <signal.h>

M_io_error_t M_io_posix_err_to_ioerr(int err)
{
	switch (err) {
		case 0:
			return M_IO_ERROR_SUCCESS;
		case EAGAIN:
#  if defined(EAGAIN) && defined(EWOULDBLOCK) && EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#  endif
		case EINPROGRESS:
			return M_IO_ERROR_WOULDBLOCK;
		case EINTR:
			return M_IO_ERROR_INTERRUPTED;
		case ENOTCONN:
			return M_IO_ERROR_NOTCONNECTED;
		case EACCES:
		case EPERM:
			return M_IO_ERROR_NOTPERM;
		case ECONNRESET:
		case ENETRESET:
			return M_IO_ERROR_CONNRESET;
		case ECONNABORTED:
			return M_IO_ERROR_CONNABORTED;
		case EPIPE:
			return M_IO_ERROR_DISCONNECT;
		case EADDRINUSE:
			return M_IO_ERROR_ADDRINUSE;
		case EAFNOSUPPORT:
		case EPROTONOSUPPORT:
			return M_IO_ERROR_PROTONOTSUPPORTED;
		case ECONNREFUSED:
			return M_IO_ERROR_CONNREFUSED;
		case ENETUNREACH:
		case EHOSTUNREACH:
		case ENETDOWN:
			return M_IO_ERROR_NETUNREACHABLE;
		case ETIMEDOUT:
			return M_IO_ERROR_TIMEDOUT;
		case EMFILE:
		case ENFILE:
		case ENOBUFS:
		case ENOMEM:
			return M_IO_ERROR_NOSYSRESOURCES;
		case ENOTSOCK:
		case EBADF:
		case EFAULT:
		case EINVAL:
		default:
			break;
	}
	return M_IO_ERROR_ERROR;
}


M_bool M_io_posix_errormsg(int err, char *error, size_t err_len)
{
	char           buf[256];
	const char    *const_temp;

	M_mem_set(buf, 0, sizeof(buf));

	if (err == 0)
		return M_FALSE;

#  if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && (!defined(__linux__) || !defined(_GNU_SOURCE))
	if (strerror_r(err, buf, sizeof(buf)) != 0)
		return M_FALSE;
	const_temp = buf;
#  elif defined(__linux__) && defined(_GNU_SOURCE)
	const_temp = strerror_r(err, buf, sizeof(buf));
	if (const_temp == NULL)
		return M_FALSE;
#  else
	const_temp = strerror(err);
	if (const_temp == NULL)
		return M_FALSE;
#  endif

	M_snprintf(error, err_len, "%s", const_temp);

	return M_TRUE;
}


M_io_error_t M_io_posix_read(M_io_t *io, int fd, unsigned char *buf, size_t *read_len, int *sys_error)
{
	size_t         request_len;
	ssize_t        retval;
	M_io_error_t   err;

	if (io == NULL || buf == NULL || read_len == NULL || *read_len == 0 || sys_error == NULL)
		return M_IO_ERROR_INVALID;

	if (fd == -1)
		return M_IO_ERROR_ERROR;

	*sys_error  = 0;
	errno       = 0;
	request_len = *read_len;
	retval      = read(fd, buf, *read_len);
	if (retval == 0) {
		/* NOTE: On Serial COMMs, this could return 0 if termios c_cc[VMIN] = 0 if there are no bytes available,
		 *       so make sure that is set to '1' instead to operate more like any other fd */
		err        = M_IO_ERROR_DISCONNECT;
	} else if (retval < 0) {
		*sys_error = errno;
		err        = M_io_posix_err_to_ioerr(*sys_error);
	} else {
		*read_len  = (size_t)retval;
		err        = M_IO_ERROR_SUCCESS;
	}

	if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && request_len >= *read_len)) { /* >= to account for known complete reads by layer */
		/* Start waiting on more read events */
		M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_ADD_WAITTYPE, io, fd, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, 0);
	} else if (err == M_IO_ERROR_SUCCESS) {
		/* Stop waiting on more read events */
		M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_DEL_WAITTYPE, io, fd, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, 0);
	}

	return err;
}


M_io_error_t M_io_posix_write(M_io_t *io, int fd, const unsigned char *buf, size_t *write_len, int *sys_error)
{
	size_t                     request_len;
	ssize_t                    retval;
	M_io_error_t               err;
	M_io_posix_sigpipe_state_t sigpipe_state;

	if (io == NULL || buf == NULL || write_len == NULL || *write_len == 0 || sys_error == NULL)
		return M_IO_ERROR_INVALID;

	if (fd == -1)
		return M_IO_ERROR_ERROR;

	M_io_posix_sigpipe_block(&sigpipe_state);

	*sys_error  = 0;
	errno       = 0;
	request_len = *write_len;
	retval      = write(fd, buf, *write_len);
	if (retval <= 0) {
		*sys_error = errno;
		err        = M_io_posix_err_to_ioerr(*sys_error);
	} else {
		*write_len = (size_t)retval;
		err        = M_IO_ERROR_SUCCESS;
	}

	M_io_posix_sigpipe_unblock(&sigpipe_state);

	if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && request_len > *write_len)) {
		/* Start waiting on more write events */
		M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_ADD_WAITTYPE, io, fd, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_WRITE, 0);
	} else if (err == M_IO_ERROR_SUCCESS) {
		/* Stop waiting on more write events */
		M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_DEL_WAITTYPE, io, fd, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_WRITE, 0);
	}

	return err;
}


M_bool M_io_posix_process_cb(M_io_layer_t *layer, M_EVENT_HANDLE rhandle, M_EVENT_HANDLE whandle, M_event_type_t *type)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	switch (*type) {
		case M_EVENT_TYPE_READ:
			/* Wait for resettable condition */
			if (rhandle != M_EVENT_INVALID_HANDLE) {
				M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, io, rhandle, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, 0);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			/* Wait for resettable condition */
			if (whandle != M_EVENT_INVALID_HANDLE) {
				M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, io, whandle, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_WRITE, 0);
			}
			break;
		default:
			break;
	}
	return M_FALSE;
}


void M_io_posix_sigpipe_block(M_io_posix_sigpipe_state_t *state)
{
	sigset_t pending;
	sigset_t old_mask;
	sigset_t sigpipe_mask;

	if (state == NULL)
		return;

	M_mem_set(state, 0, sizeof(*state));

	/* Check to see if the signal was already pending before we entered, if
	 * so, there's nothing we need to do */
	sigemptyset(&pending);
	sigpending(&pending);
	if (sigismember(&pending, SIGPIPE)) {
		state->already_pending = M_TRUE;
		return;
	}

	/* Create our signal mask */
	sigemptyset(&sigpipe_mask);
	sigaddset(&sigpipe_mask, SIGPIPE);

	/* Apply our signal mask */
	sigemptyset(&old_mask);
#ifdef HAVE_PTHREAD
	pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &old_mask);
#else
	sigprocmask(SIG_BLOCK, &sigpipe_mask, &old_mask);
#endif

	/* If the old mask did NOT already have sigpipe blocked, store that we blocked it */
	if (!sigismember(&old_mask, SIGPIPE))
		state->blocked = M_TRUE;
}


void M_io_posix_sigpipe_unblock(M_io_posix_sigpipe_state_t *state)
{
	sigset_t pending;
	sigset_t sigpipe_mask;

	if (state == NULL)
		return;

	/* Do nothing if sigpipe was already pending */
	if (state->already_pending)
		return;

	/* See if we generated a signal or not */
	sigemptyset(&pending);
	sigpending(&pending);
	sigemptyset(&sigpipe_mask);
	sigaddset(&sigpipe_mask, SIGPIPE);

	if (sigismember(&pending, SIGPIPE)) {
		/* Consume the signal from the signal queue */
#if defined(HAVE_SIGTIMEDWAIT) && _POSIX_C_SOURCE >= 199309L
		/* Prefer sigtimedwait this as it has the advantage of making sure we can't ever deadlock */
		const struct timespec timeout = { 0, 0 };

		/* Loop because other signals could cause an EINTR */
		while (sigtimedwait(&sigpipe_mask, NULL, &timeout) == -1 && errno == EINTR)
			;
#elif defined(HAVE_SIGWAIT) && !defined(__SCO_VERSION__) /* SCO6 has odd single-arg sigwait */
		/* Loop because other signals could cause an EINTR */
		while (sigwait(&sigpipe_mask, NULL) == -1 && errno == EINTR)
			;
#else
		/* WTF, braindead system (probably SCO5) */
		struct sigaction new_action;
		struct sigaction old_action;
		sigset_t         suspendmask;

		/* Set the default handler to ignore the signal */
		new_action.sa_handler = SIG_IGN;
		sigemptyset(&new_action.sa_mask);
		new_action.sa_flags   = 0;
		sigaction(SIGPIPE, &new_action, &old_action);

		/* Grab the current signal mask, remove SIGPIPE from the mask, then pass that
		 * to sigsuspend to wait on SIGPIPE ... when sigsuspend returns we should
		 * be guaranteed to have consumed SIGPIPE since we knew it was pending and
		 * that was the only signal we unblocked */
		sigprocmask(SIG_BLOCK, NULL, &suspendmask);
		sigdelset(&suspendmask, SIGPIPE);
		sigsuspend(&suspendmask);

		/* Restore the original SIGPIPE signal handler */
		sigaction(SIGPIPE, &old_action, NULL);
#endif
	}

	/* Unblock the signal if we were the ones that blocked it */
	if (state->blocked) {
#ifdef HAVE_PTHREAD
		pthread_sigmask(SIG_UNBLOCK, &sigpipe_mask, NULL);
#else
		sigprocmask(SIG_UNBLOCK, &sigpipe_mask, NULL);
#endif
	}
}


void M_io_posix_fd_set_closeonexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	int rv;
	if (flags == -1)
		return;

	rv = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	(void)rv; /* Appease coverity, really if this fails, its not a huge deal */
}


