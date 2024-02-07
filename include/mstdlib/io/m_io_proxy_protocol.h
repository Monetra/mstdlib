/* The MIT License (MIT)
 *
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

#ifndef __M_IO_PROXY_PROTOCOL_H__
#define __M_IO_PROXY_PROTOCOL_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_io_proxy_protocol Proxy Protocol
 *  \ingroup m_eventio_base_addon
 *
 * # Overview
 *
 * Inbound or outbound connection layer for handling The PROXY protocol
 * as defined by HAProxy.
 *
 * Supports versions:
 * - 1
 * - 2
 *
 * Source is the client connecting to the system (Client). Destination is the
 * server accepting the connection which will then relay using proxy protocol
 * (proxy server). There can be multiple proxies in a chain between the source
 * and the final server that is going to process the data. A such the destination
 * address may not be the connection address for the final server's connection.
 *
 * # Examples
 *
 * ## Proxy Server
 *
 * This server accepts inbound connections, and sends the data to another
 * system using the proxy protocol. The inbound client is not using proxy protocol.
 * The server the proxy is relaying the data to is using proxy protocol.
 *
 * `client <-> proxy server example (sends proxy protocol) <-> final server (receives proxy protocol) `
 *
 * \code{.c}
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_io.h>
 *
 * typedef struct {
 *     M_buf_t   *source_to_dest_buf;
 *     M_buf_t   *dest_to_source_buf;
 *     M_io_t    *source_io;
 *     M_io_t    *dest_io;
 * } ldata_t;
 *
 * static M_dns_t *dns;
 *
 * static void destination_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     ldata_t *ldata      = thunk;
 *     char     error[256] = { 0 };
 *     M_bool   clean      = M_FALSE;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CONNECTED TO SERVER: %s%s%s:%d\n",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io));
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             M_io_read_into_buf(io, ldata->dest_to_source_buf);
 *             if (M_buf_len(ldata->dest_to_source_buf) > 0) {
 *                 M_io_write_from_buf(ldata->source_io, ldata->dest_to_source_buf);
 *             }
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             if (M_buf_len(ldata->source_to_dest_buf) > 0) {
 *                 M_io_write_from_buf(io, ldata->source_to_dest_buf);
 *             }
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_OTHER:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED) {
 *                 clean = M_TRUE;
 *             } else {
 *                 M_io_get_error_string(io, error, sizeof(error));
 *             }
 *             M_printf("SERVER %s: %s%s%s:%d (%s%s%s)\n",
 *                     etype == M_EVENT_TYPE_DISCONNECTED ? "DISCONNECTED" : "ABORT",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io),
 *                     clean?"clean":"unclean", clean?"":" - ", clean?"":error);
 *
 *             M_io_disconnect(ldata->source_io);
 *             break;
 *     }
 * }
 *
 * static void source_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_io_t                   *io_out     = NULL;
 *     ldata_t                  *ldata      = thunk;
 *     char                      error[256] = { 0 };
 *     M_bool                    clean      = M_FALSE;
 *     M_io_error_t              ioerr;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CLIENT CONNECTED: %s%s%s:%d\n",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io));
 *
 *             // Create a connetion to the destination echo server.
 *             ioerr = M_io_net_client_create(&io_out, dns, "localhost", 8999, M_IO_NET_ANY);
 *             if (ioerr != M_IO_ERROR_SUCCESS) {
 *                 M_printf("Could not create client: %s\n", M_io_error_string(ioerr));
 *                 M_io_destroy(io);
 *             }
 *             // Add the proxy protocol to the destination connection so the
 *              // source information will be relayed to the echo server.
 *             M_io_proxy_protocol_outbound_add(io_out, NULL, M_IO_PROXY_PROTOCOL_FLAG_NONE);
 *             M_io_proxy_protocol_set_source_endpoints(io_out, M_io_net_get_ipaddr(io), M_io_net_get_server_ipaddr(io), M_io_net_get_ephemeral_port(io), M_io_net_get_port(io));
 *
 *             // Store the echo server's io object so we can communicate with it.
 *             ldata->dest_io = io_out;
 *
 *             M_event_add(el, io_out, destination_cb, ldata);
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             M_io_read_into_buf(io, ldata->source_to_dest_buf);
 *             if (M_buf_len(ldata->source_to_dest_buf) > 0) {
 *                 M_io_write_from_buf(ldata->dest_io, ldata->source_to_dest_buf);
 *             }
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             if (M_buf_len(ldata->dest_to_source_buf) > 0) {
 *                 M_io_write_from_buf(io, ldata->dest_to_source_buf);
 *             }
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_OTHER:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED) {
 *                 clean = M_TRUE;
 *             } else {
 *                 M_io_get_error_string(io, error, sizeof(error));
 *             }
 *             M_printf("CLIENT %s: %s%s%s:%d (%s%s%s)\n",
 *                     etype == M_EVENT_TYPE_DISCONNECTED ? "DISCONNECTED" : "ABORT",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io),
 *                     clean?"clean":"unclean", clean?"":" - ", clean?"":error);
 *
 *             M_io_destroy(ldata->dest_io);
 *             M_io_destroy(io);
 *             M_buf_cancel(ldata->source_to_dest_buf);
 *             M_buf_cancel(ldata->dest_to_source_buf);
 *             M_free(ldata);
 *             break;
 *     }
 * }
 *
 * static void source_listen_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_io_t       *io_out     = NULL;
 *     ldata_t      *ldata;
 *     M_io_error_t  ioerr;
 *     char          error[256] = { 0 };
 *
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_ACCEPT:
 *             // Accept connection form source and create an io object to communicate with it.
 *             ioerr = M_io_accept(&io_out, io);
 *             if (ioerr == M_IO_ERROR_WOULDBLOCK) {
 *                 return;
 *             } else if (ioerr != M_IO_ERROR_SUCCESS || io_out == NULL) {
 *                 M_io_get_error_string(io, error, sizeof(error));
 *                 M_printf("ACCEPT FAILURE: %s\n", error);
 *                 M_io_destroy(io_out);
 *             }
 *
 *             ldata                     = M_malloc_zero(sizeof(*ldata));
 *             ldata->source_to_dest_buf = M_buf_create();
 *             ldata->dest_to_source_buf = M_buf_create();
 *             ldata->source_io          = io_out;
 *
 *             M_event_add(el, io_out, source_cb, ldata);
 *             break;
 *         case M_EVENT_TYPE_CONNECTED:
 *         case M_EVENT_TYPE_READ:
 *         case M_EVENT_TYPE_WRITE:
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_OTHER:
 *             M_io_destroy(io);
 *             break;
 *     }
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_event_t    *el;
 *     M_io_t       *io_source = NULL;
 *     M_io_error_t  ioerr;
 *
 *     dns = M_dns_create();
 *
 *     // Setup our listening server which will listen for source connections.
 *     ioerr = M_io_net_server_create(&io_source, 8998, NULL, M_IO_NET_ANY);
 *     if (ioerr != M_IO_ERROR_SUCCESS) {
 *         M_printf("Could not start server: %s\n", M_io_error_string(ioerr));
 *         return 0;
 *     }
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     M_event_add(el, io_source, source_listen_cb, NULL);
 *     M_event_loop(el, M_TIMEOUT_INF);
 *
 *     M_event_destroy(el);
 *     M_dns_destroy(dns);
 *     return 0;
 * }
 * \endcode
 *
 * ## Echo Server (accepting proxy protocol)
 *
 * This is a basic echo server where any data received is echoed back
 * out. The server only accepts connections that use proxy protocol.
 *
 * \code{.c}
 * #include <mstdlib/mstdlib.h>
 * #include <mstdlib/mstdlib_io.h>
 *
 * // Echo server states.
 * typedef enum {
 *     STATE_CHECK = 1,
 *     STATE_ECHO,
 *     STATE_EXIT
 * } states_t;
 *
 * typedef struct {
 *     M_buf_t           *write_buf;
 *     M_parser_t        *read_parser;
 *     M_io_t            *io;
 *     M_event_t         *el;
 *     M_state_machine_t *sm;
 * } ldata_t;
 *
 * static void do_trace(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
 * {
 *     char *out;
 *
 *     switch (type) {
 *         case M_IO_TRACE_TYPE_READ:
 *             M_printf("READ:\n");
 *             break;
 *         case M_IO_TRACE_TYPE_WRITE:
 *             M_printf("WRITE:\n");
 *             break;
 *         default:
 *             return;
 *     }
 *     out = M_str_hexdump(M_STR_HEXDUMP_HEADER, 0, "\t", data, data_len);
 *     M_printf("%s\n", out);
 *     M_free(out);
 * }
 *
 * static M_state_machine_status_t state_check(void *data, M_uint64 *next)
 * {
 *     ldata_t *ldata = data;
 *
 *     (void)next;
 *
 *     // Check for new line which indicates full message to echo.
 *     M_parser_mark(ldata->read_parser);
 *     if (M_parser_len(ldata->read_parser) == 0 || M_parser_consume_until(ldata->read_parser, (const unsigned char *)"\n", 1, M_TRUE) == 0) {
 *         M_parser_mark_rewind(ldata->read_parser);
 *         return M_STATE_MACHINE_STATUS_WAIT;
 *     }
 *
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 *
 * static M_state_machine_status_t state_echo(void *data, M_uint64 *next)
 * {
 *     ldata_t *ldata = data;
 *     char    *out;
 *
 *     // Echo the data.
 *     out = M_parser_read_strdup_mark(ldata->read_parser);
 *     M_buf_add_str(ldata->write_buf, out);
 *     M_io_write_from_buf(ldata->io, ldata->write_buf);
 *
 *     // Check for exit command.
 *     if (!M_str_eq(out, "EXIT\r\n") && !M_str_eq(out, "EXIT\n"))
 *         *next = STATE_CHECK;
 *
 *     M_free(out);
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 *
 * static M_state_machine_status_t state_exit(void *data, M_uint64 *next)
 * {
 *     ldata_t *ldata = data;
 *     (void)next;
 *     // Exit the server.
 *     M_event_done_with_disconnect(ldata->el, 0, 1000);
 *     return M_STATE_MACHINE_STATUS_NEXT;
 * }
 *
 * static void connection_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     ldata_t                  *ldata      = thunk;
 *     char                      error[256] = { 0 };
 *     M_bool                    clean      = M_FALSE;
 *     M_state_machine_status_t  status;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_CONNECTED:
 *             M_printf("CLIENT CONNECTED: %s%s%s:%d\n",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io));
 *             M_printf("SERVER IP: %s%s%s\n",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_server_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"");
 *             M_printf("PROXYED SOURCE: %s%s%s:%d\n",
 *                     M_io_proxy_protocol_proxied_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_proxy_protocol_source_ipaddr(io),
 *                     M_io_proxy_protocol_proxied_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_proxy_protocol_source_port(io));
 *             M_printf("PROXYED DEST: %s%s%s:%d\n",
 *                     M_io_proxy_protocol_proxied_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_proxy_protocol_dest_ipaddr(io),
 *                     M_io_proxy_protocol_proxied_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_proxy_protocol_dest_port(io));
 *             break;
 *         case M_EVENT_TYPE_READ:
 *             if (M_io_read_into_parser(io, ldata->read_parser) == M_IO_ERROR_SUCCESS) {
 *                 status = M_state_machine_run(ldata->sm, ldata);
 *                 if (status != M_STATE_MACHINE_STATUS_WAIT) {
 *                     M_io_disconnect(io);
 *                 }
 *             }
 *             break;
 *         case M_EVENT_TYPE_WRITE:
 *             if (M_buf_len(ldata->write_buf) > 0) {
 *                 M_io_write_from_buf(io, ldata->write_buf);
 *             }
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ACCEPT:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_OTHER:
 *             if (etype == M_EVENT_TYPE_DISCONNECTED) {
 *                 clean = M_TRUE;
 *             } else {
 *                 M_io_get_error_string(io, error, sizeof(error));
 *             }
 *             M_printf("CLIENT %s: %s%s%s:%d (%s%s%s)\n",
 *                     etype == M_EVENT_TYPE_DISCONNECTED ? "DISCONNECTED" : "ABORT",
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                     M_io_net_get_ipaddr(io),
 *                     M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                     M_io_net_get_port(io),
 *                     clean?"clean":"unclean", clean?"":" - ", clean?"":error);
 *
 *             M_io_destroy(io);
 *             M_state_machine_destroy(ldata->sm);
 *             M_buf_cancel(ldata->write_buf);
 *             M_parser_destroy(ldata->read_parser);
 *             M_free(ldata);
 *             break;
 *     }
 * }
 *
 * static void listen_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 * {
 *     M_io_t       *io_out     = NULL;
 *     ldata_t      *ldata;
 *     M_io_error_t  ioerr;
 *     char          error[256] = { 0 };
 *
 *     (void)thunk;
 *
 *     switch (etype) {
 *         case M_EVENT_TYPE_ACCEPT:
 *             ioerr = M_io_accept(&io_out, io);
 *             if (ioerr == M_IO_ERROR_WOULDBLOCK) {
 *                 return;
 *             } else if (ioerr != M_IO_ERROR_SUCCESS || io_out == NULL) {
 *                 M_io_get_error_string(io, error, sizeof(error));
 *                 M_printf("ACCEPT FAILURE: %s\n", error);
 *                 M_io_destroy(io_out);
 *                 break;
 *             }
 *
 *             // If tracing, adding before the proxy protocol will output the proxy
 *             // protocol in the trace data. Putting after will not show the proxy
 *             // protocol data as it would have been eaten by the proxy protocol
 *             // layer, prior to being sent to the trace layer.
 *             //M_io_add_trace(io_out, NULL, do_trace, NULL, NULL, NULL);
 *
 *             ldata = M_malloc_zero(sizeof(*ldata));
 *             ldata->el          = el;
 *             ldata->write_buf   = M_buf_create();
 *             ldata->read_parser = M_parser_create(M_PARSER_FLAG_NONE);
 *             ldata->io          = io_out;
 *             ldata->sm          = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);
 *             M_state_machine_insert_state(ldata->sm, STATE_CHECK, 0, NULL, state_check, NULL, NULL);
 *             M_state_machine_insert_state(ldata->sm, STATE_ECHO, 0, NULL, state_echo, NULL, NULL);
 *             M_state_machine_insert_state(ldata->sm, STATE_EXIT, 0, NULL, state_exit, NULL, NULL);
 *
 *             M_event_add(el, io_out, connection_cb, ldata);
 *             break;
 *         case M_EVENT_TYPE_CONNECTED:
 *         case M_EVENT_TYPE_READ:
 *         case M_EVENT_TYPE_WRITE:
 *             break;
 *         case M_EVENT_TYPE_DISCONNECTED:
 *         case M_EVENT_TYPE_ERROR:
 *         case M_EVENT_TYPE_OTHER:
 *             M_io_destroy(io);
 *             break;
 *     }
 * }
 *
 * int main(int argc, char **argv)
 * {
 *     M_event_t    *el;
 *     M_io_t       *io = NULL;
 *     M_io_error_t  ioerr;
 *
 *     ioerr = M_io_net_server_create(&io, 8999, NULL, M_IO_NET_ANY);
 *     if (ioerr != M_IO_ERROR_SUCCESS) {
 *         M_printf("Could not start server: %s\n", M_io_error_string(ioerr));
 *         return 0;
 *     }
 *
 *     ioerr = M_io_proxy_protocol_inbound_add(io_out, NULL, M_IO_PROXY_PROTOCOL_FLAG_NONE);
 *     if (ioerr != M_IO_ERROR_SUCCESS) {
 *         M_printf("Could not add proxy protocol layer: %s\n", M_io_error_string(ioerr));
 *         M_io_destroy(io);
 *         return 0;
 *     }
 *
 *     el = M_event_create(M_EVENT_FLAG_NONE);
 *
 *     M_event_add(el, io, listen_cb, NULL);
 *     M_event_loop(el, M_TIMEOUT_INF);
 *
 *     M_event_destroy(el);
 *     return 0;
 * }
 * \endcode
 *
 * ## Using Proxy Server and Echo Server Examples
 *
 * 1. Compile the proxy server
 * 2. Compile the echo server
 * 3. Start proxy server
 * 4. Start echo server
 * 5. Use something like telnet to connect to the proxy server
 * 6. You should see:
 *    - Proxy server shows connection from telnet
 *    - Proxy server shows it connected to echo server
 *    - Echo server shows a connection from proxy server
 *    - Echo server shows proxied information for the _first_ (in cases where the chain
 *      has been expanded to include multiple) proxy server(s) and the telnet client.
 *
 * @{
 */


