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

const char *M_http_uri(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->uri;
}

M_bool M_http_set_uri(M_http_t *http, const char *uri)
{

	if (http == NULL || uri == NULL)
		return M_FALSE;

	M_url_destroy(http->url_st);
	http->url_st = M_url_create(uri);
	if (http->url_st == NULL)
		return M_FALSE;

	M_free(http->uri);
	http->uri = M_strdup(uri);

	M_hash_dict_destroy(http->query_args);
	M_http_parse_query_string(M_url_query(http->url_st), M_TEXTCODEC_UNKNOWN);

	return M_TRUE;
}

const char *M_http_host(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return M_url_host(http->url_st);
}

M_bool M_http_port(const M_http_t *http, M_uint16 *port)
{
	M_uint16 http_port;

	if (http == NULL)
		return M_FALSE;

	http_port = M_url_port_u16(http->url_st);

	if (http_port == 0)
		return M_FALSE;

	if (port != NULL)
		*port = http_port;

	return M_TRUE;
}

const char *M_http_path(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return M_url_path(http->url_st);
}

const char *M_http_query_string(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return M_url_query(http->url_st);
}

const M_hash_dict_t *M_http_query_args(const M_http_t *http)
{
	if (http == NULL)
		return NULL;
	return http->query_args;
}
