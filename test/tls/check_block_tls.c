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
    fflush(stdout);
M_thread_mutex_unlock(debug_lock);
    va_end(ap);
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
    if (M_io_block_connect(conn) != M_IO_ERROR_SUCCESS) {
        char msg[256];
        M_io_get_error_string(conn, msg, sizeof(msg));
        event_debug("%p %s Failed to %s connection: %s", conn, is_server?"netserver":"netclient", is_server?"accept":"perform", msg);
        goto cleanup;
    }
    if (is_server) {
        M_atomic_inc_u64(&active_server_connections);
        M_atomic_inc_u64(&server_connection_count);
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
    event_debug("attempting client connection");
    handle_connection(arg, M_FALSE);
    return NULL;
}

static void *listener_thread(void *arg)
{
    M_io_t            *netserver = arg;
    M_io_t            *newconn;

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

static M_bool tls_gen_key_cert(char **key, char **cert)
{
    M_tls_x509_t      *x509;

    *key = M_tls_rsa_generate_key(2048);
    if (*key == NULL) {
        event_debug("failed to generate RSA private key");
        return M_FALSE;
    }
    x509 = M_tls_x509_new(*key);
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

    *cert = M_tls_x509_selfsign(x509, 365 * 24 * 60 * 60 /* 1 year */);
    if (*cert == NULL) {
        event_debug("failed to self-sign");
        return M_FALSE;
    }
    M_tls_x509_destroy(x509);
    return M_TRUE;
}

static M_event_err_t check_block_tls_test(M_uint64 num_connections)
{
    M_io_t            *conn;
    size_t             i;
    M_threadid_t       thread;
    M_thread_attr_t   *attr = M_thread_attr_create();
    M_dns_t           *dns  = M_dns_create(NULL);
    char              *cert = NULL;
    char              *key  = NULL;
    M_io_t            *netserver = NULL;
    M_tls_serverctx_t *serverctx;
    M_tls_clientctx_t *clientctx;
    M_io_error_t       err;
    M_uint16           port = 0;

    active_client_connections = 0;
    active_server_connections = 0;
    client_connection_count   = 0;
    server_connection_count   = 0;
    expected_connections      = num_connections;
    debug_lock                = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

    event_debug("%s(): enter test for %d connections", __FUNCTION__, (int)num_connections);

    /* Generate real cert */
    if (!tls_gen_key_cert(&key, &cert))
        return M_EVENT_ERR_RETURN;

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

    if (!M_tls_clientctx_set_trust_cert(clientctx, (const M_uint8 *)cert, M_str_len(cert))) {
        event_debug("failed to set server cert trust");
        return M_EVENT_ERR_RETURN;
    }

    /* GENERATE SERVER CTX */

    serverctx = M_tls_serverctx_create((const M_uint8 *)key, M_str_len(key), (const M_uint8 *)cert, M_str_len(cert), NULL, 0);
    if (serverctx == NULL) {
        event_debug("failed to create serverctx");
        return M_EVENT_ERR_RETURN;
    }


    /* CLEAN UP */
    M_free(key);
    M_free(cert);

    event_debug("Test %llu connections", num_connections);

    err = M_io_net_server_create(&netserver, 0 /* any port */, NULL, M_IO_NET_ANY);

    if (err != M_IO_ERROR_SUCCESS) {
        event_debug("failed to create net server");
        return M_EVENT_ERR_RETURN;
    }

    port = M_io_net_get_port(netserver);

    if (M_io_tls_server_add(netserver, serverctx, NULL) != M_IO_ERROR_SUCCESS) {
        event_debug("failed to wrap net server with tls");
        return M_EVENT_ERR_RETURN;
    }
    M_thread_attr_set_create_joinable(attr, M_TRUE);
    thread = M_thread_create(attr, listener_thread, netserver);
    M_thread_attr_destroy(attr);

    M_thread_sleep(10000);
    for (i=0; i<expected_connections; i++) {
        if (M_io_net_client_create(&conn, dns, "localhost", port, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
            event_debug("failed to create client");
            return M_EVENT_ERR_RETURN;
        }
        if (M_io_tls_client_add(conn, clientctx, NULL /* host auto-grabbed from underlying net */, NULL) != M_IO_ERROR_SUCCESS) {
            event_debug("failed to wrap net client with tls");
            return M_EVENT_ERR_RETURN;
        }
        M_thread_create(NULL, client_thread, conn);
    }

    M_thread_join(thread, NULL);
    M_tls_clientctx_destroy(clientctx);
    M_tls_serverctx_destroy(serverctx);
    M_dns_destroy(dns);
    event_debug("exited");
    M_thread_mutex_destroy(debug_lock); debug_lock = NULL;
    M_library_cleanup();
    return M_EVENT_ERR_DONE;
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_block_tls)
{
    M_uint64 tests[] = { 1, 25, /* 100, */ /* 200, -- disable because of mac */ 0 };
    size_t   i;

    for (i=0; tests[i] != 0; i++) {
        M_event_err_t err = check_block_tls_test(tests[i]);
        ck_assert_msg(err == M_EVENT_ERR_DONE, "%d cnt%d expected M_EVENT_ERR_DONE got %s", (int)i, (int)tests[i], event_err_msg(err));
    }
}
END_TEST


static void *tls_disconresp_listener(void *arg)
{
    M_io_t      *netserver   = arg;
    M_io_t      *conn        = NULL;
    M_parser_t  *readparser  = M_parser_create(M_PARSER_FLAG_NONE);
    M_io_error_t err;
    size_t       len_written = 0;
    char         msg[256];

    /* Listen for a single new connection */
    if (M_io_block_accept(&conn, netserver, M_TIMEOUT_INF) != M_IO_ERROR_SUCCESS) {
        event_debug("Failed to accept connection");
        M_io_destroy(netserver);
        return NULL;
    }
    M_io_destroy(netserver);

    /* Finalize, perform negotiation */
    if (M_io_block_connect(conn) != M_IO_ERROR_SUCCESS) {
        M_io_get_error_string(conn, msg, sizeof(msg));
        event_debug("%p netserver failed to accept connection: %s", conn, msg);
        goto cleanup;
    }

    /* Read Hello World */
    while (1) {
        err = M_io_block_read_into_parser(conn, readparser, 20);
        if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
            if (err == M_IO_ERROR_DISCONNECT) {
                event_debug("%p netserver disconnected", conn);
            } else {
                event_debug("%p netserver error during read %d", conn, (int)err);
            }
            goto cleanup;
        }
        if (M_parser_len(readparser)) {
            event_debug("%p netserver has (%zu) \"%.*s\"", conn, M_parser_len(readparser), (int)M_parser_len(readparser), M_parser_peek(readparser));
        }
        /* If we have hello world, break! */
        if (M_parser_compare_str(readparser, "HelloWorld", 0, M_FALSE)) {
            break;
        }
    }

    /* Write GoodBye */
    err = M_io_block_write(conn, (const M_uint8 *)"GoodBye", M_str_len("GoodBye"), &len_written, M_TIMEOUT_INF);
    if (err != M_IO_ERROR_SUCCESS || len_written != M_str_len("GoodBye")) {
        M_io_get_error_string(conn, msg, sizeof(msg));
        event_debug("%p netserver failed to write %zu bytes: %d: %s", conn, M_str_len("GoodBye"), (int)err, msg);
        goto cleanup;
    }

    M_io_block_disconnect(conn);

cleanup:
    M_io_destroy(conn);
    M_parser_destroy(readparser);

    return NULL;
}


static M_event_err_t check_block_tls_disconresp_test(void)
{
    M_dns_t           *dns  = M_dns_create(NULL);
    char              *cert = NULL;
    char              *key  = NULL;
    M_io_t            *netserver;
    M_tls_serverctx_t *serverctx;
    M_tls_clientctx_t *clientctx;
    M_parser_t        *readparser  = M_parser_create(M_PARSER_FLAG_NONE);
    M_io_t            *conn = NULL;
    char               msg[256];
    size_t             len_written = 0;
    M_event_err_t      ev_err = M_EVENT_ERR_RETURN;
    M_bool             has_goodbye = M_FALSE;
    M_io_error_t       err;
    M_uint16           port = 0;

    debug_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

    event_debug("%s(): port %d", __FUNCTION__, (int)port);
    /* Generate real cert */
    if (!tls_gen_key_cert(&key, &cert))
        return M_EVENT_ERR_RETURN;

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

    if (!M_tls_clientctx_set_trust_cert(clientctx, (const M_uint8 *)cert, M_str_len(cert))) {
        event_debug("failed to set server cert trust");
        return M_EVENT_ERR_RETURN;
    }

    /* GENERATE SERVER CTX */

    serverctx = M_tls_serverctx_create((const M_uint8 *)key, M_str_len(key), (const M_uint8 *)cert, M_str_len(cert), NULL, 0);
    if (serverctx == NULL) {
        event_debug("failed to create serverctx");
        return M_EVENT_ERR_RETURN;
    }


    /* CLEAN UP */
    M_free(key);
    M_free(cert);

    err = M_io_net_server_create(&netserver, 0 /* any port */, NULL, M_IO_NET_ANY);
    if (err != M_IO_ERROR_SUCCESS) {
        event_debug("failed to create net server on port %d", (int)port);
        return M_EVENT_ERR_RETURN;
    }

    port = M_io_net_get_port(netserver);

    if (M_io_tls_server_add(netserver, serverctx, NULL) != M_IO_ERROR_SUCCESS) {
        event_debug("failed to wrap net server with tls");
        return M_EVENT_ERR_RETURN;
    }

    M_thread_create(NULL, tls_disconresp_listener, netserver);
    M_thread_sleep(10000);

    if (M_io_net_client_create(&conn, dns, "localhost", port, M_IO_NET_ANY) != M_IO_ERROR_SUCCESS) {
        event_debug("failed to create client");
        return M_EVENT_ERR_RETURN;
    }
    if (M_io_tls_client_add(conn, clientctx, NULL /* host auto-grabbed from underlying net */, NULL) != M_IO_ERROR_SUCCESS) {
        event_debug("failed to wrap net client with tls");
        return M_EVENT_ERR_RETURN;
    }

    /* Establish connection */
    if (M_io_block_connect(conn) != M_IO_ERROR_SUCCESS) {
        M_io_get_error_string(conn, msg, sizeof(msg));
        event_debug("%p netclient failed to connect: %s", conn, msg);
        goto cleanup;
    }

    /* Write HelloWorld */
    err = M_io_block_write(conn, (const M_uint8 *)"HelloWorld", M_str_len("HelloWorld"), &len_written, M_TIMEOUT_INF);
    if (err != M_IO_ERROR_SUCCESS || len_written != M_str_len("HelloWorld")) {
        M_io_get_error_string(conn, msg, sizeof(msg));
        event_debug("%p netclient failed to write %zu bytes: %d: %s", conn, M_str_len("HelloWorld"), (int)err, msg);
        goto cleanup;
    }

    /* Lets make sure both the data and the disconnect are buffered */
    M_thread_sleep(50000);

    /* Read GoodBye */
    while (1) {
        err = M_io_block_read_into_parser(conn, readparser, 20);
        if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
            if (err == M_IO_ERROR_DISCONNECT) {
                event_debug("%p netclient disconnected", conn);
            } else {
                M_io_get_error_string(conn, msg, sizeof(msg));
                event_debug("%p netclient error during read %d: %s", conn, (int)err, msg);
            }
            if (has_goodbye)
                ev_err = M_EVENT_ERR_DONE;
            goto cleanup;
        }
        if (M_parser_len(readparser)) {
            event_debug("%p netclient has (%zu) \"%.*s\"", conn, M_parser_len(readparser), (int)M_parser_len(readparser), M_parser_peek(readparser));
        }
        /* If we have GoodBye, break! */
        if (M_parser_compare_str(readparser, "GoodBye", 0, M_FALSE)) {
            event_debug("%p netclient read GoodBye!", conn);
            has_goodbye = M_TRUE;
        }
    }

cleanup:
    M_io_destroy(conn);
    M_parser_destroy(readparser);
    M_tls_clientctx_destroy(clientctx);
    M_tls_serverctx_destroy(serverctx);
    M_dns_destroy(dns);
    event_debug("exited");
    M_thread_mutex_destroy(debug_lock); debug_lock = NULL;
    M_library_cleanup();
    return ev_err;
}


START_TEST(check_block_tls_disconresp)
{
    ck_assert_msg(check_block_tls_disconresp_test() == M_EVENT_ERR_DONE, "test failed");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *block_net_suite(void)
{
    Suite *suite;
    TCase *tc;

    suite = suite_create("block_tls");
    tc = tcase_create("block_tls");
    tcase_set_timeout(tc, 15);
    tcase_add_test(tc, check_block_tls);
    suite_add_tcase(suite, tc);

    tc = tcase_create("tls_block_disconnect_after_resp");
    tcase_set_timeout(tc, 15);
    tcase_add_test(tc, check_block_tls_disconresp);
    suite_add_tcase(suite, tc);

    return suite;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int      nf;

    (void)argc;
    (void)argv;

    sr = srunner_create(block_net_suite());
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_block_tls.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
