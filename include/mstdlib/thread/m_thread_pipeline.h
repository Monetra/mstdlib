/* The MIT License (MIT)
 *
 * Copyright (c) 2021 Monetra Technologies, LLC.
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

#ifndef __M_THREAD_PIPELINE_H__
#define __M_THREAD_PIPELINE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/thread/m_thread.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_thread_pipeline Thread Task Pipeline
 *  \ingroup    m_thread
 *
 * Implementation of a thread pipeline.  Useful if there are a series of tasks which
 * must be completed in order, and each task has one or more CPU or I/O intensive
 * steps.  This allows handoff to a dedicated thread for each step, while ensuring
 * each task result is processed in a serialized manner.  For CPU intensive workloads
 * this helps in spreading load across multiple CPU cores, and also allows I/O to
 * be embedded into a step that can run without blocking CPU.
 *
 * Example:
 *
 * \code{.c}
 *     struct M_thread_pipeline_task {
 *        const char *filename;
 *        M_uint8    *buf;
 *        size_t      buf_len;
 *     }
 *
 *     static void finish_cb(M_thread_pipeline_task_t *task, M_thread_pipeline_result_t result)
 *     {
 *       M_free(task->buf);
 *       M_free(task);
 *     }
 *
 *     static M_bool fetch_cb(M_thread_pipeline_task_t *task)
 *     {
 *        task->buf = fetch_data(task->name, &task->buf_len);
 *        if (!task->buf)
 *          return M_FALSE;
 *        return M_TRUE;
 *     }
 *
 *     static M_bool compress_cb(M_thread_pipeline_task_t *task)
 *     {
 *        M_uint8 *uncompressed     = task->buf;
 *        size_t   uncompressed_len = task->buf_len;
 *
 *        task->buf = my_compress(uncompressed, uncompressed_len, &task->buf_len);
 *        M_free(uncompressed);
 *        if (!task->buf)
 *          return M_FALSE;
 *        return M_TRUE;
 *     }
 *
 *     static M_bool encrypt_cb(M_thread_pipeline_task_t *task)
 *     {
 *        M_uint8 *compressed     = task->buf;
 *        size_t   compressed_len = task->buf_len;
 *
 *        task->buf = my_encrypt(compressed, compressed_len, &task->buf_len);
 *        M_free(compressed);
 *        if (task->buf == NULL)
 *          return M_FALSE;
 *        return M_TRUE;
 *     }
 *
 *     static M_bool write_cb(M_thread_pipeline_task_t *task)
 *     {
 *        M_fs_file_t *fp = NULL;
 *        M_fs_error_t err = M_FS_ERROR_SUCCESS;
 *        char filename[1024];
 *        M_snprintf(filename, sizeof(filename), "%s.out", task->name);
 *        err = M_fs_file_open(&fp, task->name, 0,
 *                             M_FS_FILE_MODE_READ|M_FS_FILE_MODE_WRITE|M_FS_FILE_MODE_OVERWRITE, NULL);
 *        if (err != M_FS_ERROR_SUCCESS)
 *          goto fail;
 *
 *        err = M_fs_file_write(fp, task->buf, task->buf_len, &wrote_len, M_FS_FILE_RW_FULLBUF);
 *        if (err == M_FS_ERROR_SUCCESS && wrote_len != task->buf_len) {
 *          err = M_FS_ERROR_FILE_2BIG;
 *        }
 *     fail:
 *         M_fs_file_close(fp);
 *         return err == M_FS_ERROR_SUCCESS?M_TRUE:M_FAIL;
 *     }
 *
 *     int main()
 *     {
 *       const char *tasks[] = {
 *         "red",
 *         "white",
 *         "blue",
 *         "yellow",
 *         "green",
 *         "brown",
 *         NULL
 *       };
 *       M_thread_pipeline_t       *pipeline = NULL;
 *       M_thread_pipeline_steps_t *steps    = NULL;
 *       size_t                     i;
 *
 *       steps = M_thread_pipeline_steps_create();
 *       M_thread_pipeline_steps_insert(steps, fetch_cb);
 *       M_thread_pipeline_steps_insert(steps, compress_cb);
 *       M_thread_pipeline_steps_insert(steps, encrypt_cb);
 *       M_thread_pipeline_steps_insert(steps, write_cb);
 *
 *       pipeline = M_thread_pipeline_create(steps, M_THREAD_PIPELINE_FLAG_NONE, finish_cb);
 *       M_thread_pipeline_steps_destroy(steps);
 *
 *       for (i=0; tasks[i] != NULL; i++) {
 *         M_thread_pipeline_task_t *task = M_malloc_zero(sizeof(*task));
 *         task->name = tasks[i];
 *         M_thread_pipeline_task_insert(pipeline, task);
 *       }
 *
 *       M_thread_pipeline_wait(pipeline, 0);
 *       M_thread_pipeline_destroy(pipeline);
 *       return 0;
 *     }
 * \endcode
 *
 * @{
 */

