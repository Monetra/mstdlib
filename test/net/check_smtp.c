#include "m_config.h"
#include "check_smtp_json.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_net.h>
#include <mstdlib/mstdlib_text.h>
#include <mstdlib/io/m_io_layer.h>

#include "../../io/m_io_int.h"

#define DEBUG 0

/* globals */
M_json_node_t     *check_smtp_json          = NULL;
char              *test_address             = NULL;
char              *sendmail_emu             = NULL;
M_list_str_t      *test_external_queue      = NULL;
M_tls_serverctx_t *test_serverctx           = NULL;
const size_t       multithread_insert_count = 100;
const size_t       multithread_retry_count  = 10;

typedef enum {
	NO_ENDPOINTS            = 1,
	EMU_SENDMSG             = 2,
	EMU_ACCEPT_DISCONNECT   = 3,
	IOCREATE_RETURN_FALSE   = 4,
	NO_SERVER               = 5,
	TLS_UNSUPPORTING_SERVER = 6,
	TIMEOUTS                = 7,
	TIMEOUT_CONNECT         = 8,
	TIMEOUT_STALL           = 9,
	TIMEOUT_IDLE            = 10,
	STATUS                  = 11,
	PROC_ENDPOINT           = 12,
	DOT_MSG                 = 13,
	PROC_NOT_FOUND          = 14,
	HALT_RESTART            = 15,
	EXTERNAL_QUEUE          = 16,
	JUNK_MSG                = 17,
	DUMP_QUEUE              = 18,
	MULTITHREAD_INSERT      = 19,
	MULTITHREAD_RETRY       = 20,
	AUTH_LOGIN              = 21,
	AUTH_PLAIN              = 22,
	AUTH_CRAM_MD5           = 23,
	AUTH_DIGEST_MD5         = 24,
	STARTTLS                = 25,
	IMPLICIT_TLS            = 26,
	DUMMY_CHECKS            = 27,
	BAD_SERVER              = 28,
	BAD_SERVER_2            = 29,
	REACTIVATE_IDLE         = 30,
	DEFAULT_CBS             = 31,
	BCC_TEST                = 32,
	BAD_AUTH                = 33,
	PROCESS_ERROR_EXIT      = 34,
	DOMAIN_MISMATCH         = 35,
	MAX_ATTEMPTS_0          = 36,
} test_id_t;

#define TESTONLY 0

static void cleanup(void)
{
	M_json_node_destroy(check_smtp_json);
	M_free(test_address);
	M_free(sendmail_emu);
	M_tls_serverctx_destroy(test_serverctx);
	M_library_cleanup();
}

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
	TLS_TYPE_NONE,
	TLS_TYPE_STARTTLS,
	TLS_TYPE_IMPLICIT
} tls_types_t;

typedef struct {
	tls_types_t        tls_type;
	M_tls_serverctx_t *serverctx;
	M_uint16           port;
	M_json_node_t     *json;
	M_list_str_t      *json_keys;
	M_list_str_t      *json_values;
	M_list_t          *regexs;
	M_event_t         *el;
	M_io_t            *io_listen;
	M_io_t            *starttls_io;
	test_id_t          test_id;
	M_io_t            *stall_io;
	const char        *CONNECTED_str;
	const char        *DATA_ACK_str;
	const char        *DIGEST_MD5_str;
	struct {
		M_io_t     *io;
		M_buf_t    *out_buf;
		M_parser_t *in_parser;
		M_bool      is_data_mode;
		M_bool      is_QUIT;
		M_bool      is_STARTTLS;
		int         sttls_state;
	} conn[16];
} smtp_emulator_t;
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void smtp_emulator_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	smtp_emulator_t *emu          = thunk;
	char            *line         = NULL;
	const char      *eol          = "\r\n";
	const char      *eodata       = "\r\n.\r\n";
	const char      *ending       = NULL;
	M_parser_t      *in_parser    = NULL;
	M_buf_t         *out_buf      = NULL;
	M_io_t         **emu_io       = NULL;
	M_bool          *is_data_mode = NULL;
	M_bool          *is_QUIT      = NULL;
	M_bool          *is_STARTTLS  = NULL;
	int             *sttls_state  = NULL;
	M_io_error_t     ioerr;
	static int       error_count  = 0;
	(void)el;

	if (emu->test_id == TIMEOUT_CONNECT && etype == M_EVENT_TYPE_ACCEPT) 
		return;

	event_debug("smtp emulator:%d io:%p event %s triggered", emu->test_id, io, event_type_str(etype));

	if (etype == M_EVENT_TYPE_ACCEPT) {
		size_t i;
		if (emu->test_id == EMU_ACCEPT_DISCONNECT) {
			M_io_t *io_out = NULL;
			ioerr = M_io_accept(&io_out, io);
			if (ioerr == M_IO_ERROR_WOULDBLOCK) { return; }
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io_out);
			M_io_destroy(io_out);
			return;
		}
		for (i = 0; i < ARRAY_LEN(emu->conn); i++) {
			if (emu->conn[i].io == NULL) { break; }
		}
		if (i == ARRAY_LEN(emu->conn)) {
			M_printf("Emulator ran out of connections!");
			exit(1);
		}
		ioerr = M_io_accept(&emu->conn[i].io, io);
		if (ioerr == M_IO_ERROR_WOULDBLOCK) {
			emu->conn[i].io = NULL;
			return;
		}
		if (emu->test_id == TIMEOUT_STALL) {
			emu->stall_io = emu->conn[i].io;
		}
		M_event_add(emu->el, emu->conn[i].io, smtp_emulator_io_cb, emu);
		return;
	}

	for (size_t i = 0; i < ARRAY_LEN(emu->conn); i++) {
		if (emu->conn[i].io == io) {
			emu_io = &emu->conn[i].io;
			in_parser = emu->conn[i].in_parser;
			out_buf = emu->conn[i].out_buf;
			is_QUIT = &emu->conn[i].is_QUIT;
			is_STARTTLS = &emu->conn[i].is_STARTTLS;
			sttls_state = &emu->conn[i].sttls_state;
			is_data_mode = &emu->conn[i].is_data_mode;
		}
	}

	switch(etype) {
		case M_EVENT_TYPE_READ:
			if (*sttls_state == 2)
				return;
			ioerr = M_io_read_into_parser(io, in_parser);
			if (ioerr == M_IO_ERROR_DISCONNECT) {
				event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
				M_io_destroy(io);
				*emu_io = NULL;
				return;
			}
			if (M_parser_len(in_parser) > 0) {
#if DEBUG == 2
				event_debug("M_io_read_into_parser: %d:%.*s\n", M_parser_len(in_parser),
						M_parser_len(in_parser), (const char *)M_parser_peek(in_parser));
#endif
			} else {
				if (emu->test_id == STARTTLS) {
					error_count++;
					if (error_count >= 100) {
						exit(0);
					}
				}
			}
			break;
		case M_EVENT_TYPE_CONNECTED:
			if (*is_STARTTLS) {
				*is_STARTTLS = M_FALSE;
				*sttls_state = 3;
				return;
			}
			*sttls_state = 0;
			if (M_parser_len(in_parser) > 0) {
				M_parser_consume(in_parser, M_parser_len(in_parser));
			}
			M_buf_truncate(out_buf, M_buf_len(out_buf));
			M_buf_add_str(out_buf, emu->CONNECTED_str);
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			*emu_io = NULL;
			return;
			break;
		case M_EVENT_TYPE_WRITE:
		case M_EVENT_TYPE_ERROR:
			if (*sttls_state == 2)
				return;
			if (etype == M_EVENT_TYPE_WRITE && emu->test_id == TIMEOUT_STALL) {
				return;
			}
			if (emu->test_id == STARTTLS) {
				error_count++;
				if (error_count >= 100) {
					exit(0);
				}
			}
		case M_EVENT_TYPE_OTHER:
			if (*sttls_state == 2)
				return;
			break;
		case M_EVENT_TYPE_ACCEPT:
			return; /* Already handled */
	}

	if (*is_data_mode) {
		ending = eodata;
	} else {
		ending = eol;
	}
	if ((line = M_parser_read_strdup_until(in_parser, ending, M_TRUE)) != NULL) {
		M_bool is_no_match;
#if DEBUG == 2
		event_debug("smtp emulator %p READ %d bytes \"%s\"", io, M_str_len(line), line);
#endif
		if (*is_data_mode) {
			M_buf_add_str(out_buf, emu->DATA_ACK_str);
			*is_data_mode = M_FALSE;
		} else {
			if (M_str_eq(line, "DATA\r\n")) {
				*is_data_mode = M_TRUE;
			}
			if (M_str_eq(line, "STARTTLS\r\n") && emu->test_id != BAD_SERVER) {
				*is_STARTTLS = M_TRUE;
				*sttls_state = 1;
			}
			if (M_str_eq(line, "QUIT\r\n")) {
				*is_QUIT = M_TRUE;
			}
			is_no_match = M_TRUE;
			if (M_str_len(line) == 278) {
				M_buf_add_str(out_buf, emu->DIGEST_MD5_str);
				is_no_match = M_FALSE;
			}
			for (size_t i = 0; i < M_list_len(emu->regexs); i++) {
				const M_re_t *re = M_list_at(emu->regexs, i);
				if (M_re_eq(re, line)) {
					is_no_match = M_FALSE;
					M_buf_add_str(out_buf, M_list_str_at(emu->json_values, i));
					break;
				}
			}
			if (is_no_match) {
				M_buf_add_str(out_buf, "502 \r\n");
			}
		}
		M_free(line);
	}

	if (M_buf_len(out_buf) > 0) {
		size_t len = M_buf_len(out_buf);
#if DEBUG == 2
		event_debug("%s:%d: emu->out_buf: \"%s\"", __FILE__, __LINE__, M_buf_peek(out_buf));
#endif
		if (emu->test_id == TIMEOUT_STALL) {
			char byte;
			size_t n;
			byte = M_buf_peek(out_buf)[0];
			M_buf_drop(out_buf, 1);
			ioerr = M_io_write(emu->stall_io, (const unsigned char *)&byte, 1, &n);
			if (ioerr != M_IO_ERROR_DISCONNECT && n != 1) {
				M_event_timer_oneshot(el, 30, M_TRUE, smtp_emulator_io_cb, thunk);
			}
			event_debug("smtp emulator io:%p WRITE %d bytes", io, n);
			return;
		}
		ioerr = M_io_write_from_buf(io, out_buf);
		if (ioerr == M_IO_ERROR_DISCONNECT) {
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			*emu_io = NULL;
			return;
		}
		event_debug("smtp emulator io:%p WRITE %d bytes", io, len - M_buf_len(out_buf));
	} else {
		if (*is_QUIT) {
			event_debug("%s:%d: smtp emulator M_io_destroy(%p)", __FILE__, __LINE__, io);
			M_io_destroy(io);
			*emu_io = NULL;
			*is_QUIT = M_FALSE;
			return;
		}
		if (*is_STARTTLS && *sttls_state == 1) {
			event_debug("M_io_tls_server_add(%p)\n", io);
			M_io_tls_server_add(io, emu->serverctx, NULL);
			*sttls_state = 2;
		}
	}

}

