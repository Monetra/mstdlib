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

#define XML1 "<?xml encoding=\"UTF-8\" version=\"1.0\"?>" \
"<doc>" \
"  <e1   /><e2   ></e2><e3   name = \"elem3\" />" \
"  <e5>" \
"    <e6>" \
"      <e7>abc</e7>" \
"    </e6>" \
"  </e5>" \
"</doc>"

#define XML1_OUT_NONE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
"<doc>" \
"<e1/>" \
"<e2/>" \
"<e3 name=\"elem3\"/>" \
"<e5>" \
"<e6>" \
"<e7>abc</e7>" \
"</e6>" \
"</e5>" \
"</doc>"

#define XML1_OUT_SPACE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
"<doc>\n" \
"  <e1 />\n" \
"  <e2 />\n" \
"  <e3 name=\"elem3\" />\n" \
"  <e5>\n" \
"    <e6>\n" \
"      <e7>abc</e7>\n" \
"    </e6>\n" \
"  </e5>\n" \
"</doc>"

#define XML1_OUT_TAB "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
"<doc>\n" \
"\t<e1/>\n" \
"\t<e2/>\n" \
"\t<e3 name=\"elem3\"/>\n" \
"\t<e5>\n" \
"\t\t<e6>\n" \
"\t\t\t<e7>abc</e7>\n" \
"\t\t</e6>\n" \
"\t</e5>\n" \
"</doc>"

#define XML2 "<MonetraTrans>\n" \
"\t<Trans identifier=\"1\">\n" \
"\t\t<username>loopback</username>\n" \
"\t\t<account>5454545454545454</account>\n" \
"\t\t<action>sale</action>\n" \
"\t\t<!-- comment of many words -->\n" \
"\t\t<amount>1.00</amount>\n" \
"\t\t<ordernum>123</ordernum>\n" \
"\t\t<ordernum>456</ordernum>\n" \
"\t\t<custref>\n" \
"\t\t\tabc\n" \
"\t\t\t<!-- comment to break text into two nodes -->\n" \
"\t\t\tdef\n" \
"\t\t</custref>\n" \
"\t</Trans>\n" \
"\t<Trans identifier=\"2\">\n" \
"\t\t<username>loopback2</username>\n" \
"\t\t<account>4111111111111111</account>\n" \
"\t\t<!-- Another comment of many words -->\n" \
"\t\t<action>return</action>\n" \
"\t\t<amount>19.11</amount>\n" \
"\t\t<ordernum>789</ordernum>\n" \
"\t</Trans>\n" \
"\t<s:blah xmlns:s=\"http://ns\">\n" \
"\t\t<s:header>\n" \
"\t\t\t<a:Action xmlns:a=\"http://ns2\">PLAY</a:Action>\n" \
"\t\t</s:header>\n" \
"\t</s:blah>\n" \
"\t<multi>1</multi>\n" \
"\t<multi>2</multi>\n" \
"\t<multi>3</multi>\n" \
"\t<multi>4</multi>\n" \
"</MonetraTrans>"

#define XML3 "<MonetraResp>\n" \
"\t<DataTransferStatus code=\"SUCCESS\"/>\n" \
"\t<Resp identifier=\"1\">\n" \
"\t\t<timestamp>1396546585</timestamp>\n" \
"\t\t<cardtype>MC</cardtype>\n" \
"\t\t<msoft_code>INT_SUCCESS</msoft_code>\n" \
"\t\t<phard_code>SUCCESS</phard_code>\n" \
"\t\t<auth>338363</auth>\n" \
"\t\t<ttid>28</ttid>\n" \
"\t\t<verbiage>APPROVED</verbiage>\n" \
"\t\t<batch>1</batch>\n" \
"\t\t<account>XXXXXXXXXXXX5454</account>\n" \
"\t</Resp>\n" \
"</MonetraResp>"

#define XML4 "<r><tag1><!-- Comment -->abc<!-- Comment --></tag1><tag2><!-- again !-->123</tag2><tag3>xyz<!-- 1 --></tag3></r>"

#define XML4_OUT_NOCOMMENT "<r><tag1>abc</tag1><tag2>123</tag2><tag3>xyz</tag3></r>"

