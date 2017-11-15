/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#include <signal.h>

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#include <poll.h>

#if defined(PTHREAD_SLEEP_USE_SELECT) && defined(HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef PTHREAD_SLEEP_USE_NANOSLEEP
#  include <time.h>
#endif

#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/thread/m_thread_system.h>
#include "base/time/m_time_int.h"
#include "m_thread_int.h"
#include <errno.h>
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_pthread_attr_topattr(const M_thread_attr_t *attr, pthread_attr_t *tattr)
{
	struct sched_param tparam;

	pthread_attr_init(tattr);

	if (attr == NULL)
		return;

	if (M_thread_attr_get_create_joinable(attr)) {
		pthread_attr_setdetachstate(tattr, PTHREAD_CREATE_JOINABLE);
	} else {
		pthread_attr_setdetachstate(tattr, PTHREAD_CREATE_DETACHED);
	}

	if (M_thread_attr_get_stack_size(attr) > 0) {
		pthread_attr_setstacksize(tattr, M_thread_attr_get_stack_size(attr));
	} else {
		/* Some systems like SCO6 have unreasonably small thread stack
		 * sizes.  Make 128k a limit for 32bit systems and 256k a limit for
		 * 64bit systems */
		pthread_attr_setstacksize(tattr, 128 * 1024 * (sizeof(void *)/4));
	}

	M_mem_set(&tparam, 0, sizeof(tparam));
	tparam.sched_priority = M_thread_attr_get_priority(attr);
	pthread_attr_setschedparam(tattr, &tparam);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_pthread_init(void)
{
#ifdef HAVE_PTHREAD_INIT
	/* TODO: I think this was only needed for AIX 4.x, can probably take it out. */
	extern void pthread_init(void);
	pthread_init();
#endif
}

static M_thread_t *M_thread_pthread_create(M_threadid_t *id, const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	pthread_t      thread;
	pthread_attr_t tattr;
	int            ret;

	if (id)
		*id = 0;

	if (func == NULL) {
		return NULL;
	}

	M_thread_pthread_attr_topattr(attr, &tattr);
	ret = pthread_create(&thread, &tattr, func, arg);
	pthread_attr_destroy(&tattr);
	if (ret != 0) {
		return NULL;
	}

	if (id != NULL)
		*id = (M_threadid_t)thread;
	return (M_thread_t *)thread;
}

static M_bool M_thread_pthread_join(M_thread_t *thread, void **value_ptr)
{
	if (thread == NULL)
		return M_FALSE;

	if (pthread_join((pthread_t)thread, value_ptr) != 0)
		return M_FALSE;
	return M_TRUE;
}

static M_threadid_t M_thread_pthread_self(void)
{
	/* We use a couple of temporary variables in order to avoid
	 * compiler warnings. */
	pthread_t id = pthread_self();
	void     *th = (void *)id;
	return (M_threadid_t)th;
}

static void M_thread_pthread_sleep(M_uint64 usec)
{
#if defined(PTHREAD_SLEEP_USE_POLL)
	struct pollfd unused[1];

	M_mem_set(unused, 0, sizeof(unused));

	(void)poll(unused, 0, (int)(usec/1000));
#elif defined(PTHREAD_SLEEP_USE_SELECT)
	fd_set         readfs;
	struct timeval timeout;

	timeout.tv_sec  = usec / 1000000;
	timeout.tv_usec = usec % 1000000;
	FD_ZERO(&readfs);
	select(0, &readfs, NULL, NULL, &timeout);
#elif defined(PTHREAD_SLEEP_USE_NANOSLEEP)
	struct timespec rqtp;

	rqtp.tv_sec  = usec / (unsigned long)1000000;
	rqtp.tv_nsec = (usec % (unsigned long)1000000) * 1000;
	nanosleep(&rqtp, (struct timespec *)NULL);
#endif
}

static void M_thread_pthread_yield(M_bool force)
{
	if (!force)
		return;

#ifdef HAVE_PTHREAD_YIELD
	pthread_yield();
#else
	/* Wait shortest amount of time possible, should cause a reschedule */
	M_thread_pthread_sleep(1);
#endif
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int M_thread_pthread_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return poll(fds, nfds, timeout);
}

static M_bool M_thread_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	int ret;

	ret = pthread_sigmask(how, set, oldset);
	if (ret == 0)
		return M_TRUE;
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_mutex_t *M_thread_pthread_mutex_create(M_uint32 attr)
{
	M_thread_mutex_t    *mutex;
	pthread_mutexattr_t  myattr;
	int                  ret;

	pthread_mutexattr_init(&myattr);
	if (attr & M_THREAD_MUTEXATTR_RECURSIVE) {
		pthread_mutexattr_settype(&myattr, PTHREAD_MUTEX_RECURSIVE);
	} else {
		pthread_mutexattr_settype(&myattr, PTHREAD_MUTEX_DEFAULT);
	}
	/* NOTE: we never define "struct M_thread_mutex", as we're aliasing it to a
	 *       different type.  Bad style, but keeps our type safety */
	mutex = M_malloc_zero(sizeof(pthread_mutex_t));
	ret   = pthread_mutex_init((pthread_mutex_t *)mutex, &myattr);
	pthread_mutexattr_destroy(&myattr);

	if (ret == 0)
		return mutex;
	M_free(mutex);
	return NULL;
}

static void M_thread_pthread_mutex_destroy(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return;

	pthread_mutex_destroy((pthread_mutex_t *)mutex);
	M_free(mutex);
}

static M_bool M_thread_pthread_mutex_lock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (pthread_mutex_lock((pthread_mutex_t *)mutex) == 0)
		return M_TRUE;
	return M_FALSE;
}

