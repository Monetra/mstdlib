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

#ifndef __M_IO_H__
#define __M_IO_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_parser.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_io Common I/O functions
 *  \ingroup m_eventio
 * 
 * Common IO functions
 *
 * @{
 */

/*! io type */
enum M_io_type {
	M_IO_TYPE_STREAM   = 1, /*!< Object is a stream handle, can read and write             */
	M_IO_TYPE_WRITER   = 2, /*!< Object is write only, such as a one-way pipe              */
	M_IO_TYPE_READER   = 3, /*!< Object is read only, such as a one-way pipe               */
	M_IO_TYPE_LISTENER = 4, /*!< Object is a listener for acceptance of new connections    */
	M_IO_TYPE_EVENT    = 5  /*!< Object just handles events, cannot connect, read or write */
};
typedef enum M_io_type M_io_type_t;


/*! io object. */
struct M_io;
typedef struct M_io M_io_t;


/*! io meta object. */
struct M_io_meta;
typedef struct M_io_meta M_io_meta_t;


/*! io error. */
enum M_io_error {
	M_IO_ERROR_SUCCESS           = 0,  /*!< Success. No Error     */
	M_IO_ERROR_WOULDBLOCK        = 1,  /*!< Operation would block */
	M_IO_ERROR_DISCONNECT        = 2,  /*!< Connection disconnected during operation */
	M_IO_ERROR_ERROR             = 3,  /*!< Generic Undefined error occurred */
	M_IO_ERROR_NOTCONNECTED      = 4,  /*!< Connection is not established, invalid operation */
	M_IO_ERROR_NOTPERM           = 5,  /*!< Not a permitted action for this io object */
	M_IO_ERROR_CONNRESET         = 6,  /*!< Connection was reset by peer */
	M_IO_ERROR_CONNABORTED       = 7,  /*!< Connection aborted */
	M_IO_ERROR_ADDRINUSE         = 8,  /*!< Address or Port already in use */
	M_IO_ERROR_PROTONOTSUPPORTED = 9,  /*!< Protocol not supported by OS */
	M_IO_ERROR_CONNREFUSED       = 10, /*!< Connection refused */
	M_IO_ERROR_NETUNREACHABLE    = 11, /*!< Network requested is unreachable */
	M_IO_ERROR_TIMEDOUT          = 12, /*!< Operation timed out at the OS level */
	M_IO_ERROR_NOSYSRESOURCES    = 13, /*!< System reported not enough resources */
	M_IO_ERROR_INVALID           = 14, /*!< Invalid use or order of operation */
	M_IO_ERROR_NOTIMPL           = 15, /*!< OS Does not implement the command or parameters */
	M_IO_ERROR_NOTFOUND          = 16, /*!< File/Path not found */
/* Potential future errors
    M_IO_ERROR_AUTHFAILED -- could be used by proxies, maybe others
    M_IO_ERROR_BADCERTIFICATE -- Certificate verification failure, ssl
*/
	M_IO_ERROR_INTERRUPTED       = 99  /*!< Should never be returned to a user */
};
typedef enum M_io_error M_io_error_t;


/*! io state. */
enum M_io_state {
	M_IO_STATE_INIT          = 0, /*!< Initializing, not yet prompted to start connecting */
	M_IO_STATE_LISTENING     = 1, /*!< Listening for a client connection                  */
	M_IO_STATE_CONNECTING    = 2, /*!< Attempting to establish a connection               */
	M_IO_STATE_CONNECTED     = 3, /*!< Connected                                          */
	M_IO_STATE_DISCONNECTING = 4, /*!< In-progress graceful disconnect                    */
	M_IO_STATE_DISCONNECTED  = 5, /*!< Connection Closed/Disconnected                     */
	M_IO_STATE_ERROR         = 6  /*!< Connection in error state (not connected)          */
};
typedef enum M_io_state M_io_state_t;


/*! Passed to M_io_layer_acquire() to search for matching layer by name */
#define M_IO_LAYER_FIND_FIRST_ID SIZE_MAX


/*! Convert an error code to a string.
 *
 * \param[in] error Error code.
 *
 * \return String description.
 */
M_API const char *M_io_error_string(M_io_error_t error);


/*! Create an io meta data object.
 *
 * \return io meta data object.
 */
M_API M_io_meta_t *M_io_meta_create(void);


/*! Destory an io meta data object.
 *
 * \param[in] meta meta data object.
 */
M_API void M_io_meta_destroy(M_io_meta_t *meta);


/*! Read from an io object.
 *
 * \param[in]  comm     io object.
 * \param[out] buf      Buffer to store data read from io object.
 * \param[in]  buf_len  Lenght of provided buffer.
 * \param[out] len_read Number of bytes fread from the io object.
 *
 * \return Result.
 *
 * \see M_io_read_meta
 */
M_API M_io_error_t M_io_read(M_io_t *comm, unsigned char *buf, size_t buf_len, size_t *len_read);


/*! Read from an io object into an M_buf_t.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  comm io object.
 * \param[out] buf  Buffer to store data read from io object.
 *
 * \return Result.
 *
 * \see M_io_read_into_buf_meta
 */
M_API M_io_error_t M_io_read_into_buf(M_io_t *comm, M_buf_t *buf);


/*! Read from an io object into an M_parser_t.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  comm   io object.
 * \param[out] parser Parser to store data read from io object.
 *
 * \return Result.
 *
 * \see M_io_read_into_parser_meta
 */
M_API M_io_error_t M_io_read_into_parser(M_io_t *comm, M_parser_t *parser);


/*! Read from an io object with a meta data object.
 *
 * \param[in]  comm     io object.
 * \param[out] buf      Buffer to store data read from io object.
 * \param[in]  buf_len  Lenght of provided buffer.
 * \param[out] len_read Number of bytes fread from the io object.
 * \param[in]  meta     Meta data object.
 *
 * \return Result.
 *
 * \see M_io_read
 */
M_API M_io_error_t M_io_read_meta(M_io_t *comm, unsigned char *buf, size_t buf_len, size_t *len_read, M_io_meta_t *meta);


/*! Read from an io object into an M_buf_t with a meta data object.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  comm io object.
 * \param[out] buf  Buffer to store data read from io object.
 * \param[in]  meta Meta data object.
 *
 * \return Result.
 *
 * \see M_io_read_into_buf
 */
M_API M_io_error_t M_io_read_into_buf_meta(M_io_t *comm, M_buf_t *buf, M_io_meta_t *meta);


/*! Read from an io object into an M_parser_t with a meta data object.
 *
 * This will read all available data into the buffer.
 *
 * \param[in]  comm   io object.
 * \param[out] parser Parser to store data read from io object.
 * \param[in]  meta   Meta data object.
 *
 * \return Result.
 *
 * \see M_io_read_into_parser
 */
M_API M_io_error_t M_io_read_into_parser_meta(M_io_t *comm, M_parser_t *parser, M_io_meta_t *meta);


/*! Clear/Flush the read buffer to consume all data and dispose of it.
 *
 *  \param[in] io  io object
 *  \return M_IO_ERROR_SUCCESS if data was flushed and the connection is still
 *          active.  M_IO_ERROR_WOULDBLOCK if no data to flush, otherwise one
 *          of the additional errors if the connection failed.
 */
M_API M_io_error_t M_io_read_clear(M_io_t *io);


/*! Write data to an io object.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should wait until the next write event and then try
 * writing more data.
 *
 * \param[in]  comm        io object.
 * \param[in]  buf         Buffer to write from.
 * \param[in]  buf_len     Number of bytes in buffer to write.
 * \param[out] len_written Number of bytes from the buffer written.
 *
 * \return Result.
 *
 * \see M_io_write_meta
 */
M_API M_io_error_t M_io_write(M_io_t *comm, const unsigned char *buf, size_t buf_len, size_t *len_written);


/*! Write data to an io object from an M_buf_t.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should wait until the next write event and then try
 * writing more data.
 *
 * \param[in]  comm io object.
 * \param[in]  buf  Buffer to write from.
 *
 * \return Result.
 *
 * \see M_io_write_from_buf_meta
 */
M_API M_io_error_t M_io_write_from_buf(M_io_t *comm, M_buf_t *buf);


/*! Write data to an io object with a meta data object.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should wait until the next write event and then try
 * writing more data.
 *
 * \param[in]  comm        io object.
 * \param[in]  buf         Buffer to write from.
 * \param[in]  buf_len     Number of bytes in buffer to write.
 * \param[out] len_written Number of bytes from the buffer written.
 * \param[in]  meta        Meta data object.
 *
 * \return Result.
 *
 * \see M_io_write
 */
M_API M_io_error_t M_io_write_meta(M_io_t *comm, const unsigned char *buf, size_t buf_len, size_t *len_written, M_io_meta_t *meta);


/*! Write data to an io object from an M_buf_t with a meta data object.
 *
 * This function will attempt to write as much data as possible. If not all data
 * is written the application should wait until the next write event and then try
 * writing more data.
 *
 * \param[in]  comm io object.
 * \param[in]  buf  Buffer to write from.
 * \param[in]  meta Meta data object.
 *
 * \return Result.
 *
 * \see M_io_write_from_buf
 */
M_API M_io_error_t M_io_write_from_buf_meta(M_io_t *comm, M_buf_t *buf, M_io_meta_t *meta);


/*! Accept an io connection.
 *
 * Typically used with network io when a connection is setup as a listening socket.
 * The io object will remain valid and a new io object for the connection will be created.
 *
 * A return of M_IO_ERROR_WOULDBLOCK should not be treated as an error. It means either
 * there is more data that needs to be received and the event will trigger again. Or
 * there is no more outstanding connections waiting to be accepted.
 *
 * Example:
 *
 *     void ipserver_listen_callback(M_event_t *el, M_event_type_t type, M_io_t *io, void *thunk)
 *     {
 *         M_io_t       *io_out   = NULL;
 *         M_io_error_t  ioerr;
 *
 *         (void)thunk;
 *     
 *         if (type != M_EVENT_TYPE_ACCEPT)
 *             return;
 *     
 *         ioerr = M_io_accept(&io_out, io);
 *         if (ioerr != M_IO_ERROR_SUCCESS || io_out == NULL) {
 *             if (ioerr != M_IO_ERROR_WOULDBLOCK) {
 *                 // Connection error
 *             }
 *             return;
 *         }
 *     
 *         M_event_add(el, io_out, ipserver_connection_callback, NULL);
 *     }
 *
 * \param[out] io_out    io object created from the accept.
 * \param[in]  server_io io object which was listening.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_accept(M_io_t **io_out, M_io_t *server_io);


/*! Current state of an io object
 *
 * \param[in] io io object;
 *
 * \return State.
 */
M_API M_io_state_t M_io_get_state(M_io_t *io);


/*! Current state of a layer within an io object
 *
 * \param[in] io io object
 * \param[in] id id of a layer to query (0 for base layer)
 *
 * \return State.
 */
M_API M_io_state_t M_io_get_layer_state(M_io_t *io, size_t id);


/*! Retrieve the number of layers from an io object 
 *
 *  \param[in] io io object
 *
 *  \return count of layers in object
 */
M_API size_t M_io_layer_count(M_io_t *io);


/*! Retrieve the name of the layer at the specified index.
 *
 * \param[in] io io object
 * \param[in] idx index of layer (0 - M_io_layer_count())
 * \return internal name of layer in string form
 */
M_API const char *M_io_layer_name(M_io_t *io, size_t idx);


/*! Get a textual error message associated with the io object.
 *
 *  This message is populated by the layer that reported the error. The message
 *  could come from an external library such as an TLS library. It is meant
 *  to be a human readable description and should not be used programmatically.
 *
 *  \param[in]  io      io object.
 *  \param[out] error   Error buffer.
 *  \param[in]  err_len Size of error buffer.
 *
 *  \return Textual description of error.
 */
M_API void M_io_get_error_string(M_io_t *io, char *error, size_t err_len);


/*! Request system to tear down existing connection and reconnect using the same
 *  configuration and layers as are currently in use.  Will preserve existing
 *  event handle and callbacks.
 *
 *  \param[in]  io      io object.
 *  \return M_TRUE if object can be reconnected, M_FALSE if cannot be.  This
 *     returning M_TRUE does NOT mean the reconnect itself was successful, must
 *     still wait for CONNECT or ERROR event.
 */
M_API M_bool M_io_reconnect(M_io_t *io);


/*! Gracefully issue a disconnect to the communications object, a DISCONNECTED (or ERROR)
 * event will be triggered when complete.
 *
 * \param[in] comm io object.
 */
M_API void M_io_disconnect(M_io_t *comm);


/*! Destroy any communications object.
 *
 * This can be called from a different thread than the thread the event loop the io object
 * is running on. When this happens the destroy is queued and will happen once the event loop
 * the io object is associated with has finished processing all queued events.
 *
 * \param[in] comm io object.
 */
M_API void M_io_destroy(M_io_t *comm);

/*! @} */

__END_DECLS

#endif /* __M_IO_H__ */
