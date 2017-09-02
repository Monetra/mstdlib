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
	M_uint8       *bytes_copy;    /* If we own a copy of the data we're parsing, the copy will be stored here. */
}; /* typedef'd to M_bit_parser_t in header. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool peek_next_bit(M_bit_parser_t *bparser, M_uint8 *bit)
{
	size_t byte_idx;
	size_t pos_in_byte;
	
	if (bparser->offset >= bparser->nbits) {
		return M_FALSE;
	}

	byte_idx    = bparser->offset / 8;
	pos_in_byte = 7 - (bparser->offset % 8);
	
	if ((bparser->bytes[byte_idx] & (1 << pos_in_byte)) != 0) {
		*bit = 1;
	} else {
		*bit = 0;
	}
	
	return M_TRUE;
}


M_bit_parser_t *M_bit_parser_create(const void *bytes, size_t nbits)
{
	M_bit_parser_t *bparser;
	size_t          nbytes;
	
	nbytes = (nbits + 7) / 8;
	
	bparser = M_malloc_zero(sizeof(*bparser));
	
	bparser->nbits      = nbits;
	bparser->bytes_copy = M_memdup(bytes, nbytes);
	bparser->bytes      = bparser->bytes_copy;
	
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


void M_bit_parser_reset(M_bit_parser_t *bparser, const void *bytes, size_t nbits)
{
	size_t nbytes;

	if (bparser == NULL) {
		return;
	}

	/* Wipe out old data, if any. This ensures that, even if the new data set is smaller than the old one,
	 * any sensitive stuff at the end of the array will always be cleared out.
	 */
	if (bparser->bytes_copy != NULL) {
		M_mem_set(bparser->bytes_copy, 0xFF, (bparser->nbits + 7) / 8);
	}

	/* Store new data (if any). */
	if (nbits > 0 && bytes != NULL) {
		nbytes = (nbits + 7) / 8;
		if (bparser->bytes_copy != NULL && bparser->nbits >= nbits) {
			/* If we already have internally-allocated memory that's big enough, reuse it. */
			M_mem_copy(bparser->bytes_copy, bytes, nbytes);
		} else {
			/* If it's not big enough, reallocate a larger chunk and use that. */
			M_free(bparser->bytes_copy);
			bparser->bytes_copy = M_memdup(bytes, nbytes);
		}
	}

	/* Reset all the other parser parameters. */
	bparser->offset        = 0;
	bparser->marked_offset = 0;
	bparser->nbits         = nbits;
	bparser->bytes         = bparser->bytes_copy;
}


void M_bit_parser_destroy(M_bit_parser_t *bparser)
{
	if (bparser == NULL) {
		return;
	}
	
	M_free(bparser->bytes_copy);
	
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

	if (nbits > M_bit_parser_len(bparser)) {
		return M_FALSE;
	}
	
	bparser->offset += nbits;
	return M_TRUE;
}


M_bool M_bit_parser_peek_bit(M_bit_parser_t *bparser, M_uint8 *bit)
{
	if (bparser == NULL || bit == NULL) {
		return M_FALSE;
	}

	if (!peek_next_bit(bparser, bit)) {
		return M_FALSE;
	}

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
	M_uint8 bit;
	size_t  i;

	if (nbits == 0) {
		return M_TRUE;
	}

	if (bbuf == NULL || M_bit_parser_len(bparser) < nbits) {
		return M_FALSE;
	}

	for (i=0; i<nbits; i++) {
		peek_next_bit(bparser, &bit);
		bparser->offset++;

		M_bit_buf_add_bit(bbuf, bit);
	}

	return M_TRUE;
}


char *M_bit_parser_read_strdup(M_bit_parser_t *bparser, size_t nbits)
{
	M_buf_t *buf;
	M_uint8  bit;
	size_t   i;
	
	if (nbits == 0 || M_bit_parser_len(bparser) < nbits) {
		return NULL;
	}
	
	buf = M_buf_create();
	
	for (i=0; i<nbits; i++) {
		peek_next_bit(bparser, &bit);
		bparser->offset++;

		M_buf_add_byte(buf, (bit == 0)? '0' : '1');
	}
	
	return M_buf_finish_str(buf, NULL);
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

	if (bparser == NULL || bit == NULL || max_bits == 0) {
		return M_FALSE;
	}

	/* Read first bit in range. If no bits remain in the parser, return false. */
	if (!peek_next_bit(bparser, bit)) {
		return M_FALSE;
	}

	/* Read more bits until we hit the maximum number of bits set by the caller, we reach the end of the data, or
	 * we hit a bit that's different than the first one.
	 */
	do {
		(*nbits_in_range)++;
		bparser->offset++;
	} while ((*nbits_in_range) < max_bits && peek_next_bit(bparser, &next_bit) && next_bit == *bit);

	return M_TRUE;
}
