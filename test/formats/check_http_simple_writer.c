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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void remove_date(char *data, size_t *data_len, M_bool only_val)
{
	char   *pd;
	char   *pn;

	/* Remove the date from the Date: (leaving the bad Date: header) value because it's auto generated and will never match. */
	pd = M_str_str(data, "Date:");
	if (pd != NULL) {
		if (only_val) {
			pd += 5;
		}
		pn = M_str_str(pd, "\r\n");
		if (pn != NULL) {
			if (!only_val) {
				pn += 2;
			}
			*data_len = M_str_len(pn);
			M_mem_move(pd, pn, *data_len);
			pd[*data_len] = '\0';
			*data_len = M_str_len(data);
		}
	}
}

static void validate_output(char *out, size_t *out_len, const char *expected, size_t idx)
{
	M_http_error_t res;

	/* Remove the date from the Date: (leaving the bad Date: header) value because it's auto generated and will never match. */
	remove_date(out, out_len, M_TRUE);

	ck_assert_msg(M_str_eq(out, expected), "%zu: output does not match expected.\nGot:\n'%s'\n--\nExpected\n'%s'\n", idx, out, expected);

	/* Remove the Date: header line entirely because a header without a value will fail parsing. */
	remove_date(out, out_len, M_FALSE);

	/* Validate the output is readable. */
	res = M_http_simple_read(NULL, (const unsigned char *)out, *out_len, M_HTTP_SIMPLE_READ_NONE, NULL);
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS || res == M_HTTP_ERROR_MOREDATA, "%zu: Could not read output", idx);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define req_data_req1 "This is\n" \
	"data\n" \
	"\n\n" \
	"That I have"
#define req_data_rsp1 "GET /cgi/bin/blah HTTP/1.1\r\n" \
	"Host: example.com\r\n" \
	"User-Agent: simple-writer\r\n" \
	"Content-Length: 26\r\n" \
	"Content-Type: text/plain\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n" \
	"\n" \
	"That I have"

#define req_data_rsp2 "GET /cgi/bin/blah HTTP/1.1\r\n" \
	"Host: example.com\r\n" \
	"User-Agent: simple-writer\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: text/plain\r\n" \
	"Date:\r\n" \
	"\r\n"

#define req_data_rsp3  "GET /cgi/bin/blah HTTP/1.1\r\n" \
	"Host: example.com\r\n" \
	"User-Agent: simple-writer\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: text/plain; charset=utf-8\r\n" \
	"Date:\r\n" \
	"\r\n"

#define req_data_rsp4  "GET /cgi/bin/blah HTTP/1.1\r\n" \
	"Host: example.com\r\n" \
	"User-Agent: simple-writer\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: application/octet-stream\r\n" \
	"Date:\r\n" \
	"\r\n"

#define req_data_req5 "[ 4, 'float', { key: 'v1', key2: 1, key3: inf } ]"
#define req_data_rsp5 "POST / HTTP/1.1\r\n" \
	"Host: example2.com:443\r\n" \
	"User-Agent: swriter\r\n" \
	"Content-Length: 49\r\n" \
	"Content-Type: application/json; charset=utf-8\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"[ 4, 'float', { key: 'v1', key2: 1, key3: inf } ]"
#define req_data_rsp6 "PUT / HTTP/1.1\r\n" \
	"Host: example.com:443\r\n" \
	"User-Agent: swriter\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: application/octet-stream\r\n" \
	"Date:\r\n" \
	"\r\n"

#define req_data_req7 "test+123+%2B+done"
#define req_data_rsp7 "DELETE / HTTP/1.1\r\n" \
	"Host: e.com:7000\r\n" \
	"Content-Length: 17\r\n" \
	"Content-Type: application/x-www-form-urlencoded; charset=application/x-www-form-urlencoded\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"test+123+%2B+done"

#define req_data_req8 "test%20123%20+%20done"
#define req_data_rsp8 "DELETE / HTTP/1.1\r\n" \
	"Host: e.com:7000\r\n" \
	"Content-Length: 21\r\n" \
	"Content-Type: application/xml; charset=percent\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"test%20123%20+%20done"

#define req_data_req9 "test \r\n123 \r\n done\n+3\n\n\n"
#define req_data_rsp9 "CONNECT /no HTTP/1.1\r\n" \
	"Host: host.:999\r\n" \
	"Content-Length: 24\r\n" \
	"Content-Type: image/png; charset=latin_1\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"test \r\n123 \r\n done\n+3\n\n\n"

