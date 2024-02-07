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

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_net_http_simple HTTP Simple Net
 *  \ingroup m_net
 *
 * Simple HTTP network interface
 *
 * Allows for sending a request to a remote system and
 * receiving a response. The response will be delivered
 * as an M_http_simple_read_t via a callback.
 *
 * Redirects, and TLS upgrade/downgrades are handled
 * internally by the module.
 *
 * Redirects have a default limit of 16 but this can
 * be changed. It is imperative that the limit never
 * be disabled or be set excessively large. Loop tracking
 * is not supported and exiting redirect loops is handled
 * by the redirect limit.
 *
 * Since this buffers data in memory the maximum received
 * data size can be configured to prevent running out
 * of memory. The default if not set is 50 MB.
 *
 * By default there is no timeout waiting for the
 * operation to complete. It will wait indefinitely
 * unless timeouts are explicitly set.
 *
 * Each instance of an M_net_http_simple_t can only be
 * used once. Upon completion or cancel the object is internally
 * destroyed and all references are invalidated.
 *
 * Example:
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
 *         M_printf("Net Error: %s: %s\n", M_net_errcode_to_str(net_error), error);
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
 *     dns = M_dns_create(el);
 *
 *     ctx = M_tls_clientctx_create();
 *     M_tls_clientctx_set_default_trust(ctx);
 *     //M_tls_clientctx_set_verify_level(ctx, M_TLS_VERIFY_NONE);
 *
 *     hs   = M_net_http_simple_create(el, dns, done_cb);
 *     M_net_http_simple_set_timeouts(hs, 2000, 0, 0);
 *     M_net_http_simple_set_tlsctx(hs, ctx);
 *     M_net_http_simple_set_message(hs, M_HTTP_METHOD_GET, NULL, "text/plain", "utf-8", NULL, NULL, 0);
 *
 *     M_net_http_simple_set_iocreate(hs, iocreate_cb);
 *
 *     if (M_net_http_simple_send(hs, "http://google.com/", NULL)) {
 *         M_event_loop(el, M_TIMEOUT_INF);
 *     } else {
 *         M_net_http_simple_cancel(hs);
 *         M_printf("Send failed\n");
 *     }
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

/*! Done callback called when the request has completed.
 *
 * Once this callback returns the M_net_http_simple_t object that called this
 * callback is destroyed internally. All external references are thus
 * invalidated.
 *
 * \param[in] net_error  Indicates where there was a network problem of some type or
 *                       if the network operation succeeded. If set to anything other
 *                       than M_NET_ERROR_SUCCESS http_error and simple should not
 *                       be vaulted because HTTP data was never received and parsed.
 * \param[in] http_error Status of the HTTP response data parse.
 * \param[in] simple     The parsed HTTP data object. Will only
 *                       be non-NULL when net_error is M_NET_ERROR_SUCCESS and
 *                       http_error is M_HTTP_ERROR_SUCCESS.
 * \param[in] error      Textual error message when either net_error or http_error indicate
 *                       an error condition.
 * \param[in] thunk      Thunk parameter provided to send call.
 */
typedef void (*M_net_http_simple_done_cb)(M_net_error_t net_error, M_http_error_t http_error, const M_http_simple_read_t *simple, const char *error, void *thunk);


/*! Callback to set additional I/O layers on the internal network request I/O object.
 *
 * The primary use for this callback is to add tracing or bandwidth shaping. TLS
 * should not be added here because it is handled internally.
 *
 * Due to redirects multiple connection to multiple servers may need to be established.
 * The callback may be called multiple times. Once for each I/O object created to
 * establish a connection with a given server.
 *
 * \param[in] io     The base I/O object to add layers on top of.
 * \param[in] error  Error buffer to set a textual error message when returning a failure response.
 * \param[in] errlen Size of error buffer.
 * \param[in] thunk  Thunk parameter provided to send call.
 *
 * \return M_TRUE on success. M_FALSE if setting up the I/O object failed and the operation should abort.
 */
typedef M_bool (*M_net_http_simple_iocreate_cb)(M_io_t *io, char *error, size_t errlen, void *thunk);


/*! Create an HTTP simple network object.
 *
 * \param[in] el      Event loop to operate on.
 * \param[in] dns     DNS object. Must be valid for the duration of this object's life.
 * \param[in] done_cb Callback that's called on completion of the request.
 *
 * \return HTTP network object on success. Otherwise NULL on error.
 */
M_API M_net_http_simple_t *M_net_http_simple_create(M_event_t *el, M_dns_t *dns, M_net_http_simple_done_cb done_cb);


/*! Cancel the operation that's in progress.
 *
 * The hs object is invalided by this call.
 * The registered done callback will not be called.
 *
 * Can be used to cancel an operation that has not yet been sent
 * in order to destroy the hs object.
 *
 * \param[in] hs HTTP simple network object.
 */
M_API void M_net_http_simple_cancel(M_net_http_simple_t *hs);

/*! Set proxy server authentication.
 *
 * \param[in] hs           HTTP simple network object.
 * \param[in] user         For use in basic credential user:pass
 * \param[in] pass         For use in basic credential user:pass
 *
 */
M_API void M_net_http_simple_set_proxy_authentication(M_net_http_simple_t *hs, const char *user, const char *pass);

