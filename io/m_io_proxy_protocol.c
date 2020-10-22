/* The MIT License (MIT)
 * 
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_io_handle {
	M_io_proxy_protocol_flags_t  flags;
	M_bool                       is_inbound;
	M_bool                       complete;
	M_parser_t                  *parser;
	M_buf_t                     *buf;

	M_io_state_t                 state;
	char                         error[256];

	M_state_machine_t           *sm;
	M_bool                       local;
	char                        *source_ipaddr;
	char                        *dest_ipaddr;
	M_uint16                     source_port;
	M_uint16                     dest_port;
	M_io_net_type_t              net_type;

	size_t v2_dlen;

};

typedef enum {
	STATE_DETERMINE_VERSION = 1,
	STATE_V1,
	STATE_V2,
} inbound_states_t;

typedef enum {
	STATE_V1_HEADER = 1,
	STATE_V1_PROTOCOL,
	STATE_V1_SOURCE_ADDR,
	STATE_V1_DESTINATION_ADDR,
	STATE_V1_SOURCE_PORT,
	STATE_V1_DESTINATION_PORT,
	STATE_V1_END
} inbound_v1_states_t;

typedef enum {
	STATE_V2_HEADER = 1,
	STATE_V2_ADDR,
	STATE_V2_TLV
} inbound_v2_states_t;

static const char          *M_IO_PROXY_PROTOCOL_NAME = "PROXY PROTOCOL";
static const char          *IDENTIFIER_V1            = "PROXY";
static size_t               IDENTIFIER_V1_LEN        = 5;
static const unsigned char  IDENTIFIER_V2[]          = { 0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A };
static size_t               IDENTIFIER_V2_LEN        = 12;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_io_layer_t *M_io_proxy_protocol_get_top_proxy_protocol_layer(M_io_t *io)
{
	M_io_layer_t  *layer;
	size_t         layer_idx;
	size_t         layer_count;

	if (io == NULL) {
		return NULL;
	}

	layer       = NULL;
	layer_count = M_io_layer_count(io);
	for (layer_idx=layer_count; layer_idx-->0; ) {
		layer = M_io_layer_acquire(io, layer_idx, M_IO_PROXY_PROTOCOL_NAME);

		if (layer != NULL) {
			break;
		}
	}

	return layer;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Server states. */
