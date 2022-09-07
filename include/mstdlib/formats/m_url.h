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

#ifndef __M_URL_H__
#define __M_URL_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_url URL
 *  \ingroup m_formats
 * URL Parser.
 *
 * This parser wraps the url-parser in nghttp2's codebase, which in turn was extracted
 * from nodejs' http_parser.  It supports many of the features of RFC 3986, but is not
 * a fully compliant URI parser.
 * @{
 */

typedef struct M_url {
	char     *schema;
	char     *host;
	char     *port;
	char     *path;
	char     *query;
	char     *fragment;
	char     *userinfo;
	M_uint16  port_u16;
} M_url_t;

/*! Parse URL string into structure parts.
 *
 * \param[in]  url_str URL string to be parsed.
 *
 * \return Parsed URL struct or NULL on error.
 */
M_API M_url_t *M_url_create(const char *url_str);

/*! Destroy parsed URL struct
 *
 * \param[in]  url struct to destroy
 */
M_API void M_url_destroy(M_url_t *url);

/*! @} */

__END_DECLS

#endif
