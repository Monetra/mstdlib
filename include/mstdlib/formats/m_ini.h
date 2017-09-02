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

#ifndef __M_INI_H__
#define __M_INI_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_fs.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_ini INI
 *  \ingroup m_formats
 *
 * Configurable handling for various formats. Such as # vs ; comment identifiers.
 *
 * For easier access functions that do not take a section use the key form
 * 'section/key'. If multiple '/' characters are in the combined key the section
 * is only up until the first '/'. Meaning: 'section/key/key_part'
 *
 * Can handle multiple or single values under a single key.
 *
 * Supports:
 *   - Read
 *   - Write
 *   - Modify
 *   - Merge
 *
 * Example:
 *
 * \code{.c}
 *     M_ini_t          *ini   = NULL;
 *     M_ini_settings_t *info  = NULL;
 *     char             *out;
 *     size_t            errln = 0;
 *
 *     info = M_ini_settings_create();
 *     M_ini_settings_set_quote_char(info, '"');
 *     M_ini_settings_set_escape_char(info, '"');
 *     M_ini_settings_set_padding(info, M_INI_PADDING_AFTER_COMMENT_CHAR);
 *     M_ini_settings_reader_set_dupkvs_handling(info, M_INI_DUPKVS_REMOVE);
 *     M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_USE_LAST);
 *
 *     ini = M_ini_read_file("file.ini", info, M_TRUE, &errln, 0);
 *     if (ini == NULL) {
 *         M_printf("ini could not be parsed. Error line: %zu\n", errln);
 *         return M_FALSE;
 *     }
 *
 *     M_ini_kv_set(ini, "s1/key1", "yes");
 *
 *     out = M_ini_write(ini, info);
 *     M_printf("new ini=\n%s\n", out);
 *     M_free(out);
 *     
 *     M_ini_destroy(ini);
 *     M_ini_settings_destroy(info);
 * \endcode
 *
 * @{
 */

struct M_ini;
typedef struct M_ini M_ini_t;

struct M_ini_settings;
typedef struct M_ini_settings M_ini_settings_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Conflict handler function prototype.
 *
 * Ini merging can have conflicts resolved using a callback function. The use of the
 * resolution callback is dependent on the appropriate merge flag being set.
 *
 * \param[in] key     The key. If key is NULL then the values are they key. In this case if the value is NULL then
 *                    the key doesn't exist for that location.
 * \param[in] val_cur The value in the current ini.
 * \param[in] val_new The new value.
 *
 * \return M_TRUE if the current value should be used. Otherwise, M_FALSE if the new value should be used.
 */
typedef M_bool (*M_ini_merge_resolver_t)(const char *key, const char *val_cur, const char *val_new);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate key, value pair handling where a key is encountered multiple times. */
typedef enum {
	M_INI_DUPKVS_COMMENT_PREV = 0, /*!< Turn previous kv into comments. Last wins.*/
	M_INI_DUPKVS_REMOVE_PREV,      /*!< Remove previous kv from the tree. Last wins. */
	M_INI_DUPKVS_COMMENT,          /*!< Turn the current kv into a comment. First wins. */
	M_INI_DUPKVS_REMOVE,           /*!< Remove the current kv from the tree. First wins. */
	M_INI_DUPKVS_COLLECT           /*!< Multiple kv are allowed and their values should be collected. All win. */
} M_ini_dupkvs_t;


/*! Control padding when between parts of elements.
 * Primarily used for writing but also used for reading when a comment duplicate key flag is used. */
typedef enum {
	M_INI_PADDING_NONE               = 0,      /*!< No padding. */
	M_INI_PADDING_BEFORE_KV_DELIM    = 1 << 0, /*!< Put a space before the kv delimiter. */
	M_INI_PADDING_AFTER_KV_DELIM     = 1 << 1, /*!< Put a space after the kv delimiter. */
	M_INI_PADDING_AFTER_KV_VAL       = 1 << 2, /*!< Put a space after the kv val if followed by a comment. */
	M_INI_PADDING_AFTER_COMMENT_CHAR = 1 << 3  /*!< Put a space after the comment character. */
} M_ini_padding_t;


/*! Control how muli value keys are written. */
typedef enum {
	M_INI_MULTIVALS_USE_LAST = 0,    /*!< Multi-value keys are not supported. Use the last value. */
	M_INI_MULTIVALS_USE_FIRST,       /*!< Multi-value keys are not supported. Use the first value. */
	M_INI_MULTIVALS_KEEP_EXISTING,   /*!< Multi-value keys are supported. Keep existing values in the same location
	                                      and place new values after. */
	M_INI_MULTIVALS_MAINTAIN_ORDER   /*!< Multi-value keys are supported. Remove all existing keys and write them all
	                                      together maintaining the current value order. */
} M_ini_multivals_t;


/*! Control how conflicts are handled during merge.
 *
 * These values all override the default behavior.
 * Default behavior:
 *   - When a key is in new but not in cur and orig remove the key.
 *   - When the value (single) of cur is the same as orig but different than new use the new value. 
 *   - When a key with multiple values has a value that is in cur and orig but not in new remove the value.
 */
typedef enum {
	M_INI_MERGE_CALLBACK_FUNC          = 0,      /*!< Use a conflict resolution callback function to determine how to
	                                                  handle conflicts. A callback function must be set otherwise
	                                                  the default handling will be used. */
	M_INI_MERGE_NEW_REMOVED_KEEP       = 1 << 0, /*!< When a key is not in new but in cur and orig keep the key.
	                                                  The default is to remove the key. */
	M_INI_MERGE_NEW_CHANGED_USE_CUR    = 1 << 1, /*!< When the value of cur is the same as orig but different than
	                                                  new use the value from cur. Meaning the default value is set
	                                                  but has changed. The default is to use the new value. */
	M_INI_MERGE_MULTI_NEW_REMOVED_KEEP = 1 << 3  /*!< When a key with multiple values has a value that is in cur
	                                                  and orig but not in new keep the value. The default is to remove
													  the value. */
} M_ini_merge_conflict_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Helpers
 */

/*! Create a full key from individual parts.
 *
 * \param[in] section The section the key belongs to. Can be NULL if referencing a key not in a section.
 * \param[in] key     The key within the section. Can be NULL if referencing a section only.
 *
 * \return A string with the full key.
 */
M_API char *M_ini_full_key(const char *section, const char *key) M_MALLOC;



/*! Split a full key into it's individual parts.
 *
 * \param[in]  s       The full key.
 * \param[out] section The section part. Optional, pass NULL if not needed. May be returned as NULL.
 * \param[out] key     The key part. Optional, pass NULL if not needed. May be returned as NULL.
 */
M_API void M_ini_split_key(const char *s, char **section, char **key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Settings
 *
 * The settings object controls how an ini object is read, written, or merged. It controls
 * aspects such as what characters are delimiters. How to handle certain situations
 * that may arise.
 *
 * The same settings object can be used for read, write, and merge. A settings object augments
 * but it not tied to a specific ini object or ini operation.
 */

/*! Create an ini settings object.
 *
 * \return an ini settings object.
 */
M_API M_ini_settings_t *M_ini_settings_create(void) M_MALLOC;


/*! Destroy an ini settings object.
 *
 * \param[in] info The settings object to destroy.
 */
M_API void M_ini_settings_destroy(M_ini_settings_t *info) M_FREE(1);


/*! Get the element delimiter character.
 *
 * \param[in] info The settings.
 *
 * \return The element delimiter character. Default is "\n".
 */
M_API unsigned char M_ini_settings_get_element_delim_char(const M_ini_settings_t *info);


/*! Get the quote character.
 *
 * \param[in] info The settings.
 *
 * \return The quote character. 0 if not set.
 */
M_API unsigned char M_ini_settings_get_quote_char(const M_ini_settings_t *info);


/*! Get the quoting escape character.
 *
 * This can be the same character as the quote character which suggests CSV style quoting.
 *
 * \param[in] info The settings.
 *
 * \return The escape character. 0 if not set.
 */
M_API unsigned char M_ini_settings_get_escape_char(const M_ini_settings_t *info);


/*! Get the comment character.
 *
 * \param[in] info The settings.
 *
 * \return The comment character. Default is "#".
 */
M_API unsigned char M_ini_settings_get_comment_char(const M_ini_settings_t *info);


/*! Get the key, value delimiter character.
 *
 * \param[in] info The settings.
 *
 * \return The key, value delimiter character. Default is "=".
 */
M_API unsigned char M_ini_settings_get_kv_delim_char(const M_ini_settings_t *info);


/*! Get the padding flags.
 *
 * \param[in] info The settings.
 *
 * \return The padding flags.
 */
M_API M_uint32 M_ini_settings_get_padding(const M_ini_settings_t *info);


/*! Get the duplicate key handling value used during reading.
 *
 * \param[in] info The settings.
 *
 * \return The duplicate key handling value.
 */
M_API M_ini_dupkvs_t M_ini_settings_reader_get_dupkvs_handling(const M_ini_settings_t *info);


/*! Get the multiple value handling value used during writing.
 *
 * \param[in] info The settings.
 *
 * \return The multiple value handing value.
 */
M_API M_ini_multivals_t M_ini_settings_writer_get_multivals_handling(const M_ini_settings_t *info);


/*! Get the line ending used when writing the ini. 
 *
 * This is to allow multiple character line endings (Windows "\r\n"). This is an override of the
 * element delim character that will be used if set. The line ending string will not be used
 * when determining if quoting is necessary. The element delim is still used for this purpose even
 * when the line ending is set.
 *
 * \param[in] info The settings.
 *
 * \return The line ending characters.
 */
M_API const char *M_ini_settings_writer_get_line_ending(const M_ini_settings_t *info);


/*! Get the conflict resolution flags used for merging.
 *
 * \param[in] info The settings.
 *
 * \return The conflict resolution flags. If 0 then either the default handing is going to be used or
 *         a custom resolution callback has been registered. Check if the call back is not NULL to
 *         know if the default handling will be used.
 */
M_API M_uint32 M_ini_settings_merger_get_conflict_flags(const M_ini_settings_t *info);

/*! Get the conflict resolution function used for merging when the conflict flags are set to use
 * a custom resolution callback.
 *
 * \param[in] info The settings.
 *
 * \return the resolution function. NULL if not set.
 */
M_API M_ini_merge_resolver_t M_ini_settings_merger_get_resolver(const M_ini_settings_t *info);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set the element delimiter character.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_element_delim_char(M_ini_settings_t *info, unsigned char val);


/*! Set the quote character.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_quote_char(M_ini_settings_t *info, unsigned char val);


/*! Set the escape character.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_escape_char(M_ini_settings_t *info, unsigned char val);


/*! Set the comment character.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_comment_char(M_ini_settings_t *info, unsigned char val);


/*! Set the key value delimiter character.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_kv_delim_char(M_ini_settings_t *info, unsigned char val);


/*! Set the padding flags.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_set_padding(M_ini_settings_t *info, M_uint32 val);


/*! Set the duplicate key flags used for reading.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_reader_set_dupkvs_handling(M_ini_settings_t *info, M_ini_dupkvs_t val);


/*! Set the multiple value handling flags used for writing..
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_writer_set_multivals_handling(M_ini_settings_t *info, M_ini_multivals_t val);


/*! Set the line ending used when writing the ini. 
 *
 * This is to allow multiple character line endings (Windows "\r\n"). This is an override of the
 * element delim character that will be used if set. The line ending string will not be used
 * when determining if quoting is necessary. The element delim is still used for this purpose even
 * when the line ending is set.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_writer_set_line_ending(M_ini_settings_t *info, const char *val);


/*! Set the conflict resolution flags used for merging.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_merger_set_conflict_flags(M_ini_settings_t *info, M_uint32 val);


/*! Set the conflict resolution function.
 *
 * \param[in,out] info The settings.
 * \param[in]     val  The value to set.
 */
M_API void M_ini_settings_merger_set_resolver(M_ini_settings_t *info, M_ini_merge_resolver_t val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Common
 */

/*! Create a new ini object.
 *
 * \param[in] ignore_whitespace Should whitespace be ignored when comparing section and key names.
 *
 * \return A new ini object.
 */
M_API M_ini_t *M_ini_create(M_bool ignore_whitespace) M_MALLOC;


/*! Duplicate an ini.
 *
 * \param[in] ini The ini to duplicate
 */
M_API M_ini_t *M_ini_duplicate(const M_ini_t *ini);


/*! Destroy the ini.
 *
 * \param[in] ini The ini to destroy.
 */
M_API void M_ini_destroy(M_ini_t *ini) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Read
 */

/*! Parse a string into an ini object.
 *
 * \param[in]  s                 The string to parse.
 * \param[in]  info              The ini settings that control how the ini is structured and should be read.
 * \param[in]  ignore_whitespace Should whitespace be ignored for section and key comparison.
 * \param[out] err_line          If an error occurs the line the error is present on.
 *                               Optional, pass NULL if not needed.
 *
 * \return An ini object. NULL if data could not be parsed.
 */
M_API M_ini_t *M_ini_read(const char *s, const M_ini_settings_t *info, M_bool ignore_whitespace, size_t *err_line) M_MALLOC;


/*! Read a file based on file name into an ini object.
 *
 * \param[in]  path              The full file path to read.
 * \param[in]  info              The ini settings that control how the ini is structured and should be read.
 * \param[in]  ignore_whitespace Should whitespace be ignored for section and key comparison.
 * \param[out] err_line          If an error occurs the line the error is present on.
 *                               Optional, pass NULL if not needed.
 * \param[in]  max_read          The maximum number of bytes to read.
 *
 * \return An ini object. NULL if the file could not be parsed.
 */
M_API M_ini_t *M_ini_read_file(const char *path, const M_ini_settings_t *info, M_bool ignore_whitespace, size_t *err_line, size_t max_read) M_MALLOC;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Manipulate
 *
 * The ini stores keys in a flattened format. To differentiate keys with the same name in different
 * sections the section is prepended to the key. E.g. "Section 1/key 1". All keys passed into a
 * manipulate function must use this format. All keys returned by a manipulate function will use
 * this format. Use The M_ini_full_key and M_ini_split_key functions to aid with the use of
 * these functions.
 */

/*! Does the ini contain a given key.
 *
 * \param[in] ini The ini.
 * \param[in] key The key to check.
 *
 * \return M_TRUE if the ini contains the key otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_has_key(const M_ini_t *ini, const char *key);


/*! Get a list of all keys contained in the ini.
 *
 * \param[in] ini The ini.
 *
 * \return A list of keys contained in the ini.
 */
M_API M_list_str_t *M_ini_kv_keys(const M_ini_t *ini) M_MALLOC;


/*! Get a list of sections contained in the ini.
 *
 * \param[in] ini The ini.
 *
 * \return A list of sections contained in the ini.
 */
M_API M_list_str_t *M_ini_kv_sections(const M_ini_t *ini) M_MALLOC;


/*! Rename a section or key in the ini.
 *
 * Renaming a section can move all keys under it. Renaming a key will move it to the new location if the
 * section portion is different.
 *
 * Renaming will fail if the new name already exists. This applies to sections and keys.
 *
 * This can also be used to rename the "pretty name" for a section or key when ignore white space is in use.
 * Or when the case needs to be changed.
 * E.g. "section_1" -> "Section 1". These are equivalent when ignore whitespace is enabled and renaming
 * will simply change the pretty name.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The section or key to rename.
 * \param[in]     new_key The new name.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_rename(M_ini_t *ini, const char *key, const char *new_key);


/*! Add a key (without value) to the ini.
 *
 * \param[in,out] ini The ini.
 * \param         key The key to add.
 *
 * \return M_TRUE if added otherwise M_FALSE.
 */ 
M_API M_bool M_ini_kv_add_key(M_ini_t *ini, const char *key);


/*! Set the value for the key to this value only.
 *
 * This will clear/replace any other values (even multiple) for the key.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The key.
 * \param[in]     val The vaule.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_set(M_ini_t *ini, const char *key, const char *val);


/*! Insert the value into the values for key.
 *
 * This does not remove/replace the existing values for the key.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The key.
 * \param[in]     val The vaule.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_insert(M_ini_t *ini, const char *key, const char *val);


/*! Remove the key from the ini.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The key.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_remove(M_ini_t *ini, const char *key);


/*! Remove all values for a key but leave the key as part of the ini.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The key.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_remove_vals(M_ini_t *ini, const char *key);


/*! Remove a specific value from the key.
 *
 * \param[in,out] ini The ini.
 * \param[in]     key The key.
 * \param[in]     idx The index of the value to remove.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_remove_val_at(M_ini_t *ini, const char *key, size_t idx);


/*! Get the number of values for a given key.
 *
 * \param[in] ini The ini.
 * \param[in] key The key.
 *
 * \return The number of values for a given key.
 */
M_API size_t M_ini_kv_len(const M_ini_t *ini, const char *key);


/*! Get the value at the given index for the key.
 *
 * \param[in]  ini The ini.
 * \param[in]  key The key.
 * \param[in]  idx The index of the value to get.
 * \param[out] val The value. Can be NULL to check for value existence. Use M_ini_kv_has_key to determine key
 *                 existence because a key can be part of of the ini and not have a value.
 *
 * \return M_TRUE if the value can be retrieved. Otherwise M_FALSE.
 */
M_API M_bool M_ini_kv_get(const M_ini_t *ini, const char *key, size_t idx, const char **val);


/*! Get the value at the given index for the key.
 *
 * \param[in] ini The ini.
 * \param[in] key The key.
 * \param[in] idx The index of the value to get.
 *
 * \return The value.
 */
M_API const char *M_ini_kv_get_direct(const M_ini_t *ini, const char *key, size_t idx);


/*! Get all values for the key.
 *
 * \param[in] ini The ini.
 * \param[in] key The key.
 *
 * \return A string list of values or NULL.
 */
M_API M_list_str_t *M_ini_kv_get_vals(const M_ini_t *ini, const char *key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Write
 */

/*! Write the ini to a string.
 *
 * \param[in,out] ini  The ini.
 * \param[in]     info Settings controlling how the ini should be written.
 *
 * \return The ini as a string.
 */
M_API char *M_ini_write(M_ini_t *ini, const M_ini_settings_t *info);


/*! Write the ini directly to a file.
 *
 * \param[in,out] ini The ini.
 * \param[in]     path The file path to write the ini to. This will overwrite the data in the file at path if path is
 *                an existing file.
 * \param[in]     info Settings controlling how the ini should be written.
 *
 * \return Result.
 */
M_API M_fs_error_t M_ini_write_file(M_ini_t *ini, const char *path, const M_ini_settings_t *info);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Merge
 */

/*! Merge a new ini into an existing ini.
 *
 * The merge processes is similar to a three way diff. The current values are compared to the values
 * in new and the original. The merge process is:
 *
 * 1. Update keys.
 *    a. Only in new = in merged.
 *    b. Only in cur = in merged.
 *    c. In cur and new but not in orig = in merged.
 *    d. In orig and cur but not in new = flag handling (default: not in merged).
 *    e. In orig and new but not in cur = flag handling (default: not in merged).
 *    f. in cur, new and orig = in merged.
 * 2. Update vals.
 *    a. Cur and orig the same but new different = flag handling (default: use new).
 *    b. Cur and new the same but orig different = use cur/new.
 *    c. New and orig the same but cur different = use cur
 *    d. All there are the same = use cur/new/orig.
 * 3. Update multi-vals.
 *    a. In cur and new = use cur/new.
 *    b. Only in cur = use cur.
 *    c. In cur and orig but not in new = flag (default remove).
 *    d. In new but not in cur or orig = use new.
 *
 * \param[in] cur_ini  The current ini. Contains user changes that differ from the original ini.
 * \param[in] new_ini  The new ini.
 * \param[in] orig_ini The original ini that cur_ini is based on.
 * \param[in] info     Settings controlling how the ini should be merged.
 */
M_API M_ini_t *M_ini_merge(const M_ini_t *cur_ini, const M_ini_t *new_ini, const M_ini_t *orig_ini, const M_ini_settings_t *info);

/*! @} */

__END_DECLS

#endif /* __M_INI_H__ */