#define XML5 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
"<doc>\n" \
"\t<e1>\n" \
"\t\tabc\n" \
"\t\t<sub/>\n" \
"\t\txyz\n" \
"\t</e1>\n" \
"\t<e2>\n" \
"\t\tdef\n" \
"\t\t<!-- comment -->\n" \
"\t\tqrs\n" \
"\t</e2>\n" \
"\t<e3>123456</e3>\n" \
"</doc>"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_xml_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char *data;
	const char *out;
	M_uint32    in_flags;
	M_uint32    out_flags;
} check_xml_valid_data[] = {
	{ XML1,      XML1_OUT_NONE,      M_XML_READER_NONE,            M_XML_WRITER_NONE                                 },
	{ XML1,      XML1_OUT_SPACE,     M_XML_READER_NONE,
	    M_XML_WRITER_PRETTYPRINT_SPACE|M_XML_WRITER_SELFCLOSE_SPACE                                                  },
	{ XML1,      XML1_OUT_TAB,       M_XML_READER_NONE,            M_XML_WRITER_PRETTYPRINT_TAB                      },
	{ XML2,      XML2,               M_XML_READER_NONE,            M_XML_WRITER_PRETTYPRINT_TAB                      },
	{ XML3,      XML3,               M_XML_READER_NONE,            M_XML_WRITER_PRETTYPRINT_TAB                      },
	{ XML4,      XML4_OUT_NOCOMMENT, M_XML_READER_NONE,            M_XML_WRITER_IGNORE_COMMENTS                      },
	{ XML4,      XML4_OUT_NOCOMMENT, M_XML_READER_IGNORE_COMMENTS, M_XML_WRITER_NONE                                 },
	{ XML5,      XML5,               M_XML_READER_NONE,            M_XML_WRITER_PRETTYPRINT_TAB                      },
	{ "<a>b</A>", "<a>b</a>",        M_XML_READER_TAG_CASECMP,     M_XML_WRITER_LOWER_TAGS                           },
	{ "<A>b</A>", "<a>b</a>",        M_XML_READER_NONE,            M_XML_WRITER_LOWER_TAGS                           },
	{ "<a A=\"b\">b</a>",         "<a a=\"b\">b</a>",
	    M_XML_READER_NONE, M_XML_WRITER_LOWER_ATTRS                                                                  },
	{ "<a a=\"&amp;\">&amp;</a>", "<a a=\"&amp;\">&amp;</a>",
	    M_XML_READER_NONE, M_XML_WRITER_NONE                                                                         },
	{ "<a a=\"&amp;\">&amp;</a>", "<a a=\"&\">&</a>",
	    M_XML_READER_NONE, M_XML_WRITER_DONT_ENCODE_ATTRS|M_XML_WRITER_DONT_ENCODE_TEXT                              },
	{ "<a a=\"&amp;\">&amp;</a>", "<a a=\"&amp;\">&amp;</a>",
	    M_XML_READER_DONT_DECODE_ATTRS|M_XML_READER_DONT_DECODE_TEXT,
	   M_XML_WRITER_DONT_ENCODE_ATTRS|M_XML_WRITER_DONT_ENCODE_TEXT                                                  },
	{ "<a a=\"&amp;\">&amp;</a>", "<a a=\"&amp;amp;\">&amp;amp;</a>",
	    M_XML_READER_DONT_DECODE_ATTRS|M_XML_READER_DONT_DECODE_TEXT, M_XML_WRITER_NONE                              },
	{ "<a><b>x</b>\r\n</a>", "<a><b>x</b></a>",
	    M_XML_READER_DONT_DECODE_ATTRS, M_XML_WRITER_NONE                                                            },
	{ "<a><b>x</b>\r\n</a>", "<a><b>x</b></a>",
	    M_XML_READER_NONE, M_XML_WRITER_NONE                                                                         },
	{ "<a><b>x&#xD;</b>\r\n</a>", "<a><b>x&amp;#xD;</b></a>",
	    M_XML_READER_DONT_DECODE_TEXT, M_XML_WRITER_NONE                                                             },
	{ "<a><b>x&#xD;</b></a>", "<a><b>x\r</b></a>",
	    M_XML_READER_NONE, M_XML_WRITER_NONE                                                                         },
	{ "<a><b>x</b>&#xD;</a>", "<a><b>x</b>\r</a>",
	    M_XML_READER_NONE, M_XML_WRITER_NONE                                                                         },
	{ "\x7f\x0a\x3c 123>a\x7f\x0a\x3c/\x20 123 >",
	  "\x7f\x3c" "123>a\x7f\x3c" "/123>",
	    M_XML_READER_NONE, M_XML_WRITER_NONE                                                                         },

	{ NULL, NULL, 0, 0 }
};

