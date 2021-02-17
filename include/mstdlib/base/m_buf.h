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

#ifndef __M_BUF_H__
#define __M_BUF_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_str.h>
#include <mstdlib/base/m_decimal.h>
#include <mstdlib/base/m_endian.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_buf Buffered Data Builder
 *  \ingroup mstdlib_base
 *
 * Allows for buffered writing of string and binary data.
 * It is a safe and efficient way to append and manipulate buffered data.
 *
 * Handles resizing of the buffer, location tracking, and has various
 * helpers to modify the data being written into the buffer.
 *
 * When done adding data the contents of the buffer can be output
 * as a continuous array. Such as unsigned char * or char *.
 *
 * Example (creating a buffer, adding data, finishing the buffer):
 *
 * \code{.c}
 *     M_buf_t *buf;
 *     char    *out;
 *
 *     buf = M_buf_create();
 *     M_buf_add_byte(buf, '^');
 *     M_buf_add_str(buf, "ABC");
 *     M_buf_add_int(buf, 123);
 *     out = M_buf_finish_str(buf, NULL);
 *
 *     M_printf("out='%s'\n", out);
 *     M_free(out);
 * \endcode
 *
 * Example output:
 *
 * \code
 *     out='^ABC123'
 * \endcode
 *
 * @{
 */

struct M_buf;
typedef struct M_buf M_buf_t;

/*! Enumeration for transformation types, bitmapped type to allow multiple transformations to be run */
enum M_BUF_TRANSFORM_TYPE {
	M_BUF_TRANSFORM_NONE  = 0,      /*!< Perform no transformation */
	M_BUF_TRANSFORM_UPPER = 1 << 0, /*!< Transform into upper-case (cannot be used with M_BUF_TRANSFORM_LOWER) */
	M_BUF_TRANSFORM_LOWER = 1 << 1, /*!< Transform into lower-case (cannot be used with M_BUF_TRANSFORM_UPPER) */
	M_BUF_TRANSFORM_LTRIM = 1 << 2, /*!< Trim whitespace from left of the data                                 */
	M_BUF_TRANSFORM_RTRIM = 1 << 3, /*!< Trim whitespace from right of the data                                */
	M_BUF_TRANSFORM_TRIM  = M_BUF_TRANSFORM_LTRIM|M_BUF_TRANSFORM_RTRIM /*!< Trim whitespace from left and right of data */
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new buffer.
 *
 * \return allocated buffer.
 *
 * \see M_buf_cancel
 * \see M_buf_finish
 * \see M_buf_finish_str
 */
M_API M_buf_t *M_buf_create(void) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Free a buffer, discarding its data.
 *
 * \param[in] buf Buffer.
 */
M_API void M_buf_cancel(M_buf_t *buf) M_FREE(1);


/*! Free a buffer, saving its data.
 *
 * The caller is responsible for freeing the data.
 *
 * \param[in]  buf        Buffer
 * \param[out] out_length Data length
 *
 * \return The buffered data.
 *
 * \see M_free
 */
M_API unsigned char *M_buf_finish(M_buf_t *buf, size_t *out_length) M_FREE(1) M_WARN_UNUSED_RESULT M_MALLOC M_WARN_NONNULL(2);


/*! Free a buffer, saving its data as a C-string.
 *
 * The caller is responsible for freeing the data.
 *
 * \param[in]  buf        Buffer.
 * \param[out] out_length Data length. Optional, pass NULL if length not needed.
 *
 * \return The buffered data.
 *
 * \see M_free
 */
M_API char *M_buf_finish_str(M_buf_t *buf, size_t *out_length) M_FREE(1) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Return the length of the data held by a buffer.
 *
 * \param[in] buf Buffer.
 *
 * \return Data length.
 */
M_API size_t M_buf_len(const M_buf_t *buf);


/*! Return overall data allocation size for the buffer.
 *
 * \param[in] buf Buffer.
 *
 * \return Allocation size.
 */
M_API size_t M_buf_alloc_size(const M_buf_t *buf);


/*! Take a sneak peek at the buffer.
 *
 * \param[in] buf Buffer.
 *
 * \return Current beginning of the data in the buffer.
 */
M_API const char *M_buf_peek(const M_buf_t *buf);


/*! Truncate the length of the data to the specified size.
 *
 * Removes data from the end of the buffer.
 *
 * \param[in,out] buf    Buffer.
 * \param[in]     length Length to truncate buffer to.
 */
M_API void M_buf_truncate(M_buf_t *buf, size_t length);


/*! Drop the specified number of bytes from the beginning of the buffer.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     num Number of bytes to drop.
 */
M_API void M_buf_drop(M_buf_t *buf, size_t num);


/*! Begin a direct write operation.  In general, this function should not be used,
 *  it is meant as an optimization to prevent double buffering when reading I/O.
 *  A writable buffer will be returned of at least the length requested, often it
 *  will be much larger.  To end the direct write process,  M_buf_direct_write_end()
 *  must be called with the length actually written.  It is not valid to call any
 *  other M_buf_*() functions between start and end.
 *
 * \param[in,out] buf Buffer
 * \param[in,out] len Pass in the minimum requested buffer size, outputs the maximum
 *                    writable buffer size.
 * \return Writable buffer or NULL on failure */
M_API unsigned char *M_buf_direct_write_start(M_buf_t *buf, size_t *len);


/*! End a direct write operation.  Please see M_buf_direct_write_start() for more
 *  information.  This terminates a direct write sequence regardless of if data was
 *  written or not (len = 0 is acceptable).
 *
 * \param[in,out] buf Buffer
 * \param[in]     len Length of data written.
 */
M_API void M_buf_direct_write_end(M_buf_t *buf, size_t len);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Merge two buffers.
 *
 * The data in the source buffer is appended to the destination buffer.
 * The source buffer is freed.
 *
 * \param[in,out] dest   Buffer.
 * \param[in]     source Buffer.
 */
M_API void M_buf_merge(M_buf_t *dest, M_buf_t *source) M_FREE(2);


/*! Join an array of buffers.
 *
 * The data in the buffer array is appended to the destination buffer with sep placed between the data in each buffer.
 * The buffers in the buffer array is freed. The array itself is not freed.
 *
 * \param[in,out] dest Buffer.
 * \param[in]     sep  String to insert between element in the buffer array.
 * \param[in]     bufs Array of buffers.
 * \param[in]     cnt  Number of elements in the buffer array.
 */
M_API void M_buf_bjoin_buf(M_buf_t *dest, unsigned char sep, M_buf_t **bufs, size_t cnt);


/*! Join an array of strings.
 *
 * The data in the string array is appended to the destination buffer with sep placed between the data in each buffer.
 *
 * \param[in,out] dest Buffer.
 * \param[in]     sep  String to insert between element in the string array.
 * \param[in]     strs Array of strings.
 * \param[in]     cnt  Number of elements in the buffer array.
 */
M_API void M_buf_bjoin_str(M_buf_t *dest, unsigned char sep, const char **strs, size_t cnt);


/*! Join an array of buffers.
 *
 * The data in the buffer array is appended to the destination buffer with sep placed between the data in each buffer.
 * The buffers in the buffer array is freed. The array itself is not freed.
 *
 * \param[in,out] dest Buffer.
 * \param[in]     sep  String to insert between element in the buffer array.
 * \param[in]     bufs Array of buffers.
 * \param[in]     cnt  Number of elements in the buffer array.
 */
M_API void M_buf_sjoin_buf(M_buf_t *dest, const char *sep, M_buf_t **bufs, size_t cnt);


/*! Join an array of strings.
 *
 * The data in the string array is appended to the destination buffer with sep placed between the data in each buffer.
 *
 * \param[in,out] dest Buffer.
 * \param[in]     sep  String to insert between element in the string array.
 * \param[in]     strs Array of strings.
 * \param[in]     cnt  Number of elements in the buffer array.
 */
M_API void M_buf_sjoin_str(M_buf_t *dest, const char *sep, const char **strs, size_t cnt);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Append one byte to a buffer.
 *
 * \param[in,out] buf  Buffer.
 * \param[in]     byte Byte to append.
 */
M_API void M_buf_add_byte(M_buf_t *buf, unsigned char byte);


/*! Append zero or more bytes to a buffer.
 *
 * \param[in,out] buf          Buffer.
 * \param[in]     bytes        Bytes to append.
 * \param[in]     bytes_length Number of bytes to append.
 */
M_API void M_buf_add_bytes(M_buf_t *buf, const void *bytes, size_t bytes_length);


/*! Append zero or more bytes to a buffer (given as hex string).
 *
 * \warning
 * This function is deprecated, M_buf_add_decode() or M_buf_decode() should be used instead.
 *
 * Same as M_buf_add_bytes(), but accepts binary data encoded as a hex string.
 * The data is decoded into raw binary form before it's added to the buffer.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     hex_bytes Hex string that encodes the bytes to append.
 * \return        M_TRUE if successful, M_FALSE if error during hex decode
 */
M_API M_bool M_buf_add_bytes_hex(M_buf_t *buf, const char *hex_bytes);


/*! Append one char to a buffer.
 *
 * \param[in,out] buf  Buffer.
 * \param[in]     c    Char to append.
 */
M_API void M_buf_add_char(M_buf_t *buf, char c);


/*! Append a C string (zero or more bytes terminated with a NULL) to a buffer.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     str String to append.
 */
M_API void M_buf_add_str(M_buf_t *buf, const char *str);


/*! Append a C string up to the NUL terminator or max bytes (which ever is smaller) to a buffer.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     str String to append.
 * \param[in]     max Maximum number of bytes to add.
 */
M_API void M_buf_add_str_max(M_buf_t *buf, const char *str, size_t max);


/*! Append the given bytes to the buffer as a hex-encoded string.
 *
 * \warning
 * This function is deprecated, M_buf_add_encode() or M_buf_encode() should be used instead.
 *
 * The given binary data is converted to a hex-encoded string before being
 * added to the buffer.
 *
 * \param[in,out] buf   Buffer.
 * \param[in]     bytes Bytes to append as hex.
 * \param[in]     len   Number of bytes to use as input.
 */
M_API void M_buf_add_str_hex(M_buf_t *buf, const void *bytes, size_t len);


/*! Split string into lines while keeping words intact, then append to buffer.
 *
 * Words in this context are defined as contiguous blocks of non-whitespace characters. For each line,
 * leading and trailing whitespace will be trimmed, but internal whitespace will be left alone.
 *
 * The given newline sequence is added at the end of each line.
 *
 * An example use case is breaking up strings for display on small LCD screens.
 *
 * \see M_str_explode_lines
 *
 * \param[in,out] buf        Buffer to add output to.
 * \param[in]     str        Source string.
 * \param[in]     max_lines  Maximum number of lines to output.
 * \param[in]     max_chars  Maximum characters per line.
 * \param[in]     truncate   If true, truncation is allowed. If false, NULL will be returned if the string won't fit.
 * \param[in]     newline    Newline sequence to add to end of each line.
 * \return                   number of lines added to buffer (zero if the input string was empty or there's an error).
 */
M_API size_t M_buf_add_str_lines(M_buf_t *buf, const char *str, size_t max_lines, size_t max_chars, M_bool truncate,
	const char *newline);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer, transform
 *  the data as specified.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf            Buffer.
 * \param[in]     transform_type Type of transformation to perform, bitmap field of enum M_BUF_TRANSFORM_TYPE
 * \param[in]     str            String to append.
 */
M_API void M_buf_add_str_transform(M_buf_t *buf, M_uint32 transform_type, const char *str);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer up to
 *  max size, transform the data as specified.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf            Buffer.
 * \param[in]     transform_type Type of transformation to perform, bitmap field of enum M_BUF_TRANSFORM_TYPE
 * \param[in]     str            String to append.
 * \param[in]     max            Max length to append.
 */
M_API void M_buf_add_str_max_transform(M_buf_t *buf, M_uint32 transform_type, const char *str, size_t max);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer, ensuring all characters of the string
 * are in uppercase.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     str String to append.
 */
M_API void M_buf_add_str_upper(M_buf_t *buf, const char *str);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer, ensuring all characters of the string
 * are in lowercase.
 *
 * The NUL is not appended.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     str String to append.
 */
M_API void M_buf_add_str_lower(M_buf_t *buf, const char *str);


/*! Append a fill character to a buffer zero or more times.
 *
 * \param[in,out] buf       Buffer
 * \param[in]     fill_char Character/byte to append.
 * \param[in]     width     Number of times to add character/byte.
 */
M_API void M_buf_add_fill(M_buf_t *buf, unsigned char fill_char, size_t width);


/*! Append the character decimal representation ("%llu") of an unsigned integer
 * to a buffer.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     n   Unsigned integer to append.
 */
M_API void M_buf_add_uint(M_buf_t *buf, M_uint64 n);


/*! Append the character decimal representation ("%lld") of a signed integer
 * to a buffer.
 *
 * \param[in,out] buf Buffer.
 * \param[in]     n   Unsigned integer to append.
 */
M_API void M_buf_add_int(M_buf_t *buf, M_int64 n);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Encode binary data using the given codec, then append the result to this buffer.
 *
 * The results are only added to the end of the given buffer on success - if there's an error, the buffer's existing
 * contents are not modified.
 *
 * Passing input data with a length of zero will always succeed (contents of the buffer won't be modified).
 *
 * \see M_buf_encode
 * \see M_buf_add_decode
 * \see M_bincodec_encode
 *
 * \param[in,out] buf       buffer to append encoded data to.
 * \param[in]     bytes     binary data we want to encode into a string.
 * \param[in]     bytes_len length (in bytes) of binary data we want to encode.
 * \param[in]     wrap      max length of a given line. Longer lines will be split with a newline char.
 *                          Pass 0 if line splitting is not desired.
 * \param[in]     codec     binary codec to encode the binary data with.
 * \return                  M_TRUE on success, M_FALSE if there was some error during encoding.
 */
M_API M_bool M_buf_add_encode(M_buf_t *buf, const void *bytes, size_t bytes_len, size_t wrap, M_bincodec_codec_t codec);


/*! Encode contents of buffer in-place using the given codec.
 *
 * If successful, the entire contents of the buffer will be replaced with their encoded version.
 * If there's an error, the buffer's contents are not modified.
 *
 * Calling this function on an empty buffer will always succeed.
 *
 * \see M_buf_add_encode
 * \see M_buf_decode
 * \see M_bincodec_encode
 *
 * \param[in,out] buf   buffer whose contents we want to encode
 * \param[in]     wrap  max length of a given line. Longer lines will be split with a newline char.
 *                      Pass 0 if line splitting is not desired.
 * \param[in]     codec binary codec to use on the contents of the buffer.
 * \return              M_TRUE on success, M_FALSE if there was some error during encoding.
 */
M_API M_bool M_buf_encode(M_buf_t *buf, size_t wrap, M_bincodec_codec_t codec);


/*! Decode string to raw binary using the given codec, then append the result to this buffer.
 *
 * The results are only added to the end of the given buffer on success - if there's an error, the buffer's existing
 * contents are not modified.
 *
 * Passing an empty input string will always succeed (contents of the buffer won't be modified).
 *
 * \see M_buf_decode
 * \see M_buf_add_encode
 * \see M_bincodec_decode
 *
 * \param[in,out] buf         buffer to append decoded data to.
 * \param[in]     encoded     string we want to decode into raw binary data.
 * \param[in]     encoded_len number of chars from string that we want to decode.
 * \param[in]     codec       binary codec to decode the string with.
 * \return                    M_TRUE on success, M_FALSE if there was some error during decoding.
 */
M_API M_bool M_buf_add_decode(M_buf_t *buf, const char *encoded, size_t encoded_len, M_bincodec_codec_t codec);


/*! Decode contents of buffer in-place using the given codec.
 *
 * If successful, the entire contents of the buffer will be replaced with their decoded version.
 * If there's an error, the buffer's contents are not modified.
 *
 * Calling this function on an empty buffer will always succeed.
 *
 * \see M_buf_add_decode
 * \see M_buf_encode
 * \see M_bincodec_decode
 *
 * \param[in,out] buf   buffer whose contents we want to decode
 * \param[in]     codec binary codec to use when decoding the contents of the buffer.
 * \return              M_TRUE on success, M_FALSE if there was some error during decoding.
 */
M_API M_bool M_buf_decode(M_buf_t *buf, M_bincodec_codec_t codec);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Append zero or more bytes to a buffer, with justification.
 *
 * \param[in,out] buf          Buffer.
 * \param[in]     bytes        Bytes to append.
 * \param[in]     bytes_length Number of bytes to append.
 * \param[in]     justify_type Type of justification (left, right, etc.).
 * \param[in]     fill_char    Character to use for padding.
 * \param[in]     width        Width of field, including padding.
 */
M_API void M_buf_add_bytes_just(M_buf_t *buf, const void *bytes, size_t bytes_length, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer,
 *  with justification and transformation.
 *
 *  The NUL is not appended.
 *
 * \param[in,out] buf            Buffer.
 * \param[in]     transform_type bitmap of transformations (enum M_BUF_TRANSFORM_TYPE) to perform.
 * \param[in]     str            String to append.
 * \param[in]     justify_type   Type of justification (left, right, etc.).
 * \param[in]     fill_char      Character to use for padding.
 * \param[in]     width          Width of field, including padding.
 */
M_API void M_buf_add_str_just_transform(M_buf_t *buf, M_uint32 transform_type, const char *str, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width);


/*! Append a C string (zero or more bytes terminated with a NUL) to a buffer,
 *  with justification.
 *
 *  The NUL is not appended.
 *
 * \param[in,out] buf          Buffer.
 * \param[in]     str          String to append.
 * \param[in]     justify_type Type of justification (left, right, etc.).
 * \param[in]     fill_char    Character to use for padding.
 * \param[in]     width        Width of field, including padding.
 */
M_API void M_buf_add_str_just(M_buf_t *buf, const char *str, M_str_justify_type_t justify_type, unsigned char fill_char, size_t width);


/*! Append the character decimal representation ("%llu") of an unsigned integer
 *  to a buffer, with right justification, zero padded.
 *
 *  Bytes on the left will be truncated from the integer if there is insufficient width.
 *
 * \param[in,out] buf   Buffer.
 * \param[in]     n     Unsigned integer to append.
 * \param[in]     width Width of field, including padding.
 *
 * \return M_FALSE if input was truncated, otherwise M_TRUE.
 */
M_API M_bool M_buf_add_uint_just(M_buf_t *buf, M_uint64 n, size_t width);


/*! Append the character decimal representation ("%lld") of a signed integer
 *  to a buffer, with right justification, zero padded.
 *
 *  Bytes on the left will be truncated from the integer if there is insufficient width.
 *
 * \param[in,out] buf   Buffer.
 * \param[in]     n     Unsigned integer to append.
 * \param[in]     width Width of field, including padding.
 *
 * \return M_FALSE if input was truncated, otherwise M_TRUE.
 */
M_API M_bool M_buf_add_int_just(M_buf_t *buf, M_int64 n, size_t width);


/*! Append an integer converted to binary form based on endianness.
 *
 * \param[in,out] buf        Buffer.
 * \param[in]     n          Unsigned integer to append.
 * \param[in]     width      Exact field length, must be [1:8]
 * \param[in]     endianness Endianness the integer should be written using.
 *
 * return M_TRUE if integer could be written. Otherwise M_FALSE.
 */
M_API M_bool M_buf_add_uintbin(M_buf_t *buf, M_uint64 n, size_t width, M_endian_t endianness);


/*! Append an integer in string form to binary data based on endianness.
 *
 * The string representing a big endian number. Hex especially must be ordered as big endian.
 *
 * \param[in,out] buf        Buffer.
 * \param[in]     s          Numeric string form.
 * \param[in]     base       Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[in]     width      Width of the field [1:8].
 * \param[in]     endianness Endianness the integer should be written using.
 *
 * return M_TRUE if integer could be written for number of bytes requested. Otherwise M_FALSE.
 */
M_API M_bool M_buf_add_uintstrbin(M_buf_t *buf, const char *s, unsigned char base, size_t width, M_endian_t endianness);


/*! Append an integer converted to Binary Coded Decimal.
 *
 * Packed BCD with 4 bit numbers representing a single number. Two numbers packed into one byte.
 *
 * dec  | just | bcd                           | hex
 * -----|------|------------------------------:|----
 * 1    | 2    |                     0000 0001 | 0x01
 * 2    | 3    | 0000 0000 0000 0000 0000 0010 | 0x000002
 * 100  | 3    | 0000 0000 0000 0001 0000 0000 | 0x000100
 *
 * \param[in,out] buf   Buffer.
 * \param[in]     n     Unsigned integer to append.
 * \param[in]     width Width of field, including padding. This is the total number of bytes that should
 *                      be written. A width of 3 means 3 bytes not 3 BCD segments.
 *
 * \return M_FALSE if input would be truncated (length greater than width), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_uintbcd(M_buf_t *buf, M_uint64 n, size_t width);


/*! Append an integer in string form to Binary Coded Decimal.
 *
 * Packed BCD with 4 bit numbers representing a single number. Two numbers packed into one byte.
 *
 * \param[in,out] buf        Buffer.
 * \param[in]     s          Numeric string form.
 * \param[in]     base       Valid range 2 - 36. 0 to autodetect based on input (0x = hex, 0 = octal, anything else is decimal).
 * \param[in]     width      Width of the field [1:8].
 *
 * return M_TRUE if integer could be written for number of bytes requested. Otherwise M_FALSE.
 */
M_API M_bool M_buf_add_uintstrbcd(M_buf_t *buf, const char *s, unsigned char base, size_t width);


/*! Append an integer converted to Hex-ASCII.
 *
 * \param[in,out] buf      Buffer.
 * \param[in]     n        Unsigned integer to append.
 * \param[in]     is_upper Should data be added uppercase.
 * \param[in]     width    Number of hex bytes to write to buffer, including padding.
 *
 * return M_TRUE if integer could be written for number of bytes requested. Otherwise M_FALSE.
 */
M_API M_bool M_buf_add_uinthex(M_buf_t *buf, M_uint64 n, M_bool is_upper, size_t width);



/*! Append a byte converted to Hex-ASCII.
 *
 * \param[in,out] buf      Buffer.
 * \param[in]     byte     Byte to append.
 * \param[in]     is_upper Should data be added uppercase.
 *
 * return M_TRUE if byte could be written. Otherwise M_FALSE.
 */
M_API void M_buf_add_bytehex(M_buf_t *buf, unsigned char byte, M_bool is_upper);


/*! Append a pointer
 *
 * \param[in,out] buf Buffer.
 * \param[in]     ptr Pointer (address) to append.
 */
M_API void M_buf_add_ptr(M_buf_t *buf, void *ptr);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Append a monetary amount to a buffer.
 *
 * Input is a dollar amount with a 2 digit decimal value using '.' to separate dollar
 * and cents. A '.' is used for the decimal portion. The input is _not_ implied decimal.
 * Only the first two decimal digits are evaluated. Everything after is truncated.
 * The amount will be added as implied decimal. Negative symbol will be added if value is negative.
 *
 * E.g.
 *
 *     in  -> "12.00"
 *     out -> 1200
 *     in  -> "12.1001"
 *     out -> 1210
 *     in  -> "-12.0"
 *     out -> -1200
 *     in  -> "-12."
 *     out -> -1200
 *     in  -> "12"
 *     out -> 1200
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append. Not implied decimal.
 * \param[in]     max_width Maximum width of field. Number of digits output.
 *
 * \return M_FALSE on error (probably truncation), M_TRUE otherwise.
 */
M_API M_bool M_buf_add_money(M_buf_t *buf, const char *amount, size_t max_width) M_WARN_UNUSED_RESULT;


/*! Append a monetary amount to a buffer, adding a decimal point.
 *
 * Input is a dollar amount with a 2 digit decimal value using '.' to separate dollar
 * and cents. A '.' is used for the decimal portion. The input is _not_ implied decimal.
 * Only the first two decimal digits are evaluated. Everything after is truncated.
 * The amount will be added with a decimal. Negative symbol will be added if value is negative.
 *
 * This function is used to ensure a properly formatted monetary value.
 *
 * E.g.
 *
 *     in  -> "12.00"
 *     out -> 12.00
 *     in  -> "12.1001"
 *     out -> 12.10
 *     in  -> "-12.0"
 *     out -> -12.00
 *     in  -> "-12."
 *     out -> -12.00
 *     in  -> "12"
 *     out -> 12.00
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), Otherewise M_TRUE.
 */
M_API M_bool M_buf_add_money_dot(M_buf_t *buf, const char *amount, size_t max_width) M_WARN_UNUSED_RESULT;


/*! Append a monetary amount to a buffer, with right justification, zero padded.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 *
 * \see M_buf_add_money
 */
M_API M_bool M_buf_add_money_just(M_buf_t *buf, const char *amount, size_t max_width) M_WARN_UNUSED_RESULT;


/*! Append a monetary amount to a buffer, adding a decimal point, with right
 * justification, zero padded.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 *
 * \see M_buf_add_money_dot
 */
M_API M_bool M_buf_add_money_dot_just(M_buf_t *buf, const char *amount, size_t max_width) M_WARN_UNUSED_RESULT;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Append a monetary amount to a buffer.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_int_money(M_buf_t *buf, M_int64 amount, size_t max_width) M_WARN_UNUSED_RESULT;
#define M_buf_add_uint_money(buf, amount, max_width) M_buf_add_int_money(buf, M_ABS(amount), max_width)


/*! Append a monetary amount to a buffer, adding a decimal point.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_int_money_dot(M_buf_t *buf, M_int64 amount, size_t max_width) M_WARN_UNUSED_RESULT;
#define M_buf_add_uint_money_dot(buf, amount, max_width) M_buf_add_int_money_dot(buf, M_ABS(amount), max_width)


/*! Append a monetary amount to a buffer, with right justification, zero padded.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_int_money_just(M_buf_t *buf, M_int64 amount, size_t max_width) M_WARN_UNUSED_RESULT;
#  define M_buf_add_uint_money_just(buf, amount, max_width) M_buf_add_int_money_just(buf, M_ABS(amount), max_width)


/*! Append a monetary amount to a buffer, adding a decimal point, with right
 * justification, zero padded.
 *
 * \param[in,out] buf       Buffer.
 * \param[in]     amount    Monetary amount to append.
 * \param[in]     max_width Maximum width of field.
 *
 * \return M_FALSE on error (probably truncation), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_int_money_dot_just(M_buf_t *buf, M_int64 amount, size_t max_width) M_WARN_UNUSED_RESULT;
#define M_buf_add_uint_money_dot_just(buf, amount, max_width) M_buf_add_int_money_dot_just(buf, M_ABS(amount), max_width)


/*! Appends a decimal number to a buffer.
 *
 * The number of decimal places may be specified and whether or not the number should have an 'implied' decimal
 * but not actually output the decimal character.
 *
 * \param[in,out] buf             Buffer.
 * \param[in]     decimal         Decimal number to represent.
 * \param[in]     implied_decimal The decimal place is implied (e.g. not actually present in the output).
 * \param[in]     num_decimals    Number of digits after the decimal that should be printed.
 *                                Pass as -1 if it should output whatever is currently
 *                                in the decimal.  Required to be something other than
 *                                -1 if using implied decimal.
 * \param[in]     max_width       Maximum width of the output, if this is exceeded it is an
 *                                error condition.  A value of 0 means there is no maximum.
 *
 * \return M_FALSE on error (e.g. truncation or misuse), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_decimal(M_buf_t *buf, const M_decimal_t *decimal, M_bool implied_decimal, M_int8 num_decimals, size_t max_width) M_WARN_UNUSED_RESULT;


/*! Appends a decimal number to a buffer justifying it on the left with zeros.
 *
 * The number of decimal places may be specified and whether or not the number should have an 'implied' decimal
 * but not actually output the decimal character.
 *
 * \param[in,out] buf             Buffer.
 * \param[in]     decimal         Decimal number to represent.
 * \param[in]     implied_decimal The decimal place is implied (e.g. not actually present in the output).
 * \param[in]     num_decimals    Number of digits after the decimal that should be printed.
 *                                Pass as -1 if it should output whatever is currently
 *                                in the decimal.  Required to be something other than
 *                                -1 if using implied decimal.
 * \param[in]     max_width       Justification width of the output.
 *
 * \return M_FALSE on error (e.g. truncation or misuse), otherwise M_TRUE.
 */
M_API M_bool M_buf_add_decimal_just(M_buf_t *buf, const M_decimal_t *decimal, M_bool implied_decimal, M_int8 num_decimals, size_t max_width) M_WARN_UNUSED_RESULT;


/*! Add given bytes to destination buffer, replace all instances of a byte sequence during the add.
 *
 * The source pointer must not point to the destination buffer's memory (no aliasing allowed).
 *
 * \param[out] dest_buf      buffer to write data to
 * \param[in]  src           bytes to add
 * \param[in]  src_len       number of bytes to add
 * \param[in]  search_bytes  sequence of bytes to look for
 * \param[in]  search_len    length of search sequence
 * \param[in]  replace_bytes sequence of bytes to replace \a search_bytes with
 * \param[in]  replace_len   length of replace sequence
 * \return                   M_TRUE on success, M_FALSE on error
 */
M_API M_bool M_buf_add_bytes_replace(M_buf_t *dest_buf, const M_uint8 *src, size_t src_len,
	const M_uint8 *search_bytes, size_t search_len, const M_uint8 *replace_bytes, size_t replace_len);


/*! Add given string to destination buffer, replace all instances of a string during the add.
 *
 *  The source pointer must not point to the destination buffer's memory (no aliasing allowed).
 *
 * \param[out] dest_buf    buffer to write data to
 * \param[in]  src_str     string to add
 * \param[in]  search_str  string we're looking for
 * \param[in]  replace_str string we're going to replace \a search_str with
 * \return                 M_TRUE on success, M_FALSE on error
 */
M_API M_bool M_buf_add_str_replace(M_buf_t *dest_buf, const char *src_str,
	const char *search_str, const char *replace_str);


/*! Add the given string to the buffer, quoting if necessary.
 *
 *  This is useful for outputting delimited data like CSV.
 *
 *  If the input string is NULL, it will not output anything even if always_quoted
 *  is specified.  However, an empty string will always be output as quoted as
 *  that is what differentiates between an empty string an NULL.
 *
 *  \param[out] buf            Buffer to write quoted string to.
 *  \param[in]  quote_char     Quote character to use (often a double quote: ")
 *  \param[in]  escape_char    Escape character to use if either an embedded quote
 *                             is found or another escape character.  For CSV this
 *                             is often the same value as the quote character as
 *                             per RFC4180.
 *  \param[in] quote_req_chars NULL-terminated list of characters that would force
 *                             quoting the string.  Often ",\\r\\n" are used.
 *  \param[in] always_quote    If set to M_TRUE, will always quote the output, M_FALSE
 *                             it will decide based on quote_req_chars.
 *  \param[in] src             Data to be quoted/escaped and appended to the buffer
 */
M_API void M_buf_add_str_quoted(M_buf_t *buf, char quote_char, char escape_char, const char *quote_req_chars, M_bool always_quote, const char *src);


/*! Trim whitespace from beginning and end of buffer, in-place.
 *
 * \param[in,out] buf buffer to trim
 */
M_API void M_buf_trim(M_buf_t *buf);

/*! @} */

__END_DECLS

#endif /* __M_BUF_H__ */