#define req_data_req10 "test \r\n123 \r\n done\n+3\n\n\n"
#define req_data_rsp10 "TRACE /no HTTP/1.1\r\n" \
	"Host: host.:999\r\n" \
	"Content-Length: 24\r\n" \
	"Content-Type: none; charset=cp1252\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"test \r\n123 \r\n done\n+3\n\n\n"

#define req_data_req11 "test \r\n123 \r\n done\n+3\n\n\n"
#define req_data_rsp11 "HEAD /80 HTTP/1.1\r\n" \
	"Host: .\r\n" \
	"User-Agent: 880088\r\n" \
	"Content-Length: 24\r\n" \
	"Content-Type: uh...\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"test \r\n123 \r\n done\n+3\n\n\n"

#define req_data_req12 "<xadaaaaaaaaaaaaa version=\"1.80\" xmlns=\"http://www.website.p.com/schema\" variable=\"2789393\">\n" \
"  <authentication>\n" \
"    <user>the user n1</user>\n" \
"    <password>8uio098i</password>\n" \
"  </authentication>\n" \
"  <8ut9adaetgaon id=\"8789087898976\" otherThingp=\"3895393\">\n" \
"    <ourxrId>10</ourxrId>\n" \
"    <num3sd>10100</num3sd>\n" \
"    <bingoSource>universee</bingoSource>\n" \
"    <PersonInfoess>\n" \
"      <name>John and Mary Smith</name>\n" \
"      <addressLine1>1 Main St.</addressLine1>\n" \
"      <zip>789763747</zip>\n" \
"    </PersonInfoess>\n" \
"    <ding>\n" \
"      <dddn>II</ddne>\n" \
"      <samber>890oaifdadfa398i</samber>\n" \
"      <driFter>A1B2</driFter>\n" \
"      <doingWorkForAaaaa>349</doingWorkForAaaaa>\n" \
"    </ding>\n" \
"    <p11>\n" \
"      <capability>what?!?</capability>\n" \
"      <gotoyMstd>still?!</gotoyMstd>\n" \
"      <somekindofId>anotherone?t</somekindofId>\n" \
"    </p11>\n" \
"    <allowXMLTagsNoww>false</allowXMLTagsNoww>\n" \
"    <howNoXMLToday>false</howNoXMLToday>\n" \
"  </8ut9adaetgaon>\n" \
"</xadaaaaaaaaaaaaa>"
#define req_data_rsp12 "POST /nab/communication/olliv HTTP/1.1\r\n" \
"Host: patterts.vaneerprednee.com:443\r\n" \
"User-Agent: the main user\r\n" \
"Content-Length: 936\r\n" \
"Content-Type: text/xml; charset=ascii\r\n" \
"Date:\r\n" \
"\r\n" \
"<xadaaaaaaaaaaaaa version=\"1.80\" xmlns=\"http://www.website.p.com/schema\" variable=\"2789393\">\n" \
"  <authentication>\n" \
"    <user>the user n1</user>\n" \
"    <password>8uio098i</password>\n" \
"  </authentication>\n" \
"  <8ut9adaetgaon id=\"8789087898976\" otherThingp=\"3895393\">\n" \
"    <ourxrId>10</ourxrId>\n" \
"    <num3sd>10100</num3sd>\n" \
"    <bingoSource>universee</bingoSource>\n" \
"    <PersonInfoess>\n" \
"      <name>John and Mary Smith</name>\n" \
"      <addressLine1>1 Main St.</addressLine1>\n" \
"      <zip>789763747</zip>\n" \
"    </PersonInfoess>\n" \
"    <ding>\n" \
"      <dddn>II</ddne>\n" \
"      <samber>890oaifdadfa398i</samber>\n" \
"      <driFter>A1B2</driFter>\n" \
"      <doingWorkForAaaaa>349</doingWorkForAaaaa>\n" \
"    </ding>\n" \
"    <p11>\n" \
"      <capability>what?!?</capability>\n" \
"      <gotoyMstd>still?!</gotoyMstd>\n" \
"      <somekindofId>anotherone?t</somekindofId>\n" \
"    </p11>\n" \
"    <allowXMLTagsNoww>false</allowXMLTagsNoww>\n" \
"    <howNoXMLToday>false</howNoXMLToday>\n" \
"  </8ut9adaetgaon>\n" \
"</xadaaaaaaaaaaaaa>"

