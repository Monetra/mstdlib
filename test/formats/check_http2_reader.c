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
	M_http_message_type_t  type;
	M_http_version_t       version;
	M_http_method_t        method;
	char                  *uri;
	M_uint32               code;
	char                  *reason;
	M_hash_dict_t         *headers_full;
	M_hash_dict_t         *headers;
	M_buf_t               *body;
	M_buf_t               *preamble;
	M_buf_t               *epilouge;
	M_list_str_t          *bpieces;
	M_hash_dict_t         *cextensions;
	size_t                 idx;
} httpr_test_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Content length provided. */
#define http1_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Length: 44\r\n" \
	"Connection: close\r\n"\
	"Content-Type: text/html\r\n" \
	"\r\n"  \
	"<html><body><h1>It works!</h1></body></html>"

static const M_uint8 test_dat01[] = {
	0x00, 0x00, 0x4c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADERS frame */
	0x88, 0x00, 0x83, 0xbe, 0x34, 0x97, 0x95, 0xd0, 0x7a, 0xbe, 0x94, 0x75, 0x4d, 0x03, 0xf4, 0xa0,
	0x80, 0x17, 0x94, 0x00, 0x6e, 0x00, 0x57, 0x00, 0xca, 0x98, 0xb4, 0x6f, 0x00, 0x8a, 0xbc, 0x7a,
	0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x82, 0x69, 0xaf, 0x00, 0x87, 0xbc, 0x7a, 0xaa,
	0x29, 0x12, 0x63, 0xd5, 0x84, 0x25, 0x07, 0x41, 0x7f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92,
	0xb6, 0xff, 0x55, 0x97, 0x87, 0x49, 0x7c, 0xa5, 0x89, 0xd3, 0x4d, 0x1f,
	0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	0x3c, 0x68, 0x74, 0x6d, 0x6c, 0x3e, 0x3c, 0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x68, 0x31, 0x3e,
	0x49, 0x74, 0x20, 0x77, 0x6f, 0x72, 0x6b, 0x73, 0x21, 0x3c, 0x2f, 0x68, 0x31, 0x3e, 0x3c, 0x2f,
	0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3e,
};

/* No Content length. Duplicate header. Header list. */
#define http2_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Type: text/html\r\n" \
	"dup_header: a\r\n" \
	"dup_header: b\r\n" \
	"dup_header: c\r\n" \
	"list_header: 1, 2, 3\r\n" \
	"\r\n" \
	"<html><body><h1>It works!</h1></body></html>"

static const M_uint8 test_dat02[] = {
	0x00, 0x00, 0x64, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADERS frame */
	0x88, 0x00, 0x83, 0xbe, 0x34, 0x97, 0x95, 0xd0, 0x7a, 0xbe, 0x94, 0x75, 0x4d, 0x03, 0xf4, 0xa0,
	0x80, 0x17, 0x94, 0x00, 0x6e, 0x00, 0x57, 0x00, 0xca, 0x98, 0xb4, 0x6f, 0x00, 0x89, 0xbc, 0x7a,
	0x92, 0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97, 0x87, 0x49, 0x7c, 0xa5, 0x89, 0xd3, 0x4d, 0x1f, 0x00,
	0x88, 0x92, 0xda, 0xe2, 0x9c, 0xa3, 0x90, 0xb6, 0x7f, 0x81, 0x1f, 0x00, 0x88, 0x92, 0xda, 0xe2,
	0x9c, 0xa3, 0x90, 0xb6, 0x7f, 0x81, 0x8f, 0x00, 0x88, 0x92, 0xda, 0xe2, 0x9c, 0xa3, 0x90, 0xb6,
	0x7f, 0x81, 0x27, 0x00, 0x88, 0xa0, 0xc8, 0x4c, 0x53, 0x94, 0x72, 0x16, 0xcf, 0x86, 0x0f, 0xd2,
	0x82, 0xfa, 0x51, 0x9f,
	0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	0x3c, 0x68, 0x74, 0x6d, 0x6c, 0x3e, 0x3c, 0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x68, 0x31, 0x3e,
	0x49, 0x74, 0x20, 0x77, 0x6f, 0x72, 0x6b, 0x73, 0x21, 0x3c, 0x2f, 0x68, 0x31, 0x3e, 0x3c, 0x2f,
	0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3e,
};

/* 1.0 GET request. */
#define http3_data "GET https://www.google.com/index.html HTTP/1.0\r\n" \
	"Host: www.google.com\r\n" \
	"\r\n"

static const M_uint8 test_dat03[] = {
	0x00, 0x00, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x82, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x85,
	0x00, 0x83, 0xc6, 0x74, 0x27, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a,
	0x7f,
};

/* 1.0 HEAD request no headers. */
#define http4_data "HEAD / HTTP/1.0\r\n\r\n"

/* Modified to following for required HTTP2 :scheme and :authority entries. */
/* HEAD https://www.google.com/ HTTP/1.0\r\n\r\n" */

static const M_uint8 test_dat04[] = {
	0x00, 0x00, 0x1b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x85, 0xb9, 0x49, 0x53, 0x39, 0xe4, 0x84, 0xc7, 0x82, 0x1b, 0xff, 0x87, 0x01, 0x8b, 0xf1,
	0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x84,
};

/* Start with \r\n simulating multiple messages in a stream
 * where they are separated by a new line. Body is form encoded.
 * Ends with trailing \r\n that's not read. */
#define http5_data "\r\n" \
	"POST /login HTTP/1.1\r\n" \
	"Host: 127.0.0.1\r\n" \
	"Referer: https://127.0.0.1/login.html\r\n" \
	"Accept-Language: en-us\r\n" \
	"Content-Type: application/x-www-form-urlencoded\r\n" \
	"Accept-Encoding: gzip, deflate\r\n" \
	"User-Agent: Test Client\r\n" \
	"Content-Length: 37\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n" \
	"\r\n" \
	"User=For+Meeee&pw=ABC123&action=login" \
	"\r\n"

/* Modified to following for required HTTP2 :scheme and :authority entries. */
/* POST https://www.google.com/login */

