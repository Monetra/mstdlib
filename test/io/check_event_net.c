#include "m_config.h"
#include <stdlib.h>
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 active_client_connections;
M_uint64 active_server_connections;
M_uint64 client_connection_count;
M_uint64 server_connection_count;
M_uint64 expected_connections;
M_uint64 delay_response_ms;
M_io_t  *netserver;
M_thread_mutex_t *debug_lock = NULL;

struct conn_state {
	M_bool is_connected;
};
typedef struct conn_state conn_state_t;

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
	M_snprintf(buf, sizeof(buf), "%"PRId64".%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
M_thread_mutex_lock(debug_lock);
	M_vprintf(buf, ap);
M_thread_mutex_unlock(debug_lock);
	va_end(ap);
}
static void trace(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
	char       *buf;
	M_timeval_t tv;

	M_time_gettimeofday(&tv);
	if (type == M_IO_TRACE_TYPE_EVENT) {
		M_printf("%"PRId64".%06lld: TRACE %p: event %s\n", tv.tv_sec, tv.tv_usec, cb_arg, event_type_str(event_type));
		return;
	}

	M_printf("%"PRId64".%06lld: TRACE %p: %s\n", tv.tv_sec, tv.tv_usec, cb_arg, (type == M_IO_TRACE_TYPE_READ)?"READ":"WRITE");
	buf = M_str_hexdump(M_STR_HEXDUMP_DECLEN, 0, NULL, data, data_len); 
	M_printf("%s\n", buf);
	M_free(buf);
}
#else
static void event_debug(const char *fmt, ...)
{
	(void)fmt;
}
#endif




static void net_check_cleanup(M_event_t *event)
{
	event_debug("active_s %"PRIu64", active_c %"PRIu64", total_s %"PRIu64", total_c %"PRIu64", expect %"PRIu64"", active_server_connections, active_client_connections, server_connection_count, client_connection_count, expected_connections);
	if (active_server_connections == 0 && active_client_connections == 0 && server_connection_count == expected_connections && client_connection_count == expected_connections) {
		M_event_done(event);
	}
}

static const char *net_type(enum M_io_net_type type)
{
	switch (type) {
		case M_IO_NET_ANY:
			return "ANY";
		case M_IO_NET_IPV4:
			return "IPv4";
		case M_IO_NET_IPV6:
			return "IPv6";
	}
	return NULL;
}

