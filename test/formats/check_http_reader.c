#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
	M_http_message_type_t  type;
	M_http_version_t       version;
	M_http_method_t        method;
	char                  *uri;
	M_uint32               code;
	char                  *reason;
	M_hash_dict_t         *headers;
	M_buf_t               *body;
} httpr_test_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Content length provided. */
#define http1_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Length: 44\r\n" \
	"Connection: close\r\n"\
	"Content-Type: text/html\r\n" \
 	"\r\n"  \
	"<html><body><h1>It works!</h1></body></html>"

/* No Content length. Duplicate header. */
#define http2_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Type: text/html\r\n" \
	"dup_header: a\r\n" \
	"dup_header: b\r\n" \
	"dup_header: c\r\n" \
	"list_header: 1, 2, 3\r\n" \
 	"\r\n" \
	"<html><body><h1>It works!</h1></body></html>"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static httpr_test_t *httpr_test_create(void)
{
	httpr_test_t *ht;

	ht          = M_malloc_zero(sizeof(*ht));
	ht->headers = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	ht->body    = M_buf_create();

	return ht;
}

static void httpr_test_destroy(httpr_test_t *ht)
{
	M_free(ht->uri);
	M_free(ht->reason);
	M_hash_dict_destroy(ht->headers);
	M_buf_cancel(ht->body);
	M_free(ht);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t start_func(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	httpr_test_t *ht = thunk;

	ht->type    = type;
	ht->version = version;
	if (type == M_HTTP_MESSAGE_TYPE_REQUEST) {
		ht->method = method;
		ht->uri    = M_strdup(uri);
	} else if (type == M_HTTP_MESSAGE_TYPE_RESPONSE) {
		ht->code   = code;
		ht->reason = M_strdup(reason);
	} else {
		return M_HTTP_ERROR_USER_FAILURE;
	}
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t header_func(const char *key, const char *val, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_hash_dict_insert(ht->headers, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t header_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t body_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_buf_add_bytes(ht->body, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t body_done_func(void *thunk)
{
	httpr_test_t *ht = thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_func(const char *key, const char *val, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_done_func(void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_func(const unsigned char *data, size_t len, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_done_func(void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_func(const unsigned char *data, size_t len, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_done_func(void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_header_func(const char *key, const char *val, size_t part_idx, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_header_done_func(size_t part_idx, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_func(const unsigned char *data, size_t len, size_t part_idx, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_done_func(size_t part_idx, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_func(const unsigned char *data, size_t len, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_done_func(void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_func(const char *key, const char *val, void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_done_func(void *thunk)
{
	return M_HTTP_ERROR_SUCCESS;
}


static M_http_reader_t *gen_reader(void *thunk)
{
	M_http_reader_t *hr;
	struct M_http_reader_callbacks cbs = {
		start_func,
		header_func,
		header_done_func,
		body_func,
		body_done_func,
		chunk_extensions_func,
		chunk_extensions_done_func,
		chunk_data_func,
		chunk_data_done_func,
		multipart_preamble_func,
		multipart_preamble_done_func,
		multipart_header_func,
		multipart_header_done_func,
		multipart_data_func,
		multipart_data_done_func,
		multipart_epilouge_func,
		multipart_epilouge_done_func,
		trailer_func,
		trailer_done_func
	};

	hr = M_http_reader_create(&cbs, thunk);
	return hr;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_httpr_1)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *key;
	const char      *gval;
	const char      *eval;
	const char      *body = "<html><body><h1>It works!</h1></body></html>";

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http1_data, M_str_len(http1_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http1_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http1_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");

	/* Headers. */
	key  = "Date";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "Mon, 7 May 2018 01:02:03 GMT";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Length";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "44";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Connection";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "close";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "text/html";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Body. */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr_2)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *body = "<html><body><h1>It works!</h1></body></html>";
	const char      *key;
	const char      *gval;
	const char      *eval;
	size_t           len;
	size_t           i;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http2_data, M_str_len(http2_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http2_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http2_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");

	/* Headers. */
	key = "dup_header";
	ck_assert_msg(M_hash_dict_multi_len(ht->headers, "dup_header", &len), "No duplicate headers found");
	ck_assert_msg(len == 3, "Wrong length of duplicate headers got '%zu', expected '%zu", len, 3);
	for (i=0; i<len; i++) {
		gval = M_hash_dict_multi_get_direct(ht->headers, key, i);
		if (i == 0) {
			eval = "a";
		} else if (i == 1) {
			eval = "b";
		} else {
			eval = "c";
		}
		ck_assert_msg(M_str_eq(gval, eval), "%s (%zu) failed: got '%s', expected '%s'", key, i, gval, eval);
	}

	key = "list_header";
	ck_assert_msg(M_hash_dict_multi_len(ht->headers, "dup_header", &len), "No duplicate headers found");
	ck_assert_msg(len == 3, "Wrong length of duplicate headers got '%zu', expected '%zu", len, 3);
	for (i=0; i<len; i++) {
		gval = M_hash_dict_multi_get_direct(ht->headers, key, i);
		if (i == 0) {
			eval = "1";
		} else if (i == 1) {
			eval = "2";
		} else {
			eval = "3";
		}
		ck_assert_msg(M_str_eq(gval, eval), "%s (%zu) failed: got '%s', expected '%s'", key, i, gval, eval);
	}

	/* Body. */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *gen_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("http_reader");

	tc = tcase_create("httpr_1");
	tcase_add_test(tc, check_httpr_1);
	suite_add_tcase(suite, tc);

	tc = tcase_create("httpr_2");
	tcase_add_test(tc, check_httpr_2);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(gen_suite());
	srunner_set_log(sr, "http_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
