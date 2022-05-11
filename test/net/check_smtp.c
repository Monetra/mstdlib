#include "m_config.h"
#include "check_smtp_json.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_net.h>
#include <mstdlib/mstdlib_text.h>

#define TESTPORT 52025

/* global so we parse only once */
M_json_node_t *check_smtp_json = NULL;

#define DEBUG 1

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

typedef enum {
	NO_ENDPOINTS = 1,
	EMU_SENDMSG,
	EMU_ACCEPT_DISCONNECT,
	IOCREATE_RETURN_FALSE,
	NO_SERVER,
	TLS_UNSUPPORTING_SERVER,
} test_id_t;


typedef enum {
	TLS_TYPE_NONE,
	TLS_TYPE_STARTTLS,
	TLS_TYPE_IMPLICIT
} tls_types_t;

typedef struct {
	tls_types_t    tls_type;
	M_uint16       port;
	M_json_node_t *json;
	M_list_str_t  *json_keys;
	M_list_str_t  *json_values;
	M_list_t      *regexs;
	M_event_t     *el;
	M_io_t        *io_listen;
	M_io_t        *io;
	M_buf_t       *out_buf;
	M_parser_t    *in_parser;
	M_bool         is_data_mode;
	M_bool         is_QUIT;
	test_id_t      test_id;
} smtp_emulator_t;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void smtp_emulator_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	smtp_emulator_t *emu     = thunk;
	M_io_t          *io_out  = NULL;
	char            *line    = NULL;
	const char      *eol     = "\r\n";
	const char      *eodata  = "\r\n.\r\n";
	const char      *ending  = NULL;
	const char      *str     = NULL;
	M_io_error_t     ioerr;
	(void)el;

	event_debug("smtp emulator %p event %s triggered", io, event_type_str(etype));

	switch(etype) {
		case M_EVENT_TYPE_READ:
			ioerr = M_io_read_into_parser(io, emu->in_parser);
			if (ioerr == M_IO_ERROR_DISCONNECT) {
				event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
				M_io_destroy(io);
				return;
			}
			if (M_parser_len(emu->in_parser) > 0) {
				event_debug("M_io_read_into_parser: %d:%.*s\n", M_parser_len(emu->in_parser),
						M_parser_len(emu->in_parser), (const char *)M_parser_peek(emu->in_parser));
			}
			break;
		case M_EVENT_TYPE_CONNECTED:
			M_parser_consume(emu->in_parser, M_parser_len(emu->in_parser));
			M_buf_truncate(emu->out_buf, M_buf_len(emu->out_buf));
			str = M_list_str_at(emu->json_values, 0);
			M_buf_add_str(emu->out_buf, str);
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			if (io == emu->io) {
				emu->io = NULL;
			}
			return;
			break;
		case M_EVENT_TYPE_WRITE:
		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_OTHER:
			break;
		case M_EVENT_TYPE_ACCEPT:
			if (emu->test_id == EMU_ACCEPT_DISCONNECT) {
				M_io_accept(&io_out, io);
				event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io_out);
				M_io_destroy(io_out);
				return;
			}
			ioerr = M_io_accept(&io_out, io);
			if (ioerr == M_IO_ERROR_WOULDBLOCK)
				return;
			if (emu->io != NULL) {
				event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, emu->io);
				M_io_destroy(emu->io);
			}
			emu->io = io_out;
			M_event_add(emu->el, emu->io, smtp_emulator_io_cb, emu);
			return;
	}

	if (emu->is_data_mode) {
		ending = eodata;
	} else {
		ending = eol;
	}
	if ((line = M_parser_read_strdup_until(emu->in_parser, ending, M_TRUE)) != NULL) {
		M_bool is_no_match;
		event_debug("smtp emulator %p READ %d bytes \"%s\"", io, M_str_len(line), line);
		if (emu->is_data_mode) {
			M_buf_add_str(emu->out_buf, M_list_str_at(emu->json_values, 1));
			emu->is_data_mode = M_FALSE;
		} else {
			if (M_str_eq(line, "DATA\r\n")) {
				emu->is_data_mode = M_TRUE;
			}
			if (M_str_eq(line, "QUIT\r\n")) {
				emu->is_QUIT = M_TRUE;
			}
			is_no_match = M_TRUE;
			for (size_t i = 0; i < M_list_len(emu->regexs); i++) {
				const M_re_t *re = M_list_at(emu->regexs, i);
				if (M_re_eq(re, line)) {
					is_no_match = M_FALSE;
					M_buf_add_str(emu->out_buf, M_list_str_at(emu->json_values, i));
					break;
				}
			}
			if (is_no_match) {
				M_buf_add_str(emu->out_buf, "502 \r\n");
			}
		}
		M_free(line);
	}

	if (M_buf_len(emu->out_buf) > 0) {
		size_t len = M_buf_len(emu->out_buf);
		event_debug("%s:%d: emu->out_buf: \"%s\"", __FILE__, __LINE__, M_buf_peek(emu->out_buf));
		ioerr = M_io_write_from_buf(io, emu->out_buf);
		if (ioerr == M_IO_ERROR_DISCONNECT) {
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			if (io == emu->io) {
				emu->io = NULL;
			}
			return;
		}
		event_debug("smtp emulator %p WRITE %d bytes", io, len - M_buf_len(emu->out_buf));
	} else {
		if (emu->is_QUIT) {
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			return;
		}
	}

}