static void net_client_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	unsigned char buf[1024];
	size_t        mysize;
	char          error[256];
	conn_state_t *connstate = data;

	event_debug("net client %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_atomic_inc_u64(&active_client_connections);
			M_atomic_inc_u64(&client_connection_count);
			event_debug("net client Connected (%s) [%s]:%u:%u, %s", M_io_net_get_host(comm), M_io_net_get_ipaddr(comm), M_io_net_get_port(comm), M_io_net_get_ephemeral_port(comm), net_type(M_io_net_get_type(comm)));
			M_io_write(comm, (const unsigned char *)"HelloWorld", 10, &mysize);
			event_debug("net client %p wrote %lu bytes", comm, mysize);
			connstate->is_connected = M_TRUE;
			break;
		case M_EVENT_TYPE_READ:
			M_io_read(comm, buf, sizeof(buf), &mysize);
			event_debug("net client %p read %lu bytes: %.*s", comm, mysize, (int)mysize, buf);
			if (M_mem_eq(buf, (const unsigned char *)"GoodBye", 7)) {
				event_debug("net client %p initiating close", comm);
				M_io_disconnect(comm);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			if (type == M_EVENT_TYPE_ERROR) {
				M_io_get_error_string(comm, error, sizeof(error));
				M_printf("net client %p ERROR - %s\n", comm, error);
			}
			if (connstate->is_connected) {
				M_atomic_dec_u64(&active_client_connections);
			} else {
				event_debug("***WARN***: net client %p error or disconnect before connect", comm);
			}
			event_debug("net client %p Freeing connection", comm);
			M_free(connstate);
			M_io_destroy(comm);
			net_check_cleanup(event);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void net_serverconn_write_goodbye_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_io_t *comm = cb_arg;
	size_t  mysize;
	(void)event;
	(void)type;
	(void)io;
	M_io_write(comm, (const unsigned char *)"GoodBye", 7, &mysize);
	event_debug("net serverconn %p wrote %lu bytes", comm, mysize);
}


static void net_serverconn_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	unsigned char buf[1024];
	size_t        mysize;
	conn_state_t *connstate = data;

	event_debug("net serverconn %p (%p) event %s triggered", comm, event, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_atomic_inc_u64(&active_server_connections);
			M_atomic_inc_u64(&server_connection_count);
			connstate->is_connected = M_TRUE;
			event_debug("net serverconn Connected [%s]:%u:%u, %s", M_io_net_get_ipaddr(comm), M_io_net_get_port(comm), M_io_net_get_ephemeral_port(comm), net_type(M_io_net_get_type(comm)));
			break;
		case M_EVENT_TYPE_READ:
			M_io_read(comm, buf, sizeof(buf), &mysize);
			event_debug("net serverconn %p read %lu bytes: %.*s", comm, mysize, (int)mysize, buf);
			if (mysize == 10 && M_mem_eq(buf, (const unsigned char *)"HelloWorld", 10)) {
				if (delay_response_ms) {
					M_event_timer_oneshot(event, delay_response_ms, M_TRUE, net_serverconn_write_goodbye_cb, comm);
				} else {
					net_serverconn_write_goodbye_cb(event, 0, NULL, comm);
				}
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			event_debug("net serverconn %p Freeing connection", comm);
			if (connstate->is_connected) {
				M_atomic_dec_u64(&active_server_connections);
			} else {
				event_debug("***WARN***: net serverconn %p error or disconnect before connect", comm);
				/* Critical error */
				M_event_return(event);
			}
			M_io_destroy(comm);
			M_free(connstate);
			net_check_cleanup(event);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void net_server_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	M_io_t     *newcomm;
	(void)data;
	event_debug("net server %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_ACCEPT:
			while (M_io_accept(&newcomm, comm) == M_IO_ERROR_SUCCESS) {
				conn_state_t *connstate = M_malloc_zero(sizeof(*connstate));
				event_debug("Accepted new connection %p", newcomm);
				M_event_add(M_event_get_pool(event), newcomm, net_serverconn_cb, connstate);
			}
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


typedef struct {
	M_uint64 process_time_ms;
	M_uint64 wake_cnt;
	M_uint64 osevent_cnt;
	M_uint64 softevent_cnt;
	M_uint64 timer_cnt;
} stats_t;

static M_event_err_t check_event_net_test(M_uint64 num_connections, M_uint64 delay_ms, M_bool use_pool, M_bool scalable_only, stats_t *stats)
{
	M_event_t         *event = use_pool?M_event_pool_create(0):M_event_create(scalable_only?M_EVENT_FLAG_SCALABLE_ONLY:M_EVENT_FLAG_NONE);
	M_io_t            *netclient;
	M_dns_t           *dns   = M_dns_create(event);
	size_t             i;
	M_event_err_t      err;
	conn_state_t      *connstate;
	M_io_error_t       ioerr;
	M_uint16           port = (M_uint16)M_rand_range(NULL, 10000, 50000);

	expected_connections      = num_connections;
	active_client_connections = 0;
	active_server_connections = 0;
	client_connection_count   = 0;
	server_connection_count   = 0;
	delay_response_ms         = delay_ms;
	debug_lock                = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	event_debug("starting %"PRIu64" connection test", num_connections);

	while ((ioerr = M_io_net_server_create(&netserver, port, NULL, M_IO_NET_ANY)) == M_IO_ERROR_ADDRINUSE) {
		M_uint16 newport = (M_uint16)M_rand_range(NULL, 10000, 50000);
		event_debug("Port %d in use, switching to new port %d", (int)port, (int)newport);
		port             = newport;
	}

	if (ioerr != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create net server: %s", M_io_error_string(ioerr));
		return M_EVENT_ERR_RETURN;
	}
#if DEBUG
	M_io_add_trace(netserver, NULL, trace, netserver, NULL, NULL);
#endif
	event_debug("listener started");
	if (!M_event_add(event, netserver, net_server_cb, NULL)) {
		event_debug("failed to add net server");
		return M_EVENT_ERR_RETURN;
	}
	event_debug("listener added to event");
	for (i=0; i<num_connections; i++) {
		if (M_io_net_client_create(&netclient, dns, "localhost", port, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
			event_debug("failed to create net client");
			return M_EVENT_ERR_RETURN;
		}
		M_io_net_set_keepalives(netclient, 10, 10, 10);
#if DEBUG
		M_io_add_trace(netclient, NULL, trace, netclient, NULL, NULL);
#endif
		connstate = M_malloc_zero(sizeof(*connstate));
		if (!M_event_add(event, netclient, net_client_cb, connstate)) {
			event_debug("failed to add net client");
			return M_EVENT_ERR_RETURN;
		}
	}
	event_debug("added client connections to event loop");

	event_debug("entering loop");
	err = M_event_loop(event, 2000);

	/* Cleanup */

	/* Then clean up last io object */
	M_io_destroy(netserver);

	if (stats != NULL) {
		stats->wake_cnt        = M_event_get_statistic(event, M_EVENT_STATISTIC_WAKE_COUNT);
		stats->process_time_ms = M_event_get_statistic(event, M_EVENT_STATISTIC_PROCESS_TIME_MS);
		stats->osevent_cnt     = M_event_get_statistic(event, M_EVENT_STATISTIC_OSEVENT_COUNT);
		stats->softevent_cnt   = M_event_get_statistic(event, M_EVENT_STATISTIC_SOFTEVENT_COUNT);
		stats->timer_cnt       = M_event_get_statistic(event, M_EVENT_STATISTIC_TIMER_COUNT);
	}

	event_debug("statistics:");
	event_debug("\twake count     : %"PRIu64"", M_event_get_statistic(event, M_EVENT_STATISTIC_WAKE_COUNT));
	event_debug("\tprocess time ms: %"PRIu64"", M_event_get_statistic(event, M_EVENT_STATISTIC_PROCESS_TIME_MS));
	event_debug("\tosevent count  : %"PRIu64"", M_event_get_statistic(event, M_EVENT_STATISTIC_OSEVENT_COUNT));
	event_debug("\tsoftevent count: %"PRIu64"", M_event_get_statistic(event, M_EVENT_STATISTIC_SOFTEVENT_COUNT));
	event_debug("\ttimer count    : %"PRIu64"", M_event_get_statistic(event, M_EVENT_STATISTIC_TIMER_COUNT));

	/* Test destroying event first to make sure it can handle this */
	M_event_destroy(event);

	M_dns_destroy(dns);
	event_debug("exited");

	M_thread_mutex_destroy(debug_lock); debug_lock = NULL;
	M_library_cleanup();

	return err;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_event_net_pool)
{
	M_uint64 tests[] = { 1, 5, 25, 50, /* 100, */ 0 };
	size_t   i;

	for (i=0; tests[i] != 0; i++) {
		M_event_err_t err = check_event_net_test(tests[i], 0, M_TRUE, M_FALSE, NULL);
		ck_assert_msg(err == M_EVENT_ERR_DONE, "%d cnt%d expected M_EVENT_ERR_DONE got %s", (int)i, (int)tests[i], event_err_msg(err));
	}
}
END_TEST

START_TEST(check_event_net_stat)
{
	M_event_err_t err;
	size_t        i;

#define STATS_INIT { 0, 0, 0, 0, 0 }
	struct {
		const char *name;
		M_uint64    num_conns;
		M_uint64    delay_response_ms;
		M_bool      scalable_only;
		stats_t     stats;
	} tests[] = {
		{ "small 1 conn no delay   ", 1,   0, M_FALSE, STATS_INIT },
		{ "small 1 conn 15ms delay ", 1,  15, M_FALSE, STATS_INIT },
		{ "small 1 conn 300ms delay", 1, 300, M_FALSE, STATS_INIT },
		{ "large 1 conn no delay   ", 1,   0, M_TRUE,  STATS_INIT },
		{ "large 1 conn 15ms delay ", 1,  15, M_TRUE,  STATS_INIT },
		{ "large 1 conn 300ms delay", 1, 300, M_TRUE,  STATS_INIT },
		{ "small 2 conn no delay   ", 2,   0, M_FALSE, STATS_INIT },
		{ "small 2 conn 15ms delay ", 2,  15, M_FALSE, STATS_INIT },
		{ "small 2 conn 300ms delay", 2, 300, M_FALSE, STATS_INIT },
		{ "large 2 conn no delay   ", 2,   0, M_TRUE,  STATS_INIT },
		{ "large 2 conn 15ms delay ", 2,  15, M_TRUE,  STATS_INIT },
		{ "large 2 conn 300ms delay", 2, 300, M_TRUE,  STATS_INIT },
		{ "small 5 conn no delay   ", 5,   0, M_FALSE, STATS_INIT },
		{ "small 5 conn 15ms delay ", 5,  15, M_FALSE, STATS_INIT },
		{ "small 5 conn 300ms delay", 5, 300, M_FALSE, STATS_INIT },
		{ "large 5 conn no delay   ", 5,   0, M_TRUE,  STATS_INIT },
		{ "large 5 conn 15ms delay ", 5,  15, M_TRUE,  STATS_INIT },
		{ "large 5 conn 300ms delay", 5, 300, M_TRUE,  STATS_INIT },
		{ NULL, 0, 0, M_FALSE, STATS_INIT }
	};

	for (i=0; tests[i].name != NULL; i++) {
		err = check_event_net_test(tests[i].num_conns, tests[i].delay_response_ms, M_FALSE, tests[i].scalable_only, &tests[i].stats);
		ck_assert_msg(err == M_EVENT_ERR_DONE, "%s expected M_EVENT_ERR_DONE got %s", tests[i].name, event_err_msg(err));
	}

	M_printf("===================\n");
	for (i=0; tests[i].name != NULL; i++) {
		M_printf("%s: statistics\n", tests[i].name);
		M_printf("\twake count:      %"PRIu64"\n", tests[i].stats.wake_cnt);
		M_printf("\tosevent count:   %"PRIu64"\n", tests[i].stats.osevent_cnt);
		M_printf("\tsoftevent count: %"PRIu64"\n", tests[i].stats.softevent_cnt);
		M_printf("\ttimer count:     %"PRIu64"\n", tests[i].stats.timer_cnt);
		M_printf("\tprocess time ms: %"PRIu64"\n", tests[i].stats.process_time_ms);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *event_net_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("event_net_pool");

	tc    = tcase_create("event_net_pool");
	tcase_add_test(tc, check_event_net_pool);
	suite_add_tcase(suite, tc);

	tc    = tcase_create("event_net_stat");
	tcase_add_test(tc, check_event_net_stat);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(event_net_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_event_net.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
