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
#include "mstdlib/mstdlib_io.h"
#include "m_event_int.h"
#include "m_io_int.h"
#include "base/m_defs_int.h"
#include "m_io_w32overlap.h"
#include "m_io_win32_common.h"

#define MAX_IO_BUFFER_SIZE (8 * 1024 * 1024) /* 8MB */

static void M_io_w32overlap_unreg(M_io_t *io, M_io_handle_t *handle)
{
	M_event_t     *event  = M_io_get_event(io);

	if (handle->rhandle != M_EVENT_INVALID_HANDLE) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->roverlapped.hEvent, M_EVENT_INVALID_SOCKET, 0, 0);
	}
	if (handle->whandle != M_EVENT_INVALID_HANDLE) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_HANDLE, io, handle->woverlapped.hEvent, M_EVENT_INVALID_SOCKET, 0, 0);
	}
}


M_io_handle_t *M_io_w32overlap_init_handle(HANDLE rhandle, HANDLE whandle)
{
	M_io_handle_t *handle = M_malloc_zero(sizeof(*handle));

	M_io_w32overlap_update_handle(handle, rhandle, whandle);

	return handle;
}


void M_io_w32overlap_update_handle(M_io_handle_t *handle, HANDLE rhandle, HANDLE whandle)
{
	if (rhandle != M_EVENT_INVALID_HANDLE) {
		handle->rhandle       = rhandle;
		if (handle->roverlapped.hEvent == M_EVENT_INVALID_HANDLE)
			handle->roverlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (handle->rbuf == NULL)
			handle->rbuf               = M_buf_create();
	}
	if (handle->rhandle == NULL)
		handle->roverlapped.hEvent = M_EVENT_INVALID_HANDLE;

	if (whandle != NULL) {
		handle->whandle       = whandle;
		if (handle->woverlapped.hEvent == M_EVENT_INVALID_HANDLE)
			handle->woverlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (handle->wbuf == NULL)
			handle->wbuf               = M_buf_create();
	}
	if (handle->whandle == NULL)
		handle->woverlapped.hEvent = M_EVENT_INVALID_HANDLE;
}


static void M_io_w32overlap_close_handle(M_io_handle_t *handle)
{
	/* Stop disconnect timer if it is running */
	if (handle->disconnect_timer) {
		M_event_timer_remove(handle->disconnect_timer);
		handle->disconnect_timer = NULL;
	}

	/* Cancel any pending overlapped io operations */
#if 0 /* _WIN32_WINNT >= _WIN32_WINNT_VISTA */
	if (handle->rhandle != M_EVENT_INVALID_HANDLE && handle->rwaiting) {
		CancelIoEx(handle->rhandle, &handle->roverlapped);
		handle->rwaiting = M_FALSE;
	}
	if (handle->whandle != M_EVENT_INVALID_HANDLE && handle->wwaiting) {
		CancelIoEx(handle->whandle, &handle->woverlapped);
		handle->wwaiting = M_FALSE;
	}
#else
	/* Caveat: only closes operations started by current thread.  Hopefully this
	 *         is always true */
	if (handle->rhandle != NULL && handle->rwaiting) {
		CancelIo(handle->rhandle);
		handle->rwaiting = M_FALSE;
		/* If handles are the same, then write handle was also canceled */
		if (handle->rhandle == handle->whandle)
			handle->wwaiting = M_FALSE;
	}
	if (handle->whandle != NULL && handle->wwaiting) {
		CancelIo(handle->whandle);
		handle->wwaiting = M_FALSE;
	}
#endif

	/* Cleanup may actually operate on the open handle, so this should
	 * be called before closing the handle */
	if (handle->priv_cleanup) {
		handle->priv_cleanup(handle);
	}

	if (handle->rhandle != NULL) {
		/* Read and Write handles may be the same handle.  Lets make sure we only
		 * close it once */
		if (handle->rhandle == handle->whandle) {
			handle->whandle = NULL;
		}
		CloseHandle(handle->rhandle);
		handle->rhandle = NULL;
	}
	if (handle->roverlapped.hEvent != M_EVENT_INVALID_HANDLE) {
		CloseHandle(handle->roverlapped.hEvent);
		handle->roverlapped.hEvent = M_EVENT_INVALID_HANDLE;
	}
	if (handle->whandle != NULL) {
		CloseHandle(handle->whandle);
		handle->whandle = NULL;
	}
	if (handle->woverlapped.hEvent != M_EVENT_INVALID_HANDLE) {
		CloseHandle(handle->woverlapped.hEvent);
		handle->woverlapped.hEvent = M_EVENT_INVALID_HANDLE;
	}
}


