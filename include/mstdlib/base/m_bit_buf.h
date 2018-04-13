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

#ifndef __M_BIT_BUF_H__
#define __M_BIT_BUF_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_bit_buf Bitwise Buffered Data Builder
 *  \ingroup mstdlib_base
 *
 * Allows for buffered writing of data that's organized per-bit, instead of per-byte.
 *
 * Also allows for changing bits that were previously added to the buffer (see M_bit_buf_update_bit()). This
 * allows for random-access setting of individual bits. For example, if you're generating a bit-level image,
 * you can fill the buffer with zero bits, and then set individual bits afterwards in whatever order you wish.
 *
 * When you're done adding data, the contents of the buffer can be output as a continuous byte-array, either
 * raw as an (M_uint8 *), or inside a regular per-byte M_buf_t.
 *
 * Example (creating a buffer, adding data, finishing the buffer):
 *
 * \code{.c}
 *     M_bit_buf_t *bbuf;
 *     M_uint8     *out;
 *     size_t       out_nbytes;
 *
 *     bbuf = M_bit_buf_create();
 *     M_bbuf_add_bit(bbuf, 1);
 *     M_bbuf_add(bbuf, 0xA2C4, 14);   // adds least-significant 14 bits of 0xA2C4
 *     M_bbuf_add_str(bbuf, "100010000"); // adds 9 bits from binary-ASCII
 *
 *     out = M_bit_buf_finish(bbuf, &out_nbytes);
 *
 *     // out now points to a byte buffer containing 3 bytes. Now can output to disk, do further processing, etc.
 *
 *     M_free(out);
 * \endcode
 *
 * @{
 */

struct M_bit_buf;
typedef struct M_bit_buf M_bit_buf_t;

/*! Enumeration for byte-alignment padding mode for M_bit_buf_add().*/
typedef enum {
	M_BIT_BUF_PAD_NONE   = 0, /*!< Don't add any padding. */
	M_BIT_BUF_PAD_BEFORE,     /*!< Pad with zero bits before new value, so that bit stream after add is byte-aligned */
	M_BIT_BUF_PAD_AFTER       /*!< Pad with zero bits after new value, so that bit stream after add is byte-aligned */
} M_bit_buf_pad_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new bit buffer.
 *
 * \return allocated buffer.
 *
 * \see M_bit_buf_destroy
 * \see M_bit_buf_finish
 * \see M_bit_buf_finish_buf
 */
M_API M_bit_buf_t *M_bit_buf_create(void) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Free a bit buffer, discarding its data.
 *
 * \param[in] bbuf bit buffer to destroy
 */
M_API void M_bit_buf_destroy(M_bit_buf_t *bbuf) M_FREE(1);


/*! Free a buffer, saving its data.
 *
 * The caller is responsible for freeing the data.
 *
 * \param[in]  bbuf   Bit buffer
 * \param[out] nbytes Data length (in bytes)
 * \return            The buffered data.
 *
 * \see M_free
 */
M_API M_uint8 *M_bit_buf_finish(M_bit_buf_t *bbuf, size_t *nbytes) M_FREE(1) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Free a buffer, saving its data in a per-byte buffer.
 *
 * The caller is responsible for freeing the data.
 *
 * \param[in] bbuf Bit buffer
 *
 * \return The buffered data.
 *
 * \see M_free
 */
M_API M_buf_t *M_bit_buf_finish_buf(M_bit_buf_t *bbuf) M_FREE(1) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return the length of the data held by a buffer, in bits.
 *
 * \param[in] bbuf Bit buffer
 * \return         Data length (in bits)
 */
M_API size_t M_bit_buf_len(M_bit_buf_t *bbuf);


/*! Return the length of the data held by a buffer, in bytes.
 *
 * Partial bytes will be rounded up (i.e., 9 bits stored == 2 bytes).
 *
 * \param[in] bbuf Bit buffer
 * \return         Data length (in bytes)
 */
M_API size_t M_bit_buf_len_bytes(M_bit_buf_t *bbuf);


/*! Return pointer to internal buffer data.
 *
 * The internal data is stored as an array of bytes. The first bit in the buffer is always
 * guaranteed to be the highest bit in the first byte, second bit in the buffer is the next
 * highest bit, and so on. The implementation guarantees this will always be the case,
 * regardless of what operations you may have done on the bit buffer.
 *
 * \warning
 * The returned pointer may be invalidated when you add data to the buffer. For safety,
 * it should be used immediately after you call M_bit_buf_peek(), and then discarded.
 *
 * \param[in] bbuf Bit buffer
 * \return         pointer to current internal buffer data
 */
M_API const M_uint8 *M_bit_buf_peek(M_bit_buf_t *bbuf);


/*! Truncate the length of the data to the specified size (in bits).
 *
 * Removes data from the end of the buffer.
 *
 * \param[in,out] bbuf     Bit buffer
 * \param[in]     len_bits Length (in bits) to truncate buffer to
 */
M_API void M_bit_buf_truncate(M_bit_buf_t *bbuf, size_t len_bits);


/*! Add a number of repetitions of the same bit to the buffer.
 *
 * \param[in,out] bbuf     Bit buffer
 * \param[in]     bit      1 (to add a set bit) or 0 (to add an unset bit)
 * \param[in]     len_bits Number of bits to add
 */
M_API void M_bit_buf_fill(M_bit_buf_t *bbuf, M_uint8 bit, size_t len_bits);


/*! Add the given bit to the buffer.
 *
 * \param[in,out] bbuf Bit buffer
 * \param[in]     bit  1 (to add a set bit) or 0 (to add an unset bit)
 */
M_API void M_bit_buf_add_bit(M_bit_buf_t *bbuf, M_uint8 bit);


/*! Change one of the bits already in the buffer.
 *
 * \param[in] bbuf    Bit buffer
 * \param[in] bit_idx index of bit to change, must be less than M_bit_buf_len()
 * \param[in] bit     1 to set the bit, 0 to unset it
 * \return            M_TRUE on success, M_FALSE if requested bit index too large (not in buffer yet)
 */
M_API M_bool M_bit_buf_update_bit(M_bit_buf_t *bbuf, size_t bit_idx, M_uint8 bit);


/*! Add bits from the given variable-length chunk of data.
 *
 * \param[in,out] bbuf  Bit buffer
 * \param[in]     bytes data to add
 * \param[in]     nbits number of bits to add from the given data
 */
M_API void M_bit_buf_add_bytes(M_bit_buf_t *bbuf, const void *bytes, size_t nbits);


/*! Add bits from a given integer to the buffer.
 *
 * Note that the bit region being read is assumed to be justified against the least-significant end of the
 * integer, though the bits within that region are read from most-significant to least-significant.
 *
 * For example, if bits == 0x8F == (10001011)b, and nbits == 4, the bits "1011" will be added to the buffer.
 *
 * \param[in,out] bbuf  Bit buffer
 * \param[in]     bits  Value to draw bits from
 * \param[in]     nbits Number of bits to use (counted from least-significant end, right-to-left)
 * \param[in]     pad   Should any bits be added to force the result to end on a byte-boundary
 */
M_API void M_bit_buf_add(M_bit_buf_t *bbuf, M_uint64 bits, size_t nbits, M_bit_buf_pad_t pad);


/*! Add bits from a given binary-ascii string to the buffer.
 *
 * A binary-ascii string is a list of 1 and 0 characters (e.g., "100010").
 *
 * Any whitespace in the string will be silently ignored. So, " 1000  1 0" will add the same data as "100010".
 *
 * \param[in,out] bbuf   Bit buffer
 * \param[in]     bitstr String to draw bits from
 * \param[in]     pad    Should any bits be added to force the result to end on a byte-boundary
 *
 * \return               M_FALSE on error (given bitstr had characters other than '0', '1', or whitespace).
 */
M_API M_bool M_bit_buf_add_bitstr(M_bit_buf_t *bbuf, const char *bitstr, M_bit_buf_pad_t pad);


/*! Provide hint to buffer about how many bits we're going to add.
 *
 * If you know ahead of time how many bits are going to get added to the buffer, you can
 * use this function to grow the buffer all at once ahead of time, instead of on-demand
 * as the buffer runs out of internal space.
 *
 * This is provided purely as a performance hint.
 *
 * \param  bbuf  Bit buffer
 * \param  nbits Number of bits that we expect to add in the future
 */
M_API void M_bit_buf_reserve(M_bit_buf_t *bbuf, size_t nbits);

/*! @} */

__END_DECLS

#endif /* __M_BIT_BUF_H__ */
