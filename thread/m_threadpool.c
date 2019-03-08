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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! You want the queue larger than the threadpool size so that threads
 * that are finished always have tasks to process so they do not go
 * to sleep and have to be woken back up */
#define THREADQUEUE_MULTIPLIER 8

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Queue holding tasks to be run */
typedef struct M_threadpool_queue_st {
	void                 (*task)(void *); /*!< Task callback */
	void                  *task_arg;      /*!< Argument for task callback */
	M_threadpool_parent_t *parent;        /*!< Handle of threadpool user */
} M_threadpool_queue_t;

/*! Main structure holding metadata for threadpool */
struct M_threadpool {
	size_t                min_threads;      /*!< Min count of threads */
	size_t                max_threads;      /*!< Max count of threads */
	size_t                num_threads;      /*!< Current number of threads */
	size_t                num_idle_threads; /*!< The current number of threads that are idle */

	M_uint64              idle_time_ms;     /*!< Thread idle timeout in ms */

	M_bool                up;               /*!< M_FALSE if threadpool is shutting down */

	/* Queue */
	M_llist_t            *queue;            /*!< Task queue. */
	M_thread_mutex_t     *queue_lock;       /*!< Lock used for inserting and removing tasks */
	M_thread_cond_t      *queue_icond;      /*!< Conditional for users waiting to put tasks
	                                             into the queue */
	M_thread_cond_t      *queue_ocond;      /*!< Conditional for threads waiting to take tasks
	                                             out of the queue */
	size_t                queue_max_size;   /*!< Maximum queue size */
	size_t                queue_waiters;    /*!< Number of users waiting to insert tasks into the queue */
};

