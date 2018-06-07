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

#ifndef __M_JSON_H__
#define __M_JSON_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_fs.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_json JSON
 *  \ingroup m_formats
 *
 * Mostly EMCA-404 compliant JSON manipulation.
 *
 * Additional Features:
 * - Comments (C/C++)
 *
 * Also supports most of Stefan Gössner's JSONPath for searching.
 * Not support are features considered redundant or potential
 * security risks (script expressions).
 *
 * Example:
 *
 * \code{.c}
 *     M_json_node_t  *j;
 *     M_json_node_t **n;
 *     size_t          num_matches;
 *     size_t          i;
 *     const char     *s = "{ \"a\" :\n[1, \"abc\",2 ]\n}";
 *     char           *out;
 *
 *     j = M_json_read(s, M_str_len(s), M_JSON_READER_NONE, NULL, NULL, NULL, NULL);
 *     if (j == NULL) {
 *         M_printf("Could not parse json\n");
 *         return M_FALSE;
 *     }
 *
 *     M_json_object_insert_string(j, "b", "string");
 *
 *     n = M_json_jsonpath(j, "$.a[1::3]", &num_matches);
 *     for (i=0; i<num_matches; i++) {
 *         if (M_json_node_type(n[i]) == M_JSON_TYPE_STRING) {
 *             M_printf("%s\n", M_json_get_string(n);
 *         }
 *     }
 *     M_free(n);
 *
 *     out = M_json_write(j, M_JSON_WRITER_PRETTYPRINT_SPACE, NULL);
 *     M_printf(out=\n%s\n", out);
 *     M_free(out);
 *
 *     M_json_node_destroy(j);
 * \endcode
 *
 * @{
 */

struct M_json_node;
typedef struct M_json_node M_json_node_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Types of JSON nodes. */
typedef enum {
	M_JSON_TYPE_UNKNOWN = 0, /*!< An invalid node type. */
	M_JSON_TYPE_OBJECT,      /*!< Object (hashtable). */
	M_JSON_TYPE_ARRAY,       /*!< Array (list). */
	M_JSON_TYPE_STRING,      /*!< String. */
	M_JSON_TYPE_INTEGER,     /*!< Number. */
	M_JSON_TYPE_DECIMAL,     /*!< Floating point number. */
	M_JSON_TYPE_BOOL,        /*!< Boolean. */
	M_JSON_TYPE_NULL         /*!< JSON null type. */
} M_json_type_t;


/*! Flags to control the behavior of the JSON reader. */
typedef enum {
	M_JSON_READER_NONE                     = 0,      /*!< Normal operation. Treat decimal truncation as error and
	                                                      ignore comments. */
	M_JSON_READER_ALLOW_DECIMAL_TRUNCATION = 1 << 0, /*!< Allow decimal truncation. A decimal read and truncated will
	                                                      not be treated as an error. */
	M_JSON_READER_DISALLOW_COMMENTS        = 1 << 1, /*!< Treat comments as an error. */
	M_JSON_READER_OBJECT_UNIQUE_KEYS       = 1 << 2, /*!< Return a parse error when an object has repeating keys. By	
	                                                      default the later key in the object will be the one used and
	                                                      earlier keys ignored. This requires all keys in the object to
	                                                      be unique. */
	M_JSON_READER_DONT_DECODE_UNICODE      = 1 << 3, /*!< By default unicode escapes will be decoded into their utf-8
	                                                      byte sequence. Use this with care because "\u" will be put
	                                                      in the string. Writing will produce "\\u" because the writer
	                                                      will not understand this is a non-decoded unicode escape. */
	M_JSON_READER_REPLACE_BAD_CHARS        = 1 << 4  /*!< Replace bad characters (invalid utf-8 sequences with "?"). */
} M_json_reader_flags_t;


/*! Flags to control the behavior of the JSON writer. */
typedef enum {
	M_JSON_WRITER_NONE                   = 0,      /*!< No indent. All data on a single line. */
	M_JSON_WRITER_PRETTYPRINT_SPACE      = 1 << 0, /*!< 2 space indent. */
	M_JSON_WRITER_PRETTYPRINT_TAB        = 1 << 1, /*!< Tab indent. */
	M_JSON_WRITER_PRETTYPRINT_WINLINEEND = 1 << 2, /*!< Windows line ending "\r\n" instead of Unix line ending "\n".
	                                                    Requires space or tab pretty printing. */
	M_JSON_WRITER_DONT_ENCODE_UNICODE    = 1 << 3, /*!< By default utf-8 characters will be enocded into unicode
	                                                    escapes. */
	M_JSON_WRITER_REPLACE_BAD_CHARS      = 1 << 4  /*!< Replace bad characters (invalid utf-8 sequences with "?"). */
} M_json_writer_flags_t;


/*! Error codes. */
typedef enum {
	M_JSON_ERROR_SUCCESS = 0,              /*!< success */
	M_JSON_ERROR_GENERIC,                  /*!< generic error */
	M_JSON_ERROR_MISUSE,                   /*!< API missuse */
	M_JSON_ERROR_INVALID_START,            /*!< expected Object or Array to start */
	M_JSON_ERROR_EXPECTED_END,             /*!< expected end but more data found */
	M_JSON_ERROR_MISSING_COMMENT_CLOSE,    /*!< close comment not found */
	M_JSON_ERROR_UNEXPECTED_COMMENT_START, /*!< unexpected / */
	M_JSON_ERROR_INVALID_PAIR_START,       /*!< expected string as first half of pair */
	M_JSON_ERROR_DUPLICATE_KEY,            /*!< duplicate key */
	M_JSON_ERROR_MISSING_PAIR_SEPARATOR,   /*!< expected ':' separator in pair */
	M_JSON_ERROR_OBJECT_UNEXPECTED_CHAR,   /*!< unexpected character in object */
	M_JSON_ERROR_EXPECTED_VALUE,           /*!< expected value after ',' */
	M_JSON_ERROR_UNCLOSED_OBJECT,          /*!< expected '}' to close object */
	M_JSON_ERROR_ARRAY_UNEXPECTED_CHAR,    /*!< unexpected character in array */
	M_JSON_ERROR_UNCLOSED_ARRAY,           /*!< expected ']' to close array */
	M_JSON_ERROR_UNEXPECTED_NEWLINE,       /*!< unexpected newline */
	M_JSON_ERROR_UNEXPECTED_CONTROL_CHAR,  /*!< unexpected control character */
	M_JSON_ERROR_INVALID_UNICODE_ESACPE,   /*!< invalid unicode escape */ 
	M_JSON_ERROR_UNEXPECTED_ESCAPE,        /*!< unexpected escape */
	M_JSON_ERROR_UNCLOSED_STRING,          /*!< unclosed string */
	M_JSON_ERROR_INVALID_BOOL,             /*!< invalid bool value */
	M_JSON_ERROR_INVALID_NULL,             /*!< invalid null value */
	M_JSON_ERROR_INVALID_NUMBER,           /*!< invalid number value */
	M_JSON_ERROR_UNEXPECTED_TERMINATION,   /*!< unexpected termination of string data. \0 in data. */
	M_JSON_ERROR_INVALID_IDENTIFIER,       /*!< invalid identifier */
	M_JSON_ERROR_UNEXPECTED_END            /*!< unexpected end of data */
} M_json_error_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a JSON node.
 *
 * \param[in] type The type of the node to create.
 *
 * \return A JSON node on success. NULL on failure (an invalid type was requested).
 *
 * \see M_json_node_destroy
 */
M_API M_json_node_t *M_json_node_create(M_json_type_t type) M_MALLOC;


/*! Destory a JSON node.
 *
 * Destroying a node will destroy every node under it and remove it from it's parent node if it is a child.
 *
 * \param[in] node The node to destroy.
 */
M_API void M_json_node_destroy(M_json_node_t *node) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse a string into a JSON object.
 *
 * \param[in]  data          The data to parse.
 * \param[in]  data_len      The length of the data to parse.
 * \param[in]  flags         M_json_reader_flags_t flags to control the behavior of the reader.
 * \param[out] processed_len Length of data processed. Useful if you could have multiple JSON documents
 *                           in a stream. Optional pass NULL if not needed.
 * \param[out] error         On error this will be populated with an error reason. Optional, pass NULL if not needed.
 * \param[out] error_line    The line the error occurred. Optional, pass NULL if not needed.
 * \param[out] error_pos     The column the error occurred if error_line is not NULL, otherwise the position
 *                           in the stream the error occurred. Optional, pass NULL if not needed.
 *
 * \return The root JSON node of the parsed data, or NULL on error.
 */
M_API M_json_node_t *M_json_read(const char *data, size_t data_len, M_uint32 flags, size_t *processed_len, M_json_error_t *error, size_t *error_line, size_t *error_pos) M_MALLOC;


/*! Parse a file into a JSON object.
 *
 * \param[in]  path       The file to read.
 * \param[in]  flags      M_json_reader_flags_t flags to control the behavior of the reader.
 * \param[in]  max_read   The maximum amount of data to read from the file. If the data in the file is
 *                        larger than max_read an error will most likely result. Optional pass 0 to read all data.
 * \param[out] error      On error this will be populated with an error reason. Optional, pass NULL if not needed.
 * \param[out] error_line The line the error occurred. Optional, pass NULL if not needed.
 * \param[out] error_pos  The column the error occurred if error_line is not NULL, otherwise the position
 *                        in the stream the error occurred. Optional, pass NULL if not needed.
 * \return The root JSON node of the parsed data, or NULL on error.
 */
M_API M_json_node_t *M_json_read_file(const char *path, M_uint32 flags, size_t max_read, M_json_error_t *error, size_t *error_line, size_t *error_pos) M_MALLOC;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Write JSON to a string.
 *
 * This writes nodes to a string. The string may not be directly usable by M_json_read.
 * E.g. If you are only writing a string node.
 *
 * \param[in]  node  The node to write. This will write the node and any nodes under it. 
 * \param[in]  flags M_json_writer_flags_t flags to control writing.
 * \param[out] len   The length of the string that was returned. Optional, pass NULL if not needed.
 *
 * \return A string with data or NULL on error.
 */
M_API char *M_json_write(const M_json_node_t *node, M_uint32 flags, size_t *len) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Write JSON to a file.
 *
 * This writes nodes to a string. The string may not be directly usable by M_json_read_file (for example)
 * if you are only writing a string node (for example).
 *
 * \param[in] node  The node to write. This will write the node and any nodes under it. 
 * \param[in] path  The filename and path to write the data to.
 * \param[in] flags M_json_writer_flags_t flags to control writing.
 *
 * \return Result.
 */
M_API M_fs_error_t M_json_write_file(const M_json_node_t *node, const char *path, M_uint32 flags);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a JSON error code to a string.
 *
 * \param[in] err error code
 * \return        short string describing error code
 */
M_API const char *M_json_error_to_string(M_json_error_t err);


/*! Get the type of node.
 *
 * \param[in] node The node.
 *
 * \return The type.
 */
M_API M_json_type_t M_json_node_type(const M_json_node_t *node);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Using JSONPath expressions, scan for matches.
 *
 * Note: that full JSONPath support does not yet exist.
 *
 * Search expressions must start with $. They can use . to refer to the first element
 * or .. to search for the first matching element.
 *
 * Supports:
 *  - Patterns containing ".", "*", "..". 
 *  - Array offsets using [*]/[]/[,]/[start:end:step].
 *    - Positive offsets [0], [0,2].
 *    - Negative offsets [-1] (last item). [-2] (second to last item).
 *    - Positive and negative steps. [0:4:2]. [4:0:-1].
 *      - When counting up start is inclusive and end is exclusive. [0:3] is equivalent to [0,1,2].
 *      - When counting down start is exclusive and end is inclusive. [3:0:-1] is equivalent to [2,1,0].
 *
 * Does not Support:
 *  - Braket notation ['x'].
 *  - Filter/script expressions. [?(exp)]/[(exp)].
 *
 * \param[in]  node        The node.
 * \param[in]  search      search expression
 * \param[out] num_matches Number of matches found
 *
 * \return array of M_json_node_t pointers on success (must free array, but not internal pointers), NULL on failure
 *
 * \see M_free
 */ 
M_API M_json_node_t **M_json_jsonpath(const M_json_node_t *node, const char *search, size_t *num_matches) M_MALLOC;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the parent node of a given node.
 *
 * \param[in] node The node.
 *
 * \return The parent node or NULL if there is no parent.
 */
M_API M_json_node_t *M_json_get_parent(const M_json_node_t *node);


/*! Take the node from the parent but does not destroy it.
 *
 * This allows a node to be moved between different parents.
 *
 * \param[in,out] node The node.
 */
M_API void M_json_take_from_parent(M_json_node_t *node);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the value of an object node for a given key.
 *
 * The object still owns the returned node. You can use M_json_take_from_parent to remove the ownership.
 * At which point you will need to either insert it into another object/array or destroy it.
 *
 * \param[in] node The node.
 * \param[in] key The key.
 *
 * \return The node under key. Otherwise NULL if the key does not exist.
 */
M_API M_json_node_t *M_json_object_value(const M_json_node_t *node, const char *key);


/*! Get the string value of an object node for a given key.
 *
 * \param[in] node The node.
 * \param[in] key The key.
 *
 * \return The string value under the key. NULL if not a string or key does not exist.
 */
M_API const char *M_json_object_value_string(const M_json_node_t *node, const char *key);


/*! Get the integer value of an object node for a given key.
 *
 * If the node is not an M_JSON_TYPE_INTEGER auto conversion will be attempted.
 *
 * \param[in] node The node.
 * \param[in] key The key.
 *
 * \return The value. 0 on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 */
M_API M_int64 M_json_object_value_int(const M_json_node_t *node, const char *key);


/*! Get the decimal value of an object node for a given key.
 *
 * \param[in] node The node.
 * \param[in] key The key.
 *
 * \return The string value under the key. NULL if not a decimal or key does not exist.
 */
M_API const M_decimal_t *M_json_object_value_decimal(const M_json_node_t *node, const char *key);


/*! Get the bool value of an object node for a given key.
 *
 * If the node is not a M_JSON_TYPE_BOOL auto conversion will be attempted.
 *
 * \param[in] node The node.
 * \param[in] key The key.
 *
 * \return The value. M_FALSE on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 */
M_API M_bool M_json_object_value_bool(const M_json_node_t *node, const char *key);


/*! Get a list of all keys for the object.
 *
 * \param[in]  node The node.
 *
 * \return A list of keys.
 */
M_API M_list_str_t *M_json_object_keys(const M_json_node_t *node);


/*! Insert a node into the object.
 *
 * The object node will take ownership of the value node.
 *
 * \param[in,out] node  The node.
 * \param[in]     key   The key. If the key already exists the existing node will be destroyed and replaced with the
 *                      new value node.
 * \param[in]     value The node to add to the object.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_json_object_insert(M_json_node_t *node, const char *key, M_json_node_t *value);


/*! Insert a string into the object.
 *
 * \param[in,out] node  The node.
 * \param[in]     key   The key. If the key already exists the existing node will be destroyed and replaced with the
 *                      new value node.
 * \param[in]     value The string to add to the object.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_json_object_insert_string(M_json_node_t *node, const char *key, const char *value);


/*! Insert an integer into the object.
 *
 * \param[in,out] node  The node.
 * \param[in]     key   The key. If the key already exists the existing node will be destroyed and replaced with the
 *                      new value node.
 * \param[in]     value The integer to add to the object.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_json_object_insert_int(M_json_node_t *node, const char *key, M_int64 value);


/*! Insert an decimal into the object.
 *
 * \param[in,out] node  The node.
 * \param[in]     key   The key. If the key already exists the existing node will be destroyed and replaced with the
 *                      new value node.
 * \param[in]     value The decimal to add to the object.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_json_object_insert_decimal(M_json_node_t *node, const char *key, const M_decimal_t *value);


/*! Insert an bool into the object.
 *
 * \param[in,out] node  The node.
 * \param[in]     key   The key. If the key already exists the existing node will be destroyed and replaced with the
 *                      new value node.
 * \param[in]     value The bool to add to the object.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_json_object_insert_bool(M_json_node_t *node, const char *key, M_bool value);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of items in an array node.
 *
 * \param[in] node The node.
 *
 * \return The number of items in the array.
 */
M_API size_t M_json_array_len(const M_json_node_t *node);


/*! Get the item in the array at a given index.
 *
 * The array still owns the returned node. You can use M_json_take_from_parent to remove the ownership.
 * At which point you will need to either insert it into another object/array or destroy it.
 *
 * \param[in] node The node.
 * \param[in] idx  The index.
 *
 * \return The node at the given index or NULL if the index is invalid.
 */
M_API M_json_node_t *M_json_array_at(const M_json_node_t *node, size_t idx);


/*! Get the string value of given index in an array.
 *
 * \param[in] node The node.
 * \param[in] idx  The index.
 *
 * \return The string value at the location. NULL if not a string or key does not exist.
 */
M_API const char *M_json_array_at_string(const M_json_node_t *node, size_t idx);


/*! Get the integer value of given index in an array.
 *
 * If the node is not an M_JSON_TYPE_INTEGER auto conversion will be attempted.
 *
 * \param[in] node The node.
 * \param[in] idx  The index.
 *
 * \return The value. 0 on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 */
M_API M_int64 M_json_array_at_int(const M_json_node_t *node, size_t idx);


/*! Get the decimal value of given index in an array.
 *
 * \param[in] node The node.
 * \param[in] idx  The index.
 *
 * \return The string value under the key. NULL if not a decimal or index does not exist.
 */
M_API const M_decimal_t *M_json_array_at_decimal(const M_json_node_t *node, size_t idx);


/*! Get the string value of given index in an array.
 *
 * If the node is not a M_JSON_TYPE_BOOL auto conversion will be attempted.
 *
 * \param[in] node The node.
 * \param[in] idx  The index.
 *
 * \return The value. M_FALSE on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 */
M_API M_bool M_json_array_at_bool(const M_json_node_t *node, size_t idx);


/*! Append a node into an array node.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value node to append.
 *
 * \return M_TRUE if the value was appended otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert(M_json_node_t *node, M_json_node_t *value);


/*! Append a string into an array node.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 *
 * \return M_TRUE if the value was appended otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_string(M_json_node_t *node, const char *value);


/*! Append a integer into an array node.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 *
 * \return M_TRUE if the value was appended otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_int(M_json_node_t *node, M_int64 value);


/*! Append a decimal into an array node.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 *
 * \return M_TRUE if the value was appended otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_decimal(M_json_node_t *node, const M_decimal_t *value);


/*! Append a bool into an array node.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 *
 * \return M_TRUE if the value was appended otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_bool(M_json_node_t *node, M_bool value);


/*! Insert a node into an array node at a given index.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value node to append.
 * \param[in]     idx   The index to insert at.
 *
 * \return M_TRUE if the value was inserted otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_at(M_json_node_t *node, M_json_node_t *value, size_t idx);


/*! Insert a string into an array node at a given index.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 * \param[in]     idx   The index to insert at.
 *
 * \return M_TRUE if the value was inserted otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_at_string(M_json_node_t *node, const char *value, size_t idx);


/*! Insert a integer into an array node at a given index.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 * \param[in]     idx   The index to insert at.
 *
 * \return M_TRUE if the value was inserted otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_at_int(M_json_node_t *node, M_int64 value, size_t idx);


/*! Insert a decimal into an array node at a given index.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 * \param[in]     idx   The index to insert at.
 *
 * \return M_TRUE if the value was inserted otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_at_decimal(M_json_node_t *node, const M_decimal_t *value, size_t idx);


/*! Insert a bool into an array node at a given index.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to append.
 * \param[in]     idx   The index to insert at.
 *
 * \return M_TRUE if the value was inserted otherwise M_FALSE.
 */ 
M_API M_bool M_json_array_insert_at_bool(M_json_node_t *node, M_bool value, size_t idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the value from a string node.
 *
 * \param[in] node The node.
 *
 * \return The value.
 */
M_API const char *M_json_get_string(const M_json_node_t *node);


/*! Make the node a string node and set the value.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value to set.
 *
 * \return M_TRUE if the node updated.
 */
M_API M_bool M_json_set_string(M_json_node_t *node, const char *value);


/*! Get the value from an integer node.
 *
 * If the node is not an M_JSON_TYPE_INTEGER auto conversion will be attempted.
 *
 * \param[in] node The node.
 *
 * \return The value. 0 on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 */
M_API M_int64 M_json_get_int(const M_json_node_t *node);


/*! Make the node a integer node and set the value.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value.
 *
 * \return M_TRUE if the node updated.
 */
M_API M_bool M_json_set_int(M_json_node_t *node, M_int64 value);


/*! Get the value from a decimal node.
 *
 * \param[in] node The node.
 *
 * \return The value.
 */
M_API const M_decimal_t *M_json_get_decimal(const M_json_node_t *node);


/*! Make the node a decimal node and set the value.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value.
 *
 * \return M_TRUE if the node updated.
 */
M_API M_bool M_json_set_decimal(M_json_node_t *node, const M_decimal_t *value);


/*! Get the value from a bool node.
 *
 * If the node is not a M_JSON_TYPE_BOOL auto conversion will be attempted.
 *
 * \param[in] node The node.
 *
 * \return The value. M_FALSE on error. The only way to know if there was an error
 *         or the return is the value is to check the type.
 *
 * \see M_json_node_type
 */
M_API M_bool M_json_get_bool(const M_json_node_t *node);


/*! Make the node a bool node and set the value.
 *
 * \param[in,out] node  The node.
 * \param[in]     value The value.
 *
 * \return M_TRUE if the node updated.
 */
M_API M_bool M_json_set_bool(M_json_node_t *node, M_bool value);


/*! Make the node a null node.
 *
 * \param[in] node The node.
 *
 * \return M_TRUE if the node updated.
 */
M_API M_bool M_json_set_null(M_json_node_t *node);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the node value as a string.
 *
 * This will only work on value type nodes (string, integer, decimal, book, null).
 * Other node types (object, array) will fail.
 *
 * \param[in]  node    The node.
 * \param[out] buf     An allocated buffer to write the value as a string to. The result will be null terminated
 *                     on success.
 * \param[in]  buf_len The length of the buffer.
 *
 * \return M_TRUE on success. Otherwise M_FALSE.
 */
M_API M_bool M_json_get_value(const M_json_node_t *node, char *buf, size_t buf_len);


/*! Get the node value as a string.
 *
 * This will only work on value type nodes (string, integer, decimal, book, null).
 * Other node types (object, array) will fail.
 *
 * \param[in] node The node.
 *
 * \return The value or NULL on error.
 */
M_API char *M_json_get_value_dup(const M_json_node_t *node);

/*! @} */

__END_DECLS

#endif /* __M_JSON_H__ */
