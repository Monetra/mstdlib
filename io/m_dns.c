/* The MIT License (MIT)
 * 
 * Copyright (c) 2021 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/mstdlib_text.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_dns_int.h"
#include "ares.h"
#ifndef _WIN32
#  include <netdb.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "m_io_int.h"

typedef struct {
	M_time_t     load_ts;
	ares_channel channel;
	size_t       queries_pending;
	M_bool       destroy_pending;
	M_dns_t     *dns;
} M_dns_ares_t;

struct M_dns {
	M_thread_mutex_t     *lock;              /*!< Concurrency lock */
	M_bool                destroy_pending;   /*!< Whether or not entire DNS subsystem is terminating */
	M_event_t            *event;             /*!< Registered event object */
	M_io_t               *io;                /*!< Registered IO object */
	M_threadid_t          threadid;          /*!< Used if spawning a private thread for the event loop */

	M_queue_t            *cache;             /*!< Cache of dns lookups in order for expiration */
	M_hash_strvp_t       *cache_lookup;      /*!< Hashtable for fast lookup of names as aftype:hostname (aftype may be AF_INET, AF_INET6, AF_UNSPEC)  */

	M_list_t             *ares_channels;     /*!< List of ares_channels, can have more than 1 due to reloading server config */
	M_hash_u64vp_t       *sockhandle;        /*!< Hashtable of file descriptors to OS event handles */
	M_llist_t            *happyeb_aginglist; /*!< Linked list in insertion order of M_dns_happyeb_result_t *, first entry is oldest entry */
	M_hash_strvp_t       *happyeb;           /*!< Map of  ipaddr  to M_dns_happyeb_result_t *  */

	/* Config */
	M_uint64              query_timeout_ms;          /*!< Estimated maximum query timeout.  This is a best effort as c-ares doesn't have a true max */
	M_uint64              server_cache_timeout_s;    /*!< How long before re-reading the DNS configuration from the system */
	M_uint64              query_cache_max_s;         /*!< Maximum amount of time a DNS entry can be cached past its TTL if the DNS servers are unreachable */
	M_uint64              happyeyeballs_cache_max_s; /*!< Maximum time to cache connectivity related information as per the Happy Eyeballs specification */
};

struct M_io_handle {
	M_bool           isup;
	M_dns_t         *dns;
	M_io_t          *io;
	M_event_timer_t *timer;
};

struct M_dns_cache_entry {
	M_dns_t      *dns;      /*!< Pointer back to main dns context */
	char         *hostname; /*!< Hostname being queried */
	int           aftype;   /*!< one of AF_INET, AF_INET6, AF_UNSPEC */
	M_time_t      ts;       /*!< Last updated time */
	M_time_t      ttl;      /*!< TTL returned from DNS for how long to cache entry */

	M_list_str_t *addrs;    /*!< List of cached results */
};
typedef struct M_dns_cache_entry M_dns_cache_entry_t;

static void M_dns_cache_free_cb(void *arg)
{
	M_dns_cache_entry_t *entry = arg;
	M_list_str_destroy(entry->addrs);
	M_free(entry->hostname);
	M_free(entry);
}

static void M_dns_cache_remove_entry(M_dns_cache_entry_t *entry)
{
	char entrystr[256];
	/* Delete the entry */
	M_snprintf(entrystr, sizeof(entrystr), "%d:%s", entry->aftype, entry->hostname);
	M_hash_strvp_remove(entry->dns->cache_lookup, entrystr, M_TRUE);
	M_queue_remove(entry->dns->cache, entry);
}

static void M_dns_cache_purge_stale(M_dns_t *dns)
{
	M_dns_cache_entry_t *entry;
	M_queue_foreach_t   *q_foreach = NULL;
	M_time_t             t         = M_time();

	while (M_queue_foreach(dns->cache, &q_foreach, (void **)&entry)) {
		char entrystr[256];
		/* If we hit an entry with a non-expired cache, we can stop.
		 * NOTE we don't rely on TTL here as we keep it in cache if we
		 *      can't hit DNS servers for some reason to handle DNS blips */
		if (entry->ts + dns->query_cache_max_s > t) {
			break;
		}
		/* Delete the entry */
		M_dns_cache_remove_entry(entry);
	}
	M_queue_foreach_free(q_foreach);
}


static M_dns_cache_entry_t *M_dns_cache_get_entry(M_dns_t *dns, const char *hostname, int aftype)
{
	M_dns_cache_entry_t *entry;
	char                 entrystr[256];

	M_dns_cache_purge_stale(dns);

	M_snprintf(entrystr, sizeof(entrystr), "%d:%s", aftype, hostname);

	entry = M_hash_strvp_get_direct(dns->cache_lookup, entrystr);

	return entry;
}