static M_state_machine_status_t state_determine_version(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	size_t         len;

	len = M_parser_len(handle->parser);
	if (len < 12)
		return M_STATE_MACHINE_STATUS_WAIT;

	if (M_parser_compare(handle->parser, (const unsigned char *)IDENTIFIER_V1, IDENTIFIER_V1_LEN)) {
		*next = STATE_V1;
	} else if (M_parser_compare(handle->parser, IDENTIFIER_V2, IDENTIFIER_V2_LEN)) {
		*next = STATE_V2;
	} else {
		M_snprintf(handle->error, sizeof(handle->error), "Not proxy protocol");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Verify if V1 or V2 was explicitly requested we have that protocol version. Default is either if not specified. */
	if ((handle->flags & M_IO_PROXY_PROTOCOL_FLAG_V1 && !(handle->flags & (M_IO_PROXY_PROTOCOL_FLAG_V2)) && *next == STATE_V2) ||
			(handle->flags & M_IO_PROXY_PROTOCOL_FLAG_V2 && !(handle->flags & (M_IO_PROXY_PROTOCOL_FLAG_V1)) && *next == STATE_V1))
	{
		M_snprintf(handle->error, sizeof(handle->error), "Incompatible proxy protocol version detected");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_post(void *data, M_state_machine_status_t sub_status, M_uint64 *next)
{
	(void)data;
	(void)sub_status;
	(void)next;
	/* Return the sub status because it's either done or error.
 	 * Either way we don't want to continue since we're done processing v1 format. */
	return sub_status;
}

/* Server V1 states. */
static M_state_machine_status_t state_v1_header(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	size_t         len;

	(void)next;

	if (M_parser_len(handle->parser) < 6)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* Eat the 'PROXY' (already validated to be there) and ' '. */
	len = M_parser_consume_until(handle->parser, (const unsigned char *)" ", 1, M_TRUE);
	if (len != 6) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: invalid identity");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_protocol(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	size_t         len;
	char           buf[16];

	(void)next;

	if (M_parser_len(handle->parser) < 9)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* Check for the special case 'UNKNOWN' protocol. */
	if (M_parser_compare_str(handle->parser, "UNKNOWN\r\n", 0 , M_FALSE)) {
		handle->local = M_TRUE;
		M_parser_consume(handle->parser, 9);
		return M_STATE_MACHINE_STATUS_DONE;
	}

	len = M_parser_read_str_until(handle->parser, buf, sizeof(buf), " ", M_FALSE);
	if (len == 0) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Failed to determine protocol");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	if (M_str_caseeq(buf, "TCP4")) {
		handle->net_type = M_IO_NET_IPV4;
	} else if (M_str_caseeq(buf, "TCP6")) {
		handle->net_type = M_IO_NET_IPV6;
	} else {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Invalid protocol");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Eat the space separator. */
	M_parser_consume(handle->parser, 1);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_source_ipaddr(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	char           buf[64];
	unsigned char  ipaddr_bin[32];
	size_t         plen;
	size_t         len;

	(void)next;

	plen = M_parser_len(handle->parser);
	len  = M_parser_read_str_until(handle->parser, buf, sizeof(buf), " ", M_FALSE);
	if (len == 0) {
		/* If the start and end len in the parser are different, then
 		 * the until string was found but the buffer was too small. */
		len = M_parser_len(handle->parser);
		/* 39 characters + ' ' is the max length we can have for this field.
 		 * If we have more and no space, then it's malformed. */
		if (len >= 40 || len != plen) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Missing or invalid source address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		} else {
			return M_STATE_MACHINE_STATUS_WAIT;
		}
	}

	if (!M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), buf, &len)) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Invalid source address");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	handle->source_ipaddr = M_strdup(buf);

	/* Eat the space separator. */
	M_parser_consume(handle->parser, 1);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_dest_ipaddr(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	char           buf[64];
	unsigned char  ipaddr_bin[32];
	size_t         plen;
	size_t         len;

	(void)next;

	plen = M_parser_len(handle->parser);
	len  = M_parser_read_str_until(handle->parser, buf, sizeof(buf), " ", M_FALSE);
	if (len == 0) {
		/* If the start and end len in the parser are different, then
 		 * the until string was found but the buffer was too small. */
		len = M_parser_len(handle->parser);
		/* 39 characters + ' ' is the max length we can have for this field.
 		 * If we have more and no space, then it's malformed. */
		if (len >= 40 || len != plen) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Missing or invalid destination address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		} else {
			return M_STATE_MACHINE_STATUS_WAIT;
		}
	}

	if (!M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), buf, &len)) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Invalid destination address");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	handle->dest_ipaddr = M_strdup(buf);

	/* Eat the space separator. */
	M_parser_consume(handle->parser, 1);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_source_port(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	char           buf[8];
	M_uint32       port;
	size_t         plen;
	size_t         len;

	(void)next;

	plen = M_parser_len(handle->parser);
	len  = M_parser_read_str_until(handle->parser, buf, sizeof(buf), " ", M_FALSE);
	if (len == 0) {
		/* If the start and end len in the parser are different, then
 		 * the until string was found but the buffer was too small. */
		len = M_parser_len(handle->parser);
		if (len >= 5 || len != plen) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Missing or invalid source port");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		} else {
			return M_STATE_MACHINE_STATUS_WAIT;
		}
	}

	port                = M_str_to_uint32(buf);
	handle->source_port = (M_uint16)port;
	if (handle->source_port == 0 || port > M_UINT16_MAX) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Invalid source port");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Eat the space separator. */
	M_parser_consume(handle->parser, 1);

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v1_dest_port(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	char           buf[8];
	M_uint32       port;
	size_t         plen;
	size_t         len;

	(void)next;

	plen = M_parser_len(handle->parser);
	len  = M_parser_read_str_until(handle->parser, buf, sizeof(buf), "\r\n", M_FALSE);
	if (len == 0) {
		/* If the start and end len in the parser are different, then
 		 * the until string was found but the buffer was too small. */
		len = M_parser_len(handle->parser);
		if (len >= 5 || len != plen) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Missing or invalid destination port");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		} else {
			return M_STATE_MACHINE_STATUS_WAIT;
		}
	}

	port              = M_str_to_uint32(buf);
	handle->dest_port = (M_uint16)port;
	if (handle->dest_port == 0 || port > M_UINT16_MAX) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v1: Invalid destination port");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Eat the EOL. */
	M_parser_consume(handle->parser, 2);

	return M_STATE_MACHINE_STATUS_NEXT;
}

