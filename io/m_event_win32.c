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

/* Implementation:
 *   Windows doesn't support waiting on more than MAXIMUM_WAIT_OBJECTS (64),
 *   so we have to spawn threads to handle waiting on more than 64 objects.
 *   However, we have to reserve 1 object per thread to be used as a 'signal'
 *   to wake a thread when more events have been enqueued, or to stop waiting
 *   on events.
 *
 * Design:
 *   * The main event loop can wait on up to 63 events without spawning helper
 *     threads using WaitForMultipleObjects().  When more than 63 events need
 *     to be waited on, a new thread is spawned, and so on.  Only the main
 *     thread will timeout waiting on events based on the call by the main
 *     event loop, the helper threads will all wait indefinitely until a signal
 *     is delivered.
 *   * Threads will deliver their events to the main event loop rather than
 *     the threads delivering their events directly to user callbacks.  The
 *     main event loop is then responsible for the final delivery to the caller.
 *     This adds complexity and latency to the system, but if a user isn't
 *     expecting events to be delivered from different threads, that could lead
 *     to unexpected behavior (e.g. race conditions).
 *   * Synchronization must occur between these helper threads and the main
 *     thread event loop so that when the main thread is no longer waiting on
 *     events, the threads are asked to stop waiting as well.  If this
 *     synchronization step didn't occur, and an event handle was removed from
 *     the event loop prior to waiting on more events, those OS events would be
 *     lost.
 *   * For simplicity, the threadpool is only growable.
 *   * When an event handle is removed from event list, it will NOT wake up
 *     the event handler.  If the event handle is then deleted, then a
 *     WAIT_ABANDONED_0 will be emitted which will then just be ignored and
 *     will regenerate the wait list before sleeping again.
 *   * Must be able to handle the fact that a triggered event may no longer
 *     be for an event handle we own.  This could happen if delivery of a 
 *     prior event resulted in removal of a subsequent event handle.
 */
#include "m_config.h"
#include "mstdlib/mstdlib_io.h"
#include "m_event_int.h"
#include "m_io_int.h"
#include "base/m_defs_int.h"


#define TIMER_WAITABLE 1 /*!< Uses CreateWaitEvent() for timers */
#define TIMER_SETEVENT 2 /*!< Uses timeSetEvent() from the Multimedia Timers for timers, Microsoft says these are deprecated. */
#define TIMER_TIMEOUT  3 /*!< Uses the timeout parameter for WaitForMultipleObjects() for timers */
/*! Set the desired timer method to use */
#define TIMER_METHOD   TIMER_TIMEOUT

#if TIMER_METHOD == TIMER_SETEVENT
#  include <mmsystem.h>
#endif

struct M_event_win32_handle {
	M_EVENT_HANDLE  handle;     /*!< OS Event handle */
	size_t          thread_idx; /*!< Thread index in array of threads */
	M_llist_node_t *node;       /*!< Node in thread list of events */
};
typedef struct M_event_win32_handle M_event_win32_handle_t;


struct M_event_win32_thread {
	M_threadid_t     th_handle; /*!< Thread handle for joining */
	size_t           idx;       /*!< Thread index of self */
	M_event_data_t  *parent;    /*!< Pointer to main implementation handle                         */
	HANDLE           wake;      /*!< Event handle used to wake this thread while waiting on events */
	M_bool           changed;   /*!< Whether or not the event list for the thread has been modified */
	M_llist_t       *events;    /*!< List of events this thread should be waiting on               */
};
typedef struct M_event_win32_thread M_event_win32_thread_t;


enum M_event_win32_state {
	M_EVENT_WIN32_STATE_PREPARING,   /*!< Block until signalled that we can begin waiting for events */
	M_EVENT_WIN32_STATE_WAITEVENT,   /*!< Start waiting on events to be delivered                    */
	M_EVENT_WIN32_STATE_END          /*!< Exit all threads                                           */
};
typedef enum M_event_win32_state M_event_win32_state_t;


struct M_event_data {
	M_thread_mutex_t       *lock;
	M_thread_cond_t        *cond;
	M_event_win32_state_t   state;

	M_list_t               *threads;

