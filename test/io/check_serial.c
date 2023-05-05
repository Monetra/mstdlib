#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
//#define SERIAL_TEST

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
}
#else
static void event_debug(const char *fmt, ...)
{
	(void)fmt;
}
#endif

#ifdef SERIAL_TEST
M_bool got_response = M_FALSE;

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

static void M_serial_server_write(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_t *io = arg;
	size_t  len;

	(void)event;
	(void)type;
	(void)dummy_io;

	if (M_io_write(io, (const unsigned char *)"HelloWorld", 10, &len) != M_IO_ERROR_SUCCESS) {
		event_debug("serial server %p failed to write");
	} else {
		event_debug("serial server %p wrote %zu bytes", io, len);
	}
}

static void serial_server_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	size_t        len;
	M_parser_t   *parser = data;
	unsigned char temp[64];
	char          error[256];

	(void)event;
	(void)data;

	event_debug("serial server %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			/* We are going to read before we write, we should receive a WOULDBLOCK otherwise
			 * this could be a bad error condition */
			if (M_io_read(comm, temp, sizeof(temp), &len) != M_IO_ERROR_WOULDBLOCK) {
				event_debug("**EXPECTED READ TO RETURN WOULDBLOCK");
			}
			/* Lets make sure client side is open before we write */
			M_event_timer_oneshot(event, 15, M_TRUE, M_serial_server_write, comm);
			break;
		case M_EVENT_TYPE_READ:
			len = M_parser_len(parser);
			M_io_read_into_parser(comm, parser);
			event_debug("serial server %p read %zu bytes", comm, M_parser_len(parser) - len);
			if (M_parser_compare_str(parser, "GoodBye", 0, M_FALSE)) {
				/* Initiate Disconnect */
				event_debug("serial server %p got message, disconnecting...", comm);
				M_io_disconnect(comm);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			M_io_get_error_string(comm, error, sizeof(error));
			event_debug("serial server %p Freeing connection: %s", comm, error);
			M_io_destroy(comm);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void M_serial_client_destroy(M_event_t *event, M_event_type_t type, M_io_t *dummy_io, void *arg)
{
	M_io_t *io = arg;
	(void)event;
	(void)type;
	(void)dummy_io;
	event_debug("serial client %p Freeing connection (after delay)", io);
	M_io_destroy(io);
}


static void serial_client_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	size_t        len;
	M_parser_t   *parser = data;
	char          error[256];
	(void)event;
	(void)data;

	event_debug("serial client %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			break;
		case M_EVENT_TYPE_READ:
			len = M_parser_len(parser);
			M_io_read_into_parser(comm, parser);
			event_debug("serial client %p read %zu bytes", comm, M_parser_len(parser) - len);
			if (M_parser_compare_str(parser, "HelloWorld", 0, M_FALSE))  {
				M_io_write(comm, (const unsigned char *)"GoodBye", 7, &len);
				event_debug("serial client %p wrote %zu bytes", comm, len);
				/* Initiate Disconnect */
				event_debug("serial client %p got message, disconnecting...", comm);
				/* Since we just wrote goodbye, don't do the M_io_destroy until 15ms have passed! */
				//M_event_timer_oneshot(event, 15, M_TRUE, M_serial_client_destroy, comm);
				M_io_disconnect(comm);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			M_io_get_error_string(comm, error, sizeof(error));
			event_debug("serial client %p Freeing connection: %s", comm, error);
			M_io_destroy(comm);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void serial_trace_cb(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
	char *temp;
	(void)event_type;
	if (type == M_IO_TRACE_TYPE_READ) {
		M_printf("%s [READ]:\n", (const char *)cb_arg);
	} else if (type == M_IO_TRACE_TYPE_WRITE) {
		M_printf("%s [WRITE]:\n", (const char *)cb_arg);
	} else {
		return;
	}

	temp = M_str_hexdump(M_STR_HEXDUMP_DECLEN|M_STR_HEXDUMP_HEADER, 0, NULL, data, data_len);
	M_printf("%s\n", temp);
	M_free(temp);
}

static M_bool serial_loop_test(const char *port1, const char *port2)
{
	M_event_t         *event = M_event_create(M_EVENT_FLAG_EXITONEMPTY);
	M_io_t            *io1;
	M_io_t            *io2;
	M_parser_t        *parser1;
	M_parser_t        *parser2;
	char              *name1 = M_strdup("io1");
	char              *name2 = M_strdup("io2");

	event_debug("starting serial test");

	if (M_io_serial_create(&io1, port1, M_IO_SERIAL_BAUD_115200, M_IO_SERIAL_FLOWCONTROL_NONE, M_IO_SERIAL_MODE_8N1, M_IO_SERIAL_FLAG_BUSY_POLLING) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create %s", port1);
		return M_FALSE;
	}
	if (M_io_serial_create(&io2, port2, M_IO_SERIAL_BAUD_115200, M_IO_SERIAL_FLOWCONTROL_NONE, M_IO_SERIAL_MODE_8N1, M_IO_SERIAL_FLAG_BUSY_POLLING) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create %s", port2);
		return M_FALSE;
	}

	M_io_add_trace(io1, NULL, serial_trace_cb, name1, NULL, NULL);
	M_io_add_trace(io2, NULL, serial_trace_cb, name2, NULL, NULL);

	parser1 = M_parser_create(M_PARSER_FLAG_NONE);
	if (!M_event_add(event, io1, serial_server_cb, parser1)) {
		event_debug("failed to add serial1 to event");
		return M_FALSE;
	}
	parser2 = M_parser_create(M_PARSER_FLAG_NONE);
	if (!M_event_add(event, io2, serial_client_cb, parser2)) {
		event_debug("failed to add serial2 to event");
		return M_FALSE;
	}

	event_debug("entering loop");
	if (M_event_loop(event, 3000) != M_EVENT_ERR_DONE) {
		event_debug("event loop did not return done");
		return M_FALSE;
	}
	event_debug("loop ended");

	/* Cleanup */
	M_parser_destroy(parser1);
	M_parser_destroy(parser2);
	M_event_destroy(event);
	M_free(name1);
	M_free(name2);
	M_library_cleanup();
	event_debug("exited");
	return M_TRUE;
}
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_serial)
{
	M_io_serial_enum_t *serenum = M_io_serial_enum(M_TRUE);
	size_t              i;

	ck_assert_msg(serenum != NULL, "Serial Enumeration returned a failure");

	for (i=0; i < M_io_serial_enum_count(serenum); i++) {
		event_debug("serial port %zu: path='%s', name='%s'", i, M_io_serial_enum_path(serenum, i), M_io_serial_enum_name(serenum, i));
	}
	M_io_serial_enum_destroy(serenum);

#ifdef SERIAL_TEST
	/* NOTE: run twice to ensure we can re-open ports */
	for (i=0; i<2; i++) {
#  ifdef _WIN32
		ck_assert_msg(serial_loop_test("\\\\.\\COM3", "\\\\.\\COM4"));
#  elif defined(__linux__)
		ck_assert_msg(serial_loop_test("/dev/ttyUSB0", "/dev/ttyUSB1"));
#  endif
	}
#endif
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *serial_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("serial");

	tc = tcase_create("serial");
	tcase_add_test(tc, check_serial);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(serial_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_serial.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
