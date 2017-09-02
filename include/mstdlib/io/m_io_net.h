/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

#ifndef __M_IO_NET_H__
#define __M_IO_NET_H__

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_event.h>
#include <mstdlib/io/m_dns.h>

__BEGIN_DECLS

/*! \addtogroup m_io_net Network I/O
 *  \ingroup m_eventio_base
 * 
 * Network I/O functions
 *
 * Capable of functioning as a network server and client.
 *
 * Examples
 * ========
 *
 * Client
 * ------
 *
 * Example network client which downloads google.com's home page. This uses
 * a network client connection wrapped in TLS. A trace layer is provided and
 * is commented out.
 *
 * \code{.c}
 *     #include <mstdlib/mstdlib.h>
 *     #include <mstdlib/mstdlib_io.h>
 *     #include <mstdlib/mstdlib_tls.h>
 *     
 *     static void do_trace(void *cb_arg, M_io_trace_type_t type, M_event_type_t event_type, const unsigned char *data, size_t data_len)
 *     {
 *         char *out;
 *     
 *         switch (type) {
 *             case M_IO_TRACE_TYPE_READ:
 *                 M_printf("READ:\n");
 *                 break;
 *             case M_IO_TRACE_TYPE_WRITE:
 *                 M_printf("WRITE:\n");
 *                 break;
 *             default:
 *                 return;
 *         }
 *         out = M_str_hexdump(M_STR_HEXDUMP_HEADER, 0, "\t", data, data_len);
 *         M_printf("%s\n", out);
 *         M_free(out);
 *     }
 *     
 *     static void run_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         M_buf_t *connected_buf = thunk;
 *         char     buf[128]      = { 0 };
 *         size_t   len_written   = 0;
 *     
 *         switch (etype) {
 *             case M_EVENT_TYPE_CONNECTED:
 *                 M_printf("CONNECTED: %s%s%s:%d (%s: %s - session%sreused)\n",
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                         M_io_net_get_ipaddr(io),
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                         M_io_net_get_port(io),
 *                         M_tls_protocols_to_str(M_tls_get_protocol(io, M_IO_LAYER_FIND_FIRST_ID)),
 *                         M_tls_get_cipher(io, M_IO_LAYER_FIND_FIRST_ID),
 *                         M_tls_get_sessionreused(io, M_IO_LAYER_FIND_FIRST_ID)?"":" not ");
 *     
 *                 M_io_write_from_buf(io, connected_buf);
 *                 break;
 *             case M_EVENT_TYPE_READ:
 *                 M_io_read(io, (unsigned char *)buf, sizeof(buf), &len_written);
 *                 if (len_written > 0) {
 *                     M_printf("%.*s", (int)len_written, buf);
 *                 }
 *                 break;
 *             case M_EVENT_TYPE_WRITE:
 *                 M_io_write_from_buf(io, connected_buf);
 *                 break;
 *             case M_EVENT_TYPE_DISCONNECTED:
 *             case M_EVENT_TYPE_ERROR:
 *                 M_event_done_with_disconnect(el, 1000);
 *                 break;
 *             case M_EVENT_TYPE_ACCEPT:
 *             case M_EVENT_TYPE_OTHER:
 *                 M_event_done(el);
 *                 break;
 *         }
 *     }
 *     
 *     int main(int argc, char *argv)
 *     {
 *         M_event_t *el;
 *         M_dns_t   *dns;
 *         M_buf_t   *buf;
 *         M_io_t    *io;
 *         M_tls_clientctx_t *ctx;
 *         size_t     layer_id;
 *     
 *     
 *         dns = M_dns_create();
 *         el  = M_event_create(M_EVENT_FLAG_NONE);
 *         buf = M_buf_create();
 *         M_buf_add_str(buf, "GET / HTTP/1.1\r\n");
 *         M_buf_add_str(buf, "Host: www.google.com\r\n");
 *         M_buf_add_str(buf, "Connection: close\r\n");
 *         M_buf_add_str(buf, "\r\n");
 *     
 *         M_io_net_client_create(&io, dns, "google.com", 443, M_IO_NET_ANY);
 *         ctx = M_tls_clientctx_create();
 *         M_tls_clientctx_set_default_trust(ctx);
 *         M_io_tls_client_add(io, ctx, NULL, &layer_id);
 *         M_tls_clientctx_destroy(ctx);
 *         //M_io_add_trace(io, &layer_id, do_trace, NULL, NULL, NULL);
 *     
 *         M_event_add(el, io, run_cb, buf);
 *         M_event_loop(el, M_TIMEOUT_INF);
 *     
 *         M_io_destroy(io);
 *         M_buf_cancel(buf);
 *         M_event_destroy(el);
 *         M_dns_destroy(dns);
 *         return 0;
 *     }
 * \endcode
 *
 * Server
 * ------
 *
 * Example network sever. This is an echo server which uses a state machine
 * to determine what operation it should perform.
 *
 * \code{.c}
 *     #include <mstdlib/mstdlib.h>
 *     #include <mstdlib/mstdlib_io.h>
 *     
 *     typedef enum {
 *         STATE_CHECK = 1,
 *         STATE_ECHO,
 *         STATE_EXIT
 *     } states_t;
 *     
 *     typedef struct {
 *         M_buf_t           *write_buf;
 *         M_parser_t        *read_parser;
 *         M_io_t            *io;
 *         M_event_t         *el;
 *         M_state_machine_t *sm;
 *     } ldata_t;
 *     
 *     static M_state_machine_status_t state_check(void *data, M_uint64 *next)
 *     {
 *         ldata_t *ldata = data;
 *     
 *         (void)next;
 *     
 *         M_parser_mark(ldata->read_parser);
 *         if (M_parser_len(ldata->read_parser) == 0 || M_parser_consume_until(ldata->read_parser, (const unsigned char *)"\n", 1, M_TRUE) == 0) {
 *             M_parser_mark_rewind(ldata->read_parser);
 *             return M_STATE_MACHINE_STATUS_WAIT;
 *         }
 *     
 *         return M_STATE_MACHINE_STATUS_NEXT;
 *     }
 *     
 *     static M_state_machine_status_t state_echo(void *data, M_uint64 *next)
 *     {
 *         ldata_t *ldata = data;
 *         char    *out;
 *     
 *         out = M_parser_read_strdup_mark(ldata->read_parser);
 *         M_buf_add_str(ldata->write_buf, out);
 *         M_io_write_from_buf(ldata->io, ldata->write_buf);
 *     
 *         if (!M_str_eq(out, "EXIT\r\n") && !M_str_eq(out, "EXIT\n"))
 *             *next = STATE_CHECK;
 *     
 *         M_free(out);
 *         return M_STATE_MACHINE_STATUS_NEXT;
 *     }
 *     
 *     static M_state_machine_status_t state_exit(void *data, M_uint64 *next)
 *     {
 *         ldata_t *ldata = data;
 *         (void)next;
 *         M_event_done_with_disconnect(ldata->el, 1000);
 *     }
 *     
 *     static void connection_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         ldata_t                  *ldata      = thunk;
 *         char                      error[256] = { 0 };
 *         M_bool                    clean      = M_FALSE;
 *         M_state_machine_status_t  status;
 *     
 *         switch (etype) {
 *             case M_EVENT_TYPE_CONNECTED:
 *                 M_printf("CLIENT CONNECTED: %s%s%s:%d\n",
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                         M_io_net_get_ipaddr(io),
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                         M_io_net_get_port(io));
 *                 break;
 *             case M_EVENT_TYPE_READ:
 *                 if (M_io_read_into_parser(io, ldata->read_parser) == M_IO_ERROR_SUCCESS) {
 *                     status = M_state_machine_run(ldata->sm, ldata);
 *                     if (status != M_STATE_MACHINE_STATUS_WAIT) {
 *                         M_io_disconnect(io);
 *                     }
 *                 }
 *                 break;
 *             case M_EVENT_TYPE_WRITE:
 *                 if (M_buf_len(ldata->write_buf) > 0) {
 *                     M_io_write_from_buf(io, ldata->write_buf);
 *                 }
 *                 break;
 *             case M_EVENT_TYPE_DISCONNECTED:
 *                 clean = M_TRUE;
 *             case M_EVENT_TYPE_ACCEPT:
 *             case M_EVENT_TYPE_ERROR:
 *             case M_EVENT_TYPE_OTHER:
 *                 if (!clean) {
 *                     M_io_get_error_string(io, error, sizeof(error));
 *                 }
 *                 M_printf("CLIENT DISCONNECTED: %s%s%s:%d (%s%s%s)\n",
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"[":"",
 *                         M_io_net_get_ipaddr(io),
 *                         M_io_net_get_type(io)==M_IO_NET_IPV6?"]":"",
 *                         M_io_net_get_port(io),
 *                         clean?"clean":"unclean", clean?"":" - ", clean?"":error);
 *     
 *                 M_io_destroy(io);
 *                 M_state_machine_destroy(ldata->sm);
 *                 M_buf_cancel(ldata->write_buf);
 *                 M_parser_destroy(ldata->read_parser);
 *                 M_free(ldata);
 *                 break;
 *         }
 *     }
 *     
 *     static void listen_cb(M_event_t *el, M_event_type_t etype, M_io_t *io, void *thunk)
 *     {
 *         M_io_t       *io_out     = NULL;
 *         ldata_t      *ldata;
 *         M_io_error_t  ioerr;
 *         char          error[256] = { 0 };
 *     
 *         (void)thunk;
 *     
 *         switch (etype) {
 *             case M_EVENT_TYPE_ACCEPT:
 *                 ioerr = M_io_accept(&io_out, io);
 *                 if (ioerr == M_IO_ERROR_WOULDBLOCK) {
 *                     return;
 *                 } else if (ioerr != M_IO_ERROR_SUCCESS || io_out == NULL) {
 *                     M_io_get_error_string(io, error, sizeof(error));
 *                     M_printf("ACCEPT FAILURE: %s\n", error);
 *                     M_io_destroy(io_out);
 *                 }
 *     
 *                 ldata = M_malloc_zero(sizeof(*ldata));
 *                 ldata->el          = el;
 *                 ldata->write_buf   = M_buf_create();
 *                 ldata->read_parser = M_parser_create(M_PARSER_FLAG_NONE);
 *                 ldata->io          = io_out;
 *                 ldata->sm          = M_state_machine_create(0, NULL, M_STATE_MACHINE_NONE);
 *                 M_state_machine_insert_state(ldata->sm, STATE_CHECK, 0, NULL, state_check, NULL, NULL);
 *                 M_state_machine_insert_state(ldata->sm, STATE_ECHO, 0, NULL, state_echo, NULL, NULL);
 *                 M_state_machine_insert_state(ldata->sm, STATE_EXIT, 0, NULL, state_exit, NULL, NULL);
 *     
 *                 M_event_add(el, io_out, connection_cb, ldata);
 *                 break;
 *             case M_EVENT_TYPE_CONNECTED:
 *             case M_EVENT_TYPE_READ:
 *             case M_EVENT_TYPE_WRITE:
 *                 break;
 *             case M_EVENT_TYPE_DISCONNECTED:
 *             case M_EVENT_TYPE_ERROR:
 *             case M_EVENT_TYPE_OTHER:
 *                 M_io_destroy(io);
 *                 break;
 *         }
 *     }
 *     
 *     int main(int argc, char *argv)
 *     {
 *         M_event_t    *el;
 *         M_io_t       *io = NULL;
 *         M_io_error_t  ioerr;
 *     
 *     
 *         ioerr = M_io_net_server_create(&io, 8999, NULL, M_IO_NET_IPV4);
 *         if (ioerr != M_IO_ERROR_SUCCESS) {
 *             M_printf("Could not start server: %s\n", M_io_error_string(ioerr));
 *             return 0;
 *         }
 *     
 *         el = M_event_create(M_EVENT_FLAG_NONE);
 *     
 *         M_event_add(el, io, listen_cb, NULL);
 *         M_event_loop(el, M_TIMEOUT_INF);
 *     
 *         M_event_destroy(el);
 *         return 0;
 *     }
 * \endcode
 *
 * @{
 */

