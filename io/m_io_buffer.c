/* The MIT License (MIT)
 * 
 * Copyright (c) 2023 Monetra Technologies, LLC.
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

struct M_io_handle {
	size_t        max_read_buffer;  /*!< Maximum read buffer size allowed */
	M_buf_t      *readbuf;          /*!< buffer holding buffered data     */
	M_bool        hit_max_read;     /*!< We stopped reading because we hit current max size, track due to being edge triggered */
	size_t        max_write_buffer; /*!< Maximum size of write buffer allowed */
	M_buf_t      *writebuf;         /*!< buffer holding buffered write data */
	M_bool        hit_max_write;    /*!< we stopped allowing writes because we hit the max size, track due to being edge triggered */
};

static M_bool M_io_buffer_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
	return M_TRUE;
}


static M_bool M_io_buffer_process_read_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         alloc_size;
	size_t         req_size;
	size_t         len;
	unsigned char *buf    = NULL;
	M_io_error_t   err;
	M_bool         rv     = M_FALSE /* Propagate */;

	/* Not buffering reads, pass to next layer */
	if (handle->max_read_buffer == 0) {
		return M_FALSE; /* propagate */
	}

	/* If the prior read hit the buffer limit, double the buffer size
	 * up to the max. */
	if (handle->hit_max_read) {
		handle->hit_max_read = M_FALSE;
		alloc_size = M_size_t_round_up_to_power_of_two(M_buf_alloc_size(handle->readbuf) + 1);
		if (alloc_size > handle->max_read_buffer) {
			alloc_size = handle->max_read_buffer;
		}
	} else {
		alloc_size = M_buf_alloc_size(handle->readbuf);
	}

	/* Default buffer is 16KB */
	if (alloc_size < 16 * 1024)
		alloc_size = 16 * 1024;

	req_size = alloc_size - M_buf_len(handle->readbuf);

	/* Buffer is full, can't read.  Don't pass on event, application already knows there is data. */
	if (req_size == 0) {
		return M_TRUE; /* consume */
	}
	len      = req_size;
	buf      = M_buf_direct_write_start(handle->readbuf, &len);

	err      = M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, &len, NULL);
	if (err != M_IO_ERROR_SUCCESS) {
		len = 0;
		rv  = M_TRUE; /* consume */
	}
	M_buf_direct_write_end(handle->readbuf, len);

	if (len >= req_size) {
		handle->hit_max_read = M_TRUE;
	}
//M_dprintf(1, "%s(): layer=%p, read_len=%zu, buf_len=%zu, hit_max=%s\n", __FUNCTION__, layer, len, M_buf_alloc_size(handle->readbuf), handle->hit_max_read?"yes":"no");
	/* If the buffer contains bytes other than what we added, then the application hadn't previously read
	 * everything and knows there's more.  Don't relay */
	if (M_buf_len(handle->readbuf) != len) {
		rv = M_TRUE; /* consume */
	}

	return rv;
}

static M_bool M_io_buffer_process_write_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         len;
	M_io_error_t   err;

	/* Not buffering writes, pass to next layer */
	if (handle->max_write_buffer == 0) {
		return M_FALSE; /* propagate */
	}

	len = M_buf_len(handle->writebuf);
	if (len == 0)
		return M_FALSE; /* propagate */

	err = M_io_layer_write(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, (const unsigned char *)M_buf_peek(handle->writebuf), &len, NULL);

	if (err != M_IO_ERROR_SUCCESS || len == 0) {
		return M_TRUE; /* consume */
	}

//M_dprintf(1, "%s(): layer=%p, write_len=%zu, buf_len=%zu, hit_max=%s\n", __FUNCTION__, layer, len, M_buf_len(handle->writebuf), handle->hit_max_write?"yes":"no");

	M_buf_drop(handle->writebuf, len);

	if (!handle->hit_max_write)
		return M_TRUE; /* consume */

	handle->hit_max_write = M_FALSE;
	return M_FALSE; /* Propagate */
}