static smtp_emulator_t *smtp_emulator_create(M_event_t *el, tls_types_t tls_type, const char *json_name,
		M_uint16 port, test_id_t test_id)
{
	smtp_emulator_t *emu      = M_malloc_zero(sizeof(*emu));

	emu->el = el;
	emu->test_id = test_id;
	emu->tls_type = tls_type;
	emu->port = port;
	emu->out_buf = M_buf_create();
	emu->in_parser = M_parser_create(M_PARSER_FLAG_NONE);
	emu->json = M_json_object_value(check_smtp_json, json_name);
	emu->json_keys = M_json_object_keys(emu->json);
	emu->json_values = M_list_str_create(M_LIST_NONE);
	emu->regexs = M_list_create(NULL, M_LIST_NONE);
	for (size_t i = 0; i < M_list_str_len(emu->json_keys); i++) {
		const char *key   = M_list_str_at(emu->json_keys, i);
		const char *value = M_json_object_value_string(emu->json, key);
		M_re_t *re        = M_re_compile(key, M_RE_UNGREEDY);
		M_list_insert(emu->regexs, re);
		M_list_str_insert(emu->json_values, value);
	}
	M_io_net_server_create(&emu->io_listen, emu->port, NULL, M_IO_NET_ANY);
	M_event_add(emu->el, emu->io_listen, smtp_emulator_io_cb, emu);
	return emu;
}

static void smtp_emulator_destroy(smtp_emulator_t *emu)
{
	M_json_node_destroy(emu->json);
	for (size_t i = 0; i < M_list_len(emu->regexs); i++) {
		M_re_t *re = M_list_take_first(emu->regexs);
		M_re_destroy(re);
	}
	M_list_destroy(emu->regexs, M_FALSE);
	M_list_str_destroy(emu->json_values);
	M_list_str_destroy(emu->json_keys);
	M_io_destroy(emu->io_listen);
	M_free(emu);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef struct {
	M_bool     is_success;
	M_bool     is_connect_cb_called;
	M_bool     is_connect_fail_cb_called;
	M_bool     is_disconnect_cb_called;
	M_bool     is_process_fail_cb_called;
	M_bool     is_processing_halted_cb_called;
	M_bool     is_sent_cb_called;
	M_bool     is_send_failed_cb_called;
	M_bool     is_reschedule_cb_called;
	M_bool     is_iocreate_cb_called;
	test_id_t  test_id;
	M_event_t *el;
} args_t;

static M_email_t * generate_email(size_t idx, const char *to_address)
{
	M_email_t *e;
	char msg[256];
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;
	M_time_t           ts;
	M_time_localtm_t   ltime;
	M_hash_dict_t     *headers;

	M_mem_set(&ltime, 0, sizeof(ltime));
	ts = M_time();
	tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_AMERICA, M_TIME_TZ_ALIAS_OLSON_MAIN, M_TIME_TZ_LOAD_LAZY);
	tz  = M_time_tzs_get_tz(tzs, "America/New_York");
	M_time_tolocal(ts, &ltime, tz);

	e = M_email_create();
	M_email_set_from(e, NULL, "smtp_cli", "no-reply+smtp-test@monetra.com");
	M_email_to_append(e, NULL, NULL, to_address);
	M_email_set_subject(e, "smtp_cli testing");
	M_snprintf(msg, sizeof(msg), "%04lld%02lld%02lld:%02lld%02lld%02lld, %zu\n", ltime.year, ltime.month, ltime.day, ltime.hour, ltime.min, ltime.sec, idx);
	headers = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);
	M_hash_dict_insert(headers, "Content-Type", "text/plain; charset=\"utf-8\"");
	M_hash_dict_insert(headers, "Content-Transfer-Encoding", "7bit");
	M_email_part_append(e, msg, M_str_len(msg), headers, NULL);
	M_hash_dict_destroy(headers);
	return e;
}