/*! Set proxy server.
 *
 * \param[in] hs           HTTP simple network object.
 * \param[in] proxy_server URL to proxy request through.
 *
 */
M_API void M_net_http_simple_set_proxy(M_net_http_simple_t *hs, const char *proxy_server);


/*! Set operation timeouts.
 *
 * On timeout the operation will abort.
 *
 * No timeouts are set by default. Set to 0 to disable a timeout.
 *
 * \param[in] hs         HTTP simple network object.
 * \param[in] connect_ms Connect timeout in milliseconds. Will trigger when a connection
 *                       has not been established within this time.
 * \param[in] stall_ms   Stall timeout in milliseconds. Will trigger when the time between read
 *                       and write events has been exceeded. This helps prevent a server from causing
 *                       a denial of service by sending 1 byte at a time with a large internal between
 *                       each one.
 * \param[in] overall_ms Overall time the operation can take in milliseconds. When exceeded the operation
 *                       will abort.
 */
M_API void M_net_http_simple_set_timeouts(M_net_http_simple_t *hs, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 overall_ms);


/*! Set the maximum number of allowed redirects
 *
 * Default 16 redirects.
 *
 * \param[in] hs  HTTP simple network object.
 * \param[in] max Maximum number of redirects. 0 will disable redirects.
 */
M_API void M_net_http_simple_set_max_redirects(M_net_http_simple_t *hs, M_uint64 max);


/*! Set max receive data size
 *
 * Default 50 MB.
 *
 * \param[in] hs  HTTP simple network object.
 * \param[in] max Maximum number of bytes that can be received. 0 will disable redirects.
 *                Use the value (1024*1024*50) bytes to set a 50 MB limit.
 */
M_API void M_net_http_simple_set_max_receive_size(M_net_http_simple_t *hs, M_uint64 max);


/*! Set the TLS client context for use with HTTPS connections.
 *
 * It is highly recommend a TLS client context be provided even
 * if the initial connection address is not HTTPS. It is possible
 * for a redirect to imitate a redirect upgrade to a TLS connection.
 *
 * Even if the system is known to not support HTTPS it's possible it
 * will be changed to require it in the future. Providing the client
 * context will prevent connections from failing in the future due
 * to this type of server side change.
 *
 * The context is only applied when necessary.
 *
 * \param[in] hs  HTTP simple network object.
 * \param[in] ctx The TLS client context. The context does not have to persist after being set here.
 */
M_API void M_net_http_simple_set_tlsctx(M_net_http_simple_t *hs, M_tls_clientctx_t *ctx);


/*! Set the I/O create callback.
 *
 * The callback is called when the hs object internally creates an I/O connection with a remote system.
 *
 * \param[in] hs          HTTP simple network object.
 * \param[in] iocreate_cb I/O create callback.
 */
M_API void M_net_http_simple_set_iocreate(M_net_http_simple_t *hs, M_net_http_simple_iocreate_cb iocreate_cb);


/*! Set message data that should be sent with the request.
 *
 * This is optional. If this function is not called M_net_http_simple_send
 * will issue a GET with no data.
 *
 * \param[in] hs           HTTP simple network object.
 * \param[in] method       HTTP method.
 * \param[in] user_agent   User agent to identify the request using. Optional.
 * \param[in] content_type Type of data being sent. Optional if no data is being sent. Or if set in headers.
 * \param[in] charset      Character set of data being sent. Only applies to textual data and should not be
 *                         be set for binary. Optional depending on content type or if included in headers.
 * \param[in] headers      Additional headers to send with the request. Optional.
 * \param[in] message      The data to send. Optional.
 * \param[in] message_len  The length of the data. Required if message is not NULL otherwise should be 0.
 */
M_API void M_net_http_simple_set_message(M_net_http_simple_t *hs, M_http_method_t method, const char *user_agent, const char *content_type, const char *charset, const M_hash_dict_t *headers, const unsigned char *message, size_t message_len);


/*! Start sending the request async.
 *
 * On success, the `hs` object is freed internally once the send completes
 * and the done callback is called. It **must** not be reused.
 *
 * On failure of this call it can be called again if the error can be corrected.
 * Otherwise if this fails M_net_http_simple_cancel needs to be called to
 * cleanup the `hs` object.
 *
 * \param[in] hs    HTTP simple network object.
 * \param[in] url   The **full** URL to send the request to. Must include http:// or https://.
 * \param[in] thunk Thunk parameter that will be passed to the done callback. If allocated, must
 *                  _not_ be freed before the done callback is called. Unless this function returns
 *                  M_FALSE which prevents the done callback from running. For example, allocating a
 *                  thunk and freeing in the done callback. Or freeing when this returns M_FALSE.
 *
 * \return M_TRUE when successfully starting the send process. `hs` will be freed internally.
 *         Otherwise, M_FALSE if sending could not commence. Will not call the done callback when M_FALSE.
 *         If not attempting again, allocated memory needs to be freed. An allocated `thunk` should be freed
 *         if necessary (would have been freed in the done callback). M_net_http_simple_cancel should be
 *         called on the `hs` object
 */
M_API M_bool M_net_http_simple_send(M_net_http_simple_t *hs, const char *url, void *thunk) M_WARN_UNUSED_RESULT;

/*! @} */

__END_DECLS

#endif /* __M_NET_HTTP_SIMPLE_H__ */
