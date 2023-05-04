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
#include "m_defs_int.h"

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_bit_buf {
	M_buf_t *bits;
	size_t   nbits;
};  /* typedef'd to M_bit_buf_t in header. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bit_buf_t *M_bit_buf_create(void)
{
	M_bit_buf_t *bbuf;
	
	bbuf = M_malloc_zero(sizeof(*bbuf));
	
	bbuf->bits = M_buf_create();
	
	return bbuf;
}


void M_bit_buf_destroy(M_bit_buf_t *bbuf)
{
	if (bbuf == NULL) {
		return;
	}

	M_buf_cancel(bbuf->bits);
	M_free(bbuf);
}


M_uint8 *M_bit_buf_finish(M_bit_buf_t *bbuf, size_t *nbytes)
{
	M_buf_t *buf = M_bit_buf_finish_buf(bbuf);

	return M_buf_finish(buf, nbytes);
}


M_buf_t *M_bit_buf_finish_buf(M_bit_buf_t *bbuf)
{
	M_buf_t *buf;
	
	if (bbuf == NULL) {
		return NULL;
	}

	buf        = bbuf->bits;
	bbuf->bits = NULL;
	
	M_bit_buf_destroy(bbuf);

	return buf;
}


size_t M_bit_buf_len(const M_bit_buf_t *bbuf)
{
	if (bbuf == NULL) {
		return 0;
	}
	return bbuf->nbits;
}


size_t M_bit_buf_len_bytes(const M_bit_buf_t *bbuf)
{
	if (bbuf == NULL) {
		return 0;
	}
	return M_buf_len(bbuf->bits);
}


const M_uint8 *M_bit_buf_peek(const M_bit_buf_t *bbuf)
{
	return (const M_uint8 *)M_buf_peek(bbuf->bits);
}


void M_bit_buf_truncate(M_bit_buf_t *bbuf, size_t len_bits)
{
	size_t num_full_bytes = len_bits / 8; /* number of FULL bytes, does not include last partial byte. */
	size_t part_bits      = len_bits % 8;

	if (bbuf == NULL) {
		return;
	}
	bbuf->nbits = len_bits;

	if (part_bits != 0) {
		/* If last byte is partially filled, reset bits that are now unset to zero, truncate buf, then add
		 * partial byte back on.
		 */
		M_uint8  last_byte;
		M_uint8  mask;

		last_byte  = *(const M_uint8 *)(M_buf_peek(bbuf->bits) + num_full_bytes);
		mask       = (M_uint8)((M_uint8)1 << (M_uint8)(8 - part_bits));
		mask       = (M_uint8)~(mask - 1);
		last_byte &= mask;

		M_buf_truncate(bbuf->bits, num_full_bytes);
		M_buf_add_byte(bbuf->bits, last_byte);
	} else {
		M_buf_truncate(bbuf->bits, num_full_bytes);
	}
}


void M_bit_buf_fill(M_bit_buf_t *bbuf, M_uint8 bit, size_t len_bits)
{
	M_uint8 *last_byte;
	M_uint8  next_bit_pos;

	if (bbuf == NULL || len_bits == 0) {
		return;
	}

	while (len_bits > 0) {
		/* Calculate position in last byte where we should store the next bit. */
		next_bit_pos = 7 - (bbuf->nbits % 8);
		if (next_bit_pos == 7 && len_bits >= 8) {
			/* If we're starting a new byte and we have at least a byte worth left to add, add the whole byte at once. */
			M_buf_add_byte(bbuf->bits, bit? M_UINT8_MAX : 0);
			len_bits    -= 8;
			bbuf->nbits += 8;
		} else {
			if (next_bit_pos == 7) {
				M_buf_add_byte(bbuf->bits, 0);
			}

			/* Since new bytes are initialized to zero, only need to set the bit if it's a 1. */
			if (bit != 0) {
				const char *ptr = M_buf_peek(bbuf->bits);
				M_uint8     bits = (M_uint8)((M_uint8)1 << next_bit_pos);
				last_byte   = M_CAST_OFF_CONST(M_uint8 *, ptr) + M_buf_len(bbuf->bits) - 1;
				*last_byte |= bits;
			}

			len_bits--;
			bbuf->nbits++;
		}
	}
}


void M_bit_buf_add_bit(M_bit_buf_t *bbuf, M_uint8 bit)
{
	M_uint8  next_bit_pos;
	M_uint8 *last_byte;

	if (bbuf == NULL) {
		return;
	}

	/* Calculate position in last byte where we should store the next bit. */
	next_bit_pos = 7 - (bbuf->nbits % 8);
	if (next_bit_pos == 7) {
		M_buf_add_byte(bbuf->bits, 0);
	}

	/* Since new bytes are initialized to zero, only need to set the bit if it's a 1. */
	if (bit != 0) {
		const char *ptr  = M_buf_peek(bbuf->bits);
		M_uint8     bits = (M_uint8)(1 << next_bit_pos);
		last_byte   = M_CAST_OFF_CONST(M_uint8 *, ptr) + M_buf_len(bbuf->bits) - 1;
		*last_byte |= bits;
	}

	bbuf->nbits++;
}


void M_bit_buf_set_bit(M_bit_buf_t *bbuf, M_uint8 bit, size_t bit_idx, M_uint8 fill_bit)
{
	size_t old_len = M_bit_buf_len(bbuf);
	if (bbuf == NULL) {
		return;
	}

	if (bit_idx < old_len) {
		M_bit_buf_update_bit(bbuf, bit_idx, bit);
	} else {
		M_bit_buf_fill(bbuf, fill_bit, bit_idx - old_len);
		M_bit_buf_add_bit(bbuf, bit);
	}
}


