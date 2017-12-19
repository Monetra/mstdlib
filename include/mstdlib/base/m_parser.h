/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_PARSER_H__
#define __M_PARSER_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_decimal.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_parser Data Parser
 *  \ingroup mstdlib_base
 * 
 * Buffer based data parser.
 *
 * Efficient parser that prevents reading past the end of the data buffer.
 * Has helpers for reading specific types from the buffer (auto conversion).
 * Also supports line and column tracking.
 *
 * @{
 */

struct M_parser;
typedef struct M_parser M_parser_t;


typedef M_bool (*M_parser_predicate_func)(unsigned char c);


/*! Flags controlling behavior of the parser. */
enum M_PARSER_FLAGS {
	M_PARSER_FLAG_NONE       = 0,      /*!< No Flags. */
	M_PARSER_FLAG_TRACKLINES = 1 << 0  /*!< Track lines and columns. This should
	                                        only be enabled if needed as it will
	                                        cause an additional data scan. */
};

/*! Flags controlling what constitutes whitespace. */
enum M_PARSER_WHITESPACE_FLAGS {
	M_PARSER_WHITESPACE_NONE       = 0,      /*!< Consumes all whitespace */
	M_PARSER_WHITESPACE_TO_NEWLINE = 1 << 0, /*!< Only consume whitespace up to and including the next new line. */
	M_PARSER_WHITESPACE_SPACEONLY  = 1 << 1  /*!< Only consume space 0x20 characters. */
};

/*! Integer binary format. */
enum M_PARSER_INTEGER_TYPE {
	M_PARSER_INTEGER_ASCII        = 0, /*!< Integer represented in ASCII form. */
	M_PARSER_INTEGER_BIGENDIAN    = 1, /*!< Integer represented in Big Endian form. */
	M_PARSER_INTEGER_LITTLEENDIAN = 2  /*!< Integer represented in Little Endian form. */
};

/*! Splitting flags. */
typedef enum {
	M_PARSER_SPLIT_FLAG_NONE          = 0,      /*!< No flags, standard behavior */
	M_PARSER_SPLIT_FLAG_NODELIM_ERROR = 1 << 0  /*!< Return an error if the specified delimiter is not found, otherwise all the data is put in a single parser object */
} M_PARSER_SPLIT_FLAGS;

/*! Framing characters. */
typedef enum {
	M_PARSER_FRAME_NONE = 0,      /*!< No framing characters. */
	M_PARSER_FRAME_STX  = 1 << 0, /*!< STX (0x02) */
	M_PARSER_FRAME_ETX  = 1 << 1  /*!< ETX (0x03) */
} M_PARSER_FRAME_BYES;

/*! STX, ETX, LRC unwrapping responses. */
typedef enum {
	M_PARSER_FRAME_ERROR_SUCCESS = 0,    /*!< Success. */
	M_PARSER_FRAME_ERROR_INVALID,        /*!< Invalid input. */
	M_PARSER_FRAME_ERROR_NO_STX,         /*!< Data does not start with STX. */
	M_PARSER_FRAME_ERROR_NO_ETX,         /*!< ETX not found. */
	M_PARSER_FRAME_ERROR_NO_LRC,         /*!< Not enough data for LRC. */
	M_PARSER_FRAME_ERROR_LRC_CALC_FAILED /*!< LRC calculation failed. */
} M_PARSER_FRAME_ERROR;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initialize a parser object using const data.
 *
 * The object is initialized with constant data which cannot be appended to. Memory is not duplicated and therefore the
 * memory for the buffer pointed to must exist for the life of the parser. The parser will not clean up the memory for
 * the referenced object.
 *
 * \param[in] buf   Data to parse, must not be NULL.
 * \param[in] len   Length of data to be parsed.
 * \param[in] flags Any of the enum M_PARSER_FLAGS bitwise OR'd together.
 *
 * \return Parser object, or NULL on failure.
 *
 * \see M_parser_destroy
 */
M_API M_parser_t *M_parser_create_const(const unsigned char *buf, size_t len, M_uint32 flags);


/*! Initialize an empty parser object.
 *
 * Its initial state is empty and data must be appended to it before any data can be parsed.
 *
 * IMPLEMENTATION NOTE:  For efficiency, data which is parsed will be purged
 *                       from memory when additional internal buffer space
 *                       is required during an append operation. This is
 *                       to reclaim space and reduce the number of allocations
 *                       required when parsing stream-based data.
 *
 * \return Parser object, or NULL on failure.
 *
 * \see M_parser_destroy
 */
M_API M_parser_t *M_parser_create(M_uint32 flags);


/*! Destroy the parser object.
 *
 * \param[in] parser Parser object.
 */
M_API void M_parser_destroy(M_parser_t *parser);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Append data to a parser object.
 *
 * The parser object must have been initialized with M_parser_create(). This will append the data to the internal
 * buffer extending the available length of data to parse.
 *
 * \param[in,out] parser Parser object, but not be a const object.
 * \param[in]     data   Data to append.
 * \param[in]     len    Length of data to append.
 *
 * \return M_TRUE on success, M_FALSE on misuse.
 */
M_API M_bool M_parser_append(M_parser_t *parser, const unsigned char *data, size_t len);


/*! Begin a direct write operation.  In general, this function should not be used,
 *  it is meant as an optimization to prevent double buffering when reading I/O.
 *  A writable buffer will be returned of at least the length requested, often it
 *  will be much larger.  To end the direct write process,  M_parser_direct_write_end()
 *  must be called with the length actually written.  It is not valid to call any
 *  other M_parser_*() functions between start and end.
 *
 * \param[in,out] parser Parser object, but not be a const object
 * \param[in,out] len    Pass in the minimum requested buffer size, outputs the maximum
 *                       writable buffer size.
 * \return Writable buffer or NULL on failure */
M_API unsigned char *M_parser_direct_write_start(M_parser_t *parser, size_t *len);


/*! End a direct write operation.  Please see M_parser_direct_write_start() for more
 *  information.  This terminates a direct write sequence regardless of if data was
 *  written or not (len = 0 is acceptable).
 *
 * \param[in,out] parser Parser object, but not a const object
 * \param[in]     len Length of data written.
 */
M_API void M_parser_direct_write_end(M_parser_t *parser, size_t len);


/*! Retrieve the length of data remaining in the buffer being parsed.
 *
 * \param[in] parser Parser object.
 *
 * \return Length of remaining data.
 */
M_API size_t M_parser_len(M_parser_t *parser);


/*! Retrieve the total number of bytes processed so far.
 *
 * \param parser Parser object.
 *
 * \return Total number of processed bytes.
 */
M_API size_t M_parser_current_offset(M_parser_t *parser);


/*! Retrieves the current line number.
 *
 * Line numbers are determined based on how many @\n's have been evaluated in the data stream. This can only be called if
 * M_PARSER_FLAG_TRACKLINES was used during initialization of the parser.
 *
 * \param[in] parser parser object.
 *
 * \return Line number starting at 1.
 */
M_API size_t M_parser_current_line(M_parser_t *parser);


/*! Retrieves the current column for the current line.
 *
 * The column count resets each time a @\n is passed. This can only be called if M_PARSER_FLAG_TRACKLINES was used during
 * initialization of the parser.
 *
 * \param parser Parser object.
 *
 * \return Column number starting at 1.
 */
M_API size_t M_parser_current_column(M_parser_t *parser);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Compare parser contents to provided buffer 
 *
 * Does not advance.
 * 
 * \param[in] parser   Parser object.
 * \param[in] data     Data to compare.
 * \param[in] data_len Length of data to compare
 *
 * \return M_TRUE if match, M_FALSE otherwise
 */
M_API M_bool M_parser_compare(M_parser_t *parser, const unsigned char *data, size_t data_len);


/*! Compare parser contents to provided string.  
 *
 * Does not advance.
 * 
 * \param[in] parser   Parser object.
 * \param[in] str      String data to compare.
 * \param[in] max_len  Maximum length of data to compare, 0 for entire string.  If 0 is
 *                     specified, then also the entire parser buffer must be an exact
 *                     match.  If there are extra bytes after the match, this will not
 *                     be considered an exact match.
 * \param[in] casecmp  Perform case-insensitive comparison?
 *
 * \return M_TRUE if match, M_FALSE otherwise
 */
M_API M_bool M_parser_compare_str(M_parser_t *parser, const char *str, size_t max_len, M_bool casecmp);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Marks the current position for future reference in case additional data might need to be pulled from this marked
 * point forward.
 *
 * If a data position is marked, it will not be eligible to be destroyed/chopped until the marked position is cleared.
 *
 * \param[in,out] parser Parser object.
 */
M_API void M_parser_mark(M_parser_t *parser);


/*! Clears the current marked position, allowing it to be garbage collected.
 *
 * \param[in,out] parser Parser object.
 */
M_API void M_parser_mark_clear(M_parser_t *parser);


/*! Obtain the length of the marked position to the current position.
 *
 * \param[in] parser Parser object.
 *
 * \return Length or 0 on error.
 */
M_API size_t M_parser_mark_len(M_parser_t *parser);


/*! Rewind data back to the marked position.
 *
 * This will automaticaly clear the marked position so if the marked position
 * is still needed, the caller must re-mark it.
 *
 * \param[in,out] parser Parser object.
 *
 * \return Number of bytes rewinded or 0 on error.
 */
M_API size_t M_parser_mark_rewind(M_parser_t *parser);


/*! Reset set the parser back to the beginning of the data.
 *
 * This is only applicable to 'const' parsers, and will fail on dynamic parsers.
 * If this scans back past a marked position, the mark will be automatically cleared.
 *
 * \param[in,out] parser Parser object.
 *
 * \return Number of bytes regurgitated or 0 on error.
 */
M_API size_t M_parser_reset(M_parser_t *parser);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the internal pointer for the current position in the parse buffer.
 *
 * \param[in] parser Parser object.
 *
 * \return Pointer to data.
 */
M_API const unsigned char *M_parser_peek(M_parser_t *parser);


/*! Retrieve the internal pointer for the marked position in the parse buffer.
 *
 * \param[in]  parser Parser object.
 * \param[out] len    Length of marked data.
 *
 * \return Pointer to data.
 */
M_API const unsigned char *M_parser_peek_mark(M_parser_t *parser, size_t *len);


/*! Read a single byte from the current buffer without advancing.
 *
 * \param[in]  parser Parser object.
 * \param[out] byte   Outputs byte read.
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_parser_peek_byte(M_parser_t *parser, unsigned char *byte);


/*! Read bytes (binary) from the current buffer and output in the user-provided
 * buffer without advancing.
 *
 * The data read will not be NULL terminated and the buffer provided must be at least as large as large as the
 * requested data.
 *
 * \param[in]     parser  Parser object.
 * \param[in]     len     Length of data to read.
 * \param[in,out] buf     Buffer to hold output.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes or other error.
 */
M_API M_bool M_parser_peek_bytes(M_parser_t *parser, size_t len, unsigned char *buf);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Truncate the parse buffer at the position specified (relative to the 
 *  current parse offset).
 *
 * \param[in,out] parser Parser object.
 * \param[in]     len    Length to truncate to.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes exist in the data
 *          stream or other error.
 */
M_API M_bool M_parser_truncate(M_parser_t *parser, size_t len);


/*! Truncate all available whitespace.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     flags  A bitmap of enum M_PARSER_WHITESPACE_FLAGS.
 *
 * \return Number of bytes consumed.
 */
M_API size_t M_parser_truncate_whitespace(M_parser_t *parser, M_uint32 flags);


/*! Truncate all bytes until the specified sequence of bytes is encountered in the data stream.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     pat     Sequence of bytes to search for.
 * \param[in]     len     Length of pattern data.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return Number of bytes consumed, or 0 if not found.
 */
M_API size_t M_parser_truncate_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat);


