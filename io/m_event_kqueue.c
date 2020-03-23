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
#include "mstdlib/mstdlib_io.h"
#include "m_event_int.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include "m_io_posix_common.h"

#define KQUEUE_WAIT_EVENTS 64
struct M_event_data {
	int           kqueue_fd;
	struct kevent events[KQUEUE_WAIT_EVENTS];
	int           nevents;
};


static void M_event_impl_kqueue_data_free(M_event_data_t *data)
{
	if (data == NULL)
		return;
	if (data->kqueue_fd != -1)
		close(data->kqueue_fd);
	M_free(data);
}


static void M_event_impl_kqueue_modify_event(M_event_t *event, M_event_modify_type_t modtype, M_EVENT_HANDLE handle, M_event_wait_type_t waittype, M_event_caps_t caps)
{
	struct kevent ev[2];
	int           nev = 0;
	(void)modtype;
	(void)handle;
	(void)waittype;

	if (event->u.loop.impl_data == NULL)
		return;

	switch (modtype) {
		case M_EVENT_MODTYPE_ADD_HANDLE:
			if (caps & M_EVENT_CAPS_READ) {
				/* NOTE: EV_CLEAR sets edge-triggered instead of level-triggered */
				EV_SET(&ev[0], handle, EVFILT_READ,  EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);
				nev++;
			}
			if (caps & M_EVENT_CAPS_WRITE) {
				/* NOTE: EV_CLEAR sets edge-triggered instead of level-triggered */
				EV_SET(&ev[1], handle, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);
				nev++;
			}
			break;
		case M_EVENT_MODTYPE_DEL_HANDLE:
			EV_SET(&ev[0], handle, EVFILT_READ,  EV_DELETE, 0, 0, NULL);
			EV_SET(&ev[1], handle, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
			nev = 2;
			break;
		default:
			return;
	}
	kevent(event->u.loop.impl_data->kqueue_fd, ev, nev, NULL, 0, NULL);
}


static void M_event_impl_kqueue_data_structure(M_event_t *event)
{
	M_hash_u64vp_enum_t *hashenum = NULL;
	M_event_evhandle_t  *member   = NULL;

	if (event->u.loop.impl_data != NULL)
		return;

	event->u.loop.impl_data            = M_malloc_zero(sizeof(*event->u.loop.impl_data));
	event->u.loop.impl_data->kqueue_fd = kqueue();
	M_io_posix_fd_set_closeonexec(event->u.loop.impl_data->kqueue_fd, M_TRUE);

	M_hash_u64vp_enumerate(event->u.loop.evhandles, &hashenum);
	while (M_hash_u64vp_enumerate_next(event->u.loop.evhandles, hashenum, NULL, (void **)&member)) {
		M_event_impl_kqueue_modify_event(event, M_EVENT_MODTYPE_ADD_HANDLE, member->handle, member->waittype, member->caps);
	}
	M_hash_u64vp_enumerate_free(hashenum);
}


static M_bool M_event_impl_kqueue_wait(M_event_t *event, M_uint64 timeout_ms)
{
	struct timespec timeout;

	if (timeout_ms != M_TIMEOUT_INF) {
		timeout.tv_sec  = timeout_ms / 1000;
		timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
	}

	event->u.loop.impl_data->nevents = kevent(event->u.loop.impl_data->kqueue_fd, NULL, 0,
	                                          event->u.loop.impl_data->events, KQUEUE_WAIT_EVENTS,
	                                          (timeout_ms != M_TIMEOUT_INF)?&timeout:NULL);

	if (event->u.loop.impl_data->nevents > 0) {
		return M_TRUE;
	}
	return M_FALSE;
}


static void M_event_impl_kqueue_process(M_event_t *event)
{
	size_t i;

	if (event->u.loop.impl_data->nevents <= 0)
		return;

	/* Process events */
	for (i=0; i<(size_t)event->u.loop.impl_data->nevents; i++) {
		M_event_evhandle_t     *member  = NULL;
		if (!M_hash_u64vp_get(event->u.loop.evhandles, (M_uint64)event->u.loop.impl_data->events[i].ident, (void **)&member))
			continue;

		/* Disconnect */
		if (event->u.loop.impl_data->events[i].flags & EV_EOF) {
			/* NOTE: always deliver READ event first on a disconnect to make sure any
			 *       possible pending data is flushed. */
			if (member->waittype & M_EVENT_WAIT_READ) {
				M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
			}
			/* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
			 * it will do a partial read if there is still data buffered, and not ever attempt
			 * to read again.  */
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_DISCONNECTED);
			continue;
		}

		/* Error */
		if (event->u.loop.impl_data->events[i].flags & EV_ERROR) {
			/* NOTE: always deliver READ event first on an error to make sure any
			 *       possible pending data is flushed. */
			if (member->waittype & M_EVENT_WAIT_READ) {
				M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
			}

			/* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
			 * it will do a partial read if there is still data buffered, and not ever attempt
			 * to read again.  */
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_ERROR);
			continue;
		}

		/* Read */
		if (event->u.loop.impl_data->events[i].filter == EVFILT_READ) {
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
			continue;
		}

		/* Write */
		if (event->u.loop.impl_data->events[i].filter == EVFILT_WRITE) {
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_WRITE);
			continue;
		}
	}
}


struct M_event_impl_cbs M_event_impl_kqueue = {
	M_event_impl_kqueue_data_free,
	M_event_impl_kqueue_data_structure,
	M_event_impl_kqueue_wait,
	M_event_impl_kqueue_process,
	M_event_impl_kqueue_modify_event
};
