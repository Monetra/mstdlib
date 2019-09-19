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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include "m_event_int.h"
#include "base/m_defs_int.h"
#include "m_io_net_int.h"
#include "m_dns_int.h"
#include "m_io_int.h"

static M_io_error_t M_io_netdns_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;

	if (handle->data.netdns.io == NULL)
		return M_IO_ERROR_INVALID;

	if (handle->hard_down && handle->state != M_IO_NET_STATE_CONNECTED) {
		if (handle->state == M_IO_NET_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return M_IO_ERROR_ERROR;
	}

	/* Relay to underlying io object */
	err = M_io_read_meta(handle->data.netdns.io, buf, *read_len, read_len, meta);
	if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
		if (err == M_IO_ERROR_DISCONNECT) {
			handle->state = M_IO_NET_STATE_DISCONNECTED;
		} else {
			handle->state = M_IO_NET_STATE_ERROR;
		}
		handle->hard_down = M_TRUE;
	}

	return err;
}


static M_io_error_t M_io_netdns_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;

	if (handle->data.netdns.io == NULL)
		return M_IO_ERROR_INVALID;

	if (handle->state != M_IO_NET_STATE_CONNECTED && handle->state != M_IO_NET_STATE_DISCONNECTING) {
		if (handle->state == M_IO_NET_STATE_DISCONNECTED)
			return M_IO_ERROR_DISCONNECT;
		return M_IO_ERROR_ERROR;
	}

	/* Relay to io object */
	err = M_io_write_meta(handle->data.netdns.io, buf, *write_len, write_len, meta);
	if (err != M_IO_ERROR_SUCCESS && err != M_IO_ERROR_WOULDBLOCK) {
		handle->hard_down = M_TRUE;
		if (err == M_IO_ERROR_DISCONNECT) {
			handle->state = M_IO_NET_STATE_DISCONNECTED;
		} else {
			handle->state = M_IO_NET_STATE_ERROR;
		}
	}

	return err;
}


static M_bool M_io_netdns_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

//M_printf("%s(): [%p] event %p io %p type %d\n", __FUNCTION__, (void *)M_thread_self(), M_io_get_event(M_io_layer_get_io(layer)), M_io_layer_get_io(layer), (int)*type);

	/* We'll only really get soft events, so we're going to just use this to ignore soft events
	 * that children shouldn't get */

	/* Consume write events while disconnecting */
	if (handle->state == M_IO_NET_STATE_DISCONNECTING && *type == M_EVENT_TYPE_WRITE)
		return M_TRUE;

	/* Modify internal state */
	if (*type == M_EVENT_TYPE_DISCONNECTED)
		handle->state = M_IO_NET_STATE_DISCONNECTED;
	if (*type == M_EVENT_TYPE_ERROR)
		handle->state = M_IO_NET_STATE_ERROR;

	return M_FALSE;
}

static void M_io_netdns_realio_cb(M_event_t *event, M_event_type_t type, M_io_t *realio, void *arg);

static size_t M_io_netdns_next_io_idx(M_io_handle_t *handle)
{
	size_t i;
	for (i=handle->data.netdns.io_try_idx+1; i<handle->data.netdns.io_try_cnt; i++) {
		if (handle->data.netdns.io_try[i] != NULL)
			return i;
	}
	return 0;
}

static M_bool M_io_netdns_next_io_start(M_io_layer_t *layer);

static void M_io_netdns_happyeyeballs_timeout(M_event_t *event, M_event_type_t type, M_io_t *iodummy, void *arg)
{
	M_io_layer_t *layer = arg;

	(void)event;
	(void)iodummy;
	(void)type;

	M_io_netdns_next_io_start(layer);
}


