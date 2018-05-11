#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_thread_mutex_t *mutex;
	M_event_t        *el1;
	M_event_t        *el2;
	M_event_timer_t  *timer1;
	M_event_timer_t  *timer2;
	M_list_t         *timers;
	size_t            count;
	size_t            num;
} cb_data_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void el_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;

	(void)el;
	(void)etype;
	(void)io;

	data->count++;

	/* Sleep long enough that we know all thread2 M_event_timer_start()'s have been called */
	M_thread_sleep(1000000);

	M_event_timer_remove(data->timer1);
	data->timer1 = NULL;
}

static void el_self_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;

	(void)el;
	(void)etype;
	(void)io;

	data->count++;

	if (data->count < data->num) {
		M_event_timer_start(data->timer1, 0);
	} else {
		M_event_timer_remove(data->timer1);
		M_event_done(data->el1);
	}
}

static void el_many_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;

	(void)el;
	(void)etype;
	(void)io;

	M_thread_mutex_lock(data->mutex);

	data->count++;
	if (data->count == data->num)
		M_event_done(data->el1);

	M_thread_mutex_unlock(data->mutex);
}

static void el_many_remove_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t       *data = thunk;
	M_event_timer_t *timer;

	(void)el;
	(void)etype;
	(void)io;

	M_thread_mutex_lock(data->mutex);

	data->count++;

	M_list_remove_at(data->timers, (size_t)M_rand_range(NULL, 0, M_list_len(data->timers)));
	M_list_remove_at(data->timers, (size_t)M_rand_range(NULL, 0, M_list_len(data->timers)));
	M_list_remove_at(data->timers, (size_t)M_rand_range(NULL, 0, M_list_len(data->timers)));
	M_list_remove_at(data->timers, (size_t)M_rand_range(NULL, 0, M_list_len(data->timers)));
	M_list_remove_at(data->timers, (size_t)M_rand_range(NULL, 0, M_list_len(data->timers)));

	if (M_list_len(data->timers) == 0) {
		M_event_done(data->el1);
	} else {
		timer = M_event_timer_oneshot(el, M_rand_range(NULL, 0, 500), M_FALSE, el_many_remove_cb, data);
		M_list_insert(data->timers, timer);
	}
M_printf("cnt=%zu, len=%zu\n", data->count, M_list_len(data->timers));
	M_thread_mutex_unlock(data->mutex);
}

static void el_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;
	size_t     i;

	(void)el;
	(void)etype;
	(void)io;

	/* Try to run start a bunch of times on the same event timer */
	for (i=0; i<data->num; i++) {
		M_event_timer_start(data->timer1, 0);
		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(15000);
	}

	M_event_done(data->el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(data->el1);
}

static void el_remove_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;
	size_t     i;
	(void)el;
	(void)etype;
	(void)io;

	/* Try to run start a bunch of times on the same event timer */
	for (i=0; i<data->num; i++) {
		M_event_timer_remove(data->timer1);
		data->timer1 = M_event_timer_add(data->el1, el_cb, data);
		M_event_timer_set_firecount(data->timer1, 1);
		M_event_timer_start(data->timer1, 0);

		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(15000);
	}

	M_event_done(data->el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(data->el1);
}

static void el_stop_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	cb_data_t *data = thunk;
	size_t     i;

	(void)el;
	(void)etype;
	(void)io;

	/* Try to run start a bunch of times on the same event timer */
	for (i=0; i<data->num; i++) {
		M_event_timer_stop(data->timer1);
		M_event_timer_start(data->timer1, 0);

		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(15000);
	}

	M_event_done(data->el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(data->el1);
}

static void *run_el2(void *arg)
{
	cb_data_t *data = arg;

	M_event_loop(data->el2, M_TIMEOUT_INF);
	M_event_destroy(data->el2);
	return NULL;
}

static void *run_el1(void *arg)
{
	cb_data_t *data = arg;

	M_event_loop(data->el1, M_TIMEOUT_INF);
	M_event_destroy(data->el1);
	return NULL;
}

static void *run_els(void *arg)
{
	cb_data_t *data = arg;

	M_event_loop(data->el1, M_TIMEOUT_INF);
	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_event_stacking_start)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;
	cb_data_t        data;

	M_mem_set(&data, 0, sizeof(data));

	data.num    = 25;
	data.el1    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer1 = M_event_timer_add(data.el1, el_cb, &data);
	M_event_timer_set_firecount(data.timer1, 1);

	data.el2    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer2 = M_event_timer_add(data.el2, el_cb2, &data);
	M_event_timer_set_firecount(data.timer2, 1);
	M_event_timer_start(data.timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, &data);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(data.count == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", data.count);
}
END_TEST

START_TEST(check_event_remove)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;
	cb_data_t        data;

	M_mem_set(&data, 0, sizeof(data));

	data.num    = 25;
	data.el1    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer1 = M_event_timer_add(data.el1, el_cb, &data);
	M_event_timer_set_firecount(data.timer1, 1);

	data.el2    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer2 = M_event_timer_add(data.el2, el_remove_cb2, &data);
	M_event_timer_set_firecount(data.timer2, 1);
	M_event_timer_start(data.timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, &data);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(data.count == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", data.count);
}
END_TEST

START_TEST(check_event_stop)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;
	cb_data_t        data;

	M_mem_set(&data, 0, sizeof(data));

	data.num    = 25;
	data.el1    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer1 = M_event_timer_add(data.el1, el_cb, &data);
	M_event_timer_set_firecount(data.timer1, 1);

	data.el2    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer2 = M_event_timer_add(data.el2, el_stop_cb2, &data);
	M_event_timer_set_firecount(data.timer2, 1);
	M_event_timer_start(data.timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, &data);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(data.count == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", data.count);
}
END_TEST