/*! Truncate all bytes matching the given charset.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     charset     Character set.
 * \param[in]     charset_len Length of given character set.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_truncate_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len);


/*! Truncate all bytes matching the given predicate function.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     func   Predicate function.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_truncate_predicate(M_parser_t *parser, M_parser_predicate_func func);


/*! Truncate all bytes matching the given chr predicate function.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     func   Predicate function.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_truncate_chr_predicate(M_parser_t *parser, M_chr_predicate_func func);


/*! Truncate all bytes until the specified string is encountered in the data stream.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     pat     String to search for.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return Number of bytes consumed, or 0 if not found.
 */
M_API size_t M_parser_truncate_str_until(M_parser_t *parser, const char *pat, M_bool eat_pat);


/*! Truncate all bytes matching the given NULL-terminated charset.
 *
 * Searches backwards from end to start.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Character set, NULL-terminated.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_truncate_str_charset(M_parser_t *parser, const char *charset);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Consume the given number of bytes.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     len    Number of bytes to consume.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes.
 */
M_API M_bool M_parser_consume(M_parser_t *parser, size_t len);


/*! Consume all available whitespace.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     flags  A bitmap of enum M_PARSER_WHITESPACE_FLAGS.
 *
 * \return Number of bytes consumed.
 */
M_API size_t M_parser_consume_whitespace(M_parser_t *parser, M_uint32 flags);


