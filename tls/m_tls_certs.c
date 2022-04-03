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

#include "m_config.h"
#include <mstdlib/mstdlib_tls.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "ossl3_compat.h"
#include "base/m_defs_int.h"

struct M_tls_x509 {
	EVP_PKEY *pkey;
	X509     *x509;
};


char *M_tls_rsa_generate_key(size_t bits)
{
	EVP_PKEY *pkey = NULL;
	BIO      *bio  = NULL;
	size_t    buf_size;
	char     *buf  = NULL;

	M_tls_init(M_TLS_INIT_NORMAL);

	pkey = EVP_RSA_gen(bits);
	if (pkey == NULL)
		goto end;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		goto end;

	if (!PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL))
		goto end;

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL)
		goto end;

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto end;
	}

end:
	if (pkey != NULL)
		EVP_PKEY_free(pkey);

	if (bio != NULL)
		BIO_free(bio);

	return buf;
}


static EVP_PKEY *M_tls_rsa_pkey_read(const char *rsa_privkey)
{
	BIO      *bio;
	EVP_PKEY *pkey;

	if (M_str_isempty(rsa_privkey))
		return NULL;
	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, rsa_privkey), -1);
	if (bio == NULL)
		return NULL;

	pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	BIO_free(bio);
	return pkey;
}


static M_bool M_tls_x509_add_ext(X509 *x509, int nid, const char *value)
{
	X509_EXTENSION *ext = NULL;
	X509V3_CTX      ctx;
	M_bool          ret = M_FALSE;

	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, x509, x509, NULL, NULL, 0);

	ext = X509V3_EXT_conf_nid(NULL, &ctx, nid, M_CAST_OFF_CONST(char *, value));
	if (ext == NULL) {
		goto end;
	}

	if (!X509_add_ext(x509, ext, -1)) {
		goto end;
	}

	ret = M_TRUE;

end:
	if (ext != NULL)
		X509_EXTENSION_free(ext);

	return ret;
}


static M_tls_x509_t *M_tls_x509_new_int(void)
{
	M_tls_x509_t   *x509 = NULL;

	x509 = M_malloc_zero(sizeof(*x509));

	x509->x509 = X509_new();
	if (x509->x509 == NULL)
		goto fail;

	if (!X509_set_version(x509->x509, 2))
		goto fail;

	if (!ASN1_INTEGER_set(X509_get_serialNumber(x509->x509), (long)M_rand_range(NULL, 1, M_UINT32_MAX)))
		goto fail;

	/* Set some v3 extensions */
	if (!M_tls_x509_add_ext(x509->x509, NID_subject_key_identifier, "hash"))
		goto fail;

	if (!M_tls_x509_add_ext(x509->x509, NID_authority_key_identifier, "keyid:always"))
		goto fail;

	return x509;

fail:
	M_tls_x509_destroy(x509);
	return NULL;
}


M_tls_x509_t *M_tls_x509_new(const char *rsa_privkey)
{
	M_tls_x509_t *x509;

	M_tls_init(M_TLS_INIT_NORMAL);

	x509       = M_tls_x509_new_int();
	if (x509 == NULL)
		return NULL;

	x509->pkey = M_tls_rsa_pkey_read(rsa_privkey);
	if (x509->pkey == NULL) {
		M_tls_x509_destroy(x509);
		return NULL;
	}

	if (!X509_set_pubkey(x509->x509, x509->pkey)) {
		M_tls_x509_destroy(x509);
		return NULL;
	}

	return x509;
}


static const char *M_tls_x509_txt_name(M_tls_x509_txt_t type)
{
	switch (type) {
		case M_TLS_X509_TXT_COMMONNAME:
			return "CN";
		case M_TLS_X509_TXT_ORGANIZATION:
			return "O";
		case M_TLS_X509_TXT_COUNTRY:
			return "C";
		case M_TLS_X509_TXT_STATE:
			return "ST";
		case M_TLS_X509_TXT_ORGANIZATIONALUNIT:
			return "OU";
		case M_TLS_X509_TXT_LOCALITY:
			return "L";
	}
	return NULL;
}



