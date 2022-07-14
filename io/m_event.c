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
#include "m_io_int.h"
#include <mstdlib/io/m_io_layer.h>
#include "base/m_defs_int.h"

const char *M_event_type_string(M_event_type_t type)
{
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			return "CONNECTED";
		case M_EVENT_TYPE_ACCEPT:
			return "ACCEPT";
		case M_EVENT_TYPE_READ:
			return "READ";
		case M_EVENT_TYPE_DISCONNECTED:
			return "DISCONNECTED";
		case M_EVENT_TYPE_ERROR:
			return "ERROR";
		case M_EVENT_TYPE_WRITE:
			return "WRITE";
		case M_EVENT_TYPE_OTHER:
			return "OTHER";
	}
	return "UNKNOWN";
}

static void M_event_io_unregister(M_io_t *comm, M_bool in_destructor)
{
	size_t     i;
	size_t     num;

	if (comm->reg_event == NULL) {
		return;
	}

	num = M_list_len(comm->layer);
	for (i=0; i<num; i++) {
		M_io_layer_t *layer = M_io_layer_at(comm, i);
		if (layer->cb.cb_unregister == NULL)
			continue;
		layer->cb.cb_unregister(layer);
	}

	if (!in_destructor) {
		/* Kill any soft events. But not if in a destructor as this might
		 * cause a bad recursive operation */
		M_io_softevent_clearall(comm, M_FALSE);
	}

	comm->reg_event = NULL;

	/* Event IO objects are *private* to the event handle, so clean them up */
	if (in_destructor && comm->type == M_IO_TYPE_EVENT) {
		M_io_destroy(comm);
	}
}


/*! Unregister COMM from callback on destroy */
static void M_event_evhandle_unregister_cb(void *arg)
{
	M_io_t *comm = arg;
	M_event_io_unregister(comm, M_TRUE);
}


static void M_event_loop_init(M_event_t *event, M_uint32 flags)
{
	struct M_hashtable_callbacks member_cbs = {
		NULL,  /* key_duplicate_insert */
		NULL,  /* key_duplicate_copy */
		M_event_evhandle_unregister_cb, /* key_free */
		NULL,  /* value_duplicate_insert */
		NULL,  /* value_duplicate_copy */
		NULL,  /* value_equality */
		M_free /* value_free */
	};
	struct M_llist_callbacks softevent_cbs = {
		NULL,  /* equality */
		NULL,  /* duplicate_insert */
		NULL,  /* duplicate_copy */
		M_free /* value_free */
	};
	M_thread_model_t threadmodel;

	event->type                 = M_EVENT_BASE_TYPE_LOOP;
	event->u.loop.lock          = M_thread_mutex_create(M_THREAD_MUTEXATTR_RECURSIVE);
	event->u.loop.flags         = flags;
	event->u.loop.status        = M_EVENT_STATUS_PAUSED;

	/* On destroy, this will auto-unregister all registered M_io_t * objects */
	event->u.loop.reg_ios       = M_hashtable_create(16, 72, M_hash_func_hash_vp, M_sort_compar_vp, M_HASHTABLE_NONE, &member_cbs);

	event->u.loop.evhandles     = M_hash_u64vp_create(16, 72, M_HASH_U64VP_NONE, NULL);

	/* On destroy, this will auto-free any soft event handles left over */
	event->u.loop.soft_events   = M_llist_create(&softevent_cbs, M_LLIST_NONE);

#if defined(_WIN32)
	event->u.loop.impl          = &M_event_impl_win32;
#elif defined(HAVE_KQUEUE)
	if (flags & M_EVENT_FLAG_NON_SCALABLE) {
		event->u.loop.impl      = &M_event_impl_poll;
	} else {
		event->u.loop.impl      = &M_event_impl_kqueue;
	}
#elif defined(HAVE_EPOLL)
	if (flags & M_EVENT_FLAG_NON_SCALABLE) {
		event->u.loop.impl      = &M_event_impl_poll;
	} else {
		event->u.loop.impl      = &M_event_impl_epoll;
	}
#else
	event->u.loop.impl          = &M_event_impl_poll;
#endif

	M_thread_active_model(&threadmodel, NULL);

#if !defined(_WIN32)
	/* Coop threads always use poll, even if better alternatives are available */
	if (threadmodel == M_THREAD_MODEL_COOP) {
		event->u.loop.impl = &M_event_impl_poll;
	}
#endif

	if (!(event->u.loop.flags & M_EVENT_FLAG_NOWAKE))
		event->u.loop.parent_wake = M_io_osevent_create(event);
}


M_event_t *M_event_create(M_uint32 flags)
{
	M_event_t *event = M_malloc_zero(sizeof(*event));
	M_event_loop_init(event, flags);

	return event;
}


M_event_t *M_event_pool_create(size_t max_threads)
{
	size_t     num_threads;
	size_t     i;
	M_event_t *event;

	if (max_threads == 0)
		max_threads = SIZE_MAX;

	num_threads = M_MIN(M_thread_num_cpu_cores(), max_threads);
	if (num_threads == 0)
		num_threads = 1;

	/* If there's only one core, we won't create a pool */
	if (num_threads == 1)
		return M_event_create(M_EVENT_FLAG_NONE);

	event                       = M_malloc_zero(sizeof(*event));
	event->type                 = M_EVENT_BASE_TYPE_POOL;
	event->u.pool.thread_count  = num_threads;
	event->u.pool.thread_ids    = M_malloc_zero(sizeof(*event->u.pool.thread_ids)    * num_threads);
	event->u.pool.thread_evloop = M_malloc_zero(sizeof(*event->u.pool.thread_evloop) * num_threads);
	for (i=0; i<num_threads; i++) {
		M_event_loop_init(&event->u.pool.thread_evloop[i], M_EVENT_FLAG_NONE);
		event->u.pool.thread_evloop[i].u.loop.parent = event;
	}

	return event;
}