static const M_uint8 test_dat05[] = {
	'\r', '\n', /* start with white space */
	0x00, 0x00, 0xd9, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADERS frame */
	0x83, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x00,
	0x84, 0xb9, 0x58, 0xd3, 0x3f, 0x85, 0x62, 0x83, 0xcc, 0x6a, 0xbf, 0x00, 0x83, 0xc6, 0x74, 0x27,
	0x87, 0x08, 0x9d, 0x5c, 0x0b, 0x81, 0x70, 0xff, 0x00, 0x85, 0xda, 0x59, 0x4b, 0x61, 0x6c, 0x94,
	0x9d, 0x29, 0xad, 0x17, 0x18, 0x60, 0x22, 0x75, 0x70, 0x2e, 0x05, 0xc2, 0xc5, 0x07, 0x98, 0xd5,
	0x2f, 0x3a, 0x69, 0xa3, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69, 0x5b, 0x38, 0xea, 0x9a, 0xd1, 0xcc,
	0x5f, 0x84, 0x2d, 0x4b, 0x5a, 0x8f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff, 0x55,
	0x97, 0x98, 0x1d, 0x75, 0xd0, 0x62, 0x0d, 0x26, 0x3d, 0x4c, 0x79, 0x5b, 0xc7, 0x8f, 0x0b, 0x4a,
	0x7b, 0x29, 0x5a, 0xdb, 0x28, 0x2d, 0x44, 0x3c, 0x85, 0x93, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69,
	0x5b, 0x05, 0x44, 0x3c, 0x86, 0xaa, 0x6f, 0x8a, 0x9b, 0xd9, 0xab, 0xfa, 0x52, 0x42, 0xcb, 0x40,
	0xd2, 0x5f, 0x00, 0x88, 0xe0, 0x82, 0xd8, 0xb4, 0x33, 0x16, 0xa4, 0xff, 0x88, 0xde, 0x54, 0x25,
	0x4b, 0xd4, 0x18, 0xb5, 0x27, 0x00, 0x8a, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32,
	0x67, 0x82, 0x65, 0xdf, 0x00, 0x87, 0xbc, 0x7a, 0xaa, 0x29, 0x12, 0x63, 0xd5, 0x88, 0xcc, 0x52,
	0xd6, 0xb4, 0x34, 0x1b, 0xb9, 0x7f, 0x00, 0x8a, 0xbc, 0x32, 0x4e, 0x55, 0xaf, 0x1e, 0xa4, 0xd8,
	0x7a, 0x3f, 0x86, 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf,
	0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	0x55, 0x73, 0x65, 0x72, 0x3d, 0x46, 0x6f, 0x72, 0x2b, 0x4d, 0x65, 0x65, 0x65, 0x65, 0x26, 0x70,
	0x77, 0x3d, 0x41, 0x42, 0x43, 0x31, 0x32, 0x33, 0x26, 0x61, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3d,
	0x6c, 0x6f, 0x67, 0x69, 0x6e,
};

/* Chucked encoding. 1 chuck is headers as body
 * with extensions.
 * 1 chunk is header and data as body.
 * 1 chunk is body only. No trailers. */
#define http6_data "HTTP/1.1 200 OK\r\n" \
	"Transfer-Encoding: chunked\r\n" \
	"Content-Type: message/http\r\n" \
	"Connection: close\r\n" \
	"Server: server\r\n" \
	"\r\n" \
	"3a;ext1;ext2=abc\r\n" \
	"TRACE / HTTP/1.1\r\n" \
	"Connection: keep-alive\r\n" \
	"Host: google.com\r\n" \
	"40\r\n" \
	"\r\n" \
	"Content-Type: text/html\r\n" \
	"\r\n" \
	"<html><body>Chunk 2</body></html>\r\n" \
	"\r\n" \
	"21\r\n" \
	"<html><body>Chunk 3</body></html>\r\n" \
	"0\r\n" \
	"\r\n"

/* HTTP2 does not support chunking, it has framing instead.  In order to keep this test,
 * the chunk extensions will be encoded as header entries.
 * ext2 -> chunk-extension-ext2. The HTTP2 reader will call the chunk_extension
 * function when it finds headers with the prefix "chunk-extension-"
 */


static const M_uint8 test_dat06[] = {
	0x00, 0x00, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADERS frame */
	0x88, 0x00, 0x8d, 0xdf, 0x60, 0xea, 0x44, 0xa5, 0xb1, 0x6c, 0x15, 0x10, 0xf2, 0x1a, 0xa9, 0xbf,
	0x86, 0x24, 0xf6, 0xd5, 0xd4, 0xb2, 0x7f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff,
	0x55, 0x97, 0x89, 0xa4, 0xa8, 0x40, 0xe6, 0x2b, 0x13, 0xa5, 0x35, 0xff, 0x00, 0x87, 0xbc, 0x7a,
	0xaa, 0x29, 0x12, 0x63, 0xd5, 0x84, 0x25, 0x07, 0x41, 0x7f, 0x00, 0x85, 0xdc, 0x5b, 0x3b, 0x96,
	0xcf, 0x85, 0x41, 0x6c, 0xee, 0x5b, 0x3f,
	0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'3' , 'a' , ';' , 'e' , 'x' , 't' , '1' , ';' , 'e' , 'x' , 't' , '2' , '=' , 'a' , 'b' , 'c' ,
	'\r', '\n', 'T' , 'R' , 'A' , 'C' , 'E' , ' ' , '/' , ' ' , 'H' , 'T' , 'T' , 'P' , '/' , '1' ,
	'.' , '1' , '\r', '\n', 'C' , 'o' , 'n' , 'n' , 'e' , 'c' , 't' , 'i' , 'o' , 'n' , ':' , ' ' ,
	'k' , 'e' , 'e' , 'p' , '-' , 'a' , 'l' , 'i' , 'v' , 'e' , '\r', '\n', 'H' , 'o' , 's' , 't' ,
	':' , ' ' , 'g' , 'o' , 'o' , 'g' , 'l' , 'e' , '.' , 'c' , 'o' , 'm' , '\r', '\n', '4' , '0' ,
	'\r', '\n', '\r', '\n', 'C' , 'o' , 'n' , 't' , 'e' , 'n' , 't' , '-' , 'T' , 'y' , 'p' , 'e' ,
	':' , ' ' , 't' , 'e' , 'x' , 't' , '/' , 'h' , 't' , 'm' , 'l' , '\r', '\n', '\r', '\n', '<' ,
	'h' , 't' , 'm' , 'l' , '>' , '<' , 'b' , 'o' , 'd' , 'y' , '>' , 'C' , 'h' , 'u' , 'n' , 'k' ,
	' ' , '2' , '<' , '/' , 'b' , 'o' , 'd' , 'y' , '>' , '<' , '/' , 'h' , 't' , 'm' , 'l' , '>' ,
	'\r', '\n', '\r', '\n', '2' , '1' , '\r', '\n', '<' , 'h' , 't' , 'm' , 'l' , '>' , '<' , 'b' ,
	'o' , 'd' , 'y' , '>' , 'C' , 'h' , 'u' , 'n' , 'k' , ' ' , '3' , '<' , '/' , 'b' , 'o' , 'd' ,
	'y' , '>' , '<' , '/' , 'h' , 't' , 'm' , 'l' , '>' , '\r', '\n', '0' , '\r', '\n', '\r', '\n',
};