	size_t                  num_threads_blocking;

	M_hashtable_t          *events;           /*!< List of registered events. Key is M_EVENT_HANDLE, value is M_event_win32_handle_t */
	M_list_t               *signalled;        /*!< List of M_EVENT_HANDLES triggered */

	M_uint64                timeout_ms;       /*!< Timeout for main event handler thread, possibly M_TIMEOUT_INF */
#if TIMER_METHOD == TIMER_WAITABLE
	HANDLE                  waittimer;
#elif TIMER_METHOD == TIMER_SETEVENT
	HANDLE                  waittimer;
	UINT                    timerhandle;
#endif
};


static void M_event_impl_win32_wakeall(M_event_data_t *data)
{
	if (data->state == M_EVENT_WIN32_STATE_WAITEVENT) {
		size_t          i;
		/* Iterate across all threads and trigger wake event */
		for (i=0; i<M_list_len(data->threads); i++) {
			const void             *ptr    = M_list_at(data->threads, i);
			M_event_win32_thread_t *thread = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);
			SetEvent(thread->wake);
		}
	}

	/* Wake up any threads blocking on the conditional.  We're
	 * going to always call this even if it doesn't seem necessary, mainly
	 * because the main event thread might be waiting for all threads to leave
	 * the blocking state. */
	M_thread_cond_broadcast(data->cond);
}


static void M_event_impl_win32_shutdownthreads(M_event_data_t *data)
{
	size_t i;

	M_thread_mutex_lock(data->lock);
	M_event_impl_win32_wakeall(data);
	data->state = M_EVENT_WIN32_STATE_END;
	while (data->num_threads_blocking) {
		M_thread_cond_wait(data->cond, data->lock);
	}
	M_thread_mutex_unlock(data->lock);

	/* Join each thread (except first) to wait on them to exit */
	for (i=1; i<M_list_len(data->threads); i++) {
		const void             *ptr    = M_list_at(data->threads, i);
		M_event_win32_thread_t *thread = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);

		M_thread_join(thread->th_handle, NULL);
	}
}


static void M_event_impl_win32_data_free(M_event_data_t *data)
{
	size_t i;
	
	if (data == NULL)
		return;

	M_event_impl_win32_shutdownthreads(data);
	
	for (i=0; i<M_list_len(data->threads); i++) {
		const void             *ptr    = M_list_at(data->threads, i);
		M_event_win32_thread_t *thread = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);
		CloseHandle(thread->wake);
		M_llist_destroy(thread->events, M_TRUE);
		M_free(thread);
	}
#if TIMER_METHOD == TIMER_WAITABLE || TIMER_METHOD == TIMER_SETEVENT
	CloseHandle(data->waittimer);
#endif
	M_list_destroy(data->threads, M_TRUE);
	M_hashtable_destroy(data->events, M_TRUE);
	M_list_destroy(data->signalled, M_TRUE);
	M_thread_mutex_destroy(data->lock);
	M_thread_cond_destroy(data->cond);
	M_free(data);
}


static void M_event_impl_win32_signal(M_event_data_t *data, M_EVENT_HANDLE handle)
{
	/* Enqueue the result into the parent's event list */
	M_list_insert(data->signalled, handle);

	/* Wake up any threads waiting on events if we're changing the state */
	if (data->state == M_EVENT_WIN32_STATE_WAITEVENT)
		M_event_impl_win32_wakeall(data);

	data->state = M_EVENT_WIN32_STATE_PREPARING;
}