static void M_io_netdns_happyeyeballs_timer(M_io_layer_t *layer)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	M_io_t        *io      = M_io_layer_get_io(layer);
	M_event_t     *event   = M_io_get_event(io);
	size_t         nextidx = M_io_netdns_next_io_idx(handle);

	/* Destroy happyeyeballs timer */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	if (nextidx == 0)
		return;

	handle->timer = M_event_timer_oneshot(event, handle->settings.connect_failover_ms, M_FALSE, M_io_netdns_happyeyeballs_timeout, layer);
}


static M_bool M_io_netdns_next_io_start(M_io_layer_t *layer)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	size_t         nextidx = M_io_netdns_next_io_idx(handle);
	M_io_t        *io      = M_io_layer_get_io(layer);
	M_event_t     *event   = M_io_get_event(io);
	M_bool         rv      = M_FALSE;

	if (nextidx != 0) {
		handle->data.netdns.io_try_idx = nextidx;
		M_event_add(event, handle->data.netdns.io_try[nextidx], M_io_netdns_realio_cb, io);
		rv = M_TRUE;
	}

	/* Always call, it might just disable the timer */
	M_io_netdns_happyeyeballs_timer(layer);

	return rv;
}


static size_t M_io_netdns_find_io(M_io_handle_t *handle, M_io_t *realio)
{
	size_t         i;

	for (i=0; i<=handle->data.netdns.io_try_idx; i++) {
		if (handle->data.netdns.io_try[i] == realio)
			return i;
	}
	return 0;
}

static void M_io_netdns_handle_connect(M_io_layer_t *layer, M_io_t *realio)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	size_t         idx     = M_io_netdns_find_io(handle, realio);
	size_t         i;

	/* If doing multiple simultaneous connections, first one wins, destroy the others,
	 * and set our io object pointer.  Also destroy the timer so it doesn't
	 * try to fire off another connection */

	/* Close any older sibilings and mark them as slow */
	for (i=0; i<idx; i++) {
		if (handle->data.netdns.io_try[i] != NULL) {
			M_dns_happyeyeballs_update(handle->data.netdns.dns, M_io_net_get_ipaddr(handle->data.netdns.io_try[i]), handle->port, M_HAPPYEYEBALLS_STATUS_SLOW);
			M_io_destroy(handle->data.netdns.io_try[i]);
			handle->data.netdns.io_try[i] = NULL;
		}
	}

	/* Close any younger siblings, no need to mark them as anything */
	for (i=idx+1; i<handle->data.netdns.io_try_cnt; i++) {
		if (handle->data.netdns.io_try[i] != NULL) {
			M_io_destroy(handle->data.netdns.io_try[i]);
			handle->data.netdns.io_try[i] = NULL;
		}
	}

	/* Destroy any timer since we won't need it */
	if (handle->timer != NULL) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	/* Mark the connection as successful for happyeyeballs tracking then set our internal state and
	 * notify the connection was successful */
	M_dns_happyeyeballs_update(handle->data.netdns.dns, M_io_net_get_ipaddr(realio), handle->port, M_HAPPYEYEBALLS_STATUS_GOOD);
	handle->data.netdns.io           = realio;
	handle->state                    = M_IO_NET_STATE_CONNECTED;
	handle->data.netdns.connect_time = M_time_elapsed(&handle->data.netdns.connect_start);
	M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);

	/* Clean up */
	M_free(handle->data.netdns.io_try);
	handle->data.netdns.io_try     = NULL;
	handle->data.netdns.io_try_cnt = 0;
	handle->data.netdns.io_try_idx = 0;
}

static size_t M_io_netdns_io_count_valid(M_io_handle_t *handle)
{
	size_t i;
	size_t cnt = 0;
	for (i=0; i<handle->data.netdns.io_try_cnt; i++) {
		if (handle->data.netdns.io_try[i] != NULL)
			cnt++;
	}
	return cnt;
}