static M_dns_cache_entry_t *M_dns_cache_insert_entry(M_dns_t *dns, const char *hostname, int aftype, struct ares_addrinfo *ai)
{
	char                        entrystr[256];
	M_dns_cache_entry_t        *entry;
	struct ares_addrinfo_node  *node    = ai->nodes;
	struct ares_addrinfo_cname *cnode   = ai->cnames;
	int                         min_ttl = dns->query_cache_max_s;

	entry = M_dns_cache_get_entry(dns, hostname, aftype);
	if (entry != NULL) {
		/* Clear existing entries and temporarily remove from queue to re-order */
		M_dns_cache_remove_entry(entry);
	}

	entry             = M_malloc_zero(sizeof(*entry));
	entry->dns        = dns;
	entry->hostname   = M_strdup(hostname);
	entry->aftype     = aftype;
	entry->addrs      = M_list_str_create(M_LIST_STR_NONE);
	entry->ts         = M_time();

	while (node != NULL) {
		char str[128];
		void *ptr = NULL;
		switch (node->ai_family) {
			case AF_INET:
				ptr = &((struct sockaddr_in *)node->ai_addr)->sin_addr;
				break;
			case AF_INET6:
				ptr = &((struct sockaddr_in6 *)node->ai_addr)->sin6_addr;
				break;
		}
		if (ptr && M_dns_ntop(node->ai_family, ptr, str, sizeof(str))) {
			M_list_str_insert(entry->addrs, str);
			if (node->ai_ttl < min_ttl)
				min_ttl = node->ai_ttl;
		}
		node = node->ai_next;
	}

	/* Scan through cnames just to make sure we have the real min_ttl */
	while (cnode != NULL) {
		if (cnode->ttl < min_ttl)
			min_ttl = cnode->ttl;
	}

	/* Lets just make our real minimum 1 incase something hit 0 */
	if (min_ttl == 0)
		min_ttl = 1;
	entry->ttl = min_ttl;

	M_snprintf(entrystr, sizeof(entrystr), "%d:%s", aftype, hostname);
	M_hash_strvp_insert(dns->cache_lookup, entrystr, entry);
	M_queue_insert(dns->cache, entry);
	return entry;
}

typedef struct {
	M_time_t               ts;
	M_dns_happyeb_status_t hestatus;
	char                   addr[48];
	M_llist_node_t        *node;
} M_dns_happyeb_result_t;

typedef struct {
	size_t                  idx;
	const char             *addr;
	M_dns_happyeb_status_t  hestatus;
} M_dns_addr_t;

static int M_dns_happyeb_sort_compar(const void *arg1, const void *arg2, void *thunk)
{
	const M_dns_addr_t *s1 = arg1;
	const M_dns_addr_t *s2 = arg2;
	(void)thunk;

	/* Sort the list by  ASC status,  ASC idx */

	if (s1->hestatus < s2->hestatus)
		return -1;
	if (s1->hestatus > s2->hestatus)
		return 1;

	if (s1->idx < s2->idx)
		return -1;
	if (s1->idx > s2->idx)
		return 1;

	return 0;
}

static void M_dns_happyeb_destroy_result(void *arg)
{
	M_dns_happyeb_result_t *result = arg;
	M_llist_take_node(result->node);
	M_free(result);
}

void M_dns_happyeyeballs_update(M_dns_t *dns, const char *ipaddr, M_dns_happyeb_status_t status)
{
	M_dns_happyeb_result_t *result = NULL;

	if (!dns)
		return;

	M_thread_mutex_lock(dns->lock);
	result = M_hash_strvp_get_direct(dns->happyeb, ipaddr);
	if (result != NULL) {
		/* Remove from list since time will change */
		M_llist_take_node(result->node);
		result->node = NULL;
	} else {
		result = M_malloc_zero(sizeof(*result));
		M_str_cpy(result->addr, sizeof(result->addr), ipaddr);
		M_hash_strvp_insert(dns->happyeb, ipaddr, result);
	}

	result->ts       = M_time();
	result->hestatus = status;
	result->node     = M_llist_insert(dns->happyeb_aginglist, result);

	M_thread_mutex_unlock(dns->lock);
}


static M_dns_happyeb_status_t M_dns_happyeb_fetch_status(M_dns_t *dns, const char *addr)
{
	M_dns_happyeb_result_t *result = M_hash_strvp_get_direct(dns->happyeb, addr);
	if (result)
		return result->hestatus;
	return M_HAPPYEB_STATUS_UNKNOWN;
}


