#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

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


struct net_data {
    M_buf_t    *buf;
    M_timeval_t starttv;
    M_bool      is_disconnecting;
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
            if (data->is_disconnecting)
                break;
            mysize = M_buf_len(data->buf);
            if (mysize) {
                M_io_write_from_buf(comm, data->buf);
                event_debug("net client %p wrote %zu bytes (%lluKBps). %llums elapsed.", comm, mysize - M_buf_len(data->buf), M_io_bwshaping_get_Bps(comm, client_id, M_IO_BWSHAPING_DIRECTION_OUT) / 1024, M_time_elapsed(&data->starttv));
            }
            if (M_buf_len(data->buf) == 0) {
                if (runtime_ms == 0 || M_time_elapsed(&data->starttv) >= runtime_ms) {
                    event_debug("net client %p initiating disconnect", comm);
                    M_io_disconnect(comm);
                    data->is_disconnecting = M_TRUE;
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
            event_debug("net client %p Freeing connection (%lluKB total in %llu ms)", comm,
                M_io_bwshaping_get_totalbytes(comm, client_id, M_IO_BWSHAPING_DIRECTION_OUT) / 1024, M_io_bwshaping_get_totalms(comm, client_id));
            M_io_destroy(comm);
            net_data_destroy(data);
            event_debug("net client %zu event objects", M_event_num_objects(event));
            if (M_event_num_objects(event) == 0)
                M_event_done(event);
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
                event_debug("net serverconn %p read %zu bytes (%lluKBps). %llums elapsed.", comm, M_buf_len(data->buf) - mysize, M_io_bwshaping_get_Bps(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN) / 1024, M_time_elapsed(&data->starttv));
//              M_printf("read size %zu bytes\n", M_buf_len(data->buf) - mysize);
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
            event_debug("net serverconn %p Freeing connection (%lluKB total in %llu ms)", comm,
                M_io_bwshaping_get_totalbytes(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN) / 1024, M_io_bwshaping_get_totalms(comm, server_id));
            KBps = (M_io_bwshaping_get_totalbytes(comm, server_id, M_IO_BWSHAPING_DIRECTION_IN) / M_MAX(1, (M_io_bwshaping_get_totalms(comm, server_id) / 1000))) / 1024;
            M_printf("Speed: %llu.%03llu MB/s\n", KBps/1024, KBps % 1024);
            M_io_destroy(comm);
            net_data_destroy(data);
            event_debug("net serverconn %zu event objects", M_event_num_objects(event));
            if (M_event_num_objects(event) == 0)
                M_event_done(event);
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
            if (M_io_accept(&newcomm, comm) != M_IO_ERROR_SUCCESS)
                break;

            event_debug("Accepted new connection");
            M_event_add(event, newcomm, net_serverconn_cb, net_data_create());
            /* We aren't listening for any new connections, destroy listener */
            M_io_destroy(comm);
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

static M_bool check_netspeed_test(void)
{
    M_event_t         *event = M_event_pool_create(0);
    //M_event_t         *event = M_event_create(M_EVENT_FLAG_EXITONEMPTY);
    M_io_t            *netclient;
    M_event_err_t      err;
    M_uint16           port = 0;
    M_io_error_t       ioerr;

    runtime_ms = 4000;

    ioerr = M_io_net_server_create(&netserver, 0 /* any port */, NULL, M_IO_NET_ANY);

    if (ioerr != M_IO_ERROR_SUCCESS) {
        event_debug("failed to create net server");
        return M_FALSE;
    }

    port = M_io_net_get_port(netserver);

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

    event_debug("event loop exited (%zu objects)", M_event_num_objects(event));

    ck_assert_msg(err == M_EVENT_ERR_DONE, "expected M_EVENT_ERR_DONE got %s", event_err_msg(err));

    /* Cleanup */
    M_event_destroy(event);
    M_library_cleanup();
    event_debug("exited");

    return err==M_EVENT_ERR_DONE?M_TRUE:M_FALSE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_netspeed)
{
    check_netspeed_test();
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *netspeed_suite(void)
{
    Suite *suite;
    TCase *tc;

    suite = suite_create("netspeed");

    tc = tcase_create("netspeed");
    tcase_add_test(tc, check_netspeed);
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

    sr = srunner_create(netspeed_suite());
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_netspeed.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
