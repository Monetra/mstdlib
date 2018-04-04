/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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
#include "mstdlib/mstdlib_io.h"
#include "m_event_int.h"

struct M_event_timer {
	/* Settings */
	M_timeval_t          end_tv;
	M_timeval_t          start_tv;
	M_uint64             interval_ms;
	size_t               fire_cnt;
	M_bool               autodestroy;
	M_bool               delay_destroy; /* Set due to a self-destroy during execution. 
	                                     * Cannot overload autodestroy as another thread calling _start()
	                                     * can cause odd behavior. */
	M_event_timer_mode_t mode;
	M_event_callback_t   callback;
	void                *cb_data;

	/* State data */
	M_event_t           *event;
	M_bool               started;
	size_t               cnt;
	M_timeval_t          next_run;     /* Next run, based on M_time_elapse_start() */
	M_timeval_t          last_run;     /* Last run time, to prevent starvation of other tasks */
	M_bool               executing;    /* If we are currently executing this timer's callback -- make sure we don't really destroy ourselves */
};

/* Max interval is 30 days (in milliseconds).  This is due to Windows using a 32bit timer
 * which really has a max value of 49 or so days */
#define INTERVAL_MAX ((M_int64)30 * (M_int64)86400 * (M_int64)1000)

static int M_event_timer_compar_cb(const void *arg1, const void *arg2, void *thunk)
{
	const M_event_timer_t *t1     = *((M_event_timer_t * const *)arg1);
	const M_event_timer_t *t2     = *((M_event_timer_t * const *)arg2);
	M_int64                tdiff;

	(void)thunk;

	/* Stopped timers are considered equal */
	if (!t1->started && !t2->started)
		return 0;

	if (!t1->started && t2->started)
		return 1;

	if (t1->started && !t2->started)
		return -1;

	/* Timeval diff is start -> end, so we need to invert the params */
	tdiff = M_time_timeval_diff(&t2->next_run, &t1->next_run);
	if (tdiff < 0)
		return -1;
	if (tdiff > 0)
		return 1;

	/* They're equal, so we need to instead compare the last run timestamps, and
	 * the one run the longest time ago should be scheduled to run first */
	tdiff = M_time_timeval_diff(&t2->last_run, &t1->last_run);
	if (tdiff < 0)
		return -1;
	if (tdiff > 0)
		return 1;

	return 0;
}


static void M_event_timer_enqueue(M_event_timer_t *timer)
{
	M_event_t *event = timer->event;

	/* NOTE: This isn't part of the M_event_t initialization as not all implementations
	 *       need timers, so detect that it wasn't initialized and initialize when
	 *       needed */
	if (event->u.loop.timers == NULL) {
		event->u.loop.timers = M_queue_create(M_event_timer_compar_cb, M_free);
	}

	M_queue_insert(event->u.loop.timers, timer);
}


static void M_event_timer_dequeue(M_event_timer_t *timer)
{
	M_event_t *event = timer->event;

	M_queue_take(event->u.loop.timers, timer);
}


M_event_timer_t *M_event_timer_add(M_event_t *event, M_event_callback_t callback, void *cb_data)
{
	M_event_timer_t *timer;

	if (event == NULL || callback == NULL)
		return NULL;

	/* Balance if pool provided */
	event           = M_event_distribute(event);

	timer           = M_malloc_zero(sizeof(*timer));
	timer->mode     = M_EVENT_TIMER_MODE_RELATIVE;
	timer->callback = callback;
	timer->cb_data  = cb_data;
	timer->event    = event;

	M_event_lock(timer->event);
	M_event_timer_enqueue(timer);
	M_event_unlock(timer->event);
//M_printf("%s(): timer %p created\n", __FUNCTION__, timer); fflush(stdout);

	return timer;
}


static void M_event_timer_remove_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg);

