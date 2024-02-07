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
#include <mstdlib/mstdlib_io.h>
#include "m_event_int.h"
#include <sys/epoll.h>
#include <unistd.h>
#include "m_io_posix_common.h"

#define EPOLL_WAIT_EVENTS 64
struct M_event_data {
    int                epoll_fd;
    struct epoll_event events[EPOLL_WAIT_EVENTS];
    int                nevents;
};


static void M_event_impl_epoll_data_free(M_event_data_t *data)
{
    if (data == NULL)
        return;
    if (data->epoll_fd != -1)
        close(data->epoll_fd);
    M_free(data);
}


static void M_event_impl_epoll_modify_event(M_event_t *event, M_event_modify_type_t modtype, M_EVENT_HANDLE handle, M_event_wait_type_t waittype, M_event_caps_t caps)
{
    struct epoll_event ev;
    (void)modtype;
    (void)handle;
    (void)waittype;

    if (event->u.loop.impl_data == NULL)
        return;

    M_mem_set(&ev, 0, sizeof(ev));

    switch (modtype) {
        case M_EVENT_MODTYPE_ADD_HANDLE:
            ev.events = (M_uint32)EPOLLET;

            /* Only listen for write events if the endpoint is capable. */
            if (caps & M_EVENT_CAPS_WRITE)
                ev.events |= EPOLLOUT;

            /* Always listen for read events as for write-only pipes for some reason this is
             * how we are notified of a closure on the remote end, so no reason to look for
             * caps & M_EVENT_CAPS_READ */
            ev.events |= EPOLLIN;
#ifdef EPOLLRDHUP
            ev.events |= EPOLLRDHUP;
#endif

            ev.data.fd = handle;
            epoll_ctl(event->u.loop.impl_data->epoll_fd, EPOLL_CTL_ADD, handle, &ev);
            break;
        case M_EVENT_MODTYPE_DEL_HANDLE:
            epoll_ctl(event->u.loop.impl_data->epoll_fd, EPOLL_CTL_DEL, handle, &ev /* Can be NULL after kernel 2.6.9 */);
            break;
        default:
            return;
    }

}


static void M_event_impl_epoll_data_structure(M_event_t *event)
{
    M_hash_u64vp_enum_t *hashenum = NULL;
    M_event_evhandle_t  *member   = NULL;

    if (event->u.loop.impl_data != NULL)
        return;

    event->u.loop.impl_data            = M_malloc_zero(sizeof(*event->u.loop.impl_data));
#ifdef HAVE_EPOLL_CREATE1
    event->u.loop.impl_data->epoll_fd  = epoll_create1(EPOLL_CLOEXEC);
#else
    event->u.loop.impl_data->epoll_fd  = epoll_create(128 /* Bogus as of kernel 2.6.8 */);
    M_io_posix_fd_set_closeonexec(event->u.loop.impl_data->epoll_fd, M_TRUE);
#endif

    M_hash_u64vp_enumerate(event->u.loop.evhandles, &hashenum);
    while (M_hash_u64vp_enumerate_next(event->u.loop.evhandles, hashenum, NULL, (void **)&member)) {
        M_event_impl_epoll_modify_event(event, M_EVENT_MODTYPE_ADD_HANDLE, member->handle, member->waittype, member->caps);
    }
    M_hash_u64vp_enumerate_free(hashenum);
}


static M_bool M_event_impl_epoll_wait(M_event_t *event, M_uint64 timeout_ms)
{
    if (timeout_ms > M_INT32_MAX && timeout_ms != M_TIMEOUT_INF)
        timeout_ms = M_INT32_MAX;

    event->u.loop.impl_data->nevents = epoll_wait(event->u.loop.impl_data->epoll_fd, event->u.loop.impl_data->events,
                                                  EPOLL_WAIT_EVENTS, (timeout_ms == M_TIMEOUT_INF)?-1:(int)timeout_ms);
    if (event->u.loop.impl_data->nevents > 0) {
        return M_TRUE;
    }
    return M_FALSE;
}


static void M_event_impl_epoll_process(M_event_t *event)
{
    size_t i;

    if (event->u.loop.impl_data->nevents <= 0)
        return;

    /* Process events */
    for (i=0; i<(size_t)event->u.loop.impl_data->nevents; i++) {
        M_event_evhandle_t     *member  = NULL;
        if (!M_hash_u64vp_get(event->u.loop.evhandles, (M_uint64)event->u.loop.impl_data->events[i].data.fd, (void **)&member))
            continue;

        /* Error */
        if (event->u.loop.impl_data->events[i].events & EPOLLERR) {
            /* NOTE: always deliver READ event first on an error to make sure any
             *       possible pending data is flushed. */
            if (member->waittype & M_EVENT_WAIT_READ) {
                M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
            }
            /* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
             * it will do a partial read if there is still data buffered, and not ever attempt
             * to read again. */
            M_event_deliver_io(event, member->io, M_EVENT_TYPE_ERROR);
        }

        /* Read */
        if (event->u.loop.impl_data->events[i].events & EPOLLIN) {
            M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
        }

        /* Disconnect */
        if (event->u.loop.impl_data->events[i].events & (EPOLLHUP
#ifdef EPOLLRDHUP
            | EPOLLRDHUP
#endif
            )) {
            /* NOTE: always deliver READ event first on a disconnect to make sure any
             *       possible pending data is flushed. */
            if (member->waittype & M_EVENT_WAIT_READ) {
                M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
            }
            /* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
             * it will do a partial read if there is still data buffered, and not ever attempt
             * to read again. */
            M_event_deliver_io(event, member->io, M_EVENT_TYPE_DISCONNECTED);
        }

        /* Write */
        if (event->u.loop.impl_data->events[i].events & EPOLLOUT) {
            M_event_deliver_io(event, member->io, M_EVENT_TYPE_WRITE);
        }
    }
}


struct M_event_impl_cbs M_event_impl_epoll = {
    M_event_impl_epoll_data_free,
    M_event_impl_epoll_data_structure,
    M_event_impl_epoll_wait,
    M_event_impl_epoll_process,
    M_event_impl_epoll_modify_event
};