START_TEST(check_xml_valid)
{
	M_xml_node_t  *x;
	char          *out;
	M_buf_t       *buf;
	M_bool         buf_ok;
	M_xml_error_t  eh;
	size_t         eh_line;
	size_t         eh_pos;
	size_t         i;

	buf = M_buf_create();

	for (i=0; check_xml_valid_data[i].data!=NULL; i++) {
		x = M_xml_read(check_xml_valid_data[i].data, M_str_len(check_xml_valid_data[i].data), check_xml_valid_data[i].in_flags, NULL, &eh, &eh_line, &eh_pos);
		ck_assert_msg(x != NULL, "XML (%lu) could not be parsed: error=%d, line=%lu, pos=%lu\nxml='%s'", i, eh, eh_line, eh_pos, check_xml_valid_data[i].data);
		if (check_xml_valid_data[i].out != NULL) {
			out    = M_xml_write(x, check_xml_valid_data[i].out_flags, NULL);
			buf_ok = M_xml_write_buf(buf, x, check_xml_valid_data[i].out_flags);
			ck_assert_msg(M_str_eq(out, check_xml_valid_data[i].out), "Output not as expected (%lu):\ngot='%s'\nexpected='%s'", i, out, check_xml_valid_data[i].out);
			ck_assert_msg(buf_ok, "Buf write failed (%lu):\nexpected='%s'", i, check_xml_valid_data[i].out);
			ck_assert_msg(M_str_eq(M_buf_peek(buf), check_xml_valid_data[i].out), "Output not as expected (%lu):\ngot='%s'\nexpected='%s'", i, M_buf_peek(buf), check_xml_valid_data[i].out);
			M_free(out);
			M_buf_truncate(buf, 0);
		}

		M_xml_node_destroy(x);
	}

	M_buf_cancel(buf);
}
END_TEST

static struct {
	const char    *data;
	M_xml_error_t  error;
} check_xml_invalid_data[] = {
	{ "<x",                      M_XML_ERROR_MISSING_CLOSE_TAG                  },
	{ "<d><b></b>",              M_XML_ERROR_MISSING_CLOSE_TAG                  },
	{ "<a attr=\"abc>text</a>",  M_XML_ERROR_MISSING_CLOSE_TAG                  },
	{ "<d>abc</b>",              M_XML_ERROR_UNEXPECTED_CLOSE                   },
	{ "<a t1=\"1\" t1=\"2\" />", M_XML_ERROR_ATTR_EXISTS                        },
	{ "<!DOCTYPE html>",         M_XML_ERROR_NO_ELEMENTS                        },
	{ "<>",                      M_XML_ERROR_INVALID_START_TAG                  },
	{ "<!>",                     M_XML_ERROR_INVALID_START_TAG                  },
	{ "<?xml>",                  M_XML_ERROR_MISSING_PROCESSING_INSTRUCTION_END },
	{ "<a></A>",                 M_XML_ERROR_UNEXPECTED_CLOSE                   },
	{ "<a></a><b></b>",          M_XML_ERROR_EXPECTED_END                       },
	{ "\x7f\x0a\x65\x65\x65\x67\x74\x79\x70\x3c\x21\x20\x2d\x2d\x0a\x2d",  M_XML_ERROR_MISSING_CLOSE_TAG },
	{ "\x7f\x0a\x65\x65\x65\x67\x74\x79\x70\x3c\x21\x20\x2d\x2d\x0a\x2d>", M_XML_ERROR_MISSING_CLOSE_TAG },
	{ "\x7f\x0a\x3c 123>a<\x7f\x0a\x3c/\x20 123 >",                        M_XML_ERROR_MISSING_CLOSE_TAG },
	{ NULL, 0 }
};

