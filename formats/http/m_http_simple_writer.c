/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

static char *M_http_writer_get_date(void)
{
	M_buf_t        *buf;
	M_time_gmtm_t   tm;
	const char     *day[]   = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	const char     *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	buf = M_buf_create();
	M_mem_set(&tm, 0, sizeof(tm));
	M_time_togm(M_time(), &tm);

	if (tm.wday < 0 || tm.wday > 6)
		goto err;
	M_buf_add_str(buf, day[tm.wday]);
	M_buf_add_str(buf, ", ");

	M_buf_add_int_just(buf, tm.day, 2);
	M_buf_add_byte(buf, ' ');

	if (tm.month <= 0 || tm.month > 12)
		goto err;
	M_buf_add_str(buf, month[tm.month-1]);
	M_buf_add_byte(buf, ' ');

	M_buf_add_int(buf, tm.year);
	M_buf_add_byte(buf, ' ');

	M_buf_add_int_just(buf, tm.hour, 2);
	M_buf_add_byte(buf, ':');

	M_buf_add_int_just(buf, tm.min, 2);
	M_buf_add_byte(buf, ':');

	M_buf_add_int_just(buf, tm.sec, 2);
	M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, "GMT");

	return M_buf_finish_str(buf, NULL);

err:
	M_buf_cancel(buf);
	return NULL;
}

/* Headers that are specific to a request. */
static M_hash_dict_t *M_http_simple_write_request_headers(const M_hash_dict_t *headers, const char *host, unsigned short port, const char *user_agent)
{
	M_hash_dict_t *myheaders;
	char          *hostport;

	myheaders = M_hash_dict_create(8, 75, M_HASH_DICT_CASECMP|M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_MULTI_VALUE);
	M_hash_dict_merge(&myheaders, M_hash_dict_duplicate(headers));

	/* Add the host. */
	if (M_str_isempty(host) && M_hash_dict_get(myheaders, "Host", NULL)) {
		/* Host is required. */
		M_hash_dict_destroy(myheaders);
		return NULL;
	}

	M_hash_dict_remove(myheaders, "Host");
	if (port == 0 || port == 80) {
		M_hash_dict_insert(myheaders, "Host", host);
	} else {
		M_asprintf(&hostport, "%s:%u", host, port);
		M_hash_dict_insert(myheaders, "Host", hostport);
		M_free(hostport);
	}

	/* Add the user agent. */
	if (!M_str_isempty(user_agent))
		M_hash_dict_insert(myheaders, "User-Agent", user_agent);

	return myheaders;
}