static void connect_cb(const char *address, M_uint16 port, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_connect_cb(\"%s\", %u, %p)", address, port, thunk);
	args->is_connect_cb_called = M_TRUE;
}

static M_bool connect_fail_cb(const char *address, M_uint16 port, M_net_error_t net_err,
		const char *error, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_connect_fail_cb(\"%s\", %u, %s, \"%s\", %p)", address, port,
			M_net_errcode_to_str(net_err), error, thunk);
	if (args->test_id == NO_SERVER || args->test_id == TLS_UNSUPPORTING_SERVER) {
		if (args->is_connect_fail_cb_called) {
			/* Second Time */
			return M_TRUE; /* Remove endpoint */
		}
	}
	args->is_connect_fail_cb_called = M_TRUE;
	return M_FALSE; /* Should TCP endpoint be removed? */
}

static void disconnect_cb(const char *address, M_uint16 port, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_disconnect_cb(\"%s\", %u, %p)", address, port, thunk);
	args->is_disconnect_cb_called = M_TRUE;
}

static M_bool process_fail_cb(const char *command, int result_code, const char *proc_stdout,
		const char *proc_stderr, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_process_fail(\"%s\", %d, \"%s\", \"%s\", %p)", command, result_code, proc_stdout, proc_stderr, thunk);
	args->is_process_fail_cb_called = M_TRUE;
	return M_FALSE; /* Should process endpoint be removed? */
}

static M_uint64 processing_halted_cb(M_bool no_endpoints, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_processing_halted_cb(%s, %p)", no_endpoints ? "M_TRUE" : "M_FALSE", thunk);
	args->is_processing_halted_cb_called = M_TRUE;
	if (args->test_id == NO_SERVER || args->test_id == TLS_UNSUPPORTING_SERVER) {
		M_event_done(args->el);
	}
	if (args->test_id == NO_ENDPOINTS) {
		args->is_success = (no_endpoints == M_TRUE);
	}
	return 0;
}

static void sent_cb(const M_hash_dict_t *headers, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_sent_cb(%p, %p)", headers, thunk);
	args->is_sent_cb_called = M_TRUE;
	if (args->test_id == EMU_SENDMSG) {
		M_event_done(args->el);
	}
}

static M_bool send_failed_cb(const M_hash_dict_t *headers, const char *error, size_t attempt_num,
		M_bool can_requeue, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_send_failed_cb(%p, \"%s\", %zu, %s, %p)\n", headers, error, attempt_num, can_requeue ? "M_TRUE" : "M_FALSE", thunk);
	args->is_send_failed_cb_called = M_TRUE;
	if (args->test_id == EMU_ACCEPT_DISCONNECT) {
		M_event_done(args->el);
	}
	return M_FALSE; /* should msg be requeued? */
}

static void reschedule_cb(const char *msg, M_uint64 wait_sec, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_reschedule_cb(\"%s\", %zu, %p)", msg, wait_sec, thunk);
	args->is_reschedule_cb_called = M_TRUE;
}

static M_bool iocreate_cb(M_io_t *io, char *error, size_t errlen, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_iocreate_cb(%p, %p, %zu, %p)\n", io, error, errlen, thunk);
	if (args->test_id == IOCREATE_RETURN_FALSE) {
		if (args->is_iocreate_cb_called) {
			/* called once before */
			event_debug("M_event_done(%p)\n", args->el);
			M_event_done(args->el);
		}
		args->is_iocreate_cb_called = M_TRUE;
		event_debug("M_net_smtp_iocreate_cb(): return M_FALSE");
		return M_FALSE;
	}
	args->is_iocreate_cb_called = M_TRUE;
	return M_TRUE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(tls_unsupporting_server)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = connect_cb,
		.connect_fail_cb      = connect_fail_cb,
		.disconnect_cb        = disconnect_cb,
		.process_fail_cb      = process_fail_cb,
		.processing_halted_cb = processing_halted_cb,
		.sent_cb              = sent_cb,
		.send_failed_cb       = send_failed_cb,
		.reschedule_cb        = reschedule_cb,
		.iocreate_cb          = iocreate_cb,
	};

	args_t args = {
		.is_connect_cb_called           = M_FALSE,
		.is_connect_fail_cb_called      = M_FALSE,
		.is_disconnect_cb_called        = M_FALSE,
		.is_process_fail_cb_called      = M_FALSE,
		.is_processing_halted_cb_called = M_FALSE,
		.is_sent_cb_called              = M_FALSE,
		.is_send_failed_cb_called       = M_FALSE,
		.is_reschedule_cb_called        = M_FALSE,
		.is_iocreate_cb_called          = M_FALSE,
		.test_id                        = TLS_UNSUPPORTING_SERVER,
		.el                             = NULL,
	};

	M_event_t         *el  = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", TESTPORT, args.test_id);
	M_net_smtp_t      *sp  = M_net_smtp_create(el, &cbs, &args);
	M_dns_t           *dns = M_dns_create(el);
	M_email_t         *e   = generate_email(1, "anybody@localhost");
	M_tls_clientctx_t *ctx = M_tls_clientctx_create();

		M_tls_clientctx_set_default_trust(ctx);
		M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, ctx);
		M_tls_clientctx_destroy(ctx);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_TRUE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, 1000);

	ck_assert_msg(args.is_connect_fail_cb_called == M_TRUE, "should have called connect_fail_cb");
	ck_assert_msg(args.is_processing_halted_cb_called == M_TRUE, "should have called processing_halted_cb");

	smtp_emulator_destroy(emu);
	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
}
END_TEST