static void smtp_emulator_switch(smtp_emulator_t *emu, const char *json_name)
{
	if (emu->regexs != NULL) {
		M_re_t *re;
		while ((re = M_list_take_last(emu->regexs)) != NULL) {
			M_re_destroy(re);
		}
		M_list_destroy(emu->regexs, M_FALSE);
	}
	M_list_str_destroy(emu->json_values);
	M_list_str_destroy(emu->json_keys);

	emu->json = M_json_object_value(check_smtp_json, json_name);
	emu->json_keys = M_json_object_keys(emu->json);
	emu->json_values = M_list_str_create(M_LIST_NONE);
	emu->regexs = M_list_create(NULL, M_LIST_NONE);
	for (size_t i = 0; i < M_list_str_len(emu->json_keys); i++) {
		const char *key   = M_list_str_at(emu->json_keys, i);
		const char *value = M_json_object_value_string(emu->json, key);
		M_re_t     *re    = NULL;
		if (M_str_eq(key, "CONNECTED")) {
			emu->CONNECTED_str = M_json_object_value_string(emu->json, key);
			continue;
		}
		if (M_str_eq(key, "DATA_ACK")) {
			emu->DATA_ACK_str = M_json_object_value_string(emu->json, key);
			continue;
		}
		if (M_str_eq(key, "DIGEST-MD5")) {
			emu->DIGEST_MD5_str = M_json_object_value_string(emu->json, key);
			continue;
		}
		re = M_re_compile(key, M_RE_UNGREEDY);
		M_list_insert(emu->regexs, re);
		M_list_str_insert(emu->json_values, value);
	}
}

static M_io_t *net_server_create_search(M_uint16 *port)
{
	M_io_error_t  ioerr;
	M_io_t       *io;
	while ((ioerr = M_io_net_server_create(&io, *port, NULL, M_IO_NET_ANY)) == M_IO_ERROR_ADDRINUSE) {
		M_uint16 newport = (M_uint16)M_rand_range(NULL, 10000, 50000);
		event_debug("Port %d in use, switching to new port %d", (int)*port, (int)newport);
		*port = newport;
	}
	return io;
}

static smtp_emulator_t *smtp_emulator_create_tls(M_event_t *el, tls_types_t tls_type, const char *json_name,
		M_uint16 *testport, test_id_t test_id, M_tls_serverctx_t *serverctx)
{
	smtp_emulator_t *emu      = M_malloc_zero(sizeof(*emu));
	M_uint16         port     = (M_uint16)M_rand_range(NULL, 10000, 50000);

	emu->el = el;
	M_tls_serverctx_upref(serverctx);
	emu->serverctx = serverctx;
	emu->test_id = test_id;
	emu->tls_type = tls_type;
	smtp_emulator_switch(emu, json_name);
	emu->io_listen = net_server_create_search(&port);
	emu->port = port;
	*testport = port;
	if (emu->tls_type == TLS_TYPE_IMPLICIT) {
		M_io_tls_server_add(emu->io_listen, emu->serverctx, NULL);
	}
	M_event_add(emu->el, emu->io_listen, smtp_emulator_io_cb, emu);

	for (size_t i = 0; i < ARRAY_LEN(emu->conn); i++) {
		emu->conn[i].io           = NULL;
		emu->conn[i].out_buf      = M_buf_create();
		emu->conn[i].in_parser    = M_parser_create(M_PARSER_FLAG_NONE);
		emu->conn[i].is_data_mode = M_FALSE;
		emu->conn[i].is_QUIT      = M_FALSE;
	}
	return emu;
}

static smtp_emulator_t *smtp_emulator_create(M_event_t *el, tls_types_t tls_type, const char *json_name,
		M_uint16 *testport, test_id_t test_id)
{
	return smtp_emulator_create_tls(el, tls_type, json_name, testport, test_id, NULL);
}

static void smtp_emulator_destroy(smtp_emulator_t *emu)
{
	M_re_t *re;
	while ((re = M_list_take_last(emu->regexs)) != NULL) {
		M_re_destroy(re);
	}
	M_list_destroy(emu->regexs, M_FALSE);
	if (emu->serverctx) {
		M_tls_serverctx_destroy(emu->serverctx);
	}
	M_list_str_destroy(emu->json_values);
	M_list_str_destroy(emu->json_keys);
	M_io_destroy(emu->io_listen);
	for (size_t i = 0; i < ARRAY_LEN(emu->conn); i++) {
		M_buf_cancel(emu->conn[i].out_buf);
		M_parser_destroy(emu->conn[i].in_parser);
		M_io_destroy(emu->conn[i].io);
		emu->conn[i].io = NULL;
		emu->conn[i].out_buf = NULL;
		emu->conn[i].in_parser = NULL;
	}
	M_free(emu);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef struct {
	M_bool           is_success;
	M_bool           is_connect_cb_called;
	M_bool           is_connect_fail_cb_called;
	M_bool           is_disconnect_cb_called;
	M_bool           is_process_fail_cb_called;
	M_bool           is_processing_halted_cb_called;
	M_bool           is_sent_cb_called;
	M_bool           is_send_failed_cb_called;
	M_bool           is_reschedule_cb_called;
	M_bool           is_iocreate_cb_called;
	M_uint64         connect_cb_call_count;
	M_uint64         connect_fail_cb_call_count;
	M_uint64         disconnect_cb_call_count;
	M_uint64         process_fail_cb_call_count;
	M_uint64         processing_halted_cb_call_count;
	M_uint64         sent_cb_call_count;
	M_uint64         send_failed_cb_call_count;
	M_uint64         reschedule_cb_call_count;
	M_uint64         iocreate_cb_call_count;
	test_id_t        test_id;
	M_event_t       *el;
	M_net_smtp_t    *sp;
	smtp_emulator_t *emu;
} args_t;

static M_email_t * generate_email_with_text(const char *to_address, const char *text)
{
	M_email_t     *e;
	M_hash_dict_t *headers;

	e = M_email_create();
	M_email_set_from(e, NULL, "smtp_cli", "no-reply+smtp-test@monetra.com");
	M_email_to_append(e, NULL, NULL, to_address);
	M_email_set_subject(e, "Testing");
	headers = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);
	M_hash_dict_insert(headers, "Content-Type", "text/plain; charset=\"utf-8\"");
	M_hash_dict_insert(headers, "Content-Transfer-Encoding", "7bit");
	if (text != NULL) {
		M_email_part_append(e, text, M_str_len(text), headers, NULL);
	}
	M_hash_dict_destroy(headers);
	return e;
}

static M_email_t * generate_email(size_t idx, const char *to_address)
{
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;
	M_time_t           ts;
	M_time_localtm_t   ltime;
	char msg[256];

	M_mem_set(&ltime, 0, sizeof(ltime));
	ts = M_time();
	tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_AMERICA, M_TIME_TZ_ALIAS_OLSON_MAIN, M_TIME_TZ_LOAD_LAZY);
	tz  = M_time_tzs_get_tz(tzs, "America/New_York");
	M_time_tolocal(ts, &ltime, tz);

	M_snprintf(msg, sizeof(msg), "%04lld%02lld%02lld:%02lld%02lld%02lld, %zu\n", ltime.year, ltime.month, ltime.day, ltime.hour, ltime.min, ltime.sec, idx);

	M_time_tzs_destroy(tzs);
	return generate_email_with_text(to_address, msg);
}

