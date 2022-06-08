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

#include "m_net_smtp_int.h"
#include <openssl/hmac.h>
#include <openssl/rand.h>

typedef enum {
	STATE_AUTH_START = 1,
	STATE_AUTH_PLAIN,
	STATE_AUTH_PLAIN_RESPONSE,
	STATE_AUTH_LOGIN,
	STATE_AUTH_LOGIN_USERNAME,
	STATE_AUTH_LOGIN_PASSWORD,
	STATE_AUTH_LOGIN_RESPONSE,
	STATE_AUTH_CRAM_MD5,
	STATE_AUTH_CRAM_MD5_SECRET_RESPONSE,
	STATE_AUTH_CRAM_MD5_FINAL_RESPONSE,
	STATE_AUTH_DIGEST_MD5,
	STATE_AUTH_DIGEST_MD5_NONCE_RESPONSE,
	STATE_AUTH_DIGEST_MD5_ACK_RESPONSE,
	STATE_AUTH_DIGEST_MD5_FINAL_RESPONSE,
} m_state_ids;

static M_state_machine_status_t M_state_auth_start(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	switch (session->tcp.smtp_authtype) {
		case M_NET_SMTP_AUTHTYPE_PLAIN:
			*next = STATE_AUTH_PLAIN;
			return M_STATE_MACHINE_STATUS_NEXT;
		case M_NET_SMTP_AUTHTYPE_LOGIN:
			*next = STATE_AUTH_LOGIN;
			return M_STATE_MACHINE_STATUS_NEXT;
		case M_NET_SMTP_AUTHTYPE_CRAM_MD5:
			*next = STATE_AUTH_CRAM_MD5;
			return M_STATE_MACHINE_STATUS_NEXT;
		case M_NET_SMTP_AUTHTYPE_DIGEST_MD5:
			*next = STATE_AUTH_DIGEST_MD5;
			return M_STATE_MACHINE_STATUS_NEXT;
		case M_NET_SMTP_AUTHTYPE_NONE:
			return M_STATE_MACHINE_STATUS_DONE;
	}

	/* Classify as connect failure so endpoint can get removed */
	session->tcp.is_connect_fail = M_TRUE;
	session->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
	M_snprintf(session->errmsg, sizeof(session->errmsg), "Unsupported SMTP authentication type: %d", session->tcp.smtp_authtype);
	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static char * create_auth_plain(const char *username, const char *password)
{
	char    *auth_str_base64 = NULL;
	size_t   len             = 0;
	M_buf_t *buf             = NULL;
	char    *str             = NULL;

	buf = M_buf_create();
	M_buf_add_byte(buf, 0);
	M_buf_add_str(buf, username);
	M_buf_add_byte(buf, 0);
	M_buf_add_str(buf, password);
	str = M_buf_finish_str(buf, &len);

	auth_str_base64 = M_bincodec_encode_alloc((const unsigned char *)str, len, 0, M_BINCODEC_BASE64);
	M_free(str);
	return auth_str_base64;
}

static M_state_machine_status_t M_state_auth_plain(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session    = data;
	char                 *auth_plain = NULL;

	auth_plain = create_auth_plain(session->ep->tcp.username, session->ep->tcp.password);
	M_buf_add_str(session->out_buf, "AUTH PLAIN ");
	M_buf_add_str(session->out_buf, auth_plain);
	M_buf_add_str(session->out_buf, "\r\n");
	M_free(auth_plain);
	*next = STATE_AUTH_PLAIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

/* Same for PLAIN and CRAM-MD5 */
static M_state_machine_status_t M_auth_final_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_session_t     *session        = data;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 235))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	return M_STATE_MACHINE_STATUS_DONE;
}

