#include "m_config.h"
#include <stdlib.h>
#include <check.h>
 
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_tls.h>

//#define USE_SSL
//#define IDNA

#ifdef USE_SSL
#  define PORT 443
/* Note, www.google.com for some reason doesn't like FALLBACK_SCSV which we enable
 * by default as of early 2018.  Seems like a google bug to me.  Lets use someone else. */
#  define URL "https://www.twitter.com/"
#  define HOST "www.twitter.com"
#else
#  define PORT 80
#  ifdef IDNA
#    define HOST "domaintest.みんな"
#    define URL "http://domaintest.みんな/"
//#    define HOST "اختبارنطاق.شبكة"
//#    define URL  "http://اختبارنطاق.شبكة/"
#  else
#    define URL  "http://www.google.com/"
#    define HOST "www.google.com"
#  endif
#endif

M_bool got_response = M_FALSE;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define DEBUG 0

#if defined(DEBUG) && DEBUG > 0
#include <stdarg.h>


static void event_debug(const char *fmt, ...)
{
	va_list     ap;
	char        buf[1024];
	M_timeval_t tv;

	M_time_gettimeofday(&tv);
	va_start(ap, fmt);
	M_snprintf(buf, sizeof(buf), "%lld.%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
	M_vdprintf(1, buf, ap);
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

	M_time_togm(M_tls_x509_time_start(x509), &sgm);
	M_time_togm(M_tls_x509_time_end(x509), &egm);
	M_asprintf(&data, "subject:%s issuer:%s date:%04lld/%02lld/%02lld-%04lld/%02lld/%02lld sig(sha1):%s", subject, issuer, sgm.year, sgm.month, sgm.day, egm.year, egm.month, egm.day, sig);

end:
	M_free(subject);
	M_free(issuer);
	M_free(cert);
	M_free(sig);
	M_tls_x509_destroy(x509);

	return data;
}

static M_dns_t *dns = NULL;


static void net_client_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *data)
{
	M_buf_t *buf = NULL;
	size_t   mysize;
	char    *msg;

	(void)event;
	(void)data;

	event_debug("net client %p event %s triggered", io, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			event_debug("net client Connected to %s %s [%s]:%u:%u (DNS: %llums, IPConnect: %llums) (TLS: %llums %s %s %s)",
				M_io_net_get_host(io), net_type(M_io_net_get_type(io)), M_io_net_get_ipaddr(io), M_io_net_get_port(io), M_io_net_get_ephemeral_port(io), 
				M_io_net_time_dns_ms(io), M_io_net_time_connect_ms(io),
				M_tls_get_negotiation_time_ms(io, M_IO_LAYER_FIND_FIRST_ID),
				tls_protocol_name(M_tls_get_protocol(io, M_IO_LAYER_FIND_FIRST_ID)),
				M_tls_get_cipher(io, M_IO_LAYER_FIND_FIRST_ID),
				M_tls_get_sessionreused(io, M_IO_LAYER_FIND_FIRST_ID)?"session reused":"session not reused");

			msg = get_cert_data(io);
			event_debug("net client %p certificate info - %s", io, msg);
			M_free(msg);

			event_debug("dns subsystem no longer needed, cleaning up");
			M_dns_destroy(dns); dns = NULL;
			buf = M_buf_create();
#if 0
			M_buf_add_str(buf, "<MonetraTrans><Trans identifier='1'><username>test_retail:public</username><password>publ1ct3st</password><action>chkpwd</action></Trans></MonetraTrans>");
#else
			M_buf_add_str(buf, "GET ");
			M_buf_add_str(buf, URL);
			M_buf_add_str(buf, " HTTP/1.0\r\n\r\n");
#endif
			mysize = M_buf_len(buf);
			M_io_write_from_buf(io, buf);
			mysize -= M_buf_len(buf);
			M_buf_cancel(buf);
			event_debug("net client %p wrote %zu bytes", io, mysize);
			break;
		case M_EVENT_TYPE_READ:
			buf = M_buf_create();
			M_io_read_into_buf(io, buf);
			event_debug("net client %p read %zu bytes: %s", io, M_buf_len(buf), M_buf_len(buf) == 0?"":M_buf_peek(buf));
			if (M_buf_len(buf) > 1) {
				event_debug("net client %p initiating close", io);
				got_response = M_TRUE;
				M_io_disconnect(io);
			}
			M_buf_cancel(buf);
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			if (type == M_EVENT_TYPE_ERROR) {
				char errmsg[256];
				M_io_get_error_string(io, errmsg, sizeof(errmsg));
				event_debug("net client %p errmsg: %s", io, errmsg);
				ck_assert_msg(got_response, "No response, Received error '%s'", errmsg);
			}
			/* If we really didn't get a response, it is an error */
			if (!got_response) {
				M_event_return(event);
				event_debug("net client %p ERROR", io);
				M_io_destroy(io);
				break;
			}

			/* NOTE: since we are setting M_io_disconnect(io), we could receive a "connection reset by peer" error
			 *       as we aren't actually waiting on the full response.  This is really valid for our odd
			 *       little test case here. */
			event_debug("net client %p DISCONNECTED", io);
			M_io_destroy(io);

			break;
		default:
			/* Ignore */
			break;
	}
}