M_bool M_event_timer_remove(M_event_timer_t *timer)
{
	M_event_t *event;

	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	/* Stop the timer so it won't execute before it's destoryed.
 	 * In case destory needs to be queued. */

	event = timer->event;

	M_event_lock(event);
	timer->started = M_FALSE;

	/* Queue a destroy task to run for this in the owning event loop */
	if (event->u.loop.threadid != 0 && event->u.loop.threadid != M_thread_self()) {
		M_event_queue_task(event, M_event_timer_remove_cb, timer);
		M_event_unlock(event);
		return M_TRUE; /* queued to remove */
	}

	if (timer->executing) {
		timer->delay_destroy = M_TRUE;
		M_event_unlock(event);
		return M_TRUE;
	}
	M_event_timer_dequeue(timer);
//M_printf("%s(): timer %p destroyed\n", __FUNCTION__, timer); fflush(stdout);

	M_free(timer);
	M_event_unlock(event);

	return M_TRUE;
}


static void M_event_timer_remove_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_event_timer_t *timer = cb_arg;
	(void)event;
	(void)type;
	(void)io;

	/* Destroyed out from under us */
	if (!M_queue_exists(event->u.loop.timers, timer))
		return;

	M_event_timer_remove(timer);
}


static M_bool M_event_timer_tvset(M_timeval_t *tv)
{
	if (tv->tv_usec != 0 || tv->tv_sec != 0)
		return M_TRUE;
	return M_FALSE;
}


static M_bool M_event_timer_schedule(M_event_timer_t *timer)
{
	M_uint64 add_ms = 0;

	if (timer->interval_ms == 0 && timer->fire_cnt != 1)
		return M_FALSE;

	/* Start next run timer based on current tick counter if not already set or using relative
	 * timers */
	if (!M_event_timer_tvset(&timer->next_run) || timer->mode == M_EVENT_TIMER_MODE_RELATIVE) {
		M_time_elapsed_start(&timer->next_run);
	}

	/* If a start offset is set, go ahead and figure out how far in the future it is */
	if (M_event_timer_tvset(&timer->start_tv)) {
		M_int64     time_ms;
		M_timeval_t tv;

		M_time_gettimeofday(&tv);
		time_ms = M_time_timeval_diff(&tv, &timer->start_tv);
		if (time_ms < 0)
			time_ms = 0;
		/* 30 day max */
		if (time_ms > INTERVAL_MAX) {
			return M_FALSE;
		}
		add_ms = (M_uint64)time_ms;

		/* Clear start_tv so next iteration doesn't re-use it */
		M_mem_set(&timer->start_tv, 0, sizeof(timer->start_tv));
	} else {
		add_ms = timer->interval_ms;
	}

	timer->next_run.tv_usec += (M_suseconds_t)(add_ms * 1000);
	/* Normalize */
	timer->next_run.tv_sec  += timer->next_run.tv_usec / 1000000;
	timer->next_run.tv_usec %= 1000000;

	return M_TRUE;
}


M_bool M_event_timer_start(M_event_timer_t *timer, M_uint64 interval_ms)
{
	M_bool rv;

	if (timer == NULL || timer->event == NULL || interval_ms > INTERVAL_MAX) {
		return M_FALSE;
	}

	M_event_lock(timer->event);
	if (!timer->executing) /* Recursion! */
		M_event_timer_dequeue(timer);
	timer->interval_ms = interval_ms;
	timer->cnt         = 0;
	M_mem_set(&timer->next_run, 0, sizeof(timer->next_run));
	rv = M_event_timer_schedule(timer);
	if (rv) {
//M_printf("%s(): timer %p started for %llu ms\n", __FUNCTION__, timer, interval_ms); fflush(stdout);
		timer->started = M_TRUE;
		if (!timer->executing) { /* Recursion! */
			M_event_timer_enqueue(timer);
			M_event_wake(timer->event);
		}
	} else {
		if (!timer->executing) /* Recursion! */
			M_event_timer_enqueue(timer);
	}
	M_event_unlock(timer->event);

	return rv;
}