static M_bool M_thread_pthread_mutex_trylock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (pthread_mutex_trylock((pthread_mutex_t *)mutex) == 0)
		return M_TRUE;
	return M_FALSE;
}

static M_bool M_thread_pthread_mutex_unlock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (pthread_mutex_unlock((pthread_mutex_t *)mutex) == 0)
		return M_TRUE;
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_cond_t *M_thread_pthread_cond_create(M_uint32 attr)
{
	M_thread_cond_t *cond;

	(void)attr;
	/* NOTE: we never define "struct M_thread_cond", as we're aliasing it to a
	 *       different type.  Bad style, but keeps our type safety */
	cond = M_malloc_zero(sizeof(pthread_cond_t));
	if (pthread_cond_init((pthread_cond_t *)cond, NULL) == 0)
		return cond;
	M_free(cond);
	return NULL;
}

static void M_thread_pthread_cond_destroy(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;
	pthread_cond_destroy((pthread_cond_t *)cond);
	M_free(cond);
}

static M_bool M_thread_pthread_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	struct timespec ts;

	if (cond == NULL || mutex == NULL)
		return M_FALSE;

	M_mem_set(&ts, 0, sizeof(ts));
	ts.tv_sec  = abstime->tv_sec;
	ts.tv_nsec = abstime->tv_usec * 1000;

	return pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex, &ts)==0?M_TRUE:M_FALSE;
}

static M_bool M_thread_pthread_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	if (cond == NULL || mutex == NULL)
		return M_FALSE;
	return pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex)==0?M_TRUE:M_FALSE;
}

static void M_thread_pthread_cond_broadcast(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;
	pthread_cond_broadcast((pthread_cond_t *)cond);
}

