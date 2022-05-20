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

static void determine_auth_method(const char *line, M_net_smtp_endpoint_slot_t *slot)
{
	char   **methods;
	size_t  *method_lens;
	size_t  n;
	size_t  i;
	size_t  j;
	size_t  found_priority = 0;

	methods = M_str_explode(' ', line, M_str_len(line), &n, &method_lens);
	for (i = 0; i < n; i++) {
		/* Only need to look for higher priority methods */
		for (j = found_priority + 1; j < M_net_smtp_auth_search_len; j++) {
			if (M_str_eq(methods[i], M_net_smtp_auth_search[j].str)) {
				slot->tcp.smtp_authtype = M_net_smtp_auth_search[j].type;
				found_priority = j;
				break;
			}
		}
		if (found_priority + 1 == M_net_smtp_auth_search_len)
			break; /* The best */
	}
	M_str_explode_free(methods, n);
	M_free(method_lens);
}

static M_state_machine_status_t M_ehlo_response_post_cb(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;
	size_t                      i;
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
	for (i = 0; i < M_list_str_len(slot->tcp.smtp_response); i++) {
		const char *ehlo_line = M_list_str_at(slot->tcp.smtp_response, i);
		if (M_str_caseeq_max("STARTTLS", ehlo_line, 8)) {
			slot->tcp.is_starttls_capable = M_TRUE;
			continue;
		}
		if (M_str_caseeq_max("AUTH ", ehlo_line, 5) == 0) {
			/* "AUTH " not "AUTH=" */
			determine_auth_method(ehlo_line, slot);
			continue;
		}
	}
	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	return M_net_smtp_flow_tcp_smtp_response_post_cb_helper(data, machine_status, NULL);
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