/* Chunked with trailer. */
#define http7_data "HTTP/1.1 200 OK\r\n" \
	"Transfer-Encoding: chunked\r\n" \
	"Content-Type: message/http\r\n" \
	"Connection: close\r\n" \
	"Server: server\r\n" \
	"\r\n" \
	"1F\r\n" \
	"<html><body>Chunk</body></html>\r\n" \
	"0\r\n" \
	"Trailer 1: I am a trailer\r\n" \
	"Trailer 2: Also a trailer\r\n" \
	"\r\n"

static const M_uint8 test_dat07[] = {
	0x00, 0x00, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x88, 0x00, 0x8d, 0xdf, 0x60, 0xea, 0x44, 0xa5, 0xb1, 0x6c, 0x15, 0x10, 0xf2, 0x1a, 0xa9, 0xbf,
	0x86, 0x24, 0xf6, 0xd5, 0xd4, 0xb2, 0x7f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff,
	0x55, 0x97, 0x89, 0xa4, 0xa8, 0x40, 0xe6, 0x2b, 0x13, 0xa5, 0x35, 0xff, 0x00, 0x87, 0xbc, 0x7a,
	0xaa, 0x29, 0x12, 0x63, 0xd5, 0x84, 0x25, 0x07, 0x41, 0x7f, 0x00, 0x85, 0xdc, 0x5b, 0x3b, 0x96,
	0xcf, 0x85, 0x41, 0x6c, 0xee, 0x5b, 0x3f,
	0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'1' , 'F' , '\r', '\n', '<' , 'h' , 't' , 'm' , 'l' , '>' , '<' , 'b' , 'o' , 'd' , 'y' , '>' ,
	'C' , 'h' , 'u' , 'n' , 'k' , '<' , '/' , 'b' , 'o' , 'd' , 'y' , '>' , '<' , '/' , 'h' , 't' ,
	'm' , 'l' , '>' , '\r', '\n', '0' , '\r', '\n', 'T' , 'r' , 'a' , 'i' , 'l' , 'e' , 'r' , ' ' ,
	'1' , ':' , ' ' , 'I' , ' ' , 'a' , 'm' , ' ' , 'a' , ' ' , 't' , 'r' , 'a' , 'i' , 'l' , 'e' ,
	'r' , '\r', '\n', 'T' , 'r' , 'a' , 'i' , 'l' , 'e' , 'r' , ' ' , '2' , ':' , ' ' , 'A' , 'l' ,
	's' , 'o' , ' ' , 'a' , ' ' , 't' , 'r' , 'a' , 'i' , 'l' , 'e' , 'r' , '\r', '\n', '\r', '\n'
};

/* Multipart data. */
#define http8_data "POST /upload/data HTTP/1.1\r\n" \
	"Host: 127.0.0.1\r\n" \
	"Accept: image/gif, image/jpeg, */*\r\n" \
	"Accept-Language: en-us\r\n" \
	"Content-Type: multipart/form-data; boundary=---------------------------7d41b838504d8\r\n" \
	"Accept-Encoding: gzip, deflate\r\n" \
	"User-Agent: Test Client\r\n" \
	"Content-Length: 327\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n" \
	"\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"Content-Dispositio1: form-data; name=\"username\"\r\n" \
	"\r\n" \
	"For Meeee\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"Content-Dispositio2: form-data; name=\"fileID\"; filename=\"/temp.html\"\r\n" \
	"Content-Typ2: text/plain\r\n" \
	"\r\n" \
	"<h1>Home page on main server</h1>\r\n" \
	"-----------------------------7d41b838504d8--"

static const M_uint8 test_dat08[] = {
	0x00, 0x00, 0xf5, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x83, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x00,
	0x84, 0xb9, 0x58, 0xd3, 0x3f, 0x85, 0x62, 0x83, 0xcc, 0x6a, 0xbf, 0x00, 0x83, 0xc6, 0x74, 0x27,
	0x87, 0x08, 0x9d, 0x5c, 0x0b, 0x81, 0x70, 0xff, 0x00, 0x84, 0x84, 0x84, 0x2d, 0x69, 0x94, 0x35,
	0x23, 0x98, 0xac, 0x4c, 0x69, 0x7e, 0x94, 0x35, 0x23, 0x98, 0xac, 0x74, 0xac, 0xb3, 0x7d, 0x29,
	0xf2, 0xc7, 0xcf, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69, 0x5b, 0x38, 0xea, 0x9a, 0xd1, 0xcc, 0x5f,
	0x84, 0x2d, 0x4b, 0x5a, 0x8f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97,
	0xb4, 0xa6, 0xda, 0x12, 0x6a, 0xc7, 0x62, 0x58, 0x94, 0xf6, 0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4,
	0x8c, 0xf6, 0xd5, 0x20, 0xec, 0xf5, 0x02, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2,
	0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xec, 0x8d, 0x06, 0x37, 0x99,
	0x79, 0xb0, 0x35, 0x23, 0xdf, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69, 0x5b, 0x05, 0x44, 0x3c, 0x86,
	0xaa, 0x6f, 0x8a, 0x9b, 0xd9, 0xab, 0xfa, 0x52, 0x42, 0xcb, 0x40, 0xd2, 0x5f, 0x00, 0x88, 0xe0,
	0x82, 0xd8, 0xb4, 0x33, 0x16, 0xa4, 0xff, 0x88, 0xde, 0x54, 0x25, 0x4b, 0xd4, 0x18, 0xb5, 0x27,
	0x00, 0x8a, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x83, 0x64, 0x4e, 0xff,
	0x00, 0x87, 0xbc, 0x7a, 0xaa, 0x29, 0x12, 0x63, 0xd5, 0x88, 0xcc, 0x52, 0xd6, 0xb4, 0x34, 0x1b,
	0xb9, 0x7f, 0x00, 0x8a, 0xbc, 0x32, 0x4e, 0x55, 0xaf, 0x1e, 0xa4, 0xd8, 0x7a, 0x3f, 0x86, 0xa8,
	0xeb, 0x10, 0x64, 0x9c, 0xbf,
	0x00, 0x01, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' ,
	'1' , 'b' , '8' , '3' , '8' , '5' , '0' , '4' , 'd' , '8' , '\r', '\n', 'C' , 'o' , 'n' , 't' ,
	'e' , 'n' , 't' , '-' , 'D' , 'i' , 's' , 'p' , 'o' , 's' , 'i' , 't' , 'i' , 'o' , '1' , ':' ,
	' ' , 'f' , 'o' , 'r' , 'm' , '-' , 'd' , 'a' , 't' , 'a' , ';' , ' ' , 'n' , 'a' , 'm' , 'e' ,
	'=' , '"' , 'u' , 's' , 'e' , 'r' , 'n' , 'a' , 'm' , 'e' , '"' , '\r', '\n', '\r', '\n', 'F' ,
	'o' , 'r' , ' ' , 'M' , 'e' , 'e' , 'e' , 'e' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' ,
	'0' , '4' , 'd' , '8' , '\r', '\n', 'C' , 'o' , 'n' , 't' , 'e' , 'n' , 't' , '-' , 'D' , 'i' ,
	's' , 'p' , 'o' , 's' , 'i' , 't' , 'i' , 'o' , '2' , ':' , ' ' , 'f' , 'o' , 'r' , 'm' , '-' ,
	'd' , 'a' , 't' , 'a' , ';' , ' ' , 'n' , 'a' , 'm' , 'e' , '=' , '"' , 'f' , 'i' , 'l' , 'e' ,
	'I' , 'D' , '"' , ';' , ' ' , 'f' , 'i' , 'l' , 'e' , 'n' , 'a' , 'm' , 'e' , '=' , '"' , '/' ,
	't' , 'e' , 'm' , 'p' , '.' , 'h' , 't' , 'm' , 'l' , '"' , '\r', '\n', 'C' , 'o' , 'n' , 't' ,
	'e' , 'n' , 't' , '-' , 'T' , 'y' , 'p' , '2' , ':' , ' ' , 't' , 'e' , 'x' , 't' , '/' , 'p' ,
	'l' , 'a' , 'i' , 'n' , '\r', '\n', '\r', '\n', '<' , 'h' , '1' , '>' , 'H' , 'o' , 'm' , 'e' ,
	' ' , 'p' , 'a' , 'g' , 'e' , ' ' , 'o' , 'n' , ' ' , 'm' , 'a' , 'i' , 'n' , ' ' , 's' , 'e' ,
	'r' , 'v' , 'e' , 'r' , '<' , '/' , 'h' , '1' , '>' , '\r', '\n', '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' ,
	'5' , '0' , '4' , 'd' , '8' , '-' , '-'
};

