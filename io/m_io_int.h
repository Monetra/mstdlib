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

#ifndef __M_IO_INT_H__
#define __M_IO_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"

#if defined(__APPLE__) && !defined(IOS)
#  include <CoreFoundation/CoreFoundation.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

struct M_io_callbacks {
	/*! Attach to event subsystem.  If the base layer, start the connection sequence if not already connected */
	M_bool         (*cb_init)(M_io_layer_t *layer);

	/*! Accept a connection from a remote client */
	M_io_error_t   (*cb_accept)(M_io_t *new_comm, M_io_layer_t *orig_layer);

	/*! Attempt to read from the layer */
	M_io_error_t   (*cb_read)(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);

	/*! Attempt to write to the layer */
	M_io_error_t   (*cb_write)(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);

	/*! Process an event delivered to the layer */
	M_bool         (*cb_process_event)(M_io_layer_t *layer, M_event_type_t *type);

	/*! Unregister from event subsystem */
	void           (*cb_unregister)(M_io_layer_t *layer);

	/*! Initiate a graceful disconnect. Return M_TRUE to continue to the next layer (e.g. immediately disconnected), M_FALSE if pending */
	M_bool         (*cb_disconnect)(M_io_layer_t *layer);

	/*! Destroy the layer */
	void           (*cb_destroy)(M_io_layer_t *layer);

	/*! Determine the current state of the layer (INIT, LISTENING, CONNECTING, CONNECTED, DISCONNECTING, DISCONNECTED, ERROR) */
	M_io_state_t   (*cb_state)(M_io_layer_t *layer);
	
	/*! Generate a layer-specific error message.  If this is registered, cb_state must also be registered.  This will only
	 *  be called if cb_state() returns M_IO_STATE_ERROR.  Returns false if cannot generate an error string. */
	M_bool         (*cb_errormsg)(M_io_layer_t *layer, char *error, size_t err_len);

	/* 
	 * Append brief technical details about the layer (e.g. IP address/port IPv4 vs IPv6, SSL Ciphers/Protocol, BW Limitations):
	 * void         (*cb_describe)(M_io_layer_t *layer, M_buf_t *buf);
	 * 
	 * Get an alternate connection address (e.g. for use by proxy implementations):
	 * M_bool       (*cb_altaddress)(M_io_layer_t *layer, const char *address_in, M_uint16 port_in, char **address_out, M_uint16 *port_out);
	 */
};

struct M_io_layer {
	M_io_t             *comm;       /*!< Reference to parent */
	size_t              idx;        /*!< Index of self in layers */
	char               *name;       /*!< Name of layer */
	M_io_handle_t      *handle;     /*!< Private handle (metadata, etc) of layer */
	M_io_callbacks_t    cb;         /*!< Callbacks */
};

struct M_io_block_data;
typedef struct M_io_block_data M_io_block_data_t;

struct M_io {
	M_io_type_t         type;            /*!< Type of comm object (stream, listener, event)               */
	M_io_error_t        last_error;      /*!< Last error returned by a command (accept, read, write, etc) */
	M_list_t           *layer;           /*!< List of M_io_layer_t's associated with the connection.
	                                          The first entry is the base connection tied to the OS,
	                                          every other entry is a wrapper layer (e.g. proxy, SSL, etc) */
	M_event_t          *reg_event;       /*!< Registered event handler for this connection                */

	M_bool              private_event;   /*!< Registered event handler is a private event handler         */
	M_io_block_data_t  *sync_data;       /*!< Data handle for tracking M_io_block_*() calls               */
};

void M_io_lock(M_io_t *io);
void M_io_unlock(M_io_t *io);

M_io_layer_t *M_io_layer_at(M_io_t *io, size_t layer_id);

#ifdef _WIN32
M_bool M_io_setnonblock(SOCKET fd);
#else
M_bool M_io_setnonblock(int fd);
#endif

void M_io_block_data_free(M_io_t *io);

/* Here because DNS needs it instead of m_io_net_int.h */
void M_io_net_init_system(void);

#if defined(__APPLE__) && !defined(IOS)
void M_io_mac_runloop_start(void);
extern CFRunLoopRef M_io_mac_runloop;
#endif


__END_DECLS

#endif /* __M_IO_INT_H__ */