/* Server V2 states. */
static M_state_machine_status_t state_v2_header(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	unsigned char  byte;
	unsigned char  val;
	M_uint16       dlen;

	(void)next;

	/* Header is exactly 16 bytes. */
	if (M_parser_len(handle->parser) < 16)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* Kill the signature because we already
 	 * validated it when determining the protocol
	 * version earlier. */
	M_parser_consume(handle->parser, 12);

	/* Pull off the version and command byte */
	M_parser_read_byte(handle->parser, &byte);

	val = byte >> 4;
	if (val != 0x02) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid v2 version");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	val = byte & 0xF;
	if (val == 0) {
		handle->local = M_TRUE;
	} else if (val == 0x01) {
		handle->local = M_FALSE;
	} else {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid command"); 
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Pull off the protocol and family. */
	M_parser_read_byte(handle->parser, &byte);
	switch (byte) {
		case 0x00:
			handle->net_type = M_IO_NET_ANY;
			break;
		case 0x11:
			handle->net_type = M_IO_NET_IPV4;
			break;
		case 0x21:
			handle->net_type = M_IO_NET_IPV6;
			break;
		case 0x12:
		case 0x22:
		case 0x31:
		case 0x32:
		default:
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Unknown or unsupported address family or protocol"); 
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}

	/* Pull off the length. */
	M_parser_read_bytes(handle->parser, 2, (unsigned char *)&dlen);
	handle->v2_dlen = M_ntoh16(dlen);

	/* Validate the length is at least long enough for the connection type. */

	if (handle->net_type == M_IO_NET_IPV4) {
		if (handle->v2_dlen < 12) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Address info too short"); 
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
	} else if (handle->net_type == M_IO_NET_IPV6) {
		if (handle->v2_dlen < 36) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Address info too short"); 
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
	} else {
		/* If it's a local operation, we're going to eat the rest of
 		 * the data and stop processing. */
		M_parser_consume(handle->parser, handle->v2_dlen);
		return M_STATE_MACHINE_STATUS_DONE;
	}

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v2_ipaddr(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;
	char           tempa[64];
	M_uint16       port;

	(void)next;

	if (handle->net_type == M_IO_NET_IPV4) {
		M_uint32 addr;

		/* Verify we have enough data. */
		if (M_parser_len(handle->parser) < 12) {
			return M_STATE_MACHINE_STATUS_WAIT;
		}

		/* Source addr. */
		M_parser_read_bytes(handle->parser, 4, (unsigned char *)&addr);

		if (!M_io_net_bin_to_ipaddr(tempa, sizeof(tempa), (const unsigned char *)&addr, 4)) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid source address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		handle->source_ipaddr = M_strdup(tempa);

		/* Destination addr. */
		M_parser_read_bytes(handle->parser, 4, (unsigned char *)&addr);

		if (!M_io_net_bin_to_ipaddr(tempa, sizeof(tempa), (const unsigned char *)&addr, 4)) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid destination address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		handle->dest_ipaddr = M_strdup(tempa);

		/* Decrement the length so we know how much (if any) TLV data is next. */
		handle->v2_dlen -= 12;
	} else /* M_IO_NET_IPV6 */ {
		unsigned char addr[16];

		/* Verify we have enough data. */
		if (M_parser_len(handle->parser) < 36) {
			return M_STATE_MACHINE_STATUS_WAIT;
		}

		/* Source addr. */
		M_parser_read_bytes(handle->parser, 16, addr);

		if (!M_io_net_bin_to_ipaddr(tempa, sizeof(tempa), addr, 16)) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid source address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		handle->source_ipaddr = M_strdup(tempa);

		/* Destination addr. */
		M_parser_read_bytes(handle->parser, 16, addr);

		if (!M_io_net_bin_to_ipaddr(tempa, sizeof(tempa), addr, 16)) {
			M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid destination address");
			return M_STATE_MACHINE_STATUS_ERROR_STATE;
		}
		handle->dest_ipaddr = M_strdup(tempa);

		/* Decrement the length so we know how much (if any) TLV data is next. */
		handle->v2_dlen -= 36;
	}

	/* Source port. */
	M_parser_read_bytes(handle->parser, 2, (unsigned char *)&port);
	port = M_hton16(port);
	if (port == 0) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid source port");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	handle->source_port = port;

	/* Destination port. */
	M_parser_read_bytes(handle->parser, 2, (unsigned char *)&port);
	port = M_hton16(port);
	if (port == 0) {
		M_snprintf(handle->error, sizeof(handle->error), "Proxy protocol v2: Invalid destination port");
		return M_STATE_MACHINE_STATUS_ERROR_STATE;
	}
	handle->dest_port = port;

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_status_t state_v2_tlv(void *data, M_uint64 *next)
{
	M_io_handle_t *handle = data;

	(void)next;

	/* No data remaining means no TLV. */
	if (handle->v2_dlen == 0)
		return M_STATE_MACHINE_STATUS_DONE;

	/* Wait until we have all the data. */
	if (M_parser_len(handle->parser) < handle->v2_dlen)
		return M_STATE_MACHINE_STATUS_WAIT;

	/* TLV Data. Not supported. */
	M_parser_consume(handle->parser, handle->v2_dlen);
	handle->v2_dlen = 0;

	return M_STATE_MACHINE_STATUS_NEXT;
}

static M_state_machine_t *create_inbound_sm(void)
{
	M_state_machine_t *sm;
	M_state_machine_t *subm;

	sm = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	M_state_machine_insert_state(sm, STATE_DETERMINE_VERSION, 0, NULL, state_determine_version, NULL, NULL);


	subm = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	M_state_machine_insert_state(subm, STATE_V1_HEADER, 0, NULL, state_v1_header, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V1_PROTOCOL, 0, NULL, state_v1_protocol, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V1_SOURCE_ADDR, 0, NULL, state_v1_source_ipaddr, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V1_DESTINATION_ADDR, 0, NULL, state_v1_dest_ipaddr, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V1_SOURCE_PORT, 0, NULL, state_v1_source_port, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V1_DESTINATION_PORT, 0, NULL, state_v1_dest_port, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_V1, 0, NULL, subm, NULL, state_v1_post, NULL, NULL);
	M_state_machine_destroy(subm);


	subm = M_state_machine_create(0, NULL, M_STATE_MACHINE_LINEAR_END);
	M_state_machine_insert_state(subm, STATE_V2_HEADER, 0, NULL, state_v2_header, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V2_ADDR, 0, NULL, state_v2_ipaddr, NULL, NULL);
	M_state_machine_insert_state(subm, STATE_V2_TLV, 0, NULL, state_v2_tlv, NULL, NULL);
	M_state_machine_insert_sub_state_machine(sm, STATE_V2, 0, NULL, subm, NULL, NULL, NULL, NULL);
	M_state_machine_destroy(subm);

	return sm;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_io_proxy_protocol_init_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
	return M_TRUE;
}

static M_io_error_t M_io_proxy_protocol_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;
	size_t         buf_size;
	size_t         len;

	if (layer == NULL || handle == NULL)
		return M_IO_ERROR_INVALID;

	/* If we don't have any left over buffered data from connect,
 	 * just read from the next layer down because we have nothing. */
	len = M_parser_len(handle->parser);
	if (len == 0)
		return M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, read_len, meta);

	/* Add data that was additionally buffered during
 	 * connect that wasn't part of the proxy information. */
	buf_size = *read_len;
	len      = M_MIN(len, buf_size);

	if (!M_parser_read_bytes(handle->parser, len, buf)) {
		handle->state = M_IO_STATE_ERROR;
		return M_IO_ERROR_ERROR;
	}

	/* If we filled the buffer, we're done. */
	*read_len = len;
	if (buf_size - len == 0)
		return M_IO_ERROR_SUCCESS;

	/* If there is still room in buf, see if we can pull any data out of a lower layer. */
	buf       = buf+len;
	*read_len = buf_size - len;

	/* Attempt to read the layer under. */
	err = M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, buf, read_len, meta);
	if (err == M_IO_ERROR_SUCCESS) {
		*read_len += len;
	} else if (!M_io_error_is_critical(err)) {
		*read_len = len;
		err       = M_IO_ERROR_SUCCESS;
	} else {
		handle->state = M_IO_STATE_ERROR;
	}

	return err;
}

