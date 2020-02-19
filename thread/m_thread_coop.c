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
#include "base/platform/m_platform.h"
#include "base/time/m_time_int.h"
#include "m_thread_int.h"
#ifdef _WIN32
#  include "m_pollemu.h"
#endif

/* If we don't have get/set/swapcontext functions use setjmp/sigaltstack.
 * OS X has get/swap but it's deprecated. */
#if (!defined(HAVE_GETCONTEXT) && !defined(_WIN32)) || defined(__APPLE__)
#  define COOPTHREADS_SETJMP 1
#endif

#include <time.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#else
#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#  endif
#  ifdef HAVE_UNISTD_H
#    include <unistd.h>
#  endif
#endif

#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif

#ifdef HAVE_SYS_REGSET_H
#  include <sys/regset.h>
#endif

#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif

#if !defined(COOPTHREADS_SETJMP) && !defined(_WIN32)
#  include <ucontext.h>
#endif

#ifdef COOPTHREADS_SETJMP
#  include <signal.h>
#  include <setjmp.h>
#  include <unistd.h>
#endif

#ifdef HAVE_VALGRIND_H
#  include "valgrind/valgrind.h"
#else
#  define VALGRIND_STACK_REGISTER(start,end) (0)
#  define VALGRIND_STACK_DEREGISTER(id)
#endif

#if (defined(MAP_ANONYMOUS) || defined(MAP_ANON)) && defined(MAP_GROWSDOWN) && defined(MAP_NORESERVE)
#  ifndef MAP_ANONYMOUS
#    define MAP_ANONYMOUS MAP_ANON
#  endif
#  define USE_MMAPPED_STACK
#  define COOP_THREAD_STACK (sizeof(void *) * 256 * 1024)
#elif defined(_WIN32)
#  define COOP_THREAD_STACK (sizeof(void *) * 256 * 1024)
#else
#  define COOP_THREAD_STACK 256 * 1024
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_thread_coop;
typedef struct M_thread_coop M_thread_coop_t;


struct M_thread_mutex {
	M_thread_coop_t *thread_locked; /* Which thread currently has this mutex locked? */
	size_t           cnt;           /* Lock count for recursive mutexes */
};

struct M_thread_cond {
	M_llist_t *waiting_threads;
};

typedef struct {
	int            ret;
	struct pollfd *fds;
	nfds_t         nfds;
} M_thread_coop_poll_t;

typedef enum {
	M_THREAD_COOP_STATUS_RUN = 0,
	M_THREAD_COOP_STATUS_RUN_DETACHED,
	M_THREAD_COOP_STATUS_DONE,
	M_THREAD_COOP_STATUS_DONE_DETACHED
} M_thread_coop_status_t;


struct M_thread_coop {
#if defined(_WIN32)
	void                    *th_context;
	M_bool                   is_parent;
#elif !defined(COOPTHREADS_SETJMP)
	ucontext_t               th_context;
#elif defined(COOPTHREADS_SETJMP)
	sigjmp_buf               th_context;
#endif

#if !defined(_WIN32)
#  ifdef USE_MMAPPED_STACK
	char                    *stack;
#  else
	char                     stack[COOP_THREAD_STACK];
#  endif
	M_uint32                 vg_stackid;
#endif

	M_time_t                 to_sec;
	M_suseconds_t            to_usec;

	/* Certain OS's under VMware may have a negative clock drift,
	 * store the point the thread was put to sleep, and if we detect
	 * a negative drift, active it so it doesn't potentially stall */
	M_time_t                 sch_sec;
	M_suseconds_t            sch_usec;

	M_thread_mutex_t        *wait_mutex;  /* Mutex we are waiting on to become unlocked */
	M_thread_cond_t         *wait_cond;   /* Conditional we are waiting on to be signalled */
	M_thread_coop_poll_t    *wait_poll;   /* if we're waiting on a poll, this is the data we're waiting on */
	M_thread_coop_t         *wait_join;   /* Reference to thread waiting to be completed */

