#include "m_config.h"
#include "check_net_http_json.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_net.h>
#include <mstdlib/mstdlib_text.h>
#include <mstdlib/io/m_io_layer.h>

struct {
	M_event_t         *el;
	M_dns_t           *dns;
	M_tls_clientctx_t *ctx;
	M_json_node_t     *json;
} g;

typedef struct {
	M_bool is_success;
} test_args_t;

static void cleanup_int(void)
{
	M_tls_clientctx_destroy(g.ctx);
	M_dns_destroy(g.dns);
	M_event_destroy(g.el);
	M_library_cleanup();
}

static void cleanup(void)
{
#ifndef _WIN32
	cleanup_int();
#endif
}

static void trigger_softevent(M_io_t *io, M_event_type_t etype)
{
	M_io_layer_t *layer = M_io_layer_acquire(io, 0, NULL);
	M_io_layer_softevent_add(layer, M_FALSE, etype, M_IO_ERROR_SUCCESS);
	M_io_layer_release(layer);
}

typedef struct {
	const char *json_key;
} test_server_args_t;

struct test_server_t;
typedef struct test_server_t test_server_t;

typedef struct {
	M_io_t              *io;
	M_buf_t             *out_buf;
	M_parser_t          *in_parser;
	M_http_reader_t     *httpr;
	M_hash_dict_t       *request_headers;
	const test_server_t *server;
	M_bool               is_responded;
} test_server_stream_t;

struct test_server_t {
	M_io_t               *io_listen;
	M_uint16              port;
	M_json_node_t        *json;
	M_hash_dict_t        *index_dict;
	const char           *match_response;
	const char           *unmatch_response;
	size_t                stream_count;
	test_server_stream_t  streams[16];
};

