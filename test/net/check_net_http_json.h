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
						"\"value\": \"/badproto\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.4 200 OK\\r\\n\" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/disconnect\" " \
					"} " \
				"], " \
				"\"value\": \"(ノ° °)ノ︵┻━┻ \" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"POST\" " \
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
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/redirect_bad\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 301 Moved Permanently\\r\\nLocation:\\r\\n\\r\\n\" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/redirect_bad2\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 301 Moved Permanently\\r\\nLocation: http://localhost:0/ \\r\\n\\r\\n\" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/redirect2\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 301 Moved Permanently\\r\\nLocation: http://localhost:%hu/redirect\\r\\n\\r\\n\" " \
			"}, { " \
				"\"keys\": [ " \
					"{ " \
						"\"key\": \":method\", " \
						"\"value\": \"GET\" " \
					"}, { " \
						"\"key\": \":path\", " \
						"\"value\": \"/redirect3\" " \
					"} " \
				"], " \
				"\"value\": \"HTTP/1.1 301 Moved Permanently\\r\\nLocation: http://localhost:%hu/redirect2\\r\\n\\r\\n\" " \
			"} " \
		"], " \
		"\"notfound_response\": \"404\" " \
	"} " \
"}";
