#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>
#include "formats/xml/m_xml_entities.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_xml_entities_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define MAX_DECODED_LEN 1024
#define MAX_ENCODED_LEN (MAX_DECODED_LEN<<1)

static struct xml_entity_test {
	const char decoded[MAX_DECODED_LEN];
	const char encoded[MAX_ENCODED_LEN];
} xml_entity_tests[] = {
	{ ""       , ""                          },
	{ "'"      , "&apos;"                    },
	{ "<>&\""  , "&lt;&gt;&amp;&quot;"       },
	{ "a<a<a<" , "a&lt;a&lt;a&lt;"           },
	/* - - */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_xml_entities_xml_entity_tests)
{
	char *r1;
	char *r2;
	const char *encoded = xml_entity_tests[_i].encoded;
	const char *decoded = xml_entity_tests[_i].decoded;

	r1 = M_xml_entities_decode(encoded, M_str_len(encoded));
	ck_assert_msg(r1 != NULL, "decoded \"%s\" to \"%s\", expected \"%s\"", encoded, r1, decoded);
	ck_assert_msg(M_str_eq(r1, decoded), "'%s' != '%s'", r1, decoded);

	r2 = M_xml_entities_encode(decoded, M_str_len(decoded));
	ck_assert_msg(M_str_eq(r2, encoded) , "encoded \"%s\" to \"%s\", expected \"%s\"", decoded, r2, encoded);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_xml_entities_suite(void)
{
	Suite *suite;
	TCase *tc_xml_entity;
	
	suite = suite_create("xml_entities");

	tc_xml_entity = tcase_create("xml_entity");
	tcase_add_loop_test(tc_xml_entity, check_xml_entities_xml_entity_tests, 0, sizeof(xml_entity_tests)/sizeof(*xml_entity_tests));
	suite_add_tcase(suite, tc_xml_entity);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(M_xml_entities_suite());
	srunner_set_log(sr, "check_xml_entities.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