#if 0
static const M_uint8 test_dat08[] = {
	0x00, 0x00, 0xf5, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x83, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x00,
	0x84, 0xb9, 0x58, 0xd3, 0x3f, 0x85, 0x62, 0x83, 0xcc, 0x6a, 0xbf, 0x00, 0x83, 0xc6, 0x74, 0x27,
	0x87, 0x08, 0x9d, 0x5c, 0x0b, 0x81, 0x70, 0xff, 0x00, 0x84, 0x84, 0x84, 0x2d, 0x69, 0x94, 0x35,
	0x23, 0x98, 0xac, 0x4c, 0x69, 0x7e, 0x94, 0x35, 0x23, 0x98, 0xac, 0x74, 0xac, 0xb3, 0x7d, 0x29,
	0xf2, 0xc7, 0xcf, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69, 0x5b, 0x38, 0xea, 0x9a, 0xd1, 0xcc, 0x5f,
	0x84, 0x2d, 0x4b, 0x5a, 0x8f, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97,
	0xb4, 0xa6, 0xda, 0x12, 0x6a, 0xc7, 0x62, 0x58, 0x94, 0xf6, 0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4,
	0x8c, 0xf6, 0xd5, 0x20, 0xec, 0xf5, 0x02, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2,
	0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xec, 0x8d, 0x06, 0x37, 0x99,
	0x79, 0xb0, 0x35, 0x23, 0xdf, 0x00, 0x8b, 0x84, 0x84, 0x2d, 0x69, 0x5b, 0x05, 0x44, 0x3c, 0x86,
	0xaa, 0x6f, 0x8a, 0x9b, 0xd9, 0xab, 0xfa, 0x52, 0x42, 0xcb, 0x40, 0xd2, 0x5f, 0x00, 0x88, 0xe0,
	0x82, 0xd8, 0xb4, 0x33, 0x16, 0xa4, 0xff, 0x88, 0xde, 0x54, 0x25, 0x4b, 0xd4, 0x18, 0xb5, 0x27,
	0x00, 0x8a, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x83, 0x64, 0x4e, 0xff,
	0x00, 0x87, 0xbc, 0x7a, 0xaa, 0x29, 0x12, 0x63, 0xd5, 0x88, 0xcc, 0x52, 0xd6, 0xb4, 0x34, 0x1b,
	0xb9, 0x7f, 0x00, 0x8a, 0xbc, 0x32, 0x4e, 0x55, 0xaf, 0x1e, 0xa4, 0xd8, 0x7a, 0x3f, 0x86, 0xa8,
	0xeb, 0x10, 0x64, 0x9c, 0xbf,
	0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' ,
	'1' , 'b' , '8' , '3' , '8' , '5' , '0' , '4' , 'd' , '8' , '\r', '\n',
	0x00, 0x00, 0x24, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x00, 0x8d, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb5, 0xf3, 0x22, 0xb3, 0xa0, 0xc9, 0x31, 0xc3, 0x94,
	0x94, 0xf6, 0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4, 0xa8, 0x74, 0x96, 0x0f, 0xe6, 0xd4, 0x16, 0xca,
	0x87, 0x49, 0x7f, 0x9f,
	0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'F' , 'o' , 'r' , ' ' , 'M' , 'e' , 'e' , 'e' , 'e' , '\r', '\n', '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' ,
	'5' , '0' , '4' , 'd' , '8' , '\r', '\n',
	0x00, 0x00, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x00, 0x8d, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb5, 0xf3, 0x22, 0xb3, 0xa0, 0xc9, 0x31, 0xc5, 0xa5,
	0x94, 0xf6, 0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4, 0xa8, 0x74, 0x96, 0x0f, 0xe6, 0x53, 0x50, 0x5c,
	0x97, 0xff, 0x9f, 0xb5, 0x25, 0x35, 0x05, 0xa8, 0x74, 0x96, 0x0f, 0xe5, 0x84, 0x96, 0x9a, 0xd7,
	0x9d, 0x34, 0xd1, 0xfc, 0xff, 0x00, 0x89, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0xff, 0x55, 0x8b,
	0x87, 0x49, 0x7c, 0xa5, 0x8a, 0xe8, 0x19, 0xaa,
	0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'<' , 'h' , '1' , '>' , 'H' , 'o' , 'm' , 'e' , ' ' , 'p' , 'a' , 'g' , 'e' , ' ' , 'o' , 'n' ,
	' ' , 'm' , 'a' , 'i' , 'n' , ' ' , 's' , 'e' , 'r' , 'v' , 'e' , 'r' , '<' , '/' , 'h' , '1' ,
	'>' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' , '0' , '4' , 'd' , '8' , '-' , '-' , '\r',
	'\n',
};
#endif

/* Multipart preamble and epilouge. */
#define http9_data "POST /upload/data HTTP/1.1\r\n" \
	"Content-Type: multipart/form-data; boundary=---------------------------7d41b838504d8\r\n" \
	"Content-Length: 121\r\n" \
	"\r\n" \
	"preamble\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"\r\n" \
	"Part data\r\n" \
	"-----------------------------7d41b838504d8--\r\n" \
	"epilouge" \