	void                    *retval; /* Returned value from thread function callback */
	M_thread_coop_status_t   status;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_llist_t *coop_active_threads = NULL;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_coop_destroy(void *arg)
{
	M_thread_coop_t *thread = (M_thread_coop_t *)arg;

	if (thread == NULL)
		return;

#if !defined(_WIN32)
	/* Valgrind helper to deregister a stack */
	if (thread->vg_stackid != 0) {
		VALGRIND_STACK_DEREGISTER(thread->vg_stackid);
	}
#endif

#ifdef USE_MMAPPED_STACK
	if (thread->stack != NULL)
		munmap(thread->stack, COOP_THREAD_STACK);
#endif

#ifdef _WIN32
	if (!thread->is_parent)
		DeleteFiber(thread->th_context);
#endif
	M_free(thread);
}

static int M_thread_coop_test_poll(M_thread_coop_poll_t *pollst)
{
	int             ret;

	do {

		ret =
#ifdef _WIN32
		M_pollemu
#else
		poll
#endif
			(pollst->fds, pollst->nfds, 0);

		/* If we received EINTR, repeat poll since it was interrupted */
		if (ret != -1 || (ret == -1 && errno != EINTR))
			break;
	} while (1);

	pollst->ret = ret;

	return ret;
}

#if defined(_WIN32)
static void M_thread_coop_cpu_usleep(M_uint64 usec)
{
	Sleep((DWORD)(usec/1000));
}
#else
static void M_thread_coop_cpu_usleep(M_uint64 usec)
{
	fd_set         readfs;
	struct timeval timeout;
	M_timeval_t    starttv;
	int            ret;
	M_uint64       diff;

	do {
		M_time_elapsed_start(&starttv);
		timeout.tv_sec  = (M_time_tv_sec_t)(usec / 1000000);
		timeout.tv_usec = (M_time_tv_usec_t)(usec % 1000000);

		FD_ZERO(&readfs);
		ret = select(0, &readfs, NULL, NULL, &timeout);
		/* continue waiting upon interrupt */
		if (ret == -1 && errno == EINTR) {
			diff = M_time_elapsed(&starttv) * 1000;
			/* The diff is in millisecond accuracy, so lets compensate */
			if (diff + 999 < usec) {
				usec -= diff;
				/* loop! */
				continue;
			}
		}

		break;
	} while (1);
}
#endif

static void M_thread_coop_switch_to_thread(M_llist_node_t *node, M_thread_coop_t *thread)
{
	M_thread_coop_t *curr_thread = M_llist_node_val(M_llist_first(coop_active_threads));
	M_timeval_t      tv;

	/* Tell the threading layer the thread we're switching to is the active one */
	M_llist_set_first(node);
	thread->to_sec  = 0;
	thread->to_usec = 0;

	/* don't issue a swap context if we're the thread being scheduled */
	if (curr_thread == thread)
		return;

	/* Certain OS's under VMware may have a negative clock drift,
	 * store the point the thread was put to sleep, so we can detect
	 * a negative drift, and compensate */
	M_time_gettimeofday(&tv);
	curr_thread->sch_sec  = tv.tv_sec;
	curr_thread->sch_usec = tv.tv_usec;

#if defined(_WIN32)
	SwitchToFiber(thread->th_context);
#elif defined(COOPTHREADS_SETJMP)
	if (sigsetjmp(curr_thread->th_context, 1) == 0)
		siglongjmp(thread->th_context, 1);
#else
	/* Swap to our new context */
	swapcontext(&curr_thread->th_context, &thread->th_context);
#endif
}


static void M_thread_coop_sched(void)
{
	M_llist_node_t  *node;
	M_llist_node_t  *prev_node;
	M_thread_coop_t *ptr            = NULL;
	M_timeval_t      tv;

	/* Search for thread to execute */
	M_time_gettimeofday(&tv);
	node = M_llist_node_next(M_llist_first(coop_active_threads));
	ptr  = M_llist_node_val(node);

	while (1) {
		if (ptr->status == M_THREAD_COOP_STATUS_RUN || ptr->status == M_THREAD_COOP_STATUS_RUN_DETACHED) {
			/* Looks like the thread was simply swapped out, definitely eligible for scheduling */
			if (ptr->wait_poll == NULL && ptr->wait_cond == NULL && ptr->wait_mutex == NULL && ptr->wait_join == NULL &&
			    ptr->to_sec == 0 && ptr->to_usec == 0) {
				break;
			}

			/* Check mutex status */
			if (ptr->wait_mutex != NULL && ptr->wait_mutex->thread_locked == NULL) {
				/* Mutex is now unlocked, wake up! */
				break;
			}

			/* Check poll status */
			if (ptr->wait_poll != NULL && M_thread_coop_test_poll(ptr->wait_poll) != 0) {
				break;
			}

			/* Check join status */
			if (ptr->wait_join != NULL && ptr->wait_join->status == M_THREAD_COOP_STATUS_DONE) {
				break;
			}

			/* Check timeouts */
			if (ptr->to_sec != 0 || ptr->to_usec != 0) {
				/* Certain OS's under VMware may have a negative clock drift,
				 * if we detect a negative drift, activate it so it doesn't potentially
				 * stall, but only if we're actually waiting on a timeout and not
				 * only an event (like a conditional or poll) */
				if (tv.tv_sec < ptr->sch_sec || (tv.tv_sec == ptr->sch_sec && tv.tv_usec < ptr->sch_usec)) {
					break;
				}

				/* Normal timeout check */
				if (tv.tv_sec > ptr->to_sec || (tv.tv_sec == ptr->to_sec && tv.tv_usec >= ptr->to_usec)) {
					break;
				}
			}
		} else if (node != M_llist_first(coop_active_threads) && ptr->status == M_THREAD_COOP_STATUS_DONE_DETACHED) {
			/* Detached threads have to be cleaned up by the scheduler, so we're doing that here */
			prev_node = M_llist_node_prev(node);
			M_llist_remove_node(node);
			node = prev_node;
		}

		node = M_llist_node_next(node);
		ptr  = M_llist_node_val(node);
		if (node == M_llist_node_next(M_llist_first(coop_active_threads))) {
			M_thread_coop_cpu_usleep(10000);
			M_time_gettimeofday(&tv);
		}
	}

	/* Swap */
	M_thread_coop_switch_to_thread(node, ptr);
}

static void M_thread_coop_yield(M_bool force)
{
	(void)force;
	M_thread_coop_sched();
}

static void M_thread_coop_sleep(M_uint64 usec)
{
	M_thread_coop_t *thread;
	M_time_t         sec;
	M_suseconds_t    usecs;
	M_timeval_t      tv;

	M_mem_set(&tv, 0, sizeof(tv));
	sec   = (M_time_t)(usec / 1000000);
	usecs = (M_suseconds_t)(usec % 1000000);

	M_time_gettimeofday(&tv);

	thread           = M_llist_node_val(M_llist_first(coop_active_threads));
	thread->to_sec   = (M_time_t)(sec    + tv.tv_sec);
	thread->to_usec  = (M_suseconds_t)(usecs + tv.tv_usec);

	/* Normalize */
	thread->to_sec  += thread->to_usec / 1000000;
	thread->to_usec %= 1000000;

	M_thread_coop_yield(M_TRUE);
}

#if defined(__amd64__) && !defined(COOPTHREADS_SETJMP) && !defined(_WIN32)
/* On amd64, makecontext() can only accept int arguments, so we split the
 * 8 byte pointer addresses into 2 4-byte ints each ... */
static void coop_thfunc(int func_high, int func_low, int arg_high, int arg_low)
#else
static void coop_thfunc(void *(*func)(void *), void *arg)
#endif
{
	M_thread_coop_t *thread;
	void            *retval = NULL;
#if defined(__amd64__) && !defined(COOPTHREADS_SETJMP) && !defined(_WIN32)
	void          *(*func)(void *);
	void            *arg;

	func   = (void *(*)(void *))((((M_uintptr)((M_uint32)func_high)) << 32) | ((M_uint32)func_low));
	arg    = (void *)           ((((M_uintptr)((M_uint32)arg_high)) << 32)  | ((M_uint32)arg_low));
#endif
	retval = func(arg);

	thread         = M_llist_node_val(M_llist_first(coop_active_threads));
	thread->retval = retval;
	if (thread->status == M_THREAD_COOP_STATUS_RUN) {
		thread->status = M_THREAD_COOP_STATUS_DONE;
	} else {
		thread->status = M_THREAD_COOP_STATUS_DONE_DETACHED;
	}

	M_thread_coop_sched();
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_coop_init(void)
{
	M_thread_coop_t         *parent;
	struct M_llist_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		M_thread_coop_destroy
	};

	coop_active_threads = M_llist_create(&cbs, M_LLIST_CIRCULAR);
	parent              = M_malloc_zero(sizeof(*parent));

#ifdef _WIN32
	parent->th_context  = ConvertThreadToFiber(NULL);
	parent->is_parent   = M_TRUE;
#endif

	M_llist_insert(coop_active_threads, parent);
}

static void M_thread_coop_deinit(void)
{
	M_llist_destroy(coop_active_threads, M_TRUE);
	coop_active_threads = NULL;
}

#if defined(_WIN32)

struct fiber_arg {
	void *(*func)(void *);
	void   *arg;
};

static void CALLBACK M_thread_coop_create_int_init(void *farg)
{
	struct fiber_arg *fargs = farg;
	void           *(*func)(void *);
	void             *arg;

	func = fargs->func;
	arg  = fargs->arg;
	M_free(fargs);

	coop_thfunc(func, arg);
}

static void M_thread_coop_create_int(M_thread_coop_t *thread, void *(*func)(void *), void *arg)
{
	struct fiber_arg *farg = M_malloc_zero(sizeof(*farg));
	farg->func             = func;
	farg->arg              = arg;
	thread->th_context     = CreateFiberEx(256 * 1024, COOP_THREAD_STACK, FIBER_FLAG_FLOAT_SWITCH, M_thread_coop_create_int_init, farg);
}

#elif defined(COOPTHREADS_SETJMP)
static void          *(*M_thread_coop_setjmp_func)(void *);
static void            *M_thread_coop_setjmp_arg;
static sigset_t         M_thread_coop_setjmp_sigs;
static M_bool           M_thread_coop_setjmp_called = M_FALSE;
static M_thread_coop_t *M_thread_coop_setjmp_thread;
static jmp_buf          M_thread_coop_setjmp_pctx;
static jmp_buf          M_thread_coop_setjmp_cctx;


static void M_thread_coop_create_int_init(void)
{
	void *(*func)(void *);
	void *arg;

	/* Set signal mask */
	sigprocmask(SIG_SETMASK, &M_thread_coop_setjmp_sigs, NULL);

	/* Store function and argument to call in stack variable */
	func = M_thread_coop_setjmp_func;
	arg  = M_thread_coop_setjmp_arg;

	/* Switch back to parent using the global parent ctx, but
	 * we want to store back to our private thread ctx as from
	 * here on out we want to preserve the signal mask */
	if (sigsetjmp(M_thread_coop_setjmp_thread->th_context, 1) == 0)
		longjmp(M_thread_coop_setjmp_pctx, 1);

	/* When the parent gives us control again we start */
	coop_thfunc(func, arg);

	/* This will never get called because coop_thfunc() never returns as the
	 * thread will be deleted before it returns */
	abort();
}


static void M_thread_coop_create_int_setjmp(int sig)
{
	(void)sig;

	/* Save the thread context, but notice we're not using sigsetjmp as we
	 * don't want to possibly save the sigaltstack info, we'll use it later
	 * for context switching.  Hence the global child context we are using */
	if (setjmp(M_thread_coop_setjmp_cctx) == 0) {
		M_thread_coop_setjmp_called = M_TRUE;
		return;
	}

	M_thread_coop_create_int_init();
}

static void M_thread_coop_create_int(M_thread_coop_t *thread, void *(*func)(void *), void *arg)
{
	sigset_t         sigs;
	sigset_t         orig_sigs;
	struct sigaction sa;
	struct sigaction orig_sa;
	stack_t          stack;
	stack_t          orig_stack;

	/* Block SIGUSR1, it is what we use to signal stack creation.  We don't want
	 * it coming in during this process */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigs, &orig_sigs);