static void *M_event_impl_win32_eventthread(void *arg)
{
	M_event_win32_thread_t *threaddata = arg;
	M_llist_node_t         *node;
	M_bool                  done       = M_FALSE;
	size_t                  i;
	size_t                  nhandles   = 0;
	HANDLE                 *handles    = NULL;
	DWORD                   retval;
	DWORD                   timeout;

	M_thread_mutex_lock(threaddata->parent->lock);
	do {
		switch (threaddata->parent->state) {
			case M_EVENT_WIN32_STATE_WAITEVENT:
				/* Structure event handles to wait on */
				if (threaddata->changed || handles == NULL) {
					M_free(handles);
					handles  = NULL;
					nhandles = 0;

					nhandles   = M_llist_len(threaddata->events) + 1;
#if TIMER_METHOD == TIMER_WAITABLE || TIMER_METHOD == TIMER_SETEVENT
					nhandles++;
#endif
					handles    = M_malloc_zero(sizeof(*handles) * nhandles);
					handles[0] = threaddata->wake;
					nhandles   = 1;
#if TIMER_METHOD == TIMER_WAITABLE || TIMER_METHOD == TIMER_SETEVENT
					handles[1] = threaddata->parent->waittimer;
					nhandles++;
#endif
					node       = M_llist_first(threaddata->events);
					for (; node != NULL; nhandles++) {
						M_event_win32_handle_t *thhandle;
						thhandle          = M_llist_node_val(node);
						handles[nhandles] = thhandle->handle;
						node              = M_llist_node_next(node);
					}
					threaddata->changed = M_FALSE;
				}

				/* Make sure wake handle isn't already triggered, could be duplicate events, then
				 * wait on events */
				ResetEvent(handles[0]);
				threaddata->parent->num_threads_blocking++;

				/* Synchronize again.  Otherwise the main thread could start processing events before
				 * the helper threads wake up and cause complete event starvation */
				if (M_list_len(threaddata->parent->threads) > 1) {
					/* Only synchronize if we really have more than just the parent */
					if (threaddata->parent->num_threads_blocking == M_list_len(threaddata->parent->threads)) {
						M_thread_cond_broadcast(threaddata->parent->cond);
					} else {
						M_thread_cond_wait(threaddata->parent->cond, threaddata->parent->lock);
					}
				}

				timeout = INFINITE;
				if (threaddata->idx == 0) {
#if TIMER_METHOD == TIMER_WAITABLE
					/* NOTE: WaitForMultipleObjects() timeout is only accurate to about 15ms.  It is possible
					 *       we could use CreateWaitableTimer() for higher-precision timeouts and always use
					 *       an INFINITE timeout parameter passed to WaitForMultipleObjects to improve the
					 *       accuracy .... of course assuming that is more accurate. */
					if (threaddata->parent->timeout_ms != M_TIMEOUT_INF && threaddata->parent->timeout_ms != 0) {
						LARGE_INTEGER liDueTime;
						/* Represented in 100ns intervals, negative offset means relative */
						liDueTime.QuadPart = -1 * threaddata->parent->timeout_ms * 10000;
						SetWaitableTimer(threaddata->parent->waittimer, &liDueTime, 0, NULL, NULL, 0);
					} else {
						CancelWaitableTimer(threaddata->parent->waittimer);
						if (threaddata->parent->timeout_ms == 0)
							timeout = 0;
					}
#elif TIMER_METHOD == TIMER_SETEVENT
					ResetEvent(threaddata->parent->waittimer);
					if (threaddata->parent->timeout_ms != M_TIMEOUT_INF && threaddata->parent->timeout_ms != 0) {
						threaddata->parent->timerhandle = timeSetEvent(threaddata->parent->timeout_ms, 0, threaddata->parent->waittimer, NULL, TIME_ONESHOT|TIME_CALLBACK_EVENT_SET|TIME_KILL_SYNCHRONOUS);
					} else if (threaddata->parent->timeout_ms == 0) {
						timeout = 0;
					}
#else
					if (threaddata->parent->timeout_ms != M_TIMEOUT_INF) {
						timeout = (DWORD)threaddata->parent->timeout_ms;
					}
#endif
				}

				M_thread_mutex_unlock(threaddata->parent->lock);

				retval = WaitForMultipleObjects((DWORD)nhandles, handles, FALSE, timeout);

				M_thread_mutex_lock(threaddata->parent->lock);
#if TIMER_METHOD == TIMER_SETEVENT
				if (threaddata->idx == 0 && threaddata->parent->timerhandle != 0) {
					/* See if timer event fired */
					if (WaitForSingleObject(threaddata->parent->waittimer, 0) != WAIT_OBJECT_0) {
						timeKillEvent(threaddata->parent->timerhandle);
					}
					threaddata->parent->timerhandle = 0;
				}
#endif

				threaddata->parent->num_threads_blocking--;
				/* If we just made us the last blocking, we need to signal the parent (if we're not the parent) to wake up so they can clean up */
				if (threaddata->parent->num_threads_blocking == 0 && threaddata->idx != 0 /* parent */) {
					M_thread_cond_broadcast(threaddata->parent->cond);
				}

				/* Process all events that were triggered */
				if (
#if 0 
/* WAIT_OBJECT_0 is defined as 0, wonder why they even define it?
 * Don't emit this code as it will just cause a warning.
 */
				    retval >= WAIT_OBJECT_0 &&
#endif
				    retval <= WAIT_OBJECT_0 + nhandles
				) {
					if (retval - WAIT_OBJECT_0 != 0)
						M_event_impl_win32_signal(threaddata->parent, handles[retval - WAIT_OBJECT_0]);
					/* More events might have been signaled, we need to iterate across all to see */
					for (i=1; i<nhandles; i++) {
						/* Don't re-evaluate handle we already did */
						if (i == (retval - WAIT_OBJECT_0))
							continue;

						if (WaitForSingleObject(handles[i], 0) != WAIT_OBJECT_0)
							continue;

						M_event_impl_win32_signal(threaddata->parent, handles[i]);
					}
				}

				break;
			case M_EVENT_WIN32_STATE_PREPARING:
				/* never valid to block here for the main thread */
				if (threaddata->idx == 0)
					break;
				/* Wake on thread signal to begin waiting (or to exit) */
				M_thread_cond_wait(threaddata->parent->cond, threaddata->parent->lock);
				break;
			case M_EVENT_WIN32_STATE_END:
				done = M_TRUE;
				break;
		}
	} while (!done && threaddata->idx != 0);
	M_thread_mutex_unlock(threaddata->parent->lock);
	/* Cleanup */
	M_free(handles);

	return NULL;
}


