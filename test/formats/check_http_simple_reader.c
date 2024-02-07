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

#define ok_data "HTTP/1.1 200 OK\r\n" \
    "Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
    "Content-Length: 44\r\n" \
    "Connection: close\r\n"\
    "Content-Type: text/html\r\n" \
    "\r\n"  \
    "<html><body><h1>It works!</h1></body></html>"
#define ok_data_result M_HTTP_ERROR_SUCCESS

#define ok_no_data "HTTP/1.1 200 OK\r\n" \
    "Content-Length: 0\r\n" \
    "Content-Type: application/octet-stream\r\n" \
    "Date:\r\n" \
    "\r\n"
#define ok_no_data_result M_HTTP_ERROR_SUCCESS

#define ok_no_data_no_reason "HTTP/1.1 200 \r\n" \
    "Content-Length: 0\r\n" \
    "Content-Type: application/octet-stream\r\n" \
    "Date:\r\n" \
    "\r\n"
#define ok_no_data_no_reason_result M_HTTP_ERROR_SUCCESS

#define ok_no_data_bad_reason "HTTP/1.1 200\r\n" \
    "Content-Length: 0\r\n" \
    "Content-Type: application/octet-stream\r\n" \
    "Date:\r\n" \
    "\r\n"
#define ok_no_data_bad_reason_result M_HTTP_ERROR_STARTLINE_MALFORMED

#define ok_more_data "HTTP/1.1 200 OK\r\n" \
    "Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
    "Content-Length: 44\r\n" \
    "Connection: close\r\n"\
    "Content-Type: text/html\r\n" \
    "\r\n"  \
    "<html><b"
#define ok_more_data_result M_HTTP_ERROR_MOREDATA

#define charset_data "HTTP/1.1 200 OK\r\n" \
    "Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
    "Content-Length: 44\r\n" \
    "Connection: close\r\n"\
    "Content-Type: text/html; charset=ISO-8859-1\r\n" \
    "\r\n"  \
    "<html><body><h1>It\xA0works!</h1></body></html>"
#define charset_data_result M_HTTP_ERROR_SUCCESS

START_TEST(check_read)
{
    M_http_simple_read_t *http;
    M_http_error_t        err;
    size_t                i;
    struct {
        const char     *data;
        M_http_error_t  res;
    } tests[] = {
        { ok_data,               ok_data_result               },
        { ok_no_data,            ok_no_data_result            },
        { ok_no_data_no_reason,  ok_no_data_no_reason_result  },
        { ok_no_data_bad_reason, ok_no_data_bad_reason_result },
        { ok_more_data,          ok_more_data_result          },
        { charset_data,          charset_data_result          },
        { NULL, 0 }
    };

    for (i=0; tests[i].data!=NULL; i++ ) {
        err = M_http_simple_read(&http, (const unsigned char *)tests[i].data, M_str_len(tests[i].data), M_HTTP_SIMPLE_READ_NONE, NULL);

        ck_assert_msg(err == tests[i].res, "(%zu): result got '%d', expected '%d'", i, err, tests[i].res);

        M_http_simple_read_destroy(http);
    }
}
END_TEST

START_TEST(check_body_decode)
{
    M_http_simple_read_t *http;
    size_t                len;

    M_http_simple_read(&http, (const unsigned char *)charset_data, M_str_len(charset_data), M_HTTP_SIMPLE_READ_NONE, NULL);
    ck_assert_msg(M_http_simple_read_codec(http) == M_TEXTCODEC_UTF8, "Body codec is not utf-8 encoded, found (%d) '%s'", M_http_simple_read_codec(http), M_textcodec_codec_to_str(M_http_simple_read_codec(http)));
    ck_assert_msg(M_str_caseeq(M_http_simple_read_charset(http), "utf-8"), "Body char set is not to utf-8, found '%s'", M_http_simple_read_charset(http));
    ck_assert_msg(M_str_caseeq((const char *)M_http_simple_read_body(http, &len), "<html><body><h1>It\xC2\xA0works!</h1></body></html>"), "Body does match body");
    ck_assert_msg(len == 45, "len is '45' but should be '%zu'", len);
    M_http_simple_read_destroy(http);

    M_http_simple_read(&http, (const unsigned char *)charset_data, M_str_len(charset_data), M_HTTP_SIMPLE_READ_NODECODE_BODY, NULL);
    ck_assert_msg(M_http_simple_read_codec(http) == M_TEXTCODEC_ISO8859_1, "No decode body codec is not utf-8 encoded, found (%d) '%s'", M_http_simple_read_codec(http), M_textcodec_codec_to_str(M_http_simple_read_codec(http)));
    ck_assert_msg(M_str_caseeq(M_http_simple_read_charset(http), "ISO-8859-1"), "No decode body char set is not set to ISO-8859-1, found '%s'", M_http_simple_read_charset(http));
    ck_assert_msg(M_str_caseeq((const char *)M_http_simple_read_body(http, &len), "<html><body><h1>It\xA0works!</h1></body></html>"), "no decode body does match body");
    ck_assert_msg(len == 44, "len is '44' but should be '%zu'", len);
    M_http_simple_read_destroy(http);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
    Suite   *suite;
    SRunner *sr;
    int      nf;

    suite = suite_create("http_simple_reader");

    add_test(suite, check_read);
    add_test(suite, check_body_decode);

    sr = srunner_create(suite);
    if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_http_simple_reader.log");

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
