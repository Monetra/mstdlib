/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

#ifndef __M_TLS_CERTS__
#define __M_TLS_CERTS__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_tls_certs TLS Certificates
 *  \ingroup m_tls
 * 
 * Basic TLS certificate functionality. This is primary used
 * to aid in generating self signed certificates programmatically.
 *
 * \warning
 * These functions are experimental and unstable. They should not
 * be used in production at this time.
 *
 * Example generating a CA, server certificate and signing the
 * certificate with the CA.
 *
 * \code{.c}
 *     #include <mstdlib/mstdlib.h>
 *     #include <mstdlib/mstdlib_tls.h>
 *     
 *     #define VALID_SEC (5*365*24*60*60)
 *     
 *     int main(int argc, char **argv)
 *     {
 *         char         *CA_privkey;
 *         M_tls_x509_t *CA_x509;
 *         char         *CA_crt;
 *         char         *Server_privkey;
 *         M_tls_x509_t *Server_x509;
 *         M_tls_x509_t *Server_csr_x509;
 *         char         *Server_crt;
 *         char         *Server_csr;
 *     
 *         // Generate our certificate authority. 
 *         CA_privkey = M_tls_rsa_generate_key(2048);
 *         CA_x509    = M_tls_x509_new(CA_privkey);
 *         M_tls_x509_txt_add(CA_x509, M_TLS_X509_TXT_COMMONNAME, "MY CA", M_FALSE);
 *         M_tls_x509_txt_add(CA_x509, M_TLS_X509_TXT_ORGANIZATION, "MY ORG", M_FALSE);
 *         M_tls_x509_txt_SAN_add(CA_x509, M_TLS_X509_SAN_TYPE_DNS, "ca.myorg.local", M_FALSE);
 *         CA_crt = M_tls_x509_selfsign(CA_x509, VALID_SEC);
 *     
 *         // Generate the server x509
 *         Server_privkey = M_tls_rsa_generate_key(2048);
 *         Server_x509    = M_tls_x509_new(Server_privkey);
 *         M_tls_x509_txt_add(Server_x509, M_TLS_X509_TXT_COMMONNAME, "MY Server", M_FALSE);
 *         M_tls_x509_txt_add(Server_x509, M_TLS_X509_TXT_ORGANIZATION, "MY ORG", M_FALSE);
 *         M_tls_x509_txt_SAN_add(Server_x509, M_TLS_X509_SAN_TYPE_DNS, "server.myorg.local", M_FALSE);
 *     
 *         // Generate a server CSR from the server x509.
 *         // Sign the CSR creating a server certificate.
 *         Server_csr      = M_tls_x509_write_csr(Server_x509);
 *         Server_csr_x509 = M_tls_x509_read_csr(Server_csr);
 *         Server_crt      = M_tls_x509_sign(Server_csr_x509, CA_crt, CA_privkey, VALID_SEC);
 *     
 *         M_printf("CA Priv Key:\n%s\n", CA_privkey);
 *         M_printf("CA CRT:\n%s\n", CA_crt);
 *         M_printf("Server Priv Key:\n%s\n", Server_privkey);
 *         M_printf("Server CSR:\n%s\n", Server_csr);
 *         M_printf("Server CRT:\n%s\n", Server_crt);
 *     
 *         M_free(Server_csr);
 *         M_free(Server_crt);
 *         M_free(Server_privkey);
 *         M_free(CA_crt);
 *         M_free(CA_privkey);
 *         M_tls_x509_destroy(CA_x509);
 *         M_tls_x509_destroy(Server_csr_x509);
 *         M_tls_x509_destroy(Server_x509);
 *     }
 * \endcode
 * @{
 */

struct M_tls_x509;
typedef struct M_tls_x509 M_tls_x509_t;


/*! X509 certificate text attributes. */
typedef enum {
	M_TLS_X509_TXT_COMMONNAME         = 1, /*!< (CN) Name of certificate. */
	M_TLS_X509_TXT_ORGANIZATION       = 2, /*!< (O) Organization owning certificate. */
	M_TLS_X509_TXT_COUNTRY            = 3, /*!< (C) County where the organization is located. */
	M_TLS_X509_TXT_STATE              = 4, /*!< (S) State or providence where the organization is located. */
	M_TLS_X509_TXT_ORGANIZATIONALUNIT = 5, /*!< (OU) Group within the organization owning the certificate. */
	M_TLS_X509_TXT_LOCALITY           = 6  /*!< (L) State, township, county, etc. where the organizational unit
	                                            is located. */
} M_tls_x509_txt_t;


/*! Certificate hash algorithm. */
typedef enum {
	M_TLS_X509_SIG_ALG_SHA1   = 1, /*!< SHA 1. */
	M_TLS_X509_SIG_ALG_SHA256 = 2  /*!< SHA 256. */
} M_tls_x509_sig_alg_t;


/*! Type of subject alternative name. */
enum M_tls_x509_san_type {
	M_TLS_X509_SAN_TYPE_DNS = 1, /*!< Name is a host name that can be retrieved by DNS. */
	M_TLS_X509_SAN_TYPE_IP  = 2  /*!< Name is an ip address. */
};
typedef enum M_tls_x509_san_type M_tls_x509_san_type_t;


/*! Generate an RSA private key
 *
 * \param[in] bits Bit size of the key.
 *
 * \return Buffer containing private key
 */
M_API char *M_tls_rsa_generate_key(size_t bits);

/*! Create a new x509 certificate.
 *
 * \param[in] rsa_privkey RSA private key.
 *
 * \return X509 certificate.
 *
 * \see M_tls_rsa_generate_key
 */
M_API M_tls_x509_t *M_tls_x509_new(const char *rsa_privkey);


/*! Destroy an x509 certificate.
 *
 * \param[in] x509 Certificate.
 */
M_API void M_tls_x509_destroy(M_tls_x509_t *x509);


/*! Add a text entry to the certificate of the requested type.
 *
 * \param[in] x509   Certificate.
 * \param[in] type   Type of attribute.
 * \param[in] text   Text to put in attribute.
 * \param[in] append M_TRUE to append. M_FALSE to replace if the attribute already exists.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_x509_txt_add(M_tls_x509_t *x509, M_tls_x509_txt_t type, const char *text, M_bool append);


/*! Add subject alternative name to a certificate.
 *
 * \param[in] x509   Certificate.
 * \param[in] type   Type of attribute.
 * \param[in] text   Text to put in attribute.
 * \param[in] append M_TRUE to append. M_FALSE to replace if the attribute already exists.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_x509_txt_SAN_add(M_tls_x509_t *x509, M_tls_x509_san_type_t type, const char *text, M_bool append);


/*! Generate a CSR from an x509 certificate.
 *
 * \param[in] x509 Certificate.
 *
 * \return String on success, otherwise NULL on error.
 */
M_API char *M_tls_x509_write_csr(M_tls_x509_t *x509);


/*! Read a CSR request.
 *
 * \param[in] csr CSR requested.
 *
 * \return x509 certificate on success, otherwise NULL on error.
 */
M_API M_tls_x509_t *M_tls_x509_read_csr(const char *csr);


/*! Read a PEM-encoded certificate.
 *
 * \param[in] crt Certificate.
 *
 * \return x509 certificate on success, otherwise NULL on error.
 */
M_API M_tls_x509_t *M_tls_x509_read_crt(const char *crt);


/*! Self-sign the certificate.
 *
 * Signs using SHA 256 algorithm.
 *
 * \param[in] x509       Certificate.
 * \param[in] valid_secs The validity period for the certificate in seconds.
 *
 * \return Buffer containing x509 certificate.
 */
M_API char *M_tls_x509_selfsign(M_tls_x509_t *x509, M_uint64 valid_secs);


/*! Sign the certificate
 *
 * Signs using SHA 256 algorithm.
 *
 * \param[in] x509       Certificate.
 * \param[in] cacert     CA certificate to use for signing.
 * \param[in] caprivkey  CA certificate private key.
 * \param[in] valid_secs The validity period for the certificate in seconds.
 *
 * \return Buffer containing signed x509 certificate.
 */
M_API char *M_tls_x509_sign(M_tls_x509_t *x509, const char *cacert, const char *caprivkey, M_uint64 valid_secs);


/*! Get the start time (not before) of a certificate.
 *
 * \param[in] x509 Certificate.
 *
 * \return Time.
 */
M_API M_time_t M_tls_x509_time_start(M_tls_x509_t *x509);


/*! Get the end time (not after) of a certificate.
 *
 * \param[in] x509 Certificate.
 *
 * \return Time.
 */
M_API M_time_t M_tls_x509_time_end(M_tls_x509_t *x509);


/*! Get the subject name of a certificate.
 *
 * \param[in] x509 Certificate.
 *
 * \return String.
 */
M_API char *M_tls_x509_subject_name(M_tls_x509_t *x509);


/*! Get the issuer name of a certificate.
 *
 * \param[in] x509 Certificate.
 *
 * \return String.
 */
M_API char *M_tls_x509_issuer_name(M_tls_x509_t *x509);

/*! Retrieves the signature/digest of the x509 certificate.
 *
 * Useful for matching clients to certificates
 *
 * \param[in] x509 Certificate.
 * \param[in] alg  Algorithm to use for signature calculation.
 *
 * \return String.
 */
M_API char *M_tls_x509_signature(M_tls_x509_t *x509, M_tls_x509_sig_alg_t alg);


/*! Generate DH parameters.
 *
 * Could take a very long time, should probably occur
 * in its own thread to not block program execution.
 *
 * \param[in]  bits    Bit size of the parameters.
 * \param[out] out_len Length of the output.
 *
 * \return dhparams.
 */
M_API unsigned char *M_tls_dhparam_generate(size_t bits, size_t *out_len);

/*! @} */

__END_DECLS

#endif
