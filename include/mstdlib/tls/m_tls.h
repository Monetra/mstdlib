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

#ifndef __M_TLS_H__
#define __M_TLS_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_tls_funcs TLS functions
 *  \ingroup m_tls
 *
 * TLS functions
 *
 * @{
 */

struct M_tls_clientctx;
typedef struct M_tls_clientctx M_tls_clientctx_t;

struct M_tls_serverctx;
typedef struct M_tls_serverctx M_tls_serverctx_t;


/*! Supported TLS protocols. */
typedef enum {
    M_TLS_PROTOCOL_INVALID     = -1, /*!< Invalid protocol. */
    M_TLS_PROTOCOL_TLSv1_0     = 1 << 0,
    M_TLS_PROTOCOL_TLSv1_1     = 1 << 1,
    M_TLS_PROTOCOL_TLSv1_2     = 1 << 2,
    M_TLS_PROTOCOL_TLSv1_3     = 1 << 3,
    M_TLS_PROTOCOL_DEFAULT     = (M_TLS_PROTOCOL_TLSv1_0 | M_TLS_PROTOCOL_TLSv1_1 | M_TLS_PROTOCOL_TLSv1_2 | M_TLS_PROTOCOL_TLSv1_3) /*!< While not a define passing 0 to a function that takes a protocol will be treated as default. */
} M_tls_protocols_t;


/*! Certificate verification level.
 *
 * Used by client connections to control how they decide to trust
 * the certificate presented by the server.
 *
 */
typedef enum {
    M_TLS_VERIFY_NONE       = 0, /*!< Do not verify the certificate or hostname. */
    M_TLS_VERIFY_CERT_ONLY  = 1, /*!< Only verify the certificate. The domain name is not checked.  */
    M_TLS_VERIFY_CERT_FUZZY = 2, /*!< Verify the certificate and that the base domain name matches.
                                      Use this for servers that don't properly have a wild card cert
                                      but still use a sub domain. */
    M_TLS_VERIFY_FULL       = 3  /*!< Default. Verify the certificate and full domain name matches */
    /* XXX: OCSP, CRL? */
} M_tls_verify_level_t;


/*! How the TLS stack was/is initialized.
 *
 * The TLS system uses OpenSSL as its back ends. It has global initialization
 * and can only be initialized once. Inform the TLS system if it has already
 * been initialized.
 */
typedef enum {
    M_TLS_INIT_NORMAL   = 1, /*!< Fully initialize the TLS (OpenSSL stack) */
    M_TLS_INIT_EXTERNAL = 2  /*!< TLS initialization is handled externally (use with caution) */
} M_tls_init_t;


/*! Initialize the TLS library.
 *
 * If a TLS function is used without calling this function it
 * will be auto initialized using the NORMAL type.
 *
 * \param[in] type Type of initialization.
 */
M_API void M_tls_init(M_tls_init_t type);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Client Support
 */


/*! Create a client TLS context.
 *
 * \return Client context.
 */
M_API M_tls_clientctx_t *M_tls_clientctx_create(void);


/*! Increment reference counters.
 *
 * Intended for APIs that might take ownership.  Can only be
 * Dereferenced via M_tls_clientctx_destroy()
 *
 *\param[in] ctx Client context.
 */
M_API M_bool M_tls_clientctx_upref(M_tls_clientctx_t *ctx);


/*! Destroy a client context.
 *
 * Client CTXs use reference counters, and will delay destruction until after last consumer is destroyed.
 *
 *\param[in] ctx Client context.
 */
M_API void M_tls_clientctx_destroy(M_tls_clientctx_t *ctx);


/*! Set the TLS protocols that the context should use.
 *
 * \param[in] ctx       Client context.
 * \param[in] protocols M_tls_protocols_t bitmap of TLS protocols that should be supported.
 *                      Protocols are treated as min and max. For example if TLSv1.0 and
 *                      TLSv1.2 are enabled, then TLSv1.1 will be enabled even if not
 *                      explicitly set.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_protocols(M_tls_clientctx_t *ctx, int protocols);


/*! Set the ciphers that the context should support.
 *
 * A default list of secure ciphers is used if it is not explicitly changed
 * by this function.
 *
 * \param[in] ctx     Client context.
 * \param[in] ciphers OpenSSL cipher string.
 *
 * \see M_tls_clientctx_get_cipherlist
 */
M_API M_bool M_tls_clientctx_set_ciphers(M_tls_clientctx_t *ctx, const char *ciphers);


/* Set a certificate that will be sent to the server for the server to validate.
 *
 * Keys must be RSA or DSA.
 * Certificates must be X509 and can be PEM, or DER encoded.
 *
 * The intermediate file can have multiple certificates comprising a certificate chain.
 * Certificates in the chain should be ordered by signing with the certificate that signed
 * the client certificate on top. The root CA file should not be in this chain.
 *
 * Intermediate example:
 *
 *     crt (not part of intermediate file)
 *     Intermediate 1 that signed crt
 *     Intermediate 2 that signed intermediate 1
 *     Intermediate 3 that signed intermediate 2
 *     CA that signed intermediate 3 (not part of intermediate file).
 *
 * \param[in] ctx              Client context.
 * \param[in] key              Private key associated with certificate.
 * \param[in] key_len          Length of private key.
 * \param[in] crt              Certificate.
 * \param[in] crt_len          Length of certificate.
 * \param[in] intermediate     Intermediate certificate chain.
 * \param[in] intermediate_len Length of intermediate certificate chain.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_clientctx_set_cert_files
 */
M_API M_bool M_tls_clientctx_set_cert(M_tls_clientctx_t *ctx, const unsigned char *key, size_t key_len, const unsigned char *crt, size_t crt_len, const unsigned char *intermediate, size_t intermediate_len);


/* Set a certificate that will be sent to the server for the server to validate from a file.
 *
 * Requirements are the same as M_tls_clientctx_set_cert
 *
 * \param[in] ctx              Client context.
 * \param[in] keypath          Path to key file.
 * \param[in] crtpath          Path to certificate file.
 * \param[in] intermediatepath Path to intermediate certificate file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_clientctx_set_cert
 */
M_API M_bool M_tls_clientctx_set_cert_files(M_tls_clientctx_t *ctx, const char *keypath, const char *crtpath, const char *intermediatepath);


/*! Load the OS CA trust list for validating the certificate presented by the server.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx Client context.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_default_trust(M_tls_clientctx_t *ctx);


/*! Load a CA certificate for validating the certificate presented by the server.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx Client context.
 * \param[in] ca  CA data.
 * \param[in] len CA length.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_trust_ca(M_tls_clientctx_t *ctx, const unsigned char *ca, size_t len);


/*! Load a CA certificate from a file for validating the certificate presented by the server.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx  Client context.
 * \param[in] path
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_trust_ca_file(M_tls_clientctx_t *ctx, const char *path);


/*! Load CA certificates found in a directory for validating the certificate presented by the server.
 *
 * Files must be PEM encoded and use the ".pem" extension.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx  Client context.
 * \param[in] path Path to CA file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_clientctx_set_trust_ca
 */
M_API M_bool M_tls_clientctx_set_trust_ca_dir(M_tls_clientctx_t *ctx, const char *path);


/*! Load a certificate for validation of the certificate presented by the server.
 *
 * This is for loading intermediate certificate used as part of the trust chain.
 *
 * This will not clear existing certificates that were already loaded.
 *
 * \param[in] ctx Client context.
 * \param[in] crt Certificate.
 * \param[in] len Certificate length.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_trust_cert(M_tls_clientctx_t *ctx, const unsigned char *crt, size_t len);


/*! Load a certificate from a file for validation of the certificate presented by the server.
 *
 * This is for loading intermediate certificate used as part of the trust chain.
 *
 * This will not clear existing certificates that were already loaded.
 *
 * \param[in] ctx  Client context.
 * \param[in] path Path to certificate file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_clientctx_set_trust_cert
 */
M_API M_bool M_tls_clientctx_set_trust_cert_file(M_tls_clientctx_t *ctx, const char *path);


/* Set how the server certificate should be verified.
 *
 * \param[in] ctx   Client context.
 * \param[in] level Verification level.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_verify_level(M_tls_clientctx_t *ctx, M_tls_verify_level_t level);


/*! Enable or disable session resumption.
 *
 * Session resumption is enabled by default.
 *
 * \param[in] ctx    Client context.
 * \param[in] enable M_TRUE to enable. M_FALSE to disable.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_session_resumption(M_tls_clientctx_t *ctx, M_bool enable);


/*! Retrieves a colon separated list of ciphers that are enabled.
 *
 * \param[in] ctx Client context.
 *
 * \return String.
 */
M_API char *M_tls_clientctx_get_cipherlist(M_tls_clientctx_t *ctx);


/*! Set ALPN supported applications.
 *
 * \param[in] ctx          Client context.
 * \param[in] applications List of supported applications.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_applications(M_tls_clientctx_t *ctx, M_list_str_t *applications);


/*! Set the negotiation timeout.
 *
 * How long the client should wait to establish a connection.
 *
 * \param[in] ctx        Client context.
 * \param[in] timeout_ms Time in milliseconds.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_clientctx_set_negotiation_timeout_ms(M_tls_clientctx_t *ctx, M_uint64 timeout_ms);


/*! Wrap existing IO channel with TLS.
 *
 * \param[in]  io       io object.
 * \param[in]  ctx      Client context.
 * \param[in]  hostname Hostname is optional if wrapping an outbound network connection where
 *                      it can be retrieved from the lower layer
 * \param[out] layer_id Layer id this is added at.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_tls_client_add(M_io_t *io, M_tls_clientctx_t *ctx, const char *hostname, size_t *layer_id);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ServerSupport
 */


/*! Create a server TLS context.
 *
 * \param[in] key              Private key associated with certificate.
 * \param[in] key_len          Length of private key.
 * \param[in] crt              Certificate.
 * \param[in] crt_len          Length of certificate.
 * \param[in] intermediate     Intermediate certificate chain.  Can be NULL.
 * \param[in] intermediate_len Length of intermediate certificate chain.
 *
 * \return Server context.
 */
M_API M_tls_serverctx_t *M_tls_serverctx_create(const unsigned char *key, size_t key_len, const unsigned char *crt, size_t crt_len, const unsigned char *intermediate, size_t intermediate_len);


/*! Create a server TLS context from files.
 *
 * \param[in] keypath          Path to key file.
 * \param[in] crtpath          Path to certificate file.
 * \param[in] intermediatepath Path to intermediate certificate file.  Can be NULL.
 *
 * \return Server context.
 */
M_API M_tls_serverctx_t *M_tls_serverctx_create_from_files(const char *keypath, const char *crtpath, const char *intermediatepath);

/*! Increment reference counters.
 *
 * Intended for APIs that might take ownership.  Can only be
 * Dereferenced via M_tls_serverctx_destroy()
 *
 * \param[in] ctx Server context.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_upref(M_tls_serverctx_t *ctx);


/*! Destroy a server context.
 *
 * Server CTXs use reference counters, and will delay destruction until after last consumer is destroyed.
 *
 * \param[in] ctx Server context.
 */
M_API void M_tls_serverctx_destroy(M_tls_serverctx_t *ctx);


/*! Add a sub context under this one to allow multiple certificates to be used with SNI.
 *
 * For SNI support, if a certificate does not list a subject alt name, a server context
 * needs to be created for each certificate. The certificate to be used as the
 * default when the client does not support SNI will be the parent context. All
 * of the additional contexts are added to this one.
 *
 * This is not necessary if a certificate lists all expected host names as subject alt names.
 *
 * \param[in] ctx   Server context.
 * \param[in] child Child server context.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_SNI_ctx_add(M_tls_serverctx_t *ctx, M_tls_serverctx_t *child);


/*! Number of child contexts associated with this server context used for SNI.
 *
 * \param[in] ctx Server context.
 *
 * \return Count.
 *
 * \see M_tls_serverctx_SNI_at
 */
M_API size_t M_tls_serverctx_SNI_count(M_tls_serverctx_t *ctx);


/*! Get a child SNI context from a context based on host name.
 *
 * \param[in] ctx      Server context.
 * \param[in] hostname Host name to look for.
 *
 * \return Server context on success, otherwise NULL if not found.
 */
M_API M_tls_serverctx_t *M_tls_serverctx_SNI_lookup(M_tls_serverctx_t *ctx, const char *hostname);


/*! Get a child SNI context from a context at a given index.
 *
 * \param[in] ctx Server context.
 * \param[in] idx Index.
 *
 * \return Server context on success, otherwise NULL on error.
 *
 * \see M_tls_serverctx_SNI_count
 */
M_API M_tls_serverctx_t *M_tls_serverctx_SNI_at(M_tls_serverctx_t *ctx, size_t idx);


/* Get the X509 certificate for a server context.
 *
 * \param[in] ctx Server context.
 *
 * \return PEM encoded certificate
 */
M_API char *M_tls_serverctx_get_cert(M_tls_serverctx_t *ctx);


/*! Set the TLS protocols that the context should use.
 *
 * \param[in] ctx       Server context.
 * \param[in] protocols M_tls_protocols_t bitmap of TLS protocols that should be supported.
 *                      Protocols are treated as min and max. For example if TLSv1.0 and
 *                      TLSv1.2 are enabled, then TLSv1.1 will be enabled even if not
 *                      explicitly set.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_protocols(M_tls_serverctx_t *ctx, int protocols);


/*! Set the ciphers that the context should support.
 *
 * A default list of secure ciphers is used if it is not explicitly changed
 * by this function.
 *
 * \param[in] ctx     Server context.
 * \param[in] ciphers OpenSSL cipher string.
 *
 * \see M_tls_clientctx_get_cipherlist
 */
M_API M_bool M_tls_serverctx_set_ciphers(M_tls_serverctx_t *ctx, const char *ciphers);


/*! Set the server to prefer its own cipher order rather than the client.
 *
 * By default, the client cipher order is preferred, this is recommended as a
 * client may be a mobile device where a cipher like TLS_CHACHA20_POLY1305_SHA256
 * is more efficient than TLS_AES_256_GCM_SHA384 and will provide a better
 * customer experience.  However a desktop client may prefer TLS_AES_256_GCM_SHA384
 * as it supports AES-NI instruction helpers or similar.  Since the server is
 * often more powerful than the client, it is better suited to the additional
 * compute.
 *
 * Assuming the server is configured to only allow strong ciphers, there should
 * be no security risk in allowing the client to decide the most efficient.
 *
 * \param[in] ctx     Server context.
 * \param[in] tf      M_TRUE to enable server preference, M_FALSE to disable.
 *
 * \see M_tls_clientctx_get_cipherlist
 */
M_API M_bool M_tls_serverctx_set_server_preference(M_tls_serverctx_t *ctx, M_bool tf);



/*! Load a CA certificate for validating the certificate presented by the client.
 *
 * If set the client will be required to present a certificate.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx Server context.
 * \param[in] ca  CA data.
 * \param[in] len CA length.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_trust_ca(M_tls_serverctx_t *ctx, const unsigned char *ca, size_t len);


/*! Load a CA certificate from a file for validating the certificate presented by the client.
 *
 * This will not clear existing CAs that were already loaded.
 *
 * \param[in] ctx  Server context.
 * \param[in] path Path to CA file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_serverctx_set_trust_ca
 */
M_API M_bool M_tls_serverctx_set_trust_ca_file(M_tls_serverctx_t *ctx, const char *path);


/*! Load a certificate for validation of the certificate presented by the client.
 *
 * This is for loading intermediate certificate used as part of the trust chain.
 *
 * This will not clear existing certificates that were already loaded.
 *
 * \param[in] ctx  Server context.
 * \param[in] path Path to CA directory.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_serverctx_set_trust_ca
 */
M_API M_bool M_tls_serverctx_set_trust_ca_dir(M_tls_serverctx_t *ctx, const char *path);


/*! Load a certificate for validation of the certificate presented by the client.
 *
 * This is for loading intermediate certificate used as part of the trust chain.
 *
 * This will not clear existing certificates that were already loaded.
 *
 * \param[in] ctx Server context.
 * \param[in] crt Certificate.
 * \param[in] len Certificate length.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_trust_cert(M_tls_serverctx_t *ctx, const unsigned char *crt, size_t len);


/*! Load a certificate from a file for validation of the certificate presented by the client.
 *
 * This is for loading intermediate certificate used as part of the trust chain.
 *
 * This will not clear existing certificates that were already loaded.
 *
 * \param[in] ctx  Server context.
 * \param[in] path Path to certificate file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_serverctx_set_trust_cert
 */
M_API M_bool M_tls_serverctx_set_trust_cert_file(M_tls_serverctx_t *ctx, const char *path);


/*! Load a certificate revocation list to validate the certificate presented by the client.
 *
 * This will not clear existing revocations already loaded.
 *
 * \param[in] ctx Server context.
 * \param[in] crl CRL.
 * \param[in] len CRL Length.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_add_trust_crl(M_tls_serverctx_t *ctx, const unsigned char *crl, size_t len);


/*! Load a certificate revocation from a file list to validate the certificate presented by the client.
 *
 * This will not clear existing revocations already loaded.
 *
 * \param[in] ctx  Server context.
 * \param[in] path Path to certificate revocation list file.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_serverctx_set_trust_cert_file
 */
M_API M_bool M_tls_serverctx_add_trust_crl_file(M_tls_serverctx_t *ctx, const char *path);


/*! Set the dhparam for the context.
 *
 * If not set, uses internal 2236 dhparam.
 * DHparam data must be PEM-encoded.
 *
 * \param[in] ctx         Server context.
 * \param[in] dhparam     DHparam data. If dhparam is NULL, disables the use of DHE negotiation.
 * \param[in] dhparam_len Length of data.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_dhparam(M_tls_serverctx_t *ctx, const unsigned char *dhparam, size_t dhparam_len);


/*! Set the dhparam for the context from a file.
 *
 * If not set, uses internal 2236 dhparam.
 * DHparam data must be PEM-encoded.
 *
 * \param[in] ctx          Server context.
 * \param[in] dhparam_path Path to DHparam data.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *
 * \see M_tls_serverctx_set_dhparam
 */
M_API M_bool M_tls_serverctx_set_dhparam_file(M_tls_serverctx_t *ctx, const char *dhparam_path);


/*! Enable or disable session resumption.
 *
 * Session resumption is enabled by default.
 *
 * \param[in] ctx    Server context.
 * \param[in] enable M_TRUE to enable. M_FALSE to disable.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_session_resumption(M_tls_serverctx_t *ctx, M_bool enable);


/*! Retrieves a colon separated list of ciphers that are enabled.
 *
 * \param[in] ctx Server context.
 *
 * \return String.
 */
M_API char *M_tls_serverctx_get_cipherlist(M_tls_serverctx_t *ctx);

/*! Set ALPN supported applications.
 *
 * \param[in] ctx          Server context.
 * \param[in] applications List of supported applications.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_applications(M_tls_serverctx_t *ctx, M_list_str_t *applications);


/*! Set the negotiation timeout.
 *
 * How long the server should wait to establish a connection.
 *
 * \param[in] ctx        Server context.
 * \param[in] timeout_ms Time in milliseconds.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_tls_serverctx_set_negotiation_timeout_ms(M_tls_serverctx_t *ctx, M_uint64 timeout_ms);

/*! Wrap existing IO channel with TLS.
 *
 * \param[in]  io       io object.
 * \param[in]  ctx      Server context.
 * \param[out] layer_id Layer id this is added at.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_tls_server_add(M_io_t *io, M_tls_serverctx_t *ctx, size_t *layer_id);


/*! Get the host name the connected client requested.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return String.
 */
M_API const char *M_tls_server_get_hostname(M_io_t *io, size_t id);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Common to both server and client
 */


/*! Get the protocol the connection was establish with.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return Protocol.
 */
M_API M_tls_protocols_t M_tls_get_protocol(M_io_t *io, size_t id);


/*! Was the session for this connection reused from a previous connection?
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return M_TRUE if reused, otherwise M_FALSE.
 */
M_API M_bool M_tls_get_sessionreused(M_io_t *io, size_t id);


/*! Get the cipher negotiated.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return String.
 */
M_API const char *M_tls_get_cipher(M_io_t *io, size_t id);


/*! Get the application negotiated.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return NULL if there is no ALPN support or on error, or will return the application.
 */
M_API char *M_tls_get_application(M_io_t *io, size_t id);


/*! Get the certificate presented by the other end.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return X509 PEM encoded certificate.
 */
M_API char *M_tls_get_peer_cert(M_io_t *io, size_t id);


/*! How long negotiated took.
 *
 * \param[in] io io object.
 * \param[in] id Layer id.
 *
 * \return Negotiation time (success or fail) in ms */
M_API M_uint64 M_tls_get_negotiation_time_ms(M_io_t *io, size_t id);


/*! Convert a protocol to string.
 *
 * Only single protocol should be specified. If multiple are provided
 * it is undefined which will be returned. Used primarily for logging to
 * print what protocol a connection is using.
 *
 * \param[in] protocol
 *
 * \return String.
 */
M_API const char *M_tls_protocols_to_str(M_tls_protocols_t protocol);


/*! Convert a string to protocols bitmap
 *
 * The value for this field is a space separated list of protocols. Valid
 * protocols are: tlsv1, tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.3.
 *
 * Entry tlsv1 implies all tls 1.y protocols.
 *
 * If the protocol is appended with a plus (+) sign, then it means that protocol
 * version or higher, for instance, "tlsv1.1+" implies "tlsv1.1 tlsv1.2 tlsv1.3"
 *
 * Protocols are treated as min and max. Enabling protocols with
 * version gaps will result in the gaps being enabled. E.g. specifying
 * "tlsv1.0 tlsv1.2" will enable tlsv1.0, _tlsv1.1_, and tlsv1.2.
 *
 * Unknown entries will be ignored. Protocols that are not supported
 * by the backend will be removed from the list of returned protocols.
 *
 * \param[in] protocols_str String of protocols
 *
 * \return Protocol bitmap. M_TLS_PROTOCOL_INVALID on error.
 */
M_API M_tls_protocols_t M_tls_protocols_from_str(const char *protocols_str);

/*! @} */

__END_DECLS

#endif /* __M_TLS_H__ */
