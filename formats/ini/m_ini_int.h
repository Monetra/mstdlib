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

#ifndef __M_INI_INT_H__
#define __M_INI_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/* Internal functions for working with ini objects */

/*! Represents an element within an ini. */
struct M_ini_element;
typedef struct M_ini_element M_ini_element_t;

/*! Represents a list of elements. This is a thin wrapper around M_list_t to provide
 * some level of type safety. The list will take ownership of elements it holds.
 * The list uses insertion ordering. */
struct M_ini_elements;
/*! Opaque type.  Currently a direct map to M_ini_elements private opaque type,
 *  simply using casting to prevent the 'wrap' overhead of mallocing when it
 *  is not necessary */
typedef struct M_ini_elements M_ini_elements_t;

/*! A multi-value ordered hashtable used for storing ini key, value pairs.
 * The kv store holds a flattened list of keys and their values that are part
 * of an ini.
 */
struct M_ini_kvs;
typedef struct M_ini_kvs M_ini_kvs_t;

struct M_ini_kvs_enum;
typedef struct M_ini_kvs_enum M_ini_kvs_enum_t;

/*! An ini object.
 *
 * Maintains the structure and all key value pairs in the ini.
 *
 * Key formats:
 *   * minimal:       Only the key or section name is stored.
 *
 *   * internal:      The internal format is primarily used to lookup. It stores the key or name as is but with
 *                    white space removed if white space is being ignored.
 *
 *   * full:          The full format is primarily used for flat storage. It is the form "section/key" or "section/"
 *                    or "key". It is to allow keys from all sections to be used in a flat list.
 *
 *   * full internal: The full internal format is a combination of the internal and full format.
 *
 *
 * The ini is comprised of several parts:
 *   * elements:  When combined with sections this mimics a tree. The "tree" is used to maintain proper order, maintain
 *                comments and general formatting when writing. While data is stored in the tree it is not considered
 *                the definitive data store.
 *
 *                This is currently a list of elements.  This should be thought of as the top or root level of the ini.
 *                It can have arbitrary elements but in most cases will be a list of sections. Sections will always
 *                follow all non-section elements.
 *
 *                Element names/keys are stored in internal format.
 *
 *   * sections:  This is a hash table where the key is the section name and the value is a list of element
 *                that are part of the section. The section name corresponds to the section elements in
 *                the elements list.
 *
 *                Section elements can never be within a section list. Section elements are only allowed
 *                in the top level elements list.
 *
 *                Reading the elements list and then reading the section's elements list when a section
 *                is encountered follows the ini format.
 *
 *                Section keys are stored in internal format.
 *
 *   * kvs:       When an element is updated, added or removed due to manipulation of the ini this is what gets
 *                updated. The pseudo tree is only updated upon writing. Instead changes are handled here. The kvs uses
 *                a flat set of keys for the values. Meaning the keys in the kvs are stored in the form: "section/key"
 *                or "key" for keys in the root level.
 *
 *                A key can have multiple values so kvs values are a list of values.
 *
 *                keys are stored using the full internal format.
 *
 *   *key_lookup: Translate our full internal key name (whitespace ignore which is optional) into the pretty name that
 *                should be used for writing. The format key format is "section/key" or "section/" or "key". The value
 *                is "pretty_section" or "pretty_key".
 *
 *                keys are stored using the full internal format. Values are stored in minimal format.
 *
 *   *ignore_whitespace: Determines whether whitespace "([ -_\t])" should be ignored when comparing keys.
 */ 
struct M_ini {
	M_ini_elements_t *elements; /*!< A tree of elements used to maintain order when writing. */
	M_hash_strvp_t   *sections; /*!< Access to sections within the tree for fast access instead of having to
	                                 traverse the tree to find the section. */
	M_ini_kvs_t      *kvs;
	M_hash_dict_t    *key_lookup;
	M_bool            ignore_whitespace;
};

/*! Types of elements. */
typedef enum {
	M_INI_ELEMENT_TYPE_UNKNOWN = 0, /*!< Unknown. */
	M_INI_ELEMENT_TYPE_COMMENT,     /*!< Comment. */
	M_INI_ELEMENT_TYPE_EMPTY_LINE,  /*!< Empty line */
	M_INI_ELEMENT_TYPE_SECTION,     /*!< Section */
	M_INI_ELEMENT_TYPE_KV           /*!< Key and/or value and/or comment */
} M_ini_element_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Utility */

