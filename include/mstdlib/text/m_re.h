/* The MIT License (MIT)
 * 
 * Copyright (c) 2019 Monetra Technologies, LLC.
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

#ifndef __M_RE_H__
#define __M_RE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_re Regular Expression
 *  \ingroup m_text
 *
 * The engine targets Perl/Python/PCRE expression syntax. However, this is
 * not a full implementation of the syntax.
 *
 * The re engine is uses DFA processing to ensure evaluation happens in a
 * reasonable amount of time. It does not use back tracking to avoid pathological
 * expressions causing very slow run time. Due to this back references in patterns
 * are not supported.
 *
 * Patterns are thread safe and re-entrant.
 *
 * ## Supported:
 *
 * ### Syntax
 * 
 * Expression      | Description
 * ----------      | -----------
 * `.`             | any character (except newline, see DOTALL)
 * `^`             | Start of string. Or start of line in MULTILINE
 * `$`             | End of string. Or end of line in MULTILINE
 * `*`             | 0 or more repetitions
 * `+`             | 1 or more repetitions
 * `?`             | 0 or 1 repetitions
 * `*? +? ??`      | Ungreedy version of repetition
 * `{#}`           | Exactly # of repetitions
 * `{#,}`          | # or more repetitions
 * `{#,#}`         | Inclusive of # and # repetitions
 * `\`             | Escape character. E.g. `\\ => \`
 * `[]`            | Character range. Can be specific characters or '-' specified range. Multiple ranges can be specified. E.g. `[a-z-8XYZ]`
 * `[^]`           | Negative character range. Can be specific characters or '-' specified range. Multiple ranges can be specified. E.g. `[^a-z-8XYZ]`
 * \|              | Composite A or B. E.g. A\|B
 * `()`            | Pattern and capture group. Groups expressions together for evaluation when used with \|. Also, defines a capture group.
 * `(?imsU-imsU)`  | Allows specifying compile flags in the expression. Supports `i` (ignore case), `m` (multiline), `s` (dot all), `U` (ungreedy). - can be used to disable a flag. E.g. (?im-s). Only allowed to be used once at the start of the pattern.
 * `\s`            | White space. Equivalent to `[ \t\n\r\f\v]`
 * `\S`            | Not white space. Equivalent to `[^ \t\n\r\f\v]`
 * `\d`            | Digit (number). Equivalent to `[0-9]`
 * `\D`            | Not digit Equivalent to `[^0-9]`
 * `\w`            | Word
 * `\W`            | Not word
 * `\xHH \x{HHHH}` | Hex values
 * `\<`            | Beginning of word
 * `\>`            | End of word
 *
 * \note \ as part of \| (pipe) shown in table is for escaping and not part of syntax.
 *
 * ### POSIX character classes for bracket expressions
 *
 * Character ranges _must_ be used in `[]` expressions. `^` negation is supported with ranges.
 *
 * Range           | Description
 * -----           | -----------
 * `[:alpha:]`     | Alpha characters. Contains `[a-zA-Z]`
 * `[:alnum:]`     | Alpha numeric characters. Contains `[a-zA-Z0-9]`
 * `[:word:]`      | Alpha numeric characters. Contains `[a-zA-Z0-9_]`. Equivalent to `\w`
 * `[:space:]`     | White space characters. Contains `[ \t\r\n\v\f]`. Equivalent to `\s`
 * `[:digit:]`     | Digit (number) characters. Contains `[0-9]`. Equivalent to `\d`
 * `[:cntrl:]`     | Control characters. Contains `[\x00-\x1F\x7F]`. Note: `\x00` is the NULL string terminator so this is really `[\x01-\x1F\x7F]` because `\x00` can never be encountered in a string.
 * `[:print:]`     | Printable characters range. Contains `[\x20-\x7E]`
 * `[:xdigit:]`    | Hexadecimal digit range. Contains `[0-9a-fA-F]`
 * `[:lower:]`     | Lower case character range. Contains `[a-z]`
 * `[:upper:]`     | Upper case character range. Contains `[A-Z]`
 * `[:blank:]`     | Blank character range. Contains `[ \t]`
 * `[:graph:]`     | Graph character range. Contains `[\x21-\x7E]`
 * `[:punct:]`     | Punctuation character range. Contains `[!"\#$%&'()*+,\-./:;<=>?@\[\\\]^_\`{\|}~]`
 *
 * \note \ as part of \| (pipe) and \` shown in `[:punct:]` is for escaping and not part of character set.
 *
 * ### Features
 * - Numbered captures (up to 99) are supported in M_re_sub's replacement string.
 *
 * ## Not supported:
 *
 * - Back references in patterns
 * - Collating symbols (in brackets)
 * - Equivalence classes (in brackets)
 * - 100% POSIX conformance
 * - BRE (Basic Regular Expression) syntax
 * - \ escape short hands (\\d, \\w, ...) inside of a bracket ([]) expression.
 *
 * ## Match object
 *
 * Patterns can have capture groups which can be filled in a match object
 * during string evaluation. Only numbered capture indexes are supported.
 * Up to 99 captures can be recorded.
 *
 * Index 0 is the full match for the regular expression. If the pattern matches
 * the string, this will always be populated. Groups (when present) are number
 * 1-99.
 *
 * If a capture is present the index will be available. Composite (|) patterns
 * can cause gaps in captures. Meaning capture 1, and 5 could be present but capture
 * 3 and 4 not. Also, captures can be present but have zero length.
 *
 * Finally, captures are reported with offset from the start of the string and
 * the length of the captured data. This is different than some other libraries
 * which return start and end offsets. Utilizing length instead of end offsets
 * was decided based on captures being passed to other functions, the majority
 * of which take a start and length; not an end offset.
 *
 * @{
 */ 

