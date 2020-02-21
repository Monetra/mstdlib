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

#include "m_config.h"

#include <signal.h>

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#ifdef HAVE_PTHREAD_NP_H
#  include <pthread_np.h>
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
#include <string.h>

#ifdef __linux__
#  include <sys/syscall.h>
#  include <sys/resource.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

#ifdef __sun__
#  include <sys/types.h>
#  include <sys/processor.h>
#  include <sys/procset.h>
#endif

#ifdef _AIX
#  include <sys/thread.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_pthread_attr_topattr(const M_thread_attr_t *attr, pthread_attr_t *tattr)
{
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
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_pthread_init(void)
{
#ifdef HAVE_PTHREAD_INIT
	pthread_init();
#endif
}


static M_threadid_t M_thread_pthread_self(M_thread_t **thread)
{
	M_threadid_t rv = 0;

#if defined(__linux__)
	/* Get pid of thread.  Yes, linux actually assigns an internal pid of the thread
	 * due to use of clone(). */
	pid_t tid = (pid_t)syscall(__NR_gettid);
	rv = (M_threadid_t)tid;
#elif defined(_AIX)
	tid_t tid = thread_self();
	rv = (M_threadid_t)tid;
#else
	/* Generic */
	/* Yes, of course we could do this in a single line, but due to all the possible
	 * underlying datatypes of pthread_t, we actually need to do some work to prevent
	 * warnings on various systems */
	pthread_t id = pthread_self();
	void     *th = (void *)((M_uintptr)id);

	rv = (M_threadid_t)th;
#endif

	if (thread != NULL)
		*thread = (M_thread_t *)pthread_self();

/* NOTE: for apple should we instead return  thread_port_t pthread_mach_thread_np(pthread_self()); ?? */
	return rv;
}


static M_bool M_thread_pthread_set_priority(M_thread_t *thread, M_threadid_t tid, M_uint8 mthread_priority)
{
	struct sched_param tparam;
	int                sys_priority_min;
	int                sys_priority_max;
	int                sys_priority_range;
	int                mthread_priority_range;
	double             priority_scale;
	int                priority;
	M_bool             use_setpriority = M_FALSE;
	M_bool             rv              = M_TRUE;
	int                retval;

	M_mem_set(&tparam, 0, sizeof(tparam));

	sys_priority_min       = sched_get_priority_min(SCHED_OTHER);
	sys_priority_max       = sched_get_priority_max(SCHED_OTHER);
	sys_priority_range     = (sys_priority_max - sys_priority_min) + 1;

	/* Linux can't seem to use thread priorities, they say the range is 0-0, so we have to recalculate using process nice priorities */
	if (sys_priority_range <= 1) {
#ifdef __linux__
		use_setpriority = M_TRUE;
		/* Nice levels are -20 (highest priority) -> 19 (lowest priority), with normal being 0 */
		sys_priority_max       = -20;
		sys_priority_min       = 19;
		sys_priority_range     = M_ABS(sys_priority_max - sys_priority_min) + 1;
#endif
	}

	/* Lets handle min and max without scaling */
	if (mthread_priority == M_THREAD_PRIORITY_MAX) {
		priority = sys_priority_range - 1;
	} else if (mthread_priority == M_THREAD_PRIORITY_MIN) {
		priority = 0;
	} else {
		mthread_priority_range = (M_THREAD_PRIORITY_MAX - M_THREAD_PRIORITY_MIN) + 1;
		priority_scale         = ((double)sys_priority_range) / ((double)mthread_priority_range);
		priority               = (int)(((double)(mthread_priority - M_THREAD_PRIORITY_MIN)) * priority_scale);
	}

	/* check bounds */
	if (priority < 0)
		priority = 0;
	if (priority > sys_priority_range - 1)
		priority = sys_priority_range - 1;

	/* Check for inverted scale */
	if (sys_priority_max < sys_priority_min) {
		priority = (sys_priority_range - 1) - priority;
		priority += sys_priority_max;
	} else {
		/* Normalize into range */
		priority += sys_priority_min;
	}

	if (sys_priority_range > 1 && !use_setpriority) {
		/* Lets handle min and max without scaling */
		if (mthread_priority == M_THREAD_PRIORITY_MIN) {
			tparam.sched_priority = sys_priority_min;
		} else if (mthread_priority == M_THREAD_PRIORITY_MAX) {
			tparam.sched_priority = sys_priority_max;
		} else {
			tparam.sched_priority = priority;
		}
		retval = pthread_setschedparam((pthread_t)thread, SCHED_OTHER, &tparam);
		if (retval != 0) {
			M_fprintf(stderr, "Thread TID %lld: pthread_setschedparam %d (min %d, max %d): failed: %d: %s\n", (M_int64)tid, priority, sys_priority_min, sys_priority_max, retval, strerror(retval));
			rv = M_FALSE;
		}
#ifdef __linux__
	} else if (use_setpriority) {
		/* Set the Nice priority. This may fail if set higher than allowed.  I don't think
		 * its worth calling  getrlimit(RLIMIT_NICE) just to see if it might fail before calling it. */
		retval = setpriority(PRIO_PROCESS, (id_t)tid, priority);
		if (retval != 0) {
			M_fprintf(stderr, "Thread TID %lld: nice priority %d: failed: %d: %s\n", (M_int64)tid, priority, retval, strerror(errno));
			rv = M_FALSE;
		}
#endif
	} else {
		M_fprintf(stderr, "Thread TID %lld: could not determine how to set priority due to limited range\n", (M_int64)tid);
		rv = M_FALSE;
	}

	return rv;
}


static M_bool M_thread_pthread_set_processor(M_thread_t *thread, M_threadid_t tid, int processor_id)
{
#if defined(HAVE_PTHREAD_SETAFFINITY_NP)
#  if defined(HAVE_CPUSET_T)
	cpuset_t  cpuset;
#  elif defined(HAVE_CPU_SET_T)
	cpu_set_t cpuset;
#  else
#    error unknown cpuset data type
#  endif

	CPU_ZERO(&cpuset);
	if (processor_id == -1) {
		size_t i;
		for (i=0; i<M_thread_num_cpu_cores(); i++) {
			CPU_SET((int)i, &cpuset);
		}
	} else {
		CPU_SET(processor_id, &cpuset);
	}
	(void)tid;
	if (pthread_setaffinity_np((pthread_t)thread, sizeof(cpuset), &cpuset) != 0) {
		M_fprintf(stderr, "pthread_setaffinity_np thread %lld to processor %d failed\n", (M_int64)thread, processor_id);
		return M_FALSE;
	}
#elif defined(HAVE_SCHED_SETAFFINITY) && defined(HAVE_CPU_SET_T)
	/* Other C libraries on Linux may not wrap sched_setaffinity() into a pthread_setaffinity_np().  So lets support passing
	 * the tid to sched_setaffinity() which according to the docs has the same effect on linux. */
	cpu_set_t cpuset;

	(void)thread;

	CPU_ZERO(&cpuset);
	if (processor_id == -1) {
		size_t i;
		for (i=0; i<M_thread_num_cpu_cores(); i++) {
			CPU_SET((int)i, &cpuset);
		}
	} else {
		CPU_SET(processor_id, &cpuset);
	}

	if (sched_setaffinity(tid, sizeof(cpuset), &cpuset) != 0) {
		M_fprintf(stderr, "sched_setaffinity thread %lld to processor %d failed: %s\n", (M_int64)thread, processor_id, strerror(errno));
		return M_FALSE;
	}
#elif defined(__sun__)
	(void)tid;
	if (processor_bind(P_LWPID, (id_t)((pthread_t)thread), (processorid_t)((processor_id == -1)?PBIND_NONE:processor_id), NULL) != 0) {
		M_fprintf(stderr, "processor_bind thread %lld to processor %d failed\n", (M_int64)thread, processor_id);
		return M_FALSE;
	}
#elif defined(__APPLE__)
	thread_port_t                 mach_thread = pthread_mach_thread_np((pthread_t)thread);
	thread_affinity_policy_data_t policy      = { (processor_id == -1)?THREAD_AFFINITY_TAG_NULL:processor_id+1 };
	(void)tid;
	if (thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1) != KERN_SUCCESS) {
		M_fprintf(stderr, "thread_policy_set thread %lld to processor %d failed\n", (M_int64)thread, processor_id);
		return M_FALSE;
	}
#elif defined(_AIX)
	(void)thread;
	if (bindprocessor(BINDTHREAD, (id_t)tid, (cpu_t)((processor_id == -1)?PROCESSOR_CLASS_ANY:processor_id)) != 0) {
		M_fprintf(stderr, "bindprocessor thread %lld to processor %d failed\n", (M_int64)thread, processor_id);
		return M_FALSE;
	}
#elif defined(__ANDROID__)
#  warning thread affinity not supported on this OS

#else
#  error do not know how to set thread affinity
#endif
	return M_TRUE;
}