static M_bool M_tls_x509_txt_add_subjaltname(X509 *x509, M_tls_x509_san_type_t type, const char *text, M_bool append)
{
	STACK_OF(GENERAL_NAME) *SANs    = NULL;
	STACK_OF(GENERAL_NAME) *oldSANs = NULL;
	GENERAL_NAME           *name    = NULL;
	ASN1_IA5STRING         *ia5     = NULL;
	ASN1_OCTET_STRING      *os      = NULL;
	M_bool                  retval  = M_FALSE;
	unsigned long           flags   = X509V3_ADD_REPLACE_EXISTING;
	unsigned char           data[16];
	size_t                  data_len;
	int                     i;


	oldSANs = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
	if (oldSANs == NULL) {
		/* Can't use X509V3_ADD_REPLACE_EXISTING if there is no existing */
		flags = X509V3_ADD_APPEND;
	} else {
		if (!append) {
			/* We're not appending them, so kill the reference to them. */
			sk_GENERAL_NAME_pop_free(oldSANs, GENERAL_NAME_free);
			oldSANs = NULL;
		}
	}

	SANs = sk_GENERAL_NAME_new_null();

	if (SANs == NULL)
		goto end;

	/* Copy existing entries into new SANs structure */
	if (oldSANs != NULL) {
		for (i=0; i<sk_GENERAL_NAME_num(oldSANs); i++) {
			sk_GENERAL_NAME_push(SANs, sk_GENERAL_NAME_value(oldSANs, i));
		}
	}


	name = GENERAL_NAME_new();
	if (name == NULL) {
		goto end;
	}

	switch (type){
		case M_TLS_X509_SAN_TYPE_DNS:
			ia5 = ASN1_IA5STRING_new();
			if (ia5 == NULL)
				goto end;

			if (!ASN1_STRING_set(ia5, text, -1))
				goto end;

			GENERAL_NAME_set0_value(name, GEN_DNS, ia5);
			ia5=NULL; /* Referenced by name */
			break;

		case M_TLS_X509_SAN_TYPE_IP:
			os = ASN1_OCTET_STRING_new();
			if (os == NULL)
				goto end;

			if (!M_io_net_ipaddr_to_bin(data, sizeof(data), text, &data_len))
				goto end;

			if (!ASN1_OCTET_STRING_set(os, data, (int)data_len))
				goto end;

			GENERAL_NAME_set0_value(name, GEN_IPADD, os);
			os=NULL; /* Referenced by name */
			break;
	}

	sk_GENERAL_NAME_push(SANs, name);
	name=NULL; /* Referenced by SANs */

	if (!X509_add1_ext_i2d(x509, NID_subject_alt_name, SANs, 0, flags)) {
		goto end;
	}

	retval = M_TRUE;

end:
	if (ia5 != NULL)
		ASN1_IA5STRING_free(ia5);
	if (os != NULL)
		ASN1_OCTET_STRING_free(os);
	if (name != NULL)
		GENERAL_NAME_free(name);
	if (SANs != NULL)
		sk_GENERAL_NAME_pop_free(SANs, GENERAL_NAME_free);
	if (oldSANs != NULL)
		sk_GENERAL_NAME_free(oldSANs); /* Don't pop_free() since we reference the internal general name */

	return retval;
}


/*! Add a text entry to the certificate of the requested type */
M_bool M_tls_x509_txt_add(M_tls_x509_t *x509, M_tls_x509_txt_t type, const char *text, M_bool append /* vs replace */)
{
	const char *tname;
	X509_NAME  *name;

	if (x509 == NULL || M_str_isempty(text))
		return M_FALSE;

	tname = M_tls_x509_txt_name(type);
	if (tname == NULL)
		return M_FALSE;

	name = X509_get_subject_name(x509->x509);
	if (name == NULL)
		return M_FALSE;

	if (!X509_NAME_add_entry_by_txt(name, tname, MBSTRING_ASC, (const unsigned char *)text, -1, -1, append?1:0))
		return M_FALSE;

	return M_TRUE;
}


