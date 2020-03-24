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
#include "m_io_w32overlap.h"

M_EVENT_HANDLE M_io_pipe_get_fd(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, NULL);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_type_t    type   = M_io_get_type(io);
	M_EVENT_HANDLE fd     = type == M_IO_TYPE_READER?handle->rhandle:handle->whandle;
	M_io_layer_release(layer);
	return fd;
}

#define PIPE_BUFSIZE 4096

static M_uint32 M_io_pipe_id = 0;
M_io_error_t M_io_pipe_create(M_uint32 flags, M_io_t **reader, M_io_t **writer)
{
	HANDLE              r;
	HANDLE              w;
	M_io_handle_t      *riohandle;
	M_io_handle_t      *wiohandle;
	char                pipename[256];
	M_io_callbacks_t   *callbacks;
	SECURITY_ATTRIBUTES sa;

	if (reader == NULL || writer == NULL)
		return M_IO_ERROR_ERROR;

	*reader = NULL;
	*writer = NULL;

	M_snprintf(pipename, sizeof(pipename), "\\\\.\\Pipe\\Anon.%08x.%08x", GetCurrentProcessId(), M_atomic_inc_u32(&M_io_pipe_id));

	/* NOTE: only 1 end of a named pipe can be inherited, and it MUST be the *pipe* not the CreateFile end.  So we need to
	 *       switch between inbound and outbound on the pipe, and GENERIC_READ vs GENERIC_WRITE on the file */

	M_mem_set(&sa, 0, sizeof(sa));
	sa.nLength              = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle       = flags & (M_IO_PIPE_INHERIT_READ|M_IO_PIPE_INHERIT_WRITE)?TRUE : FALSE;

	if (flags & M_IO_PIPE_INHERIT_WRITE) {
		w = CreateNamedPipeA(pipename,
			PIPE_ACCESS_OUTBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE|FILE_FLAG_OVERLAPPED,
			PIPE_READMODE_BYTE /* |PIPE_REJECT_REMOTE_CLIENTS */, 
			1,
			/* These are supposedly advisory and the OS will grow them */
			PIPE_BUFSIZE,
			PIPE_BUFSIZE,
			0,
			&sa);

		r = CreateFileA(pipename,
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,
			NULL);
	} else { /* M_IO_PIP_INHERIT_READ or no inheritance */
		r = CreateNamedPipeA(pipename,
			PIPE_ACCESS_INBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE|FILE_FLAG_OVERLAPPED,
			PIPE_READMODE_BYTE /* |PIPE_REJECT_REMOTE_CLIENTS */, 
			1,
			/* These are supposedly advisory and the OS will grow them */
			PIPE_BUFSIZE,
			PIPE_BUFSIZE,
			0,
			&sa);

		w = CreateFileA(pipename,
			GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,
			NULL);
	}

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