void M_io_w32overlap_close(M_io_layer_t *layer)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->rhandle == NULL && handle->whandle == NULL)
		return;

	if (event) {
		M_io_w32overlap_unreg(io, handle);
	}

	M_io_w32overlap_close_handle(handle);
}


/* Add prototype since we call ourselves recursively */
static M_io_error_t M_io_w32overlap_startrw(M_io_layer_t *layer, M_bool is_read);

static M_io_error_t M_io_w32overlap_startrw(M_io_layer_t *layer, M_bool is_read)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	BOOL           rv;
	M_io_error_t   err;

	if (is_read) {
		size_t         bufsize;
		unsigned char *buf;
		if (handle->rwaiting)
			return M_IO_ERROR_WOULDBLOCK;
		handle->rwaiting = M_TRUE;
		ResetEvent(handle->roverlapped.hEvent);

		/* Get handle to buffer for writing.  Check to see if we should grow */
		bufsize = M_buf_alloc_size(handle->rbuf);
		if (handle->rbuffull && bufsize < MAX_IO_BUFFER_SIZE)
			bufsize *= 2;
		buf     = M_buf_direct_write_start(handle->rbuf, &bufsize);
//M_printf("%s(): started read layer %p, evhandle %p\n", __FUNCTION__, layer, handle->rhandle);
		rv      = ReadFile(handle->rhandle, buf, (DWORD)bufsize, NULL, &handle->roverlapped);

		handle->rbuffull = M_FALSE;
	} else {
		if (handle->wwaiting)
			return M_IO_ERROR_WOULDBLOCK;
		handle->wwaiting = M_TRUE;
		ResetEvent(handle->woverlapped.hEvent);
//M_printf("%s(): started write layer %p, evhandle %p\n", __FUNCTION__, layer, handle->whandle);
		rv      = WriteFile(handle->whandle, (const unsigned char *)M_buf_peek(handle->wbuf), (DWORD)M_buf_len(handle->wbuf), NULL, &handle->woverlapped);
	}
	if (rv == TRUE) {
		/* Our event handle will still be triggered to let us know there is data. */
		return M_IO_ERROR_WOULDBLOCK;
	}

	handle->last_error_sys = GetLastError();
	err                    = M_io_win32_err_to_ioerr(handle->last_error_sys);
	if (err == M_IO_ERROR_WOULDBLOCK)
		return err;

	M_io_w32overlap_close(layer);
	M_io_layer_softevent_add(layer, M_TRUE, (err == M_IO_ERROR_DISCONNECT)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR, err);
	return err;
}


M_bool M_io_w32overlap_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	M_io_type_t    type   = M_io_get_type(io);

	if (handle->rhandle == NULL && handle->whandle == NULL)
		return M_FALSE;

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);

	/* Connect event handles to event system */
	if (type == M_IO_TYPE_WRITER || type == M_IO_TYPE_STREAM) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->woverlapped.hEvent, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_WRITE, M_EVENT_CAPS_WRITE);
	}
	if (type == M_IO_TYPE_READER || type == M_IO_TYPE_STREAM) {
		M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, handle->roverlapped.hEvent, M_EVENT_INVALID_SOCKET, M_EVENT_WAIT_READ, M_EVENT_CAPS_READ);
	}

	return M_TRUE;
}


