/* The MIT License (MIT)
 *
 * Copyright (c) 2021 Monetra Technologies, LLC.
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

#ifndef __M_BITLIST_H__
#define __M_BITLIST_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_bitlist Bitwise flags parser and generator
 *  \ingroup mstdlib_base
 *
 * Allows for easy creation of a data structure to parse and generate human
 * readable flags lists made up of bits.
 *
 * \code{.c}
 *     static const M_bitlist_t myflags[] = {
 *       { 1 << 0, "flag1" },
 *       { 1 << 1, "flag2" },
 *       { 1 << 2, "flag3" },
 *       { 0,      NULL    }
 *     };
 *
 *     M_uint64  initial_flags = (1<<0) | (1<<2);
 *     char     *human_flags   = NULL;
 *     M_uint64  final_flags   = 0;
 *     M_bool    rv            = M_FALSE;
 *
 *     if (!M_bitlist_list(&human_flags, M_BITLIST_FLAG_NONE, myflags, initial_flags, '|', error, sizeof(error)))
 *       goto fail;
 *
 *     if (!M_bitlist_parse(&final_flags, M_BITLIST_FLAG_NONE, myflags, human_flags, '|', error, sizeof(error)))
 *       goto fail;
 *
 *     if (initial_flags != final_flags) {
 *       M_snprintf(error, sizeof(error), "initial flags don't match converted");
 *       goto fail;
 *     }
 *     rv = M_TRUE;
 *
 *     fail:
 *     M_free(human_flags);
 *     if (!rv) {
 *       M_printf("FAILURE: %s", error);
 *     }
 *
 * \endcode
 *
 * @{
 */

/*! Data structure to be created as an array and filled in by caller.  Must be 
 *  terminated by an entry where the name parameter is set to NULL */
typedef struct {
	M_uint64    id;   /*!< The bit to set, usually a power of 2 */
	const char *name; /*!< Human-readable name associated with the flag/bit */ 
} M_bitlist_t;

/*! Flags that may be passed on to the parser or human-readable generator */
typedef enum {
	M_BITLIST_FLAG_NONE                  = 0,    /*!< No Flags */
	M_BITLIST_FLAG_DONT_TRIM_WHITESPACE  = 1<<0, /*!< Parse only. Don't trim whitespace that might surround flags. */
	M_BITLIST_FLAG_CASE_SENSITIVE        = 1<<1, /*!< Parse only. Case sensitive flag matching. */
	M_BITLIST_FLAG_IGNORE_UNKNOWN        = 1<<2, /*!< If a bit is set that is not known, ignore */
	M_BITLIST_FLAG_DONT_REQUIRE_POWEROF2 = 1<<3  /*!< Don't require a field to be a power of 2. */
} M_bitlist_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Generate a human-readable list from the array provided and bits provided, 
 *  delimited by the specified delimiter.
 *
 * \param[out]    out       Null-terminated string representing the human-readable bit list
 * \param[in]     flags     Flags used when generating the list or determining errors
 * \param[in]     list      Array passed in defining all bits to human-readable
 * \param[in]     bits      Machine-readable bits passed in to be converted
 * \param[in]     delim     Delimiter for human-readable list output
 * \param[in,out] error     Buffer to hold error string.
 * \param[in]     error_len Length of error buffer
 * \return M_TRUE on success, M_FALSE on failure */
M_API M_bool M_bitlist_list(char **out, M_bitlist_flags_t flags, const M_bitlist_t *list, M_uint64 bits, unsigned char delim, char *error, size_t error_len);


/*! Generate an integer after parsing the provided human-readable string of bits/flags.
 *
 * \param[out]    out       64bit integer of set bits
 * \param[in]     flags     Flags used when parsing the provided human-readable string
 * \param[in]     list      Array passed in defining all bits to human-readable name
 * \param[in]     data      Human-readable bits/flags in null terminated string form
 * \param[in]     delim     Delimiter for human-readable list input
 * \param[in,out] error     Buffer to hold error string.
 * \param[in]     error_len Length of error buffer
 * \return M_TRUE on success, M_FALSE on failure */
M_API M_bool M_bitlist_parse(M_uint64 *out, M_bitlist_flags_t flags, const M_bitlist_t *list, const char *data, unsigned char delim, char *error, size_t error_len);


/*! @} */

__END_DECLS

#endif /* __M_BIT_BUF_H__ */
