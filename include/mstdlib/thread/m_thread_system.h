/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

#ifndef __M_THREAD_SYSTEM_H__
#define __M_THREAD_SYSTEM_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

#ifdef _WIN32
#  include <Winsock2.h>
#  include <Windows.h>
#  if _WIN32_WINNT < 0x0600
    struct pollfd {
        SOCKET fd;       /* file descriptor */
        short  events;   /* events to look for */
        short  revents;  /* events returned */
    };
#     define POLLIN          0x0001          /* any readable data available */
#     define POLLOUT         0x0004          /* file descriptor is writeable */
#     define POLLERR         0x0008          /* some poll error occurred */
#     define POLLHUP         0x0010          /* file descriptor was "hung up" */
#     define POLLNVAL        0x0020          /* requested events "invalid" */
#  endif
    typedef unsigned int nfds_t;
#else
/* Need to include struct pollfd for M_thread_poll() */
#  include <poll.h>
/* Needed for M_thread_sigmask() */
#  include <signal.h>
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_thread_system Thread - System Specific
 *  \ingroup    m_thread_common
 *
 * Low level threading functionality. These are only provided due to fundamental and
 * irreconcilable differences that they cannot be provided in a platform agnostic manner.
 *
 * @{
 */

/*! Monitor a file descriptor waiting for it to become ready for I/O operations.
 *
 * \param[in] fds       Array of FDs to monitor with monitoring flags
 * \param[in] nfds      Number of file descriptors in array.
 * \param[in] timeout   How long to wait before giving up in ms. -1 = infinite
 *
 * \return The number of ready fds, 0 if timeout, -1 on error.
 */
M_API int M_thread_poll(struct pollfd fds[], nfds_t nfds, int timeout);

#ifndef _WIN32
/*! Examine and change blocked signals.
 *
 * \param[in] how    How to change signal behavior. Values are
 *                     - SIG_BLOCK
 *                     - SIG_UNBLOCK
 *                     - SIG_SETMASK
 * \param[in] set    What signals to set using how. Optional, NULL if only getting current signals from oldset.
 * \param[in] oldset Previous signal status. Optional, NULL if only setting signals.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_thread_sigmask(int how, const sigset_t *set, sigset_t *oldset);
#endif

/*! @} */

__END_DECLS

#endif /* __M_THREAD_SYSTEM_H__ */
