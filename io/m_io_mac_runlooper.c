#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_io_hid.h>
#include "m_io_int.h"

#include <CoreFoundation/CoreFoundation.h>

CFRunLoopRef             M_io_mac_runloop = NULL;
static M_thread_once_t   loop_starter     = M_THREAD_ONCE_STATIC_INITIALIZER;
static CFRunLoopTimerRef loop_timer       = NULL;
static M_threadid_t      loop_thread      = 0;
static M_thread_mutex_t *looper_lock      = NULL;
static M_thread_cond_t  *looper_cond      = NULL;

/* no-op. We need a timer callback that doesn't do anything. */
static void M_io_mac_runloop_fire(CFRunLoopTimerRef timer, void *info)
{
	(void)timer;
	(void)info;
}

static void M_io_mac_runloop_stop(void *arg)
{
	(void)arg;

	if (M_io_mac_runloop == NULL)
		return;

	/* Signal the runloop to stop and wait for it to do so. */
	CFRunLoopStop(M_io_mac_runloop);
	M_thread_join(loop_thread, NULL);
	M_io_mac_runloop = NULL;
	M_thread_once_reset(&loop_starter);
}

static void *M_io_mac_runloop_runner(void *arg)
{
	(void)arg;

	M_thread_mutex_lock(looper_lock);

	/* Every thread has a runloop associated with it. We need to use a thread
 	 * because we don't want to get the main thread's runloop. If we did we'd
	 * end up blocking the entire application.
	 *
	 * Later when we call CFRunLoopRun there isn't a reference because internally
	 * it's using the runloop object we're getting here. This doesn't create a
	 * new object. Instead it gets the runloop already present for this thread.
	 * We need to store it because we can only access it using this function
	 * from within this thread. To stop the runloop during exit we'll need the
	 * reference. */
	M_io_mac_runloop = CFRunLoopGetCurrent();
	M_library_cleanup_register(M_io_mac_runloop_stop, NULL);

	/* Runloops exit once there are no sources, timers, or observers
 	 * associated.  If we just start the runloop it will immediately exit
	 * because nothing is attached to it. To prevent this we create and attach
	 * a timer. The timer is set to run very far in the future (2069) and at a
	 * very large interval (68 years). Basically, it will never be called and
	 * is a light weight way to keep the event loop running.  The callback is a
	 * no-op so even if it were to be called it's not going to impact anything. */
	loop_timer = CFRunLoopTimerCreate(NULL, M_INT32_MAX, M_INT32_MAX, 0, 0, M_io_mac_runloop_fire, NULL);
	CFRunLoopAddTimer(M_io_mac_runloop, loop_timer, kCFRunLoopCommonModes);

	/* Let the initialization code know we have a runloop. It doesn't matter that
 	 * the signal goes out before the loop it started. Things can be added before it
	 * starts and they'll start receiving events once the runloop does start. */
	M_thread_cond_signal(looper_cond);
	M_thread_mutex_unlock(looper_lock);

	/* Blocks the thread until it's told to stop. */
	CFRunLoopRun();

	return NULL;
}

static void M_io_mac_runloop_starter(M_uint64 flags)
{
	M_thread_attr_t *tattr;

	(void)flags;

	/* This is nasty but we need to have the event loop created before
 	 * this function exits. Since the event loop starts in a thread
	 * the only way we can do this is to block until it's running.
	 * This shouldn't be more than nano seconds so it's not going
	 * to be a problem.
	 *
	 * If we don't do this we could get into a situation where something
	 * initializes the run loop then tries to use the object and due to thread
	 * startup timing it could have a NULL object or read partial write
	 * of the variable's value. */
	looper_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	looper_cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);

	M_thread_mutex_lock(looper_lock);

	/* We'll join the thread on shutdown so we can fully exit
 	 * the runloop when it's told to. */
	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	loop_thread = M_thread_create(tattr, M_io_mac_runloop_runner, NULL);
	M_thread_attr_destroy(tattr);

	M_thread_cond_wait(looper_cond, looper_lock);
	M_thread_mutex_unlock(looper_lock);

	/* We don't need these anymore. */
	M_thread_cond_destroy(looper_cond);
	M_thread_mutex_destroy(looper_lock);
	looper_cond = NULL;
	looper_lock = NULL;
}

void M_io_mac_runloop_start(void)
{
	M_thread_once(&loop_starter, M_io_mac_runloop_starter, 0);
}
