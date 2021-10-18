#include "m_config.h"
#include <stdlib.h>
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t   writer_id;
size_t   reader_id;
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
	M_snprintf(buf, sizeof(buf), "%"PRId64".%06lld: %s\n", tv.tv_sec, tv.tv_usec, fmt);
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


struct pipe_data {
	M_buf_t    *buf;
	M_timeval_t starttv;
};
typedef struct pipe_data pipe_data_t;

static pipe_data_t *pipe_data_create(void)
{
	pipe_data_t *data = M_malloc_zero(sizeof(*data));
	data->buf = M_buf_create();
	M_time_elapsed_start(&data->starttv);
	return data;
}

static void net_data_destroy(pipe_data_t *data)
{
	M_buf_cancel(data->buf);
	M_free(data);
}

static void pipe_writer_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *arg)
{
	size_t        mysize;
	pipe_data_t   *data = arg;

	(void)event;

	event_debug("pipe writer %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_READ:
			/* Do nothing */
			break;
		case M_EVENT_TYPE_CONNECTED:
			event_debug("pipe writer %p connected", comm);
			M_buf_add_fill(data->buf, '0', 1024 * 1024 * 8);
			/* Fall-thru */
		case M_EVENT_TYPE_WRITE:
			mysize = M_buf_len(data->buf);
			if (mysize) {
				M_io_write_from_buf(comm, data->buf);
				event_debug("pipe writer %p wrote %zu bytes (%"PRIu64" Bps)", comm, mysize - M_buf_len(data->buf), M_io_bwshaping_get_Bps(comm, writer_id, M_IO_BWSHAPING_DIRECTION_OUT));
			}
			if (M_buf_len(data->buf) == 0) {
				if (runtime_ms == 0 || M_time_elapsed(&data->starttv) >= runtime_ms) {
					event_debug("pipe writer %p initiating disconnect", comm);
					M_io_disconnect(comm);
					break;
				}
				/* Refill */
				M_buf_add_fill(data->buf, '0', 1024 * 1024 * 8);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			event_debug("pipe writer %p Freeing connection (%"PRIu64" total bytes in %"PRIu64" ms)", comm,
				M_io_bwshaping_get_totalbytes(comm, writer_id, M_IO_BWSHAPING_DIRECTION_OUT), M_io_bwshaping_get_totalms(comm, writer_id));
			M_io_destroy(comm);
			net_data_destroy(data);
			break;
		default:
			/* Ignore */
			break;
	}
}


static void pipe_reader_cb(M_event_t *event, M_event_type_t type, M_io_t *comm, void *arg)
{
	size_t        mysize;
	pipe_data_t   *data = arg;
	M_io_error_t  err;
	M_uint64      KBps;
	(void)event;

	event_debug("pipe reader %p event %s triggered", comm, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			event_debug("pipe reader %p Connected", comm);
			break;
		case M_EVENT_TYPE_READ:
			mysize = M_buf_len(data->buf);
			err    = M_io_read_into_buf(comm, data->buf);
			if (err == M_IO_ERROR_SUCCESS) {
				event_debug("pipe reader %p read %zu bytes (%"PRIu64" Bps)", comm, M_buf_len(data->buf) - mysize, M_io_bwshaping_get_Bps(comm, reader_id, M_IO_BWSHAPING_DIRECTION_IN));
				M_buf_truncate(data->buf, 0);
			} else {
				event_debug("pipe reader %p read returned %d", comm, (int)err);
			}
			break;
		case M_EVENT_TYPE_WRITE:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			event_debug("pipe reader %p Freeing connection (%"PRIu64" total bytes in %"PRIu64" ms)", comm,
				M_io_bwshaping_get_totalbytes(comm, reader_id, M_IO_BWSHAPING_DIRECTION_IN), M_io_bwshaping_get_totalms(comm, reader_id));
			KBps = (M_io_bwshaping_get_totalbytes(comm, reader_id, M_IO_BWSHAPING_DIRECTION_IN) / M_MAX(1, (M_io_bwshaping_get_totalms(comm, reader_id) / 1000))) / 1024;
			M_printf("Speed: %"PRIu64".%03llu MB/s\n", KBps/1024, KBps % 1024);
			M_io_destroy(comm);
			net_data_destroy(data);
			M_event_done(event);
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


static M_bool check_pipespeed_test(void)
{
	M_event_t     *event = M_event_pool_create(0);
//	M_event_t     *event = M_event_create(M_EVENT_FLAG_NONE);
	M_io_t        *pipereader;
	M_io_t        *pipewriter;
	M_event_err_t  err;

	runtime_ms = 4000;

	if (M_io_pipe_create(M_IO_PIPE_NONE, &pipereader, &pipewriter) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to create pipe");
		return M_FALSE;
	}

	if (M_io_add_bwshaping(pipewriter, &writer_id) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to add bwshaping to pipe writer");
		return M_FALSE;
	}
#if 0
	if (!M_io_bwshaping_set_throttle_period(pipewriter, writer_id, M_IO_BWSHAPING_DIRECTION_IN, 2, 50)) {
		event_debug("failed to add throttle period to pipe writer");
		return M_FALSE;
	}
	if (!M_io_bwshaping_set_throttle_mode(pipewriter, writer_id, M_IO_BWSHAPING_DIRECTION_IN, M_IO_BWSHAPING_MODE_TRICKLE)) {
		event_debug("failed to add trickle mode to pipe writer");
		return M_FALSE;
	}
#endif
	if (M_io_add_bwshaping(pipereader, &reader_id) != M_IO_ERROR_SUCCESS) {
		event_debug("failed to add bwshaping to client");
		return M_FALSE;
	}


	if (!M_event_add(event, pipereader, pipe_reader_cb, pipe_data_create())) {
		event_debug("failed to add pipe reader");
		return M_FALSE;
	}
	if (!M_event_add(event, pipewriter, pipe_writer_cb, pipe_data_create())) {
		event_debug("failed to add pipe writer");
		return M_FALSE;
	}
	event_debug("added pipes to event loop");

	event_debug("entering loop");
	err = M_event_loop(event, 10000);

	/* Cleanup */
	M_event_destroy(event);
	M_library_cleanup();
	event_debug("exited");

	ck_assert_msg(err == M_EVENT_ERR_DONE, "expected M_EVENT_ERR_DONE got %s", event_err_msg(err));
	return err==M_EVENT_ERR_DONE?M_TRUE:M_FALSE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_pipespeed)
{
	check_pipespeed_test();
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *pipespeed_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("pipespeed");

	tc = tcase_create("pipespeed");
	tcase_add_test(tc, check_pipespeed);
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

	sr = srunner_create(pipespeed_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_pipespeed.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
