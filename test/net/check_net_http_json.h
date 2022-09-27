const char * json_str = \
"{ " \
	"\"basic\": { " \
		"\"entries\": [ " \
			"{ " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 200 OK\\r\\nContent-Length: 44\\r\\n\\r\\n<html><body><h1>It works!</h1></body></html>\" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/redirect\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 301 Moved Permanently\\r\\nLocation: http://localhost:%hu/\\r\\n\\r\\n\" " \
			"} " \
		"], " \
		"\"notfound_response\": \"404\" " \
	"} " \
"}";
