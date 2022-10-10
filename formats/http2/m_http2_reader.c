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

#include <mstdlib/mstdlib.h>
#include <formats/http2/m_http2.h>

struct M_http2_reader {
	struct M_http2_reader_callbacks  cbs;
	M_uint32                         flags;
	void                            *thunk;
	char                             errmsg[256];
};

static M_http_error_t M_http2_reader_frame_begin_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_frame_end_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_goaway_func_default(M_http2_goaway_t *goaway, void *thunk)
{
	(void)goaway;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_data_func_default(M_http2_data_t *data, void *thunk)
{
	(void)data;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_settings_begin_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_settings_end_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_setting_func_default(M_http2_setting_t *setting, void *thunk)
{
	(void)setting;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static void M_http2_reader_error_func_default(M_http_error_t errcode, const char *errmsg)
{
	(void)errcode;
	(void)errmsg;
}

static M_http_error_t M_http2_reader_headers_begin_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_headers_end_func_default(M_http2_framehdr_t *framehdr, void *thunk)
{
	(void)framehdr;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_header_priority_func_default(M_http2_header_priority_t *priority, void *thunk)
{
	(void)priority;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}
static M_http_error_t M_http2_reader_header_func_default(M_http2_header_t *header, void *thunk)
{
	(void)header;
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_pri_str_func_default(void *thunk)
{
	(void)thunk;
	return M_HTTP_ERROR_SUCCESS;
}

M_http2_reader_t *M_http2_reader_create(const struct M_http2_reader_callbacks *cbs, M_uint32 flags, void *thunk)
{
	const struct M_http2_reader_callbacks default_cbs = {
		M_http2_reader_frame_begin_func_default,
		M_http2_reader_frame_end_func_default,
		M_http2_reader_goaway_func_default,
		M_http2_reader_data_func_default,
		M_http2_reader_settings_begin_func_default,
		M_http2_reader_settings_end_func_default,
		M_http2_reader_setting_func_default,
		M_http2_reader_error_func_default,
		M_http2_reader_headers_begin_func_default,
		M_http2_reader_headers_end_func_default,
		M_http2_reader_header_priority_func_default,
		M_http2_reader_header_func_default,
		M_http2_reader_pri_str_func_default,
	};

	M_http2_reader_t *h2r = M_malloc_zero(sizeof(*h2r));

	if (cbs == NULL) {
		M_mem_copy(&h2r->cbs, &default_cbs, sizeof(h2r->cbs));
	} else {
		h2r->cbs.frame_begin_func    = cbs->frame_begin_func    ? cbs->frame_begin_func    : default_cbs.frame_begin_func;
		h2r->cbs.frame_end_func      = cbs->frame_end_func      ? cbs->frame_end_func      : default_cbs.frame_end_func;
		h2r->cbs.goaway_func         = cbs->goaway_func         ? cbs->goaway_func         : default_cbs.goaway_func;
		h2r->cbs.data_func           = cbs->data_func           ? cbs->data_func           : default_cbs.data_func;
		h2r->cbs.settings_begin_func = cbs->settings_begin_func ? cbs->settings_begin_func : default_cbs.settings_begin_func;
		h2r->cbs.settings_end_func   = cbs->settings_end_func   ? cbs->settings_end_func   : default_cbs.settings_end_func;
		h2r->cbs.setting_func        = cbs->setting_func        ? cbs->setting_func        : default_cbs.setting_func;
		h2r->cbs.error_func          = cbs->error_func          ? cbs->error_func          : default_cbs.error_func;
		h2r->cbs.headers_begin_func  = cbs->headers_begin_func  ? cbs->headers_begin_func  : default_cbs.headers_begin_func;
		h2r->cbs.headers_end_func    = cbs->headers_end_func    ? cbs->headers_end_func    : default_cbs.headers_end_func;
		h2r->cbs.header_priority_func= cbs->header_priority_func? cbs->header_priority_func: default_cbs.header_priority_func;
		h2r->cbs.header_func         = cbs->header_func         ? cbs->header_func         : default_cbs.header_func;
		h2r->cbs.pri_str_func        = cbs->pri_str_func        ? cbs->pri_str_func        : default_cbs.pri_str_func;
	}

	h2r->flags = flags;
	h2r->thunk = thunk;

	return h2r;

}

void M_http2_reader_destroy(M_http2_reader_t *h2r)
{
	M_free(h2r);
}

static M_bool M_http2_frame_type_is_valid(M_http2_frame_type_t type)
{
	switch(type) {
		case M_HTTP2_FRAME_TYPE_DATA:
		case M_HTTP2_FRAME_TYPE_HEADERS:
		case M_HTTP2_FRAME_TYPE_PRIORITY:
		case M_HTTP2_FRAME_TYPE_RST_STREAM:
		case M_HTTP2_FRAME_TYPE_SETTINGS:
		case M_HTTP2_FRAME_TYPE_PUSH_PROMISE:
		case M_HTTP2_FRAME_TYPE_PING:
		case M_HTTP2_FRAME_TYPE_GOAWAY:
		case M_HTTP2_FRAME_TYPE_WINDOW_UPDATE:
		case M_HTTP2_FRAME_TYPE_CONTINUATION:
			return M_TRUE;
		default:
			break;
	}
	return M_FALSE;
}

static M_bool M_parser_read_bytes_ntoh(M_parser_t *parser, M_uint8 *bytes, size_t len)
{
	for (; len-->0;) {
		if (!M_parser_read_byte(parser, &bytes[len]))
			return M_FALSE;
	}
	return M_TRUE;
}

static M_bool M_parser_read_stream(M_parser_t *parser, M_http2_stream_t *stream)
{
	M_uint8 byte;
	if (!M_parser_read_byte(parser, &byte))
		return M_FALSE;
	stream->is_R_set = (byte & 0x80) != 0;
	stream->id.u8[3] = byte & 0x7F;
	return M_parser_read_bytes_ntoh(parser, stream->id.u8, 3);
}

static M_http_error_t M_http2_reader_read_data(M_http2_reader_t *h2r, M_http2_framehdr_t *framehdr, M_parser_t *parser)
{
	M_http2_data_t  data      = { 0 };
	M_http_error_t  errcode   = M_HTTP_ERROR_SUCCESS;
	M_bool          is_padded = (framehdr->flags & 0x8) != 0;

	data.framehdr = framehdr;

	if (is_padded)
		M_parser_read_byte(parser, &data.pad_len); /* uint8 explicitly */

	data.data     = M_parser_peek(parser);
	data.data_len = framehdr->len.u32 - data.pad_len;

	if (is_padded)
		data.pad = &data.data[data.data_len];

	errcode = h2r->cbs.data_func(&data, h2r->thunk);
	return errcode;
}

static M_bool M_http2_setting_is_valid(M_http2_setting_type_t type)
{
	switch (type) {
		case M_HTTP2_SETTING_HEADER_TABLE_SIZE:
		case M_HTTP2_SETTING_ENABLE_PUSH:
		case M_HTTP2_SETTING_MAX_CONCURRENT_STREAMS:
		case M_HTTP2_SETTING_INITIAL_WINDOW_SIZE:
		case M_HTTP2_SETTING_MAX_FRAME_SIZE:
		case M_HTTP2_SETTING_MAX_HEADER_LIST_SIZE:
		case M_HTTP2_SETTING_ENABLE_CONNECT_PROTOCOL:
		case M_HTTP2_SETTING_NO_RFC7540_PRIORITIES:
			return M_TRUE;
	}
	return M_FALSE;
}

static M_http2_header_type_t M_http2_header_type(M_uint8 byte)
{
	if ((byte & 0x80) == 0x80)
		return M_HTTP2_HT_RFC7541_6_1;
	if (byte == 0x40)
		return M_HTTP2_HT_RFC7541_6_2_1_2_KEY_VAL;
	if ((byte & 0xC0) == 0x40)
		return M_HTTP2_HT_RFC7541_6_2_1_1_VAL;
	if (byte == 0x00)
		return M_HTTP2_HT_RFC7541_6_2_2_2_KEY_VAL;
	if ((byte & 0xF0) == 0x00)
		return M_HTTP2_HT_RFC7541_6_2_2_1_VAL;
	if (byte == 0x10)
		return M_HTTP2_HT_RFC7541_6_2_3_2_KEY_VAL;
	if ((byte & 0xF0) == 0x10)
		return M_HTTP2_HT_RFC7541_6_2_3_1_VAL;

/*
 * Must be
 * if ((byte & 0xE0) == 0x20)
 */
	return M_HTTP2_HT_RFC7541_6_3_DYNAMIC_TABLE;
}

static M_http_error_t M_http2_reader_read_header_number(M_parser_t *parser, M_uint64 *num, size_t *len)
{
	size_t parser_len = M_parser_len(parser);
	if (!M_http2_decode_number_chain(parser, num))
		return M_HTTP_ERROR_INTERNAL;
	*len -= (parser_len - M_parser_len(parser));
	return M_HTTP_ERROR_SUCCESS;
}

static M_http_error_t M_http2_reader_read_header_string(M_parser_t *parser, char **str, size_t *len)
{
	size_t parser_len = M_parser_len(parser);
	*str = M_http2_decode_string_alloc(parser);
	if (*str == NULL) {
		if ((parser_len - M_parser_len(parser)) == 1) {
			/* This was a parsed out value */
			(*len)--;
			return M_HTTP_ERROR_SUCCESS;
		}
		/* There was a failure */
		return M_HTTP_ERROR_INTERNAL;
	}
	*len -= (parser_len - M_parser_len(parser));
	return M_HTTP_ERROR_SUCCESS;
}


static M_http_error_t M_http2_header_table_lookup(M_http2_reader_t *h2r, size_t idx, M_http2_header_t *header)
{
	if (idx == 0) {
		M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Table index must be > 0");
		return M_HTTP_ERROR_INVALID_TABLE_INDEX;
	}

	if (M_http2_static_table_lookup(idx, &header->key, &header->value))
		return M_HTTP_ERROR_SUCCESS;

	return M_HTTP_ERROR_INVALID_TABLE_INDEX;
}

static M_http_error_t M_http2_header_dynamic_table_entry(M_http2_reader_t *h2r, const char *key, const char *value)
{
	(void)key;
	(void)value;
	M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Unsupported dynamic table entries");
	return M_HTTP_ERROR_UNSUPPORTED_DATA;
}

static M_http_error_t M_http2_header_dynamic_table_size(M_http2_reader_t *h2r, size_t table_size)
{
	if (table_size == 0) {
		return M_HTTP_ERROR_SUCCESS;
	}
	M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Unsupported dynamic table size > 0");
	return M_HTTP_ERROR_UNSUPPORTED_DATA;
}

static M_http_error_t M_http2_reader_read_headers(M_http2_reader_t *h2r, M_http2_framehdr_t *framehdr, M_parser_t *parser)
{
	size_t           len;
	M_http2_header_t header;
	M_http_error_t   errcode        = M_HTTP_ERROR_SUCCESS;
	M_bool           is_padded      = framehdr->flags & 0x8;
	M_bool           is_prioritized = framehdr->flags & 0x20;
	M_uint8          pad_len        = 0;

	errcode = h2r->cbs.headers_begin_func(framehdr, h2r->thunk);
	if (errcode != M_HTTP_ERROR_SUCCESS)
		return errcode;

	len = framehdr->len.u32;

	if (is_padded) {
		if (!M_parser_read_byte(parser, &pad_len)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed reading 1 byte into pad length.");
			return M_HTTP_ERROR_INTERNAL;
		}
		len--;
	}

	if (is_prioritized) {
		M_http2_header_priority_t priority;
		priority.framehdr = framehdr;
		if (!M_parser_read_stream(parser, &priority.stream)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed reading 4 bytes into priority stream.");
			return M_HTTP_ERROR_INTERNAL;
		}
		if (!M_parser_read_byte(parser, &priority.weight)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed reading 1 byte into priority weight");
			return M_HTTP_ERROR_INTERNAL;
		}
		errcode = h2r->cbs.header_priority_func(&priority, h2r->thunk);
		if (errcode != M_HTTP_ERROR_SUCCESS)
			return errcode;
		len -= 5;
	}

	header.framehdr = framehdr;

	while (len > pad_len) {
		M_uint8                byte;
		M_http2_header_type_t  type;
		char                  *key;
		char                  *value;
		size_t                 idx;
		size_t                 table_size;
		M_uint8                mask;

		if (!M_parser_read_byte(parser, &byte)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed reading 1 byte into next header entry");
			return M_HTTP_ERROR_INTERNAL;
		}
		len--;
		type = M_http2_header_type(byte);
		switch (type) {
			case M_HTTP2_HT_RFC7541_6_1:
				byte = byte & 0x7F;
				if (byte == 0x7F) {
					M_uint64 num;
					errcode = M_http2_reader_read_header_number(parser, &num, &len);
					if (errcode != M_HTTP_ERROR_SUCCESS) {
						M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed to read number chain (header type 6.1)");
						return errcode;
					}
					idx = byte + num;
				} else {
					idx = byte;
				}
				errcode = M_http2_header_table_lookup(h2r, idx, &header);
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				h2r->cbs.header_func(&header, h2r->thunk);
				break;
			case M_HTTP2_HT_RFC7541_6_2_1_2_KEY_VAL:
			case M_HTTP2_HT_RFC7541_6_2_2_2_KEY_VAL:
			case M_HTTP2_HT_RFC7541_6_2_3_2_KEY_VAL:
				errcode = M_http2_reader_read_header_string(parser, &key, &len);
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				errcode = M_http2_reader_read_header_string(parser, &value, &len);
				if (errcode != M_HTTP_ERROR_SUCCESS) {
					M_free(key);
					M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed reading header value");
					return errcode;
				}
				header.key   = key;
				header.value = value;
				h2r->cbs.header_func(&header, h2r->thunk);
				if (type == M_HTTP2_HT_RFC7541_6_2_1_2_KEY_VAL) {
					errcode = M_http2_header_dynamic_table_entry(h2r, header.key, header.value);
				}
				M_free(key);
				M_free(value);
				key   = NULL;
				value = NULL;
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				break;
			case M_HTTP2_HT_RFC7541_6_2_1_1_VAL:
			case M_HTTP2_HT_RFC7541_6_2_2_1_VAL:
			case M_HTTP2_HT_RFC7541_6_2_3_1_VAL:
				mask = 0x0F;
				if (type == M_HTTP2_HT_RFC7541_6_2_1_1_VAL)
					mask = 0x3F;
				byte = byte & mask;
				if (byte == mask) {
					M_uint64 num;
					errcode = M_http2_reader_read_header_number(parser, &num, &len);
					if (errcode != M_HTTP_ERROR_SUCCESS) {
						M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed to read number chain (header type 6.2)");
						return errcode;
					}
					idx = byte + num;
				} else {
					idx = byte;
				}
				errcode = M_http2_header_table_lookup(h2r, idx, &header);
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				errcode = M_http2_reader_read_header_string(parser, &value, &len);
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				header.value = value;
				h2r->cbs.header_func(&header, h2r->thunk);
				if (type == M_HTTP2_HT_RFC7541_6_2_1_1_VAL)
					errcode = M_http2_header_dynamic_table_entry(h2r, header.key, header.value);
				M_free(value);
				value = NULL;
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				break;
			case M_HTTP2_HT_RFC7541_6_3_DYNAMIC_TABLE:
				byte = byte & 0x1F;
				if (byte == 0x1F) {
					M_uint64 num;
					errcode = M_http2_reader_read_header_number(parser, &num, &len);
					if (errcode != M_HTTP_ERROR_SUCCESS) {
						M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failed to read number chain (header type 6.3)");
						return errcode;
					}
					table_size = 0x1F + num;
				} else {
					table_size = byte & 0x1F;
				}
				errcode = M_http2_header_dynamic_table_size(h2r, table_size);
				if (errcode != M_HTTP_ERROR_SUCCESS)
					return errcode;
				break;
		}
	}

	return h2r->cbs.headers_end_func(framehdr, h2r->thunk);
}

static M_http_error_t M_http2_reader_read_settings(M_http2_reader_t *h2r, M_http2_framehdr_t *framehdr, M_parser_t *parser)
{
	size_t            len;
	M_union_u16_u8    setting_type;
	M_http2_setting_t setting;
	M_http_error_t    errcode = M_HTTP_ERROR_SUCCESS;

	errcode = h2r->cbs.settings_begin_func(framehdr, h2r->thunk);
	if (errcode != M_HTTP_ERROR_SUCCESS)
		return errcode;

	len = framehdr->len.u32;
	while (len >= 6) {
		if (!M_parser_read_bytes_ntoh(parser, setting_type.u8, 2)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "read settings type failed reading next 2 bytes");
			return M_HTTP_ERROR_INTERNAL;
		}
		setting.type = (M_http2_setting_type_t)setting_type.u16;
		if (!M_http2_setting_is_valid(setting.type)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Invalid setting type: %d", setting.type);
			return M_HTTP_ERROR_INVALID_SETTING_TYPE;
		}
		if (!M_parser_read_bytes_ntoh(parser, setting.value.u8, 4)) {
			M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "read settings value failed reading next 4 bytes");
			return M_HTTP_ERROR_INTERNAL;
		}
		errcode = h2r->cbs.setting_func(&setting, h2r->thunk);
		if (errcode != M_HTTP_ERROR_SUCCESS)
			return errcode;

		len -= 6;
	}

	if (len != 0) {
		M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Misalignment finished with len: %zu instead of 0", len);
		return M_HTTP_ERROR_MISALIGNED_SETTINGS;
	}

	return h2r->cbs.settings_end_func(framehdr, h2r->thunk);
}


static M_http_error_t M_http2_reader_read_goaway(M_http2_reader_t *h2r, M_http2_framehdr_t *framehdr, M_parser_t *parser)
{
	M_http2_goaway_t goaway  = { 0 };
	M_http_error_t   errcode = M_HTTP_ERROR_SUCCESS;

	goaway.framehdr = framehdr;
	if (!M_parser_read_stream(parser, &goaway.stream)) {
		M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failure reading next 4 bytes (len: %zu)", M_parser_len(parser));
		return M_HTTP_ERROR_INTERNAL;
	}

	if (!M_parser_read_bytes_ntoh(parser, goaway.errcode.u8, 4)) {
		M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Failure reading next 4 bytes (len: %zu)", M_parser_len(parser));
		return M_HTTP_ERROR_INTERNAL;
	}

	goaway.debug_data_len = framehdr->len.u32 - 8; /* minus stream and errcode */
	goaway.debug_data     = NULL;
	if (goaway.debug_data_len > 0)
		goaway.debug_data = M_parser_peek(parser);

	errcode = h2r->cbs.goaway_func(&goaway, h2r->thunk);

	if (goaway.debug_data_len > 0)
		M_parser_consume(parser, goaway.debug_data_len);

	return errcode;
}

M_http_error_t M_http2_reader_read(M_http2_reader_t *h2r, const unsigned char *data, size_t data_len, size_t *len_read)
{
	static const char  *pri_str      = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
	M_http_error_t      res          = M_HTTP_ERROR_INVALIDUSE;
	size_t              internal_len;
	M_http2_framehdr_t  framehdr;
	M_parser_t         *parser;
	size_t              len_skipped;

	if (h2r == NULL || data == NULL || data_len == 0)
		return M_HTTP_ERROR_INVALIDUSE;

	if (len_read == NULL)
		len_read = &internal_len;

	*len_read = 0;

	parser = M_parser_create_const(data, data_len, M_PARSER_SPLIT_FLAG_NONE);

	len_skipped = M_parser_consume_whitespace(parser, M_PARSER_WHITESPACE_NONE);

	if (data_len >= M_str_len(pri_str)) {
		if (M_mem_eq(data, pri_str, M_str_len(pri_str))) {
			res = h2r->cbs.pri_str_func(h2r->thunk);
			if (res != M_HTTP_ERROR_SUCCESS)
				goto done;
			M_parser_consume(parser, M_str_len(pri_str));
		}
	}

	while (M_http2_decode_framehdr(parser, &framehdr)) {
		if (framehdr.len.u32 > M_parser_len(parser)) {
			res = M_HTTP_ERROR_MOREDATA;
			goto done;
		}
		if (!M_http2_frame_type_is_valid(framehdr.type)) {
			res = M_HTTP_ERROR_INVALID_FRAME_TYPE;
			goto done;
		}
		h2r->cbs.frame_begin_func(&framehdr, h2r->thunk);
		M_parser_mark(parser);
		res = M_HTTP_ERROR_SUCCESS;
		switch(framehdr.type) {
			case M_HTTP2_FRAME_TYPE_DATA:
				res = M_http2_reader_read_data(h2r, &framehdr, parser);
				break;
			case M_HTTP2_FRAME_TYPE_HEADERS:
				res = M_http2_reader_read_headers(h2r, &framehdr, parser);
				break;
			case M_HTTP2_FRAME_TYPE_SETTINGS:
				res = M_http2_reader_read_settings(h2r, &framehdr, parser);
				break;
			case M_HTTP2_FRAME_TYPE_GOAWAY:
				res = M_http2_reader_read_goaway(h2r, &framehdr, parser);
				break;
			case M_HTTP2_FRAME_TYPE_PUSH_PROMISE:
			case M_HTTP2_FRAME_TYPE_PING:
			case M_HTTP2_FRAME_TYPE_PRIORITY:
			case M_HTTP2_FRAME_TYPE_RST_STREAM:
			case M_HTTP2_FRAME_TYPE_WINDOW_UPDATE:
			case M_HTTP2_FRAME_TYPE_CONTINUATION:
				M_snprintf(h2r->errmsg, sizeof(h2r->errmsg), "Unsupported frame type: %u\n", framehdr.type);
				res = M_HTTP_ERROR_UNSUPPORTED_DATA;
				break;
		}
		if (res != M_HTTP_ERROR_SUCCESS)
			goto done;
		M_parser_mark_rewind(parser);
		M_parser_consume(parser, framehdr.len.u32);
		h2r->cbs.frame_end_func(&framehdr, h2r->thunk);
	}
done:
	*len_read = data_len - M_parser_len(parser) - len_skipped;
	M_parser_destroy(parser);
	if (res != M_HTTP_ERROR_SUCCESS) {
		h2r->cbs.error_func(res, h2r->errmsg);
	}
	return res;
}