/*! IP connection type. */
enum M_io_net_type {
	M_IO_NET_ANY  = 1, /*!< Either ipv4 or ipv6 */
	M_IO_NET_IPV4 = 2, /*!< ipv4 only           */
	M_IO_NET_IPV6 = 3  /*!< ipv6 only           */
};
typedef enum M_io_net_type M_io_net_type_t;

/*! Create a server listener net object.
 *
 * \param[out] io_out  io object for communication.
 * \param[in]  port    Port to listen on.
 * \param[in]  bind_ip NULL to listen on all interfaces, or an explicit ip address to listen on.
 *                     Note that listening on localhost ::1 will be ipv6 only, or localhost 127.0.0.1 will be
 *                     ipv4 only.
 * \param[in]  type    Connection type.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_net_server_create(M_io_t **io_out, unsigned short port, const char *bind_ip, M_io_net_type_t type);


/*! Create a client net object.
 *
 * \param[out] io_out  io object for communication.
 * \param[in]  dns     DNS object for host name lookup. Required. It will be reference counted allowing it to be
 *                     destroyed while still in use by the io object.
 * \param[in]  host    Host to connect to. Can be a host name or ip address.
 * \param[in]  port    Port to connect to.
 * \param[in]  type    Connection type.
 *
 * \return Result.
 */
