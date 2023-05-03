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
#include "mstdlib/thread/m_thread_system.h"
#include "m_event_int.h"
#include <poll.h>


struct M_event_data {
	M_bool         evhandles_changed;
	int            retval;
	struct pollfd *fds;
	nfds_t         num_fds;
};


static void M_event_impl_poll_data_free(M_event_data_t *data)
{
	if (data == NULL)
		return;

	M_free(data->fds);
	M_free(data);
}


static void M_event_impl_poll_data_structure(M_event_t *event)
{
	size_t               num;
	M_hash_u64vp_enum_t *hashenum = NULL;
	M_event_evhandle_t  *member   = NULL;

	if (event->u.loop.impl_data != NULL && !event->u.loop.impl_data->evhandles_changed)
		return;

	if (event->u.loop.impl_data != NULL) {
		M_event_impl_poll_data_free(event->u.loop.impl_data);
		event->u.loop.impl_data = NULL;
	}
	num      = M_hash_u64vp_num_keys(event->u.loop.evhandles);

	event->u.loop.impl_data      = M_malloc_zero(sizeof(*event->u.loop.impl_data));
	event->u.loop.impl_data->fds = M_malloc_zero(sizeof(*event->u.loop.impl_data->fds) * num);

	M_hash_u64vp_enumerate(event->u.loop.evhandles, &hashenum);
	while (M_hash_u64vp_enumerate_next(event->u.loop.evhandles, hashenum, NULL, (void **)&member)) {
		/* Event if we're not waiting on real events, we want to wait on POLLHUP, so
		 * still add it to the fd list ... meaning don't do the below ...
		 *   if (member->waittype == 0)
		 *      continue;
		 */

		event->u.loop.impl_data->fds[event->u.loop.impl_data->num_fds].fd = member->handle;

#ifdef POLLRDHUP
		event->u.loop.impl_data->fds[event->u.loop.impl_data->num_fds].events |= POLLRDHUP;
#endif

		if (member->waittype & M_EVENT_WAIT_READ)
			event->u.loop.impl_data->fds[event->u.loop.impl_data->num_fds].events |= POLLIN;
		if (member->waittype & M_EVENT_WAIT_WRITE)
			event->u.loop.impl_data->fds[event->u.loop.impl_data->num_fds].events |= POLLOUT;

		/* If capabilities for the connection are write-only, we need to always listedn for POLLIN
		 * to be notified of disconnects for some reason */
		if (member->caps & M_EVENT_CAPS_WRITE) {
			event->u.loop.impl_data->fds[event->u.loop.impl_data->num_fds].events |= POLLIN;
		}

		event->u.loop.impl_data->num_fds++;
	}

	M_hash_u64vp_enumerate_free(hashenum);
}

/*! Wait for events with timeout in milliseconds.
 *  \param data       implementation-specific data
 *  \param timeout_ms Timeout in milliseconds.  -1 for infinite,
 *                    0 returns immediately after checking events,
 *                    >0 milliseconds to wait
 *  \return M_TRUE if events were available, M_FALSE if timeout
 */
static M_bool M_event_impl_poll_wait(M_event_t *event, M_uint64 timeout_ms)
{
	M_bool is_inf = (timeout_ms == M_TIMEOUT_INF)?M_TRUE:M_FALSE;
	if (timeout_ms > M_INT32_MAX)
		timeout_ms = M_INT32_MAX;

	event->u.loop.impl_data->retval = M_thread_poll(event->u.loop.impl_data->fds, event->u.loop.impl_data->num_fds,
	                                                (is_inf)?-1:(int)timeout_ms);
	if (event->u.loop.impl_data->retval > 0) {
		return M_TRUE;
	}
	return M_FALSE;
}


static void M_event_impl_poll_process(M_event_t *event)
{
	size_t i;
	size_t processed = 0;

	if (event->u.loop.impl_data->retval <= 0)
		return;

	/* Process events */
	for (i=0; i<event->u.loop.impl_data->num_fds; i++) {
		M_bool stop_writing = M_FALSE;
		if (event->u.loop.impl_data->fds[i].revents) {
			M_event_evhandle_t     *member  = NULL;
			if (!M_hash_u64vp_get(event->u.loop.evhandles, (M_uint64)event->u.loop.impl_data->fds[i].fd, (void **)&member))
				continue;

			/* Read */
			if (event->u.loop.impl_data->fds[i].revents & (POLLPRI|POLLIN)) {
				if (member->caps & M_EVENT_CAPS_READ) {
					M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
				}
			}

			/* Error */
			if (event->u.loop.impl_data->fds[i].revents & (POLLERR|POLLNVAL)) {
				stop_writing = M_TRUE;

				/* NOTE: always deliver READ event first on an error to make sure any
				 *       possible pending data is flushed. */
				if (member->waittype & M_EVENT_WAIT_READ) {
					M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
				}

				/* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
				 * it will do a partial read if there is still data buffered, and not ever attempt
				 * to read again.   We do this as a soft event as it is delivered after processing
				 * of normal events.  We don't know why this is necessary as it is very hard to
				 * reproduce outside of a PRODUCTION environment!
				 * NOTE: if not waiting on a READ event, deliver the real error */
				M_event_deliver_io(event, member->io, M_EVENT_TYPE_ERROR);
			}

			/* Disconnect */
			if (event->u.loop.impl_data->fds[i].revents & (POLLHUP
#ifdef POLLRDHUP
			      | POLLRDHUP
#endif
			    )) {
				stop_writing = M_TRUE;

				/* NOTE: always deliver READ event first on a disconnect to make sure any
				 *       possible pending data is flushed. */
				if (member->waittype & M_EVENT_WAIT_READ) {
					M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
				}

				/* Enqueue a softevent for a READ on a disconnect or ERROR as otherwise
				 * it will do a partial read if there is still data buffered, and not ever attempt
				 * to read again.   We do this as a soft event as it is delivered after processing
				 * of normal events.  We don't know why this is necessary as it is very hard to
				 * reproduce outside of a PRODUCTION environment!
				 * NOTE: if not waiting on a READ event, deliver the real error */
				M_event_deliver_io(event, member->io, M_EVENT_TYPE_DISCONNECTED);
			}

			/* Write */
			if (event->u.loop.impl_data->fds[i].revents & (POLLOUT|POLLWRBAND) && !stop_writing) {
				M_event_deliver_io(event, member->io, M_EVENT_TYPE_WRITE);
			}
			event->u.loop.impl_data->fds[i].revents = 0;
			processed++;
		}

		/* Optimization: No need to keep on scanning, we processed all available events */
		if (processed == (size_t)event->u.loop.impl_data->retval)
			break;
	}
}


static void M_event_impl_poll_modify_event(M_event_t *event, M_event_modify_type_t modtype, M_EVENT_HANDLE handle, M_event_wait_type_t waittype, M_event_caps_t caps)
{
	(void)modtype;
	(void)handle;
	(void)waittype;
	(void)caps;

	if (event->u.loop.impl_data)
		event->u.loop.impl_data->evhandles_changed = M_TRUE;
	M_event_wake(event);

}


struct M_event_impl_cbs M_event_impl_poll = {
	M_event_impl_poll_data_free,
	M_event_impl_poll_data_structure,
	M_event_impl_poll_wait,
	M_event_impl_poll_process,
	M_event_impl_poll_modify_event
};
