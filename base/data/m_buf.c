/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define M_BUF_INITIAL_SIZE 1024 /* Must be a multiple of 2 */

struct M_buf {
	unsigned char *data;          /*!< Pointer to buffer */
	size_t         data_size;     /*!< Total allocated size of buffer (minus 1 as it does not include
	                               *   room for null terminator which is allocated but hidden) */
	size_t         data_length;   /*!< Length of meaningful bytes in buffer */
	size_t         data_consumed; /*!< Bytes which have been marked as consumed on the left side of the buffer */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_buf_t *M_buf_create(void)
{
	M_buf_t *buf;

	buf = M_malloc(sizeof(*buf));
	M_mem_set(buf, 0, sizeof(*buf));

	return buf;
}

void M_buf_cancel(M_buf_t *buf)
{
	if (buf == NULL)
		return;

	M_free(buf->data);
	M_free(buf);
}

/*! Move memory so that data_consumed becomes 0 */
static void M_buf_consume(M_buf_t *buf)
{
	if (!buf->data_consumed)
		return;

	/* M_buf_drop() might have dropped all data so nothing to move */
	if (buf->data_length)
		M_mem_move(buf->data, buf->data + buf->data_consumed, buf->data_length);

	buf->data_consumed          = 0;

	/* Null-terminate for safety */
	buf->data[buf->data_length] = 0;
}


unsigned char *M_buf_finish(M_buf_t *buf, size_t *out_length)
{
	unsigned char *out;

	if (buf == NULL) {
		if (out_length)
			*out_length = 0;
		return NULL;
	}

	/* Ensure entire buffer is the real output data */
	M_buf_consume(buf);

	out       = buf->data;
	buf->data = NULL;

	if (out_length)
		*out_length = buf->data_length;
	buf->data_length = 0;

	M_free(buf);
	return out;
}


char *M_buf_finish_str(M_buf_t *buf, size_t *out_length)
{
	return (char *)M_buf_finish(buf, out_length);
}

size_t M_buf_len(const M_buf_t *buf)
{
	if (buf == NULL)
		return 0;
	return buf->data_length;
}

size_t M_buf_alloc_size(const M_buf_t *buf)
{
	if (buf == NULL)
		return 0;

	/* 0 means it hasn't been allocated yet, lets really return the *minimum*
	 * size */
	if (buf->data_size == 0)
		return M_BUF_INITIAL_SIZE;
	return buf->data_size;
}

const char *M_buf_peek(const M_buf_t *buf)
{
	if (buf == NULL)
		return NULL;

	return ((char *)buf->data) + buf->data_consumed;
}

void M_buf_truncate(M_buf_t *buf, size_t length)
{
	if (buf == NULL || buf->data_length <= length || buf->data == NULL)
		return;

	/* Clear truncated memory */
	M_mem_set(buf->data+buf->data_consumed+length, 0xFF, buf->data_length-length);

	buf->data_length                                 = length;
	buf->data[buf->data_consumed + buf->data_length] = 0;
}

void M_buf_drop(M_buf_t *buf, size_t num)
{
	if (buf == NULL || num == 0)
		return;

	if (num > buf->data_length)
		num = buf->data_length;

	/* Clear dropped memory */
	M_mem_set(buf->data+buf->data_consumed, 0xFF, num);

	buf->data_consumed += num;
	buf->data_length   -= num;

	/* Data is really consumed when more memory is needed */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_buf_merge(M_buf_t *dest, M_buf_t *source)
{
	if (dest == NULL || source == NULL)
		return;
	M_buf_add_bytes(dest, source->data, source->data_length);
	M_buf_cancel(source);
}

void M_buf_bjoin_buf(M_buf_t *dest, unsigned char sep, M_buf_t **bufs, size_t cnt)
{
	size_t i;

	if (dest == NULL || bufs == NULL || cnt == 0) {
		return;
	}

	for (i=0; i<cnt; i++) {
		M_buf_merge(dest, bufs[i]);
		if (i != cnt-1) {
			M_buf_add_byte(dest, sep);
		}
	}
}

void M_buf_bjoin_str(M_buf_t *dest, unsigned char sep, const char **strs, size_t cnt)
{
	size_t i;

	if (dest == NULL || strs == NULL || cnt == 0) {
		return;
	}

	for (i=0; i<cnt; i++) {
		M_buf_add_str(dest, strs[i]);
		if (i != cnt-1) {
			M_buf_add_byte(dest, sep);
		}
	}
}

void M_buf_sjoin_buf(M_buf_t *dest, const char *sep, M_buf_t **bufs, size_t cnt)
{
	size_t i;

	if (dest == NULL || bufs == NULL || cnt == 0) {
		return;
	}

	for (i=0; i<cnt; i++) {
		M_buf_merge(dest, bufs[i]);
		if (i != cnt-1) {
			M_buf_add_str(dest, sep);
		}
	}
}

void M_buf_sjoin_str(M_buf_t *dest, const char *sep, const char **strs, size_t cnt)
{
	size_t i;

	if (dest == NULL || strs == NULL || cnt == 0) {
		return;
	}

	for (i=0; i<cnt; i++) {
		M_buf_add_str(dest, strs[i]);
		if (i != cnt-1) {
			M_buf_add_str(dest, sep);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static size_t next_multiple_of_block_size(size_t needed_size, size_t curr_size)
{
	size_t n;

	n = curr_size;
	if (n == 0)
		n = M_BUF_INITIAL_SIZE;

	while (needed_size > n) {
		n <<= 1;

		/* Detect wrap */
		if (n < curr_size)
			return 0;
	}

	return n;
}

static M_bool ensure_space(M_buf_t *buf, size_t add_length)
{
	size_t new_data_length;
	size_t new_data_size;

	new_data_length = buf->data_length + add_length;

	/* If we have enough space, no need to continue */
	if (new_data_length < buf->data_size - buf->data_consumed)
		return M_TRUE;

	/* If we have consumed memory on the left, go ahead and force it to M_mem_move
	 * the data to see if that frees up enough buffer space.  It is assumed a
	 * M_mem_move is cheaper than a realloc (which will also do a M_mem_move) */
	if (buf->data_consumed)
		M_buf_consume(buf);

	/* See if buffer is large enough. data_consumed here is guaranteed to be 0 */
	if (new_data_length > buf->data_size) {
		new_data_size  = next_multiple_of_block_size(new_data_length, buf->data_size);
		if (new_data_size == 0)
			return M_FALSE;
		buf->data      = M_realloc(buf->data, new_data_size + 1 /* NULL Term */);
		buf->data_size = new_data_size;
	}

	return M_TRUE;
}


unsigned char *M_buf_direct_write_start(M_buf_t *buf, size_t *len)
{
	if (buf == NULL || len == NULL || *len == 0)
		return NULL;

	ensure_space(buf, *len);

	*len = buf->data_size - buf->data_consumed - buf->data_length;
	return buf->data + buf->data_consumed + buf->data_length;
}


void M_buf_direct_write_end(M_buf_t *buf, size_t len)
{
	if (buf == NULL || len == 0)
		return;

	buf->data_length += len;
	/* NULL Term */
	buf->data[buf->data_consumed + buf->data_length] = 0;
}


static size_t M_buf_transform_upper_cb(unsigned char *data, size_t data_len)
{
	size_t i;

	for (i=0; i<data_len; i++) {
		if (M_chr_islower((char)data[i])) {
			data[i] = (unsigned char)M_chr_toupper((char)data[i]);
		}
	}

	return data_len;
}

static size_t M_buf_transform_lower_cb(unsigned char *data, size_t data_len)
{
	size_t i;

	for (i=0; i<data_len; i++) {
		if (M_chr_isupper((char)data[i])) {
			data[i] = (unsigned char)M_chr_tolower((char)data[i]);
		}
	}

	return data_len;
}

static size_t M_buf_transform_ltrim_cb(unsigned char *data, size_t data_len)
{
	size_t i;

	for (i=0; M_chr_isspace((char)data[i]); i++)
		;

	/* Not left-padded with space */
	if (i == 0)
		return data_len;

	M_mem_move(data, data + i, data_len - i);

	return data_len - i;
}

static size_t M_buf_transform_rtrim_cb(unsigned char *data, size_t data_len)
{
	while (data_len && M_chr_isspace((char)data[data_len-1]))
		data_len--;

	return data_len;
}


static size_t M_buf_add_bytes_transform(M_buf_t *buf, M_uint32 transform_type, const void *bytes, size_t bytes_length)
{
	size_t offset;
	const struct {
		enum M_BUF_TRANSFORM_TYPE type;
		size_t                  (*func)(unsigned char *, size_t);
	} transformations[] = {
		{ M_BUF_TRANSFORM_UPPER, M_buf_transform_upper_cb },
		{ M_BUF_TRANSFORM_LOWER, M_buf_transform_lower_cb },
		{ M_BUF_TRANSFORM_LTRIM, M_buf_transform_ltrim_cb },
		{ M_BUF_TRANSFORM_RTRIM, M_buf_transform_rtrim_cb },
		{ M_BUF_TRANSFORM_NONE,  NULL                     },
	};
	size_t i;

	if (buf == NULL || bytes == NULL || bytes_length == 0)
		return 0;

	if (!ensure_space(buf, bytes_length))
		return 0;

	offset = buf->data_consumed + buf->data_length;
	M_mem_copy(buf->data + offset, bytes, bytes_length);
	buf->data_length += bytes_length;

	/* NULL Term */
	buf->data[offset + bytes_length] = 0;

	/* Perform transformations on data */
	if (transform_type == M_BUF_TRANSFORM_NONE) {
		return bytes_length;
	}

	for (i=0; transformations[i].type != M_BUF_TRANSFORM_NONE && bytes_length != 0; i++) {
		size_t len;

		if (!(transform_type & transformations[i].type))
			continue;

		len = transformations[i].func(buf->data + offset, bytes_length);
		if (len != bytes_length) {
			if (len > bytes_length) {
				buf->data_length += len - bytes_length;
			} else {
				buf->data_length -= bytes_length - len;
			}

			bytes_length = len;

			/* NULL Term */
			buf->data[offset + bytes_length] = 0;
		}
	}

	return bytes_length;
}


void M_buf_add_bytes(M_buf_t *buf, const void *bytes, size_t bytes_length)
{
	M_buf_add_bytes_transform(buf, M_BUF_TRANSFORM_NONE, bytes, bytes_length);
}


M_bool M_buf_add_bytes_hex(M_buf_t *buf, const char *hex_bytes)
{
	size_t   hex_len = M_str_len(hex_bytes);
	size_t   bin_len;
	M_uint8 *bin;

	if (hex_len == 0) {
		return M_TRUE;
	}

	if (buf == NULL || (hex_len % 2) != 0) {
		return M_FALSE;
	}

	bin_len = hex_len / 2;
	bin     = M_buf_direct_write_start(buf, &bin_len);
	bin_len = M_bincodec_decode(bin, bin_len, hex_bytes, hex_len, M_BINCODEC_HEX);
	M_buf_direct_write_end(buf, bin_len);

	return M_TRUE;
}


void M_buf_add_str_transform(M_buf_t *buf, M_uint32 transform_type, const char *str)
{
	size_t str_length;

	if (buf == NULL)
		return;

	str_length = M_str_len(str);
	if (str_length == 0)
		return;

	M_buf_add_bytes_transform(buf, transform_type, str, str_length);
}


void M_buf_add_str(M_buf_t *buf, const char *str)
{
	M_buf_add_str_transform(buf, M_BUF_TRANSFORM_NONE, str);
}

void M_buf_add_str_max(M_buf_t *buf, const char *str, size_t max)
{
	size_t len;

	if (buf == NULL || M_str_isempty(str) || max == 0)
		return;

	len = M_MIN(M_str_len(str), max);
	M_buf_add_bytes(buf, str, len);
}

void M_buf_add_str_hex(M_buf_t *buf, const void *bytes, size_t len)
{
	size_t   hex_len;
	M_uint8 *hex;

	if (buf == NULL || bytes == NULL || len == 0)
		return;

	hex_len = M_bincodec_encode_size(len, 0, M_BINCODEC_HEX);
	hex     = M_buf_direct_write_start(buf, &hex_len);
	hex_len = M_bincodec_encode((char *)hex, hex_len, bytes, len, 0, M_BINCODEC_HEX);
	M_buf_direct_write_end(buf, hex_len);
}


size_t M_buf_add_str_lines(M_buf_t *buf, const char *str, size_t max_lines, size_t max_chars, M_bool truncate,
	const char *newline)
{
	size_t   i;
	size_t   num_lines;
	char   **strs;

	if (buf == NULL || M_str_isempty(str))
		return 0;

	if  (M_str_isempty(newline)) {
		M_buf_add_str(buf, str);
		return 1;
	}

	strs = M_str_explode_lines(max_lines, max_chars, str, truncate, &num_lines);

	for (i=0; i<num_lines; i++) {
		M_buf_add_str(buf, strs[i]);
		M_buf_add_str(buf, newline);
	}

	M_str_explode_free(strs, num_lines);
	return num_lines;
}

void M_buf_add_str_upper(M_buf_t *buf, const char *str)
{
	M_buf_add_str_transform(buf, M_BUF_TRANSFORM_UPPER, str);
}

void M_buf_add_str_lower(M_buf_t *buf, const char *str)
{
	M_buf_add_str_transform(buf, M_BUF_TRANSFORM_LOWER, str);
}

void M_buf_add_fill(M_buf_t *buf, unsigned char fill_char, size_t width)
{
	if (buf == NULL || width == 0)
		return;

	if (!ensure_space(buf, width))
		return;

	M_mem_set(buf->data + buf->data_consumed + buf->data_length, fill_char, width);
	buf->data_length += width;

	/* NULL Term */
	buf->data[buf->data_consumed + buf->data_length] = 0;
}

void M_buf_add_uint(M_buf_t *buf, M_uint64 n)
{
	char space[32];

	if (buf == NULL)
		return;

	M_snprintf(space, sizeof(space), "%" M_PRIu64, n);
	M_buf_add_str(buf, space);
}

void M_buf_add_int(M_buf_t *buf, M_int64 n)
{
	char space[32];

	if (buf == NULL)
		return;

	M_snprintf(space, sizeof(space), "%lld", n);
	M_buf_add_str(buf, space);
}

M_bool M_buf_add_encode(M_buf_t *buf, const void *bytes, size_t bytes_len, size_t wrap, M_bincodec_codec_t codec)
{
	char   *encoded;
	size_t  encoded_len;

	if (bytes_len == 0) {
		return M_TRUE;
	}
	if (buf == NULL || bytes == NULL) {
		return M_FALSE;
	}

	encoded_len = M_bincodec_encode_size(bytes_len, wrap, codec);
	encoded     = (char *)M_buf_direct_write_start(buf, &encoded_len);
	encoded_len = M_bincodec_encode(encoded, encoded_len, bytes, bytes_len, wrap, codec); /* returns 0 on error */
	M_buf_direct_write_end(buf, encoded_len);

	return (encoded_len > 0)? M_TRUE : M_FALSE;
}

M_bool M_buf_encode(M_buf_t *buf, size_t wrap, M_bincodec_codec_t codec)
{
	char          *encoded;
	size_t         encoded_len;
	const M_uint8 *bytes;
	size_t         bytes_len;

	if (buf == NULL || M_buf_len(buf) == 0) {
		return M_TRUE;
	}

	bytes_len   = M_buf_len(buf);

	encoded_len = M_bincodec_encode_size(bytes_len, wrap, codec);
	encoded     = (char *)M_buf_direct_write_start(buf, &encoded_len);

	/* WARNING: you MUST call M_buf_peek() AFTER M_buf_direct_write_start(), because starting a direct write may
	 *          reallocate the internal buffer if there isn't enough space in the existing buffer.
	 */
	bytes       = (const M_uint8 *)M_buf_peek(buf);

	encoded_len = M_bincodec_encode(encoded, encoded_len, bytes, bytes_len, wrap, codec); /* returns 0 on error */
	M_buf_direct_write_end(buf, encoded_len);

	if (encoded_len == 0) {
		return M_FALSE;
	}

	/* Drop raw binary from beginning of buffer, leaving only the encoded data. */
	M_buf_drop(buf, bytes_len);
	return M_TRUE;
}

M_bool M_buf_add_decode(M_buf_t *buf, const char *encoded, size_t encoded_len, M_bincodec_codec_t codec)
{
	M_uint8 *bytes;
	size_t   bytes_len;

	if (encoded_len == 0) {
		return M_TRUE;
	}
	if (buf == NULL || encoded == NULL) {
		return M_FALSE;
	}

	bytes_len = M_bincodec_decode_size(encoded_len, codec);
	bytes     = M_buf_direct_write_start(buf, &bytes_len);
	bytes_len = M_bincodec_decode(bytes, bytes_len, encoded, encoded_len, codec); /* returns 0 on error */
	M_buf_direct_write_end(buf, bytes_len);

	return (bytes_len > 0)? M_TRUE : M_FALSE;
}

M_bool M_buf_decode(M_buf_t *buf, M_bincodec_codec_t codec)
{
	M_uint8    *bytes;
	size_t      bytes_len;
	const char *encoded;
	size_t      encoded_len;

	if (buf == NULL || M_buf_len(buf) == 0) {
		return M_TRUE;
	}

	encoded_len = M_buf_len(buf);

	bytes_len   = M_bincodec_decode_size(encoded_len, codec);
	bytes       = M_buf_direct_write_start(buf, &bytes_len);

	/* WARNING: you MUST call M_buf_peek() AFTER M_buf_direct_write_start(), because starting a direct write may
	 *          reallocate the internal buffer if there isn't enough space in the existing buffer.
	 */
	encoded     = M_buf_peek(buf);

	bytes_len = M_bincodec_decode(bytes, bytes_len, encoded, encoded_len, codec); /* returns 0 on error */
	M_buf_direct_write_end(buf, bytes_len);

	if (bytes_len == 0) {
		return M_FALSE;
	}

	/* Drop encoded data from beginning of buffer, leaving only the decoded raw binary data. */
	M_buf_drop(buf, encoded_len);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_buf_add_bytes_just_transform(M_buf_t *buf, M_uint32 transform_type, const void *bytes, size_t bytes_length, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width)
{
	size_t  offset;
	size_t  justified_length;

	if (buf == NULL || width == 0)
		return;

	/* If bytes == NULL and the length passed states that, don't exit, we really
	 * _do_ want to just fill this field with the fill_char! */
	if (bytes == NULL && bytes_length > 0)
		return;

	/* Record offset before appending anything */
	offset       = buf->data_consumed + buf->data_length;

	/* Perform transformation on data *first*, record length appended */
	bytes_length = M_buf_add_bytes_transform(buf, transform_type, bytes, bytes_length);

	/* Ensure there is enough space to do the justification */
	if (width > bytes_length) {
		ensure_space(buf, width);
	}

	/* Justify data in-place */
	justified_length   = M_str_justify_max(((char *)buf->data) + offset, M_MAX(bytes_length, width) + 1, ((char *)buf->data) + offset, bytes_length, justify_type, fill_char, width);
	if (justified_length >= bytes_length) {
		buf->data_length += justified_length - bytes_length;
	} else {
		/* e.g. truncation */
		buf->data_length -= bytes_length - justified_length;
	}

	/* ensure NULL Term */
	buf->data[buf->data_consumed + buf->data_length] = 0;
}


void M_buf_add_bytes_just(M_buf_t *buf, const void *bytes, size_t bytes_length, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width)
{
	M_buf_add_bytes_just_transform(buf, M_BUF_TRANSFORM_NONE, bytes, bytes_length, justify_type, fill_char, width);
}


void M_buf_add_str_just_transform(M_buf_t *buf, M_uint32 transform_type, const char *str, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width)
{
	size_t str_len;

	/* A NULL string is ok, it just means we want to completely fill the destination with the fill_char */
	str_len = M_str_len(str);

	M_buf_add_bytes_just_transform(buf, transform_type, str, str_len, justify_type, fill_char, width);
}


void M_buf_add_str_just(M_buf_t *buf, const char *str, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width)
{
	M_buf_add_str_just_transform(buf, M_BUF_TRANSFORM_NONE, str, justify_type, fill_char, width);
}


static size_t num_udigits(M_uint64 n, unsigned char base)
{
	size_t count = 0;

	do {
		n /= base;
		count++;
	} while (n != 0);

	return count;
}

M_bool M_buf_add_uint_just(M_buf_t *buf, M_uint64 n, size_t width)
{
	char space[32];

	if (buf == NULL || width == 0)
		return M_FALSE;

	M_snprintf(space, sizeof(space), "%llu", n);
	M_buf_add_str_just(buf, space, M_STR_JUSTIFY_RIGHT, '0', width);
	return M_str_len(space) <= width;
}

void M_buf_add_byte(M_buf_t *buf, unsigned char byte)
{
	M_buf_add_bytes(buf, &byte, 1);
}

void M_buf_add_char(M_buf_t *buf, char c)
{
	M_buf_add_bytes(buf, (unsigned char *)&c, 1);
}

void M_buf_add_ptr(M_buf_t *buf, void *ptr)
{
	char temp[64];

	if (buf == NULL)
		return;

	M_snprintf(temp, sizeof(temp), "%p", ptr);
	M_buf_add_str(buf, temp);
}

static size_t num_digits(M_int64 n, unsigned char base)
{
	size_t count = 0;

	if (n < 0) {
		n *= -1;
		count++;
	}

	do {
		n /= base;
		count++;
	} while (n != 0);

	return count;
}

M_bool M_buf_add_int_just(M_buf_t *buf, M_int64 n, size_t width)
{
	char space[32];

	if (buf == NULL || width == 0)
		return M_FALSE;

	/* Prepend negative */
	if (width >= 1 && n < 0) {
		M_buf_add_byte(buf, '-');
		n *= -1;
		width--;
	}

	M_snprintf(space, sizeof(space), "%lld", n);
	M_buf_add_str_just(buf, space, M_STR_JUSTIFY_RIGHT, '0', width);
	return M_str_len(space) <= width;
}

M_bool M_buf_add_uintbin(M_buf_t *buf, M_uint64 n, size_t width, M_endian_t endianness)
{
	size_t i;
	size_t shift;

	if (buf == NULL)
		return M_FALSE;

	/* Range is 1-8. */
	if (width > 8 || width < 1) 
		return M_FALSE; 

	/* Check to make sure n isn't too large for the number of bytes requestd. */ 
	if (width != 8 && n >= ((M_uint64)1 << (width * 8))) 
		return M_FALSE; 

	for (i=0; i<width; i++) {
		if (endianness == M_ENDIAN_BIG) {
			shift = (width-1-i) * 8;
		} else {
			shift = i * 8;
		}
		M_buf_add_byte(buf, (unsigned char)((n >> shift) & 0xFF));
	}

	return M_TRUE;
}

M_bool M_buf_add_uintstrbin(M_buf_t *buf, const char *s, unsigned char base, size_t width, M_endian_t endianness)
{
	M_uint64 n;

	if (buf == NULL || M_str_isempty(s))
		return M_FALSE;

	if (M_str_to_uint64_ex(s, M_str_len(s), base, &n, NULL) != M_STR_INT_SUCCESS)
		return M_FALSE;

	return M_buf_add_uintbin(buf, n, width, endianness);
}

M_bool M_buf_add_uintbcd(M_buf_t *buf, M_uint64 n, size_t width)
{
	/* Maximum number of digits for a M_uint64 is 20. Packing 2 digits into 1 bytes means
 	 * we have a maximum size of 10. */
	unsigned char tmp[10];
	size_t        len;
	size_t        digits;
	size_t        i;

	if (buf == NULL)
		return M_FALSE;

	digits = num_udigits(n, 10); 
	len    = (digits/2) + (digits%2);
	if (len > width)
		return M_FALSE;

	if (len < width)
		M_buf_add_fill(buf, 0, width-len);

	if (n == 0) {
		M_buf_add_byte(buf, 0);
		return M_TRUE;
	}

	M_mem_set(tmp, 0, sizeof(tmp));
	for (i=0; n>0; i++) {
		tmp[i] = (unsigned char)(n % 100);
		n /= 100;
	}

	while (i>0) {
		i--;
		M_buf_add_byte(buf, (unsigned char)(((tmp[i]/10) << 4) | (tmp[i] % 10)));
	}

	return M_TRUE;
}

M_bool M_buf_add_uinthex(M_buf_t *buf, M_uint64 n, M_bool is_upper, size_t width)
{
	size_t digits;
	char   temp[17]; /* Max value is 64bits : 0xFFFFFFFFFFFFFFFF */

	if (buf == NULL)
		return M_FALSE;

	if (is_upper) {
		digits = M_snprintf(temp, sizeof(temp), "%llX", n);
	} else {
		digits = M_snprintf(temp, sizeof(temp), "%llx", n);
	}

	if (width && digits > width)
		return M_FALSE;

	if (digits < width) {
		M_buf_add_fill(buf, '0', width-digits);
	}

	M_buf_add_str(buf, temp);
	return M_TRUE;
}

void M_buf_add_bytehex(M_buf_t *buf, unsigned char byte, M_bool is_upper)
{
	M_buf_add_uinthex(buf, byte, is_upper, 2);
}

M_bool M_buf_add_uintstrbcd(M_buf_t *buf, const char *s, unsigned char base, size_t width)
{
	M_uint64 n;

	if (buf == NULL || M_str_isempty(s))
		return M_FALSE;

	if (M_str_to_uint64_ex(s, M_str_len(s), base, &n, NULL) != M_STR_INT_SUCCESS)
		return M_FALSE;

	return M_buf_add_uintbcd(buf, n, width);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_buf_add_money(M_buf_t *buf, const char *amount, size_t max_width)
{
	return M_buf_add_int_money(buf, M_atofi_prec(amount, 2), max_width);
}

M_bool M_buf_add_money_dot(M_buf_t *buf, const char *amount, size_t max_width)
{
	return M_buf_add_int_money_dot(buf, M_atofi_prec(amount, 2), max_width);
}

M_bool M_buf_add_money_just(M_buf_t *buf, const char *amount, size_t max_width)
{
	return M_buf_add_int_money_just(buf, M_atofi_prec(amount, 2), max_width);
}

M_bool M_buf_add_money_dot_just(M_buf_t *buf, const char *amount, size_t max_width)
{
	return M_buf_add_int_money_dot_just(buf, M_atofi_prec(amount, 2), max_width);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool  M_buf_add_int_money(M_buf_t *buf, M_int64 amount, size_t max_width)
{
	if (buf == NULL)
		return M_FALSE;

	if (num_digits(amount, 10) > max_width)
		return M_FALSE;

	M_buf_add_int(buf, amount);
	return M_TRUE;
}

M_bool M_buf_add_int_money_dot(M_buf_t *buf, M_int64 amount, size_t max_width)
{
	char   space[32];
	size_t r;
	M_uint64 amnt = (M_uint64)M_ABS(amount);

	if (buf == NULL)
		return M_FALSE;

	r = M_snprintf(space, sizeof(space), "%s%llu.%02llu", amount<0?"-":"", amnt / 100, amnt % 100);
	if (r > max_width)
		return M_FALSE;

	M_buf_add_bytes(buf, space, r);
	return M_TRUE;
}

M_bool M_buf_add_int_money_just(M_buf_t *buf, M_int64 amount, size_t max_width)
{
	return M_buf_add_int_just(buf, amount, max_width);
}

M_bool M_buf_add_int_money_dot_just(M_buf_t *buf, M_int64 amount, size_t max_width)
{
	char   space[32];
	size_t r;

	if (buf == NULL)
		return M_FALSE;

	/* Prepend negative */
	if (max_width >= 1 && amount < 0) {
		M_buf_add_byte(buf, '-');
		amount *= -1;
		max_width--;
	}

	r = M_snprintf(space, sizeof(space), "%lld.%02lld", amount / 100, amount % 100);
	if (r > max_width)
		return M_FALSE;

	M_buf_add_str_just(buf, space, M_STR_JUSTIFY_RIGHT, '0', max_width);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_buf_add_decimal(M_buf_t *buf, const M_decimal_t *decimal, M_bool implied_decimal, M_int8 num_decimals, size_t max_width)
{
	char                  outbuf[256];
	M_decimal_t           dupdec;
	enum M_DECIMAL_RETVAL rv;
	size_t                len;

	if (buf == NULL || decimal == NULL)
		return M_FALSE;

	M_decimal_duplicate(&dupdec, decimal);
	if (num_decimals != -1) {
		rv = M_decimal_transform(&dupdec, (M_uint8)num_decimals);
		if (rv != M_DECIMAL_SUCCESS && rv != M_DECIMAL_TRUNCATION)
			return M_FALSE;
	}

	if (implied_decimal) {
		/* Can't use M_buf_add_int() as it doesn't take a width */
		len = M_snprintf(outbuf, sizeof(outbuf), "%lld", dupdec.num);
	} else {
		if (M_decimal_to_str(&dupdec, outbuf, sizeof(outbuf)) != M_DECIMAL_SUCCESS)
			return M_FALSE;
		len = M_str_len(outbuf);
	}

	if (max_width) {
		M_buf_add_str_just(buf, outbuf, M_STR_JUSTIFY_TRUNC_LEFT, 0, max_width);
	} else {
		M_buf_add_str(buf, outbuf);
	}

	if (max_width && len > max_width)
		return M_FALSE;

	return M_TRUE;
}

M_bool M_buf_add_decimal_just(M_buf_t *buf, const M_decimal_t *decimal, M_bool implied_decimal, M_int8 num_decimals, size_t max_width)
{
	char                  outbuf[256];
	const char           *ptr;
	M_decimal_t           dupdec;
	enum M_DECIMAL_RETVAL rv;
	size_t                len;

	if (buf == NULL || decimal == NULL || max_width == 0)
		return M_FALSE;

	M_decimal_duplicate(&dupdec, decimal);
	if (num_decimals != -1) {
		rv = M_decimal_transform(&dupdec, (M_uint8)num_decimals);
		if (rv != M_DECIMAL_SUCCESS && rv != M_DECIMAL_TRUNCATION)
			return M_FALSE;
	}

	if (implied_decimal) {
		return M_buf_add_int_just(buf, dupdec.num, max_width);
	}

	if (M_decimal_to_str(&dupdec, outbuf, sizeof(outbuf)) != M_DECIMAL_SUCCESS)
		return M_FALSE;

	len = M_str_len(outbuf);
	if (len > max_width || len == 0)
		return M_FALSE;

	ptr = outbuf;

	/* Add negative sign before padding rest with zeros */
	if (*ptr == '-') {
		ptr++;
		max_width--;
		M_buf_add_byte(buf, '-');
	}

	M_buf_add_str_just(buf, ptr, M_STR_JUSTIFY_LEFT, '0', max_width);

	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_buf_add_bytes_replace(M_buf_t *dest_buf, const M_uint8 *src, size_t src_len,
	const M_uint8 *search_bytes, size_t search_len, const M_uint8 *replace_bytes, size_t replace_len)
{
	const M_uint8 *src_end  = src + src_len;
	const M_uint8 *chunk_end;

	if (dest_buf == NULL || search_bytes == NULL || search_len == 0) {
		return M_FALSE;
	}

	/* Alias check: don't allow caller to pass memory from dest_buf as src. */
	if (src == (const M_uint8 *)M_buf_peek(dest_buf)) {
		return M_FALSE;
	}

	/* If given input array is empty, return success without modifying anything. */
	if (src == NULL || src_len == 0) {
		return M_TRUE;
	}

	chunk_end = M_mem_mem(src, (size_t)(src_end - src), search_bytes, search_len);
	while (chunk_end != NULL) {
		/* Add bytes from current position to just before search string, then add replace string bytes. */
		M_buf_add_bytes(dest_buf, src, (size_t)(chunk_end - src));
		M_buf_add_bytes(dest_buf, replace_bytes, replace_len);

		/* Update data to point to start of next chunk (first char after end of search string). */
		src = chunk_end + search_len;

		/* Find next instance of search string. */
		chunk_end = M_mem_mem(src, (size_t)(src_end - src), search_bytes, search_len);
	}

	/* If there's any data left after the last instance of the search string, add it. */
	if (src < src_end) {
		M_buf_add_bytes(dest_buf, src, (size_t)(src_end - src));
	}

	return M_TRUE;
}

M_bool M_buf_add_str_replace(M_buf_t *dest_buf, const char *src_str,
	const char *search_str, const char *replace_str)
{
	return M_buf_add_bytes_replace(dest_buf, (const M_uint8 *)src_str, M_str_len(src_str),
		(const M_uint8 *)search_str, M_str_len(search_str),
		(const M_uint8 *)replace_str, M_str_len(replace_str));
}


void M_buf_add_str_quoted(M_buf_t *buf, char quote_char, char escape_char, const char *quote_req_chars, M_bool always_quote, const char *src)
{
	M_int8 quote_level = 0; /* 0 = no quoting, 1 = simple quoting, 2 = escaping */
	size_t i;

	if (buf == NULL || src == NULL)
		return;

	if (always_quote)
		quote_level = 1;

	for (i=0; src[i] != '\0' && quote_level < 2; i++) {
		size_t j;

		if (src[i] == quote_char || src[i] == escape_char) {
			quote_level = 2;
			break;
		}

		for (j=0; quote_level ==0 && quote_req_chars && quote_req_chars[j] != 0; j++) {
			if (src[i] == quote_req_chars[j])
				quote_level = 1;
		}
	}

	if (quote_level) {
		M_buf_add_byte(buf, (M_uint8)quote_char);
	}

	if (quote_level < 2) {
		M_buf_add_str(buf, src);
	} else {
		for (i=0; src[i] != '\0'; i++) {
			if (src[i] == quote_char || src[i] == escape_char) {
				M_buf_add_byte(buf, (M_uint8)escape_char);
			}
			M_buf_add_byte(buf, (M_uint8)src[i]);
		}
	}

	if (quote_level) {
		M_buf_add_byte(buf, (M_uint8)quote_char);
	}

}

void M_buf_trim(M_buf_t *buf)
{
	char   *start;
	char   *curr;
	size_t  drop_num;
	size_t  trunc_num;

	if (buf == NULL || buf->data == NULL || buf->data_length == 0) {
		return;
	}

	start = ((char *)buf->data) + buf->data_consumed;

	drop_num = 0;
	curr     = start;
	while (M_chr_isspace(*curr)) {
		drop_num++;
		curr++;
	}

	trunc_num = buf->data_length;
	curr      = start + buf->data_length;
	while (curr > start && M_chr_isspace(*(curr - 1))) {
		trunc_num--;
		curr--;
	}

	M_buf_truncate(buf, trunc_num);
	M_buf_drop(buf, drop_num);
}
