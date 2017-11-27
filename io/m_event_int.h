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

#ifndef __M_EVENT_INT_H__
#define __M_EVENT_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>

/* Need types for SOCKET and INVALID_SOCKET */
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <mstcpip.h>
#  include <windows.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


#ifdef _WIN32
#  define M_EVENT_SOCKET         SOCKET
#  define M_EVENT_INVALID_SOCKET INVALID_SOCKET
#else
#  define M_EVENT_SOCKET         int
#  define M_EVENT_INVALID_SOCKET -1
#endif

#define M_EVENT_TYPE__CNT 7  /*!< Count of event types */

enum M_event_caps {
	M_EVENT_CAPS_WRITE = 1 << 0, /*!< Also implies Connect */
	M_EVENT_CAPS_READ  = 1 << 1  /*!< Also implies Accept */
};
typedef enum M_event_caps M_event_caps_t;


enum M_event_wait_type {
	M_EVENT_WAIT_READ  = 1 << 0, /*!< Wait for read events */
	M_EVENT_WAIT_WRITE = 1 << 1, /*!< Wait for write events */
};
typedef enum M_event_wait_type M_event_wait_type_t;

struct M_event_evhandle {
	M_EVENT_HANDLE      handle;
	M_EVENT_SOCKET      sock;
	M_event_wait_type_t waittype;
	M_event_caps_t      caps; 
	M_io_t             *io;
};
typedef struct M_event_evhandle M_event_evhandle_t;

struct M_event_io {
	M_event_callback_t callback;       /*!< User-supplied callback                                       */
	void              *cb_data;        /*!< Data to pass to user-supplied callback                       */
	M_llist_node_t    *softevent_node; /*!< Reference to the node in the soft event list for this M_io_t */
};
typedef struct M_event_io M_event_io_t;

struct M_event_trigger {
	M_io_t *io;
};

M_uint64 M_event_timer_minimum_ms(M_event_t *event);
void M_event_timer_process(M_event_t *event);
void M_event_deliver_io(M_event_t *event, M_io_t *io, M_event_type_t type);
void M_io_softevent_add(M_io_t *io, size_t layer_id, M_event_type_t type);


struct M_event_data;
typedef struct M_event_data M_event_data_t;

enum M_event_modify_type {
	M_EVENT_MODTYPE_ADD_HANDLE   = 1, /*!< Add new handle                          */
	M_EVENT_MODTYPE_ADD_WAITTYPE = 2, /*!< Make sure a waittype is set on a handle */
	M_EVENT_MODTYPE_DEL_WAITTYPE = 3, /*!< Unset a waittype on a handle            */
	M_EVENT_MODTYPE_DEL_HANDLE   = 4  /*!< Delete an existing handle completely    */
};
typedef enum M_event_modify_type M_event_modify_type_t;

struct M_event_impl_cbs {
	void   (*data_free)(M_event_data_t *data);
	/* Store in event->impl_data */
	void   (*data_structure)(M_event_t *event);
	M_bool (*wait_event)(M_event_t *event, M_uint64 timeout_ms);
	void   (*process_events)(M_event_t *event);
	void   (*modify_event)(M_event_t *event, M_event_modify_type_t modtype, M_EVENT_HANDLE handle, M_event_wait_type_t waittype, M_event_caps_t caps);
};
typedef struct M_event_impl_cbs M_event_impl_cbs_t;


struct M_event_softevent {
	M_io_t      *io;
	M_uint16     events[M_IO_LAYERS_MAX]; /*!< each event sets its bit */
};
typedef struct M_event_softevent M_event_softevent_t;


struct M_event_pending {
	M_uint16     events[M_IO_LAYERS_MAX]; /*!< each event sets its bit and layer to deliver to */
};
typedef struct M_event_pending M_event_pending_t;


