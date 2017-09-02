/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
#ifndef _WIN32
#  include <unistd.h>
#  include <fcntl.h>
#endif
#include "base/m_defs_int.h"

static void M_io_layer_free_cb(void *arg)
{
	M_io_layer_t *layer = arg;
	M_free(layer->name);
	M_free(layer);
}

M_io_t *M_io_init(M_io_type_t type)
{
	struct M_list_callbacks layer_cbs = {
		NULL,  /* equality */
		NULL,  /* duplicate_insert */
		NULL,  /* duplicate_copy */
		M_io_layer_free_cb /* value_free */
	};
	M_io_t *comm     = M_malloc_zero(sizeof(*comm));
	comm->type       = type;
	comm->layer      = M_list_create(&layer_cbs, M_LIST_NONE);
	comm->last_error = M_IO_ERROR_SUCCESS;
	return comm;
}


void M_io_lock(M_io_t *io)
{
	if (io == NULL || io->reg_event == NULL) {
		return;
	}

	M_event_lock(io->reg_event);
//M_printf("%s(): [%p] io %p event %p\n", __FUNCTION__, (void *)M_thread_self(), io, io->reg_event);  fflush(stdout);
}


void M_io_unlock(M_io_t *io)
{
	if (io == NULL || io->reg_event == NULL) {
		return;
	}

//M_printf("%s(): [%p] io %p event %p\n", __FUNCTION__, (void *)M_thread_self(), io, io->reg_event);  fflush(stdout);
	M_event_unlock(io->reg_event);
}



M_io_layer_t *M_io_layer_add(M_io_t *comm, const char *layer_name, M_io_handle_t *handle, const M_io_callbacks_t *callbacks)
{
	M_io_layer_t *layer;

	if (comm == NULL || layer_name == NULL || handle == NULL || callbacks == NULL)
		return NULL;

	M_io_lock(comm);

	/* XXX: validate necessary callbacks are registered, this may depend on count of
	 *      existing layers */

	if (comm->type == M_IO_TYPE_EVENT && M_list_len(comm->layer) == 1) {
		M_io_unlock(comm);
		return NULL;
	}

	if (M_list_len(comm->layer) == M_IO_LAYERS_MAX-1 /* 1 reserved for user */ ) {
		M_io_unlock(comm);
		return NULL;
	}

	layer                = M_malloc_zero(sizeof(*layer));
	layer->comm          = comm;
	layer->idx           = M_list_len(comm->layer);
	layer->name          = M_strdup(layer_name);
	layer->handle        = handle;

	M_mem_copy(&layer->cb, callbacks, sizeof(layer->cb));
	M_list_insert(comm->layer, layer);

	M_io_unlock(comm);
	return layer;
}


M_io_layer_t *M_io_layer_at(M_io_t *io, size_t layer_id)
{
	const void   *ptr;

	if (io == NULL)
		return NULL;

	ptr = M_list_at(io->layer, layer_id);
	if (ptr == NULL)
		return NULL;

	return M_CAST_OFF_CONST(M_io_layer_t *, ptr);
}


M_io_layer_t *M_io_layer_acquire(M_io_t *io, size_t layer_id, const char *name)
{
	M_io_layer_t *layer;
	size_t        i;

	if (io == NULL)
		return NULL;

	if (layer_id == M_IO_LAYER_FIND_FIRST_ID && M_str_isempty(name))
		return NULL;

	M_io_lock(io);
//M_printf("%s(): [%p] %p\n", __FUNCTION__, (void *)M_thread_self(), io);

	/* Search by name and reset layer id based on the name */
	if (layer_id == M_IO_LAYER_FIND_FIRST_ID) {
		for (i=0; i<M_io_layer_count(io); i++) {
			layer = M_io_layer_at(io, i);
			if (M_str_eq(layer->name, name)) {
				layer_id = i;
				break;
			}
		}
	}

	layer = M_io_layer_at(io, layer_id);
	if (layer == NULL || (name != NULL && !M_str_eq(layer->name, name))) {
		M_io_unlock(io);
		return NULL;
	}

	return layer;
}