START_TEST(check_event_self)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	cb_data_t        data;

	M_mem_set(&data, 0, sizeof(data));

	data.num    = 5;
	data.el1    = M_event_create(M_EVENT_FLAG_NONE);
	data.timer1 = M_event_timer_add(data.el1, el_self_cb, &data);
	M_event_timer_set_firecount(data.timer1, 1);
	M_event_timer_start(data.timer1, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t1, NULL);

	ck_assert_msg(data.count == 5, "Timer calling itself fired unexpected number of times (%zu) expected (%zu)", data.count, data.num);
}
END_TEST

START_TEST(check_event_many)
{
	M_thread_attr_t *tattr;
	M_event_timer_t *timer;
	M_threadid_t     t1;
	size_t           i;
	cb_data_t        data;
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_event_timer_remove
	};

	M_mem_set(&data, 0, sizeof(data));

	data.num   = 100000;
	data.el1   = M_event_pool_create(0);
	data.mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	data.timers = M_list_create(&cbs, M_LIST_NONE);
	for (i=0; i<data.num; i++) {
		timer = M_event_timer_add(data.el1, el_many_cb, &data);
		M_event_timer_set_firecount(timer, 1);
		M_event_timer_start(timer, 0);
		M_list_insert(data.timers, timer);
	}

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_els, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t1, NULL);

	M_list_destroy(data.timers, M_TRUE);
	M_event_destroy(data.el1);
	M_thread_mutex_destroy(data.mutex);

	ck_assert_msg(data.count == data.num, "Many queued timers called event cb unexpected number of times (%zu) expected (%zu)", data.count, data.num);
}
END_TEST

START_TEST(check_event_many2)
{
	M_thread_attr_t *tattr;
	M_event_timer_t *timer;
	M_rand_t        *rander;
	M_threadid_t     t1;
	size_t           i;
	cb_data_t        data;
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_event_timer_remove
	};

	M_mem_set(&data, 0, sizeof(data));

	data.num   = 100000;
	data.el1   = M_event_pool_create(0);
	data.mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	data.timers = M_list_create(&cbs, M_LIST_NONE);
	rander      = M_rand_create(0);
	for (i=0; i<data.num; i++) {
		timer = M_event_timer_oneshot(data.el1, M_rand_range(rander, 0, 50000), M_FALSE, el_many_cb, &data);
		M_list_insert(data.timers, timer);
	}
	M_rand_destroy(rander);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_els, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t1, NULL);

	M_list_destroy(data.timers, M_TRUE);
	M_event_destroy(data.el1);
	M_thread_mutex_destroy(data.mutex);

	ck_assert_msg(data.count == data.num, "Many queued timers called event cb unexpected number of times (%zu) expected (%zu)", data.count, data.num);
}
END_TEST

START_TEST(check_event_many_remove)
{
	M_thread_attr_t *tattr;
	M_event_timer_t *timer;
	M_rand_t        *rander;
	M_threadid_t     t1;
	size_t           i;
	cb_data_t        data;
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_event_timer_remove
	};

	M_mem_set(&data, 0, sizeof(data));

	data.num   = 25000; /* Need a low number for slow travis build systems */
	data.el1   = M_event_pool_create(0);
	data.mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	data.timers = M_list_create(&cbs, M_LIST_NONE);
	rander      = M_rand_create(0);
	for (i=0; i<data.num; i++) {
		timer = M_event_timer_oneshot(data.el1, M_rand_range(rander, 0, 50000), M_FALSE, el_many_remove_cb, &data);
		M_list_insert(data.timers, timer);
	}
	M_rand_destroy(rander);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_els, &data);
	M_thread_attr_destroy(tattr);

	M_thread_join(t1, NULL);

	M_list_destroy(data.timers, M_TRUE);
	M_event_destroy(data.el1);
	M_thread_mutex_destroy(data.mutex);

	/* Don't care how many times it's called we only care that we don't time out because everything
 	 * should have been removed or run and the event cb stopped. */
	ck_assert_msg(data.count > 1, "Many queued timers removed and added called too few times");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Checks the following:
 * stack:
 * 1. Multiple calls to starting a timer with a fire count of 1 will not queue
 *    additional calls enqueue an event. (for loop in el_cb2).
 * 2. A timer be enqueued while it's within the event callback that it triggered.
 *    el_cb2 should cause more than 1 event to fire because of thread timing.
 *    el_cb should be running while M_event_timer_start is called to enqueue
 *    it to run again (legitimate). An M_event_timer_remove should remove all
 *    pending events (in el_cb).
 * 3. Timers can be manipulated across multiple threads.
 *
 * remove:
 * 1. Removing a timer will prevent it from running if it's queued.
 *
 * stop:
 * 1. Stopping a timer will prevent it from running if it's queued.
 */
static Suite *event_interactions_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("event_interactions");

	tc = tcase_create("event_stacking_start");
	tcase_add_test(tc, check_event_stacking_start);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_remove");
	tcase_add_test(tc, check_event_remove);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_stop");
	tcase_add_test(tc, check_event_stop);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_self");
	tcase_add_test(tc, check_event_self);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_many");
	tcase_add_test(tc, check_event_many);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_many2");
	tcase_add_test(tc, check_event_many2);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("event_many_remove");
	tcase_add_test(tc, check_event_many_remove);
	tcase_set_timeout(tc, 90);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(event_interactions_suite());
	srunner_set_log(sr, "check_event_interactions.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