static M_bool M_io_proxy_protocol_process_inbound(M_io_layer_t *layer, M_io_handle_t *handle, M_event_type_t *etype)
{
	unsigned char            buf[4096];
	size_t                   buf_len;
	M_io_error_t             err;
	M_state_machine_status_t smerr;

	/* Suppress connected event. We'll convert the final read to
 	 * a connect once we've established this is proxy protocol and
	 * have stripped the proxy data. */
	if (*etype == M_EVENT_TYPE_CONNECTED)
		return M_TRUE;

	/* Not a read don't care about it. */
	if (*etype != M_EVENT_TYPE_READ)
		return M_FALSE;

	/* Start reading data into our parser so we can look for and remove the
 	 * proxy information. */
	do {
		buf_len = sizeof(buf);
		err     = M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer) - 1, buf, &buf_len, NULL);
		if (M_io_error_is_critical(err)) {
			handle->state = M_IO_STATE_ERROR;
			*etype = M_EVENT_TYPE_ERROR;
			return M_FALSE;
		}
		if (err == M_IO_ERROR_SUCCESS) {
			M_parser_append(handle->parser, buf, buf_len);
		}
	} while (err == M_IO_ERROR_SUCCESS);

	/* Try to process the data. Even if we have a non-critical error,
 	 * we might have read data successfully before it happened. */
	smerr = M_state_machine_run(handle->sm, handle);
	if (smerr == M_STATE_MACHINE_STATUS_DONE) {
		handle->state    = M_IO_STATE_CONNECTED;
		handle->complete = M_TRUE;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		if (M_parser_len(handle->parser) > 0) {
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ, M_IO_ERROR_SUCCESS);
		}
		*etype = M_EVENT_TYPE_CONNECTED;
	} else if (smerr == M_STATE_MACHINE_STATUS_ERROR_STATE) {
		handle->state = M_IO_STATE_ERROR;
		*etype = M_EVENT_TYPE_ERROR;
		return M_FALSE;
	}

	return M_TRUE;
}