static void trace(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
	char       *buf;
	M_timeval_t tv;
	(void)cb_arg;

	M_time_gettimeofday(&tv);
	if (type == M_IO_TRACE_TYPE_EVENT) {
		event_debug("%lld.%06lld: TRACE: event %s", tv.tv_sec, tv.tv_usec, event_type_str(event_type));
		return;
	}

	event_debug("%lld.%06lld: TRACE: %s", tv.tv_sec, tv.tv_usec, (type == M_IO_TRACE_TYPE_READ)?"READ":"WRITE");
	buf = M_str_hexdump(M_STR_HEXDUMP_DECLEN, 0, NULL, data, data_len); 
	event_debug("%s", buf);
	M_free(buf);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_dns)
{
	M_event_t         *event     = M_event_create(M_EVENT_FLAG_EXITONEMPTY);
	M_io_t            *netclient = NULL;
#ifdef USE_SSL
	M_tls_clientctx_t *ctx       = NULL;
#endif

	dns = M_dns_create(event);
	ck_assert_msg(dns != NULL, "DNS failed to initialized");

#ifdef USE_SSL
	ctx = M_tls_clientctx_create();
	ck_assert_msg(ctx != NULL, "clientctx failed to initialize");
	ck_assert_msg(M_tls_clientctx_set_default_trust(ctx), "failed to load default trust list");
#endif

	ck_assert_msg(M_io_net_client_create(&netclient, dns, HOST, PORT, M_IO_NET_ANY) == M_IO_ERROR_SUCCESS, "failed to initialize net client");
	ck_assert_msg(netclient != NULL, "net client failed to initialized");

#ifdef USE_SSL
	ck_assert_msg(M_io_tls_client_add(netclient, ctx, HOST, NULL) == M_IO_ERROR_SUCCESS, "failed to add ssl");
	M_tls_clientctx_destroy(ctx); /* Reference counters keep this around */
#endif
	M_io_add_trace(netclient, NULL, trace, NULL, NULL, NULL);

	ck_assert_msg(M_event_add(event, netclient, net_client_cb, NULL), "failed to add net client to event");

	event_debug("entering loop");
	ck_assert_msg(M_event_loop(event, 8000) == M_EVENT_ERR_DONE, "event loop did not complete");
	event_debug("loop exited");

	M_event_destroy(event);
	M_library_cleanup();
	event_debug("exited");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static volatile M_uint32 queries = 0;

static void ghbn_cb(const M_list_str_t *ipaddrs, void *cb_data, M_dns_result_t result)
{
	M_uint32 num_left;
	const char *addr;
	ck_assert_msg(result == M_DNS_RESULT_SUCCESS, "Expected successful DNS query for %s, got %d", (const char *)cb_data, (int)result);
	ck_assert_msg(M_list_str_len(ipaddrs) > 0, "Expected DNS query for %s to return ip addresses", (const char *)cb_data);

	addr     = M_list_str_at(ipaddrs, M_rand_range(NULL, 0, M_list_str_len(ipaddrs)));

	M_dns_happyeyeballs_update(dns, addr, M_HAPPYEB_STATUS_GOOD);

	/* decrement last */
	num_left = M_atomic_dec_u32(&queries) - 1;
	event_debug("result for %s returned %zu ip addresses. marking %s as heb GOOD. %u queries remaining", (const char *)cb_data, M_list_str_len(ipaddrs), addr, num_left);

}

static void ghbn_cache_cb(const M_list_str_t *ipaddrs, void *cb_data, M_dns_result_t result)
{
	M_uint32 num_left;
	M_bool   is_success;
	is_success = (result == M_DNS_RESULT_SUCCESS_CACHE || result == M_DNS_RESULT_SUCCESS_CACHE_EVICT);
	ck_assert_msg(is_success, "Expected successful cached DNS query for %s, got %d", (const char *)cb_data, (int)result);
	ck_assert_msg(M_list_str_len(ipaddrs) > 0, "Expected DNS query for %s to return ip addresses", (const char *)cb_data);

	num_left = M_atomic_dec_u32(&queries) - 1;
	event_debug("result for %s returned %zu ip addresses. first ip is %s. %u queries remaining", (const char *)cb_data, M_list_str_len(ipaddrs), M_list_str_at(ipaddrs, 0), num_left);
}



START_TEST(check_dns_reload)
{
	const char * const hosts[]        = {
		"google.com",       "www.google.com",
		"microsoft.com",    "www.microsoft.com",
		"facebook.com",     "www.facebook.com",
		"amazon.com",       "www.amazon.com",
		"apple.com",        "www.apple.com",
		"linkedin.com",     "www.linkedin.com",
		"ibm.com",          "www.ibm.com",
		"cloudflare.com",   "www.cloudflare.com",
		NULL
	};
	size_t             i;

	dns = M_dns_create(NULL);

	for (i=0; hosts[i] != NULL; i++) {
		M_atomic_inc_u32(&queries);
	}
	for (i=0; hosts[i] != NULL; i++) {
		event_debug("query: %s", hosts[i]);
		M_dns_gethostbyname(dns, NULL, hosts[i], M_IO_NET_ANY, ghbn_cb, M_CAST_OFF_CONST(void *, hosts[i]));

		/* Force reload of server by changing the config */
		M_dns_set_query_timeout(dns, 5000 - (i+1));
	}

	while (queries)
		M_thread_sleep(20000);

	event_debug("query cached results");

	/* This should get cached results now */
	for (i=0; hosts[i] != NULL; i++) {
		M_atomic_inc_u32(&queries);
	}
	for (i=0; hosts[i] != NULL; i++) {
		event_debug("query: %s", hosts[i]);
		M_dns_gethostbyname(dns, NULL, hosts[i], M_IO_NET_ANY, ghbn_cache_cb, M_CAST_OFF_CONST(void *, hosts[i]));
	}

	event_debug("all queries done");
	M_thread_sleep(200000);

	M_dns_destroy(dns);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *dns_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("dns");

	tc = tcase_create("dns_reload");
	tcase_add_test(tc, check_dns_reload);
	tcase_set_timeout(tc, 60);
	suite_add_tcase(suite, tc);

	tc = tcase_create("dns");
	tcase_add_test(tc, check_dns);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(dns_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_dns.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