/* Adds headers and body. */
static M_bool M_http_simple_write_int(M_buf_t *buf, const char *content_type, const M_hash_dict_t *headers,
		const unsigned char *data, size_t data_len, const char *charset)
{
	M_http_t           *http = NULL;
	M_hash_dict_t      *myheaders;
	M_list_str_t       *header_keys;
	const char         *key;
	const char         *val;
	char               *temp;
	M_buf_t            *tbuf;
	unsigned char      *mydata = NULL;
	char                tempa[128];
	M_int64             i64v;
	size_t              len;
	size_t              i;

	/* We want to push the headers into an http object to ensure they're in a
	 * properly configured hashtable. We need to ensure flags like casecomp
	 * are enabled. Also allows us to get joined header values back out. */
	http = M_http_create();
	if (headers != NULL)
		M_http_set_headers(http, headers, M_FALSE);
	myheaders = M_http_headers_dict(http);

	/* Validate some headers. */
	if (data != NULL && data_len != 0) {
		/* Can't have transfer-encoding AND data. */
		if (M_hash_dict_get(myheaders, "Transfer-Encoding", NULL)) {
			goto err;
		}
	}

	/* Ensure that content-length is present (even if body length is zero). */
	if (M_hash_dict_get(myheaders, "Content-Length", &val)) {
		/* If content-length is already set we need to ensure it matches data
		 * since this is considered a complete message. */
		if (M_str_to_int64_ex(val, M_str_len(val), 10, &i64v, NULL) != M_STR_INT_SUCCESS || i64v < 0) {
			goto err;
		}

		/* If we have data the data length must match the length of the data. */
		if (data != NULL && (size_t)i64v != data_len) {
			goto err;
		}
	} else {
		/* Data can be binary so we'll check for a text encoding to know if
		 * we can treat the data as text. */
		if (!M_str_isempty(charset) && !M_str_isempty((const char *)data)) {
			/* If we have data and a content length wasn't already set then we
			 * must have the data length. */
			if (data_len == 0) {
				data_len = M_str_len((const char *)data);
			}
		}

		M_snprintf(tempa, sizeof(tempa), "%zu", data_len);
		M_http_set_header(http, "Content-Length", tempa);
	}

	if (!M_str_isempty(content_type)) {
		M_http_set_header(http, "Content-Type", content_type);
	} else {
		/* Ensure something is set for content type. */
		if (!M_hash_dict_get(myheaders, "Content-Type", NULL)) {
			/* If there isn't a content type we set a default. */
			if (M_str_isempty(charset)) {
				M_http_set_header(http, "Content-Type", "application/octet-stream");
			} else {
				M_http_set_header(http, "Content-Type", "text/plain");
			}
		}
	}

	/* If we've encoded the data (or utf-8) mark the content as such. */
	if (!M_str_isempty(charset)) {
		/* We might already have a charaset modifier but that's okay.
 		 * The last value of the same key will be used. Since we're
		 * adding charset as the last element ours will be used.
		 *
		 * It is *possible* that we'll get back a Content-Type value
		 * with multiple elements. That's invalid because Content-Type
		 * is only allowed one type to be specified. We're not going
		 * to deal with someone making invalid headers. */
		temp = M_http_header(http, "Content-Type");
		tbuf = M_buf_create();
		M_buf_add_str(tbuf, temp);
		M_free(temp);
		M_buf_add_str(tbuf, "; ");
		M_snprintf(tempa, sizeof(tempa), "charset=%s", charset);
		M_buf_add_str(tbuf, tempa);
		temp = M_buf_finish_str(tbuf, NULL);
		M_http_add_header(http, "Content-Type", temp);
		M_free(temp);
	}

	/* Set the date if not present. */
	if (!M_hash_dict_get(myheaders, "Date", NULL)) {
		temp = M_http_writer_get_date();
		M_http_set_header(http, "Date", temp);
		M_free(temp);
	}

	/* Write out the headers. We use the key list
	 * instead of enumerating the headers dict directly
	 * because a multi dict will have the key value
	 * multiple times for each value. We only want
	 * a header added once because we're going to
	 * have all values combined. */
	header_keys = M_http_headers(http);
	len         = M_list_str_len(header_keys);
	for (i=0; i<len; i++) {
		key = M_list_str_at(header_keys, i);

		/* Get the combined header value. */
		temp = M_http_header(http, key);

		M_buf_add_str(buf, key);
		/* We're adding the optional white space after the colon because it's
		 * easier to read when looking at a trace. */
		M_buf_add_str(buf, ": ");
		M_buf_add_str(buf, temp);
		M_buf_add_str(buf, "\r\n");

		M_free(temp);
	}
	M_list_str_destroy(header_keys);

	/* End of start/headers. */
	M_buf_add_str(buf, "\r\n");

	/* Add the body data. */
	if (data != NULL)
		M_buf_add_bytes(buf, data, data_len);

	M_free(mydata);
	M_hash_dict_destroy(myheaders);
	M_http_destroy(http);
	return M_TRUE;

err:
	M_free(mydata);
	M_hash_dict_destroy(myheaders);
	M_http_destroy(http);
	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unsigned char *M_http_simple_write_request(M_http_method_t method,
	const char *host, unsigned short port, const char *uri,
	const char *user_agent, const char *content_type, const M_hash_dict_t *headers,
	const unsigned char *data, size_t data_len, const char *charset, size_t *len)
{
	M_bool   res;
	M_buf_t *buf = M_buf_create();

	res = M_http_simple_write_request_buf(buf, method, host, port, uri, user_agent, content_type, headers, data, data_len, charset);

	if (!res) {
		if (len != NULL) {
			*len = 0;
		}
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish(buf, len);
}

M_bool M_http_simple_write_request_buf(M_buf_t *buf, M_http_method_t method,
	const char *host, unsigned short port, const char *uri,
	const char *user_agent, const char *content_type, const M_hash_dict_t *headers,
	const unsigned char *data, size_t data_len, const char *charset)
{
	M_hash_dict_t *myheaders = NULL;
	size_t         start_len = M_buf_len(buf);

	if (method == M_HTTP_METHOD_UNKNOWN)
		return M_FALSE;

	if  (M_str_isempty(uri))
		uri = "/";

	/* request-line = method SP request-target SP HTTP-version CRLF */
	M_buf_add_str(buf, M_http_method_to_str(method));
	M_buf_add_byte(buf, ' ');

	/* We expect the uri to be encoded. We'll check for spaces and
	 * non-ascii characters. If found we'll encode it to be safe because
	 * we don't want to build an invalid request. We're going to use URL minimal
	 * encoding to try to fix any thing that shouldn't be there. Minimal will
	 * keep things like '/' so we won't end up with something unreadable. */
	if (M_str_chr(uri, ' ') != NULL || !M_str_isascii(uri)) {
		if (M_textcodec_encode_buf(buf, uri, M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PERCENT_URLMIN) != M_TEXTCODEC_ERROR_SUCCESS) {
			goto err;
		}
	} else {
		M_buf_add_str(buf, uri);
	}
	M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, M_http_version_to_str(M_HTTP_VERSION_1_1));
	M_buf_add_str(buf, "\r\n");

	/* Generate the request headers we might want added. */
	myheaders = M_http_simple_write_request_headers(headers, host, port, user_agent);
	if (myheaders == NULL)
		goto err;

	if (!M_http_simple_write_int(buf, content_type, myheaders, data, data_len, charset))
		goto err;

	M_hash_dict_destroy(myheaders);
	return M_TRUE;

err:
	M_buf_truncate(buf, start_len);
	return M_FALSE;
}

unsigned char *M_http_simple_write_response(M_uint32 code, const char *reason,
	const char *content_type, const M_hash_dict_t *headers, const unsigned char *data, size_t data_len,
	const char *charset, size_t *len)
{
	M_bool   res;
	M_buf_t *buf = M_buf_create();

	res = M_http_simple_write_response_buf(buf, code, reason, content_type, headers, data, data_len, charset);

	if (!res) {
		if (len != NULL) {
			*len = 0;
		}
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish(buf, len);
}

M_bool M_http_simple_write_response_buf(M_buf_t *buf, M_uint32 code, const char *reason,
	const char *content_type, const M_hash_dict_t *headers, const unsigned char *data, size_t data_len,
	const char *charset)
{
	size_t start_len = M_buf_len(buf);

	/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */
	M_buf_add_str(buf, M_http_version_to_str(M_HTTP_VERSION_1_1));
	M_buf_add_byte(buf, ' ');

	M_buf_add_int(buf, code);
	M_buf_add_byte(buf, ' ');

	if (M_str_isempty(reason)) {
		reason = M_http_code_to_reason(code);
	}
	M_buf_add_str(buf, reason);
	M_buf_add_str(buf, "\r\n");

	if (!M_http_simple_write_int(buf, content_type, headers, data, data_len, charset)) {
		M_buf_truncate(buf, start_len);
		return M_FALSE;
	}

	return M_TRUE;
}
