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
#include "m_thread_int.h"
#include "base/platform/m_platform.h"
#include "base/time/m_time_int.h"
#include "m_pollemu.h"

static M_hashtable_t    *M_thread_win_thread_rv = NULL;
static CRITICAL_SECTION  M_thread_win_lock;

static void M_thread_win_init(void)
{
	M_thread_win_thread_rv = M_hashtable_create(16, 75, NULL, NULL, M_HASHTABLE_NONE, NULL);
	InitializeCriticalSection(&M_thread_win_lock);
}

static void M_thread_win_deinit(void)
{
	M_hashtable_destroy(M_thread_win_thread_rv, M_TRUE);
	DeleteCriticalSection(&M_thread_win_lock);
}



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static DWORD win32_abstime2msoffset(const M_timeval_t *abstime)
{
	M_timeval_t tv;
	M_int64     ret;

	M_time_gettimeofday(&tv);

	/* Subtract current time from abstime and return result in milliseconds */
	ret = (((abstime->tv_sec - tv.tv_sec) * 1000) + ((abstime->tv_usec / 1000) - (tv.tv_usec / 1000)));

	/* Sanity check to make sure GetSystemTimeAsFileTime() didn't return
	 * something bogus.  Also makes sure too much time hasn't already elapsed.
	 * If this were to return negative, it could hang indefinitely */
	if (ret < 0)
		ret = 1;
	/* check for DWORD overflow, would indicate bogus time */
	if ((ret >> 32) != 0)
		ret = 1;
	return (DWORD)ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static M_bool M_thread_win_set_priority(M_thread_t *thread, M_threadid_t tid, M_uint8 mthread_priority)
{
	static const int win_priorities[7] = {
		THREAD_PRIORITY_IDLE,
		THREAD_PRIORITY_LOWEST,
		THREAD_PRIORITY_BELOW_NORMAL,
		THREAD_PRIORITY_NORMAL,
		THREAD_PRIORITY_ABOVE_NORMAL,
		THREAD_PRIORITY_HIGHEST,
		THREAD_PRIORITY_TIME_CRITICAL
	};
	int     sys_priority_min = 0;
	int     sys_priority_max = 6;
	int     sys_priority_range;
	int     mthread_priority_range;
	double  priority_scale;
	int     priority;
	HANDLE  hThread;

	(void)thread;

	sys_priority_range     = (sys_priority_max - sys_priority_min) + 1;
	mthread_priority_range = (M_THREAD_PRIORITY_MAX - M_THREAD_PRIORITY_MIN) + 1;
	priority_scale         = ((double)sys_priority_range) / ((double)mthread_priority_range);

	/* Lets handle min, max, and normal without scaling */
	if (mthread_priority == M_THREAD_PRIORITY_MIN) {
		priority = sys_priority_min;
	} else if (mthread_priority == M_THREAD_PRIORITY_MAX) {
		priority = sys_priority_max;
	} else if (mthread_priority == M_THREAD_PRIORITY_NORMAL) {
		priority = 3;
	} else {
		priority = sys_priority_min + (int)(((double)(mthread_priority - M_THREAD_PRIORITY_MIN)) * priority_scale);
		if (priority > sys_priority_max)
			priority = sys_priority_max;
		if (priority < sys_priority_min)
			priority = sys_priority_min;
	}

	/* NOTE: On Windows we need to create a new thread handle from the thread id as it may have been closed if the thread
	 *       was created as detached. */
	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)tid);
	if (hThread == NULL) {
		M_fprintf(stderr, "%s(): Unable to get thread handle for Thread %lld: %ld\n", __FUNCTION__, (M_int64)tid, (long)GetLastError());
		return M_FALSE;
	}

	if (SetThreadPriority(hThread, win_priorities[priority]) == 0) {
		M_fprintf(stderr, "SetThreadPriority on thread %lld to %d failed: %ld\n", (M_int64)tid, win_priorities[priority], (long)GetLastError());
		CloseHandle(hThread);
		return M_FALSE;
	}
	CloseHandle(hThread);
	return M_TRUE;
}


