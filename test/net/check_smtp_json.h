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
	"\"bad1\": {" \
		"\"CONNECTED\": \"Hi\\r\\n\"" \
	"}," \
	"\"bad2\": {" \
		"\"CONNECTED\": \"Greetings, fellow human!\\r\\n\"" \
	"}," \
	"\"bad3\": {" \
		"\"CONNECTED\": \"999 Auf deutsch\\r\\n\"" \
	"}," \
	"\"bad4\": {" \
		"\"CONNECTED\": \"220...\\r\\n\"" \
	"}," \
	"\"bad5\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250-VRFY\\r\\n251 AUTH PLAIN\\r\\n\"" \
	"}," \
	"\"bad6\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"251 AUTH PLAIN\\r\\n\"" \
	"}," \
	"\"bad7\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 STARTTLS\\r\\n\"," \
		"\"STARTTLS\\r\\n\": \"No\\r\\n\"" \
	"}," \
	"\"bad8\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 STARTTLS\\r\\n\"," \
		"\"STARTTLS\\r\\n\": \"219 not 220\\r\\n\"" \
	"}," \
	"\"starttls\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 STARTTLS\\r\\n\"," \
		"\"STARTTLS\\r\\n\": \"220 \\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH[^\\r]*\\r\\n\": \"503 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}," \
	"\"auth_digest_md5\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 AUTH LOGIN PLAIN CRAM-MD5 DIGEST-MD5\\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH DIGEST-MD5[^\\r]*\\r\\n\": \"334 cmVhbG09ImxvY2FsaG9zdCIsbm9uY2U9IjJqWmo2RzE3bHVXNy9xbXdzdmV3QWc9PSIscW9wPSJhdXRoIixjaGFyc2V0PSJ1dGYtOCIsYWxnb3JpdGhtPSJtZDUtc2VzcyI=\\r\\n\"," \
		"\"DIGEST-MD5\": \"334 \\r\\n\"," \
		"\"\\r\\n\": \"235 \\r\\n\"," \
		"\"RCPT TO:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"DATA\\r\\n\": \"354 \\r\\n\"," \
		"\"QUIT\\r\\n\": \"221 \\r\\n\"" \
	"}," \
	"\"auth_cram_md5\": {" \
		"\"CONNECTED\": \"220 \\r\\n\"," \
		"\"DATA_ACK\": \"250 \\r\\n\"," \
		"\"EHLO[^\\r]*\\r\\n\": \"250 AUTH CRAM-MD5\\r\\n\"," \
		"\"MAIL FROM:<[^>]*>\\r\\n\": \"250 \\r\\n\"," \
		"\"AUTH CRAM-MD5[^\\r]*\\r\\n\": \"334 PDA0NjQ1NDA4NzU3ODk5OTEuMTY1NDc5OTM4NEBha2lzdGxlci5wMTBqYXgub2ZmaWNlLm1vbmV0cmEuY29tPg==\\r\\n\"," \
		"\"dXNlciAzMjEwMGVlYzdhMzJiNDNlNWE1NzQ3NjY4NTc2ODQ5MQ[^\\r]*\\r\\n\": \"235 \\r\\n\"," \
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