static void M_io_netdns_handle_connect_error(M_io_layer_t *layer, M_io_t *realio)
{
	M_io_handle_t *handle  = M_io_layer_get_handle(layer);
	size_t         idx     = M_io_netdns_find_io(handle, realio);

	/* Mark as bad for happy eyeballs tracking */
	M_dns_happyeyeballs_update(handle->data.netdns.dns, M_io_net_get_ipaddr(handle->data.netdns.io_try[idx]), handle->port, M_HAPPYEYEBALLS_STATUS_BAD);

	/* Start next connection to next ip in line */
	if (!M_io_netdns_next_io_start(layer) && M_io_netdns_io_count_valid(handle) == 1) {
		/* No more layers, don't destroy io object as we might want to pull metadata from it */
		handle->data.netdns.io           = realio;
		handle->state                    = M_IO_NET_STATE_ERROR;
		handle->data.netdns.connect_time = M_time_elapsed(&handle->data.netdns.connect_start);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_io_get_error(realio));

		/* Clean up */
		M_free(handle->data.netdns.io_try);
		handle->data.netdns.io_try     = NULL;
		handle->data.netdns.io_try_cnt = 0;
		handle->data.netdns.io_try_idx = 0;
		return;
	}

	/* Destroy self */
	M_io_destroy(handle->data.netdns.io_try[idx]);
	handle->data.netdns.io_try[idx] = NULL;
}


static void M_io_netdns_realio_cb(M_event_t *event, M_event_type_t type, M_io_t *realio, void *arg)
{
	M_io_t        *io     = arg;
	M_io_layer_t  *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	(void)event;
//M_printf("%s(): [%p] event %p io %p type %d realio %p\n", __FUNCTION__, (void *)M_thread_self(), event, io, (int)type, realio);

	/* If already disconnected or in error state, nothing to do.  Not sure why we'd get this */
	if (handle->state == M_IO_NET_STATE_DISCONNECTED || handle->state == M_IO_NET_STATE_ERROR)
		goto done;

	switch (type) {
		case M_EVENT_TYPE_CONNECTED:
			M_io_netdns_handle_connect(layer, realio);
			break;

		case M_EVENT_TYPE_READ:
		case M_EVENT_TYPE_WRITE:
			/* Pass-on */
			M_io_layer_softevent_add(layer, M_FALSE /* Self, must be same as below or order of events may be reversed, bad! */, type, M_IO_ERROR_SUCCESS);
			break;

		case M_EVENT_TYPE_DISCONNECTED:
			/* Relay to self, we won't change our own state until we receive it from
			 * M_io_netdns_process_cb() as it will properly re-order events to make
			 * sure a read event is delivered first so a user can read */
			M_io_layer_softevent_add(layer, M_FALSE /* Self */, M_EVENT_TYPE_DISCONNECTED, M_IO_ERROR_DISCONNECT);
			break;

		case M_EVENT_TYPE_ERROR:
			if (handle->state == M_IO_NET_STATE_CONNECTING) {
				M_io_netdns_handle_connect_error(layer, realio);
			} else {
				/* Relay to self, we won't change our own state until we receive it from
				 * M_io_netdns_process_cb() as it will properly re-order events to make
				 * sure a read event is delivered first so a user can read */
				M_io_layer_softevent_add(layer, M_FALSE /* Self */, M_EVENT_TYPE_ERROR, M_io_get_error(realio));
			}
			break;

		case M_EVENT_TYPE_OTHER:
		case M_EVENT_TYPE_ACCEPT:
			/* Should not be possible to get these, ignore */
			break;
	}

done:
	M_io_layer_release(layer);
}


