#include <mstdlib/mstdlib.h>
#include <mstdlib/formats/m_json.h>
#include <mstdlib/net/m_net.h>
#include <mstdlib/net/m_net_smtp.h>

typedef struct {
	M_bool      is_bailout;
	M_bool      is_debug;
	char        errmsg[256];
	M_list_t   *endpoints;
	M_event_t  *el;
	M_int64     num_to_generate;
	char       *to_address;
} prag_t;

static M_email_t * generate_email(size_t idx, const char *to_address)
{
	M_email_t *e;
	char msg[256];
	M_time_tzs_t      *tzs;
	const M_time_tz_t *tz;
	M_time_t           ts;
	M_time_localtm_t   ltime;

	M_mem_set(&ltime, 0, sizeof(ltime));
	ts = M_time();
	tzs = M_time_tzs_load_zoneinfo(NULL, M_TIME_TZ_ZONE_AMERICA, M_TIME_TZ_ALIAS_OLSON_MAIN, M_TIME_TZ_LOAD_LAZY);
	tz  = M_time_tzs_get_tz(tzs, "America/New_York");
	M_time_tolocal(ts, &ltime, tz);

	e = M_email_create();
	M_email_set_from(e, "Monetra Testing", "smtp_cli", "no-reply+smtp-test@monetra.com");
	M_email_to_append(e, NULL, NULL, to_address);
	M_email_set_subject(e, "smtp_cli testing");
	M_snprintf(msg, sizeof(msg), "%04lld%02lld%02lld:%02lld%02lld%02lld, %zu\n", ltime.year, ltime.month, ltime.day, ltime.hour, ltime.min, ltime.sec, idx);
	M_email_part_append(e, msg, M_str_len(msg), NULL, NULL);
	return e;
}

static void connect_cb(const char *address, M_uint16 port, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%s,%u,%p)\n", __FILE__, __LINE__, __FUNCTION__, address, port, thunk);
	}
	return;
}

static M_bool connect_fail_cb(const char *address, M_uint16 port, M_net_error_t net_err, const char *error, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%s,%u,%u,*(%s),%p)\n", __FILE__, __LINE__, __FUNCTION__, address, port, net_err, error, thunk);
	}
	return M_TRUE; /* M_FALSE: remove from pool. M_TRUE: retry later */
}

static void disconnect_cb(const char *address, M_uint16 port, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%s,%u,%p)\n", __FILE__, __LINE__, __FUNCTION__, address, port, thunk);
	}
	return;
}

static M_bool process_fail_cb(const char *command, int result_code, const char *proc_stdout, const char *proc_stderr, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(*(%s),%d,*(%s),*(%s),%p)\n", __FILE__, __LINE__, __FUNCTION__, command, result_code, 
				proc_stdout, proc_stderr, thunk);
	}
	return M_TRUE; /* M_FALSE: remove from pool.  M_TRUE: retry later */
}

static M_uint64 processing_halted_cb(M_bool no_endpoint, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%d, %p)\n", __FILE__, __LINE__, __FUNCTION__, no_endpoint, thunk);
	}
	return 0; /* Seconds to wait before retry.  0 stops trying */
}

static void sent_cb(const M_hash_dict_t *headers, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%p, %p)\n", __FILE__, __LINE__, __FUNCTION__, headers, thunk);
	}
	return;
}

static M_bool send_failed_cb(const M_hash_dict_t *headers, const char *error, size_t attempt_run, M_bool can_requeue,
		void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%p, *(%s), %zu, %d, %p)\n", __FILE__, __LINE__, __FUNCTION__, headers, error, attempt_run,
				can_requeue, thunk);
	}
	return M_TRUE; /* requeue message?  Ignored with external queue. */
}

static void reschedule_cb(const char *msg, M_uint64 wait_sec, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(*(%s), %llu, %p)\n", __FILE__, __LINE__, __FUNCTION__, msg, wait_sec, thunk);
	}
	return;
}