static M_bool M_thread_win_set_processor(M_thread_t *thread, M_threadid_t tid, int processor_id)
{
	DWORD_PTR mask;
	HANDLE    hThread;

	(void)thread;

	if (processor_id == -1) {
		/* Set to same as process as a whole */
		DWORD_PTR dwSystemAffinity;
		GetProcessAffinityMask(GetCurrentProcess(), &mask, &dwSystemAffinity /* unused */);
	} else {
		mask = ((DWORD_PTR)1) << processor_id;
	}

	/* NOTE: On Windows we need to create a new thread handle from the thread id as it may have been closed if the thread
	 *       was created as detached. */
	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)tid);
	if (hThread == NULL) {
		M_fprintf(stderr, "%s(): Unable to get thread handle for Thread %lld: %ld\n", __FUNCTION__, (M_int64)tid, (long)GetLastError());
		return M_FALSE;
	}

	if (SetThreadAffinityMask(hThread, mask) == 0) {
		M_fprintf(stderr, "SetThreadAffinityMask for %lld to processor %d failed: %ld\n", (M_int64)tid, processor_id, (long)GetLastError());
		CloseHandle(hThread);
		return M_FALSE;
	}
	CloseHandle(hThread);
	return M_TRUE;
}

static M_threadid_t M_thread_win_self(M_thread_t **thread)
{
	if (thread != NULL)
		*thread = (M_thread_t *)GetCurrentThread();

	return (M_threadid_t)GetCurrentThreadId();
}

typedef struct {
	void *(*func)(void *);
	void   *arg;
	M_bool  is_joinable;
} M_thread_win_func_arg;

static DWORD WINAPI M_thread_win_func_wrapper(void *arg)
{
	M_thread_win_func_arg *funcarg       = arg;
	void                *(*func)(void *) = funcarg->func;
	void                  *farg          = funcarg->arg;
	M_bool                 is_joinable   = funcarg->is_joinable;
	void                  *rv;
	M_thread_t            *thread        = NULL;

	M_free(arg);

	rv = func(farg);

	if (is_joinable) {
		/* Stuff real return value into result hashtable since windows doesn't allow threads to return pointers  */
		M_thread_win_self(&thread);
		EnterCriticalSection(&M_thread_win_lock);
		M_hashtable_insert(M_thread_win_thread_rv, thread, rv);
		LeaveCriticalSection(&M_thread_win_lock);
	}

	/* We can't actually use this exit code at all */
	return 0;
}

static M_thread_t *M_thread_win_create(const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	DWORD                  dwThreadId;
	HANDLE                 hThread;
	M_thread_win_func_arg *funcarg = M_malloc_zero(sizeof(*funcarg));
	M_thread_t            *rv      = NULL;

	if (func == NULL)
		goto fail;

	/* Wrap callback due to different arguments and return value for Windows */
	funcarg->func        = func;
	funcarg->arg         = arg;
	funcarg->is_joinable = (attr == NULL)?M_FALSE:M_thread_attr_get_create_joinable(attr);

	hThread = CreateThread(NULL, 0, M_thread_win_func_wrapper, funcarg, 0, &dwThreadId);
	if (hThread == NULL)
		return NULL;

	if (attr != NULL && !M_thread_attr_get_create_joinable(attr)) {
		CloseHandle(hThread);
		rv = (M_thread_t *)1;
	} else {
		rv = (M_thread_t *)hThread;
	}

fail:
	if (rv == NULL) {
		M_free(funcarg);
	}
	return rv;
}


static M_bool M_thread_win_join(M_thread_t *thread, void **value_ptr)
{
	void *rv = NULL;

	if (thread == NULL || thread == (M_thread_t *)1)
		return M_FALSE;

	if (WaitForSingleObject((HANDLE)thread, INFINITE) != WAIT_OBJECT_0)
		return M_FALSE;

	/* Exit codes are pointers, so we can't use this as it isn't the right size
	 * on 64bit
	 * if (value_ptr)
	 *   GetExitCodeThread((HANDLE)thread, (LPDWORD)value_ptr);
	 */
	EnterCriticalSection(&M_thread_win_lock);
	if (M_hashtable_get(M_thread_win_thread_rv, thread, &rv)) {
		M_hashtable_remove(M_thread_win_thread_rv, thread, M_TRUE);
	}
	LeaveCriticalSection(&M_thread_win_lock);

	CloseHandle((HANDLE)thread);

	if (value_ptr != NULL)
		*value_ptr = rv;

	return M_TRUE;
}


