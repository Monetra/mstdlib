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

#include "m_config.h"

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_net.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <iphlpapi.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <ifaddrs.h>
#  include <sys/ioctl.h>
#  include <net/if.h>
#  include <netinet/in.h>
#endif

typedef struct {
	char                    *name;
	char                    *addr;
	M_net_iface_ips_flags_t  flags;
} M_ipentry_t;

struct M_net_iface_ips {
	M_list_t *ips;
};

static void M_ipentry_free(void *arg)
{
	M_ipentry_t *ipentry = arg;
	if (ipentry == NULL)
		return;

	M_free(ipentry->name);
	M_free(ipentry->addr);
}

/* OS might list an interface first with no address followed by an address, purge the
 * no-address entry */
static void M_ipentry_remove_noaddr(M_net_iface_ips_t *ips, const char *name)
{
	size_t i;

	for (i=0; i<M_list_len(ips->ips); i++) {
		const M_ipentry_t *ipentry = M_list_at(ips->ips, i);

		if (!M_str_caseeq(ipentry->name, name))
			continue;

		if (ipentry->addr != NULL)
			continue;

		M_list_remove_at(ips->ips, i);
	}
}

static void M_ipentry_add(M_net_iface_ips_t *ips, const char *name, const unsigned char *addr, size_t addr_len, M_net_iface_ips_flags_t flags)
{
	M_ipentry_t *ipentry    = M_malloc_zero(sizeof(*ipentry));
	char         ipaddr[40] = { 0 };

	ipentry->name        = M_strdup(name);

	if (addr != NULL) {
		if (M_io_net_bin_to_ipaddr(ipaddr, sizeof(ipaddr), addr, addr_len)) {
			ipentry->addr = M_strdup(ipaddr);
			if (addr_len == 4) {
				flags |= M_NET_IFACE_IPS_FLAG_IPV4;
			} else if (addr_len == 16) {
				flags |= M_NET_IFACE_IPS_FLAG_IPV6;
			}
			M_ipentry_remove_noaddr(ips, name);
		}
	}

	ipentry->flags       = flags;
	M_list_insert(ips->ips, ipentry);
}

#ifdef _WIN32
static M_bool M_net_iface_ips_enumerate(M_net_iface_ips_t *ips, M_net_iface_ips_flags_t flags)
{
	/* TODO: Implement me
	 * https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses
	 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_addresses_lh
	 * - flags IP_ADAPTER_IPV4_ENABLED|IP_ADAPTER_IPV6_ENABLED
	 * - iftype IF_TYPE_SOFTWARE_LOOPBACK
	 * - OperStatus IfOperStatusUp
	 */
	return M_FALSE;
}
#else
static M_bool M_net_iface_ips_enumerate(M_net_iface_ips_t *ips, M_net_iface_ips_flags_t flags)
{
	struct ifaddrs *ifap = NULL;
	struct ifaddrs *ifa  = NULL;

	if (getifaddrs(&ifap) != 0)
		return M_FALSE;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		M_net_iface_ips_flags_t addrflag = 0;
		const void             *addr     = NULL;
		size_t                  addr_len = 0;

		/* User is not enumerating offline interfaces */
		if (!(ifa->ifa_flags & IFF_UP) && !(flags & M_NET_IFACE_IPS_FLAG_OFFLINE))
			continue;

		/* User is not enumerating loopback interfaces */
		if (ifa->ifa_flags & IFF_LOOPBACK && !(flags & M_NET_IFACE_IPS_FLAG_LOOPBACK))
			continue;

		/* User is restricting based on address class */
		if (flags & (M_NET_IFACE_IPS_FLAG_IPV4|M_NET_IFACE_IPS_FLAG_IPV6)) {
			/* No interface family */
			if (ifa->ifa_addr == NULL)
				continue;

			/* user is not enumerating ipv4 */
			if (ifa->ifa_addr->sa_family == AF_INET && !(flags & M_NET_IFACE_IPS_FLAG_IPV4))
				continue;

			/* user is not enumerating ipv6 */
			if (ifa->ifa_addr->sa_family == AF_INET6 && !(flags & M_NET_IFACE_IPS_FLAG_IPV6))
				continue;
		}

		/* Extract pointer to ip address */
		if (ifa->ifa_addr != NULL) {
			if (ifa->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)((void *)ifa->ifa_addr);
				addr     = &sockaddr_in->sin_addr;
				addr_len = 4;
			} if (ifa->ifa_addr->sa_family == AF_INET6) {
				struct sockaddr_in6 *sockaddr_in6 = (struct sockaddr_in6 *)((void *)ifa->ifa_addr);
				addr     = &sockaddr_in6->sin6_addr;
				addr_len = 16;
			}
		}

		/* Set flags */
		if (ifa->ifa_flags & IFF_LOOPBACK)
			addrflag |= M_NET_IFACE_IPS_FLAG_LOOPBACK;
		if (!(ifa->ifa_flags & IFF_UP))
			addrflag |= M_NET_IFACE_IPS_FLAG_OFFLINE;

		M_ipentry_add(ips, ifa->ifa_name, addr, addr_len, addrflag);
	}
	return M_TRUE;
}
#endif