static void M_dns_happyeb_purge_expired(M_dns_t *dns)
{
	M_llist_node_t *node = M_llist_first(dns->happyeb_aginglist);
	M_time_t        t    = M_time();

	while (node != NULL) {
		M_dns_happyeb_result_t *result = M_llist_node_val(node);
		M_llist_node_t         *next   = M_llist_node_next(node);

		/* Stop when we hit non-expired entries */
		if (result->ts + dns->happyeyeballs_cache_max_s < t) {
			break;
		}

		M_hash_strvp_remove(dns->happyeb, result->addr, M_TRUE);
		node = next;
	}
}


static M_list_str_t *M_dns_happyeb_sort(M_dns_t *dns, const M_list_str_t *ipaddrs)
{
	size_t        num_ipv4       = 0;
	size_t        num_ipv6       = 0;
	size_t        ipv6idx        = 0;
	size_t        ipv4idx        = 0;
	size_t        i;
	size_t        len            = M_list_str_len(ipaddrs);
	M_dns_addr_t *list           = M_malloc_zero(len * sizeof(*list));
	const char  **ipv4list       = M_malloc_zero(len * sizeof(*ipv4list));
	const char  **ipv6list       = M_malloc_zero(len * sizeof(*ipv4list));
	M_list_str_t *out            = M_list_str_create(M_LIST_STR_NONE);

	M_dns_happyeb_purge_expired(dns);

	/* List is pre-sorted by ares_getaddrinfo() using Destination Addres Selection,
	 * but with happy eyeballs, we then want to interleave ipv6 and ipv4 addresses,
	 * so we need to into 2 arrays. */
	for (i=0; i<len; i++) {
		const char *addr = M_list_at(ipaddrs, i);
		if (M_str_chr(addr,':')) {
			ipv6list[num_ipv6++] = addr;
		} else {
			ipv4list[num_ipv4++] = addr;
		}
	}

	for (i=0; i<len; i++) {
		if ((i % 2 == 0 && ipv6idx < num_ipv6) ||
			(i % 2 != 0 && ipv4idx == num_ipv4)) {
			list[i].idx    = ipv6idx;
			list[i].addr   = ipv6list[ipv6idx++];
		} else {
			list[i].idx    = ipv4idx;
			list[i].addr   = ipv4list[ipv4idx++];
		}

		/* Fetch prior connection results */
		list[i].hestatus = M_dns_happyeb_fetch_status(dns, list[i].addr);
	}
	M_free(ipv4list);
	M_free(ipv6list);


	/* Sort the list by  ASC status,  ASC idx */
	M_sort_qsort(list, len, sizeof(*list), M_dns_happyeb_sort_compar, NULL);

	for (i=0; i<len; i++)
		M_list_str_insert(out, list[i].addr);

	M_free(list);
	return out;
}


static M_thread_once_t M_ares_once = M_THREAD_ONCE_STATIC_INITIALIZER;

static void M_io_dns_init_destroy(void *arg)
{
	(void)arg;
	if (!M_thread_once_reset(&M_ares_once))
		return;
	ares_library_cleanup();
}

static void M_io_dns_init_run(M_uint64 flags)
{
	(void)flags;
	/* Ignore failures? Could this fail? */
	ares_library_init(ARES_LIB_INIT_ALL);
	M_library_cleanup_register(M_io_dns_init_destroy, NULL);
}

static void M_io_dns_init(void)
{
	M_thread_once(&M_ares_once, M_io_dns_init_run, 0);
}

static void M_io_dns_ares_update_timeout(M_io_layer_t *layer)
{
	struct timeval to;
	M_uint64       interval_ms;
	M_uint64       min_ms = M_TIMEOUT_INF;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         i;

	for (i=0; i<M_list_len(handle->dns->ares_channels); i++) {
		M_dns_ares_t *achannel = M_list_at(handle->dns->ares_channels, i);

		M_mem_set(&to, 0, sizeof(to));
		to.tv_sec  = 86400;
		to.tv_usec = 0;
		ares_timeout(achannel->channel, &to, &to);

		interval_ms = (M_uint64)((M_uint64)to.tv_sec * 1000) + (M_uint64)(to.tv_usec / 1000) + 1 /* +1 to handle conv from usec to msec, otherwise we may loop a few times consuming cpu */;
		if (interval_ms < min_ms)
			min_ms = interval_ms;
	}

	/* Update timer value */
	if (min_ms == M_TIMEOUT_INF) {
		M_event_timer_stop(handle->timer);
	} else {
		M_event_timer_reset(handle->timer, min_ms);
	}
}

static void M_dns_removeevent_self_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *arg)
{
	M_event_remove(arg);
}