static void connect_cb(const char *address, M_uint16 port, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_connect_cb(\"%s\", %u, %p)", address, port, thunk);
	args->is_connect_cb_called = M_TRUE;
	args->connect_cb_call_count++;
}

static M_bool connect_fail_cb(const char *address, M_uint16 port, M_net_error_t net_err,
		const char *error, void *thunk)
{
	args_t *args = thunk;
	args->is_connect_fail_cb_called = M_TRUE;
	args->connect_fail_cb_call_count++;
	event_debug("M_net_smtp_connect_fail_cb(\"%s\", %u, %s, \"%s\", %p)", address, port,
			M_net_errcode_to_str(net_err), error, thunk);
	if (args->test_id == NO_SERVER || args->test_id == TLS_UNSUPPORTING_SERVER) {
		if (args->connect_fail_cb_call_count == 2) {
			return M_TRUE; /* Remove endpoint */
		}
	}
	if (args->test_id == DOMAIN_MISMATCH) {
		return M_TRUE;
	}
	if (args->test_id == REACTIVATE_IDLE) {
		return M_TRUE;
	}
	if (args->test_id == AUTH_PLAIN || args->test_id == AUTH_LOGIN || args->test_id == AUTH_CRAM_MD5 || args->test_id == AUTH_DIGEST_MD5) {
		M_event_done(args->el);
	}
	if (args->test_id == TIMEOUTS) {
		return M_TRUE;
	}
	if (args->test_id == BAD_SERVER || args->test_id == BAD_AUTH) {
		return M_TRUE;
	}
	return M_FALSE; /* Should TCP endpoint be removed? */
}

static void disconnect_cb(const char *address, M_uint16 port, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_disconnect_cb(\"%s\", %u, %p)", address, port, thunk);
	args->is_disconnect_cb_called = M_TRUE;
	args->disconnect_cb_call_count++;
	if (args->test_id == TIMEOUTS && args->sent_cb_call_count >= 3) {
		event_debug("TIMEOUTS: M_event_done(%p) (%d >= 3)\n", args->el, args->sent_cb_call_count);
		M_event_done(args->el);
	}
}

static M_bool process_fail_cb(const char *command, int result_code, const char *proc_stdout,
		const char *proc_stderr, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_process_fail(\"%s\", %d, \"%s\", \"%s\", %p)", command, result_code, proc_stdout, proc_stderr, thunk);
	args->is_process_fail_cb_called = M_TRUE;
	args->process_fail_cb_call_count++;

	if (args->test_id == PROCESS_ERROR_EXIT) {
		M_event_done(args->el);
	}

	return M_TRUE; /* Should process endpoint be removed? */
}

static M_uint64 processing_halted_cb(M_bool no_endpoints, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_processing_halted_cb(%s, %p)", no_endpoints ? "M_TRUE" : "M_FALSE", thunk);
	args->is_processing_halted_cb_called = M_TRUE;
	args->processing_halted_cb_call_count++;
	if (args->test_id == NO_SERVER || args->test_id == TLS_UNSUPPORTING_SERVER) {
		M_event_done(args->el);
	}
	if (args->test_id == DOMAIN_MISMATCH) {
		M_event_done(args->el);
	}
	if (args->test_id == REACTIVATE_IDLE) {
		M_event_done(args->el);
	}
	if (args->test_id == NO_ENDPOINTS) {
		args->is_success = (no_endpoints == M_TRUE);
	}
	if (args->test_id == HALT_RESTART) {
		return 10; /* restart in 10ms */
	}
	if (args->test_id == BAD_SERVER || args->test_id == BAD_AUTH) {
		M_event_done(args->el);
		return 0;
	}

	if (args->test_id == PROC_NOT_FOUND) {
		args->is_success = no_endpoints;
		M_event_done(args->el);
	}
	return 0;
}

static void sent_cb(const M_hash_dict_t *headers, void *thunk)
{
	args_t *args = thunk;
	args->is_sent_cb_called = M_TRUE;
	args->sent_cb_call_count++;
	event_debug("M_net_smtp_sent_cb(%p, %p): %llu (failed: %llu) (connfail: %llu) (args->test_id: %d) (EMU_SENDMSG: %d)", headers, thunk, args->sent_cb_call_count, args->send_failed_cb_call_count, args->connect_fail_cb_call_count, args->test_id, EMU_SENDMSG);
	if (args->test_id == EMU_SENDMSG || args->test_id == AUTH_PLAIN || args->test_id == AUTH_LOGIN || args->test_id == AUTH_CRAM_MD5 || args->test_id == AUTH_DIGEST_MD5 || args->test_id == IMPLICIT_TLS || args->test_id == STARTTLS) {
		event_debug("M_event_done()");
		M_event_done(args->el);
	}

	if (args->test_id == BCC_TEST) {
		args->is_success = M_hash_dict_get(headers, "BCC", NULL) == M_FALSE;
		M_event_done(args->el);
	}

	if (args->test_id == REACTIVATE_IDLE) {
		if (args->sent_cb_call_count == 3 || args->sent_cb_call_count == 6 || args->sent_cb_call_count == 7) {
			M_event_done(args->el);
		}
	}

	if (args->test_id == MULTITHREAD_RETRY) {
		/*const char *subject;
		M_printf("before\n");
		M_hash_dict_get(headers, "subject", &subject);
		M_printf("subject: %p\n", subject);
		*/
		if (args->sent_cb_call_count == multithread_retry_count) {
			M_event_done(args->el);
		}
	}

	if (args->test_id == MULTITHREAD_INSERT) {
		if (args->sent_cb_call_count == multithread_insert_count) {
			M_event_done(args->el);
		}
	}

	if (args->test_id == EXTERNAL_QUEUE) {
		M_event_done(args->el);
	}

	if (args->test_id == HALT_RESTART) {
		M_event_done(args->el);
	}

	if (args->test_id == DOT_MSG) {
		if ((args->sent_cb_call_count == 2 && args->send_failed_cb_call_count == 1) || args->sent_cb_call_count == 3) {
			M_event_done(args->el);
		}
	}

	if (args->test_id == STATUS) {
		if (args->sent_cb_call_count == 1) {
			M_net_smtp_pause(args->sp);
			args->is_success = (M_net_smtp_status(args->sp) == M_NET_SMTP_STATUS_STOPPING);
		}
		if (args->sent_cb_call_count == 2) {
			M_event_done(args->el);
		}
	}
}

static M_bool send_failed_cb(const M_hash_dict_t *headers, const char *error, size_t attempt_num,
		M_bool can_requeue, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_send_failed_cb(%p, \"%s\", %zu, %s, %p)", headers, error, attempt_num, can_requeue ? "M_TRUE" : "M_FALSE", thunk);
	args->is_send_failed_cb_called = M_TRUE;
	args->send_failed_cb_call_count++;
	if (args->test_id == BCC_TEST) {
		if (args->send_failed_cb_call_count == 4) {
			M_event_done(args->el);
		}
	}
	if (args->test_id == BAD_SERVER_2) {
		if (args->send_failed_cb_call_count == 4) {
			M_event_done(args->el);
		}
		return M_FALSE;
	}
	if (args->test_id == MAX_ATTEMPTS_0) {
		args->is_success = (can_requeue == M_FALSE);
		M_event_done(args->el);
	}
	if (args->test_id == MULTITHREAD_RETRY) {
		if (args->send_failed_cb_call_count == multithread_retry_count) {
			M_printf("Send failed for %zu msgs, retry in 3 sec\n", multithread_retry_count);
			smtp_emulator_switch(args->emu, "minimal");
		}
		return M_TRUE; /* requeue message */
	}
	if (args->test_id == EMU_ACCEPT_DISCONNECT) {
		M_event_done(args->el);
	}
	if (args->test_id == DOT_MSG) {
		if ((args->sent_cb_call_count == 2 && args->send_failed_cb_call_count == 1) || args->sent_cb_call_count == 3) {
			M_event_done(args->el);
		}
		return M_FALSE;
	}
	if (args->test_id == JUNK_MSG) {
		args->is_success = (can_requeue == M_FALSE);
		M_event_done(args->el);
	}
	return M_FALSE; /* should msg be requeued? */
}

static void reschedule_cb(const char *msg, M_uint64 wait_sec, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_reschedule_cb(\"%s\", %zu, %p)", msg, wait_sec, thunk);
	args->is_reschedule_cb_called = M_TRUE;
	args->reschedule_cb_call_count++;
}

static M_bool iocreate_cb(M_io_t *io, char *error, size_t errlen, void *thunk)
{
	args_t *args = thunk;
	event_debug("M_net_smtp_iocreate_cb(%p, %p, %zu, %p)", io, error, errlen, thunk);
	args->is_iocreate_cb_called = M_TRUE;
	args->iocreate_cb_call_count++;
	if (args->test_id == IOCREATE_RETURN_FALSE) {
		if (args->iocreate_cb_call_count == 2) {
			event_debug("M_event_done(%p)\n", args->el);
			M_event_done(args->el);
		}
		event_debug("M_net_smtp_iocreate_cb(): return M_FALSE");
		return M_FALSE;
	}
	return M_TRUE;
}

