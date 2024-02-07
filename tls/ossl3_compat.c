#include "m_config.h"
#include <mstdlib/mstdlib_tls.h>
#include "ossl3_compat.h"

#if OPENSSL_VERSION_NUMBER < 0x3000000fL

EVP_PKEY *EVP_RSA_gen(size_t bits)
{
    EVP_PKEY *pkey = NULL;
    RSA      *rsa  = NULL;
    BIGNUM   *bne  = NULL;
    M_bool    rv   = M_FALSE;

    bne = BN_new();
    if (BN_set_word(bne, RSA_F4) != 1)
        goto end;

    pkey = EVP_PKEY_new();
    if (pkey == NULL)
        goto end;

    rsa = RSA_new();
    if (rsa == NULL)
        goto end;

    if (RSA_generate_key_ex(rsa, (int)bits, bne, NULL) != 1)
        goto end;

    if (!EVP_PKEY_set1_RSA(pkey, rsa))
        goto end;

    rv = M_TRUE;

end:
    if (bne != NULL)
        BN_free(bne);

    if (rsa != NULL)
        RSA_free(rsa);

    if (!rv) {
        EVP_PKEY_free(pkey);
        return NULL;
    }

    return pkey;
}

int SSL_set0_tmp_dh_pkey(SSL *ssl, EVP_PKEY *dhpkey)
{
    DH *dhtemp = EVP_PKEY_get0_DH(dhpkey);
    if (ssl == NULL || dhtemp == NULL)
        return 0;

    SSL_set_tmp_dh(ssl, dhtemp);
    /* downref by using free as SSL_set_tmp_dh() duplicates internally */
    EVP_PKEY_free(dhpkey);
    return 1;
}

#endif
