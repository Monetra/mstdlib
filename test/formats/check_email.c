#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

#define DEBUG_INFO_ADDRESS_MAP 0

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define add_test(SUITENAME, TESTNAME)\
do {\
	TCase *tc;\
	tc = tcase_create(#TESTNAME);\
	tcase_add_test(tc, TESTNAME);\
	suite_add_tcase(SUITENAME, tc);\
} while (0)

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_testing)
{
	M_email_t        *email;
	char             *out;
	M_email_error_t   res;
	size_t            len;
	size_t            len_read = 0;
	const char       *test_data = "a";

	len = M_str_len(test_data);
	res = M_email_simple_read(&email, test_data, len, M_EMAIL_SIMPLE_READ_NONE, &len_read);
	M_printf("res = %d\n", res);
	M_printf("len = %zu, len_read = %zu\n", M_str_len(test_data), len_read);

	out = M_email_simple_write(email);
	M_printf("WRITE:\n'%s'\n", out);
	M_free(out);

	M_email_destroy(email);
}
END_TEST

START_TEST(check_date)
{
	M_email_t           *email = M_email_create();
	const char          *date;
	const M_hash_dict_t *headers;

	M_email_date(email, NULL);
	headers = M_email_headers(email);
	date = M_hash_dict_get_direct(headers, "Date");

	ck_assert_msg(date != NULL, "date header should exist");

	M_email_destroy(email);
}
END_TEST

START_TEST(check_messageid)
{
	M_email_t           *email = M_email_create();
	const char          *messageid;
	const M_hash_dict_t *headers;

	M_email_messageid(email, "<MONETRA.", "@mydomain.com>");
	headers = M_email_headers(email);
	messageid = M_hash_dict_get_direct(headers, "Message-ID");

	ck_assert_msg(messageid != NULL, "date header should exist");

	M_email_destroy(email);
}
END_TEST

START_TEST(check_splitting)
{
	M_hash_dict_t    *headers = NULL;
	char             *body    = NULL;
	char             *out     = NULL;
	M_email_error_t   res;
	const char       *test_data = "a";


	res = M_email_simple_split_header_body(test_data, &headers, &body);
	M_printf("res: %d\n", res);

	out = M_hash_dict_serialize(headers, ';', '=', '\"', '\\', M_HASH_DICT_SER_FLAG_NONE);
	M_printf("HEADERS:\n'''\n%s\n'''\n", out);
	M_printf("BODY:\n'''\n%s\n'''\n", body);

	M_free(body);
	M_hash_dict_destroy(headers);

}
END_TEST

START_TEST(check_mixed_alternate)
{
	const char       *test_data = \
"To: test@localhost\r\n" \
"From: test@localhost\r\n" \
"MIME-Version: 1.0\r\n" \
"Content-Type: multipart/alternative; boundary=\"DTGHJ678IJDA-242_S124\"\r\n" \
"\r\n" \
"--DTGHJ678IJDA-242_S124\r\n" \
"Content-Type: text/plain; charset=\"utf-8\"\r\n" \
"Content-Transfer-Encoding: 7bit\r\n" \
"\r\n" \
"Status:       SUCCESS\r\n" \
"\r\n" \
"--DTGHJ678IJDA-242_S124\r\n" \
"Content-type: text/html; charset=\"us-ascii\"\r\n" \
"Content-Transfer-Encoding: 7bit\r\n" \
"\r\n" \
"<html>\r\n" \
"<head></head>\r\n" \
"<body>\r\n" \
"</body>\r\n" \
"</html>\r\n" \
"\r\n" \
"--DTGHJ678IJDA-242_S124--\r\n" \
"";
	char             *out     = NULL;
	M_email_error_t   eer;
	M_email_t        *e;
	size_t            len;

	eer = M_email_simple_read(&e, test_data, M_str_len(test_data), M_EMAIL_SIMPLE_READ_NONE, &len);

	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Should return M_EMAIL_ERROR_SUCCESS");
	ck_assert_msg(len == M_str_len(test_data), "Should have read the entire message");
	out = M_email_simple_write(e);
	ck_assert_msg(out != NULL, "Should have written message");
	/* 1 text/plain + 1 text/html = 2 */
	ck_assert_msg(M_email_parts_len(e) == 2, "Should have 2 parts.  Has %zu", M_email_parts_len(e));

	M_email_destroy(e);
	M_free(out);
}
END_TEST