/*! Consume all bytes until the specified sequence of bytes is encountered in the data stream.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     pat     Sequence of bytes to search for.
 * \param[in]     len     Length of pattern data.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return Number of bytes consumed, or 0 if not found.
 */
M_API size_t M_parser_consume_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat);


/*! Consume all bytes matching the given charset.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     charset     Character set.
 * \param[in]     charset_len Length of given character set.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len);


/*! Consume all bytes matching the given predicate function.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     func   Predicate function.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_predicate(M_parser_t *parser, M_parser_predicate_func func);


/*! Consume all bytes matching the given chr predicate function.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     func   Predicate function.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_chr_predicate(M_parser_t *parser, M_chr_predicate_func func);


/*! Consume all bytes until the specified string is encountered in the data stream.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     pat    String to search for.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return Number of bytes consumed, or 0 if not found.
 */
M_API size_t M_parser_consume_str_until(M_parser_t *parser, const char *pat, M_bool eat_pat);


/*! Consume all bytes matching the given NULL-terminated charset.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Character set, NULL-terminated.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_str_charset(M_parser_t *parser, const char *charset);


/*! Consume all bytes not matching the given charset.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Character set, NULL-terminated.
 * \param[in]     charset_len Length of given character set.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_not_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len);


/*! Consume all bytes not matching the given NULL-terminated charset.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Character set, NULL-terminated.
 *
 * \return Number of bytes consumed, or 0 if none/error.
 */
