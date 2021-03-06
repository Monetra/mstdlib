/* The MIT License (MIT)
 *
 * Copyright (c) 2020 Monetra Technologies, LLC.
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
#include "m_io_pipe_int.h"

#ifndef _WIN32
#  include "m_io_posix_common.h"
#  include <unistd.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <string.h>
#else
#  include "m_io_win32_common.h"
#endif
#include <errno.h>

/* Overview:
 *   - We must search the PATH environment variable for the specified "command" if it is not an absolute path.  Check
 *     for executability on each file.
 *     - PATH must be taken from the passed in 'env' if specified.
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

typedef enum {
	M_IO_PROC_STATUS_INIT     = 0,
	M_IO_PROC_STATUS_STARTING,
	M_IO_PROC_STATUS_RUNNING,
	M_IO_PROC_STATUS_ERROR,
	M_IO_PROC_STATUS_EXITED
} M_io_process_status_t;

struct M_io_handle {
	char                 *command;
	M_list_str_t         *args;
	M_hash_dict_t        *env;
	M_io_t               *proc;
	M_io_t               *pipe_stdin;
	M_io_t               *pipe_stdout;
	M_io_t               *pipe_stderr;
	int                   pid;
	int                   return_code;
	M_uint64              timeout_ms;
	M_bool                timedout;
	M_io_process_status_t status;
#ifdef _WIN32
	DWORD                 last_sys_error;
#else
	int                   last_sys_error;
#endif
	M_timeval_t           start_timer;
	M_event_timer_t      *timer;
	M_threadid_t          thread;
};

#ifndef _WIN32

static char **M_io_process_dict_to_env(M_hash_dict_t *dict)
{
	char              **env      = NULL;
	M_hash_dict_enum_t *hashenum = NULL;
	const char         *key      = NULL;
	const char         *val      = NULL;
	size_t              i        = 0;
	if (dict == NULL)
		return NULL;

	/* Must be NULL-terminated */
	env = M_malloc_zero(sizeof(*env) * (M_hash_dict_num_keys(dict) + 1));
	M_hash_dict_enumerate(dict, &hashenum);

	while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
		M_asprintf(&env[i++], "%s=%s", key, val);
	}
	M_hash_dict_enumerate_free(hashenum);

	return env;
}


static char **M_io_process_list_to_args(const char *command, M_list_str_t *list)
{
	char              **args     = NULL;
	size_t              i        = 0;
	size_t              len      = M_list_str_len(list);

	/* Must be NULL-terminated, and include the command in the first entry */
	args    = M_malloc_zero(sizeof(*args) * (len + 2));
	args[0] = M_strdup(command);
	for (i=0; i<len; i++) {
		args[i+1] = M_strdup(M_list_str_at(list, i));
	}

	return args;
}


#define DEFAULT_EXIT_ERRORCODE 130

static struct {
	int err;
	int exitcode;
} M_io_process_exitcodes[] = {
	{ EACCES,  126 },
	{ ENOENT,  127 },
	{ ENOEXEC, 128 },
	{ E2BIG,   129 }
};


static int M_io_process_errno_to_exitcode(int my_errno)
{
	size_t i;
	for (i=0; i<sizeof(M_io_process_exitcodes) / sizeof(*M_io_process_exitcodes); i++) {
		if (my_errno == M_io_process_exitcodes[i].err)
			return M_io_process_exitcodes[i].exitcode;
	}
	return DEFAULT_EXIT_ERRORCODE;
}

static int M_io_process_exitcode_to_errno(int exitcode)
{
	size_t i;
	if (exitcode == DEFAULT_EXIT_ERRORCODE)
		return EFAULT;

	for (i=0; i<sizeof(M_io_process_exitcodes) / sizeof(*M_io_process_exitcodes); i++) {
		if (exitcode == M_io_process_exitcodes[i].exitcode)
			return M_io_process_exitcodes[i].err;
	}

	return 0;
}