static M_bool write_event_header_data(M_io_layer_t *layer, M_io_handle_t *handle, M_event_type_t *etype)
{
	M_io_error_t err;
	size_t       write_len;

	do {
		write_len = M_buf_len(handle->buf);
		err       = M_io_layer_write(M_io_layer_get_io(layer), M_io_layer_get_index(layer) - 1, (const unsigned char *)M_buf_peek(handle->buf), &write_len, NULL);
		if (M_io_error_is_critical(err)) {
			handle->state = M_IO_STATE_ERROR;
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR, err);
			*etype = M_EVENT_TYPE_ERROR;
		}
		if (err == M_IO_ERROR_SUCCESS) {
			M_buf_drop(handle->buf, write_len);
		}
	} while (err == M_IO_ERROR_SUCCESS && M_buf_len(handle->buf) > 0);

	if (M_buf_len(handle->buf) == 0) {
		handle->state = M_IO_STATE_CONNECTED;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_CONNECTED, M_IO_ERROR_SUCCESS);
		if (*etype == M_EVENT_TYPE_WRITE) {
			M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE, M_IO_ERROR_SUCCESS);
		}
	}

	return M_TRUE;
}

static void M_io_proxy_protocol_build_proxy_message_v1(M_io_handle_t *handle)
{
	/* Identifier */
	M_buf_add_str(handle->buf, IDENTIFIER_V1);
	M_buf_add_byte(handle->buf, ' ');

	if (handle->local) {
		M_buf_add_str(handle->buf, "UNKNOWN\r\n");
		return;
	}

	/* Protocol. */
	if (handle->net_type == M_IO_NET_IPV4) {
		M_buf_add_str(handle->buf, "TCP4 ");
	} else /* IPV6 */ {
		M_buf_add_str(handle->buf, "TCP6 ");
	}

	/* Source address. */
	M_buf_add_str(handle->buf, handle->source_ipaddr);
	M_buf_add_byte(handle->buf, ' ');

	/* Destination address. */
	M_buf_add_str(handle->buf, handle->dest_ipaddr);
	M_buf_add_byte(handle->buf, ' ');

	/* Source port. */
	M_buf_add_uint(handle->buf, handle->source_port);
	M_buf_add_byte(handle->buf, ' ');

	/* Destination port. */
	M_buf_add_uint(handle->buf, handle->dest_port);

	/* End marker. */
	M_buf_add_str(handle->buf, "\r\n");
}

