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

#ifndef __M_IO_PIPE_H__
#define __M_IO_PIPE_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_pipe PIPE I/O functions
 *  \ingroup m_eventio_base
 *
 * Pipe's are uni-directional connected endpoints, meaning one endpoint
 * is a write-only endpoint, and the other endpoint is a read-only endpoint.
 * Pipes are often used for inter-process communication.
 *
 * @{
 */

typedef enum {
    M_IO_PIPE_NONE           = 0,      /*!< No flags */
    M_IO_PIPE_INHERIT_READ   = 1 << 0, /*!< Read handle can be inherited by child */
    M_IO_PIPE_INHERIT_WRITE  = 1 << 1  /*!< Write handle can be inherited by child */
} M_io_pipe_flags_t;


/*! Create a pipe object
 *
 * \param[in]  flags  One or more M_io_pipe_flags_t flags controlling pipe creation
 * \param[out] reader io object for reading.
 * \param[out] writer io object for writing.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_pipe_create(M_uint32 flags, M_io_t **reader, M_io_t **writer);

/*! @} */

__END_DECLS

#endif