static void *M_io_process_thread(void *arg)
{
	M_io_handle_t *handle  = arg;
	pid_t          pid;
	M_io_layer_t  *layer   = NULL;
	int            wstatus = 0;

	pid = fork();

	/* Error */
	if (pid == -1) {
		int           myerrno  = errno;

		layer                  = M_io_layer_acquire(handle->proc, 0, "PROCESS");
		handle->status         = M_IO_PROC_STATUS_ERROR;
		handle->last_sys_error = myerrno;
		M_io_destroy(handle->pipe_stdin);
		M_io_destroy(handle->pipe_stdout);
		M_io_destroy(handle->pipe_stderr);
		handle->pipe_stdin     = NULL;
		handle->pipe_stdout    = NULL;
		handle->pipe_stderr    = NULL;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, M_io_posix_err_to_ioerr(handle->last_sys_error));
		M_io_layer_release(layer);
		return NULL;
	}

	/* Child */
	if (pid == 0) {
		int fd;

		char **args = M_io_process_list_to_args(handle->command, handle->args);
		char **env  = M_io_process_dict_to_env(handle->env);

		/* Clone file descriptors to the proper ids */
		fd = M_io_pipe_get_fd(handle->pipe_stdin);
		dup2(fd, STDIN_FILENO);
		if (fd > 2)
			close(fd);

		fd = M_io_pipe_get_fd(handle->pipe_stdout);
		dup2(fd, STDOUT_FILENO);
		if (fd > 2)
			close(fd);

		fd = M_io_pipe_get_fd(handle->pipe_stderr);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);

		if (env) {
			execve(handle->command, args, env);
		} else {
			execv(handle->command, args);
		}

		/* A typical shell will reserve some exit codes: http://www.tldp.org/LDP/abs/html/exitcodes.html
		 *    126   - Command invoked cannot execute - Permission problem or command is not an executable
		 *    127   - "command not found" - Possible problem with $PATH or a typo
		 *    128   - Invalid argument to exit
		 *    128+n - Fatal error signal "n". Control-C is signal 2, so code would be 130
		 * In general this means the user can use 0-125 for themselves, so we can use 126+ for our own
		 * purposes.  Really what we need to be able to do is map these error codes:
		 *  - E2BIG   - The total number of bytes in the environment (envp) and argument list (argv) is too large.
		 *  - EACCESS - a) The file or a script interpreter is not a regular file  b)execute permission is denied for
		 *              the file or a script or ELF interpreter. c) The filesystem is mounted noexec.
		 *  - ENOENT  - The file filename or a script or ELF interpreter does not exist, or a shared library needed for
		 *              the file or interpreter cannot be found.
		 *  - ENOEXEC - An executable is not in a recognized format, is for the wrong architecture, or has some other
		 *              format error that means it cannot be executed.
		 *  - OTHERS  - Any other reason
		 */
		exit(M_io_process_errno_to_exitcode(errno));
	}

	/* Parent */

	/* Close pipe endpoints, record pid, notify parent the process has started */
	layer               = M_io_layer_acquire(handle->proc, 0, "PROCESS");
	M_io_destroy(handle->pipe_stdin);
	M_io_destroy(handle->pipe_stdout);
	M_io_destroy(handle->pipe_stderr);
	handle->pipe_stdin  = NULL;
	handle->pipe_stdout = NULL;
	handle->pipe_stderr = NULL;
	handle->status      = M_IO_PROC_STATUS_RUNNING;
	handle->pid         = pid;
	M_time_elapsed_start(&handle->start_timer);

	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	/* Wait for process to exit - indefinitely, yes, really ... we have to be killed externally. */
	while (waitpid(pid, &wstatus, 0) == -1) {
		if (errno != EINTR)
			break;
	}

	/* Record exit code and notify watcher */
	layer                  = M_io_layer_acquire(handle->proc, 0, "PROCESS");
	handle->return_code    = WEXITSTATUS(wstatus);
	handle->status         = M_IO_PROC_STATUS_EXITED;
	handle->last_sys_error = M_io_process_exitcode_to_errno(handle->return_code);
	M_io_layer_softevent_add(layer, M_FALSE, (handle->last_sys_error == 0 && !handle->timedout)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR, handle->timedout?M_IO_ERROR_TIMEDOUT:M_io_posix_err_to_ioerr(handle->last_sys_error));
	M_io_layer_release(layer);

	return NULL;
}
#else