static void M_io_proxy_protocol_build_proxy_message_v2(M_io_handle_t *handle)
{
	M_uint8       byte;
	M_uint16      len;
	unsigned char ipaddr_bin[32];

	/* Identifier */
	M_buf_add_bytes(handle->buf, IDENTIFIER_V2, IDENTIFIER_V2_LEN);

	/* Protocol version and command. */
	byte = 0x02 << 4;
	if (!handle->local)
		byte |= 0x1;
	M_buf_add_byte(handle->buf, byte);

	/* Transport protocol and address family. */
	if (handle->local) {
		byte = 0;
	} else if (handle->net_type == M_IO_NET_IPV4) {
		byte = 0x11;
	} else /* M_IO_NET_IPV6 */ {
		byte = 0x21;
	} 
	M_buf_add_byte(handle->buf, byte);

	/* Address + TLV length */
	if (handle->local) {
		len = 0;
	} else if (handle->net_type == M_IO_NET_IPV4) {
		len = 12;
	} else /* M_IO_NET_IPV6 */ {
		len = 36;
	}
	len = M_hton16(len);
	M_buf_add_bytes(handle->buf, (const void *)&len, 2);

	if (!handle->local) {
		size_t addr_len = 0;

		/* Source address. */
		M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), handle->source_ipaddr, &addr_len);
		M_buf_add_bytes(handle->buf, ipaddr_bin, addr_len);

		/* Destination address. */
		M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), handle->dest_ipaddr, &addr_len);
		M_buf_add_bytes(handle->buf, ipaddr_bin, addr_len);

		/* Source port. */
		len = M_hton16(handle->source_port);
		M_buf_add_bytes(handle->buf, (const void *)&len, 2);

		/* Destination port. */
		len = M_hton16(handle->dest_port);
		M_buf_add_bytes(handle->buf, (const void *)&len, 2);
	}

	/* TLV data. Not currently supported.
	 * NOTE: If we do want to support these we need to adjust the
	 *       address length to account for the length of TLV data. */
}

static void M_io_proxy_protocol_build_proxy_message(M_io_handle_t *handle)
{
	if (handle->flags & M_IO_PROXY_PROTOCOL_FLAG_V2 || !(handle->flags & (M_IO_PROXY_PROTOCOL_FLAG_V1|M_IO_PROXY_PROTOCOL_FLAG_V2))) {
		M_io_proxy_protocol_build_proxy_message_v2(handle);
	} else {
		/* V1 (Must have been explicitly set) */
		/* Identifier. */
		M_io_proxy_protocol_build_proxy_message_v1(handle);
	}
}

static M_bool M_io_proxy_protocol_process_outbound(M_io_layer_t *layer, M_io_handle_t *handle, M_event_type_t *etype)
{
	size_t len = M_buf_len(handle->buf);

	if (*etype == M_EVENT_TYPE_CONNECTED && len == 0) {
		M_io_proxy_protocol_build_proxy_message(handle);
		return write_event_header_data(layer, handle, etype);
	}

	if (*etype == M_EVENT_TYPE_WRITE && len != 0) {
		/* All the proxy message data wasn't written. We can write
 		 * again so try to write some more. */
		return write_event_header_data(layer, handle, etype);
	}

	return M_FALSE;
}