START_TEST(check_xml_invalid)
{
	M_xml_node_t  *x;
	M_xml_error_t  eh;
	size_t         i;

	for (i=0; check_xml_invalid_data[i].data!=NULL; i++) {
		x = M_xml_read(check_xml_invalid_data[i].data, M_str_len(check_xml_invalid_data[i].data), M_XML_READER_NONE, NULL, &eh, NULL, NULL);
		ck_assert_msg(x == NULL, "Invalid xml (%lu) parsed successfully", i);
		ck_assert_msg(eh == check_xml_invalid_data[i].error, "Invalid xml (%lu) error incorrect. got=%d, expected=%d", i, eh, check_xml_invalid_data[i].error);
		M_xml_node_destroy(x);
	}
}
END_TEST

static struct {
	const char *search;
	size_t      num_matches;
	int         match_num;
	const char *match_text_val;
	const char *match_attr_key;
	const char *match_attr_val;
} check_xml_xpath_data[] = {
	{ "/MonetraTrans/Trans/account",           2,  0, "5454545454545454", NULL,         NULL },
	{ "MonetraTrans/Trans/account",            2,  1, "4111111111111111", NULL,         NULL },
	{ "/MonetraTrans/Trans[1]/account",        1,  0, "5454545454545454", NULL,         NULL },
	{ "MonetraTrans/Trans[2]/account",         1,  0, "4111111111111111", NULL,         NULL },
	{ "/MonetraTrans/Trans[1]/account/text()", 1,  0, "5454545454545454", NULL,         NULL },
	{ "MonetraTrans/Trans[2]/account/text()",  1,  0, "4111111111111111", NULL,         NULL },
	{ "MonetraTrans//account",                 2,  0, "5454545454545454", NULL,         NULL },
	{ "MonetraTrans//account",                 2,  1, "4111111111111111", NULL,         NULL },
	{ "MonetraTrans//account/text()",          2,  0, "5454545454545454", NULL,         NULL },
	{ "MonetraTrans//account/text()",          2,  1, "4111111111111111", NULL,         NULL },
	{ "//custref//text()",                     2,  0, "abc",              NULL,         NULL },
	{ "//custref//text()",                     2,  1, "def",              NULL,         NULL },
	{ "//custref/text()",                      2,  0, "abc",              NULL,         NULL },
	{ "//custref/text()",                      2,  1, "def",              NULL,         NULL },
	{ "//custref/text()[1]",                   1,  0, "abc",              NULL,         NULL },
	{ "//custref/text()[2]",                   1,  0, "def",              NULL,         NULL },
	{ "//account",                             2,  0, "5454545454545454", NULL,         NULL },
	{ "//account",                             2,  1, "4111111111111111", NULL,         NULL },
	{ "//account/..",                          2,  0, NULL,               "identifier", "1"  },
	{ "//account/..",                          2,  1, NULL,               "identifier", "2"  },
	{ "/MonetraTrans/Trans",                   2,  0, NULL,               "identifier", "1"  },
	{ "/MonetraTrans/Trans",                   2,  1, NULL,               "identifier", "2"  },
	{ "/*:MonetraTrans/Trans",                 2,  1, NULL,               "identifier", "2"  },
	{ "./MonetraTrans/Trans",                  2,  1, NULL,               "identifier", "2"  },
	{ "./*:MonetraTrans/*:Trans",              2,  1, NULL,               "identifier", "2"  },
	{ "MonetraTrans/Trans",                    2,  0, NULL,               "identifier", "1"  },
	{ "MonetraTrans/Trans",                    2,  1, NULL,               "identifier", "2"  },
	{ "MonetraTrans//Trans",                   2,  0, NULL,               "identifier", "1"  },
	{ "MonetraTrans//Trans",                   2,  1, NULL,               "identifier", "2"  },
	{ "MonetraTrans//*:Trans",                 2,  1, NULL,               "identifier", "2"  },
	{ "*:MonetraTrans//Trans",                 2,  1, NULL,               "identifier", "2"  },
	{ "//Trans",                               2,  0, NULL,               "identifier", "1"  },
	{ "//Trans",                               2,  1, NULL,               "identifier", "2"  },
	{ "//Trans[@*]",                           2,  0, NULL,               "identifier", "1"  },
	{ "//Trans[@*]",                           2,  1, NULL,               "identifier", "2"  },
	{ "//Trans[@identifier]",                  2,  0, NULL,               "identifier", "1"  },
	{ "//Trans[@identifier]",                  2,  1, NULL,               "identifier", "2"  },
	{ "//Trans[@identifier=1]",                1,  0, NULL,               "identifier", "1"  },
	{ "//Trans[@identifier='1']",              1,  0, NULL,               "identifier", "1"  },
	{ "//Trans[@identifier=\"1\"]",            1,  0, NULL,               "identifier", "1"  },
	{ "//Trans[@identifier=\"a\"]",            0,  0, NULL,               NULL,         NULL },
	{ "//Trans[@*][@identifier=1]",            1,  0, NULL,               "identifier", "1"  },
	{ "//Trans[1][@*][@identifier=1]",         1,  0, NULL,               "identifier", "1"  },
	{ "//Trans[1][@*][@identifier=2]",         0,  0, NULL,               NULL,         NULL },
	{ "//Trans/ordernum[1]",                   2,  0, "123",              NULL,         NULL },
	{ "//*:Trans/ordernum[1]",                 2,  0, "123",              NULL,         NULL },
	{ "//Trans/ordernum[1]",                   2,  1, "789",              NULL,         NULL },
	{ "//Trans/ordernum[2]",                   1,  0, "456",              NULL,         NULL },
	{ "//Trans/ordernum[last()]",              2,  0, "456",              NULL,         NULL },
	{ "//Trans/ordernum[last()]",              2,  1, "789",              NULL,         NULL },
	{ "//Trans/ordernum[last()-1]",            1,  0, "123",              NULL,         NULL },
	{ "//Trans/ordernum[1]/text()",            2,  0, "123",              NULL,         NULL },
	{ "//Trans/ordernum[1]/text()",            2,  1, "789",              NULL,         NULL },
	{ "//Trans/ordernum[2]/text()",            1,  0, "456",              NULL,         NULL },
	{ "//ordernum",                            3,  0, NULL,               NULL,         NULL },
	/* Appears in two Trans groups. */
	{ "//ordernum[1]",                         2,  0, "123",              NULL,         NULL },
	/* Appears in two Trans groups but -1 means last-1, so first group has two which is [1] and
 	 * second group has 1 which is [0] (DNE). */
	{ "//ordernum[-1]",                        1,  0, "123",              NULL,         NULL },
	{ "//ordernum[- 1]",                       1,  0, "123",              NULL,         NULL },
	{ "//ordernum[-2]",                        0,  0, NULL,               NULL,         NULL },
	{ "//ordernum[-3]",                        0,  0,  NULL,              NULL,         NULL },
	{ "//ordernum[-5]",                        0,  0,  NULL,              NULL,         NULL },
	{ "//ordernum[2]",                         1,  0, "456",              NULL,         NULL },
	{ "//ordernum[3]",                         0,  0, NULL,               NULL,         NULL },
	{ "//ordernum[9]",                         0,  0,  NULL,              NULL,         NULL },
	{ "//ordernum[last()]",                    2,  0, "456",              NULL,         NULL },
	{ "//ordernum[last()]",                    2,  1, "789",              NULL,         NULL },
	{ "//ordernum[last()-1]",                  1,  0, "123",              NULL,         NULL },
	{ "//ordernum[last() - 1]",                1,  0, "123",              NULL,         NULL },
	{ "//ordernum[last()+1]",                  0,  0,  NULL,              NULL,         NULL },
	{ "//ordernum[last()+ 1]",                 0,  0,  NULL,              NULL,         NULL },
	{ "MonetraTrans//ordernum[1]",             2,  0, "123",              NULL,         NULL },
	{ "MonetraTrans//ordernum[1]",             2,  1, "789",              NULL,         NULL },
	{ "MonetraTrans//ordernum[2]",             1,  0, "456",              NULL,         NULL },
	{ "MonetraTrans//ordernum[last()]",        2,  0, "456",              NULL,         NULL },
	{ "MonetraTrans//ordernum[last()]",        2,  1, "789",              NULL,         NULL },
	{ "MonetraTrans//ordernum[last()-1]",      1,  0, "123",              NULL,         NULL },
	{ "//Trans[1]",                            1,  0, NULL,               "identifier", "1"  },
	{ "//*/text()",                            18, 0, "loopback",         NULL,         NULL },
	{ "//*/text()[1]",                         17, 0, NULL,               NULL,         NULL },
	{ "//*/text()[2]",                         1,  0, "def",              NULL,         NULL },
	{ "//*[1]/text()",                         3,  0, "loopback",         NULL,         NULL },
	{ "//*[2]/text()",                         2,  0, "5454545454545454", NULL,         NULL },
	{ "//*[last()+1]",                         0,  0, NULL,               NULL,         NULL },
	{ "//s:blah/s:header/*:Action",            1,  0, NULL,               NULL,         NULL },
	{ "//s:blah/*:header/a:Action",            1,  0, NULL,               NULL,         NULL },
	{ "//*:blah/s:header/*:Action",            1,  0, NULL,               NULL,         NULL },
	{ "//*:blah/*:header/*:Action",            1,  0, NULL,               NULL,         NULL },
	{ "//s:blah//*:Action",                    1,  0, NULL,               NULL,         NULL },
	{ "//s:blah//a:Action",                    1,  0, NULL,               NULL,         NULL },
	{ "//*:blah//a:Action",                    1,  0, NULL,               NULL,         NULL },
	{ "//*:blah//*:Action",                    1,  0, NULL,               NULL,         NULL },
	{ "//s:blah/s:header/*:Action/text()",     1,  0, "PLAY",             NULL,         NULL },
	{ "//s:blah/*:header/a:Action/text()",     1,  0, "PLAY",             NULL,         NULL },
	{ "//*:blah/s:header/*:Action/text()",     1,  0, "PLAY",             NULL,         NULL },
	{ "//*:blah/*:header/*:Action/text()",     1,  0, "PLAY",             NULL,         NULL },
	{ "//s:blah//*:Action/text()",             1,  0, "PLAY",             NULL,         NULL },
	{ "//s:blah//a:Action/text()",             1,  0, "PLAY",             NULL,         NULL },
	{ "//*:blah//a:Action/text()",             1,  0, "PLAY",             NULL,         NULL },
	{ "//*:blah//*:Action/text()",             1,  0, "PLAY",             NULL,         NULL },
	{ "//multi[position() = 3]",               1,  0, "3",                NULL,         NULL },
	{ "//multi[position() <= 2]",              2,  1, "2",                NULL,         NULL },
	{ "//multi[position() >= 1]",              4,  0, "1",                NULL,         NULL },
	{ "//multi[position() < 2]",               1,  0, "1",                NULL,         NULL },
	{ "//multi[position() > 3]",               1,  0, "4",                NULL,         NULL },
	{ "//multi[position() >= 4]",              1,  0, "4",                NULL,         NULL },
	{ "//multi[position() < 4]",               3,  2, "3",                NULL,         NULL },
	{ "//multi[position() < last()]",          3,  1, "2",                NULL,         NULL },
	{ "//multi[position() < last()-1]",        2,  1, "2",                NULL,         NULL },
	{ "//multi[position() < last()-2]",        1,  0, "1",                NULL,         NULL },
	{ "//multi[position() < last()-3]",        0,  0, NULL,               NULL,         NULL },
	{ "//multi[position() <= last()]",         4,  3, "4",                NULL,         NULL },
	{ "//multi[position() > 4]",               0,  0, NULL,               NULL,         NULL },
	{ "//multi[position() < 1]",               0,  0, NULL,               NULL,         NULL },
	{ "//multi[position() > 19]",              0,  0, NULL,               NULL,         NULL },
	{ NULL, 0, -1, NULL, NULL, NULL }
};

