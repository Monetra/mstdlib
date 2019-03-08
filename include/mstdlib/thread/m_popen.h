/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

#ifndef __M_POPEN_H__
#define __M_POPEN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/thread/m_thread.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_popen Process Open (popen)
 *  \ingroup m_thread
 *
 * Open and interact with a process.
 *
 * \note On Unix SIGCHLD _cannot_ be set to SIG_IGN.
 *       ECHILD could be generated and M_popen_check
 *       may return M_POPEN_ERR_WAIT when the process
 *       exits. See M_backtrace_set_ignore_signal for
 *       explantion.
 *
 * Example:
 *
 * \code{.c}
 *     const char       *data = "<x><t>data</t></x>"
 *     M_popen_err_t     mperr;
 *     M_popen_handle_t  mp;
 *     M_popen_status_t  status;
 *     int               retval;
 *     char             *stdout_buf     = NULL;
 *     size_t            stdout_buf_len = 0;
 *     char             *stderr_buf     = NULL;
 *     size_t            stderr_buf_len = 0;
 *     
 *     mp = M_popen("curl <url>", &mperr);
 *     if (mp == NULL) {
 *         printf("m_popen failed: %s\n", M_popen_strerror(mperr));
 *         return M_FALSE;
 *     }
 *     
 *     M_printf("Process spawned....\n");
 *     
 *     retval = M_popen_write(mp, M_POPEN_FD_WRITE, data, M_str_len(data));
 *     if (retval <= 0) {
 *         M_printf("M_popen_write failed, retval = %d\n", retval);
 *         M_popen_close(mp, &mperr);
 *         return M_FALSE;
 *     }
 *     
 *     / * Close file descriptor to let process know we're done * /
 *     if (!M_popen_closefd(mp, M_POPEN_FD_WRITE)) {
 *         M_printf("M_popen_closefd() failed\n");
 *         M_popen_close(mp, &mperr);
 *         return M_FALSE;
 *     }
 *     
 *     M_printf("Wrote process stream....\n");
 *     
 *     while ((status=M_popen_check(mp)) == M_POPEN_STATUS_RUNNING) {
 *         M_thread_sleep(50000);
 *     }
 *     
 *     if (status == M_POPEN_STATUS_ERROR) {
 *         retval = M_popen_close(mp, &mperr);
 *         printf("Error during M_popen_check(): %d: %s\n", retval, M_popen_strerror(mperr));
 *         return M_FALSE;
 *     }
 *     
 *     M_printf("Process done...\n");
 *     
 *     retval = M_popen_close_ex(mp, &stdout_buf, &stdout_buf_len, &stderr_buf, &stderr_buf_len, &mperr, 0);
 *     if (retval < 0) {
 *         M_printf("error: %s\n", M_popen_strerror(mperr));
 *         return M_FALSE;
 *     }
 *     
 *     M_printf("stdout: %d:\n%s\n", (int)stdout_buf_len, stdout_buf);
 *     M_printf("stderr: %d:\n%s\n", (int)stderr_buf_len, stderr_buf);
 *     M_free(stdout_buf);
 *     M_free(stderr_buf);
 *     M_printf("return code: %d\n", retval);
 * \endcode
 *
 * @{
 */

/*! Handle to M_popen object */
struct M_popen_handle;
typedef struct M_popen_handle M_popen_handle_t;


/*! Types of file descriptors that can be retrieved and used */
typedef enum {
	M_POPEN_FD_READ = 0,
	M_POPEN_FD_WRITE,
	M_POPEN_FD_ERR
} M_popen_fd_t;

/*! Possible error reason codes */
typedef enum {
	M_POPEN_ERR_NONE = 0,
	M_POPEN_ERR_INVALIDUSE,  /*!< invalid API usage                         */
	M_POPEN_ERR_CMDNOTFOUND, /*!< command not found                         */
	M_POPEN_ERR_PERM,        /*!< permission denied                         */
	M_POPEN_ERR_NOEXEC,      /*!< file not executable                       */
	M_POPEN_ERR_KILLSIGNAL,  /*!< killed by signal                          */
	M_POPEN_ERR_PIPE,        /*!< pipe creation failed                      */
	M_POPEN_ERR_WAIT,        /*!< attempting to check process status failed */
	M_POPEN_ERR_SPAWN        /*!< fork failed                               */
} M_popen_err_t;

/*! Status codes for command being executed */
typedef enum {
	M_POPEN_STATUS_RUNNING = 0,
	M_POPEN_STATUS_ERROR,
	M_POPEN_STATUS_DONE
} M_popen_status_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start the specified command and open stdin (write), stdout (read), and
 * stderr (read) file descriptors for communication. 
 *
 * Must call M_popen_close() to clean up the returned handle.
 *
 * \param[in]  cmd     Command to execute.
 * \param[out] errorid Pointer to store error id if an error occurs.
 *
 * \return NULL on failure, M_popen_handle_t on success.
 */
M_API M_popen_handle_t *M_popen(const char *cmd, M_popen_err_t *errorid);


/*! Read from a file descriptor
 *
 * \param[in]  mp         Open M_popen_t object.
 * \param[in]  fd         Which FD to read from.
 * \param[out] out        Buffer to hold read data.
 * \param[in]  out_len    Length of out buffer.
 * \param[in]  timeout_ms Time in ms to wait for data.
 *                        M_TIMEOUT_INF will cause this to block.
 *                        Note: Windows only has 15 ms resolution.
 *
 * \return -1 on error, -2 if fd was closed, 0 if a timeout occurred and no
 *         bytes were read, otherwise number of bytes read.
 */
M_API ssize_t M_popen_read(M_popen_handle_t *mp, M_popen_fd_t fd, char *out, size_t out_len, M_uint64 timeout_ms);


/*! Write to a file descriptor
 *
 * \param[in,out] mp     Open M_popen_t object.
 * \param[in]     fd     Which FD to write to.
 * \param[in]     in     Buffer to holding data to be written.
 * \param[in]     in_len Length of data to be written.
 *
 * \return -1 on error, otherwise number of bytes written.
 */
M_API ssize_t M_popen_write(M_popen_handle_t *mp, M_popen_fd_t fd, const char *in, size_t in_len);


/*! Close the provided file descriptor.
 *
 * This is used mainly to close the stdin stream to signal the command being
 * executed that there is no more data left to be read.  Any file open file
 * descriptors are automatically closed by M_popen_close().
 *
 * \param[in,out] mp Open M_popen_t object.
 * \param[in]     fd Which FD to close.
 *
 * \return 1 on success, 0 on error.
 */
M_API int M_popen_closefd(M_popen_handle_t *mp, M_popen_fd_t fd);


/*! Checks the current state of the command being executed and returns a code
 * identifying the state.
 *
 * Even if the state returns DONE or ERROR, M_popen_close() must be called.
 *
 * \param[in] mp Open M_popen_t object.
 *
 * \return M_popen_status_t code.
 */
M_API M_popen_status_t M_popen_check(M_popen_handle_t *mp);


/*! Close the M_popen_t object.
 *
 * This will perform a blocking wait for the process to exit before returning
 * control to the caller.
 *
 * \param[in]  mp             M_popen_t object
 * \param[out] stdout_buf     Optional parameter.  Will return allocated buffer containing
 *                            the contents of the process's stdout.  If specified, must
 *                            also specify stdout_buf_len.
 * \param[out] stdout_buf_len Optional parameter.  Will return the length of stdout_buf.
 * \param[out] stderr_buf     Optional parameter.  Will return allocated buffer containing
 *                            the contents of the process's stderr.  If specified, must
 *                            also specify stderr_buf_len.
 * \param[out] stderr_buf_len Optional parameter.  Will return the length of stderr_buf.
 * \param[out] errorid        if an error has occurred, will populate with a
 *                            reason code.
 * \param[in]  timeout        Time in ms to wait for the processes to exit. If the process
 *                            has not finished after the timeout expires it will be killed.
 *                            M_TIMEOUT_INF will cause this to block until the process exits.
 *                            Note: the time out only has 15 ms resolution.
 *
 * \return -1 on error, -2 on timeout, otherwise the exit code from the process.
 */
M_API int M_popen_close_ex(M_popen_handle_t *mp, char **stdout_buf, size_t *stdout_buf_len, char **stderr_buf, size_t *stderr_buf_len, M_popen_err_t *errorid, M_uint64 timeout);


/*! Close the M_popen_t object.
 *
 * This is a simplified wrapper around M_popen_close_ex(). This command blocks forever until the
 * child process is done. If you need to force-kill the process after a given timeout, use
 * M_popen_close_ex() instead of this function.
 *
 * \param[in]  mp      M_popen_t object
 * \param[out] errorid if an error has occurred, will populate with a reason code.
 *
 * \see M_popen_close_ex
 *
 * \return -1 on error, otherwise the exit code from the process.
 */
M_API int M_popen_close(M_popen_handle_t *mp, M_popen_err_t *errorid);


/*! Output human-readable error string.
 *
 * \param[in] err Error as returned by M_popen() or M_popen_close().
 *
 * \return string error message.
 */
M_API const char *M_popen_strerror(M_popen_err_t err);

/*! @} */

__END_DECLS

#endif /* __M_POPEN_H__ */
