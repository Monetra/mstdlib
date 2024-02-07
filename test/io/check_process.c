#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
    TEST_CASE_ECHO      = 0,
    TEST_CASE_TIMEOUT   = 1,
    TEST_CASE_CAT       = 2,
    TEST_CASE_CAT_DELAY = 3,
} process_test_cases_t;

static const char * const process_test_names[] = { "echo", "timeout", "cat", "cat_delay" };

typedef struct {
    process_test_cases_t test;
    M_io_t              *io_stdin;
    M_io_t              *io_stdout;
    M_io_t              *io_stderr;
    M_io_t              *io_proc;
    M_event_timer_t     *timer;
} process_state_t;

static const char *process_name(process_test_cases_t test)
{
    return process_test_names[test];
}

static const char *process_io_name(const process_state_t *state, const M_io_t *io)
{
    if (io == state->io_stdin)
        return "stdin";
    if (io == state->io_stdout)
        return "stdout";
    if (io == state->io_stderr)
        return "stderr";
    if (io == state->io_proc)
        return "process";
    return "unknown";
}

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


static M_bool write_stdin(process_state_t *state)
{
    size_t       written;
    M_io_error_t io_error;
    const char   str[] = "hello world!";

    if (state->io_stdin == NULL) {
        event_debug("stdin already closed, can't write");
        return M_FALSE;
    }

    io_error = M_io_write(state->io_stdin, (const unsigned char *)str, sizeof(str), &written);
    if (io_error != M_IO_ERROR_SUCCESS || written != sizeof(str)) {
        event_debug("stdin write failed, returned %s", M_io_error_string(io_error));
        return M_FALSE;
    }

    return M_TRUE;
}

static void process_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *data)
{
    char             error[256];
    M_buf_t         *buf;
    process_state_t *state = data;

    (void)event;

    event_debug("io %p %s %s event %s triggered", io, process_name(state->test), process_io_name(state, io), event_type_str(type));
    switch (type) {
        case M_EVENT_TYPE_CONNECTED:
            if (io == state->io_proc) {
                event_debug("process %p %s %s created with pid %d", io, process_name(state->test), process_io_name(state, io), M_io_process_get_pid(io));
            } else {
                event_debug("io %p %s %s connected", io, process_name(state->test), process_io_name(state, io));
            }

            if (state->test == TEST_CASE_CAT && io == state->io_stdin) {
                if (!write_stdin(state)) {
                    M_event_return(event);
                    return;
                }
                /* Let caller know we're done writing */
                M_io_disconnect(state->io_stdin);
            }
            break;
        case M_EVENT_TYPE_READ:
            buf = M_buf_create();
            M_io_read_into_buf(io, buf);
            event_debug("io %p %s %s read %zu bytes", io, process_name(state->test), process_io_name(state, io), M_buf_len(buf));
            M_buf_cancel(buf);
            break;
        case M_EVENT_TYPE_WRITE:
            break;
        case M_EVENT_TYPE_OTHER:
            if (state->test == TEST_CASE_CAT_DELAY) {
                if (!write_stdin(state)) {
                    M_event_return(event);
                    return;
                }
                /* Let caller know we're done writing */
                M_io_disconnect(state->io_stdin);
            }
            /* Timer fired off, clear it so we don't destroy later */
            state->timer = NULL;
            break;
        case M_EVENT_TYPE_DISCONNECTED:
        case M_EVENT_TYPE_ERROR:
            M_io_get_error_string(io, error, sizeof(error));

            if (io == state->io_proc) {
                int return_code = 0;
                M_io_process_get_result_code(io, &return_code);
                event_debug("process %p %s %s ended with return code (%d), cleaning up: %s", io, process_name(state->test), process_io_name(state, io), return_code, error);
                M_io_destroy(io);

                /* Forcibly close stdin.  On Linux we're automatically notified of closure on process exit, but not necessarily
                 * on other systems. */
                if (state->io_stdin)
                    M_io_destroy(state->io_stdin);
                state->io_stdin = NULL;

                /* Error if process didn't return 0 */
                if (state->test != TEST_CASE_TIMEOUT && return_code != 0) {
                    M_event_return(event);
                }
                break;
            }

            event_debug("io %p %s %s closed, cleaning up: %s", io, process_name(state->test), process_io_name(state, io), error);
            /* On Linux/Mac we will be notified of stdin being disconnected, so mark as cleaned up already */
            if (io == state->io_stdin)
                state->io_stdin = NULL;
            if (io == state->io_stdout)
                state->io_stdout = NULL;
            if (io == state->io_stderr)
                state->io_stderr = NULL;
            M_io_destroy(io);

            /* If things are disconnecting, then we need to clean up the timer so it doesn't fire off */
            if (state->timer) {
                M_event_timer_remove(state->timer);
                state->timer = NULL;
            }
            break;
        default:
            /* Ignore */
            break;
    }
}


