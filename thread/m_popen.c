/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/thread/m_thread_system.h>
#include "base/platform/m_platform.h"

#include <stdio.h>
#include <stdlib.h>
/* Needed because FD_ZERO macro calls memset. */
#include <string.h>

#ifndef _WIN32
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <signal.h>
#endif

#ifdef __SCO_VERSION__
# include <sys/time.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define EXEC "exec"

#ifdef _WIN32
#  define M_POPEN_BAD_FD NULL
#  define M_POPEN_HANDLE HANDLE
#else
#  define M_POPEN_BAD_FD -1
#  define M_POPEN_HANDLE int
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_popen_handle {
#ifdef _WIN32
	/* Process Info (basically pidlike) */
	PROCESS_INFORMATION processInfo;
#else
	/* Pid of Child */
	pid_t          pid;
#endif
	/* Write, Read, Error file handles,
	 * from the perspective of the caller */
	M_POPEN_HANDLE stdin_fd;
	M_POPEN_HANDLE stdout_fd;
	M_POPEN_HANDLE stderr_fd;

	/* errorcode or result */
	M_popen_err_t  errcode;
	int            resultcode;

	/* Whether or not the child has exited */
	M_bool         done;
};


static M_POPEN_HANDLE *M_popen_getfd(M_popen_handle_t *mp, M_popen_fd_t fd)
{
	if (mp == NULL) {
		return NULL;
	}

	switch (fd) {
		case M_POPEN_FD_WRITE:
			return &mp->stdin_fd;
		case M_POPEN_FD_READ:
			return &mp->stdout_fd;
		case M_POPEN_FD_ERR:
			return &mp->stderr_fd;
	}
	return NULL;
}


ssize_t M_popen_read(M_popen_handle_t *mp, M_popen_fd_t fd, char *out, size_t out_len, M_uint64 timeout_ms)
{
	M_POPEN_HANDLE *handle;
	ssize_t         retval     = 0;
#ifdef _WIN32
	DWORD           err        = 0;
	unsigned long   bytesAval  = 0;
	int             first      = 1;
	DWORD           d_out_len;
	DWORD           bytes_read = 0;
#else
	struct pollfd   fds[1];
#endif

	if (mp == NULL || (fd != M_POPEN_FD_READ && fd != M_POPEN_FD_ERR))
		return -1;

	handle = M_popen_getfd(mp, fd);
	if (handle == NULL || *handle == M_POPEN_BAD_FD)
		return -1;

#ifdef _WIN32
	if (!M_win32_size_t_to_dword(out_len, &d_out_len))
		return -1;

	if (timeout_ms == M_TIMEOUT_INF) {
		if (!ReadFile(*handle, out, d_out_len, &bytes_read, NULL)) {
			err = GetLastError();
		}
	} else {
		do {
			/* We don't want to sleep on the first iteration of the loop */
			if (first) {
				first = 0;
			} else {
				M_thread_sleep(15000);
			}

			/* Check if we have data on the pipe and if so read it */
			if (PeekNamedPipe(*handle, NULL, 0, NULL, &bytesAval, NULL)) {
				/* PeekNamedPipe can succeed even if there is no data so we
				 * check if there really is data available for reading. */
				if (bytesAval > 0) {
					if (ReadFile(*handle, out, d_out_len, &bytes_read, NULL)) {
						break;
					} else {
						err = GetLastError();
						break;
					}
				}
			} else {
				err = GetLastError();
				break;
			}

			timeout_ms = (timeout_ms > 15) ? timeout_ms - 15 : 0;
		} while (timeout_ms > 0);
	}
	retval = bytes_read;
	if (err != 0) {
		if (err == ERROR_HANDLE_EOF || err == ERROR_BROKEN_PIPE) {
			retval = -2;
		} else {
			retval = -1;
		}
	}
#else
	if (timeout_ms != M_TIMEOUT_INF) {
		M_mem_set(&fds[0], 0, sizeof(*fds));
		fds[0].fd     = *handle;
		fds[0].events = POLLIN;

		retval = M_thread_poll(fds, 1, (int)timeout_ms);
		if (retval == -1) {
			return 0;
		}
	}

	if (timeout_ms == M_TIMEOUT_INF || fds[0].revents & (POLLIN|POLLHUP|POLLERR)) {
		retval = read(*handle, out, out_len);
		if (retval == 0) {
			retval = -2;
		}
	}
#endif

	/* Close the fd if we have an error or signal that the pipe is closed. */
	if (retval < 0) {
		M_popen_closefd(mp, fd);
	}

	return retval;
}