static M_bool M_io_proxy_protocol_process_cb(M_io_layer_t *layer, M_event_type_t *etype)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->complete)
		return M_FALSE;

	switch (*etype) {
		case M_EVENT_TYPE_CONNECTED:
		case M_EVENT_TYPE_ACCEPT:
		case M_EVENT_TYPE_READ:
		case M_EVENT_TYPE_WRITE:
		case M_EVENT_TYPE_OTHER:
			handle->state = M_IO_STATE_CONNECTING;
			break;
		case M_EVENT_TYPE_DISCONNECTED:
			handle->state = M_IO_STATE_DISCONNECTED;
			break;
		case M_EVENT_TYPE_ERROR:
			handle->state = M_IO_STATE_ERROR;
			break;
	}

	if (handle->is_inbound)
		return M_io_proxy_protocol_process_inbound(layer, handle, etype);
	return M_io_proxy_protocol_process_outbound(layer, handle, etype);
}

static M_io_error_t M_io_proxy_protocol_accept_cb(M_io_t *io, M_io_layer_t *orig_layer)
{
	size_t         layer_id;
	M_io_handle_t *orig_handle = M_io_layer_get_handle(orig_layer);

	/* Add a new layer into the new comm object with the same settings as we have.
 	 * Only inbound will ever have accept cb called. */
	return M_io_proxy_protocol_inbound_add(io, &layer_id, orig_handle->flags);
}

static void M_io_proxy_protocol_unregister_cb(M_io_layer_t *layer)
{
	(void)layer;
	/* No-op */
}

static M_bool M_io_proxy_protocol_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return M_FALSE;

	M_parser_consume(handle->parser, M_parser_len(handle->parser));
	M_buf_truncate(handle->buf, 0);
	M_state_machine_reset(handle->sm, M_STATE_MACHINE_CLEANUP_REASON_RESET);
	M_free(handle->source_ipaddr);
	M_free(handle->dest_ipaddr);
	handle->complete      = M_FALSE;
	handle->state         = M_IO_STATE_INIT;
	handle->error[0]      = '\0';
	handle->source_ipaddr = NULL;
	handle->dest_ipaddr   = NULL;
	handle->source_port   = 0;
	handle->dest_port     = 0;
	handle->net_type      = M_IO_NET_ANY;
	handle->v2_dlen       = 0;

	return M_TRUE;
}

static void M_io_proxy_protocol_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	M_state_machine_destroy(handle->sm);
	M_parser_destroy(handle->parser);
	M_buf_cancel(handle->buf);
	M_free(handle->source_ipaddr);
	M_free(handle->dest_ipaddr);

	M_free(handle);
}

static M_io_state_t M_io_proxy_protocol_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	return handle->state;
}

static M_bool M_io_proxy_protocol_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_io_error_t M_io_proxy_protocol_inbound_add(M_io_t *io, size_t *layer_id, M_uint32 flags)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_layer_t     *layer;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	handle            = M_malloc_zero(sizeof(*handle));
	handle->flags     = flags;
	handle->is_inbound = M_TRUE;
	handle->parser    = M_parser_create(M_PARSER_FLAG_NONE);
	handle->sm        = create_inbound_sm();

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_proxy_protocol_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_proxy_protocol_read_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_proxy_protocol_process_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_proxy_protocol_accept_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_proxy_protocol_unregister_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_proxy_protocol_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_proxy_protocol_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_proxy_protocol_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_proxy_protocol_errormsg_cb);
	layer = M_io_layer_add(io, M_IO_PROXY_PROTOCOL_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);
	return M_IO_ERROR_SUCCESS;
}

M_io_error_t M_io_proxy_protocol_outbound_add(M_io_t *io, size_t *layer_id, M_uint32 flags)
{
	M_io_handle_t    *handle;
	M_io_callbacks_t *callbacks;
	M_io_layer_t     *layer;

	if (io == NULL)
		return M_IO_ERROR_INVALID;

	handle             = M_malloc_zero(sizeof(*handle));
	handle->flags      = flags;
	handle->is_inbound = M_FALSE;
	handle->buf        = M_buf_create();

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_proxy_protocol_init_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_proxy_protocol_process_cb);
	M_io_callbacks_reg_unregister(callbacks, M_io_proxy_protocol_unregister_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_proxy_protocol_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_proxy_protocol_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_proxy_protocol_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_proxy_protocol_errormsg_cb);
	layer = M_io_layer_add(io, M_IO_PROXY_PROTOCOL_NAME, handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);
	return M_IO_ERROR_SUCCESS;
	return M_IO_ERROR_NOTIMPL;
}

