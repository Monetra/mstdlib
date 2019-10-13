#include "m_config.h"
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_text.h>


START_TEST(check_verify_domain)
{
	M_buf_t *buf = M_buf_create();
	int      i;
	
	ck_assert_msg(M_verify_domain("abc.def.g"));
	ck_assert_msg(M_verify_domain("a"));
	ck_assert_msg(M_verify_domain("abc"));
	ck_assert_msg(M_verify_domain("localserver"));
	ck_assert_msg(M_verify_domain("s.solutions"));
	ck_assert_msg(M_verify_domain("what-ever"));
	ck_assert_msg(M_verify_domain("what-ever.com"));
	ck_assert_msg(M_verify_domain("what-ever.cool-beans.com"));
	
	ck_assert_msg(!M_verify_domain("$$abc.def.g"));
	ck_assert_msg(!M_verify_domain("abc[]"));
	ck_assert_msg(!M_verify_domain("s..solutions"));
	ck_assert_msg(!M_verify_domain("-whatever"));
	ck_assert_msg(!M_verify_domain("whatever-"));
	ck_assert_msg(!M_verify_domain("some.website-.com"));
	ck_assert_msg(!M_verify_domain("some.-website.com"));
	ck_assert_msg(!M_verify_domain("some.-web-site.com"));
	ck_assert_msg(!M_verify_domain("some.-web-site-.com"));
	ck_assert_msg(!M_verify_domain("some.website.com-"));
	ck_assert_msg(!M_verify_domain("some.website.-com"));
	
	/* Check length limits. */
	M_buf_add_fill(buf, 'a', 63); /*max dns label (the sub field) length is 63 chars */
	ck_assert_msg(M_verify_domain(M_buf_peek(buf)));
	M_buf_add_byte(buf, '.');
	M_buf_add_fill(buf, 'a', 63);
	ck_assert_msg(M_verify_domain(M_buf_peek(buf)));
	M_buf_add_byte(buf, 'a');
	ck_assert_msg(!M_verify_domain(M_buf_peek(buf)));
	
	M_buf_drop(buf, M_buf_len(buf));
	for (i=0; i<3; i++) { /* 192 chars (max is 253) */
		M_buf_add_fill(buf, 'a', 63);
		M_buf_add_byte(buf, '.');
	}
	M_buf_add_fill(buf, 'a', 61); /* Now at exact max length. */
	ck_assert_msg(M_verify_domain(M_buf_peek(buf)));
	M_buf_add_byte(buf, 'a');
	ck_assert_msg(!M_verify_domain(M_buf_peek(buf)));
	
	M_buf_cancel(buf);
}
END_TEST


START_TEST(check_verify_email_address)
{
	M_buf_t *buf = M_buf_create();
	
	/* Our own tests (mostly related to display name). */
	ck_assert_msg(M_verify_email_address("user@example.com"));
	ck_assert_msg(M_verify_email_address("<user@example.com>"));
	ck_assert_msg(M_verify_email_address(" <user@example.com>"));
	ck_assert_msg(M_verify_email_address("John User <user@example.com>"));
	ck_assert_msg(M_verify_email_address("\"John Q. User\"  \t <user@example.com>"));
	ck_assert_msg(M_verify_email_address("\"John Q. User\"<user@example.com>"));
	ck_assert_msg(M_verify_email_address("\"User, John Q.\" <user@example.com>"));
	ck_assert_msg(M_verify_email_address("\"User, @[gs;] John <Q.>$\" <user@example.com>"));
	ck_assert_msg(M_verify_email_address("'*+-=^_.{}~@example.org"));
	ck_assert_msg(M_verify_email_address("\"<user@example.com>\"<user@example.com>"));
	ck_assert_msg(M_verify_email_address("\"Public, John Q. (Support)\" <jq..public+support@generic-server.com>"));
	
	ck_assert_msg(!M_verify_email_address(" user@example.com"));
	ck_assert_msg(!M_verify_email_address("user@example.com "));
	ck_assert_msg(!M_verify_email_address("<user@example.com"));
	ck_assert_msg(!M_verify_email_address("user@example.com>"));
	ck_assert_msg(!M_verify_email_address("user@example.com <>"));
	ck_assert_msg(!M_verify_email_address(">user@example.com<"));
	ck_assert_msg(!M_verify_email_address("\"user\"@example.com "));
	ck_assert_msg(!M_verify_email_address("John Q. User user@example.com"));
	ck_assert_msg(!M_verify_email_address("\"John Q. User\" user@example.com"));
	ck_assert_msg(!M_verify_email_address("<user@example.com> "));
	ck_assert_msg(!M_verify_email_address("\"\\\"Whassup\\\"\" <user@example.com>"));
	ck_assert_msg(!M_verify_email_address("\"Coolbeanz \r\n\"<user@whatever.com>"));
	ck_assert_msg(!M_verify_email_address("<user@whatever.com>\n"));
	
	/* Wikipedia examples (some of the ones they list as valid we chose not to allow). */
	ck_assert_msg(M_verify_email_address("disposable.stype.email.with+symbol@example.com"));
	ck_assert_msg(M_verify_email_address("other.email-with-dash@example.com"));
	ck_assert_msg(M_verify_email_address("x@example.com"));
	ck_assert_msg(M_verify_email_address("example-indeed@strange-example.com"));
	ck_assert_msg(M_verify_email_address("admin@mailserver1"));
	ck_assert_msg(M_verify_email_address("example@localhost"));
	ck_assert_msg(M_verify_email_address("example@s.solutions"));
	ck_assert_msg(M_verify_email_address("example@localserver"));
	ck_assert_msg(M_verify_email_address("example@tt"));
	ck_assert_msg(M_verify_email_address("john..doe@example.com"));
	
	ck_assert_msg(!M_verify_email_address("\"much.more unusual\"@example.com"));
	ck_assert_msg(!M_verify_email_address("#!$%&'*+-/=?^_`{}|~@example.org"));
	ck_assert_msg(!M_verify_email_address("\"()<>[]:,;@\\\"!#$%&'-/=?^_`{}| ~.a\"@example.org"));
	ck_assert_msg(!M_verify_email_address("\" \"@example.org"));
	ck_assert_msg(!M_verify_email_address("user@[IPv6:2001:DB8::1]"));
	ck_assert_msg(!M_verify_email_address("Abc.example.com"));
	ck_assert_msg(!M_verify_email_address("A@b@c@example.com"));
	ck_assert_msg(!M_verify_email_address("a\"b(c)d,e:f;g<h>i[j\\k]l@example.com"));
	ck_assert_msg(!M_verify_email_address("just\"not\"right@example.com"));
	ck_assert_msg(!M_verify_email_address("this is\"not\\allowed@example.com"));
	ck_assert_msg(!M_verify_email_address("this\\ still\\\"not\\\\allowed@example.com"));
	ck_assert_msg(!M_verify_email_address("1234567890123456789012345678901234567890123456789012345678901234+x@example.com"));
	ck_assert_msg(!M_verify_email_address("john.doe@example..com"));
	
	M_buf_cancel(buf);
}
END_TEST


