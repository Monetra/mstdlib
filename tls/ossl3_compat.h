#ifndef __OSSL3_COMPAT_H__
#define __OSSL3_COMPAT_H__ 1

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER < 0x3000000fL

EVP_PKEY *EVP_RSA_gen(size_t bits);
int SSL_set0_tmp_dh_pkey(SSL *ssl, EVP_PKEY *dhpkey);

#endif

#endif