static void M_event_destroy_loop(M_event_t *event)
{
	M_event_lock(event);

	if (event->u.loop.parent_wake)
		M_io_destroy(event->u.loop.parent_wake);
	event->u.loop.parent_wake          = NULL;

	/* Should unregister self from every registered COMM handle */
	M_hashtable_destroy(event->u.loop.reg_ios, M_TRUE);
	event->u.loop.reg_ios              = NULL;

	/* Should have been automatically destroyed by the above! */
	event->u.loop.parent_wake          = NULL;

	/* At this point, there really shouldn't be any registered handles left */
	M_hash_u64vp_destroy(event->u.loop.evhandles, M_TRUE);
	event->u.loop.evhandles            = NULL;

	/* Should auto-destroy any lingering soft event handles automatically */
	M_llist_destroy(event->u.loop.soft_events, M_TRUE);
	event->u.loop.soft_events          = NULL;

	/* Should auto-destroy any lingering timer handles automatically */
	M_queue_destroy(event->u.loop.timers);
	event->u.loop.timers        = NULL;

	if (event->u.loop.impl_data != NULL) {
		if (event->u.loop.impl->data_free != NULL) {
			event->u.loop.impl->data_free(event->u.loop.impl_data);
		}
		event->u.loop.impl_data = NULL;
	}

	M_event_unlock(event);
	M_thread_mutex_destroy(event->u.loop.lock);
}

void M_event_lock(M_event_t *event)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP) {
		return;
	}
//M_printf("%s(): [%p] %p\n", __FUNCTION__, (void *)M_thread_self(), event);
	M_thread_mutex_lock(event->u.loop.lock);
//M_printf("%s(): LOCKED [%p] %p\n", __FUNCTION__, (void *)M_thread_self(), event);  fflush(stdout);
}

void M_event_unlock(M_event_t *event)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP) {
		return;
	}
//M_printf("%s(): [%p] %p\n",  __FUNCTION__, (void *)M_thread_self(), event); fflush(stdout);
	M_thread_mutex_unlock(event->u.loop.lock);
}

void M_event_destroy(M_event_t *event)
{
	if (event == NULL)
		return;

	/* MISUSE! */
	if (M_event_get_status(event) == M_EVENT_STATUS_RUNNING)
		return;

	if (event->type == M_EVENT_BASE_TYPE_LOOP) {
		M_event_destroy_loop(event);
	} else {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++) {
			M_event_destroy_loop(&event->u.pool.thread_evloop[i]);
		}
		M_free(event->u.pool.thread_evloop);
		M_free(event->u.pool.thread_ids);
	}

	M_free(event);
}


void M_io_set_error(M_io_t *io, M_io_error_t err)
{
	if (io == NULL)
		return;

	/* Save first error, or replace with more specific error than ERROR */
	if (err != M_IO_ERROR_SUCCESS && (io->last_error == M_IO_ERROR_SUCCESS || io->last_error == M_IO_ERROR_ERROR))
		io->last_error = err;
}


void M_io_softevent_add(M_io_t *io, size_t layer_id, M_event_type_t type, M_io_error_t err)
{
	M_event_t            *event  = M_io_get_event(io);
	M_event_io_t         *ioev   = NULL;
	M_event_softevent_t  *softevent;

	/* Its possible someone could try to reference an io object that is not currently
	 * associated with an event object.  In which case, we just ignore this request */
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	M_event_lock(event);

	if (layer_id >= M_io_layer_count(io) + 1 /* User layer */)
		goto done;


	if (M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev)) {
		M_uint16 ev;
		if (ioev->softevent_node == NULL) {
			softevent            = M_malloc_zero(sizeof(*softevent));
			softevent->io        = io;
			ioev->softevent_node = M_llist_insert(event->u.loop.soft_events, softevent);
		} else {
			softevent            = M_llist_node_val(ioev->softevent_node);
		}
		ev = (M_uint16)(1 << type);
		softevent->events[layer_id] |= ev;
//M_printf("%s(): softevent io %p layer %zu type %d\n", __FUNCTION__, io, layer_id, (int)type);
	} else {
//M_printf("%s(): WARN: added softevent of io %p that does not exist\n", __FUNCTION__, io);
	}

	/* Save first error, or replace with more specific error than ERROR */
	M_io_set_error(io, err);

	/* If the event loop is sleeping and this was sent from another thread, wake it */
	M_event_wake(event);

done:
	M_event_unlock(event);

}

static M_bool M_io_layer_softevent_is_empty(M_io_t *io, M_event_softevent_t *softevent)
{
	size_t num_layers = M_io_layer_count(io) + 1;
	size_t i;

	for (i=0; i<num_layers; i++) {
		if (softevent->events[i] != 0)
			return M_FALSE;
	}
	return M_TRUE;
}

void M_io_layer_softevent_add(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type, M_io_error_t err)
{
	M_io_t               *io    = M_io_layer_get_io(layer);
	size_t                id    = M_io_layer_get_index(layer);

	if (sibling_only)
		id++;

	M_io_softevent_add(io, id, type, err);
}


void M_io_user_softevent_add(M_io_t *io, M_event_type_t type, M_io_error_t err)
{
	M_io_softevent_add(io, M_io_layer_count(io), type, err);
}


static void M_io_softevent_clear(M_io_t *io, size_t layer_id)
{
	M_event_t            *event = M_io_get_event(io);
	M_event_io_t         *ioev  = NULL;
	M_event_softevent_t  *softevent;

	if (layer_id >= M_io_layer_count(io) + 1 /* User layer */)
		return;

	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	M_event_lock(event);

	if (M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev)) {
		if (ioev->softevent_node != NULL) {
			softevent                   = M_llist_node_val(ioev->softevent_node);
			softevent->events[layer_id] = 0;
			if (M_io_layer_softevent_is_empty(io, softevent)) {
				M_llist_remove_node(ioev->softevent_node);
				ioev->softevent_node = NULL;
			}
		}
	}

	M_event_unlock(event);
}


void M_io_layer_softevent_clear(M_io_layer_t *layer)
{
	M_io_t *io    = M_io_layer_get_io(layer);
	size_t  id    = M_io_layer_get_index(layer);

	M_io_softevent_clear(io, id);
}