START_TEST(check_mixed_multipart)
{
	const char       *test_data =
"To: test@localhost\r\n"
"From: test@localhost\r\n"
"MIME-Version: 1.0\r\n"
"Content-Type: multipart/mixed; boundary=\"A2DX_654FDAD-BSDA\"\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA\r\n"
"Content-Type: multipart/alternative; boundary=\"DTGHJ678IJDA-242_S124\"\r\n"
"\r\n"
"--DTGHJ678IJDA-242_S124\r\n"
"Content-Type: text/plain; charset=\"utf-8\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"Status:       SUCCESS\r\n"
"\r\n"
"--DTGHJ678IJDA-242_S124\r\n"
"Content-type: text/html; charset=\"us-ascii\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"\r\n"
"<html>\r\n"
"<head></head>\r\n"
"<body>\r\n"
"</body>\r\n"
"</html>\r\n"
"\r\n"
"--DTGHJ678IJDA-242_S124--\r\n"
"\r\n"
"--A2DX_654FDAD-BSDA\r\n"
"Content-Type: text/text; charset=\"utf-8\"; name=\"1_details.csv\"\r\n"
"Content-Transfer-Encoding: 7bit\r\n"
"Content-Disposition: attachment; size=\"85\"; filename=\"1_details.csv\"\r\n"
"\r\n"
"type,card,account,amount,ordernum,status\r\n"
"SALE,MC,5454,19.92,T218769465123080,Success\r\n"
"--A2DX_654FDAD-BSDA--\r\n"
"";
	char             *out     = NULL;
	M_email_error_t   eer;
	M_email_t        *e;
	size_t            len;

	eer = M_email_simple_read(&e, test_data, M_str_len(test_data), M_EMAIL_SIMPLE_READ_NONE, &len);

	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Should return M_EMAIL_ERROR_SUCCESS");
	ck_assert_msg(len == M_str_len(test_data), "Should have read the entire message");
	out = M_email_simple_write(e);
	ck_assert_msg(out != NULL, "Should have written message");
	/* 1 multipart/alternative + 1 text/plain + 1 text/html + 1 attachment = 4 */
	ck_assert_msg(M_email_parts_len(e) == 4, "Should have 4 parts.  Has %zu", M_email_parts_len(e));

	M_email_destroy(e);
	M_free(out);
}
END_TEST


START_TEST(check_addresses)
{
	size_t i;
	struct {
		const char      *address;
		M_email_error_t  eer;
	} test[] = {
		{ "<fully@qualified.com"         , M_EMAIL_ERROR_SUCCESS },
		{ "Fred@here.com, Zeke@there.com", M_EMAIL_ERROR_SUCCESS },
		{ "Fred@here.com; Zeke@there.com", M_EMAIL_ERROR_SUCCESS },
		{ "Fred@invalid"                 , M_EMAIL_ERROR_SUCCESS },
		{ "Mal <mal@formed.com"          , M_EMAIL_ERROR_SUCCESS },
		{ "Mal < mal@formed.com >"       , M_EMAIL_ERROR_SUCCESS },
		{ "Mal <mal>"                    , M_EMAIL_ERROR_ADDRESS },
		{ "Mal <>"                       , M_EMAIL_ERROR_ADDRESS },
	};

	const char *test_data =
"From: monetra@mydomain.com\r\n"
"Date: Wed, 06 Jul 2022 16:03:34 -0400\r\n"
"MIME-Version: 1.0\r\n"
"To: %s\r\n"
"Content-Type: multipart/alternative; boundary=\"------------j7eBAXe8KOcZ@cBNaMqQbJlx3uGM\"\r\n"
"Subject: Urgent Monetra Notification : START\r\n"
"\r\n"
"--------------j7eBAXe8KOcZ@cBNaMqQbJlx3uGM\r\n"
"Content-Type: TEXT/PLAIN\r\n"
"\r\n"
"Monetra has been successfully started\r\n"
"\r\n"
"\r\n"
"--------------j7eBAXe8KOcZ@cBNaMqQbJlx3uGM--\r\n";
	for (i=0; i<sizeof(test)/sizeof(test[0]); i++) {
		char            *msg;
		char            *msg2;
		M_email_t       *e;
		M_email_error_t  eer;
#if DEBUG_INFO_ADDRESS_MAP
		M_hash_dict_t   *headers;
#endif

		M_asprintf(&msg, test_data, test[i].address);
#if DEBUG_INFO_ADDRESS_MAP
		M_email_simple_split_header_body(msg, &headers, NULL);
		M_printf("%s\n", M_hash_dict_get_direct(headers, "To"));
		M_hash_dict_destroy(headers);
#endif
		eer = M_email_simple_read(&e, msg, M_str_len(msg), M_EMAIL_SIMPLE_READ_NONE, NULL);
		ck_assert_msg(eer == test[i].eer, "Expected %d, got %d", test[i].eer, eer);
		msg2 = M_email_simple_write(e);
#if DEBUG_INFO_ADDRESS_MAP
		M_email_simple_split_header_body(msg2, &headers, NULL);
		M_printf("|->%s\n", M_hash_dict_get_direct(headers, "To"));
		M_hash_dict_destroy(headers);
#endif
		M_email_destroy(e);
		M_free(msg);
		M_free(msg2);
	}

}
END_TEST