static M_bool M_io_dns_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t  *handle = M_io_layer_get_handle(layer);
	size_t          cnt    = 0;
	size_t          i;

	/* Iterate across all open channels.  There should be always 1 good one, then
	 * there could be some that are pending a destroy until all queries are flushed */
	for (i=0; i<M_list_len(handle->dns->ares_channels); i++) {
		M_dns_ares_t *achannel = M_list_at(handle->dns->ares_channels, i);

		cnt = 0;

		if (*type != M_EVENT_TYPE_OTHER) {
			/* We don't know which socket notified, most likely there's
			 * only one or two sockets, just enumerate them and notify
			 * on all */
			M_hash_u64vp_enum_t *hashenum = NULL;
			M_uint64             fd64;
			M_list_u64_t        *fdlist = M_list_u64_create(M_LIST_U64_NONE);
			size_t               j;

			M_hash_u64vp_enumerate(handle->dns->sockhandle, &hashenum);
			while (M_hash_u64vp_enumerate_next(handle->dns->sockhandle, hashenum, &fd64, NULL)) {
				M_list_u64_insert(fdlist, fd64);
			}
			M_hash_u64vp_enumerate_free(hashenum);

			for (j=0; j<M_list_u64_len(fdlist); j++) {
					ares_process_fd(achannel->channel,
					                (ares_socket_t)(*type != M_EVENT_TYPE_WRITE)?M_list_u64_at(fdlist, i):ARES_SOCKET_BAD,
					                (ares_socket_t)(*type == M_EVENT_TYPE_WRITE)?M_list_u64_at(fdlist, i):ARES_SOCKET_BAD);
				cnt++;
			}

			M_list_u64_destroy(fdlist);
		}

		if (cnt == 0) {
			/* Always call ares_process_fd() as we may need to call it with bad handles during
			 * timeout conditions */
			ares_process_fd(achannel->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
		}

	}

	/* Re-evaluate timer from ares_timeout() */
	M_io_dns_ares_update_timeout(layer);

	/* Allow a real disconnect to passthru */
	if (*type == M_EVENT_TYPE_DISCONNECTED && !handle->isup) {

		M_thread_mutex_lock(handle->dns->lock);

		/* Kill all ares channels forcibly */
		M_list_destroy(handle->dns->ares_channels, M_TRUE);
		handle->dns->ares_channels = NULL;

		/* Destroy any timers */
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
		M_thread_mutex_unlock(handle->dns->lock);

		/* Remove self from event processing, but can't do it from own callback */
		M_event_queue_task(handle->dns->event, M_dns_removeevent_self_cb, M_io_layer_get_io(layer));
		return M_FALSE;
	}

	/* Consume -- though there should be no one to deliver to */
	return M_TRUE;
}

static void M_io_dns_timeout_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *arg)
{
	M_io_layer_t *layer = arg;

	(void)event;
	(void)type;
	(void)io;
	/* Call into the normal event processing system which will modify handles being waited
	 * on as well as set up a new timer. */
	M_io_dns_process_cb(layer, &type);
}

static void M_io_dns_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t  *handle = M_io_layer_get_handle(layer);
	M_io_t         *io     = M_io_layer_get_io(layer);
	M_event_t      *event  = M_io_get_event(io);

	if (io == NULL || event == NULL)
		return;

	/* Destroy any timers */
	M_event_timer_remove(handle->timer);
	handle->timer = NULL;
}


static void M_io_dns_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	M_free(handle);
}

static M_bool M_io_dns_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	/* Create a timer */
	handle->timer = M_event_timer_add(event, M_io_dns_timeout_cb, layer);
	M_event_timer_set_firecount(handle->timer, 1);

	return M_TRUE;
}

static M_io_state_t M_io_dns_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (!handle->isup)
		return M_IO_STATE_DISCONNECTED;
	return M_IO_STATE_CONNECTED;
}


static M_bool M_io_dns_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (!handle->isup)
		return M_TRUE;

	/* Set down */
	handle->isup = M_FALSE;

	/* Signal disconnect */
	M_io_layer_softevent_add(layer, M_FALSE, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);

	return M_FALSE;
}

static M_io_t *M_io_dns_create(M_dns_t *dns, M_event_t *event)
{
	M_io_t           *io;
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;

	if (dns == NULL)
		return NULL;

	handle           = M_malloc_zero(sizeof(*handle));
	handle->dns      = dns;
	handle->isup     = M_TRUE;

	io        = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_dns_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_dns_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_dns_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_dns_destroy_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_dns_disconnect_cb);
	M_io_callbacks_reg_state(callbacks, M_io_dns_state_cb);
	M_io_layer_add(io, "DNS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);
	handle->io = io;
	M_event_add(event, io, NULL, NULL);

	return io;
}