struct M_net_smtp_callbacks test_cbs  = {
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


static char * test_external_queue_get_cb(void)
{
	return M_list_str_take_first(test_external_queue);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef struct {
	M_net_smtp_t *sp;
	M_email_t    *e;
} multithread_arg_t;

static void multithread_insert_task(void *thunk)
{
	multithread_arg_t *arg = thunk;
	M_net_smtp_queue_smtp(arg->sp, arg->e);
}

START_TEST(multithread_retry)
{
	M_uint16               testport;
	args_t                 args      = { 0 };
	M_event_t             *el        = M_event_pool_create(0);
	smtp_emulator_t       *emu       = smtp_emulator_create(el, TLS_TYPE_NONE, "reject_457", &testport, MULTITHREAD_RETRY);
	M_net_smtp_t          *sp        = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t               *dns       = M_dns_create(el);
	multithread_arg_t     *tests     = NULL;
	void                 **testptrs  = NULL;
	M_email_t             *e         = generate_email(1, test_address);
	M_threadpool_t        *tp        = M_threadpool_create(10, 10, 10, 0);
	M_threadpool_parent_t *tp_parent = M_threadpool_parent_create(tp);
	size_t                 i         = 0;

	args.test_id = MULTITHREAD_RETRY;
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 10) == M_FALSE,
			"should fail adding tcp endpoint without setting dns");

	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	tests = M_malloc_zero(sizeof(*tests) * multithread_retry_count);
	testptrs = M_malloc_zero(sizeof(*testptrs) * multithread_retry_count);
	for (i = 0; i < multithread_retry_count; i++) {
		tests[i].sp = sp;
		tests[i].e  = e;
		testptrs[i] = &tests[i];
	}
	M_threadpool_dispatch(tp_parent, multithread_insert_task, testptrs, multithread_retry_count);
	args.el = el;
	args.emu = emu;

	M_threadpool_parent_wait(tp_parent);
	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count = multithread_retry_count, "should have called sent_cb count times");

	M_threadpool_parent_destroy(tp_parent);
	M_threadpool_destroy(tp);
	M_free(testptrs);
	M_free(tests);
	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(multithread_insert)
{
	M_uint16               testport;
	args_t                 args      = { 0 };
	M_event_t             *el        = M_event_pool_create(0);
	smtp_emulator_t       *emu       = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, MULTITHREAD_INSERT);
	M_net_smtp_t          *sp        = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t               *dns       = M_dns_create(el);
	M_email_t             *e         = generate_email(1, test_address);
	multithread_arg_t     *tests     = NULL;
	void                 **testptrs  = NULL;
	M_threadpool_t        *tp        = M_threadpool_create(10, 10, 10, 0);
	M_threadpool_parent_t *tp_parent = M_threadpool_parent_create(tp);
	size_t                 i         = 0;

	args.test_id = MULTITHREAD_INSERT;
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_FALSE,
			"should fail adding tcp endpoint without setting dns");

	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	tests = M_malloc_zero(sizeof(*tests) * multithread_insert_count);
	testptrs = M_malloc_zero(sizeof(*testptrs) * multithread_insert_count);
	for (i = 0; i < multithread_insert_count; i++) {
		tests[i].sp = sp;
		tests[i].e  = e;
		testptrs[i] = &tests[i];
	}
	M_threadpool_dispatch(tp_parent, multithread_insert_task, testptrs, multithread_insert_count);
	args.el = el;

	M_threadpool_parent_wait(tp_parent);
	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count = multithread_insert_count, "should have called sent_cb count times");

	M_threadpool_parent_destroy(tp_parent);
	M_threadpool_destroy(tp);
	M_free(testptrs);
	M_free(tests);
	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
	cleanup();
}
END_TEST
START_TEST(dump_queue)
{
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_list_str_t      *list        = NULL;

	args.test_id = DUMP_QUEUE;
	M_net_smtp_queue_message(sp, "junk");
	list = M_net_smtp_dump_queue(sp);
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, NULL, NULL, 1000, 0), "Couldn't add endpoint_process");
	args.el = el;
	args.sp = sp;

	M_event_loop(el, 10);

	ck_assert_msg(args.sent_cb_call_count == 0, "shouldn't have sent anything");
	ck_assert_msg(args.send_failed_cb_call_count == 0, "shouldn't have sent anything");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be in idle");

	M_list_str_destroy(list);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST
START_TEST(junk_msg)
{
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);

	args.test_id = JUNK_MSG;
	M_net_smtp_queue_message(sp, "junk");

	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, NULL, NULL, 1000, 1), "Couldn't add endpoint_process");
	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.send_failed_cb_call_count == 1, "should have failed to sent 1 message");
	ck_assert_msg(args.is_success, "shouldn't allow retry");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be in idle");

	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST
START_TEST(external_queue)
{
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e           = generate_email(1, test_address);
	char              *msg         = M_email_simple_write(e);

	args.test_id = EXTERNAL_QUEUE;
	test_external_queue = M_list_str_create(M_LIST_STR_NONE);
	M_net_smtp_use_external_queue(sp, test_external_queue_get_cb);

	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, NULL, NULL, 1000, 1), "Couldn't add endpoint_process");

	args.el = el;
	args.sp = sp;

	M_list_str_insert(test_external_queue, msg);
	M_net_smtp_external_queue_have_messages(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count == 1, "should have sent 1 message");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be in idle");

	M_free(msg);
	M_email_destroy(e);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(test_external_queue);
	test_external_queue = NULL;
	cleanup();
}
END_TEST

START_TEST(halt_restart)
{
	M_list_str_t      *cmd_args    = M_list_str_create(M_LIST_STR_NONE);
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e           = generate_email(1, test_address);

	args.test_id = HALT_RESTART;
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, cmd_args, NULL, 10000, 1), "Couldn't add endpoint_process");

	args.el = el;
	args.sp = sp;

	M_net_smtp_pause(sp);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count == 1, "should have sent 1 message");
	ck_assert_msg(args.processing_halted_cb_call_count == 1, "should have processing halted from pause()");

	M_email_destroy(e);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(cmd_args);
	cleanup();
}
END_TEST

START_TEST(proc_not_found)
{
	M_list_str_t      *cmd_args    = M_list_str_create(M_LIST_STR_NONE);
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e           = generate_email(1, test_address);

	args.test_id = PROC_NOT_FOUND;
	M_net_smtp_queue_smtp(sp, e);

	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, "proc_not_found", cmd_args, NULL, 10000, 1), "Couldn't add endpoint_process");

	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.process_fail_cb_call_count == 1, "should have had a process fail");
	ck_assert_msg(args.processing_halted_cb_call_count == 1, "should have halted processing");
	ck_assert_msg(args.is_success, "should have NOENDPOINTS set in processing_halted_cb");

	M_email_destroy(e);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(cmd_args);
	cleanup();
}
END_TEST

static void warmup_io_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	int *mask = thunk;
	if (etype == M_EVENT_TYPE_CONNECTED) {
		*mask |= 1;
	}
	if (etype == M_EVENT_TYPE_DISCONNECTED) {
		*mask &= ~1;
		M_io_destroy(io);
	}
	event_debug("warmup_io_cb - mask: %X", *mask);
	if (*mask == 0) {
		M_event_done(el);
	}
}


static void warmup_io_stdin_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	int *mask = thunk;
	(void)el;
	if (etype == M_EVENT_TYPE_CONNECTED) {
		M_io_destroy(io);
	}
	event_debug("warmup_io_stdin_cb - mask: %X", *mask);
}

static void warmup_io_stderr_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	int *mask = thunk;
	if (etype == M_EVENT_TYPE_CONNECTED) {
		*mask |= 4;
	}
	if (etype == M_EVENT_TYPE_DISCONNECTED) {
		M_io_destroy(io);
		*mask &= ~4;
	}
	event_debug("warmup_io_stderr_cb - mask: %X", *mask);
	if (*mask == 0) {
		M_event_done(el);
	}
}

static void warmup_io_stdout_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
{
	int *mask = thunk;
	if (etype == M_EVENT_TYPE_CONNECTED) {
		*mask |= 8;
	}
	if (etype == M_EVENT_TYPE_DISCONNECTED) {
		M_io_destroy(io);
		*mask &= ~8;
	}
	event_debug("warmup_io_stdout_cb - mask: %X", *mask);
	if (*mask == 0) {
		M_event_done(el);
	}
}

static M_bool warmup_sendmail_emu(M_event_t *el)
{
	M_io_error_t io_err;
	int          mask      = 0;
	M_io_t      *io        = NULL;
	M_io_t      *io_stdin  = NULL;
	M_io_t      *io_stdout = NULL;
	M_io_t      *io_stderr = NULL;

	io_err = M_io_process_create(sendmail_emu, NULL, NULL, 10000, &io, &io_stdin, &io_stdout, &io_stderr);

	if (io_err != M_IO_ERROR_SUCCESS)
		return M_FALSE;

	M_event_add(el, io, warmup_io_cb, &mask);
	M_event_add(el, io_stdin, warmup_io_stdin_cb, &mask);
	M_event_add(el, io_stdout, warmup_io_stdout_cb, &mask);
	M_event_add(el, io_stderr, warmup_io_stderr_cb, &mask);

	M_event_loop(el, M_TIMEOUT_INF);

	return M_TRUE;
}