M_bool M_tls_x509_txt_SAN_add(M_tls_x509_t *x509, M_tls_x509_san_type_t type, const char *text, M_bool append /* vs replace */)
{
	if (x509 == NULL || text == NULL)
		return M_FALSE;

	return M_tls_x509_txt_add_subjaltname(x509->x509, type, text, append);
}


#if 1
static M_time_t M_tls_asn1time_to_timet(ASN1_TIME *asntime)
{
	M_time_localtm_t tm;
	const char      *str = (const char *)asntime->data;

	M_mem_set(&tm, 0, sizeof(tm));
	/*  asntime->type == V_ASN1_UTCTIME -> 2 digit year YYmmddHHMMSS UTC
	 *  asntime->type == V_ASN1_GENERALIZEDTIME -> 4 digit year YYYYmmddHHMMSS UTC
	 */
	if (M_time_parsefmt(str, (asntime->type == V_ASN1_UTCTIME)?"%y%m%d%H%M%S":"%Y%m%d%H%M%S", &tm) == NULL)
		return 0;

	return M_time_fromgm((M_time_gmtm_t *)&tm);
}

#else
/* Requires OpenSSL 1.0.2 or higher */
static M_time_t M_tls_asn1time_to_timet(ASN1_TIME *tm)
{
	M_time_t   t     = 0;
	ASN1_TIME *epoch = NULL;
	int        days  = 0;
	int        secs  = 0;

	if (tm == NULL)
		goto end;

	epoch = ASN1_TIME_set(NULL, 0);
	if (epoch == NULL)
		goto end;

	if (ASN1_TIME_diff(&days, &secs, epoch, tm) != 1)
		goto end;

	t = (((M_time_t)days) * 86400) + (M_time_t)secs;

end:
	if (epoch != NULL)
		ASN1_TIME_free(epoch);

	return t;
}
#endif


M_time_t M_tls_x509_time_start(M_tls_x509_t *x509)
{
	if (x509 == NULL || x509->x509 == NULL)
		return 0;

	return M_tls_asn1time_to_timet(X509_get_notBefore(x509->x509));
}


M_time_t M_tls_x509_time_end(M_tls_x509_t *x509)
{
	if (x509 == NULL || x509->x509 == NULL)
		return 0;

	return M_tls_asn1time_to_timet(X509_get_notAfter(x509->x509));
}


char *M_tls_x509_subject_name(M_tls_x509_t *x509)
{
	X509_NAME *name;
	char       temp[512];

	if (x509 == NULL || x509->x509 == NULL)
		return NULL;

	name = X509_get_subject_name(x509->x509);
	if (name == NULL)
		return NULL;

	M_mem_set(temp, 0, sizeof(temp));

	X509_NAME_oneline(name, temp, sizeof(temp));

	if (M_str_isempty(temp))
		return NULL;

	return M_strdup(temp);
}


char *M_tls_x509_issuer_name(M_tls_x509_t *x509)
{
	X509_NAME *name;
	char       temp[512];

	if (x509 == NULL || x509->x509 == NULL)
		return NULL;

	name = X509_get_issuer_name(x509->x509);
	if (name == NULL)
		return NULL;

	M_mem_set(temp, 0, sizeof(temp));

	X509_NAME_oneline(name, temp, sizeof(temp));

	if (M_str_isempty(temp))
		return NULL;

	return M_strdup(temp);
}


char *M_tls_x509_signature(M_tls_x509_t *x509, M_tls_x509_sig_alg_t alg)
{
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int  md_size = sizeof(md);
	unsigned int  i;
	unsigned int  cnt;
	char         *out;
	const char   *hexval = "0123456789abcdef";

	if (x509 == NULL || x509->x509 == NULL)
		return NULL;

	if (!X509_digest(x509->x509, (alg == M_TLS_X509_SIG_ALG_SHA1)?EVP_sha1():EVP_sha256(), md, &md_size))
		return NULL;

	out = M_malloc_zero(md_size * 3);
	cnt = 0;

	for (i=0; i<md_size; i++) {
		if (i != 0)
			out[cnt++] = ':';
		out[cnt++] = hexval[md[i] >> 4];
		out[cnt++] = hexval[md[i] & 0x0F];
	}

	return out;
}


