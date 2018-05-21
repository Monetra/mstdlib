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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
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

/* Needed for M_io_net_init_system */
#include "m_io_int.h"

/* Thoughts:
 *   * User creates global M_dns_t which caches DNS server information, and can do some result
 *     caching and happy-eyeball caching
 *   * User creates a query and registers a callback to be called when the query is done.
 *   * When a user creates a query, they pass the event handle of the same event handle they
 *     are using.
 *   * When the callback exits, EVERYTHING related to that query is automatically cleaned
 *     up so there is nothing for the user to clean up afterwards
 *   * If a user cancels a query, they can manually clean it up
 *   * We should support an additional timeout value.
 *   * Should provide a way to update happy eyeballs success/fail
 *   * Should provide options for controlling things like round-robin dns address
 *   * Because the ares gethostbyname result callback cannot call its own cleanup functions
 *     ares_destroy(), etc ... we must use M_event_queue_task() to run the task on the
 *     event loop.
 */


/*! Result of a connection attempt with metadata */
struct M_dns_happyeyeballs_result {
	M_time_t                     last_update; /*!< Timestamp of last update                       */
	char                         ipport[64];  /*!< "[ipaddr]:port" associated with entry          */
	M_dns_happyeyeballs_status_t status;      /*!< Result of last lookup                          */
	M_llist_node_t              *node;        /*!< Pointer to node in expired list for reordering */
};
typedef struct M_dns_happyeyeballs_result M_dns_happyeyeballs_result_t;


/*! Container holding all connection attempts for ip:port combinations for happyeyeballs RFC6555 */
struct M_dns_happyeyeballs {
	M_llist_t      *expired; /*!< Linked list in insertion order of M_dns_happyeyeballs_result_t *, first entry is oldest entry */
	M_hash_strvp_t *results; /*!< Map of  "[ipaddr]:port"  to M_dns_happyeyeballs_result_t *  */
};
typedef struct M_dns_happyeyeballs M_dns_happyeyeballs_t;


struct M_dns_entry {
	M_dns_t      *dns;
	char         *hostname;
	M_list_str_t *ipv4_addrs;
	M_time_t      ipv4_cache_t;
	M_list_str_t *ipv6_addrs;
	M_time_t      ipv6_cache_t;
};
typedef struct M_dns_entry M_dns_entry_t;

struct M_dns_sock_handle {
	ares_socket_t       fd;
	M_EVENT_HANDLE      handle;
	M_event_wait_type_t waittype;
	M_bool              updated;
};
typedef struct M_dns_sock_handle M_dns_sock_handle_t;


struct M_io_handle {
	M_dns_t              *dns;
	M_io_t               *io;
	char                 *hostname;
	M_uint16              port;     /*!< For happyeyeballs */
	M_io_net_type_t       type;
	M_llist_t            *socklist; /*!< Socket list */
	ares_channel          channel;
	size_t                num_queries;
	size_t                num_responses;
	M_event_timer_t      *timer;
	M_timeval_t           start_tv;

	M_io_dns_callback_t   callback;
	void                 *cb_data;

	M_dns_result_t        result;
	M_list_str_t         *result_ips;
};


struct M_dns {
	M_thread_mutex_t     *lock;
	M_queue_t            *cache;
	M_hash_strvp_t       *cache_lookup;

	M_time_t              last_load_t;
	ares_channel          base_channel;

	size_t                active_queries;

	M_dns_happyeyeballs_t happyeyeballs;

	/* Config */
	M_uint64              max_query_ms;
	M_uint64              query_timeout_ms;
	M_uint64              query_tries;
	M_uint64              server_cache_timeout_s;
	M_uint64              server_cache_max_s;
	M_uint64              query_cache_timeout_s;
	M_uint64              query_cache_max_s;
	M_uint64              happyeyeballs_cache_max_s;

};


static void M_dns_cache_free_cb(void *arg)
{
	M_dns_entry_t *entry = arg;
	M_list_str_destroy(entry->ipv4_addrs);
	M_list_str_destroy(entry->ipv6_addrs);
	M_free(entry->hostname);
	M_free(entry);
}