void M_io_softevent_clearall(M_io_t *io, M_bool nonerror_only)
{
	M_event_t            *event = M_io_get_event(io);
	M_event_io_t         *ioev  = NULL;
//M_printf("%s(): io = %p\n", __FUNCTION__, io);
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	M_event_lock(event);

	if (M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev)) {
		if (ioev->softevent_node != NULL) {
			M_event_softevent_t  *softevent = M_llist_node_val(ioev->softevent_node);
			size_t                num       = M_io_layer_count(io);
			size_t                layer_id;

			if (softevent) {
				for (layer_id=0; layer_id <= num /* <= as num is user layer */; layer_id++) {
					if (nonerror_only) {
						softevent->events[layer_id]    &= (M_uint16)((~(1 << M_EVENT_TYPE_CONNECTED)) & 0xFFFF);
						softevent->events[layer_id]    &= (M_uint16)((~(1 << M_EVENT_TYPE_ACCEPT))    & 0xFFFF);
						softevent->events[layer_id]    &= (M_uint16)((~(1 << M_EVENT_TYPE_READ))      & 0xFFFF);
						softevent->events[layer_id]    &= (M_uint16)((~(1 << M_EVENT_TYPE_WRITE))     & 0xFFFF);
						softevent->events[layer_id]    &= (M_uint16)((~(1 << M_EVENT_TYPE_OTHER))     & 0xFFFF);
					} else {
						softevent->events[layer_id]     = 0;
					}
				}
				if (M_io_layer_softevent_is_empty(io, softevent)) {
					M_llist_remove_node(ioev->softevent_node);
					ioev->softevent_node = NULL;
				}
			}
		}
	}

	M_event_unlock(event);
}


static void M_io_softevent_del(M_io_t *io, size_t layer_id, M_event_type_t type)
{
	M_event_t            *event = M_io_get_event(io);
	M_event_io_t         *ioev  = NULL;
	M_event_softevent_t  *softevent;
//M_printf("%s(): io = %p, layer %zu, type %d\n", __FUNCTION__, io, layer_id, (int)type);

	if (layer_id >= M_io_layer_count(io) + 1 /* User layer */)
		return;

	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	M_event_lock(event);

	if (M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev)) {
		if (ioev->softevent_node != NULL) {
			softevent                   = M_llist_node_val(ioev->softevent_node);
			softevent->events[layer_id] &= (M_uint16)((~(1 << type)) & 0xFFFF);
			if (M_io_layer_softevent_is_empty(io, softevent)) {
				M_llist_remove_node(ioev->softevent_node);
				ioev->softevent_node = NULL;
			}
		}
	}

	M_event_unlock(event);
}


void M_io_layer_softevent_del(M_io_layer_t *layer, M_bool sibling_only, M_event_type_t type)
{
	M_io_t               *io    = M_io_layer_get_io(layer);
	size_t                id    = M_io_layer_get_index(layer);

	if (sibling_only)
		id++;
//M_printf("%s(): io = %p, layer = %zu, type = %d\n", __FUNCTION__, io, id, (int)type);

	M_io_softevent_del(io, id, type);
}


void M_io_user_softevent_del(M_io_t *io, M_event_type_t type)
{
	M_io_softevent_del(io, M_io_layer_count(io), type);
}


void M_event_wake(M_event_t *event)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	/* Only signal event loop if currently blocked */
	if (event->u.loop.waiting && event->u.loop.parent_wake != NULL) {
		M_io_osevent_trigger(event->u.loop.parent_wake);
	}
}


M_bool M_event_handle_modify(M_event_t *event, M_event_modify_type_t modtype, M_io_t *io, M_EVENT_HANDLE handle, M_EVENT_SOCKET sock, M_event_wait_type_t waittype, M_event_caps_t caps)
{
	M_event_evhandle_t     *member  = NULL;
	M_bool                  retval  = M_FALSE;

	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return M_FALSE;

	M_event_lock(event);

	member = M_hash_u64vp_get_direct(event->u.loop.evhandles, (M_uint64)((M_uintptr)handle));

	if (member != NULL && modtype == M_EVENT_MODTYPE_ADD_HANDLE) {
		goto cleanup;
	}

	if (member == NULL && (modtype == M_EVENT_MODTYPE_ADD_WAITTYPE || modtype == M_EVENT_MODTYPE_DEL_WAITTYPE || modtype == M_EVENT_MODTYPE_DEL_HANDLE)) {
		goto cleanup;
	}

	if (modtype == M_EVENT_MODTYPE_ADD_HANDLE && io == NULL) {
		goto cleanup;
	}

	if (member == NULL) {
		/* Create member */
		member           = M_malloc_zero(sizeof(*member));
		member->handle   = handle;
		member->sock     = sock;
		member->io       = io;
	}

	if (modtype == M_EVENT_MODTYPE_ADD_HANDLE) {
		member->waittype =  waittype;
		member->caps     =  caps;
	} else if (modtype == M_EVENT_MODTYPE_ADD_WAITTYPE) {
		/* Do nothing if already set */
		if ((member->waittype & waittype) == waittype)
			goto cleanup;
		member->waittype |= waittype;
	} else if (modtype == M_EVENT_MODTYPE_DEL_WAITTYPE) {
		/* Do nothing if wasn't set */
		if (!(member->waittype & waittype))
			goto cleanup;
		member->waittype &= ~waittype;
	} else if (modtype == M_EVENT_MODTYPE_DEL_HANDLE) {
		caps = member->caps;
	}

	switch (modtype) {
		case M_EVENT_MODTYPE_ADD_HANDLE:
//M_printf("%s(): %p add handle %d for io %p\n", __FUNCTION__, event, (int)handle, comm);
			M_hash_u64vp_insert(event->u.loop.evhandles, (M_uint64)((M_uintptr)handle), member);
			break;
		case M_EVENT_MODTYPE_ADD_WAITTYPE:
		case M_EVENT_MODTYPE_DEL_WAITTYPE:
			/* Do nothing, we modified the member directly */
			break;
		case M_EVENT_MODTYPE_DEL_HANDLE:
			/* Remove the node */
//M_printf("%s(): %p remove handle %d for io %p\n", __FUNCTION__, event, (int)handle, comm);

			M_hash_u64vp_remove(event->u.loop.evhandles, (M_uint64)((M_uintptr)handle), M_TRUE);
			/* XXX: Should this be part of the initializer instead so it auto-frees? */
			M_free(member);
			break;
	}
	retval = M_TRUE;

	/* Implementation-specific */
	if (event->u.loop.impl != NULL && event->u.loop.impl->modify_event != NULL)
		event->u.loop.impl->modify_event(event, modtype, handle, waittype, caps);

	/* NOTE: No need to call M_event_wake(), it is the responsibiilty of the impl->modify_event() callback
	 *       to wake if needed. Some implementations like kqueue() and epoll() don't need to wake up
	 *       when the event list changes since we modify the kernel poll list directly. */

cleanup:
	M_event_unlock(event);

	return retval;
}