static void M_thread_win_yield(M_bool force)
{
	if (!force)
		return;
	SwitchToThread();
}

static void M_thread_win_sleep(M_uint64 usec)
{
	DWORD    msec;
	M_uint64 r;

	r = usec/1000;
	if (r > M_UINT32_MAX) {
		msec = M_UINT32_MAX;
	} else{
		msec = (DWORD)r;
	}
	Sleep(msec);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int M_thread_win_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return M_pollemu(fds, nfds, timeout);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_mutex_t *M_thread_win_mutex_create(M_uint32 attr)
{
	M_thread_mutex_t *mutex;

	(void)attr;
	/* NOTE: we never define "struct M_thread_mutex", as we're aliasing it to a
	 *       different type.  Bad style, but keeps our type safety */
	mutex = M_malloc_zero(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)mutex);

	return mutex;
}

static void M_thread_win_mutex_destroy(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return;

	DeleteCriticalSection((LPCRITICAL_SECTION)mutex);
	M_free(mutex);
}

static M_bool M_thread_win_mutex_lock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;
	EnterCriticalSection((LPCRITICAL_SECTION)mutex);
	return M_TRUE;
}

static M_bool M_thread_win_mutex_trylock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (TryEnterCriticalSection((LPCRITICAL_SECTION)mutex) != 0)
		return M_TRUE;
	return M_FALSE;
}

static M_bool M_thread_win_mutex_unlock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;
	LeaveCriticalSection((LPCRITICAL_SECTION)mutex);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if 0
//#if _WIN32_WINNT >= 0x0600 /* Vista */

struct M_thread_cond {
	CONDITION_VARIABLE cond;
};

static M_thread_cond_t *M_thread_win_cond_create(M_uint32 attr)
{
	M_thread_cond_t *cond = M_malloc_zero(sizeof(*cond));
	(void)attr;
	InitializeConditionVariable(&cond->cond);
	return cond;
}

static void M_thread_win_cond_destroy(M_thread_cond_t *cond)
{
	if (!cond)
		return;
	/* NOTE: doesn't appear there is any necessary deinitialization for condition variables */
	M_free(cond);
}

static M_bool M_thread_win_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	DWORD dwMilliseconds = 0;

	if (!cond || !mutex)
		return M_FALSE;

	if (abstime == NULL) {
		dwMilliseconds = INFINITE;
	} else {
		dwMilliseconds = win32_abstime2msoffset(abstime);
	}

	return SleepConditionVariableCS(&cond->cond, (LPCRITICAL_SECTION)mutex, dwMilliseconds);
}

static M_bool M_thread_win_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	return M_thread_win_cond_timedwait(cond, mutex, NULL);
}

static void M_thread_win_cond_broadcast(M_thread_cond_t *cond)
{
	if (!cond)
		return;
	WakeAllConditionVariable(&cond->cond);
}

static void M_thread_win_cond_signal(M_thread_cond_t *cond)
{
	if (!cond)
		return;
	WakeConditionVariable(&cond->cond);
}


struct M_thread_rwlock {
	SRWLOCK rwlock;
	M_bool  locked_exclusive;
};

M_thread_rwlock_t *M_thread_win_rwlock_create(void)
{
	M_thread_rwlock_t *rwlock;

	rwlock = M_malloc_zero(sizeof(*rwlock));
	InitializeSRWLock(&rwlock->rwlock);
	return rwlock;
};

void M_thread_win_rwlock_destroy(M_thread_rwlock_t *rwlock)
{
	if (!rwlock)
		return;
	/* NOTE: no destroy mechanism for rwlocks */
	M_free(rwlock);
}

