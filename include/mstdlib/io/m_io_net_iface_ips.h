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

#ifndef __M_IO_NET_IFACE_IPS_H__
#define __M_IO_NET_IFACE_IPS_H__

#include <mstdlib/mstdlib.h>

__BEGIN_DECLS

/*! \addtogroup m_io_net_iface_ips Network Interface Enumeration
 *  \ingroup m_io_net
 *
 * Network Interface Enumeration
 *
 * @{
 *
 */

struct M_io_net_iface_ips;
/* Data structure to hold result of M_io_net_iface_ips() */
typedef struct M_io_net_iface_ips M_io_net_iface_ips_t;

/* Flags that can be passed to M_io_net_iface_ips() or returned from M_io_net_iface_ips_get_flags() */
typedef enum {
    M_NET_IFACE_IPS_FLAG_OFFLINE  = 1 << 0, /*!< Interface is currently offline. For enumeration, by default does
                                             *   not return offline interfaces without this flag. */
    M_NET_IFACE_IPS_FLAG_LOOPBACK = 1 << 1, /*!< Interface is loopback.  For enumeration, by default, does not
                                             *   return addresses for loopback interfaces. */
    M_NET_IFACE_IPS_FLAG_IPV4     = 1 << 2, /*!< Address is ipv4.  For enumeration, only list interfaces and addresses
                                             *   containing ipv4 addresses. */
    M_NET_IFACE_IPS_FLAG_IPV6     = 1 << 3, /*!< Address is ipv6.  For enumeration, only list interfaces and addresses
                                             *   containing ipv6 addresses. */
} M_io_net_iface_ips_flags_t;


/*! Query OS for network interfaces and ip addresses assigned to interfaces.
 *
 * \param[in] flags  M_io_net_iface_ips_flags_t flags or 0 for none
 * \return M_io_net_iface_ips_t * on success or NULL on failure. Must be free'd with
 *         M_io_net_iface_ips_free()
 */
M_API M_io_net_iface_ips_t *M_io_net_iface_ips(int flags);

/*! Return count of all interfaces and ip addresses.  Note that not all
 *  interfaces may contain ip addreses.
 *
 * \param[in] ips  Value returned from M_io_net_iface_ips()
 * \return count
 */
M_API size_t M_io_net_iface_ips_count(M_io_net_iface_ips_t *ips);

/*! Return name of interface associated with specified index.
 *
 * \param[in] ips  Value returned from M_io_net_iface_ips()
 * \param[in] idx  Index to query
 * \return NULL on failure otherwise a pointer to the name of the interface
 */
M_API const char *M_io_net_iface_ips_get_name(M_io_net_iface_ips_t *ips, size_t idx);

/*! Return ip address of interface associated with specified index.
 *
 * \param[in] ips  Value returned from M_io_net_iface_ips()
 * \param[in] idx  Index to query
 * \return NULL on failure (or even possibly if no ip address available for
 *    interface), otherwise a pointer to the name of the interface
 */
M_API const char *M_io_net_iface_ips_get_addr(M_io_net_iface_ips_t *ips, size_t idx);

/*! Return the netmask for the ip address of interface associated with specified index.
 *
 * \param[in] ips  Value returned from M_io_net_iface_ips()
 * \param[in] idx  Index to query
 * \return netmask, only relevant if there is an ip address
 */
M_API M_uint8 M_io_net_iface_ips_get_netmask(M_io_net_iface_ips_t *ips, size_t idx);

/*! Return flags on interface associated with specified index.
 *
 * \param[in] ips  Value returned from M_io_net_iface_ips()
 * \param[in] idx  Index to query
 * \return flags associated with the interface
 */
M_API int M_io_net_iface_ips_get_flags(M_io_net_iface_ips_t *ips, size_t idx);

/*! Retrieve a list of ip addresses from the result set matching the query.
 *  Will only return ip addresses and not any flags or interface names.
 *
 * \param[in] ips   Value returned from M_io_net_iface_ips()
 * \param[in] flags Must specify at least M_NET_IFACE_IPS_FLAG_IPV4 or M_NET_IFACE_IPS_FLAG_IPV6.
 * \param[in] name  Only enumerate for a specific interface name.
 * \return list of ip addresses matching query, NULL on no matches.
 *         Must be free'd with M_list_str_destroy().
 */
M_API M_list_str_t *M_io_net_iface_ips_get_ips(M_io_net_iface_ips_t *ips, int flags, const char *name);

/*! Retrieve a list of interfaces from the result set matching the query.
 *  Will only return interface names and not any flags or ip addresses.
 *
 * \param[in] ips    Value returned from M_io_net_iface_ips()
 * \param[in] flags  If either M_NET_IFACE_IPS_FLAG_IPV4 or M_NET_IFACE_IPS_FLAG_IPV6 is specified, will
 *                   exclude interfaces that don't have the specified address class.
 * \param[in] ipaddr Optional. Use NULL if not wanted. Search for interface containing specified
 *                   IP address.
 * \return list of interface names matching query, NULL on no matches.
 *         Must be free'd with M_list_str_destroy().
 */
M_API M_list_str_t *M_io_net_iface_ips_get_names(M_io_net_iface_ips_t *ips, int flags, const char *ipaddr);

/*! Free the ip address returned from M_io_net_iface_ips().
 *
 * \param[in] ips   Value returned from M_io_net_iface_ips()
 */
M_API void M_io_net_iface_ips_free(M_io_net_iface_ips_t *ips);

/*! Given a set of flags, convert into human-readable form
 *
 * \param[in] flags  Flags to print
 * \return human readable string, must be M_free()'d.
 */
M_API char *M_io_net_iface_ips_flags_to_str(int flags);


/*! @} */

__END_DECLS

#endif /* __M_NET_IFACE_IPS_H__ */