	/* Set the signal action handler for SIGUSR1 to our helper function */
	M_mem_set(&sa, 0, sizeof(sa));
	sa.sa_handler = M_thread_coop_create_int_setjmp;
	sa.sa_flags = SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, &orig_sa);

	/* Set up our stack_t */
	stack.ss_sp    = thread->stack;
	stack.ss_size  = COOP_THREAD_STACK;
	stack.ss_flags = 0;
	sigaltstack(&stack, &orig_stack);

	/* Set up globals */
	M_thread_coop_setjmp_func   = func;
	M_thread_coop_setjmp_arg    = arg;
	M_thread_coop_setjmp_sigs   = orig_sigs;
	M_thread_coop_setjmp_thread = thread;
	M_thread_coop_setjmp_called = M_FALSE;

	/* Raise signal, then set a mask and wait for it to be called */
	kill(getpid(), SIGUSR1);
	sigfillset(&sigs);
	sigdelset(&sigs, SIGUSR1);
	while (!M_thread_coop_setjmp_called)
		sigsuspend(&sigs);

	/* Disable alternate stack, we're done with it */
	sigaltstack(NULL, &stack);
	stack.ss_flags = SS_DISABLE;
	sigaltstack(&stack, NULL);

	/* Check to see if the orig stack also had an alternate stack, if so, restore that */
	if (!(orig_stack.ss_flags & SS_DISABLE))
		sigaltstack(&orig_stack, NULL);

	/* Restore signal handlers */
	sigaction(SIGUSR1, &orig_sa, NULL);
	sigprocmask(SIG_SETMASK, &orig_sigs, NULL);

	/* Switch to new thread.  Again, we're not yet able to use the sigsetjmp/siglongjmp yet,
	 * but the next time the thread returns control to the parent, the parent's context
	 * will be stored in the thread handle for the parent */
	if (setjmp(M_thread_coop_setjmp_pctx) == 0)
		longjmp(M_thread_coop_setjmp_cctx, 1);
}