void M_io_layer_release(M_io_layer_t *layer)
{
	if (layer == NULL)
		return;
//M_printf("%s(): [%p] %p\n", __FUNCTION__, (void *)M_thread_self(), layer->comm);
	M_io_unlock(layer->comm);
}


static void M_io_destroy_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	(void)event;
	(void)type;
	(void)io;
	M_io_destroy(cb_arg);
}


void M_io_destroy(M_io_t *comm)
{
	ssize_t    i;
	size_t     num;
	M_event_t *event         = NULL;
	M_bool     destroy_event = M_FALSE;

	if (comm == NULL)
		return;

//M_printf("%s(): [%p] io %p event %p enter\n", __FUNCTION__, (void *)M_thread_self(), comm, comm->reg_event);


	if (comm->reg_event) {
		M_io_lock(comm);

		event = comm->reg_event;

		/* If we are executing this from a separate thread than is running the event loop, we need to
		 * delay the cleanup process and actually have it run within the assigned event loop instead */
		if (!comm->private_event && event->u.loop.threadid != 0 && event->u.loop.threadid != M_thread_self()) {

#if 1
			M_event_remove(comm);
			/* Queue a destroy task to run for this */
			M_event_queue_task(event, M_io_destroy_cb, comm);

			/* IO objects dont have their own locks, they rely on the event subsystem lock,
			 * but M_event_remove() removes the ->reg_event pointer, therefore we cannot use
			 * M_io_unlock(). */
			M_event_unlock(event);
#else
			/* Clear any pending events so they don't get delivered */
			M_io_softevent_clearall(comm);
			M_event_queue_pending_clear(event, comm);

			/* Queue a destroy task to run for this */
			M_event_queue_task(event, M_io_destroy_cb, comm);

			M_io_unlock(comm);
#endif
			return;
		}

		M_event_remove(comm);

		/* Unlock the lock since we are no longer bound to an event object */
		comm->reg_event = event;
		M_io_unlock(comm);
		comm->reg_event = NULL;

		if (comm->private_event) {
			destroy_event = M_TRUE;
		}
	}

	M_io_block_data_free(comm);

	num = M_list_len(comm->layer);
	for (i=(ssize_t)num - 1; i >= 0; i--) {
		M_io_layer_t *layer = M_io_layer_at(comm, (size_t)i);
		if (layer->cb.cb_destroy != NULL)
			layer->cb.cb_destroy(layer);
		layer->handle = NULL;
	}
	M_list_destroy(comm->layer, M_TRUE);
	comm->layer = NULL;

//M_printf("%s(): [%p] io %p event %p exit\n", __FUNCTION__, (void *)M_thread_self(), comm, event);

	M_free(comm);

	/* Delay cleaning up event until last possible time */
	if (event && destroy_event)
		M_event_destroy(event);
}


void M_io_disconnect(M_io_t *comm)
{
	ssize_t i;
	size_t  num;
	M_bool  cont = M_TRUE;

	if (comm == NULL)
		return;

	M_io_lock(comm);

	num = M_list_len(comm->layer);
	for (i=(ssize_t)num - 1; i >= 0 && cont; i--) {
		M_io_layer_t *layer  = M_io_layer_at(comm, (size_t)i);
		if (layer->cb.cb_disconnect != NULL)
			cont = layer->cb.cb_disconnect(layer);
	}

	/* Layer reported disconnect, trigger soft event */
	if (cont && comm->reg_event) {
		M_io_user_softevent_add(comm, M_EVENT_TYPE_DISCONNECTED);
	}
	M_io_unlock(comm);
}


