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

typedef enum {
	STATE_AUTH_START = 1,
	STATE_AUTH_PLAIN,
	STATE_AUTH_PLAIN_RESPONSE,
	STATE_AUTH_LOGIN,
	STATE_AUTH_LOGIN_USERNAME,
	STATE_AUTH_LOGIN_PASSWORD,
	STATE_AUTH_LOGIN_RESPONSE,
} m_state_ids;

static M_state_machine_status_t M_state_auth_start(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	switch (slot->smtp_authtype) {
		case M_NET_SMTP_AUTHTYPE_PLAIN:
			*next = STATE_AUTH_PLAIN;
			return M_STATE_MACHINE_STATUS_NEXT;
			break;
		case M_NET_SMTP_AUTHTYPE_LOGIN:
			*next = STATE_AUTH_LOGIN;
			return M_STATE_MACHINE_STATUS_NEXT;
		case M_NET_SMTP_AUTHTYPE_CRAM_MD5:
		case M_NET_SMTP_AUTHTYPE_NONE:
			break;
	}
	return M_STATE_MACHINE_STATUS_DONE;
}

static M_state_machine_status_t M_state_auth_plain(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "AUTH PLAIN %s\r\n", slot->str_auth_plain_base64);
	*next = STATE_AUTH_PLAIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_bool M_auth_plain_response_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)status;
	(void)next;

	slot->smtp_response = M_list_str_create(M_LIST_STR_NONE);
	slot->smtp_response_code = 0;
	return M_TRUE;
}

static M_state_machine_status_t M_auth_plain_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->smtp_response_code != 235) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 235 auth response, got: %s",
				M_list_str_at(slot->smtp_response, 0));
		goto done;
	}

	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	M_list_str_destroy(slot->smtp_response);
	slot->smtp_response = NULL;
	slot->smtp_response_code = 0;
	return machine_status;
}

static M_state_machine_status_t M_state_auth_login(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "AUTH LOGIN\r\n");
	slot->auth_login_response_count = 0;
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_username(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "%s\r\n", slot->str_auth_login_username_base64);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_state_auth_login_password(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	M_bprintf(slot->out_buf, "%s\r\n", slot->str_auth_login_password_base64);
	*next = STATE_AUTH_LOGIN_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_bool M_auth_login_response_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)status;
	(void)next;

	slot->smtp_response = M_list_str_create(M_LIST_STR_NONE);
	slot->smtp_response_code = 0;
	return M_TRUE;
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

	line = M_list_str_at(slot->smtp_response, 0);

	slot->auth_login_response_count++;
	if (slot->auth_login_response_count < 3 && slot->smtp_response_code != 334) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 334 auth response, got: %s", line);
		goto done;
	}

	if (slot->auth_login_response_count == 3 && slot->smtp_response_code != 235) {
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 235 auth response, got: %s", line);
		goto done;
	}

	if (slot->auth_login_response_count == 3) {
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
	M_list_str_destroy(slot->smtp_response);
	slot->smtp_response = NULL;
	slot->smtp_response_code = 0;
	return machine_status;
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
			M_auth_plain_response_pre_cb, M_auth_plain_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	M_state_machine_insert_state(m, STATE_AUTH_LOGIN, 0, NULL, M_state_auth_login, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_USERNAME, 0, NULL, M_state_auth_login_username, NULL, NULL);
	M_state_machine_insert_state(m, STATE_AUTH_LOGIN_PASSWORD, 0, NULL, M_state_auth_login_password, NULL, NULL);
	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_AUTH_LOGIN_RESPONSE, 0, NULL, sub_m,
			M_auth_login_response_pre_cb, M_auth_login_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);


	return m;
}