#else /* !COOPTHREADS_SETJMP */

static void M_thread_coop_create_int(M_thread_coop_t *thread, void *(*func)(void *), void *arg)
{
	M_thread_coop_t *athread = M_llist_node_val(M_llist_first(coop_active_threads));;

	/* Create the new context */
	getcontext(&thread->th_context);
	thread->th_context.uc_stack.ss_sp   = thread->stack;
	thread->th_context.uc_stack.ss_size = COOP_THREAD_STACK;
	thread->th_context.uc_link          = &athread->th_context;

#  ifdef __amd64__
	/* Damn int arg only stuff on amd64, see prior comment */
	makecontext(&thread->th_context, (void (*)(void))coop_thfunc, 4,
	            (int)((((M_uintptr)func) >> 32) & 0xFFFFFFFF),
	            (int)(((M_uintptr)func)         & 0xFFFFFFFF),
	            (int)((((M_uintptr)arg) >> 32)  & 0xFFFFFFFF),
	            (int)(((M_uintptr)arg)          & 0xFFFFFFFF)
	           );
#  else
	makecontext(&thread->th_context, (void (*)(void))coop_thfunc, 2, func, arg);
#  endif
}

#endif /* COOPTHREADS_SETJMP */


static M_thread_t *M_thread_coop_create(M_threadid_t *id, const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	M_thread_coop_t *thread = NULL;

	thread = M_malloc_zero(sizeof(*thread));
	if (M_thread_attr_get_create_joinable(attr)) {
		thread->status = M_THREAD_COOP_STATUS_RUN;
	} else {
		thread->status = M_THREAD_COOP_STATUS_RUN_DETACHED;
	}

#ifdef USE_MMAPPED_STACK
	thread->stack = mmap(NULL, COOP_THREAD_STACK, PROT_EXEC|PROT_WRITE|PROT_READ,
		MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE|MAP_GROWSDOWN, -1, 0);
	if (thread->stack == MAP_FAILED) {
		M_free(thread);
		return NULL;
	}
#endif

#ifndef _WIN32
	/* Helper to let valgrind know the alternative stack location */
	thread->vg_stackid = VALGRIND_STACK_REGISTER(thread->stack, (void *)((size_t)thread->stack + COOP_THREAD_STACK));
#endif

	/* Insert thread into end of queue (right before current running thread) */
	M_llist_insert(coop_active_threads, thread);

	M_thread_coop_create_int(thread, func, arg);

	if (id != NULL)
		*id = (M_threadid_t)thread;

	M_thread_coop_switch_to_thread(M_llist_last(coop_active_threads), thread);

	return thread;
}