M_API size_t M_parser_consume_str_not_charset(M_parser_t *parser, const char *charset);


/*! Consume all bytes until and including the next end of line.
 *
 * Useful for ignoring data until end of single-line comment. If there is no new line, will consume all remaining data.
 *
 * \param[in,out] parser Parser object.
 *
 * \return Number of bytes consumed.
 */
M_API size_t M_parser_consume_eol(M_parser_t *parser);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a signed integer from the current buffer and advance.
 *
 *  For ASCII formatted integers:
 *    - if len is not specified, it will read up until the first non-numeric
 *      character is encountered. At least one numeric must be encountered or
 *      it is considered an error.
 *    - if len is specified, the integer must be exactly that length (no shorter)
 *      or it is considered an error.
 *    - if base is specified as 0, will attempt to auto-detect the base.
 *
 *  For BigEndian or Little Endian formatted integers:
 *    - The len is mandatory, and base is ignored. Maximum len is 8.
 *
 *  \param[in,out] parser  Parser object.
 *  \param[in]     type    How integer is represented in the data stream.
 *  \param[in]     len     Length of integer, or 0 to auto-determine for ASCII.
 *  \param[in]     base    Base represented in ASCII, or 0 to auto-determine (or non-ascii).
 *  \param[out]    integer Integer storage.
 *
 *  \return M_FALSE on failure, M_TRUE on success.
 */
M_API M_bool M_parser_read_int(M_parser_t *parser, enum M_PARSER_INTEGER_TYPE type, size_t len, unsigned char base, M_int64 *integer);


/*! Read an unsigned integer from the current buffer and advance.
 *
 * See M_parser_read_int() for details on usage as requirements are the same.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     type    How integer is represented in the data stream.
 * \param[in]     len     Length of integer, or 0 to auto-determine for ASCII.
 * \param[in]     base    Base represented in ASCII, or 0 to auto-determine (or non-ascii).
 * \param[out]    integer Integer storage.
 *
 * \return M_FALSE on failure, M_TRUE on success.
 *
 * \see M_parser_read_int
 */
M_API M_bool M_parser_read_uint(M_parser_t *parser, enum M_PARSER_INTEGER_TYPE type, size_t len, unsigned char base, M_uint64 *integer);


/*! Read and unsigned Binary Coded Decimal integer from the current buffer and advance
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     len     Length of integer in bytes.
 * \param[out]    integer Integer storage.
 *
 * \return M_FALSE on failure, M_TRUE on success.
 */
M_API M_bool M_parser_read_uint_bcd(M_parser_t *parser, size_t len, M_uint64 *integer);


/*! Read a decimal number from current buffer and advance.
 *
 * The number must be represented in base 10 and in ASCII form.
 *
 * \param[in,out] parser        Parser object.
 * \param[in]     len           Length of decimal, or 0 to auto-determine.
 * \param[in]     truncate_fail M_TRUE to treat a truncation as a failure and not increment the consumer.
 *                              M_FALSE otherwise.
 * \param[out]    decimal       Decimal storage.
 *
 * \return enum M_DECIMAL_RETVAL values.
 */
M_API enum M_DECIMAL_RETVAL M_parser_read_decimal(M_parser_t *parser, size_t len, M_bool truncate_fail, M_decimal_t *decimal);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a single byte from the current buffer and advance.
 *
 * \param[in,out] parser Parser object.
 * \param[out]    byte   Outputs byte read.
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_parser_read_byte(M_parser_t *parser, unsigned char *byte);


/*! Read the exact number of bytes (binary) from the current buffer and output in the user-provided buffer and advance.
 *  If there are fewer than the requested bytes available, an error will be returned.
 *
 * The data read will not be NULL terminated and the buffer provided must be at least as large as large as the
 * requested data.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     len     Length of data to read.
 * \param[out]    buf     Buffer to hold output.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes or other error.
 */
