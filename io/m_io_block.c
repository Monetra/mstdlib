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


enum M_io_block_request {
	M_IO_SYNC_REQUEST_CONNECT    = 1,
	M_IO_SYNC_REQUEST_READUCHAR  = 2,
	M_IO_SYNC_REQUEST_READBUF    = 3,
	M_IO_SYNC_REQUEST_READPARSER = 4,
	M_IO_SYNC_REQUEST_WRITEUCHAR = 5,
	M_IO_SYNC_REQUEST_WRITEBUF   = 6,
	M_IO_SYNC_REQUEST_DISCONNECT = 7,
	M_IO_SYNC_REQUEST_ACCEPT     = 8
};
typedef enum M_io_block_request M_io_block_request_t;


struct M_io_block_data {
	M_io_block_request_t request;
	M_io_error_t         retval;
	M_bool               done;

	M_io_t              *newconn;     /* Used for ACCEPT */
	M_buf_t             *buf_buf;     /* Used for read_into_buf and write_from_buf */
	M_parser_t          *buf_parser;  /* Used for read_into_parser */
	unsigned char       *buf_uchar;   /* Used for read */
	const unsigned char *buf_cuchar;  /* Used for write */
	size_t               buf_len;     /* Used for read/write */
	size_t               out_len;     /* Used for read/write output length */
};


void M_io_block_data_free(M_io_t *io)
{
	M_free(io->sync_data);
	io->sync_data = NULL;
}


static void M_io_block_event(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_data)
{
	M_io_block_data_t *data = cb_data;

	/* Sanity check to prevent duplicate checks overwriting eachother.  We need to
	 * actually re-add the event we are ignoring as a soft event so it can be
	 * delivered later */
	if (data->done) {
		if (io != NULL) {
			size_t        idx   = M_io_layer_count(io)-1; /* Use last index */
			M_io_layer_t *layer = M_io_layer_acquire(io, idx, NULL);
			M_io_layer_softevent_add(layer, M_TRUE /* Sibling-only=true will make it only notify user callbacks */, type, M_IO_ERROR_SUCCESS);
			M_io_layer_release(layer);
		}
		return;
	}

	if (type == M_EVENT_TYPE_CONNECTED && data->request == M_IO_SYNC_REQUEST_CONNECT) {
		data->retval = M_IO_ERROR_SUCCESS;
		M_event_return(event);
		data->done = M_TRUE;
		return;
	}

	if (type == M_EVENT_TYPE_ACCEPT && data->request == M_IO_SYNC_REQUEST_ACCEPT) {
		data->retval = M_io_accept(&data->newconn, io);
		if (data->retval != M_IO_ERROR_WOULDBLOCK) {
			M_event_return(event);
			data->done = M_TRUE;
		}
		return;
	}

	if (type == M_EVENT_TYPE_READ) {
		if (data->request == M_IO_SYNC_REQUEST_READUCHAR) {
			data->retval = M_io_read(io, data->buf_uchar, data->buf_len, &data->out_len);
			if (data->retval != M_IO_ERROR_WOULDBLOCK) {
				M_event_return(event);
				data->done = M_TRUE;
			}
			return;
		}
		if (data->request == M_IO_SYNC_REQUEST_READBUF) {
			data->retval = M_io_read_into_buf(io, data->buf_buf);
			if (data->retval != M_IO_ERROR_WOULDBLOCK) {
				M_event_return(event);
				data->done = M_TRUE;
			}
			return;
		}
		if (data->request == M_IO_SYNC_REQUEST_READPARSER) {
			data->retval = M_io_read_into_parser(io, data->buf_parser);
			if (data->retval != M_IO_ERROR_WOULDBLOCK) {
				M_event_return(event);
				data->done = M_TRUE;
			}
			return;
		}
	}

	if (type == M_EVENT_TYPE_WRITE) {
		if (data->request == M_IO_SYNC_REQUEST_WRITEUCHAR) {
			data->retval = M_io_write(io, data->buf_cuchar, data->buf_len, &data->out_len);
			if (data->retval != M_IO_ERROR_WOULDBLOCK) {
				M_event_return(event);
				data->done = M_TRUE;
			}
			return;
		}
		if (data->request == M_IO_SYNC_REQUEST_WRITEBUF) {
			data->retval = M_io_write_from_buf(io, data->buf_buf);
			if (data->retval != M_IO_ERROR_WOULDBLOCK) {
				M_event_return(event);
				data->done = M_TRUE;
			}
			return;
		}
	}

	if (type == M_EVENT_TYPE_DISCONNECTED) {
		data->retval = M_IO_ERROR_DISCONNECT;
		M_event_return(event);
		data->done = M_TRUE;
		return;
	}

	if (type == M_EVENT_TYPE_ERROR) {
		data->retval = M_IO_ERROR_ERROR;
		M_event_return(event);
		data->done = M_TRUE;
		return;
	}
}