START_TEST(check_verify_email_address_list)
{
	M_buf_t *buf = M_buf_create();
	char    *str = NULL;
	int      i;
	
	
	/* Make sure weird whitespace (esp. line endings) get rejected. */
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>\n", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>\r", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>\v", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>\f", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com> \n,  generic-user@acme.com", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com> \r , generic-user@acme.com", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>\v,   generic-user@acme.com", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	ck_assert_msg(!M_verify_email_address_list("<user@whatever.com>  \f, generic-user@acme.com", M_VERIFY_EMAIL_LISTDELIM_AUTO));
	
	/* Make sure an individual address (no delimiters) passes. */
	M_buf_add_str(buf, "<user@whatever.com>");
	ck_assert_msg(M_verify_email_address_list(M_buf_peek(buf), M_VERIFY_EMAIL_LISTDELIM_AUTO));
	
	/* Add 98 more recipients (one less than limit). */
	M_buf_add_str(buf, " , ");
	for (i=0; i<98; i++) {
		M_buf_add_str(buf, "\"User, @[gs;] John <Q.>$\" <user_25@example.com>");
		if (i < 97) {
			M_buf_add_str(buf, ",  ");
		}
	}
	ck_assert_msg(M_verify_email_address_list(M_buf_peek(buf), M_VERIFY_EMAIL_LISTDELIM_AUTO));
	str = M_strdup(M_buf_peek(buf));
	
	/* Add the 100th recipient, make sure everything still works. */
	M_buf_add_str(buf, " , user@example.com");
	ck_assert_msg(M_verify_email_address_list(M_buf_peek(buf), M_VERIFY_EMAIL_LISTDELIM_AUTO));
	
	/* Add the 101st recipient, make sure we fail. */
	M_buf_add_str(buf, " , <another-user@example.com>");
	
	/* Replace 100th recipient with a bad address. */
	M_buf_drop(buf, M_buf_len(buf));
	M_buf_add_str(buf, str);
	M_buf_add_str(buf, " , user$$$BL!ING$$@example.com");
	ck_assert_msg(!M_verify_email_address_list(M_buf_peek(buf), M_VERIFY_EMAIL_LISTDELIM_AUTO));
	
	/* Replace 100th recipient with a good address, but a bad delimiter. */
	M_buf_drop(buf, M_buf_len(buf));
	M_buf_add_str(buf, str);
	M_buf_add_str(buf, " ; user@example.com");
	ck_assert_msg(!M_verify_email_address_list(M_buf_peek(buf), M_VERIFY_EMAIL_LISTDELIM_AUTO));
	
	M_free(str);
	M_buf_cancel(buf);
}
END_TEST



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *M_ini_suite(void)
{
	Suite *suite;
	TCase *tc_verify_domain;
	TCase *tc_verify_email_address;
	TCase *tc_verify_email_address_list;
	
	suite = suite_create("verify");
	
	tc_verify_domain = tcase_create("check_verify_domain");
	tcase_add_test(tc_verify_domain, check_verify_domain);
	suite_add_tcase(suite, tc_verify_domain);
	
	tc_verify_email_address = tcase_create("check_verify_email_address");
	tcase_add_test(tc_verify_email_address, check_verify_email_address);
	suite_add_tcase(suite, tc_verify_email_address);
	
	tc_verify_email_address_list = tcase_create("check_verify_email_address_list");
	tcase_add_test(tc_verify_email_address_list, check_verify_email_address_list);
	suite_add_tcase(suite, tc_verify_email_address_list);
	
	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;
	
	(void)argc;
	(void)argv;
	
	if (!M_verify_email_address("'*+-=^_.{}~@example.org")) { /* DEBUG_161 */
		M_printf("*** BUG ***\n");
	}
	
	sr = srunner_create(M_ini_suite());
	srunner_set_log(sr, "check_verify.log");
	
	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (nf == 0)? EXIT_SUCCESS : EXIT_FAILURE;
}
