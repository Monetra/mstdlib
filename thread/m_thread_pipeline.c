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

#include "m_config.h"

#include <mstdlib/mstdlib_thread.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_thread_pipeline_steps {
	M_list_t *steps;
};

M_thread_pipeline_steps_t *M_thread_pipeline_steps_create(void)
{
	M_thread_pipeline_steps_t *steps = M_malloc_zero(sizeof(*steps));
	steps->steps = M_list_create(NULL, M_LIST_NONE);
	return steps;
}

M_bool M_thread_pipeline_steps_insert(M_thread_pipeline_steps_t *steps, M_thread_pipeline_task_cb task_cb)
{
	if (steps == NULL || task_cb == NULL)
		return M_FALSE;
	M_list_insert(steps->steps, (void *)task_cb);
	return M_TRUE;
}

void M_thread_pipeline_steps_destroy(M_thread_pipeline_steps_t *steps)
{
	if (steps == NULL)
		return;
	M_list_destroy(steps->steps, M_TRUE);
	M_free(steps);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Thread-specific step/task */
typedef struct {
	M_thread_pipeline_t      *parent;   /*!< Link to parent */
	size_t                    idx;      /*!< Index of step in pipeline */
	M_thread_pipeline_task_cb task_cb;  /*!< Callback registered to process task */
	M_threadid_t              threadid; /*!< ID of Thread (for joining) */
	M_thread_pipeline_task_t *task;     /*!< Current task to be processed. NULL if no task. */
	M_thread_cond_t          *cond;     /*!< Thread specific conditional used to wake thread to look for task or exit */
} M_thread_pipeline_step_t;


struct M_thread_pipeline {
	M_bool                          status;    /*!< Whether pipeline is active or not.  M_FALSE if task fail or M_thread_pipeline_destroy() is called */
	M_thread_mutex_t               *lock;      /*!< Global lock, all threads block on this */
	M_thread_pipeline_step_t       *steps;     /*!< Steps/Threads */
	size_t                          num_steps; /*!< Count of steps/threads */
	M_thread_pipeline_flags_t       flags;     /*!< Flags passed on call to create */
	M_thread_pipeline_taskfinish_cb finish_cb; /*!< Callback to call when task is completed */
	M_list_t                       *queue;     /*!< List of queued requests */
	size_t                          cnt;       /*!< Count of queued tasks, including ones being processed
	                                                (might be different than M_list_count(queue) due to a deep pipeline) */
	M_thread_cond_t                *cond;      /*!< Wake any callers waiting on M_thread_pipeline_wait() */
};


void M_thread_pipeline_wait(M_thread_pipeline_t *pipeline, size_t queue_limit)
{
	if (pipeline == NULL)
		return;
	while (1) {
		M_thread_mutex_lock(pipeline->lock);

		if (pipeline->cnt <= queue_limit)
			break;

		M_thread_cond_wait(pipeline->cond, pipeline->lock);
	}
	M_thread_mutex_unlock(pipeline->lock);
}

size_t M_thread_pipeline_queue_count(M_thread_pipeline_t *pipeline)
{
	size_t cnt;
	if (pipeline == NULL)
		return 0;

	M_thread_mutex_lock(pipeline->lock);
	cnt = pipeline->cnt;
	M_thread_mutex_unlock(pipeline->lock);

	return cnt;
}

M_bool M_thread_pipeline_status(M_thread_pipeline_t *pipeline)
{
	M_bool status;
	if (pipeline == NULL)
		return M_FALSE;

	M_thread_mutex_lock(pipeline->lock);
	status = pipeline->status;
	M_thread_mutex_unlock(pipeline->lock);

	return status;
}

void M_thread_pipeline_destroy(M_thread_pipeline_t *pipeline)
{
	size_t i;

	if (pipeline == NULL)
		return;

	M_thread_mutex_lock(pipeline->lock);
	pipeline->status = M_FALSE;

	/* Wake all threads to cleanup */
	for (i=0; i<pipeline->num_steps; i++) {
		M_thread_cond_signal(pipeline->steps[i].cond);
	}
	M_thread_cond_broadcast(pipeline->cond);

	M_thread_mutex_unlock(pipeline->lock);

	M_thread_pipeline_wait(pipeline, 0);

	/* Make sure all threads are signalled to shutdown */
	M_thread_mutex_lock(pipeline->lock);
	for (i=0; i<pipeline->num_steps; i++) {
		void *rv = NULL;
		M_thread_cond_signal(pipeline->steps[i].cond);
		M_thread_mutex_unlock(pipeline->lock);
		if (pipeline->steps[i].threadid)
			M_thread_join(pipeline->steps[i].threadid, &rv);
		M_thread_mutex_lock(pipeline->lock);
		pipeline->steps[i].threadid = 0;

		/* Tear down the thread conditional */
		M_thread_cond_destroy(pipeline->steps[i].cond);
	}

	M_thread_mutex_unlock(pipeline->lock);

	/* Kill all memory */
	M_free(pipeline->steps);
	M_thread_mutex_destroy(pipeline->lock);
	M_thread_cond_destroy(pipeline->cond);
	M_list_destroy(pipeline->queue, M_TRUE);
	M_free(pipeline);
}

static M_thread_pipeline_task_t *pipeline_fetch_task(M_thread_pipeline_step_t *step)
{
	M_thread_pipeline_t      *pipeline = step->parent;
	M_thread_pipeline_task_t *task     = NULL;
	size_t                    idx      = step->idx;

	if (step->task) {
		task       = step->task;
		step->task = NULL;
	} else if (idx == 0) {
		/* We need to fetch a task from the main queue */
		task = M_list_take_first(pipeline->queue);
	}

	if (task != NULL) {
		/* If we took a task, notify prior thread we have a slot */
		if (step->idx != 0) {
			M_thread_cond_signal(pipeline->steps[idx - 1].cond);
		}
	} else {
		M_thread_cond_wait(step->cond, pipeline->lock);
	}

	return task;
}

static void pipeline_finish_task(M_thread_pipeline_t *pipeline, M_thread_pipeline_task_t *task, M_thread_pipeline_result_t rv)
{
	pipeline->finish_cb(task, rv);
	pipeline->cnt--;
	/* Notify any waiters */
	M_thread_cond_broadcast(pipeline->cond);
}

static void pipeline_finish_step(M_thread_pipeline_step_t *step, M_thread_pipeline_task_t *task, M_thread_pipeline_result_t rv)
{
	M_thread_pipeline_t *pipeline = step->parent;
	size_t               idx      = step->idx;

	/* Abort all tasks on failure if configured to */
	if (rv == M_THREAD_PIPELINE_RESULT_FAIL && !(pipeline->flags & M_THREAD_PIPELINE_FLAG_NOABORT)) {
		size_t i;
		pipeline->status = M_FALSE;

		/* Wake all threads to cleanup */
		for (i=0; i<pipeline->num_steps; i++) {
			M_thread_cond_signal(pipeline->steps[i].cond);
		}
		M_thread_cond_broadcast(pipeline->cond);
	}

	/* If system went down, abort, don't pass on */
	if (!pipeline->status && idx != pipeline->num_steps-1)
		rv = M_THREAD_PIPELINE_RESULT_ABORT;

	/* Complete task if last step, or failure */
	if (rv != M_THREAD_PIPELINE_RESULT_SUCCESS || idx == pipeline->num_steps-1) {
		pipeline_finish_task(pipeline, task, rv);
		return;
	}

	/* send to next step in pipeline */
	while (pipeline->steps[idx+1].task != NULL) {
		/* Wait to be notified */
		M_thread_cond_wait(step->cond, pipeline->lock);
	}

	/* Make sure system is still online, if not, don't enqueue, abort! */
	if (!pipeline->status) {
		pipeline_finish_task(pipeline, task, M_THREAD_PIPELINE_RESULT_FAIL);
		return;
	}

	/* Queue to next step and signal */
	pipeline->steps[idx+1].task = task;
	M_thread_cond_signal(pipeline->steps[idx+1].cond);
}

static void *pipeline_thread_cb(void *arg)
{
	M_thread_pipeline_step_t *step = arg;
	M_thread_pipeline_task_t *task = NULL;

	M_thread_mutex_lock(step->parent->lock);

	while (1) {
		M_bool rv;

		if (!step->parent->status)
			break;

		task = pipeline_fetch_task(step);
		if (task == NULL)
			continue;

		M_thread_mutex_unlock(step->parent->lock);

		rv = step->task_cb(task);

		M_thread_mutex_lock(step->parent->lock);

		pipeline_finish_step(step, task, rv?M_THREAD_PIPELINE_RESULT_SUCCESS:M_THREAD_PIPELINE_RESULT_FAIL);
	}

	/* Clean up any assigned tasks */
	if (step->task) {
		pipeline_finish_task(step->parent, step->task, M_THREAD_PIPELINE_RESULT_ABORT);
		step->task = NULL;
	}

	/* Dequeue and abort any queued tasks */
	if (step->idx == 0) {
		while ((task=M_list_take_first(step->parent->queue)) != NULL) {
			pipeline_finish_task(step->parent, task, M_THREAD_PIPELINE_RESULT_ABORT);
		}
	}

	M_thread_mutex_unlock(step->parent->lock);
	return NULL;
}


M_thread_pipeline_t *M_thread_pipeline_create(const M_thread_pipeline_steps_t *steps, int flags, M_thread_pipeline_taskfinish_cb finish_cb)
{
	M_thread_pipeline_t *pipeline = NULL;
	size_t               i;
	M_thread_attr_t     *attr     = NULL;

	if (steps == NULL || M_list_len(steps->steps) == 0 || finish_cb == NULL)
		return NULL;

	pipeline            = M_malloc_zero(sizeof(*pipeline));
	pipeline->status    = M_TRUE;
	pipeline->lock      = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	pipeline->flags     = (M_thread_pipeline_flags_t)flags;
	pipeline->num_steps = M_list_len(steps->steps);
	pipeline->steps     = M_malloc_zero(sizeof(*(pipeline->steps)) * pipeline->num_steps);
	pipeline->finish_cb = finish_cb;
	pipeline->queue     = M_list_create(NULL, M_LIST_NONE);
	pipeline->cond      = M_thread_cond_create(M_THREAD_CONDATTR_NONE);

	M_thread_mutex_lock(pipeline->lock);
	attr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(attr, M_TRUE);

	for (i=0; i<pipeline->num_steps; i++) {
		pipeline->steps[i].parent   = pipeline;
		pipeline->steps[i].idx      = i;
		pipeline->steps[i].cond     = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
		pipeline->steps[i].task_cb  = M_list_at(steps->steps, i);
		pipeline->steps[i].threadid = M_thread_create(attr, pipeline_thread_cb, &pipeline->steps[i]);
		if (pipeline->steps[i].threadid == 0)
			goto fail;
	}

	M_thread_attr_destroy(attr);
	M_thread_mutex_unlock(pipeline->lock);

	return pipeline;

fail:
	M_thread_mutex_unlock(pipeline->lock);
	M_thread_attr_destroy(attr);
	M_thread_pipeline_destroy(pipeline);
	return NULL;
}


M_bool M_thread_pipeline_task_insert(M_thread_pipeline_t *pipeline, M_thread_pipeline_task_t *task)
{
	M_bool rv = M_TRUE;
	if (pipeline == NULL || task == NULL)
		return M_FALSE;

	M_thread_mutex_lock(pipeline->lock);
	if (!pipeline->status) {
		rv = M_FALSE;
		goto fail;
	}

	M_list_insert(pipeline->queue, task);
	M_thread_cond_signal(pipeline->steps[0].cond);

fail:
	M_thread_mutex_unlock(pipeline->lock);
	return rv;
}