static void M_dns_ares_sock_state_cb(void *arg, ares_socket_t sock_fd, int readable, int writable)
{
	M_dns_t *dns = arg;
	void    *val = NULL;

	M_EVENT_HANDLE handle = M_EVENT_INVALID_HANDLE;

	if (M_hash_u64vp_get(dns->sockhandle, sock_fd, &val)) {
		handle = (M_EVENT_HANDLE)val;
	}

	/* We don't know anything about this, safe to exit */
	if (!readable && !writable && handle == M_EVENT_INVALID_HANDLE) {
		return;
	}

	/* Not a handle known to us, but we'll need to do something with it
	 * so create an OS handle */
	if (handle == M_EVENT_INVALID_HANDLE) {
#ifdef _WIN32
		handle = WSACreateEvent();
		WSAEventSelect(sock_fd, handle, FD_READ|FD_WRITE|FD_CLOSE);
#else
		handle = sock_fd;
#endif
		M_hash_u64vp_insert(dns->sockhandle, sock_fd, (void *)handle);
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_ADD_HANDLE, dns->io, handle, sock_fd, 0, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);
	}

	if (readable) {
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_ADD_WAITTYPE, dns->io, handle, sock_fd, M_EVENT_WAIT_READ, 0);
	} else {
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_DEL_WAITTYPE, dns->io, handle, sock_fd, M_EVENT_WAIT_READ, 0);
	}

	if (writable) {
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_ADD_WAITTYPE, dns->io, handle, sock_fd, M_EVENT_WAIT_WRITE, 0);
	} else {
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_DEL_WAITTYPE, dns->io, handle, sock_fd, M_EVENT_WAIT_WRITE, 0);
	}

	if (!readable && !writable) {
		M_event_handle_modify(dns->event, M_EVENT_MODTYPE_DEL_HANDLE, dns->io, handle, sock_fd, 0, 0);
#ifdef _WIN32
		CloseHandle(handle);
#endif
	}
}


static void M_dns_destroy_ares_channel(M_dns_ares_t *achannel)
{
	achannel->destroy_pending = M_TRUE;

	if (achannel->queries_pending)
		return;

	ares_destroy(achannel->channel);
	achannel->channel = NULL;
	M_list_remove_val(achannel->dns->ares_channels, achannel, M_LIST_MATCH_PTR);
}

static size_t M_dns_num_servers(ares_channel channel)
{
	size_t                 num_servers = 0;
	struct ares_addr_node *node        = NULL;

	if (channel == NULL)
		return 3;

	ares_get_servers(channel, &node);
	while (node != NULL) {
		num_servers++;
		node=node->next;
	}

	if (num_servers == 0)
		num_servers = 3;

	return num_servers;
}


static M_bool M_dns_reload_server(M_dns_t *dns, M_bool force_reload)
{
	ares_channel        channel = NULL;
	int                 err;
	struct ares_options options;
	size_t              num_servers;
	M_dns_ares_t       *achannel = M_list_last(dns->ares_channels);

	if (achannel && !force_reload && M_time() < achannel->load_ts + (M_time_t)dns->server_cache_timeout_s)
		return M_TRUE;

	M_mem_set(&options, 0, sizeof(options));
	/* c-ares doesn't currently have an overall query timeout, just a timeout
	 * per server that gets queried and a number of tries per server.  So
	 * we set the number of queries per server at 2, then we fetch the number
	 * of configured servers, and finally we can then determine the timeout per
	 * query to allow */
	num_servers                = M_dns_num_servers(achannel?achannel->channel:NULL);
	options.tries              = 2;
	options.timeout            = (int)(dns->query_timeout_ms / (num_servers * options.tries));
	options.sock_state_cb      = M_dns_ares_sock_state_cb;
	options.sock_state_cb_data = dns;

	if ((err = ares_init_options(&channel, &options, ARES_OPT_TIMEOUTMS|ARES_OPT_TRIES|ARES_OPT_SOCK_STATE_CB)) != ARES_SUCCESS) {
M_printf("ares_init_options failed (%d): %s\n", err, ares_strerror(err));
		if (!achannel)
			return M_FALSE;
		return M_TRUE;
	}

	/* Mark other channel as destroy */
	if (achannel != NULL) {
		M_dns_destroy_ares_channel(achannel);
	}

	achannel          = M_malloc_zero(sizeof(*achannel));
	achannel->load_ts = M_time();
	achannel->channel = channel;
	achannel->dns     = dns;
	M_list_insert(dns->ares_channels, achannel);

	return M_TRUE;
}

static void M_dns_reload_server_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	(void)event;
	(void)type;
	(void)io;
	M_dns_reload_server(cb_arg, M_TRUE);
}

