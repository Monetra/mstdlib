/*
 * CONNECTED: string sends on open connection
 * DATA_ACK: sends after \r\n.\r\n read
 */

const char * json_str = \
"{" \
	"\"minimal\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 \\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH[^\\r]*\\r\\n\": \"503 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}," \
	"\"auth_plain\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 AUTH PLAIN\\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH PLAIN[^\\r]*\\r\\n\": \"235 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}," \
	"\"auth_login\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 AUTH LOGIN\\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH LOGIN[^\\r]*\\r\\n\": \"334 VXNlcm5hbWU6\\r\\n\"," \
		"\"dXNlcg[^\\r]*\\r\\n\": \"334 UGFzc3dvcmQ6\\r\\n\"," \
		"\"cGFzcw[^\\r]*\\r\\n\": \"235 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}," \
	"\"reject_457\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"457 Testing timeout try again in 3000ms\\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 \\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH[^\\r]*\\r\\n\": \"503 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}" \
"}";