static M_event_win32_thread_t *M_event_win32_add_thread(M_event_data_t *data)
{
	M_event_win32_thread_t *thread;
	size_t                  idx;

	idx             = M_list_len(data->threads);
	thread          = M_malloc_zero(sizeof(*thread));
	thread->events  = M_llist_create(NULL, M_LLIST_NONE);
	thread->wake    = CreateEvent(NULL, FALSE, FALSE, NULL);
	thread->parent  = data;
	thread->idx     = idx;
	M_list_insert(data->threads, thread);

	/* First thread isn't a real thread, only spawn new threads for threads after the first */
	if (M_list_len(data->threads) > 1) {
		M_thread_attr_t *attr;
		attr              = M_thread_attr_create();
		M_thread_attr_set_create_joinable(attr, M_TRUE);
		thread->th_handle = M_thread_create(attr, M_event_impl_win32_eventthread, thread);
		M_thread_attr_destroy(attr);
	}

	return thread;
}


static void M_event_impl_win32_modify_event(M_event_t *event, M_event_modify_type_t modtype, M_EVENT_HANDLE handle, M_event_wait_type_t waittype, M_event_caps_t caps)
{
	M_event_data_t         *data     = event->u.loop.impl_data;
	M_event_win32_handle_t *evhandle = NULL;
	const void             *ptr;
	size_t                  idx;
	M_event_win32_thread_t *thread   = NULL;

	(void)waittype;
	(void)caps;

	if (!data)
		return;

	/* We're edge-triggered, no need to do anything */
	if (modtype == M_EVENT_MODTYPE_ADD_WAITTYPE || modtype == M_EVENT_MODTYPE_DEL_WAITTYPE)
		return;


	if (modtype == M_EVENT_MODTYPE_DEL_HANDLE) {
		M_thread_mutex_lock(data->lock);
		/* Find registered event */
		if (M_hashtable_get(data->events, handle, (void **)&evhandle) && evhandle != NULL) {
			ptr    = M_list_at(data->threads, evhandle->thread_idx);
			thread = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);
			/* Remove from Thread's event list */
			thread->changed = M_TRUE;
			(void)M_llist_take_node(evhandle->node);
			/* Remove from main event, will auto-free the evhandle when removed from the hashtable */
			M_hashtable_remove(data->events, handle, M_TRUE);
		}
		M_thread_mutex_unlock(data->lock);
		return;
	}


	/* modtype == M_EVENT_MODTYPE_ADD_HANDLE */
	M_thread_mutex_lock(data->lock);

	/* Locate thread index with sufficient space to add event handle */
	for (idx=0; idx<M_list_len(data->threads); idx++) {
		size_t reservedev;
		ptr        = M_list_at(data->threads, idx);
		thread     = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);
		reservedev = 1; /* Thread wake handle */
