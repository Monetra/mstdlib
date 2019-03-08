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

static void M_str_hexdump_center(M_buf_t *buf, const char *str, size_t width, unsigned char pad)
{
	size_t lpad;
	size_t rpad;
	size_t len = M_str_len(str);

	lpad = (width - len) / 2;
	rpad = (width - len - lpad);

	M_buf_add_fill(buf, pad, lpad);
	M_buf_add_str(buf, str);
	M_buf_add_fill(buf, pad, rpad);
}


char *M_str_hexdump(int flags, size_t bytes_per_line, const char *line_prefix, const unsigned char *data, size_t data_len)
{
	char     temp[9];
	size_t   len_size   = 0;
	size_t   hex_size   = 0;
	size_t   ascii_size = 0;
	size_t   offset     = 0;
	size_t   num_sects  = 0;
	size_t   sep_len    = 0;
	M_buf_t *buf        = M_buf_create();

	/* Get maximum length for counter */
	if (flags & M_STR_HEXDUMP_NOLEN) {
		len_size = 0;
	} else {
		if (flags & M_STR_HEXDUMP_DECLEN) {
			len_size = M_snprintf(temp, sizeof(temp), "%zu", data_len);
		} else {
			len_size = M_snprintf(temp, sizeof(temp), "%zX", data_len);
		}
		if (len_size < 4) {
			len_size = 4;
		}
	}

	/* Get size of hexdump */
	if (bytes_per_line == 0)
		bytes_per_line = 16;
	hex_size = bytes_per_line * 3 - 1;
	if (hex_size < 3)
		hex_size = 3;

	/* Calculate number of 8-byte sections */
	num_sects = (bytes_per_line + 7) / 8;
	if (num_sects > 0)
		num_sects--;

	if (!(flags & M_STR_HEXDUMP_NOSECTS)) {
		hex_size += num_sects;
		sep_len   = 2;
	} else {
		sep_len   = 1;
	}

	/* Get size of ASCII */
	if (flags & M_STR_HEXDUMP_NOASCII) {
		ascii_size = 0;
	} else {
		ascii_size = bytes_per_line + 2;
	}

	/* Add header */
	if (flags & M_STR_HEXDUMP_HEADER) {
		M_buf_add_str(buf, line_prefix);

		/* LEN header */
		if (!(flags & M_STR_HEXDUMP_NOLEN)) {
			M_str_hexdump_center(buf, (flags & M_STR_HEXDUMP_DECLEN)?"LEN":"ADDR", len_size, ' ');
			M_buf_add_fill(buf, ' ', sep_len);
		}

		/* HEX header */
		M_str_hexdump_center(buf, "HEX", hex_size, ' ');

		/* ASCII Header */
		if (!(flags & M_STR_HEXDUMP_NOASCII)) {
			M_buf_add_fill(buf, ' ', sep_len);
			M_str_hexdump_center(buf, "ASCII", ascii_size, ' ');
		}

		M_buf_add_str(buf, (flags & M_STR_HEXDUMP_CRLF)?"\r\n":"\n");

		/* Separator ==========+===========+========= */
		M_buf_add_str(buf, line_prefix);

		if (!(flags & M_STR_HEXDUMP_NOLEN)) {
			M_str_hexdump_center(buf, "", len_size, '=');
			M_buf_add_fill(buf, ' ', sep_len);
		}

		M_str_hexdump_center(buf, "", hex_size, '=');

		if (!(flags & M_STR_HEXDUMP_NOASCII)) {
			M_buf_add_fill(buf, ' ', sep_len);
			M_str_hexdump_center(buf, "", ascii_size, '=');
		}

		M_buf_add_str(buf, (flags & M_STR_HEXDUMP_CRLF)?"\r\n":"\n");
	}

	/* Output each line */
	while (offset < data_len) {
		size_t i;

		M_buf_add_str(buf, line_prefix);

		/* Len */
		if (!(flags & M_STR_HEXDUMP_NOLEN)) {
			if (flags & M_STR_HEXDUMP_DECLEN) {
				M_buf_add_uint_just(buf, offset, len_size);
			} else {
				M_buf_add_uinthex(buf, offset, (flags & M_STR_HEXDUMP_UPPER)?M_TRUE:M_FALSE, len_size);
			}
			M_buf_add_fill(buf, ' ', sep_len);
		}

		/* Hex */
		for (i=0; i<bytes_per_line; i++) {
			if (i != 0) {
				/* Put slight emphasis on grouping of 8 bytes for readability */
				if (i % 8 == 0) {
					M_buf_add_fill(buf, ' ', sep_len);
				} else {
					M_buf_add_fill(buf, ' ', 1);
				}
			}

			if (offset + i < data_len) {
				M_buf_add_uinthex(buf, data[offset+i], (flags & M_STR_HEXDUMP_UPPER)?M_TRUE:M_FALSE, 2);
			} else {
				M_buf_add_fill(buf, ' ', 2);
			}
		}

		if (!(flags & M_STR_HEXDUMP_NOASCII)) {
			M_buf_add_fill(buf, ' ', sep_len);
			M_buf_add_byte(buf, '|');
			for (i=0; i<bytes_per_line; i++) {
				if (offset + i < data_len) {
					M_buf_add_byte(buf, (unsigned char)((M_chr_isgraph((char)data[offset+i]) || data[offset+i] == ' ')?data[offset+i]:'.'));
				} else {
					M_buf_add_byte(buf, ' ');
				}
			}
			M_buf_add_byte(buf, '|');
		}

		M_buf_add_str(buf, (flags & M_STR_HEXDUMP_CRLF)?"\r\n":"\n");

		offset += bytes_per_line;
	}

	/* Common output format has trailing line only having a length */
	if (!(flags & M_STR_HEXDUMP_NOLEN)) {
		M_buf_add_str(buf, line_prefix);
		if (flags & M_STR_HEXDUMP_DECLEN) {
			M_buf_add_uint_just(buf, data_len, len_size);
		} else {
			M_buf_add_uinthex(buf, data_len, (flags & M_STR_HEXDUMP_UPPER)?M_TRUE:M_FALSE, len_size);
		}
	}

	return M_buf_finish_str(buf, NULL);
}

