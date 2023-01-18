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

#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/thread/m_thread_system.h>
#include "m_thread_int.h"
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif
#include "m_defs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_model_t            thread_model            = M_THREAD_MODEL_INVALID;
static M_thread_model_callbacks_t  thread_cbs;
static M_thread_mutex_t           *threadid_mutex          = NULL;
static M_hash_u64vp_t             *threadid_map            = NULL;
static M_thread_mutex_t           *thread_count_mutex      = NULL;
static M_uint64                    thread_count            = 0;   /* M_uint64 so we can use atomics (instead of size_t) */
static M_thread_mutex_t           *thread_destructor_mutex = NULL;
static M_list_t                   *thread_destructors      = NULL;
#ifdef __linux__
static M_list_u64_t               *thread_cpus             = NULL; /* List of available CPUs, for proper handling of linux containers */
#endif

static struct {
	M_thread_model_t  id;
	const char       *name;
	void (*register_func)(M_thread_model_callbacks_t *cbs);
} M_thread_models[] = {
	/* Native */
#if defined(_WIN32)
	{ M_THREAD_MODEL_NATIVE, "native - win32",   M_thread_win_register     },
#elif defined(HAVE_PTHREAD) || defined(__ANDROID__) || defined(IOS) || defined(IOSSIM)
	{ M_THREAD_MODEL_NATIVE, "native - pthread", M_thread_pthread_register },
#else
	{ M_THREAD_MODEL_NATIVE, "native - coop",    M_thread_coop_register    },
#endif
	/* Coop */
#if !defined(__ANDROID__) && !defined(IOS)
	{ M_THREAD_MODEL_COOP,   "coop",             M_thread_coop_register    },
#endif
	{ M_THREAD_MODEL_INVALID, NULL, NULL }
};

typedef struct {
	void             *(*func)(void *arg);
	void             *arg;
	M_bool            joinable;
	M_threadid_t     *thread_id;  /*!< Pointer to OS thread id */
	M_thread_cond_t  *cond;       /*!< Pointer for Conditional for signalling when OS thread id has been filled */
	M_thread_mutex_t *mutex;      /*!< Pointer for Mutex for OS thread id filling */
} M_thread_wrapfunc_data_t;

static M_bool M_thread_once_int(M_thread_once_t *once_control, M_bool atomics_only, void (*init_routine)(M_uint64 flags), M_uint64 init_flags);
static M_bool M_thread_once_reset_int(M_thread_once_t *once_control, M_bool atomics_only);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


