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

#ifndef __M_IO_TRACE_H__
#define __M_IO_TRACE_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_trace Addon for event and I/O tracing
 *  \ingroup m_eventio_base_addon
 * 
 * Event and I/O tracing addon
 *
 * Allows data to be traced as it flows through the trace layer. For example communication
 * over serial with an external device could have a trace layer that logs read and
 * write commands.
 *
 * This can be very useful when combined with the M_log logging module.
 *
 * @{
 */

/*! Trace even type. */
enum M_io_trace_type {
	M_IO_TRACE_TYPE_READ  = 1,
	M_IO_TRACE_TYPE_WRITE = 2,
	M_IO_TRACE_TYPE_EVENT = 3
};
typedef enum M_io_trace_type M_io_trace_type_t;

/*! Definition for a function callback that is called every time a traceable event is triggered by
 *  the event subsystem.
 *
 *  \param[in] cb_arg     User-specified callback argument registered when the trace was added to the
 *                        event handle.
 *  \param[in] type       The type of type that has been triggered, see M_io_trace_type_t.
 *  \param[in] event_type The type of event that has been triggered, see M_event_type_t.
 *  \param[in] data       Data that is passing though this trace layer.
 *  \param[in] data_len   Length of data.
 */
typedef void (*M_io_trace_cb_t)(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len); 

/*! Definition for a function that duplicates a callback argument.
 *
 * An io that has an accept event such as net can have connection specific arguments. The trace
 * layer will be duplicated from the server to the *new* io client connection. This allows
 * the cb_arg to be duplicated as well.
 *
 * \param[in] cb_arg Callback argument of the type provided to M_io_add_trace.
 */
typedef void *(*M_io_trace_cb_dup_t)(void *cb_arg);

/*! Definition for a function that destroys a user provided callback data associated with the trace.
 *
 * \param[in] cb_arg Callback argument of the type provided to M_io_add_trace.
 */
typedef void (*M_io_trace_cb_free_t)(void *cb_arg);


/*! Add a trace layer
 *
 * \param[in]  io       io object.
 * \param[out] layer_id Layer id this is added at.
 * \param[in]  callback Function called when the trace is triggered.
 * \param[in]  cb_arg   Argument passed to callback.
 * \param[in]  cb_dup   Function to duplicate cb_arg if needed. Optional and can be NULL if not used.
 * \param[in]  cb_free  Function to destroy cb_arg when the io object id destroyed.
 *                      Optional and can be NULL if not used.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_add_trace(M_io_t *io, size_t *layer_id, M_io_trace_cb_t callback, void *cb_arg, M_io_trace_cb_dup_t cb_dup, M_io_trace_cb_free_t cb_free);


/*! Get the callback arg for a trace layer.
 *
 * \param[in] io       io object.
 * \param[in] layer_id Layer id.
 *
 * \return Argument on success or NULL on error. NULL is success if a cb_arg was not set.
 */
M_API void *M_io_trace_get_callback_arg(M_io_t *io, size_t layer_id);


/*! Set the callback arg for the trace layer.
 *
 * \param[in] io       io object.
 * \param[in] layer_id Layer id.
 * \param[in]  cb_arg  Argument passed to callback.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_io_trace_set_callback_arg(M_io_t *io, size_t layer_id, void *cb_arg);

/*! @} */

__END_DECLS

#endif

