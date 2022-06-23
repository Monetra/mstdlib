#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

#define DEBUG 0

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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)


typedef struct {
	M_bool one_member;
} emailr_test_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static emailr_test_t *emailr_test_create(void)
{
	emailr_test_t *et;

	et = M_malloc_zero(sizeof(*et));

	return et;
}

static void emailr_test_destroy(emailr_test_t *et)
{
	M_free(et);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t header_func(const char *key, const char *val, void *thunk)
{
	(void)thunk;

	event_debug("HEADER\t\t    '%s' : '%s'\n", key, val);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t address_func(const char *group, const char *name, const char *address, void *thunk)
{
	(void)thunk;

	event_debug("ADDRESS\t\t    '%s', '%s', '%s'\n", group, name, address);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t subject_func(const char *subject, void *thunk)
{
	(void)thunk;

	event_debug("SUBJECT\t\t    '%s'\n", subject);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t header_done_func(M_email_data_format_t format, void *thunk)
{
	(void)thunk;

	event_debug("HEADER DONE = format '%d'\n", format);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t body_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	event_debug("BODY = '%.*s\n", (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_preamble_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	event_debug("M PREAMBLE = '%.*s'\n", (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_preamble_done_func(void *thunk)
{
	(void)thunk;

	event_debug("M PREAMBLE DONE!!!\n");
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_func(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)thunk;

	event_debug("M HEADER (%zu)\t\t    '%s' : '%s'\n", idx, key, val);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_attachment_func(const char *content_type, const char *transfer_encoding, const char *filename, size_t idx, void *thunk)
{
	(void)thunk;

	event_debug("M (%zu) is ATTACHMENT:\t\t content type = '%s', transfer encoding = '%s', filename = '%s'\n", idx, content_type, transfer_encoding, filename);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_done_func(size_t idx, void *thunk)
{
	(void)thunk;

	event_debug("M HEADER (%zu) DONE!!!\n", idx);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_func(const char *data, size_t len, size_t idx, void *thunk)
{
	(void)thunk;

	event_debug("M BODY (%zu) = '%.*s'\n", idx, (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_done_func(size_t idx, void *thunk)
{
	(void)thunk;

	event_debug("M BODY (%zu) DONE!!!\n", idx);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_finished_func(void *thunk)
{
	(void)thunk;

	event_debug("M DATA FINISHED!!!\n");
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_epilouge_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	event_debug("M EPILOUGE = '%.*s'\n", (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_reader_t *gen_reader(void *thunk)
{
	M_email_reader_t *er;
	struct M_email_reader_callbacks cbs = {
		header_func,
		address_func,
		address_func,
		address_func,
		address_func,
		address_func,
		subject_func,
		header_done_func,
		body_func,
		multipart_preamble_func,
		multipart_preamble_done_func,
		multipart_header_func,
		multipart_header_attachment_func,
		multipart_header_done_func,
		multipart_data_func,
		multipart_data_done_func,
		multipart_data_finished_func,
		multipart_epilouge_func
	};

	er = M_email_reader_create(&cbs, M_EMAIL_READER_NONE, thunk);
	return er;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_email_error_t email_test(const char *test_data)
{
	M_email_reader_t *er;
	emailr_test_t    *et;
	M_email_error_t   res;
	size_t            len_read;

	et  = emailr_test_create();
	er  = gen_reader(et);
	res = M_email_reader_read(er, test_data, M_str_len(test_data), &len_read);

	emailr_test_destroy(et);
	M_email_reader_destroy(er);

	return res;
}

START_TEST(check_testing)
{
	M_email_error_t  eer;
	const char      *test_data = "a";

	eer = email_test(test_data);
	ck_assert_msg(eer == M_EMAIL_ERROR_MOREDATA, "Should require more data");
}
END_TEST

START_TEST(check_boundary)
{
	M_email_error_t   eer;
	const char       *test_data =

"Content-Type: multipart/alternative; boundary=\"A2DX_654FDAD-BSDA\"\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA--\r\n"
"\r\n";

	eer = email_test(test_data);
	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Should succeed at parsing message");
}
END_TEST

START_TEST(check_Boundary)
{
	M_email_error_t   eer;
	const char       *test_data =

"Content-Type: multipart/alternative; Boundary=\"A2DX_654FDAD-BSDA\"\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA--\r\n"
"\r\n";

	eer = email_test(test_data);
	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Should succeed at parsing message");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("email_reader");

	add_test(suite, check_testing);
	add_test(suite, check_boundary);
	add_test(suite, check_Boundary);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_email_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
