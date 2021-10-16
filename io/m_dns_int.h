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

#ifndef __M_DNS_INT_H__
#define __M_DNS_INT_H__


/*! Status of a single connection attempt */
enum M_dns_happyeb_status {
	M_HAPPYEB_STATUS_GOOD    = 0, /*!< Successfully connected to server                */
	M_HAPPYEB_STATUS_UNKNOWN = 1, /*!< Don't know, probably not attempted              */
	M_HAPPYEB_STATUS_SLOW    = 2, /*!< Don't know for sure its bad, but we started and
	                                     * a different connection finished first             */
	M_HAPPYEB_STATUS_BAD     = 3, /*!< Recieved a connection error                     */
};
typedef enum M_dns_happyeb_status M_dns_happyeb_status_t;


enum M_dns_result {
	M_DNS_RESULT_SUCCESS       = 0, /*!< DNS result successful                      */
	M_DNS_RESULT_SUCCESS_CACHE = 1, /*!< DNS result successful, returned from cache */
	M_DNS_RESULT_SERVFAIL      = 2, /*!< DNS server failure                         */
	M_DNS_RESULT_NOTFOUND      = 3, /*!< DNS server returned a Not Found error      */
	M_DNS_RESULT_TIMEOUT       = 4, /*!< Timeout resolving DNS name                 */
	M_DNS_RESULT_INVALID       = 5  /*!< Invalid use                                */
};
typedef enum M_dns_result M_dns_result_t;

/*! Returned IP address list is cleaned up immediately after the callback returns, if persistence
 *  is needed, duplicate the list.  The list is returned sorted in preference order:
 *    * List starts as alternating between ipv6 and ipv4 addresses in the order returned from
 *      the DNS server, such as ipv6-1, ipv4-1, ipv6-2, ipv4-2 and so on.
 *    * List then is updated with the happyeyeballs RFC6555 status for prior connection
 *      attempts.
 *    * Finally the list is sorted by happyeyeballs status as the primary sort comparison,
 *      followed by the original order as per server preference.
 */
typedef void (*M_io_dns_callback_t)(const M_list_str_t *ipaddrs, void *cb_data, M_dns_result_t result);

/*! Request to resolve a DNS hostname to one or more IP addresses.  When the resolution
 *  is complete, the callback passed in will be called.  This function may call the
 *  callback immediately if the DNS result is cached.   Once the supplied callback is
 *  called, the query will be automatically cleaned up.  */
void M_dns_gethostbyname(M_dns_t *dns, M_event_t *event, const char *hostname, M_io_net_type_t type, M_io_dns_callback_t callback, void *cb_data);

void M_dns_happyeyeballs_update(M_dns_t *dns, const char *ipaddr, M_dns_happyeb_status_t status);

M_bool M_dns_pton(int af, const char *src, void *dst);
M_bool M_dns_ntop(int af, const void *src, char *addr, size_t addr_size);

#endif