M_event_t *M_event_distribute(M_event_t *event)
{
	M_event_t *best_event       = NULL;
	M_uint64   best_event_time  = 0;    /* Lower is better */
	size_t     best_event_count = 0;    /* Lower is better */
	size_t     i;

	if (event == NULL)
		return NULL;

	if (event->type == M_EVENT_BASE_TYPE_LOOP)
		return event;

	/* If a pool, choose the best thread */
	for (i=0; i<event->u.pool.thread_count; i++) {
		M_uint64 curr_time  = M_event_get_statistic(&event->u.pool.thread_evloop[i], M_EVENT_STATISTIC_PROCESS_TIME_MS);
		size_t   curr_count = M_event_num_objects(&event->u.pool.thread_evloop[i]);

		/* If the event loop has nothing, it automatically wins */
		if (curr_count == 0)
			return &event->u.pool.thread_evloop[i];

		/* Worse match */
		if (best_event != NULL && curr_time > best_event_time)
			continue;

		/* Worse match */
		if (best_event != NULL && curr_time == best_event_time && curr_count > best_event_count)
			continue;

		/* Best so far */
		best_event       = &event->u.pool.thread_evloop[i];
		best_event_time  = curr_time;
		best_event_count = curr_count;
	}

	return best_event;
}


M_bool M_event_add(M_event_t *event, M_io_t *comm, M_event_callback_t callback, void *cb_data)
{
	size_t                  i;
	size_t                  num;
	M_bool                  retval = M_TRUE;
	M_event_io_t           *ioev   = NULL;

	if (event == NULL || comm == NULL)
		return M_FALSE;

	/* Choose child event handle if pool was provided */
	event = M_event_distribute(event);
//M_printf("%s(): event %p io %p enter\n", __FUNCTION__, event, comm);
	M_event_lock(event);

	if (comm->reg_event != NULL) {
		if (comm->private_event) {
			/* Unassociate from sync event private handle */
			M_event_destroy(comm->reg_event);
			comm->private_event = M_FALSE;
			comm->reg_event     = NULL;
		} else {
			M_event_unlock(event);
			return M_FALSE;
		}
	}

	comm->reg_event = event;

	ioev            = M_malloc_zero(sizeof(*ioev));
	ioev->callback  = callback;
	ioev->cb_data   = cb_data;
	M_hashtable_insert(event->u.loop.reg_ios, comm, ioev);

	num = M_list_len(comm->layer);
	for (i=0; i<num; i++) {
		M_io_layer_t *layer = M_io_layer_at(comm, i);
		if (layer->cb.cb_init == NULL)
			continue;
		if (!layer->cb.cb_init(layer)) {
			retval = M_FALSE;
			comm->reg_event = NULL;
			break;
		}
	}

	M_event_unlock(event);

	if (!retval) {
		M_event_remove(comm);
	}
//M_printf("%s(): event %p io %p exit\n", __FUNCTION__, event, comm);

	return retval;
}


M_bool M_event_edit_io_cb(M_io_t *io, M_event_callback_t callback, void *cb_data)
{
	M_event_t     *event = NULL;
	M_event_io_t  *ioev  = NULL;
	M_bool         rv    = M_FALSE;

	if (io == NULL)
		return M_FALSE;

	M_io_lock(io);

	event = io->reg_event;
	if (event == NULL)
		goto done;

	M_event_lock(event);

	if (!M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev) || ioev == NULL)
		goto done;

	ioev->callback = callback;
	ioev->cb_data  = cb_data;

	rv = M_TRUE;

done:
	M_event_unlock(event);
	M_io_unlock(io);

	return rv;
}


M_event_callback_t M_event_get_io_cb(M_io_t *io, void **cb_data_out)
{
	M_event_t         *event = NULL;
	M_event_io_t      *ioev  = NULL;
	M_event_callback_t cb    = NULL;

	(*cb_data_out) = NULL;

	if (io == NULL)
		return NULL;

	M_io_lock(io);

	event = io->reg_event;
	if (event == NULL)
		goto done;

	M_event_lock(event);

	if (!M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev) || ioev == NULL)
		goto done;

	cb             = ioev->callback;
	(*cb_data_out) = ioev->cb_data;

done:
	M_event_unlock(event);
	M_io_unlock(io);

	return cb;
}


void M_event_remove(M_io_t *io)
{
	M_event_t *event;

	if (io == NULL || io->reg_event == NULL)
		return;

	event = io->reg_event;
//M_printf("%s(): event %p io %p exit\n", __FUNCTION__, event, io);

	/* NOTE: event is guaranteed to be a loop type, no need to check */

	M_event_lock(event);
	M_event_io_unregister(io, M_FALSE);
	M_hashtable_remove(event->u.loop.reg_ios, io, M_TRUE);
	M_event_queue_pending_clear(event, io);

	M_event_unlock(event);
//M_printf("%s(): event %p io %p exit\n", __FUNCTION__, event, io);

}


M_bool M_event_queue_task(M_event_t *event, M_event_callback_t callback, void *cb_data)
{
	M_event_timer_t *timer;

	timer = M_event_timer_oneshot(event, 0, M_TRUE, callback, cb_data);

	if (timer != NULL) {
		return M_TRUE;
	}

	return M_FALSE;
}