static char *M_io_process_dict_to_env(M_hash_dict_t *dict)
{
	M_buf_t            *buf      = NULL;
	M_hash_dict_enum_t *hashenum = NULL;
	const char         *key      = NULL;
	const char         *val      = NULL;

	if (dict == NULL)
		return NULL;

	/* Windows environment is a NULL-terminated list of environment variables in key=value format,
	 * that is then NULL terminated, so the last 2 characters are always \0\0 */

	buf = M_buf_create();
	while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
		M_buf_add_str(buf, key);
		M_buf_add_byte(buf, '=');
		M_buf_add_str(buf, val);
		M_buf_add_byte(buf, 0); /* Terminate entry */
	}
	M_hash_dict_enumerate_free(hashenum);

	/* Terminate list */
	M_buf_add_byte(buf, 0);

	return M_buf_finish_str(buf, NULL);
}


static M_bool M_io_process_arg_needs_quoting(const char *data)
{
	size_t i;
	for (i=0; data[i] != 0; i++) {
		if (data[i] == ' ' || data[i] == '"')
			return M_TRUE;
	}
	return M_FALSE;
}


static void M_io_process_append_escaped(M_buf_t *buf, const char *data)
{
	size_t i;

	for (i=0; data[i] != 0; i++) {
		if (data[i] == '"') {
			M_buf_add_byte(buf, '\\');
		}
		M_buf_add_byte(buf, (unsigned char)data[i]);
	}
}


static char *M_io_process_list_to_args(const char *command, M_list_str_t *list)
{
	M_buf_t *buf           = M_buf_create();
	size_t   i             = 0;
	size_t   len           = M_list_str_len(list);

	/* Windows docs on how it handles command line parsing:
	 *   https://docs.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?redirectedfrom=MSDN&view=vs-2019
	 * I can't even make this stuff up.
	 * So if a parameter contains a space or a double quote, we'll quote and escape it.  Otherwise, emit as-is.
	 * The escape character is '\' but its not necessary to escape another escape character it appears.
	 */

	/* Output command */
	if (M_io_process_arg_needs_quoting(command)) {
		M_buf_add_byte(buf, '"');
		M_io_process_append_escaped(buf, command);
		M_buf_add_byte(buf, '"');
	} else {
		M_buf_add_str(buf, command);
	}

	for (i=0; i<len; i++) {
		M_buf_add_byte(buf, ' ');
		if (M_io_process_arg_needs_quoting(M_list_str_at(list, i))) {
			M_buf_add_byte(buf, '"');
			M_io_process_append_escaped(buf, M_list_str_at(list, i));
			M_buf_add_byte(buf, '"');
		} else {
			M_buf_add_str(buf, M_list_str_at(list, i));
		}
	}

	return M_buf_finish_str(buf, NULL);
}