M_io_error_t M_io_layer_read(M_io_t *io, size_t layer_id, unsigned char *buf, size_t *read_len)
{
	ssize_t       i;
	M_io_error_t  err   = M_IO_ERROR_ERROR;
	M_io_layer_t *layer = NULL;

	if (io == NULL || buf == NULL || read_len == NULL) {
		err = M_IO_ERROR_INVALID;
		goto fail;
	}

	if (layer_id >= M_list_len(io->layer)) {
		err = M_IO_ERROR_INVALID;
		goto fail;
	}

	for (i=(ssize_t)layer_id; i >= 0; i--) {
		layer = M_io_layer_at(io, (size_t)i);

		if (layer->cb.cb_read == NULL)
			continue;

		err = layer->cb.cb_read(layer, buf, read_len);
		break;
	}

fail:
	/* Clear all existing soft events, the connection is destroyed, enqueue a disconnect or error softevent */
	if (M_io_error_is_critical(err)) {
		M_io_softevent_clearall(io);
		M_io_layer_softevent_add(layer, M_FALSE, (err == M_IO_ERROR_DISCONNECT)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR);
	}

	return err;
}


M_io_error_t M_io_read(M_io_t *io, unsigned char *buf, size_t buf_len, size_t *len_read)
{
	M_io_error_t err;

	if (io == NULL || buf == NULL || buf_len == 0 || len_read == NULL) {
		err = M_IO_ERROR_INVALID;
		goto fail;
	}

	*len_read = buf_len;
	err       = M_io_layer_read(io, M_list_len(io->layer)-1, buf, len_read);
	if (err != M_IO_ERROR_SUCCESS)
		*len_read = 0;

	/* Users are told events are delivered as level-triggered-resettable, so we need to trigger
	 * soft events since the event subsystem is edge-triggered */
	if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && buf_len > *len_read)) {
		/* Delete any read soft events, as we know we'd block */
		M_io_user_softevent_del(io, M_EVENT_TYPE_READ);
	} else if (err == M_IO_ERROR_SUCCESS) {
		/* There's more likely more data to read, trigger a soft event */
		M_io_user_softevent_add(io, M_EVENT_TYPE_READ);
	}

fail:
	io->last_error = err;

	return err;
}


M_io_error_t M_io_read_into_buf(M_io_t *comm, M_buf_t *buf)
{
	size_t         buf_len;
	size_t         len_read;
	unsigned char *wbuf;
	M_bool         bytes_read = M_FALSE;
	M_io_error_t err;

	if (comm == NULL || buf == NULL) {
		return M_IO_ERROR_INVALID;
	}

	while (1) {
		buf_len  = 1024; /* Requested size */
		len_read = 0;
		wbuf     = M_buf_direct_write_start(buf, &buf_len); /* Actual size is probably much larger */
		err      = M_io_read(comm, wbuf, buf_len, &len_read);
		M_buf_direct_write_end(buf, len_read);

		/* Cancel loop on error */
		if (err != M_IO_ERROR_SUCCESS)
			break;

		bytes_read = M_TRUE;

		/* If we didn't fill our buffer, break out as next read will return wouldblock */
		if (len_read != buf_len)
			break;
	}

	/* Throw away any error condition if bytes were read and always return success */
	if (bytes_read)
		return M_IO_ERROR_SUCCESS;

	return err;
}


M_io_error_t M_io_read_into_parser(M_io_t *comm, M_parser_t *parser)
{
	size_t         buf_len;
	size_t         len_read;
	unsigned char *wbuf;
	M_bool         bytes_read = M_FALSE;
	M_io_error_t err;

	if (comm == NULL || parser == NULL) {
		return M_IO_ERROR_INVALID;
	}

	while (1) {
		buf_len  = 1024; /* Requested size */
		len_read = 0;
		wbuf     = M_parser_direct_write_start(parser, &buf_len); /* Actual size is probably much larger */
		/* Must have passed in a const parser */
		if (wbuf == NULL)
			return M_IO_ERROR_INVALID;
		err      = M_io_read(comm, wbuf, buf_len, &len_read);
		M_parser_direct_write_end(parser, len_read);

		/* Cancel loop on error */
		if (err != M_IO_ERROR_SUCCESS)
			break;

		bytes_read = M_TRUE;

		/* If we didn't fill our buffer, break out as next read will return wouldblock */
		if (len_read != buf_len)
			break;
	}

	/* Throw away any error condition if bytes were read and always return success */
	if (bytes_read)
		return M_IO_ERROR_SUCCESS;

	return err;
}