static void M_event_queue_pending(M_event_t *event, M_io_t *io, size_t layer_id, M_event_type_t type)
{
	M_event_pending_t *entry  = NULL;
	size_t             i;
	M_uint16           ev;

	/* If this is the first event, create the container */
	if (event->u.loop.pending_events == NULL) {
		const struct M_hashtable_callbacks pending_events_cb = {
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			M_free
		};
		event->u.loop.pending_events = M_hashtable_create(16, 75, NULL, NULL, M_HASHTABLE_KEYS_ORDERED, &pending_events_cb);
	}

	/* If this is the first event for this io object, create the container */
	if (!M_hashtable_get(event->u.loop.pending_events, io, (void **)&entry)) {
		entry                = M_malloc_zero(sizeof(*entry));
		M_hashtable_insert(event->u.loop.pending_events, io, entry);
	}

//M_printf("%s(): io %p layer %zu handle %d type %d\n", __FUNCTION__, io_or_timer, layer_id, handle, (int)type);
	ev                       = (M_uint16)(1 << type);
	entry->events[layer_id] |= ev;

	/* NOTE: we use the highest bit as an indicator that the *next* layer needs
	 *       to be checked when processing events, so tag all layers below this
	 *       one */
	for (i=0; i<layer_id; i++) {
		entry->events[i] |= 0x8000;
	}
}


static void M_event_queue_pending_delivered(M_event_t *event, M_io_t *io, M_event_type_t type, size_t layer_id)
{
	M_event_pending_t *entry = NULL;
	ssize_t            i;
	M_uint16           mask;

	if (!M_hashtable_get(event->u.loop.pending_events, io, (void **)&entry))
		return;

	/* Unset delivered event */
	mask                     = (M_uint16)((M_uint16)1 << (M_uint16)type);
	entry->events[layer_id] &= (M_uint16)(~mask);

	/* Clear high bits if next layer is empty */
	if (layer_id > 0) {
		for (i=(ssize_t)layer_id-1; i>=0; i--) {
			if (entry->events[i+1] != 0)
				break;
			entry->events[i] &= (M_uint16)0x7FFF;
		}
	}

}


void M_event_queue_pending_clear(M_event_t *event, M_io_t *io)
{
	/* We must not modify the hashtables themselves, we just need to clear the events so we
	 * don't force the thread processing the events to know they have to restart enumeration. */
	M_event_pending_t        *entry        = NULL;

	/* No events for this object were enqueued */
	if (!M_hashtable_get(event->u.loop.pending_events, io, (void **)&entry)) {
		return;
	}

	/* Clear events */
	M_mem_set(entry->events, 0, sizeof(entry->events));
}


/*! Event handle must be locked before calling this */
static void M_event_deliver(M_event_t *event, M_io_t *io, size_t layer_id, M_event_type_t type)
{
	size_t               num_layers;
	size_t               i;
	M_bool               consumed  = M_FALSE;
	M_event_callback_t   callback  = NULL;
	void                *cb_data   = NULL;
	M_event_io_t        *ioev      = NULL;

	/* IO object has been removed */
	if (!M_hashtable_get(event->u.loop.reg_ios, io, (void **)&ioev))
		return;

	num_layers = M_list_len(io->layer);
	for (i=layer_id; i<num_layers && !consumed; i++) {
		M_io_layer_t *layer = M_io_layer_at(io, i);

		M_event_queue_pending_delivered(event, io, type, i);

		if (!layer->cb.cb_process_event)
			continue;

		/* Event handlers may rewrite "type" presented to user.  Such as if the
		 * event really resulted in a disconnect.  Or even a new connection coming
		 * in. */
		consumed = layer->cb.cb_process_event(layer, &type);
	}


	/* Retrieve user-specified callback if not consumed by internal handlers */
	if (!consumed) {
		M_event_queue_pending_delivered(event, io, type, num_layers /* User layer */);

		if (ioev != NULL) {
			callback = ioev->callback;
			cb_data  = ioev->cb_data;
		}
	}

	if (callback == NULL)
		return;

	/* Release locks before calling user callbacks */
	M_event_unlock(event);
//M_printf("%s(): user deliver io %p handle %d type %d - cb %p, %p\n", __FUNCTION__, io, handle, (int)type, callback, cb_data);

	callback(event, type, io, cb_data);
//M_printf("%s(): user deliver io %p handle %d type %d DONE - cb %p, %p\n", __FUNCTION__, io, handle, (int)type, callback, cb_data);

	/* Re-obtain locks */
	M_event_lock(event);

}


/* NOTE: event must be locked before calling this */
static void M_event_queue_deliver(M_event_t *event)
{
	M_hashtable_enum_t        enumevent;
	const void               *key = NULL;
	const void               *val = NULL;

	if (M_hashtable_enumerate(event->u.loop.pending_events, &enumevent) == 0)
		goto done;

	/* Enumerate io or timer objects */
	while (M_hashtable_enumerate_next(event->u.loop.pending_events, &enumevent, &key, &val)) {
		M_io_t            *io     = NULL;
		M_event_pending_t *entry  = M_CAST_OFF_CONST(M_event_pending_t *, val);
		size_t             i;
		size_t             j;

		io = M_CAST_OFF_CONST(M_io_t *, key);

		/* Process all events, even if there are no events for this layer, the high
		 * bit is set if there are events for a higher layer */
		for (j=0; entry->events[j] != 0; j++) {
			for (i=0; i<M_EVENT_TYPE__CNT && (entry->events[j] & 0x7FFF) != 0; i++) {
				if (entry->events[j] & (((M_uint16)1) << (M_uint8)i)) {
					M_event_deliver(event, io, j, (M_event_type_t)i);
				}
			}
		}
	}

done:

	/* Clean up events */
	M_hashtable_destroy(event->u.loop.pending_events, M_TRUE);
	event->u.loop.pending_events = NULL;
}


