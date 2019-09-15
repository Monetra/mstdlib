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


M_io_handle_t *M_io_w32overlap_busyemu_init_handle(HANDLE rhandle, HANDLE whandle)
{
	M_io_handle_t *handle = M_malloc_zero(sizeof(*handle));

	M_io_w32overlap_busyemu_update_handle(handle, rhandle, whandle);

	return handle;
}


void M_io_w32overlap_busyemu_update_handle(M_io_handle_t *handle, HANDLE rhandle, HANDLE whandle)
{
	if (rhandle != NULL) {
		handle->rhandle       = rhandle;
		if (handle->rbuf == NULL)
			handle->rbuf               = M_buf_create();
	}

	if (whandle != NULL) {
		handle->whandle       = whandle;
		if (handle->wbuf == NULL)
			handle->wbuf               = M_buf_create();
	}
}


static void M_io_w32overlap_busyemu_close_handle(M_io_handle_t *handle)
{
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

	if (handle->whandle != NULL) {
		CloseHandle(handle->whandle);
		handle->whandle = NULL;
	}

}

/* close will unregister and wait on the thread to shutdown, vs closehandle will not */
void M_io_w32overlap_busyemu_close(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->rhandle == NULL && handle->whandle == NULL)
		return;

	M_io_w32overlap_busyemu_unregister_cb(layer);

	M_io_w32overlap_busyemu_close_handle(handle);
}


typedef struct {
	M_bool      timer;
	M_timeval_t tv;
} M_io_w32overlap_busyemu_threadstate_t;


static M_bool M_io_w32overlap_busyemu_thread_should_run(M_io_handle_t *handle, M_io_w32overlap_busyemu_threadstate_t *state)
{
	switch (handle->busyemu_state) {
		case M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED:
			return M_FALSE;
		case M_IO_W32OVERLAP_BUSYEMU_STATE_RUNNING:
			return M_TRUE;
		case M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT:
			break;
	}

	/* == M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT == */

	/* No data left to be written, no need to keep running */
	if (M_buf_len(handle->wbuf) == 0)
		return M_FALSE;

	/* If timer already started, and more than 1s has passed, stop running */
	if (state->timer && M_time_elapsed(&state->tv) > 1000) {
		return M_FALSE;
	}

	/* Timer not yet started, start it */
	if (!state->timer) {
		state->timer = M_TRUE;
		M_time_elapsed_start(&state->tv);
	}

	/* Keep running */
	return M_TRUE;
}


static void *M_io_w32overlap_busyemu_thread(void *arg)
{
	M_io_layer_t                         *layer     = arg;
	M_io_t                               *io        = M_io_layer_get_io(layer);
	M_io_handle_t                        *handle    = M_io_layer_get_handle(layer);
	unsigned char                        *buf;
	DWORD                                 retlen;
	size_t                                bufsize;
	M_io_w32overlap_busyemu_threadstate_t state;

	M_mem_set(&state, 0, sizeof(state));

	while (M_io_w32overlap_busyemu_thread_should_run(handle, &state)) {
		/* Lock! */
		M_io_layer_acquire(io, 0, NULL);

		/* Try to do a direct read into the buffer, nonblocking */
		bufsize          = M_buf_alloc_size(handle->rbuf);
		if (handle->rbuffull && bufsize < MAX_IO_BUFFER_SIZE)
			bufsize *= 2;
		buf              = M_buf_direct_write_start(handle->rbuf, &bufsize);
		handle->rbuffull = M_FALSE;
		if (!ReadFile(handle->rhandle, buf, (DWORD)bufsize, &retlen, NULL)) {
			M_buf_direct_write_end(handle->rbuf, 0);
			goto fail;
		}

		M_buf_direct_write_end(handle->rbuf, retlen);
		if (retlen == bufsize)
			handle->rbuffull = M_TRUE;

		/* Buffer was empty, trigger a READ signal */
		if (retlen != 0 && M_buf_len(handle->rbuf) == retlen) {
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		}

		/* Try to do a direct write from the buffer, non-blocking */
		if (M_buf_len(handle->wbuf)) {
			if (!WriteFile(handle->whandle, M_buf_peek(handle->wbuf), (DWORD)M_buf_len(handle->wbuf), &retlen, NULL))
				goto fail;

			/* If all requested data wasn't written, mark as such so buffer doesn't grow */
			if (retlen != M_buf_len(handle->wbuf))
				handle->wbuffull = M_FALSE;

			M_buf_drop(handle->wbuf, retlen);

			/* No data left, need more, trigger WRITE signal */
			if (M_buf_len(handle->wbuf) == 0) {
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
			}
		}

		/* Unlock! */
		M_io_layer_release(layer);

		/* Loop slowly, this is busy polling */
		M_thread_sleep(15000); /* 15ms */
	}

	/* If a disconnect was requested, close the handle, issue a disconnect event */
	if (handle->busyemu_state == M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT) {
		/* Delay 1/10th of a second to make sure all data is really flushed */
		M_thread_sleep(100000);
		M_io_layer_acquire(io, 0, NULL);
		M_io_w32overlap_busyemu_close_handle(handle);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
		M_io_layer_release(layer);
	}

	handle->busyemu_state = M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED;
	return NULL;

fail:

	/* Record error */
	handle->last_error_sys = GetLastError();

	/* Close device down */
	M_io_w32overlap_busyemu_close_handle(handle);

	/* Send disconnect or error signal depending on which is appropriate */
	M_io_layer_softevent_add(layer, M_TRUE, (M_io_win32_err_to_ioerr(handle->last_error_sys) == M_IO_ERROR_DISCONNECT)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR, M_io_win32_err_to_ioerr(handle->last_error_sys));

	/* Mark thread as shutdown */
	handle->busyemu_state = M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED;

	/* Failure is always holding a lock, unlock */
	M_io_layer_release(layer);

	return NULL;
}