M_io_error_t M_io_layer_write(M_io_t *io, size_t layer_id, const unsigned char *buf, size_t *write_len)
{
	ssize_t       i;
	M_io_error_t  err   = M_IO_ERROR_ERROR;
	M_io_layer_t *layer = NULL;

	if (io == NULL || buf == NULL || write_len == NULL)
		return M_IO_ERROR_INVALID;

	if (layer_id >= M_list_len(io->layer))
		return M_IO_ERROR_INVALID;

	for (i=(ssize_t)layer_id; i >= 0; i--) {
		layer = M_io_layer_at(io, (size_t)i);

		if (layer->cb.cb_write == NULL)
			continue;

		err = layer->cb.cb_write(layer, buf, write_len);
		break;
	}

	/* Clear all existing soft events, the connection is destroyed, enqueue a disconnect or error softevent */
	if (M_io_error_is_critical(err)) {
		M_io_softevent_clearall(io);
		M_io_layer_softevent_add(layer, M_FALSE, (err == M_IO_ERROR_DISCONNECT)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR);
	}
	return err;
}


M_io_error_t M_io_write(M_io_t *comm, const unsigned char *buf, size_t buf_len, size_t *len_written)
{
	M_io_error_t err;

	if (comm == NULL || buf == NULL || buf_len == 0 || len_written == NULL) {
		err = M_IO_ERROR_INVALID;
		goto fail;
	}

	*len_written = buf_len;
	err = M_io_layer_write(comm, M_list_len(comm->layer)-1, buf, len_written);
	if (err != M_IO_ERROR_SUCCESS)
		*len_written = 0;

	/* Users are told events are delivered as level-triggered-resettable, so we need to trigger
	 * soft events since the event subsystem is edge-triggered */
	if (err == M_IO_ERROR_WOULDBLOCK || (err == M_IO_ERROR_SUCCESS && buf_len > *len_written)) {
		/* Delete any write soft events, as we know we'd block */
		M_io_user_softevent_del(comm, M_EVENT_TYPE_WRITE);
	} else if (err == M_IO_ERROR_SUCCESS) {
		/* There's more likely more room to write, trigger a soft event */
		M_io_user_softevent_add(comm, M_EVENT_TYPE_WRITE);
	}
fail:
	comm->last_error = err;

	return err;
}


M_io_error_t M_io_write_from_buf(M_io_t *comm, M_buf_t *buf)
{
	size_t         len_written;
	M_io_error_t err;

	if (comm == NULL || buf == NULL)
		return M_IO_ERROR_INVALID;

	if (M_buf_len(buf) == 0)
		return M_IO_ERROR_SUCCESS;

	err = M_io_write(comm, (const unsigned char *)M_buf_peek(buf), M_buf_len(buf), &len_written);
	if (err == M_IO_ERROR_SUCCESS) {
		M_buf_drop(buf, len_written);
	}
	return err;
}


M_io_error_t M_io_accept(M_io_t **io_out, M_io_t *server_io)
{
	size_t       i;
	size_t       num;
	M_io_error_t err;

	if (io_out == NULL || server_io == NULL) {
		return M_IO_ERROR_INVALID;
	}

	*io_out = M_io_init(M_IO_TYPE_STREAM);

	num = M_list_len(server_io->layer);
	for (i=0; i<num; i++) {
		M_io_layer_t *orig_layer = M_io_layer_at(server_io, i);
		err = orig_layer->cb.cb_accept(*io_out, orig_layer);
		if (err != M_IO_ERROR_SUCCESS) {
			M_io_destroy(*io_out);
			*io_out = NULL;
			goto fail;
		}
	}

	/* On success, we may have more connections to accept, trigger a soft event */
	M_io_user_softevent_add(server_io, M_EVENT_TYPE_ACCEPT);
	err = M_IO_ERROR_SUCCESS;

fail:
	server_io->last_error = err;

	if (err == M_IO_ERROR_WOULDBLOCK) {
		M_io_user_softevent_del(server_io, M_EVENT_TYPE_ACCEPT);
	}

	return err;
}

M_io_type_t M_io_get_type(M_io_t *io)
{
	if (io == NULL)
		return 0;
	return io->type;
}