M_API M_bool M_parser_read_bytes(M_parser_t *parser, size_t len, unsigned char *buf);


/*! Read bytes (binary) from the current buffer and output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated and the buffer provided must be at least as large as large as the
 * requested data.  If the length of data specified is not available, it will return the number of 
 * bytes actually read.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     len     Requested length of data to read.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     buf_len Length of buffer to hold output.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API size_t M_parser_read_bytes_max(M_parser_t *parser, size_t len, unsigned char *buf, size_t buf_len);


/*! Read bytes (binary) until the specified sequence of bytes is encountered in the data stream.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     buf_len Length of buffer to hold output.
 * \param[in]     pat     Sequence of bytes to search for.
 * \param[in]     pat_len Length of pattern data.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API size_t M_parser_read_bytes_until(M_parser_t *parser, unsigned char *buf, size_t buf_len, const unsigned char *pat, size_t pat_len, M_bool eat_pat);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided character set, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     charset     Array of characters that are allowed.
 * \param[in]     charset_len Length of character set.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     buf_len     Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_bytes_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len, unsigned char *buf, size_t buf_len);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided predicate, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     func        Predicate function.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     buf_len     Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_bytes_predicate(M_parser_t *parser, M_parser_predicate_func func, unsigned char *buf, size_t buf_len);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided chr predicate, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     func        Predicate function.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     buf_len     Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_bytes_chr_predicate(M_parser_t *parser, M_chr_predicate_func func, unsigned char *buf, size_t buf_len);


/*! Read data from a marked position until the current parser position.
 *
 * The marked position will be automatically cleared. Provided buffer must be at least M_parser_mark_len() bytes long.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to store result.
 * \param[in]     buf_len Size of result buffer.
 *
 * \return number of bytes written to buffer or 0 on error.
 */
M_API size_t M_parser_read_bytes_mark(M_parser_t *parser, unsigned char *buf, size_t buf_len);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a string from the current buffer of the exact given length, output in
 *  the user-provided buffer and advance. If there are insufficient bytes a
 *  failure will be returned.
 *
 * The length of the requested string must be at least one byte shorter than
 * the buffer size to account for the null termination. If you do not already
 * have a buffer, use M_parser_read_strdup() which will return a newly allocated
 * buffer for you.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     len     Length of string to read (must be at least one byte shorter than buf_len).
 * \param[out]    buf     Output pointer to store result.
 * \param[in]     buf_len Length of output buffer (must be at least one byte greater than len).
 *
 * \return M_TRUE on success, or M_FALSE if not enough bytes or other error.
 * 
 * \see M_parser_read_strdup
 */
M_API M_bool M_parser_read_str(M_parser_t *parser, size_t len, char *buf, size_t buf_len);


/*! Read a string from the current buffer, output in the user-provided buffer and advance.
 *
 * The length of the requested string must be at least one byte shorter than
 * the buffer size to account for the null termination. If you do not already
 * have a buffer, use M_parser_read_strdup() which will return a newly allocated
 * buffer for you.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     len     Requested length of string to read (must be at least one
 *                        byte shorter than buf_len).
 * \param[out]    buf     Output pointer to store result.
 * \param[in]     buf_len Length of output buffer (must be at least one byte greater than len).
 *
 * \return number of bytes read, or 0 on failure.
 * 
 * \see M_parser_read_str
 */
M_API size_t M_parser_read_str_max(M_parser_t *parser, size_t len, char *buf, size_t buf_len);


/*! Read data until the specified sequence of bytes is encountered in the data stream.
 *
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     buf_len Length of buffer to hold output.
 * \param[in]     pat     String to search for.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API size_t M_parser_read_str_until(M_parser_t *parser, char *buf, size_t buf_len, const char *pat, M_bool eat_pat);


/*! Read data from the buffer for as long as it matches one of the bytes in the given character set and advance.
 *
 * Put the resulting bytes in the provided buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Array of characters that are allowed, NULL terminated.
 * \param[out]    buf     Buffer to store result. Will be NULL terminated.
 * \param[in]     buf_len Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 *
 * \see M_parser_read_strdup_charset
 */
M_API size_t M_parser_read_str_charset(M_parser_t *parser, const char *charset, char *buf, size_t buf_len);


/*! Read data from the buffer for as long as it matches the given predicate function and advance.
 *
 * Put the resulting bytes in the provided buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     func    predicate function.
 * \param[out]    buf     Buffer to store result. Will be NULL terminated.
 * \param[in]     buf_len Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 *
 * \see M_parser_read_strdup_predicate
 */