M_API M_io_error_t M_io_net_client_create(M_io_t **io_out, M_dns_t *dns, const char *host, unsigned short port, M_io_net_type_t type);


/*! Set keep alive.
 *
 * \param[in] io           io object.
 * \param[in] idle_time_s  Idle time in seconds.
 * \param[in] retry_time_s Retry time in seconds.
 * \param[in] retry_cnt    Retry count.
 *
 * \return M_TRUE on success, otherwise M_FALSE on failure.
 */
M_API M_bool M_io_net_set_keepalives(M_io_t *io, M_uint64 idle_time_s, M_uint64 retry_time_s, M_uint64 retry_cnt);


/*! Enable/disable Nagle algorithm.
 *
 * \param[in] io            io object.
 * \param[in] nagle_enabled M_TRUE to enable, M_FALSE to disable.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 *         Will return M_TRUE if state being set is the same as already set.
 *         Meaning, enabling on an io that already has it enabled will return success.
 */
M_API M_bool M_io_net_set_nagle(M_io_t *io, M_bool nagle_enabled);


/*! Set connect timeout.
 *
 * This is the timeout to wait for a connection to finish.
 *
 * \param[in] io         io object.
 * \param[in] timeout_ms Timeout in milliseconds.
 *
 * \return M_TRUE on success, otherwise M_FALSE on error.
 */