static M_bool M_io_netdns_init_connect(M_io_layer_t *layer)
{
	M_io_handle_t *handle      = M_io_layer_get_handle(layer);
	M_io_t        *io          = M_io_layer_get_io(layer);
	M_event_t     *event       = M_io_get_event(io);
	size_t         first_idx   = SIZE_MAX;
	size_t         i;

	for (i=0; i<handle->data.netdns.io_try_cnt; i++) {
		if (handle->data.netdns.io_try[i] != NULL) {
			first_idx = i;
			break;
		}
	}
	if (first_idx == SIZE_MAX)
		return M_FALSE;

	handle->state = M_IO_NET_STATE_CONNECTING;
	M_time_elapsed_start(&handle->data.netdns.connect_start);

	M_event_add(event, handle->data.netdns.io_try[first_idx], M_io_netdns_realio_cb, io);
	handle->data.netdns.io_try_idx = first_idx;
	M_io_netdns_happyeyeballs_timer(layer);

	return M_TRUE;
}


static void M_io_netdns_dns_callback(const M_list_str_t *ips, void *cb_data, M_dns_result_t result)
{
	M_io_layer_t  *layer       = cb_data;
	M_io_handle_t *handle      = M_io_layer_get_handle(layer);
	size_t         i;

	handle->data.netdns.io_dns     = NULL;
	handle->data.netdns.query_time = M_time_elapsed(&handle->data.netdns.query_start);
	if (result != M_DNS_RESULT_SUCCESS && result != M_DNS_RESULT_SUCCESS_CACHE) {
		handle->state = M_IO_NET_STATE_ERROR;
		switch (result) {
			case M_DNS_RESULT_SERVFAIL:
				M_snprintf(handle->data.netdns.error, sizeof(handle->data.netdns.error), "%s", "DNS Server Failure");
				break;
			case M_DNS_RESULT_NOTFOUND:
				M_snprintf(handle->data.netdns.error, sizeof(handle->data.netdns.error), "%s", "Host not found");
				break;
			case M_DNS_RESULT_TIMEOUT:
				M_snprintf(handle->data.netdns.error, sizeof(handle->data.netdns.error), "%s", "DNS Timeout");
				break;
			case M_DNS_RESULT_INVALID:
				M_snprintf(handle->data.netdns.error, sizeof(handle->data.netdns.error), "%s", "DNS Invalid Request");
				break;
			/* Not possible */
			case M_DNS_RESULT_SUCCESS:
			case M_DNS_RESULT_SUCCESS_CACHE:
				break;

		}
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_NOTFOUND /* TODO: do we need more error codes? */);
		return;
	}

	/* Initialize */
	handle->data.netdns.io_try_cnt = M_list_str_len(ips);
	if (handle->data.netdns.io_try_cnt) {
		handle->data.netdns.io_try = M_malloc_zero(sizeof(*handle->data.netdns.io_try) * handle->data.netdns.io_try_cnt);
		for (i=0; i<handle->data.netdns.io_try_cnt; i++) {
			handle->data.netdns.io_try[i] = M_io_netraw_client_create(M_list_str_at(ips, i), handle->port, M_IO_NET_ANY);
			if (handle->data.netdns.io_try[i] != NULL) {
				M_io_net_set_settings(handle->data.netdns.io_try[i], &handle->settings);
			}
		}
	}

	/* Failure to initialize */
	if (!M_io_netdns_init_connect(layer)) {
		handle->state = M_IO_NET_STATE_ERROR;
		M_snprintf(handle->data.netdns.error, sizeof(handle->data.netdns.error), "%s", "Unable to start IP connection");
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, M_IO_ERROR_NOTFOUND /* Only reason it couldn't start is if there were no ips */);
		return;
	}
}