static M_state_machine_status_t M_state_auth_login(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	M_buf_add_str(session->out_buf, "AUTH LOGIN\r\n");
	session->tcp.auth_login_response_count = 0;
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_username(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session      = data;
	char                 *username_b64 = NULL;

	username_b64 = M_bincodec_encode_alloc(
		(const unsigned char *)session->ep->tcp.username,
		M_str_len(session->ep->tcp.username),
		0,
		M_BINCODEC_BASE64
	);

	M_buf_add_str(session->out_buf, username_b64);
	M_buf_add_str(session->out_buf, "\r\n");
	M_free(username_b64);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_password(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session      = data;
	char                 *password_b64 = NULL;

	password_b64 = M_bincodec_encode_alloc(
		(const unsigned char *)session->ep->tcp.password,
		M_str_len(session->ep->tcp.password),
		0,
		M_BINCODEC_BASE64
	);

	M_buf_add_str(session->out_buf, password_b64);
	M_buf_add_str(session->out_buf, "\r\n");
	M_free(password_b64);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_login_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_session_t     *session        = data;
	const char               *line           = NULL;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	line = M_list_str_last(session->tcp.smtp_response);

	if (session->tcp.auth_login_response_count < 3 && !M_net_smtp_flow_tcp_check_smtp_response_code(session, 334))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (session->tcp.auth_login_response_count == 3 && !M_net_smtp_flow_tcp_check_smtp_response_code(session, 235))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (session->tcp.auth_login_response_count == 3)
		return M_STATE_MACHINE_STATUS_DONE;

	if (M_str_caseeq_max(line, "VXNlcm5hbWU6", 12)) {
		/* base64 for "Username" */
		*next = STATE_AUTH_LOGIN_USERNAME;
		return M_STATE_MACHINE_STATUS_NEXT;
	}

	if (M_str_caseeq_max(line, "UGFzc3dvcmQ6", 12)) {
		/* base64 for "Password" */
		*next = STATE_AUTH_LOGIN_PASSWORD;
		return M_STATE_MACHINE_STATUS_NEXT;
	}

	M_snprintf(session->errmsg, sizeof(session->errmsg), "Unknown auth-login request: %s", line);
	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static M_state_machine_status_t M_state_auth_cram_md5(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	M_buf_add_str(session->out_buf, "AUTH CRAM-MD5\r\n");
	*next = STATE_AUTH_CRAM_MD5_SECRET_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_cram_md5_secret_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_session_t     *session        = data;
	size_t                    len            = 0;
	unsigned int              uint           = 0;
	unsigned char             buf[512]       = { 0 };
	char                     *challenge      = NULL;
	const char               *line;
	unsigned char             d[16]; /* digest */

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 334))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	line = M_list_str_last(session->tcp.smtp_response);
	if (M_bincodec_decode(buf, sizeof(buf)-1, line, M_str_len(line), M_BINCODEC_BASE64) <= 0) {
		session->tcp.is_connect_fail = M_TRUE;
		session->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Failed to decode cram-md5 secret: %s", line);
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	uint = sizeof(d);
	HMAC(
		EVP_md5(),
		session->ep->tcp.password, (int)M_str_len(session->ep->tcp.password),
		buf, M_str_len((const char *)buf), /* buf contains cram-md5 secret */
		d, &uint
	);

	len = M_snprintf(
		(char *)buf, sizeof(buf),
		"%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		session->ep->tcp.username,
		d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]
	);

	if (!(challenge = M_bincodec_encode_alloc(buf, len, 0, M_BINCODEC_BASE64))) {
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Allocation failed");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	M_buf_add_str(session->out_buf, challenge);
	M_buf_add_str(session->out_buf, "\r\n");
	M_free(challenge);

	*next = STATE_AUTH_CRAM_MD5_FINAL_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static void RFC2831_HEX(unsigned char b[16], char s[33])
{
	M_snprintf(s, 33, /* sizeof(s) == sizeof(void*) */
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		b[0], b[1], b[2] , b[3] , b[4] , b[5] , b[6] , b[7],
		b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]
	);
}

static void RFC2831_H(char *s, size_t len, unsigned char b[16])
{
	unsigned int mdlen = 16;
	EVP_Digest(s, len, b, &mdlen, EVP_md5(), NULL);
}

static M_state_machine_status_t M_state_auth_digest_md5(void *data, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;
	M_buf_add_str(session->out_buf, "AUTH DIGEST-MD5\r\n");
	*next = STATE_AUTH_DIGEST_MD5_NONCE_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

typedef struct {
	const char *username;
	const char *realm;
	const char *password;
	const char *algorithm;
	const char *nonce;
	const char *nonce_count;
	const char *cnonce;
	const char *qop;
	const char *method;
	const char *digest_uri;
	const char *H_entity_body;
	const char *authzid;
} digest_md5_parameters_t;

static void digest_md5_compute_HA1(digest_md5_parameters_t *parameters, unsigned char HA1[16])
{
	M_buf_t *buf   = NULL;
	char    *str   = NULL;
	size_t   len;

	buf = M_buf_create();
	M_bprintf(buf, "%s:%s:%s", parameters->username, parameters->realm, parameters->password);
	str = M_buf_finish_str(buf, &len);
	RFC2831_H(str, len, HA1);
	M_free(str);

	buf = M_buf_create();
	M_buf_add_bytes(buf, HA1, 16);
	M_bprintf(buf, ":%s:%s", parameters->nonce, parameters->cnonce);
	if (parameters->authzid != NULL) {
		M_bprintf(buf, ":%s", parameters->authzid);
	}
	str = M_buf_finish_str(buf, &len);
	RFC2831_H(str, len, HA1);
	M_free(str);
}

static void digest_md5_compute_HA2(digest_md5_parameters_t *parameters, unsigned char HA2[16])
{
	M_buf_t *buf   = NULL;
	char    *str   = NULL;
	size_t   len;

	buf = M_buf_create();
	M_bprintf(buf, "%s:%s", parameters->method, parameters->digest_uri);
	if (parameters->qop != NULL &&
		(M_str_caseeq(parameters->qop, "auth-int") ||
			M_str_caseeq(parameters->qop, "auth-conf"))
	) {
		 M_bprintf(buf, ":%s", parameters->H_entity_body);
	}
	str = M_buf_finish_str(buf, &len);
	RFC2831_H(str, len, HA2);
	M_free(str);
}

static void digest_md5_compute_response(digest_md5_parameters_t *parameters, char response[32])
{
	M_buf_t       *buf             = NULL;
	char          *str             = NULL;
	size_t         i               = 0;
	unsigned char  HA1[16]         = { 0 };
	unsigned char  HA2[16]         = { 0 };
	unsigned char  HFINAL[16]      = { 0 };
	size_t         len;

	digest_md5_compute_HA1(parameters, HA1);
	digest_md5_compute_HA2(parameters, HA2);

	buf = M_buf_create();

	for (i = 0; i < 16; i++) {
		M_buf_add_bytehex(buf, HA1[i], M_FALSE);
	}

	M_bprintf(buf, ":%s:", parameters->nonce);
	if (parameters->qop != NULL) {
		M_bprintf(buf, "%s:%s:%s:", parameters->nonce_count, parameters->cnonce, parameters->qop);
	}

	for (i = 0; i < 16; i++) {
		M_buf_add_bytehex(buf, HA2[i], M_FALSE);
	}

	str = M_buf_finish_str(buf, &len);
	RFC2831_H(str, len, HFINAL);
	M_free(str);

	RFC2831_HEX(HFINAL, response);
}

static M_state_machine_status_t M_auth_digest_md5_nonce_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_session_t     *session          = data;
	M_state_machine_status_t  machine_status   = M_STATE_MACHINE_STATUS_ERROR_STATE;
	char                     *parameters_str   = NULL;
	M_hash_dict_t            *parameters_dict  = NULL;
	char                     *digest_uri       = NULL;
	M_buf_t                  *buf              = NULL;
	char                     *str              = NULL;
	char                     *str_b64          = NULL;
	const char               *line             = NULL;
	size_t                    digest_uri_size  = 0;
	char                      response[33]     = { 0 };
	unsigned char             cnonce_bytes[16] = { 0 };
	char                      cnonce[33]       = { 0 };
	digest_md5_parameters_t   parameters       = { 0 };
	size_t                    len;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 334))
		goto done;

	line = M_list_str_last(session->tcp.smtp_response);
	if (
		!(parameters_str  = (char *)M_bincodec_decode_alloc(line, M_str_len(line), &len, M_BINCODEC_BASE64)) ||
		!(parameters_dict = M_hash_dict_deserialize(parameters_str, len, ',', '=', '"', '\\', M_HASH_DICT_NONE))
	) {
		session->tcp.is_connect_fail = M_TRUE;
		session->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Failed to decode digest-md5 parameters: %s", line);
		goto done;
	}

	parameters.username = session->ep->tcp.username;
	//parameters.username = "user";
	M_hash_dict_get(parameters_dict, "realm", &parameters.realm);
	parameters.password = session->ep->tcp.password;
	M_hash_dict_get(parameters_dict, "algorithm", &parameters.algorithm);
	M_hash_dict_get(parameters_dict, "nonce", &parameters.nonce);
	M_hash_dict_get(parameters_dict, "qop", &parameters.qop);
	M_hash_dict_get(parameters_dict, "authzid", &parameters.authzid);
	RAND_bytes(cnonce_bytes, sizeof(cnonce_bytes));
	RFC2831_HEX(cnonce_bytes, cnonce);
	parameters.cnonce = cnonce;
	parameters.nonce_count = "00000001"; /* Always - we will terminate if it doesn't work */
	parameters.method = "AUTHENTICATE"; /* Always */
	parameters.H_entity_body = "00000000000000000000000000000000"; /* Always */
	digest_uri_size = 5 + M_str_len(parameters.realm) + 1;
	digest_uri = M_malloc(digest_uri_size);
	M_snprintf(digest_uri, digest_uri_size, "smtp/%s", parameters.realm);
	parameters.digest_uri = digest_uri;

	digest_md5_compute_response(&parameters, response);

	M_hash_dict_remove(parameters_dict, "algorithm");
	M_hash_dict_insert(parameters_dict, "username", parameters.username);
	M_hash_dict_insert(parameters_dict, "cnonce", parameters.cnonce);
	M_hash_dict_insert(parameters_dict, "nc", parameters.nonce_count);
	M_hash_dict_insert(parameters_dict, "digest-uri", parameters.digest_uri);
	M_hash_dict_insert(parameters_dict, "response", response);

	buf = M_buf_create();
	M_hash_dict_serialize_buf(parameters_dict, buf, ',', '=', '"', '\\', M_HASH_DICT_SER_FLAG_NONE);
	str = M_buf_finish_str(buf, &len);
	str_b64 = M_bincodec_encode_alloc((unsigned char*)str, len, 0, M_BINCODEC_BASE64);
	M_buf_add_str(session->out_buf, str_b64);
	M_buf_add_str(session->out_buf, "\r\n");

	*next = STATE_AUTH_DIGEST_MD5_ACK_RESPONSE;
	machine_status = M_STATE_MACHINE_STATUS_NEXT;
done:
	M_free(str);
	M_free(str_b64);
	M_free(digest_uri);
	M_free(parameters_str);
	M_hash_dict_destroy(parameters_dict);
	return machine_status;
}