static void *M_thread_wrapfunc(void *arg)
{
	M_thread_wrapfunc_data_t *data = arg;
	void                     *ret;
	size_t                    len;
	size_t                    i;
	void                   (**destructors)(void);

	M_thread_mutex_lock(thread_count_mutex);
	thread_count++;
	M_thread_mutex_unlock(thread_count_mutex);


	/* Notify parent we have set the thread id */
	M_thread_mutex_lock(data->mutex);
	*(data->thread_id) = thread_cbs.thread_self(NULL);
	M_thread_cond_signal(data->cond);

	/* Wait for parent to say it is ok to continue */
	M_thread_cond_wait(data->cond, data->mutex);

	/* Destroy unneeded data */
	M_thread_mutex_unlock(data->mutex);
	M_thread_cond_destroy(data->cond);
	M_thread_mutex_destroy(data->mutex);
	data->cond      = NULL;
	data->mutex     = NULL;
	data->thread_id = NULL;

	/* Run user function */
	ret         = data->func(data->arg);

	M_thread_mutex_lock(thread_destructor_mutex);
	len         = M_list_len(thread_destructors);
	destructors = M_malloc(sizeof(void (*)(void))*len);

	for (i=0; i<len; i++) {
		const void *dst = M_list_at(thread_destructors, i);
		destructors[i] = M_CAST_OFF_CONST(void *, dst);
	}

	M_thread_mutex_unlock(thread_destructor_mutex);

	for (i=0; i<len; i++) {
		destructors[i]();
	}

	M_free(destructors);

	M_thread_mutex_lock(thread_count_mutex);
	thread_count--;
	M_thread_mutex_unlock(thread_count_mutex);

	if (!data->joinable) {
		M_threadid_t id = M_thread_self();
		M_thread_mutex_lock(threadid_mutex);
		M_hash_u64vp_remove(threadid_map, id, M_FALSE);
		M_thread_mutex_unlock(threadid_mutex);
	}

	M_free(data);

	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_once_t M_thread_init_once = M_THREAD_ONCE_STATIC_INITIALIZER;

static void M_thread_deinit(void *arg)
{
	(void)arg;

	if (thread_model == M_THREAD_MODEL_INVALID || M_thread_count() != 0 || M_hash_u64vp_num_keys(threadid_map) > 1 /* Main thread is counted */)
		return;

	if (!M_thread_once_reset_int(&M_thread_init_once, M_TRUE))
		return;

	if (thread_cbs.deinit != NULL)
		thread_cbs.deinit();

	M_thread_tls_deinit();

	M_thread_mutex_destroy(threadid_mutex);
	threadid_mutex = NULL;
	M_hash_u64vp_destroy(threadid_map, M_FALSE);
	threadid_map = NULL;

	M_thread_mutex_destroy(thread_count_mutex);
	thread_count_mutex = NULL;

	M_thread_mutex_destroy(thread_destructor_mutex);
	thread_destructor_mutex = NULL;

	M_list_destroy(thread_destructors, M_FALSE);
	thread_destructors = NULL;

	M_mem_set(&thread_cbs, 0, sizeof(thread_cbs));
	thread_model = M_THREAD_MODEL_INVALID;

#ifdef __linux__
	M_list_u64_destroy(thread_cpus);
	thread_cpus = NULL;
#endif

}


static void M_thread_init_routine(M_uint64 flags)
{
	size_t i;
	M_thread_t  *thread = NULL;
	M_threadid_t threadid = 0;

	M_thread_model_t model = (M_thread_model_t)flags;

	thread_model = model;
	for (i=0; M_thread_models[i].id!=M_THREAD_MODEL_INVALID; i++) {
		if (thread_model == M_thread_models[i].id) {
			M_thread_models[i].register_func(&thread_cbs);
			break;
		}
	}

	if (thread_cbs.init != NULL)
		thread_cbs.init();

	M_thread_tls_init();

	threadid_mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	threadid_map   = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, NULL);

	/* Add self to thread map -- needed for things like setting a priority/processor */
	threadid = thread_cbs.thread_self(&thread);
	M_hash_u64vp_insert(threadid_map, threadid, thread);

	thread_count_mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	thread_destructor_mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	thread_destructors      = M_list_create(NULL, M_LIST_SET_PTR);
	M_thread_destructor_insert(M_thread_tls_purge_thread);

#ifdef __linux__
	/* Support for containerized environments */
	if (thread_model != M_THREAD_MODEL_COOP) {
		long      max_cpus = M_MAX(sysconf(_SC_NPROCESSORS_ONLN), sysconf(_SC_NPROCESSORS_CONF));
		cpu_set_t cs;

		thread_cpus = M_list_u64_create(M_LIST_U64_NONE);

		CPU_ZERO(&cs);

		if (max_cpus < 1)
			max_cpus = 1;

		if (sched_getaffinity(0, sizeof(cs), &cs) == 0) {
			for (i=0; i<(size_t)max_cpus; i++) {
				if (CPU_ISSET(i, &cs)) {
					M_list_u64_insert(thread_cpus, i);
				}
			}
		}

		if (M_list_u64_len(thread_cpus) == 0){
			/* Assume exactly 1 cpu: 0 */
			M_list_u64_insert(thread_cpus, 0);
		}
	}
#endif
}


M_bool M_thread_init(M_thread_model_t model)
{
	M_bool retval;

	if (thread_model != M_THREAD_MODEL_INVALID || (model != M_THREAD_MODEL_NATIVE && model != M_THREAD_MODEL_COOP))
		return M_FALSE;

	retval = M_thread_once_int(&M_thread_init_once, M_TRUE, M_thread_init_routine, (M_uint64)model);
	if (retval) {
		/* Register self for destruction ... can't do it ini the actual init routine as this cleanup_register function
		 * itself could recursively call into the thread initializer! */
		M_library_cleanup_register(M_thread_deinit, NULL);
	}
	return retval;
}