static const M_uint8 test_dat09[] = {
	0x00, 0x00, 0x6a, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADERS frame */
	0x83, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x00,
	0x84, 0xb9, 0x58, 0xd3, 0x3f, 0x85, 0x62, 0x83, 0xcc, 0x6a, 0xbf, 0x00, 0x89, 0xbc, 0x7a, 0x92,
	0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97, 0xb4, 0xa6, 0xda, 0x12, 0x6a, 0xc7, 0x62, 0x58, 0x94, 0xf6,
	0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4, 0x8c, 0xf6, 0xd5, 0x20, 0xec, 0xf5, 0x02, 0xcb, 0x2c, 0xb2,
	0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb,
	0x2c, 0xec, 0x8d, 0x06, 0x37, 0x99, 0x79, 0xb0, 0x35, 0x23, 0xdf, 0x00, 0x8a, 0xbc, 0x7a, 0x92,
	0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x82, 0x08, 0x83,
	0x00, 0x00, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'p' , 'r' , 'e' , 'a' , 'm' , 'b' , 'l' , 'e' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' ,
	'0' , '4' , 'd' , '8' , '\r', '\n', '\r', '\n', 'P' , 'a' , 'r' , 't' , ' ' , 'd' , 'a' , 't' ,
	'a' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' , '0' , '4' , 'd' , '8' , '-' , '-' , '\r',
	'\n', 'e' , 'p' , 'i' , 'l' , 'o' , 'u' , 'g' , 'e',
};

/* 3 messages stacked into one stream. */
#define http10_data "HTTP/1.1 200 OK\r\n" \
	"Content-Length:9\r\n" \
	"\r\n" \
	"Message 1\r\n" \
	"\r\n" \
	"\r\n" \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length:9\r\n" \
	"\r\n" \
	"Message 2\r\n" \
	"HTTP/1.1 200 OK\r\n" \
	"\r\n" \
	"Message 3"

static const M_uint8 test_dat10[] = {
	0x00, 0x00, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x88, 0x00, 0x8a, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x81, 0x7f,
	0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'M' , 'e' , 's' , 's' , 'a' , 'g' , 'e' , ' ' , '1' ,
	0x00, 0x00, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, /* HEADER frame */
	0x88, 0x00, 0x8a, 0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x81, 0x7f,
	0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, /* DATA frame */
	'M' , 'e' , 's' , 's' , 'a' , 'g' , 'e' , ' ' , '2' ,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x05, /* HEADER frame */
	0x88,
	0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, /* DATA frame */
	'M' , 'e' , 's' , 's' , 'a' , 'g' , 'e' , ' ' , '3' ,
};

#define http11_data "HTTP/1.1 200 OK\r\n" \
	"Host: blah\r\n"

static const M_uint8 test_dat11[] = {
	0x00, 0x00, 0x0b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x88, 0x00, 0x83, 0xc6, 0x74, 0x27, 0x83, 0x8e, 0x81, 0xcf,
};


#define http12_data "POST /upload/data HTTP/1.1\r\n" \
	"Content-Type: multipart/form-data; boundary=---------------------------7d41b838504d8\r\n" \
	"Content-Length: 115\r\n" \
	"\r\n" \
	"preamble\r\n" \
	"-----------------------------7d41b838504d8\r\n" \
	"\r\n" \
	"Part data\r\n" \
	"-----------------------------7d41b838504d8--\r\n" \
	"ep"

static const M_uint8 test_dat12[] = {
	0x00, 0x00, 0x6a, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
	0x83, 0x87, 0x01, 0x8b, 0xf1, 0xe3, 0xc2, 0xf3, 0x1c, 0xf3, 0x50, 0x55, 0xc8, 0x7a, 0x7f, 0x00,
	0x84, 0xb9, 0x58, 0xd3, 0x3f, 0x85, 0x62, 0x83, 0xcc, 0x6a, 0xbf, 0x00, 0x89, 0xbc, 0x7a, 0x92,
	0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97, 0xb4, 0xa6, 0xda, 0x12, 0x6a, 0xc7, 0x62, 0x58, 0x94, 0xf6,
	0x52, 0xb4, 0x83, 0x48, 0xfe, 0xd4, 0x8c, 0xf6, 0xd5, 0x20, 0xec, 0xf5, 0x02, 0xcb, 0x2c, 0xb2,
	0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb, 0x2c, 0xb2, 0xcb,
	0x2c, 0xec, 0x8d, 0x06, 0x37, 0x99, 0x79, 0xb0, 0x35, 0x23, 0xdf, 0x00, 0x8a, 0xbc, 0x7a, 0x92,
	0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x82, 0x08, 0x5b,
	0x00, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
	'p' , 'r' , 'e' , 'a' , 'm' , 'b' , 'l' , 'e' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' ,
	'0' , '4' , 'd' , '8' , '\r', '\n', '\r', '\n', 'P' , 'a' , 'r' , 't' , ' ' , 'd' , 'a' , 't' ,
	'a' , '\r', '\n', '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' , '-' ,
	'7' , 'd' , '4' , '1' , 'b' , '8' , '3' , '8' , '5' , '0' , '4' , 'd' , '8' , '-' , '-' , '\r',
	'\n', 'e' , 'p' ,
};

/* charset provided. */
#define http13_data "HTTP/1.1 200 OK\r\n" \
	"Date: Mon, 7 May 2018 01:02:03 GMT\r\n" \
	"Content-Security-Policy: default-src 'none'; base-uri 'self'; block-all-mixed-content\r\n" \
	"Content-Length: 44\r\n" \
	"Connection: close\r\n"\
	"Content-Type: text/html; charset=ISO-8859-1\r\n" \
	"\r\n"  \
	"<html><body><h1>It works!</h1></body></html>"