static void M_thread_pthread_cond_signal(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;
	pthread_cond_signal((pthread_cond_t *)cond);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef HAVE_PTHREAD_RWLOCK_INIT
static M_thread_rwlock_t *M_thread_pthread_rwlock_create(void)
{
	M_thread_rwlock_t    *rwlock;
	pthread_rwlockattr_t  attr;
	int                   ret;

	pthread_rwlockattr_init(&attr);
#if defined(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP) && defined(PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)
	pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

	/* NOTE: we never define "struct M_thread_rwlock", as we're aliasing it to a
	 *       different type.  Bad style, but keeps our type safety */
	rwlock = M_malloc_zero(sizeof(pthread_rwlock_t));
	ret = pthread_rwlock_init((pthread_rwlock_t *)rwlock, &attr);
	pthread_rwlockattr_destroy(&attr);
	if (ret == 0)
		return rwlock;
	M_free(rwlock);
	return NULL;
}

static void M_thread_pthread_rwlock_destroy(M_thread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return;
	pthread_rwlock_destroy((pthread_rwlock_t *)rwlock);
	M_free(rwlock);
}

static M_bool M_thread_pthread_rwlock_lock(M_thread_rwlock_t *rwlock, M_thread_rwlock_type_t type)
{
	if (rwlock == NULL)
		return M_FALSE;

	if (type == M_THREAD_RWLOCK_TYPE_READ) {
		if (pthread_rwlock_rdlock((pthread_rwlock_t *)rwlock) == 0) {
			return M_TRUE;
		}
	} else {
		if (pthread_rwlock_wrlock((pthread_rwlock_t *)rwlock) == 0) {
			return M_TRUE;
		}
	}

	return M_FALSE;
}

static M_bool M_thread_pthread_rwlock_unlock(M_thread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return M_FALSE;
	if (pthread_rwlock_unlock((pthread_rwlock_t *)rwlock) == 0)
		return M_TRUE;
	return M_FALSE;
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_thread_pthread_register(M_thread_model_callbacks_t *cbs)
{
	if (cbs == NULL)
		return;

	M_mem_set(cbs, 0, sizeof(*cbs));

	cbs->init   = M_thread_pthread_init;
	cbs->deinit = NULL;
	/* Thread */
	cbs->thread_create  = M_thread_pthread_create;
	cbs->thread_join    = M_thread_pthread_join;
	cbs->thread_self    = M_thread_pthread_self;
	cbs->thread_yield   = M_thread_pthread_yield;
	cbs->thread_sleep   = M_thread_pthread_sleep;
	/* System */
	cbs->thread_poll    = M_thread_pthread_poll;
	cbs->thread_sigmask = M_thread_pthread_sigmask;
	/* Mutex */
	cbs->mutex_create   = M_thread_pthread_mutex_create;
	cbs->mutex_destroy  = M_thread_pthread_mutex_destroy;
	cbs->mutex_lock     = M_thread_pthread_mutex_lock;
	cbs->mutex_trylock  = M_thread_pthread_mutex_trylock;
	cbs->mutex_unlock   = M_thread_pthread_mutex_unlock;
	/* Cond */
	cbs->cond_create    = M_thread_pthread_cond_create;
	cbs->cond_destroy   = M_thread_pthread_cond_destroy;
	cbs->cond_timedwait = M_thread_pthread_cond_timedwait;
	cbs->cond_wait      = M_thread_pthread_cond_wait;
	cbs->cond_broadcast = M_thread_pthread_cond_broadcast;
	cbs->cond_signal    = M_thread_pthread_cond_signal;
	/* Read Write Lock */
#ifdef HAVE_PTHREAD_RWLOCK_INIT
	cbs->rwlock_create  = M_thread_pthread_rwlock_create;
	cbs->rwlock_destroy = M_thread_pthread_rwlock_destroy;
	cbs->rwlock_lock    = M_thread_pthread_rwlock_lock;
	cbs->rwlock_unlock  = M_thread_pthread_rwlock_unlock;
#else
	cbs->rwlock_create  = M_thread_rwlock_emu_create;
	cbs->rwlock_destroy = M_thread_rwlock_emu_destroy;
	cbs->rwlock_lock    = M_thread_rwlock_emu_lock;
	cbs->rwlock_unlock  = M_thread_rwlock_emu_unlock;
#endif
}