/*! Flags controlling behavior. */
typedef enum {
    M_IO_PROXY_PROTOCOL_FLAG_NONE = 0,      /*!< Default operation. Support both V1 and V2 in inbound configuration.
                                                 Send V2 in client configuration. */
    M_IO_PROXY_PROTOCOL_FLAG_V1   = 1 << 0, /*!< Only allow V1 connections for inbound configuration. Receiving V2
                                                 is an error condition. Send V1 format for outbound connections.
                                                 Specifying with V2 flag negates this flag operation. */
    M_IO_PROXY_PROTOCOL_FLAG_V2   = 1 << 1  /*!< Only allow V2 connections for inbound configuration. Receiving V1
                                                 is an error condition. Send V2 format for outbound connections.
                                                 Specifying with V1 flag negates this flag operation. */
} M_io_proxy_protocol_flags_t;


/*! Add an inbound handler for proxy protocol connections.
 *
 * The system will look for the PROXY protocol data upon connect.
 * If Proxy protocol data is not present this is considered an error
 * condition per the proxy protocol spec. An error event will be
 * generated instead of a connect event in this situation.
 *
 * This should be added to an `io` object created by `M_io_accept`
 * during a server `M_EVENT_TYPE_ACCEPT` event. It should not be
 * added to the server `io` object created by `M_io_net_server_create`.
 *
 * The proxy protocol data will be parsed and accessible
 * though the relevant helper functions.
 *
 * \param[in]  io       io object.
 * \param[out] layer_id Layer id this is added at.
 * \param[in]  flags    M_io_proxy_protocol_flags_t flags.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_proxy_protocol_inbound_add(M_io_t *io, size_t *layer_id, M_uint32 flags);


/*! Add an outbound handler for proxy protocol connections.
 *
 * Information about the proxyed endpoints (source and destination) need to
 * be set before the connect event. If endpoints are not set the connection
 * is assumed to be local where any data is being sent by the proxy itself
 * and not being relayed on behalf of another client.
 *
 * \param[in]  io       io object.
 * \param[out] layer_id Layer id this is added at.
 * \param[in]  flags    M_io_proxy_protocol_flags_t flags.
 *
 * \return Result.
 *
 * \see M_io_proxy_protocol_set_source_endpoints
 */
