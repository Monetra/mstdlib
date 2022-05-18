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
	STATE_EHLO = 1,
	STATE_EHLO_RESPONSE,
} m_state_ids;

static M_state_machine_status_t M_state_ehlo(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;

	M_bprintf(slot->out_buf, "EHLO %s\r\n", slot->tcp.ehlo_domain);
	*next = STATE_EHLO_RESPONSE;
	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t M_ehlo_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	(void)next;

	if (sub_status == M_STATE_MACHINE_STATUS_ERROR_STATE)
		goto done;

	if (slot->tcp.smtp_response_code != 250) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_PROTOFORMAT;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Expected 250 EHLO response code, got: %llu",
				slot->tcp.smtp_response_code);
		goto done;
	}

	slot->tcp.is_starttls_capable = M_FALSE;
	slot->tcp.smtp_authtype = M_NET_SMTP_AUTHTYPE_NONE;
	for (size_t i = 0; i < M_list_str_len(slot->tcp.smtp_response); i++) {
		const char *ehlo_line = M_list_str_at(slot->tcp.smtp_response, i);
		if (M_str_casecmpsort_max("STARTTLS", &ehlo_line[4], 8) == 0) {
			slot->tcp.is_starttls_capable = M_TRUE;
			continue;
		}
		if (M_str_casecmpsort_max("AUTH ", &ehlo_line[4], 5) == 0) {
			do {
				if (M_str_casestr(ehlo_line, "CRAM-MD5") != NULL) {
					slot->tcp.smtp_authtype = M_NET_SMTP_AUTHTYPE_CRAM_MD5;
					break;
				}
				if (M_str_casestr(ehlo_line, "PLAIN") != NULL) {
					slot->tcp.smtp_authtype = M_NET_SMTP_AUTHTYPE_PLAIN;
					break;
				}
				if (M_str_casestr(ehlo_line, "LOGIN") != NULL) {
					slot->tcp.smtp_authtype = M_NET_SMTP_AUTHTYPE_LOGIN;
					break;
				}
			} while(0);
			continue;
		}
	}
	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	M_list_str_destroy(slot->tcp.smtp_response);
	slot->tcp.smtp_response = NULL;
	slot->tcp.smtp_response_code = 0;
	return machine_status;
}

M_state_machine_t * M_net_smtp_flow_tcp_ehlo()
{
	M_state_machine_t *m      = NULL;
	M_state_machine_t *sub_m  = NULL;

	m = M_state_machine_create(0, "SMTP-flow-tcp-ehlo", M_STATE_MACHINE_NONE);
	M_state_machine_insert_state(m, STATE_EHLO, 0, NULL, M_state_ehlo, NULL, NULL);

	sub_m = M_net_smtp_flow_tcp_smtp_response();
	M_state_machine_insert_sub_state_machine(m, STATE_EHLO_RESPONSE, 0, NULL, sub_m,
			M_net_smtp_flow_tcp_smtp_response_pre_cb_helper, M_ehlo_response_post_cb, NULL, NULL);
	M_state_machine_destroy(sub_m);

	return m;
}