static M_bool M_thread_coop_join(M_thread_t *thread, void **value_ptr)
{
	M_llist_node_t  *node;
	M_thread_coop_t *mythread;
	M_thread_coop_t *fthread;

	if (thread == NULL)
		return M_FALSE;

	mythread = (M_thread_coop_t *)thread;

	/* Sanity check to make sure they didn't pass a garbage pointer */
	node = M_llist_find(coop_active_threads, mythread, M_LLIST_MATCH_PTR);
	if (node == NULL) {
		/* Invalid thread */
		return M_FALSE;
	}

	/* Thread must be in a detached state, can't get status/retval */
	if (mythread->status != M_THREAD_COOP_STATUS_RUN && mythread->status != M_THREAD_COOP_STATUS_DONE)
		return M_FALSE;

	fthread = M_llist_node_val(M_llist_first(coop_active_threads));
	/* State that we are waiting for a thread to finish */
	while (mythread->status != M_THREAD_COOP_STATUS_DONE) {
		fthread->wait_join = mythread;
		M_thread_coop_yield(M_TRUE);
		fthread->wait_join = NULL;
		/* Technically if we've woken up, we shouldn't loop again */
	}

	/* If we've gotten here, that means we have been woken up because our thread is done */

	if (value_ptr)
		*value_ptr = mythread->retval;

	M_llist_remove_node(node);

	return M_TRUE;
}