M_bool M_io_w32overlap_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    ctype  = M_io_get_type(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;

//M_printf("%s(): layer %p, evhandle %p, type %d, ctype %d, handle %p, io %p\n", __FUNCTION__, layer, evhandle, (int)*type, (int)ctype, handle, io);

	/* Upon connect, start read operation now if not already going */
	if (*type == M_EVENT_TYPE_CONNECTED && (ctype == M_IO_TYPE_READER || ctype == M_IO_TYPE_STREAM) && !handle->rwaiting) {
		err = M_io_w32overlap_startrw(layer, M_TRUE);
		if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
			M_io_w32overlap_close(layer);
			*type = M_EVENT_TYPE_ERROR;
			return M_FALSE;
		}
	}


	if (*type == M_EVENT_TYPE_WRITE && (ctype == M_IO_TYPE_WRITER || ctype == M_IO_TYPE_STREAM)) {
		DWORD bytes = 0;
		BOOL  rv    = GetOverlappedResult(handle->whandle, &handle->woverlapped, &bytes, FALSE);

		if (rv != TRUE) {
			handle->last_error_sys = GetLastError();
			err                    = M_io_win32_err_to_ioerr(handle->last_error_sys);
			if (err == M_IO_ERROR_WOULDBLOCK)
				return M_TRUE;

			M_io_w32overlap_close(layer);
			if (err == M_IO_ERROR_DISCONNECT) {
				*type = M_EVENT_TYPE_DISCONNECTED;
			} else {
				*type = M_EVENT_TYPE_ERROR;
			}

			return M_FALSE;
		}

		handle->wwaiting = M_FALSE;

		/* Drop bytes that were successfully written */
		M_buf_drop(handle->wbuf, bytes);

		if (M_buf_len(handle->wbuf)) {
			/* Not all data was written, enqueue the remainder to be written */

			/* Tell the system that the last write was not full so the buffer won't grow */
			handle->wbuffull = M_FALSE;

			err = M_io_w32overlap_startrw(layer, M_FALSE);
			if (err == M_IO_ERROR_SUCCESS) {
				/* Fall Thru */
			} else if (err == M_IO_ERROR_DISCONNECT) {
				*type = M_EVENT_TYPE_DISCONNECTED;
				return M_FALSE;
			} else if (err == M_IO_ERROR_WOULDBLOCK) {
				/* Consume the event like nothing happened as we re-enqueued more data */
				return M_TRUE;
			} else { /* E.g. Error */
				*type = M_EVENT_TYPE_ERROR;
				return M_FALSE;
			}
		}
	}

	if (*type == M_EVENT_TYPE_READ && (ctype == M_IO_TYPE_READER || ctype == M_IO_TYPE_STREAM)) {
		/* Fetch result */
		DWORD bytes = 0;
		BOOL  rv    = GetOverlappedResult(handle->rhandle, &handle->roverlapped, &bytes, FALSE);

		if (rv != TRUE) {
			handle->last_error_sys = GetLastError();
			err                    = M_io_win32_err_to_ioerr(handle->last_error_sys);
			if (err == M_IO_ERROR_WOULDBLOCK)
				return M_TRUE;

			/* Error, record that we read nothing */
			M_buf_direct_write_end(handle->rbuf, 0);

			/* Error, disconnect? */
			if (err == M_IO_ERROR_DISCONNECT) {
				*type = M_EVENT_TYPE_DISCONNECTED;
			} else {
				*type = M_EVENT_TYPE_ERROR;
			}
			M_io_w32overlap_close(layer);

			return M_FALSE;
		}
		handle->rwaiting = M_FALSE;
		/* Record number of bytes read */
		M_buf_direct_write_end(handle->rbuf, bytes);
		if (M_buf_len(handle->rbuf) == M_buf_alloc_size(handle->rbuf))
			handle->rbuffull = M_TRUE;
	}

	/* Check to see if a disconnect was requested, if so and we got a WRITE event,
	 * reset the event timer to 1/10s longer to ensure data is really flushed
	 * and consume the write event */
	if (*type == M_EVENT_TYPE_WRITE && handle->disconnect_timer != NULL) {
		M_event_timer_reset(handle->disconnect_timer, 100 /* 1/10s */);
		return M_TRUE;
	}

	return M_FALSE;
}


void M_io_w32overlap_unregister_cb(M_io_layer_t *layer)
{
	M_io_t *io = M_io_layer_get_io(layer);
	M_io_w32overlap_unreg(io, M_io_layer_get_handle(layer));
}


void M_io_w32overlap_destroy_handle(M_io_handle_t *handle)
{
	M_io_w32overlap_close_handle(handle);
	M_buf_cancel(handle->wbuf);
	M_buf_cancel(handle->rbuf);

	M_free(handle);
}


void M_io_w32overlap_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;
	M_io_w32overlap_destroy_handle(handle);
}