M_bool M_thread_win_rwlock_lock(M_thread_rwlock_t *rwlock, M_thread_rwlock_type_t type)
{
	if (rwlock == NULL)
		return M_FALSE;

	if (type == M_THREAD_RWLOCK_TYPE_READ) {
		AcquireSRWLockExclusive(&rwlock->rwlock);
		rwlock->locked_exclusive = M_FALSE;
	} else {
		AcquireSRWLockShared(&rwlock->rwlock);
		rwlock->locked_exclusive = M_TRUE;
	}

	return M_TRUE;
}

M_bool M_thread_win_rwlock_unlock(M_thread_rwlock_t *rwlock)
{
	if (!rwlock)
		return M_FALSE;

	if (rwlock->locked_exclusive) {
		ReleaseSRWLockExclusive(&rwlock->rwlock);
	} else {
		ReleaseSRWLockShared(&rwlock->rwlock);
	}
	return M_TRUE;
}

#else

#  define SIGNAL    0
#  define BROADCAST 1

struct M_thread_cond {
	HANDLE           events[2];
	HANDLE           gate;
	CRITICAL_SECTION mutex;
	int              waiters;
	int              event;
};


/* Pre-Vista implementation of conditionals */
static M_thread_cond_t *M_thread_win_cond_create(M_uint32 attr)
{
	M_thread_cond_t *cond;

	(void)attr;

	cond = M_malloc_zero(sizeof(*cond));
	cond->events[SIGNAL]    = CreateEvent(NULL, FALSE, FALSE, NULL);
	cond->events[BROADCAST] = CreateEvent(NULL, TRUE, FALSE, NULL);
	/* Use a semaphore as a gate so we don't lose signals */
	cond->gate              = CreateSemaphore(NULL, 1, 1, NULL);
	InitializeCriticalSection(&cond->mutex);
	cond->waiters           = 0;
	cond->event             = -1;

	return cond;
}

static void M_thread_win_cond_destroy(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	CloseHandle(cond->events[SIGNAL]);
	CloseHandle(cond->events[BROADCAST]);
	CloseHandle(cond->gate);
	DeleteCriticalSection(&cond->mutex);

	M_free(cond);
}

static M_bool M_thread_win_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	DWORD                retval;
	DWORD                dwMilliseconds;

	if (cond == NULL || mutex == NULL)
		return M_FALSE;

	/* We may only enter when no wakeups active
	 * this will prevent the lost wakeup */
	WaitForSingleObject(cond->gate, INFINITE);

	EnterCriticalSection(&cond->mutex);
	/* count waiters passing through */
	cond->waiters++;
	LeaveCriticalSection(&cond->mutex);

	ReleaseSemaphore(cond->gate, 1, NULL);

	LeaveCriticalSection((LPCRITICAL_SECTION)mutex);

	if (abstime == NULL) {
		dwMilliseconds = INFINITE;
	} else {
		dwMilliseconds = win32_abstime2msoffset(abstime);
	}
	retval = WaitForMultipleObjects(2, cond->events, FALSE, dwMilliseconds);

	/* We go into a critical section to make sure wcond->waiters
	 * isn't checked while we decrement.  This is especially
	 * important for a timeout since the gate may not be closed.
	 * We need to check to see if a broadcast/signal was pending as
	 * this thread could have been preempted prior to EnterCriticalSection
	 * but after WaitForMultipleObjects() so we may be responsible
	 * for reseting the event and closing the gate */
	EnterCriticalSection(&cond->mutex);
	cond->waiters--;
	if (cond->event != -1 && cond->waiters == 0) {
		/* Last waiter needs to reset the event on(as a
		 * broadcast event is not automatic) and also
		 * re-open the gate */
		if (cond->event == BROADCAST)
			ResetEvent(cond->events[BROADCAST]);

		ReleaseSemaphore(cond->gate, 1, NULL);
		cond->event = -1;
	} else if (retval == WAIT_OBJECT_0+SIGNAL) {
		/* If specifically, this thread was signalled and there
		 * are more waiting, re-open the gate and reset the event */
		ReleaseSemaphore(cond->gate, 1, NULL);
		cond->event = -1;
	} else {
		/* This could be a standard timeout with more
		 * waiters, don't do anything */
	}
	LeaveCriticalSection(&cond->mutex);

	EnterCriticalSection((LPCRITICAL_SECTION)mutex);
	if (retval == WAIT_TIMEOUT)
		return M_FALSE;

	return M_TRUE;
}