static M_threadid_t M_thread_coop_self(M_thread_t **thread)
{
	void *th = M_llist_node_val(M_llist_first(coop_active_threads));

	if (thread != NULL)
		*thread = th;

	return (M_threadid_t)th;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int M_thread_coop_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	M_thread_coop_poll_t   *pollst = NULL;
	M_thread_coop_t        *thread;
	M_timeval_t             to_tv;
	int                     ret;

	/* Don't do the whole yield thing if it wants a quick check */
	if (timeout == 0) {
		do {
			ret =
#ifdef _WIN32
				M_pollemu
#else
				poll
#endif
				(fds, nfds, 0);
			/* If we didn't catch a EINTR signal */
			if (ret != -1 || (ret == -1 && errno != EINTR))
				break;
		} while (1);
	} else {
		thread = M_llist_node_val(M_llist_first(coop_active_threads));

		pollst       = M_malloc_zero(sizeof(*pollst));
		pollst->nfds = nfds;
		pollst->fds  = fds;
		if (timeout != -1) {
			M_time_gettimeofday(&to_tv);
			to_tv.tv_usec += timeout * 1000;
			if (to_tv.tv_usec > 1000000) {
				to_tv.tv_sec += to_tv.tv_usec / 1000000;
				to_tv.tv_usec = to_tv.tv_usec % 1000000;
			}
			thread->to_sec  = to_tv.tv_sec;
			thread->to_usec = to_tv.tv_usec;
		}

		thread->wait_poll = pollst;
		M_thread_coop_yield(M_TRUE);
		ret = pollst->ret;
		M_free(pollst);
		thread->wait_poll = NULL;
	}

	return ret;
}