static M_bool M_io_netdns_init_cb(M_io_layer_t *layer)
{
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_event_t     *event  = M_io_get_event(io);

	switch (handle->state) {
		case M_IO_NET_STATE_INIT:
			/* Start DNS lookup */
			handle->state              = M_IO_NET_STATE_RESOLVING;
//M_printf("%s(): looking up %s\n", __FUNCTION__, handle->host);
			M_time_elapsed_start(&handle->data.netdns.query_start);
			handle->data.netdns.io_dns = M_dns_gethostbyname(handle->data.netdns.dns, event, handle->host, handle->port, handle->type, M_io_netdns_dns_callback, layer);
			break;
		case M_IO_NET_STATE_CONNECTING:
			/* Re-bind io event handle(s) */
			if (!M_io_netdns_init_connect(layer))
				return M_FALSE;
			break;
		case M_IO_NET_STATE_CONNECTED:
//M_printf("%s(): already connected\n", __FUNCTION__); fflush(stdout);
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
			/* Fallthrough */
		case M_IO_NET_STATE_DISCONNECTING:
			/* Re-bind io event handle */
			M_event_add(event, handle->data.netdns.io, M_io_netdns_realio_cb, io);
			break;
		case M_IO_NET_STATE_DISCONNECTED:
		case M_IO_NET_STATE_ERROR:
		case M_IO_NET_STATE_RESOLVING: /* Not possible */
		case M_IO_NET_STATE_LISTENING: /* Not possible */
			/* Do Nothing */
			break;
	}
	return M_TRUE;
}


static M_bool M_io_netdns_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	if (handle->data.netdns.io == NULL || handle->state != M_IO_NET_STATE_CONNECTED) {
		/* If already requested a disconnect, tell caller to wait longer */
		if (handle->state == M_IO_NET_STATE_DISCONNECTING)
			return M_FALSE;
		return M_TRUE;
	}

	handle->state = M_IO_NET_STATE_DISCONNECTING;

	/* Relay to io object */
	M_io_disconnect(handle->data.netdns.io);
	return M_FALSE;
}


static void M_io_netdns_unregister_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* Destroy any happy eyeballs timer objects */
	if (handle->timer) {
		M_event_timer_remove(handle->timer);
		handle->timer = NULL;
	}

	/* If connecting, remove all bound io objects from event handle */
	if (handle->state == M_IO_NET_STATE_CONNECTING) {
		size_t i;
		for (i=0; i<=handle->data.netdns.io_try_idx; i++) {
			if (handle->data.netdns.io_try[i] != NULL)
				M_event_remove(handle->data.netdns.io_try[i]);
		}
	}

	/* If we're already connected, make sure we remove event object */
	if (handle->data.netdns.io) {
		M_event_remove(handle->data.netdns.io);
	}

	/* If DNS resolving, kill the DNS operation and reset state back to init */
	if (handle->state == M_IO_NET_STATE_RESOLVING) {
		handle->state = M_IO_NET_STATE_INIT;
		M_io_destroy(handle->data.netdns.io_dns);
		handle->data.netdns.io_dns = NULL;
	}
}


static M_bool M_io_netdns_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         i;

	if (handle == NULL)
		return M_FALSE;

	for (i=0; i<handle->data.netdns.io_try_cnt; i++) {
		if (handle->data.netdns.io_try[i] != NULL)
			M_io_destroy(handle->data.netdns.io_try[i]);
	}
	M_free(handle->data.netdns.io_try);
	if (handle->data.netdns.io_dns)
		M_io_destroy(handle->data.netdns.io_dns);
	if (handle->data.netdns.io)
		M_io_destroy(handle->data.netdns.io);

	handle->state                     = M_IO_NET_STATE_INIT;
	handle->hard_down                 = M_FALSE;
	handle->data.netdns.io_try_cnt    = 0;
	handle->data.netdns.io_try_idx    = 0;
	M_mem_set(&handle->data.netdns.query_start, 0, sizeof(handle->data.netdns.query_start));
	handle->data.netdns.query_time    = 0;
	M_mem_set(&handle->data.netdns.connect_start, 0, sizeof(handle->data.netdns.connect_start));
	handle->data.netdns.connect_time  = 0;
	*(handle->data.netdns.error)      = '\0';
	return M_TRUE;
}


static void M_io_netdns_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	/* reset_cb() clears the rest and is called first */

	M_free(handle->host);
	M_free(handle);
}