static const M_uint8 test_dat13[] = {
0x00, 0x00, 0x9d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
0x88, 0x00, 0x83, 0xbe, 0x34, 0x97, 0x95, 0xd0, 0x7a, 0xbe, 0x94, 0x75, 0x4d, 0x03, 0xf4, 0xa0,
0x80, 0x17, 0x94, 0x00, 0x6e, 0x00, 0x57, 0x00, 0xca, 0x98, 0xb4, 0x6f, 0x00, 0x91, 0xbc, 0x7a,
0x92, 0x5a, 0x92, 0xb6, 0xe2, 0x92, 0xdb, 0x0c, 0x9f, 0x4b, 0x6b, 0x3d, 0x06, 0x27, 0xaf, 0xae,
0x90, 0xb2, 0x8e, 0xda, 0x12, 0xb2, 0x2c, 0x22, 0x9f, 0xea, 0xa3, 0xd4, 0x5f, 0xf5, 0xf6, 0xa4,
0x63, 0x41, 0x56, 0xb6, 0xc3, 0x29, 0xfe, 0x90, 0x5a, 0x25, 0xff, 0x5f, 0x6a, 0x47, 0x41, 0xc9,
0xd5, 0x61, 0xd1, 0x42, 0xd4, 0x9b, 0xc9, 0x64, 0x58, 0x87, 0xa9, 0x25, 0xa9, 0x3f, 0x00, 0x8a,
0xbc, 0x7a, 0x92, 0x5a, 0x92, 0xb6, 0x72, 0xd5, 0x32, 0x67, 0x82, 0x69, 0xaf, 0x00, 0x87, 0xbc,
0x7a, 0xaa, 0x29, 0x12, 0x63, 0xd5, 0x84, 0x25, 0x07, 0x41, 0x7f, 0x00, 0x89, 0xbc, 0x7a, 0x92,
0x5a, 0x92, 0xb6, 0xff, 0x55, 0x97, 0x96, 0x49, 0x7c, 0xa5, 0x89, 0xd3, 0x4d, 0x1f, 0x6a, 0x12,
0x71, 0xd8, 0x82, 0xa6, 0x0c, 0x9b, 0xb5, 0x2c, 0xf3, 0xcd, 0xbe, 0xb0, 0x7f,
0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* DATA frame */
0x3c, 0x68, 0x74, 0x6d, 0x6c, 0x3e, 0x3c, 0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x68, 0x31, 0x3e,
0x49, 0x74, 0x20, 0x77, 0x6f, 0x72, 0x6b, 0x73, 0x21, 0x3c, 0x2f, 0x68, 0x31, 0x3e, 0x3c, 0x2f,
0x62, 0x6f, 0x64, 0x79, 0x3e, 0x3c, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3e,
};

/*
HTTP/1.1 301 Moved Permanently
Location: http://localhost/
*/

static const M_uint8 test_redirect[] = {
0x00, 0x00, 0x1f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, /* HEADER frame */
0x00, 0x85, 0xb8, 0x84, 0x8d, 0x36, 0xa3, 0x82, 0x64, 0x01, 0x00, 0x86, 0xce, 0x72, 0x0d, 0x26,
0x3d, 0x5f, 0x8c, 0x9d, 0x29, 0xae, 0xe3, 0x0c, 0x50, 0x72, 0x0e, 0x89, 0xce, 0x84, 0xb1,
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static httpr_test_t *httpr_test_create(void)
{
	httpr_test_t *ht;

	ht               = M_malloc_zero(sizeof(*ht));
	ht->headers_full = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED);
	ht->headers      = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	ht->cextensions  = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	ht->body         = M_buf_create();
	ht->preamble     = M_buf_create();
	ht->epilouge     = M_buf_create();
	ht->bpieces      = M_list_str_create(M_LIST_NONE);

	return ht;
}

