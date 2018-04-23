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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_http_version_t M_http_version_from_str(const char *version)
{
	if (M_str_eq_max(version, "HTTP/", 5))
		version += 5;

	if (M_str_eq(version, "1.0"))
		return M_HTTP_VERSION_1_0;

	if (M_str_eq(version, "1.0"))
		return M_HTTP_VERSION_1_1;

	if (M_str_eq(version, "2"))
		return M_HTTP_VERSION_2;

	return M_HTTP_VERSION_UNKNOWN;
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
