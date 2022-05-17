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

#include "m_flow.h"
#include <openssl/hmac.h>

typedef enum {
	STATE_AUTH_START = 1,
	STATE_AUTH_PLAIN,
	STATE_AUTH_PLAIN_RESPONSE,
	STATE_AUTH_LOGIN,
	STATE_AUTH_LOGIN_USERNAME,
	STATE_AUTH_LOGIN_PASSWORD,
	STATE_AUTH_LOGIN_RESPONSE,
	STATE_AUTH_CRAM_MD5,
	STATE_AUTH_CRAM_MD5_SALT_RESPONSE,
	STATE_AUTH_CRAM_MD5_FINAL_RESPONSE,
} m_state_ids;

static M_state_machine_status_t M_state_auth_start(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	switch (slot->tcp.smtp_authtype) {
		case M_NET_SMTP_AUTHTYPE_PLAIN:
			*next = STATE_AUTH_PLAIN;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		case M_NET_SMTP_AUTHTYPE_LOGIN:
			*next = STATE_AUTH_LOGIN;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		case M_NET_SMTP_AUTHTYPE_CRAM_MD5:
			*next = STATE_AUTH_CRAM_MD5;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		case M_NET_SMTP_AUTHTYPE_NONE:
			return M_STATE_MACHINE_STATUS_DONE;
			break;
	}

	/* Classify as connect failure so endpoint can get removed */
	slot->tcp.is_connect_fail = M_TRUE;
	slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
	M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Something weird happened");
	return M_STATE_MACHINE_STATUS_ERROR_STATE;
}

static M_state_machine_status_t M_state_auth_plain(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "AUTH PLAIN %s\r\n", slot->tcp.auth_plain);
	*next = STATE_AUTH_PLAIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_plain_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->tcp.smtp_response_code != 235) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 235 auth response, got: %s",
				M_list_str_last(slot->tcp.smtp_response));
		goto done;
	}

	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_state_auth_login(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "AUTH LOGIN\r\n");
	slot->tcp.auth_login_response_count = 0;
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_username(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "%s\r\n", slot->tcp.auth_login_user);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_password(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "%s\r\n", slot->tcp.auth_login_pass);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_login_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	const char                 *line           = NULL;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	line = M_list_str_last(slot->tcp.smtp_response);

	slot->tcp.auth_login_response_count++;
	if (slot->tcp.auth_login_response_count < 3 && slot->tcp.smtp_response_code != 334) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 334 auth response, got: %s", line);
		goto done;
	}

	if (slot->tcp.auth_login_response_count == 3 && slot->tcp.smtp_response_code != 235) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 235 auth response, got: %s", line);
		goto done;
	}

	if (slot->tcp.auth_login_response_count == 3) {
		machine_status = M_STATE_MACHINE_STATUS_DONE;
		goto done;
	}

	if (M_str_cmpsort_max(&line[4], "VXNlcm5hbWU6\r\n", 14) == 0) {
		/* base64 for "Username" */
		*next = STATE_AUTH_LOGIN_USERNAME;
		machine_status = M_STATE_MACHINE_STATUS_NEXT;
		goto done;
	}

	if (M_str_cmpsort_max(&line[4], "UGFzc3dvcmQ6\r\n", 14) == 0) {
		/* base64 for "Password" */
		*next = STATE_AUTH_LOGIN_PASSWORD;
		machine_status = M_STATE_MACHINE_STATUS_NEXT;
		goto done;
	}

	M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Unknown auth-login request: %s", line);
	machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_state_auth_cram_md5(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "AUTH CRAM-MD5\r\n");
	*next = STATE_AUTH_CRAM_MD5_SALT_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_auth_cram_md5_salt_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	size_t                      len            = 0;
	unsigned int                uint           = 0;
	unsigned char               buf[512]       = { 0 };
	char                       *challenge      = NULL;
	const char                 *line;
	unsigned char               d[16]; /* digest */

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	line = M_list_str_last(slot->tcp.smtp_response);
	if (slot->tcp.smtp_response_code != 334) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 334 auth response, got: %s", line);
		goto done;
	}

	if (M_bincodec_decode(buf, sizeof(buf)-1, &line[4], M_str_len(&line[4]), M_BINCODEC_BASE64) <= 0) {
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Failed to decode salt: %s", line);
		goto done;
	}

	uint = sizeof(d);
	HMAC(
		EVP_md5(),
		slot->tcp.password, (int)slot->tcp.password_len,
		buf, M_str_len((const char *)buf), /* buf contains salt */
		d, &uint
	);

	len = M_snprintf(
		(char *)buf, sizeof(buf),
		"%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		slot->tcp.username,
		d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]
	);

	if (!(challenge = M_bincodec_encode_alloc(buf, len, 0, M_BINCODEC_BASE64))) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Allocation failed");
		goto done;
	}
	M_bprintf(slot->out_buf, "%s\r\n", challenge);
	M_free(challenge);

	*next = STATE_AUTH_CRAM_MD5_FINAL_RESPONSE;
	machine_status = M_STATE_MACHINE_STATUS_NEXT;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

static M_state_machine_status_t M_auth_cram_md5_final_response_post_cb(void *data,
		M_state_machine_status_t sub_status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->tcp.smtp_response_code != 235) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_AUTHENTICATION;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 235 auth response, got: %s",
				M_list_str_last(slot->tcp.smtp_response));
		goto done;
	}
	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb(data, machine_status, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_state_machine_t * M_net_smtp_flow_tcp_auth()
{
	M_state_machine_t *m      = NULL;
	M_state_machine_t *sub_m  = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp-auth", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_AUTH_START, 0, NULL, M_state_auth_start, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_PLAIN, 0, NULL, M_state_auth_plain, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH_PLAIN_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_auth_plain_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_AUTH_LOGIN, 0, NULL, M_state_auth_login, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_USERNAME, 0, NULL, M_state_auth_login_username, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_PASSWORD, 0, NULL, M_state_auth_login_password, NULL, NULL);
	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH_LOGIN_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_auth_login_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_AUTH_CRAM_MD5, 0, NULL, M_state_auth_cram_md5, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH_CRAM_MD5_SALT_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_auth_cram_md5_salt_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH_CRAM_MD5_FINAL_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb, M_auth_cram_md5_final_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	return m;
}
