#include "m_config.h"
#include <stdlib.h>
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_t  *netserver;
size_t   server_id;
size_t   client_id;
M_uint64 runtime_ms;

#ifdef DEBUG
#  undef DEBUG
#endif
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
	M_snprintf(buf, sizeof(buf), "%" PRId64 ".%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
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


struct net_data {
	M_buf_t    *buf;
	M_timeval_t starttv;
};
typedef struct net_data net_data_t;

static net_data_t *net_data_create(void)
{
	net_data_t *data = M_malloc_zero(sizeof(*data));
	data->buf = M_buf_create();
	M_time_elapsed_start(&data->starttv);
	return data;
}

static void net_data_destroy(net_data_t *data)
{
	M_buf_cancel(data->buf);
	M_free(data);
}

static void net_client_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *arg)
{
	size_t        mysize;
	net_data_t   *data = arg;

	(void)event;

	event_debug("net client %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_READ:
			/* Do nothing */
			break;
		case M_EVENT_TYPE_CONNECTED:
			event_debug("net client %p connected", comm);
			M_buf_add_fill(data->buf, '0', 1024 * 1024 * 8);
			/* Fall-thru */
		case M_EVENT_TYPE_WRITE:
			mysize = M_buf_len(data->buf);
			if (mysize) {
				M_io_write_from_buf(comm, data->buf);
				event_debug("net client %p wrote %" PRIu64 " bytes (%" PRIu64 " Bps)", comm, mysize - M_buf_len(data->buf), M_io_bwshaping_get_Bps(comm, client_id, M_IO_BWSHAPING_DIRECTION_OUT));
			}
			if (M_buf_len(data->buf) == 0) {
				if (runtime_ms == 0 || M_time_elapsed(&data->starttv) >= runtime_ms) {
					event_debug("net client %p initiating disconnect", comm);
					M_io_disconnect(comm);
					break;
				}
				/* Refill */
				M_buf_add_fill(data->buf, '0', 1024 * 1024 * 8);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			if (type == M_EVENT_TYPE_ERROR) {
				char error[256];
				M_io_get_error_string(comm, error, sizeof(error));
				event_debug("net client %p ERROR %s", comm, error);
			}
			event_debug("net client %p Freeing connection (%" PRIu64 " total bytes in %" PRIu64 " ms)", comm,
				M_io_bwshaping_get_totalbytes(comm, client_id, M_IO_BWSHAPING_DIRECTION_OUT), M_io_bwshaping_get_totalms(comm, client_id));
			M_io_destroy(comm);
			net_data_destroy(data);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void net_serverconn_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *arg)
{
	size_t        mysize;
	net_data_t   *data = arg;
	M_io_error_t  err;
	M_uint64      KBps;

	(void)event;

	event_debug("net serverconn %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			event_debug("net serverconn %p Connected", comm);
			break;
		case M_EVENT_TYPE_READ:
			mysize = M_buf_len(data->buf);
			err    = M_io_read_into_buf(comm, data->buf);
			if (err == M_IO_ERROR_SUCCESS) {
				event_debug("net serverconn %p read %" PRIu64 " bytes (%" PRIu64 " Bps)", comm, M_buf_len(data->buf) - mysize, M_io_bwshaping_get_Bps(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN));
//				M_printf("read size %" PRIu64 " bytes\n", M_buf_len(data->buf) - mysize);
				M_buf_truncate(data->buf, 0);
			} else {
				event_debug("net serverconn %p read returned %d", comm, (int)err);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			if (type == M_EVENT_TYPE_ERROR) {
				char error[256];
				M_io_get_error_string(comm, error, sizeof(error));
				event_debug("net serverconn %p ERROR %s", comm, error);
			}
			event_debug("net serverconn %p Freeing connection (%" PRIu64 " total bytes in %" PRIu64 " ms)", comm,
				M_io_bwshaping_get_totalbytes(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN), M_io_bwshaping_get_totalms(comm, server_id));
			KBps = (M_io_bwshaping_get_totalbytes(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN) / M_MAX(1,(M_io_bwshaping_get_totalms(comm, server_id) / 1000))) / 1024;
			M_printf("Speed: %" PRIu64 ".%03llu MB/s\n", KBps/1024, KBps % 1024);
			M_io_destroy(comm);
			M_io_destroy(netserver);
			M_event_done_with_disconnect(event, 0, 5*1000 /* 5 sec */);
			net_data_destroy(data);
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
				event_debug("Accepted new connection");
				M_event_add(event, newcomm, net_serverconn_cb, net_data_create());

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

static M_bool check_tlsspeed_test(void)
{
	M_event_t         *event = M_event_pool_create(0);
	//M_event_t         *event = M_event_create(M_EVENT_FLAG_NONE);
	M_io_t            *netclient;
	M_event_err_t      err;
	char              *cert;
	char              *key;
	M_tls_x509_t      *x509;
	M_tls_serverctx_t *serverctx;
	M_tls_clientctx_t *clientctx;
	M_uint16           port = (M_uint16)M_rand_range(NULL, 10000, 50000);
	M_io_error_t       ioerr;

	/* GENERATE CERTIFICATES */
	event_debug("Generating certificates");
	key = M_tls_rsa_generate_key(2048);
	if (key == NULL) {
		event_debug("failed to generate RSA private key");
		return M_FALSE;
	}
	x509 = M_tls_x509_new(key);
	if (x509 == NULL) {
		event_debug("failed to generate X509 cert");
		return M_FALSE;
	}
	if (!M_tls_x509_txt_add(x509, M_TLS_X509_TXT_COMMONNAME, "localhost", M_FALSE)) {
		event_debug("failed to add common name");
		return M_FALSE;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost", M_TRUE)) {
		event_debug("failed to add subjectaltname1");
		return M_FALSE;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost.localdomain", M_TRUE)) {
		event_debug("failed to add subjectaltname2");
		return M_FALSE;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "127.0.0.1", M_TRUE)) {
		event_debug("failed to add subjectaltname3");
		return M_FALSE;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "::1", M_TRUE)) {
		event_debug("failed to add subjectaltname3");
		return M_FALSE;
	}

	cert = M_tls_x509_selfsign(x509, 365 * 24 * 60 * 60 /* 1 year */);
	if (cert == NULL) {
		event_debug("failed to self-sign");
		return M_FALSE;
	}
	M_tls_x509_destroy(x509);

	/* GENERATE CLIENT CTX */
	clientctx = M_tls_clientctx_create();
	if (clientctx == NULL) {
		event_debug("failed to create clientctx");
		return M_FALSE;
	}

	if (!M_tls_clientctx_set_default_trust(clientctx)) {
		event_debug("failed to set default clientctx trust list");
		/* Non-fatal since we set our own trust.
		 * return M_EVENT_ERR_RETURN; */
	}

	if (!M_tls_clientctx_set_trust_cert(clientctx, (const M_uint8 *)cert, M_str_len(cert))) {
		event_debug("failed to set server cert trust");
		return M_FALSE;
	}

	//M_tls_clientctx_set_protocols(clientctx, M_TLS_PROTOCOL_TLSv1_2);

	/* GENERATE SERVER CTX */
	serverctx = M_tls_serverctx_create((const M_uint8 *)key, M_str_len(key), (const M_uint8 *)cert, M_str_len(cert), NULL, 0);
	if (serverctx == NULL) {
		event_debug("failed to create serverctx");
		return M_FALSE;
	}

	/* CLEAN UP */
	M_free(key);
	M_free(cert);

	runtime_ms = 4000;

	while ((ioerr = M_io_net_server_create(&netserver, port, NULL, M_IO_NET_ANY)) == M_IO_ERROR_ADDRINUSE) {
		M_uint16 newport = (M_uint16)M_rand_range(NULL, 10000, 50000);
		event_debug("Port %d in use, switching to new port %d", (int)port, (int)newport);
		port             = newport;
	}

	if (ioerr != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create net server");
		return M_FALSE;
	}

	if (M_io_tls_server_add(netserver, serverctx, NULL) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to wrap net server with tls");
		return M_FALSE;
	}

	if (M_io_add_bwshaping(netserver, &server_id) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to add bwshaping to server");
		return M_FALSE;
	}

	event_debug("listener started");
	if (!M_event_add(event, netserver, net_server_cb, NULL)) {
		event_debug("failed to add net server");
		return M_FALSE;
	}
	event_debug("listener added to event");

	if (M_io_net_client_create(&netclient, NULL, "127.0.0.1", port, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create net client");
		return M_FALSE;
	}

	if (M_io_tls_client_add(netclient, clientctx, "localhost", NULL) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to wrap net client with tls");
		return M_FALSE;
	}

	if (M_io_add_bwshaping(netclient, &client_id) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to add bwshaping to client");
		return M_FALSE;
	}

	if (!M_event_add(event, netclient, net_client_cb, net_data_create())) {
		event_debug("failed to add net client");
		return M_FALSE;
	}
	event_debug("added client connections to event loop");

	err = M_event_loop(event, 10000);

	ck_assert_msg(err == M_EVENT_ERR_DONE, "expected M_EVENT_ERR_DONE got %s", event_err_msg(err));

	/* Cleanup */
	//M_io_destroy(netserver);
	M_event_destroy(event);
	M_tls_clientctx_destroy(clientctx);
	M_tls_serverctx_destroy(serverctx);
	M_library_cleanup();
	event_debug("exited");

	return err==M_EVENT_ERR_DONE?M_TRUE:M_FALSE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_tlsspeed)
{
	check_tlsspeed_test();
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *tlsspeed_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("tlsspeed");

	tc = tcase_create("tlsspeed");
	tcase_add_test(tc, check_tlsspeed);
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

	sr = srunner_create(tlsspeed_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_tlsspeed.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
