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

/* Adds headers and body. */
static M_bool M_http_simple_write_int(M_buf_t *buf, const M_hash_dict_t *headers, const char *data, size_t data_len)
{
	M_http_t           *http = NULL;
	M_hash_dict_enum_t *he;
	const char         *key;
	const char         *val;
	char                tempa[32];
	M_int64             i64v;

	/* We want to push the headers into an http object to ensure they're in a
 	 * properly configured hashtable. We need to ensure flags like casecomp
	 * are enabled. */
	http = M_http_create();
	if (headers != NULL) {
		M_http_set_headers(http, headers, M_FALSE);
		headers = M_http_headers(http);
	}

	/* Validate some headers. */
	if (data != NULL && data_len != 0) {
		/* Can't have transfer-encoding AND data. */
		if (M_hash_dict_get(headers, "transfer-encoding", NULL)) {
			goto err;
		}
	}

	/* Ensure that content-length is present (even if body length is zero). */
	if (M_hash_dict_get(headers, "content-length", &val)) {
		/* If content-length is already set we need to ensure it matches data
		 * since this is considered a complete message. */
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS || i64v < 0) {
			goto err;
		}

		if ((size_t)i64v != data_len) {
			goto err;
		}
	} else {
		M_snprintf(tempa, sizeof(tempa), "%zu", data_len);
		M_http_set_header(http, "content-length", tempa);
	}

	/* We're not going to convert duplicates into a list.
 	 * We'll write them as individual ones. */
	M_hash_dict_enumerate(headers, &he);
	while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
		M_buf_add_str(buf, key);
		M_buf_add_byte(buf, ':');
		M_buf_add_str(buf, val);
		M_buf_add_str(buf, "\r\n");
	}
	M_hash_dict_enumerate_free(he);

	/* End of start/headers. */
	M_buf_add_str(buf, "\r\n");

	/* Add the body data. */
	M_buf_add_bytes(buf, data, data_len);

	M_http_destroy(http);
	return M_TRUE;

err:
	M_http_destroy(http);
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unsigned char *M_http_simple_write_request(M_http_method_t method, const char *uri, M_http_version_t version,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len)
{
	M_bool   res;
	M_buf_t *buf = M_buf_create();

	res = M_http_simple_write_request_buf(buf, method, uri, version, headers, data, data_len);

	if (!res) {
		if (len != NULL) {
			*len = 0;
		}
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish(buf, len);
}

M_bool M_http_simple_write_request_buf(M_buf_t *buf, M_http_method_t method, const char *uri,
	M_http_version_t version, const M_hash_dict_t *headers, const char *data, size_t data_len)
{
	size_t start_len = M_buf_len(buf);

	if (method == M_HTTP_METHOD_UNKNOWN || M_str_isempty(uri) || version == M_HTTP_VERSION_UNKNOWN ||
		(headers == NULL && (data == NULL || data_len == 0))) {
		return M_FALSE;
	}

	/* request-line = method SP request-target SP HTTP-version CRLF */
	M_buf_add_str(buf, M_http_method_to_str(method));
	M_buf_add_byte(buf, ' ');

	/* We expect the uri to be encoded. We'll check for spaces and
 	 * non-ascii characters. If found we'll encode it to be safe because
	 * we don't want to build an invalid request. We're going to use URL
	 * encoding with %20 for spaces. Some web sites want %20 and some want
	 * +. We have no way to know so we'll go with %20 since it's more common. */
	if (M_str_chr(uri, ' ') != NULL || !M_str_isascii(uri)) {
		if (M_textcodec_encode_buf(buf, uri, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_URL) != M_TEXTCODEC_ERROR_SUCCESS) {
			M_buf_truncate(buf, start_len);
			return M_FALSE;
		}
	} else {
		M_buf_add_str(buf, uri);
	}
	M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, M_http_version_to_str(version));
	M_buf_add_str(buf, "\r\n");

	if (!M_http_simple_write_int(buf, headers, data, data_len)) {
		M_buf_truncate(buf, start_len);
		return M_FALSE;
	}

	return M_TRUE;
}

unsigned char *M_http_simple_write_response(M_http_version_t version, M_uint32 code, const char *reason,
	const M_hash_dict_t *headers, const char *data, size_t data_len, size_t *len)
{
	M_bool   res;
	M_buf_t *buf = M_buf_create();

	res = M_http_simple_write_response_buf(buf, version, code, reason, headers, data, data_len);

	if (!res) {
		if (len != NULL) {
			*len = 0;
		}
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish(buf, len);
}

M_bool M_http_simple_write_response_buf(M_buf_t *buf, M_http_version_t version, M_uint32 code,
	const char *reason, const M_hash_dict_t *headers, const char *data, size_t data_len)
{
	size_t start_len = M_buf_len(buf);

	if (version == M_HTTP_VERSION_UNKNOWN) {
		return M_FALSE;
	}

	/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */
	M_buf_add_str(buf, M_http_version_to_str(version));
	M_buf_add_byte(buf, ' ');

	M_buf_add_int(buf, code);
	M_buf_add_byte(buf, ' ');

	if (M_str_isempty(reason)) {
		reason = M_http_code_to_reason(code);
	}
	M_buf_add_str(buf, reason);
	M_buf_add_str(buf, "\r\n");

	if (!M_http_simple_write_int(buf, headers, data, data_len)) {
		M_buf_truncate(buf, start_len);
		return M_FALSE;
	}

	return M_TRUE;
}
