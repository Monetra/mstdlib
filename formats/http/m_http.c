/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* XXX: Here until we add m_http.h to mstdlib_formats.h */
#include <mstdlib/formats/m_http.h>
#include "http/m_http_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_http_create_init(M_http_t *http)
{
	struct M_list_callbacks cbs = {
		NULL,
		NULL,
		NULL,
		(M_list_free_func)M_http_chunk_destory
	};

	if (http == NULL)
		return;

	/* Note: we don't create the query args dict here
 	 * because it's created when the URI is set. There is
	 * no other way to manipulate it so we don't need
	 * it before hand. */
	http->headers     = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	http->trailers    = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);
	http->set_cookies = M_list_str_create(M_LIST_STR_STABLE);
	http->body        = M_buf_create();
	http->chunks      = M_list_create(&cbs, M_LIST_NONE);
}

static void M_http_reset_int(M_http_t *http)
{
	if (http == NULL)
		return;

	M_free(http->reason_phrase);
	M_free(http->uri);
	M_free(http->host);
	M_free(http->path);
	M_free(http->query_string);
	M_hash_dict_destroy(http->query_args);
	M_hash_dict_destroy(http->headers);
	M_hash_dict_destroy(http->trailers);
	M_list_str_destroy(http->set_cookies);
	M_buf_cancel(http->body);
	M_list_destroy(http->chunks, M_TRUE);

	M_mem_set(http, 0, sizeof(*http));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_t *M_http_create(void)
{
	M_http_t *http;

	http = M_malloc_zero(sizeof(*http));
	M_http_create_init(http);

	return http;
}

void M_http_destroy(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_reset_int(http);
	M_free(http);
}

void M_http_reset(M_http_t *http)
{
	if (http == NULL)
		return;

	M_http_reset_int(http);
	M_http_create_init(http);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_message_type_t M_http_message_type(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_MESSAGE_TYPE_UNKNOWN;
	return http->type;
}

void M_http_set_message_type(M_http_t *http, M_http_message_type_t type)
{
	if (http == NULL)
		return;
	http->type = type;
}

M_http_version_t M_http_version(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_VERSION_UNKNOWN;
	return http->version;
}

void M_http_set_version(M_http_t *http, M_http_version_t version)
{
	if (http == NULL)
		return;
	http->version = version;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_uint32 M_http_status_code(const M_http_t *http)
{
	if (http == NULL)
		return 0;
	return http->status_code;
}

void M_http_set_status_code(M_http_t *http, M_uint32 code)
{
	if (http == NULL)
		return;
	http->status_code = code;
}

const char *M_http_reason_phrase(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->reason_phrase;
}

void M_http_set_reason_phrase(M_http_t *http, const char *phrase)
{
	if (http == NULL)
		return;
	M_free(http->reason_phrase);
	http->reason_phrase = M_strdup(phrase);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_method_t M_http_method(const M_http_t *http)
{
	if (http == NULL)
		return M_HTTP_METHOD_UNKNOWN;
	return http->method;
}

void M_http_set_method(M_http_t *http, M_http_method_t method)
{
	if (http == NULL)
		return;
	http->method = method;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_version_t M_http_version_from_str(const char *version)
{
	if (M_str_eq_max(version, "HTTP/", 5))
		version += 5;

	if (M_str_eq(version, "1.0"))
		return M_HTTP_VERSION_1_0;

	if (M_str_eq(version, "1.1"))
		return M_HTTP_VERSION_1_1;

	if (M_str_eq(version, "2"))
		return M_HTTP_VERSION_2;

	return M_HTTP_VERSION_UNKNOWN;
}

const char *M_http_version_to_str(M_http_version_t version)
{
	switch (version) {
		case M_HTTP_VERSION_1_0:
			return "HTTP/1.0";
		case M_HTTP_VERSION_1_1:
			return "HTTP/1.1";
		case M_HTTP_VERSION_2:
			return "HTTP/2";
		case M_HTTP_VERSION_UNKNOWN:
			break;
	}

	return NULL;
}

M_http_method_t M_http_method_from_str(const char *method)
{
	if (M_str_caseeq(method, "OPTIONS"))
		return M_HTTP_METHOD_OPTIONS;

	if (M_str_caseeq(method, "GET"))
		return M_HTTP_METHOD_GET;

	if (M_str_caseeq(method, "HEAD"))
		return M_HTTP_METHOD_HEAD;

	if (M_str_caseeq(method, "POST"))
		return M_HTTP_METHOD_POST;

	if (M_str_caseeq(method, "PUT"))
		return M_HTTP_METHOD_PUT;

	if (M_str_caseeq(method, "DELETE"))
		return M_HTTP_METHOD_DELETE;

	if (M_str_caseeq(method, "TRACE"))
		return M_HTTP_METHOD_TRACE;

	if (M_str_caseeq(method, "CONNECT"))
		return M_HTTP_METHOD_CONNECT;

	return M_HTTP_METHOD_UNKNOWN;
}

const char *M_http_method_to_str(M_http_method_t method)
{
	switch (method) {
		case M_HTTP_METHOD_OPTIONS:
			return "OPTIONS";
		case M_HTTP_METHOD_GET:
			return "GET";
		case M_HTTP_METHOD_HEAD:
			return "HEAD";
		case M_HTTP_METHOD_POST:
			return "POST";
		case M_HTTP_METHOD_PUT:
			return "PUT";
		case M_HTTP_METHOD_DELETE:
			return "DELETE";
		case M_HTTP_METHOD_TRACE:
			return "TRACE";
		case M_HTTP_METHOD_CONNECT:
			return "CONNECT";
		case M_HTTP_METHOD_UNKNOWN:
			break;
	}

	return NULL;
}

const char *M_http_code_to_reason(M_uint32 code)
{
	switch (code) {
		case 100:
			return "Continue";
		case 101:
			return "Switching Protocols";
		case 102:
			return "Processing";
		case 103:
			return "Early Hints";
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 202:
			return "Accepted";
		case 203:
			return "Non-Authoritative Information";
		case 204:
			return "No Content";
		case 205:
			return "Reset Content";
		case 206:
			return "Partial Content";
		case 207:
			return "Multi-Status";
		case 208:
			return "Already Reported";
		case 226:
			return "IM Used";
		case 300:
			return "Multiple Choices";
		case 301:
			return "Moved Permanently";
		case 302:
			return "Found";
		case 303:
			return "See Other";
		case 304:
			return "Not Modified";
		case 305:
			return "Use Proxy";
		case 306:
			return "Switch Proxy";
		case 307:
			return "Temporary Redirect";
		case 308:
			return "Permanent Redirect";
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 402:
			return "Payment Required";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 406:
			return "Not Acceptable";
		case 407:
			return "Proxy Authentication Required";
		case 408:
			return "Request Timeout";
		case 409:
			return "Conflict";
		case 410:
			return "Gone";
		case 411:
			return "Length Required";
		case 412:
			return "Precondition Failed";
		case 413:
			return "Payload Too Large";
		case 414:
			return "URI Too Long";
		case 415:
			return "Unsupported Media Type";
		case 416:
			return "Range Not Satisfiable";
		case 417:
			return "Expectation Failed";
		case 418:
			return "I'm a teapot";
		case 421:
			return "Misdirected Request";
		case 422:
			return "Unprocessable Entity";
		case 423:
			return "Locked";
		case 424:
			return "Failed Dependency";
		case 426:
			return "Upgrade Required";
		case 428:
			return "Precondition Required";
		case 429:
			return "Too Many Requests";
		case 431:
			return "Request Header Fields Too Large";
		case 451:
			return "Unavailable For Legal Reasons";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Supported";
		case 506:
			return "Variant Also Negotiates";
		case 507:
			return "Insufficient Storage";
		case 508:
			return "Loop Detected";
		case 510:
			return "Not Extended";
		case 511:
			return "Network Authentication Required";
		default:
			break;
	}

	return "Generic";
}


const char *M_http_error_to_string(M_http_error_t err)
{
	const char *ret = "unknown";
	switch (err) {
		case M_HTTP_ERROR_SUCCESS:
			ret = "success";
			break;
		case M_HTTP_ERROR_INVALIDUSE:
			ret = "invalid use";
			break;
		case M_HTTP_ERROR_STOP:
			ret = "stop"; /* TODO: change to a more informative error message */
			break;
		case M_HTTP_ERROR_SKIP:
			ret = "skip"; /* TODO: change to a more informative error message */
			break;
		case M_HTTP_ERROR_MOREDATA:
			ret = "message incomplete, wait for more data";
			break;
		case M_HTTP_ERROR_LENGTH_REQUIRED: /* 411 */
			ret = "Content-Length header is required, but not present in message (411)";
			break;
		case M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED:
			ret = "message uses chunked encoding, but reader is set to not allow it";
			break;
		case M_HTTP_ERROR_TRAILER_NOTALLOWED:
			ret = "message has trailers, but reader is set to not allow them";
			break;
		case M_HTTP_ERROR_URI: /* 400 */
			ret = "URI malformed or not found (400)";
			break;
		case M_HTTP_ERROR_STARTLINE_LENGTH: /* 414 (6k limit) */
			ret = "first line of HTTP message exceeds max length - 6 KiB (414)";
			break;
		case M_HTTP_ERROR_STARTLINE_MALFORMED: /* 400 */
			ret = "first line of HTTP message malformed (400)";
			break;
		case M_HTTP_ERROR_UNKNOWN_VERSION:
			ret = "unrecognized protocol version";
			break;
		case M_HTTP_ERROR_REQUEST_METHOD: /* 501 */
			ret = "given request method is unsupported (501)";
			break;
		case M_HTTP_ERROR_REQUEST_URI:
			ret = "error parsing URI (may be malformed)";
			break;
		case M_HTTP_ERROR_HEADER_LENGTH: /* 413 (8k limit) */
			ret = "header exceeds max length - 8KiB (413)";
			break;
		case M_HTTP_ERROR_HEADER_FOLD: /* 400/502 */
			ret = "message uses header folding, this is no longer supported (400/502)";
			break;
		case M_HTTP_ERROR_HEADER_NOTALLOWED:
			ret = "header unsupported"; /* TODO: change to a more informative error message */
			break;
		case M_HTTP_ERROR_HEADER_INVALID:
			ret = "header missing info";
			break;
		case M_HTTP_ERROR_HEADER_MALFORMEDVAL: /* 400 */
			ret = "header value malformed (400)";
			break;
		case M_HTTP_ERROR_HEADER_DUPLICATE: /* 400 */
			ret = "detected duplicate header, for header type that must be unique (400)";
			break;
		case M_HTTP_ERROR_CHUNK_LENGTH:
			ret = "chunk length is malformed, or exceeds maximum - 6KiB";
			break;
		case M_HTTP_ERROR_CHUNK_MALFORMED:
			ret = "chunk length missing or negative";
			break;
		case M_HTTP_ERROR_CHUNK_EXTENSION:
			ret = "chunk extension missing or malformed"; /* TODO: change to a more informative error message */
			break;
		case M_HTTP_ERROR_CHUNK_DATA_MALFORMED:
			ret = "chunk missing line-end characters";
			break;
		case M_HTTP_ERROR_MALFORMED:
			ret = "message malformed";
			break;
		case M_HTTP_ERROR_BODYLEN_REQUIRED:
			ret = "body length required"; /* TODO: remove this error code? Not used, and seems same as M_HTTP_ERROR_LENGTH_REQUIRED */
			break;
		case M_HTTP_ERROR_MULTIPART_NOBOUNDARY:
			ret = "multipart boundary error"; /* TODO: change to a more informative error message */
			break;
		case M_HTTP_ERROR_MULTIPART_MISSING:
			ret = "multipart section missing"; /* TODO: change to a better error message (not even sure this is right) */
			break;
		case M_HTTP_ERROR_MULTIPART_MISSING_DATA:
			ret = "multipart section missing end sequence";
			break;
		case M_HTTP_ERROR_MULTIPART_INVALID:
			ret = "multipart section malformed";
			break;
		case M_HTTP_ERROR_UNSUPPORTED_DATA:
			ret = "message's body format is unrecognized or unsupported";
			break;
		case M_HTTP_ERROR_TEXTCODEC_FAILURE:
			ret = "text decode error";
			break;
		case M_HTTP_ERROR_USER_FAILURE:
			ret = "user failure"; /* TODO: change to a more informative error message */
			break;
	}
	return ret;
}