static M_bool M_dns_reload_server(M_dns_t *dns, M_bool force_reload)
{
	ares_channel        channel = NULL;
	int                 err;
	struct ares_options options;

	if (!force_reload && M_time() < dns->last_load_t + (M_time_t)dns->server_cache_timeout_s)
		return M_TRUE;

	M_mem_set(&options, 0, sizeof(options));
	options.tries   = (int)dns->query_tries;
	options.timeout = (int)dns->query_timeout_ms;

	if ((err = ares_init_options(&channel, &options, ARES_OPT_TIMEOUTMS|ARES_OPT_TRIES)) != ARES_SUCCESS) {
M_printf("ares_init_options failed (%d): %s\n", err, ares_strerror(err));
		/* If we've exceeded the max server cache time ... we need to go ahead and return an error */
		if (dns->base_channel == NULL || dns->last_load_t + (M_time_t)dns->server_cache_max_s > M_time())
			return M_FALSE;
		return M_TRUE;
	}

	if (dns->base_channel != NULL)
		ares_destroy(dns->base_channel);

	dns->last_load_t  = M_time();
	dns->base_channel = channel;
	return M_TRUE;
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


static void M_dns_happyeyeballs_destroy_result(void *arg)
{
	M_dns_happyeyeballs_result_t *result = arg;
	if (result == NULL)
		return;

	/* Detach from linked list and free */
	(void)M_llist_take_node(result->node);
	M_free(result);
}


void M_dns_happyeyeballs_update(M_dns_t *dns, const char *ipaddr, M_uint16 port, M_dns_happyeyeballs_status_t status)
{
	M_dns_happyeyeballs_result_t *result;
	char                          ipport[64];

	if (dns == NULL || M_str_isempty(ipaddr) || status == M_HAPPYEYEBALLS_STATUS_UNKNOWN)
		return;

	M_snprintf(ipport, sizeof(ipport), "[%s]:%u", ipaddr, (unsigned int)port);

	M_thread_mutex_lock(dns->lock);
	result = M_hash_strvp_get_direct(dns->happyeyeballs.results, ipport);
	if (result == NULL) {
		result = M_malloc_zero(sizeof(*result));
		M_str_cpy(result->ipport, sizeof(result->ipport), ipport);
		result->status = M_HAPPYEYEBALLS_STATUS_UNKNOWN;
		M_hash_strvp_insert(dns->happyeyeballs.results, ipport, result);
	}

	/* Don't override a BAD result with a SLOW result, and don't override the timestamp if the
	 * current result is the same as the cached one otherwise we may never clear the cached
	 * value which may not let a higher-priority server reactivate after it comes back online */
	if ((result->status == M_HAPPYEYEBALLS_STATUS_BAD && status == M_HAPPYEYEBALLS_STATUS_SLOW) ||
	    result->status == status) {
		goto done;
	}

	/* Unlink current list node since we're updating */
	if (result->node) {
		(void)M_llist_take_node(result->node);
		result->node = NULL;
	}

	result->status      = status;
	result->last_update = M_time();
	result->node        = M_llist_insert(dns->happyeyeballs.expired, result);
done:
	M_thread_mutex_unlock(dns->lock);
}


static void M_dns_happyeyeballs_purge_expired(M_dns_t *dns)
{
	M_llist_node_t *node;
	M_time_t        t    = M_time();

	if (dns == NULL)
		return;

	while ((node = M_llist_first(dns->happyeyeballs.expired)) != NULL) {
		M_dns_happyeyeballs_result_t *result = M_llist_node_val(node);

		/* No more entries, oldest entries are first */
		if (t < result->last_update + (M_time_t)dns->happyeyeballs_cache_max_s)
			break;

		/* Remove this entry */
		M_hash_strvp_remove(dns->happyeyeballs.results, result->ipport, M_TRUE);
	}
}


static M_dns_happyeyeballs_status_t M_dns_happyeyeballs_get(M_dns_t *dns, const char *ipaddr, M_uint16 port)
{
	M_dns_happyeyeballs_result_t *result;
	M_dns_happyeyeballs_status_t  ret = M_HAPPYEYEBALLS_STATUS_UNKNOWN;
	char                          ipport[64];

	if (dns == NULL || M_str_isempty(ipaddr))
		return M_HAPPYEYEBALLS_STATUS_UNKNOWN;

	M_snprintf(ipport, sizeof(ipport), "[%s]:%u", ipaddr, (unsigned int)port);

	M_dns_happyeyeballs_purge_expired(dns);

	result = M_hash_strvp_get_direct(dns->happyeyeballs.results, ipport);
	if (result != NULL)
		ret = result->status;

	return ret;
}

struct M_dns_happyeyeballs_sortlist {
	const char                  *ipaddr;
	size_t                       idx;
	M_dns_happyeyeballs_status_t status;
};
typedef struct M_dns_happyeyeballs_sortlist M_dns_happyeyeballs_sortlist_t;


static int M_dns_happyeyeballs_sort_compar(const void *arg1, const void *arg2, void *thunk)
{
	const M_dns_happyeyeballs_sortlist_t *s1 = arg1;
	const M_dns_happyeyeballs_sortlist_t *s2 = arg2;
	(void)thunk;

	/* Sort the list by  ASC status,  ASC idx */

	if (s1->status < s2->status)
		return -1;
	if (s1->status > s2->status)
		return 1;

	if (s1->idx < s2->idx)
		return -1;
	if (s1->idx > s2->idx)
		return 1;

	return 0;
}


static M_list_str_t *M_dns_happyeyeballs_sort(M_dns_t *dns, const M_list_str_t *ipv6_addrs, const M_list_str_t *ipv4_addrs, M_uint16 port)
{
	size_t                          ipv6_cnt = M_list_str_len(ipv6_addrs);
	size_t                          ipv6_idx = 0;
	size_t                          ipv4_cnt = M_list_str_len(ipv4_addrs);
	size_t                          ipv4_idx = 0;
	M_dns_happyeyeballs_sortlist_t *sortlist;
	size_t                          i;
	M_list_str_t                   *ret;

	if (ipv6_cnt + ipv4_cnt == 0)
		return NULL;

	sortlist = M_malloc_zero(sizeof(*sortlist) * (ipv6_cnt + ipv4_cnt));

	/* Insert alternating between ipv4 and ipv6 */
	while (ipv4_idx + ipv6_idx < ipv4_cnt + ipv6_cnt) {
		/* Even numbers are ipv6 if available, or if we've exhausted all ipv4 entries */
		if (((ipv4_idx + ipv6_idx) % 2 == 0 || ipv4_idx == ipv4_cnt) && ipv6_idx < ipv6_cnt) {
			sortlist[ipv4_idx + ipv6_idx].ipaddr = M_list_str_at(ipv6_addrs, ipv6_idx);
			sortlist[ipv4_idx + ipv6_idx].idx    = ipv4_idx + ipv6_idx;
			sortlist[ipv4_idx + ipv6_idx].status = M_dns_happyeyeballs_get(dns, sortlist[ipv4_idx + ipv6_idx].ipaddr, port);
			ipv6_idx++;
			continue;
		}

		/* Otherwise pull ipv4 */
		sortlist[ipv4_idx + ipv6_idx].ipaddr = M_list_str_at(ipv4_addrs, ipv4_idx);
		sortlist[ipv4_idx + ipv6_idx].idx    = ipv4_idx + ipv6_idx;
		sortlist[ipv4_idx + ipv6_idx].status = M_dns_happyeyeballs_get(dns, sortlist[ipv4_idx + ipv6_idx].ipaddr, port);
		ipv4_idx++;
	}

	/* Sort the list by  ASC status,  ASC idx */
	M_sort_qsort(sortlist, ipv4_cnt + ipv6_cnt, sizeof(*sortlist), M_dns_happyeyeballs_sort_compar, NULL);

	ret = M_list_str_create(M_LIST_STR_NONE);
	for (i=0; i<ipv4_cnt + ipv6_cnt; i++) {
		M_list_str_insert(ret, sortlist[i].ipaddr);
	}
	M_free(sortlist);

	return ret;
}



M_dns_t *M_dns_create(void)
{
	M_dns_t                 *dns;

	M_io_net_init_system();
	M_io_dns_init();

	dns                            = M_malloc_zero(sizeof(*dns));

	dns->max_query_ms              = 5000;  /* 5s */
	dns->query_timeout_ms          = 600;   /* 600ms */
	dns->query_tries               = 4;
	dns->server_cache_timeout_s    = 120;   /* 2 minutes */
	dns->server_cache_max_s        = 3600;  /* 1 hr */
	dns->query_cache_timeout_s     = 300;   /* 5 minutes */
	dns->query_cache_max_s         = 3600;  /* 1 hr */
	dns->happyeyeballs_cache_max_s = 600;   /* 10 minutes */

	if (!M_dns_reload_server(dns, M_TRUE)) {
		M_free(dns);
		return NULL;
	}

	dns->lock                  = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	dns->cache                 = M_queue_create(NULL /* Insertion sort order should be time based */, M_dns_cache_free_cb);
	dns->cache_lookup          = M_hash_strvp_create(16, 75, M_HASH_STRVP_NONE, NULL);
	dns->happyeyeballs.expired = M_llist_create(NULL, M_LLIST_NONE);
	dns->happyeyeballs.results = M_hash_strvp_create(16, 75, M_HASH_STRVP_CASECMP, M_dns_happyeyeballs_destroy_result);
	return dns;
}


M_bool M_dns_destroy(M_dns_t *dns)
{

	if (dns == NULL)
		return M_FALSE;

	if (dns->active_queries)
		return M_FALSE;

	ares_destroy(dns->base_channel);
	M_queue_destroy(dns->cache);
	M_hash_strvp_destroy(dns->cache_lookup, M_TRUE);

	/* NOTE: HappyEyeballs hash *must* be destroyed before the expire list since the
	 *       hashtable destroy will detach the entry from the expire list */
	M_hash_strvp_destroy(dns->happyeyeballs.results, M_TRUE);
	M_llist_destroy(dns->happyeyeballs.expired, M_TRUE);

	M_thread_mutex_destroy(dns->lock);
	M_free(dns);

	return M_TRUE;
}


static void M_dns_purge_stale(M_dns_t *dns)
{
	M_dns_entry_t     *entry;
	M_queue_foreach_t *q_foreach = NULL;
	M_time_t           t         = M_time();

	while (M_queue_foreach(dns->cache, &q_foreach, (void **)&entry)) {
		/* If we hit an entry with a non-expired cache, we can stop */
		if (entry->ipv6_cache_t + (M_time_t)dns->query_cache_max_s > t ||
			entry->ipv4_cache_t + (M_time_t)dns->query_cache_max_s > t) {
			break;
		}
		/* Delete the entry */
		M_hash_strvp_remove(dns->cache_lookup, entry->hostname, M_TRUE);
		M_queue_remove(dns->cache, entry);
	}
	M_queue_foreach_free(q_foreach);
}


static M_dns_entry_t *M_dns_get_entry(M_dns_t *dns, const char *hostname, M_bool for_update)
{
	M_dns_entry_t     *entry;

	M_dns_purge_stale(dns);

	entry = M_hash_strvp_get_direct(dns->cache_lookup, hostname);
	if (entry == NULL) {
		if (!for_update) {
			return NULL;
		}

		entry             = M_malloc_zero(sizeof(*entry));
		entry->dns        = dns;
		entry->hostname   = M_strdup(hostname);
		entry->ipv4_addrs = M_list_str_create(M_LIST_STR_NONE);
		entry->ipv6_addrs = M_list_str_create(M_LIST_STR_NONE);
		M_hash_strvp_insert(dns->cache_lookup, hostname, entry);
	} else {
		if (!for_update) {
			/* Check to see if we actually have results, if not, purge! */
			if (!M_list_str_len(entry->ipv4_addrs) && !M_list_str_len(entry->ipv6_addrs)) {
				M_hash_strvp_remove(dns->cache_lookup, hostname, M_TRUE);
				M_queue_take(dns->cache, entry);
				return NULL;
			}
			return entry;
		}

		/* Since we're updating, we need to remove it from the linkedlist as it needs to be re-added with a new cache time */
		M_queue_take(dns->cache, entry);
	}

	M_queue_insert(dns->cache, entry);

	return entry;
}


static void M_io_dns_ares_zeroupdate(M_io_handle_t *handle)
{
	M_llist_node_t *node = M_llist_first(handle->socklist);

	while (node != NULL) {
		M_dns_sock_handle_t *sock = M_llist_node_val(node);
		sock->updated             = M_FALSE;
		node                      = M_llist_node_next(node);
	}

}


static void M_io_dns_ares_removeclosed(M_io_t *io, M_io_handle_t *handle)
{
	M_llist_node_t *node = M_llist_first(handle->socklist);

	while (node != NULL) {
		M_llist_node_t      *next = M_llist_node_next(node);
		M_dns_sock_handle_t *sock = M_llist_node_val(node);
		if (!sock->updated) {
			M_event_handle_modify(M_io_get_event(io), M_EVENT_MODTYPE_DEL_HANDLE, io, sock->handle, sock->fd, 0, 0);
#ifdef _WIN32
			WSAEventSelect(sock->fd, sock->handle, 0);
			WSACloseEvent(sock->handle);
#endif
			(void)M_llist_take_node(node);
			M_free(sock);
		}
		node = next;
	}

}


static void M_io_dns_ares_add(M_io_t *io, M_io_handle_t *handle, ares_socket_t fd, M_event_wait_type_t waittype)
{
	M_llist_node_t      *node  = M_llist_first(handle->socklist);
	M_dns_sock_handle_t *sock;
	M_event_t           *event = M_io_get_event(io);

	while (node != NULL) {
		sock = M_llist_node_val(node);
		if (sock->fd == fd)
			break;
		node = M_llist_node_next(node);
	}

	/* Node exists, just being modified */
	if (node != NULL) {
		sock->updated = M_TRUE;
		if (waittype != sock->waittype) {
			if (waittype & ~sock->waittype)
				M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_WAITTYPE, io, sock->handle, sock->fd, waittype & ~sock->waittype, 0);
			if (sock->waittype & ~waittype)
				M_event_handle_modify(event, M_EVENT_MODTYPE_DEL_WAITTYPE, io, sock->handle, sock->fd, sock->waittype & ~waittype, 0);
			sock->waittype = waittype;
		}
		return;
	}

	/* Need to create a new node */
	sock           = M_malloc_zero(sizeof(*sock));
	sock->fd       = fd;
#ifdef _WIN32
	sock->handle   = WSACreateEvent();
	WSAEventSelect(sock->fd, sock->handle, FD_READ|FD_WRITE|FD_CLOSE);
#else
	sock->handle   = fd;
#endif
	sock->waittype = waittype;
	sock->updated  = M_TRUE;
	M_event_handle_modify(event, M_EVENT_MODTYPE_ADD_HANDLE, io, sock->handle, sock->fd, sock->waittype, M_EVENT_CAPS_READ|M_EVENT_CAPS_WRITE);
	M_llist_insert(handle->socklist, sock);
}


