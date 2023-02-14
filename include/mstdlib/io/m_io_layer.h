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

#ifndef __M_IO_LAYER_H__
#define __M_IO_LAYER_H__

#include <mstdlib/mstdlib_io.h>

__BEGIN_DECLS

/*! \addtogroup m_io_layer Functions for creating and using custom I/O layers
 *  \ingroup m_eventio_semipublic
 *
 * Included using the semi-public header of <mstdlib/io/m_io_layer.h>
 *
 * This is a semi-public header meant for those writing their own io layers.
 * This could be a low level layer that's not current supported for comms. More
 * commonly it would be intermediate layers to accommodate specific data handling.
 *
 * # Layer Design
 *
 * Layers are stacked with the application being on top and the comms layer on
 * the bottom (typically comms later is the bottom layer). Layers in between are
 * typically data processing layers. For example, Application, TLS, and network.
 * Where the TLS layer is an intermediate data processing layer.
 *
 * Intermediate layers are bidirectional with data flowing down and up.
 *
 * ## Processing Events Callback
 *
 * The `processevent_cb` set by `M_io_callbacks_reg_processevent()` flows upward. From
 * the bottom comms layer through the intermediate layer and then to the application layer.
 * This is where data manipulation coming in can be handled. The callback can either allow
 * the even that trigged it to continue up the layer stack or it can suppress the event so
 * no further processing takes place.
 *
 * For example, if the intermediate layer doesn't need to do any processing of the data or has completed
 * all processing it will allow the event to propagate up. If the layer needs more data before it
 * can be used by the next layer it will suppress the event so processing the event stops.
 *
 * A read event from `processevent_cb` needs to read the data from the layer under in order to
 * get the data flowing up to process. A write event needs to write any pending data to the layer under
 * in order for it to be sent out. Read flows up and write flows down.
 *
 * Events always come from the bottom up. Either the lower layer(s) are stating there is data to read,
 * or it is stating data can be written. If there is no processing of read data or no data to write
 * the events would be allowed to propagate upwards so other layers (or the application) can handle the event.
 *
 * For processing read events, from the `processevent_cb` it is necessary to use
 * `M_io_layer_read()` like so `M_io_layer_read(io, M_io_layer_get_index(layer) - 1, buf, &buf_len, meta);`.
 * Since data is flowing up, the layer under a given layer has the pending read data that needs to be
 * processed.
 *
 * For processing write events, from the `processevent_cb` it is necessary to use
 * `M_io_layer_write()` like so `M_io_layer_write(io, M_io_layer_get_index(layer) - 1, NULL, NULL, meta);`.
 * Since data is flowing down, the layer under a given layer has needs to write the pending data.
 *
 * An application would use `M_io_read()` and `M_io_write()`. These always flow from the top layer down.
 * Since this layer is in the middle we need to always work with the layer beneath.
 *
 * ## Read / Write Callbacks
 *
 * The `read_cb` and `write_cb` set by `M_io_callbacks_reg_read()` and `M_io_callbacks_reg_write()`
 * flow down.
 *
 * A layer above will call `M_io_layer_read` or if the top most layer the application would
 * have called `M_io_read`. These call the layers `read_cb` If there is no read callback registered the
 * layer is skipped and the next layer in the sequence if called. This happens internally.
 * The `read_cb` will return any buffered data that has been read and passes it upward.
 * The data is typically buffered via the read event form `processevent_cb`.
 *
 * A layer above will call `M_io_layer_write` or if the top most layer the application would
 * have called `M_io_write`. These call the layers `write_cb` If there is no write callback registered the
 * layer is skipped and the next layer in the sequence if called. This happens internally.
 * The `write_cb` will receive and data that needs to be passed down for writing. Typically, the
 * `write_cb` will attempt to write the data immediately (after handling any processing) but may
 * need to buffer the data and write more later when the `processevent_cb` receives a write event
 * stating layers below can accept data to write.
 *
 * ## Examples
 *
 * Example layers:
 * - Basic layer that marshals data. Useful for starting a new layer.
 * - Processing STX+ETX+LRC with ACK/NAK and resending message
 * - BLE helper layer that handles secure pairing (if necessary) and setting read/write characteristic endpoints.
 *
 * ### Basic
 *
 * \code{.c}
 * #include <mstdlib/io/m_io_layer.h>
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * struct M_io_handle {
 *     M_buf_t     *read_buf;
 *     M_buf_t     *write_buf;
 *     char         err[256];
 * };
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * // Writes as much data as possible from the given buffer to the layer below this one.
 * static M_io_error_t write_to_next_layer(M_io_layer_t *layer, M_buf_t *buf)
 * {
 *     M_io_t       *io;
 *     M_io_error_t  err;
 *     size_t        layer_idx;
 *     size_t        write_len;
 *
 *     if (layer == NULL || M_buf_len(buf) == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     io        = M_io_layer_get_io(layer);
 *     layer_idx = M_io_layer_get_index(layer);
 *
 *     if (io == NULL || layer_idx == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     err = M_IO_ERROR_SUCCESS;
 *     do {
 *         write_len = M_buf_len(buf);
 *         err       = M_io_layer_write(io, layer_idx - 1, (const M_uint8 *)M_buf_peek(buf), &write_len, NULL);
 *         if (err == M_IO_ERROR_SUCCESS) {
 *             M_buf_drop(buf, write_len);
 *         }
 *     } while (err == M_IO_ERROR_SUCCESS && M_buf_len(buf) > 0);
 *
 *     return err;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_bool processevent_cb(M_io_layer_t *layer, M_event_type_t *type)
 * {
 *     M_io_handle_t *handle  = M_io_layer_get_handle(layer);
 *     M_io_t        *io      = M_io_layer_get_io(layer);
 *     M_bool         consume = M_FALSE; // Default to passing onto the next layer.
 *     M_io_error_t   err;
 *
 *     if (handle == NULL || io == NULL)
 *         return M_FALSE;
 *
 *     switch (*type) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             if (M_buf_len(handle->write_buf) != 0) {
 *                 err = write_to_next_layer(layer, handle->write_buf);
 *                 if (M_io_error_is_critical(err)) {
 *                     M_snprintf(handle->err, sizeof(handle->err), "Error writing data: %s", M_io_error_string(err));
 *                     *type = M_EVENT_TYPE_ERROR;
 *                 } else if (M_buf_len(handle->write_buf) != 0) {
 *                     // Don't inform higher levels we can write if we have more data pending.
 *                     consume = M_TRUE;
 *                 }
 *             }
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             // We're getting data from the device. Let's pull out the data and check if we want it or not.
 *             do {
 *                 M_uint8 buf[256] = { 0 };
 *                 size_t  buf_len  = sizeof(buf);
 *
 *                 err = M_io_layer_read(io, M_io_layer_get_index(layer) - 1, buf, &buf_len, NULL);
 *                 if (err == M_IO_ERROR_SUCCESS) {
 *                     // Save the data into the handler's buffer so we
 *                     // can pass it up to the layer above in the read callback.
 *                     M_buf_add_bytes(handle->read_buf, buf, buf_len);
 *                 } else if (M_io_error_is_critical(err)) {
 *                     M_snprintf(handle->err, sizeof(handle->err), "Error reading data: %s", M_io_error_string(err));
 *                     *type = M_EVENT_TYPE_ERROR;
 *                     break;
 *                 }
 *             } while (err == M_IO_ERROR_SUCCESS);
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(io, handle->err, sizeof(handle->err));
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 *
 *     // M_TRUE to discard this event, or M_FALSE to pass it on to the next layer.
 *     return consume;
 * }
 *
 * static M_io_error_t read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *buf_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     (void)meta;
 *
 *     if (handle == NULL || buf == NULL || buf_len == NULL || *buf_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     // Check if we have any more data to pass on or not.
 *     if (M_buf_len(handle->read_buf) == 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     // Pass on as much data as possible.
 *     *buf_len = M_MIN(*buf_len, M_buf_len(handle->read_buf));
 *     M_mem_copy(buf, M_buf_peek(handle->read_buf), *buf_len);
 *     M_buf_drop(handle->read_buf, *buf_len);
 *
 *     return M_IO_ERROR_SUCCESS;
 * }
 *
 * static M_io_error_t write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *buf_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t *handle;
 *     M_io_error_t   err;
 *
 *     (void)meta;
 *
 *     handle = M_io_layer_get_handle(layer);
 *
 *     if (handle == NULL || buf == NULL || buf_len == NULL || *buf_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     // Don't allow buffering data if we have data waiting to write.
 *     if (M_buf_len(handle->write_buf) > 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     M_buf_add_bytes(handle->write_buf, buf, *buf_len);
 *
 *     // Try to write as much of the message as we can right now.
 *     err = write_to_next_layer(layer, handle->write_buf);
 *
 *     // Treat would block as success because we've buffered the data and it will
 *      // be written when possible.
 *     if (err == M_IO_ERROR_WOULDBLOCK)
 *         err = M_IO_ERROR_SUCCESS;
 *     return err;
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static void unregister_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static M_bool reset_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 *     return M_TRUE;
 * }
 *
 * static void destroy_cb(M_io_layer_t *layer)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);;
 *
 *     M_buf_cancel(handle->read_buf);
 *     M_buf_cancel(handle->write_buf);
 *
 *     M_free(handle);
 * }
 *
 * static M_bool error_cb(M_io_layer_t *layer, char *error, size_t err_len)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     if (M_str_isempty(handle->err))
 *         return M_FALSE;
 *
 *     M_str_cpy(error, err_len, handle->err);
 *
 *     return M_TRUE;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // --- MAIN ENTRY ---
 *
 * void basic_example_layer(M_io_t *io)
 * {
 *     M_io_handle_t    *handle;
 *     M_io_callbacks_t *callbacks;
 *
 *     if (io == NULL)
 *         return;
 *
 *     handle             = M_malloc_zero(sizeof(*handle));
 *     handle->read_buf   = M_buf_create();
 *     handle->write_buf  = M_buf_create();
 *
 *     callbacks = M_io_callbacks_create();
 *     M_io_callbacks_reg_processevent(callbacks, processevent_cb);
 *     M_io_callbacks_reg_read(callbacks, read_cb);
 *     M_io_callbacks_reg_write(callbacks, write_cb);
 *     M_io_callbacks_reg_unregister(callbacks, unregister_cb);
 *     M_io_callbacks_reg_reset(callbacks, reset_cb);
 *     M_io_callbacks_reg_destroy(callbacks, destroy_cb);
 *     M_io_callbacks_reg_errormsg(callbacks, error_cb);
 *
 *     M_io_layer_add(io, "BASIC_LAYER", handle, callbacks);
 *
 *     M_io_callbacks_destroy(callbacks);
 * }
 * \endcode
 *
 * ### STX + ETX + LRC with ACK / NAK and resending
 *
 * \code{.c}
 * // IO Layer for generating and unwrapping STX+ETX+LRC with ACK/NAK messaging.
 * // Supports resending message if no response is received within a specified time.
 * // Will only resent X times before giving up and erring.
 *
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/io/m_io_layer.h>
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * struct M_io_handle {
 *     // Read.
 *     M_parser_t      *readparser;   // Buffer containing message that's currently being stitched together from packets.
 *     M_buf_t         *readbuf;      // Buffer containing message that's currently being consumed by the caller.
 *
 *     // Write.
 *     M_buf_t         *last_msg;     // Last buffered message, used for resend if ACK not received.
 *     M_buf_t         *writebuf;     // Buffer containing message that's currently being consumed by the caller.
 *     M_bool           can_write;    // Whether we can write more data. We only allow 1 outstanding message at a time.
 *
 *     size_t           resend_cnt;   // Number of times we've tried resending the message.
 *     M_event_timer_t *resend_timer; // Timer to track when a response hasn't been received and the message should be resent.
 *
 *     // Error
 *     char             err[256];     // Error message buffer.
 * }; // Typedef'd to M_io_handle_t in m_io_layer.h
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static const char          *LAYER_NAME      = "STXETXLRCACKNAK";
 * static const size_t         RESEND_MAX      = 5;
 * static const size_t         RESEND_INTERVAL = 3*1000; // 3 seconds
 * static const char          *STX_STR         = "\x02";
 * static const char          *ACK_STR         = "\x06";
 * static const char          *NAK_STR         = "\x15";
 * static const unsigned char  STX             = 0x02;
 * static const unsigned char  ETX             = 0x03;
 * static const unsigned char  ACK             = 0x06;
 * static const unsigned char  NAK             = 0x15;
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_io_error_t write_int(M_io_layer_t *layer, M_io_handle_t *handle, M_io_meta_t *meta)
 * {
 *     M_io_t       *io;
 *     size_t        layer_idx;
 *     M_io_error_t  err       = M_IO_ERROR_SUCCESS;;
 *     size_t        write_len = 0;
 *
 *     if (layer == NULL || handle == NULL)
 *         return M_IO_ERROR_INVALID;
 *
 *     if (M_buf_len(handle->writebuf) == 0)
 *         return M_IO_ERROR_SUCCESS;
 *
 *     io        = M_io_layer_get_io(layer);
 *     layer_idx = M_io_layer_get_index(layer);
 *
 *     if (io == NULL || layer_idx == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     while (err == M_IO_ERROR_SUCCESS && M_buf_len(handle->writebuf) > 0) {
 *         write_len = M_buf_len(handle->writebuf);
 *         err = M_io_layer_write(io, layer_idx-1, (const M_uint8 *)M_buf_peek(handle->writebuf), &write_len, meta);
 *         if (err != M_IO_ERROR_SUCCESS) {
 *             write_len = 0;
 *         }
 *         M_buf_drop(handle->writebuf, write_len);
 *     }
 *
 *     if (err == M_IO_ERROR_SUCCESS && M_buf_len(handle->writebuf) == 0 && handle->can_write)
 *         M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
 *
 *     if (M_buf_len(handle->writebuf) == 0)
 *         M_event_timer_start(handle->resend_timer, RESEND_INTERVAL);
 *
 *     return err;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static void resend_message(M_io_layer_t *layer, M_io_handle_t *handle)
 * {
 *     M_event_timer_stop(handle->resend_timer);
 *
 *     if (handle->resend_cnt >= RESEND_MAX) {
 *         M_snprintf(handle->err, sizeof(handle->err), "Timeout: Exceeded resent attempts");
 *         M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_TIMEDOUT);
 *         return;
 *     }
 *
 *     M_printf("%s: RESEND!", LAYER_NAME);
 *
 *     // Write the last message again.
 *     M_buf_truncate(handle->writebuf, 0);
 *     M_buf_add_bytes(handle->writebuf, M_buf_peek(handle->last_msg), M_buf_len(handle->last_msg));
 *     write_int(layer, handle, NULL);
 *
 *     handle->resend_cnt++;
 *     // Restart the resend timer and try again.
 *     M_event_timer_start(handle->resend_timer, RESEND_INTERVAL);
 * }
 *
 * static void resend_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_io_layer_t  *layer  = thunk;
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     (void)el;
 *     (void)io;
 *     (void)etype;
 *
 *     M_printf("%s: NO RESPONSE!", LAYER_NAME);
 *     resend_message(layer, handle);
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_io_handle_t *create_handle(void)
 * {
 *     M_io_handle_t *handle;
 *
 *     handle                  = M_malloc_zero(sizeof(*handle));
 *     handle->readparser      = M_parser_create(M_PARSER_FLAG_NONE);
 *     handle->readbuf         = M_buf_create();
 *     handle->writebuf        = M_buf_create();
 *     handle->last_msg        = M_buf_create();
 *     handle->can_write       = M_TRUE;
 *
 *     return handle;
 * }
 *
 * static void destroy_handle(M_io_handle_t *handle)
 * {
 *     if (handle == NULL)
 *         return;
 *
 *     M_parser_destroy(handle->readparser);
 *     M_buf_cancel(handle->readbuf);
 *     M_buf_cancel(handle->writebuf);
 *     M_buf_cancel(handle->last_msg);
 *     M_event_timer_remove(handle->resend_timer);
 *
 *     M_free(handle);
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_bool process_message(M_io_layer_t *layer, M_io_handle_t *handle)
 * {
 *     M_parser_t           *msg_parser  = NULL;
 *     M_PARSER_FRAME_ERROR  frame_error;
 *     size_t                len;
 *
 *     if (M_parser_len(handle->readparser) == 0)
 *         return M_TRUE;
 *
 *     // NAK means we need to send the last message again.
 *     len = M_parser_consume_charset(handle->readparser, (const unsigned char *)NAK_STR, 1);
 *     if (len != 0) {
 *         M_printf("%s: NAK!", LAYER_NAME);
 *         resend_message(layer, handle);
 *         return M_TRUE;
 *     }
 *
 *     // Got something that's not a NAK.
 *      // Even if it's not an ACK we'll consider
 *     // the last message we sent processed.
 *     handle->resend_cnt = 0;
 *     M_event_timer_stop(handle->resend_timer);
 *     M_buf_truncate(handle->last_msg, 0);
 *
 *     // ACK tells us we can write more data.
 *     len = M_parser_consume_charset(handle->readparser, (const unsigned char *)ACK_STR, 1);
 *     if (len > 1)
 *         handle->can_write = M_TRUE;
 *
 *     // We could have garbage that will be ignored.
 *     len = M_parser_consume_not_charset(handle->readparser, (const unsigned char *)STX_STR, 1);
 *     if (len > 0)
 *         M_printf("%s: Unexpected data was dropped (%zu bytes)", LAYER_NAME, len);
 *
 *     // We either have an STX at the start or no data. We also need at least 4 bytes of data to have a framed message.
 *      // We could have only received an ACK earlier and not need to process any data.
 *     if (M_parser_len(handle->readparser) < 4)
 *         return M_TRUE;
 *
 *     // Pull the data out of the wrapper.
 *     frame_error = M_parser_read_stxetxlrc_message(handle->readparser, &msg_parser, M_PARSER_FRAME_ETX);
 *     if (frame_error != M_PARSER_FRAME_ERROR_SUCCESS) {
 *         // M_PARSER_FRAME_ERROR_INVALID, M_PARSER_FRAME_ERROR_NO_ETX, and M_PARSER_FRAME_ERROR_NO_LRC indicating needing more data
 *          // will result in waiting for more data to come in. M_PARSER_FRAME_ERROR_NO_STX shouldn't happen because
 *         // we've already validated the first character is an STX. M_PARSER_FRAME_ERROR_LRC_CALC_FAILED is a real faulre.
 *         if (frame_error == M_PARSER_FRAME_ERROR_LRC_CALC_FAILED) {
 *             M_printf("%s: Message LRC verification failed: dropped (%zu bytes)", LAYER_NAME, M_parser_len(msg_parser));
 *
 *             M_buf_add_byte(handle->writebuf, NAK);
 *             write_int(layer, handle, NULL);
 *         }
 *         M_parser_destroy(msg_parser);
 *         return M_TRUE;
 *     }
 *
 *     // Store the data.
 *     M_buf_add_bytes(handle->readbuf, M_parser_peek(msg_parser), M_parser_len(msg_parser));
 *
 *     // Write an ACK.
 *     M_buf_add_byte(handle->writebuf, ACK);
 *     write_int(layer, handle, NULL);
 *
 *     M_parser_destroy(msg_parser);
 *     return M_FALSE;
 * }
 *
 * static M_bool handle_read(M_io_layer_t *layer, M_io_handle_t *handle, M_event_type_t *type)
 * {
 *     M_io_t        *io        = M_io_layer_get_io(layer);
 *     M_bool         discard   = M_TRUE; // Default to processing and then discarding all events
 *     M_io_error_t   err;
 *     unsigned char  buf[8192] = { 0 };
 *     size_t         len;
 *
 *     do {
 *         len = sizeof(buf);
 *         err = M_io_layer_read(io, M_io_layer_get_index(layer)-1, buf, &len, NULL); 
 *
 *         if (err == M_IO_ERROR_SUCCESS) {
 *             M_parser_append(handle->readparser, buf, len);
 *             discard = process_message(layer, handle);
 *         } else if (M_io_error_is_critical(err)) {
 *             M_snprintf(handle->err, sizeof(handle->err), "Error reading message: %s", M_io_error_string(err));
 *             *type = M_EVENT_TYPE_ERROR;
 *         }
 *     } while (err == M_IO_ERROR_SUCCESS);
 *
 *     return discard;
 * }
 *
 * static M_bool handle_write(M_io_layer_t *layer, M_io_handle_t *handle, M_event_type_t *type)
 * {
 *     M_io_error_t err;
 *
 *     err = write_int(layer, handle, NULL);
 *     if (M_io_error_is_critical(err)) {
 *         M_snprintf(handle->err, sizeof(handle->err), "Error writing message: %s", M_io_error_string(err));
 *         *type = M_EVENT_TYPE_ERROR;
 *         return M_TRUE;
 *     }
 *
 *     return M_FALSE;
 * }
 *
 * static M_io_error_t read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t *handle;
 *     size_t         bytes_left;
 *
 *     (void)meta;
 *
 *     handle = M_io_layer_get_handle(layer);
 *
 *     // Zero-length reads are not allowed.
 *     if (handle == NULL || buf == NULL || read_len == NULL || *read_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     // Process any outstanding messages and fill our read out buffer.
 *     process_message(layer, handle);
 *
 *     // Check if we have a full message.
 *     if (M_buf_len(handle->readbuf) == 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     // Read everything we can.
 *     bytes_left = M_buf_len(handle->readbuf);
 *     *read_len  = M_MIN(*read_len, bytes_left);
 *
 *     M_mem_copy(buf, M_buf_peek(handle->readbuf), *read_len);
 *     M_buf_drop(handle->readbuf, *read_len);
 *
 *     // If we still have data available to read (needs to be processed)
 *      // send another read event.
 *     if (M_parser_len(handle->readparser) > 0)
 *         M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
 *
 *     return M_IO_ERROR_SUCCESS;
 * }
 *
 * static M_io_error_t write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *buf_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t  *handle      = M_io_layer_get_handle(layer);;
 *     M_io_t         *io          = M_io_layer_get_io(layer);
 *     size_t          mywrite_len = 0;
 *
 *     if (handle == NULL || buf == NULL || buf_len == NULL || *buf_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     if (M_buf_len(handle->writebuf) > 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     // Wrap the message in "<stx> + <data> + <etx> + <lrc>"
 *     M_buf_add_byte(handle->writebuf, STX);
 *     M_buf_add_bytes(handle->writebuf, buf, *buf_len);
 *     M_buf_add_byte(handle->writebuf, ETX);
 *     M_buf_add_byte(handle->writebuf, M_mem_calc_lrc(M_buf_peek(handle->writebuf)+1, M_buf_len(handle->writebuf)-1));
 *
 *     // Store this message in case we need to resend because writebuf will be truncated as data gets sent.
 *     M_buf_truncate(handle->last_msg, 0);
 *     M_buf_add_bytes(handle->last_msg, M_buf_peek(handle->writebuf), M_buf_len(handle->writebuf));
 *
 *     handle->can_write = M_FALSE;
 *     return write_int(layer, handle, meta);
 * }
 *
 * static M_bool processevent_cb(M_io_layer_t *layer, M_event_type_t *type)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *     M_io_t        *io     = M_io_layer_get_io(layer);
 *     const char    *estr;
 *
 *     switch (*type) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             handle->can_write = M_TRUE;
 *             // Fall thru
 *         case M_EVENT_TYPE_WRITE:
 *             return handle_write(layer, handle, type);
 *         case M_EVENT_TYPE_READ:
 *             return handle_read(layer, handle, type);
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_OTHER:
 *             return M_FALSE;
 *     }
 *
 *     return M_FALSE;
 * }
 *
 * static M_bool error_cb(M_io_layer_t *layer, char *error, size_t err_len)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     if (M_str_isempty(handle->err))
 *         return M_FALSE;
 *
 *     M_str_cpy(error, err_len, handle->err);
 *     return M_TRUE;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_bool init_cb(M_io_layer_t *layer)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *     M_io_t        *io     = M_io_layer_get_io(layer);
 *
 *     if (handle->resend_timer != NULL)
 *         M_event_timer_remove(handle->resend_timer);
 *     handle->resend_timer = M_event_timer_add(M_io_get_event(io), resend_cb, layer);
 *     return M_TRUE;
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static void unregister_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static M_bool reset_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 *     return M_TRUE;
 * }
 *
 * static void destroy_cb(M_io_layer_t *layer)
 * {
 *     if (layer == NULL)
 *         return;
 *     destroy_handle(M_io_layer_get_handle(layer));
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // --- MAIN ENTRY ---
 *
 * void stxetxlrcacknak_io_layer(M_io_t *io, M_bool reconnect)
 * {
 *     M_io_handle_t    *handle    = create_handle();
 *     M_io_callbacks_t *callbacks = M_io_callbacks_create();
 *
 *     M_io_callbacks_reg_init(callbacks, init_cb);
 *     M_io_callbacks_reg_read(callbacks, read_cb);
 *     M_io_callbacks_reg_write(callbacks, write_cb);
 *     M_io_callbacks_reg_processevent(callbacks, processevent_cb);
 *     M_io_callbacks_reg_unregister(callbacks, unregister_cb);
 *     M_io_callbacks_reg_reset(callbacks, reset_cb);
 *     M_io_callbacks_reg_destroy(callbacks, destroy_cb);
 *     M_io_callbacks_reg_errormsg(callbacks, error_cb);
 *
 *     M_io_layer_add(io, LAYER_NAME, handle, callbacks);
 *
 *     M_io_callbacks_destroy(callbacks);
 * }
 *
 * M_bool stxetxlrcacknak_layer_waiting_for_response(M_io_t *io)
 * {
 *     size_t layer_count;
 *     size_t layer_idx;
 *     M_bool ret = M_FALSE;
 *
 *     layer_count = M_io_layer_count(io);
 *
 *     for (layer_idx=0; layer_idx<layer_count; layer_idx++) {
 *         M_io_layer_t *layer = M_io_layer_acquire(io, layer_idx, LAYER_NAME);
 *         if (layer == NULL) {
 *             continue;
 *         }
 *         M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *         ret = !handle->can_write;
 *         M_io_layer_release(layer);
 *         break;
 *     }
 *
 *     return ret;
 * }
 * \endcode
 *
 * ### BLE Helper
 *
 * \code{.c}
 * #include <mstdlib/io/m_io_layer.h>
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static const char *LAYER_NAME         = "BLE_SERVICE_HELPER";
 * static const char *BLE_SERVICE        = "68950001-FBA1-BB3D-A043-647EF78ACD44";
 * static const char *BLE_CHARACTERISTIC = "68951001-FBA1-BB3D-A043-647EF78ACD44";
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * struct M_io_handle {
 *     M_io_meta_t *write_meta;
 *     M_buf_t     *read_buf;
 *     M_buf_t     *write_buf;
 *     char         err[256];
 *     M_bool       connected;
 * };
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * // Writes as much data as possible from the given buffer to the layer below this one.
 * static M_io_error_t write_to_next_layer(M_io_layer_t *layer, M_buf_t *buf, M_io_meta_t *meta)
 * {
 *     M_io_t       *io;
 *     M_io_error_t  err;
 *     size_t        layer_idx;
 *     size_t        write_len;
 *
 *     if (layer == NULL || M_buf_len(buf) == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     io        = M_io_layer_get_io(layer);
 *     layer_idx = M_io_layer_get_index(layer);
 *
 *     if (io == NULL || layer_idx == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     err = M_IO_ERROR_SUCCESS;
 *     do {
 *         write_len = M_buf_len(buf);
 *         err       = M_io_layer_write(io, layer_idx - 1, (const M_uint8 *)M_buf_peek(buf), &write_len, meta);
 *         if (err == M_IO_ERROR_SUCCESS) {
 *             M_buf_drop(buf, write_len);
 *         }
 *     } while (err == M_IO_ERROR_SUCCESS && M_buf_len(buf) > 0);
 *
 *     return err;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_io_error_t write_pair_check_data(M_io_layer_t *layer, M_buf_t *write_buf, M_io_meta_t *write_meta)
 * {
 *     // Simple command used as a dummy write to initiate pairing if needed. We just want to send something.
 *     // But this should be a real, simple, command for the device.
 *     M_buf_add_str(write_buf, "123\r\n");
 *     return write_to_next_layer(layer, write_buf, write_meta);
 * }
 *
 * static M_io_error_t register_notifications(M_io_t *io, M_io_layer_t *layer, const char *service_uuid, const char *char_uuid)
 * {
 *     M_io_meta_t  *meta;
 *     M_io_error_t  err;
 *
 *     meta = M_io_meta_create();
 *     M_io_ble_meta_set_service(io, meta, service_uuid);
 *     M_io_ble_meta_set_characteristic(io, meta, char_uuid);
 *
 *     M_io_ble_meta_set_notify(io, meta, M_TRUE);
 *     M_io_ble_meta_set_write_type(io, meta, M_IO_BLE_WTYPE_REQNOTIFY);
 *
 *     // Write the metadata to the device to register we want to receive notification read events.
 *     err = M_io_layer_write(io, M_io_layer_get_index(layer) - 1, NULL, NULL, meta);
 *     M_io_meta_destroy(meta);
 *
 *     return err;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * static M_bool processevent_cb(M_io_layer_t *layer, M_event_type_t *type)
 * {
 *     M_io_handle_t *handle  = M_io_layer_get_handle(layer);
 *     M_io_t        *io      = M_io_layer_get_io(layer);
 *     M_io_meta_t   *meta;
 *     M_bool         consume = M_FALSE; // Default to passing onto the next layer.
 *     M_io_error_t   err;
 *
 *     if (handle == NULL || io == NULL)
 *         return M_FALSE;
 *
 *     switch (*type) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             // If the device uses secure bonded/paired connections and a device is not paired, pairing takes place
 *              // when a secure characteristic is first written. At which point the device will show a PIN
 *             // and system (iOS, macOS...) will show a prompt to enter the PIN. We'll delay sending the 
 *             // connected event until we get a response to a basic message that we use internally
 *             // to verify or initiate the pairing.
 *             //
 *             // We need to do this before trying to register for notifications. If
 *             // we're already paired then there is no problem. However, if we aren't
 *             // paired and need to go through pairing, attempting to register a notification
 *             // before writing a message will cause the device to disconnect.
 *             err = write_pair_check_data(layer, handle->write_buf, handle->write_meta);
 *             if (err != M_IO_ERROR_SUCCESS) {
 *                 M_snprintf(handle->err, sizeof(handle->err), "Error writing initial pairing message: %s", M_io_error_string(err));
 *                 *type = M_EVENT_TYPE_ERROR;
 *                 break;
 *             }
 *             consume = M_TRUE;
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             // Write event if we're not connected means this is a confirmation event that the data we
 *              // wrote was processed. If we're paired we'll get this right away. If we're not paired
 *             // we'll get this when pairing is successful. Otherwise, we'd get an error event.
 *             if (!handle->connected) {
 *                 // We're paired so we can register for our read events.
 *                 consume = M_TRUE;
 *                 register_notifications(io, layer, BLE_SERVICE, BLE_CHARACTERISTIC);
 *                 break;
 *             }
 *
 *             if (M_buf_len(handle->write_buf) != 0) {
 *                 err = write_to_next_layer(layer, handle->write_buf, handle->write_meta);
 *                 if (M_io_error_is_critical(err)) {
 *                     M_snprintf(handle->err, sizeof(handle->err), "Error writing data: %s", M_io_error_string(err));
 *                     *type = M_EVENT_TYPE_ERROR;
 *                 } else if (M_buf_len(handle->write_buf) != 0) {
 *                     // Don't inform higher levels we can write if we have more data pending.
 *                     consume = M_TRUE;
 *                 }
 *             }
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             // We're getting data from the device. Let's pull out the data and check if we want it or not.
 *             meta = M_io_meta_create();
 *             do {
 *                 M_uint8 buf[256] = { 0 };
 *                 size_t  buf_len  = sizeof(buf);
 *
 *                 err = M_io_layer_read(io, M_io_layer_get_index(layer) - 1, buf, &buf_len, meta);
 *                 // We should get one notification response to the single notification we requested.
 *                  // If we aren't connected and we get it, then we want to tell those above us we're
 *                 // connected now that we've finished setup. Otherwise, if we get an unexpected notification
 *                 // response, eat it because there is no data for anyone to read.
 *                 if (M_io_ble_meta_get_read_type(io, meta) == M_IO_BLE_RTYPE_NOTIFY) {
 *                     if (handle->connected) {
 *                         consume = M_TRUE;
 *                     } else {
 *                         handle->connected  = M_TRUE;
 *                         *type              = M_EVENT_TYPE_CONNECTED;
 *                         break;
 *                     }
 *                 }
 *
 *                 if (err == M_IO_ERROR_SUCCESS) {
 *                     // Save the data into the handler's buffer so we
 *                     // can pass it up to the layer above in the read callback.
 *                     M_buf_add_bytes(handle->read_buf, buf, buf_len);
 *                 } else if (M_io_error_is_critical(err)) {
 *                     M_snprintf(handle->err, sizeof(handle->err), "Error reading data: %s", M_io_error_string(err));
 *                     *type = M_EVENT_TYPE_ERROR;
 *                     break;
 *                 }
 *             } while (err == M_IO_ERROR_SUCCESS);
 *             M_io_meta_destroy(meta);
 *             break;
 *         case M_EVENT_TYPE_ERROR:
 *             M_io_get_error_string(io, handle->err, sizeof(handle->err));
 *             break;
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_OTHER:
 *             break;
 *     }
 *
 *     // M_TRUE to discard this event, or M_FALSE to pass it on to the next layer.
 *     return consume;
 * }
 *
 * static M_io_error_t read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *buf_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     (void)meta;
 *
 *     if (handle == NULL || buf == NULL || buf_len == NULL || *buf_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     // Check if we have any more data to pass on or not.
 *     if (M_buf_len(handle->read_buf) == 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     // Pass on as much data as possible.
 *     *buf_len = M_MIN(*buf_len, M_buf_len(handle->read_buf));
 *     M_mem_copy(buf, M_buf_peek(handle->read_buf), *buf_len);
 *     M_buf_drop(handle->read_buf, *buf_len);
 *
 *     return M_IO_ERROR_SUCCESS;
 * }
 *
 * static M_io_error_t write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *buf_len, M_io_meta_t *meta)
 * {
 *     M_io_handle_t *handle;
 *     M_io_error_t   err;
 *
 *     (void)meta;
 *
 *     handle = M_io_layer_get_handle(layer);
 *
 *     if (handle == NULL || buf == NULL || buf_len == NULL || *buf_len == 0)
 *         return M_IO_ERROR_INVALID;
 *
 *     if (!handle->connected)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     // Don't allow buffering data if we have data waiting to write.
 *     if (M_buf_len(handle->write_buf) > 0)
 *         return M_IO_ERROR_WOULDBLOCK;
 *
 *     M_buf_add_bytes(handle->write_buf, buf, *buf_len);
 *
 *     // Try to write as much of the message as we can right now.
 *     err = write_to_next_layer(layer, handle->write_buf, handle->write_meta);
 *
 *     // Treat would block as success because we've buffered the data and it will
 *      // be written when possible.
 *     if (err == M_IO_ERROR_WOULDBLOCK)
 *         err = M_IO_ERROR_SUCCESS;
 *     return err;
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static void unregister_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 * }
 *
 * // Dummy callback - only here because the docs for M_io_callbacks_t requires it to be present.
 * static M_bool reset_cb(M_io_layer_t *layer)
 * {
 *     (void)layer;
 *     // No-op
 *     return M_TRUE;
 * }
 *
 * static void destroy_cb(M_io_layer_t *layer)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);;
 *
 *     M_io_meta_destroy(handle->write_meta);
 *     M_buf_cancel(handle->read_buf);
 *     M_buf_cancel(handle->write_buf);
 *
 *     M_free(handle);
 * }
 *
 * static M_bool error_cb(M_io_layer_t *layer, char *error, size_t err_len)
 * {
 *     M_io_handle_t *handle = M_io_layer_get_handle(layer);
 *
 *     if (M_str_isempty(handle->err))
 *         return M_FALSE;
 *
 *     M_str_cpy(error, err_len, handle->err);
 *
 *     return M_TRUE;
 * }
 *
 * // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * // --- MAIN ENTRY ---
 * //
 * void ble_helper_io_layer(M_io_t *io)
 * {
 *     M_io_handle_t    *handle;
 *     M_io_callbacks_t *callbacks;
 *
 *     if (io == NULL)
 *         return;
 *
 *     handle             = M_malloc_zero(sizeof(*handle));
 *     handle->write_meta = M_io_meta_create();
 *     handle->read_buf   = M_buf_create();
 *     handle->write_buf  = M_buf_create();
 *
 *     M_io_ble_meta_set_service(io, handle->write_meta, BLE_SERVICE);
 *     M_io_ble_meta_set_characteristic(io, handle->write_meta, BLE_CHARACTERISTIC);
 *     M_io_ble_meta_set_write_type(io, handle->write_meta, M_IO_BLE_WTYPE_WRITE);
 *
 *     callbacks = M_io_callbacks_create();
 *     M_io_callbacks_reg_processevent(callbacks, processevent_cb);
 *     M_io_callbacks_reg_read(callbacks, read_cb);
 *     M_io_callbacks_reg_write(callbacks, write_cb);
 *     M_io_callbacks_reg_unregister(callbacks, unregister_cb);
 *     M_io_callbacks_reg_reset(callbacks, reset_cb);
 *     M_io_callbacks_reg_destroy(callbacks, destroy_cb);
 *     M_io_callbacks_reg_errormsg(callbacks, error_cb);
 *
 *     M_io_layer_add(io, LAYER_NAME, handle, callbacks);
 *
 *     M_io_callbacks_destroy(callbacks);
 * }
 * \endcode
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

/*! Register callback to process events.  Optional. If returns M_TRUE event is consumed and not propagated to the next layer. */
M_API M_bool M_io_callbacks_reg_processevent(M_io_callbacks_t *callbacks, M_bool (*cb_process_event)(M_io_layer_t *layer, M_event_type_t *type));