static M_http_error_t respond_start_func(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	test_server_stream_t *stream = thunk;
	(void)reason;
	(void)thunk;
	(void)version;
	M_hash_dict_insert(stream->request_headers, ":method", M_http_method_to_str(method));
	M_hash_dict_insert(stream->request_headers, ":path", uri);
	if (type == M_HTTP_MESSAGE_TYPE_RESPONSE) {
		char *num;
		M_asprintf(&num, "%u", code);
		M_hash_dict_insert(stream->request_headers, ":status", num);
		M_free(num);
	}
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t respond_header_full_func(const char *key, const char *val, void *thunk)
{
	test_server_stream_t *stream = thunk;
	M_hash_dict_insert(stream->request_headers, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static void respond(test_server_stream_t *stream)
{
	size_t               len;
	M_hash_dict_enum_t  *hashenum;
	const M_hash_dict_t *hash;
	const char          *key;
	const char          *value;
	M_bool               is_match;
	struct M_http_reader_callbacks cbs = {
		respond_start_func,
		respond_header_full_func,
		NULL, /* header_func */
		NULL, /* header_done_func */
		NULL, /* body_func */
		NULL, /* body_done_func */
		NULL, /* chunk_extensions_func */
		NULL, /* chunk_extensions_done_func */
		NULL, /* chunk_data_func */
		NULL, /* chunk_data_done_func */
		NULL, /* chunk_data_finished_func */
		NULL, /* multipart_preamble_func */
		NULL, /* multipart_preamble_done_func */
		NULL, /* multipart_header_full_func */
		NULL, /* multipart_header_func */
		NULL, /* multipart_header_done_func */
		NULL, /* multipart_data_func */
		NULL, /* multipart_data_done_func */
		NULL, /* multipart_data_finished_func */
		NULL, /* multipart_epilouge_func */
		NULL, /* multipart_epilouge_done_func */
		NULL, /* trailer_full_func */
		NULL, /* trailer_func */
		NULL, /* trailer_done_func */
	};
	M_http_reader_t *respond_httpr = M_http_reader_create(&cbs, M_HTTP_READER_NONE, stream);
	M_http_reader_read(respond_httpr, M_parser_peek(stream->in_parser), M_parser_len(stream->in_parser), &len);
	M_http_reader_destroy(respond_httpr);
	M_parser_consume(stream->in_parser, len);
	hash = stream->server->index_dict;
	len = M_hash_dict_enumerate(hash, &hashenum);
	is_match = M_TRUE;
	while (M_hash_dict_enumerate_next(hash, hashenum, &key, &value)) {
		const char *request_value = M_hash_dict_get_direct(stream->request_headers, key);
		if (request_value == NULL || !M_str_eq(request_value, value)) {
			is_match = M_FALSE;
			break;
		}
	}
	M_hash_dict_enumerate_free(hashenum);
	if (is_match) {
		M_buf_add_str(stream->out_buf, stream->server->match_response);
	} else {
		M_buf_add_str(stream->out_buf, stream->server->unmatch_response);
	}

	stream->is_responded = M_TRUE;

	trigger_softevent(stream->io, M_EVENT_TYPE_WRITE);
}

static void test_server_stream_init(test_server_stream_t *stream);
static void test_server_stream_deinit(test_server_stream_t *stream);

static void test_server_event_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *thunk)
{
	test_server_stream_t *stream = thunk;
	M_io_error_t          ioerr;
	M_http_error_t        herr;
	(void)event;
	(void)io;
	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			test_server_stream_init(stream);
			break;
		case M_EVENT_TYPE_READ:
			ioerr = M_io_read_into_parser(stream->io, stream->in_parser);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			herr = M_http_reader_read(stream->httpr, M_parser_peek(stream->in_parser), M_parser_len(stream->in_parser), NULL);
			if (herr == M_HTTP_ERROR_SUCCESS)
				respond(stream);
			break;
		case M_EVENT_TYPE_WRITE:
			ioerr = M_io_write_from_buf(stream->io, stream->out_buf);
			if (ioerr == M_IO_ERROR_DISCONNECT)
				goto disconnect;
			if (stream->is_responded && M_buf_len(stream->out_buf) == 0) {
				M_io_disconnect(stream->io);
			}
			break;
		case M_EVENT_TYPE_ERROR:
		case M_EVENT_TYPE_OTHER:
			break;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ACCEPT:
			goto disconnect;
	}
	return;
disconnect:
	test_server_stream_deinit(stream);
	return;
}

static void test_server_listen_cb(M_event_t *event, M_event_type_t type, M_io_t *io_listen, void *thunk)
{
	test_server_t *srv = thunk;
	(void)event;
	if (type == M_EVENT_TYPE_ACCEPT) {
		size_t                 idx    = srv->stream_count;
		test_server_stream_t  *stream = &srv->streams[idx];
		stream->server                = srv;

		M_io_accept(&stream->io, io_listen);

		srv->stream_count++;
		M_event_add(g.el, stream->io, test_server_event_cb, stream);
	}
}

static test_server_t *test_server_create(test_server_args_t *args)
{
	test_server_t *srv;
	M_list_str_t  *json_keys;
	M_json_node_t *json;
	M_json_node_t *json_index;
	size_t         i;

	srv             = M_malloc_zero(sizeof(*srv));

	M_io_net_server_create(&srv->io_listen, 0, NULL, M_IO_NET_ANY);

	srv->port             = M_io_net_get_port(srv->io_listen);
	json                  = M_json_object_value(g.json, args->json_key);
	srv->match_response   = M_json_object_value_string(json, "match_response");
	srv->unmatch_response = M_json_object_value_string(json, "unmatch_response");
	json_index            = M_json_object_value(json, "index");
	json_keys             = M_json_object_keys(json_index);
	srv->index_dict       = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);

	for (i=0; i<M_list_str_len(json_keys); i++) {
		const char *key   = M_list_str_at(json_keys, i);
		const char *value = M_json_object_value_string(json_index, key);
		M_hash_dict_insert(srv->index_dict, key, value);
	}

	M_list_str_destroy(json_keys);

	M_event_add(g.el, srv->io_listen, test_server_listen_cb, srv);
	return srv;
}