ssize_t M_popen_write(M_popen_handle_t *mp, M_popen_fd_t fd, const char *in, size_t in_len)
{
	M_POPEN_HANDLE *handle;
	ssize_t         retval;
#ifdef _WIN32
	DWORD           d_in_len;
	DWORD           bytes_written;
#endif

	if (mp == NULL || fd != M_POPEN_FD_WRITE)
		return -1;

	handle = M_popen_getfd(mp, fd);
	if (handle == NULL || *handle == M_POPEN_BAD_FD)
		return -1;

	if (in_len == 0)
		return 0;

#ifdef _WIN32
	if (!M_win32_size_t_to_dword(in_len, &d_in_len))
		return -1;
	if (!WriteFile(*handle, in, d_in_len, &bytes_written, NULL)) {
		retval = -1;
	} else {
		retval = bytes_written;
	}
#else
	retval = write(*handle, in, in_len);
	if (retval == 0) {
		retval = -1;
	}
#endif

	if (retval < 0) {
		M_popen_closefd(mp, fd);
	}

	return retval;
}


int M_popen_closefd(M_popen_handle_t *mp, M_popen_fd_t fd)
{
	M_POPEN_HANDLE *handle;
	int             retval;

	if (mp == NULL) {
		return 0;
	}

	handle = M_popen_getfd(mp, fd);
	if (handle == NULL || *handle == M_POPEN_BAD_FD)
		return 0;

#ifdef _WIN32
	retval = CloseHandle(*handle);
#else
	retval = close(*handle) == 0?1:0;
#endif
	*handle = M_POPEN_BAD_FD;
	return retval;
}


#ifdef _WIN32
static M_popen_status_t M_popen_check_int(M_popen_handle_t *mp, M_uint64 timeout)
{
	DWORD retval;
	DWORD resultcode;

	if (mp == NULL) {
		return M_POPEN_STATUS_ERROR;
	}

	if (mp->done)
		return M_POPEN_STATUS_DONE;

	retval = WaitForSingleObject(mp->processInfo.hProcess, (timeout == M_TIMEOUT_INF) ? INFINITE : (DWORD)timeout);
	switch (retval) {
		case WAIT_OBJECT_0:
			GetExitCodeProcess(mp->processInfo.hProcess, &resultcode);
			mp->resultcode = (int)resultcode;
			mp->errcode    = M_POPEN_ERR_NONE;
			mp->done       = M_TRUE;
			return M_POPEN_STATUS_DONE;

		case WAIT_TIMEOUT:
			return M_POPEN_STATUS_RUNNING;
		case WAIT_ABANDONED:
		case WAIT_FAILED:
		default:
			break;
	};

	mp->resultcode = -1;
	mp->errcode    = M_POPEN_ERR_WAIT;
	mp->done       = M_TRUE;
	return M_POPEN_STATUS_ERROR;
}

#else

static void M_popen_set_statuscode(M_popen_handle_t *mp, int status)
{
	if (mp == NULL) {
		return;
	}

	mp->resultcode = -1;
	mp->errcode    = M_POPEN_ERR_WAIT;

	if (WIFEXITED(status)) {
		switch (WEXITSTATUS(status)) {
			case 124:
			case 125:
				mp->errcode    = M_POPEN_ERR_NOEXEC;
				return;
			case 126:
				mp->errcode    = M_POPEN_ERR_PERM;
				return;
			case 127:
				mp->errcode    = M_POPEN_ERR_CMDNOTFOUND;
				return;
			default:
				mp->errcode    = M_POPEN_ERR_NONE;
				mp->resultcode = WEXITSTATUS(status);
				return;
		}
	}

	if (WIFSIGNALED(status)) {
		mp->errcode = M_POPEN_ERR_KILLSIGNAL;
		return;
	}
}


