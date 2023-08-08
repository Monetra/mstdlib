/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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
	http->headers     = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void(*)(void *))M_http_header_destroy);
	http->trailers    = M_hash_strvp_create(8, 75, M_HASH_STRVP_CASECMP|M_HASH_STRVP_KEYS_ORDERED, (void(*)(void *))M_http_header_destroy);
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
	M_hash_strvp_destroy(http->headers, M_TRUE);
	M_free(http->content_type);
	M_free(http->charset);
	M_hash_strvp_destroy(http->trailers, M_TRUE);
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

	return M_HTTP_VERSION_UNKNOWN;
}

const char *M_http_version_to_str(M_http_version_t version)
{
	switch (version) {
		case M_HTTP_VERSION_1_0:
			return "HTTP/1.0";
		case M_HTTP_VERSION_1_1:
			return "HTTP/1.1";
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

	if (M_str_caseeq(method, "PATCH"))
		return M_HTTP_METHOD_PATCH;

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
		case M_HTTP_METHOD_PATCH:
			return "PATCH";
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


#define ERRCASE(x) case x: return #x

const char *M_http_errcode_to_str(M_http_error_t err)
{
	switch (err) {
		ERRCASE(M_HTTP_ERROR_SUCCESS);
		ERRCASE(M_HTTP_ERROR_SUCCESS_MORE_POSSIBLE);
		ERRCASE(M_HTTP_ERROR_INVALIDUSE);
		ERRCASE(M_HTTP_ERROR_STOP);
		ERRCASE(M_HTTP_ERROR_MOREDATA);
		ERRCASE(M_HTTP_ERROR_LENGTH_REQUIRED);
		ERRCASE(M_HTTP_ERROR_CHUNK_EXTENSION_NOTALLOWED);
		ERRCASE(M_HTTP_ERROR_TRAILER_NOTALLOWED);
		ERRCASE(M_HTTP_ERROR_URI);
		ERRCASE(M_HTTP_ERROR_STARTLINE_LENGTH);
		ERRCASE(M_HTTP_ERROR_STARTLINE_MALFORMED);
		ERRCASE(M_HTTP_ERROR_UNKNOWN_VERSION);
		ERRCASE(M_HTTP_ERROR_REQUEST_METHOD);
		ERRCASE(M_HTTP_ERROR_HEADER_LENGTH);
		ERRCASE(M_HTTP_ERROR_HEADER_FOLD);
		ERRCASE(M_HTTP_ERROR_HEADER_INVALID);
		ERRCASE(M_HTTP_ERROR_HEADER_DUPLICATE);
		ERRCASE(M_HTTP_ERROR_CHUNK_STARTLINE_LENGTH);
		ERRCASE(M_HTTP_ERROR_CHUNK_LENGTH);
		ERRCASE(M_HTTP_ERROR_CHUNK_MALFORMED);
		ERRCASE(M_HTTP_ERROR_CHUNK_EXTENSION);
		ERRCASE(M_HTTP_ERROR_CHUNK_DATA_MALFORMED);
		ERRCASE(M_HTTP_ERROR_CONTENT_LENGTH_MALFORMED);
		ERRCASE(M_HTTP_ERROR_NOT_HTTP);
		ERRCASE(M_HTTP_ERROR_MULTIPART_NOBOUNDARY);
		ERRCASE(M_HTTP_ERROR_MULTIPART_MISSING);
		ERRCASE(M_HTTP_ERROR_MULTIPART_MISSING_DATA);
		ERRCASE(M_HTTP_ERROR_MULTIPART_INVALID);
		ERRCASE(M_HTTP_ERROR_UNSUPPORTED_DATA);
		ERRCASE(M_HTTP_ERROR_TEXTCODEC_FAILURE);
		ERRCASE(M_HTTP_ERROR_USER_FAILURE);
	}

	return "unknown";
}


char *M_http_generate_query_string(const char *uri, const M_hash_dict_t *params)
{
	M_buf_t *buf = M_buf_create();

	if (!M_http_generate_query_string_buf(buf, uri, params)) {
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish_str(buf, NULL);
}


M_bool M_http_generate_query_string_buf(M_buf_t *buf, const char *uri, const M_hash_dict_t *params)
{
	size_t start_len = M_buf_len(buf);
	M_bool ret;

	if (buf == NULL)
		return M_FALSE;

	if (M_str_isempty(uri) && M_hash_dict_num_keys(params) == 0)
		return M_FALSE;

	M_buf_add_str(buf, uri);

	if (M_hash_dict_num_keys(params) == 0)
		return M_TRUE;

	M_buf_add_byte(buf, '?');

	ret = M_http_generate_form_data_string_buf(buf, params);
	if (!ret) {
		M_buf_truncate(buf, start_len);
	}
	return ret;
}


M_hash_dict_t *M_http_parse_query_string(const char *data, M_textcodec_codec_t codec)
{
	if (M_str_isempty(data))
		return NULL;

	if (*data == '?')
		data++;

	return M_http_parse_form_data_string(data, codec);
}


char *M_http_generate_form_data_string(const M_hash_dict_t *params)
{
	M_buf_t *buf = M_buf_create();

	if (!M_http_generate_form_data_string_buf(buf, params)) {
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish_str(buf, NULL);
}


M_bool M_http_generate_form_data_string_buf(M_buf_t *buf, const M_hash_dict_t *params)
{
	size_t               start_len = M_buf_len(buf);
	M_bool               ret       = M_FALSE;
	M_hash_dict_enum_t  *it        = NULL;
	const char          *key;
	const char          *value;
	M_bool               first     = M_TRUE;

	/* A form data string will look like this:
	 *
	 *   f1=v1&f2=v2&f3=v3
	 *
	 * Most web frameworks allow multiple values to be set for the same field. If you a multi-map is passed in
	 * as 'params' (multiple values per key), the following will be output:
	 *   f1=v1_1&f1=v1_2&f2=v2_1
	 *
	 * The second case is handled for us automatically, due to how a enumeration over a multi-map is implemented.
	 */

	if (buf == NULL) {
		return M_FALSE;
	}

	M_hash_dict_enumerate(params, &it);
	while (M_hash_dict_enumerate_next(params, it, &key, &value)) {

		if (M_str_isempty(key) || M_str_isempty(value)) {
			continue;
		}

		if (!first)
			M_buf_add_byte(buf, '&');
		first = M_FALSE;

		if (M_textcodec_encode_buf(buf, key, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_FORM) != M_TEXTCODEC_ERROR_SUCCESS) {
			goto done;
		}

		M_buf_add_byte(buf, '=');

		if (!M_str_isempty(value)) {
			if (M_textcodec_encode_buf(buf, value, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_FORM) != M_TEXTCODEC_ERROR_SUCCESS) {
				goto done;
			}
		}
	}

	ret = M_TRUE;

done:
	M_hash_dict_enumerate_free(it);

	/* If there was an error, set buffer contents back to what they were before this function was called. */
	if (!ret) {
		M_buf_truncate(buf, start_len);
	}

	return ret;
}


M_hash_dict_t *M_http_parse_form_data_string(const char *data, M_textcodec_codec_t codec)
{
	M_hash_dict_t  *args      = NULL;
	char          **parts     = NULL;
	char          **kv        = NULL;
	size_t          num_parts = 0;
	size_t          num_kv    = 0;
	size_t          i;

	if (M_str_isempty(data))
		return NULL;

	args = M_hash_dict_create(16, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE|M_HASH_DICT_MULTI_CASECMP);

	/* Split on & to get sets of key value pairs. */
	parts = M_str_explode_str('&', data, &num_parts);
	if (parts == NULL || num_parts == 0)
		goto err;

	for (i=0; i<num_parts; i++) {
		const char          *key  = NULL;
		const char          *val  = NULL;
		char                *dkey = NULL;
		char                *dval = NULL;
		char                *ekey = NULL;
		char                *eval = NULL;
		M_textcodec_error_t  tres = M_TEXTCODEC_ERROR_SUCCESS;

		/* Split the key and value. We'll ignore multiple ='s
		 * and treat additional ones as part of the value. */
		kv = M_str_explode_str_quoted('=', parts[i], 0, 0, 2, &num_kv);
		if (kv == NULL) {
			goto err;
		}

		/* Get the key. */
		if (num_kv >= 1) {
			key = kv[0];
		}
		/* Get the value (optional). */
		if (num_kv >= 2) {
			val = kv[1];
		}

		/* Decode the key from form encoding. */
		tres = M_textcodec_decode(&dkey, key, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_FORM);
		if (M_textcodec_error_is_error(tres)) {
			goto loop_end;
		}

		if (M_str_isempty(val)) {
			/* Since value can be empty we don't need to waste time decoding. Set to an empty default. */
			dval = M_strdup("");
		} else {
			/* Decode the value form form encoding. */
			tres = M_textcodec_decode(&dval, val, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_FORM);
			if (M_textcodec_error_is_error(tres)) {
				goto loop_end;
			}
		}

		/* If an additional codec was specified we need to decode the form decoded data. */
		if (codec == M_TEXTCODEC_UNKNOWN || codec == M_TEXTCODEC_UTF8) {
			/* No need to decode if asked not to or already in utf-8. */
			ekey = dkey;
			dkey = NULL;
			eval = dval;
			dval = NULL;
		} else {
			/* Decode the key and value. */
			tres = M_textcodec_decode(&ekey, dkey, M_TEXTCODEC_EHANDLER_FAIL, codec);
			if (M_textcodec_error_is_error(tres)) {
				goto loop_end;
			}
			tres = M_textcodec_decode(&eval, dval, M_TEXTCODEC_EHANDLER_FAIL, codec);
			if (M_textcodec_error_is_error(tres)) {
				goto loop_end;
			}
		}

		/* Insert our data. */
		M_hash_dict_insert(args, ekey, eval);

loop_end:
		M_free(ekey);
		M_free(eval);
		M_free(dkey);
		M_free(dval);
		M_str_explode_free(kv, num_kv);
		kv     = NULL;
		num_kv = 0;

		if (M_textcodec_error_is_error(tres)) {
			goto err;
		}
	}

	M_str_explode_free(parts, num_parts);
	return args;

err:
	M_str_explode_free(parts, num_parts);
	M_hash_dict_destroy(args);
	return NULL;
}