static void M_dns_ares_channel_free(void *arg)
{
	M_dns_ares_t *achannel = arg;
	if (achannel->channel) {
		ares_destroy(achannel->channel);
		achannel->channel = NULL;
	}

	M_free(achannel);
}


static void *M_dns_eventthread(void *arg)
{
	M_event_t *event = arg;
	M_event_loop(event, M_TIMEOUT_INF);
	return NULL;
}


M_bool M_dns_destroy(M_dns_t *dns)
{
	if (dns == NULL)
		return M_FALSE;
	M_list_destroy(dns->ares_channels, M_TRUE);
	dns->ares_channels = NULL;

	M_io_destroy(dns->io);
	dns->io = NULL;

	M_hash_u64vp_destroy(dns->sockhandle, M_TRUE);
	dns->sockhandle = NULL;

	/* Private eventloop, lets kill it */
	if (dns->threadid) {
		void *rv = NULL;
		M_event_done(dns->event);
		M_thread_join(dns->threadid, &rv);
		dns->threadid = 0;
	}

	M_queue_destroy(dns->cache);
	M_hash_strvp_destroy(dns->cache_lookup, M_TRUE);

	/* NOTE: HappyEyeballs hash *must* be destroyed before the expire list since the
	 *       hashtable destroy will detach the entry from the expire list */
	M_hash_strvp_destroy(dns->happyeb, M_TRUE);
	M_llist_destroy(dns->happyeb_aginglist, M_TRUE);

	M_thread_mutex_destroy(dns->lock);
	dns->lock = NULL;

	M_free(dns);

	return M_TRUE;
}


M_dns_t *M_dns_create(M_event_t *event)
{
	M_dns_t *dns;
	struct M_list_callbacks listcb = {
		NULL, NULL, NULL, M_dns_ares_channel_free
	};

	M_io_net_init_system();

	M_io_dns_init();

	dns                            = M_malloc_zero(sizeof(*dns));
	dns->query_timeout_ms          = 5000;  /* 5s */
	dns->server_cache_timeout_s    = 120;   /* 2 minutes - server config */
	dns->query_cache_max_s         = 3600;  /* 1 hr */
	dns->happyeyeballs_cache_max_s = 600;   /* 10 minutes */
	dns->lock                      = M_thread_mutex_create(M_THREAD_MUTEXATTR_RECURSIVE);

	dns->cache                     = M_queue_create(NULL /* Naturally sorted by TS on insert */, M_dns_cache_free_cb);
	dns->cache_lookup              = M_hash_strvp_create(16, 75, M_HASH_STRVP_NONE, NULL);
	dns->happyeb_aginglist         = M_llist_create(NULL, M_LLIST_NONE);
	dns->happyeb                   = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_dns_happyeb_destroy_result);

	dns->sockhandle                = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, NULL);
	dns->ares_channels             = M_list_create(&listcb, M_LIST_NONE);

	if (!M_dns_reload_server(dns, M_FALSE)) {
		M_dns_destroy(dns);
		return NULL;
	}

	/* If no even subsystem is provided, we need to spawn our own private one
	 * in a thread */
	if (event == NULL) {
		M_thread_attr_t *attr = M_thread_attr_create();
		M_thread_attr_set_create_joinable(attr, M_TRUE);
		event         = M_event_create(M_EVENT_FLAG_NONE);
		dns->threadid = M_thread_create(attr, M_dns_eventthread, event);
		M_thread_attr_destroy(attr);
	}

	dns->io                    = M_io_dns_create(dns, event);
	dns->event                 = M_io_get_event(dns->io);

	return dns;
}


typedef struct  {
	M_dns_t            *dns;          /*!< Handle to DNS context                         */
	M_dns_ares_t       *achannel;     /*!< Pointer to ares_channel handling query        */
	char               *hostname;     /*!< Requested hostname to query                   */
	M_uint16            port;         /*!< Requested port (happy eyeballs requirement)   */
	int                 aftype;       /*!< Requested type (AF_INET, AF_INET6, AF_UNSPEC) */
	M_event_t          *event;        /*!< Event loop to run callback on                 */

	M_io_dns_callback_t callback;     /*!< User-provided callback                        */
	void               *cb_data;      /*!< User-provided callback data                   */

	/* Result Data */
	M_list_str_t       *ipaddrs;      /*!< List of ip addresses returned */
	M_dns_result_t      result;       /*!< Ending result code */
} M_dns_query_t;


static void M_dns_gethostbyname_result_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_dns_query_t *query    = cb_arg;

	query->callback(query->ipaddrs, query->cb_data, query->result);

	M_list_str_destroy(query->ipaddrs);
	M_free(query->hostname);
	M_free(query);
}