START_TEST(dot_msg)
{
	M_uint16           testport;
	M_list_str_t      *cmd_args1   = M_list_str_create(M_LIST_STR_NONE);
	M_list_str_t      *cmd_args2   = M_list_str_create(M_LIST_STR_NONE);
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	smtp_emulator_t   *emu         = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, DOT_MSG);
	M_dns_t           *dns         = M_dns_create(el);
	M_email_t         *e           = generate_email_with_text(test_address, "\r\n.\r\n after message");

	ck_assert_msg(warmup_sendmail_emu(el), "Should succeed with warmup");

	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_setup_tcp_timeouts(sp, 200, 300, 400);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1), "Couldn't add TCP endpoint");

	args.test_id = DOT_MSG;
	M_list_str_insert(cmd_args2, "-t");
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, cmd_args2, NULL, 10000, 1), "Couldn't add endpoint_process");

	M_list_str_insert(cmd_args1, "-t");
	M_list_str_insert(cmd_args1, "-i");
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, cmd_args1, NULL, 10000, 1), "Couldn't add endpoint_process");


	M_net_smtp_pause(sp);

	ck_assert_msg(M_net_smtp_load_balance(sp, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN), "Set load balance should succeed");

	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	M_net_smtp_resume(sp);

	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	event_debug("send_cb_call_count: %d, send_failed_cb_call_count: %d\n", args.sent_cb_call_count, args.send_failed_cb_call_count);
	ck_assert_msg(args.sent_cb_call_count == 2, "2 Messages should have sent");
	ck_assert_msg(args.send_failed_cb_call_count == 1, "1 Message should have failed to send");

	M_list_str_destroy(cmd_args1);
	M_list_str_destroy(cmd_args2);
	smtp_emulator_destroy(emu);
	M_dns_destroy(dns);
	M_email_destroy(e);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	/* M_list_str_destroy(cmd_args); */
	cleanup();
}
END_TEST
START_TEST(process_error_exit)
{
	M_list_str_t      *cmd_args    = M_list_str_create(M_LIST_STR_NONE);
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e1          = generate_email(1, test_address);

	args.test_id = PROCESS_ERROR_EXIT;
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "Should return status no endpoints");

	M_net_smtp_queue_smtp(sp, e1);

	M_list_str_insert(cmd_args, "-x");
	M_list_str_insert(cmd_args, "15");
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, cmd_args, NULL, 100, 2), "Couldn't add endpoint_process");

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_PROCESSING, "Should start processing as soon as endpoint added");

	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.process_fail_cb_call_count == 1, "Should have called process_fail");

	M_email_destroy(e1);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(cmd_args);
	cleanup();
}
END_TEST

START_TEST(proc_endpoint)
{
	M_list_str_t      *cmd_args    = M_list_str_create(M_LIST_STR_NONE);
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e1          = generate_email(1, test_address);
	M_email_t         *e2          = generate_email(2, test_address);

	args.test_id = STATUS; /* Does the same thing as Status, but with Proc endpoints */
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "Should return status no endpoints");

	M_net_smtp_queue_smtp(sp, e1);
	M_net_smtp_queue_smtp(sp, e2);

	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, sendmail_emu, cmd_args, NULL, 100, 2), "Couldn't add endpoint_process");

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_PROCESSING, "Should start processing as soon as endpoint added");

	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_success, "Should have seen status STOPPING after pause() call");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_STOPPED, "Should have stopped processing");
	M_net_smtp_resume(sp);
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "Should be idle on restart");

	M_email_destroy(e1);
	M_email_destroy(e2);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(cmd_args);
	cleanup();
}
END_TEST
START_TEST(status)
{
	M_uint16           testport;
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu         = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, STATUS);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t           *dns         = M_dns_create(el);
	M_email_t         *e1          = generate_email(1, test_address);
	M_email_t         *e2          = generate_email(2, test_address);

	args.test_id = STATUS;
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "Should return status no endpoints");

	M_net_smtp_queue_smtp(sp, e1);
	M_net_smtp_queue_smtp(sp, e2);
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 2);

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_PROCESSING, "Should start processing as soon as endpoint added");

	args.el = el;
	args.sp = sp;

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_success, "Should have seen status STOPPING after pause() call");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_STOPPED, "Should have stopped processing");
	M_net_smtp_resume(sp);
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "Should be idle on restart");

	smtp_emulator_destroy(emu);
	M_email_destroy(e1);
	M_email_destroy(e2);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(timeouts)
{
	M_uint16           testport1;
	M_uint16           testport2;
	M_uint16           testport3;
	args_t             args        = { 0 };
	M_event_t         *el          = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu_connect = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport1, TIMEOUT_CONNECT);
	smtp_emulator_t   *emu_stall   = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport2, TIMEOUT_STALL);
	smtp_emulator_t   *emu_idle    = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport3, TIMEOUT_IDLE);
	M_net_smtp_t      *sp          = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t           *dns         = M_dns_create(el);
	M_email_t         *e1          = generate_email(1, test_address);
	M_email_t         *e2          = generate_email(2, test_address);
	M_email_t         *e3          = generate_email(3, test_address);

	args.test_id = TIMEOUTS;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_setup_tcp_timeouts(sp, 200, 300, 400);
	M_net_smtp_load_balance(sp, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport1, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport2, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport3, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e1);
	M_net_smtp_queue_smtp(sp, e2);
	M_net_smtp_queue_smtp(sp, e3);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.connect_fail_cb_call_count == 2, "connect/stall timeouts should have called connect_fail");
	ck_assert_msg(args.sent_cb_call_count == 3, "idle timeout should have sent all 3 messages");
	ck_assert_msg(args.disconnect_cb_call_count == 1, "idle timeout should have called disconnect once");

	smtp_emulator_destroy(emu_connect);
	smtp_emulator_destroy(emu_stall);
	smtp_emulator_destroy(emu_idle);
	M_email_destroy(e1);
	M_email_destroy(e2);
	M_email_destroy(e3);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(tls_unsupporting_server)
{
	M_uint16           testport;
	args_t             args     = { 0 };
	M_event_t         *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, TLS_UNSUPPORTING_SERVER);
	M_net_smtp_t      *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t           *dns      = M_dns_create(el);
	M_email_t         *e        = generate_email(1, test_address);
	M_tls_clientctx_t *ctx      = M_tls_clientctx_create();

	args.test_id = TLS_UNSUPPORTING_SERVER;
	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_TRUE, "user", "pass", 1) == M_FALSE, "should fail to add TLS because ctx is NULL");

	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_tls_clientctx_destroy(ctx);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_TRUE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_connect_fail_cb_called == M_TRUE, "should have called connect_fail_cb");
	ck_assert_msg(args.is_processing_halted_cb_called == M_TRUE, "should have called processing_halted_cb");

	smtp_emulator_destroy(emu);
	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(no_server)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, NO_SERVER);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = NO_SERVER;
	smtp_emulator_destroy(emu); /* just needed an open port */

	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_setup_tcp_timeouts(sp, 100, 100, 100);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_connect_fail_cb_called == M_TRUE, "should have called connect_fail_cb");
	ck_assert_msg(args.is_processing_halted_cb_called == M_TRUE, "should have called processing_halted_cb");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(iocreate_return_false)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, IOCREATE_RETURN_FALSE);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = IOCREATE_RETURN_FALSE;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

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
	cleanup();
}
END_TEST

START_TEST(emu_accept_disconnect)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, EMU_ACCEPT_DISCONNECT);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = EMU_ACCEPT_DISCONNECT;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1);

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
	cleanup();
}
END_TEST