static M_popen_status_t M_popen_check_int(M_popen_handle_t *mp, M_uint64 timeout)
{
	int   first;
	int   status;
	pid_t retval;

	if (mp == NULL) {
		return M_POPEN_STATUS_ERROR;
	}

	if (mp->done)
		return M_POPEN_STATUS_DONE;

	first = 1;
	do {
		if (first) {
			first = 0;
		} else {
			M_thread_sleep(15000);
		}
		retval = waitpid(mp->pid, &status, (timeout == M_TIMEOUT_INF) ? 0 : WNOHANG);
		if (retval == -1 && errno == EINTR) {
			first = 1;
			continue;
		} else if (retval == -1) {
			mp->done       = M_TRUE;
			mp->resultcode = -1;
			mp->errcode    = M_POPEN_ERR_WAIT;
			return M_POPEN_STATUS_ERROR;
		} else if (retval == 0) {
			/* ignore */
		} else {
			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				M_popen_set_statuscode(mp, status);
				mp->done = M_TRUE;
				return M_POPEN_STATUS_DONE;
			}
		}
		timeout = (timeout > 15) ? timeout - 15 : 0;
	} while (timeout > 0);

	return M_POPEN_STATUS_RUNNING;
}
#endif


M_popen_status_t M_popen_check(M_popen_handle_t *mp)
{
	return M_popen_check_int(mp, 0);
}


int M_popen_close_ex(M_popen_handle_t *mp, char **stdout_buf, size_t *stdout_buf_len, char **stderr_buf, size_t *stderr_buf_len, M_popen_err_t *errorid, M_uint64 timeout_ms)
{
	M_buf_t *mbuf;
	char     buf[4096];
	int      ret;
	ssize_t  result;

	if (stdout_buf != NULL)
		*stdout_buf = NULL;
	if (stdout_buf_len != NULL)
		*stdout_buf_len = 0;
	if (stderr_buf != NULL)
		*stderr_buf = NULL;
	if (stderr_buf_len != NULL)
		*stderr_buf_len = 0;

	if (mp == NULL) {
		return 0;
	}

	/* Close the write handle if it is still open just in case the process is
	 * expecting to read data */
	M_popen_closefd(mp, M_POPEN_FD_WRITE);

	/* Wait till the process exits or kill it if the timeout elapsed */
	if (M_popen_check_int(mp, timeout_ms) == M_POPEN_STATUS_RUNNING) {
#ifdef _WIN32
		TerminateProcess(mp->processInfo.hProcess, 0);
#else
		kill(mp->pid, SIGKILL);
#endif
		M_popen_check_int(mp, M_TIMEOUT_INF);
		mp->errcode = M_POPEN_ERR_KILLSIGNAL;
		mp->resultcode = -2;
	}

	if (mp->resultcode >= 0) {
		if (stdout_buf != NULL && stdout_buf_len != NULL && mp->stdout_fd != M_POPEN_BAD_FD) {
			mbuf = M_buf_create();
			while ((result = M_popen_read(mp, M_POPEN_FD_READ, buf, sizeof(buf), M_TIMEOUT_INF)) > 0) {
				M_buf_add_bytes(mbuf, buf, (size_t)result);
			}
			*stdout_buf = M_buf_finish_str(mbuf, stdout_buf_len);
		}


		if (stderr_buf != NULL && stderr_buf_len != NULL && mp->stderr_fd != M_POPEN_BAD_FD) {
			mbuf = M_buf_create();
			while ((result = M_popen_read(mp, M_POPEN_FD_ERR, buf, sizeof(buf), M_TIMEOUT_INF)) > 0) {
				M_buf_add_bytes(mbuf, buf, (size_t)result);
			}
			*stderr_buf = M_buf_finish_str(mbuf, stderr_buf_len);
		}
	}

	/* Close handles if they're open */
	M_popen_closefd(mp, M_POPEN_FD_READ);
	M_popen_closefd(mp, M_POPEN_FD_ERR);

#ifdef _WIN32
	if (mp->processInfo.hProcess != NULL)
		CloseHandle(mp->processInfo.hProcess);
	if (mp->processInfo.hThread != NULL)
		CloseHandle(mp->processInfo.hThread);
#endif

	if (errorid != NULL) {
		*errorid = mp->errcode;
	}
	ret = mp->resultcode;

	M_free(mp);

	return ret;
}


int M_popen_close(M_popen_handle_t *mp, M_popen_err_t *errorid)
{
	return M_popen_close_ex(mp, NULL, NULL, NULL, NULL, errorid, 0);
}

