#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef struct event_data {
	M_event_trigger_t *trigger;
	M_uint64           delay;
	M_bool             use_trigger;
	size_t             events;
} event_data_t;

#define DEBUG 1

#if defined(DEBUG) && DEBUG
#include <stdarg.h>

static void event_debug(const char *fmt, ...)
{
	va_list     ap;
	char        buf[1024];
	M_timeval_t tv;

	M_time_gettimeofday(&tv);
	va_start(ap, fmt);
	M_snprintf(buf, sizeof(buf), "%lld.%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
	M_vprintf(buf, ap);
	va_end(ap);
	fflush(stdout);
}
#else
static void event_debug(const char *fmt, ...)
{
	(void)fmt;
}
#endif

static void timer_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	event_data_t  *evdata  = data;
	(void)event;
	(void)type;
	(void)comm;
	event_debug("timer triggered");
	if (evdata->use_trigger) {
		M_event_trigger_signal(evdata->trigger);
	}
	if (evdata->events == 0 && evdata->delay) {
		event_debug("event emulate long event handler, delay %llu ms", evdata->delay);
		M_thread_sleep(evdata->delay * 1000);
	}
	if (!evdata->use_trigger)
		evdata->events++;
}


static void trigger_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	event_data_t *evdata = data;
	(void)event;
	(void)type;
	(void)comm;
	(void)data;
	evdata->events++;
	event_debug("event triggered");
}


/* start_delay_ms       = how many milliseconds before first timer (0=immediate)
 * end_ms               = how many milliseconds until timer stops (0=run until done/forever)
 * interval_ms          = how many milliseconds between events
 * max_runtime_ms       = maximum runtime in ms
 * fire_cnt             = maximum number of times event will fire (0=unlimited)
 * mode                 = Timer mode M_EVENT_TIMER_MODE_MONOTONIC vs M_EVENT_TIMER_MODE_RELATIVE
 * use_trigger          = Whether or not to fire a trigger to use to keep the event count rather the timer itself
 * first_event_delay_ms = How many milliseconds to delay on first timer event (simulate extended processing time)
 * Returns number of events.
 */