M_API M_io_error_t M_io_proxy_protocol_outbound_add(M_io_t *io, size_t *layer_id, M_uint32 flags);


/*! Whether data is being is being relayed via a proxy.
 *
 * A connection is relayed when the data is being sent on behalf of another
 * system (proxied). When it is not relayed it is a local connection that has
 * been established by the proxy for the proxy's own communication with the system.
 * Typically, this is used for health checking.
 *
 * return M_TRUE if relayed. Otherwise, M_FALSE.
 */
M_API M_bool M_io_proxy_protocol_relayed(M_io_t *io);


/*! Source IP address.
 *
 * IP address of the client that connected to the proxy.
 *
 * \param[in] io io object.
 *
 * \return IP address as string.
 */
M_API const char *M_io_proxy_protocol_source_ipaddr(M_io_t *io);


/*! Destination IP address.
 *
 * IP address of the proxy server that is relaying the client's (source) data.
 *
 * \param[in] io io object.
 *
 * \return IP address as string.
 */
M_API const char *M_io_proxy_protocol_dest_ipaddr(M_io_t *io);


/*! Source port.
 *
 * Ephemeral port the client is connecting out on.
 *
 * \param[in] io io object.
 *
 * \return Port
 */
M_API M_uint16 M_io_proxy_protocol_source_port(M_io_t *io);


