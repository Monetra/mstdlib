/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_DNS_H__
#define __M_DNS_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/io/m_event.h>

__BEGIN_DECLS

/*! \addtogroup m_dns DNS Functions
 *  \ingroup m_io_net
 *
 * It's intended that a single global DNS object will be created to allow for
 * caching. Aiding with caching, Happy eyeballs is used to aid in choosing the
 * best server when DNS resolves multiple addresses.
 *
 * @{
 */

struct M_dns;
typedef struct M_dns M_dns_t;

/*! Create a DNS resolver handle.
 *
 * This resolver handle is responsible for caching DNS results as well as
 * tracking which associated IP addresses resulted in successful or failed
 * connections for optimizing future connection attempts. This DNS handle will
 * be passed into functions that require DNS resolution like
 * M_io_net_client_create().
 *
 *  It is recommended to create a single DNS resolver handle at startup and
 *  pass the same handle to all functions which need it, and destroy the handle
 *  at shutdown.
 *
 *  \return Initialized DNS handle
 */
M_API M_dns_t *M_dns_create(M_event_t *event);


/*! Destroys the memory associated with a DNS handle.
 *
 * DNS uses reference counters, and will delay destruction until after last consumer is destroyed.
 *
 *  \param[in] dns Handle initialized via M_dns_create().
 *  \return M_TRUE on success, M_FALSE if handle is actively being used.
 */
M_API M_bool M_dns_destroy(M_dns_t *dns);


/*! Set the maximum query time before a timeout is returned.
 *
 * In some cases, if a prior result is cached, the query may still return
 * success rather than a timeout failure at the end of a timeout, but the
 * result will be stale. If set to 0, will use the internal default of 5000ms
 * (5s)
 *
 *  \param[in] dns        Initialized DNS object
 *  \param[in] timeout_ms Timeout specified in milliseconds.  If provided as 0,
 *                        will use the default of 5000ms (5s).
 *
 *  \return M_TRUE if set successfully, M_FALSE otherwise
 */
M_API M_bool M_dns_set_query_timeout(M_dns_t *dns, M_uint64 timeout_ms);


/*! Set the maximum amount of time a DNS query can be cached for where results are
 *  served out of the cache rather than querying a remote DNS server.
 *
 *  \param[in] dns           Initialized DNS object
 *  \param[in] max_timeout_s Specify the maximum timeout of the cached results, even if no
 *                           DNS server is reachable.  Stale results can no longer be delivered
 *                           after this timeframe and will result in DNS errors being returned.
 *                           If 0 is specified, will use the default of 3600s (1 hr).
 *
 *  \return M_TRUE if set successfully, M_FALSE otherwise
 */
M_API M_bool M_dns_set_cache_timeout(M_dns_t *dns, M_uint64 max_timeout_s);


/*! RFC 6555/8305 Happy Eyeballs status codes */
enum M_dns_happyeb_status {
    M_HAPPYEB_STATUS_GOOD    = 0, /*!< Successfully connected to server                */
    M_HAPPYEB_STATUS_UNKNOWN = 1, /*!< Don't know, probably not attempted              */
    M_HAPPYEB_STATUS_SLOW    = 2, /*!< Don't know for sure its bad, but we started and
                                         * a different connection finished first             */
    M_HAPPYEB_STATUS_BAD     = 3, /*!< Recieved a connection error                     */
};
typedef enum M_dns_happyeb_status M_dns_happyeb_status_t;


/*! Result codes for DNS queries */
enum M_dns_result {
    M_DNS_RESULT_SUCCESS             = 0, /*!< DNS result successful                      */
    M_DNS_RESULT_SUCCESS_CACHE       = 1, /*!< DNS result successful, returned from cache */
    M_DNS_RESULT_SUCCESS_CACHE_EVICT = 2, /*!< DNS result successful, evicting old cache */
    M_DNS_RESULT_SERVFAIL            = 3, /*!< DNS server failure                         */
    M_DNS_RESULT_NOTFOUND            = 4, /*!< DNS server returned a Not Found error      */
    M_DNS_RESULT_TIMEOUT             = 5, /*!< Timeout resolving DNS name                 */
    M_DNS_RESULT_INVALID             = 6  /*!< Invalid use                                */
};
typedef enum M_dns_result M_dns_result_t;