static M_bool M_io_buffer_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	switch (*type) {
		case M_EVENT_TYPE_READ:
			return M_io_buffer_process_read_cb(layer);

		case M_EVENT_TYPE_WRITE:
			return M_io_buffer_process_write_cb(layer);

		case M_EVENT_TYPE_OTHER:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			break;
		case M_EVENT_TYPE_ERROR:
			break;
		case M_EVENT_TYPE_CONNECTED:
			break;
		case M_EVENT_TYPE_ACCEPT:
			break;
	}

	/* Pass on event to next layer */
	return M_FALSE;
}


static void M_io_buffer_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
}


static M_io_error_t M_io_buffer_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         len    = 0;

	if (layer == NULL || handle == NULL || meta != NULL)
		return M_IO_ERROR_INVALID;

	/* Not doing buffered reads, just pass through */
	if (handle->max_read_buffer == 0) {
		return M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, read_len, NULL);
	}

	len = *read_len;
	if (M_buf_len(handle->readbuf) < len)
		len = M_buf_len(handle->readbuf);

	if (len == 0)
		return M_IO_ERROR_WOULDBLOCK;

	M_mem_copy(buf, M_buf_peek(handle->readbuf), len);
	M_buf_drop(handle->readbuf, len);

	*read_len = len;

	if (handle->hit_max_read) {
		/* We cleared room in the buffer, queue another read event */
		M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
	}

	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_buffer_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         len    = 0;

	if (layer == NULL || handle == NULL || meta != NULL)
		return M_IO_ERROR_INVALID;

	/* Not doing buffered writes, just pass through */
	if (handle->max_write_buffer == 0) {
		return M_io_layer_write(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, write_len, NULL);
	}

	len = *write_len;

	if (M_buf_len(handle->writebuf) + len >= handle->max_write_buffer) {
		len                   = handle->max_write_buffer - M_buf_len(handle->writebuf);
		handle->hit_max_write = M_TRUE;
	}

	if (len == 0)
		return M_IO_ERROR_WOULDBLOCK;

	M_buf_add_bytes(handle->writebuf, buf, len);
	*write_len = len;

	/* Lets tell ourselves that we have data to write. */
	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);

	return M_IO_ERROR_SUCCESS;
}


static M_bool M_io_buffer_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_buf_truncate(handle->readbuf, 0);
	handle->hit_max_read = M_FALSE;
	M_buf_truncate(handle->writebuf, 0);
	return M_TRUE;
}


static void M_io_buffer_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	M_buf_cancel(handle->readbuf);
	M_buf_cancel(handle->writebuf);

	M_free(handle);
}

static M_io_error_t M_io_buffer_accept_cb(M_io_t *io, M_io_layer_t *orig_layer)
{
	size_t         layer_id;
	M_io_handle_t *orig_handle = M_io_layer_get_handle(orig_layer);

	/* Add a new layer into the new comm object with the same settings as we have */
	return M_io_add_buffer(io, &layer_id, orig_handle->max_read_buffer, orig_handle->max_write_buffer);
}

M_io_error_t M_io_add_buffer(M_io_t *io, size_t *layer_id, size_t max_read_buffer, size_t max_write_buffer)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_layer_t     *layer;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	handle = M_malloc_zero(sizeof(*handle));
	if (max_read_buffer != 0) {
		handle->max_read_buffer  = M_size_t_round_up_to_power_of_two(max_read_buffer);
	}

	if (max_write_buffer != 0) {
		handle->max_write_buffer = M_size_t_round_up_to_power_of_two(max_write_buffer);
	}

	if (handle->max_read_buffer)
		handle->readbuf          = M_buf_create();

	if (handle->max_write_buffer)
		handle->writebuf         = M_buf_create();

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_buffer_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_buffer_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_buffer_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_buffer_process_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_buffer_accept_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_buffer_unregister_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_buffer_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_buffer_destroy_cb);
	layer = M_io_layer_add(io, "BUFFER", handle, callbacks);
	M_io_callbacks_destroy(callbacks);
//M_dprintf(1, "%s(): added buffer to %p as layer %zu\n", __FUNCTION__, io, M_io_layer_get_index(layer));
	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);
	return M_IO_ERROR_SUCCESS;
}

