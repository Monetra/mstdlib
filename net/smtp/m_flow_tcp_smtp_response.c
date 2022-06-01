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

typedef enum {
	STATE_READ_LINE = 1,
} m_state_ids;

static M_state_machine_status_t M_state_read_line(void *data, M_uint64 *next)
{
	M_net_smtp_endpoint_session_t *session        = data;
	unsigned char                  byte           = 0;
	M_uint64                       response_code  = 0;
	char                          *line           = NULL;

	M_parser_mark(session->in_parser);
	if (!M_parser_consume_str_until(session->in_parser, "\r\n", M_TRUE)) {
		M_parser_mark_clear(session->in_parser);
		return M_STATE_MACHINE_STATUS_WAIT;
	}
	M_parser_mark_rewind(session->in_parser);
	M_parser_mark(session->in_parser);

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
		M_parser_len(session->in_parser) < 5                                                   ||
		!M_parser_read_uint(session->in_parser, M_PARSER_INTEGER_ASCII, 3, 10, &response_code) ||
		!(response_code >= 200 && response_code <= 559)                                     ||
		!M_parser_peek_byte(session->in_parser, &byte)                                         ||
		!M_str_chr(" -\r", (char)byte)                                                      ||
		(session->tcp.smtp_response_code != 0 && session->tcp.smtp_response_code != response_code)
	) {
		M_parser_mark_clear(session->in_parser);
		/* Classify as connect failure so endpoint can get removed */
		session->tcp.is_connect_fail = M_TRUE;
		session->tcp.net_error = M_NET_ERROR_PROTOFORMAT;
		M_snprintf(session->errmsg, sizeof(session->errmsg), "Ill-formed SMTP response");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	session->tcp.smtp_response_code = response_code;
	M_parser_mark_rewind(session->in_parser);
	M_parser_consume(session->in_parser, 4); /* skip over number code */
	line = M_parser_read_strdup_until(session->in_parser, "\r\n", M_FALSE);
	M_list_str_insert(session->tcp.smtp_response, line);
	M_free(line);

	M_parser_consume(session->in_parser, 2); /* skip over \r\n */

	if (byte == '-') {
		*next = STATE_READ_LINE;
		return M_STATE_MACHINE_STATUS_NEXT;
	}

	return M_STATE_MACHINE_STATUS_DONE;
}

M_bool M_net_smtp_flow_tcp_smtp_response_pre_cb_helper(void *data, M_state_machine_status_t *status, M_uint64 *next)
{
	M_net_smtp_endpoint_session_t *session = data;
	(void)status;
	(void)next;

	session->tcp.smtp_response = M_list_str_create(M_LIST_STR_NONE);
	session->tcp.smtp_response_code = 0;
	return M_TRUE;
}

M_state_machine_status_t M_net_smtp_flow_tcp_smtp_response_post_cb_helper(void *data, M_state_machine_status_t sub_status,
		M_uint64 *next)
{
	M_net_smtp_endpoint_session_t *session = data;
	(void)next;
	M_list_str_destroy(session->tcp.smtp_response);
	session->tcp.smtp_response = NULL;
	session->tcp.smtp_response_code = 0;
	return sub_status;
}

M_state_machine_t * M_net_smtp_flow_tcp_smtp_response()
{
	M_state_machine_t *m;
	m = M_state_machine_create(0, "SMTP-flow-tcp-smtp-response", M_STATE_MACHINE_CONTINUE_LOOP | M_STATE_MACHINE_SELF_CALL);
	M_state_machine_insert_state(m, STATE_READ_LINE, 0, NULL, M_state_read_line, NULL, NULL);
	return m;
}