static void ares_addrinfo_cb(void *arg, int status, int timeouts, struct ares_addrinfo *result)
{
	M_dns_query_t       *query = arg;
	M_dns_cache_entry_t *entry = NULL;

	switch (status) {
		case ARES_SUCCESS:
			query->result = M_DNS_RESULT_SUCCESS;
			break;
		case ARES_EBADNAME:
		case ARES_ENOTFOUND:
		case ARES_ENODATA:
			query->result = M_DNS_RESULT_NOTFOUND;
			break;
		case ARES_ECANCELLED:
			query->result = M_DNS_RESULT_TIMEOUT;
			break;
		case ARES_ENOTIMP:
		case ARES_ENOMEM:
		default:
			query->result = M_DNS_RESULT_SERVFAIL;
			break;
	}

	M_thread_mutex_lock(query->dns->lock);

	entry = M_dns_cache_get_entry(query->dns, query->hostname, query->aftype);

	if (query->result == M_DNS_RESULT_NOTFOUND) {
		M_dns_cache_remove_entry(entry);
		entry = NULL;
	}

	if (query->result == M_DNS_RESULT_SUCCESS) {
		entry = M_dns_cache_insert_entry(query->dns, query->hostname, query->aftype, result);
	}

	ares_freeaddrinfo(result);

	if (query->result != M_DNS_RESULT_SUCCESS && entry != NULL) {
		query->result = M_DNS_RESULT_SUCCESS_CACHE;
	}

	if (entry != NULL) {
		query->ipaddrs = M_dns_happyeb_sort(query->dns, entry->addrs);
	}

	/* If there is a destroy pending and we were the last query result, destroy! */
	query->achannel->queries_pending--;
	if (query->achannel->destroy_pending && query->achannel->queries_pending == 0) {
		M_dns_destroy_ares_channel(query->achannel);
	}

	M_thread_mutex_unlock(query->dns->lock);

	M_event_queue_task(query->event, M_dns_gethostbyname_result_cb, query);
}

static void M_dns_gethostbyname_enqueue(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
	M_dns_query_t *query    = cb_arg;
	M_dns_ares_t  *achannel = NULL;
	struct ares_addrinfo_hints hints = {
		0,
		query->aftype,
		0,
		0
	};

	M_thread_mutex_lock(query->dns->lock);

	/* Reload DNS settings if system cache timeout has expired */
	M_dns_reload_server(query->dns, M_FALSE);

	achannel = M_list_last(query->dns->ares_channels);
	achannel->queries_pending++;
	query->achannel = achannel;
	M_thread_mutex_unlock(query->dns->lock);
	ares_getaddrinfo(achannel->channel, query->hostname, NULL, &hints, ares_addrinfo_cb, query);
}


static M_bool M_dns_host_is_addr(const char *host, int *type)
{
	struct in_addr  addr4;
#ifdef AF_INET6
	struct in6_addr addr6;
#endif
	if (M_dns_pton(AF_INET, host, &addr4)) {
		*type = AF_INET;
		return M_TRUE;
	}

#ifdef AF_INET6
	if (M_dns_pton(AF_INET6, host, &addr6)) {
		*type = AF_INET6;
		return M_TRUE;
	}
#endif
	return M_FALSE;
}


static char *M_dns_punyhostname(const char *hostname)
{
	char         **parts     = NULL;
	M_list_str_t  *l;
	M_buf_t       *buf;
	char          *out       = NULL;
	size_t         num_parts = 0;
	size_t         i;

	/* Each part is encoded separately. */
	parts = M_str_explode_str('.', hostname, &num_parts);
	if (parts == NULL || num_parts == 0) {
		M_str_explode_free(parts, num_parts);
		return NULL;
	}

	/* We're going to use a list because it will make it easy
 	 * to join all the parts back together. */
	l = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<num_parts; i++) {
		/* Ascii parts don't need to be encoded> */
		if (M_str_isascii(parts[i])) {
			M_list_str_insert(l, parts[i]);
			continue;
		}

		buf = M_buf_create();
		/* Add the IDNA prefix to denote this is an encoded part. */
		M_buf_add_str(buf, "xn--");

		/* Encode. */
		if (M_textcodec_encode_buf(buf, parts[i], M_TEXTCODEC_EHANDLER_FAIL, M_TEXTCODEC_PUNYCODE) != M_TEXTCODEC_ERROR_SUCCESS) {
			M_buf_cancel(buf);
			M_str_explode_free(parts, num_parts);
			return NULL;
		}

		out = M_buf_finish_str(buf, NULL);
		M_list_str_insert(l, out);
		M_free(out);
	}
	M_str_explode_free(parts, num_parts);

	/* Put it all together. */
	out = M_list_str_join(l, '.');
	M_list_str_destroy(l);
	return out;
}


