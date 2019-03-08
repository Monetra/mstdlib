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

#ifndef __M_TLS_SERVERCTX_INT_H__
#define __M_TLS_SERVERCTX_INT_H__

#include <openssl/ssl.h>
#include "m_tls_ctx_common.h"

struct M_tls_serverctx {
	M_thread_mutex_t   *lock;                   /*!< Mutex to protect concurrent access                                 */
	M_list_t           *children;               /*!< List of M_tls_serverctx_t children for SNI                         */
	M_tls_serverctx_t  *parent;                 /*!< If this CTX is an SNI child, this will be set                      */
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
	X509               *x509;                   /*!< OpenSSL < 1.0.2 doesn't allow retrieval of certs from a CTX, cache */
#endif
	SSL_CTX            *ctx;                    /*!< OpenSSL ctx                                                        */
	DH                 *dh;                     /*!< DH parameters to use for forward secrecy                           */
	size_t              ref_cnt;                /*!< Reference count to prevent destroy of CTX while connections active */
	M_uint64            negotiation_timeout_ms; /*!< Amount of time negotiation can take                                */
	M_bool              sessions_enabled;       /*!< Whether or not to enable session resumption support                */
	unsigned char      *alpn_apps;              /*!< ALPN supported applications                                        */
	size_t              alpn_apps_len;          /*!< ALPN supported applications length                                 */
};

void M_tls_serverctx_refcnt_decrement(M_tls_serverctx_t *ctx);

#endif
