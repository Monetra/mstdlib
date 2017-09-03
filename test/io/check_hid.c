#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if 0
/* INGENICO HID LAYER */
#include <mstdlib/io/m_io_layer.h>
struct M_io_handle {
	M_parser_t *parser;
};

static M_bool M_io_ingenicohid_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */

	return M_TRUE;
}

static M_io_error_t M_io_ingenicohid_write_int(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	unsigned char hidbuf[32];
	size_t        msglen;
	M_io_error_t  err;
	M_io_t       *io     = M_io_layer_get_io(layer);

	if (buf == NULL || *write_len == 0)
		return M_IO_ERROR_INVALID;

	msglen = *write_len;
	if (*write_len > 30)
		msglen = 30;

	M_mem_set(hidbuf, 0, sizeof(hidbuf));
	hidbuf[0] = 0x02;                           /* Report ID                   */
	hidbuf[1] = (unsigned char)(msglen & 0xFF); /* 1-byte binary report length */
	M_mem_copy(hidbuf+2, buf, msglen);          /* Up to 30 byte report data   */

	msglen = sizeof(hidbuf);
	err = M_io_layer_write(io, M_io_layer_get_index(layer)-1, hidbuf, &msglen);
	if (err != M_IO_ERROR_SUCCESS)
		return err;

	/* Shouldn't be possible */
	if (msglen != sizeof(hidbuf))
		return M_IO_ERROR_ERROR;

	if (*write_len > 30) {
		*write_len = 30;
	}

	return M_IO_ERROR_SUCCESS;
}

static M_io_error_t M_io_ingenicohid_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len)
{
	size_t       bytes_written = 0;
	M_io_error_t err;
	size_t       len           = *write_len;

	/* Loop writing all data */
	while (len && (err = M_io_ingenicohid_write_int(layer, buf + bytes_written, &len)) == M_IO_ERROR_SUCCESS) {
		bytes_written += len;
		len            = *write_len - bytes_written;
	}

	/* If we wrote any data, ignore any error and return success */
	if (bytes_written) {
		*write_len = bytes_written;
		return M_IO_ERROR_SUCCESS;
	}

	return err;
}

static M_io_error_t M_io_ingenicohid_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len)
{
	M_io_handle_t *handle     = M_io_layer_get_handle(layer);
	unsigned char  report_len = 0;

	if (buf == NULL || *read_len < 30)
		return M_IO_ERROR_INVALID;

	if (M_parser_len(handle->parser) < 32)
		return M_IO_ERROR_WOULDBLOCK;

	/* Skip report id */
	M_parser_consume(handle->parser, 1);

	/* Read length byte */
	M_parser_read_byte(handle->parser, &report_len);

	/* Invalid message format */
	if (report_len == 0 || report_len > 30)
		return M_IO_ERROR_ERROR;

	/* Read report */
	M_parser_read_bytes(handle->parser, report_len, buf);
	*read_len = (size_t)report_len;

	/* Ignore trailing bytes in report */
	M_parser_consume(handle->parser, 30 - report_len);

	/* Let consumer know there is more data */
	if (M_parser_len(handle->parser) >=  32)
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);

	return M_IO_ERROR_SUCCESS;
}


static M_bool M_io_ingenicohid_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	(void)layer;

	/* When a read comes in, just buffer the data.  When a consumer tries to read from us, we'll
	 * parse on the fly */
	if (*type == M_EVENT_TYPE_READ) {
		unsigned char buf[32];
		size_t        len = sizeof(buf);
		M_io_error_t  err;

		while ((err = M_io_layer_read(io, M_io_layer_get_index(layer)-1, buf, &len)) == M_IO_ERROR_SUCCESS) {
			M_parser_append(handle->parser, buf, len);
			len = sizeof(buf);
		}

		/* Consume event if no data to hand out */
		if (M_parser_len(handle->parser) == 0)
			return M_TRUE;
	}

	/* Pass-thru */
	return M_FALSE;
}


static void M_io_ingenicohid_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
}


static void M_io_ingenicohid_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_parser_destroy(handle->parser);
	M_free(handle);
}


M_io_error_t M_io_add_ingenicohid(M_io_t *io, size_t *layer_idx)
{
	M_io_handle_t    *handle;
	M_io_layer_t     *layer;
	M_io_callbacks_t *callbacks;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	handle          = M_malloc_zero(sizeof(*handle));
	handle->parser  = M_parser_create(M_PARSER_FLAG_NONE);

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_ingenicohid_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_ingenicohid_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_ingenicohid_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_ingenicohid_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_ingenicohid_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_ingenicohid_destroy_cb);
	layer = M_io_layer_add(io, "INGENICOHID", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_idx != NULL)
		*layer_idx = M_io_layer_get_index(layer);

	return M_IO_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool got_response = M_FALSE;


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