START_TEST(check_unecessary_first_multipart)
{
	M_hash_dict_t   *headers  = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	M_hash_dict_t   *headers0 = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	M_hash_dict_t   *headers1 = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	M_hash_dict_t   *headers2 = M_hash_dict_create(16, 75, M_HASH_DICT_NONE);
	M_email_t       *e        = M_email_create();
	const char      *data     = "data";
	char            *msg      = NULL;
	M_email_t       *e_test   = NULL;
	M_email_error_t  eer;

	M_hash_dict_insert(headers, "From", "from@localhost");
	M_hash_dict_insert(headers, "Message-ID", "<message-id>");
	M_hash_dict_insert(headers, "MIME-Version", "1.0");
	M_hash_dict_insert(headers, "Date", "Thu, 17 Nov 2022 13:10:56 -0500");
	M_hash_dict_insert(headers, "To", "to@localhost");
	M_hash_dict_insert(headers, "Content-Type", "multipart/alternative; boundary=\"------------QNN54Ad4VRImGnteNAiMT6ZVgoFk\"");
	M_hash_dict_insert(headers, "Subject", "subject");


	M_hash_dict_insert(headers0, "Content-Type", "multipart/alternative; boundary=\"------------QNN54Ad4VRImGnteNAiMT6ZVgoFk\"");

	M_hash_dict_insert(headers1, "Content-Type", "text/plain; charset=\"utf-8\"");
	M_hash_dict_insert(headers1, "Content-Transfer-Encoding", "7bit");

	M_hash_dict_insert(headers2, "Content-Type", "text/html; charset=\"us-ascii\"");
	M_hash_dict_insert(headers2, "Content-Transfer-Encoding", "7bit");

	M_email_set_headers(e, headers);
	M_email_part_append(e, data, M_str_len(data), headers0, NULL);
	M_email_part_append(e, data, M_str_len(data), headers1, NULL);
	M_email_part_append(e, data, M_str_len(data), headers2, NULL);

	msg = M_email_simple_write(e);
	eer = M_email_simple_read(&e_test, msg, M_str_len(msg), M_EMAIL_SIMPLE_READ_NONE, NULL);
	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Failed to read email message after writing");
	M_hash_dict_destroy(headers);
	M_hash_dict_destroy(headers0);
	M_hash_dict_destroy(headers1);
	M_hash_dict_destroy(headers2);
	M_free(msg);
	M_email_destroy(e);
	M_email_destroy(e_test);
}
END_TEST

START_TEST(check_bcc_scrubbed)
{
	M_email_error_t   eer;
	M_email_t        *e;
	char             *msg;
	const char       *test_data = \
"From: test@localhost\r\n" \
"Bcc: bcc@localhot\r\n" \
"MIME-Version: 1.0\r\n" \
"Content-Type: text/plain; charset=\"utf-8\"\r\n" \
"Content-Transfer-Encoding: 7bit\r\n" \
"\r\n" \
"FTX token price is going back up, right?";
	eer = M_email_simple_read(&e, test_data, M_str_len(test_data), M_EMAIL_SIMPLE_READ_NONE, NULL);
	ck_assert_msg(eer == M_EMAIL_ERROR_SUCCESS, "Should successfully read test data");
	msg = M_email_simple_write(e);
	ck_assert_msg(M_str_str(msg, "FTX token price is going back up, right?") != NULL, "Should write out message");
	M_free(msg);
	M_email_bcc_clear(e);
	msg = M_email_simple_write(e);
	ck_assert_msg(M_str_str(msg, "FTX token price is going back up, right?") != NULL, "Should write out message");
	M_free(msg);
	M_email_destroy(e);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("email");

	add_test(suite, check_testing);
	add_test(suite, check_splitting);
	add_test(suite, check_mixed_alternate);
	add_test(suite, check_mixed_multipart);
	add_test(suite, check_addresses);
	add_test(suite, check_date);
	add_test(suite, check_messageid);
	add_test(suite, check_unecessary_first_multipart);
	add_test(suite, check_bcc_scrubbed);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_email.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

