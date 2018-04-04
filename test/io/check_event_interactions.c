#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_event_t       *el1    = NULL;
static M_event_t       *el2    = NULL;
static M_event_timer_t *timer1 = NULL;
static M_event_timer_t *timer2 = NULL;
static size_t           count_stack  = 0;
static size_t           count_remove = 0;
static size_t           count_stop   = 0;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void el_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	(void)thunk;

	/* Try to run start a bunch of times on the same event timer */
	for (size_t i=0; i<25; i++) {
		M_event_timer_start(timer1, 0);
		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(15000);
	}

	M_event_done(el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(el1);
}

static void el_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	size_t *count = thunk;

	(void)el;
	(void)etype;
	(void)io;

	(*count)++;

	/* Sleep long enough that we know all thread2 M_event_timer_start()'s have been called */
	M_thread_sleep(500000);

	M_event_timer_remove(timer1);
	timer1 = NULL;
}

static void el_remove_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	(void)thunk;

	/* Try to run start a bunch of times on the same event timer */
	for (size_t i=0; i<25; i++) {
		M_event_timer_remove(timer1);
		timer1 = M_event_timer_add(el1, el_cb, &count_remove);
		M_event_timer_set_firecount(timer1, 1);
		M_event_timer_start(timer1, 0);

		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(1500);
	}

	M_event_done(el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(el1);
}

static void el_stop_cb2(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	(void)el;
	(void)etype;
	(void)io;
	(void)thunk;

	/* Try to run start a bunch of times on the same event timer */
	for (size_t i=0; i<25; i++) {
		M_event_timer_stop(timer1);
		M_event_timer_start(timer1, 0);

		/* Sleep enough to yield execution for each in case more timers go off */
		M_thread_sleep(1500);
	}

	M_event_done(el2);
	/* Sleep long enough that we know the thread1 callback is complete */
	M_thread_sleep(2000000);
	M_event_done(el1);
}

static void *run_el2(void *arg)
{
	(void)arg;

	M_event_loop(el2, M_TIMEOUT_INF);
	M_event_destroy(el2);
	return NULL;
}

static void *run_el1(void *arg)
{
	(void)arg;

	M_event_loop(el1, M_TIMEOUT_INF);
	M_event_destroy(el1);
	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_event_stacking_start)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;

	el1    = M_event_create(M_EVENT_FLAG_NONE);
	timer1 = M_event_timer_add(el1, el_cb, &count_stack);
	M_event_timer_set_firecount(timer1, 1);

	el2    = M_event_create(M_EVENT_FLAG_NONE);
	timer2 = M_event_timer_add(el2, el_cb2, NULL);
	M_event_timer_set_firecount(timer2, 1);
	M_event_timer_start(timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, NULL);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, NULL);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(count_stack == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", count_stack);
}
END_TEST

START_TEST(check_event_remove)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;

	el1    = M_event_create(M_EVENT_FLAG_NONE);
	timer1 = M_event_timer_add(el1, el_cb, &count_remove);
	M_event_timer_set_firecount(timer1, 1);

	el2    = M_event_create(M_EVENT_FLAG_NONE);
	timer2 = M_event_timer_add(el2, el_remove_cb2, NULL);
	M_event_timer_set_firecount(timer2, 1);
	M_event_timer_start(timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, NULL);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, NULL);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(count_remove == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", count_remove);
}
END_TEST

START_TEST(check_event_stop)
{
	M_thread_attr_t *tattr;
	M_threadid_t     t1;
	M_threadid_t     t2;

	el1    = M_event_create(M_EVENT_FLAG_NONE);
	timer1 = M_event_timer_add(el1, el_cb, &count_stop);
	M_event_timer_set_firecount(timer1, 1);

	el2    = M_event_create(M_EVENT_FLAG_NONE);
	timer2 = M_event_timer_add(el2, el_stop_cb2, NULL);
	M_event_timer_set_firecount(timer2, 1);
	M_event_timer_start(timer2, 0);

	tattr = M_thread_attr_create();
	M_thread_attr_set_create_joinable(tattr, M_TRUE);
	t1 = M_thread_create(tattr, run_el1, NULL);
	/* Give up time slice to make sure thread 1 is fully initialized */
	M_thread_sleep(100000);

	t2 = M_thread_create(tattr, run_el2, NULL);
	M_thread_attr_destroy(tattr);

	M_thread_join(t2, NULL);
	M_thread_join(t1, NULL);

	ck_assert_msg(count_stop == 1, "Timer started by different thread fired unexpected number of times (%zu) expected (1)", count_stop);
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