M_API M_bool M_io_net_set_connect_timeout_ms(M_io_t *io, M_uint64 timeout_ms);


/*! Get the Fully Qualified Domain Name.
 *
 * \return String.
 */
M_API char *M_io_net_get_fqdn(void);


/*! Get the hostname of the connected endpoint.
 *
 * This may return an IP address for inbound connections, or for outbound connections
 * where an ip address was passed. This will not do a reverse hostname lookup.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_net_get_host(M_io_t *io);


/*! Get the IP address of the connected endpoint.
 *
 * \param[in] io io object.
 *
 * \return String.
 */
M_API const char *M_io_net_get_ipaddr(M_io_t *io);


/*! Get the port of the connected endpoint.
 *
 * \param[in] io io object.
 *
 * \return Port.
 */
M_API unsigned short M_io_net_get_port(M_io_t *io);


/*! Get the ephemeral (dynamic) port of the connected endpoint
 *
 * \param[in] io io object.
 *
 * \return Port.
 */
M_API unsigned short M_io_net_get_ephemeral_port(M_io_t *io);


/*! Get connection type
 *
 * \param[in] io io object.
 *
 * \return Type.
 */
M_API enum M_io_net_type M_io_net_get_type(M_io_t *io);


/*! Amount of time DNS query took
 *
 * \param[in] io io object.
 *
 * \return Time in milliseconds.
 */
M_API M_uint64 M_io_net_time_dns_ms(M_io_t *io);


/*! Amount of time connection establishment took, not including DNS resolution time
 *
 * \param[in] io io object.
 *
 * \return Time in milliseconds.
 */
M_API M_uint64 M_io_net_time_connect_ms(M_io_t *io);


/*! Convert an ip address in string form into its binary network byte order representation.
 *
 *  \param[out] ipaddr_bin      User-supplied buffer of at least 16 bytes to store result.
 *  \param[in]  ipaddr_bin_size Size of user-supplied buffer.
 *  \param[in]  ipaddr_str      IPv4 or IPv6 address in string form.
 *  \param[out] ipaddr_bin_len  Pointer to hold length of ip address in binary form.  Result
 *                              will be 4 or 16 depending on the address type.
 *  \return M_TRUE if conversion was possible, M_FALSE otherwise
 */
M_API M_bool M_io_net_ipaddr_to_bin(unsigned char *ipaddr_bin, size_t ipaddr_bin_size, const char *ipaddr_str, size_t *ipaddr_bin_len);


/*! Convert an ip address in its binary network byte order representation to string form.
 *
 *  \param[out] ipaddr_str      User-supplied buffer of at least 40 bytes to store IPv6
 *                              address, or 16 bytes to store IPv4 address.
 *  \param[in]  ipaddr_str_size Size of user-supplied buffer.
 *  \param[in]  ipaddr_bin      IPv4 or IPv6 address in binary form.
 *  \param[in]  ipaddr_bin_len  Length of ip address in binary form (must be 4 or 16).
 *
 *  \return M_TRUE if conversion was possible, M_FALSE otherwise
 */
M_API M_bool M_io_net_bin_to_ipaddr(char *ipaddr_str, size_t ipaddr_str_size, const unsigned char *ipaddr_bin, size_t ipaddr_bin_len);

/*! @} */

__END_DECLS


#endif