START_TEST(no_server)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = connect_cb,
		.connect_fail_cb      = connect_fail_cb,
		.disconnect_cb        = disconnect_cb,
		.process_fail_cb      = process_fail_cb,
		.processing_halted_cb = processing_halted_cb,
		.sent_cb              = sent_cb,
		.send_failed_cb       = send_failed_cb,
		.reschedule_cb        = reschedule_cb,
		.iocreate_cb          = iocreate_cb,
	};

	args_t args = {
		.is_connect_cb_called           = M_FALSE,
		.is_connect_fail_cb_called      = M_FALSE,
		.is_disconnect_cb_called        = M_FALSE,
		.is_process_fail_cb_called      = M_FALSE,
		.is_processing_halted_cb_called = M_FALSE,
		.is_sent_cb_called              = M_FALSE,
		.is_send_failed_cb_called       = M_FALSE,
		.is_reschedule_cb_called        = M_FALSE,
		.is_iocreate_cb_called          = M_FALSE,
		.test_id                        = NO_SERVER,
		.el                             = NULL,
	};

	M_event_t       *el  = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t    *sp  = M_net_smtp_create(el, &cbs, &args);
	M_dns_t         *dns = M_dns_create(el);
	M_email_t       *e   = generate_email(1, "anybody@localhost");

	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, 1000);

	ck_assert_msg(args.is_connect_fail_cb_called == M_TRUE, "should have called connect_fail_cb");
	ck_assert_msg(args.is_processing_halted_cb_called == M_TRUE, "should have called processing_halted_cb");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
}
END_TEST

START_TEST(iocreate_return_false)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = connect_cb,
		.connect_fail_cb      = connect_fail_cb,
		.disconnect_cb        = disconnect_cb,
		.process_fail_cb      = process_fail_cb,
		.processing_halted_cb = processing_halted_cb,
		.sent_cb              = sent_cb,
		.send_failed_cb       = send_failed_cb,
		.reschedule_cb        = reschedule_cb,
		.iocreate_cb          = iocreate_cb,
	};

	args_t args = {
		.is_connect_cb_called           = M_FALSE,
		.is_connect_fail_cb_called      = M_FALSE,
		.is_disconnect_cb_called        = M_FALSE,
		.is_process_fail_cb_called      = M_FALSE,
		.is_processing_halted_cb_called = M_FALSE,
		.is_sent_cb_called              = M_FALSE,
		.is_send_failed_cb_called       = M_FALSE,
		.is_reschedule_cb_called        = M_FALSE,
		.is_iocreate_cb_called          = M_FALSE,
		.test_id                        = IOCREATE_RETURN_FALSE,
		.el                             = NULL,
	};

	M_event_t       *el  = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", TESTPORT, args.test_id);
	M_net_smtp_t    *sp  = M_net_smtp_create(el, &cbs, &args);
	M_dns_t         *dns = M_dns_create(el);
	M_email_t       *e   = generate_email(1, "anybody@localhost");

	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, 1000);

	ck_assert_msg(args.is_iocreate_cb_called == M_TRUE, "should have called iocreate_cb");
	ck_assert_msg(args.is_connect_cb_called == M_FALSE, "shouldn't have called send_failed_cb");
	ck_assert_msg(args.is_disconnect_cb_called == M_FALSE, "shouldn't have called disconnect_cb");
	ck_assert_msg(args.is_sent_cb_called == M_FALSE, "shouldn't have called send_failed_cb");
	ck_assert_msg(args.is_send_failed_cb_called == M_FALSE, "shouldn't have called send_failed_cb");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
}
END_TEST

