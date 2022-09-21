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

static struct {
	const char *key;
	const char *value;
} M_http2_header_table[] = {
	{ NULL, NULL },
	{":authority", NULL },
	{":method", "GET" },
	{":method", "POST" },
	{":path", "/" },
	{":path", "/index.html" },
	{":scheme", "http" },
	{":scheme", "https" },
	{":status", "200" },
	{":status", "204" },
	{":status", "206" },
	{":status", "304" },
	{":status", "400" },
	{":status", "404" },
	{":status", "500" },
	{"accept-charset", NULL },
	{"accept-encoding", "gzip,deflate" },
	{"accept-language", NULL },
	{"accept-ranges", NULL },
	{"accept", NULL },
	{"access-control-allow-origin", NULL },
	{"age", NULL },
	{"allow", NULL },
	{"authorization", NULL },
	{"cache-control", NULL },
	{"content-disposition", NULL },
	{"content-encoding", NULL },
	{"content-language", NULL },
	{"content-length", NULL },
	{"content-location", NULL },
	{"content-range", NULL },
	{"content-type", NULL },
	{"cookie", NULL },
	{"date", NULL },
	{"etag", NULL },
	{"expect", NULL },
	{"expires", NULL },
	{"from", NULL },
	{"host", NULL },
	{"if-match", NULL },
	{"if-modified-since", NULL },
	{"if-none-match", NULL },
	{"if-range", NULL },
	{"if-unmodified-since", NULL },
	{"last-modified", NULL },
	{"link", NULL },
	{"location", NULL },
	{"max-forwards", NULL },
	{"proxy-authenticate", NULL },
	{"proxy-authorization", NULL },
	{"range", NULL },
	{"referer", NULL },
	{"refresh", NULL },
	{"retry-after", NULL },
	{"server", NULL },
	{"set-cookie", NULL },
	{"strict-transport-security", NULL },
	{"transfer-encoding", NULL },
	{"user-agent", NULL },
	{"vary", NULL },
	{"via", NULL },
	{"www-authenticate", NULL },
};
