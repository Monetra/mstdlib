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
#include "m_defs_int.h"

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_bit_parser {
	const M_uint8 *bytes;

	size_t         nbits;
	size_t         offset;
	size_t         marked_offset;
	M_bit_buf_t   *bbuf;         /* If we own a copy of the data we're parsing, the copy will be stored here. */
}; /* typedef'd to M_bit_parser_t in header. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_uint8 peek_next_bit(M_bit_parser_t *bparser)
{
	size_t  byte_idx    = bparser->offset / 8;
	size_t  pos_in_byte = 7 - (bparser->offset % 8);
	M_uint8 bit;

	if ((bparser->bytes[byte_idx] & (1 << pos_in_byte)) != 0) {
		bit = 1;
	} else {
		bit = 0;
	}

	return bit;
}


/* Return number of bytes written. */
static size_t peek_next_bytes(M_bit_parser_t *bparser, M_uint8 *dest, size_t destlen, size_t nbits, M_bool strict_pad)
{
	size_t         byteidx;
	size_t         bitskip;
	size_t         nbytes;
	const M_uint8 *src;

	if (nbits == 0) {
		return 0;
	}
	if (bparser->offset + nbits > bparser->nbits) {
		return 0;
	}

	nbytes  = (nbits + 7) / 8;
	byteidx = bparser->offset / 8;
	bitskip = bparser->offset % 8; /* number of bits to skip past on left side of first byte of source. */
	src     = bparser->bytes + byteidx;

	if (destlen < nbytes) {
		return 0;
	}

	if (bitskip == 0) {
		/* If the data we're reading starts on a byte boundary, copy the data over byte-wise. */
		M_mem_copy(dest, src, nbytes);
	} else {
		M_uint8 invskip = (M_uint8)(8 - bitskip); /* number of bits to skip from right side of next byte */
		size_t  i;
		for (i=0; i<nbytes; i++) {
			dest[i] = (M_uint8)((src[i] << bitskip) | (src[i + 1] >> invskip));
		}
	}

	/* Make sure any extra padding bits on end of last byte are set to zero. */
	if (strict_pad) {
		size_t pad = nbits % 8;
		if (pad > 0) {
			pad = 8 - pad;
			dest[nbytes - 1] = (M_uint8)(dest[nbytes - 1] & ~(M_uint8)((1 << pad) - 1));
		}
	}

	return nbytes;
}


M_bit_parser_t *M_bit_parser_create(const void *bytes, size_t nbits)
{
	M_bit_parser_t *bparser;

	bparser        = M_malloc_zero(sizeof(*bparser));

	bparser->bbuf  = M_bit_buf_create();
	M_bit_buf_add_bytes(bparser->bbuf, bytes, nbits);

	bparser->nbits = nbits;
	bparser->bytes = M_bit_buf_peek(bparser->bbuf);

	return bparser;
}


M_bit_parser_t *M_bit_parser_create_const(const void *bytes, size_t nbits)
{
	M_bit_parser_t *bparser;

	bparser = M_malloc_zero(sizeof(*bparser));

	bparser->nbits = nbits;
	bparser->bytes = bytes;

	return bparser;
}


void M_bit_parser_append(M_bit_parser_t *bparser, const void *bytes, size_t nbits)
{
	if (bparser == NULL || bytes == NULL || nbits == 0) {
		return;
	}

	if (bparser->bbuf == NULL) {
		/* If this used to be a const parser, create a new internal buffer and copy the old
		 * const data into it.
		 */
		bparser->bbuf = M_bit_buf_create();
		M_bit_buf_reserve(bparser->bbuf, bparser->nbits + nbits);
		M_bit_buf_add_bytes(bparser->bbuf, bparser->bytes, bparser->nbits);
	}

	M_bit_buf_add_bytes(bparser->bbuf, bytes, nbits);
	bparser->nbits = M_bit_buf_len(bparser->bbuf);
	bparser->bytes = M_bit_buf_peek(bparser->bbuf);
}


