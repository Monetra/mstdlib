#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 active_client_connections;
M_uint64 active_server_connections;
M_uint64 client_connection_count;
M_uint64 server_connection_count;
M_uint64 expected_connections;
M_io_t  *netserver;

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


static void net_check_cleanup(M_event_t *event)
{
	event_debug("active_s %llu, active_c %llu, total_s %llu, total_c %llu, expect %llu", active_server_connections, active_client_connections, server_connection_count, client_connection_count, expected_connections);
	if (active_server_connections == 0 && active_client_connections == 0 && server_connection_count == expected_connections && client_connection_count == expected_connections) {
		if (netserver != NULL) {
			M_io_destroy(netserver);
			netserver = NULL;
//	event_debug("%s(): ev:%p Calling M_event_done_with_disconnect", __FUNCTION__, event);
			M_event_done_with_disconnect(event, 5);
		}
	}
//	event_debug("%s(): ev:%p RETURN", __FUNCTION__, event);
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

static const char *tls_protocol_name(M_tls_protocols_t protocol)
{
	switch (protocol) {
		case M_TLS_PROTOCOL_TLSv1_0:
			return "TLSv1.0";
		case M_TLS_PROTOCOL_TLSv1_1:
			return "TLSv1.1";
		case M_TLS_PROTOCOL_TLSv1_2:
			return "TLSv1.2";
		default:
			break;
	}
	return "unknown protocol";
}

static char *get_cert_data(M_io_t *io)
{
	M_tls_x509_t *x509    = NULL;
	char         *cert    = M_tls_get_peer_cert(io, M_IO_LAYER_FIND_FIRST_ID);
	char         *data    = NULL;
	char         *subject = NULL;
	char         *issuer  = NULL;
	char         *sig     = NULL;
	char         *app     = NULL;

	M_time_gmtm_t sgm;
	M_time_gmtm_t egm;

	if (cert == NULL)
		goto end;

	x509 = M_tls_x509_read_crt(cert);
	if (x509 == NULL)
		goto end;

	subject = M_tls_x509_subject_name(x509);
	if (subject == NULL)
		goto end;

	sig = M_tls_x509_signature(x509, M_TLS_X509_SIG_ALG_SHA1);
	if (sig == NULL)
		goto end;

	/* Don't fail for issuer or app */
	issuer  = M_tls_x509_issuer_name(x509);
	app     = M_tls_get_application(io, M_IO_LAYER_FIND_FIRST_ID);

	M_time_togm(M_tls_x509_time_start(x509), &sgm);
	M_time_togm(M_tls_x509_time_end(x509), &egm);
	M_asprintf(&data, "subject:%s issuer:%s date:%04lld/%02lld/%02lld-%04lld/%02lld/%02lld sig(sha1):%s app:%s", subject, issuer, sgm.year, sgm.month, sgm.day, egm.year, egm.month, egm.day, sig, app);

end:
	M_free(subject);
	M_free(issuer);
	M_free(cert);
	M_free(app);
	M_free(sig);
	M_tls_x509_destroy(x509);

	return data;
}

static void net_client_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	unsigned char buf[1024];
	size_t        mysize;
	char          *msg;

	(void)data;

	event_debug("net client %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_atomic_inc_u64(&active_client_connections);
			M_atomic_inc_u64(&client_connection_count);
			event_debug("net client Connected to %s %s [%s]:%u:%u (DNS: %llums, IPConnect: %llums) (TLS: %llums %s %s %s)",
				M_io_net_get_host(comm), net_type(M_io_net_get_type(comm)), M_io_net_get_ipaddr(comm), M_io_net_get_port(comm), M_io_net_get_ephemeral_port(comm), 
				M_io_net_time_dns_ms(comm), M_io_net_time_connect_ms(comm),
				M_tls_get_negotiation_time_ms(comm, M_IO_LAYER_FIND_FIRST_ID),
				tls_protocol_name(M_tls_get_protocol(comm, M_IO_LAYER_FIND_FIRST_ID)),
				M_tls_get_cipher(comm, M_IO_LAYER_FIND_FIRST_ID),
				M_tls_get_sessionreused(comm, M_IO_LAYER_FIND_FIRST_ID)?"session reused":"session not reused");

			msg = get_cert_data(comm);
			event_debug("net client %p certificate info - %s", comm, msg);
			M_free(msg);

			M_io_write(comm, (const unsigned char *)"HelloWorld", 10, &mysize);
			event_debug("net client %p wrote %zu bytes", comm, mysize);
			break;
		case M_EVENT_TYPE_READ:
			M_io_read(comm, buf, sizeof(buf), &mysize);
			event_debug("net client %p read %zu bytes: %.*s", comm, mysize, (int)mysize, buf);
			if (mysize == 7 && M_mem_eq(buf, (const unsigned char *)"GoodBye", 7)) {
				event_debug("net client %p initiating close", comm);
				M_io_disconnect(comm);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			if (type == M_EVENT_TYPE_ERROR) {
				char error[256];
				M_io_get_error_string(comm, error, sizeof(error));
				event_debug("net client %p ERROR %s", comm, error);
				msg = get_cert_data(comm);
				event_debug("net client %p certificate info - %s", comm, msg);
				M_free(msg);
			}
			event_debug("net client %p Freeing connection", comm);
			M_io_destroy(comm);
			M_atomic_dec_u64(&active_client_connections);
			net_check_cleanup(event);
			break;
		default:
			/* Ignore */
			break;
	}
//	event_debug("%s(): ev:%p RETURN", __FUNCTION__, event);
}


static void net_serverconn_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	unsigned char buf[1024];
	size_t        mysize;

	(void)event;
	(void)data;

	event_debug("net serverconn %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_atomic_inc_u64(&active_server_connections);
			M_atomic_inc_u64(&server_connection_count);
			event_debug("net serverconn Connected %s [%s]:%u:%u, (TLS: %llums %s %s %s)",
				net_type(M_io_net_get_type(comm)), M_io_net_get_ipaddr(comm), M_io_net_get_port(comm), M_io_net_get_ephemeral_port(comm), 
				M_tls_get_negotiation_time_ms(comm, M_IO_LAYER_FIND_FIRST_ID),
				tls_protocol_name(M_tls_get_protocol(comm, M_IO_LAYER_FIND_FIRST_ID)),
				M_tls_get_cipher(comm, M_IO_LAYER_FIND_FIRST_ID),
				M_tls_get_sessionreused(comm, M_IO_LAYER_FIND_FIRST_ID)?"session reused":"session not reused");
			break;
		case M_EVENT_TYPE_READ:
			M_io_read(comm, buf, sizeof(buf), &mysize);
			event_debug("net serverconn %p read %zu bytes: %.*s", comm, mysize, (int)mysize, buf);
			if (mysize == 10 && M_mem_eq(buf, (const unsigned char *)"HelloWorld", 10)) {
				M_io_write(comm, (const unsigned char *)"GoodBye", 7, &mysize);
				event_debug("net serverconn %p wrote %zu bytes", comm, mysize);
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
			event_debug("net serverconn %p Freeing connection", comm);
			M_io_destroy(comm);
			M_atomic_dec_u64(&active_server_connections);
			net_check_cleanup(event);
			break;
		default:
			/* Ignore */
			break;
	}
//event_debug("%s(): RETURN", __FUNCTION__);

}

#if DEBUG
static void trace(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
	char       *buf;
	M_timeval_t tv;

	M_time_gettimeofday(&tv);
	if (type == M_IO_TRACE_TYPE_EVENT) {
		M_printf("%lld.%06lld: TRACE %p: event %s\n", tv.tv_sec, tv.tv_usec, cb_arg, event_type_str(event_type));
		return;
	}

	M_printf("%lld.%06lld: TRACE %p: %s\n", tv.tv_sec, tv.tv_usec, cb_arg, (type == M_IO_TRACE_TYPE_READ)?"READ":"WRITE");
	buf = M_str_hexdump(M_STR_HEXDUMP_DECLEN, 0, NULL, data, data_len); 
	M_printf("%s\n", buf);
	M_free(buf);
}
#endif

static void net_server_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
	M_io_t     *newcomm;
	(void)data;
	event_debug("net server %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_ACCEPT:
			while (M_io_accept(&newcomm, comm) == M_IO_ERROR_SUCCESS) {
				event_debug("Accepted new connection");
				M_event_add(M_event_get_pool(event), newcomm, net_serverconn_cb, NULL);
			}
			break;
		default:
			/* Ignore */
			break;
	}
//	event_debug("%s(): RETURN", __FUNCTION__);
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


static M_event_err_t check_tls_test(M_uint64 num_connections)
{
	M_event_t          *event = M_event_pool_create(0);
	//M_event_t        *event = M_event_create(M_EVENT_FLAG_NONE);
	M_io_t             *netclient;
	size_t              i;
	M_event_err_t       err;
	char               *boguscert;
	char               *boguskey;
	char               *realcert;
	char               *realkey;
	M_tls_x509_t       *x509;
	M_tls_serverctx_t  *serverctx;
	M_tls_serverctx_t  *child_serverctx;
	M_tls_clientctx_t  *clientctx;
	M_list_str_t       *applist;
	const char * const hosts[] = { "localhost", "127.0.0.1"
#ifndef _WIN32
 ,"::1"
#endif
}; 

	expected_connections      = num_connections;
	active_client_connections = 0;
	active_server_connections = 0;
	client_connection_count   = 0;
	server_connection_count   = 0;

	/* GENERATE CERTIFICATES */
	event_debug("Generating certificates");

	/* Generate bogus cert */
	boguskey = M_tls_rsa_generate_key(2048);
	if (boguskey == NULL) {
		event_debug("failed to generate RSA private key");
		return M_EVENT_ERR_RETURN;
	}
	x509 = M_tls_x509_new(boguskey);
	if (x509 == NULL) {
		event_debug("failed to generate X509 cert");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_add(x509, M_TLS_X509_TXT_COMMONNAME, "somewhere.com", M_FALSE)) {
		event_debug("failed to add common name");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "somewhere.com", M_TRUE)) {
		event_debug("failed to add subjectaltname1");
		return M_EVENT_ERR_RETURN;
	}
	boguscert = M_tls_x509_selfsign(x509, 365 * 24 * 60 * 60 /* 1 year */);
	if (boguscert == NULL) {
		event_debug("failed to self-sign");
		return M_EVENT_ERR_RETURN;
	}
	M_tls_x509_destroy(x509);


	/* Generate real cert */
	realkey = M_tls_rsa_generate_key(2048);
	if (realkey == NULL) {
		event_debug("failed to generate RSA private key");
		return M_EVENT_ERR_RETURN;
	}
	x509 = M_tls_x509_new(realkey);
	if (x509 == NULL) {
		event_debug("failed to generate X509 cert");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_add(x509, M_TLS_X509_TXT_COMMONNAME, "localhost", M_FALSE)) {
		event_debug("failed to add common name");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost", M_TRUE)) {
		event_debug("failed to add subjectaltname1");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost.localdomain", M_TRUE)) {
		event_debug("failed to add subjectaltname2");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "127.0.0.1", M_TRUE)) {
		event_debug("failed to add subjectaltname3");
		return M_EVENT_ERR_RETURN;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "::1", M_TRUE)) {
		event_debug("failed to add subjectaltname4");
		return M_EVENT_ERR_RETURN;
	}

	realcert = M_tls_x509_selfsign(x509, 365 * 24 * 60 * 60 /* 1 year */);
	if (realcert == NULL) {
		event_debug("failed to self-sign");
		return M_EVENT_ERR_RETURN;
	}
	M_tls_x509_destroy(x509);
