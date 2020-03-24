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

#ifndef __M_IO_PROCESS_H__
#define __M_IO_PROCESS_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_process Start a process and perform I/O using stdin, stdout, and stderr
 *  \ingroup m_eventio_base
 *
 * A process starts an executable and opens the processes communication endpoints of
 * stdin, stdout, and stderr as unidirectional pipes for communicating with the process.
 *
 * @{
 */


/*! Create a process and return IO handles for the process itself as well as
 *  unidirectional pipes for stdin, stdout, and stderr.
 *
 * \param[in]  command     Required. Command to execute.  If an absolute path is not provided, will search the PATH environment variable.  Will honor PATH specified in environ.
 * \param[in]  args        Optional. List of arguments to pass to command.
 * \param[in]  env         Optional. List of environment variables to pass on to process.  Use NULL to pass current environment through.
 * \param[in]  timeout_ms  Optional. Maximum execution time of the process before it is forcibly terminated.  Use 0 for infinite.
 * \param[out] proc        Required. The io object handle for the process itself.  Used to notify when the process has exited, or request termination of process.
 * \param[out] proc_stdin  Optional. The io object handle for the write-only stdin process handle.  If NULL is passed, will close handle to process.
 * \param[out] proc_stdout Optional. The io object handle for the read-only stdout process handle.  If NULL is passed, will close handle to process.
 * \param[out] proc_stderr Optional. The io object handle for the read-only stderr process handle.  If NULL is passed, will close handle to process.
 *
 * \return M_IO_ERROR_SUCCESS on SUCCESS.  Otherwise, M_IO_ERROR_INVALID on misuse, M_IO_ERROR_NOTFOUND if specified executable not found, M_IO_ERROR_NOTPERM if execution not permitted.
 */
M_API M_io_error_t M_io_process_create(const char *command, M_list_str_t *args, M_hash_dict_t *env, M_uint64 timeout_ms, M_io_t **proc, M_io_t **proc_stdin, M_io_t **proc_stdout, M_io_t **proc_stderr);

/*! Retrieve the result code of the process.
 *
 * \param[in] proc         Proc IO object returned from M_io_process_create();
 * \param[out] result_code Exit code returned from process.
 *
 * \return M_TRUE if process exited with a return code.  M_FALSE if invalid request due to process state or other error.
 */
M_API M_bool M_io_process_get_result_code(M_io_t *proc, int *result_code);

/*! Retrieve the OS Process ID of the process.
 *
 * \param[in] proc   Proc IO object returned from M_io_process_create();
 * \return Return the process id of the process
 */
M_API int M_io_process_get_pid(M_io_t *proc);

/*! @} */

__END_DECLS

#endif