static void M_io_dns_ares_regsocks(M_io_t *io, M_io_handle_t *handle)
{
	ares_socket_t socks[ARES_GETSOCK_MAXNUM];
	int           mask;
	size_t        i;

	/* Initialize sockets to an invalid handle */
	for (i=0; i<ARES_GETSOCK_MAXNUM; i++)
		socks[i] = M_EVENT_INVALID_SOCKET;

	/* Retrieve socket list */
	mask = ares_getsock(handle->channel, socks, ARES_GETSOCK_MAXNUM);

	/* Mark all existing entries as not-updated so we can assume anything
	 * not in the update list has been closed */
	M_io_dns_ares_zeroupdate(handle);

	/* Iterate across list of sockets */
	for (i=0; i<ARES_GETSOCK_MAXNUM; i++) {
		if (socks[i] != M_EVENT_INVALID_SOCKET) {
			M_event_wait_type_t waittype = 0;
			if (ARES_GETSOCK_READABLE(mask, i))
				waittype |= M_EVENT_WAIT_READ;
			if (ARES_GETSOCK_WRITABLE(mask, i))
				waittype |= M_EVENT_WAIT_WRITE;

			/* Add or modify an existing socket entry */
			M_io_dns_ares_add(io, handle, socks[i], waittype);
		}
	}

	/* Remove sockets that weren't updated, they must be closed */
	M_io_dns_ares_removeclosed(io, handle);

}

