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
M_API M_dns_t *M_dns_create(void);


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
 *  \param[in] timeout_s     Timeout in seconds of cache before another DNS query attempt will
 *                           be made to refresh the cache.  If 0 is specified, will use the
 *                           default of 300s (5 minutes).
 *  \param[in] max_timeout_s Specify the maximum timeout of the cached results, even if no
 *                           DNS server is reachable.  Stale results can no longer be delivered
 *                           after this timeframe and will result in DNS errors being returned.
 *                           If 0 is specified, will use the default of 3600s (1 hr).
 *
 *  \return M_TRUE if set successfully, M_FALSE otherwise
 */
M_API M_bool M_dns_set_cache_timeout(M_dns_t *dns, M_uint64 timeout_s, M_uint64 max_timeout_s);

/*! @} */

__END_DECLS

#endif
