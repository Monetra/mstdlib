/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_TLS_HOSTVALIDATE_H__
#define __M_TLS_HOSTVALIDATE_H__

enum M_tls_verify_host_flags {
	M_TLS_VERIFY_HOST_FLAG_NONE                = 0,      /*!< Perform no host matching                                                                */
	M_TLS_VERIFY_HOST_FLAG_VALIDATE_CN         = 1 << 0, /*!< Validate against the certificate common name                                            */
	M_TLS_VERIFY_HOST_FLAG_VALIDATE_SAN        = 1 << 1, /*!< Validate against the certificate SubjectAltName                                         */
	M_TLS_VERIFY_HOST_FLAG_ALLOW_WILDCARD      = 1 << 2, /*!< Allow the use of wildcards                                                              */
	M_TLS_VERIFY_HOST_FLAG_MULTILEVEL_WILDCARD = 1 << 3, /*!< Out-of-spec: Allow multilevel wildcards                                                 */
	M_TLS_VERIFY_HOST_FLAG_FUZZY_BASE_DOMAIN   = 1 << 4, /*!< Out-of-spec: Validate only the base domain, not subdomain. Localhost is assumed trusted */
	M_TLS_VERIFY_HOST_FLAG_NORMAL              = M_TLS_VERIFY_HOST_FLAG_VALIDATE_CN|M_TLS_VERIFY_HOST_FLAG_VALIDATE_SAN|M_TLS_VERIFY_HOST_FLAG_ALLOW_WILDCARD /*!< Default setting, normal expected behavior */
};

typedef enum M_tls_verify_host_flags M_tls_verify_host_flags_t;

M_bool M_tls_verify_host(X509 *x509, const char *hostname, M_tls_verify_host_flags_t flags);

#endif
