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
} emailr_test_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define test_data "a"

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

	M_printf("HEADER\t\t    '%s' : '%s'\n", key, val);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t address_func(const char *group, const char *name, const char *address, void *thunk)
{
	(void)thunk;

	M_printf("ADDRESS\t\t    '%s', '%s', '%s'\n", group, name, address);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t subject_func(const char *subject, void *thunk)
{
	(void)thunk;

	M_printf("SUBJECT\t\t    '%s'\n", subject);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t header_done_func(M_email_data_format_t format, void *thunk)
{
	(void)thunk;

	M_printf("HEADER DONE = format '%d'\n", format);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t body_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	M_printf("BODY = '%.*s\n", (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_preamble_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	M_printf("M PREAMBLE = '%.*s'\n", (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_preamble_done_func(void *thunk)
{
	(void)thunk;

	M_printf("M PREAMBLE DONE!!!\n");
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_func(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)thunk;

	M_printf("M HEADER (%"PRIu64")\t\t    '%s' : '%s'\n", idx, key, val);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_attachment_func(const char *content_type, const char *transfer_encoding, const char *filename, size_t idx, void *thunk)
{
	(void)thunk;

	M_printf("M (%"PRIu64") is ATTACHMENT:\t\t content type = '%s', transfer encoding = '%s', filename = '%s'\n", idx, content_type, transfer_encoding, filename);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_header_done_func(size_t idx, void *thunk)
{
	(void)thunk;

	M_printf("M HEADER (%"PRIu64") DONE!!!\n", idx);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_func(const char *data, size_t len, size_t idx, void *thunk)
{
	(void)thunk;

	M_printf("M BODY (%"PRIu64") = '%.*s'\n", idx, (int)len, data);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_done_func(size_t idx, void *thunk)
{
	(void)thunk;

	M_printf("M BODY (%"PRIu64") DONE!!!\n", idx);
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_data_finished_func(void *thunk)
{
	(void)thunk;

	M_printf("M DATA FINISHED!!!\n");
	return M_EMAIL_ERROR_SUCCESS;
}

static M_email_error_t multipart_epilouge_func(const char *data, size_t len, void *thunk)
{
	(void)thunk;

	M_printf("M EPILOUGE = '%.*s'\n", (int)len, data);
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

START_TEST(check_testing)
{
	M_email_reader_t *er;
	emailr_test_t    *et;
	M_email_error_t   res;
	size_t           len_read;

	et  = emailr_test_create();
	er  = gen_reader(et);
	res = M_email_reader_read(er, test_data, M_str_len(test_data), &len_read);
	M_printf("res = %d\n", res);

	emailr_test_destroy(et);
	M_email_reader_destroy(er);
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

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_email_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
