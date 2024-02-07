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

#ifndef __M_TLS_CTX_COMMON_H__
#define __M_TLS_CTX_COMMON_H__

#include <openssl/ssl.h>

SSL_CTX *M_tls_ctx_init(M_bool is_server);

/* Duplicates a server ctx, except for the server key/cert */
SSL_CTX *M_tls_ctx_duplicate_serverctx(SSL_CTX *orig_ctx, DH *dhparams, STACK_OF(X509) *trustlist, M_list_t *crls);

/*! Returns false if ctx can't be used */
void M_tls_ctx_destroy(SSL_CTX *ctx);
M_bool M_tls_ctx_set_protocols(SSL_CTX *ctx, int protocols);
M_bool M_tls_ctx_set_ciphers(SSL_CTX *ctx, const char *ciphers);
M_bool M_tls_ctx_set_cert(SSL_CTX *ctx, const unsigned char *key, size_t key_len, const unsigned char *crt, size_t crt_len, const unsigned char *intermediate, size_t intermediate_len, X509 **x509_out);
M_bool M_tls_ctx_load_os_trust(SSL_CTX *ctx);
M_bool M_tls_ctx_set_x509trust(SSL_CTX *ctx, STACK_OF(X509) *trustlist);
M_bool M_tls_ctx_set_trust_ca(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const unsigned char *ca, size_t len);
M_bool M_tls_ctx_set_trust_ca_file(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path);
M_bool M_tls_ctx_set_trust_cert(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const unsigned char *crt, size_t len);
M_bool M_tls_ctx_set_trust_cert_file(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path);
M_bool M_tls_ctx_set_trust_ca_dir(SSL_CTX *ctx, STACK_OF(X509) *trustlist_cache, const char *path, const char *pattern);
char *M_tls_ctx_get_cipherlist(SSL_CTX *ctx);

unsigned char *M_tls_alpn_list(M_list_str_t *apps, size_t *applen);

#endif