/* Win32 */
static void *M_io_process_thread(void *arg)
{
	STARTUPINFO         siStartInfo;
	PROCESS_INFORMATION pi;
	M_io_handle_t      *handle  = arg;
	M_io_layer_t       *layer   = NULL;
	char               *command = M_io_process_list_to_args(handle->command, handle->args);
	char               *env     = M_io_process_dict_to_env(handle->env);
	DWORD               err;
	BOOL                rv;
	DWORD               resultcode = 0;

	M_mem_set(&siStartInfo, 0, sizeof(siStartInfo));
	siStartInfo.cb         = sizeof(siStartInfo);
	siStartInfo.hStdInput  = M_io_pipe_get_fd(handle->pipe_stdin);
	siStartInfo.hStdOutput = M_io_pipe_get_fd(handle->pipe_stdout);
	siStartInfo.hStdError  = M_io_pipe_get_fd(handle->pipe_stderr);
	siStartInfo.dwFlags   |= STARTF_USESTDHANDLES;

	M_mem_set(&pi, 0, sizeof(pi));

	rv  = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, env, NULL, &siStartInfo, &pi);
	err = GetLastError();
	M_free(command);
	M_free(env);

	/* Error starting process */
	if (!rv) {
		layer                  = M_io_layer_acquire(handle->proc, 0, "PROCESS");
		handle->status         = M_IO_PROC_STATUS_ERROR;
		handle->last_sys_error = err;
		M_io_destroy(handle->pipe_stdin);
		M_io_destroy(handle->pipe_stdout);
		M_io_destroy(handle->pipe_stderr);
		handle->pipe_stdin     = NULL;
		handle->pipe_stdout    = NULL;
		handle->pipe_stderr    = NULL;
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_ERROR, M_io_win32_err_to_ioerr(handle->last_sys_error));
		M_io_layer_release(layer);
		return NULL;
	}


	/* In parent */

	/* Close pipe endpoints, record pid, notify parent the process has started */
	layer               = M_io_layer_acquire(handle->proc, 0, "PROCESS");
	M_io_destroy(handle->pipe_stdin);
	M_io_destroy(handle->pipe_stdout);
	M_io_destroy(handle->pipe_stderr);
	handle->pipe_stdin  = NULL;
	handle->pipe_stdout = NULL;
	handle->pipe_stderr = NULL;
	handle->status      = M_IO_PROC_STATUS_RUNNING;
	handle->pid         = (int)pi.dwProcessId;
	M_time_elapsed_start(&handle->start_timer);

	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	/* Wait for process to exit - indefinitely, yes, really ... we have to be killed externally. */
	while (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0)
		;
	M_thread_sleep(100000);

	/* Record exit code and notify watcher */
	layer                  = M_io_layer_acquire(handle->proc, 0, "PROCESS");
	if (GetExitCodeProcess(pi.hProcess, &resultcode)) {
		handle->return_code    = (int)resultcode;
	} else {
		handle->return_code    = 130;
	}
	handle->status         = M_IO_PROC_STATUS_EXITED;
	handle->last_sys_error = 0;
	M_io_layer_softevent_add(layer, M_FALSE, handle->timedout?M_EVENT_TYPE_ERROR:M_EVENT_TYPE_DISCONNECTED, handle->timedout?M_IO_ERROR_TIMEDOUT:M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);

	if (pi.hProcess != NULL)
		CloseHandle(pi.hProcess);
	if (pi.hThread != NULL)
		CloseHandle(pi.hThread);

	return NULL;
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
	if (retpath_len_final == 0 || retpath_len-1 != retpath_len_final) {
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
				M_str_explode_free(paths, num_paths);
				return fullpath;
			}
		}

next:
		M_free(fullpath);
	}

	M_str_explode_free(paths, num_paths);
	/* Return a matching found file, but that we don't have perms for */
	return first_invalid_found;
}

static M_io_error_t M_io_process_os_error(M_io_handle_t *handle)
{
	if (handle->timedout)
		return M_IO_ERROR_TIMEDOUT;

#ifdef _WIN32
	return M_io_win32_err_to_ioerr(handle->last_sys_error);
#else
	return M_io_posix_err_to_ioerr(handle->last_sys_error);
#endif
}

static M_bool M_io_process_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->status == M_IO_PROC_STATUS_INIT) {
		/* Spawn thread */
		M_thread_attr_t *attr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(attr, M_TRUE);
		handle->status        = M_IO_PROC_STATUS_STARTING;
		handle->thread        = M_thread_create(attr, M_io_process_thread, handle);
		M_thread_attr_destroy(attr);
	} else if (handle->status == M_IO_PROC_STATUS_RUNNING) {
		/* Trigger connected soft event when registered with event handle */
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
	} else if (handle->status == M_IO_PROC_STATUS_EXITED) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_io_process_os_error(handle));
	} else if (handle->status == M_IO_PROC_STATUS_ERROR) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_io_process_os_error(handle));
	}

	return M_TRUE;
}

static void M_io_process_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}
}

static void M_io_process_kill(M_io_handle_t *handle)
{
#ifdef _WIN32
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)handle->pid);
	TerminateProcess(hProcess, 130);
	CloseHandle(hProcess);
#else
	kill(handle->pid, SIGKILL);
#endif
}