static M_bool M_thread_win_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	if (cond == NULL || mutex == NULL)
		return M_FALSE;
	return M_thread_win_cond_timedwait(cond, mutex, NULL);
}

static void M_thread_win_cond_broadcast(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	/* close gate to prevent more waiters while broadcasting */
	WaitForSingleObject(cond->gate, INFINITE);

	/* If there are waiters, send a broadcast event,
	 * otherwise, just reopen the gate */
	EnterCriticalSection(&cond->mutex);
	cond->event = BROADCAST;
	/* if no waiters just reopen gate */
	if (cond->waiters) {
		/* wake all */
		SetEvent(cond->events[BROADCAST]);
	} else {
		ReleaseSemaphore(cond->gate, 1, NULL);
	}
	LeaveCriticalSection(&cond->mutex);
}

static void M_thread_win_cond_signal(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	/* close gate to prevent more waiters while signalling */
	WaitForSingleObject(cond->gate, INFINITE);

	/* If there are waiters, wake one, otherwise, just
	 * reopen the gate */
	EnterCriticalSection(&cond->mutex);
	cond->event = SIGNAL;
	if (cond->waiters) {
		/* wake one */
		SetEvent(cond->events[SIGNAL]);
	} else {
		ReleaseSemaphore(cond->gate, 1, NULL);
	}
	LeaveCriticalSection(&cond->mutex);
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_thread_win_register(M_thread_model_callbacks_t *cbs)
{
	if (cbs == NULL)
		return;

	M_mem_set(cbs, 0, sizeof(*cbs));

	cbs->init                 = M_thread_win_init;
	cbs->deinit               = M_thread_win_deinit;
	/* Thread */
	cbs->thread_create        = M_thread_win_create;
	cbs->thread_join          = M_thread_win_join;
	cbs->thread_self          = M_thread_win_self;
	cbs->thread_yield         = M_thread_win_yield;
	cbs->thread_sleep         = M_thread_win_sleep;
	cbs->thread_set_priority  = M_thread_win_set_priority;
	cbs->thread_set_processor = M_thread_win_set_processor;

	/* System */
	cbs->thread_poll    = M_thread_win_poll;
	/* Mutex */
	cbs->mutex_create   = M_thread_win_mutex_create;
	cbs->mutex_destroy  = M_thread_win_mutex_destroy;
	cbs->mutex_lock     = M_thread_win_mutex_lock;
	cbs->mutex_trylock  = M_thread_win_mutex_trylock;
	cbs->mutex_unlock   = M_thread_win_mutex_unlock;
	/* Cond */
	cbs->cond_create    = M_thread_win_cond_create;
	cbs->cond_destroy   = M_thread_win_cond_destroy;
	cbs->cond_timedwait = M_thread_win_cond_timedwait;
	cbs->cond_wait      = M_thread_win_cond_wait;
	cbs->cond_broadcast = M_thread_win_cond_broadcast;
	cbs->cond_signal    = M_thread_win_cond_signal;
	/* Read Write Lock */
#if 0
//#if _WIN32_WINNT >= 0x0600 /* Vista */
	cbs->rwlock_create  = M_thread_win_rwlock_create;
	cbs->rwlock_destroy = M_thread_win_rwlock_destroy;
	cbs->rwlock_lock    = M_thread_win_rwlock_lock;
	cbs->rwlock_unlock  = M_thread_win_rwlock_unlock;
#else
	cbs->rwlock_create  = M_thread_rwlock_emu_create;
	cbs->rwlock_destroy = M_thread_rwlock_emu_destroy;
	cbs->rwlock_lock    = M_thread_rwlock_emu_lock;
	cbs->rwlock_unlock  = M_thread_rwlock_emu_unlock;
#endif
}