/*! Generate a CSR from an x509 certificate */
char *M_tls_x509_write_csr(M_tls_x509_t *x509)
{
	X509_REQ *certreq = NULL;
	BIO      *bio     = NULL;
	size_t    buf_size;
	char     *buf     = NULL;

	if (x509 == NULL || x509->pkey == NULL)
		return NULL;

	certreq = X509_to_X509_REQ(x509->x509, x509->pkey, EVP_sha256());
	if (certreq == NULL)
		return NULL;

	if (X509_REQ_sign(certreq, x509->pkey, EVP_sha256()) <= 0)
		goto end;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		goto end;

	if (!PEM_write_bio_X509_REQ(bio, certreq))
		goto end;

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL)
		goto end;

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto end;
	}

end:
	X509_REQ_free(certreq);
	BIO_free_all(bio);

	return buf;
}


M_tls_x509_t *M_tls_x509_read_crt(const char *crt)
{
	M_tls_x509_t           *x509    = NULL;
	BIO                    *bio     = NULL;
	M_bool                  retval  = M_FALSE;

	M_tls_init(M_TLS_INIT_NORMAL);

	if (M_str_isempty(crt))
		return NULL;

	x509 = M_malloc_zero(sizeof(*x509));

	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, crt), -1);
	if (bio == NULL)
		goto end;

	x509->x509= PEM_read_bio_X509(bio, NULL, NULL, NULL);
	if (x509->x509 == NULL)
		goto end;

	retval = M_TRUE;

end:
	BIO_free(bio);
	if (!retval) {
		if (x509->x509)
			X509_free(x509->x509);
		M_free(x509);
		x509 = NULL;
	}

	return x509;
}


/*! Read a CSR request into an x509 object (really just initializes a new object and copies subject, issuer, subjaltname) */
M_tls_x509_t *M_tls_x509_read_csr(const char *csr)
{
	M_tls_x509_t           *x509    = NULL;
	X509_REQ               *certreq = NULL;
	BIO                    *bio     = NULL;
	M_bool                  retval  = M_FALSE;
	X509_NAME              *name    = NULL;
	EVP_PKEY               *pubkey  = NULL;

	M_tls_init(M_TLS_INIT_NORMAL);

	if (M_str_isempty(csr))
		return NULL;

	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, csr), -1);
	if (bio == NULL)
		goto end;

	certreq = PEM_read_bio_X509_REQ(bio, NULL, NULL, NULL);
	if (certreq == NULL)
		goto end;

	/* Copy public key */
	pubkey = X509_REQ_get_pubkey(certreq);
	if (pubkey == NULL)
		goto end;

	/* verify signature on request */
	if (X509_REQ_verify(certreq, pubkey) != 1)
		goto end;

	x509 = M_tls_x509_new_int();
	if (x509 == NULL)
		goto end;

	/* Copy subject */
	name = X509_REQ_get_subject_name(certreq);
	if (name == NULL)
		goto end;

	if (X509_set_subject_name(x509->x509, name) != 1)
		goto end;

	/* Set the public key */
	if (X509_set_pubkey(x509->x509, pubkey) != 1)
		goto end;

	/* XXX: How do you copy SANs? */

	retval = M_TRUE;

end:
	if (bio != NULL)
		BIO_free(bio);

	if (certreq != NULL)
		X509_REQ_free(certreq);

	if (x509 != NULL && !retval) {
		M_tls_x509_destroy(x509);
		x509 = NULL;
	}

	/* Reference counted */
	if (pubkey != NULL)
		EVP_PKEY_free(pubkey);

	return x509;
}


