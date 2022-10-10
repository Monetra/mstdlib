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

struct M_http2_frame_settings {
	M_buf_t *buf;
};

M_http2_frame_settings_t *M_http2_frame_settings_create(M_uint32 stream_id, M_uint8 flags)
{
	M_http2_frame_settings_t *h2f_settings;
	M_http2_framehdr_t        framehdr     = { { 0 }, M_HTTP2_FRAME_TYPE_SETTINGS, flags, { M_FALSE, { stream_id } } };

	h2f_settings      = M_malloc_zero(sizeof(*h2f_settings));
	h2f_settings->buf = M_buf_create();

	/* We will have to come back and patch the len when finishing */
	M_http2_encode_framehdr(&framehdr, h2f_settings->buf);

	return h2f_settings;
}

M_uint8 *M_http2_frame_settings_finish(M_http2_frame_settings_t *h2f_settings, size_t *len)
{
	M_union_u32_u8  framehdr_len;
	M_uint8        *data;

	*len              = M_buf_len(h2f_settings->buf);
	framehdr_len.u32  = (M_uint32)(*len - 9);
	data              = (M_uint8*)M_buf_finish_str(h2f_settings->buf, NULL);
	h2f_settings->buf = NULL;

	M_http2_frame_settings_destroy(h2f_settings);

	/* Patch len */
	data[0] = framehdr_len.u8[2];
	data[1] = framehdr_len.u8[1];
	data[2] = framehdr_len.u8[0];

	return data;
}

void M_http2_frame_settings_add(M_http2_frame_settings_t *h2f_settings, M_http2_setting_type_t type, M_uint32 val)
{
	M_uint8           setting[6];
	M_union_u16_u8    setting_type;
	M_union_u32_u8    setting_val;

	if (h2f_settings == NULL)
		return;

	setting_type.u16 = (M_uint16)type;
	setting_val.u32  = (M_uint32)val;

	setting[0] = setting_type.u8[1];
	setting[1] = setting_type.u8[0];
	setting[2] = setting_val.u8[3];
	setting[3] = setting_val.u8[2];
	setting[4] = setting_val.u8[1];
	setting[5] = setting_val.u8[0];

	M_buf_add_bytes(h2f_settings->buf, setting, sizeof(setting));
}

M_bool M_http2_frame_settings_finish_to_buf(M_http2_frame_settings_t *h2f_settings, M_buf_t *buf)
{
	size_t   len;
	M_uint8 *data;

	if (h2f_settings == NULL || buf == NULL)
		return M_FALSE;

	data = M_http2_frame_settings_finish(h2f_settings, &len);
	M_buf_add_bytes(buf, data, len);
	M_free(data);
	return M_TRUE;
}

void M_http2_frame_settings_destroy(M_http2_frame_settings_t *h2f_settings)
{
	if (h2f_settings == NULL)
		return;

	M_buf_cancel(h2f_settings->buf);
	M_free(h2f_settings);
}