START_TEST(auth_digest_md5)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "auth_digest_md5", &testport, AUTH_DIGEST_MD5);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = AUTH_DIGEST_MD5;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(auth_cram_md5)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "auth_cram_md5", &testport, AUTH_CRAM_MD5);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = AUTH_CRAM_MD5;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(auth_plain)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "auth_plain", &testport, AUTH_PLAIN);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = AUTH_PLAIN;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(bad_auth)
{
	M_uint16         testport1;
	M_uint16         testport2;
	M_uint16         testport3;
	M_uint16         testport4;
	M_uint16         testport5;
	M_uint16         testport6;
	M_uint16         testport7;
	M_uint16         testport8;
	M_uint16         testport9;
	M_uint16         testport10;
	M_uint16         testport11;
	M_uint16         testport12;
	M_uint16         testport13;
	M_uint16         testport14;
	M_uint16         testport15;
	M_uint16         testport16;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu1     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth1", &testport1, BAD_SERVER);
	smtp_emulator_t *emu2     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth2", &testport2, BAD_SERVER);
	smtp_emulator_t *emu3     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth3", &testport3, BAD_SERVER);
	smtp_emulator_t *emu4     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth4", &testport4, BAD_SERVER);
	smtp_emulator_t *emu5     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth5", &testport5, BAD_SERVER);
	smtp_emulator_t *emu6     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth6", &testport6, BAD_SERVER);
	smtp_emulator_t *emu7     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth7", &testport7, BAD_SERVER);
	smtp_emulator_t *emu8     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth8", &testport8, BAD_SERVER);
	smtp_emulator_t *emu9     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth9", &testport9, BAD_SERVER);
	smtp_emulator_t *emu10    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth10", &testport10, BAD_SERVER);
	smtp_emulator_t *emu11    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth11", &testport11, BAD_SERVER);
	smtp_emulator_t *emu12    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth12", &testport12, BAD_SERVER);
	smtp_emulator_t *emu13    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth13", &testport13, BAD_SERVER);
	smtp_emulator_t *emu14    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth14", &testport14, BAD_SERVER);
	smtp_emulator_t *emu15    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth15", &testport15, BAD_SERVER);
	smtp_emulator_t *emu16    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad_auth16", &testport16, BAD_SERVER);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);
	M_tls_clientctx_t *ctx    = M_tls_clientctx_create();

	args.test_id = BAD_SERVER;

	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_tls_clientctx_destroy(ctx);

	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport1, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport2, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport3, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport4, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport5, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport6, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport7, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport8, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport9, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport10, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport11, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport12, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport13, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport14, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport15, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport16, M_FALSE, "user", "pass", 1);

	M_net_smtp_load_balance(sp, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.connect_fail_cb_call_count == 16, "should have failed 16 connections ");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "should have process halted w/ noendpoints");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu1);
	smtp_emulator_destroy(emu2);
	smtp_emulator_destroy(emu3);
	smtp_emulator_destroy(emu4);
	smtp_emulator_destroy(emu5);
	smtp_emulator_destroy(emu6);
	smtp_emulator_destroy(emu7);
	smtp_emulator_destroy(emu8);
	smtp_emulator_destroy(emu9);
	smtp_emulator_destroy(emu10);
	smtp_emulator_destroy(emu11);
	smtp_emulator_destroy(emu12);
	smtp_emulator_destroy(emu13);
	smtp_emulator_destroy(emu14);
	smtp_emulator_destroy(emu15);
	smtp_emulator_destroy(emu16);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(auth_login)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "auth_login", &testport, AUTH_LOGIN);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = AUTH_LOGIN;

	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(starttls)
{
	M_uint16           testport;
	args_t             args       = { 0 };
	M_event_t         *el         = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu        = smtp_emulator_create_tls(el, TLS_TYPE_STARTTLS, "starttls", &testport, STARTTLS, test_serverctx);
	M_net_smtp_t      *sp         = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t           *dns        = M_dns_create(el);
	M_email_t         *e          = generate_email(1, test_address);
	M_tls_clientctx_t *ctx        = M_tls_clientctx_create();

	args.test_id = STARTTLS;
	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_tls_clientctx_destroy(ctx);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_TRUE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(implicit_tls)
{
	M_uint16           testport;
	args_t             args     = { 0 };
	M_event_t         *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t   *emu      = smtp_emulator_create_tls(el, TLS_TYPE_IMPLICIT, "minimal", &testport, IMPLICIT_TLS, test_serverctx);
	M_net_smtp_t      *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t           *dns      = M_dns_create(el);
	M_email_t         *e        = generate_email(1, test_address);
	M_tls_clientctx_t *ctx      = M_tls_clientctx_create();

	args.test_id = IMPLICIT_TLS;
	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_tls_clientctx_destroy(ctx);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_TRUE, "user", "pass", 1) == M_TRUE,
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
	cleanup();
}
END_TEST

START_TEST(bad_server_2)
{
	M_uint16         testport13;
	M_uint16         testport14;
	M_uint16         testport15;
	M_uint16         testport16;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu13    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad13", &testport13, BAD_SERVER);
	smtp_emulator_t *emu14    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad14", &testport14, BAD_SERVER);
	smtp_emulator_t *emu15    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad15", &testport15, BAD_SERVER);
	smtp_emulator_t *emu16    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad16", &testport16, BAD_SERVER);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id              = BAD_SERVER_2;

	M_net_smtp_setup_tcp(sp, dns, NULL);

	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport13, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport14, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport15, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport16, M_FALSE, "user", "pass", 1);

	M_net_smtp_load_balance(sp, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.send_failed_cb_call_count == 4, "should have failed 8 messages ");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu13);
	smtp_emulator_destroy(emu14);
	smtp_emulator_destroy(emu15);
	smtp_emulator_destroy(emu16);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(bad_server)
{
	M_uint16         testport1;
	M_uint16         testport2;
	M_uint16         testport3;
	M_uint16         testport4;
	M_uint16         testport4_2;
	M_uint16         testport5;
	M_uint16         testport6;
	M_uint16         testport7;
	M_uint16         testport8;
	M_uint16         testport9;
	M_uint16         testport10;
	M_uint16         testport11;
	M_uint16         testport12;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu1     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad1", &testport1, BAD_SERVER);
	smtp_emulator_t *emu2     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad2", &testport2, BAD_SERVER);
	smtp_emulator_t *emu3     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad3", &testport3, BAD_SERVER);
	smtp_emulator_t *emu4     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad4", &testport4, BAD_SERVER);
	smtp_emulator_t *emu4_2   = smtp_emulator_create(el, TLS_TYPE_NONE, "bad4_2", &testport4_2, BAD_SERVER);
	smtp_emulator_t *emu5     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad5", &testport5, BAD_SERVER);
	smtp_emulator_t *emu6     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad6", &testport6, BAD_SERVER);
	smtp_emulator_t *emu7     = smtp_emulator_create(el, TLS_TYPE_STARTTLS, "bad7", &testport7, BAD_SERVER);
	smtp_emulator_t *emu8     = smtp_emulator_create(el, TLS_TYPE_STARTTLS, "bad8", &testport8, BAD_SERVER);
	smtp_emulator_t *emu9     = smtp_emulator_create(el, TLS_TYPE_NONE, "bad9", &testport9, BAD_SERVER);
	smtp_emulator_t *emu10    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad10", &testport10, BAD_SERVER);
	smtp_emulator_t *emu11    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad11", &testport11, BAD_SERVER);
	smtp_emulator_t *emu12    = smtp_emulator_create(el, TLS_TYPE_NONE, "bad12", &testport12, BAD_SERVER);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);
	M_tls_clientctx_t *ctx    = M_tls_clientctx_create();

	args.test_id = BAD_SERVER;

	M_tls_clientctx_set_default_trust(ctx);
	M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
	M_net_smtp_setup_tcp(sp, dns, ctx);
	M_tls_clientctx_destroy(ctx);

	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport1, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport2, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport3, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport4, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport4_2, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport5, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport6, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport7, M_TRUE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport8, M_TRUE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport9, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport10, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport11, M_FALSE, "user", "pass", 1);
	M_net_smtp_add_endpoint_tcp(sp, "localhost", testport12, M_FALSE, "user", "pass", 1);

	M_net_smtp_load_balance(sp, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.connect_fail_cb_call_count == 13, "should have failed 13 connections ");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "should have process halted w/ noendpoints");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu1);
	smtp_emulator_destroy(emu2);
	smtp_emulator_destroy(emu3);
	smtp_emulator_destroy(emu4);
	smtp_emulator_destroy(emu4_2);
	smtp_emulator_destroy(emu5);
	smtp_emulator_destroy(emu6);
	smtp_emulator_destroy(emu7);
	smtp_emulator_destroy(emu8);
	smtp_emulator_destroy(emu9);
	smtp_emulator_destroy(emu10);
	smtp_emulator_destroy(emu11);
	smtp_emulator_destroy(emu12);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(reactivate_idle)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, REACTIVATE_IDLE);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = REACTIVATE_IDLE;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_setup_tcp_timeouts(sp, 1000, 1000, 4000);

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 3) == M_TRUE,
			"should succeed adding tcp after setting dns");

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count == 3, "should have called sent_cb 3 times");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be idle");

	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);


	smtp_emulator_switch(emu, "bad9");
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count == 6, "should have called sent_cb 6 times");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "should have halted");

	smtp_emulator_switch(emu, "minimal");
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 3) == M_TRUE,
			"should succeed adding tcp after setting dns");
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.sent_cb_call_count == 7, "should have called sent_cb 7 times");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should idle");

	smtp_emulator_destroy(emu);
	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(default_cbs)
{
	M_uint16         testport;
	M_uint16         testport2;
	args_t           args     = { 0 };
	M_list_str_t    *cmd_args = M_list_str_create(M_LIST_STR_NONE);
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, DEFAULT_CBS);
	smtp_emulator_t *emu2     = smtp_emulator_create(el, TLS_TYPE_NONE, "reject_457", &testport2, DEFAULT_CBS);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, NULL, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);
	char            *msg      = M_email_simple_write(e);

	args.test_id = DEFAULT_CBS;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	M_net_smtp_setup_tcp_timeouts(sp, 100, 100, 0);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, "proc_not_found", cmd_args, NULL, 10000, 1), "Couldn't add endpoint_process");

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport2, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, 500);

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");
	M_net_smtp_pause(sp);

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_STOPPED, "should return to stopped after pause()");
	M_net_smtp_pause(sp);

	M_net_smtp_resume(sp);

	smtp_emulator_destroy(emu);
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, 300);

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be idle after failovers and reject_457");

	M_list_str_destroy(M_net_smtp_dump_queue(sp));
	/* send again using external_queue to trigger reschedule_cb */
	test_external_queue = M_list_str_create(M_LIST_STR_NONE);
	ck_assert_msg(M_net_smtp_use_external_queue(sp, test_external_queue_get_cb) == M_TRUE, "should be able to switch to external queue");
	M_list_str_insert(test_external_queue, msg);
	M_net_smtp_external_queue_have_messages(sp);

	M_event_loop(el, 300);

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should be idle after reject_457");

	smtp_emulator_destroy(emu2);
	M_list_str_destroy(cmd_args);
	M_email_destroy(e);
	M_free(msg);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	M_list_str_destroy(test_external_queue);
	test_external_queue = NULL;
	cleanup();
}
END_TEST

