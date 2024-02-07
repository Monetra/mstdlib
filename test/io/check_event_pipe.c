#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint64 active_client_connections;
M_uint64 active_server_connections;
M_uint64 client_connection_count;
M_uint64 server_connection_count;
M_uint64 expected_connections;

#define DEBUG 0

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
#else
static void event_debug(const char *fmt, ...)
{
    (void)fmt;
}
#endif


static void pipe_check_cleanup(M_event_t *event)
{
    event_debug("active_s %llu, active_c %llu, total_s %llu, total_c %llu, expect %llu", active_server_connections, active_client_connections, server_connection_count, client_connection_count, expected_connections);
    if (active_server_connections == 0 && active_client_connections == 0 && server_connection_count == expected_connections && client_connection_count == expected_connections) {
        M_event_done(event);
    }
}

static void pipe_writer_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
    size_t        mysize;

    (void)event;
    (void)data;

    event_debug("pipe writer %p event %s triggered", comm, event_type_str(type));
    switch (type) {
        case M_EVENT_TYPE_CONNECTED:
            M_atomic_inc_u64(&active_client_connections);
            M_atomic_inc_u64(&client_connection_count);
            M_io_write(comm, (const unsigned char *)"HelloWorld", 10, &mysize);
            event_debug("pipe writer %p wrote %zu bytes", comm, mysize);

            /* Fall-thru */
        case M_EVENT_TYPE_DISCONNECTED:
        case M_EVENT_TYPE_ERROR:
            event_debug("pipe writer %p Freeing connection", comm);
            M_io_destroy(comm);
            M_atomic_dec_u64(&active_client_connections);
            pipe_check_cleanup(event);
            break;
        case M_EVENT_TYPE_READ:
        case M_EVENT_TYPE_WRITE:
        default:
            /* Ignore */
            break;
    }
}


static void pipe_reader_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *data)
{
    unsigned char buf[1024] = { 0 };
    size_t        mysize    = 0;

    (void)event;
    (void)data;

    event_debug("pipe reader %p event %s triggered", comm, event_type_str(type));
    switch (type) {
        case M_EVENT_TYPE_CONNECTED:
            M_atomic_inc_u64(&active_server_connections);
            M_atomic_inc_u64(&server_connection_count);
            event_debug("pipe reader Connected");
            break;
        case M_EVENT_TYPE_READ:
            M_io_read(comm, buf, sizeof(buf), &mysize);
            event_debug("pipe reader %p read %zu bytes: %.*s", comm, mysize, (int)mysize, buf);
            if (mysize == 10 && M_mem_eq(buf, (const unsigned char *)"HelloWorld", 10)) {
                M_io_destroy(comm);
                M_atomic_dec_u64(&active_server_connections);
                pipe_check_cleanup(event);
            }
            break;

        case M_EVENT_TYPE_DISCONNECTED:
        case M_EVENT_TYPE_ERROR:
        case M_EVENT_TYPE_WRITE:
        default:
            /* Ignore, do not clean up as it could mask an error */
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


static M_event_err_t check_event_pipe_test(M_uint64 num_connections)
{
    M_event_t         *event = M_event_create(M_EVENT_FLAG_NONE);
//  M_event_t         *event = M_event_pool_create(0);
    M_io_t            *pipereader;
    M_io_t            *pipewriter;
    char               msg[256];
    size_t             i;
    M_event_err_t      err;

    expected_connections      = num_connections;
    active_client_connections = 0;
    active_server_connections = 0;
    client_connection_count   = 0;
    server_connection_count   = 0;

    event_debug("starting %llu pipe test", num_connections);

    for (i=0; i<num_connections; i++) {
        if (M_io_pipe_create(M_IO_PIPE_NONE, &pipereader, &pipewriter) != M_IO_ERROR_SUCCESS) {
            event_debug("failed to create pipe %zu", i);
            return M_EVENT_ERR_RETURN;
        }
#if DEBUG
        M_io_add_trace(pipereader, NULL, trace, pipereader, NULL, NULL);
        M_io_add_trace(pipewriter, NULL, trace, pipewriter, NULL, NULL);
#endif
        if (!M_event_add(event, pipereader, pipe_reader_cb, NULL)) {
            M_io_get_error_string(pipereader, msg, sizeof(msg));
            event_debug("failed to add pipe reader %zu: %p: %s", i, pipereader, msg);
            return M_EVENT_ERR_RETURN;
        }
        if (!M_event_add(event, pipewriter, pipe_writer_cb, NULL)) {
            M_io_get_error_string(pipewriter, msg, sizeof(msg));
            event_debug("failed to add pipe writer %zu: %p: %s", i, pipewriter, msg);
            return M_EVENT_ERR_RETURN;
        }
    }
    event_debug("added pipes event loop");

    event_debug("entering loop");
    err = M_event_loop(event, 2000);
    event_debug("loop ended");

    /* Cleanup */
    M_event_destroy(event);
    M_library_cleanup();
    event_debug("exited");

    return err;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_event_pipe)
{
    M_uint64 tests[] = { 1,  25, 50, /* 100,  200, -- disable because of mac */ 0 };
    size_t   i;

    for (i=0; tests[i] != 0; i++) {
        M_event_err_t err = check_event_pipe_test(tests[i]);
        ck_assert_msg(err == M_EVENT_ERR_DONE, "%d cnt%d expected M_EVENT_ERR_DONE got %s", (int)i, (int)tests[i], event_err_msg(err));
    }
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *event_pipe_suite(void)
{
    Suite *suite;
    TCase *tc_event_pipe;

    suite = suite_create("event_pipe");

    tc_event_pipe = tcase_create("event_pipe");
    tcase_add_test(tc_event_pipe, check_event_pipe);
    suite_add_tcase(suite, tc_event_pipe);

    return suite;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int      nf;

    (void)argc;
    (void)argv;

    sr = srunner_create(event_pipe_suite());
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_event_pipe.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
