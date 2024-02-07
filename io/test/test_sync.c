/* Compile with:  gcc -Wall -W -g -o test_sync test_sync.c -L/usr/local/lib -lmstdlib_io -lmstdlib_thread -lmstdlib
 * Run with: LD_LIBRARY_PATH=/usr/local/lib ./test_sync
 */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

M_uint64 active_client_connections;
M_uint64 active_server_connections;
M_uint64 client_connection_count;
M_uint64 server_connection_count;
M_uint64 expected_connections;

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

static void handle_connection(M_io_t *conn, M_bool is_server)
{
    M_parser_t  *readparser = M_parser_create(M_PARSER_FLAG_NONE);
    M_buf_t     *writebuf   = M_buf_create();
    M_io_error_t err;

    /* Odd, but we need to wait on a connection right now even though this
     * was an accept() */
    if (M_io_sync_connect(conn) != M_IO_ERROR_SUCCESS) {
        event_debug("%p %s Failed to %s connection", conn, is_server?"netserver":"netclient", is_server?"accept":"perform");
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
            err        = M_io_sync_write_from_buf(conn, writebuf, 20);
            if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
                event_debug("%p %s error during write", conn, is_server?"netserver":"netclient");
                goto cleanup;
            }
            event_debug("%p %s wrote %zu bytes", conn, is_server?"netserver":"netclient", len - M_buf_len(writebuf));
        }

        err = M_io_sync_read_into_parser(conn, readparser, 20);
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
            M_io_sync_close(conn);
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
        M_atomic_dec_u64(&active_client_connections);
    }
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
    M_io_t *netserver = M_io_net_server_create(1234, NULL, M_IO_NET_ANY);
    M_io_t *newconn;
    (void)arg;
    if (netserver == NULL) {
        event_debug("failed to create net server");
        return NULL;
    }

    event_debug("waiting on new connections");
    while (active_server_connections != 0 || active_client_connections != 0 || server_connection_count != expected_connections || client_connection_count != expected_connections) {
        newconn = M_io_sync_accept(netserver, 20);
        if (newconn != NULL) {
            event_debug("Accepted new connection");
            M_thread_create(NULL, server_thread, newconn);
        }
    }
    M_io_destroy(netserver);
    return NULL;
}

static void check_sync_net_test(M_uint64 num_connections)
{
    M_io_t          *conn;
    size_t           i;
    M_threadid_t     thread;
    M_thread_attr_t *attr = M_thread_attr_create();

    active_client_connections = 0;
    active_server_connections = 0;
    client_connection_count   = 0;
    server_connection_count   = 0;
    expected_connections      = num_connections;

    M_thread_attr_set_create_joinable(attr, M_TRUE);
    thread = M_thread_create(attr, listener_thread, NULL);
    M_thread_attr_destroy(attr);

    M_thread_sleep(10000);
    for (i=0; i<expected_connections; i++) {
        conn = M_io_net_client_create("127.0.0.1", 1234, M_IO_NET_ANY);
        M_thread_create(NULL, client_thread, conn);
    }

    M_thread_join(thread, NULL);

    event_debug("exited");
    M_thread_deinit();
}

int main()
{
    check_sync_net_test(25);
    return 0;
}