M_event_t *M_io_get_event(M_io_t *io)
{
	if (io == NULL)
		return 0;
	return io->reg_event;
}

M_io_t *M_io_layer_get_io(M_io_layer_t *layer)
{
	if (layer == NULL)
		return NULL;
	return layer->comm;
}

const char *M_io_layer_get_name(M_io_layer_t *layer)
{
	if (layer == NULL)
		return NULL;
	return layer->name;
}

M_io_handle_t *M_io_layer_get_handle(M_io_layer_t *layer)
{
	if (layer == NULL)
		return NULL;
	return layer->handle;
}

size_t M_io_layer_get_index(M_io_layer_t *layer)
{
	if (layer == NULL)
		return 0;
	return layer->idx;
}

size_t M_io_layer_count(M_io_t *io)
{
	if (io == NULL)
		return 0;
	return M_list_len(io->layer);
}


M_io_callbacks_t *M_io_callbacks_create(void)
{
	return M_malloc_zero(sizeof(M_io_callbacks_t));
}


M_bool M_io_callbacks_reg_init(M_io_callbacks_t *callbacks, M_bool (*cb_init)(M_io_layer_t *layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_init = cb_init;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_accept(M_io_callbacks_t *callbacks, M_io_error_t (*cb_accept)(M_io_t *new_conn, M_io_layer_t *orig_layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_accept = cb_accept;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_read(M_io_callbacks_t *callbacks, M_io_error_t (*cb_read)(M_io_layer_t *layer, unsigned char *buf, size_t *read_len))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_read = cb_read;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_write(M_io_callbacks_t *callbacks, M_io_error_t (*cb_write)(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_write = cb_write;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_processevent(M_io_callbacks_t *callbacks, M_bool (*cb_process_event)(M_io_layer_t *layer, M_event_type_t *type))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_process_event = cb_process_event;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_unregister(M_io_callbacks_t *callbacks, void (*cb_unregister)(M_io_layer_t *layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_unregister = cb_unregister;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_disconnect(M_io_callbacks_t *callbacks, M_bool (*cb_disconnect)(M_io_layer_t *layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_disconnect = cb_disconnect;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_destroy(M_io_callbacks_t *callbacks, void (*cb_destroy)(M_io_layer_t *layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_destroy = cb_destroy;
	return M_TRUE;
}

M_bool M_io_callbacks_reg_state(M_io_callbacks_t *callbacks, M_io_state_t (*cb_state)(M_io_layer_t *layer))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_state = cb_state;
	return M_TRUE;
}


M_bool M_io_callbacks_reg_errormsg(M_io_callbacks_t *callbacks, M_bool (*cb_errormsg)(M_io_layer_t *layer, char *error, size_t err_len))
{
	if (callbacks == NULL)
		return M_FALSE;
	callbacks->cb_errormsg = cb_errormsg;
	return M_TRUE;
}


void M_io_callbacks_destroy(M_io_callbacks_t *callbacks)
{
	M_free(callbacks);
}


M_io_state_t M_io_get_state(M_io_t *io)
{
	M_io_state_t state = M_IO_STATE_INIT;
	size_t       num;
	size_t       i;

	if (io == NULL)
		return M_IO_STATE_ERROR;

	M_io_lock(io);

	num = M_io_layer_count(io);
	for (i=0; i<num; i++) {
		M_io_layer_t *layer = M_io_layer_at(io, i);

		if (layer->cb.cb_state == NULL)
			continue;

		/* As soon as we hit a state that isn't connected, we want to return
		 * that state.  We could have   CONNECTED;CONNECTED;DISCONNECTING;CONNECTED,
		 * so the most important state is 'DISCONNECTING'.  Or we could have a state
		 * like  INIT;CONNECTED;CONNECTED  as some layers may always state they
		 * are connected because they are stateless so INIT makes sense.
		 */
		state = layer->cb.cb_state(layer);
		if (state != M_IO_STATE_CONNECTED) {
			break;
		}
	}

	M_io_unlock(io);
	return state;
}


M_io_state_t M_io_get_layer_state(M_io_t *io, size_t id)
{
	M_io_state_t state = M_IO_STATE_INIT;
	size_t       num;

	if (io == NULL)
		return M_IO_STATE_ERROR;

	M_io_lock(io);

	num = M_io_layer_count(io);
	if (id >= num) {
		state = M_IO_STATE_ERROR;
	} else {
		M_io_layer_t *layer = M_io_layer_at(io, id);
		if (layer->cb.cb_state == NULL) {
			state = M_IO_STATE_CONNECTED;
		} else {
			state = layer->cb.cb_state(layer);
		}
	}

	M_io_unlock(io);
	return state;
}


const char *M_io_error_string(M_io_error_t error)
{
	switch (error) {
		case M_IO_ERROR_SUCCESS:
			return "Success. No Error";
		case M_IO_ERROR_WOULDBLOCK:
			return "Would Block";
		case M_IO_ERROR_DISCONNECT:
			return "Remote Disconnect";
		case M_IO_ERROR_ERROR:
			return "Error";
		case M_IO_ERROR_NOTCONNECTED:
			return "Not Connected to Remote";
		case M_IO_ERROR_NOTPERM:
			return "Not Permitted";
		case M_IO_ERROR_CONNRESET:
			return "Connection Reset By Peer";
		case M_IO_ERROR_CONNABORTED:
			return "Connection Aborted";
		case M_IO_ERROR_ADDRINUSE:
			return "Address already in use";
		case M_IO_ERROR_PROTONOTSUPPORTED:
			return "Protocol Not Supported";
		case M_IO_ERROR_CONNREFUSED:
			return "Connection Refused";
		case M_IO_ERROR_NETUNREACHABLE:
			return "Network or Host Unreachable";
		case M_IO_ERROR_TIMEDOUT:
			return "Operation Timed Out";
		case M_IO_ERROR_NOSYSRESOURCES:
			return "Out of system resources";
		case M_IO_ERROR_INVALID:
			return "Invalid Use";
		case M_IO_ERROR_NOTIMPL:
			return "OS Does Not Implement";
		case M_IO_ERROR_INTERRUPTED:
			return "Command Interrupted";
		case M_IO_ERROR_NOTFOUND:
			return "File or Resource Not Found";
	}
	return "UNKNOWN";
}


void M_io_get_error_string(M_io_t *io, char *error, size_t err_len)
{
	size_t       num;
	size_t       i;
	M_bool       done = M_FALSE;

	if (io == NULL || error == NULL || err_len == 0)
		return;

	M_mem_set(error, 0, err_len);

	M_io_lock(io);

	num = M_io_layer_count(io);
	for (i=0; i<num; i++) {
		M_io_layer_t *layer = M_io_layer_at(io, i);

		if (layer->cb.cb_state == NULL || layer->cb.cb_errormsg == NULL)
			continue;

		if (layer->cb.cb_state(layer) != M_IO_STATE_ERROR)
			continue;

		done = layer->cb.cb_errormsg(layer, error, err_len);
		break;
	}

	if (!done) {
		M_io_state_t state = M_io_get_state(io);
		if (state == M_IO_STATE_DISCONNECTED) {
			/* We want to avoid a "Success. No Error" response when someone queries
			 * for the reason for a disconnect.  Even though that is technically
			 * valid, there was no error, it can be confusing. */
			M_snprintf(error, err_len, "%s", "Remote Closed Connection");
		} else {
			M_snprintf(error, err_len, "%s", M_io_error_string(io->last_error));
		}
	}

	M_io_unlock(io);
}


M_bool M_io_error_is_critical(M_io_error_t err)
{
	switch (err) {
		case M_IO_ERROR_SUCCESS:
		case M_IO_ERROR_WOULDBLOCK:
		case M_IO_ERROR_INTERRUPTED:
		/* No need to generate a signal for these 2, its either API misuse or the connection
		 * was already notified of a disconnect */
		case M_IO_ERROR_NOTCONNECTED: 
		case M_IO_ERROR_INVALID: 
			return M_FALSE;
		default:
			break;
	}
	return M_TRUE;
}
