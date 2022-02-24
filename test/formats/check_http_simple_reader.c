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

#define ok_more_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Length: 44\r\n" \
	"Connection: close\r\n"\
	"Content-Type: text/html\r\n" \
 	"\r\n"  \
	"<html><b"
#define ok_more_data_result M_HTTP_ERROR_MOREDATA

START_TEST(check_read)
{
	M_http_simple_read_t *http;
	M_http_error_t        err;
	size_t                i;
	struct {
		const char     *data;
		M_http_error_t  res;
	} tests[] = {
		{ ok_data, ok_data_result           },
		{ ok_no_data, ok_no_data_result     },
		{ ok_more_data, ok_more_data_result },
		{ NULL, 0 }
	};

	for (i=0; tests[i].data!=NULL; i++ ) {
		err = M_http_simple_read(&http, (const unsigned char *)tests[i].data, M_str_len(tests[i].data), M_HTTP_SIMPLE_READ_NONE, NULL);

		ck_assert_msg(err == tests[i].res, "(%zu): result got '%d', expected '%d'", i, err, tests[i].res);

		M_http_simple_read_destroy(http);
	}
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

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_http_simple_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