/*! Self-sign the certificate, returns buffer containing x509 certificate */
char *M_tls_x509_selfsign(M_tls_x509_t *x509, M_uint64 valid_secs)
{
	X509_NAME  *name = NULL;
	BIO        *bio  = NULL;
	size_t      buf_size;
	char       *buf  = NULL;

	if (x509 == NULL || x509->pkey == NULL)
		return NULL;

	/* Set validity period */
	if (X509_gmtime_adj(X509_get_notBefore(x509->x509), 0) == NULL) {
		goto end;
	}

	if (X509_gmtime_adj(X509_get_notAfter(x509->x509), (long)(valid_secs)) == NULL) {
		goto end;
	}

	/* Copy subject to issuer */
	name = X509_get_subject_name(x509->x509);
	if (name == NULL) {
		goto end;
	}

	if (!X509_set_issuer_name(x509->x509, name)) {
		goto end;
	}

	/* Sign it */
	if (!X509_sign(x509->x509, x509->pkey, EVP_sha256())) {
		goto end;
	}

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL) {
		goto end;
	}

	if (!PEM_write_bio_X509(bio, x509->x509)) {
		goto end;
	}

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL) {
		goto end;
	}

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto end;
	}

end:
	if (bio != NULL)
		BIO_free(bio);

	return buf;
}


/*! Sign the certificate, takes CA certificate and private key, copies CA subject
 *  as issuer, returns buffer containing signed x509 certificate */
char *M_tls_x509_sign(M_tls_x509_t *x509, const char *cacert, const char *caprivkey, M_uint64 valid_secs)
{
	X509          *ca        = NULL;
	BIO           *bio       = NULL;
	X509_NAME     *name      = NULL;
	EVP_PKEY      *cakey     = NULL;
	char          *buf       = NULL;
	size_t         buf_size;

	if (x509 == NULL || M_str_isempty(cacert) || M_str_isempty(caprivkey))
		return NULL;

	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, cacert), -1);
	if (bio == NULL)
		goto end;

	ca = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	if (ca == NULL)
		goto end;

	BIO_free(bio);
	bio = NULL;

	bio = BIO_new_mem_buf(M_CAST_OFF_CONST(void *, caprivkey), -1);
	if (bio == NULL)
		goto end;

	cakey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	if (cakey == NULL)
		goto end;

	BIO_free(bio);
	bio = NULL;

	if (X509_check_private_key(ca, cakey) != 1)
		goto end;

	/* Set validity period */
	if (X509_gmtime_adj(X509_get_notBefore(x509->x509), 0) == NULL) {
		goto end;
	}

	if (X509_gmtime_adj(X509_get_notAfter(x509->x509), (long)(valid_secs)) == NULL) {
		goto end;
	}

	/* Copy subject of CA to issuer of cert */
	name = X509_get_subject_name(ca);
	if (name == NULL) {
		goto end;
	}

	if (!X509_set_issuer_name(x509->x509, name)) {
		goto end;
	}

	if (!X509_sign(x509->x509, cakey, EVP_sha256())) {
		goto end;
	}

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL) {
		goto end;
	}

	if (!PEM_write_bio_X509(bio, x509->x509)) {
		goto end;
	}

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL) {
		goto end;
	}

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto end;
	}

end:
	if (bio != NULL)
		BIO_free(bio);
	if (ca != NULL)
		X509_free(ca);
	if (cakey != NULL)
		EVP_PKEY_free(cakey);

	return buf;
}


void M_tls_x509_destroy(M_tls_x509_t *x509)
{
	if (x509 == NULL)
		return;

	if (x509->x509 != NULL)
		X509_free(x509->x509);

	if (x509->pkey != NULL)
		EVP_PKEY_free(x509->pkey);

	M_free(x509);
}


unsigned char *M_tls_dhparam_generate(size_t bits, size_t *out_len)
{
	DH            *dh       = NULL;
	BIO           *bio      = NULL;
	int            dhcodes  = 0;
	size_t         buf_size = 0;
	unsigned char *buf      = NULL;

	M_tls_init(M_TLS_INIT_NORMAL);

	if (out_len == NULL)
		return NULL;

	dh = DH_new();

	if (dh == NULL)
		return NULL;

	if (!DH_generate_parameters_ex(dh, (int)bits, 2 /* 2 or 5 only */, NULL) ||
	    DH_check(dh, &dhcodes) != 1 || 
	    dhcodes != 0
	    ) {
		goto end;
	}

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		goto end;

	if (!PEM_write_bio_DHparams(bio, dh))
		goto end;

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL)
		goto end;

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto end;
	}

	*out_len = buf_size;

end:
	if (dh)
		DH_free(dh);

	if (bio)
		BIO_free(bio);

	return buf;
}