void M_event_deliver_io(M_event_t *event, M_io_t *io, M_event_type_t type)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP || io == NULL)
		return;

	M_printf("%s:%d: M_event_deliver_io(%p,%p,%d)\n", __FILE__, __LINE__, event, io, type);
	fflush(stdout);
	event->u.loop.osevent_cnt++;
	M_event_queue_pending(event, io, 0 /* real events always start at layer 0 */, type);
}


static void M_event_softevent_process(M_event_t *event)
{
	M_llist_node_t *node;
	M_event_io_t   *ioev = NULL;

	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	node = M_llist_first(event->u.loop.soft_events);
	while (node != NULL) {
		M_llist_node_t      *next      = M_llist_node_next(node);
		M_event_softevent_t *softevent;
		size_t               i;
		size_t               j;
		size_t               num_layers;

		softevent  = M_llist_take_node(node);
		num_layers = M_io_layer_count(softevent->io) + 1 /* User layer */;

		/* Enqueue all events */
		for (j=0; j<num_layers; j++) {
			for (i=0; i<M_EVENT_TYPE__CNT && softevent->events[j] != 0; i++) {
				if (softevent->events[j] & (((M_uint16)1) << i)) {
					M_uint16 mask = (M_uint16)((M_uint16)1 << (M_uint16)i);
					M_event_queue_pending(event, softevent->io, j, (M_event_type_t)i);
					softevent->events[j] &= (M_uint16)(~mask);
					event->u.loop.softevent_cnt++;
				}
			}
		}

		/* Remove softevent handle */
		if (M_hashtable_get(event->u.loop.reg_ios, softevent->io, (void **)&ioev)) {
			ioev->softevent_node = NULL;
		}
		M_free(softevent);

		node = next;
	}
}

static void M_event_done_with_disconnect_cb(M_event_t *event, M_event_type_t type, M_io_t *io_dummy, void *cb_arg)
{
	M_uint64 disconnect_timeout_ms = (M_uint64)((M_uintptr)cb_arg);
	M_uint64            elapsed_ms;
	M_hashtable_enum_t  hashenum;
	const void         *key     = NULL;
	M_list_t           *ios     = NULL;
	size_t              i;

	(void)type;
	(void)io_dummy;

	if (disconnect_timeout_ms == M_UINTPTR_MAX)
		disconnect_timeout_ms = M_TIMEOUT_INF;

	/* Executed from within the event loop */
	M_event_lock(event);

	elapsed_ms = M_time_elapsed(&event->u.loop.start_tv);

	if (disconnect_timeout_ms == M_TIMEOUT_INF || elapsed_ms + disconnect_timeout_ms < event->u.loop.timeout_ms) {
		event->u.loop.timeout_ms = elapsed_ms + disconnect_timeout_ms;
	}

	/* Iterate across all registered IOs and issue a disconnect. We have to generate a
	 * list first, then we have to check to ensure the io object hasn't been removed
	 * by a prior disconnect. */
	ios = M_list_create(NULL, M_LIST_NONE);

	if (M_hashtable_enumerate(event->u.loop.reg_ios, &hashenum) > 0) {
		while (M_hashtable_enumerate_next(event->u.loop.reg_ios, &hashenum, &key, NULL)) {
			M_list_insert(ios, key);
		}
	}

	for (i=0; i<M_list_len(ios); i++) {
		M_io_t *io;
		key = M_list_at(ios, i);
		io  = M_CAST_OFF_CONST(M_io_t *, key);
		/* Make sure io object pointer is still valid */
		if (M_hashtable_get(event->u.loop.reg_ios, key, NULL)) {
			M_io_disconnect(io);
		}
	}

	M_list_destroy(ios, M_TRUE);

	M_event_unlock(event);
}


static void M_event_done_with_disconnect_int(M_event_t *event, M_uint64 timeout_before_disconnect_ms, M_uint64 disconnect_timeout_ms)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	/* Add timer to trigger disconnects after timeout period */
	M_event_lock(event);

	/* Exit when no more objects, except timers */
	event->u.loop.flags |= M_EVENT_FLAG_EXITONEMPTY|M_EVENT_FLAG_EXITONEMPTY_NOTIMERS;

	M_event_unlock(event);

	/* Add timer to start issuing disconnects on owned objects */
	if (disconnect_timeout_ms > M_UINTPTR_MAX)
		disconnect_timeout_ms = M_UINTPTR_MAX;
	M_event_timer_oneshot(event, timeout_before_disconnect_ms, M_TRUE, M_event_done_with_disconnect_cb, (void *)((M_uintptr)disconnect_timeout_ms));
}


void M_event_done_with_disconnect(M_event_t *event, M_uint64 timeout_before_disconnect_ms, M_uint64 disconnect_timeout_ms)
{
	if (event == NULL)
		return;

	if (event->type == M_EVENT_BASE_TYPE_LOOP) {
		if (event->u.loop.parent) {
			M_event_done_with_disconnect(event->u.loop.parent, timeout_before_disconnect_ms, disconnect_timeout_ms);
		} else {
			M_event_done_with_disconnect_int(event, timeout_before_disconnect_ms, disconnect_timeout_ms);
		}
		return;
	}

	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++)
			M_event_done_with_disconnect_int(&event->u.pool.thread_evloop[i], timeout_before_disconnect_ms, disconnect_timeout_ms);
		return;
	}
}


static void M_event_status_change(M_event_t *event, M_event_status_t status)
{
	if (event == NULL || event->type != M_EVENT_BASE_TYPE_LOOP)
		return;

	M_event_lock(event);
	event->u.loop.status_change = status;
	M_event_wake(event);
	M_event_unlock(event);
}


void M_event_done(M_event_t *event)
{
	if (event == NULL)
		return;

	if (event->type == M_EVENT_BASE_TYPE_LOOP) {
		if (event->u.loop.parent) {
			M_event_done(event->u.loop.parent);
		} else {
			M_event_status_change(event, M_EVENT_STATUS_DONE);
		}
		return;
	}

	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++)
			M_event_status_change(&event->u.pool.thread_evloop[i], M_EVENT_STATUS_DONE);
		return;
	}
}


