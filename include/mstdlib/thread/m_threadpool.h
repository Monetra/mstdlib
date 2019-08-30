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

#ifndef __M_THREADPOOL_H__
#define __M_THREADPOOL_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/thread/m_thread.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_threadpool Thread Pool
 *  \ingroup    m_thread
 *
 * Implementation of a thread pool for having a limited the number of threads available
 * to workers. Threads in the pool will only be destroyed when the pool is destroyed.
 * A maximum number of threads will be created by the pool. Workers are assigned to
 * parents which can be used to logically separate workers by tasks.
 *
 * Example:
 *
 * \code{.c}
 *     static M_uint32 count = 0;
 *
 *     static void pool_task(void *arg)
 *     {
 *         (void)arg;
 *         M_atomic_inc_u32(&count);
 *     }
 *
 *     int main(int argc, char **argv)
 *     {
 *         M_threadpool_t        *pool;
 *         M_threadpool_parent_t *parent;
 *         char                   args[32];
 *
 *         M_mem_set(args, 0, sizeof(args));
 *
 *         pool   = M_threadpool_create(16, 16, 0, SIZE_MAX);
 *         parent = M_threadpool_parent_create(pool);
 *
 *         M_threadpool_dispatch(parent, pool_task, (void **)&args, sizeof(args));
 *         M_threadpool_parent_wait(parent);
 *         
 *         M_threadpool_parent_destroy(parent);
 *         M_threadpool_destroy(pool);
 *
 *         M_printf("count='%u'\n", count);
 *
 *         return 0;
 *     } 
 * \endcode
 *
 * @{
 */

struct M_threadpool;
typedef struct M_threadpool M_threadpool_t;

struct M_threadpool_parent;
typedef struct M_threadpool_parent M_threadpool_parent_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initializes a new threadpool and spawns the minimum number of threads requested.
 *
 * \param[in] min_threads     Minimum number of threads to spawn, 0 to not pre-spawn
 *                            any.
 * \param[in] max_threads     Maximum number of threads to spawn, any number above the
 *                            min_threads number will be spawned on demand, and idle
 *                            threads will be shutdown after the specified idle time.
 *                            Must be greater than 0.
 * \param[in] idle_time_ms    Number of milliseconds a thread can be idle for before
 *                            it is destroyed when the total thread count is above
 *                            min_threads.  If min_threads and max_threads are the same
 *                            value, this parameter is ignored.  Use M_UINT64_MAX to
 *                            never terminate an idle thread, or use 0 to never allow
 *                            idle threads.
 * \param[in] queue_max_size  If 0, will calculate a desirable queue size based on the
 *                            maximum thread count.  Otherwise, must be at least the size
 *                            of the thread pool.  It often makes sense to have the queue
 *                            larger than the threadpool size to prevent the threads from
 *                            sleeping.  When inserting into the queue, if there are no
 *                            available slots the M_threadpool_dispatch() function will
 *                            block.  If blocking is not desirable, use SIZE_MAX to
 *                            allow an unbounded number of queue slots.
 *
 * \return initialized threadpool or NULL on failure
 */
M_API M_threadpool_t *M_threadpool_create(size_t min_threads, size_t max_threads, M_uint64 idle_time_ms, size_t queue_max_size);


/*! Shuts down the thread pool, waits for all threads to exit.
 *
 * \param[in] pool initialized threadpool.
 */
M_API void M_threadpool_destroy(M_threadpool_t *pool);


/*! Creates a new parent/user/consumer of the threadpool.
 *
 * This is the handle used to insert tasks and wait for task completion
 * specific to the consumer.  
 *
 * It is safe to share this handle across multiple threads if convenient
 * as long as it is guaranteed to not be destroyed until all consumers are done
 * using it.  If sharing across multiple threads, it probably would mean you
 * would not be using M_threadpool_parent_wait() from multiple threads
 * simultaneously.
 *
 * \param[in] pool Initialized thread pool.
 *
 * \return initialized parent/user/consumer handle.
 */
M_API M_threadpool_parent_t *M_threadpool_parent_create(M_threadpool_t *pool);


/*! Frees the parent handle.
 *
 * There must be no oustanding tasks prior to calling this.  Call
 * M_threadpool_parent_wait() first if unsure to wait on all tasks to complete.
 *
 * \param[in] parent Initialized parent handle.
 *
 * \return M_FALSE if there are tasks remaining, M_TRUE if successfully
 *         cleaned up.
 */
M_API M_bool M_threadpool_parent_destroy(M_threadpool_parent_t *parent);


/*! Dispatch a task or set of tasks to the threadpool.  Identical to
 *  M_threadpool_dispatch_notify() if passed a NULL finished argument.
 *
 * Requires a callback function to do the processing and an argument that is
 * passed to the function.  There is no way to retrieve a return value from the
 * task, so the argument passed to the task should hold a result parameter if
 * it is necessary to know the completion status.  Multiple tasks may be queued
 * simultaneously.
 *
 * This may take a while to complete if there are no queue slots available.
 *
 * \param[in,out] parent    Initialized parent handle.
 * \param[in]     task      Task callback.
 * \param[in,out] task_args Argument array to pass to each task (one per task).
 * \param[in]     num_tasks total number of tasks being enqueued.
 */
M_API void M_threadpool_dispatch(M_threadpool_parent_t *parent, void (*task)(void *), void **task_args, size_t num_tasks);


/*! Dispatch a task or set of tasks to the threadpool and notify on task completion.
 *
 * Requires a callback function to do the processing and an argument that is
 * passed to the function.  There is no way to retrieve a return value from the
 * task, so the argument passed to the task should hold a result parameter if
 * it is necessary to know the completion status.  Multiple tasks may be queued
 * simultaneously.
 *
 * This may take a while to complete if there are no queue slots available.
 *
 * \param[in,out] parent    Initialized parent handle.
 * \param[in]     task      Task callback.
 * \param[in,out] task_args Argument array to pass to each task (one per task).
 * \param[in]     num_tasks total number of tasks being enqueued.
 * \param[in]     finished  Optional. Callback to call for each task completion.
 *                          Will pass the callback the same argument passed to the task.
 *                          Use NULL if no notification desired.
 */
M_API void M_threadpool_dispatch_notify(M_threadpool_parent_t *parent, void (*task)(void *), void **task_args, size_t num_tasks, void (*finished)(void *));


/*! Count the number of queue slots available to be enqueued for a threadpool.
 *
 *  \param[in] pool initialized threadpool.
 */
M_API size_t M_threadpool_available_slots(const M_threadpool_t *pool);


/*! Wait for a thread to become available for processing tasks.
 *
 * This explicitly waits for a THREAD and NOT an available queue slot which
 * there could be available slots.  This is meant as an optimization in some
 * instances where you want to ensure you enqueue some things together,
 * especially if you're trying to manage SQL locks for tasks being performed.
 * Typically though, this function would never be used.
 *
 * \param[in] parent Initialized parent handle.
 */
M_API void M_threadpool_wait_available_thread(M_threadpool_parent_t *parent);


/*! Get the current count of the number of threads in the thread pool.
 *
 * \param[in] pool Initialized pool handle.
 * \return count of threads
 */
M_API size_t M_threadpool_num_threads(const M_threadpool_t *pool);


/*! Wait for all queued tasks to complete then return.
 *
 * This is a blocking function with no return value.  It is not recommended to
 * call this from mulitiple threads simultaneously.
 *
 * \param[in] parent the initialized parent/user/consumer handle.
 */
M_API void M_threadpool_parent_wait(M_threadpool_parent_t *parent);


/*! @} */

__END_DECLS

#endif /* __M_THREADPOOL_H__ */