START_TEST(check_request)
{
	char   *out;
	size_t  out_len;
	size_t  i;
	struct {
		M_http_method_t  method;
		const char      *host;
		unsigned short   port;
		const char      *uri;
		const char      *user_agent;
		const char      *content_type;
		const char      *data;
		const char      *charset;
		const char      *out;
	} params[] = {
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", "text/plain",                        req_data_req1,  NULL,                                                 req_data_rsp1  },
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", "text/plain",                        NULL,           "",                                                   req_data_rsp2  },
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", "text/plain",                        NULL,           "utf-8",                                              req_data_rsp3  },
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", NULL,                                "",             NULL,                                                 req_data_rsp4  },
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", "",                                  "",             M_textcodec_codec_to_str(M_TEXTCODEC_UTF8),           req_data_rsp3  },
		{ M_HTTP_METHOD_GET,     "example.com",                0,    "/cgi/bin/blah",            "simple-writer", NULL,                                "",             "utf-8",                                              req_data_rsp3  },
		{ M_HTTP_METHOD_POST,    "example2.com",               443,  "/",                        "swriter",       "application/json",                  req_data_req5,  "utf-8",                                              req_data_rsp5  },
		{ M_HTTP_METHOD_PUT,     "example.com",                443,  "/",                        "swriter",       "",                                  NULL,           "",                                                   req_data_rsp6  },
		{ M_HTTP_METHOD_DELETE,  "e.com",                      7000, NULL,                       NULL,            "application/x-www-form-urlencoded", req_data_req7,  M_textcodec_codec_to_str(M_TEXTCODEC_PERCENT_FORM),   req_data_rsp7  },
		{ M_HTTP_METHOD_DELETE,  "e.com",                      7000, "",                         NULL,            "application/xml",                   req_data_req8,  M_textcodec_codec_to_str(M_TEXTCODEC_PERCENT_URL),    req_data_rsp8  },
		{ M_HTTP_METHOD_OPTIONS, NULL,                         0,    "/did",                     NULL,            "text/html",                         req_data_req8,  M_textcodec_codec_to_str(M_TEXTCODEC_PERCENT_URLMIN), NULL           }, /* host is required so this will fail to structure. */
		{ M_HTTP_METHOD_CONNECT, "host.",                      999,  "/no",                      NULL,            "image/png",                         req_data_req9,  M_textcodec_codec_to_str(M_TEXTCODEC_ISO8859_1),      req_data_rsp9  }, /* Yes, the mime and contents don't match and this will say a PNG is using a text charset. If this was binary data, this would be very bad. */
		{ M_HTTP_METHOD_TRACE,   "host.",                      999,  "/no",                      NULL,            "none",                              req_data_req10, "cp1252",                                             req_data_rsp10 },
		{ M_HTTP_METHOD_HEAD,    ".",                          80,   "/80",                      "880088",        "uh...",                             req_data_req11, NULL,                                                 req_data_rsp11 },
		{ M_HTTP_METHOD_POST,    "patterts.vaneerprednee.com", 443,  "/nab/communication/olliv", "the main user", "text/xml",                          req_data_req12, "ascii",                                              req_data_rsp12 },
		{ M_HTTP_METHOD_UNKNOWN, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL }
	};

	for (i=0; params[i].method!=M_HTTP_METHOD_UNKNOWN; i++) {
		/* Generate our message. */
		out = (char *)M_http_simple_write_request(params[i].method,
				params[i].host, params[i].port, params[i].uri,
				params[i].user_agent, params[i].content_type, NULL /* Not testing custom headers. */,
				(const unsigned char *)params[i].data, M_str_len(params[i].data), params[i].charset, &out_len);

		/* Check if it was supposed to fail. */
		if (params[i].out == NULL) {
			ck_assert_msg(out == NULL, "%zu: output structured when expected failure.\nGot:\n'%s'\n", i, out);
			continue;
		}

		validate_output(out, &out_len, params[i].out, i);

		M_free(out);
	}
}
END_TEST

static M_hash_dict_t *check_request_headers_cb1(void)
{
	M_hash_dict_t *headers;

	headers = M_hash_dict_create(8, 16, M_HASH_DICT_KEYS_ORDERED);
	M_hash_dict_insert(headers, "ABC", "XYZ");
	M_hash_dict_insert(headers, "Val", "123");
	M_hash_dict_insert(headers, "val", "456");
	M_hash_dict_insert(headers, "C-V", "This is a test");

	return headers;
}
#define hreq_data_rsp1 "GET / HTTP/1.1\r\n" \
	"ABC: XYZ\r\n" \
	"Val: 123, 456\r\n" \
	"C-V: This is a test\r\n" \
	"Host: localhost:443\r\n" \
	"User-Agent: test\r\n" \
	"Content-Length: 26\r\n" \
	"Content-Type: t\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n" \
	"\n" \
	"That I have"