void M_event_return(M_event_t *event)
{
	if (event == NULL)
		return;

	if (event->type == M_EVENT_BASE_TYPE_LOOP) {
		if (event->u.loop.parent) {
			M_event_done(event->u.loop.parent);
		} else {
			M_event_status_change(event, M_EVENT_STATUS_DONE);
		}
		return;
	}

	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++)
			M_event_status_change(&event->u.pool.thread_evloop[i], M_EVENT_STATUS_RETURN);
		return;
	}
}


M_event_status_t M_event_get_status(M_event_t *event)
{
	M_event_status_t status;

	if (event == NULL)
		return M_EVENT_STATUS_PAUSED;

	/* Status for all should be the same, just get the first */
	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		event = &event->u.pool.thread_evloop[0];
	}

	M_event_lock(event);
	status = event->u.loop.status;
	M_event_unlock(event);

	return status;
}


M_uint64 M_event_get_statistic(M_event_t *event, M_event_statistic_t type)
{
	M_uint64 cnt = 0;

	if (event == NULL)
		return 0;

	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++)
			cnt += M_event_get_statistic(&event->u.pool.thread_evloop[i], type);
		return cnt;
	}

	M_event_lock(event);
	switch (type) {
		case M_EVENT_STATISTIC_WAKE_COUNT:
			cnt = event->u.loop.wake_cnt;
			break;
		case M_EVENT_STATISTIC_OSEVENT_COUNT:
			cnt = event->u.loop.osevent_cnt;
			break;
		case M_EVENT_STATISTIC_SOFTEVENT_COUNT:
			cnt = event->u.loop.softevent_cnt;
			break;
		case M_EVENT_STATISTIC_TIMER_COUNT:
			cnt = event->u.loop.timer_cnt;
			break;
		case M_EVENT_STATISTIC_PROCESS_TIME_MS:
			cnt = event->u.loop.process_time_ms;
			break;
	}
	M_event_unlock(event);

	return cnt;
}


size_t M_event_num_objects(M_event_t *event)
{
	size_t num_objects = 0;

	if (event == NULL)
		return 0;

	if (event->type == M_EVENT_BASE_TYPE_POOL) {
		size_t i;
		for (i=0; i<event->u.pool.thread_count; i++) {
			num_objects += M_event_num_objects(&event->u.pool.thread_evloop[i]);
		}

		return num_objects;
	}

	M_event_lock(event);
	num_objects = M_hashtable_num_keys(event->u.loop.reg_ios) + M_queue_len(event->u.loop.timers);
//M_printf("%s(): ev:%p io objects = %zu, timers = %zu\n", __FUNCTION__, event, M_hashtable_num_keys(event->u.loop.reg_ios), M_queue_len(event->u.loop.timers));
	if (!(event->u.loop.flags & M_EVENT_FLAG_NOWAKE) && num_objects && event->u.loop.parent_wake)
		num_objects--;
	M_event_unlock(event);

	return num_objects;
}

#define M_EVENT_LARGE_MEMBERS 32