/*! Destination port.
 *
 * Destination port the client is connecting to.
 *
 * \param[in] io io object.
 *
 * \return Port
 */
M_API M_uint16 M_io_proxy_protocol_dest_port(M_io_t *io);


/*! Connection type that was used between source and destination.
 *
 * \param[in] io io object.
 *
 * \return Type
 */
M_API M_io_net_type_t M_io_proxy_protocol_proxied_type(M_io_t *io);


/*! Get the IP address of the client falling back to the network connection.
 *
 * When using proxy protocol this should be used instead of `M_io_net_get_ipaddr`
 * in most instances. This can be used even when proxy protocol is not in use.
 * This is especially useful when using an internal IP based blacklist for denying
 * connections to a client as part of an intrusion prevention system (IPS).
 *
 * This function is the equivalent of checking `M_io_proxy_protocol_relayed`
 * and then calling either `M_io_proxy_protocol_source_ipaddr` or `M_io_net_get_ipaddr`
 * based on whether the connection is relayed.
 *
 * This is a conscience especially for instances where proxy protocol could be used.
 * For example, a configuration option or when some but not all connections will
 * use the protocol. This function allows for use in both scenerios and will always
 * return the correct IP address for the client, whether proxied for not.
 *
 * param[in] io io object.
 *
 * \return String
 */