static void M_thread_auto_init(void)
{
	/* Initalize using the system's native thread model. */
	M_thread_init(M_THREAD_MODEL_NATIVE);
}


M_bool M_thread_active_model(M_thread_model_t *model, const char **name)
{
	size_t i;

	if (thread_model == M_THREAD_MODEL_INVALID)
		return M_FALSE;

	if (model != NULL)
		*model = thread_model;

	if (name != NULL) {
		*name = NULL;
		for (i=0; M_thread_models[i].id!=M_THREAD_MODEL_INVALID; i++) {
			if (thread_model == M_thread_models[i].id) {
				*name = M_thread_models[i].name;
				break;
			}
		}
	}

	return M_TRUE;
}


static size_t M_thread_num_cpu_cores_int(void)
{
	int count = 0;

#ifdef _WIN32
	SYSTEM_INFO sysinfo;

	GetSystemInfo(&sysinfo);
	count = (int)sysinfo.dwNumberOfProcessors;
#elif defined(__linux__)
	count = (int)M_list_u64_len(thread_cpus);
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
	count = (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROC_ONLN)
	count = (int)sysconf(_SC_NPROC_ONLN);
#elif defined(HW_AVAILCPU) || defined(HW_NCPU)
	int    mib[4];
	size_t len    = sizeof(count);

	/* set the mib for hw.ncpu */
	mib[0] = CTL_HW;
#  if defined(HW_AVAILCPU)
	mib[1] = HW_AVAILCPU;

	/* get the number of CPUs from the system */
	sysctl(mib, 2, &count, &len, NULL, 0);
#  endif /* HW_AVAILCPU */

#  if defined(HW_NCPU)
	if (count < 1) {
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &count, &len, NULL, 0);
	}
#  endif /* HW_NCPU */
#else
	count = 0;
#endif

	if (count < 1)
		return 0;

	return (size_t)count;
}


size_t M_thread_num_cpu_cores(void)
{
	M_thread_auto_init();

	/* If in cooperative threading mode, only 1 cpu core can be used even
	 * if more are available */
	if (thread_model != M_THREAD_MODEL_NATIVE)
		return 1;

	return M_thread_num_cpu_cores_int();
}

#ifdef __linux__
void M_thread_linux_cpu_set(cpu_set_t *set, int cpu)
{
	int real_cpu = (int)M_list_u64_at(thread_cpus, (size_t)cpu);
	CPU_SET(real_cpu, set);
}
#endif

M_bool M_thread_destructor_insert(void (*destructor)(void))
{
	M_bool ret = M_FALSE;

	if (destructor == NULL)
		return M_FALSE;

	M_thread_auto_init();

	M_thread_mutex_lock(thread_destructor_mutex);
	ret = M_list_insert(thread_destructors, destructor);
	M_thread_mutex_unlock(thread_destructor_mutex);

	return ret;
}