M_net_iface_ips_t *M_net_iface_ips(int flags)
{
	M_net_iface_ips_t *ips = M_malloc_zero(sizeof(*ips));
	struct M_list_callbacks listcb = {
		NULL /* equality */,
		NULL /* duplicate_insert */,
		NULL /* duplicate_copy */,
		M_ipentry_free /* value_free */
	};
	ips->ips = M_list_create(&listcb, M_LIST_NONE);

	if (!M_net_iface_ips_enumerate(ips, (M_net_iface_ips_flags_t)flags) || M_net_iface_ips_count(ips) == 0) {
		M_net_iface_ips_free(ips);
		return NULL;
	}

	return ips;
}

size_t M_net_iface_ips_count(M_net_iface_ips_t *ips)
{
	if (ips == NULL)
		return 0;
	return M_list_len(ips->ips);
}

const char *M_net_iface_ips_get_name(M_net_iface_ips_t *ips, size_t idx)
{
	const M_ipentry_t *entry = NULL;

	if (ips == NULL)
		return NULL;
	if (idx >= M_net_iface_ips_count(ips))
		return NULL;
	entry = M_list_at(ips->ips, idx);
	if (entry == NULL)
		return NULL;
	return entry->name;
}

const char *M_net_iface_ips_get_addr(M_net_iface_ips_t *ips, size_t idx)
{
	const M_ipentry_t *entry = NULL;

	if (ips == NULL)
		return NULL;
	if (idx >= M_net_iface_ips_count(ips))
		return NULL;
	entry = M_list_at(ips->ips, idx);
	if (entry == NULL)
		return NULL;
	return entry->addr;
}


int M_net_iface_ips_get_flags(M_net_iface_ips_t *ips, size_t idx)
{
	const M_ipentry_t *entry = NULL;

	if (ips == NULL)
		return 0;
	if (idx >= M_net_iface_ips_count(ips))
		return 0;
	entry = M_list_at(ips->ips, idx);
	if (entry == NULL)
		return 0;
	return (int)entry->flags;
}


M_list_str_t *M_net_iface_ips_get_ips(M_net_iface_ips_t *ips, int flags, const char *name)
{
	size_t        i;
	M_list_str_t *list = NULL;

	/* We need to have at least IPV4 or IPV6 specified */
	if (!(flags & (M_NET_IFACE_IPS_FLAG_IPV4|M_NET_IFACE_IPS_FLAG_IPV6)))
		return NULL;

	/* We're not marking this as a set as it is very unlikedly to have duplicate
	 * IPs */
	list = M_list_str_create(M_LIST_STR_NONE);

	for (i=0; i<M_net_iface_ips_count(ips); i++) {
		const M_ipentry_t *entry = M_list_at(ips->ips, i);
		if (entry == NULL)
			continue;

		/* Don't enumerate offline interface addresses by default */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_OFFLINE && !(flags & M_NET_IFACE_IPS_FLAG_OFFLINE))
			continue;

		/* Don't enumerate loopback interface addresses by default */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_LOOPBACK && !(flags & M_NET_IFACE_IPS_FLAG_LOOPBACK))
			continue;

		/* User isn't enumerating ipv4 */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_IPV4 && !(flags & M_NET_IFACE_IPS_FLAG_IPV4))
			continue;

		/* User isn't enumerating ipv6 */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_IPV6 && !(flags & M_NET_IFACE_IPS_FLAG_IPV6))
			continue;

		/* User is wanting to enumerate only a single interface */
		if (!M_str_isempty(name) && !M_str_caseeq(name, entry->name))
			continue;

		/* Match! */
		M_list_str_insert(list, entry->addr);
	}

	if (M_list_str_len(list) == 0) {
		M_list_str_destroy(list);
		list = NULL;
	}
	return list;
}


M_list_str_t *M_net_iface_ips_get_names(M_net_iface_ips_t *ips, int flags)
{
	size_t        i;
	M_list_str_t *list = NULL;

	/* If neither IPv6 nor ipv4 were specified, act like both were */
	if (!(flags & (M_NET_IFACE_IPS_FLAG_IPV4|M_NET_IFACE_IPS_FLAG_IPV6)))
		flags |= (M_NET_IFACE_IPS_FLAG_IPV4|M_NET_IFACE_IPS_FLAG_IPV6);

	/* We mark this as a set so if the name already exists, it won't be
	 * output more than once */
	list = M_list_str_create(M_LIST_STR_SET|M_LIST_STR_CASECMP);

	for (i=0; i<M_net_iface_ips_count(ips); i++) {
		const M_ipentry_t *entry = M_list_at(ips->ips, i);
		if (entry == NULL)
			continue;

		/* Don't enumerate offline interface addresses by default */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_OFFLINE && !(flags & M_NET_IFACE_IPS_FLAG_OFFLINE))
			continue;

		/* Don't enumerate loopback interface addresses by default */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_LOOPBACK && !(flags & M_NET_IFACE_IPS_FLAG_LOOPBACK))
			continue;

		/* User isn't enumerating ipv4 */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_IPV4 && !(flags & M_NET_IFACE_IPS_FLAG_IPV4))
			continue;

		/* User isn't enumerating ipv6 */
		if (entry->flags & M_NET_IFACE_IPS_FLAG_IPV6 && !(flags & M_NET_IFACE_IPS_FLAG_IPV6))
			continue;

		/* Match! */
		M_list_str_insert(list, entry->name);
	}

	if (M_list_str_len(list) == 0) {
		M_list_str_destroy(list);
		list = NULL;
	}
	return list;
}

void M_net_iface_ips_free(M_net_iface_ips_t *ips)
{
	if (ips == NULL)
		return;
	M_list_destroy(ips->ips, M_TRUE);
	M_free(ips);
}