static void process_trace_cb(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
    char *temp;
    (void)event_type;
    if (type == M_IO_TRACE_TYPE_READ) {
        M_printf("%s [READ]:\n", (const char *)cb_arg);
    } else if (type == M_IO_TRACE_TYPE_WRITE) {
        M_printf("%s [WRITE]:\n", (const char *)cb_arg);
    } else {
        M_printf("%s [%s]\n", (const char *)cb_arg, event_type_str(event_type));
        return;
    }

    temp = M_str_hexdump(M_STR_HEXDUMP_DECLEN|M_STR_HEXDUMP_HEADER, 0, NULL, data, data_len);
    M_printf("%s\n", temp);
    M_free(temp);
}


static M_bool process_test(process_test_cases_t test_case)
{
    M_event_t         *event   = M_event_create(M_EVENT_FLAG_EXITONEMPTY);
    const char        *command;
    M_list_str_t      *args    = M_list_str_create(M_LIST_STR_NONE);
    process_state_t    state;

    switch (test_case) {
        case TEST_CASE_CAT:
        case TEST_CASE_CAT_DELAY:
#ifdef _WIN32
            command = "cmd.exe";
            M_list_str_insert(args, "/c");
            M_list_str_insert(args, "type");
#else
            command = "cat";
            M_list_str_insert(args, "-");
#endif
            break;
        case TEST_CASE_ECHO:
#ifdef _WIN32
            command = "cmd.exe";
            M_list_str_insert(args, "/c");
            M_list_str_insert(args, "echo");
            M_list_str_insert(args, "Hello World!");
#else
            command = "echo";
            M_list_str_insert(args, "Hello World!");
#endif
            break;
        case TEST_CASE_TIMEOUT:
#ifdef _WIN32
            command = "cmd.exe";
            M_list_str_insert(args, "/c");
            M_list_str_insert(args, "sleep");
            M_list_str_insert(args, "4");
#else
            command = "sleep";
            M_list_str_insert(args, "4");
#endif
            break;
    }

    event_debug("**** starting process test case %d: %s", test_case, process_name(test_case));
    M_mem_set(&state, 0, sizeof(state));
    state.test = test_case;
    if (M_io_process_create(command, args, NULL, 2000, &state.io_proc, &state.io_stdin, &state.io_stdout, &state.io_stderr) != M_IO_ERROR_SUCCESS) {
        event_debug("failed to create process %s", command);
        return M_FALSE;
    }
    M_list_str_destroy(args);

    M_io_add_trace(state.io_proc,   NULL, process_trace_cb, (void *)"process", NULL, NULL);
    M_io_add_trace(state.io_stdin,  NULL, process_trace_cb, (void *)"stdin",   NULL, NULL);
    M_io_add_trace(state.io_stdout, NULL, process_trace_cb, (void *)"stdout",  NULL, NULL);
    M_io_add_trace(state.io_stderr, NULL, process_trace_cb, (void *)"stderr",  NULL, NULL);

    if (!M_event_add(event, state.io_proc, process_cb, &state)) {
        event_debug("failed to add process io handle");
        return M_FALSE;
    }
    if (!M_event_add(event, state.io_stdin, process_cb, &state)) {
        event_debug("failed to add stdin io handle");
        return M_FALSE;
    }
    if (!M_event_add(event, state.io_stdout, process_cb, &state)) {
        event_debug("failed to add stdout io handle");
        return M_FALSE;
    }
    if (!M_event_add(event, state.io_stderr, process_cb, &state)) {
        event_debug("failed to add stderr io handle");
        return M_FALSE;
    }

    if (state.test == TEST_CASE_CAT_DELAY) {
        state.timer = M_event_timer_oneshot(event, 1000, M_TRUE, process_cb, &state);
    }

    event_debug("entering loop");
    if (M_event_loop(event, 5000) != M_EVENT_ERR_DONE) {
        event_debug("event loop did not return done");
        return M_FALSE;
    }
    event_debug("loop ended");

    /* Cleanup */
    M_event_destroy(event);
    M_library_cleanup();
    event_debug("exited");
    return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_process_echo)
{
    ck_assert(process_test(TEST_CASE_ECHO));
}
END_TEST

START_TEST(check_process_timeout)
{
    ck_assert(process_test(TEST_CASE_TIMEOUT));
}
END_TEST

START_TEST(check_process_cat)
{
    ck_assert(process_test(TEST_CASE_CAT));
}
END_TEST

START_TEST(check_process_cat_delay)
{
    ck_assert(process_test(TEST_CASE_CAT_DELAY));
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *process_suite(void)
{
    Suite *suite;
    TCase *tc;

    suite = suite_create("process");

    tc = tcase_create("process");
    tcase_add_test(tc, check_process_echo);
    tcase_add_test(tc, check_process_timeout);
    tcase_add_test(tc, check_process_cat);
    tcase_add_test(tc, check_process_cat_delay);
    suite_add_tcase(suite, tc);


    return suite;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int      nf;

    (void)argc;
    (void)argv;

    sr = srunner_create(process_suite());
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_process.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
