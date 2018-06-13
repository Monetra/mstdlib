#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


typedef struct {
	M_http_message_type_t  type;
	M_http_version_t       version;
	M_http_method_t        method;
	char                  *uri;
	M_uint32               code;
	char                  *reason;
	M_hash_dict_t         *headers;
	M_buf_t               *body;
	M_buf_t               *preamble;
	M_buf_t               *epilouge;
	M_list_str_t          *bpieces;
	M_hash_dict_t         *cextensions;
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

/* No Content length. Duplicate header. Header list. */
#define http2_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Type: text/html\r\n" \
	"dup_header: a\r\n" \
	"dup_header: b\r\n" \
	"dup_header: c\r\n" \
	"list_header: 1, 2, 3\r\n" \
 	"\r\n" \
	"<html><body><h1>It works!</h1></body></html>"

/* 1.0 GET request. */
#define http3_data "GET https://www.google.com/index.html HTTP/1.0\r\n" \
	"Host: www.google.com\r\n" \
	"\r\n"

/* 1.0 HEAD request no headers. */
#define http4_data "HEAD / HTTP/1.0\r\n\r\n"

/* Start with \r\n simulating multiple messages in a stream
 * where they are separated by a new line. Body is form encoded.
 * Ends with trailing \r\n that's not read. */
#define http5_data "\r\n" \
	"POST /login HTTP/1.1\r\n" \
	"Host: 127.0.0.1\r\n" \
	"Referer: https://127.0.0.1/login.html\r\n" \
	"Accept-Language: en-us\r\n" \
	"Content-Type: application/x-www-form-urlencoded\r\n" \
	"Accept-Encoding: gzip, deflate\r\n" \
	"User-Agent: Test Client\r\n" \
	"Content-Length: 37\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n" \
	"\r\n" \
	"User=For+Meeee&pw=ABC123&action=login" \
	"\r\n"

/* Chucked encoding. 1 chuck is headers as body
 * with extensions.
 * 1 chunk is header and data as body.
 * 1 chunk is body only. No trailers. */
#define http6_data "HTTP/1.1 200 OK\r\n" \
	"Transfer-Encoding: chunked\r\n" \
	"Content-Type: message/http\r\n" \
	"Connection: close\r\n" \
	"Server: server\r\n" \
	"\r\n" \
	"3a;ext1;ext2=abc\r\n" \
	"TRACE / HTTP/1.1\r\n" \
	"Connection: keep-alive\r\n" \
	"Host: google.com\r\n" \
	"40\r\n" \
	"\r\n" \
	"Content-Type: text/html\r\n" \
	"\r\n" \
	"<html><body>Chunk 2</body></html>\r\n" \
	"\r\n" \
	"21\r\n" \
	"<html><body>Chunk 3</body></html>\r\n" \
	"0\r\n" \
	"\r\n"

/* Chunked with trailer. */
#define http7_data "HTTP/1.1 200 OK\r\n" \
	"Transfer-Encoding: chunked\r\n" \
	"Content-Type: message/http\r\n" \
	"Connection: close\r\n" \
	"Server: server\r\n" \
	"\r\n" \
	"1F\r\n" \
	"<html><body>Chunk</body></html>\r\n" \
	"0\r\n" \
	"Trailer 1: I am a trailer\r\n" \
	"Trailer 2: Also a trailer\r\n" \
	"\r\n"

/* Multipart data. */
#define http8_data "POST /upload/data HTTP/1.1\r\n" \
	"Host: 127.0.0.1\r\n" \
	"Accept: image/gif, image/jpeg, */*\r\n" \
	"Accept-Language: en-us\r\n" \
	"Content-Type: multipart/form-data; boundary=---------------------------7d41b838504d8\r\n" \
	"Accept-Encoding: gzip, deflate\r\n" \
	"User-Agent: Test Client\r\n" \
	"Content-Length: 333\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n" \
	"\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"Content-Dispositio1: form-data; name=\"username\"\r\n" \
	"\r\n" \
	"For Meeee\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"Content-Dispositio2: form-data; name=\"fileID\"; filename=\"/temp.html\"\r\n" \
	"Content-Typ2: text/plain\r\n" \
	"\r\n" \
	"<h1>Home page on main server</h1>\r\n" \
	"-----------------------------7d41b838504d8--"

/* Multipart preamble and epilouge. */
#define http9_data "POST /upload/data HTTP/1.1\r\n" \
	"Content-Type: multipart/form-data; boundary=---------------------------7d41b838504d8\r\n" \
	"\r\n" \
	"preamble\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"\r\n" \
	"Part data\r\n" \
	"-----------------------------7d41b838504d8--\r\n" \
	"epilouge" \

/* 3 messages stacked into one stream. */
#define http10_data "HTTP/1.1 200 OK\r\n" \
	"Content-Length:9\r\n" \
	"\r\n" \
	"Message 1\r\n" \
	"\r\n" \
	"\r\n" \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length:9\r\n" \
	"\r\n" \
	"Message 2\r\n" \
	"HTTP/1.1 200 OK\r\n" \
	"\r\n" \
	"Message 3"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static httpr_test_t *httpr_test_create(void)
{
	httpr_test_t *ht;

	ht              = M_malloc_zero(sizeof(*ht));
	ht->headers     = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	ht->cextensions = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	ht->body        = M_buf_create();
	ht->preamble    = M_buf_create();
	ht->epilouge    = M_buf_create();
	ht->bpieces     = M_list_str_create(M_LIST_NONE);

	return ht;
}

static void httpr_test_destroy(httpr_test_t *ht)
{
	M_free(ht->uri);
	M_free(ht->reason);
	M_hash_dict_destroy(ht->headers);
	M_hash_dict_destroy(ht->cextensions);
	M_buf_cancel(ht->body);
	M_buf_cancel(ht->preamble);
	M_buf_cancel(ht->epilouge);
	M_list_str_destroy(ht->bpieces);
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

static M_http_error_t header_done_func(M_http_data_format_t format, void *thunk)
{
	(void)thunk;
	(void)format;
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
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_func(const char *key, const char *val, size_t idx, void *thunk)
{
	httpr_test_t *ht = thunk;

	(void)idx;

	M_hash_dict_insert(ht->cextensions, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_func(const unsigned char *data, size_t len, size_t idx, void *thunk)
{
	httpr_test_t *ht = thunk;
	M_buf_t      *buf;
	const char   *const_temp;
	char         *out;

	buf        = M_buf_create();
	const_temp = M_list_str_at(ht->bpieces, idx);
	M_buf_add_str(buf, const_temp);
	M_buf_add_bytes(buf, data, len);

	M_list_str_remove_at(ht->bpieces, idx);
	out = M_buf_finish_str(buf, NULL);
	M_list_str_insert_at(ht->bpieces, out, idx);
	M_free(out);

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_finished_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_buf_add_bytes(ht->preamble, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_header_func(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)idx;
	return header_func(key, val, thunk);
}

static M_http_error_t multipart_header_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_func(const unsigned char *data, size_t len, size_t idx, void *thunk)
{
	return chunk_data_func(data, len, idx, thunk);
}

static M_http_error_t multipart_data_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_finished_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_buf_add_bytes(ht->epilouge, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_func(const char *key, const char *val, void *thunk)
{
	return header_func(key, val, thunk);
}

static M_http_error_t trailer_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
		chunk_data_finished_func,
		multipart_preamble_func,
		multipart_preamble_done_func,
		multipart_header_func,
		multipart_header_done_func,
		multipart_data_func,
		multipart_data_done_func,
		multipart_data_finished_func,
		multipart_epilouge_func,
		multipart_epilouge_done_func,
		trailer_func,
		trailer_done_func
	};

	hr = M_http_reader_create(&cbs, M_HTTP_READER_NONE, thunk);
	return hr;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_httpr1)
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

START_TEST(check_httpr2)
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

START_TEST(check_httpr3)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http3_data, M_str_len(http3_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http3_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http3_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_GET, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_GET);
	ck_assert_msg(M_str_eq(ht->uri, "https://www.google.com/index.html"), "Wrong uri: got '%s', expected '%s'", ht->uri, "https://www.google.com/index.html");
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_0, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr4)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http4_data, M_str_len(http4_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http4_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http4_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_HEAD, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_HEAD);
	ck_assert_msg(M_str_eq(ht->uri, "/"), "Wrong uri: got '%s', expected '%s'", ht->uri, "/");
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_0, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr5)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *body = "User=For+Meeee&pw=ABC123&action=login";
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http5_data, M_str_len(http5_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http5_data)-2, "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http5_data)-2);

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_POST, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_POST);
	ck_assert_msg(M_str_eq(ht->uri, "/login"), "Wrong uri: got '%s', expected '%s'", ht->uri, "/login");
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	/* Headers. */
	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "application/x-www-form-urlencoded";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Body */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr6)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http6_data, M_str_len(http6_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http6_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http6_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");


	/* Headers. */
	key  = "Transfer-Encoding";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "chunked";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "message/http";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Chunks extensions. */
	key = "ext1";
	ck_assert_msg(M_hash_dict_get(ht->cextensions, key, &gval), "%s failed: Not found");
	ck_assert_msg(gval == NULL, "%s failed: got '%s', expected NULL", key, gval);

	key  = "ext2";
	gval = M_hash_dict_get_direct(ht->cextensions, key);
	eval = "abc";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Chunks data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 3, "Wrong number of chunks: got '%zu', expected '%d'", len, 3);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "TRACE / HTTP/1.1\r\nConnection: keep-alive\r\nHost: google.com";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", 0, gval, eval);

	gval = M_list_str_at(ht->bpieces, 1);
	eval = "\r\nContent-Type: text/html\r\n\r\n<html><body>Chunk 2</body></html>\r\n";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", 1, gval, eval);

	gval = M_list_str_at(ht->bpieces, 2);
	eval = "<html><body>Chunk 3</body></html>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", 2, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr7)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http7_data, M_str_len(http7_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http7_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http7_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");


	/* Trailers. */
	key  = "Trailer 1";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "I am a trailer";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Trailer 2";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "Also a trailer";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);


	/* Chunks data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 1, "Wrong number of chunks: got '%zu', expected '%d'", len, 1);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "<html><body>Chunk</body></html>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", 1, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr8)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http8_data, M_str_len(http8_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http8_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http8_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_POST, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_POST);
	ck_assert_msg(M_str_eq(ht->uri, "/upload/data"), "Wrong uri: got '%s', expected '%s'", ht->uri, "/upload/data");
	ck_assert_msg(ht->version == M_HTTP_VERSION_1_1, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	/* Part Headers. */
	key  = "Content-Dispositio1";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "form-data; name=\"username\"";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Typ2";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "text/plain";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Part data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 2, "Wrong number of parts: got '%zu', expected '%d'", len, 2);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "For Meeee";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", 1, gval, eval);

	gval = M_list_str_at(ht->bpieces, 1);
	eval = "<h1>Home page on main server</h1>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", 1, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr9)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http9_data, M_str_len(http9_data), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == M_str_len(http9_data), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http9_data));

	/* data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 1, "Wrong number of parts: got '%zu', expected '%d'", len, 1);

	gval = M_buf_peek(ht->preamble);
	eval = "preamble";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong preamble data: got '%s', expected '%s'", gval, eval);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "Part data";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", 0, gval, eval);

	gval = M_buf_peek(ht->epilouge);
	eval = "epilouge";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong epilouge data: got '%s', expected '%s'", gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr10)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len      = 0;
	size_t           len_read = 0;

	/* message 1. */
	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http10_data+len, M_str_len(http10_data)-len, &len_read);
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed message %d: %d", 1, res);
	len += len_read;

	gval = M_buf_peek(ht->body);
	eval = "Message 1";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 1, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);

	/* message 2. */
	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http10_data+len, M_str_len(http10_data)-len, &len_read);
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed message %d: %d", 2, res);
	len += len_read;

	gval = M_buf_peek(ht->body);
	eval = "Message 2";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 2, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);

	/* message 3. */
	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)http10_data+len, M_str_len(http10_data)-len, &len_read);
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed message %d: %d", 3, res);
	/*len += len_read;*/

	gval = M_buf_peek(ht->body);
	eval = "Message 3";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 3, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

