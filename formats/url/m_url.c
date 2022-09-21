/* The MIT License (MIT)
 *
 * Copyright (c) 2022 Monetra Technologies, LLC.
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

#include <mstdlib/mstdlib.h>
#include <mstdlib/formats/m_url.h>
#include "url_parser.h"

struct M_url {
	char     *schema;
	char     *host;
	char     *port;
	char     *path;
	char     *query;
	char     *fragment;
	char     *userinfo;
	M_uint16  port_u16;
};

static void M_url_set_field(const char *url_str, struct http_parser_url *url, char **field, int idx)
{
	if ((url->field_set & (1 << idx)) != 0) {
		const size_t  off = url->field_data[idx].off;
		const int     len = url->field_data[idx].len;
		const char   *s   = &url_str[off];
		M_asprintf(field, "%.*s", len, s);
	}
}

static void M_url_default_port(M_url_t *url)
{
	size_t i;
	struct {
		const char *schema;
		M_uint16    port;
	} lookup[] = {
		{ "https", 443 },
		{ "http" , 80  },
	};

	for (i=0; i<sizeof(lookup)/sizeof(lookup[0]); i++) {
		if (M_str_eq(url->schema, lookup[i].schema)) {
			url->port_u16 = lookup[i].port;
			break;
		}
	}

}

M_url_t *M_url_create(const char *url_str)
{
	struct http_parser_url  url_st = { 0 };
	int                     res    = 0;
	M_url_t                *url    = NULL;

	if (url_str == NULL)
		return NULL;

	res = http_parser_parse_url(url_str, M_str_len(url_str), 0, &url_st);
	if (res != 0) 
		return NULL;

	url = M_malloc_zero(sizeof(*url));
	url->port_u16 = url_st.port;

	M_url_set_field(url_str, &url_st, &url->schema  , UF_SCHEMA);
	M_url_set_field(url_str, &url_st, &url->host    , UF_HOST);
	M_url_set_field(url_str, &url_st, &url->port    , UF_PORT);
	M_url_set_field(url_str, &url_st, &url->path    , UF_PATH);
	M_url_set_field(url_str, &url_st, &url->query   , UF_QUERY);
	M_url_set_field(url_str, &url_st, &url->fragment, UF_FRAGMENT);
	M_url_set_field(url_str, &url_st, &url->userinfo, UF_USERINFO);

	if (url->port == NULL)
		M_url_default_port(url);

	return url;
}

const char *M_url_schema(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->schema;
}

void M_url_set_schema(M_url_t *url, const char *schema)
{
	if (url == NULL)
		return;
	M_free(url->schema);
	url->schema = M_strdup(schema);
}

const char *M_url_host(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->host;
}

void M_url_set_host(M_url_t *url, const char *host)
{
	if (url == NULL)
		return;
	M_free(url->host);
	url->host = M_strdup(host);
}

const char *M_url_port(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->port;
}

void M_url_set_port(M_url_t *url, const char *port)
{
	if (url == NULL)
		return;
	M_free(url->port);
	url->port = M_strdup(port);
	url->port_u16 = (M_uint16)M_str_to_uint32(port);
}

const char *M_url_path(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->path;
}

void M_url_set_path(M_url_t *url, const char *path)
{
	if (url == NULL)
		return;
	M_free(url->path);
	url->path = M_strdup(path);
}

const char *M_url_query(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->query;
}

void M_url_set_query(M_url_t *url, const char *query)
{
	if (url == NULL)
		return;
	M_free(url->query);
	url->query = M_strdup(query);
}

const char *M_url_fragment(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->fragment;
}

void M_url_set_fragment(M_url_t *url, const char *fragment)
{
	if (url == NULL)
		return;
	M_free(url->fragment);
	url->fragment = M_strdup(fragment);
}

const char *M_url_userinfo(const M_url_t *url)
{
	if (url == NULL)
		return NULL;
	return url->userinfo;
}

void M_url_set_userinfo(M_url_t *url, const char *userinfo)
{
	if (url == NULL)
		return;
	M_free(url->userinfo);
	url->userinfo = M_strdup(userinfo);
}

M_uint16 M_url_port_u16(const M_url_t *url)
{
	if (url == NULL)
		return 0;
	return url->port_u16;
}

void M_url_set_port_u16(M_url_t *url, M_uint16 port)
{
	if (url == NULL)
		return;
	url->port_u16 = port;
}

void M_url_destroy(M_url_t *url)
{
	if (url == NULL)
		return;

	M_free(url->schema);
	M_free(url->host);
	M_free(url->port);
	M_free(url->path);
	M_free(url->query);
	M_free(url->fragment);
	M_free(url->userinfo);
	M_free(url);
}