/*! Remove whitespace from a string.
 * Whitespace characters are ' ', '\t', '_', '-'. This is how MySQL's ini format defines
 * whitespace.
 * \param[in,out] s The string. The string is modified.
 * \return The string with whitespace removed.
 *         This is a convince to allow the following pattern:
 *         b = M_ini_delete_whitespace(M_strdup(a));
 */
M_API char *M_ini_delete_whitespace(char *s);

/*! Take a key and turn into into internal format.
 * This is a convince for handling whitespace ignore ini's.
 * \param s[in,out] The string. The string is modified.
 * \param ignore_whitespace Should whitespace be ignored.
 * \return The string converted to the internal form (whitespace removed if necessary).
 *         This is a convince to allow the following pattern:
 *         b = M_ini_internal_key(M_strdup(a));
 */
M_API char *M_ini_internal_key(char *s, M_bool ignore_whitespace);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Section handling.
 *
 * All internal because the public interface is based on they full keys. Sections won't be
 * added, deleted, or modified directly. Only the keys that define a section are will be
 * used publicly. We need these here to manipulate the sections when dealing with the
 * internal tree format for reading and writing. */

/*! Add a section to the ini.
 * \param ini The ini.
 * \param name The section name.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_section_insert(M_ini_t *ini, const char *name);

/*! Get a section in the ini.
 * \param ini The ini.
 * \param name The section name.
 * \param section The section data. Can be NULL if checking for existence.
 * \return True if the section was in the ini. Otherwise false.
 */
M_API M_bool M_ini_section_get(M_ini_t *ini, const char *name, M_ini_elements_t **section);

/*! Get the section in the ini directly.
 * \param ini The ini.
 * \param name The section name.
 * \return The section data.
 */
M_API M_ini_elements_t *M_ini_section_get_direct(M_ini_t *ini, const char *name);

/*! Remove a section from the ini
 * \param ini The ini.
 * \param name The section name.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_section_remove(M_ini_t *ini, const char *name);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Writer */

/*! Write a kv element to a buffer.
 * This is internal because it is used by the writer for writing the element and the reader for converting
 * a kv into a comment.
 * \param elem The element.
 * \param key The pretty key to use.
 * \param info The ini settings.
 * \param buf The buf to write the data to.
 */
