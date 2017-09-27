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

#ifndef __M_EVENT_H__
#define __M_EVENT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_event Event Subsystem
 *  \ingroup m_eventio
 * 
 * Cross platform event based processing. A platform specific backend will be used
 * as the underlying event system but all events will be exposed though this interface.
 * No platform specific knowledge is needed.
 *
 * Developers used to working with macOS event loop style of programming can use
 * this event system to use that paradigm on other platforms. In this scenario most
 * events would be triggered as OTHER. Some sort of tracking would be necessary to
 * determine why an event was triggered if the same callback is used for multiple 
 * situations.
 *
 * The event system is thread safe allowing io objects and times can be added
 * to and moved between different event loops running on different threads.
 * Triggers can be triggered from different threads. Destruction of an io object
 * from a different thread will be queued in the event loop it's running on.
 *
 * Note: the CONNECTED event will be triggered when a io object is added to an event loop
 * using M_event_add().
 *
 *
 * Example application that demonstrates read/write events, timers, and queued tasks.
 * 
 * \code{.c}
 *     #include <mstdlib/mstdlib.h>
 *     #include <mstdlib/mstdlib_io.h>
 *     
 *     typedef struct {
 *         M_buf_t    *buf;
 *         M_parser_t *parser;
 *         M_io_t     *io; // Necessary for the queued task and timer to have the correct io object.
 *     } ldata_t;
 *     
 *     static void add_queued_data(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         ldata_t *ldata = thunk;
 *     
 *         (void)el;
 *         (void)etype;
 *     
 *         M_buf_add_str(ldata->buf, "STARTING\n");
 *         M_io_write_from_buf(ldata->io, ldata->buf);
 *     }
 *     
 *     static void add_data(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         ldata_t    *ldata = thunk;
 *         static int  i     = 1;
 *     
 *         (void)el;
 *         (void)etype;
 *     
 *         M_buf_add_str(ldata->buf, "TEST ");
 *         M_buf_add_int(ldata->buf, i++);
 *         M_buf_add_byte(ldata->buf, '\n');
 *         M_io_write_from_buf(ldata->io, ldata->buf);
 *     }
 *     
 *     static void stop(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         (void)etype;
 *         (void)io;
 *         (void)thunk;
 *     
 *         M_event_done_with_disconnect(el, 1000);
 *     }
 *     
 *     static void run_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         ldata_t *ldata = thunk;
 *         char    *out;
 *     
 *         switch (etype) {
 *             case M_EVENT_TYPE_CONNECTED:
 *                 break;
 *             case M_EVENT_TYPE_READ:
 *                 M_io_read_into_parser(io, ldata->parser);
 *                 out = M_parser_read_strdup(ldata->parser, M_parser_len(ldata->parser));
 *                 M_printf("%s", out);
 *                 M_free(out);
 *                 break;
 *             case M_EVENT_TYPE_WRITE:
 *                 // Write any data pending in the buffer.
 *                 M_io_write_from_buf(io, ldata->buf);
 *                 break;
 *             case M_EVENT_TYPE_DISCONNECTED:
 *                 break;
 *             case M_EVENT_TYPE_ACCEPT:
 *             case M_EVENT_TYPE_ERROR:
 *             case M_EVENT_TYPE_OTHER:
 *                 M_event_done(el);
 *                 break;
 *         }
 *     }
 *     
 *     int main(int argc, char *argv)
 *     {
 *         M_event_t       *el;
 *         M_io_t          *io;
 *         M_event_timer_t *timer;
 *         ldata_t          ldata;
 *     
 *         el = M_event_create(M_EVENT_FLAG_NONE);
 *     
 *         M_io_loopback_create(&io);
 *         ldata.buf    = M_buf_create();
 *         ldata.parser = M_parser_create(M_PARSER_FLAG_NONE);
 *         ldata.io     = io;
 *     
 *         M_event_add(el, io, run_cb, &ldata);
 *         M_event_queue_task(el, add_queued_data, &ldata);
 *     
 *         timer = M_event_timer_add(el, add_data, &ldata);
 *         M_event_timer_start(timer, 500);
 *         timer = M_event_timer_add(el, stop, NULL);
 *         M_event_timer_start(timer, 5000);
 *     
 *         M_event_loop(el, M_TIMEOUT_INF);
 *
 *         M_io_destroy(io);
 *         M_event_destroy(el);
 *         M_buf_cancel(ldata.buf);
 *         M_parser_destroy(ldata.parser);
 *         return 0;
 *     }
 * \endcode
 *
 * @{
 */

/*! Events that can be generated. 
 *
 * Events are enumerated in priority of delivery order
 */
enum M_event_type {
	M_EVENT_TYPE_CONNECTED    = 0, /*!< The connection has been completed                 */
	M_EVENT_TYPE_ACCEPT,           /*!< A new incoming connection is ready to be accepted */
	M_EVENT_TYPE_READ,             /*!< There is available data to be read                */
	M_EVENT_TYPE_DISCONNECTED,     /*!< The connection has been successfully disconnected.
	                                *    This is only triggered after a disconnect request,
	                                *    Otherwise most failures are determined by a Read
	                                *    event followed by a read failure.  The connection
	                                *    object should be closed after this.              */
	M_EVENT_TYPE_ERROR,            /*!< An error occurred.  Most likely during connection
	                                    establishment by a higher-level protocol.  The
	                                    connection object should be closed after this.    */
	M_EVENT_TYPE_WRITE,            /*!< There is room available in the write buffer       */
	M_EVENT_TYPE_OTHER             /*!< Some other event occurred, such as a triggered or
	                                *   timer-based event                                 */
};
/*! Events that can be generated. */
typedef enum M_event_type M_event_type_t;


/*! Opaque structure for event trigger */
struct M_event_trigger;

/*! Handle for an event trigger */
typedef struct M_event_trigger M_event_trigger_t;


/*! Opaque structure for event timers */
struct M_event_timer;

/*! Handle for an event timer */
typedef struct M_event_timer M_event_timer_t;


/*! Opaque structure for an event loop or pool handle */
struct M_event;

/* Handle for an event loop or pool handle */
typedef struct M_event M_event_t;


/*! Definition for a function callback that is called every time an event is triggered by
 *  the event subsystem.
 *
 *  \param[in] event  Internal event object, this is an event-thread specific object which could be
 *                    the member of a pool.  This object may be used to add new events to the same
 *                    event thread, or M_event_get_pool() can be used to retrieve the master pool
 *                    handle for distributing events across threads.
 *  \param[in] type   The type of event that has been triggered, see M_event_type_t.  Always 
 *                    M_EVENT_TYPE_OTHER for trigger, timer, and queued tasks.
 *  \param[in] io     Pointer to the M_io_t object associated with the event, or NULL for trigger,
 *                    timer, and queued tasks.
 *  \param[in] cb_arg User-specified callback argument registered when the object was added to the
 *                    event handle.
 */
typedef void (*M_event_callback_t)(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg);


/*! Possible list of flags that can be used when initializing an event loop */
enum M_EVENT_FLAGS {
	M_EVENT_FLAG_NONE                 = 0,      /*!< No specialized flags */
	M_EVENT_FLAG_NOWAKE               = 1 << 0, /*!< We will never need to wake the event loop from another thread */
	M_EVENT_FLAG_EXITONEMPTY          = 1 << 1, /*!< Exit the event loop when there are no registered events */
	M_EVENT_FLAG_EXITONEMPTY_NOTIMERS = 1 << 2  /*!< When combined with M_EVENT_FLAG_EXITONEMPTY, will ignore timers */
};


/*! Create a base event loop object.
 * 
 *  An event loop is typically run in the main process thread and will block until
 *  process termination.  IO and timer objects are enqueued into the event loop and
 *  dispatched within the event loop.  Event loops are more efficient and scalable
 *  than using a thread per tracked io object.
 *
 *  \param[in] flags One or more enum M_EVENT_FLAGS
 *
 *  \return Initialized event loop object.
 */
M_API M_event_t *M_event_create(M_uint32 flags);


/*! Create a pool of M_event_t objects bound to a master pool handle to distribute load
 *  of event handling across multiple threads.
 * 
 *  One thread per CPU core will be created for handling events, up to the maximum
 *  specified during creationg of the pool.  When an object is added to the event
 *  pool handle, an internal search is performed, and the least-loaded thread will
 *  receive the new object.
 *
 *  Objects bound to the same internal event object will always execute in the same thread
 *  which may be desirable for co-joined objects (otherwise additional locking may be
 *  required since multiple events could fire from different threads for some shared
 *  resource).  Typically this co-joined objects will be created based on events that
 *  have been fired, so the M_event_t object returned from the M_event_callback_t callback
 *  should be used to ensure they stay co-joined.
 *
 *  For non co-joined objects, always ensure the event handle used is the pool by calling
 *  M_event_get_pool() otherwise load will not be distributed at all.
 *
 *  \param[in] max_threads Artificial limitation on the maximum number of threads, the 
 *                         actual number of threads will be the lesser of this value
 *                         and the number of cpu cores in the system.  Use 0 for this
 *                         value to simply use the number of cpu cores.
 *
 *  \return Initialized event pool, or in the case only a single thread would be used,
 *          a normal event object.
 */
M_API M_event_t *M_event_pool_create(size_t max_threads);


/*! Retrieve the distributed pool handle for balancing the load across an event pool, or
 *  self if not part of a pool.
 *
 *  This should be called to get the event handle during M_event_add(), M_event_trigger_add(),
 *  M_event_timer_add(), M_event_timer_oneshot(), or M_event_queue_task() as by default
 *  tasks will otherwise not be distributed if using the event handle returned by the M_event_callback_t.
 *  In some cases it is desirable to ensure co-joined objects run within the same event thread
 *  and therefore desirable to enqueue multiple tasks for an internal event loop handle rather
 *  than the distributed pool.
 *
 *  \param[in] event Pointer to event handle either returned by M_event_create(), M_event_pool_create(),
 *                   or from an M_event_callback_t.
 *
 *  \return Pointer to event pool, or self if not part of a pool (or if a pool object already passed in).
 */
M_API M_event_t *M_event_get_pool(M_event_t *event);


/*! Destroy the event loop or pool object
 * 
 *  \param[in] event Pointer to event handle either returned by M_event_create(), M_event_pool_create(),
 *                   or from an M_event_callback_t.
 */
M_API void M_event_destroy(M_event_t *event);


/*! Add an io object to the event loop handle with a registered callback to deliver events to.
 * 
 *  Adding handles to an event handle is threadsafe and can be executed either within an event
 *  callback or from a separate thread.
 * 
 *  \param[in] event    Event handle to add the event to.  If desirable to ensure this io object
 *                      is distributed across a pool, it is recommended to pass the return value
 *                      of M_event_get_pool() rather than the event handle returned by an
 *                      M_event_callback_t callback.
 *  \param[in] io       IO object to bind to the event handle.
 *  \param[in] callback Callback to be called when events occur.
 *  \param[in] cb_data  Optional. User-defined callback data that will be passed to the user-defined
 *                      callback.  Use NULL if no data is necessary.
 *
 *  \return M_TRUE on success, or M_FALSE on failure (e.g. misuse, or io handle already bound to
 *          an event).
 */
M_API M_bool M_event_add(M_event_t *event, M_io_t *io, M_event_callback_t callback, void *cb_data);


/*! Edit the callback associated with an io object in the event subsystem.
 *
 *  Editing allows a user to re-purpose an io object while processing events without
 *  needing to remove and re-add the object which may cause a loss of events.
 *
 *  \note This will NOT cause a connected event to be triggered like M_event_add() does when
 *        you first add an io object to an event loop for already-established connections.
 *
 *  \param[in] io       IO object to modify the callback for
 *  \param[in] callback Callback to set. NULL will set it to no callback.
 *  \param[in] cb_data  Data passed to callback function.  NULL will remove the cb_data.
 *  \return M_FALSE on error, such as if the IO object is not currently attached to an
 *          event loop.
 */
M_API M_bool M_event_edit_io_cb(M_io_t *io, M_event_callback_t callback, void *cb_data);


/*! Remove an io object from its associated event handle.
 * 
 *  Removing handles is threadsafe and can be executed either within an event
 *  callback or from a separate thread.
 * 
 *  \param[in] io IO object.
 */
M_API void M_event_remove(M_io_t *io);


/*! Create a user-callable trigger which will call the pre-registered callback.  Useful for
 *  cross-thread completion or status update notifications.  Triggering events is threadsafe.
 * 
 *  \param[in] event    Event handle to add the event to.  If desirable to ensure this io object
 *                      is distributed across a pool, it is recommended to pass the return value
 *                      of M_event_get_pool() rather than the event handle returned by an
 *                      M_event_callback_t callback.
 *  \param[in] callback Callback to be called when the trigger is called.
 *  \param[in] cb_data  Optional. User-defined callback data that will be passed to the user-defined
 *                      callback.  Use NULL if no data is necessary.
 *
 *  \return handle to trigger to be used to execute callback, or NULL on failure
 */
M_API M_event_trigger_t *M_event_trigger_add(M_event_t *event, M_event_callback_t callback, void *cb_data);


/*! Remove the user-callable trigger, once removed, the trigger is no longer valid and cannot be called.
 *
 *  \param[in] trigger Trigger returned from M_event_trigger_add()
 */
M_API void M_event_trigger_remove(M_event_trigger_t *trigger);


/*! Signal the registered callback associated with the trigger to be called.  This is threadsafe and
 *  may be called cross thread.  If multiple signals are delivered before the callback is called, the
 *  duplicate signals will be silently discarded.
 *
 *  \param[in] trigger Trigger returned from M_event_trigger_add()
 */
M_API void M_event_trigger_signal(M_event_trigger_t *trigger);


/*! Add a timer object to the event loop specified that will call the user-supplied callback
 *  when the timer expires.  The timer is created in a stopped state and must be started before
 *  it will fire.
 * 
 *  If the timer is associated with another object (e.g. co-joined) then the same event handle
 *  as the other object should be used.
 * 
 *  \param[in] event    Event handle to add the timer to.  If the event handle is a pool object,
 *                      it will automatically distribute to an event thread.
 *  \param[in] callback User-specified callback to call when the timer expires
 *  \param[in] cb_data  Optional. User-specified data supplied to user-specified callback when
 *                      executed.
 *
 *  \return Timer handle on success, NULL on failure.
 */
M_API M_event_timer_t *M_event_timer_add(M_event_t *event, M_event_callback_t callback, void *cb_data);


/*! Starts the specified timer with timeout specified.  When the timeout expires, the
 *  callback associated with the timer will be executed.
 * 
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] interval_ms Time in milliseconds before the timer will expire.  May only be
 *                         0 if the configured "firecount" is 1. 
 *
 *  \return M_TRUE on success, M_FALSE on failure (such as timer already running or invalid use).
 */
M_API M_bool M_event_timer_start(M_event_timer_t *timer, M_uint64 interval_ms);


/*! Stops the specified timer.
 *
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *
 *  \return M_TRUE on success, M_FALSE if timer not running
 */
M_API M_bool M_event_timer_stop(M_event_timer_t *timer);


/*! Restart the timer.
 *
 *  If the timer is already stopped, will simply start it again.  If the timer 
 *  has "autoremove" configured, the removal will be skipped on stop.
 * 
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] interval_ms Time in milliseconds before the timer will expire.  
 *                         If specified as 0, will use the same interval_ms as the
 *                         original M_event_timer_start() call (NOTE: this is different
 *                         behavior than the value of 0 for M_event_timer_start())
 *
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_event_timer_reset(M_event_timer_t *timer, M_uint64 interval_ms);


/*! Set absolute time for first event to be fired.
 * 
 *  This will not take effect until the next call to M_event_timer_start() or M_event_timer_reset().
 *
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] start_tv    Absolute time of first event to be fired, or NULL to clear.
 *
 *  \return M_TRUE on success, M_FALSE on failure
 */
M_API M_bool M_event_timer_set_starttv(M_event_timer_t *timer, M_timeval_t *start_tv);


/*! Set absolute time for when the timer will automatically stop
 * 
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] end_tv      Absolute time of when to stop the timer, or NULL to clear.
 *
 *  \return M_TRUE on success, M_FALSE on failure
 */
M_API M_bool M_event_timer_set_endtv(M_event_timer_t *timer, M_timeval_t *end_tv);


/*! Set the maximum number of times the timer should fire.  Default is unlimited.
 * 
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] cnt         Maximum number of times timer should fire.  Use 0 for unlimited.
 *
 *  \return M_TRUE on success, M_FALSE on failure
 */
M_API M_bool M_event_timer_set_firecount(M_event_timer_t *timer, size_t cnt);


/*! Set the timer to automatically remove itself and free all used memory when the timer
 *  enters the stopped state.  This will happen when exceeding the fire count, exceeding
 *  the configured end_tv or explicitly calling M_event_timer_stop().  
 * 
 *  NOTE: Be careful not to attempt to use the timer handle once it has been autoremoved
 *        as it will result in access to uninitialized memory.
 * 
 *  \param[in] timer        Timer handle returned by M_event_timer_add()
 *  \param[in] enabled      M_TRUE to enable autoremove, M_FALSE to disable autoremove.
 *
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_event_timer_set_autoremove(M_event_timer_t *timer, M_bool enabled);

/*! Timer modes of operation */
enum M_event_timer_modes {
	M_EVENT_TIMER_MODE_RELATIVE  = 1, /*!< The interval will be added on to the end of the last actual run time */
	M_EVENT_TIMER_MODE_MONOTONIC = 2  /*!< The interval will be added on to the last scheduled run time, even if that
	                                   *   time has already passed.  This means you could have events that run closer
	                                   *   together than the specified interval if it is trying to "catch up" due to a
	                                   *   long running event handler.  In general this is more useful for needing an
	                                   *   event to run as close to a certain interval as possible without skewing
	                                   *   the interval between events by the amount of time it takes to handle event
	                                   *   callbacks. */
};
/*! Timer modes of operation */
typedef enum M_event_timer_modes M_event_timer_mode_t;


/*! Sets the timer mode of operation.
 *
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *  \param[in] mode        Defaults to M_EVENT_TIMER_MODE_RELATIVE if not specified.
 *
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_event_timer_set_mode(M_event_timer_t *timer, M_event_timer_mode_t mode);


/*! Retrieve number of milliseconds remaining on timer.
 *
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *
 *  \return Number of milliseconds remaining on timer, or 0 if stopped.
 */
M_API M_uint64 M_event_timer_get_remaining_ms(M_event_timer_t *timer);


/*! Retrieves if the timer is active(started) or not.
 *
 *  NOTE: Do not use with auto-destroy timers as the timer handle
 *        may not be valid if you don't already know the status.
 * 
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *
 *  \return M_TRUE if timer is started, M_FALSE if timer is stopped.
 */
M_API M_bool M_event_timer_get_status(M_event_timer_t *timer);


/*! Create a single-event timer.  
 *
 *  This is a convenience function equivalent to:
 *    M_event_timer_add(event, callback, cbdata) +
 *    M_event_timer_set_firecount(timer, 1) +
 *    M_event_timer_set_autoremove(timer, autoremove) +
 *    M_event_timer_start(timer, interval_ms)
 *
 *  \param[in] event       Event handle to add the timer to.  If the event handle is a pool object,
 *                         it will automatically distribute to an event thread.
 *  \param[in] interval_ms Time in milliseconds before the timer will expire. 
 *  \param[in] autoremove  Whether the timer should automatically remove itself when it fires.
 *  \param[in] callback    User-specified callback to call when the timer expires
 *  \param[in] cb_data     Optional. User-specified data supplied to user-specified callback when
 *                         executed.
 *
 *  \return Timer handle on success, NULL on failure.
 */
M_API M_event_timer_t *M_event_timer_oneshot(M_event_t *event, M_uint64 interval_ms, M_bool autoremove, M_event_callback_t callback, void *cb_data);


/*! Remove the timer and free all memory used by the timer.  
 * 
 *  If the timer isn't already stopped, this will prevent the timer from firing.
 *
 *  \param[in] timer       Timer handle returned by M_event_timer_add()
 *
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_event_timer_remove(M_event_timer_t *timer);


/*! Queue a task to run in the same thread as the event loop.
 * 
 *  This is threadsafe to call, and convenient when wanting to avoid
 *  additional locks when operating on an object in the event loop.
 *
 *  This is currently implemented as a oneshot timer set for 0ms. 
 *
 *  \param[in] event       Event handle to add task to.  Does not make sense to hand an event
 *                         pool object since the purpose is to choose the event loop to use.
 *  \param[in] callback    User-specified callback to call
 *  \param[in] cb_data     Optional. User-specified data supplied to user-specified callback when
 *                         executed.
 *
 *  \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_event_queue_task(M_event_t *event, M_event_callback_t callback, void *cb_data);

/*! Possible event status codes for an event loop or pool */
enum M_event_status {
	M_EVENT_STATUS_RUNNING = 0, /*!< The event loop is current running and processing events */
	M_EVENT_STATUS_PAUSED  = 1, /*!< The event loop is not running due to not being started or a timeout occurring */
	M_EVENT_STATUS_RETURN  = 2, /*!< The event loop was explicitly told to return using M_event_return() */
	M_EVENT_STATUS_DONE    = 3  /*!< The event loop either exited due to M_event_done() or there were no objects
	                             *   remaining as the event loop was initialized with M_EVENT_FLAG_EXITONEMPTY */
};

/*! Possible event status codes for an event loop or pool */
typedef enum M_event_status M_event_status_t;


/*! Possible return codes for M_event_loop() */
enum M_event_err {
	M_EVENT_ERR_DONE    = 1, /*!< The event loop either exited due to M_event_done() or M_event_done_with_disconnect()
	                          * or there were no objects remaining as the event loop was initialized with
	                          * M_EVENT_FLAG_EXITONEMPTY */
	M_EVENT_ERR_TIMEOUT = 2, /*!< The timeout specified in M_event_loop() has expired */
	M_EVENT_ERR_RETURN  = 3, /*!< M_event_return() was explicitly called */
	M_EVENT_ERR_MISUSE  = 4  /*!< Misuse, e.g. NULL event handle */
};

/*! Possible return codes for M_event_loop() */
typedef enum M_event_err M_event_err_t;


/*! Start the event loop to start processing events.
 * 
 *  Events will not be delivered unless the event loop is running.  If the event
 *  handle is a pool, will spawn threads for each member of the pool except one
 *  which will run and block the thread executing this function.
 *
 *  \param[in] event      Initialized event handle
 *  \param[in] timeout_ms Time in milliseconds to wait for events. Use M_TIMEOUT_INF to
 *                        wait until an explicit exit condition has been met, which is
 *                        the recommended way to run the event loop.
 *
 *  \return One of the M_event_err_t conditions.
 */
M_API M_event_err_t M_event_loop(M_event_t *event, M_uint64 timeout_ms);


/*! Exit the event loop immediately.
 * 
 *  This is safe to call from a thread other than the event loop.  Will set
 *  the M_EVENT_ERR_DONE return code for the event loop.
 *
 *  This will exit all threads for event pools as well, and if an event child
 *  handle is passed instead of the pool handle, it will automatically escalate
 *  to the pool handle.
 *
 *  This does not clean up the resources for the event loop and it is safe to
 *  re-execute the same event loop handle once it has returned.
 *
 *  \param[in] event      Initialized event handle
 */
M_API void M_event_done(M_event_t *event);


/*! Exit the event loop immediately.
 * 
 *  This is safe to call from a thread other than the event loop.  Will set
 *  the M_EVENT_ERR_RETURN return code for the event loop, this is the only
 *  way this call differs from M_event_done().
 *
 *  This will exit all threads for event pools as well, and if an event child
 *  handle is passed instead of the pool handle, it will automatically escalate
 *  to the pool handle.
 *
 *  This does not clean up the resources for the event loop and it is safe to
 *  re-execute the same event loop handle once it has returned.
 *
 *  \param[in] event      Initialized event handle
 */
M_API void M_event_return(M_event_t *event);


/*! Signal all IO objects in the event loop to start their disconnect sequence
 *  and exit the event loop when all are closed, or the specified timeout
 *  has elapsed.
 * 
 *  This is safe to call from a thread other than the event loop.  Will set
 *  the M_EVENT_ERR_DONE return code for the event loop.  The only difference
 *  between this and M_event_done() is it attempts to close the IO objects
 *  gracefully, some users may want to use this for program termination.
 *
 *  This will exit all threads for event pools as well, and if an event child
 *  handle is passed instead of the pool handle, it will automatically escalate
 *  to the pool handle.
 *
 *  This does not clean up the resources for the event loop and it is safe to
 *  re-execute the same event loop handle once it has returned.
 *
 *  \param[in] event       Initialized event handle
 *  \param[in] timeout_ms  Number of milliseconds to wait on IO handles to close
 *                         before giving up.  This should be set to some reasonable
 *                         number to accommodate for proper disconnect sequences.
 *                         A good starting point may be 5s (5000ms).
 */
M_API void M_event_done_with_disconnect(M_event_t *event, M_uint64 timeout_ms);


/*! Get the current running status of the event loop.
 * 
 *  If an event child handle is passed instead of the pool handle, it will
 *  automatically escalate to the pool handle.
 *
 *  \param[in] event       Initialized event handle
 *
 *  \return one of M_event_status_t results.
 */
M_API M_event_status_t M_event_get_status(M_event_t *event);


/*! Retrieve the number of milliseconds spent processing events, roughly equivalent to
 *  actual CPU time, not including idle time waiting on events to come in.
 *
 *  Will return results for the actual handle passed.  If the handle is a child of
 *  an event pool, it will only return the child's processing time.  If all processing
 *  time is desired, use M_event_get_pool() to get the pool handle before calling
 *  this function.
 *
 *  \param[in] event       Initialized event handle
 *
 *  \return milliseconds spent processing events.
 */
M_API M_uint64 M_event_process_time_ms(M_event_t *event);


/*! Retrieve the number of M_io_t objects plus the number of M_event_timer_t objects
 *  associated with an event handle
 *
 *  Will return results for the actual handle passed.  If the handle is a child of
 *  an event pool, it will only return the child's processing time.  If all processing
 *  time is desired, use M_event_get_pool() to get the pool handle before calling
 *  this function.
 *
 *  \param[in] event       Initialized event handle
 *
 *  \return count of objects.
  */
M_API size_t M_event_num_objects(M_event_t *event);

/*! @} */

__END_DECLS

#endif