static M_bool M_io_dns_process_cb(M_io_layer_t *layer, M_event_type_t *type);

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


static void M_io_dns_ares_update_timeout(M_io_layer_t *layer)
{
	struct timeval to;
	M_uint64       interval_ms;
	M_uint64       elapsed_ms;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	M_mem_set(&to, 0, sizeof(to));
	ares_timeout(handle->channel, NULL, &to);

	interval_ms = (M_uint64)((M_uint64)to.tv_sec * 1000) + (M_uint64)(to.tv_usec / 1000) + 1 /* +1 to handle conv from usec to msec, otherwise we may loop a few times consuming cpu */;
	elapsed_ms  = M_time_elapsed(&handle->start_tv);

	/* We went over so timeout is expired, but we need to do math on it so make it equal to the max */
	if (elapsed_ms > handle->dns->max_query_ms)
		elapsed_ms = handle->dns->max_query_ms;

	/* Timeout needs to be the lesser of the C-Ares requested timeout and the max_query_ms config value,
	 * unless C-Ares is no longer doing processing, then we just set it to the config value ... it 
	 * shouldn't really be used anyhow at this point as it should be processing cleanups. */
	if (handle->dns->max_query_ms - elapsed_ms < interval_ms || handle->num_queries == 0)
		interval_ms = handle->dns->max_query_ms - elapsed_ms;

	/* Update timer value */
	if (interval_ms == 0) {
		M_event_timer_stop(handle->timer);
		M_event_timer_start(handle->timer, interval_ms);
	} else {
		M_event_timer_reset(handle->timer, interval_ms);
	}

}