START_TEST(bcc_test)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, BCC_TEST);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);
	const char      *no_from  =
"To: akistler@localhost\r\n"
"CC: cc@localhost\r\n"
"Content-Type: multipart/alternative; boundary=\"------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\"\r\n"
"Subject: Testing\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\r\n"
"Content-Type: text/plain; charset=\"utf-8\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"20220615:104441, 1\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao--\r\n";
	const char      *no_to    =
"From: smtp_cli <no-reply+smtp-test@monetra.com>\r\n"
"Content-Type: multipart/alternative; boundary=\"------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\"\r\n"
"Subject: Testing\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\r\n"
"Content-Type: text/plain; charset=\"utf-8\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"20220615:104441, 1\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao--\r\n";
	const char      *bad_from1  =
"From: <mal>\r\n"
"To: akistler@localhost\r\n"
"CC: cc@localhost\r\n"
"Content-Type: multipart/alternative; boundary=\"------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\"\r\n"
"Subject: Testing\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\r\n"
"Content-Type: text/plain; charset=\"utf-8\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"20220615:104441, 1\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao--\r\n";
	const char      *bad_from2  =
"From: <mal@>\r\n"
"To: akistler@localhost\r\n"
"CC: cc@localhost\r\n"
"Content-Type: multipart/alternative; boundary=\"------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\"\r\n"
"Subject: Testing\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao\r\n"
"Content-Type: text/plain; charset=\"utf-8\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"20220615:104441, 1\r\n"
"\r\n"
"--------------HAaLZiQJuZlCrCgRbX5QSim4b[ao--\r\n";

	M_email_cc_append(e, NULL, NULL, "cc@localhost");
	M_email_bcc_append(e, NULL, NULL, "bcc@localhost");


	args.test_id = BCC_TEST;

	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_iocreate_cb_called, "should have called iocreate_cb");
	ck_assert_msg(args.is_connect_cb_called, "should have called connect_cb");
	ck_assert_msg(args.is_sent_cb_called, "should have called sent_cb");
	ck_assert_msg(args.is_success, "should have removed BCC from headers");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");

	ck_assert_msg(M_net_smtp_queue_message(sp, no_from) == M_TRUE, "should add");
	ck_assert_msg(M_net_smtp_queue_message(sp, no_to) == M_TRUE, "should add");
	ck_assert_msg(M_net_smtp_queue_message(sp, bad_from1) == M_TRUE, "should add");
	ck_assert_msg(M_net_smtp_queue_message(sp, bad_from2) == M_TRUE, "should add");

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.send_failed_cb_call_count == 4, "should fail to send msg with no from and no to");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(domain_mismatch)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, DOMAIN_MISMATCH);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = DOMAIN_MISMATCH;
	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "127.0.0.1", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.connect_fail_cb_call_count == 1, "Should have connect failed with wrong domain");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_NOENDPOINTS, "should process halted after endpoint removed");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(max_attempts_0)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, MAX_ATTEMPTS_0);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = MAX_ATTEMPTS_0;
	M_net_smtp_setup_tcp(sp, dns, NULL);
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_set_num_attempts(sp, 0);
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_success, "should not be able to requeue");
	ck_assert_msg(args.send_failed_cb_call_count == 1, "should have failed to send message");
	ck_assert_msg(args.is_connect_cb_called == M_FALSE, "should not have called connect_cb");

	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");

	M_email_destroy(e);
	M_dns_destroy(dns);
	M_net_smtp_destroy(sp);
	smtp_emulator_destroy(emu);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(emu_sendmsg)
{
	M_uint16         testport;
	args_t           args     = { 0 };
	M_event_t       *el       = M_event_create(M_EVENT_FLAG_NONE);
	smtp_emulator_t *emu      = smtp_emulator_create(el, TLS_TYPE_NONE, "minimal", &testport, EMU_SENDMSG);
	M_net_smtp_t    *sp       = M_net_smtp_create(el, &test_cbs, &args);
	M_dns_t         *dns      = M_dns_create(el);
	M_email_t       *e        = generate_email(1, test_address);

	args.test_id = EMU_SENDMSG;
	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_FALSE,
			"should fail adding tcp endpoint without setting dns");

	M_net_smtp_setup_tcp(sp, dns, NULL);

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", testport, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	args.el = el;
	M_net_smtp_queue_smtp(sp, e);
	M_net_smtp_resume(sp);

	M_event_loop(el, M_TIMEOUT_INF);
	event_debug("exit loop");

	ck_assert_msg(args.is_iocreate_cb_called, "should have called iocreate_cb");
	ck_assert_msg(args.is_connect_cb_called, "should have called connect_cb");
	ck_assert_msg(args.is_sent_cb_called, "should have called sent_cb");
	ck_assert_msg(M_net_smtp_status(sp) == M_NET_SMTP_STATUS_IDLE, "should return to idle after sent_cb()");

	ck_assert_msg(M_net_smtp_add_endpoint_tcp(sp, "localhost", 0, M_FALSE, "user", "pass", 1) == M_TRUE,
			"should succeed adding tcp after setting dns");

	event_debug("M_email_destroy()");
	M_email_destroy(e);
	event_debug("M_dns_destroy()");
	M_dns_destroy(dns);
	event_debug("M_net_smtp_destroy()");
	M_net_smtp_destroy(sp);
	event_debug("M_smtp_emulator_destroy()");
	smtp_emulator_destroy(emu);
	event_debug("M_event_destroy()");
	M_event_destroy(el);
	event_debug("cleanup()");
	cleanup();
}
END_TEST


START_TEST(check_no_endpoints)
{
	args_t        args = { 0 };
	M_event_t    *el   = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t *sp   = M_net_smtp_create(el, &test_cbs, &args);

	args.test_id = NO_ENDPOINTS;
	ck_assert_msg(M_net_smtp_resume(sp) == M_FALSE, "should fail with no endpoints");
	ck_assert_msg(args.is_success, "should trigger processing_halted_cb with no endpoints");
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

START_TEST(dummy_checks)
{

	args_t             args = { 0 };
	M_event_t         *el   = M_event_create(M_EVENT_FLAG_NONE);
	M_net_smtp_t      *sp   = M_net_smtp_create(el, &test_cbs, &args);
	M_email_t         *e    = generate_email(1, test_address);
	M_dns_t           *dns  = M_dns_create(el);
	M_tls_clientctx_t *ctx  = M_tls_clientctx_create();

	args.test_id = DUMMY_CHECKS;

	M_net_smtp_external_queue_have_messages(NULL);
	ck_assert_msg(M_net_smtp_use_external_queue(NULL, test_external_queue_get_cb) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_use_external_queue(sp, NULL) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_queue_message(NULL, "junk") == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_queue_message(sp, NULL) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_dump_queue(NULL) == NULL, "should fail");
	ck_assert_msg(M_net_smtp_queue_smtp(NULL, e) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_queue_smtp(sp, NULL) == M_FALSE, "should fail");
	M_net_smtp_set_num_attempts(NULL, 1);
	ck_assert_msg(M_net_smtp_load_balance(NULL, M_NET_SMTP_LOAD_BALANCE_ROUNDROBIN) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_add_endpoint_process(NULL, sendmail_emu, NULL, NULL, 100, 2) == M_FALSE, "should fail");
	ck_assert_msg(M_net_smtp_add_endpoint_process(sp, NULL, NULL, NULL, 100, 2) == M_FALSE, "should fail");
	M_net_smtp_setup_tcp_timeouts(NULL, 1, 2, 3);
	M_net_smtp_setup_tcp(NULL, dns, ctx);
	ck_assert_msg(M_net_smtp_status(NULL) == M_NET_SMTP_STATUS_NOENDPOINTS, "should fail");
	ck_assert_msg(M_net_smtp_resume(NULL) == M_FALSE, "should fail");
	M_net_smtp_pause(NULL);
	M_net_smtp_destroy(NULL);
	ck_assert_msg(M_net_smtp_create(NULL, NULL, NULL) == NULL, "should fail");

	ck_assert_msg(M_net_smtp_use_external_queue(sp, test_external_queue_get_cb) == M_TRUE, "should work");
	/* should only work on internal */
	ck_assert_msg(M_net_smtp_queue_smtp(sp, e) == M_FALSE, "should fail");
	M_net_smtp_set_num_attempts(sp, 3);

	M_tls_clientctx_destroy(ctx);
	M_dns_destroy(dns);
	M_email_destroy(e);
	M_net_smtp_destroy(sp);
	M_event_destroy(el);
	cleanup();
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *smtp_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("smtp");

	/*NO_ENDPOINTS            = 1, */
#if TESTONLY == 0 || TESTONLY == 1
	tc = tcase_create("no-endpoints");
	tcase_add_test(tc, check_no_endpoints);
	suite_add_tcase(suite, tc);
#endif

/*EMU_SENDMSG             = 2, */
#if TESTONLY == 0 || TESTONLY == 2
	tc = tcase_create("emu-sendmsg");
	tcase_add_test(tc, emu_sendmsg);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*EMU_ACCEPT_DISCONNECT   = 3, */
#if TESTONLY == 0 || TESTONLY == 3
	tc = tcase_create("emu-accept-disconnect");
	tcase_add_test(tc, emu_accept_disconnect);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*IOCREATE_RETURN_FALSE   = 4,*/
#if TESTONLY == 0 || TESTONLY == 4
	tc = tcase_create("iocreate_return_false");
	tcase_add_test(tc, iocreate_return_false);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*NO_SERVER               = 5,*/
#if TESTONLY == 0 || TESTONLY == 5
	tc = tcase_create("no server");
	tcase_add_test(tc, no_server);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*TLS_UNSUPPORTING_SERVER = 6,*/
#if TESTONLY == 0 || TESTONLY == 6
	tc = tcase_create("tls unsupporting server");
	tcase_add_test(tc, tls_unsupporting_server);
	tcase_set_timeout(tc, 10); /* M_tls_clientctx_set_default_trust(ctx) takes a long time in valgrind */
	suite_add_tcase(suite, tc);
#endif

/*TIMEOUTS                = 7,*/
#if TESTONLY == 0 || TESTONLY == 7
	tc = tcase_create("timeouts");
	tcase_add_test(tc, timeouts);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*STATUS                  = 11, */
#if TESTONLY == 0 || TESTONLY == 11
	tc = tcase_create("status");
	tcase_add_test(tc, status);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*PROC_ENDPOINT           = 12,*/
#if TESTONLY == 0 || TESTONLY == 12
	tc = tcase_create("proc_endpoint");
	tcase_add_test(tc, proc_endpoint);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);
#endif

/*DOT_MSG                 = 13,*/
#if TESTONLY == 0 || TESTONLY == 13
	tc = tcase_create("dot msg");
	tcase_add_test(tc, dot_msg);
	tcase_set_timeout(tc, 20);
	suite_add_tcase(suite, tc);
#endif

/*PROC_NOT_FOUND          = 14,*/
#if TESTONLY == 0 || TESTONLY == 14
	tc = tcase_create("proc_not_found");
	tcase_add_test(tc, proc_not_found);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*HALT_RESTART            = 15,*/
#if TESTONLY == 0 || TESTONLY == 15
	tc = tcase_create("halt restart");
	tcase_add_test(tc, halt_restart);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);
