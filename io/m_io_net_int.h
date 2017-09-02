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

#ifndef __M_IO_NET_INT_H__
#define __M_IO_NET_INT_H__

#include "m_event_int.h"

typedef struct M_io_net_settings {
	M_uint64  connect_timeout_ms;
	M_uint64  disconnect_timeout_ms;
	M_uint64  connect_failover_ms;

	/* Keepalives */
	M_bool    ka_enable;
	M_uint64  ka_idle_time_s;
	M_uint64  ka_retry_time_s;
	M_uint64  ka_retry_cnt;

	/* Nagle */
	M_bool    nagle_enable;
} M_io_net_settings_t;

enum M_io_net_state {
	M_IO_NET_STATE_INIT          = 0,
	M_IO_NET_STATE_RESOLVING     = 1,
	M_IO_NET_STATE_CONNECTING    = 2,
	M_IO_NET_STATE_CONNECTED     = 3,
	M_IO_NET_STATE_DISCONNECTING = 4,
	M_IO_NET_STATE_DISCONNECTED  = 5,
	M_IO_NET_STATE_ERROR         = 6,
	M_IO_NET_STATE_LISTENING     = 7
};
typedef enum M_io_net_state M_io_net_state_t;


struct M_io_handle_net {
	M_EVENT_HANDLE       evhandle;       /*!< Event handle                                                   */
	M_EVENT_SOCKET       sock;           /*!< Socket/File Descriptor                                         */
	unsigned short       eport;          /*!< Ephemeral port for informational purposes                      */
	int                  last_error_sys; /*!< Last recorded system error                                     */
	M_io_error_t         last_error;     /*!< Last recorded error mapped                                     */
};

struct M_io_handle_netdns {
	M_dns_t             *dns;            /*!< Handle for DNS resolver                                        */
	M_io_t             **io_try;         /*!< IO handles for each DNS entry                                  */
	size_t               io_try_cnt;     /*!< Count of IO handle being attempted                             */
	size_t               io_try_idx;     /*!< Last index used to start a connection                          */
	M_io_t              *io_dns;         /*!< DNS lookup IO handle                                           */
	M_io_t              *io;             /*!< Pointer to either IPv6 or IPv4 IO handle, whichever was chosen */
	char                 error[256];     /*!< Error message if we generated it (most likely DNS)             */
	M_timeval_t          query_start;    /*!< When query was initiated                                       */
	M_uint64             query_time;     /*!< Time DNS query took                                            */
	M_timeval_t          connect_start;  /*!< Time connection start was attempted                            */
	M_uint64             connect_time;   /*!< Amount of time it took to establish a connection               */

};

struct M_io_handle {
	char               *host;          /*!< Hostname or IP address                           */
	unsigned short      port;          /*!< Port being used                                  */
	M_io_net_type_t     type;          /*!< Network type                                     */
	M_io_net_state_t    state;         /*!< Current state                                    */
	M_io_net_settings_t settings;      /*!< Settings for the connection                      */
	M_event_timer_t    *timer;         /*!< Happy Eyeballs (DNS) or connection timer         */

	M_bool              is_netdns;     /*!< Whether or not to use the DNS wrapper            */
	union {
		struct M_io_handle_net    net;     /*!< used for non-dns */
		struct M_io_handle_netdns netdns;  /*!< used for dns     */
	} data;
};

M_io_t *M_io_netraw_client_create(const char *host, unsigned short port, M_io_net_type_t type);
void M_io_net_set_settings(M_io_t *io, M_io_net_settings_t *settings);
void M_io_net_settings_set_default(M_io_net_settings_t *settings);

#endif







