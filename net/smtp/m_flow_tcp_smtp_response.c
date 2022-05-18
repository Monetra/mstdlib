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
	STATE_READ_LINE = 1,
} m_state_ids;

static M_state_machine_status_t M_state_read_line(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot           = data;
	char                       *line           = NULL;
	M_int16                     response_code  = 0;
	M_state_machine_status_t    machine_status = M_STATE_MACHINE_STATUS_ERROR_STATE;

	line = M_parser_read_strdup_until(slot->in_parser, "\r\n", M_TRUE);

	if (line == NULL)
		return M_STATE_MACHINE_STATUS_WAIT;

/* RFC 5321 p47
 * Greeting       = ( "220 " (Domain / address-literal)
 *                [ SP textstring ] CRLF ) /
 *                ( "220-" (Domain / address-literal)
 *                [ SP textstring ] CRLF
 *                *( "220-" [ textstring ] CRLF )
 *                "220" [ SP textstring ] CRLF )
 *
 * textstring     = 1*(%d09 / %d32-126) ; HT, SP, Printable US-ASCII
 *
 * Reply-line     = *( Reply-code "-" [ textstring ] CRLF )
 *                Reply-code [ SP textstring ] CRLF
 *
 * Reply-code     = %x32-35 %x30-35 %x30-39
 *
 * ...
 *
 *  An SMTP client MUST determine its actions only by the reply code, not
 * by the text (except for the "change of address" 251 and 551 and, if
 * necessary, 220, 221, and 421 replies); in the general case, any text,
 * including no text at all (although senders SHOULD NOT send bare
 * codes), MUST be acceptable.  The space (blank) following the reply
 * code is considered part of the text.  Whenever possible, a receiver-
 * SMTP SHOULD test the first digit (severity indication) of the reply
 * code.
 */

/* So, the smallest possible response is [2-5][0-5][0-9]\r\n */


	if (
		M_str_len(line) < 5                 ||
		!(line[0] >= '2' && line[0] <= '5') ||
		!(line[1] >= '0' && line[1] <= '5') ||
		!(line[2] >= '0' && line[2] <= '9') ||
		!(line[3] == '-' || line[3] == ' ' || line[3] == '\r')
	) {
		/* Classify as connect failure so endpoint can get removed */
		slot->tcp.is_connect_fail = M_TRUE;
		slot->tcp.net_error = M_NET_ERROR_PROTOFORMAT;
		M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Ill-formed SMTP response: %s", line);
		goto done;
	}

	response_code = 100 * (line[0] - '0') + 10 * (line[1] - '0') + (line[2] - '0');
	if (slot->tcp.smtp_response_code == 0) {
		slot->tcp.smtp_response_code = response_code;
	} else {
		if (slot->tcp.smtp_response_code != response_code) {
			/* Classify as connect failure so endpoint can get removed */
			slot->tcp.is_connect_fail = M_TRUE;
			slot->tcp.net_error = M_NET_ERROR_PROTOFORMAT;
			M_snprintf(slot->errmsg, sizeof(slot->errmsg), "Mismatched SMTP response code: %d != %s",
					slot->tcp.smtp_response_code, line);
			goto done;
		}
	}

	M_list_str_insert(slot->tcp.smtp_response, line);

	if (line[3] == '-') {
		*next = STATE_READ_LINE;
		machine_status = M_STATE_MACHINE_STATUS_NEXT;
		goto done;
	}

	machine_status = M_STATE_MACHINE_STATUS_DONE;

done:
	M_free(line);
	return machine_status;

}

M_bool M_net_smtp_flow_tcp_smtp_response_pre_cb(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)status;
	(void)next;

	slot->tcp.smtp_response = M_list_str_create(M_LIST_STR_NONE);
	slot->tcp.smtp_response_code = 0;
	return M_TRUE;
}

M_state_machine_status_t M_net_smtp_flow_tcp_smtp_response_post_cb_helper(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_slot_t *slot = data;
	(void)next;
	M_list_str_destroy(slot->tcp.smtp_response);
	slot->tcp.smtp_response = NULL;
	slot->tcp.smtp_response_code = 0;
	return sub_status;
}

M_state_machine_t * M_net_smtp_flow_tcp_smtp_response()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-smtp-response", M_STATE_MACHINE_CONTINUE_LOOP | M_STATE_MACHINE_SELF_CALL);
	M_state_machine_insert_state(m, STATE_READ_LINE, 0, NULL, M_state_read_line, NULL, NULL);
	return m;
}