struct M_re;
typedef struct M_re M_re_t;

struct M_ret_match;
typedef struct M_ret_match M_re_match_t;


/*! Pattern  modifier options. */
typedef enum {
	M_RE_NONE      = 0,      /*!< No modifiers applied. */
	M_RE_CASECMP   = 1 << 0, /*!< Matching should be case insensitive. */
	M_RE_MULTILINE = 1 << 1, /*!< ^ and $ match start and end of lines instead of start and end of string. */
	M_RE_DOTALL    = 1 << 2, /*!< Dot matches all characters including new line. */
	M_RE_UNGREEDY  = 1 << 3  /*!< Invert behavior of greedy qualifiers. E.g. ? acts like ?? and ?? acts like ?. */
} M_re_flags_t;


/*! Compile a regular expression pattern.
 *
 * \param[in] pattern The pattern to compile.
 * \param[in] flags   M_re_flags_t flags controlling pattern behavior.
 *
 * \return Re object on success. NULL on compilation error.
 */
M_API M_re_t *M_re_compile(const char *pattern, M_uint32 flags);


/*! Destroy a re object.
 *
 * \param[in] re Re object.
 */
M_API void M_re_destroy(M_re_t *re);


/*! Search for the first match of patten in string.
 *
 * \param[in]  re    Re object.
 * \param[in]  str   String to evaluate.
 * \param[out] match Optional match object.
 *
 * \return M_TRUE if match was found. Otherwise, M_FALSE.
 */
M_API M_bool M_re_search(const M_re_t *re, const char *str, M_re_match_t **match);


/*! Check if the pattern matches from the beginning of the string.
 *
 * Equivalent to the pattern starting with ^ and not multi line.
 *
 * \param[in] re  Re object.
 * \param[in] str String to evaluate.
 *
 * \return M_TRUE if match was found. Otherwise, M_FALSE.
 */
M_API M_bool M_re_eq_start(const M_re_t *re, const char *str);


/*! Check if the pattern matches the entire string
 *
 * Equivalent to the pattern starting with ^, ending with $ and not multi line.
 *
 * \param[in] re  Re object.
 * \param[in] str String to evaluate.
 *
 * \return M_TRUE if match was found. Otherwise, M_FALSE.
 */
M_API M_bool M_re_eq(const M_re_t *re, const char *str);


/*! Get all pattern matches within a string.
 *
 * \param[in] re  Re object.
 * \param[in] str String to evaluate.
 * 
 * \return List of M_re_match_t objects for every match found in the string.
 *         NULL if no matches found.
 */
M_API M_list_t *M_re_matches(const M_re_t *re, const char *str);


/*! Get all matching text within a string.
 *
 * If locations of the text or captures are needed use
 * M_re_matches.
 *
 * \param[in] re  Re object.
 * \param[in] str String to evaluate.
 * 
 * \return List of matching strings for every match found in the string.
 *         NULL if no matches found.
 */
M_API M_list_str_t *M_re_find_all(const M_re_t *re, const char *str);


/*! Substitute matching pattern in string.
 *
 * The replacement string can reference capture groups using
 * `\#`, `\##`, `\g<#>`, `\g<##>`. The capture data applies to the match
 * being evaluated. For example:
 *
 * \code
 * pattern: ' (c-e)'
 * string:  'a b c d e f g'
 * repl:    '\1'
 *
 * result:  'a bcde f g'
 * \endcode
 *
 * \param[in] re   Re object.
 * \param[in] repl Replacement string.
 * \param[in] str  String to evaluate.
 *
 * \return String with substitutions or original string if no sub situations were made.
 */
M_API char *M_re_sub(const M_re_t *re, const char *repl, const char *str);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Destroy a match object.
 *
 * \param[in] match Match object.
 */
M_API void M_re_match_destroy(M_re_match_t *match);


/*! Get a list of all the captured indexes.
 *
 * \param[in] match  Match object.
 *
 * \return List of indexes. Otherwise NULL if no indexes captured.
 */
M_API M_list_u64_t *M_re_match_idxs(const M_re_match_t *match);


/*! Get the offset and length of a match at a given index.
 *
 * \param[in]  match  Match object.
 * \param[in]  idx    Index.
 * \param[out] offset Start of match from the beginning of evaluated string.
 * \param[out] len    Length of matched data.
 *
 * \return M_TRUE if match found for index. Otherwise, M_FALSE.
 */
M_API M_bool M_re_match_idx(const M_re_match_t *match, size_t idx, size_t *offset, size_t *len);

/*! @} */

__END_DECLS

#endif /* __M_RE_H__ */