#endif

/*EXTERNAL_QUEUE          = 16,*/
#if TESTONLY == 0 || TESTONLY == 16
	tc = tcase_create("external queue");
	tcase_add_test(tc, external_queue);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);
#endif

/*JUNK_MSG                = 17,*/
#if TESTONLY == 0 || TESTONLY == 17
	tc = tcase_create("junk msg");
	tcase_add_test(tc, junk_msg);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*DUMP_QUEUE              = 18,*/
#if TESTONLY == 0 || TESTONLY == 18
	tc = tcase_create("dump queue");
	tcase_add_test(tc, dump_queue);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*MULTITHREAD_INSERT      = 19, */
#if TESTONLY == 0 || TESTONLY == 19
	tc = tcase_create("multithread insert");
	tcase_add_test(tc, multithread_insert);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*MULTITHREAD_RETRY       = 20, */
#if TESTONLY == 0 || TESTONLY == 20
	tc = tcase_create("multithread retry");
	tcase_add_test(tc, multithread_retry);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);
#endif

/*AUTH_LOGIN              = 21, */
#if TESTONLY == 0 || TESTONLY == 21
	tc = tcase_create("auth login");
	tcase_add_test(tc, auth_login);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*AUTH_PLAIN              = 22, */
#if TESTONLY == 0 || TESTONLY == 22
	tc = tcase_create("auth plain");
	tcase_add_test(tc, auth_plain);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*AUTH_CRAM_MD5           = 23, */
#if TESTONLY == 0 || TESTONLY == 23
	tc = tcase_create("auth cram md5");
	tcase_add_test(tc, auth_cram_md5);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*AUTH_DIGEST_MD5         = 24, */
#if TESTONLY == 0 || TESTONLY == 24
	tc = tcase_create("auth digest md5");
	tcase_add_test(tc, auth_digest_md5);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*STARTTLS                = 25, */
#if TESTONLY == 0 || TESTONLY == 25
	tc = tcase_create("starttls");
	tcase_add_test(tc, starttls);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*IMPLICIT_TLS            = 26, */
#if TESTONLY == 0 || TESTONLY == 26
	tc = tcase_create("implicit_tls");
	tcase_add_test(tc, implicit_tls);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*IMPLICIT_TLS            = 27, */
#if TESTONLY == 0 || TESTONLY == 27
	tc = tcase_create("dummy checks");
	tcase_add_test(tc, dummy_checks);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*BAD_SERVER              = 28, */
#if TESTONLY == 0 || TESTONLY == 28
	tc = tcase_create("bad server");
	tcase_add_test(tc, bad_server);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*BAD_SERVER_2            = 29, */
#if TESTONLY == 0 || TESTONLY == 29
	tc = tcase_create("bad server 2");
	tcase_add_test(tc, bad_server_2);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*REACTIVATE_IDLE         = 30, */
#if TESTONLY == 0 || TESTONLY == 30
	tc = tcase_create("reactivate_idle");
	tcase_add_test(tc, reactivate_idle);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*DEFAULT_CBS             = 31, */
#if TESTONLY == 0 || TESTONLY == 31
	tc = tcase_create("default cbs");
	tcase_add_test(tc, default_cbs);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*BCC_TEST                = 32, */
#if TESTONLY == 0 || TESTONLY == 32
	tc = tcase_create("bcc test");
	tcase_add_test(tc, bcc_test);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*BAD_AUTH                = 33, */
#if TESTONLY == 0 || TESTONLY == 33
	tc = tcase_create("bad auth");
	tcase_add_test(tc, bad_auth);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*PROCESS_ERROR_EXIT      = 34, */
#if TESTONLY == 0 || TESTONLY == 34
	tc = tcase_create("process error exit");
	tcase_add_test(tc, process_error_exit);
	tcase_set_timeout(tc, 10);
	suite_add_tcase(suite, tc);
#endif

/*DOMAIN_MISMATCH         = 35, */
#if TESTONLY == 0 || TESTONLY == 35
	tc = tcase_create("domain mismatch");
	tcase_add_test(tc, domain_mismatch);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

/*MAX_ATTEMPTS_0          = 36, */
#if TESTONLY == 0 || TESTONLY == 36
	tc = tcase_create("max attempts 0");
	tcase_add_test(tc, max_attempts_0);
	tcase_set_timeout(tc, 5);
	suite_add_tcase(suite, tc);
#endif

	return suite;
}

static M_tls_serverctx_t *self_signed_serverctx(void)
{
	char               *realcert;
	char               *realkey;
	M_tls_x509_t       *x509;
	M_tls_serverctx_t  *serverctx;
	/* Generate real cert */
	realkey = M_tls_rsa_generate_key(2048);
	if (realkey == NULL) {
		event_debug("failed to generate RSA private key");
		return NULL;
	}
	x509 = M_tls_x509_new(realkey);
	if (x509 == NULL) {
		event_debug("failed to generate X509 cert");
		return NULL;
	}
	if (!M_tls_x509_txt_add(x509, M_TLS_X509_TXT_COMMONNAME, "localhost", M_FALSE)) {
		event_debug("failed to add common name");
		return NULL;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost", M_TRUE)) {
		event_debug("failed to add subjectaltname1");
		return NULL;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_DNS, "localhost.localdomain", M_TRUE)) {
		event_debug("failed to add subjectaltname2");
		return NULL;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "127.0.0.1", M_TRUE)) {
		event_debug("failed to add subjectaltname3");
		return NULL;
	}
	if (!M_tls_x509_txt_SAN_add(x509, M_TLS_X509_SAN_TYPE_IP, "::1", M_TRUE)) {
		event_debug("failed to add subjectaltname4");
		return NULL;
	}

	realcert = M_tls_x509_selfsign(x509, 365 * 24 * 60 * 60 /* 1 year */);
	if (realcert == NULL) {
		event_debug("failed to self-sign");
		return NULL;
	}
	M_tls_x509_destroy(x509);
//M_printf("PrivateKey: %s\n", key);
	event_debug("ServerCert: %s\n", realcert);
	serverctx = M_tls_serverctx_create((const M_uint8 *)realkey, M_str_len(realkey), (const M_uint8 *)realcert, M_str_len(realcert), NULL, 0);
	M_free(realkey);
	M_free(realcert);
	return serverctx;
}

int main(int argc, char **argv)
{
	int         nf;
	size_t      len;
	char       *dirname;
	const char *env_USER;
	SRunner    *sr;

	(void)argc;
	(void)argv;

	env_USER = getenv("USER");
	if (env_USER == NULL) {
		env_USER = "test";
	}
	len = M_str_len(env_USER) + M_str_len("@localhost");
	test_address = M_malloc_zero(len + 1);
	M_snprintf(test_address, len + 1, "%s@localhost", env_USER);

	dirname = M_fs_path_dirname(argv[0], M_FS_SYSTEM_AUTO);
	len = M_str_len(dirname) + M_str_len("/sendmail_emu");
	sendmail_emu = M_malloc_zero(len + 1);
	M_snprintf(sendmail_emu, len + 1, "%s/sendmail_emu", dirname);
	M_free(dirname);

	test_serverctx = self_signed_serverctx();

	check_smtp_json = M_json_read(json_str, M_str_len(json_str), M_JSON_READER_NONE, NULL, NULL, NULL, NULL);

	sr = srunner_create(smtp_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_smtp.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	cleanup();

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
