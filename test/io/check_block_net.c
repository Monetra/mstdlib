#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_dns_t          *dns;
M_uint16          port;
M_uint64          active_client_connections;
M_uint64          active_server_connections;
M_uint64          client_connection_count;
M_uint64          server_connection_count;
M_uint64          expected_connections;
M_thread_mutex_t *debug_lock = NULL;

#define DEBUG 0

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
M_thread_mutex_lock(debug_lock);
	M_vprintf(buf, ap);
M_thread_mutex_unlock(debug_lock);
	va_end(ap);
	fflush(stdout);
}
#else
static void event_debug(const char *fmt, ...)
{
	(void)fmt;
}
#endif

static void handle_connection(M_io_t *conn, M_bool is_server)
{
	M_parser_t  *readparser = M_parser_create(M_PARSER_FLAG_NONE);
	M_buf_t     *writebuf   = M_buf_create();
	M_io_error_t err;

	/* Odd, but we need to wait on a connection right now even though this
	 * was an accept() */
	if (is_server && M_io_block_connect(conn) != M_IO_ERROR_SUCCESS) {
		char msg[256];
		M_io_get_error_string(conn, msg, sizeof(msg));
		event_debug("%p %s Failed to %s connection: %s", conn, is_server?"netserver":"netclient", is_server?"accept":"perform", msg);
		goto cleanup;
	}
	if (is_server) {
		M_atomic_inc_u64(&active_server_connections);
		/* Delay server connection count until we actually receive a message.
		 * Sometimes windows does a spurious connection for some reason */
	} else {
		M_atomic_inc_u64(&active_client_connections);
		M_atomic_inc_u64(&client_connection_count);
	}
	event_debug("%p %s connected", conn, is_server?"netserver":"netclient");

	if (is_server) {
		M_buf_add_str(writebuf, "HelloWorld");
	}

	while (1) {
		if (M_buf_len(writebuf)) {
			size_t len = M_buf_len(writebuf);
			err        = M_io_block_write_from_buf(conn, writebuf, 20);
			if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
				event_debug("%p %s error during write", conn, is_server?"netserver":"netclient");
				goto cleanup;
			}
			event_debug("%p %s wrote %zu bytes", conn, is_server?"netserver":"netclient", len - M_buf_len(writebuf));
		}

		err = M_io_block_read_into_parser(conn, readparser, 20);
		if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
			if (err == M_IO_ERROR_DISCONNECT) {
				event_debug("%p %s disconnected", conn, is_server?"netserver":"netclient");
			} else {
				event_debug("%p %s error during read %d", conn, is_server?"netserver":"netclient", (int)err);
			}
			goto cleanup;
		}
		if (M_parser_len(readparser)) {
			event_debug("%p %s has (%zu) \"%.*s\"", conn, is_server?"netserver":"netclient", M_parser_len(readparser), (int)M_parser_len(readparser), M_parser_peek(readparser));
		}
		if (M_parser_compare_str(readparser, "GoodBye", 0, M_FALSE)) {
			/* Delay server connection count until we actually receive a message.
			 * Sometimes windows does a spurious connection for some reason */
			M_atomic_inc_u64(&server_connection_count);

			M_parser_truncate(readparser, 0);
			event_debug("%p %s closing connection", conn, is_server?"netserver":"netclient");
			M_io_block_disconnect(conn);
			goto cleanup;
		}
		if (M_parser_compare_str(readparser, "HelloWorld", 0, M_FALSE)) {
			M_parser_truncate(readparser, 0);
			M_buf_add_str(writebuf, "GoodBye");
		}
		/* event_debug("%p %s loop", conn, is_server?"netserver":"netclient"); */
	}
cleanup:
	event_debug("%p %s cleaning up", conn, is_server?"netserver":"netclient");
	M_io_destroy(conn);
	M_parser_destroy(readparser);
	M_buf_cancel(writebuf);
	if (is_server) {
		M_atomic_dec_u64(&active_server_connections);
	} else {
		if (active_client_connections)
			M_atomic_dec_u64(&active_client_connections);
	}
	event_debug("active_s %llu, active_c %llu, total_s %llu, total_c %llu, expected %llu", active_server_connections, active_client_connections, server_connection_count, client_connection_count, expected_connections);
}

static void *server_thread(void *arg)
{
	handle_connection(arg, M_TRUE);
	return NULL;
}

static void *client_thread(void *arg)
{
	M_io_t *conn        = NULL;
	(void)arg;
	event_debug("attempting client connection");
	if (M_io_net_client_create(&conn, dns, "localhost", port, M_IO_NET_ANY) == M_IO_ERROR_SUCCESS) {
		if (M_io_block_connect(conn) == M_IO_ERROR_SUCCESS) {
			handle_connection(conn, M_FALSE);
			return NULL;
		} else {
			char msg[256];
			M_io_get_error_string(conn, msg, sizeof(msg));
			event_debug("M_io_block_connect(): \"%s\"\n", msg);
		}
	}
	event_debug("client connection failed");
	return NULL;
}

static void *listener_thread(void *arg)
{
	M_io_t *netserver = arg;
	M_io_t *newconn;

	event_debug("waiting on new connections");
	while (active_server_connections != 0 || active_client_connections != 0 || server_connection_count != expected_connections || client_connection_count != expected_connections) {
		if (M_io_block_accept(&newconn, netserver, 20) == M_IO_ERROR_SUCCESS) {
			event_debug("Accepted new connection");
			M_thread_create(NULL, server_thread, newconn);
		}
	}
	M_io_destroy(netserver);
	return NULL;
}

static void check_block_net_test(M_uint64 num_connections)
{
	size_t           i;
	M_threadid_t     thread;
	M_thread_attr_t *attr = M_thread_attr_create();
	M_io_t          *netserver = NULL;
	M_io_error_t     ioerr;

	dns                       = M_dns_create(NULL);
	active_client_connections = 0;
	active_server_connections = 0;
	client_connection_count   = 0;
	server_connection_count   = 0;
	expected_connections      = num_connections;
	debug_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	event_debug("Test %llu connections", num_connections);

	do {
		port  = (M_uint16)M_rand_range(NULL, 10000, 48000);
		ioerr = M_io_net_server_create(&netserver, port, NULL, M_IO_NET_ANY);
	} while (ioerr == M_IO_ERROR_ADDRINUSE);

	if (ioerr != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create net server");
		return;
	}

	event_debug("Server created on port %d", (int)port);

	M_thread_attr_set_create_joinable(attr, M_TRUE);
	thread = M_thread_create(attr, listener_thread, netserver);
	M_thread_attr_destroy(attr);

	M_thread_sleep(10000);
	for (i=0; i<expected_connections; i++) {
		M_thread_create(NULL, client_thread, NULL);
	}

	M_thread_join(thread, NULL);
	M_dns_destroy(dns);
	event_debug("exited");
	M_thread_mutex_destroy(debug_lock); debug_lock = NULL;
	M_library_cleanup();
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_block_net)
{
	M_uint64 tests[] = { 1, 25, /* 100, 200, */ 0 };
	size_t   i;

	for (i=0; tests[i] != 0; i++) {
		check_block_net_test(tests[i]);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *block_net_suite(void)
{
	Suite *suite;
	TCase *tc_block_net;

	suite = suite_create("block_net");

	tc_block_net = tcase_create("block_net");
	//tcase_set_timeout(tc_block_net, 30);
	tcase_add_test(tc_block_net, check_block_net);
	suite_add_tcase(suite, tc_block_net);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(block_net_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_block_net.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