static void M_io_dns_handle_netbios(M_io_handle_t *handle)
{
#ifdef _WIN32
	struct hostent *h;
	char            addr[128];
	M_dns_entry_t  *entry;

	if (handle->result == M_DNS_RESULT_SUCCESS || handle->result == M_DNS_RESULT_SUCCESS_CACHE)
		return;

	/* If a name is provided without a period, or ends in ".local", it might be a netbios name,
	 * we need to hand this off to the system's gethostbyname.  This COULD block, its definitely
	 * not optimal. */
	if (M_str_isempty(handle->hostname) || (M_str_chr(handle->hostname, '.') != NULL && !M_str_caseeq(handle->hostname + M_str_len(handle->hostname) - 6, ".local")))
		return;

	h = gethostbyname(handle->hostname);
	if (h == NULL || h->h_length <= 0 || h->h_addr_list[0] == NULL || (h->h_addrtype != AF_INET
#  ifdef AF_INET6
	    && h->h_addrtype != AF_INET6
#  endif
	    )) {
		return;
	}

	/* Convert into normalized string form, we only care about first address */
	if (!M_dns_ntop(h->h_addrtype, h->h_addr_list[0], addr, sizeof(addr)))
		return;

	/* Lookup or create new entry */
	entry = M_dns_get_entry(handle->dns, handle->hostname, M_TRUE);

	M_list_str_insert((h->h_addrtype == AF_INET)?entry->ipv4_addrs:entry->ipv6_addrs, addr);

	if (h->h_addrtype == AF_INET) {
		entry->ipv4_cache_t = M_time();
	} else {
		entry->ipv6_cache_t = M_time();
	}

	handle->result = M_DNS_RESULT_SUCCESS;
#else
	(void)handle;
#endif
}


static void M_io_dns_finish_cb(M_event_t *event, M_event_type_t type, M_io_t *io_bad, void *arg)
{
	M_io_handle_t *handle = arg;
	M_io_t        *io     = handle->io;

	(void)io_bad;
	(void)type;
	(void)event;

	handle->callback(handle->result_ips, handle->cb_data, handle->result);
	
	/* Clean up, delete self.  This should be safe since we're allowed to in an event handler. */
	M_io_destroy(io);
}


static M_bool M_io_dns_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_llist_node_t *node;
	M_io_handle_t  *handle = M_io_layer_get_handle(layer);
	M_io_t         *io     = M_io_layer_get_io(layer);
	size_t          cnt    = 0;

	if (*type == M_EVENT_TYPE_READ || *type == M_EVENT_TYPE_WRITE) {
		/* We don't know which socket notified, most likely there's
		 * only one or two sockets, just enumerate them and notify
		 * on all */
		node = M_llist_first(handle->socklist);
		while (node != NULL) {
			M_dns_sock_handle_t *sock = M_llist_node_val(node);
			ares_process_fd(handle->channel, (*type == M_EVENT_TYPE_READ)?sock->fd:ARES_SOCKET_BAD, (*type == M_EVENT_TYPE_WRITE)?sock->fd:ARES_SOCKET_BAD);
			cnt++;

			node = M_llist_node_next(node);
		}
	}

	if (cnt == 0) {
		/* Always call ares_process_fd() as we may need to call it with bad handles during
		 * timeout conditions */
		ares_process_fd(handle->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
	}

	/* Check to see if there was a max query timeout and pending queries */
	if (M_time_elapsed(&handle->start_tv) >= handle->dns->max_query_ms && handle->num_queries) {
		ares_cancel(handle->channel);
	}

	/* Re-evaluate socket list from ares_getsock() */
	M_io_dns_ares_regsocks(io, handle);

	/* Re-evaluate timer from ares_timeout() */
	M_io_dns_ares_update_timeout(layer);

	/* Consume -- though there should be no one to deliver to */
	return M_TRUE;
}