M_bool M_bit_buf_update_bit(M_bit_buf_t *bbuf, size_t bit_idx, M_uint8 bit)
{
	size_t      byte_idx;
	M_uint8     bit_pos;
	const char *ptr;
	M_uint8    *byte;
	M_uint8     bits_to_set;

	if (bit_idx >= bbuf->nbits) {
		return M_FALSE;
	}

	byte_idx = bit_idx / 8;       /* which byte contains the bit to set */
	bit_pos  = 7 - (bit_idx % 8); /* location within byte of bit to set */

	if (byte_idx >= M_buf_len(bbuf->bits)) {
		/* Shouldn't ever happen unless we have a bug someplace else, but verify just in case. */
		return M_FALSE;
	}

	ptr  = M_buf_peek(bbuf->bits);
	byte = M_CAST_OFF_CONST(M_uint8 *, ptr) + byte_idx;

	bits_to_set = (M_uint8)(1 << bit_pos);
	if (bit == 0) {
		/* Unset the bit. */
		bits_to_set = (M_uint8)(~bits_to_set);
		*byte &= bits_to_set;
	} else {
		/* Set the bit. */
		*byte |= bits_to_set;
	}

	return M_TRUE;
}


void M_bit_buf_add_bytes(M_bit_buf_t *bbuf, const void *vbytes, size_t nbits)
{
	const M_uint8 *bytes        = vbytes;
	size_t         nbytes_whole;
	M_uint8        nbits_left;
	size_t         i;

	if (bbuf == NULL || bytes == NULL || nbits == 0) {
		return;
	}

	nbytes_whole = nbits / 8;
	nbits_left   = nbits % 8;

	/* If the current contents of the bit buffer are fully packed (all bytes full), just add the new bytes directly
	 * to the internal buffer.
	 */
	if ((bbuf->nbits % 8) == 0) {
		M_buf_add_bytes(bbuf->bits, bytes, nbytes_whole + (nbits_left > 0? 1 : 0));
		bbuf->nbits += nbits;
		return;
	}

	for (i = 0; i<nbytes_whole; i++) {
		M_bit_buf_add(bbuf, bytes[i], 8, M_BIT_BUF_PAD_NONE);
	}
	if (nbits_left > 0) {
		/* The last partially-full byte will be justified against the most-significant side of the byte.
		 * Since M_bit_buf_add() assumes that it's justified against the least-significant side, need to
		 * shift the data down to change it to the correct justification.
		 */
		M_bit_buf_add(bbuf, (M_uint64)(bytes[nbytes_whole] >> (8 - nbits_left)), nbits_left, M_BIT_BUF_PAD_NONE);
	}
}


void M_bit_buf_add(M_bit_buf_t *bbuf, M_uint64 bits, size_t nbits, M_bit_buf_pad_t pad)
{
	size_t num_pad_bits = 0;

	if (bbuf == NULL || nbits < 1 || nbits > 64) {
		return;
	}
	
	if (pad != M_BIT_BUF_PAD_NONE) {
		size_t rem = (bbuf->nbits + nbits) % 8;
		if (rem != 0) {
			num_pad_bits = 8 - rem;
		}
	}

	if (pad == M_BIT_BUF_PAD_BEFORE) {
		M_bit_buf_fill(bbuf, 0, num_pad_bits);
	}
	
	while (nbits > 0) {
		M_bit_buf_add_bit(bbuf, (bits >> (nbits - 1)) & 0x1);
		nbits--;
	}
	
	if (pad == M_BIT_BUF_PAD_AFTER) {
		M_bit_buf_fill(bbuf, 0, num_pad_bits);
	}
}


M_bool M_bit_buf_add_bitstr(M_bit_buf_t *bbuf, const char *bitstr, M_bit_buf_pad_t pad)
{
	size_t num_pad_bits = 0;

	if (M_str_isempty(bitstr)) {
		return M_TRUE;
	}
	
	if (bbuf == NULL) {
		return M_FALSE;
	}

	if (pad != M_BIT_BUF_PAD_NONE) {
		size_t      nbits;
		const char *ptr;
		size_t      rem;

		/* Calculate number of non-whitespace chars to get bit length. */
		nbits = 0;
		ptr = bitstr;
		while (*ptr != '\0') {
			if (*ptr == '0' || *ptr == '1') {
				nbits++;
			}
			ptr++;
		}

		rem = (bbuf->nbits + nbits) % 8;
		if (rem != 0) {
			num_pad_bits = 8 - rem;
		}
	}

	if (pad == M_BIT_BUF_PAD_BEFORE) {
		M_bit_buf_fill(bbuf, 0, num_pad_bits);
	}
	
	while (*bitstr != '\0') {
		/* Ignore any whitespace chars we encounter. */
		if (M_chr_isspace(*bitstr)) {
			bitstr++;
			continue;
		}

		if (*bitstr != '0' && *bitstr != '1') {
			return M_FALSE;
		}
		
		M_bit_buf_add_bit(bbuf, (*bitstr == '0')? 0 : 1);
		bitstr++;
	}
	
	if (pad == M_BIT_BUF_PAD_AFTER) {
		M_bit_buf_fill(bbuf, 0, num_pad_bits);
	}
	
	return M_TRUE;
}


void M_bit_buf_reserve(M_bit_buf_t *bbuf, size_t nbits)
{
	size_t nbytes;

	/* Do an empty direct write - this forces ensure_space() to be called internally, without actually
	 * adding anything to the buffer.
	 */
	nbytes = (nbits + 7) / 8;
	M_buf_direct_write_start(bbuf->bits, &nbytes);
	M_buf_direct_write_end(bbuf->bits, 0);
}