static M_bool add_proc_endpoint(const char *command, M_net_smtp_t *sp, prag_t *prag, const M_json_node_t *endpoint)
{
	M_list_str_t * args          = NULL;
	M_hash_dict_t *env           = NULL;
	M_uint64       timeout_ms    = 0;
	size_t         max_processes = 1;

	if (!M_net_smtp_add_endpoint_process(sp, command, args, env, timeout_ms, max_processes)) {

		M_printf("%s:%d: M_net_smtp_add_endpoint_process(<%s>) failed\n", __FILE__, __LINE__, command);
		return M_FALSE;
	}
	return M_TRUE;
}

static M_bool add_endpoint(M_net_smtp_t *sp, prag_t *prag, const M_json_node_t *endpoint)
{
	const char* proc = M_json_object_value_string(endpoint, "proc");
	if (proc != NULL) {
		return add_proc_endpoint(proc, sp, prag, endpoint);
	}
	M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: unsupported endpoint", __FILE__, __LINE__);
	return M_FALSE;
}

static M_bool iocreate_cb(M_io_t *io, char *error, size_t errlen, void *thunk)
{
	prag_t *prag = thunk;
	if (prag->is_debug) {
		M_printf("%s:%d: %s(%p,*(%s), %zu, %p)\n", __FILE__, __LINE__, __FUNCTION__, io, error, errlen, thunk);
	}
	return M_TRUE; /* M_FALSE: fail/abort, M_TRUE: success */
}