static M_hash_dict_t *check_request_headers_cb2(void)
{
	M_hash_dict_t *headers;

	headers = M_hash_dict_create(8, 16, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);
	M_hash_dict_insert(headers, "ABC", "XYZ");
	M_hash_dict_insert(headers, "val", "123");
	M_hash_dict_insert(headers, "val", "456");
	M_hash_dict_insert(headers, "Val", "456");
	M_hash_dict_insert(headers, "Val", "123");
	M_hash_dict_insert(headers, "Host", "1.2"); /* Will be ignored since overriding in call. */

	return headers;
}
#define hreq_data_rsp2 "GET / HTTP/1.1\r\n" \
	"ABC: XYZ\r\n" \
	"val: 123, 456\r\n" \
	"Host: localhost:443\r\n" \
	"User-Agent: test\r\n" \
	"Content-Length: 26\r\n" \
	"Content-Type: t\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n" \
	"\n" \
	"That I have"

static M_hash_dict_t *check_request_headers_cb3(void)
{
	M_hash_dict_t *headers;

	headers = M_hash_dict_create(8, 16, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_CASECMP|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	M_hash_dict_insert(headers, "ABC", "XYZ");
	M_hash_dict_insert(headers, "val", "123");
	M_hash_dict_insert(headers, "val", "456");
	M_hash_dict_insert(headers, "Val", "456");
	M_hash_dict_insert(headers, "Val", "789");
	M_hash_dict_insert(headers, "user-agent", "The checker");
	M_hash_dict_insert(headers, "Content-TYPe", "application/json");
	M_hash_dict_insert(headers, "Host", "l.internal:8080");

	return headers;
}
#define hreq_data_rsp3 "GET / HTTP/1.1\r\n" \
	"ABC: XYZ\r\n" \
	"val: 123, 456, 789\r\n" \
	"user-agent: The checker\r\n" \
	"Content-TYPe: application/json\r\n" \
	"Host: l.internal:8080\r\n" \
	"Content-Length: 26\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n" \
	"\n" \
	"That I have"

static M_hash_dict_t *check_request_headers_cb4(void)
{
	M_hash_dict_t *headers;

	headers = M_hash_dict_create(8, 16, M_HASH_DICT_KEYS_ORDERED);
	M_hash_dict_insert(headers, "Content-Length", "9430");

	return headers;
}
#define hreq_data_rsp4 "GET / HTTP/1.1\r\n" \
	"Content-Length: 9430\r\n" \
	"Host: localhost:443\r\n" \
	"User-Agent: test\r\n" \
	"Content-Type: t\r\n" \
	"Date:\r\n" \
	"\r\n"

static M_hash_dict_t *check_request_headers_cb5(void)
{
	M_hash_dict_t *headers;

	headers = M_hash_dict_create(8, 16, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);
	M_hash_dict_insert(headers, "Accept-Language", "en, mi");
	M_hash_dict_insert(headers, "Modifiers", "text/*; q=0.3; m=9, text/html; q=0.7, text/html; level=1, text/html; level=2; q=0.4, */*; q=0.5");

	return headers;
}
#define hreq_data_rsp5 "GET / HTTP/1.1\r\n" \
	"Accept-Language: en, mi\r\n" \
	"Modifiers: text/*; q=0.3; m=9, text/html; q=0.4; level=2, */*; q=0.5\r\n" \
	"Host: localhost:443\r\n" \
	"User-Agent: test\r\n" \
	"Content-Length: 26\r\n" \
	"Content-Type: t\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n" \
	"\n" \
	"That I have"

START_TEST(check_request_headers)
{
	M_hash_dict_t *headers;
	char          *out;
	size_t         out_len;
	size_t         i;
	struct {
		M_hash_dict_t *(*header_cb)(void);
		const char    *data;
		M_bool         use_defs;
		const char    *out;
	} params[] = {
		{ check_request_headers_cb1, req_data_req1, M_TRUE,  hreq_data_rsp1 },
		{ check_request_headers_cb2, req_data_req1, M_TRUE,  hreq_data_rsp2 },
		{ check_request_headers_cb3, req_data_req1, M_FALSE, hreq_data_rsp3 },
		{ check_request_headers_cb4, NULL,          M_TRUE,  hreq_data_rsp4 },
		{ check_request_headers_cb5, req_data_req1, M_TRUE,  hreq_data_rsp5 },
		{ NULL, NULL, M_FALSE, NULL }
	};

	for (i=0; params[i].header_cb!=NULL; i++) {

		headers = params[i].header_cb();

		/* Generate our message. */
		if (params[i].use_defs) {
			out = (char *)M_http_simple_write_request(M_HTTP_METHOD_GET,
					"localhost", 443, "/", "test", "t", headers,
					(const unsigned char *)params[i].data, M_str_len(params[i].data), NULL, &out_len);
		} else {
			out = (char *)M_http_simple_write_request(M_HTTP_METHOD_GET,
					NULL, 0, "/", NULL, NULL, headers,
					(const unsigned char *)req_data_req1, M_str_len(req_data_req1), "", &out_len);
		}

		validate_output(out, &out_len, params[i].out, i);

		M_free(out);
		M_hash_dict_destroy(headers);
	}
}
END_TEST

#define rsp_data_rsp1 "HTTP/1.1 200 OK\r\n" \
	"Content-Length: 26\r\n" \
	"Content-Type: application/json\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"This is\n" \
	"data\n" \
	"\n\n" \
	"That I have"

#define rsp_data_rsp2 "HTTP/1.1 201 OMG\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: application/octet-stream\r\n" \
	"Date:\r\n" \
	"\r\n"

#define rsp_data_rsp3 "HTTP/1.1 400 Bad Request\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: text/plain; charset=utf-8\r\n" \
	"Date:\r\n" \
	"\r\n"

#define rsp_data_rsp4 "HTTP/1.1 600 Generic\r\n" \
	"Content-Length: 49\r\n" \
	"Content-Type: text/plain; charset=utf-8\r\n" \
	"Date:\r\n" \
	"\r\n" \
	"[ 4, 'float', { key: 'v1', key2: 1, key3: inf } ]"

START_TEST(check_response)
{
	char   *out;
	size_t  out_len;
	size_t  i;
	struct {
		M_uint32    code;
		const char *reason;
		const char *content_type;
		const char *data;
		const char *charset;
		const char *out;
	} params[] = {
		{ 200, NULL,  "application/json", req_data_req1, NULL,    rsp_data_rsp1 },
		{ 201, "OMG", NULL,               NULL,          "",      rsp_data_rsp2 },
		{ 400, NULL,  "text/plain",       NULL,          "utf-8", rsp_data_rsp3 },
		{ 600, NULL,  NULL,               req_data_req5, "utf-8", rsp_data_rsp4 },
		{  0, NULL, NULL, NULL, NULL, NULL }
	};

	for (i=0; params[i].code!=0; i++) {
		/* Generate our message. */
		out = (char *)M_http_simple_write_response(params[i].code, params[i].reason,
			params[i].content_type, NULL, (const unsigned char *)params[i].data, M_str_len(params[i].data),
			params[i].charset, &out_len);

		/* Check if it was supposed to fail. */
		if (params[i].out == NULL && out != NULL) {
			ck_assert_msg(out == NULL, "%zu: output structured when expected failure.\nGot:\n'%s'\n", i, out);
			continue;
		}

		validate_output(out, &out_len, params[i].out, i);

		M_free(out);
	}
}
END_TEST

#define clen_rsp1 "HTTP/1.1 200 OK\r\n" \
	"Content-Length: 102\r\n" \
	"Content-Type: application/octet-stream\r\n" \
	"Date:\r\n" \
	"\r\n"

#define clen_rsp2 "HTTP/1.1 200 OK\r\n" \
	"Content-Length: 0\r\n" \
	"Content-Type: application/octet-stream\r\n" \
	"Date:\r\n" \
	"\r\n"

/* Content-Length from headers tested in check_request_headers. */
START_TEST(check_content_length)
{
	char   *out;
	size_t  out_len;
	size_t  i;
	struct {
		size_t      len;
		const char *out;
	} params[] = {
		{ 102, clen_rsp1 },
		{ 0,   clen_rsp2 },
		{ 0, NULL }
	};

	for (i=0; params[i].out!=NULL; i++) {
		/* Generate our message. */
		out = (char *)M_http_simple_write_response(200, NULL, NULL, NULL, NULL, params[i].len, NULL, &out_len);
		validate_output(out, &out_len, params[i].out, i);

		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("http_simple_writer");

	add_test(suite, check_request);
	add_test(suite, check_request_headers);
	add_test(suite, check_response);
	add_test(suite, check_content_length);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_http_simple_writer.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