static void test_server_stream_init(test_server_stream_t *stream)
{
	stream->out_buf         = M_buf_create();
	stream->in_parser       = M_parser_create(M_PARSER_FLAG_NONE);
	stream->httpr           = M_http_reader_create(NULL, M_HTTP_READER_NONE, NULL);
	stream->request_headers = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP | M_HASH_DICT_MULTI_VALUE);
}

static void test_server_stream_deinit(test_server_stream_t *stream)
{
	M_buf_cancel(stream->out_buf);
	M_parser_destroy(stream->in_parser);
	M_io_destroy(stream->io);
	M_http_reader_destroy(stream->httpr);
	M_hash_dict_destroy(stream->request_headers);

	stream->out_buf         = NULL;
	stream->in_parser       = NULL;
	stream->io              = NULL;
	stream->httpr           = NULL;
	stream->request_headers = NULL;
}

static void test_server_destroy(test_server_t *srv)
{
	size_t i;
	for (i=0; i<srv->stream_count; i++) {
		test_server_stream_deinit(&srv->streams[i]);
	}
	M_hash_dict_destroy(srv->index_dict);
	M_io_destroy(srv->io_listen);
	srv->io_listen = NULL;
	M_free(srv);
}

static void done_cb(M_net_error_t net_error, M_http_error_t http_error, const M_http_simple_read_t *simple, const char *error, void *thunk)
{
	test_args_t *args = thunk;
	const char  *body = (const char *)M_http_simple_read_body(simple, NULL);
	(void)net_error;
	(void)http_error;
	(void)error;
	if (M_str_eq(body, "<html><body><h1>It works!</h1></body></html>")) {
		args->is_success = M_TRUE;
	}
	M_event_done(g.el);
}

START_TEST(check_basic)
{
	test_server_args_t   srv_args  = { "basic" };
	test_args_t          args      = { 0 };
	test_server_t       *srv       = test_server_create(&srv_args);
	M_net_http_simple_t *hs        = M_net_http_simple_create(g.el, g.dns, done_cb);
	char                 url[]     = "http://localhost:99999";

	sprintf(url, "http://localhost:%hu", srv->port);
	M_net_http_simple_set_timeouts(hs, 2000, 0, 0);
	M_net_http_simple_set_message(hs, M_HTTP_METHOD_GET, NULL, "text/plain", "utf-8", NULL, NULL, 0);
	ck_assert_msg(M_net_http_simple_send(hs, url, &args), "Should send message");

	M_event_loop(g.el, M_TIMEOUT_INF);

	ck_assert_msg(args.is_success == M_TRUE, "Should have received 'It works!' HTML");

	test_server_destroy(srv);
	cleanup();
}
END_TEST

static Suite *net_http_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("net_http");

#define add_test(label,name) \
	tc = tcase_create(label); \
	tcase_add_test(tc, name); \
	suite_add_tcase(suite, tc);

#define add_test_timeout(label,name,timeout) \
	tc = tcase_create(label); \
	tcase_add_test(tc, name); \
	tcase_set_timeout(tc, timeout); \
	suite_add_tcase(suite, tc);

	add_test("basic", check_basic);

#undef add_test_timeout
#undef add_test
	return suite;
}

int main(int argc, char **argv)
{
	int         nf;
	SRunner    *sr;

	(void)argc;
	(void)argv;

	sr = srunner_create(net_http_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_net_http.log");

	g.el   = M_event_create(M_EVENT_FLAG_NONE);
	g.dns  = M_dns_create(g.el);
	g.ctx  = M_tls_clientctx_create();
	g.json = M_json_read(json_str, M_str_len(json_str), M_JSON_READER_NONE, NULL, NULL, NULL, NULL);

	M_tls_clientctx_set_default_trust(g.ctx);

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	cleanup_int();

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