//M_printf("PrivateKey: %s\n", key);
M_printf("ServerCert: %s\n", realcert);

	applist = M_list_str_create(M_LIST_STR_NONE);
	M_list_str_insert(applist, "badapp");
	M_list_str_insert(applist, "testapp");

	/* GENERATE CLIENT CTX */
	clientctx = M_tls_clientctx_create();
	if (clientctx == NULL) {
		event_debug("failed to create clientctx");
		return M_EVENT_ERR_RETURN;
	}

	if (!M_tls_clientctx_set_default_trust(clientctx)) {
		event_debug("failed to set default clientctx trust list");
		/* Non-fatal since we set our own trust.
		 * return M_EVENT_ERR_RETURN; */
	}

	if (!M_tls_clientctx_set_trust_cert(clientctx, realcert, M_str_len(realcert))) {
		event_debug("failed to set server cert trust");
		return M_EVENT_ERR_RETURN;
	}

	M_tls_clientctx_set_applications(clientctx, applist);

	/* GENERATE SERVER CTX */
	M_list_str_remove_first(applist); /* Alter app list */

	serverctx = M_tls_serverctx_create(boguskey, M_str_len(boguskey), boguscert, M_str_len(boguscert), NULL, 0);
	if (serverctx == NULL) {
		event_debug("failed to create base serverctx");
		return M_EVENT_ERR_RETURN;
	}

	M_tls_serverctx_set_applications(serverctx, applist);

	child_serverctx = M_tls_serverctx_create(realkey, M_str_len(realkey), realcert, M_str_len(realcert), NULL, 0);
	if (child_serverctx == NULL) {
		event_debug("failed to create child serverctx");
		return M_EVENT_ERR_RETURN;
	}
	M_tls_serverctx_set_applications(child_serverctx, applist);

	if (!M_tls_serverctx_SNI_ctx_add(serverctx, child_serverctx)) {
		event_debug("failed to add child serverctx");
		return M_EVENT_ERR_RETURN;
	}
	
	/* CLEAN UP */
	M_list_str_destroy(applist);
	M_free(boguskey);
	M_free(boguscert);
	M_free(realkey);
	M_free(realcert);

	event_debug("starting %llu connection test", num_connections);
	if (M_io_net_server_create(&netserver, 1234, NULL, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create net server");
		return M_EVENT_ERR_RETURN;
	}

	if (M_io_tls_server_add(netserver, serverctx, NULL) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to wrap net server with tls");
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
		if (M_io_net_client_create(&netclient, NULL, hosts[i % (sizeof(hosts) / sizeof(*hosts))], 1234, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
			event_debug("failed to create net client");
			return M_EVENT_ERR_RETURN;
		}

		if (M_io_tls_client_add(netclient, clientctx, NULL /* host auto-grabbed from underlying net */, NULL) != M_IO_ERROR_SUCCESS) {
			event_debug("failed to wrap net client with tls");
			return M_EVENT_ERR_RETURN;
		}

#if DEBUG
		M_io_add_trace(netclient, NULL, trace, netclient, NULL, NULL);
#endif

		if (!M_event_add(event, netclient, net_client_cb, NULL)) {
			event_debug("failed to add net client");
			return M_EVENT_ERR_RETURN;
		}
	}
	event_debug("added client connections to event loop");

	event_debug("entering loop");
	err = M_event_loop(event, 10000);
	event_debug("%zu remaining objects", M_event_num_objects(event));
	/* Cleanup */

	M_event_destroy(event);

	M_tls_clientctx_destroy(clientctx);
	M_tls_serverctx_destroy(serverctx);
	M_library_cleanup();
	event_debug("exited");

	return err;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_tls)
{
	M_uint64 tests[] = { 1, 25, 100, /* 200, -- disable because of mac */ 0 };
	size_t   i;

	for (i=0; tests[i] != 0; i++) {
		M_event_err_t err = check_tls_test(tests[i]);
		ck_assert_msg(err == M_EVENT_ERR_DONE, "%d cnt%d expected M_EVENT_ERR_DONE got %s", (int)i, (int)tests[i], event_err_msg(err));
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *tls_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("tls");

	tc = tcase_create("tls");
	tcase_set_timeout(tc, 60);
	tcase_add_test(tc, check_tls);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(tls_suite());
	srunner_set_log(sr, "check_tls.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