static void M_io_dns_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t  *handle = M_io_layer_get_handle(layer);
	M_io_t         *io     = M_io_layer_get_io(layer);
	M_event_t      *event  = M_io_get_event(io);

	if (io == NULL || event == NULL)
		return;

	/* Unregister any registered ares handles */
	M_io_dns_ares_zeroupdate(handle);
	M_io_dns_ares_removeclosed(io, handle);

	/* Destroy any timers */
	M_event_timer_remove(handle->timer);
	handle->timer = NULL;
}


static void M_io_dns_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	/* Free the ares_channel */
	ares_destroy(handle->channel);

	M_free(handle->hostname);
	M_llist_destroy(handle->socklist, M_TRUE);
	M_thread_mutex_lock(handle->dns->lock);
	handle->dns->active_queries--;
	M_thread_mutex_unlock(handle->dns->lock);
	M_list_str_destroy(handle->result_ips);

	M_free(handle);
}

static void M_io_dns_ares_host_callback(void *arg, int status, int timeouts, struct hostent *hostent, M_bool is_ipv6)
{
	M_io_handle_t *handle = arg;
	M_dns_entry_t *entry  = NULL;

	(void)timeouts; /* We don't care how many times a query timedout */
	/* If we receive a destruction notification, that means the user is cleaning us up
	 * and we should NOT trigger a callback or any cleanup operations */
	if (status == ARES_EDESTRUCTION)
		return;

	M_thread_mutex_lock(handle->dns->lock);

	/* Save the query result */
	if (status == ARES_SUCCESS) {
		if (hostent != NULL && hostent->h_length > 0 && hostent->h_addr_list[0] != NULL && (hostent->h_addrtype == AF_INET
#ifdef AF_INET6
 || hostent->h_addrtype == AF_INET6
#endif
		    )) {
			size_t         i;

			/* Lookup or create new entry */
			entry = M_dns_get_entry(handle->dns, handle->hostname, M_TRUE);

			/* Clear existing entries */
			while (M_list_str_remove_first((hostent->h_addrtype == AF_INET)?entry->ipv4_addrs:entry->ipv6_addrs))
				;

			for (i=0; hostent->h_addr_list[i] != NULL; i++) {
				char addr[128];

				/* Convert into normalized string form */
				if (!M_dns_ntop(hostent->h_addrtype, hostent->h_addr_list[i], addr, sizeof(addr)))
					continue;

				M_list_str_insert((hostent->h_addrtype == AF_INET)?entry->ipv4_addrs:entry->ipv6_addrs, addr);
//M_printf("%s(): add %s\n", __FUNCTION__, addr);
			}

			if (is_ipv6) {
				entry->ipv6_cache_t = M_time();
			} else {
				entry->ipv4_cache_t = M_time();
			}
		} else {
			/* Simulate host not found error for empty address list */
			if (hostent != NULL && (hostent->h_addrtype == AF_INET 
#ifdef AF_INET6
|| hostent->h_addrtype == AF_INET6
#endif
			    ) && hostent->h_addr_list[0] == NULL) {
				status = ARES_ENOTFOUND;
			} else {
				/* Simulate error for unrecognized result */
				status = ARES_ENOTIMP;
			}
		}
	}

	/* Save the first error encountered, or success if there was a successful transaction */
	if (handle->result == M_DNS_RESULT_INVALID || status == ARES_SUCCESS) {
		switch (status) {
			case ARES_SUCCESS:
				handle->result = M_DNS_RESULT_SUCCESS;
				break;
			case ARES_EBADNAME:
			case ARES_ENOTFOUND:
				handle->result = M_DNS_RESULT_NOTFOUND;
				break;
			case ARES_ECANCELLED:
				handle->result = M_DNS_RESULT_TIMEOUT;
				break;
			case ARES_ENOTIMP:
			case ARES_ENOMEM:
			default:
				handle->result = M_DNS_RESULT_SERVFAIL;
				break;
		}
	}

	/* Clear old entries only when we receive a response from the DNS server stating our cache can't be valid.  */
	if (status == ARES_EBADNAME || status == ARES_ENOTFOUND) {
		entry = M_dns_get_entry(handle->dns, handle->hostname, M_FALSE);
		if (entry != NULL) {
			if (is_ipv6) {
				while (M_list_str_remove_first(entry->ipv6_addrs))
					;
			}
			if (!is_ipv6) {
				while (M_list_str_remove_first(entry->ipv4_addrs))
					;
			}
		}
	}

	/* Clear stale entries if they have exceeded the maximum cache time */
	if (status != ARES_SUCCESS) {
		entry = M_dns_get_entry(handle->dns, handle->hostname, M_FALSE);
		if (entry != NULL) {
			if (is_ipv6 && entry->ipv6_cache_t + (M_time_t)handle->dns->query_cache_max_s < M_time()) {
				while (M_list_str_remove_first(entry->ipv6_addrs))
					;
			}
			if (!is_ipv6 && entry->ipv4_cache_t + (M_time_t)handle->dns->query_cache_max_s < M_time()) {
				while (M_list_str_remove_first(entry->ipv4_addrs))
					;
			}
		}
	}

	handle->num_responses++;
	if (handle->num_queries == handle->num_responses) {
		/* This will check for failures and if we think it is a netbios name, we'll delegate
		 * to netbios */
		M_io_dns_handle_netbios(handle);

		/* Look up entry DNS results if any, regardless of what handle->result is set to.
		 * NOTE: The act of looking up entries will actually purge anything that is past the
		 * maximum cache time, so we don't have to worry about checking that or manually
		 * purging. */
		entry = M_dns_get_entry(handle->dns, handle->hostname, M_FALSE);
		if (entry != NULL) {
			if ( ( (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV6) && M_list_str_len(entry->ipv6_addrs) )  ||
			     ( (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV4) && M_list_str_len(entry->ipv4_addrs) )
			   ) {
				/* Even though we might have gotten a failure from the DNS server, we can
				 * still return cached results if we have them */
				if (handle->result != M_DNS_RESULT_SUCCESS)
					handle->result = M_DNS_RESULT_SUCCESS_CACHE;

				handle->result_ips = M_dns_happyeyeballs_sort(handle->dns,
				                                              (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV6)?entry->ipv6_addrs:NULL,
				                                              (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV4)?entry->ipv4_addrs:NULL,
				                                              handle->port);
			} else {
				if (handle->result == M_DNS_RESULT_SUCCESS)
					handle->result = M_DNS_RESULT_NOTFOUND;
			}
		}

		
		/* Run task to clean up self and notify DNS is finished */
		M_event_queue_task(M_io_get_event(handle->io), M_io_dns_finish_cb, handle);
	}
	M_thread_mutex_unlock(handle->dns->lock);
}