/*! Each Parent/User/Consumer needs a handle to manage their own state */
struct M_threadpool_parent {
	M_thread_cond_t  *cond;            /*!< Conditional to block while waiting on a queue slot
	                                        to be emptied */
	M_thread_mutex_t *lock;            /*!< Lock used in conjunction with conditional */
	M_bool            is_waiting;      /*!< Whether or not the parent is waiting to be signalled */
	size_t            tasks_remaining; /*!< Number of tasks remaining to be processed for parent */
	M_threadpool_t   *pool;            /*!< Pointer to the threadpool handle */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initialize the threadpool queue.
 *  \param pool           handle to initialized threadpool
 *  \param queue_max_size Max size of the queue.  Must be at least the size
 *                        of the number of threads.  If 0, will calculate an
 *                        optimal size based on the number of threads.  use
 *                        SIZE_MAX for unlimited;
 */
static void M_threadpool_queue_init(M_threadpool_t *pool, size_t queue_max_size)
{
	size_t size         = queue_max_size;

	if (size == 0) {
		/* Make sure there is not an overflow in the calculation */
		if (pool->max_threads < SIZE_MAX / THREADQUEUE_MULTIPLIER) {
			size = pool->max_threads * THREADQUEUE_MULTIPLIER;
		} else {
			size = SIZE_MAX;
		}
	}

	pool->queue          = M_llist_create(NULL, M_LLIST_NONE);
	pool->queue_max_size = size;
	pool->queue_waiters  = 0;
	pool->queue_lock     = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	pool->queue_icond    = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	pool->queue_ocond    = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
}


/*! Free the queue
 *  \param pool handle to initialized threadpool */
static void M_threadpool_queue_finish(M_threadpool_t *pool)
{
	M_threadpool_queue_t *q;

	/* Destroy any queue entries that still exist */
	while ((q = M_llist_take_node(M_llist_first(pool->queue))) != NULL)
		M_free(q);

	M_llist_destroy(pool->queue, M_TRUE);
	pool->queue          = NULL;
	pool->queue_max_size = 0;
	M_thread_mutex_destroy(pool->queue_lock);
	M_thread_cond_destroy(pool->queue_icond);
	M_thread_cond_destroy(pool->queue_ocond);
}


/*! Fetch the next task to perform from the pool.  Used by the threads in the
 *  threadpool
 *  \param pool            Initialized threadpool handle
 *  \param queue_copy[out] Copy of queue task to perform
 *  \return M_TRUE on success, M_FALSE on queue shutdown or thread idle timeout expired */
static M_bool M_threadpool_queue_fetch(M_threadpool_t *pool, M_threadpool_queue_t *queue_copy)
{
	M_bool acquired   = M_FALSE;
	M_bool is_timeout = M_FALSE;

	M_mem_set(queue_copy, 0, sizeof(*queue_copy));

	M_thread_mutex_lock(pool->queue_lock);

	while (pool->up) {
		M_threadpool_queue_t *q = M_llist_take_node(M_llist_first(pool->queue));
		if (q != NULL) {
			queue_copy->parent   = q->parent;
			queue_copy->task     = q->task;
			queue_copy->task_arg = q->task_arg;
			M_free(q);

			/* Signal someone waiting for a queue slot to put a task in */
			if (pool->queue_waiters) {
				M_thread_cond_signal(pool->queue_icond);
			}

			acquired = M_TRUE;
			break;
		}

		/* NOTE: even on a timeout condition, we'll look for a task first before
		 *       exiting just to make sure we don't have an accidental stall */
		if (is_timeout && pool->num_threads > pool->min_threads) {
			break;
		}
		is_timeout = M_FALSE;


		/* Wait for a signal to see if there is something to be processed
		 * there is no need here to ensure order or anything, the tasks
		 * fetching from the queue is first to get a lock */
		pool->num_idle_threads++;

		/* Odd, we have a waiter registered, but no queued entries.  This could 
		 * happen if all threads were busy, but the caller was waiting on 
		 * M_threadpool_wait_available_thread() instead of an actual queue slot.
		 */
		if (pool->queue_waiters) {
			M_thread_cond_signal(pool->queue_icond);
		}


		if (pool->num_threads > pool->min_threads && pool->idle_time_ms != M_UINT64_MAX) {
			if (pool->idle_time_ms == 0 || !M_thread_cond_timedwait(pool->queue_ocond, pool->queue_lock, pool->idle_time_ms)) {
					is_timeout = M_TRUE;
			}
		} else {
			M_thread_cond_wait(pool->queue_ocond, pool->queue_lock);
		}

		pool->num_idle_threads--;
	}
	M_thread_mutex_unlock(pool->queue_lock);

	return acquired;
}


/*! Function implementing an individual thread.  Loops looking for and
 *  performing tasks until the threadpool is shutdown
 * \param arg is the initialized threadpool handle
 * \return always returns NULL, return value is meaningless
 */
static void *M_threadpool_thread(void *arg)
{
	M_threadpool_t       *pool = arg;
	M_threadpool_queue_t  task;

	while (1) {
		/* The only reason this would fail is on shutdown or idle timeout */
		if (!M_threadpool_queue_fetch(pool, &task))
			break;

		/* Perform task */
		task.task(task.task_arg);

		/* Tell the parent the task is done, and wake them up if we were the
		 * last task left */
		M_thread_mutex_lock(task.parent->lock);
		task.parent->tasks_remaining--;
		if (task.parent->is_waiting && task.parent->tasks_remaining == 0) {
			/* use broadcast instead of signal incase multiple threads are
			 * calling the parent wait function even though it isn't recommended */
			M_thread_cond_broadcast(task.parent->cond);
		}
		M_thread_mutex_unlock(task.parent->lock);
	}

	M_thread_mutex_lock(pool->queue_lock);
	pool->num_threads--;
	/* On M_threadpool_destroy() it will block on queue_icond until woken up
	 * with the thread count at 0 */
	if (!pool->up && pool->num_threads == 0)
		M_thread_cond_broadcast(pool->queue_icond);
	M_thread_mutex_unlock(pool->queue_lock);

	return NULL;
}


/*! pool->queue_lock must be locked before calling this function */
static M_bool M_threadpool_thread_spawn(M_threadpool_t *pool)
{
	M_threadid_t threadid = M_thread_create(NULL, M_threadpool_thread, pool);
	if (threadid == 0)
		return M_FALSE;

	pool->num_threads++;
	return M_TRUE;
}



/*! Insert a new task into the threadpool.  If there are no free slots, wait
 *  for one to open up.  Can insert multiple tasks at once.
 *  \param parent    initialized parent (user/consumer) of threadpool
 *  \param task      Callback for task to perform
 *  \param task_args Argument passed to task (array, one per task)
 *  \param num_tasks Number of tasks being inserted */
static void M_threadpool_queue_insert(M_threadpool_parent_t *parent, void (*task)(void *), void **task_args, size_t num_tasks)
{
	M_bool          i_just_woke_up = M_FALSE;
	M_threadpool_t *pool           = parent->pool;

	M_thread_mutex_lock(pool->queue_lock);
	while (1) {

		/* Spawn a new thread on demand if needed */
		if (pool->num_idle_threads <= M_llist_len(pool->queue) && pool->num_threads < pool->max_threads)
			M_threadpool_thread_spawn(pool);

		if (pool->queue_waiters == 0 || i_just_woke_up) {
			if (pool->queue_max_size > M_llist_len(pool->queue)) {
				M_threadpool_queue_t *q = M_malloc_zero(sizeof(*q));
				q->parent   = parent;
				q->task     = task;
				if (task_args != NULL)
					q->task_arg = *task_args;

				M_llist_insert(pool->queue, q);

				/* Wake up a thread waiting for things to be queued */
				M_thread_cond_signal(pool->queue_ocond);

				if (task_args != NULL)
					task_args++;
				num_tasks--;

				/* Allow us to keep looping while we have tasks to queue */
				if (num_tasks == 0) {
					break;
				} else {
					i_just_woke_up = M_TRUE;
					continue;
				}
			}
		}

		pool->queue_waiters++;
		M_thread_cond_wait(pool->queue_icond, pool->queue_lock);
		pool->queue_waiters--;
		i_just_woke_up = M_TRUE;
	}
	M_thread_mutex_unlock(pool->queue_lock);
}



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Shuts down the thread pool, waits for all threads to exit.
 *  \param pool initialized threadpool */
void M_threadpool_destroy(M_threadpool_t *pool)
{
	if (pool == NULL)
		return;

	/* Tell all threads to shutdown */
	M_thread_mutex_lock(pool->queue_lock);
	pool->up = M_FALSE;
	M_thread_cond_broadcast(pool->queue_ocond);

	/* Wait for all threads to shutdown.  The last thread will signal
	 * queue_icond when it exits so we know, but we need to handle
	 * spurious wakeups as well. */
	while (pool->num_threads) {
		M_thread_cond_wait(pool->queue_icond, pool->queue_lock);
	}

	M_thread_mutex_unlock(pool->queue_lock);

	/* Cleanup */
	M_threadpool_queue_finish(pool);

	M_free(pool);
}


M_threadpool_t *M_threadpool_create(size_t min_threads, size_t max_threads, M_uint64 idle_time_ms, size_t queue_max_size)
{
	M_threadpool_t  *pool;
	size_t           i;

	pool = M_malloc_zero(sizeof(*pool));

	pool->min_threads = min_threads;
	pool->max_threads = max_threads;
	if (pool->max_threads < pool->min_threads)
		pool->max_threads = pool->min_threads;

	if (queue_max_size != 0 && queue_max_size < pool->max_threads)
		queue_max_size = 0;
	M_threadpool_queue_init(pool, queue_max_size);

	pool->idle_time_ms = idle_time_ms;
	pool->up           = M_TRUE;
	M_thread_mutex_lock(pool->queue_lock);

	for (i=0; i<pool->min_threads; i++) {
		if (!M_threadpool_thread_spawn(pool)) {
			M_thread_mutex_unlock(pool->queue_lock);
			M_threadpool_destroy(pool);
			return NULL;
		}
	}

	M_thread_mutex_unlock(pool->queue_lock);

	return pool;
}


M_threadpool_parent_t *M_threadpool_parent_create(M_threadpool_t *pool)
{
	M_threadpool_parent_t *parent;

	if (pool == NULL)
		return NULL;

	parent       = M_malloc_zero(sizeof(*parent));
	parent->pool = pool;
	parent->cond = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	parent->lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	return parent;
}


M_bool M_threadpool_parent_destroy(M_threadpool_parent_t *parent)
{
	if (parent == NULL)
		return M_FALSE;

	M_thread_mutex_lock(parent->lock);
	if (parent->tasks_remaining) {
		M_thread_mutex_unlock(parent->lock);
		return M_FALSE;
	}
	M_thread_mutex_unlock(parent->lock);
	M_thread_cond_destroy(parent->cond);
	M_thread_mutex_destroy(parent->lock);
	M_free(parent);
	return M_TRUE;
}


size_t M_threadpool_available_slots(const M_threadpool_t *pool)
{
	size_t cnt;

	if (pool == NULL)
		return 0;

	M_thread_mutex_lock(pool->queue_lock);
	cnt = pool->queue_max_size - M_llist_len(pool->queue);
	M_thread_mutex_unlock(pool->queue_lock);
	return cnt;
}


size_t M_threadpool_num_threads(const M_threadpool_t *pool)
{
	size_t cnt;

	if (pool == NULL)
		return 0;

	M_thread_mutex_lock(pool->queue_lock);
	cnt = pool->num_threads;
	M_thread_mutex_unlock(pool->queue_lock);
	return cnt;
}


void M_threadpool_wait_available_thread(M_threadpool_parent_t *parent)
{
	M_threadpool_t *pool;

	if (parent == NULL)
		return;

	pool = parent->pool;

	M_thread_mutex_lock(pool->queue_lock);
	while (1) {
		if (pool->num_idle_threads || pool->num_threads < pool->max_threads)
			break;

		/* Sleep until we're awoken.  Doesn't mean there is an available
		 * thread, we'll get signalled as each slot is cleared so we need
		 * to loop again */
		pool->queue_waiters++;
		M_thread_cond_wait(pool->queue_icond, pool->queue_lock);
		pool->queue_waiters--;

		/* Relay signal so we don't starve an ACTUAL consumer */
		if (pool->queue_waiters) {
			M_thread_cond_signal(pool->queue_icond);
		}

	}

	M_thread_mutex_unlock(pool->queue_lock);
}


void M_threadpool_dispatch(M_threadpool_parent_t *parent, void (*task)(void *), void **task_args, size_t num_tasks)
{
	if (parent == NULL || task == NULL || num_tasks == 0)
		return;

	M_thread_mutex_lock(parent->lock);
	parent->tasks_remaining += num_tasks;
	M_thread_mutex_unlock(parent->lock);
	M_threadpool_queue_insert(parent, task, task_args, num_tasks);
}


void M_threadpool_parent_wait(M_threadpool_parent_t *parent)
{
	if (parent == NULL)
		return;

	M_thread_mutex_lock(parent->lock);
	while (1) {
		if (parent->tasks_remaining == 0)
			break;
		parent->is_waiting = M_TRUE;
		M_thread_cond_wait(parent->cond, parent->lock);
	}
	parent->is_waiting = M_FALSE;
	M_thread_mutex_unlock(parent->lock);
}