START_TEST(check_xml_xpath)
{
	M_xml_node_t **results;
	M_xml_node_t  *x;
	M_xml_node_t  *n1;
	M_xml_node_t  *n2;
	const char    *const_temp;
	M_bool         has_text;
	size_t         num_matches;
	size_t         len;
	size_t         i;
	size_t         j;

	x = M_xml_read(XML2, M_str_len(XML2), M_XML_READER_NONE, NULL, NULL, NULL, NULL);
	ck_assert_msg(x != NULL, "XML could not be parsed");

	for (i=0; check_xml_xpath_data[i].search!=NULL; i++) {
		results = M_xml_xpath(x, check_xml_xpath_data[i].search, M_XML_READER_NONE, &num_matches);
		ck_assert_msg(num_matches == check_xml_xpath_data[i].num_matches, "(%lu) '%s': Number of matches does not match expected. got=%lu, expected=%lu, '%s'", i, check_xml_xpath_data[i].search, num_matches, check_xml_xpath_data[i].num_matches, check_xml_xpath_data[i].search);

		if (num_matches == 0)
			continue;

		n1 = results[check_xml_xpath_data[i].match_num];
		if (check_xml_xpath_data[i].match_text_val != NULL) {
			has_text = M_FALSE;
			if (M_xml_node_type(n1) == M_XML_NODE_TYPE_ELEMENT) {
				len      = M_xml_node_num_children(n1);
				for (j=0; j<len; j++) {
					n2 = M_xml_node_child(n1, j);
					if (M_xml_node_type(n2) != M_XML_NODE_TYPE_TEXT) {
						continue;
					}
					has_text = M_TRUE;
					ck_assert_msg(M_str_eq(M_xml_node_text(n2), check_xml_xpath_data[i].match_text_val), "(%lu) '%s': node text does not match expected value. got='%s', expected='%s'", i, check_xml_xpath_data[i].search, M_xml_node_text(n2), check_xml_xpath_data[i].match_text_val);
				}
			} else if (M_xml_node_type(n1) == M_XML_NODE_TYPE_TEXT) {
				has_text = M_TRUE;
				ck_assert_msg(M_str_eq(M_xml_node_text(n1), check_xml_xpath_data[i].match_text_val), "(%lu) '%s': node text does not match expected value. got='%s', expected='%s'", i, check_xml_xpath_data[i].search, M_xml_node_text(n1), check_xml_xpath_data[i].match_text_val);
			}
			ck_assert_msg(has_text, "(%lu) '%s': Node does not contain any text nodes", i, check_xml_xpath_data[i].search);
		}
		if (check_xml_xpath_data[i].match_attr_key != NULL) {
			const_temp = M_xml_node_attribute(n1, check_xml_xpath_data[i].match_attr_key);
			ck_assert_msg(const_temp != NULL, "(%lu) '%s': Node does not contain expected attribute '%s'", i, check_xml_xpath_data[i].search, check_xml_xpath_data[i].match_attr_key);
			ck_assert_msg(M_str_eq(const_temp, check_xml_xpath_data[i].match_attr_val), "(%lu) '%s': Attribute value does not match. got='%s', expected='%s'", i, check_xml_xpath_data[i].search, const_temp, check_xml_xpath_data[i].match_attr_val);
		}

		M_free(results);
	}

	M_xml_node_destroy(x);
}
END_TEST