static void M_io_dns_ares_host_callback6(void *arg, int status, int timeouts, struct hostent *hostent)
{
	M_io_dns_ares_host_callback(arg, status, timeouts, hostent, M_TRUE);
}


static void M_io_dns_ares_host_callback4(void *arg, int status, int timeouts, struct hostent *hostent)
{
	M_io_dns_ares_host_callback(arg, status, timeouts, hostent, M_FALSE);
}


static M_io_state_t M_io_dns_state_cb(M_io_layer_t *layer)
{
	(void)layer;
	return M_IO_STATE_CONNECTED;
}


static M_bool M_io_dns_init_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	/* Start IPv4 and IPv6 queries */
	handle->num_queries = 1;

	if (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV6) {
#ifdef AF_INET6
		handle->num_queries++;
//M_printf("%s(): started query for ipv6\n", __FUNCTION__);
		ares_gethostbyname(handle->channel, handle->hostname, AF_INET6, M_io_dns_ares_host_callback6, handle);
#endif
	}
	if (handle->type == M_IO_NET_ANY || handle->type == M_IO_NET_IPV4) {
//M_printf("%s(): started query for ipv4\n", __FUNCTION__);
		ares_gethostbyname(handle->channel, handle->hostname, AF_INET,  M_io_dns_ares_host_callback4, handle);
	}

	/* Register ares handles with the event subsystem */
	M_io_dns_ares_regsocks(io, handle);

	/* No handles could legitimately be returned if a dns lookup was 100% local (/etc/hosts).
	 * lets not fail anymore */
#if 0
	if (M_llist_len(handle->socklist) == 0) {
		return M_FALSE;
	}
#endif

	/* Create a timer */
	handle->timer = M_event_timer_add(event, M_io_dns_timeout_cb, layer);
	M_event_timer_set_firecount(handle->timer, 1);

	/* Update timer for Ares timeouts */
	M_io_dns_ares_update_timeout(layer);

	return M_TRUE;
}


