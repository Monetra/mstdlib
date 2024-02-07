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

#ifndef __M_IO_BUFFER_H__
#define __M_IO_BUFFER_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_buffer Addon for I/O buffering
 *  \ingroup m_eventio_base_addon
 *
 * Intermediate layer for buffering reads/writes from the OS.
 *
 * Allows data to be buffered, this can reduce the number of syscalls required
 * to the OS and increase performance at the cost of memory.
 *
 * @{
 */


/*! Add a buffer layer.  Cannot be combined with base IO objects which utilize
 *  M_io_meta_t.
 *
 * \param[in]  io               io object.
 * \param[out] layer_id         Layer id this is added at.
 * \param[in]  max_read_buffer  Maximum read buffer size in bytes. If not a power of 2 will be rounded up to the
 *                              next power of 2.  Use 0 to disable read buffering.
 * \param[in]  max_write_buffer Maximum write buffer size in bytes. If not a power of 2 will be rounded up to the
 *                              next power of 2.  Use 0 to disable write buffering.
 * \return Result.
 */
M_API M_io_error_t M_io_add_buffer(M_io_t *io, size_t *layer_id, size_t max_read_buffer, size_t max_write_buffer);


/*! @} */

__END_DECLS

#endif

