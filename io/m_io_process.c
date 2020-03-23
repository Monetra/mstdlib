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
#  include <unistd.h>
#  include <signal.h>
#endif

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

struct M_io_handle {
	char          *command;
	M_list_str_t  *args;
	M_hash_dict_t *environ;
	M_io_t        *proc;
	M_io_t        *pipe_stdin;
	M_io_t        *pipe_stdout;
	M_io_t        *pipe_stderr;
	int            pid;
	int            return_code;
	M_uint64       timeout_ms;
};


static void *M_io_process_thread(void *arg)
{
	M_io_handle_t *handle = arg;
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

#ifndef _WIN32
static M_thread_once_t block_sigchild_once = M_THREAD_ONCE_STATIC_INITIALIZER;
static void block_sigchild(M_uint64 flags)
{
	(void)flags;
	signal(SIGCHLD, SIG_IGN);
}
#endif


static char *M_io_process_get_envpath(void)
{
#ifdef _WIN32
	char  *retpath           = NULL;
	DWORD  retpath_len_final = 0;
	DWORD  retpath_len       = GetEnvironmentVariable("PATH", NULL, 0);
	if (retpath_len == 0) {
		return NULL;
	}

	retpath           = M_malloc_zero(retpath_len);
	retpath_len_final = GetEnvironmentVariable("PATH", retpath, retpath_len);
	/* On failure the '\0' is included in the len. On sucess it is not. */
	if (retpath_final_len == 0 || retpath_len-1 != retpath_final_len) {
		M_free(retpath);
		return NULL;
	}
	return retpath;
#else
	const char *const_path = NULL;
#  ifdef HAVE_SECURE_GETENV
	const_path = secure_getenv("PATH");
#  else
	const_path = getenv("PATH");
#  endif

	if (const_path == NULL)
		return NULL;

	return M_strdup(const_path);
#endif
}


static char *M_io_process_search_command(const char *command, const char *path)
{
#ifdef _WIN32
	unsigned char delim               = ';';
#else
	unsigned char delim               = ':';
#endif
	size_t        num_paths           = 0;
	char        **paths               = M_str_explode_str_quoted(delim, path, '"', '\\', 0, &num_paths);
	size_t        i;
	char         *first_invalid_found = NULL;

	for (i=0; i<num_paths; i++) {
		char *fullpath = NULL;
		if (M_str_isempty(paths[i]))
			goto next;

		fullpath = M_fs_path_join(paths[i], command, M_FS_SYSTEM_AUTO);
		/* First check to see if the file exists */
		if (M_fs_perms_can_access(fullpath, 0) == M_FS_ERROR_SUCCESS) {
			/* Now check to see if we can execute it */
			if (M_fs_perms_can_access(fullpath, M_FS_PERMS_MODE_EXEC) != M_FS_ERROR_SUCCESS) {
				/* Cache this as we'll return it so they can be returned an exec perm issue */
				if (first_invalid_found == NULL) {
					first_invalid_found = fullpath;
					fullpath            = NULL;
					goto next;
				}
			} else {
				/* Found! */
				M_free(first_invalid_found); /* No need for this */
				return fullpath;
			}
		}

next:
		M_free(fullpath);
	}

	/* Return a matching found file, but that we don't have perms for */
	return first_invalid_found;
}


M_io_error_t M_io_process_create(const char *command, M_list_str_t *args, M_hash_dict_t *environ, M_uint64 timeout_ms, M_io_t **proc, M_io_t **proc_stdin, M_io_t **proc_stdout, M_io_t **proc_stderr)
{
	M_io_handle_t    *handle        = NULL;
	M_io_callbacks_t *callbacks     = NULL;
	char             *full_command  = NULL;
	M_io_t           *pipe_stdin_r  = NULL;
	M_io_t           *pipe_stdin_w  = NULL;
	M_io_t           *pipe_stdout_r = NULL;
	M_io_t           *pipe_stdout_w = NULL;
	M_io_t           *pipe_stderr_r = NULL;
	M_io_t           *pipe_stderr_w = NULL;
	M_io_error_t      rv            = M_IO_ERROR_SUCCESS;

#ifndef _WIN32
	/* Thread_once to block SIGCHLD ... as if someone starts using this, we assume they're not spawning children any other way. */
	M_thread_once(&block_sigchild_once, block_sigchild, 0);
#endif

	if (M_str_isempty(command) || proc == NULL) {
		rv = M_IO_ERROR_INVALID;
		goto fail;
	}

	/* Search "PATH" for "command" with execute perms - Order: 'environ', getenv(), confstr(_CS_PATH)
	 * - If not found, return error */
	if (!M_fs_path_isabs(command, M_FS_SYSTEM_AUTO)) {
		char       *path       = NULL;

		if (environ != NULL) {
			const char *const_path = M_hash_dict_get_direct(environ, "PATH");
			if (const_path) {
				path = M_strdup(const_path);
			}
		} else {
			path = M_io_process_get_envpath();
		}

#ifndef _WIN32
		if (path == NULL) {
			size_t len = confstr(_CS_PATH, NULL, 0);
			if (len != 0) {
				path = M_malloc_zero(len);
				confstr(_CS_PATH, path, len);
			}
		}
#endif
		full_command = M_io_process_search_command(command, path);
		M_free(path);
	} else {
		full_command = M_strdup(command);
	}

	if (M_str_isempty(full_command)) {
		rv = M_IO_ERROR_NOTFOUND;
		goto fail;
	}

	/* Check perms */
	if (M_fs_perms_can_access(full_command, M_FS_PERMS_MODE_EXEC) != M_FS_ERROR_SUCCESS) {
		rv = M_IO_ERROR_NOTPERM;
		goto fail;
	}

	/* Create pipes for stdin, stdout, and stderr, regardless of if the end-user needs them */
	if (M_io_pipe_create(M_IO_PIPE_INHERIT_READ,  &pipe_stdin_r,  &pipe_stdin_w)  != M_IO_ERROR_SUCCESS ||
		M_io_pipe_create(M_IO_PIPE_INHERIT_WRITE, &pipe_stdout_r, &pipe_stdout_w) != M_IO_ERROR_SUCCESS ||
		M_io_pipe_create(M_IO_PIPE_INHERIT_WRITE, &pipe_stderr_r, &pipe_stderr_w) != M_IO_ERROR_SUCCESS) {
		rv = M_IO_ERROR_NOSYSRESOURCES;
		goto fail;
	}


	handle    = M_malloc_zero(sizeof(*handle));
	*proc     = M_io_init(M_IO_TYPE_EVENT);
	callbacks = M_io_callbacks_create();
#if 0
	M_io_callbacks_reg_init        (callbacks, M_io_w32overlap_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_w32overlap_process_cb);
	M_io_callbacks_reg_unregister  (callbacks, M_io_w32overlap_unregister_cb);
	M_io_callbacks_reg_destroy     (callbacks, M_io_w32overlap_destroy_cb);
	M_io_callbacks_reg_state       (callbacks, M_io_w32overlap_state_cb);
	M_io_callbacks_reg_errormsg    (callbacks, M_io_w32overlap_errormsg_cb);
#endif
	M_io_layer_add(*proc, "PROCESS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);


	/* Spawn thread, pass M_io_handle */
	handle->proc        = *proc;
	handle->pipe_stdin  = pipe_stdin_r;
	handle->pipe_stdout = pipe_stdout_w;
	handle->pipe_stderr = pipe_stderr_w;
	handle->command     = full_command;
	if (environ)
		handle->environ = M_hash_dict_duplicate(environ);
	if (args)
		handle->args    = M_list_str_duplicate(args);
	handle->timeout_ms  = timeout_ms;

	// M_thread_create()

	/* If no desire to return stdin/stdout/stderr, close respective ends otherwise bind them to the output params */
	if (proc_stdin == NULL) {
		M_io_destroy(pipe_stdin_w);
	} else {
		*proc_stdin = pipe_stdin_w;
	}

	if (proc_stdout == NULL) {
		M_io_destroy(pipe_stdout_r);
	} else {
		*proc_stdout = pipe_stdout_r;
	}

	if (proc_stderr == NULL) {
		M_io_destroy(pipe_stderr_r);
	} else {
		*proc_stderr = pipe_stderr_r;
	}

	return M_IO_ERROR_SUCCESS;

	/* NOTE: when _init() is called, it may need to signal itself if process has already exited ... it also may need to set a timer if timeout_ms is non-zero */

fail:
	M_free(full_command);
	M_io_destroy(pipe_stdin_r);
	M_io_destroy(pipe_stdin_w);
	M_io_destroy(pipe_stdout_r);
	M_io_destroy(pipe_stdout_w);
	M_io_destroy(pipe_stderr_r);
	M_io_destroy(pipe_stderr_w);
	return rv;
}




M_bool M_io_process_get_result_code(M_io_t *proc, int *result_code)
{

}


int M_io_process_get_pid(M_io_t *proc)
{

}