static struct {
	const char *search;
	const char *expected;
} check_xml_xpath_text_first_data[] = {
	{ "/MonetraTrans/Trans/account", "5454545454545454" },
	{ "MonetraTrans/Trans/account",  "5454545454545454" },
	{ "MonetraTrans//account",       "5454545454545454" },
	{ "//account",                   "5454545454545454" },
	{ NULL, NULL }
};

START_TEST(check_xml_xpath_text_first)
{
	M_xml_node_t *x;
	const char   *const_temp;
	size_t        i;

	x = M_xml_read(XML2, M_str_len(XML2), M_XML_READER_NONE, NULL, NULL, NULL, NULL);
	ck_assert_msg(x != NULL, "XML could not be parsed");

	for (i=0; check_xml_xpath_text_first_data[i].search!=NULL; i++) {
		const_temp = M_xml_xpath_text_first(x, check_xml_xpath_text_first_data[i].search);
		ck_assert_msg(M_str_eq(const_temp, check_xml_xpath_text_first_data[i].expected), "(%lu) Text does not match. got='%s', expected='%s'", i, const_temp, check_xml_xpath_text_first_data[i].expected);
	}

	M_xml_node_destroy(x);
}
END_TEST


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("xml");

	add_test(suite, check_xml_valid);
	add_test(suite, check_xml_invalid);
	add_test(suite, check_xml_xpath);
	add_test(suite, check_xml_xpath_text_first);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_xml.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