static M_bool M_io_block_regevent(M_io_t *io, M_io_block_request_t request, M_io_block_data_t **ret_data)
{
	M_event_t *event;
	M_io_block_data_t *data;

	if (io == NULL)
		return M_FALSE;

	if (io->reg_event != NULL) {
		if (!io->private_event || io->sync_data == NULL) {
			return M_FALSE;
		}
		data = io->sync_data;
	} else {
		event             = M_event_create(M_EVENT_FLAG_NOWAKE);
		if (io->sync_data != NULL) {
			data          = io->sync_data;
		} else {
			data          = M_malloc_zero(sizeof(*data));
			io->sync_data = data;
		}
		M_event_add(event, io, M_io_block_event, data);
		io->private_event = M_TRUE; /* Mark the event as private to the M_io_t */
	}

	M_mem_set(data, 0, sizeof(*data));
	data->request = request;
	data->retval  = M_IO_ERROR_ERROR;
	*ret_data     = data;
	return M_TRUE;
}


M_io_error_t M_io_block_connect(M_io_t *io)
{
	M_io_block_data_t *data;
	M_io_state_t       state = M_io_get_state(io);

	if (state == M_IO_STATE_CONNECTED)
		return M_IO_ERROR_SUCCESS;

	if (state != M_IO_STATE_INIT)
		return M_IO_ERROR_ERROR;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_CONNECT, &data)) {
		return M_IO_ERROR_ERROR;
	}

	M_event_loop(io->reg_event, M_TIMEOUT_INF);
	return data->retval;
}


M_io_error_t M_io_block_accept(M_io_t **io_out, M_io_t *server_io, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t       err;

	if (io_out == NULL || server_io == NULL || server_io->type != M_IO_TYPE_LISTENER) {
		return M_IO_ERROR_INVALID;
	}

	/* Attempt to accept a new connection */
	if (server_io->reg_event) {
		err = M_io_accept(io_out, server_io);
		if (err != M_IO_ERROR_WOULDBLOCK)
			return err;
	}

	if (!M_io_block_regevent(server_io, M_IO_SYNC_REQUEST_ACCEPT, &data)) {
		return M_IO_ERROR_WOULDBLOCK;
	}
	M_event_loop(server_io->reg_event, timeout_ms);

	*io_out = data->newconn;

	return data->retval;
}