/*! Flags for pipeline initialization */
typedef enum {
    M_THREAD_PIPELINE_FLAG_NONE    = 0,      /*!< No flags, normal operation */
    M_THREAD_PIPELINE_FLAG_NOABORT = 1 << 0  /*!< Do not abort all other enqueued tasks due to a failure of another task */
} M_thread_pipeline_flags_t;

/*! Caller-defined structure to hold task data.  It is the only data element
 *  passed from thread to thread and must track its own state based on knowing
 *  how the pipeline is configured */
struct M_thread_pipeline_task;
typedef struct M_thread_pipeline_task M_thread_pipeline_task_t;

/*! Structure used to pass steps into M_thread_pipeline_create().  Initialized
 *  with M_thread_pipeline_steps_create() and destroyed with M_thread_pipeline_steps_destroy() */
struct M_thread_pipeline_steps;
typedef struct M_thread_pipeline_steps M_thread_pipeline_steps_t;

/*! Internal state tracking for thread pipeline, initialized via M_thread_pipeline_create()
 *  and destroyed via M_thread_pipeline_destroy() */
struct M_thread_pipeline;
typedef struct M_thread_pipeline M_thread_pipeline_t;

/*! User-defined callback for each step
 *  \param[in] task User-defined task data to be operated on
 *  \return M_TRUE if completed successfully, M_FALSE otherwise
 */
typedef M_bool (*M_thread_pipeline_task_cb)(M_thread_pipeline_task_t *task);

/*! Result codes passed to M_thread_pipeline_taskfinish_cb() */
typedef enum {
    M_THREAD_PIPELINE_RESULT_SUCCESS = 1, /*!< Task completed successfully */
    M_THREAD_PIPELINE_RESULT_FAIL    = 2, /*!< Task failed -- record error in user-defined task structure */
    M_THREAD_PIPELINE_RESULT_ABORT   = 3  /*!< Task was forcibly aborted due to a failure of another task,
                                               or M_thread_pipeline_destroy() was called before completion */
} M_thread_pipeline_result_t;

/*! User-defined, and required, callback at the completion of each task.
 *  This may be called:
 *   - Upon completion of task, whether successful or not (see result)
 *   - Upon abort due to a prior task failure if the pipeline is configured
 *     to abort all other tasks if a single task fails (default).
 *  At a minimum, this must free any memory associated with the user-defined
 *  task structure.
 *
 *  \param[in] task User-defined task structure
 *  \param[in] result the result of the task, one of M_thread_pipeline_result_t.
 */
typedef void (*M_thread_pipeline_taskfinish_cb)(M_thread_pipeline_task_t *task, M_thread_pipeline_result_t result);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initialize an empty pipeline step list
 *  \return initialized and empty step list.  NULL on failure.
 *          freed with M_thread_pipeline_steps_destroy()*/
M_API M_thread_pipeline_steps_t *M_thread_pipeline_steps_create(void);

/*! Insert a step into the task pipeline
 *
 *  \param[in] steps   Initialized pipeline steps structure from M_thread_pipeline_steps_create()
 *  \param[in] task_cb Task to perform
 *  \return M_TRUE on success, M_FALSE on usage error.
 */
