#include <mstdlib/mstdlib.h>
#include <mstdlib/formats/m_json.h>
#include <mstdlib/net/m_net.h>
#include <mstdlib/net/m_net_smtp.h>

typedef struct {
	M_bool      is_bailout;
	M_bool      is_debug;
	M_bool      is_show_only;
	char        errmsg[256];
	M_list_t   *endpoints;
	M_event_t  *el;
	M_int64     num_sent;
	M_int64     num_to_generate;
	char       *to_address;
	char       *to_address_default;
	M_dns_t    *dns;
} prag_t;

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
	M_event_done(prag->el);
	return 0; /* Seconds to wait before retry.  0 stops trying */
}

static void sent_cb(const M_hash_dict_t *headers, void *thunk)
{
	prag_t *prag = thunk;
	prag->num_sent++;
	if (prag->num_sent == prag->num_to_generate) {
		M_event_done(prag->el);
	}
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

static M_list_str_t *json_array_to_list_str(M_json_node_t *node)
{
	M_list_str_t *list = M_list_str_create(M_LIST_STR_NONE);
	size_t        i;
	for (i = 0; i < M_json_array_len(node); i++) {
		M_list_str_insert(list, M_json_array_at_string(node, i));
	}
	return list;
}

static M_hash_dict_t *json_object_to_hash_dict(M_json_node_t *node)
{
	M_hash_dict_t *h;
	M_list_str_t  *keys;
	size_t         i;
	keys = M_json_object_keys(node);
	for (i = 0; i < M_list_str_len(keys); i++) {
		const char *key = M_list_str_at(keys, i);
		const char *value = M_json_object_value_string(node, key);
		M_hash_dict_insert(h, key, value);
	}
	M_list_str_destroy(keys);
	return h;
}

static M_bool add_tcp_endpoint(const char *address, M_net_smtp_t *sp, prag_t *prag, const M_json_node_t *endpoint)
{
	M_uint16      port           = M_json_object_value_int(endpoint, "port");
	M_bool        connect_tls    = M_json_object_value_bool(endpoint, "connect_tls");
	const char   *username       = M_json_object_value_string(endpoint, "username");
	const char   *password       = M_json_object_value_string(endpoint, "password");
	M_uint64      max_conns      = M_json_object_value_int(endpoint, "max_conns");

	if (prag->dns == NULL) {
		M_tls_clientctx_t *ctx = M_tls_clientctx_create();
		M_tls_clientctx_set_default_trust(ctx);
		M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
		prag->dns = M_dns_create(prag->el);
		M_net_smtp_setup_tcp(sp, prag->dns, ctx);
		M_tls_clientctx_destroy(ctx);
	}

	if (!M_net_smtp_add_endpoint_tcp(sp, address, port, connect_tls, username, password, max_conns)) {

		M_printf("%s:%d: M_net_smtp_add_endpoint_tcp(<%s>) failed\n", __FILE__, __LINE__, address);
		return M_FALSE;
	}
	return M_TRUE;
}

static M_bool add_proc_endpoint(const char *command, M_net_smtp_t *sp, prag_t *prag, const M_json_node_t *endpoint)
{
	M_list_str_t  *args          = NULL;
	M_hash_dict_t *env           = NULL;
	M_json_node_t *args_node     = M_json_object_value(endpoint, "args");
	M_json_node_t *env_node      = M_json_object_value(endpoint, "env");
	M_uint64       timeout_ms    = M_json_object_value_int(endpoint, "timeout_ms");
	size_t         max_processes = M_json_object_value_int(endpoint, "max_processes");

	if (M_json_node_type(args_node) == M_JSON_TYPE_ARRAY) {
		args = json_array_to_list_str(args_node);
	}

	if (M_json_node_type(env_node) == M_JSON_TYPE_OBJECT) {
		env = json_object_to_hash_dict(env_node);
	}

	if (!M_net_smtp_add_endpoint_process(sp, command, args, env, timeout_ms, max_processes)) {

		M_printf("%s:%d: M_net_smtp_add_endpoint_process(<%s>) failed\n", __FILE__, __LINE__, command);
		return M_FALSE;
	}
	return M_TRUE;
}

static M_bool add_endpoint(M_net_smtp_t *sp, prag_t *prag, const M_json_node_t *endpoint)
{
	const char* proc = M_json_object_value_string(endpoint, "proc");
	const char* tcp = M_json_object_value_string(endpoint, "tcp");
	if (proc != NULL) {
		return add_proc_endpoint(proc, sp, prag, endpoint);
	}
	if (tcp != NULL) {
		return add_tcp_endpoint(tcp, sp, prag, endpoint);
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
	int    rc = 0;
	size_t i;

	prag->el  = M_event_create(M_EVENT_FLAG_NONE);
	sp = M_net_smtp_create(prag->el, &cbs, prag);
	M_net_smtp_setup_tcp_timeouts(sp, 300000, 300000, 300000);

	for (i = 0; i < M_list_len(prag->endpoints); i++) {
		if (!add_endpoint(sp, prag, M_list_at(prag->endpoints, i))) {
			M_printf("Error: %s\n", prag->errmsg);
			rc = 1;
			goto done;
		}
	}

	for (i = 0; i < prag->num_to_generate; i++) {
		M_email_t *e = generate_email(i, prag->to_address);
		if (prag->is_show_only) {
			char *msg = M_email_simple_write(e);
			M_printf("%s", msg);
			M_printf("\r\n.\r\n");
			M_free(msg);
		} else {
			if (!M_net_smtp_queue_smtp(sp, e)) {
				M_printf("M_net_smtp_queue_smtp(): Error\n");
				goto done;
			}
		}
		M_email_destroy(e);
	}

	if (!prag->is_show_only) {

		if (!M_net_smtp_resume(sp)) {
			M_printf("M_net_smtp_resume(): Error");
			goto done;
		}

		M_event_loop(prag->el, M_TIMEOUT_INF);
	}

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
		M_json_node_t *port              = M_json_object_value(endpoint, "port");
		M_json_node_t *connect_tls       = M_json_object_value(endpoint, "connect_tls");
		M_json_node_t *username          = M_json_object_value(endpoint, "username");
		M_json_node_t *password          = M_json_object_value(endpoint, "password");
		M_json_node_t *max_conns         = M_json_object_value(endpoint, "max_conns");
		M_json_type_t tcp_type           = M_json_node_type(tcp);
		M_json_type_t port_type          = M_json_node_type(port);
		M_json_type_t connect_tls_type   = M_json_node_type(connect_tls);
		M_json_type_t username_type      = M_json_node_type(username);
		M_json_type_t password_type      = M_json_node_type(password);
		M_json_type_t max_conns_type     = M_json_node_type(max_conns);

		if ((tcp_type         != M_JSON_TYPE_STRING)  ||
		    (port_type        != M_JSON_TYPE_INTEGER) ||
		    (connect_tls_type != M_JSON_TYPE_BOOL)    ||
		    (username_type    != M_JSON_TYPE_STRING)  ||
		    (password_type    != M_JSON_TYPE_STRING)  ||
		    (max_conns_type   != M_JSON_TYPE_INTEGER)) {
			M_snprintf(prag->errmsg, sizeof(prag->errmsg), "%s:%d: json for tcp needs to be { tcp: \"\", port: 25, connect_tls: false, username: \"\", password: \"\", max_conns: 1  }", __FILE__, __LINE__);
			return M_FALSE;
		}
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
M_bool getopt_string_cb(char short_opt, const char *long_opt, const char *value, void *thunk)
{
	prag_t *prag = thunk;
	switch(short_opt) {
		case 't':
			prag->to_address = M_strdup(value);
			return M_TRUE;
			break;
	}
	return M_FALSE;
}

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
		case 's':
			prag->is_show_only = M_TRUE;
			return M_TRUE;
			break;
	}
	return M_FALSE;
}

static void destroy_prag(prag_t *prag)
{
	M_json_node_t *json;
	size_t         len  = M_list_len(prag->endpoints);
	size_t         i;

	for (i = 0; i < len; i++) {
		json = M_list_take_first(prag->endpoints);
		M_json_node_destroy(json);
	}
	M_list_destroy(prag->endpoints, M_FALSE);
	if (prag->to_address != prag->to_address_default) {
		M_free(prag->to_address);
	}
	M_free(prag->to_address_default);
	M_dns_destroy(prag->dns);
}

int main(int argc, const char * const *argv)
{
	prag_t           prag         = { 0 };
	M_getopt_t      *getopt       = NULL;
	const char      *fail         = NULL;
	int              rc           = 0;
	M_getopt_error_t getopt_error = M_GETOPT_ERROR_SUCCESS;

	prag.to_address_default = M_malloc_zero(128);
	M_snprintf(prag.to_address_default, 128, "%s@localhost", getenv("USER"));
	prag.to_address = prag.to_address_default;
	prag.endpoints = M_list_create(NULL, M_LIST_NONE);
	getopt = M_getopt_create(getopt_nonopt_cb);

	M_getopt_addboolean(getopt, 'h', "help", M_FALSE, "Print help", getopt_boolean_cb);
	M_getopt_addboolean(getopt, 'd', "debug", M_FALSE, "Debug printing", getopt_boolean_cb);
	M_getopt_addboolean(getopt, 's', "show-only", M_FALSE, "Show emails, but don't queue", getopt_boolean_cb);
	M_getopt_addinteger(getopt, 'g', "generate", M_TRUE, "Number of messages to generate", getopt_integer_cb);
	M_getopt_addstring(getopt, 't', "send-to", M_TRUE, "Email address to send to (default: ${USER}@localhost)", getopt_string_cb);
	/* Also: M_getopt_add{integer,decimal,string}() */
	getopt_error = M_getopt_parse(getopt, argv, argc, &fail, &prag);
	if (getopt_error != M_GETOPT_ERROR_SUCCESS || (M_list_len(prag.endpoints) == 0 && !prag.is_show_only)) {
		char *help;
		if (prag.is_bailout) {
			M_printf("Error: %s\n", prag.errmsg);
			return 1;
		}
		help = M_getopt_help(getopt);
		M_printf("usage: %s [OPTION]...ENDPOINT(s)\n", argv[0]);
		M_printf("Endpoint:\n");
		M_printf("\"{ \\\"proc\\\": \\\"sendmail\\\", \\\"args\\\": [ \\\"-t\\\" ], \\\"env\\\": null, \\\"timeout_ms\\\": 5000, \\\"max_processes\\\": 1 }\"\n");
		M_printf("\"{ \\\"tcp\\\": \\\"localhost\\\", \\\"port\\\": 25, \\\"connect_tls\\\": false, \\\"username\\\": \\\"%s@localhost\\\", \\\"password\\\": \\\"<secret>\\\", \\\"max_conns\\\": 1 }\"\n", getenv("USER"));
		M_printf("\"{ \\\"tcp\\\": \\\"localhost\\\", \\\"port\\\": 587, \\\"connect_tls\\\": true, \\\"username\\\": \\\"%s@localhost\\\", \\\"password\\\": \\\"<secret>\\\", \\\"max_conns\\\": 1 }\"\n", getenv("USER"));
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
