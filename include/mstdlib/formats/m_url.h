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
 * This parser supports many of the features of RFC 3986, but is not
 * a fully compliant URI parser.
 *
 * Specifically from RFC3986 section 1.1.2:
 *
 * SUPPORTED?
 *
 * - YES  ftp://ftp.is.co.za/rfc/rfc1808.txt
 * - YES  http://www.ietf.org/rfc/rfc2396.txt
 * - YES  ldap://[2001:db8::7]/c=GB?objectClass?one
 * - NO   mailto:John.Doe@example.com
 * - NO   news:comp.infosystems.www.servers.unix
 * - NO   tel:+1-816-555-1212
 * - YES  telnet://192.0.2.16:80/
 * - NO   urn:oasis:names:specification:docbook:dtd:xml:4.1.2
 * - YES  http://http:%2f%2fhttp:%2f%2f@http://http://?http://#http://
 *
 * @{
 */

struct M_url;
typedef struct M_url M_url_t;


/*! Parse URL string into structure parts.
 *
 * \param[in]  url_str URL string to be parsed.
 *
 * \return Parsed URL struct or NULL on error.
 */
M_API M_url_t *M_url_create(const char *url_str);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return schema string (NULL if none)
 */
M_API const char *M_url_schema(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url  parsed URL.
 * \param[in]  schema new schema to use
 */
M_API void        M_url_set_schema(M_url_t *url, const char *schema);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return host string (NULL if none)
 */
M_API const char *M_url_host(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url  parsed URL.
 * \param[in]  host new host to use
 */
M_API void        M_url_set_host(M_url_t *url, const char *host);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return port string (NULL if none)
 */
M_API const char *M_url_port(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url  parsed URL.
 * \param[in]  port new port to use
 */
M_API void        M_url_set_port(M_url_t *url, const char *port);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return path string (NULL if none)
 */
M_API const char *M_url_path(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url  parsed URL.
 * \param[in]  path new path to use
 */
M_API void        M_url_set_path(M_url_t *url, const char *path);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return query string (NULL if none)
 */
M_API const char *M_url_query(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url   parsed URL.
 * \param[in]  query new query to use
 */
M_API void        M_url_set_query(M_url_t *url, const char *query);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return fragment string (NULL if none)
 */
M_API const char *M_url_fragment(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url      parsed URL.
 * \param[in]  fragment new fragment to use
 */
M_API void        M_url_set_fragment(M_url_t *url, const char *fragment);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return userinfo string (NULL if none)
 */
M_API const char *M_url_userinfo(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url      parsed URL.
 * \param[in]  userinfo new userinfo to use
 */
M_API void        M_url_set_userinfo(M_url_t *url, const char *userinfo);

/*! Getter function
 *
 * \param[in]  url parsed URL.
 *
 * \return port as M_uint16 (0 if none)
 */
M_API M_uint16    M_url_port_u16(const M_url_t *url);

/*! Setter function
 *
 * \param[in]  url  parsed URL.
 * \param[in]  port new port to use
 */
M_API void        M_url_set_port_u16(M_url_t *url, M_uint16 port);

/*! Destroy parsed URL struct
 *
 * \param[in]  url struct to destroy
 */
M_API void M_url_destroy(M_url_t *url);

/*! @} */

__END_DECLS

#endif