#ifndef _WIN32
static M_bool M_thread_coop_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	int ret;

	ret = sigprocmask(how, set, oldset);
	if (ret == 0)
		return M_TRUE;
	return M_FALSE;
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_mutex_t *M_thread_coop_mutex_create(M_uint32 attr)
{
	M_thread_mutex_t *mutex;
	(void)attr;
	mutex = M_malloc_zero(sizeof(*mutex));
	return mutex;
}

static void M_thread_coop_mutex_destroy(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return;
	M_free(mutex);
}

static M_bool M_thread_coop_mutex_lock(M_thread_mutex_t *mutex)
{
	M_thread_coop_t       *thread;

	if (mutex == NULL)
		return M_FALSE;

	thread = M_llist_node_val(M_llist_first(coop_active_threads));
	if (mutex->thread_locked != thread) {
		while (mutex->thread_locked) {
			thread->wait_mutex = mutex;
			/* Should not return control to this thread until
			 * mutex is available, loop just in case! */
			M_thread_coop_yield(M_TRUE);
			thread->wait_mutex = NULL;
		}
	}
	mutex->thread_locked = thread;
	mutex->cnt++;

	return M_TRUE;
}

static M_bool M_thread_coop_mutex_trylock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	/* If the current thread owns the lock, or no thread
	 * owns the lock, go ahead and lock it */
	if (mutex->thread_locked == M_llist_node_val(M_llist_first(coop_active_threads)) || mutex->thread_locked == NULL)
		return M_thread_coop_mutex_lock(mutex);

	/* Mutex already locked by another thread */
	return M_FALSE;
}

static M_bool M_thread_coop_mutex_unlock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (mutex->thread_locked != M_llist_node_val(M_llist_first(coop_active_threads)))
		return M_FALSE;

	mutex->cnt--;
	if (mutex->cnt == 0) {
		mutex->thread_locked = NULL;
	}

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_cond_t *M_thread_coop_cond_create(M_uint32 attr)
{
	M_thread_cond_t *cond;
	(void)attr;

	cond = M_malloc_zero(sizeof(*cond));

	return cond;
}

static void M_thread_coop_cond_destroy(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	if (cond->waiting_threads != NULL)
		M_llist_destroy(cond->waiting_threads, M_TRUE);

	M_free(cond);
}

static M_bool M_thread_coop_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	M_thread_coop_t      *thread;

	if (cond == NULL || mutex == NULL || abstime == NULL)
		return M_FALSE;

	thread = M_llist_node_val(M_llist_first(coop_active_threads));

	/* Set some data so the scheduler will know when to wake up */
	thread->wait_cond = cond;
	thread->to_sec    = (M_time_t)abstime->tv_sec;
	thread->to_usec   = (M_suseconds_t)abstime->tv_usec;

	/* Add this thread to the list of waiters on the conditional */
	if (cond->waiting_threads == NULL)
		cond->waiting_threads = M_llist_create(NULL, M_LLIST_NONE);
	M_llist_insert(cond->waiting_threads, thread);

	M_thread_coop_mutex_unlock(mutex);

	/* When we wake up it'll either be because of a timeout, or a signal or broadcast */
	M_thread_coop_yield(M_TRUE);

	if (thread->wait_cond != NULL) {
	/* TIMEOUT */
		/* Remove from list of threads to wake on signal/broadcast */
		M_llist_remove_val(cond->waiting_threads, thread, M_LLIST_MATCH_PTR);

		/* Remove from thread that we're waiting on a conditional */
		thread->wait_cond = NULL;
		thread->to_sec    = 0;
		thread->to_usec   = 0;
		M_thread_coop_mutex_lock(mutex);
		return M_FALSE;
	}

	M_thread_coop_mutex_lock(mutex);
	return M_TRUE;
}