START_TEST(emu_accept_disconnect)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = NULL,
		.connect_fail_cb      = NULL,
		.disconnect_cb        = NULL,
		.process_fail_cb      = NULL,
		.processing_halted_cb = NULL,
		.sent_cb              = NULL,
		.send_failed_cb       = send_failed_cb,
		.reschedule_cb        = NULL,
		.iocreate_cb          = NULL,
	};

	args_t args = {
		.is_send_failed_cb_called = M_FALSE,
		.test_id = EMU_ACCEPT_DISCONNECT,
		.el = NULL,
	};

	M_event_t       *el  = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", TESTPORT, args.test_id);
	M_net_smtp_t    *sp  = M_net_smtp_create(el, &cbs, &args);
	M_dns_t         *dns = M_dns_create(el);
	M_email_t       *e   = generate_email(1, "anybody@localhost");

	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_send_failed_cb_called, "should have called send_failed_cb");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
}
END_TEST

START_TEST(emu_sendmsg)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = connect_cb,
		.connect_fail_cb      = NULL,
		.disconnect_cb        = NULL,
		.process_fail_cb      = NULL,
		.processing_halted_cb = NULL,
		.sent_cb              = sent_cb,
		.send_failed_cb       = NULL,
		.reschedule_cb        = NULL,
		.iocreate_cb          = iocreate_cb,
	};

	args_t args = {
		.is_success             = M_FALSE,
		.is_iocreate_cb_called  = M_FALSE,
		.is_sent_cb_called      = M_FALSE,
		.is_connect_cb_called   = M_FALSE,
		.test_id                = EMU_SENDMSG,
		.el                     = NULL,
	};

	M_event_t       *el  = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", TESTPORT, args.test_id);
	M_net_smtp_t    *sp  = M_net_smtp_create(el, &cbs, &args);
	M_dns_t         *dns = M_dns_create(el);
	M_email_t       *e   = generate_email(1, "anybody@localhost");
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_FALSE, "user", "pass", 1) == M_FALSE,
			"should fail adding tcp endpoint without setting dns");

	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", TESTPORT, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_iocreate_cb_called, "should have called iocreate_cb");
	ck_assert_msg(args.is_connect_cb_called, "should have called connect_cb");
	ck_assert_msg(args.is_sent_cb_called, "should have called sent_cb");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
}
END_TEST


START_TEST(check_no_endpoints)
{
	struct M_net_smtp_callbacks cbs  = {
		.connect_cb           = NULL,
		.connect_fail_cb      = NULL,
		.disconnect_cb        = NULL,
		.process_fail_cb      = NULL,
		.processing_halted_cb = processing_halted_cb,
		.sent_cb              = NULL,
		.send_failed_cb       = NULL,
		.reschedule_cb        = NULL,
		.iocreate_cb          = NULL,
	};

	args_t args = {
		.is_success = M_FALSE,
		.test_id = NO_ENDPOINTS,
	};

	M_event_t *el = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t *sp = M_net_smtp_create(el, &cbs, &args);
	ck_assert_msg(M_net_smtp_resume(sp) == M_FALSE, "should fail with no endpoints");
	ck_assert_msg(args.is_success, "should trigger processing_halted_cb with no endpoints");
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *smtp_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("smtp");

	tc = tcase_create("no-endpoints");
	tcase_add_test(tc, check_no_endpoints);
	suite_add_tcase(suite, tc);

	tc = tcase_create("emu-sendmsg");
	tcase_add_test(tc, emu_sendmsg);
	tcase_set_timeout(tc, 1);
	suite_add_tcase(suite, tc);

	tc = tcase_create("emu-accept-disconnect");
	tcase_add_test(tc, emu_accept_disconnect);
	tcase_set_timeout(tc, 1);
	suite_add_tcase(suite, tc);

	tc = tcase_create("iocreate_return_false");
	tcase_add_test(tc, iocreate_return_false);
	tcase_set_timeout(tc, 1);
	suite_add_tcase(suite, tc);

	tc = tcase_create("no server");
	tcase_add_test(tc, no_server);
	tcase_set_timeout(tc, 1);
	suite_add_tcase(suite, tc);

	tc = tcase_create("tls unsupporting server");
	tcase_add_test(tc, tls_unsupporting_server);
	tcase_set_timeout(tc, 1);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	check_smtp_json = M_json_read(json_str, M_str_len(json_str), M_JSON_READER_NONE, NULL, NULL, NULL, NULL);

	sr = srunner_create(smtp_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_smtp.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	M_json_node_destroy(check_smtp_json);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