static void httpr_test_destroy(httpr_test_t *ht)
{
	M_free(ht->uri);
	M_free(ht->reason);
	M_hash_dict_destroy(ht->headers_full);
	M_hash_dict_destroy(ht->headers);
	M_hash_dict_destroy(ht->cextensions);
	M_buf_cancel(ht->body);
	M_buf_cancel(ht->preamble);
	M_buf_cancel(ht->epilouge);
	M_list_str_destroy(ht->bpieces);
	M_free(ht);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_error_t start_func(M_http_message_type_t type, M_http_version_t version, M_http_method_t method, const char *uri, M_uint32 code, const char *reason, void *thunk)
{
	httpr_test_t *ht = thunk;

	ht->type    = type;
	ht->version = version;
	if (type == M_HTTP_MESSAGE_TYPE_REQUEST) {
		ht->method = method;
		ht->uri    = M_strdup(uri);
	} else if (type == M_HTTP_MESSAGE_TYPE_RESPONSE) {
		ht->code   = code;
		M_free(ht->reason);
		ht->reason = M_strdup(reason);
	} else {
		return M_HTTP_ERROR_USER_FAILURE;
	}
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_full_func(const char *key, const char *val, void *thunk);

static M_http_error_t header_full_func(const char *key, const char *val, void *thunk)
{
	httpr_test_t *ht           = thunk;
	const char   *trailer_str  = "trailer-";

	if (M_str_eq_start(key, trailer_str))
		return trailer_full_func(&key[M_str_len(trailer_str)], val, thunk);

	M_hash_dict_insert(ht->headers_full, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_func(const char *key, const char *val, size_t idx, void *thunk);
static M_http_error_t trailer_func(const char *key, const char *val, void *thunk);

static M_http_error_t header_func(const char *key, const char *val, void *thunk)
{
	const char   *chunkext_str = "chunk-extension-";
	const char   *trailer_str  = "trailer-";
	httpr_test_t *ht           = thunk;

	if (M_str_eq_start(key, chunkext_str)) {
		size_t idx = 0;
		return chunk_extensions_func(&key[M_str_len(chunkext_str)], val, idx, thunk);
	}

	if (M_str_eq_start(key, trailer_str))
		return trailer_func(&key[M_str_len(trailer_str)], val, thunk);

	if (M_str_isempty(val))
		return M_HTTP_ERROR_SUCCESS;

	M_hash_dict_insert(ht->headers, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t header_done_func(M_http_data_format_t format, void *thunk)
{
	(void)thunk;
	(void)format;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_func(const unsigned char *data, size_t len, size_t idx, void *thunk);

static M_http_error_t body_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t  *ht  = thunk;

	chunk_data_func(data, len, ht->idx, thunk);
	ht->idx++;
	M_buf_add_bytes(ht->body, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t body_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_func(const char *key, const char *val, size_t idx, void *thunk)
{
	httpr_test_t *ht = thunk;

	(void)idx;

	M_hash_dict_insert(ht->cextensions, key, val);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_extensions_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_func(const unsigned char *data, size_t len, size_t idx, void *thunk)
{
	httpr_test_t *ht = thunk;
	M_buf_t      *buf;
	const char   *const_temp;
	char         *out;


	buf        = M_buf_create();
	const_temp = M_list_str_at(ht->bpieces, idx);
	M_buf_add_str(buf, const_temp);
	M_buf_add_bytes(buf, data, len);

	M_list_str_remove_at(ht->bpieces, idx);
	out = M_buf_finish_str(buf, NULL);
	M_list_str_insert_at(ht->bpieces, out, idx);
	M_free(out);

	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t chunk_data_finished_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_buf_add_bytes(ht->preamble, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_preamble_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_header_full_func(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)key;
	(void)val;
	(void)idx;
	(void)thunk;
	return header_full_func(key, val, thunk);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_header_func(const char *key, const char *val, size_t idx, void *thunk)
{
	(void)idx;
	return header_func(key, val, thunk);
}

static M_http_error_t multipart_header_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_func(const unsigned char *data, size_t len, size_t idx, void *thunk)
{
	return chunk_data_func(data, len, idx, thunk);
}

static M_http_error_t multipart_data_done_func(size_t idx, void *thunk)
{
	(void)idx;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_data_finished_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_func(const unsigned char *data, size_t len, void *thunk)
{
	httpr_test_t *ht = thunk;

	M_buf_add_bytes(ht->epilouge, data, len);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t multipart_epilouge_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_full_func(const char *key, const char *val, void *thunk)
{
	(void)key;
	(void)val;
	(void)thunk;
	return header_full_func(key, val, thunk);
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t trailer_func(const char *key, const char *val, void *thunk)
{
	return header_func(key, val, thunk);
}

static M_http_error_t trailer_done_func(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_http_reader_t *gen_reader(void *thunk)
{
	M_http_reader_t *hr;
	struct M_http_reader_callbacks cbs = {
		start_func,
		header_full_func,
		header_func,
		header_done_func,
		body_func,
		body_done_func,
		chunk_extensions_func,
		chunk_extensions_done_func,
		chunk_data_func,
		chunk_data_done_func,
		chunk_data_finished_func,
		multipart_preamble_func,
		multipart_preamble_done_func,
		multipart_header_full_func,
		multipart_header_func,
		multipart_header_done_func,
		multipart_data_func,
		multipart_data_done_func,
		multipart_data_finished_func,
		multipart_epilouge_func,
		multipart_epilouge_done_func,
		trailer_full_func,
		trailer_func,
		trailer_done_func
	};

	hr = M_http_reader_create(&cbs, M_HTTP_READER_NONE, thunk);
	return hr;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

START_TEST(check_httpr1)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *key;
	const char      *gval;
	const char      *eval;
	const char      *body = "<html><body><h1>It works!</h1></body></html>";

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat01, sizeof(test_dat01), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat01), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat01));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");

	/* Headers. */
	key  = "Date";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "Mon, 7 May 2018 01:02:03 GMT";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Data is a special case and should not split on ',' since it's part of the value and not a list. */
	key  = "Date";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "Mon, 7 May 2018 01:02:03 GMT";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed (did split): got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Length";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "44";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Connection";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "close";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "text/html";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Body. */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr2)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *body = "<html><body><h1>It works!</h1></body></html>";
	const char      *key;
	const char      *gval;
	const char      *eval;
	size_t           len;
	size_t           i;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat02, sizeof(test_dat02), &len_read);

	/* M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE can't happen in HTTP2. all data is sent with content-length information */
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat02), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http2_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");

	/* Headers. */
	key = "dup_header";
	ck_assert_msg(M_hash_dict_multi_len(ht->headers, key, &len), "No duplicate headers found");
	ck_assert_msg(len == 3, "Wrong length of duplicate headers got '%zu', expected '%d", len, 3);
	for (i=0; i<len; i++) {
		gval = M_hash_dict_multi_get_direct(ht->headers, key, i);
		if (i == 0) {
			eval = "a";
		} else if (i == 1) {
			eval = "b";
		} else {
			eval = "c";
		}
		ck_assert_msg(M_str_eq(gval, eval), "%s (%zu) failed part: got '%s', expected '%s'", key, i, gval, eval);
	}
	/* Full headers should only have the last occurrence since we're replacing as we go on duplicate headers. */
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "c";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed full: got '%s', expected '%s'", key, gval, eval);

	key = "list_header";
	ck_assert_msg(M_hash_dict_multi_len(ht->headers, key, &len), "No duplicate headers found");
	ck_assert_msg(len == 3, "Wrong length of duplicate headers got '%zu', expected '%d", len, 3);
	for (i=0; i<len; i++) {
		gval = M_hash_dict_multi_get_direct(ht->headers, key, i);
		if (i == 0) {
			eval = "1";
		} else if (i == 1) {
			eval = "2";
		} else {
			eval = "3";
		}
		ck_assert_msg(M_str_eq(gval, eval), "%s (%zu) failed: got '%s', expected '%s'", key, i, gval, eval);
	}
	/* Full headers should have the full list. */
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "1, 2, 3";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed full: got '%s', expected '%s'", key, gval, eval);

	/* Body. */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr3)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat03, sizeof(test_dat03), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat03), "Did not read full message: got '%zu', expected '%zu'", len_read, M_str_len(http3_data));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_GET, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_GET);
	ck_assert_msg(M_str_eq(ht->uri, "https://www.google.com/index.html"), "Wrong uri: got '%s', expected '%s'", ht->uri, "https://www.google.com/index.html");
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr4)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat04, sizeof(test_dat04), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat04), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat04));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_HEAD, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_HEAD);
	ck_assert_msg(M_str_eq(ht->uri, "https://www.google.com/"), "Wrong uri: got '%s', expected '%s'", ht->uri, "https://www.google.com/");
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr5)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *body = "User=For+Meeee&pw=ABC123&action=login";
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat05, sizeof(test_dat05), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat05)-2, "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat05)-2);

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_POST, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_POST);
	ck_assert_msg(M_str_eq(ht->uri, "https://www.google.com/login"), "Wrong uri: got '%s', expected '%s'", ht->uri, "https://www.google.com/login");
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_2);

	/* Headers. */
	key  = "Content-Type";
	/* Checking split header dict to ensure we have a value. */
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "application/x-www-form-urlencoded";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Body */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr6)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat06, sizeof(test_dat06), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat06), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat06));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");


	/* Headers. */
	key  = "Transfer-Encoding";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "chunked";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "message/http";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Chunks extensions. */
	key = "ext1";
	ck_assert_msg(M_hash_dict_get(ht->cextensions, key, &gval), "%s failed: Not found", key);
	ck_assert_msg(gval == NULL, "%s failed: got '%s', expected NULL", key, gval);

	key  = "ext2";
	gval = M_hash_dict_get_direct(ht->cextensions, key);
	eval = "abc";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Chunks data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 3, "Wrong number of chunks: got '%zu', expected '%d'", len, 3);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "TRACE / HTTP/1.1\r\nConnection: keep-alive\r\nHost: google.com";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", (size_t)0, gval, eval);

	gval = M_list_str_at(ht->bpieces, 1);
	eval = "\r\nContent-Type: text/html\r\n\r\n<html><body>Chunk 2</body></html>\r\n";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", (size_t)1, gval, eval);

	gval = M_list_str_at(ht->bpieces, 2);
	eval = "<html><body>Chunk 3</body></html>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", (size_t)2, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr7)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat07, sizeof(test_dat07), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat07), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat07));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_2);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");


	/* Trailers. */
	key  = "Trailer 1";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "I am a trailer";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Trailer 2";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "Also a trailer";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);


	/* Chunks data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 1, "Wrong number of chunks: got '%zu', expected '%d'", len, 1);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "<html><body>Chunk</body></html>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong chunk data: got '%s', expected '%s'", (size_t)1, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr8)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *key;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat08, sizeof(test_dat08), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: (%d): %s", res, M_http_errcode_to_str(res));
	ck_assert_msg(len_read == sizeof(test_dat08), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat08));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_REQUEST, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_REQUEST);
	ck_assert_msg(ht->method == M_HTTP_METHOD_POST, "Wrong method: got '%d', expected '%d'", ht->method, M_HTTP_METHOD_POST);
	ck_assert_msg(M_str_eq(ht->uri, "https://www.google.com/login"), "Wrong uri: got '%s', expected '%s'", ht->uri, "https://www.google.com/login");
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_2);

	/* Part Headers. */
	key  = "Content-Dispositio1";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "form-data; name=\"username\"";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Typ2";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "text/plain";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Part data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 2, "Wrong number of parts: got '%zu', expected '%d'", len, 2);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "For Meeee";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", (size_t)1, gval, eval);

	gval = M_list_str_at(ht->bpieces, 1);
	eval = "<h1>Home page on main server</h1>";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", (size_t)1, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr9)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat09, sizeof(test_dat09), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat09), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat09));

	/* data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 1, "Wrong number of parts: got '%zu', expected '%d'", len, 1);

	gval = M_buf_peek(ht->preamble);
	eval = "preamble";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong preamble data: got '%s', expected '%s'", gval, eval);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "Part data";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", (size_t)0, gval, eval);

	gval = M_buf_peek(ht->epilouge);
	eval = "epilouge";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong epilouge data: got '%s', expected '%s'", gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr10)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len_read = 0;

	/* message 1. */
	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat10, sizeof(test_dat10), &len_read);
	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed message %d: %d", 1, res);
	ck_assert_msg(len_read == sizeof(test_dat10), "Didn't read entire message: %zu != %zu", len_read, sizeof(test_dat10));

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "Message 1";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 1, gval, eval);

	gval = M_list_str_at(ht->bpieces, 1);
	eval = "Message 2";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 2, gval, eval);

	gval = M_list_str_at(ht->bpieces, 2);
	eval = "Message 3";
	ck_assert_msg(M_str_eq(gval, eval), "Message %d body does not match: got '%s', expected '%s'", 3, gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr11)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read = 0;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat11, sizeof(test_dat11), &len_read);
	ck_assert_msg(res == M_HTTP_ERROR_MOREDATA, "Parse failed: (%d): %s", res, M_http_errcode_to_str(res));

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr12)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	const char      *gval;
	const char      *eval;
	M_http_error_t   res;
	size_t           len;
	size_t           len_read;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat12, sizeof(test_dat12), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat12), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat12));

	/* data. */
	len = M_list_str_len(ht->bpieces);
	ck_assert_msg(len == 1, "Wrong number of parts: got '%zu', expected '%d'", len, 1);

	gval = M_buf_peek(ht->preamble);
	eval = "preamble";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong preamble data: got '%s', expected '%s'", gval, eval);

	gval = M_list_str_at(ht->bpieces, 0);
	eval = "Part data";
	ck_assert_msg(M_str_eq(gval, eval), "%zu: wrong part data: got '%s', expected '%s'", (size_t)0, gval, eval);

	gval = M_buf_peek(ht->epilouge);
	eval = "ep";
	ck_assert_msg(M_str_eq(gval, eval), "Wrong epilouge data: got '%s', expected '%s'", gval, eval);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_httpr13)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *key;
	const char      *gval;
	const char      *eval;
	const char      *body = "<html><body><h1>It works!</h1></body></html>";

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_dat13, sizeof(test_dat13), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_dat13), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_dat13));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_2);
	ck_assert_msg(ht->code == 200, "Wrong code: got '%u', expected '%u'", ht->code, 200);
	ck_assert_msg(M_str_eq(ht->reason, "OK"), "Wrong reason: got '%s', expected '%s'", ht->reason, "OK");

	/* Headers. */
	key  = "Date";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "Mon, 7 May 2018 01:02:03 GMT";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Data is a special case and should not split on ',' since it's part of the value and not a list. */
	key  = "Date";
	gval = M_hash_dict_get_direct(ht->headers, key);
	eval = "Mon, 7 May 2018 01:02:03 GMT";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed (did split): got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Length";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "44";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Connection";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "close";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	key  = "Content-Type";
	gval = M_hash_dict_get_direct(ht->headers_full, key);
	eval = "text/html; charset=ISO-8859-1";
	ck_assert_msg(M_str_eq(gval, eval), "%s failed: got '%s', expected '%s'", key, gval, eval);

	/* Body. */
	ck_assert_msg(M_str_eq(M_buf_peek(ht->body), body), "Body failed: got '%s', expected '%s'", M_buf_peek(ht->body), body);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