struct M_event_loop {
	M_event_t          *parent;               /*!< For event pools, this is the pool object, otherwise NULL */
	M_threadid_t        threadid;             /*!< ThreadID currently processing the event loop              */
	M_thread_mutex_t   *lock;                 /*!< Lock to prevent concurrent access */
	M_uint64            timeout_ms;           /*!< Cache variable for tracking the current event loop timeout */
	M_timeval_t         start_tv;             /*!< Elapsed timer start of current event loop                  */
	enum M_EVENT_FLAGS  flags;                /*!< Flags that control behavior */
	M_event_status_t    status;               /*!< Status of event loop */
	M_event_status_t    status_change;        /*!< Requested status change */

	M_hash_u64vp_t     *evhandles;            /*!< Registered list of OS event handles. M_EVENT_HANDLE to M_event_evhandle_t (M_io_t, M_event_wait_type_t) */

	M_io_t             *parent_wake;          /*!< Event handle for waking self when changes are made */
	M_bool              waiting;              /*!< Whether or not the event loop is currently blocked waiting on new events (event->impl->wait_event()) */

	M_queue_t          *timers;               /*!< Sorted list of M_event_timer_t members */

	M_llist_t          *soft_events;          /*!< Linked list of M_event_softevent_t which are M_event-generated events to turn edge-triggered events into resettable events */
	M_hashtable_t      *reg_ios;              /*!< M_io_t * to M_event_io_t * for tracking M_io_t handles and associated user callbacks and soft events */
	M_hashtable_t      *pending_events;       /*!< M_io_t * or M_event_timer_t * to M_event_pending_t * ordered hashtable (in insertion order for prioritization) */

	M_uint64            process_time_ms;      /*!< Number of milliseconds spent processing events (to track load) */
	M_event_impl_cbs_t *impl_large;           /*!< Implementation callbacks when the event list is large (required) */
	M_event_impl_cbs_t *impl_short;           /*!< Implementation callbacks when the event list is short (optional) */
	M_event_impl_cbs_t *impl;                 /*!< Which callback is currently in use */
	M_event_data_t     *impl_data;            /*!< Implementation data used by the registered callbacks above */
};

typedef struct M_event_loop M_event_loop_t;

struct M_event_pool {
	M_event_t     *thread_evloop;       /*!< Array of event loop structures, one per thread */
	M_threadid_t  *thread_ids;          /*!< Array of thread ids */
	size_t         thread_count;        /*!< Count of threads */
};

typedef struct M_event_pool M_event_pool_t;

enum M_event_base_type {
	M_EVENT_BASE_TYPE_LOOP = 0,
	M_EVENT_BASE_TYPE_POOL = 1
};

struct M_event {
	enum M_event_base_type type;
	union {
		M_event_loop_t loop;
		M_event_pool_t pool;
	} u;
};

/*! Get child event handle if a pool was provided that is least loaded */
M_event_t *M_event_distribute(M_event_t *event);

M_bool M_event_handle_modify(M_event_t *event, M_event_modify_type_t modtype, M_io_t *io, M_EVENT_HANDLE handle, M_EVENT_SOCKET sock, M_event_wait_type_t waittype, M_event_caps_t caps);

/*! Should hold event->lock before calling this */
void M_event_wake(M_event_t *event);
void M_event_lock(M_event_t *event);
void M_event_unlock(M_event_t *event);

void M_io_user_softevent_add(M_io_t *io, M_event_type_t type);
void M_io_user_softevent_del(M_io_t *io, M_event_type_t type);
void M_io_softevent_clearall(M_io_t *io);
void M_event_queue_pending_clear(M_event_t *event, M_io_t *io);

M_io_t *M_io_osevent_create(M_event_t *event);
void M_io_osevent_trigger(M_io_t *io);

#if !defined(_WIN32)
extern struct M_event_impl_cbs M_event_impl_poll;
#endif
#if defined(_WIN32)
extern struct M_event_impl_cbs M_event_impl_win32;
#elif defined(HAVE_KQUEUE)
extern struct M_event_impl_cbs M_event_impl_kqueue;
#elif defined(HAVE_EPOLL)
extern struct M_event_impl_cbs M_event_impl_epoll;
#endif

__END_DECLS

#endif /* __M_EVENT_INT_H__ */