M_bool M_io_proxy_protocol_relayed(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_bool         ret;

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	ret = handle->local;

	M_io_layer_release(layer);
	return ret;
}

const char *M_io_proxy_protocol_source_ipaddr(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	const char    *ret;

	if (layer == NULL || handle == NULL)
		return NULL;

	ret = handle->source_ipaddr;

	M_io_layer_release(layer);
	return ret;
}

const char *M_io_proxy_protocol_dest_ipaddr(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	const char    *ret;

	if (layer == NULL || handle == NULL)
		return NULL;

	ret = handle->dest_ipaddr;

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_proxy_protocol_source_port(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret;

	if (layer == NULL || handle == NULL)
		return 0;

	ret = handle->source_port;

	M_io_layer_release(layer);
	return ret;
}

M_uint16 M_io_proxy_protocol_dest_port(M_io_t *io)
{
	M_io_layer_t  *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_uint16       ret;

	if (layer == NULL || handle == NULL)
		return 0;

	ret = handle->dest_port;

	M_io_layer_release(layer);
	return ret;
}

M_io_net_type_t M_io_proxy_protocol_proxied_type(M_io_t *io)
{
	M_io_layer_t    *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	M_io_net_type_t  ret;

	if (layer == NULL || handle == NULL)
		return 0;

	ret = handle->net_type;

	M_io_layer_release(layer);
	return ret;
}

M_bool M_io_proxy_protocol_set_source_endpoints(M_io_t *io, const char *source_ipaddr, const char *dest_ipaddr, M_uint16 source_port, M_uint16 dest_port)
{
	M_io_layer_t    *layer  = M_io_proxy_protocol_get_top_proxy_protocol_layer(io);
	M_io_handle_t   *handle = M_io_layer_get_handle(layer);
	unsigned char    ipaddr_bin[32];
	M_io_net_type_t  net_type;
	size_t           len;

	if (layer == NULL || handle == NULL)
		return M_FALSE;

	if (handle->is_inbound)
		return M_FALSE;

	/* For local we need both IPs to be NULL. Otherwise, we need both set. */
	if ((M_str_isempty(source_ipaddr) && !M_str_isempty(dest_ipaddr)) ||
			(!M_str_isempty(source_ipaddr) && M_str_isempty(dest_ipaddr)))
	{
		return M_FALSE;
	}

	/* Empty IP, this is a local. */
	if (M_str_isempty(source_ipaddr)) {
		M_free(handle->source_ipaddr);
		M_free(handle->dest_ipaddr);
		handle->source_ipaddr = NULL;
		handle->dest_ipaddr   = NULL;
		handle->source_port   = 0;
		handle->dest_port     = 0;
		handle->local         = M_TRUE;
		handle->net_type      = M_IO_NET_ANY;
		return M_TRUE;
	}

	/* Not local at this point.
 	 * Verify we have valid ports. */
	if (source_port == 0 || dest_port == 0)
		return M_FALSE;

	/* Verify the ip format for our source and dest are valid. */
	len = M_str_len(source_ipaddr);
	if (!M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), source_ipaddr, &len))
		return M_FALSE;

	/* Determine the net type from the IP. */
	if (len == 4) {
		net_type = M_IO_NET_IPV4;
	} else {
		net_type = M_IO_NET_IPV6;
	}

	len = M_str_len(dest_ipaddr);
	if (!M_io_net_ipaddr_to_bin(ipaddr_bin, sizeof(ipaddr_bin), dest_ipaddr, &len))
		return M_FALSE;

	/* Verify the source and dest IPs are the same net type. */
	if (net_type == M_IO_NET_IPV4 && len != 4)
		return M_FALSE;

	if (net_type == M_IO_NET_IPV6 && len != 16)
		return M_FALSE;

	M_free(handle->source_ipaddr);
	M_free(handle->dest_ipaddr);
	handle->source_ipaddr = M_strdup(source_ipaddr);
	handle->dest_ipaddr   = M_strdup(source_ipaddr);
	handle->source_port   = source_port;
	handle->dest_port     = dest_port;
	handle->local         = M_FALSE;
	handle->net_type      = net_type;

	return M_TRUE;
}