static void M_io_process_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	void          *rv;

	/* Forcibly kill process as it appears to still be running */
	if (handle->status == M_IO_PROC_STATUS_STARTING || handle->status == M_IO_PROC_STATUS_RUNNING) {
		M_io_process_kill(handle);
	}

	/* Join the thread to wait for it to exit */
	if (handle->thread)
		M_thread_join(handle->thread, &rv);

	/* Clean up */
	M_free(handle->command);
	M_list_str_destroy(handle->args);
	M_hash_dict_destroy(handle->env);
	M_io_destroy(handle->pipe_stdin);
	M_io_destroy(handle->pipe_stdout);
	M_io_destroy(handle->pipe_stderr);
	M_free(handle);
}

static M_io_state_t M_io_process_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	switch (handle->status) {
		case M_IO_PROC_STATUS_INIT:
			return M_IO_STATE_INIT;
		case M_IO_PROC_STATUS_STARTING:
			return M_IO_STATE_CONNECTING;
		case M_IO_PROC_STATUS_RUNNING:
			return M_IO_STATE_CONNECTED;
		case M_IO_PROC_STATUS_EXITED:
			return M_IO_STATE_DISCONNECTED;
		case M_IO_PROC_STATUS_ERROR:
			return M_IO_STATE_ERROR;
	}
	return M_IO_STATE_ERROR;
}


static M_bool M_io_process_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->timedout) {
		M_snprintf(error, err_len, "Killed by timeout");
		return M_TRUE;
	}
#ifndef _WIN32
	M_io_posix_errormsg(handle->last_sys_error, error, err_len);
#else
	M_io_win32_errormsg(handle->last_sys_error, error, err_len);
#endif
	return M_TRUE;
}


static void M_io_process_timeout_cb(M_event_t *event, M_event_type_t type, M_io_t *io_dummy, void *cb_arg)
{
	M_io_handle_t *handle = cb_arg;
	M_io_t        *io     = handle->proc;
	M_io_layer_t  *layer;

	(void)event;
	(void)type;
	(void)io_dummy;

	layer            = M_io_layer_acquire(io, 0, NULL);
	handle->timedout = M_TRUE;
	M_io_process_kill(handle);

	M_event_timer_remove(handle->timer);
	handle->timer = NULL;
	M_io_layer_release(layer);

}


static M_bool M_io_process_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (*type == M_EVENT_TYPE_CONNECTED) {
		if (handle->timeout_ms) {
			M_uint64 elapsed   = M_time_elapsed(&handle->start_timer);
			M_uint64 remaining = 0;
			if (elapsed < handle->timeout_ms)
				remaining = handle->timeout_ms - elapsed;

			handle->timer = M_event_timer_oneshot(event, remaining, M_FALSE, M_io_process_timeout_cb, handle);
		}
	}

	if (*type == M_EVENT_TYPE_ERROR || *type == M_EVENT_TYPE_DISCONNECTED) {
		if (handle->timer) {
			M_event_timer_remove(handle->timer);
			handle->timer = NULL;
		}
	}

	/* Pass on */
	return M_FALSE;
}