M_API void M_ini_writer_write_element_kv(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Element */

/*! Create a new ini element.
 * \param type The type of element to be created.
 * \return An ini element.
 */
M_API M_ini_element_t *M_ini_element_create(M_ini_element_type_t type);

/*! Destroy the ini element.
 * \param elem The element.
 */
M_API void M_ini_element_destroy(M_ini_element_t *elem);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the element type.
 * \param elem The element.
 * \return The type.
 */
M_API M_ini_element_type_t M_ini_element_get_type(const M_ini_element_t *elem);

/*! Get the value for a comment element.
 * \param elem The element.
 * \return The value.
 */
M_API const char *M_ini_element_comment_get_val(const M_ini_element_t *elem);

/*! Get the name for a section element.
 * \param elem The element.
 * \return The value.
 */
M_API const char *M_ini_element_section_get_name(const M_ini_element_t *elem);

/*! Get the key for a kv element.
 * \param elem The element.
 * \return The key.
 */
M_API const char *M_ini_element_kv_get_key(const M_ini_element_t *elem);

/*! Get the value for a kv element.
 * \param elem The element.
 * \return The value.
 */
M_API const char *M_ini_element_kv_get_val(const M_ini_element_t *elem);

/*! Get the comment for a kv element.
 * \param elem The element.
 * \return The comment.
 */
M_API const char *M_ini_element_kv_get_comment(const M_ini_element_t *elem);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set the value for a comment element.
 * \param elem The element.
 * \param val The value to set.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_element_comment_set_val(M_ini_element_t *elem, const char *val);

/*! Set the name for a section element.
 * \param elem The element.
 * \param name The name to set.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_element_section_set_name(M_ini_element_t *elem, const char *name);

/*! Set the key for a kv element.
 * \param elem The element.
 * \param key The key to set.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_element_kv_set_key(M_ini_element_t *elem, const char *key);

/*! Set the val for a kv element.
 * \param elem The element.
 * \param val The val to set.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_element_kv_set_val(M_ini_element_t *elem, const char *val);

/*! Set the comment for a kv element.
 * \param elem The element.
 * \param comment The comment to set.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_element_kv_set_comment(M_ini_element_t *elem, const char *comment);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert a kv element into a comment element.
 * \param elem The element.
 * \param info The ini settings used to determine how the kv should be written (padding) as a comment.
 */
M_API void M_ini_element_kv_to_comment(M_ini_element_t *elem, const M_ini_settings_t *info);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate an element.
 * \param elem The element to duplicate.
 * \return A copy of the element.
 */
M_API M_ini_element_t *M_ini_element_duplicate(const M_ini_element_t *elem);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Elements */

/*! Create a new dynamic list of ini elements.
 * \return A list to hold ini elements.
 */
M_API M_ini_elements_t *M_ini_elements_create(void) M_MALLOC;

/*! Destroy a list of ini elements.
 * All elements in the list will be destroyed.
 * \param d The list.
 */
M_API void M_ini_elements_destroy(M_ini_elements_t *d) M_FREE(1);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert an ini element into the list.
 * \param d The list.
 * \param val The element to add.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_elements_insert(M_ini_elements_t *d, M_ini_element_t *val);

/*! Insert an ini element into the list at a specific index.
 * \param d The list.
 * \param val The element to add.
 * \param idx The index to insert the element at.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_elements_insert_at(M_ini_elements_t *d, M_ini_element_t *val, size_t idx);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! The number of ini elements in the list.
 * \param d The list.
 * \return The number of elements.
 */
M_API size_t M_ini_elements_len(const M_ini_elements_t *d);

/*! Get the element at a given location.
 * The element will remain a member of the list.
 * \param d The list.
 * \param idx The location to retrieve the element from.
 * \return The element or NULL if the index is out of range.
 */
M_API M_ini_element_t *M_ini_elements_at(const M_ini_elements_t *d, size_t idx);

/*! Take the element at a given index.
 * The element will be removed from the list and returned. The caller is
 * responsible for freeing the element if necessary.
 * \param[in] d The list.
 * \param[in] idx The location to retrieve the element from.
 * \return The element or NULL if index is out range.
 */
M_API M_ini_element_t *M_ini_elements_take_at(M_ini_elements_t *d, size_t idx);

/*! Remove an element at a given index from the list.
 * The value will at index will be freed. 
 * \param[in] d The list.
 * \param[in] idx The index to remove.
 * \return True if the idx was removed. Otherwise false.
 */
M_API M_bool M_ini_elements_remove_at(M_ini_elements_t *d, size_t idx);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Duplicate an existing list. Will copy all elements of the list.
 * \param d[in] list to duplicate.
 * \return new list.
 */
M_API M_ini_elements_t *M_ini_elements_duplicate(const M_ini_elements_t *d);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * kvs */

/*! Create a new ini kv store.
 * \return The kv store.
 */
M_API M_ini_kvs_t *M_ini_kvs_create(void) M_MALLOC;

/*! Destory an ini kv store
 * \param dict The store to destory.
 */
M_API void M_ini_kvs_destroy(M_ini_kvs_t *dict) M_FREE(1);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a list of keys contained in the kv store
 * \param dict The kv store.
 * \return A list of keys in the store.
 */
M_API M_list_str_t *M_ini_kvs_keys(M_ini_kvs_t *dict) M_MALLOC;

/*! Check if a key exists in the store
 * \param dict The kv store.
 * \param key The key.
 * \return True if the key exists in the store otherwise false.
 */
M_API M_bool M_ini_kvs_has_key(M_ini_kvs_t *dict, const char *key);

/*! Rename a key in the store.
 * Renaming will fail if the new name already exists.
 * \param dict The kv store.
 * \param key The section or key to rename.
 * \param new_key The new name.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_rename(M_ini_kvs_t *dict, const char *key, const char *new_key);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add a key (without any values) to the kvs.
 * \param dict The kv store.
 * \param key The key.
 * \return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_val_add_key(M_ini_kvs_t *dict, const char *key);

/*! Set the value for the key to this value only.
 * This will clear/replace any other values (even multiple) for the key.
 * \param dict The kv store.
 * \param key The key.
 * \param val The vaule.
 * \param return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_val_set(M_ini_kvs_t *dict, const char *key, const char *value);

/*! Insert the value into the values for key.
 * This does not remove/replace the existing values for the key.
 * \param dict The kv store.
 * \param key The key.
 * \param val The vaule.
 * \param return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_val_insert(M_ini_kvs_t *dict, const char *key, const char *value);

/*! Remove a specific value from the key.
 * \param dict The kv store.
 * \param key The key.
 * \param idx The index of the value to remove.
 * \param return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_val_remove_at(M_ini_kvs_t *dict, const char *key, size_t idx);

/*! Remove all values for a key but leave the key as part of the ini.
 * \param dict The kv store.
 * \param key The key.
 * \param return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_val_remove_all(M_ini_kvs_t *dict, const char *key);

/*! Get the number of values for a given key.
 * \param dict The kv store.
 * \param key The key.
 * \return The number of values for a given key.
 */
M_API size_t M_ini_kvs_val_len(M_ini_kvs_t *dict, const char *key);

/*! Get the value at the given index for the key.
 * \param dict The kv store.
 * \param key The key.
 * \param idx The index of the value to get.
 * \param[out] val The value. Can be NULL to check for value existence. Use M_ini_kv_has_key to determine key
 *                 existence because a key can be part of of the ini and not have a value.
 * \return true if the value can be retrieved. Otherwise false.
 */
M_API M_bool M_ini_kvs_val_get(M_ini_kvs_t *dict, const char *key, size_t idx, const char **value);

/*! Get the value at the given index for the key.
 * \param dict The kv store.
 * \param key The key.
 * \param idx The index of the value to get.
 * \return The value.
 */
M_API const char *M_ini_kvs_val_get_direct(M_ini_kvs_t *dict, const char *key, size_t idx);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Remove the key from the kv store.
 * \param dict The kv store.
 * \param key The key.
 * \param return True on success otherwise false.
 */
M_API M_bool M_ini_kvs_remove(M_ini_kvs_t *dict, const char *key);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the current size (number of buckets/slots, not necessarily used) of
 * the store.
 * \param dict The kv store.
 * \return Size of the M_ohash_dict */
M_API M_uint64 M_ini_kvs_size(M_ini_kvs_t *dict);

/*! Retrieve the number of collisions for kv store entries that has occurred since the
 * store was created.
 * \param dict kv store.
 * \return number of collisions.
 */
M_API M_uint64 M_ini_kvs_num_collisions(M_ini_kvs_t *dict);

/*! Retrieve the number of expansions/rehashes since the kv store was created.
 * \param dict kv store.
 * \return number of expansions/rehashes.
 */
M_API M_uint64 M_ini_kvs_num_expansions(M_ini_kvs_t *dict);

/*! Retrieve the number of keys in the kv store.
 * \param dict kv store.
 * \return the number of keys in the store.
 */
M_API M_uint64 M_ini_kvs_num_keys(M_ini_kvs_t *dict);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start a sorted enumeration of the keys within a kv store.
 * \param dict kv store.
 * \param dictenum[out] Outputs an initialized state variable for
 *                       starting an enumeration. This must be
 *                       free'd via M_ini_kvs_enumerate_free().
 * \return Number of items (values) in the kv store.
 */
M_API M_uint64 M_ini_kvs_enumerate(M_ini_kvs_t *dict, M_ini_kvs_enum_t **dictenum);

/*! Retrieve the next sorted item from a kvs store enumeration.
 * \param dict kv store.
 * \param dictenum[in,out] State variable for tracking the enumeration process.
 * \param key[out]         Value of next enumerated key, optional, may be NULL.
 * \param value[out]       Value of next enumerated value, optional, may be NULL.
 * \return True if enumeration succeeded, false if no more keys.
 */
M_API M_bool M_ini_kvs_enumerate_next(M_ini_kvs_t *dict, M_ini_kvs_enum_t *hashenum, const char **key, size_t *idx, const char **value);

/*! Free the enumeration state
 * \param dictenum  Enumeration being free'd
 */
M_API void M_ini_kvs_enumerate_free(M_ini_kvs_enum_t *dictenum);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Merge two kv stores together. The second (src) kv store will be destroyed
 * automatically upon completion of this function. Any key/value pointers for
 * the kv store will be directly copied over to the destination kv store,
 * they will not be duplicated. Any keys which exist in 'dest' that also
 * exist in 'src' will have the values from 'src' appended.
 * \param dest[in,out] Pointer by reference to the kv store receiving the key/value pairs.
 *                     if dest is NULL, the src address will simply be copied to dest.
 * \param src[in,out]  Pointer to the kv store giving up its key/value pairs.
 */
M_API void M_ini_kvs_merge(M_ini_kvs_t **dest, M_ini_kvs_t *src) M_FREE(2);

/*! Duplicate an existing M_ohash_dict.  Copying all keys and values
 * \param dict kv store to be copied
 * \return duplicated kv store.
 */
M_API M_ini_kvs_t *M_ini_kvs_duplicate(M_ini_kvs_t *dict) M_MALLOC;

__END_DECLS

#endif /* __M_INI_INT_H__ */
