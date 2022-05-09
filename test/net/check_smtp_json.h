/* Opening needs to be in position 0
	* DATA ACK of \r\n.\r\n needs to be in position 1
	* */

const char * json_str = \
"{" \
	"\"minimal\": {" \
		"\"\": \"220 \\r\\n\"," \
		"\"\\r\\n.\\r\\n\": \"220 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 \\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH[^\\r]*\\r\\n\": \"503 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}" \
"}";