#if TIMER_METHOD == TIMER_WAITABLE || TIMER_METHOD == TIMER_SETEVENT
		/* Waittimer on thread 0 only */
		if (idx == 0)
			reservedev++;
#endif
		if (M_llist_len(thread->events)+reservedev < MAXIMUM_WAIT_OBJECTS)
			break;
	}

	/* See if we had room in a thread, if not, create another thread */
	if (idx == M_list_len(data->threads) || thread == NULL) {
		thread = M_event_win32_add_thread(data);
	}

	evhandle             = M_malloc_zero(sizeof(*evhandle));
	evhandle->handle     = handle;
	evhandle->thread_idx = idx;
	evhandle->node       = M_llist_insert(thread->events, evhandle);
	thread->changed      = M_TRUE;

	M_hashtable_insert(data->events, handle, evhandle);
	M_thread_mutex_unlock(data->lock);

	/* We need to wake since the event list changed */
	M_event_wake(event);
}


static void M_event_impl_win32_data_structure(M_event_t *event)
{
	M_hash_u64vp_enum_t *hashenum = NULL;
	M_event_evhandle_t  *member   = NULL;
	M_event_data_t      *data;
	struct M_hashtable_callbacks events_cbs = {
		NULL,  /* key_duplicate_insert */
		NULL,  /* key_duplicate_copy */
		NULL, /* key_free */
		NULL,  /* value_duplicate_insert */
		NULL,  /* value_duplicate_copy */
		NULL,  /* value_equality */
		M_free /* value_free */
	};

	if (event->u.loop.impl_data != NULL)
		return;

	data                     = M_malloc_zero(sizeof(*data));
	event->u.loop.impl_data  = data;
	data->lock               = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	data->cond               = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	data->state              = M_EVENT_WIN32_STATE_PREPARING;
	data->threads            = M_list_create(NULL, M_LIST_NONE);
	data->events             = M_hashtable_create(16, 75, M_hash_func_hash_vp, M_sort_compar_vp, M_HASHTABLE_NONE, &events_cbs);
	data->signalled          = M_list_create(NULL, M_LIST_NONE);
#if TIMER_METHOD == TIMER_WAITABLE
	data->waittimer          = CreateWaitableTimer(NULL, TRUE, NULL);
#elif TIMER_METHOD == TIMER_SETEVENT
	data->waittimer          = CreateEvent(NULL, TRUE, FALSE, NULL);
	data->timerhandle        = 0;
#endif
	/* Create thread 0, which isn't really a thread at all */
	M_event_win32_add_thread(data);

	M_hash_u64vp_enumerate(event->u.loop.evhandles, &hashenum);
	while (M_hash_u64vp_enumerate_next(event->u.loop.evhandles, hashenum, NULL, (void **)&member)) {
		M_event_impl_win32_modify_event(event, M_EVENT_MODTYPE_ADD_HANDLE, member->handle, member->waittype, member->caps);
	}
	M_hash_u64vp_enumerate_free(hashenum);
}