static M_bool M_event_timer_stop_int(M_event_timer_t *timer, M_bool allow_autodestroy)
{
	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	M_event_lock(timer->event);
	if (!timer->executing) /* Recursion! */
		M_event_timer_dequeue(timer);
	timer->started = M_FALSE;
	if (!timer->executing) /* Recursion! */
		M_event_timer_enqueue(timer);
	M_event_unlock(timer->event);
//M_printf("%s(): timer %p stopped\n", __FUNCTION__, timer); fflush(stdout);

	if (allow_autodestroy && timer->autodestroy)
		M_event_timer_remove(timer);

	return M_TRUE;
}


M_bool M_event_timer_stop(M_event_timer_t *timer)
{
	return M_event_timer_stop_int(timer, M_TRUE);
}


M_bool M_event_timer_reset(M_event_timer_t *timer, M_uint64 interval_ms)
{
	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	if (timer->started) {
		if (!M_event_timer_stop_int(timer, M_FALSE))
			return M_FALSE;
	}

	return M_event_timer_start(timer, (interval_ms == 0)?timer->interval_ms:interval_ms);
}


M_bool M_event_timer_set_starttv(M_event_timer_t *timer, M_timeval_t *start_tv)
{
	if (timer == NULL || timer->event == NULL || (start_tv != NULL && !M_event_timer_tvset(start_tv)))
		return M_FALSE;

	if (start_tv == NULL && !M_event_timer_tvset(&timer->start_tv))
		return M_FALSE;

	if (start_tv == NULL) {
		M_mem_set(&timer->start_tv, 0, sizeof(timer->start_tv));
	} else {
		M_mem_copy(&timer->start_tv, start_tv, sizeof(timer->start_tv));
	}
	return M_TRUE;
}


M_bool M_event_timer_set_endtv(M_event_timer_t *timer, M_timeval_t *end_tv)
{
	if (timer == NULL || timer->event == NULL || (end_tv != NULL && !M_event_timer_tvset(end_tv)))
		return M_FALSE;

	if (end_tv == NULL && !M_event_timer_tvset(&timer->end_tv))
		return M_FALSE;

	if (end_tv == NULL) {
		M_mem_set(&timer->end_tv, 0, sizeof(timer->end_tv));
	} else {
		M_mem_copy(&timer->end_tv, end_tv, sizeof(timer->end_tv));
	}
	return M_TRUE;
}


M_bool M_event_timer_set_firecount(M_event_timer_t *timer, size_t cnt)
{
	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	timer->fire_cnt = cnt;
	return M_TRUE;
}


M_bool M_event_timer_set_autoremove(M_event_timer_t *timer, M_bool enabled)
{
	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	timer->autodestroy = enabled;
	return M_TRUE;
}


M_bool M_event_timer_set_mode(M_event_timer_t *timer, M_event_timer_mode_t mode)
{
	if (timer == NULL || timer->event == NULL)
		return M_FALSE;

	timer->mode = mode;
	return M_TRUE;
}


M_uint64 M_event_timer_get_remaining_ms(M_event_timer_t *timer)
{
	M_int64     remaining_ms;
	M_timeval_t curr;

	if (timer == NULL || timer->event == NULL || !timer->started)
		return 0;

	M_time_elapsed_start(&curr);
	remaining_ms = M_time_timeval_diff(&curr, &timer->next_run);
	if (remaining_ms < 0)
		remaining_ms = 0;

	return (M_uint64)remaining_ms;
}


M_bool M_event_timer_get_status(M_event_timer_t *timer)
{
	if (timer == NULL || timer->event == NULL || !timer->started)
		return M_FALSE;
	return M_TRUE;
}