static M_bool M_thread_coop_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	M_timeval_t abstime;

	if (cond == NULL || mutex == NULL)
		return M_FALSE;

	M_mem_set(&abstime, 0, sizeof(abstime));
	return M_thread_coop_cond_timedwait(cond, mutex, &abstime);
}

static void M_thread_coop_cond_broadcast(M_thread_cond_t *cond)
{
	M_thread_coop_t      *thread;
	M_llist_node_t       *node;

	if (cond == NULL)
		return;

	node  = M_llist_first(cond->waiting_threads);
	while (node != NULL) {
		thread = M_llist_node_val(node);
		/* Clear scheduling marker to wake up all threads waiting */
		thread->wait_cond = NULL;
		thread->to_sec    = 0;
		thread->to_usec   = 0;
		node              = M_llist_node_next(node);
	}

	/* Clear waiting threads */
	M_llist_destroy(cond->waiting_threads, M_FALSE);
	cond->waiting_threads = NULL;
}

static void M_thread_coop_cond_signal(M_thread_cond_t *cond)
{
	M_thread_coop_t      *thread;

	if (cond == NULL)
		return;

	thread = M_llist_take_node(M_llist_first(cond->waiting_threads));
	if (thread != NULL) {
		thread->wait_cond = NULL;
		thread->to_sec    = 0;
		thread->to_usec   = 0;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_thread_coop_register(M_thread_model_callbacks_t *cbs)
{
	if (cbs == NULL)
		return;

	M_mem_set(cbs, 0, sizeof(*cbs));

	cbs->init   = M_thread_coop_init;
	cbs->deinit = M_thread_coop_deinit;
	/* Thread */
	cbs->thread_create        = M_thread_coop_create;
	cbs->thread_join          = M_thread_coop_join;
	cbs->thread_self          = M_thread_coop_self;
	cbs->thread_yield         = M_thread_coop_yield;
	cbs->thread_sleep         = M_thread_coop_sleep;
	cbs->thread_set_priority  = NULL;
	cbs->thread_set_processor = NULL;
	/* System */
	cbs->thread_poll    = M_thread_coop_poll;
#ifndef _WIN32
	cbs->thread_sigmask = M_thread_coop_sigmask;
#endif
	/* Mutex */
	cbs->mutex_create  = M_thread_coop_mutex_create;
	cbs->mutex_destroy = M_thread_coop_mutex_destroy;
	cbs->mutex_lock    = M_thread_coop_mutex_lock;
	cbs->mutex_trylock = M_thread_coop_mutex_trylock;
	cbs->mutex_unlock  = M_thread_coop_mutex_unlock;
	/* Cond */
	cbs->cond_create    = M_thread_coop_cond_create;
	cbs->cond_destroy   = M_thread_coop_cond_destroy;
	cbs->cond_timedwait = M_thread_coop_cond_timedwait;
	cbs->cond_wait      = M_thread_coop_cond_wait;
	cbs->cond_broadcast = M_thread_coop_cond_broadcast;
	cbs->cond_signal    = M_thread_coop_cond_signal;
	/* Read Write Lock */
	cbs->rwlock_create  = M_thread_rwlock_emu_create;
	cbs->rwlock_destroy = M_thread_rwlock_emu_destroy;
	cbs->rwlock_lock    = M_thread_rwlock_emu_lock;
	cbs->rwlock_unlock  = M_thread_rwlock_emu_unlock;
}