M_API size_t M_parser_read_str_predicate(M_parser_t *parser, M_parser_predicate_func func, char *buf, size_t buf_len);


/*! Read data from the buffer for as long as it matches the given chr predicate function and advance.
 *
 * Put the resulting bytes in the provided buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     func    predicate function.
 * \param[out]    buf     Buffer to store result. Will be NULL terminated.
 * \param[in]     buf_len Size of result buffer.
 *
 * \return Length of data read, or 0 on error.
 *
 * \see M_parser_read_strdup_predicate
 */
M_API size_t M_parser_read_str_chr_predicate(M_parser_t *parser, M_chr_predicate_func func, char *buf, size_t buf_len);


/*! Read data from a marked position until the current parser position.
 *
 * The marked position will be automatically cleared. Provided buffer must be at least M_parser_mark_len() bytes plus
 * the NULL terminator.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to store result.
 * \param[in]     buf_len Size of result buffer.
 *
 * \return Number of bytes written to buffer or 0 on error.
 */
M_API size_t M_parser_read_str_mark(M_parser_t *parser, char *buf, size_t buf_len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a string for the given length from the current buffer, allocates an output buffer and advance.
 *
 * \param[in,out] parser Parser object.
 * \param[in]     len    Length of string to read.
 *
 * \return Buffer containing the string on success, or NULL on failure.
 *
 * \see M_parser_read_str
 */
M_API char *M_parser_read_strdup(M_parser_t *parser, size_t len);


/*! Read bytes (binary) from the parser, return as hex-encoded string and advance.
 *
 * \param[in,out] parser Parser object to read binary bytes from.
 * \param[in]     len    Number of binary bytes to read from parser.
 *
 * \return null-terminated hex string on success, NULL if not enough bytes or other error
 *
 * \see M_parser_read_buf_hex
 */
M_API char *M_parser_read_strdup_hex(M_parser_t *parser, size_t len);


/*! Read data until the specified sequence of bytes is encountered in the data stream.
 *
 * Put the resulting bytes in a newly allocated buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     pat     Sequence of bytes to search for.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API char *M_parser_read_strdup_until(M_parser_t *parser, const char *pat, M_bool eat_pat);


/*! Read data from the buffer for as long as it matches one of the bytes in the given character set and advance.
 *
 * Put the resulting bytes in a newly allocated buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     charset Array of characters that are allowed, NULL terminated.
 *
 * \return NULL-terminated result buffer, or NULL on error.
 *
 * \see M_parser_read_str_charset
 */
M_API char *M_parser_read_strdup_charset(M_parser_t *parser, const char *charset);


/*! Read data from the buffer for as long as it matches the given predicate function and advance.
 *
 * Put the resulting bytes in a newly allocated buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     func    Predicate function.
 *
 * \return NULL-terminated result buffer, or NULL on error.
 *
 * \see M_parser_read_str_predicate
 */
M_API char *M_parser_read_strdup_predicate(M_parser_t *parser, M_parser_predicate_func func);


/*! Read data from the buffer for as long as it matches the given predicate function and advance.
 *
 * Put the resulting bytes in a newly allocated buffer.
 *
 * \param[in,out] parser  Parser object.
 * \param[in]     func    predicate function.
 *
 * \return NULL-terminated result buffer, or NULL on error.
 *
 * \see M_parser_read_str_predicate
 */
M_API char *M_parser_read_strdup_chr_predicate(M_parser_t *parser, M_chr_predicate_func func);


/*! Read data from a marked position until the current parser position.
 *
 * The marked position will be automatically cleared. An allocated buffer with the requested data will be returned,
 * NULL terminated.
 *
 * \param[in,out] parser Parser object.
 *
 * \return NULL-terminated result, or NULL on error.
 */
M_API char *M_parser_read_strdup_mark(M_parser_t *parser);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/*! Read the exact number of bytes (binary) from the current buffer and output in the user-provided buffer and advance.
 *
 * If there are fewer than the requested bytes available, an error will be returned.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     len     Length of data to read.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes or other error.
 */
M_API M_bool M_parser_read_buf(M_parser_t *parser, M_buf_t *buf, size_t len);


/*! Read bytes (binary) from the parser, write as hex-encoded string into the provided buffer, and advance.
 *
 * \param[in,out] parser Parser object to read binary bytes from.
 * \param[out]    buf    Buffer to hold hex-ascii output.
 * \param[in]     len    Number of binary bytes to read from parser.
 *
 * \return M_TRUE on success, M_FALSE if not enough bytes or other error.
 */
M_API M_bool M_parser_read_buf_hex(M_parser_t *parser, M_buf_t *buf, size_t len);


/*! Read bytes (binary) from the current buffer and output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated. If the length of data specified
 * is not available, it will return the number of bytes actually read.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     len     Requested length of data to read.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API size_t M_parser_read_buf_max(M_parser_t *parser, M_buf_t *buf, size_t len);


/*! Read bytes (binary) until the specified sequence of bytes is encountered in the data stream.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to hold output.
 * \param[in]     pat     Sequence of bytes to search for.
 * \param[in]     pat_len Length of pattern data.
 * \param[in]     eat_pat Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return number of bytes read, or 0 on error or no bytes available.
 */
M_API size_t M_parser_read_buf_until(M_parser_t *parser, M_buf_t *buf, const unsigned char *pat, size_t pat_len, M_bool eat_pat);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided character set, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     charset     Array of characters that are allowed.
 * \param[in]     charset_len Length of character set.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_buf_charset(M_parser_t *parser, M_buf_t *buf, const unsigned char *charset, size_t charset_len);


/*! Read bytes (binary) from the current buffer as long as the bytes do not match the
 * provided character set, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     charset     Array of characters that are allowed.
 * \param[in]     charset_len Length of character set.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_buf_not_charset(M_parser_t *parser, M_buf_t *buf, const unsigned char *charset, size_t charset_len);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided predicate, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     func        Predicate function.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_buf_predicate(M_parser_t *parser, M_buf_t *buf, M_parser_predicate_func func);


/*! Read bytes (binary) from the current buffer as long as the bytes match the
 * provided chr predicate, output in the user-provided buffer and advance.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser      Parser object.
 * \param[out]    buf         Buffer to store result.
 * \param[in]     func        Predicate function.
 *
 * \return Length of data read, or 0 on error.
 */
M_API size_t M_parser_read_buf_chr_predicate(M_parser_t *parser, M_buf_t *buf, M_chr_predicate_func func);


/*! Read data from a marked position until the current parser position.
 *
 * The marked position will be automatically cleared.
 *
 * The data read will not be NULL terminated.
 *
 * \param[in,out] parser  Parser object.
 * \param[out]    buf     Buffer to store result.
 *
 * \return number of bytes written to buffer or 0 on error.
 */
M_API size_t M_parser_read_buf_mark(M_parser_t *parser, M_buf_t *buf);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create new parser from the current position for the given length from the current buffer, allocates a parser and advance.
 *  All data is copied into a new memory segment.
 *
 * \param[in,out] parser    Parser object.
 * \param[in]     len       Length to read.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser(M_parser_t *parser, size_t len);


/*! Read data from the buffer until the specified sequence of bytes is encountered in the data stream.
 *  All data is copied into a new memory segment.
 *
 *  Put the resulting bytes in a newly allocated parser.
 *
 * \param[in,out] parser    Parser object.
 * \param[in]     pat       Sequence of bytes to search for.
 * \param[in]     len       Length of pattern data.
 * \param[in]     eat_pat   Should the sequence of bytes be consumed. Useful for ignoring data until end of comment.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat);


/*! Read data from the buffer for as long as it matches one of the bytes in the given character set and advance.
 *  All data is copied into a new memory segment.
 * 
 * Put the resulting bytes in a newly allocated parser.
 *
 * \param[in,out] parser      Parser object.
 * \param[in]     charset     Array of characters that are allowed, NULL terminated.
 * \param[in]     charset_len Number of characters in the set.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser_charset(M_parser_t *parser, unsigned const char *charset, size_t charset_len);


/*! Create new parser from the buffer for as long as it matches the given predicate function and advance.
 *  All data is copied into a new memory segment.
 *
 * Put the resulting bytes in a newly allocated parser.
 *
 * \param[in,out] parser    Parser object.
 * \param[in]     func      Predicate function.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser_predicate(M_parser_t *parser, M_parser_predicate_func func);


/*! Create new parser from the buffer for as long as it matches the given predicate function and advance.
 *  All data is copied into a new memory segment.
 * 
 * Put the resulting bytes in a newly allocated parser.
 *
 * \param[in,out] parser    Parser object.
 * \param[in]     func      Predicate function.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser_chr_predicate(M_parser_t *parser, M_parser_predicate_func func);


/*! Create new parser from a marked position until the current parser position, allocates a parser and advance.
 *  All data is copied into a new memory segment.
 *
 * \param[in,out] parser    Parser object.
 *
 * \return parser.
 */
M_API M_parser_t *M_parser_read_parser_mark(M_parser_t *parser);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read an STX, ETX, LRC wrapped message.
 *
 * The first character in the parser must be an STX.
 *
 * \param[in,out] parser          Parser object.
 * \param[out]    out             Parser object with result message.
 * \param[in]     lrc_frame_chars Framing characters that should be included in LRC calculation.
 *
 * \return result. On success and LRC calculation failure the message will be returned in the output parser.
 *         Otherwise the output parser's contents are undefined.
 *
 *         Results M_PARSER_FRAME_ERROR_NO_STX, M_PARSER_FRAME_ERROR_NO_ETX, and M_PARSER_FRAME_ERROR_NO_LRC 
 *         will not advance the parser.
 */
M_API M_PARSER_FRAME_ERROR M_parser_read_stxetxlrc_message(M_parser_t *parser, M_parser_t **out, M_uint32 lrc_frame_chars);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Validate the parser matches the given predicate function.
 *
 * \param[in] parser Parser object.
 * \param[in] len    Length to validate. If larger than the parser length the parser length is used.
 * \param[in] func   Predicate function.
 *
 * \return M_TRUE if matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_predicate(M_parser_t *parser, size_t len, M_parser_predicate_func func);


/*! Validate the parser matches the given chr predicate function.
 *
 * \param[in] parser Parser object.
 * \param[in] len    Length to validate. If larger than the parser length the parser length is used.
 * \param[in] func   Char predicate function.
 *
 * \return M_TRUE if matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_chr_predicate(M_parser_t *parser, size_t len, M_chr_predicate_func func);


/*! Validate the parser matches the given character set.
 *
 * \param[in] parser      Parser object.
 * \param[in] len         Length to validate. If larger than the parser length the parser length is used.
 * \param[in] charset     Character set.
 * \param[in] charset_len Length of given character set.
 *
 * \return M_TRUE if matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_charset(M_parser_t *parser, size_t len, const unsigned char *charset, size_t charset_len);


/*! Validate the parser matches the given NULL-terminated charset.
 *
 * \param[in] parser      Parser object.
 * \param[in] len         Length to validate. If larger than the parser length the parser length is used.
 * \param[in] charset     Character set.
 *
 * \return M_TRUE if matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_str_charset(M_parser_t *parser, size_t len, const char *charset);


/*! Validate the parser does not match the given predicate function.
 *
 * \param[in] parser Parser object.
 * \param[in] len    Length to validate. If larger than the parser length the parser length is used.
 * \param[in] func   Predicate function.
 *
 * \return M_TRUE if not matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_not_predicate(M_parser_t *parser, size_t len, M_parser_predicate_func func);


/*! Validate the parser does not match the given chr predicate function.
 *
 * \param[in] parser Parser object.
 * \param[in] len    Length to validate. If larger than the parser length the parser length is used.
 * \param[in] func   Char predicate function.
 *
 * \return M_TRUE if not matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_not_chr_predicate(M_parser_t *parser, size_t len, M_chr_predicate_func func);


/*! Validate the parser does not match the given character set.
 *
 * \param[in] parser      Parser object.
 * \param[in] len         Length to validate. If larger than the parser length the parser length is used.
 * \param[in] charset     Character set.
 * \param[in] charset_len Length of given character set.
 *
 * \return M_TRUE if not matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_not_charset(M_parser_t *parser, size_t len, const unsigned char *charset, size_t charset_len);


/*! Validate the parser does not match the given NULL-terminated charset.
 *
 * \param[in] parser      Parser object.
 * \param[in] len         Length to validate. If larger than the parser length the parser length is used.
 * \param[in] charset     Character set.
 *
 * \return M_TRUE if not matching. Otherwise M_FALSE;
 */
M_API M_bool M_parser_is_not_str_charset(M_parser_t *parser, size_t len, const char *charset);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Split the data in the parser object by the delimiter specified into
 *  additional parser objects. 
 *
 *  \param[in,out] parser     Parser
 *  \param[in]     delim      The delimiter to split on
 *  \param[in]     maxcnt     Maximum number of objects to create, remaining data will be part of the last object.
 *                            0 if no maximum.
 *  \param[in]     flags      M_PARSER_SPLIT_FLAGS flags controlling behavior of parser
 *  \param[out]    num_output The number of parser objects output
 *  \return array of parser objects or NULL on failure
 */
M_API M_parser_t **M_parser_split(M_parser_t *parser, unsigned char delim, size_t maxcnt, M_uint32 flags, size_t *num_output);

/*! Free child parser objects returned from M_parser_split
 * \param[in] parsers Array of parser objects
 * \param[in] cnt     Count of objects as returned from M_parser_split
 */
M_API void M_parser_split_free(M_parser_t **parsers, size_t cnt);

/*! @} */

__END_DECLS

#endif /* __M_PARSER_H__ */