M_event_timer_t *M_event_timer_oneshot(M_event_t *event, M_uint64 interval_ms, M_bool autodestroy, M_event_callback_t callback, void *cb_data)
{
	M_event_timer_t *timer;

	timer = M_event_timer_add(event, callback, cb_data);
	if (timer == NULL)
		return NULL;

	M_event_timer_set_firecount(timer, 1);
	M_event_timer_set_autoremove(timer, autodestroy);
	M_event_timer_start(timer, interval_ms);
	return timer;
}


/*! Returns time in ms for the minimum timer trigger value, or M_TIMEOUT_INF if there
 *  are no timers.  A lock on M_event_t should already be held before calling this. */
M_uint64 M_event_timer_minimum_ms(M_event_t *event)
{
	M_event_timer_t   *timer;
	M_int64            time_ms;
	M_timeval_t        curr;

	timer = M_queue_first(event->u.loop.timers);
	if (timer == NULL) {
		return M_TIMEOUT_INF;
	}

	if (!timer->started) {
		return M_TIMEOUT_INF;
	}

	/* Elapsed_start just pulls the current counter */
	M_time_elapsed_start(&curr);

	time_ms = M_time_timeval_diff(&curr, &timer->next_run);
	if (time_ms < 0)
		time_ms = 0;
	return (M_uint64)time_ms;
}


/* NOTE: event handle must be locked when this function is called */
void M_event_timer_process(M_event_t *event)
{
	M_event_timer_t   *timer;
	M_event_timer_t   *last_timer = NULL;
	M_timeval_t        curr;
	size_t             cnt = 0;

	M_time_elapsed_start(&curr);

	/* Iterate across timers until either we run out or hit one that isn't yet triggered */
	while ((timer = M_queue_first(event->u.loop.timers)) != NULL && timer != last_timer && timer->started && M_time_timeval_diff(&timer->next_run, &curr) >= 0) {
//M_printf("%s(): processing timer %p\n", __FUNCTION__, timer); fflush(stdout);
		last_timer = timer;
		/* We always dequeue the timer from the list as we may add it back in if it is to be rescheduled */
		M_event_timer_dequeue(timer);

		/* See if timer expired, if so mark it as such */
		if (M_event_timer_tvset(&timer->end_tv)) {
			M_timeval_t tv;
			M_time_gettimeofday(&tv);
			if (M_time_timeval_diff(&tv, &timer->end_tv) <= 0) {
				timer->started = M_FALSE;
			}
		}

		/* Trigger callback */
		if (timer->started) {
			timer->cnt++;
			timer->executing = M_TRUE;

			/* Unlock event lock since the callback may take some time */
			M_event_unlock(event);

			timer->callback(event, M_EVENT_TYPE_OTHER, NULL, timer->cb_data);

			/* Relock to possibly re-queue or loop */
			M_event_lock(event);

			timer->executing = M_FALSE;
			cnt++;
		}

		/* Determine if timer should be stopped */
		if (timer->fire_cnt != 0 && timer->cnt >= timer->fire_cnt) {
//M_printf("%s(): stopping timer %p-- max fire count\n", __FUNCTION__, timer); fflush(stdout);
			timer->started = M_FALSE;
		}

		/* If autodestroy and timer went to stopped mode, kill it */
		if (!timer->started && timer->autodestroy) {
			M_free(timer);
			continue;
		}

		/* If self-deleted during the callback, cleanup now */
		if (timer->delay_destroy) {
			M_free(timer);
			continue;
		}

		/* Record the last run, that way it can't re-insert itself in front of other
		 * tasks ready to run immediately, thus starving them */
		M_time_elapsed_start(&timer->last_run);

		/* Reschedule */
		if (timer->started)
			M_event_timer_schedule(timer);

		/* re-enqueue */
		M_event_timer_enqueue(timer);

		/* Pull current time as we do not know how long this iteration took */
		M_time_elapsed_start(&curr);
	}

	event->u.loop.timer_cnt += cnt;
//M_printf("%s(): delivered %zu events\n", __FUNCTION__, cnt);
}