/*! Returned IP address list is cleaned up immediately after the callback returns, if persistence
 *  is needed, duplicate the list.  The list is returned sorted in preference order:
 *    * List starts as alternating between ipv6 and ipv4 addresses in the order returned from
 *      the DNS server, such as ipv6-1, ipv4-1, ipv6-2, ipv4-2 and so on.
 *    * List then is updated with the happyeyeballs RFC6555/8305 status for prior connection
 *      attempts.
 *    * Finally the list is sorted by happyeyeballs status as the primary sort comparison,
 *      followed by the original order as per server preference.
 *  \param[in] ipaddrs Ip addresses resolved in string form, nor NULL if none
 *  \param[in] cb_data Callback data provided to M_dns_gethostbyname()
 *  \param[in] result  Result code of DNS lookup
 */
typedef void (*M_dns_ghbn_callback_t)(const M_list_str_t *ipaddrs, void *cb_data, M_dns_result_t result);


/*! Request to resolve a DNS hostname to one or more IP addresses.  When the resolution
 *  is complete, the callback passed in will be called.  This function may call the
 *  callback immediately if the DNS result is cached.   Once the supplied callback is
 *  called, the query will be automatically cleaned up.
 *
 *  \param[in] dns      Handle to DNS pointer created with M_dns_create()
 *  \param[in] event    Optional.  Event handle to use to deliver the result callback
 *                      to.  This is useful to ensure the result is enqueued to the
 *                      same event loop as requested which may limit need for
 *                      possible external locking.
 *  \param[in] hostname Hostname to look up.
 *  \param[in] type     Type of lookup to perform (IPv4, IPv6, or both/any)
 *  \param[in] callback Callback to call on completion
 *  \param[in] cb_data  User data handle passed to callback on completion
 */
M_API void M_dns_gethostbyname(M_dns_t *dns, M_event_t *event, const char *hostname, M_io_net_type_t type, M_dns_ghbn_callback_t callback, void *cb_data);

/*! Notify the DNS subsystem of any updates to connection status on a given
 *  IP address.  This will cause future results of M_dns_gethostbyname() to be
 *  sorted based on success or failure of past connections.
 *  \param[in] dns     Handle to DNS pointer created with M_dns_create()
 *  \param[in] ipaddr  String form of IP address
 *  \param[in] status  Status of connection attempt.
 */
M_API void M_dns_happyeyeballs_update(M_dns_t *dns, const char *ipaddr, M_dns_happyeb_status_t status);


/*! Convert the string form ip address of the given address family to its binary form.
 *
 *  \param[in]  af   Address family of AF_INET or AF_INET6
 *  \param[in]  src  Null-terminated string represenation of IP address
 *  \param[out] dst  Buffer of adequate size to hold AF_INET (32bits) or AF_INET6 (128bits) result
 *  \return M_TRUE on success, M_FALSE on conversion failure
 */
M_API M_bool M_dns_pton(int af, const char *src, void *dst);

/*! Convert the binary form of ip address of the given address family to its string form.
 *  \param[in]  af        Address family of AF_INET or AF_INET6
 *  \param[in]  src       Buffer containing binary representation of ip address. 32bits for AF_INET, 128bits for AF_INET6.
 *  \param[out] addr      Destination buffer to write string represenation of ip address.
 *  \param[in]  addr_size Size of destination buffer
 *  \return M_TRUE on success, M_FALSE on conversion failure.
 */
M_API M_bool M_dns_ntop(int af, const void *src, char *addr, size_t addr_size);

/*! @} */

__END_DECLS

#endif