void M_dns_gethostbyname(M_dns_t *dns, M_event_t *event, const char *hostname, M_uint16 port, M_io_net_type_t type, M_io_dns_callback_t callback, void *cb_data)
{
	char                *punyhost = NULL;
	int                  aftype;
	M_dns_query_t       *query    = NULL;
	M_dns_cache_entry_t *entry    = NULL;

	if (callback == NULL)
		return;

	if (M_str_isempty(hostname)) {
		callback(NULL, cb_data, M_DNS_RESULT_INVALID);
		return;
	}

	/* Requested hostname is an IP address, make a fake entry and call callback */
	if (M_dns_host_is_addr(hostname, &aftype)) {
		M_list_str_t *ipaddr;

		if (aftype == AF_INET && type == M_IO_NET_IPV6) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return;
		}
#ifdef AF_INET6
		if (aftype == AF_INET6 && type == M_IO_NET_IPV4) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return;
		}
#endif

		ipaddr = M_list_str_create(M_LIST_STR_NONE);
		M_list_str_insert(ipaddr, hostname);

		callback(ipaddr, cb_data, M_DNS_RESULT_SUCCESS);
		M_list_str_destroy(ipaddr);
		return;
	}

	/* We need to do a real DNS lookup, so we'd better have what we need */
	if (dns == NULL || event == NULL) {
		callback(NULL, cb_data, M_DNS_RESULT_INVALID);
		return;
	}

	/* IDNA host name if necessary because lookups should always be
 	 * ASCII only. Which is the purpouse of IDNA punycode encoding
	 * domains that have non-ASCII characters. */
	if (!M_str_isascii(hostname)) {
		/* Punycode encode international domain names. */
		punyhost = M_dns_punyhostname(hostname);
		if (punyhost == NULL) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return;
		}
		hostname = punyhost;
	}

	aftype = type == M_IO_NET_IPV4?AF_INET:(type == M_IO_NET_IPV6?AF_INET6:AF_UNSPEC);

	M_thread_mutex_lock(dns->lock);
	entry = M_dns_cache_get_entry(dns, hostname, aftype);
	if (entry != NULL && (entry->ts + entry->ttl) > M_time()) {
		M_list_str_t *ipaddrs = M_dns_happyeb_sort(dns, entry->addrs);
		M_thread_mutex_unlock(dns->lock);

		callback(ipaddrs, cb_data, M_DNS_RESULT_SUCCESS_CACHE);
		M_list_str_destroy(ipaddrs);
		M_free(punyhost);
		return;
	}

	M_thread_mutex_unlock(dns->lock);

	/* XXX: Reference count dns */

	query           = M_malloc_zero(sizeof(*query));
	query->hostname = M_strdup(hostname);
	query->dns      = dns;
	query->port     = port;
	query->aftype   = aftype;
	query->event    = event;
	query->callback = callback;
	query->cb_data  = cb_data;

	M_event_queue_task(dns->event, M_dns_gethostbyname_enqueue, query);

	M_free(punyhost);
}


M_bool M_dns_set_query_timeout(M_dns_t *dns, M_uint64 timeout_ms)
{
	if (dns == NULL)
		return M_FALSE;

	if (timeout_ms == 0)
		timeout_ms = 5000;

	M_thread_mutex_lock(dns->lock);

	dns->query_timeout_ms = timeout_ms;
	M_event_queue_task(dns->event, M_dns_reload_server_cb, dns);

	M_thread_mutex_unlock(dns->lock);
	return M_TRUE;
}


M_bool M_dns_set_cache_timeout(M_dns_t *dns, M_uint64 max_timeout_s)
{
	if (dns == NULL)
		return M_FALSE;

	if (max_timeout_s == 0)
		max_timeout_s = 3600; /* 1 hr */

	M_thread_mutex_lock(dns->lock);

	dns->query_cache_max_s = max_timeout_s;
	M_event_queue_task(dns->event, M_dns_reload_server_cb, dns);

	M_thread_mutex_unlock(dns->lock);
	return M_TRUE;
}


M_bool M_dns_pton(int af, const char *src, void *dst)
{
	if (ares_inet_pton(af, src, dst) != 1)
		return M_FALSE;
	return M_TRUE;
}


M_bool M_dns_ntop(int af, const void *src, char *addr, size_t addr_size)
{
	M_mem_set(addr, 0, addr_size);
	if (!ares_inet_ntop(af, src, addr, (ares_socklen_t)addr_size))
		return M_FALSE;
	return M_TRUE;
}