M_io_error_t M_io_block_read(M_io_t *io, unsigned char *buf, size_t buf_len, size_t *len_read, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t      err;

	if (io == NULL || buf == NULL || buf_len == 0 || len_read == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_DISCONNECTED:
			/* Allow reading after we think we're disconnected as there may still be OS data buffered */
		case M_IO_STATE_CONNECTED:
		case M_IO_STATE_DISCONNECTING:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	err = M_io_read(io, buf, buf_len, len_read);
	if (err != M_IO_ERROR_WOULDBLOCK) {
		/* Overwrite error if we allowed a read on a disconnected socket */
		if (err != M_IO_ERROR_SUCCESS && M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return err;
	}

	/* If we read nothing, but we're disconnected, we won't register to wait for events */
	if (M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
		return M_IO_ERROR_DISCONNECT;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_READUCHAR, &data))
		return M_IO_ERROR_ERROR;

	data->buf_uchar = buf;
	data->buf_len   = buf_len;

	if (M_event_loop(io->reg_event, timeout_ms) == M_EVENT_ERR_TIMEOUT)
		return M_IO_ERROR_WOULDBLOCK;

	if (data->retval == M_IO_ERROR_SUCCESS)
		*len_read = data->out_len;

	return data->retval;
}


M_io_error_t M_io_block_read_into_buf(M_io_t *io, M_buf_t *buf, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t      err;

	if (io == NULL || buf == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_DISCONNECTED:
			/* Allow reading after we think we're disconnected as there may still be OS data buffered */
		case M_IO_STATE_CONNECTED:
		case M_IO_STATE_DISCONNECTING:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	err = M_io_read_into_buf(io, buf);
	if (err != M_IO_ERROR_WOULDBLOCK) {
		/* Overwrite error if we allowed a read on a disconnected socket */
		if (err != M_IO_ERROR_SUCCESS && M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return err;
	}

	/* If we read nothing, but we're disconnected, we won't register to wait for events */
	if (M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
		return M_IO_ERROR_DISCONNECT;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_READBUF, &data))
		return M_IO_ERROR_ERROR;

	data->buf_buf = buf;

	if (M_event_loop(io->reg_event, timeout_ms) == M_EVENT_ERR_TIMEOUT)
		return M_IO_ERROR_WOULDBLOCK;

	return data->retval;
}


M_io_error_t M_io_block_read_into_parser(M_io_t *io, M_parser_t *parser, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t      err;

	if (io == NULL || parser == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_DISCONNECTED:
			/* Allow reading after we think we're disconnected as there may still be OS data buffered */
		case M_IO_STATE_CONNECTED:
		case M_IO_STATE_DISCONNECTING:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	err = M_io_read_into_parser(io, parser);
	if (err != M_IO_ERROR_WOULDBLOCK) {
		/* Overwrite error if we allowed a read on a disconnected socket */
		if (err != M_IO_ERROR_SUCCESS && M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return err;
	}

	/* If we read nothing, but we're disconnected, we won't register to wait for events */
	if (M_io_get_state(io) == M_IO_STATE_DISCONNECTED)
		return M_IO_ERROR_DISCONNECT;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_READPARSER, &data)) {
		return M_IO_ERROR_ERROR;
	}

	data->buf_parser = parser;

	if (M_event_loop(io->reg_event, timeout_ms) == M_EVENT_ERR_TIMEOUT) {
		return M_IO_ERROR_WOULDBLOCK;
	}
	return data->retval;
}


M_io_error_t M_io_block_write(M_io_t *io, const unsigned char *buf, size_t buf_len, size_t *len_written, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t      err;

	if (io == NULL || buf == NULL || buf_len == 0 || len_written == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_DISCONNECTED:
			return M_IO_ERROR_DISCONNECT;
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_CONNECTED:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	err = M_io_write(io, buf, buf_len, len_written);
	if (err != M_IO_ERROR_WOULDBLOCK)
		return err;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_WRITEUCHAR, &data))
		return M_IO_ERROR_ERROR;

	data->buf_cuchar = buf;
	data->buf_len    = buf_len;

	if (M_event_loop(io->reg_event, timeout_ms) == M_EVENT_ERR_TIMEOUT)
		return M_IO_ERROR_WOULDBLOCK;

	if (data->retval == M_IO_ERROR_SUCCESS)
		*len_written = data->out_len;

	return data->retval;
}


M_io_error_t M_io_block_write_from_buf(M_io_t *io, M_buf_t *buf, M_uint64 timeout_ms)
{
	M_io_block_data_t *data;
	M_io_error_t      err;

	if (io == NULL || buf == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_DISCONNECTED:
			return M_IO_ERROR_DISCONNECT;
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_CONNECTED:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	err = M_io_write_from_buf(io, buf);
	if (err != M_IO_ERROR_WOULDBLOCK)
		return err;

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_WRITEBUF, &data))
		return M_IO_ERROR_ERROR;

	data->buf_buf = buf;

	if (M_event_loop(io->reg_event, timeout_ms) == M_EVENT_ERR_TIMEOUT)
		return M_IO_ERROR_WOULDBLOCK;

	return data->retval;
}


M_io_error_t M_io_block_disconnect(M_io_t *io)
{
	M_io_block_data_t *data;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	switch (M_io_get_state(io)) {
		case M_IO_STATE_DISCONNECTED:
			return M_IO_ERROR_DISCONNECT;
		case M_IO_STATE_ERROR:
			return M_IO_ERROR_ERROR;
		case M_IO_STATE_CONNECTED:
			break;
		default:
			return M_IO_ERROR_WOULDBLOCK;
	}

	M_io_disconnect(io);

	if (!M_io_block_regevent(io, M_IO_SYNC_REQUEST_DISCONNECT, &data))
		return M_IO_ERROR_ERROR;

	M_event_loop(io->reg_event, M_TIMEOUT_INF);
	return data->retval;
}