/*! Wrapper argument for thread startup for task */
typedef struct {
	void          *(*entry_func)(void *); /*!< Real function entry point */
	void            *entry_arg;           /*!< Passed in function entry argument */
	M_threadid_t    *thread_id;           /*!< Pointer to OS thread id */
	pthread_cond_t  *cond;                /*!< Pointer for Conditional for signalling when OS thread id has been filled */
	pthread_mutex_t *mutex;               /*!< Pointer for Mutex for OS thread id filling */
} M_thread_pthread_entry_arg_t;


/*! Some OS'd like Linux and AIX  have a concept of an OS Thread ID that is different
 *  then the pthread_t, and you cannot obtain this OS Thread ID by using the pthread_t
 *  but the thread itself can query it.  So we need a hook when a thread initializes
 *  to capture this data and pass it back to the parent */
static void *M_thread_pthread_entry(void *arg)
{
	M_thread_pthread_entry_arg_t   *larg                = arg;
	void                         *(*entry_func)(void *) = larg->entry_func;
	void                           *entry_arg           = larg->entry_arg;
	pthread_cond_t                 *cond                = larg->cond;
	pthread_mutex_t                *mutex               = larg->mutex;
	M_threadid_t                   *thread_id           = larg->thread_id;

	M_free(larg);
	larg = NULL;

	pthread_mutex_lock(mutex);
	*thread_id = M_thread_pthread_self(NULL);
	pthread_cond_signal(cond);
	pthread_mutex_unlock(mutex);
	return entry_func(entry_arg);
}