static M_io_t *M_io_dns_create(M_dns_t *dns, M_event_t *event, const char *hostname, M_uint16 port, M_io_net_type_t type, M_io_dns_callback_t callback, void *cb_data)
{
	M_io_t           *io;
	M_io_handle_t    *handle;
	ares_channel      channel;
	M_io_callbacks_t *callbacks;

	if (dns == NULL || M_str_isempty(hostname) || callback == NULL)
		return NULL;

	if (type == M_IO_NET_IPV6) {
#ifndef AF_INET6
		return NULL;
#endif
	}

	M_thread_mutex_lock(dns->lock);
	if (!M_dns_reload_server(dns, M_FALSE)) {
		M_thread_mutex_unlock(dns->lock);
		return NULL;
	}
	ares_dup(&channel, dns->base_channel);
	dns->active_queries++;
	M_thread_mutex_unlock(dns->lock);

	handle           = M_malloc_zero(sizeof(*handle));
	handle->hostname = M_strdup(hostname);
	handle->port     = port;
	handle->callback = callback;
	handle->cb_data  = cb_data;
	handle->dns      = dns;
	handle->type     = type;
	M_time_elapsed_start(&handle->start_tv);
	handle->socklist = M_llist_create(NULL, M_LLIST_NONE);
	handle->channel  = channel;
	handle->result   = M_DNS_RESULT_INVALID;


	io        = M_io_init(M_IO_TYPE_STREAM);
	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_dns_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_dns_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_dns_unregister_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_dns_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_dns_state_cb);
	M_io_layer_add(io, "DNS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);
	handle->io = io;
	M_event_add(event, io, NULL, NULL);

	return io;
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
	if (parts == NULL || num_parts == 0)
		return NULL;

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


M_io_t *M_dns_gethostbyname(M_dns_t *dns, M_event_t *event, const char *hostname, M_uint16 port, M_io_net_type_t type, M_io_dns_callback_t callback, void *cb_data)
{
	M_dns_entry_t *entry;
	M_io_t        *io;
	char          *punyhost = NULL;
	int            aftype;

	if (callback == NULL)
		return NULL;

	if (M_str_isempty(hostname)) {
		callback(NULL, cb_data, M_DNS_RESULT_INVALID);
		return NULL;
	}

	/* Requested hostname is an IP address, make a fake entry and call callback */
	if (M_dns_host_is_addr(hostname, &aftype)) {
		M_list_str_t *ipaddr;

		if (aftype == AF_INET && type == M_IO_NET_IPV6) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return NULL;
		}
#ifdef AF_INET6
		if (aftype == AF_INET6 && type == M_IO_NET_IPV4) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return NULL;
		}
#endif

		ipaddr = M_list_str_create(M_LIST_STR_NONE);
		M_list_str_insert(ipaddr, hostname);
		callback(ipaddr, cb_data, M_DNS_RESULT_SUCCESS);
		M_list_str_destroy(ipaddr);
		return NULL;
	}


	/* Windows no longer has "localhost" in the etc/hosts file, so we need to
	 * always return the IPv4 and IPv6 entries, otherwise it queries the DNS
	 * server.  See:
	 * https://daniel.haxx.se/blog/2011/02/21/localhost-hack-on-windows/
	 * It appears Linux may also not reliably work here, so we always just make
	 * localhost sane.
	 */
	if (M_str_caseeq(hostname, "localhost")) {
		M_list_str_t *ip6addr = NULL;
		M_list_str_t *ip4addr = NULL;
		M_list_str_t *ipaddrs = NULL;

		if (type == M_IO_NET_IPV4 || type == M_IO_NET_ANY) {
			ip4addr = M_list_str_create(M_LIST_STR_NONE);
			M_list_str_insert(ip4addr, "127.0.0.1");
		}
#  ifdef AF_INET6
		if (type == M_IO_NET_IPV6 || type == M_IO_NET_ANY) {
			ip6addr = M_list_str_create(M_LIST_STR_NONE);
			M_list_str_insert(ip6addr, "::1");
		}
#  endif

		if (dns)
			M_thread_mutex_lock(dns->lock);

		ipaddrs = M_dns_happyeyeballs_sort(dns, ip6addr, ip4addr, port);

		if (dns)
			M_thread_mutex_unlock(dns->lock);

		callback(ipaddrs, cb_data, M_DNS_RESULT_SUCCESS);
		M_list_str_destroy(ip6addr);
		M_list_str_destroy(ip4addr);
		M_list_str_destroy(ipaddrs);
		return NULL;
	}

	/* We need to do a real DNS lookup, so we'd better have what we need */
	if (dns == NULL || event == NULL) {
		callback(NULL, cb_data, M_DNS_RESULT_INVALID);
		return NULL;
	}

	/* IDNA host name if necessary because lookups should always be
 	 * ASCII only. Which is the purpouse of IDNA punycode encoding
	 * domains that have non-ASCII characters. */
	if (!M_str_isascii(hostname)) {
		/* Punycode encode international domain names. */
		punyhost = M_dns_punyhostname(hostname);
		if (punyhost == NULL) {
			callback(NULL, cb_data, M_DNS_RESULT_INVALID);
			return NULL;
		}
		hostname = punyhost;
	}

	M_thread_mutex_lock(dns->lock);
	entry = M_dns_get_entry(dns, hostname, M_FALSE);
	if (entry != NULL && (entry->ipv4_cache_t + (M_time_t)dns->query_cache_timeout_s > M_time() || entry->ipv6_cache_t + (M_time_t)dns->query_cache_timeout_s > M_time())) {
		M_list_str_t *ipaddrs = M_dns_happyeyeballs_sort(dns, entry->ipv6_addrs, entry->ipv4_addrs, port);
		M_thread_mutex_unlock(dns->lock);

		callback(ipaddrs, cb_data, M_DNS_RESULT_SUCCESS_CACHE);
		M_list_str_destroy(ipaddrs);
		M_free(punyhost);
		return NULL;
	}
	M_thread_mutex_unlock(dns->lock);

	io = M_io_dns_create(dns, event, hostname, port, type, callback, cb_data);
	if (io == NULL) {
		callback(NULL, cb_data, M_DNS_RESULT_INVALID);
		M_free(punyhost);
		return NULL;
	}

	M_free(punyhost);
	return io;
}


M_bool M_dns_set_query_timeout(M_dns_t *dns, M_uint64 timeout_ms)
{
	if (dns == NULL)
		return M_FALSE;

	if (timeout_ms == 0)
		timeout_ms = 5000;

	M_thread_mutex_lock(dns->lock);
	dns->max_query_ms = timeout_ms;
	M_thread_mutex_unlock(dns->lock);
	return M_TRUE;
}


M_bool M_dns_set_cache_timeout(M_dns_t *dns, M_uint64 timeout_s, M_uint64 max_timeout_s)
{
	if (dns == NULL)
		return M_FALSE;

	if (timeout_s == 0)
		timeout_s = 300; /* 5 minutes */

	if (max_timeout_s == 0)
		max_timeout_s = 3600; /* 1 hr */

	M_thread_mutex_lock(dns->lock);
	dns->query_cache_timeout_s = timeout_s;
	dns->query_cache_max_s     = max_timeout_s;
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