M_API const char *M_io_proxy_protocol_get_ipaddr(M_io_t *io);


/*! Set connect timeout.
 *
 * This is the timeout to wait for a connection to receive
 * all proxy protocol data. This timeout applies after the net
 * connect timeout.
 *
 * Proxy protocol is designed for all data to fit within a
 * single TCP frame. Meaning, the data should not buffer between
 * multiple events. As such the default timeout is 500 ms. This
 * function can be used to increase that timeout for obscenely slow
 * connections.
 *
 * Connect timeout applies to both inbound and outbound (receiving
 * and writing), the proxy data.
 *
 * param[in] io         io object.
 * param[in] timeout_ms Timeout in milliseconds.
 *
 * \return M_TRUE when proxy protocol in use. Otherwise, M_FALSE.
 */
M_API M_bool M_io_proxy_protocol_set_connect_timeout_ms(M_io_t *io, M_uint64 timeout_ms);


/*! Source and destination information that will be sent on connect.
 *
 * Only applies to outbound connections.
 *
 * The source and destination IP address must be the same address family (IPv4/IPv6).
 * If IP addresses are `NULL` the connection is assumed to be local (not proxied data).
 *
 * This can be called multiple times setting or clearing proxy client information. However,
 * the information is only sent on connect. Multiple inbound connections cannot be multiplexed
 * on the same outbound connection. If changing endpoint information the outbound connection
 * needs to disconnect first.
 *
 * This should be called using an inbound network connection to determine the connection information.
 *
 * \code{.c}
 * M_io_proxy_protocol_set_source_endpoints(io_out,
 *     M_io_net_get_ipaddr(io_in),
 *     M_io_net_get_server_ipaddr(io_in),
 *     M_io_net_get_ephemeral_port(io_in),
 *     M_io_net_get_port(io_in));
 * \endcode
 *
 * \param[in] io            io object.
 * \param[in] source_ipaddr Source ipaddress
 * \param[in] dest_ipaddr   Destination ipaddress
 * \param[in] source_port   Source port
 * \param[in] dest_port     Destination port
 *
 */
M_API M_bool M_io_proxy_protocol_set_source_endpoints(M_io_t *io, const char *source_ipaddr, const char *dest_ipaddr, M_uint16 source_port, M_uint16 dest_port);

/*! @} */

__END_DECLS

#endif
