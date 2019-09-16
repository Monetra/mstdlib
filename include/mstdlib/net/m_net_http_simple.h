/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

#ifndef __M_NET_HTTP_SIMPLE_H__
#define __M_NET_HTTP_SIMPLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/formats/m_http.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_net_http_simple HTTP Simple Net
 *  \ingroup m_net
 * 
 * Simple HTTP interface
 *
 * \code{.c}
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_io.h>
 * #include <mstdlib/mstdlib_net.h>
 * #include <mstdlib/mstdlib_formats.h>
 * 
 * M_event_t *el;
 * 
 * static void done_cb(M_net_error_t net_error, M_http_error_t http_error, const M_http_simple_read_t *simple, const char *error, void *thunk)
 * {
 *     M_hash_dict_t       *headers;
 *     M_hash_dict_enum_t  *he;
 *     const char          *key;
 *     const char          *val;
 *     const unsigned char *data;
 *     size_t               len;
 * 
 *     if (net_error != M_NET_ERROR_SUCCESS) {
 *         M_printf("Ner Error: %s: %s\n", M_net_errcode_to_str(net_error), error);
 *         M_event_done(el);
 *         return;
 *     }
 * 
 *     if (http_error != M_HTTP_ERROR_SUCCESS) {
 *         M_printf("HTTP Error: %s: %s\n", M_http_errcode_to_str(http_error), error);
 *         M_event_done(el);
 *         return;
 *     }
 * 
 *     M_printf("---\n");
 * 
 *     M_printf("Status code: %u - %s\n", M_http_simple_read_status_code(simple), M_http_simple_read_reason_phrase(simple));
 * 
 *     M_printf("---\n");
 * 
 *     M_printf("Headers:\n");
 *     headers = M_http_simple_read_headers_dict(simple);
 *     M_hash_dict_enumerate(headers, &he);
 *     while (M_hash_dict_enumerate_next(headers, he, &key, &val)) {
 *         M_printf("\t%s: %s\n", key, val);
 *     }
 *     M_hash_dict_enumerate_free(he);
 *     M_hash_dict_destroy(headers);
 * 
 *     M_printf("---\n");
 * 
 *     M_printf("Body:\n");
 *     data = M_http_simple_read_body(simple, &len);
 *     M_printf("%.*s\n", (int)len, data);
 * 
 *     M_printf("---\n");
 * 
 *     M_event_done(el);
 * }
 * 
 * static void trace_cb(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
 * {
 *     const char *io_type = "UNKNOWN";
 *     char       *dump    = NULL;
 * 
 *     switch (type) {
 *         case M_IO_TRACE_TYPE_READ:
 *             io_type = "READ";
 *             break;
 *         case M_IO_TRACE_TYPE_WRITE:
 *             io_type = "WRITE";
 *             break;
 *         case M_IO_TRACE_TYPE_EVENT:
 *             return;
 *     }
 * 
 *     dump = M_str_hexdump(M_STR_HEXDUMP_NONE, 0, "\t", data, data_len);
 *     if (M_str_isempty(dump)) {
 *         M_free(dump);
 *         dump = M_strdup("\t<No Data>");
 *     }
 * 
 *     M_printf("%s\n%s\n", io_type, dump);
 *     M_free(dump);
 * }
 * 
 * static M_bool iocreate_cb(M_io_t *io, char *error, size_t errlen, void *thunk)
 * {
 *     (void)error;
 *     (void)errlen;
 *     (void)thunk;
 * 
 *     M_io_add_trace(io, NULL, trace_cb, io, NULL, NULL);
 *     return M_TRUE;
 * }
 * 
 * int main(int argc, char **argv)
 * {
 *     M_net_http_simple_t *hs;
 *     M_dns_t             *dns;
 *     M_tls_clientctx_t   *ctx;
 * 
 *     el  = M_event_create(M_EVENT_FLAG_NONE);
 *     dns = M_dns_create();
 * 
 *     ctx = M_tls_clientctx_create();
 *     M_tls_clientctx_set_default_trust(ctx);
 *     M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
 * 
 *     hs   = M_net_http_simple_create(el, dns, done_cb);
 *     M_net_http_simple_set_timeouts(hs, 2000, 0, 0);
 *     M_net_http_simple_set_tlsctx(hs, ctx);
 *     M_net_http_simple_set_message(hs, M_HTTP_METHOD_GET, NULL, "text/plain", "utf-8", NULL, NULL, 0);
 * 
 *     M_net_http_simple_set_iocreate(hs, iocreate_cb);
 * 
 *     M_net_http_simple_send(hs, "http://google.com/", NULL);
 * 
 *     M_event_loop(el, M_TIMEOUT_INF);
 * 
 *     M_tls_clientctx_destroy(ctx);
 *     M_dns_destroy(dns);
 *     M_event_destroy(el);
 *     return 0;
 * }
 * \endcode
 *
 * @{
 *
 */

struct M_net_http_simple;
typedef struct M_net_http_simple M_net_http_simple_t;

typedef void (*M_net_http_simple_done_cb)(M_net_error_t net_error, M_http_error_t http_error, const M_http_simple_read_t *simple, const char *error, void *thunk);
typedef M_bool (*M_net_http_simple_iocreate_cb)(M_io_t *io, char *error, size_t errlen, void *thunk);

M_API M_net_http_simple_t *M_net_http_simple_create(M_event_t *el, M_dns_t *dns, M_net_http_simple_done_cb done_cb);
M_API void M_net_http_simple_cancel(M_net_http_simple_t *hs);

M_API void M_net_http_simple_set_timeouts(M_net_http_simple_t *hs, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 overall_ms);
M_API void M_net_http_simple_set_max_redirects(M_net_http_simple_t *hs, M_uint64 max);
M_API void M_net_http_simple_set_tlsctx(M_net_http_simple_t *hs, M_tls_clientctx_t *ctx);
M_API void M_net_http_simple_set_iocreate(M_net_http_simple_t *hs, M_net_http_simple_iocreate_cb iocreate_cb);

M_API void M_net_http_simple_set_message(M_net_http_simple_t *hs, M_http_method_t method, const char *user_agent, const char *content_type, const char *charset, const M_hash_dict_t *headers, const unsigned char *message, size_t message_len);

M_API M_bool M_net_http_simple_send(M_net_http_simple_t *hs, const char *url, void *thunk);

/*! @} */

__END_DECLS

#endif /* __M_NET_HTTP_SIMPLE_H__ */
