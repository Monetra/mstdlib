#include "m_config.h"
#include <stdlib.h>
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 active_connections;
M_uint64 expected_connections;
M_uint64 connection_count;

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
	M_snprintf(buf, sizeof(buf), "%"PRId64".%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
	M_vprintf(buf, ap);
	va_end(ap);
}
#else
static void event_debug(const char *fmt, ...)
{
	(void)fmt;
}
#endif


static const char *event_type_str(M_event_type_t type)
{
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			return "CONNECTED";
		case M_EVENT_TYPE_ACCEPT:
			return "ACCEPT";
		case M_EVENT_TYPE_READ:
			return "READ";
		case M_EVENT_TYPE_WRITE:
			return "WRITE";
		case M_EVENT_TYPE_DISCONNECTED:
			return "DISCONNECT";
		case M_EVENT_TYPE_ERROR:
			return "ERROR";
		case M_EVENT_TYPE_OTHER:
			return "OTHER";
	}
	return "UNKNOWN";
}


static void loopback_check_cleanup(void)
{
	event_debug("active %"PRIu64", total %"PRIu64", expect %"PRIu64"", active_connections, connection_count, expected_connections);
}

static void loopback_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	size_t        mysize;
	unsigned char buf[1024] = { 0 };

	(void)event;
	(void)data;

	event_debug("loopback %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_atomic_inc_u64(&active_connections);
			M_atomic_inc_u64(&connection_count);
			M_io_write(comm, (const unsigned char *)"HelloWorld", 10, &mysize);
			event_debug("loopback %p wrote %"PRIu64" bytes", comm, mysize);
			break;
		case M_EVENT_TYPE_READ:
			M_io_read(comm, buf, sizeof(buf), &mysize);
			event_debug("loopback %p read %"PRIu64" bytes: %.*s", comm, mysize, (int)mysize, buf);
			if (mysize == 10 && M_mem_eq(buf, (const unsigned char *)"HelloWorld", 10)) {
				/* Initiate Disconnect */
				M_io_disconnect(comm);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			event_debug("loopback %p Freeing connection", comm);
			M_io_destroy(comm);
			M_atomic_dec_u64(&active_connections);
			loopback_check_cleanup();
			break;
		default:
			/* Ignore */
			break;
	}
}


static const char *event_err_msg(M_event_err_t err)
{
	switch (err) {
		case M_EVENT_ERR_DONE:
			return "DONE";
		case M_EVENT_ERR_RETURN:
			return "RETURN";
		case M_EVENT_ERR_TIMEOUT:
			return "TIMEOUT";
		case M_EVENT_ERR_MISUSE:
			return "MISUSE";
	}
	return "UNKNOWN";
}


static M_event_err_t check_event_loopback_test(M_uint64 num_connections)
{
	M_event_t         *event = M_event_create(M_EVENT_FLAG_EXITONEMPTY|M_EVENT_FLAG_NOWAKE);
	M_io_t            *io;
	size_t             i;
	M_event_err_t      err;

	expected_connections = num_connections;
	active_connections   = 0;
	connection_count     = 0;

	event_debug("starting %"PRIu64" loopback test", num_connections);

	for (i=0; i<num_connections; i++) {
		if (M_io_loopback_create(&io) != M_IO_ERROR_SUCCESS) {
			event_debug("failed to loopback %"PRIu64"", i);
			return M_EVENT_ERR_RETURN;
		}
		if (!M_event_add(event, io, loopback_cb, NULL)) {
			event_debug("failed to add loopback to event");
			return M_EVENT_ERR_RETURN;
		}
	}
	event_debug("added loopback ios to event loop");

	event_debug("entering loop");
	err = M_event_loop(event, 2000);
	event_debug("loop ended");

	/* Cleanup */
	M_event_destroy(event);
	M_library_cleanup();
	event_debug("exited");

	return err;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_event_loopback)
{
	M_uint64 tests[] = { 1, 25, 100, /*  200, -- disable because of mac */ 0 };
	size_t   i;

	for (i=0; tests[i] != 0; i++) {
		M_event_err_t err = check_event_loopback_test(tests[i]);
		ck_assert_msg(err == M_EVENT_ERR_DONE, "%d cnt%d expected M_EVENT_ERR_DONE got %s", (int)i, (int)tests[i], event_err_msg(err));
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *event_loopback_suite(void)
{
	Suite *suite;
	TCase *tc_event_loopback;

	suite = suite_create("event_loopback");

	tc_event_loopback = tcase_create("event_loopback");
	tcase_add_test(tc_event_loopback, check_event_loopback);
	suite_add_tcase(suite, tc_event_loopback);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(event_loopback_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_event_loopback.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