START_TEST(check_redirect)
{
	M_http_reader_t *hr;
	httpr_test_t    *ht;
	M_http_error_t   res;
	size_t           len_read;
	const char      *location;

	ht  = httpr_test_create();
	hr  = gen_reader(ht);
	res = M_http_reader_read(hr, (const unsigned char *)test_redirect, sizeof(test_redirect), &len_read);

	ck_assert_msg(res == M_HTTP_ERROR_SUCCESS, "Parse failed: %d", res);
	ck_assert_msg(len_read == sizeof(test_redirect), "Did not read full message: got '%zu', expected '%zu'", len_read, sizeof(test_redirect));

	/* Start. */
	ck_assert_msg(ht->type == M_HTTP_MESSAGE_TYPE_RESPONSE, "Wrong type: got '%d', expected '%d'", ht->type, M_HTTP_MESSAGE_TYPE_RESPONSE);
	ck_assert_msg(ht->code == 301, "Wrong status code: %u != 301\n", ht->code);
	location = M_hash_dict_get_direct(ht->headers_full, "Location");
	ck_assert_msg(M_str_eq(location, "http://localhost/"), "Wrong location '%s' != 'http://localhost/'", location);
	ck_assert_msg(ht->version == M_HTTP_VERSION_2, "Wrong version: got '%d', expected '%d'", ht->version, M_HTTP_VERSION_1_1);

	httpr_test_destroy(ht);
	M_http_reader_destroy(hr);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int main(void)
{
	Suite   *suite;
	SRunner *sr;
	int      nf;

	suite = suite_create("http_reader");

	add_test(suite, check_httpr1);
	add_test(suite, check_httpr2);
	add_test(suite, check_httpr3);
	add_test(suite, check_httpr4);
	add_test(suite, check_httpr5);
	add_test(suite, check_httpr6);
	add_test(suite, check_httpr7);
	add_test(suite, check_httpr8);
	add_test(suite, check_httpr9);
	add_test(suite, check_httpr10);
	add_test(suite, check_httpr11);
	add_test(suite, check_httpr12);
	add_test(suite, check_httpr13);
	add_test(suite, check_redirect);

	sr = srunner_create(suite);
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_http_reader.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