#define do_query_check(URI, PARAMS, USE_PLUS, EXPECTED)\
do {\
	M_buf_t *buf;\
	char    *query;\
	buf = M_buf_create();\
	ck_assert_msg(M_http_add_query_string_buf(buf, URI, PARAMS, USE_PLUS), "Query string failed: expected '%s'", EXPECTED);\
	ck_assert_msg(M_str_eq(M_buf_peek(buf), EXPECTED), "Query buf does not match: got '%s', expected '%s'", M_buf_peek(buf), EXPECTED);\
	M_buf_cancel(buf);\
	query = M_http_add_query_string(URI, PARAMS, USE_PLUS);\
	ck_assert_msg(M_str_eq(query, EXPECTED), "Query string does not match: got '%s', expected '%s'", query, EXPECTED);\
	M_free(query);\
} while (0)

START_TEST(check_query_string)
{
	M_hash_dict_t *params = M_hash_dict_create(16, 75, M_HASH_DICT_MULTI_VALUE | M_HASH_DICT_KEYS_ORDERED);

	do_query_check("/cgi-bin/some_app", NULL, M_TRUE, "/cgi-bin/some_app");
	do_query_check("/cgi-bin/some_app", params, M_TRUE, "/cgi-bin/some_app");

	M_hash_dict_insert(params, "field 1", "value 1_1");
	M_hash_dict_insert(params, "field 1", "value 1_2");
	M_hash_dict_insert(params, "f2", "v2");
	M_hash_dict_insert(params, "f3", "v3");
	M_hash_dict_insert(params, "f4", "");

	do_query_check(NULL, params, M_FALSE, "?field%201=value%201_1&field%201=value%201_2&f2=v2&f3=v3");
	do_query_check(NULL, params, M_TRUE, "?field+1=value+1_1&field+1=value+1_2&f2=v2&f3=v3");

	do_query_check("/cgi-bin/some_app", params, M_FALSE,
		"/cgi-bin/some_app?field%201=value%201_1&field%201=value%201_2&f2=v2&f3=v3");
	do_query_check("/cgi-bin/some_app", params, M_TRUE,
		"/cgi-bin/some_app?field+1=value+1_1&field+1=value+1_2&f2=v2&f3=v3");

	M_hash_dict_destroy(params);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("http_reader");

	add_test(suite, check_httpr1);
	add_test(suite, check_httpr2);
	add_test(suite, check_httpr3);
	add_test(suite, check_httpr4);
	add_test(suite, check_httpr5);
	add_test(suite, check_httpr6);
	add_test(suite, check_httpr7);
	add_test(suite, check_httpr8);
	add_test(suite, check_httpr9);
	add_test(suite, check_httpr10);

	add_test(suite, check_query_string);

	sr = srunner_create(suite);
	srunner_set_log(sr, "check_http_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
