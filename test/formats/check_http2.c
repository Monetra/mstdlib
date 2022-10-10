#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <formats/http2/m_http2.h>
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
	size_t frame_begin_func_call_count;
	size_t frame_end_func_call_count;
	size_t goaway_func_call_count;
	size_t data_func_call_count;
	size_t settings_begin_func_call_count;
	size_t settings_end_func_call_count;
	size_t setting_func_call_count;
	size_t headers_begin_func_call_count;
	size_t headers_end_func_call_count;
	size_t header_priority_func_call_count;
	size_t header_func_call_count;
	size_t pri_str_func_call_count;
} args_t;

static M_http_error_t check_http2_reader_frame_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->frame_begin_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_frame_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->frame_end_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_goaway_func(M_http2_goaway_t *goaway, void *thunk)
{
	(void)goaway;
	args_t *args = thunk;
	M_printf("(stream.id=%u,stream.is_R_set=%s,errcode=%u,debug_data=%p,debug_data_len=%zu)\n", goaway->stream.id.u32, goaway->stream.is_R_set ? "M_TRUE" : "M_FALSE", goaway->errcode.u32, goaway->debug_data, goaway->debug_data_len);
	args->goaway_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_data_func(M_http2_data_t *data, void *thunk)
{
	args_t *args = thunk;
	M_printf("BODY: %.*s\n", (int)data->data_len, data->data);
	args->data_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_settings_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->settings_begin_func_call_count++;
	M_printf("settings_begin_func()\n");
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_settings_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->settings_end_func_call_count++;
	M_printf("settings_end_func()\n");
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_setting_func(M_http2_setting_t *setting, void *thunk)
{
	(void)setting;
	args_t *args = thunk;
	M_printf("setting: %04x: %08x\n", (M_uint16)setting->type, setting->value.u32);
	args->setting_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static void check_http2_reader_error_func(M_http_error_t errcode, const char *errmsg)
{
	M_printf("ERROR: (%d) \"%s\"\n", errcode, errmsg);
}

static M_http_error_t check_http2_reader_headers_begin_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->headers_begin_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_headers_end_func(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	args_t *args = thunk;
	args->headers_end_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_header_priority_func(M_http2_header_priority_t *priority, void *thunk)
{
	(void)priority;
	args_t *args = thunk;
	args->header_priority_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_reader_header_func(M_http2_header_t *header, void *thunk)
{
	args_t *args = thunk;
	M_printf("HEADER \"%s\": \"%s\"\n", header->key, header->value);
	args->header_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t check_http2_pri_str_func(void *thunk)
{
	args_t *args = thunk;
	args->pri_str_func_call_count++;
	return M_HTTP_ERROR_SUCCESS;
}

struct M_http2_reader_callbacks test_cbs = {
	check_http2_reader_frame_begin_func,
	check_http2_reader_frame_end_func,
	check_http2_reader_goaway_func,
	check_http2_reader_data_func,
	check_http2_reader_settings_begin_func,
	check_http2_reader_settings_end_func,
	check_http2_reader_setting_func,
	check_http2_reader_error_func,
	check_http2_reader_headers_begin_func,
	check_http2_reader_headers_end_func,
	check_http2_reader_header_priority_func,
	check_http2_reader_header_func,
	check_http2_pri_str_func,
};


/*
static void print_dict(M_hash_dict_t *dict)
{
	M_hash_dict_enum_t *hashenum;
	const char         *key;
	const char         *value;
	M_hash_dict_enumerate(dict, &hashenum);
	while (M_hash_dict_enumerate_next(dict, hashenum, &key, &value)) {
		M_printf("\"%s\" = \"%s\"\n", key, value);
	}
	M_hash_dict_enumerate_free(hashenum);
}
*/

static const M_uint8 test_goaway_frame[] = {
	0x00, 0x00, 0x08, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static const M_uint8 test_data_frame[] = {
	0x00, 0x00, 0x57, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x3c, 0x21, 0x44, 0x4f, 0x43, 0x54,
	0x59, 0x50, 0x45, 0x20, 0x68, 0x74, 0x6d, 0x6c, 0x3e, 0x0a, 0x3c, 0x21, 0x2d, 0x2d, 0x5b, 0x69,
	0x66, 0x20, 0x49, 0x45, 0x4d, 0x6f, 0x62, 0x69, 0x6c, 0x65, 0x20, 0x37, 0x20, 0x5d, 0x3e, 0x3c,
	0x68, 0x74, 0x6d, 0x6c, 0x20, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x3d, 0x22, 0x6e, 0x6f, 0x2d, 0x6a,
	0x73, 0x20, 0x69, 0x65, 0x6d, 0x37, 0x22, 0x3e, 0x3c, 0x21, 0x5b, 0x65, 0x6e, 0x64, 0x69, 0x66,
	0x5d, 0x2d, 0x2d, 0x3e, 0x0a, 0x3c, 0x21, 0x2d, 0x2d, 0x5b, 0x69, 0x66, 0x20, 0x6c, 0x74, 0x20,
};

static const M_uint8 test_settings_frame[] = {
	0x00, 0x00, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x64
};
static const M_uint8 test_headers_frame[] = {
	0x00, 0x01, 0x05, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01, 0x20, 0x88, 0x0f, 0x12, 0x96, 0xe4, 0x59,
	0x3e, 0x94, 0x0b, 0xaa, 0x43, 0x6c, 0xca, 0x08, 0x02, 0x12, 0x81, 0x66, 0xe3, 0x4e, 0x5c, 0x65,
	0xe5, 0x31, 0x68, 0xdf, 0x0f, 0x10, 0x87, 0x49, 0x7c, 0xa5, 0x89, 0xd3, 0x4d, 0x1f, 0x0f, 0x1d,
	0x96, 0xdf, 0x69, 0x7e, 0x94, 0x03, 0x6a, 0x65, 0xb6, 0x85, 0x04, 0x01, 0x09, 0x40, 0x3f, 0x71,
	0xa6, 0x6e, 0x36, 0x25, 0x31, 0x68, 0xdf, 0x0f, 0x13, 0x8c, 0xfe, 0x5c, 0x11, 0x1a, 0x03, 0xb2,
	0x3c, 0xb0, 0x5e, 0x8d, 0xaf, 0xe7, 0x0f, 0x03, 0x84, 0x8f, 0xd2, 0x4a, 0x8f, 0x0f, 0x0d, 0x83,
	0x71, 0x91, 0x35, 0x00, 0x8f, 0xf2, 0xb4, 0x63, 0x27, 0x52, 0xd5, 0x22, 0xd3, 0x94, 0x72, 0x16,
	0xc5, 0xac, 0x4a, 0x7f, 0x86, 0x02, 0xe0, 0x03, 0x4f, 0x80, 0x5f, 0x0f, 0x29, 0x8c, 0xa4, 0x7e,
	0x56, 0x1c, 0xc5, 0x81, 0x90, 0xb6, 0xcb, 0x80, 0x00, 0x3f, 0x0f, 0x27, 0x86, 0xaa, 0x69, 0xd2,
	0x9a, 0xfc, 0xff, 0x00, 0x85, 0x1d, 0x09, 0x59, 0x1d, 0xc9, 0xa1, 0x9d, 0x98, 0x3f, 0x9b, 0x8d,
	0x34, 0xcf, 0xf3, 0xf6, 0xa5, 0x23, 0x81, 0x97, 0x00, 0x0f, 0xa5, 0x27, 0x65, 0x61, 0x3f, 0x07,
	0xf3, 0x71, 0xa6, 0x99, 0xfe, 0x7e, 0xd4, 0xa4, 0x70, 0x32, 0xe0, 0x01, 0x0f, 0x2d, 0x87, 0x12,
	0x95, 0x4d, 0x3a, 0x53, 0x5f, 0x9f, 0x00, 0x8b, 0xf2, 0xb4, 0xb6, 0x0e, 0x92, 0xac, 0x7a, 0xd2,
	0x63, 0xd4, 0x8f, 0x89, 0xdd, 0x0e, 0x8c, 0x1a, 0xb6, 0xe4, 0xc5, 0x93, 0x4f, 0x00, 0x8c, 0xf2,
	0xb7, 0x94, 0x21, 0x6a, 0xec, 0x3a, 0x4a, 0x44, 0x98, 0xf5, 0x7f, 0x8a, 0x0f, 0xda, 0x94, 0x9e,
	0x42, 0xc1, 0x1d, 0x07, 0x27, 0x5f, 0x00, 0x90, 0xf2, 0xb1, 0x0f, 0x52, 0x4b, 0x52, 0x56, 0x4f,
	0xaa, 0xca, 0xb1, 0xeb, 0x49, 0x8f, 0x52, 0x3f, 0x85, 0xa8, 0xe8, 0xa8, 0xd2, 0xcb,
};


static const char *test_pri_str = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

START_TEST(check_http2_frame_funcs)
{
	args_t            args = { 0 };
	size_t            len;
	M_http2_reader_t *h2r  = M_http2_reader_create(&test_cbs, M_HTTP2_READER_NONE, &args);

	M_http2_reader_read(h2r, test_goaway_frame, sizeof(test_goaway_frame), &len);
	ck_assert_msg(len == sizeof(test_goaway_frame), "Should have read '%zu' not '%zu'", sizeof(test_goaway_frame), len);
	ck_assert_msg(args.frame_begin_func_call_count == 1, "Should have called frame_begin_func once");
	ck_assert_msg(args.frame_end_func_call_count == 1, "Should have called frame_end_func once");
	ck_assert_msg(args.goaway_func_call_count == 1, "Should have called goaway_func once");

	M_http2_reader_read(h2r, test_data_frame, sizeof(test_data_frame), &len);
	ck_assert_msg(len == sizeof(test_data_frame), "Should have read '%zu' not '%zu'", sizeof(test_data_frame), len);
	ck_assert_msg(args.frame_begin_func_call_count == 2, "Should have called frame_begin_func twice");
	ck_assert_msg(args.frame_end_func_call_count == 2, "Should have called frame_end_func twice");
	ck_assert_msg(args.data_func_call_count == 1, "Should have called data_func once");

	M_http2_reader_read(h2r, test_settings_frame, sizeof(test_settings_frame), &len);
	ck_assert_msg(len == sizeof(test_settings_frame), "Should have read '%zu' not '%zu'", sizeof(test_settings_frame), len);
	ck_assert_msg(args.frame_begin_func_call_count == 3, "Should have called frame_begin_func thrice");
	ck_assert_msg(args.frame_end_func_call_count == 3, "Should have called frame_end_func thrice");
	ck_assert_msg(args.settings_begin_func_call_count == 1, "Should have called settings_begin_func once");
	ck_assert_msg(args.setting_func_call_count == 1, "Should have called setting_func once");
	ck_assert_msg(args.settings_end_func_call_count == 1, "Should have called settings_end_func once");

	M_http2_reader_read(h2r, test_headers_frame, sizeof(test_headers_frame), &len);
	ck_assert_msg(len == sizeof(test_headers_frame), "Should have read '%zu' not '%zu'", sizeof(test_headers_frame), len);
	ck_assert_msg(args.frame_begin_func_call_count == 4, "Should have called frame_begin_func 4 times");
	ck_assert_msg(args.frame_end_func_call_count == 4, "Should have called frame_end_func 4 times");
	ck_assert_msg(args.headers_begin_func_call_count == 1, "Should have called headers_begin_func once");
	ck_assert_msg(args.headers_end_func_call_count == 1, "Should have called headers_end_func once");

	M_http2_reader_read(h2r, (M_uint8*)test_pri_str, M_str_len(test_pri_str), &len);
	ck_assert_msg(len == M_str_len(test_pri_str), "Should have read '%zu' not '%zu'", M_str_len(test_pri_str), len);
	ck_assert_msg(args.pri_str_func_call_count == 1, "Should have called pri_str_func once");

	M_http2_reader_destroy(h2r);
}
END_TEST

START_TEST(check_http2_huffman)
{
	char          *str           = NULL;
	size_t         str_len;
	const M_uint8  huffman_str[] = {
		0xaa, 0x69, 0xd2, 0x9a, 0xc4, 0xb9, 0xec, 0x9b
	};
	const char    *huffman_str_decoded = "nghttp2.org";
	const M_uint8  huffman_str2[] = {
	  0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3, 0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60,
   0xd5, 0xaf, 0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f, 0xb5, 0x29, 0x1f, 0x95, 0x87,
   0x31, 0x60, 0x65, 0xc0, 0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07,
	};
	const char    *huffman_str2_decoded = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1";
	const M_uint8  huffman_str3[] = {
		0xff, 0x21 //11111111|00100001
	};
	const char    *huffman_str3_decoded = "?A";
	M_buf_t *buf = M_buf_create();
	ck_assert_msg(M_http2_decode_huffman(huffman_str, sizeof(huffman_str), buf), "Should succeed");
	str = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(str, huffman_str_decoded), "Should huffman decode to \"%s\" not \"%s\"", huffman_str_decoded, str);
	buf = M_buf_create();
	ck_assert_msg(M_http2_encode_huffman((unsigned char *)str, M_str_len(str), buf), "Should succeed");
	M_free(str);
	str = M_buf_finish_str(buf, &str_len);
	ck_assert_msg(M_mem_eq(str, huffman_str, sizeof(huffman_str)), "Should huffman encode back");
	ck_assert_msg(str_len == sizeof(huffman_str), "Should be the same length");
	M_free(str);

	buf = M_buf_create();
	ck_assert_msg(M_http2_decode_huffman(huffman_str2, sizeof(huffman_str2), buf), "Should succeed");
	str = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(str, huffman_str2_decoded), "Should huffman decode to \"%s\" not \"%s\"", huffman_str2_decoded, str);
	buf = M_buf_create();
	ck_assert_msg(M_http2_encode_huffman((unsigned char *)str, M_str_len(str), buf), "Should succeed");
	M_free(str);
	str = M_buf_finish_str(buf, &str_len);
	ck_assert_msg(M_mem_eq(str, huffman_str2, sizeof(huffman_str2)), "Should huffman encode back");
	ck_assert_msg(str_len == sizeof(huffman_str2), "Should be the same length");
	M_free(str);

	buf = M_buf_create();
	ck_assert_msg(M_http2_decode_huffman(huffman_str3, sizeof(huffman_str3), buf), "Should succeed");
	str = M_buf_finish_str(buf, NULL);
	ck_assert_msg(M_str_eq(str, huffman_str3_decoded), "Should huffman decode to \"%s\" not \"%s\"", huffman_str3_decoded, str);
	buf = M_buf_create();
	ck_assert_msg(M_http2_encode_huffman((unsigned char *)str, M_str_len(str), buf), "Should succeed");
	M_free(str);
	str = M_buf_finish_str(buf, &str_len);
	ck_assert_msg(M_mem_eq(str, huffman_str3, sizeof(huffman_str3)), "Should huffman encode back");
	ck_assert_msg(str_len == sizeof(huffman_str3), "Should be the same length");
	M_free(str);
}
END_TEST

/*
START_TEST(check_http2_pri_str)
{
	M_buf_t *buf = M_buf_create();
	M_http2_write_pri_str(buf);
	ck_assert_msg(M_http2_read_pri_str(M_buf_peek(buf), M_buf_len(buf)), "Should succeed");
	M_buf_cancel(buf);
}
END_TEST

START_TEST(check_http2_frame_headers)
{
	const M_uint8 frame[] = {
		0x00, 0x01, 0x05, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01, 0x20, 0x88, 0x0f, 0x12, 0x96, 0xe4, 0x59,
		0x3e, 0x94, 0x0b, 0xaa, 0x43, 0x6c, 0xca, 0x08, 0x02, 0x12, 0x81, 0x66, 0xe3, 0x4e, 0x5c, 0x65,
		0xe5, 0x31, 0x68, 0xdf, 0x0f, 0x10, 0x87, 0x49, 0x7c, 0xa5, 0x89, 0xd3, 0x4d, 0x1f, 0x0f, 0x1d,
		0x96, 0xdf, 0x69, 0x7e, 0x94, 0x03, 0x6a, 0x65, 0xb6, 0x85, 0x04, 0x01, 0x09, 0x40, 0x3f, 0x71,
		0xa6, 0x6e, 0x36, 0x25, 0x31, 0x68, 0xdf, 0x0f, 0x13, 0x8c, 0xfe, 0x5c, 0x11, 0x1a, 0x03, 0xb2,
		0x3c, 0xb0, 0x5e, 0x8d, 0xaf, 0xe7, 0x0f, 0x03, 0x84, 0x8f, 0xd2, 0x4a, 0x8f, 0x0f, 0x0d, 0x83,
		0x71, 0x91, 0x35, 0x00, 0x8f, 0xf2, 0xb4, 0x63, 0x27, 0x52, 0xd5, 0x22, 0xd3, 0x94, 0x72, 0x16,
		0xc5, 0xac, 0x4a, 0x7f, 0x86, 0x02, 0xe0, 0x03, 0x4f, 0x80, 0x5f, 0x0f, 0x29, 0x8c, 0xa4, 0x7e,
		0x56, 0x1c, 0xc5, 0x81, 0x90, 0xb6, 0xcb, 0x80, 0x00, 0x3f, 0x0f, 0x27, 0x86, 0xaa, 0x69, 0xd2,
		0x9a, 0xfc, 0xff, 0x00, 0x85, 0x1d, 0x09, 0x59, 0x1d, 0xc9, 0xa1, 0x9d, 0x98, 0x3f, 0x9b, 0x8d,
		0x34, 0xcf, 0xf3, 0xf6, 0xa5, 0x23, 0x81, 0x97, 0x00, 0x0f, 0xa5, 0x27, 0x65, 0x61, 0x3f, 0x07,
		0xf3, 0x71, 0xa6, 0x99, 0xfe, 0x7e, 0xd4, 0xa4, 0x70, 0x32, 0xe0, 0x01, 0x0f, 0x2d, 0x87, 0x12,
		0x95, 0x4d, 0x3a, 0x53, 0x5f, 0x9f, 0x00, 0x8b, 0xf2, 0xb4, 0xb6, 0x0e, 0x92, 0xac, 0x7a, 0xd2,
		0x63, 0xd4, 0x8f, 0x89, 0xdd, 0x0e, 0x8c, 0x1a, 0xb6, 0xe4, 0xc5, 0x93, 0x4f, 0x00, 0x8c, 0xf2,
		0xb7, 0x94, 0x21, 0x6a, 0xec, 0x3a, 0x4a, 0x44, 0x98, 0xf5, 0x7f, 0x8a, 0x0f, 0xda, 0x94, 0x9e,
		0x42, 0xc1, 0x1d, 0x07, 0x27, 0x5f, 0x00, 0x90, 0xf2, 0xb1, 0x0f, 0x52, 0x4b, 0x52, 0x56, 0x4f,
		0xaa, 0xca, 0xb1, 0xeb, 0x49, 0x8f, 0x52, 0x3f, 0x85, 0xa8, 0xe8, 0xa8, 0xd2, 0xcb,
	};
	const M_uint8 frame2[] = {
		0x00, 0x00, 0x0d, 0x01, 0x05, 0x00, 0x00, 0x00, 0x01, 0x82, 0x87, 0x84, 0x41, 0x88, 0xaa, 0x69,
		0xd2, 0x9a, 0xc4, 0xb9, 0xec, 0x9b,
	};

	const struct {
		const char *key;
		const char *val;
	} keyvals[] = {
		{ ":status", "200" },
		{ "x-content-type-options", "nosniff" },
		{ "last-modified", "Tue, 05 Jul 2022 09:43:52 GMT" },
		{ "etag", "\"62c407d8-18b4\"" },
		{ "x-xss-protection", "1; mode=block" },
		{ "server", "nghttpx" },
		{ "x-frame-options", "SAMEORIGIN" },
		{ "content-type", "text/html" },
		{ "date", "Wed, 17 Aug 2022 13:46:38 GMT" },
		{ "accept-ranges", "bytes" },
		{ "content-length", "6324" },
		{ "x-backend-header-rtt", "0.004902" },
		{ "strict-transport-security", "max-age=31536000" },
		{ "alt-svc", "h3=\":443\"; ma=3600, h3-29=\":443\"; ma=3600" },
		{ "via", "2 nghttpx" },
	};
	const struct {
		const char *key;
		const char *val;
	} keyvals2[] = {
		{ ":method", "GET" },
		{ ":scheme", "https" },
		{ ":path", "/" },
		{ ":authority", "nghttp2.org" },
	};
	const size_t        len      = (sizeof(keyvals) / sizeof(keyvals[0]));
	const size_t        len2     = (sizeof(keyvals2) / sizeof(keyvals2[0]));
	size_t              i;
	M_hash_dict_t      *headers  = M_http2_frame_read_headers(frame, sizeof(frame));
	ck_assert_msg(M_hash_dict_num_keys(headers) == len, "Should have read %zu header entries, not %zu", len, M_hash_dict_num_keys(headers));
	for (i=0; i<len; i++) {
		const char *val = M_hash_dict_get_direct(headers, keyvals[i].key);
		ck_assert_msg(M_str_eq(val, keyvals[i].val), "Should have \"%s\" = \"%s\", not \"%s\"", keyvals[i].key, keyvals[i].val, val);
	}
	M_hash_dict_destroy(headers);

	headers = M_http2_frame_read_headers(frame2, sizeof(frame2));
	ck_assert_msg(M_hash_dict_num_keys(headers) == len2, "Should have read %zu header entries, not %zu", len2, M_hash_dict_num_keys(headers));
	for (i=0; i<len2; i++) {
		const char *val = M_hash_dict_get_direct(headers, keyvals2[i].key);
		ck_assert_msg(M_str_eq(val, keyvals2[i].val), "Should have \"%s\" = \"%s\", not \"%s\"", keyvals2[i].key, keyvals2[i].val, val);
	}
	M_hash_dict_destroy(headers);
}
END_TEST
*/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("http2");

	add_test(suite, check_http2_huffman);
	add_test(suite, check_http2_frame_funcs);
	/*
	add_test(suite, check_http2_frame_headers);
	add_test(suite, check_http2_pri_str);
	*/

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_http2.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