/*! Register callback that is called when io object is removed from event object. Mandatory */
M_API M_bool M_io_callbacks_reg_unregister(M_io_callbacks_t *callbacks, void (*cb_unregister)(M_io_layer_t *layer));

/*! Register callback to start a graceful disconnect sequence.  Optional. */
M_API M_bool M_io_callbacks_reg_disconnect(M_io_callbacks_t *callbacks, M_bool (*cb_disconnect)(M_io_layer_t *layer));

/*! Register callback to reset any state (M_io_handle_t *). Optional.
 *
 * Will reset the state of the layer for re-connection.
 */
M_API M_bool M_io_callbacks_reg_reset(M_io_callbacks_t *callbacks, M_bool (*cb_reset)(M_io_layer_t *layer));

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

/*! Add a soft-event.  If sibling_only is true, will only notify next layer and not self. Must specify an error. */
M_API void M_io_layer_softevent_add(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type, M_io_error_t err);

/*! Clear all soft events for the current layer */
M_API void M_io_layer_softevent_clear(M_io_layer_t *layer);

/*! Add a soft-event.  If sibling_only is true, will only delete the soft event for the next layer up and not self. */
M_API void M_io_layer_softevent_del(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type);

/*! Sets the internal error for the IO object.  Used within a process events callback if emitting an error */
M_API void M_io_set_error(M_io_t *io, M_io_error_t err);

/*! @} */

__END_DECLS


#endif