#ifdef _WIN32
M_popen_handle_t *M_popen(const char *cmd, M_popen_err_t *errorid)
{
	M_popen_handle_t    *mp;
	char                *mycmd;
	M_POPEN_HANDLE       stdin_fd[2];
	M_POPEN_HANDLE       stdout_fd[2];
	M_POPEN_HANDLE       stderr_fd[2];
	SECURITY_ATTRIBUTES  saAttr;
	STARTUPINFO          siStartInfo;
	DWORD                err;
	BOOL                 ret;

	mp = M_malloc_zero(sizeof(*mp));

	/* Allow child to inherit handles */
	M_mem_set(&saAttr, 0, sizeof(saAttr));
	saAttr.nLength              = sizeof(saAttr);
	saAttr.bInheritHandle       = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	/* Create stdin pipes */
	if (!CreatePipe(&stdin_fd[0], &stdin_fd[1], &saAttr, 0) ||
	    !SetHandleInformation(stdin_fd[1], HANDLE_FLAG_INHERIT, 0)) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		return NULL;
	}

	/* Create stdout pipes */
	if (!CreatePipe(&stdout_fd[0], &stdout_fd[1], &saAttr, 0) ||
	    !SetHandleInformation(stdout_fd[0], HANDLE_FLAG_INHERIT, 0)) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		CloseHandle(stdin_fd[0]);
		CloseHandle(stdin_fd[1]);
		return NULL;
	}

	/* Create stderr pipes */
	if (!CreatePipe(&stderr_fd[0], &stderr_fd[1], &saAttr, 0) ||
	    !SetHandleInformation(stderr_fd[0], HANDLE_FLAG_INHERIT, 0)) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		CloseHandle(stdin_fd[0]);
		CloseHandle(stdin_fd[1]);
		CloseHandle(stdout_fd[0]);
		CloseHandle(stdout_fd[1]);
		return NULL;
	}

	mp->stdin_fd  = stdin_fd[1];
	mp->stdout_fd = stdout_fd[0];
	mp->stderr_fd = stderr_fd[0];

	M_mem_set(&siStartInfo, 0, sizeof(siStartInfo));
	siStartInfo.cb = sizeof(siStartInfo);
	siStartInfo.hStdInput  = stdin_fd[0];
	siStartInfo.hStdOutput = stdout_fd[1];
	siStartInfo.hStdError  = stderr_fd[1];
	siStartInfo.dwFlags   |= STARTF_USESTDHANDLES;

	mycmd = M_strdup(cmd);
	/* From http://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx
 	 * The Unicode version of this function, CreateProcessW, can modify the contents of this string.
 	 * Therefore, this parameter cannot be a pointer to read-only memory (such as a const variable or a
	 * literal string). If this parameter is a constant string, the function may cause an access violation.
	 *
	 * Even though we're specifically calling CreateProcessA the prototype still doesn't take a const value
	 * for cmd. We're wrapping in a non-const str partly to prevent errors if this is ever changed from the A
	 * version and to silence compiler warnings. Not to mention this function takes a non-const value, so we
	 * shouldn't assume that future versions of Windows won't modify the value (which is the assumption when
	 * using a const. */
	ret   = CreateProcessA(NULL, mycmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &mp->processInfo);
	M_free(mycmd);
	if (!ret) {
		err = GetLastError();
		switch (err) {
			case ERROR_FILE_NOT_FOUND:
				*errorid = M_POPEN_ERR_CMDNOTFOUND;
				break;
			case ERROR_BAD_EXE_FORMAT:
			case ERROR_INVALID_EXE_SIGNATURE:
				*errorid = M_POPEN_ERR_NOEXEC;
				break;
			default:
				//printf("GetLastError=%d", (int)GetLastError());
				*errorid = M_POPEN_ERR_SPAWN;
				break;
		};
		CloseHandle(stdin_fd[0]);
		CloseHandle(stdin_fd[1]);
		CloseHandle(stdout_fd[0]);
		CloseHandle(stdout_fd[1]);
		CloseHandle(stderr_fd[0]);
		CloseHandle(stderr_fd[1]);
		return NULL;
	}

	/* Close the child's side of the pipe in the parent! */
	CloseHandle(stdin_fd[0]);
	CloseHandle(stdout_fd[1]);
	CloseHandle(stderr_fd[1]);
	return mp;
}

#else

