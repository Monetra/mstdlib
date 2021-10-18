#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>
#include <inttypes.h>
#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_bincodec_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char         *data;
	const char         *out;
	M_bincodec_codec_t  codec;
	size_t              wrap;
} check_bincodec_encode_data[] = {
	{ "abcdefghijklmnopqrstuvwxyz", "6162636465666768696A6B6C6D6E6F707172737475767778797A",                         M_BINCODEC_HEX,    0 },
	{ "abcdefghijklmnopqrstuvwxyz", "6162\n6364\n6566\n6768\n696A\n6B6C\n6D6E\n6F70\n7172\n7374\n7576\n7778\n797A", M_BINCODEC_HEX,    4 },
	{ "abcdefghijklmnopqrstuvwxyz", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo=",                                         M_BINCODEC_BASE64, 0 },
	{ "abcdefghijklmnopqrstuvwxyz", "YWJj\nZGVm\nZ2hp\namts\nbW5v\ncHFy\nc3R1\ndnd4\neXo=",                         M_BINCODEC_BASE64, 4 },
	{ "abcd",                       "61626364",                                                                     M_BINCODEC_HEX,    8 },
	{ "abcd",                       "YWJjZA==",                                                                     M_BINCODEC_BASE64, 8 },
	{ NULL, NULL, 0, 0 }
};

START_TEST(check_bincodec_encode_alloc)
{
	const char *in;
	char       *out;
	size_t      i;

	for (i=0; check_bincodec_encode_data[i].data!=NULL; i++) {
		in  = check_bincodec_encode_data[i].data;
		out = M_bincodec_encode_alloc((const M_uint8 *)in, M_str_len(in), check_bincodec_encode_data[i].wrap, check_bincodec_encode_data[i].codec);
		ck_assert_msg(out != NULL, "%"PRIu64": Could not encode", (M_uint64)i);
		if (out == NULL)
			continue;
		ck_assert_msg(M_str_eq(out, check_bincodec_encode_data[i].out), "%"PRIu64": got='%s', expected='%s'", (M_uint64)i, out, check_bincodec_encode_data[i].out);
		M_free(out);
	}
}
END_TEST

START_TEST(check_bincodec_encode)
{
	const char *in;
	char       *out;
	size_t      out_len;
	size_t      encode_len;
	size_t      i;

	for (i=0; check_bincodec_encode_data[i].data!=NULL; i++) {
		in         = check_bincodec_encode_data[i].data;
		out_len    = M_bincodec_encode_size(M_str_len(in), check_bincodec_encode_data[i].wrap, check_bincodec_encode_data[i].codec);
		out        = M_malloc(out_len+1);
		encode_len = M_bincodec_encode(out, out_len, (const M_uint8 *)in, M_str_len(in), check_bincodec_encode_data[i].wrap, check_bincodec_encode_data[i].codec);
		ck_assert_msg(encode_len != 0 && encode_len <= out_len, "%"PRIu64": Could not encode", (M_uint64)i);
		out[encode_len] = '\0';
		ck_assert_msg(M_str_eq(out, check_bincodec_encode_data[i].out), "%"PRIu64": got='%s', expected='%s'", (M_uint64)i, out, check_bincodec_encode_data[i].out);
		M_free(out);
	}
}
END_TEST

static struct {
	const char         *data;
	const char         *out;
	M_bincodec_codec_t  codec;
} check_bincodec_decode_data[] = {
	{ "6162636465666768696A6B6C6D6E6F707172737475767778797A",                         "abcdefghijklmnopqrstuvwxyz", M_BINCODEC_HEX    },
	{ "6162\n6364\n6566\n6768\n696A\n6B6C\n6D6E\n6F70\n7172\n7374\n7576\n7778\n797A", "abcdefghijklmnopqrstuvwxyz", M_BINCODEC_HEX    },
	{ "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo=",                                         "abcdefghijklmnopqrstuvwxyz", M_BINCODEC_BASE64 },
	{ "YWJj\nZGVm\nZ2hp\namts\nbW5v\ncHFy\nc3R1\ndnd4\neXo=",                         "abcdefghijklmnopqrstuvwxyz", M_BINCODEC_BASE64 },
	{ "61626364",                                                                     "abcd",                       M_BINCODEC_HEX    },
	{ "YWJjZA==",                                                                     "abcd",                       M_BINCODEC_BASE64 },
	{ NULL, NULL, 0 }
};

START_TEST(check_bincodec_decode)
{
	const char *in;
	char       *out;
	size_t      i;
	size_t      out_len;

	for (i=0; check_bincodec_decode_data[i].data!=NULL; i++) {
		in  = check_bincodec_decode_data[i].data;
		out = (char *)M_bincodec_decode_alloc(in, M_str_len(in), &out_len, check_bincodec_decode_data[i].codec);
		ck_assert_msg(out != NULL, "%"PRIu64": Could not decode", (M_uint64)i);
		ck_assert_msg(M_str_eq(out, check_bincodec_decode_data[i].out), "%"PRIu64": got='%s', expected='%s'", (M_uint64)i, out, check_bincodec_decode_data[i].out);
		M_free(out);
	}
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_bincodec_suite(void)
{
	Suite *suite;
	TCase *tc_bincodec_encode_alloc;
	TCase *tc_bincodec_encode;
	TCase *tc_bincodec_decode;

	suite = suite_create("bincodec");

	tc_bincodec_encode_alloc = tcase_create("check_bincodec_encode_alloc");
	tcase_add_test(tc_bincodec_encode_alloc, check_bincodec_encode_alloc);
	suite_add_tcase(suite, tc_bincodec_encode_alloc);

	tc_bincodec_encode = tcase_create("check_bincodec_encode");
	tcase_add_test(tc_bincodec_encode, check_bincodec_encode);
	suite_add_tcase(suite, tc_bincodec_encode);

	tc_bincodec_decode = tcase_create("check_bincodec_decode");
	tcase_add_test(tc_bincodec_decode, check_bincodec_decode);
	suite_add_tcase(suite, tc_bincodec_decode);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_bincodec_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_bincodec.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