M_API M_bool M_thread_pipeline_steps_insert(M_thread_pipeline_steps_t *steps, M_thread_pipeline_task_cb task_cb);

/*! Destroy the task step list initialized with M_thread_pipeline_steps_create()
 *
 *  \param[in] steps   Initialized piipeline steps structure from M_thread_pipeline_steps_create()
 */
M_API void M_thread_pipeline_steps_destroy(M_thread_pipeline_steps_t *steps);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initialize the thread pipeline with the various steps to be performed for each task.
 *  This will spawn one thread per step and immediately start all threads.  There is no
 *  additional function to start the pipeline other than to insert each task to be
 *  processed.
 *
 *  \param[in] steps     Pointer to steps to perform for each task.  The passed in pointer
 *                       is internally duplicated, so it may be destroyed immediately
 *                       after this function returns.
 *  \param[in] flags   One or more pipeline flags from M_thread_pipeline_flags_t
 *  \param[in] finish_cb Callback to be called after each task is completed.  At a minimum,
 *                       this should free the memory assocated with the task pointer.  The
 *                       finish_cb is not called from the same thread as enqueued it so
 *                       proper thread concurrency protections (e.g. mutexes) must be
 *                       in place.
 *  \return initialized M_thread_pipeline_t or NULL on failure (usage, thread limits)
 */
M_API M_thread_pipeline_t *M_thread_pipeline_create(const M_thread_pipeline_steps_t *steps, int flags, M_thread_pipeline_taskfinish_cb finish_cb);

/*! Insert a task into the thread pipeline.
 *
 *  This function will enqueue tasks into an internal task list indefinitely and will not block. If it
 *  is desirable to cap the enqueued task list, please see M_thread_pipeline_wait() and M_thread_pipeline_queue_count().
 *
 *  \param[in] pipeline Pointer to the initialized thread pipeline returned from M_thread_pipeline_create().
 *  \param[in] task     User-defined task structure describing task to be perfomed for each step.
 *                      It is the responsibility of the user to define their own private
 *                      struct M_thread_pipeline_task with all members and necessary state tracking
 *                      to perform each step callback.  It is guaranteed that no more than 1 step will
 *                      be accessing this structure in parallel.
 *  \return M_TRUE if task is inserted, M_FALSE on either usage error or if task could not be
 *          enqueued as a prior step had failed.
 */
M_API M_bool M_thread_pipeline_task_insert(M_thread_pipeline_t *pipeline, M_thread_pipeline_task_t *task);

/*! Wait pipeline tasks/steps to complete to the task queue limit specified.
 *  \param[in] pipeline    Pipeline initialized with M_thread_pipeline_create()
 *  \param[in] queue_limit Will block until the queued task list is reduced to
 *                         at least this size.  Use 0 to wait until all tasks are
 *                         completed.
 */
M_API void M_thread_pipeline_wait(M_thread_pipeline_t *pipeline, size_t queue_limit);


/*! Count of queued tasks, this includes the task currently being processed if any.
 * \param[in] pipeline Pipeline initialized with M_thread_pipeline_create()
 * \return count of queued tasks.
 */
M_API size_t M_thread_pipeline_queue_count(M_thread_pipeline_t *pipeline);


/*! Retrieve if the pipeline is in a good state.  The only time a pipeline will
 *  not be in a good state is if a step failed.
 *  \return M_TRUE if in a good state
 */
M_API M_bool M_thread_pipeline_status(M_thread_pipeline_t *pipeline);


/*! Destroy the thread pipeline.  If there are any outstanding tasks/steps, they will be
 *  aborted and return an abort error code to their finish_cb
 *  \param[in] pipeline Pipeline initialized with M_thread_pipeline_create()
 */
M_API void M_thread_pipeline_destroy(M_thread_pipeline_t *pipeline);



/*! @} */

__END_DECLS

#endif /* __M_THREAD_PIPELINE_H__ */