void M_bit_parser_reset(M_bit_parser_t *bparser, const void *bytes, size_t nbits)
{
	if (bparser == NULL) {
		return;
	}

	/* Wipe out old data, if any. This ensures that, even if the new data set is smaller than the old one,
	 * any sensitive stuff at the end of the array will always be cleared out.
	 */
	if (bparser->bbuf != NULL) {
		M_bit_buf_truncate(bparser->bbuf, 0);
	} else {
		bparser->bbuf = M_bit_buf_create();
	}

	/* Store new data (if any). */
	M_bit_buf_add_bytes(bparser->bbuf, bytes, nbits);

	/* Reset all the other parser parameters. */
	bparser->offset        = 0;
	bparser->marked_offset = 0;
	bparser->nbits         = M_bit_buf_len(bparser->bbuf);
	bparser->bytes         = M_bit_buf_peek(bparser->bbuf);
}


void M_bit_parser_destroy(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return;
	}

	M_bit_buf_destroy(bparser->bbuf);

	M_free(bparser);
}


size_t M_bit_parser_len(M_bit_parser_t *bparser)
{
	if (bparser == NULL || bparser->offset >= bparser->nbits) {
		return 0;
	}

	return bparser->nbits - bparser->offset;
}


size_t M_bit_parser_current_offset(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return 0;
	}

	return bparser->offset;
}


void M_bit_parser_rewind_to_start(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return;
	}

	bparser->offset        = 0;
	bparser->marked_offset = 0;
}


void M_bit_parser_mark(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return;
	}

	bparser->marked_offset = bparser->offset;
}


size_t M_bit_parser_mark_len(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return 0;
	}

	return bparser->offset - bparser->marked_offset;
}


size_t M_bit_parser_mark_rewind(M_bit_parser_t *bparser)
{
	size_t nrewind;

	if (bparser == NULL) {
		return 0;
	}

	nrewind         = bparser->offset - bparser->marked_offset;
	bparser->offset = bparser->marked_offset;

	return nrewind;
}


M_bool M_bit_parser_consume(M_bit_parser_t *bparser, size_t nbits)
{
	if (nbits == 0) {
		return M_TRUE;
	}

	if (bparser == NULL || nbits > M_bit_parser_len(bparser)) {
		return M_FALSE;
	}

	bparser->offset += nbits;
	return M_TRUE;
}


M_bool M_bit_parser_peek_bit(M_bit_parser_t *bparser, M_uint8 *bit)
{
	if (bparser == NULL || bit == NULL || bparser->offset >= bparser->nbits) {
		return M_FALSE;
	}

	*bit = peek_next_bit(bparser);

	return M_TRUE;
}


M_bool M_bit_parser_read_bit(M_bit_parser_t *bparser, M_uint8 *bit)
{
	if (!M_bit_parser_peek_bit(bparser, bit)) {
		return M_FALSE;
	}
	bparser->offset++;
	return M_TRUE;
}


M_bool M_bit_parser_read_bit_buf(M_bit_parser_t *bparser, M_bit_buf_t *bbuf, size_t nbits)
{
	size_t i;

	if (nbits == 0) {
		return M_TRUE;
	}

	if (bbuf == NULL || M_bit_parser_len(bparser) < nbits) {
		return M_FALSE;
	}

	for (i=0; i<nbits; i++) {
		M_bit_buf_add_bit(bbuf, peek_next_bit(bparser));
		bparser->offset++;
	}

	return M_TRUE;
}


M_bool M_bit_parser_read_buf(M_bit_parser_t *bparser, M_buf_t *buf, size_t nbits)
{
	M_uint8 *dest;
	size_t   destlen;

	if (nbits == 0) {
		return M_TRUE;
	}
	if (bparser == NULL || buf == NULL) {
		return M_FALSE;
	}

	destlen = (nbits + 7) / 8;
	dest    = M_buf_direct_write_start(buf, &destlen);
	destlen = peek_next_bytes(bparser, dest, destlen, nbits, M_TRUE);

	M_buf_direct_write_end(buf, destlen);

	if (destlen == 0) {
		return M_FALSE;
	}

	bparser->offset += nbits;
	return M_TRUE;
}


M_bool M_bit_parser_read_bytes(M_bit_parser_t *bparser, M_uint8 *dest, size_t *destlen, size_t nbits)
{
	if (nbits == 0) {
		if (destlen != NULL) {
			*destlen = 0;
		}
		return M_TRUE;
	}
	if (bparser == NULL || dest == NULL || destlen == NULL || *destlen == 0) {
		if (destlen != NULL) {
			*destlen = 0;
		}
		return M_FALSE;
	}

	*destlen = peek_next_bytes(bparser, dest, *destlen, nbits, M_TRUE);

	if (*destlen == 0) {
		return M_FALSE;
	}
	bparser->offset += nbits;
	return M_TRUE;
}