static M_bool M_event_impl_win32_wait(M_event_t *event, M_uint64 timeout_ms)
{
	M_event_data_t         *data   = event->u.loop.impl_data;
	M_bool                  retval = M_FALSE;
	M_event_win32_thread_t *parent_thread;
	const void             *ptr;

	data->timeout_ms       = timeout_ms;
	/* Signal all threads to start waiting for events */
	M_thread_mutex_lock(data->lock);
	M_event_impl_win32_wakeall(data);
	data->state = M_EVENT_WIN32_STATE_WAITEVENT;
	M_thread_mutex_unlock(data->lock);

	/* Main thread should now start waiting on events, will return when woken up */
	ptr           = M_list_at(data->threads, 0);
	parent_thread = M_CAST_OFF_CONST(M_event_win32_thread_t *, ptr);
	M_event_impl_win32_eventthread(parent_thread);

	/* Signal threads if necessary and wait for them to finish */
	M_thread_mutex_lock(data->lock);
	if (data->state == M_EVENT_WIN32_STATE_WAITEVENT) {
		/* This should only be true if a timeout occurred */
		M_event_impl_win32_wakeall(data);
		data->state = M_EVENT_WIN32_STATE_PREPARING;
	}

	while (data->num_threads_blocking) {
		M_thread_cond_wait(data->cond, data->lock);
	}

	if (M_list_len(data->signalled))
		retval = M_TRUE;

	M_thread_mutex_unlock(data->lock);

	return retval;
}


static void M_event_impl_win32_process(M_event_t *event)
{
	M_event_data_t  *data   = event->u.loop.impl_data;
	M_EVENT_HANDLE   handle;
	WSANETWORKEVENTS NetEvents;

	/* NOTE: shouldn't need to lock as we should be guaranteed that there will
	 *       be no modifications to data->signalled since all threads are blocking */
	while ((handle = M_list_take_first(data->signalled)) != NULL) {
		/* Lets look up the metadata about this event handle so we can rewrite it 
		 * appropriately */
		M_event_evhandle_t     *member  = NULL;
		M_event_type_t          type    = M_EVENT_TYPE_OTHER;

		if (!M_hash_u64vp_get(event->u.loop.evhandles, (M_uint64)((M_uintptr)handle), (void **)&member))
			continue;

		if (member->sock == M_EVENT_INVALID_SOCKET) {
			/* If event registered only with read capability, then this *must* be a read event */
			if (member->caps == M_EVENT_CAPS_READ) {
				type = M_EVENT_TYPE_READ;
			}
			/* If event registered only with write capability, then this *must* be a write event */
			if (member->caps == M_EVENT_CAPS_WRITE) {
				type = M_EVENT_TYPE_WRITE;
			}
			M_event_deliver_io(event, member->io, type);
			continue;
		}

		/* Enumerate network events since we have a socket */

		M_mem_set(&NetEvents, 0, sizeof(NetEvents));

		if (WSAEnumNetworkEvents(member->sock, handle, &NetEvents) != 0) {
			/* Error enumerating events, skip */
			continue;
		}

		/* Treat ACCEPT, READ, and ERROR events as READ events */
		if (NetEvents.lNetworkEvents & (FD_ACCEPT|FD_READ)) {
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);
		}

		/* Send Disconnect or Error as if it was a READ event and let the read
		 * get the real error code.  Reason for this is we've seen where there is
		 * data available, but we get an FD_CLOSE event rather than a read event */
		if (NetEvents.lNetworkEvents & FD_CLOSE) {
			/* NetEvents.iErrorCode[FD_CLOSE_BIT] == 0 is disconnect, non-zero is error,
			 * could use the value as last_error_sys */
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_READ);

			/* Enqueue a softevent for a disconnect (or READ for ERROR) as otherwise
			 * it will do a partial read if there is still data buffered, and not ever attempt
			 * to read again.   We do this as a soft event as it is delivered after processing
			 * of normal events.  We tried using M_event_deliver_io() again instead and it didn't
			 * work ... at least for blocking i/o */
			M_io_softevent_add(member->io, 0, (NetEvents.iErrorCode[FD_CLOSE_BIT] == 0)?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_READ);
		}

		/* Treat CONNECT and WRITE events as WRITE events */
		if (NetEvents.lNetworkEvents & (FD_CONNECT|FD_WRITE)) {
			M_event_deliver_io(event, member->io, M_EVENT_TYPE_WRITE);
		}
	}
}


struct M_event_impl_cbs M_event_impl_win32 = {
	M_event_impl_win32_data_free,
	M_event_impl_win32_data_structure,
	M_event_impl_win32_wait,
	M_event_impl_win32_process,
	M_event_impl_win32_modify_event
};