M_bool M_io_w32overlap_busyemu_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_thread_attr_t *tattr;

	if (handle->rhandle == NULL && handle->whandle == NULL)
		return M_FALSE;

	/* Trigger connected soft event when registered with event handle */
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);

	/* Start thread */
	handle->busyemu_state  = M_IO_W32OVERLAP_BUSYEMU_STATE_RUNNING;
	tattr                  = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	handle->busyemu_thread = M_thread_create(tattr, M_io_w32overlap_busyemu_thread, layer);
	M_thread_attr_destroy(tattr);

	return M_TRUE;
}


M_bool M_io_w32overlap_busyemu_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	(void)layer;
	(void)type;

	/* No-op, pass thru */
	return M_FALSE;
}


void M_io_w32overlap_busyemu_unregister_cb(M_io_layer_t *layer)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Wait for thread to exit.
	 * WARNING: this may technically block if the thread is sleeping, however in
	 *          most use cases, the thread will already be stopped when it gets
	 *          here either due to a requested disconnect or an error condition. */
	if (handle->busyemu_state != M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED) {
		handle->busyemu_state = M_IO_W32OVERLAP_BUSYEMU_STATE_STOPPED;

		/* Temporarily release lock */
		M_io_layer_release(layer);

		/* Wait for thread to exit */
		M_thread_join(handle->busyemu_thread, NULL);
		handle->busyemu_thread = 0;

		/* Re-gain lock */
		M_io_layer_acquire(io, 0, NULL);
	}

	/* Join thread that is already stopped to clean up resources */
	if (handle->busyemu_thread != 0) {
		M_thread_join(handle->busyemu_thread, NULL);
		handle->busyemu_thread = 0;
	}
}

void M_io_w32overlap_busyemu_destroy_handle(M_io_handle_t *handle)
{
	/* NOTE: thread is guaranteed to be NOT running if we're here */
	M_io_w32overlap_busyemu_close_handle(handle);
	M_buf_cancel(handle->wbuf);
	M_buf_cancel(handle->rbuf);

	M_free(handle);
}


void M_io_w32overlap_busyemu_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle == NULL)
		return;
	M_io_w32overlap_busyemu_destroy_handle(handle);
}


M_io_error_t M_io_w32overlap_busyemu_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
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

	if (M_buf_len(handle->rbuf) == 0)
		return M_IO_ERROR_WOULDBLOCK;

	len = *read_len;
	if (len > M_buf_len(handle->rbuf))
		len = M_buf_len(handle->rbuf);

	if (len)
		M_mem_copy(buf, (const unsigned char *)M_buf_peek(handle->rbuf), len);
	*read_len = len;

	M_buf_drop(handle->rbuf, len);

	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_w32overlap_busyemu_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
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

	/* Add data to the buffer */
	len = *write_len;
	if (len > M_buf_alloc_size(handle->wbuf) - M_buf_len(handle->wbuf)) {
		/* If the last write was a full write, and there's no additional data already in the buffer, and we
		 * haven't exceeded our pre-determined size, allow the buffer to double */
		if (handle->wbuffull && M_buf_len(handle->wbuf) == 0 && M_buf_alloc_size(handle->wbuf) < MAX_IO_BUFFER_SIZE) {
			size_t maxsize = M_buf_alloc_size(handle->wbuf) * 2;
			if (len > maxsize)
				len = maxsize;
		} else {
			/* Truncate write request to remaining size of buffer */
			len = M_buf_alloc_size(handle->wbuf) - M_buf_len(handle->wbuf);
		}
	}
	if (len) {
		M_buf_add_bytes(handle->wbuf, buf, len);
		*write_len = len;
	} else {
		*write_len = 0;
		return M_IO_ERROR_WOULDBLOCK;
	}

	/* Pre-set the full write flag if we filled the buffer.  We'll unset it at the end of
	 * the write if it wasn't actually a full write ... we do this otherwise we'd need to
	 * track another variable for the partial write sequence */
	if (M_buf_alloc_size(handle->wbuf) == M_buf_len(handle->wbuf) && M_buf_len(handle->wbuf) == len)
		handle->wbuffull = M_TRUE;

	return M_IO_ERROR_SUCCESS;
}


M_io_state_t M_io_w32overlap_busyemu_state_cb(M_io_layer_t *layer)
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


M_bool M_io_w32overlap_busyemu_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	return M_io_win32_errormsg(handle->last_error_sys, error, err_len);
}


M_bool M_io_w32overlap_busyemu_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_type_t    type   = M_io_get_type(io);

	/* Can't write because not a writer or the handle is already closed */
	if ((type != M_IO_TYPE_WRITER && type != M_IO_TYPE_STREAM) || handle->whandle == M_EVENT_INVALID_HANDLE)
		return M_TRUE;

	/* If already trying to disconnect, wait longer */
	if (handle->busyemu_state == M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT)
		return M_FALSE;

	if (handle->busyemu_state != M_IO_W32OVERLAP_BUSYEMU_STATE_RUNNING)
		return M_TRUE;

	/* Request thread to shutdown, it will notify us when it does */
	handle->busyemu_state = M_IO_W32OVERLAP_BUSYEMU_STATE_REQ_DISCONNECT;

	return M_FALSE;
}