char *M_bit_parser_read_strdup(M_bit_parser_t *bparser, size_t nbits)
{
	M_buf_t *buf;
	size_t   i;

	if (nbits == 0 || M_bit_parser_len(bparser) < nbits) {
		return NULL;
	}

	buf = M_buf_create();

	for (i=0; i<nbits; i++) {
		M_buf_add_byte(buf, (peek_next_bit(bparser) == 0)? '0' : '1');
		bparser->offset++;
	}

	return M_buf_finish_str(buf, NULL);
}


M_bool M_bit_parser_read_uint(M_bit_parser_t *bparser, size_t nbits, M_uint64 *res)
{
	M_uint8 arr[8] = {0};
	size_t  i;
	size_t  nbytes;

	if (res == NULL) {
		return M_FALSE;
	}
	*res = 0;

	if (nbits == 0) {
		return M_TRUE;
	}

	if (bparser == NULL || nbits > 64 || M_bit_parser_len(bparser) < nbits) {
		return M_FALSE;
	}

	nbytes = peek_next_bytes(bparser, arr, sizeof(arr), nbits, M_FALSE);
	if (nbytes == 0) {
		return M_FALSE;
	}
	for (i=0; i<nbytes; i++) {
		*res <<= 8;
		*res = (M_uint64)(*res | arr[i]);
	}

	/* If last byte was a partial, shift right to remove padding bits. */
	i = nbits % 8;
	if (i > 0) {
		*res >>= (8 - i);
	}

	bparser->offset += nbits;

	return M_TRUE;
}


M_bool M_bit_parser_read_int(M_bit_parser_t *bparser, size_t nbits, M_bit_parser_int_format_t fmt, M_int64 *res)
{
	M_uint64 mask;
	M_uint8  sign;
	M_uint64 val;

	if (res != NULL) {
		*res = 0;
	}

	if (bparser == NULL || nbits < 2 || res == NULL) {
		return M_FALSE;
	}

	switch (fmt) {
		case M_BIT_PARSER_SIGN_MAG:  /* Signed magnitude. */
			if (M_bit_parser_read_bit(bparser, &sign) && M_bit_parser_read_uint(bparser, nbits - 1, &val)) {
				*res = (sign == 0)? (M_int64)val : -(M_int64)val;
			}
			break;
		case M_BIT_PARSER_ONES_COMP: /* One's complement. */
			if (M_bit_parser_read_uint(bparser, nbits, &val)) {
				mask = (M_uint64)1 << (nbits - 1);
				if ((val & mask) != 0) {
					val  = (~val) & (mask - 1);
					*res = -(M_int64)val;
				} else {
					*res = (M_int64)val;
				}
			}
			break;
		case M_BIT_PARSER_TWOS_COMP: /* Two's complement. */
			if (M_bit_parser_read_uint(bparser, nbits, &val)) {
				mask = (M_uint64)1 << (nbits - 1);
				*res = (M_int64)((val ^ mask) - mask);
			}
			break;
		default:
			return M_FALSE;
	}

	return M_TRUE;
}


M_bool M_bit_parser_consume_range(M_bit_parser_t *bparser, size_t max_bits)
{
	M_uint8 bit;
	size_t  nbits;

	return M_bit_parser_read_range(bparser, &bit, &nbits, max_bits);
}


M_bool M_bit_parser_read_range(M_bit_parser_t *bparser, M_uint8 *bit, size_t *nbits_in_range, size_t max_bits)
{
	M_uint8 next_bit;

	if (nbits_in_range == NULL) {
		return M_FALSE;
	}
	*nbits_in_range = 0;

	if (max_bits == 0) {
		return M_FALSE;
	}

	/* Read first bit in range. If no bits remain in the parser, return false. */
	if (!M_bit_parser_peek_bit(bparser, bit)) {
		return M_FALSE;
	}

	/* Read more bits until we hit the maximum number of bits set by the caller, we reach the end of the data, or
	 * we hit a bit that's different than the first one.
	 */
	do {
		(*nbits_in_range)++;
		bparser->offset++;
	} while ((*nbits_in_range) < max_bits && M_bit_parser_peek_bit(bparser, &next_bit) && next_bit == *bit);

	return M_TRUE;
}