static M_event_err_t M_event_loop_loop(M_event_t *event, M_uint64 timeout_ms)
{
	M_timeval_t     event_process_tv;
	M_uint64        elapsed   = 0;
	M_bool          has_events;
	M_uint64        event_timeout_ms;
	M_uint64        min_timer_ms;
	M_bool          has_soft_events;
	size_t          num_objects;
	M_event_err_t   retval = M_EVENT_ERR_TIMEOUT;

	if (event == NULL || event->u.loop.impl == NULL)
		return M_EVENT_ERR_MISUSE;

	M_event_lock(event);

	if (event->u.loop.status == M_EVENT_STATUS_RUNNING) {
		M_event_unlock(event);
		return M_EVENT_ERR_MISUSE;
	}
	event->u.loop.status        = M_EVENT_STATUS_RUNNING;
	event->u.loop.status_change = 0;
	event->u.loop.timeout_ms    = timeout_ms;
	event->u.loop.threadid      = M_thread_self();

	M_time_elapsed_start(&event->u.loop.start_tv);
	do {
		/* User requested we exit the loop */
		if (event->u.loop.status_change != 0)
			break;

		if (event->u.loop.flags & M_EVENT_FLAG_EXITONEMPTY) {
			num_objects = M_hashtable_num_keys(event->u.loop.reg_ios);

			/* Only count timers if they are not stopped and we haven't been explicitly told not to count them */
			if (M_event_timer_minimum_ms(event) != M_TIMEOUT_INF && !(event->u.loop.flags & M_EVENT_FLAG_EXITONEMPTY_NOTIMERS))
				num_objects += M_queue_len(event->u.loop.timers);

			/* Subtract the internal wake object */
			if (!(event->u.loop.flags & M_EVENT_FLAG_NOWAKE) && num_objects && event->u.loop.parent_wake)
				num_objects--;

			if (num_objects == 0) {
				retval = M_EVENT_ERR_DONE;
				break;
			}
		}

		if (event->u.loop.impl != NULL /* appease clang, not possible */ && event->u.loop.impl->data_structure != NULL)
			event->u.loop.impl->data_structure(event);
		event->u.loop.waiting  = M_TRUE;
		min_timer_ms           = M_event_timer_minimum_ms(event);
		has_soft_events        = M_FALSE;
		if (M_llist_len(event->u.loop.soft_events))
			has_soft_events = M_TRUE;

		M_event_unlock(event);

		event_timeout_ms = event->u.loop.timeout_ms;
		if (event_timeout_ms != M_TIMEOUT_INF) {
			if (event_timeout_ms > elapsed) {
				event_timeout_ms -= elapsed;
			} else {
				event_timeout_ms = 0;
			}
		}
		if (min_timer_ms < event_timeout_ms)
			event_timeout_ms = min_timer_ms;
		if (has_soft_events)
			event_timeout_ms = 0;
//M_printf("%s(): ev:%p waiting on events for %llums\n", __FUNCTION__, event, event_timeout_ms);
		has_events = event->u.loop.impl->wait_event(event, event_timeout_ms);
//M_printf("%s(): ev:%p woken by %s\n", __FUNCTION__, event, has_events?"event":"timeout");


		M_event_lock(event);

		event->u.loop.wake_cnt++;
		event->u.loop.waiting            = M_FALSE;

		/* ----- Process Events ----- */

		/* Start recording how much time event processing takes */
		M_time_elapsed_start(&event_process_tv);
//M_printf("%s(): %p processing soft events\n", __FUNCTION__, event);

		/* Process soft events -- NOTE: we must always process these first, as a CONNECTED event
		 * and an OS read event may be queued simultaneously, and we need to make sure the CONNECTED
		 * event is delivered *FIRST* */
		M_event_softevent_process(event);

		/* Process OS events */
		if (has_events) {
//M_printf("%s(): %p processing OS events\n", __FUNCTION__, event);
			event->u.loop.impl->process_events(event);
		}

//M_printf("%s(): %p processing timers\n", __FUNCTION__, event);
		/* Deliver all queued events */
		M_event_queue_deliver(event);

		/* Process timer events */
		M_event_timer_process(event);


		/* NOTE: Re-process any soft events that might have been delivered, as calling
		 * out to a syscall to check for new OS-events can add significant latency
		 * (Windows, I'm looking at you) and we don't want to do that until we've
		 * processed soft events that may have been triggered due to io-object
		 * chaining (such as in io_netdns) */
		M_event_softevent_process(event);
		M_event_queue_deliver(event);


		/* Record event processing time */
		event->u.loop.process_time_ms += M_time_elapsed(&event_process_tv);
		/* ----- End Process Events ----- */

	} while ((elapsed = M_time_elapsed(&event->u.loop.start_tv)) < event->u.loop.timeout_ms);

	/* Mark as paused */
	if (event->u.loop.status == M_EVENT_STATUS_RUNNING)
		event->u.loop.status = M_EVENT_STATUS_PAUSED;

	/* Handle a status change event */
	if (event->u.loop.status_change == M_EVENT_STATUS_DONE) {
		event->u.loop.status        = event->u.loop.status_change;
		event->u.loop.status_change = 0;
		retval                      = M_EVENT_ERR_DONE;
	} else if (event->u.loop.status_change == M_EVENT_STATUS_RETURN) {
		event->u.loop.status        = event->u.loop.status_change;
		event->u.loop.status_change = 0;
		retval                      = M_EVENT_ERR_RETURN;
	}
#if 0
{
	M_hashtable_enum_t hashenum;
	const void        *key;
	M_hashtable_enumerate(event->u.loop.reg_ios, &hashenum);
	M_printf("%s(): ev:%p ---- THESE IO OBJECTS STILL EXIST ----\n", __FUNCTION__, event);
	while (M_hashtable_enumerate_next(event->u.loop.reg_ios, &hashenum, &key, NULL)) {
		M_io_t       *io    = M_CAST_OFF_CONST(M_io_t *, key);
		M_io_layer_t *layer = M_io_layer_at(io, 0);
		M_printf("%s(): ev:%p %p - %s%s\n", __FUNCTION__, event, io, layer->name, (io == event->u.loop.parent_wake)?" (internal)":"");
	}
	M_printf("%s(): ev:%p ---- DONE ----\n", __FUNCTION__, event);
}
#endif
	event->u.loop.threadid = 0;
	M_event_unlock(event);

	return retval;
}


struct M_event_pool_loop_thread_arg {
	M_event_t *event;
	M_uint64   timeout_ms;
};
typedef struct M_event_pool_loop_thread_arg M_event_pool_loop_thread_arg_t;


static void *M_event_pool_loop_thread(void *arg)
{
	M_event_pool_loop_thread_arg_t *data = arg;
	M_event_err_t                   rv;

	rv = M_event_loop_loop(data->event, data->timeout_ms);

	M_free(data);
	return (void *)rv;
}


static M_event_err_t M_event_pool_loop(M_event_t *event, M_uint64 timeout_ms)
{
	size_t           i;
	M_thread_attr_t *attr = M_thread_attr_create();
	M_event_err_t    rv;

	M_thread_attr_set_create_joinable(attr, M_TRUE);

	/* Create threads for 1->thread_count, 0 is run on the current thread */
	for (i=1; i<event->u.pool.thread_count; i++) {
		M_event_pool_loop_thread_arg_t *thread_arg = M_malloc_zero(sizeof(*thread_arg));

		/* Bind thread to single cpu core */
		M_thread_attr_set_processor(attr, (int)i);

		thread_arg->event                          = &event->u.pool.thread_evloop[i];
		thread_arg->timeout_ms                     = timeout_ms;
		event->u.pool.thread_ids[i]                = M_thread_create(attr, M_event_pool_loop_thread, thread_arg);
	}

	M_thread_attr_destroy(attr);

	/* Bind self to first CPU core */
	M_thread_set_processor(M_thread_self(), 0);

	rv = M_event_loop_loop(&event->u.pool.thread_evloop[0], timeout_ms);

	/* Wait for all threads to exit */
	for (i=1; i<event->u.pool.thread_count; i++) {
		void *val = NULL;
		M_thread_join(event->u.pool.thread_ids[i], &val);
	}

	/* Unbind the main thread from the first core */
	M_thread_set_processor(M_thread_self(), -1);

	/* All threads return values should be the same */
	return rv;
}


M_event_err_t M_event_loop(M_event_t *event, M_uint64 timeout_ms)
{
	if (event == NULL)
		return M_EVENT_ERR_MISUSE;

	if (event->type == M_EVENT_BASE_TYPE_LOOP)
		return M_event_loop_loop(event, timeout_ms);

	return M_event_pool_loop(event, timeout_ms);
}


M_event_t *M_event_get_pool(M_event_t *event)
{
	if (event == NULL)
		return NULL;

	/* Already referencing the pool */
	if (event->type == M_EVENT_BASE_TYPE_POOL)
		return event;

	/* Not part of a pool, return self */
	if (event->u.loop.parent == NULL)
		return event;

	/* Return parent which is the pool */
	return event->u.loop.parent;
}

