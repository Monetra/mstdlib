/* Compile with:  gcc -Wall -W -g -o test_event test_event.c -L/usr/local/lib -lmstdlib_io -lmstdlib_thread -lmstdlib
 * Run with: LD_LIBRARY_PATH=/usr/local/lib ./test_event
 */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

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


const char *event_type_str(M_event_type_t type)
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

static void net_check_cleanup(void)
{
    event_debug("active_s %llu, active_c %llu, total_s %llu, total_c %llu, expect %llu", active_server_connections, active_client_connections, server_connection_count, client_connection_count, expected_connections);
    if (active_server_connections == 0 && active_client_connections == 0 && server_connection_count == expected_connections && client_connection_count == expected_connections) {
        if (netserver != NULL) {
            event_debug("destroying netserver listener");
            M_io_destroy(netserver);
            netserver = NULL;
        }
    }
}

static void net_client_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
    unsigned char buf[1024];
    size_t        mysize;

    (void)event;
    (void)data;

    event_debug("net client %p event %s triggered", comm, event_type_str(type));
    switch (type) {
        case M_EVENT_TYPE_CONNECTED:
            M_atomic_inc_u64(&active_client_connections);
            M_atomic_inc_u64(&client_connection_count);
            M_io_write(comm, (const unsigned char *)"HelloWorld", 10, &mysize);
            event_debug("net client %p wrote %zu bytes", comm, mysize);
            break;
        case M_EVENT_TYPE_READ:
            M_io_read(comm, buf, sizeof(buf), &mysize);
            event_debug("net client %p read %zu bytes: %.*s", comm, mysize, (int)mysize, buf);
            if (M_mem_eq(buf, (const unsigned char *)"GoodBye", 7)) {
                event_debug("net client %p initiating close", comm);
                M_io_close(comm);
            }
            break;
        case M_EVENT_TYPE_WRITE:
            break;
        case M_EVENT_TYPE_DISCONNECTED:
        case M_EVENT_TYPE_ERROR:
            event_debug("net client %p Freeing connection", comm);
            M_io_destroy(comm);
            M_atomic_dec_u64(&active_client_connections);
            net_check_cleanup();
            break;
        default:
            /* Ignore */
            break;
    }
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
            event_debug("net serverconn Connected");
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
            event_debug("net serverconn %p Freeing connection", comm);
            M_io_destroy(comm);
            M_atomic_dec_u64(&active_server_connections);
            net_check_cleanup();
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
            while ((newcomm = M_io_net_accept(comm)) != NULL) {
                event_debug("Accepted new connection");
                M_event_add(event, newcomm, net_serverconn_cb, NULL);
            }
            break;
        default:
            /* Ignore */
            break;
    }
}

#if 0

static void timer_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
    M_event_trigger_t *trigger = data;
    (void)event;
    (void)type;
    (void)comm;
    event_debug("timer triggered");
    M_event_trigger_signal(trigger);
}

static void trigger_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
    (void)event;
    (void)type;
    (void)comm;
    (void)data;

    event_debug("event triggered");
}

#endif

static M_bool run_test(M_uint64 num_connections)
{
    M_event_t         *event = M_event_create(M_EVENT_FLAG_EXITONEMPTY|M_EVENT_FLAG_NOWAKE);
    M_io_t            *netclient;
    size_t             i;
    M_event_err_t      err;

#if 0
    M_event_timer_t   *timer;
    M_event_trigger_t *trigger;
    M_timeval_t        tv;

    M_time_gettimeofday(&tv);
    tv.tv_sec += 5;

    trigger = M_event_trigger_add(event, trigger_cb, NULL);
    timer   = M_event_timer_add(event, NULL, &tv, 1000, M_FALSE, timer_cb, trigger);
#endif
    expected_connections = num_connections;
    netserver = M_io_net_server_create(1234, NULL, M_IO_NET_ANY);
    if (netserver == NULL) {
        event_debug("failed to create net server");
        return M_FALSE;
    }
    if (!M_event_add(event, netserver, net_server_cb, NULL)) {
        event_debug("failed to add net server");
        return M_FALSE;
    }
    for (i=0; i<num_connections; i++) {
        netclient = M_io_net_client_create("127.0.0.1", 1234, M_IO_NET_ANY);
        if (netclient== NULL) {
            event_debug("failed to create net client");
            return M_FALSE;
        }
        if (!M_event_add(event, netclient, net_client_cb, NULL)) {
            event_debug("failed to add net client");
        }
    }

    event_debug("entering loop");
    err = M_event_loop(event, 6000);

    /* Cleanup */
    M_io_destroy(netserver);
    M_event_destroy(event);
    M_thread_deinit();
    event_debug("exited");

    if (err != M_EVENT_ERR_DONE) {
        event_debug("returned status other than done");
        return M_FALSE;
    }
    return M_TRUE;
}

int main()
{
    if (!run_test(25))
        return 1;
    return 0;
}