static size_t event_timer_test(M_uint64 start_delay_ms, M_uint64 end_ms, M_uint64 interval_ms, M_uint64 max_runtime_ms,
                               size_t fire_cnt, M_event_timer_mode_t mode, M_bool use_trigger, M_uint64 first_event_delay_ms)
{
	M_event_t         *event = M_event_create(M_EVENT_FLAG_EXITONEMPTY|M_EVENT_FLAG_NOWAKE);
	event_data_t      *data  = M_malloc_zero(sizeof(*data));
	M_event_timer_t   *timer;
	size_t             events;

	event_debug("start_delay_ms=%llu, end_ms=%llu, interval_ms=%llu, max_runtime_ms=%llu, fire_cnt=%zu, mode=%s, use_trigger=%s, first_event_delay_ms=%llu",
	            start_delay_ms, end_ms, interval_ms, max_runtime_ms, fire_cnt,
	            mode == M_EVENT_TIMER_MODE_RELATIVE?"RELATIVE":"MONOTONIC", use_trigger?"yes":"no", first_event_delay_ms);

	data->use_trigger = use_trigger;
	if (use_trigger)
		data->trigger = M_event_trigger_add(event, trigger_cb, data);
	timer         = M_event_timer_add(event, timer_cb, data);

	if (start_delay_ms) {
		M_timeval_t tv;
		M_time_gettimeofday(&tv);
		tv.tv_usec += start_delay_ms * 1000;
		tv.tv_sec  += tv.tv_usec / 1000000;
		tv.tv_usec %= 1000000;
		M_event_timer_set_starttv(timer, &tv);
	}

	if (end_ms) {
		M_timeval_t tv;
		M_time_gettimeofday(&tv);
		tv.tv_usec += end_ms * 1000;
		tv.tv_sec  += tv.tv_usec / 1000000;
		tv.tv_usec %= 1000000;
		M_event_timer_set_endtv(timer, &tv);
	}

	data->delay = first_event_delay_ms;

	M_event_timer_set_mode(timer, mode);
	M_event_timer_set_firecount(timer, fire_cnt);
	M_event_timer_set_autoremove(timer, M_TRUE);
	if (!M_event_timer_start(timer, interval_ms))
		goto cleanup;

	event_debug("entering loop");
	M_event_loop(event, max_runtime_ms);

cleanup:
	events = data->events;
	M_free(data);
	M_event_destroy(event);
	M_library_cleanup();
	event_debug("exited (%zu events)", events);
	return events;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct timer_test {
	M_uint64             start_delay_ms;
	M_uint64             end_ms;
	M_uint64             interval_ms;
	M_uint64             max_runtime_ms;
	size_t               fire_cnt;
	M_event_timer_mode_t mode;
	M_bool               use_trigger;
	M_uint64             first_event_delay_ms;
	size_t               expected_events;
	size_t               tolerance;
};
struct timer_test timer_tests[] = {
	/* start ,  end, intvl,  max, cnt,                         mode, trigger?, delay, expected, tolerance */
	{       0, 1099,   100, 1200,   0, M_EVENT_TIMER_MODE_MONOTONIC,   M_TRUE,     0,       10, 0 },
	{       0, 1099,   100, 1200,   0, M_EVENT_TIMER_MODE_MONOTONIC,  M_FALSE,     0,       10, 0 },
	{       0, 1099,   100, 1200,   0,  M_EVENT_TIMER_MODE_RELATIVE,   M_TRUE,     0,       10, 0 },
	{       0, 1099,   100, 1200,   0,  M_EVENT_TIMER_MODE_RELATIVE,  M_FALSE,     0,       10, 0 },
	{       0, 1099,   100, 1200,   0, M_EVENT_TIMER_MODE_MONOTONIC,   M_TRUE,   200,       10, 0 },
	{       0, 1099,   100, 1200,   0, M_EVENT_TIMER_MODE_MONOTONIC,  M_FALSE,   200,       10, 0 },
	{       0, 1099,   100, 1200,   0,  M_EVENT_TIMER_MODE_RELATIVE,   M_TRUE,   200,        8, 0 },
	{       0, 1099,   100, 1200,   0,  M_EVENT_TIMER_MODE_RELATIVE,  M_FALSE,   200,        8, 0 },
	/* Interval is so short we had to add a tolerence as time isn't all that reliable */
	{     100,  225,    50,  500,   0, M_EVENT_TIMER_MODE_MONOTONIC,  M_FALSE,     0,        3, 1 },
	{     100,  225,    50,  500,   0,  M_EVENT_TIMER_MODE_RELATIVE,  M_FALSE,     0,        3, 1 },
	{       0,    0,    50, 1200,  10, M_EVENT_TIMER_MODE_MONOTONIC,  M_FALSE,     0,       10, 1 },
	{       0,    0,    50, 1200,  10,  M_EVENT_TIMER_MODE_RELATIVE,  M_FALSE,     0,       10, 1 },
};

START_TEST(check_event_timer)
{
	size_t i;
	for (i=0; i<sizeof(timer_tests) / sizeof(*timer_tests); i++) {
		size_t events;
		events = event_timer_test(timer_tests[i].start_delay_ms, timer_tests[i].end_ms, timer_tests[i].interval_ms,
		                          timer_tests[i].max_runtime_ms, timer_tests[i].fire_cnt, timer_tests[i].mode,
		                          timer_tests[i].use_trigger, timer_tests[i].first_event_delay_ms);
		ck_assert_msg(events >= timer_tests[i].expected_events - timer_tests[i].tolerance && events <= timer_tests[i].expected_events + timer_tests[i].tolerance, "test %d: expected %d events, got %d", (int)i, (int)timer_tests[i].expected_events, (int)events);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *event_timer_suite(void)
{
	Suite *suite;
	TCase *tc_event_timer;

	suite = suite_create("event_timer");

	tc_event_timer = tcase_create("event_timer");
	tcase_add_test(tc_event_timer, check_event_timer);
	tcase_set_timeout(tc_event_timer, 60);
	suite_add_tcase(suite, tc_event_timer);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(event_timer_suite());
	srunner_set_log(sr, "check_event_timer.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