void M_thread_destructor_remove(void (*destructor)(void))
{
	M_thread_auto_init();

	M_thread_mutex_lock(thread_destructor_mutex);
	M_list_remove_val(thread_destructors, destructor, M_LIST_MATCH_PTR);
	M_thread_mutex_unlock(thread_destructor_mutex);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_spinlock_lock_int(M_thread_spinlock_t *spinlock, M_bool atomics_only /* TRUE for pre-thread init */)
{
	M_uint32 myqueue;
	M_uint32 current;
	M_uint32 spins = 0;

	/* Atomic increment the queue position, return our position */
	myqueue = M_atomic_inc_u32(&spinlock->queue);

	/* Wait until the current queue position matches our assigned position using
	 * reads only.  We should be guaranteed that a read of a 32bit integer
	 * is both cheap and reliable (we won't read a corrupt value) */
	while ((current = spinlock->current) != myqueue) {

		M_uint32 diff;

		if (!atomics_only) {
			spins++;

			/* For every 500 spins, backoff */
			if (spins % 500 != 0) {
				continue;
			}

			/* Back off based on slot, cap at 13 -- 8ms sleep */
			if (myqueue > current) {
				/* Handle wrap */
				diff = (M_UINT32_MAX - myqueue) + current;
			} else {
				diff = myqueue - current;
			}
			if (diff > 13)
				diff = 13;

			if (diff > 8) {
				/* Force a delay */
				M_thread_sleep(((M_uint64)1 << diff) + 1000);
			} else {
				/* Tell kernel to reschedule us, should be less of a hit than a delay */
				M_thread_yield(M_TRUE);
			}
		}
	}
	if (!atomics_only)
		spinlock->threadid = M_thread_self();
}


static void M_thread_spinlock_unlock_int(M_thread_spinlock_t *spinlock, M_bool atomics_only /* TRUE for pre-thread init */)
{
	/* Protects against rogue unlocks and double unlocks */
	if (!atomics_only && spinlock->threadid != M_thread_self())
		return;

	/* Since only the holder of the lock can ever increment the current
	 * counter, this should be safe */
	spinlock->threadid = 0;
	M_atomic_inc_u32(&spinlock->current);
}


void M_thread_spinlock_lock(M_thread_spinlock_t *spinlock)
{
	M_thread_spinlock_lock_int(spinlock, M_FALSE);
}

void M_thread_spinlock_unlock(M_thread_spinlock_t *spinlock)
{
	M_thread_spinlock_unlock_int(spinlock, M_FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_t *M_thread_from_threadid(M_threadid_t threadid)
{
	M_thread_t *thread = NULL;

	M_thread_mutex_lock(threadid_mutex);
	thread = M_hash_u64vp_get_direct(threadid_map, threadid);
	M_thread_mutex_unlock(threadid_mutex);
	return thread;
}

M_threadid_t M_thread_create(const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	M_thread_wrapfunc_data_t *data;
	M_thread_t               *thread;
	M_threadid_t              id     = 0;
	M_thread_attr_t          *myattr = NULL;
	M_thread_cond_t          *cond   = NULL;
	M_thread_mutex_t         *mutex  = NULL;

	M_thread_auto_init();
	if (thread_cbs.thread_create == NULL)
		return 0;

	data            = M_malloc(sizeof(*data));
	data->func      = func;
	data->arg       = arg;
	cond            = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	mutex           = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	data->cond      = cond;
	data->mutex     = mutex;
	data->thread_id = &id;
	data->joinable  = M_thread_attr_get_create_joinable(attr);

	/* ensure an attr is created so threads are setup properly (in a detachted state by default). */
	if (attr == NULL) {
		myattr = M_thread_attr_create();
		attr   = myattr;
	}

	M_thread_mutex_lock(mutex);

	thread = thread_cbs.thread_create(attr, M_thread_wrapfunc, data);
	if (thread == NULL) {
		M_thread_mutex_unlock(mutex);
		M_thread_cond_destroy(cond);
		M_thread_mutex_destroy(mutex);
		M_free(data);
		if (myattr != NULL) {
			M_thread_attr_destroy(myattr);
		}
		return 0;
	}

	/* Wait to be signaled that the thread id has been set by the thread */
	M_thread_cond_wait(cond, mutex);

	/* Add to thread map */
	M_thread_mutex_lock(threadid_mutex);
	M_hash_u64vp_insert(threadid_map, id, thread);
	M_thread_mutex_unlock(threadid_mutex);

	/* Set thread processor and priority if necessary */
	if (M_thread_attr_get_processor(attr) != -1) {
		M_thread_set_processor(id, M_thread_attr_get_processor(attr));
	}

	if (M_thread_attr_get_priority(attr) != M_THREAD_PRIORITY_NORMAL) {
		M_thread_set_priority(id, M_thread_attr_get_priority(attr));
	}

	/* Signal thread its ok to continue as we've recorded the necessary information
	 * NOTE:  it is the responsibility of the thread to destroy cond and mutex */
	M_thread_cond_signal(cond);
	M_thread_mutex_unlock(mutex);

	if (myattr != NULL) {
		M_thread_attr_destroy(myattr);
	}

	return id;
}


M_bool M_thread_join(M_threadid_t id, void **value_ptr)
{
	M_thread_t *thread;
	M_bool      rv;

	M_thread_auto_init();
	if (thread_cbs.thread_join == NULL)
		return M_FALSE;

	M_thread_mutex_lock(threadid_mutex);
	thread = M_hash_u64vp_get_direct(threadid_map, id);
	M_thread_mutex_unlock(threadid_mutex);

	if (thread == NULL)
		return M_FALSE;

	rv = thread_cbs.thread_join(thread, value_ptr);

	if (rv) {
		M_thread_mutex_lock(threadid_mutex);
		M_hash_u64vp_remove(threadid_map, id, M_FALSE);
		M_thread_mutex_unlock(threadid_mutex);
	}

	return rv;
}

M_threadid_t M_thread_self(void)
{
	M_thread_t  *thread = NULL;
	M_threadid_t id     = 0;

	M_thread_auto_init();
	if (thread_cbs.thread_self == NULL)
		return 0;

	id = thread_cbs.thread_self(&thread);

	return id;
}

M_bool M_thread_set_priority(M_threadid_t tid, M_uint8 priority)
{
	M_thread_t *thread;
	M_thread_auto_init();

	if (priority < M_THREAD_PRIORITY_MIN || priority > M_THREAD_PRIORITY_MAX)
		return M_FALSE;

	if (thread_cbs.thread_set_priority == NULL)
		return M_FALSE;

	thread = M_thread_from_threadid(tid);
	if (thread == NULL) {
		M_fprintf(stderr, "%s(): ThreadID %lld could not find Thread pointer\n", __FUNCTION__, (M_int64)tid);
		return M_FALSE;
	}
	return thread_cbs.thread_set_priority(thread, tid, priority);
}

M_bool M_thread_set_processor(M_threadid_t tid, int processor_id)
{
	M_thread_t *thread;
	M_thread_auto_init();

	/* NOTE: -1 is valid to unbind a thread from a processor */
	if (processor_id < -1 || processor_id >= (int)M_thread_num_cpu_cores())
		return M_FALSE;

	if (thread_cbs.thread_set_processor == NULL)
		return M_FALSE;

	thread = M_thread_from_threadid(tid);
	if (thread == NULL) {
		M_fprintf(stderr, "%s(): ThreadID %lld could not find Thread pointer\n", __FUNCTION__, (M_int64)tid);
		return M_FALSE;
	}

	return thread_cbs.thread_set_processor(thread, tid, processor_id);
}


void M_thread_yield(M_bool force)
{
	M_thread_auto_init();
	if (thread_cbs.thread_yield == NULL)
		return;
	thread_cbs.thread_yield(force);
}

size_t M_thread_count(void)
{
	size_t ret;

	M_thread_mutex_lock(thread_count_mutex);
	ret = (size_t)thread_count;
	M_thread_mutex_unlock(thread_count_mutex);
	return ret;
}

void M_thread_sleep(M_uint64 usec)
{
	M_thread_auto_init();
	if (thread_cbs.thread_sleep == NULL)
		return;
	thread_cbs.thread_sleep(usec);
}


static M_bool M_thread_once_int(M_thread_once_t *once_control, M_bool atomics_only, void (*init_routine)(M_uint64 flags), M_uint64 init_flags)
{
	M_bool retval = M_FALSE;

	if (once_control == NULL || init_routine == NULL)
		return M_FALSE;

	if (once_control->initialized)
		return M_FALSE;

	M_thread_spinlock_lock_int(&once_control->spinlock, atomics_only);
	/* check to see if another thread beat us here */
	if (!once_control->initialized) {
		init_routine(init_flags);
		once_control->initialized = M_TRUE;
		retval = M_TRUE;
	}
	M_thread_spinlock_unlock_int(&once_control->spinlock, atomics_only);
	return retval;
}


static M_bool M_thread_once_reset_int(M_thread_once_t *once_control, M_bool atomics_only)
{
	M_bool retval = M_FALSE;

	if (once_control == NULL)
		return M_FALSE;

	M_thread_spinlock_lock_int(&once_control->spinlock, atomics_only);
	if (once_control->initialized) {
		once_control->initialized = M_FALSE;
		retval = M_TRUE;
	}
	M_thread_spinlock_unlock_int(&once_control->spinlock, atomics_only);

	return retval;
}


M_bool M_thread_once(M_thread_once_t *once_control, void (*init_routine)(M_uint64 flags), M_uint64 init_flags)
{
	return M_thread_once_int(once_control, M_FALSE, init_routine, init_flags);
}

M_bool M_thread_once_reset(M_thread_once_t *once_control)
{
	return M_thread_once_reset_int(once_control, M_FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int M_thread_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	int rv;
#ifdef POLLRDHUP
        nfds_t i;
        for (i=0; i<nfds; i++) {
                fds[i].events |= POLLRDHUP;
        }
#endif

	M_thread_auto_init();
	if (thread_cbs.thread_poll == NULL)
		return -1;

	rv = thread_cbs.thread_poll(fds, nfds, timeout);
#ifdef POLLRDHUP
        if (rv > 0) {
                for (i=0; i<nfds; i++) {
                        if (fds[i].revents & POLLRDHUP)
                                fds[i].revents |= POLLHUP;
                }
        }
#endif
	return rv;
}

#ifndef _WIN32
M_bool M_thread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	M_thread_auto_init();
	if (thread_cbs.thread_sigmask == NULL)
		return 0;
	return thread_cbs.thread_sigmask(how, set, oldset);
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_thread_mutex_t *M_thread_mutex_create(M_uint32 attr)
{
	M_thread_auto_init();
	if (thread_cbs.mutex_create == NULL)
		return NULL;
	return thread_cbs.mutex_create(attr);
}

void M_thread_mutex_destroy(M_thread_mutex_t *mutex)
{
	M_thread_auto_init();
	if (thread_cbs.mutex_destroy == NULL)
		return;
	thread_cbs.mutex_destroy(mutex);
}

M_bool M_thread_mutex_lock(M_thread_mutex_t *mutex)
{
	M_thread_auto_init();
	if (thread_cbs.mutex_lock == NULL)
		return M_FALSE;
	return thread_cbs.mutex_lock(mutex);
}

M_bool M_thread_mutex_trylock(M_thread_mutex_t *mutex)
{
	M_thread_auto_init();
	if (thread_cbs.mutex_trylock == NULL)
		return M_FALSE;
	return thread_cbs.mutex_trylock(mutex);
}

M_bool M_thread_mutex_unlock(M_thread_mutex_t *mutex)
{
	M_thread_auto_init();
	if (thread_cbs.mutex_unlock == NULL)
		return M_FALSE;
	return thread_cbs.mutex_unlock(mutex);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_thread_cond_t *M_thread_cond_create(M_uint32 attr)
{
	M_thread_auto_init();
	if (thread_cbs.cond_create == NULL)
		return NULL;
	return thread_cbs.cond_create(attr);
}

void M_thread_cond_destroy(M_thread_cond_t *cond)
{
	M_thread_auto_init();
	if (thread_cbs.cond_destroy == NULL)
		return;
	thread_cbs.cond_destroy(cond);
}

M_bool M_thread_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, M_uint64 millisec)
{
	M_timeval_t tv;
	M_mem_set(&tv, 0, sizeof(tv));

	M_time_gettimeofday(&tv);

	/* Convert delay to microseconds, add to microseconds field. */
	tv.tv_usec += (M_uint32)(millisec * 1000);

	/* M_thread_cond_timedwait_abs() will normalize the time struct for us. */
	return M_thread_cond_timedwait_abs(cond, mutex, &tv);
}

M_bool M_thread_cond_timedwait_abs(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	M_timeval_t tv;

	M_thread_auto_init();
	if (thread_cbs.cond_timedwait == NULL || abstime == NULL)
		return M_FALSE;

	/* Normalize abstime field, if it's not already - need to limit usec to < 1e6. */
	if (abstime->tv_usec >= (1000*1000)) {
		tv.tv_sec  = abstime->tv_sec + (abstime->tv_usec / (1000*1000));
		tv.tv_usec = abstime->tv_usec % (1000*1000);
		abstime = &tv;
	}

	return thread_cbs.cond_timedwait(cond, mutex, abstime);
}

M_bool M_thread_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	M_thread_auto_init();
	if (thread_cbs.cond_wait == NULL)
		return M_FALSE;
	return thread_cbs.cond_wait(cond, mutex);
}

void M_thread_cond_broadcast(M_thread_cond_t *cond)
{
	M_thread_auto_init();
	if (thread_cbs.cond_broadcast == NULL)
		return;
	thread_cbs.cond_broadcast(cond);
}

void M_thread_cond_signal(M_thread_cond_t *cond)
{
	M_thread_auto_init();
	if (thread_cbs.cond_signal == NULL)
		return;
	thread_cbs.cond_signal(cond);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_thread_rwlock_t *M_thread_rwlock_create(void)
{
	M_thread_auto_init();
	if (thread_cbs.rwlock_create == NULL)
		return NULL;
	return thread_cbs.rwlock_create();
}

void M_thread_rwlock_destroy(M_thread_rwlock_t *rwlock)
{
	M_thread_auto_init();
	if (thread_cbs.rwlock_destroy == NULL)
		return;
	thread_cbs.rwlock_destroy(rwlock);
}

M_bool M_thread_rwlock_lock(M_thread_rwlock_t *rwlock, M_thread_rwlock_type_t type)
{
	M_thread_auto_init();
	if (thread_cbs.rwlock_lock == NULL)
		return M_FALSE;
	return thread_cbs.rwlock_lock(rwlock, type);
}

M_bool M_thread_rwlock_unlock(M_thread_rwlock_t *rwlock)
{
	M_thread_auto_init();
	if (thread_cbs.rwlock_unlock == NULL)
		return M_FALSE;
	return thread_cbs.rwlock_unlock(rwlock);
}

/* -------------------------------------------------------------------------- */

typedef struct {
	void (*cleanup_cb)(void *);
	void *arg;
} M_library_cleanup_member_t;

static M_thread_mutex_t *M_library_cleanup_lock = NULL;
static M_list_t         *M_library_cleanup_list = NULL;

static void M_library_cleanup_init_routine(M_uint64 flags)
{
	struct M_list_callbacks callbacks = {
		NULL,
		NULL,
		NULL,
		M_free
	};
	(void)flags;
	M_library_cleanup_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	M_library_cleanup_list = M_list_create(&callbacks, M_LIST_STACK);
}

static M_thread_once_t M_library_cleanup_once = M_THREAD_ONCE_STATIC_INITIALIZER;

void M_library_cleanup_register(void (*cleanup_cb)(void *arg), void *arg)
{
	M_library_cleanup_member_t *member;

	M_thread_auto_init();

	M_thread_once(&M_library_cleanup_once, M_library_cleanup_init_routine, 0);

	if (cleanup_cb == NULL)
		return;

	member             = M_malloc_zero(sizeof(*member));
	member->cleanup_cb = cleanup_cb;
	member->arg        = arg;

	M_thread_mutex_lock(M_library_cleanup_lock);
	M_list_insert(M_library_cleanup_list, member);
	M_thread_mutex_unlock(M_library_cleanup_lock);
}


void M_library_cleanup(void)
{
	M_library_cleanup_member_t *member;

	if (!M_thread_once_reset_int(&M_library_cleanup_once, M_TRUE))
		return;

	/* Well, if M_library_cleanup() is called simultaneously with M_library_cleanup_register(),
	 * could cause issues ... should we even worry about that? */

	M_thread_mutex_destroy(M_library_cleanup_lock);
	M_library_cleanup_lock = NULL;

	while ((member = M_list_take_first(M_library_cleanup_list)) != NULL) {
		member->cleanup_cb(member->arg);
		M_free(member);
	}

	M_list_destroy(M_library_cleanup_list, M_TRUE);

	M_library_cleanup_list = NULL;
}