static M_io_state_t M_io_netdns_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	switch (handle->state) {
		case M_IO_NET_STATE_INIT:
			return M_IO_STATE_INIT;
		case M_IO_NET_STATE_RESOLVING:
		case M_IO_NET_STATE_CONNECTING:
			return M_IO_STATE_CONNECTING;
		case M_IO_NET_STATE_CONNECTED:
			return M_IO_STATE_CONNECTED;
		case M_IO_NET_STATE_DISCONNECTING:
			return M_IO_STATE_DISCONNECTING;
		case M_IO_NET_STATE_DISCONNECTED:
			return M_IO_STATE_DISCONNECTED;
		case M_IO_NET_STATE_ERROR:
			return M_IO_STATE_ERROR;
		case M_IO_NET_STATE_LISTENING:
			return M_IO_STATE_LISTENING;
	}
	return M_IO_STATE_INIT;
}


static M_bool M_io_netdns_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	/* If we have an io object handle, get the error state from it as it would have
	 * been what generated the error */
	if (handle->data.netdns.io) {
		M_io_get_error_string(handle->data.netdns.io, error, err_len);
		return M_TRUE;
	}

	/* We're not in an error state, nothing to say */
	if (handle->state != M_IO_NET_STATE_ERROR)
		return M_FALSE;

	/* Otherwise we are the one that generated the error, we need to make
	 * sure we can output it.  In general, if we're responsible it would
	 * be a DNS lookup error. */
	M_str_cpy(error, err_len, handle->data.netdns.error);
	return M_TRUE;
}


M_io_error_t M_io_net_client_create(M_io_t **io_out, M_dns_t *dns, const char *host, unsigned short port, M_io_net_type_t type)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;

	if (io_out == NULL || M_str_isempty(host) || port == 0)
		return M_IO_ERROR_INVALID;

	*io_out = NULL;

	M_io_net_init_system();

	handle                                 = M_malloc_zero(sizeof(*handle));
	handle->host                           = M_strdup(host);
	handle->port                           = port;
	handle->type                           = type;
	handle->is_netdns                      = M_TRUE;
	handle->data.netdns.dns                = dns;
	M_io_net_settings_set_default(&handle->settings);
	*io_out                                = M_io_init(M_IO_TYPE_STREAM);
	callbacks                              = M_io_callbacks_create();

	M_io_callbacks_reg_init(callbacks, M_io_netdns_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_netdns_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_netdns_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_netdns_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_netdns_unregister_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_netdns_disconnect_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_netdns_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_netdns_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_netdns_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_netdns_errormsg_cb);
	M_io_layer_add(*io_out, "NET", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	return M_IO_ERROR_SUCCESS;
}


M_uint64 M_io_net_time_dns_ms(M_io_t *io)
{
	M_io_layer_t      *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t     *handle = M_io_layer_get_handle(layer);
	M_uint64           ret    = 0;

	if (layer == NULL || handle == NULL)
		return 0;

	if (handle->is_netdns) {
		if (handle->state == M_IO_NET_STATE_RESOLVING) {
			ret = M_time_elapsed(&handle->data.netdns.query_start);
		} else {
			ret = handle->data.netdns.query_time;
		}
	}

	M_io_layer_release(layer);
	return ret;
}


M_uint64 M_io_net_time_connect_ms(M_io_t *io)
{
	M_io_layer_t      *layer  = M_io_layer_acquire(io, 0, "NET");
	M_io_handle_t     *handle = M_io_layer_get_handle(layer);
	M_uint64           ret    = 0;

	if (layer == NULL || handle == NULL)
		return 0;

	if (handle->is_netdns) {
		if (handle->state == M_IO_NET_STATE_CONNECTING) {
			ret = M_time_elapsed(&handle->data.netdns.connect_start);
		} else {
			ret = handle->data.netdns.connect_time;
		}
	}

	M_io_layer_release(layer);
	return ret;
}