static M_state_machine_status_t M_auth_digest_md5_ack_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_session_t *session = data;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		return M_STATE_MACHINE_STATUS_ERROR_STATE;

	/*
	 * If everything worked, the line will contain a base64 encoded rspauth=<md5hash>
	 * It is sometimes used for sessioning information, but we are going to
	 * drop it on the floor for our SMTP purposes.
	 */


	if (session->tcp.smtp_response_code == 250) {
		/* It is possible for the SMTP server to send a
			* 250 <respcode> to eliminate a tedious back and forth */
		return M_STATE_MACHINE_STATUS_DONE;
	}

	if (!M_net_smtp_flow_tcp_check_smtp_response_code(session, 334))
		return M_STATE_MACHINE_STATUS_ERROR_STATE;


	M_buf_add_str(session->out_buf, "\r\n");

	*next = STATE_AUTH_DIGEST_MD5_FINAL_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_state_machine_t * M_net_smtp_flow_tcp_auth(void)
{
	M_state_machine_t *m = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp-auth", M_STATE_MACHINE_NONE);

	M_state_machine_insert_state(m, STATE_AUTH_START, 0, NULL, M_state_auth_start, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_PLAIN, 0, NULL, M_state_auth_plain, NULL, NULL);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_PLAIN_RESPONSE, M_auth_final_response_post_cb);

	M_state_machine_insert_state(m, STATE_AUTH_LOGIN, 0, NULL, M_state_auth_login, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_USERNAME, 0, NULL, M_state_auth_login_username, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_PASSWORD, 0, NULL, M_state_auth_login_password, NULL, NULL);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_LOGIN_RESPONSE, M_auth_login_response_post_cb);

	M_state_machine_insert_state(m, STATE_AUTH_CRAM_MD5, 0, NULL, M_state_auth_cram_md5, NULL, NULL);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_CRAM_MD5_SECRET_RESPONSE, M_auth_cram_md5_secret_response_post_cb);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_CRAM_MD5_FINAL_RESPONSE, M_auth_final_response_post_cb);

	M_state_machine_insert_state(m, STATE_AUTH_DIGEST_MD5, 0, NULL, M_state_auth_digest_md5, NULL, NULL);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_DIGEST_MD5_NONCE_RESPONSE, M_auth_digest_md5_nonce_response_post_cb);
	M_net_smtp_flow_tcp_smtp_response_insert_subm(m, STATE_AUTH_DIGEST_MD5_FINAL_RESPONSE, M_auth_final_response_post_cb);

	return m;
}