M_io_error_t M_io_w32overlap_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	size_t         len;
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    type   = M_io_get_type(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)meta;

	if (io == NULL || layer == NULL || buf == NULL || read_len == NULL || *read_len == 0 || (type != M_IO_TYPE_READER && type != M_IO_TYPE_STREAM))
		return M_IO_ERROR_INVALID;

	if (handle->rhandle == NULL)
		return M_IO_ERROR_ERROR;

	if (handle->rwaiting)
		return M_IO_ERROR_WOULDBLOCK;

	len = *read_len;
	if (len > M_buf_len(handle->rbuf))
		len = M_buf_len(handle->rbuf);

	if (len)
		M_mem_copy(buf, (const unsigned char *)M_buf_peek(handle->rbuf), len);
	*read_len = len;

	M_buf_drop(handle->rbuf, len);

	if (M_buf_len(handle->rbuf)) {
		/* Partial read from our buffer, don't start another read op yet */
		return M_IO_ERROR_SUCCESS;
	}

	/* If we're here, we can start a new Read operation as the buffer is empty, ignore
	 * any error conditions since we have data we need to return */
	M_io_w32overlap_startrw(layer, M_TRUE);

	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_w32overlap_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	size_t         len;
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    type   = M_io_get_type(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)meta;

	if (io == NULL || layer == NULL || buf == NULL || write_len == NULL || *write_len == 0 || (type != M_IO_TYPE_WRITER && type != M_IO_TYPE_STREAM))
		return M_IO_ERROR_INVALID;

	if (handle->whandle == NULL)
		return M_IO_ERROR_ERROR;

	/* See if a write operation is already in progress */
	if (M_buf_len(handle->wbuf))
		return M_IO_ERROR_WOULDBLOCK;

	/* Add data to the buffer */
	len = *write_len;
	if (len > M_buf_alloc_size(handle->wbuf)) {
		if (handle->wbuffull && M_buf_alloc_size(handle->wbuf) < MAX_IO_BUFFER_SIZE) {
			/* Allow the buffer to double! */
			size_t maxsize = M_buf_alloc_size(handle->wbuf) * 2;
			if (len > maxsize)
				len = maxsize;
		} else {
			/* Truncate write request to size of buffer */
			len = M_buf_alloc_size(handle->wbuf);
		}
	}
	M_buf_add_bytes(handle->wbuf, buf, len);
	*write_len              = len;

	/* Pre-set the full write flag if we filled the buffer.  We'll unset it at the end of
	 * the write if it wasn't actually a full write ... we do this otherwise we'd need to
	 * track another variable for the partial write sequence in process_cb */
	if (M_buf_len(handle->wbuf) == M_buf_alloc_size(handle->wbuf))
		handle->wbuffull = M_TRUE;

	/* If we're here, we just enqueued new data into the write buffer. Ignore
	 * any error conditions since we have enqueued data. */
	M_io_w32overlap_startrw(layer, M_FALSE);

/* XXX: We need a way to silence the internally-generated soft WRITE event when this function
 *      returns success.  Since Windows uses io completion ports, we'll get a WRITE event
 *      from that as the soft even will be generated before the data can *actually* be
 *      written */
	return M_IO_ERROR_SUCCESS;
}


M_io_state_t M_io_w32overlap_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    type   = M_io_get_type(io);

	if ((type == M_IO_TYPE_WRITER || type == M_IO_TYPE_STREAM) && handle->whandle == NULL)
		return M_IO_STATE_ERROR;

	if ((type == M_IO_TYPE_READER || type == M_IO_TYPE_STREAM) && handle->rhandle == NULL)
		return M_IO_STATE_ERROR;

	return M_IO_STATE_CONNECTED;
}


M_bool M_io_w32overlap_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	return M_io_win32_errormsg(handle->last_error_sys, error, err_len);
}


static void M_io_w32overlap_disc_timer_cb(M_event_t *event, M_event_type_t type, M_io_t *iodummy, void *arg)
{
	M_io_layer_t  *layer  = arg;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)event;
	(void)type;
	(void)iodummy;

	if (handle->whandle != NULL) {
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
	}

	handle->disconnect_timer = NULL;
}


/* We use the disconnect callback because we want to try to delay closing until all
 * data has been written.  That said, we need to use a timer so we don't hang forever */
M_bool M_io_w32overlap_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    type   = M_io_get_type(io);
	M_event_t     *event  = M_io_get_event(io);

	/* Can't write because not a writer or the handle is already closed */
	if ((type != M_IO_TYPE_WRITER && type != M_IO_TYPE_STREAM) || handle->whandle == NULL)
		return M_TRUE;

	/* If the buffer length is empty, that means there is no pending write operation and we
	 * can go ahead and close */
	if (M_buf_len(handle->wbuf) == 0)
		return M_TRUE;

	/* Already disconnecting */
	if (handle->disconnect_timer)
		return M_FALSE;

	handle->disconnect_timer = M_event_timer_oneshot(event, 1000 /* 1s */, M_TRUE, M_io_w32overlap_disc_timer_cb, layer);

	return M_FALSE;
}
