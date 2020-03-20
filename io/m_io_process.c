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


/* Overview:
 *   - We must search the PATH environment variable for the specified "command" if it is not an absolute path.  Check
 *     for executability on each file.
 *     - PATH must be taken from the passed in 'environ' if environ is specified.
 *     - if no PATH set, on Unix use confstr(_CS_PATH, path, sizeof(path));
 *   - Assuming the command can be found, we need to spawn a thread before we can launch the process.
 *     the reason for this is there is no easy way to be notified when a process exits.  So the thread will create the
 *     process then wait indefinitely for it to exit.  Upon exit of the process (or timeout via specified command), it
 *     will notify the "proc" io handle that an event has occurred.
 *     - NOTE: waitpid() will block indefinitely, so if there is a timeout, it is up to the parent to set a timer and
 *             kill() the process when the timer elapses so waitpid() will return.
 *   - We will use standard M_io_pipe_create() to create the necessary endpoints for stdin, stdout, and stderr.  We may
 *     just need access to the internals to pull out the handles to bind to the process, and add a flag to allow
 *     inheritable handles to M_io_pipe_create().
 */

struct M_io_handle {
	M_io_t  *io;
	int      pid;
	int      return_code;
	M_uint64 timeout_ms;
};


static void *M_io_process_thread(void *arg)
{
	/* fork() */

	/* in-parent :
	 *    set pid in handle
	 *  - wait_pid
	 *  - set return code 
	 *  - wake up caller
	 */

	/* in-child :
	 *   dup2() for STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
	 *   execve() */
}


M_io_error_t M_io_process_create(const char *command, M_list_str_t *args, M_list_str_t *environ, M_uint64 timeout_ms, M_io_t **proc, M_io_t **proc_stdin, M_io_t **proc_stdout, M_io_t **proc_stderr)
{
	/* Thread_once to block SIGCHLD ... as if someone starts using this, we assume they're not spawning children any other way. */

	/* Search "PATH" for "command" with execute perms - Order: 'environ', getenv(), confstr(_CS_PATH)
	 * - If not found, return error */

	/* Create pipes for stdin, stdout, and stderr, regardless of if the end-user needs them */

	/* Spawn thread, pass M_io_handle, and stdin/stdout/stderr endpoints */

	/* If no desire to return stdin/stdout/stderr, close respective ends otherwise bind them to the output params */

	/* NOTE: when _init() is called, it may need to signal itself if process has already exited ... it also may need to set a timer if timeout_ms is non-zero */
}

/* TODO:  Does it make sense to create an 
 *   M_io_process_create_func(int (*func)(void *), void *arg, M_uint64 timeout_ms, M_io_t **proc, M_io_t **proc_stdin, M_io_t **proc_stdout, M_io_t **proc_stderr)) 
 * ??
 * For unix it is easy as we'd just fork() and call the function instead of execve().
 * For Windows, we'd call ZwCreateProcess() and friends ... a fork() for windows is here:
 *    https://github.com/jonclayden/multicore/blob/master/src/forknt.c
 *    https://github.com/opencollab/scilab/blob/master/scilab/modules/parallel/src/c/forkWindows.c
 * More useful info:
 *    https://books.google.com/books?id=Fp1ct-bKYdcC&pg=PA168&lpg=PA168&dq=use+ZwCreateProcess+instead+of+CreateThread
 * But really we don't need everything its doing, we'd just set our own child entry to the function, so instead of copying the current thread context, we'd create a brand new thread context.
 *.. The function is passed in Eip or Rip (32 vs 64) need to figure out how to pass the arg (most likely in the first register). I also don't think we need to do any exception state copying.
 *   We're not trying to emulate fork() afterall. */


M_bool M_io_process_get_result_code(M_io_t *proc, int *result_code)
{

}


int M_io_process_get_pid(M_io_t *proc)
{

}


M_io_error_t M_io_pipe_create(M_io_t **reader, M_io_t **writer)
{
	HANDLE            r;
	HANDLE            w;
	M_io_handle_t    *riohandle;
	M_io_handle_t    *wiohandle;
	char              pipename[256];
	M_io_callbacks_t *callbacks;

	if (reader == NULL || writer == NULL)
		return M_IO_ERROR_ERROR;

	*reader = NULL;
	*writer = NULL;

	M_snprintf(pipename, sizeof(pipename), "\\\\.\\Pipe\\Anon.%08x.%08x", GetCurrentProcessId(), M_atomic_inc_u32(&M_io_pipe_id));

	r = CreateNamedPipeA(pipename,
		PIPE_ACCESS_INBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE|FILE_FLAG_OVERLAPPED,
		PIPE_READMODE_BYTE /* |PIPE_REJECT_REMOTE_CLIENTS */, 
		1,
		/* These are supposedly advisory and the OS will grow them */
		PIPE_BUFSIZE,
		PIPE_BUFSIZE,
		0,
		NULL);

	w = CreateFileA(pipename,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,
		NULL);

	if (r == NULL || w == NULL) {
		CloseHandle(r);
		CloseHandle(w);
		return M_IO_ERROR_ERROR;
	}

	riohandle = M_io_w32overlap_init_handle(r, M_EVENT_INVALID_HANDLE);
	wiohandle = M_io_w32overlap_init_handle(M_EVENT_INVALID_HANDLE, w);

	*reader   = M_io_init(M_IO_TYPE_READER);
	*writer   = M_io_init(M_IO_TYPE_WRITER);

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_w32overlap_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_w32overlap_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_w32overlap_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_w32overlap_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_w32overlap_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_w32overlap_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_w32overlap_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_w32overlap_errormsg_cb);
	M_io_layer_add(*reader, "PIPEREAD", riohandle, callbacks);
	M_io_layer_add(*writer, "PIPEWRITE", wiohandle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}

