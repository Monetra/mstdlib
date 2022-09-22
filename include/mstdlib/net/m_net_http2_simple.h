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

#ifndef __M_NET_HTTP2_SIMPLE_H__
#define __M_NET_HTTP2_SIMPLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_net_http2_simple HTTP2 Simple Net
 *  \ingroup m_net
 * 
 * Simple HTTP2 network interface
 *
 * TLS contexts is handled internally by the module.
 * There is no non TLS option.  TLS Application Layer
 * Protocol Negotiation configuration.  Disables
 * PUSH_PROMISE frames, and sets dynamic table size to
 * 0.
 *
 * Example:
 *
 * \code{.c}
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_net.h>
 *
 * void response_cb(const char *url_str, M_hash_dict_t *headers, const char *data, size_t data_len, void *thunk)
 * {
 * 	M_event_t *el = thunk;
 * 	M_printf("%s\n", url_str);
 * 	M_printf("%.*s", (int)data_len, data);
 * 	M_event_done(el);
 * }
 *
 * void disconnect_cb(void *thunk)
 * {
 * 	M_event_t *el = thunk;
 * 	M_event_done(el);
 * }
 *
 * int main(int argc, char **argv)
 * {
 * 	struct M_net_http2_simple_callbacks cbs = {
 * 		NULL,
 * 		NULL,
 * 		disconnect_cb,
 * 	};
 *
 * 	M_event_t            *el  = M_event_create(M_EVENT_FLAG_NONE);
 * 	M_dns_t              *dns = M_dns_create(el);
 * 	M_net_http2_simple_t *h2  = M_net_http2_simple_create(el, dns, &cbs, M_TLS_VERIFY_FULL, el);
 *
 * 	M_net_http2_simple_request(h2, "https://nghttp2.org/", response_cb);
 * 	M_event_loop(el, M_TIMEOUT_INF);
 *
 * 	M_net_http2_simple_goaway(h2);
 * 	M_event_loop(el, M_TIMEOUT_INF);
 *
 * 	M_net_http2_simple_destroy(h2);
 * 	M_dns_destroy(dns);
 * 	return 0;
 * }
 * \endcode
 *
 * @{
 *
 */

struct M_net_http2_simple;
typedef struct M_net_http2_simple M_net_http2_simple_t;

typedef void   (*M_net_http2_simple_response_cb)(const char *url_str, M_hash_dict_t *headers, const char *data, size_t data_len, void *thunk);
typedef void   (*M_net_http2_simple_error_cb   )  (M_http_error_t error, const char *errmsg);
typedef M_bool (*M_net_http2_simple_iocreate_cb)  (M_io_t *io, char *error, size_t errlen, void *thunk);
typedef void   (*M_net_http2_simple_disconnect_cb)(void *thunk);

struct M_net_http2_simple_callbacks {
	M_net_http2_simple_iocreate_cb   iocreate_cb;
	M_net_http2_simple_error_cb      error_cb;
	M_net_http2_simple_disconnect_cb disconnect_cb;
};

/*! Create a HTTP2 simple network object.
 *
 * \param[in] el      Event loop to operate on.
 * \param[in] dns     DNS object. Must be valid for the duration of this object's life.
 * \param[in] cbs     Callbacks that are called on completion of the requests.
 * \param[in] level   TLS verify level
 * \param[in] thunk   Thunk to pass to callbacks.
 *
 * \return HTTP network object on success. Otherwise NULL on error.
 */
M_API M_net_http2_simple_t *M_net_http2_simple_create(M_event_t *el, M_dns_t *dns, const struct M_net_http2_simple_callbacks *cbs, M_tls_verify_level_t level, void *thunk);

/*! Destroy a HTTP2 simple network object.
 *
 * \param[in] h2      HTTP2 simple object to destroy.
 *
 * \return HTTP network object on success. Otherwise NULL on error.
 */
M_API void M_net_http2_simple_destroy(M_net_http2_simple_t *h2);

/*! Request URL via HTTP2
 *
 * \param[in] h2          HTTP2 simple object managing session.
 * \param[in] url         the URL to request.
 * \param[in] response_cb callback for response completion.
 *
 * \return TRUE on success
 */
M_API M_bool M_net_http2_simple_request(M_net_http2_simple_t *h2, const char *url, M_net_http2_simple_response_cb response_cb);

/*! Request disconnect via HTTP2 GOAWAY
 *
 * \param[in] h2          HTTP2 simple object managing session.
 *
 * \return TRUE on success
 */
M_API M_bool M_net_http2_simple_goaway(M_net_http2_simple_t *h2);

/*! @} */

__END_DECLS

#endif /* __M_NET_HTTP_SIMPLE_H__ */