int run(prag_t *prag)
{
	M_net_smtp_t                *sp   = NULL;
	struct M_net_smtp_callbacks  cbs  = {
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
	int rc = 0;

	prag->el  = M_event_create(M_EVENT_FLAG_NONE);
	sp = M_net_smtp_create(prag->el, &cbs, prag);

	for (size_t i = 0; i < M_list_len(prag->endpoints); i++) {
		if (!add_endpoint(sp, prag, M_list_at(prag->endpoints, i))) {
			M_printf("Error: %s\n", prag->errmsg);
			rc = 1;
			goto done;
		}
	}

	for (size_t i = 0; i < prag->num_to_generate; i++) {
		M_email_t *e   = generate_email(i, prag->to_address);
		M_net_smtp_queue_smtp(sp, e);
		M_email_destroy(e);
	}

	M_net_smtp_resume(sp);

	M_event_loop(args->el, M_TIMEOUT_INF);

done:
	M_net_smtp_destroy(sp);

	return rc;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool validate_endpoint_json(M_json_node_t *endpoint, prag_t *prag)
{
	M_json_node_t *proc = M_json_object_value(endpoint, "proc");
	M_json_node_t *tcp = M_json_object_value(endpoint, "tcp");

	if ((proc == NULL && tcp == NULL) || (proc != NULL && tcp != NULL)) {
		M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: json must have exactly one of (\"proc\", \"tcp\") defined", __FILE__, __LINE__);
		return M_FALSE;
	}

	if (proc != NULL) {
		M_json_node_t *args              = M_json_object_value(endpoint, "args");
		M_json_node_t *env               = M_json_object_value(endpoint, "env");
		M_json_node_t *timeout_ms        = M_json_object_value(endpoint, "timeout_ms");
		M_json_node_t *max_processes     = M_json_object_value(endpoint, "max_processes");
		M_json_type_t proc_type          = M_json_node_type(proc);
		M_json_type_t args_type          = M_json_node_type(args);
		M_json_type_t env_type           = M_json_node_type(env);
		M_json_type_t timeout_ms_type    = M_json_node_type(timeout_ms);
		M_json_type_t max_processes_type = M_json_node_type(max_processes);

		if ((proc_type != M_JSON_TYPE_STRING) ||
		    (args_type != M_JSON_TYPE_ARRAY) ||
		    (env_type != M_JSON_TYPE_NULL && env_type != M_JSON_TYPE_OBJECT) ||
		    (timeout_ms_type != M_JSON_TYPE_INTEGER) ||
		    (max_processes_type != M_JSON_TYPE_INTEGER)) {
			M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: json for proc needs to be { proc: \"\", args: [], env: { } (or null), timeout_ms: 0, max_processes: 1  }", __FILE__, __LINE__);
			return M_FALSE;
		}
	} else {
		M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: TCP unsupported", __FILE__, __LINE__);
		return M_FALSE;
	}

	return M_TRUE;
}

M_bool getopt_nonopt_cb(size_t idx, const char *option, void *thunk)
{
	/* parse EndPoint */
	size_t          option_len;
	M_json_node_t  *json;
	size_t          processed_len;
	M_json_error_t  error;
	size_t          error_line;
	size_t          error_pos;
	prag_t         *prag          = thunk;

	option_len = M_str_len(option);
	M_printf("json: %s\n", option);
	json = M_json_read(option, option_len, M_JSON_READER_NONE, &processed_len, &error, &error_line, &error_pos);
	if (json == NULL) {
		M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: M_json_read(%s): %s @(%zu,%zu)", __FILE__, __LINE__, option, M_json_errcode_to_str(error), error_line, error_pos);
		prag->is_bailout = M_TRUE;
		return M_FALSE;
	}
	if (!validate_endpoint_json(json, prag)) {
		prag->is_bailout = M_TRUE;
		return M_FALSE;
	}
	if (!M_list_insert(prag->endpoints, json)) {
		M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: M_list_insert(%p,%p) failed.", __FILE__, __LINE__, prag->endpoints, json);
		prag->is_bailout = M_TRUE;
		return M_FALSE;
	}
	return M_TRUE;
}

/* Also: getopt_{integer,decimal,string}_cb(,,{M_int64*,M_decimal_t*,const char*},) */
M_bool getopt_integer_cb(char short_opt, const char *long_opt, M_int64 *num, void *thunk)
{
	prag_t *prag = thunk;
	switch (short_opt) {
		case 'g':
			prag->num_to_generate = *num;
			return M_TRUE;
			break;
	}
	return M_FALSE;
}

M_bool getopt_boolean_cb(char short_opt, const char *long_opt, M_bool boolean, void *thunk)
{
	prag_t *prag = thunk;
	switch (short_opt) {
		case 'h':
			return M_FALSE;
			break;
		case 'd':
			prag->is_debug = M_TRUE;
			return M_TRUE;
			break;
	}
	return M_FALSE;
}

static void destroy_prag(prag_t *prag)
{
	M_json_node_t *json;
	size_t         len  = M_list_len(prag->endpoints);

	for (size_t i = 0; i < len; i++) {
		json = M_list_take_first(prag->endpoints);
		M_json_node_destroy(json);
	}
	M_list_destroy(prag->endpoints, M_FALSE);
}

int main(int argc, const char * const *argv)
{
	prag_t           prag         = { 0 };
	M_getopt_t      *getopt       = NULL;
	const char      *fail         = NULL;
	int              rc           = 0;
	M_getopt_error_t getopt_error = M_GETOPT_ERROR_SUCCESS;

	prag.to_address = M_malloc_zero(128);
	M_snprintf(prag.to_address, 128, "%s@localhost", getenv("USER"));
	prag.endpoints = M_list_create(NULL, M_LIST_NONE);
	getopt = M_getopt_create(getopt_nonopt_cb);

	M_getopt_addboolean(getopt, 'h', "help", M_FALSE, "Print help", getopt_boolean_cb);
	M_getopt_addboolean(getopt, 'd', "debug", M_FALSE, "Debug printing", getopt_boolean_cb);
	M_getopt_addinteger(getopt, 'g', "generate", M_TRUE, "Number of messages to generate", getopt_integer_cb);
	/* Also: M_getopt_add{integer,decimal,string}() */
	getopt_error = M_getopt_parse(getopt, argv, argc, &fail, &prag);
	if (getopt_error != M_GETOPT_ERROR_SUCCESS || M_list_len(prag.endpoints) == 0) {
		char *help;
		if (prag.is_bailout) {
			M_printf("Error: %s\n", prag.errmsg);
			return 1;
		}
		help = M_getopt_help(getopt);
		M_printf("usage: %s [OPTION]...ENDPOINT(s)\n", argv[0]);
		M_printf("Endpoint:\n\"{ \\\"proc\\\": \\\"sendmail\\\", \\\"args\\\": [ \\\"-t\\\" ], \\\"env\\\": {}, \\\"timeout_ms\\\": 0, \\\"max_processes\\\": 1 }\"\n");
		M_printf("Options:\n%s\n", help);
		destroy_prag(&prag);
		M_getopt_destroy(getopt);
		M_free(help);
		return 0;
	}
	M_getopt_destroy(getopt);
	rc = run(&prag);
	destroy_prag(&prag);
	return rc;
}
