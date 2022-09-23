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

void M_http2_goaway_to_buf(M_http2_stream_t *stream, M_uint32 errcode, const M_uint8 *data, size_t data_len, M_buf_t *buf)
{
	M_union_u32_u8           u32u8;
	M_http2_framehdr_t       framehdr    = { { 8 + (M_uint32)data_len }, M_HTTP2_FRAME_TYPE_GOAWAY, 0, { M_FALSE, { 0 } } };

	M_http2_encode_framehdr(&framehdr, buf);
	M_buf_add_byte(buf, stream->id.u8[3] | (stream->is_R_set ? 0x80 : 0x00));
	M_buf_add_byte(buf, stream->id.u8[2]);
	M_buf_add_byte(buf, stream->id.u8[1]);
	M_buf_add_byte(buf, stream->id.u8[0]);

	u32u8.u32 = errcode;
	M_buf_add_byte(buf, u32u8.u8[3]);
	M_buf_add_byte(buf, u32u8.u8[2]);
	M_buf_add_byte(buf, u32u8.u8[1]);
	M_buf_add_byte(buf, u32u8.u8[0]);

	if (data_len > 0) {
		M_buf_add_bytes(buf, data, data_len);
	}
}

M_uint8 *M_http2_goaway_to_data(M_http2_stream_t *stream, M_uint32 errcode, const M_uint8 *data, size_t data_len)
{
	M_uint8 *frame;
	M_buf_t *buf = M_buf_create();
	M_http2_goaway_to_buf(stream, errcode, data, data_len, buf);
	frame = (M_uint8*)M_buf_finish_str(buf, NULL);
	return frame;
}