M_io_error_t M_io_process_create(const char *command, M_list_str_t *args, M_hash_dict_t *env, M_uint64 timeout_ms, M_io_t **proc, M_io_t **proc_stdin, M_io_t **proc_stdout, M_io_t **proc_stderr)
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
	char             *dirname       = NULL;
	M_fs_path_norm_t  normflags     = M_FS_PATH_NORM_FOLLOWSYMLINKS|M_FS_PATH_NORM_HOME;

	if (proc)
		*proc        = NULL;
	if (proc_stdin)
		*proc_stdin  = NULL;
	if (proc_stdout)
		*proc_stdout = NULL;
	if (proc_stderr)
		*proc_stderr = NULL;

	if (M_str_isempty(command) || proc == NULL) {
		rv = M_IO_ERROR_INVALID;
		goto fail;
	}

	dirname = M_fs_path_dirname(command, M_FS_SYSTEM_AUTO);
	if (!M_str_isempty(dirname)) {
		/* If the command is something like just 'echo', M_fs_path_norm() with M_FS_PATH_NORM_ABSOLUTE
		 * will prepend the cwd which isn't right for unix systems */
		normflags |= M_FS_PATH_NORM_ABSOLUTE;
	}
	M_free(dirname);

	/* Normalize command */
	if (M_fs_path_norm(&full_command, command,
		normflags,
		M_FS_SYSTEM_AUTO)  != M_FS_ERROR_SUCCESS) {
		rv = M_IO_ERROR_NOTFOUND;
		goto fail;
	}

	/* Search "PATH" for "command" with execute perms - Order: 'env', getenv(), confstr(_CS_PATH)
	 * - If not found, return error */
	if (M_fs_path_isabs(command, M_FS_SYSTEM_AUTO)) {
		/* Nothing to do */
	} else {
		char       *path = NULL;
		char       *temp = NULL;
		if (env != NULL) {
			const char *const_path = M_hash_dict_get_direct(env, "PATH");
			if (const_path) {
				path = M_strdup(const_path);
			}
		} else {
			path = M_io_process_get_envpath();
		}

#ifdef HAVE_CONFSTR
		if (path == NULL) {
			size_t len = confstr(_CS_PATH, NULL, 0);
			if (len != 0) {
				path = M_malloc_zero(len);
				confstr(_CS_PATH, path, len);
			}
		}
#endif

#ifdef _WIN32
		/* Windows puts the current working directory in the search path */
		if (M_fs_path_get_cwd(&temp) == M_FS_ERROR_SUCCESS) {
			char *temppath = NULL;
			M_asprintf(&temppath, "%s;%s", path, temp);
			M_free(temp);
			M_free(path);
			path = temppath;
		}
#endif

		temp = M_io_process_search_command(full_command, path);
		M_free(full_command);
		full_command = temp;
		M_free(path);
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
	handle->proc        = *proc;
	handle->pipe_stdin  = pipe_stdin_r;
	handle->pipe_stdout = pipe_stdout_w;
	handle->pipe_stderr = pipe_stderr_w;
	handle->command     = full_command;
	if (env)
		handle->env     = M_hash_dict_duplicate(env);
	if (args)
		handle->args    = M_list_str_duplicate(args);
	handle->timeout_ms  = timeout_ms;

	*proc        = M_io_init(M_IO_TYPE_EVENT);
	handle->proc = *proc;
	callbacks    = M_io_callbacks_create();
	M_io_callbacks_reg_init        (callbacks, M_io_process_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_process_process_cb);
	M_io_callbacks_reg_unregister  (callbacks, M_io_process_unregister_cb);
	M_io_callbacks_reg_destroy     (callbacks, M_io_process_destroy_cb);
	M_io_callbacks_reg_state       (callbacks, M_io_process_state_cb);
	M_io_callbacks_reg_errormsg    (callbacks, M_io_process_errormsg_cb);
	M_io_layer_add(*proc, "PROCESS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

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

	/* NOTE: process will not be started until it is connected to an event loop */

	return M_IO_ERROR_SUCCESS;

fail:
	if (proc)
		*proc = NULL;
	M_free(full_command);
	M_free(handle);
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
	M_io_layer_t  *layer  = NULL;
	M_io_handle_t *handle = NULL;
	M_bool         rv     = M_FALSE;

	if (proc == NULL || result_code == NULL)
		return M_FALSE;

	*result_code = -1;

	layer = M_io_layer_acquire(proc, 0, "PROCESS");
	if (layer == NULL)
		return M_FALSE;

	handle = M_io_layer_get_handle(layer);

	if (handle->status == M_IO_PROC_STATUS_EXITED && !handle->timedout) {
		rv           = M_TRUE;
		*result_code = handle->return_code;
	}

	M_io_layer_release(layer);

	return rv;
}


int M_io_process_get_pid(M_io_t *proc)
{
	M_io_layer_t  *layer  = NULL;
	M_io_handle_t *handle = NULL;
	int            pid    = 0;

	if (proc == NULL)
		return 0;

	layer = M_io_layer_acquire(proc, 0, "PROCESS");
	if (layer == NULL)
		return M_FALSE;

	handle = M_io_layer_get_handle(layer);

	pid    = handle->pid;

	M_io_layer_release(layer);

	return pid;
}