M_popen_handle_t *M_popen(const char *cmd, M_popen_err_t *errorid)
{
	M_popen_handle_t *mp;
	pid_t             pid;
	int               stdin_fd[2];
	int               stdout_fd[2];
	int               stderr_fd[2];
	char             *full_cmd     = NULL;
	size_t            full_cmd_len = 0;
	M_popen_err_t     myerrorid;
	char * const     *argv         = NULL;

	if (errorid == NULL)
		errorid = &myerrorid;

	mp = M_malloc_zero(sizeof(*mp));

	/* Create stdin pipes */
	if (pipe(stdin_fd) != 0) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		return NULL;
	}

	/* Create stdout pipes */
	if (pipe(stdout_fd) != 0) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		close(stdin_fd[0]);
		close(stdin_fd[1]);
		return NULL;
	}

	/* Create stderr pipes */
	if (pipe(stderr_fd) != 0) {
		*errorid = M_POPEN_ERR_PIPE;
		M_free(mp);
		close(stdin_fd[0]);
		close(stdin_fd[1]);
		close(stdout_fd[0]);
		close(stdout_fd[1]);
		return NULL;
	}

	mp->stdout_fd = stdout_fd[0];
	mp->stdin_fd  = stdin_fd[1];
	mp->stderr_fd = stderr_fd[0];

	pid = fork();
	if (pid == -1) {
		/* Error */
		*errorid = M_POPEN_ERR_SPAWN;
		M_free(mp);
		close(stdin_fd[0]);
		close(stdin_fd[1]);
		close(stdout_fd[0]);
		close(stdout_fd[1]);
		close(stderr_fd[0]);
		close(stderr_fd[1]);
		return NULL;
	} else if (pid == 0) {
		/* Child */

		/* Need to clone the file descriptors to the proper ids */

		/* redirect writefd pipe (read) to STDIN */
		dup2(stdin_fd[0], STDIN_FILENO);

		/* close unused writefds */
		close(stdin_fd[1]);
		if (stdin_fd[0] > 2)
			close(stdin_fd[0]);

		/* redirect readfd pipe (write) to STDOUT */
		dup2(stdout_fd[1], STDOUT_FILENO);

		/* Close unused readfds */
		close(stdout_fd[0]);
		if (stdout_fd[1] > 2)
			close(stdout_fd[1]);

		/* redirect errfd pipe (write) to STDERR */
		dup2(stderr_fd[1], STDERR_FILENO);

		/* Close unused errfds */
		close(stderr_fd[0]);
		if (stderr_fd[1] > 2)
			close(stderr_fd[1]);

		/* Put exec before command so the pid of cmd is the pid returned instead
		 * of the pid belonging to the /bin/sh wrapper. */
		full_cmd_len = M_str_len(cmd) + M_str_len(EXEC) + 2;
		full_cmd     = M_malloc(full_cmd_len);
		M_snprintf(full_cmd, full_cmd_len, "%s %s", EXEC, cmd);
		full_cmd[full_cmd_len-1] = '\0';

		argv = (char * const []){ M_strdup("/bin/sh"), M_strdup("-c"), full_cmd, NULL };

		/* Work around for issue on Solaris (found on 10 x86/x64) where the shell would
		 * return a broken pipe (retval 141) result when the process exited successfully.
		 * Testing on *nix OS (Linux, SCO 5/6, Solaris) did not show any issues so not ifdefing
		 * this to be Solaris only. */
		signal(SIGPIPE, SIG_IGN);
		execvp(argv[0], argv);

		/* NOTE: Will only get here on failure of execvp() */
		M_free(argv[0]);
		M_free(argv[1]);
		M_free(argv[2]);

		/* Map errno to exit codes */
		if (errno == EACCES || errno == EPERM) {
			exit(126);
		} else if (errno == ENOEXEC) {
			exit(125);
		} else if (errno == ENOENT) {
			exit(127);
		}
		exit(124);

	} else {
		/* Parent */

		/* Close FDs owned by child */
		close(stdout_fd[1]);
		close(stdin_fd[0]);
		close(stderr_fd[1]);

		/* Save child's pid */
		mp->pid = pid;
	}

	return mp;
}
#endif


const char *M_popen_strerror(M_popen_err_t err)
{
	switch (err) {
		case M_POPEN_ERR_NONE:
			return "No Error";
		case M_POPEN_ERR_INVALIDUSE:
			return "Invalid Use";
		case M_POPEN_ERR_CMDNOTFOUND:
			return "Command Not Found";
		case M_POPEN_ERR_PERM:
			return "Permission Denied";
		case M_POPEN_ERR_NOEXEC:
			return "File Not Executable";
		case M_POPEN_ERR_KILLSIGNAL:
			return "Command Terminated by Signal";
		case M_POPEN_ERR_PIPE:
			return "Error constructing pipe channel";
		case M_POPEN_ERR_WAIT:
			return "Error waiting on process";
		case M_POPEN_ERR_SPAWN:
			return "Error spawning process";
	}
	return "Unknown Error";
}
