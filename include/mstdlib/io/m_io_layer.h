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

#ifndef __M_IO_LAYER_H__
#define __M_IO_LAYER_H__

#include <mstdlib/mstdlib_io.h>

__BEGIN_DECLS

/*! \addtogroup m_io_layer Functions for creating and using custom I/O layers
 *  \ingroup m_eventio_semipublic
 *
 * Included using the semi-public header of <mstdlib/io/m_io_layer.h>
 *
 * This is a semi-public header meant for those writing their own io layers. Unlike the
 * normal public-facing API, these may change at any time.
 *
 * @{
 */


#ifdef _WIN32
/* Needed for types like SOCKET */
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <mstcpip.h>
#  include <windows.h>
#  define M_EVENT_SOCKET         SOCKET
#  define M_EVENT_INVALID_SOCKET INVALID_SOCKET
#  define M_EVENT_HANDLE         HANDLE
#  define M_EVENT_INVALID_HANDLE NULL
#else
#  define M_EVENT_HANDLE         int
#  define M_EVENT_INVALID_HANDLE -1
#  define M_EVENT_SOCKET         int
#  define M_EVENT_INVALID_SOCKET -1
#endif


struct M_io_layer;
typedef struct M_io_layer M_io_layer_t;

struct M_io_handle;
typedef struct M_io_handle M_io_handle_t;

struct M_io_callbacks;
typedef struct M_io_callbacks M_io_callbacks_t;



/*! Find the appropriate layer and grab the handle and lock it.
 *
 * \warning Locking the layer locks the entire event loop. Only very
 *          short operations that will not block should be performed
 *          while a layer lock is being held.
 *
 *  \param[in] io        Pointer to io object
 *  \param[in] layer_id  id of layer to lock, or M_IO_LAYER_FIND_FIRST_ID to search for layer.
 *  \param[in] name      Name of layer to lock.  This can be used as a sanity check to ensure
 *                       the layer id really matches the layer type.  Use NULL if name matching
 *                       is not required.  If M_IO_LAYER_FIND_FIRST_ID is used for the layer_id,
 *                       this parameter cannot be NULL.
 *
 * \return locked io layer, or NULL on failure
 *
 * \see M_io_layer_release
 */
M_API M_io_layer_t *M_io_layer_acquire(M_io_t *io, size_t layer_id, const char *name);

/*! Release the lock on the layer */
M_API void M_io_layer_release(M_io_layer_t *layer);

/*! Initialize a new io object of given type */
M_API M_io_t *M_io_init(M_io_type_t type);

/*! Get the type of the io object */
M_API M_io_type_t M_io_get_type(M_io_t *io);

/*! Create M_io_callbacks_t object that can be passed to M_io_layer_add */
M_API M_io_callbacks_t *M_io_callbacks_create(void);

/*! Register callback to initialize/begin.  Is called when the io object is attached
 *  to an event. Mandatory. */
M_API M_bool M_io_callbacks_reg_init(M_io_callbacks_t *callbacks, M_bool (*cb_init)(M_io_layer_t *layer));

/*! Register callback to accept a new connection. Conditional. */
M_API M_bool M_io_callbacks_reg_accept(M_io_callbacks_t *callbacks, M_io_error_t (*cb_accept)(M_io_t *new_conn, M_io_layer_t *orig_layer));

/*! Register callback to read from the connection. Optional if not base layer, required if base layer */
M_API M_bool M_io_callbacks_reg_read(M_io_callbacks_t *callbacks, M_io_error_t (*cb_read)(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta));

/*! Register callback to write to the connection. Optional if not base layer, required if base layer */
M_API M_bool M_io_callbacks_reg_write(M_io_callbacks_t *callbacks, M_io_error_t (*cb_write)(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta));

/*! Register callback to process events.  Optional. */
M_API M_bool M_io_callbacks_reg_processevent(M_io_callbacks_t *callbacks, M_bool (*cb_process_event)(M_io_layer_t *layer, M_event_type_t *type));

/*! Register callback that is called when io object is removed from event object. Mandatory */
M_API M_bool M_io_callbacks_reg_unregister(M_io_callbacks_t *callbacks, void (*cb_unregister)(M_io_layer_t *layer));

/*! Register callback to start a graceful disconnect sequence.  Optional. */
M_API M_bool M_io_callbacks_reg_disconnect(M_io_callbacks_t *callbacks, M_bool (*cb_disconnect)(M_io_layer_t *layer));

/*! Register callback to destroy any state (M_io_handle_t *). Mandatory.
 *
 * The event loop has already been disassociated from the layer when this
 * callback is called. The layer will not be locked and M_io_layer_acquire
 * will not lock the layer as the layer cannot be locked.
 */
M_API M_bool M_io_callbacks_reg_destroy(M_io_callbacks_t *callbacks, void (*cb_destroy)(M_io_layer_t *layer));

/*! Register callback to get the layer state. Optional if not base layer, required if base layer. */
M_API M_bool M_io_callbacks_reg_state(M_io_callbacks_t *callbacks, M_io_state_t (*cb_state)(M_io_layer_t *layer));

/*! Register callback to get the error message, will be called if cb_state returns M_IO_STATE_ERROR.  If registered, cb_state must also be registered */
M_API M_bool M_io_callbacks_reg_errormsg(M_io_callbacks_t *callbacks, M_bool (*cb_errormsg)(M_io_layer_t *layer, char *error, size_t err_len));

/*! Destroy M_io_callbacks_t object */
M_API void M_io_callbacks_destroy(M_io_callbacks_t *callbacks);

/*! Maximum number of layers for an I/O object.  One reserved for the user layer */
#define M_IO_LAYERS_MAX 16

/*! Add a layer to an io object */
M_API M_io_layer_t *M_io_layer_add(M_io_t *io, const char *layer_name, M_io_handle_t *handle, const M_io_callbacks_t *callbacks);

/*! Given a layer object, retrieve the M_io_t reference */
M_API M_io_t *M_io_layer_get_io(M_io_layer_t *layer);

/*! Given a layer object, retrieve the name of the layer */
M_API const char *M_io_layer_get_name(M_io_layer_t *layer);

/*! Given a layer object, retrieve the implementation-specific handle */
M_API M_io_handle_t *M_io_layer_get_handle(M_io_layer_t *layer);

/*! Given a layer object, retrieve the index of the layer in the parent M_io_t object */
M_API size_t M_io_layer_get_index(M_io_layer_t *layer);

/*! Perform a read operation at the given layer index */
M_API M_io_error_t M_io_layer_read(M_io_t *io, size_t layer_id, unsigned char *buf, size_t *read_len, M_io_meta_t *meta);

/*! Perform a write operation at the given layer index */
M_API M_io_error_t M_io_layer_write(M_io_t *io, size_t layer_id, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta);

M_API M_bool M_io_error_is_critical(M_io_error_t err);

/*! Add a soft-event.  If sibling_only is true, will only notify next layer and not self. */
M_API void M_io_layer_softevent_add(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type);

/*! Clear all soft events for the current layer */
M_API void M_io_layer_softevent_clear(M_io_layer_t *layer);

/*! Add a soft-event.  If sibling_only is true, will only delete the soft event for the next layer up and not self. */
M_API void M_io_layer_softevent_del(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type);


/*! @} */

__END_DECLS


#endif