static void hid_trace_cb(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
{
	char *temp;
	(void)event_type;
	if (type == M_IO_TRACE_TYPE_READ) {
		event_debug("[READ]:");
	} else if (type == M_IO_TRACE_TYPE_WRITE) {
		event_debug("[WRITE]:");
	} else {
		return;
	}

	temp = M_str_hexdump(M_STR_HEXDUMP_DECLEN|M_STR_HEXDUMP_HEADER, 0, NULL, data, data_len);
	event_debug("%s", temp);
	M_free(temp);
}

static void hid_event_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *data)
{
	const unsigned char request[] = { 0x02, 0x31, 0x31, 0x2e, 0x03, 0x2d };
	//const unsigned char request[] = { 0x02, 0x33, 0x33, 0x2e, 0x30, 0x31, 0x2e, 0x30, 0x30, 0x30, 0x30, 0x1c, 0x1c, 0x1c, 0x1c, 0x03, 0x02 };
	const unsigned char ack[]     = { 0x06 };
	const unsigned char etx[]     = { 0x03 };
	size_t              len;
	M_parser_t         *parser    = data;
	(void)event;

	event_debug("hid %p event %s triggered", io, event_type_str(type));
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_io_write(io, request, sizeof(request), &len);
			break;
		case M_EVENT_TYPE_READ:
			len = M_parser_len(parser);
			M_io_read_into_parser(io, parser);
			event_debug("hid %p read %zu bytes", io, M_parser_len(parser) - len);
			if (M_parser_compare(parser, ack, sizeof(ack)))  {
				event_debug("hid %p read ACK, discarding...", io);
				M_parser_consume(parser, sizeof(ack));
			}

			len = M_parser_consume_until(parser, etx, sizeof(etx), M_TRUE);
			if (len != 0) {
				M_parser_consume(parser, 1); /* LRC */
				event_debug("hid %p read full message, acking and disconnecting", io);
				M_io_write(io, ack, sizeof(ack), &len);
				M_io_disconnect(io);
			}
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			event_debug("hid %p Freeing connection", io);
			M_io_destroy(io);
			break;
		default:
			/* Ignore */
			break;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_ingenico)
{
	M_event_t     *event  = M_event_create(M_EVENT_FLAG_EXITONEMPTY);
	M_io_t        *io     = NULL;
	const M_uint16 pids[] = { 0x0071, 0x0072, 0x0073, 0x0074, 0x0076 };
	M_io_error_t   err    = M_io_hid_create_one(&io, 0x0b00, pids, sizeof(pids) / sizeof(*pids), NULL);
	M_parser_t    *parser = M_parser_create(M_PARSER_FLAG_NONE);

	ck_assert_msg(err == M_IO_ERROR_SUCCESS, "HID device not found");
	M_io_add_trace(io, NULL, hid_trace_cb, NULL, NULL, NULL);
	M_io_add_ingenicohid(io, NULL);
	ck_assert_msg(M_event_add(event, io, hid_event_cb, parser), "failed to add hid device to event");
	event_debug("entering loop");
	ck_assert_msg(M_event_loop(event, 2000) == M_EVENT_ERR_DONE, "Event loop did not return done");
	event_debug("loop ended");

	/* Cleanup */
	M_parser_destroy(parser);
	M_event_destroy(event);
	M_library_cleanup();
	event_debug("exited");
}
END_TEST

#endif

START_TEST(check_hid)
{
	M_io_hid_enum_t *hidenum;
	size_t           i;

	hidenum = M_io_hid_enum(0, NULL, 0, NULL);

	ck_assert_msg(hidenum != NULL, "HID Enumeration returned a failure");

	for (i=0; i < M_io_hid_enum_count(hidenum); i++) {
		event_debug("Device %zu: path='%s', manufacturer='%s', product='%s', serial='%s', vendorid='%04x', productid='%04x'",
				i, M_io_hid_enum_path(hidenum, i), M_io_hid_enum_manufacturer(hidenum, i),
				M_io_hid_enum_product(hidenum, i), M_io_hid_enum_serial(hidenum, i), 
				M_io_hid_enum_vendorid(hidenum, i), M_io_hid_enum_productid(hidenum, i));
	}
	M_io_hid_enum_destroy(hidenum);

}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *hid_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("hid");

	tc = tcase_create("hid");
	tcase_add_test(tc, check_hid);
	suite_add_tcase(suite, tc);
#if 0
	tc = tcase_create("ingenico");
	tcase_add_test(tc, check_ingenico);
	suite_add_tcase(suite, tc);
#endif
	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(hid_suite());
	srunner_set_log(sr, "check_hid.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