static M_thread_t *M_thread_pthread_create(M_threadid_t *id, const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	pthread_t                     thread;
	pthread_attr_t                tattr;
	int                           ret;
	M_thread_pthread_entry_arg_t *larg;
	pthread_cond_t                cond;
	pthread_mutex_t               mutex;

	*id = 0;

	if (func == NULL) {
		return NULL;
	}

	M_thread_pthread_attr_topattr(attr, &tattr);

	larg = M_malloc_zero(sizeof(*larg));
	larg->entry_func                 = func;
	larg->entry_arg                  = arg;
	larg->thread_id                  = id;
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	larg->cond                       = &cond;
	larg->mutex                      = &mutex;

	pthread_mutex_lock(&mutex);

	ret = pthread_create(&thread, &tattr, M_thread_pthread_entry, larg);
	pthread_attr_destroy(&tattr);

	if (ret != 0) {
		pthread_mutex_unlock(&mutex);
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
		M_free(larg);
		return NULL;
	}

	/* Wait for thread to signal us that it has set the OS thread ID */
	pthread_cond_wait(&cond, &mutex);

	/* Cleanup */
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);

	if (M_thread_attr_get_processor(attr) != -1) {
		M_thread_pthread_set_processor((M_thread_t *)thread, *id, M_thread_attr_get_processor(attr));
	}

	if (M_thread_attr_get_priority(attr) != M_THREAD_PRIORITY_NORMAL) {
		M_thread_pthread_set_priority((M_thread_t *)thread, *id, M_thread_attr_get_priority(attr));
	}

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
	ts.tv_sec  = (M_time_tv_sec_t)(abstime->tv_sec);
	ts.tv_nsec = (M_time_tv_usec_t)(abstime->tv_usec * 1000);

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
	cbs->thread_create        = M_thread_pthread_create;
	cbs->thread_join          = M_thread_pthread_join;
	cbs->thread_self          = M_thread_pthread_self;
	cbs->thread_yield         = M_thread_pthread_yield;
	cbs->thread_sleep         = M_thread_pthread_sleep;
	cbs->thread_set_priority  = M_thread_pthread_set_priority;
	cbs->thread_set_processor = M_thread_pthread_set_processor;
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
