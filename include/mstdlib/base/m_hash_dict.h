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

#ifndef __M_HASH_DICT_H__
#define __M_HASH_DICT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_hash_dict Hashtable - Dictionary (string/string)
 *  \ingroup m_hashtable
 *
 * Hashtable, meant for storing string key/value pairs.
 *
 * References to the data will always be read-only.
 * All keys and values will be duplicated by the hashtable.
 *
 * @{
 */

struct M_hash_dict;
/* Currently a direct map to M_hashtable private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary.
 */
typedef struct M_hash_dict M_hash_dict_t;

struct M_hash_dict_enum;
/* Used for enumerating a M_hash_dict. */
typedef struct M_hash_dict_enum M_hash_dict_enum_t;


/*! Flags for controlling the behavior of the hashtable. */
typedef enum {
	M_HASH_DICT_NONE           = 0,       /*!< Case sensitive single value (new values replace). */
	M_HASH_DICT_CASECMP        = 1 << 0,  /*!< Key compare is case insensitive. */
	M_HASH_DICT_KEYS_UPPER     = 1 << 1,  /*!< Keys will be upper cased before being inserted. Should be used
	                                           in conjunction with M_HASH_DICT_CASECMP. */
	M_HASH_DICT_KEYS_LOWER     = 1 << 2,  /*!< Keys will be lower cased before being inserted. Should be used
	                                           in conjunction with M_HASH_DICT_CASECMP. */
	M_HASH_DICT_KEYS_ORDERED   = 1 << 3,  /*!< Keys should be ordered. Default is insertion order unless the
	                                           sorted option is specified. */
	M_HASH_DICT_KEYS_SORTASC   = 1 << 4,  /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_DICT_KEYS_SORTDESC  = 1 << 5,  /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_DICT_MULTI_VALUE    = 1 << 6,  /*!< Allow keys to contain multiple values.
	                                           Sorted in insertion order another sorting is specified. */
	M_HASH_DICT_MULTI_SORTASC  = 1 << 7,  /*!< Allow keys to contain multiple values sorted in ascending order */
	M_HASH_DICT_MULTI_SORTDESC = 1 << 8,  /*!< Allow keys to contain multiple values sorted in descending order */
	M_HASH_DICT_MULTI_GETLAST  = 1 << 9,  /*!< When using get and get_direct function get the last value from the list
	                                           when allowing multiple values. The default is to get the first value. */
	M_HASH_DICT_MULTI_CASECMP  = 1 << 10, /*!< Value compare is case insensitive. */
	M_HASH_DICT_STATIC_SEED    = 1 << 11  /*!< Use a static seed for hash function initialization. This greatly reduces
	                                           the security of the hashtable and removes collision attack protections.
	                                           This should only be used as a performance optimization when creating
	                                           millions of hashtables with static data specifically for quick look up.
	                                           DO _NOT_ use this flag with any hashtable that could store user
	                                           generated data! Be very careful about duplicating a hashtable that
	                                           was created with this flag. All duplicates will use the static seed. */
} M_hash_dict_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new hashtable.
 *
 * The hashtable will pre-allocate an array of buckets based on the rounded up size specified. Any hash collisions
 * will result in those collisions being chained together via a linked list. The hashtable will auto-expand by a
 * power of 2 when the fill percentage specified is reached. All key entries are compared in a case-insensitive
 * fashion, and are duplicated internally. Values are duplicated. Case is preserved for both keys and values.
 *
 * \param[in] size     Size of the hash table. If not specified as a power of 2, will
 *                     be rounded up to the nearest power of 2.
 * \param[in] fillpct  The maximum fill percentage before the hash table is expanded. If
 *                     0 is specified, the hashtable will never expand, otherwise the
 *                     value must be between 1 and 99 (recommended: 75).
 * \param[in] flags    M_hash_dict_flags_t flags for modifying behavior.
 *
 * \return Allocated hashtable.
 *
 * \see M_hash_dict_destroy
 */
M_API M_hash_dict_t *M_hash_dict_create(size_t size, M_uint8 fillpct, M_uint32 flags) M_MALLOC;


/*! Destroy the hashtable.
 *
 * \param[in] h Hashtable to destroy
 */
M_API void M_hash_dict_destroy(M_hash_dict_t *h) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert an entry into the hashtable.
 *
 * If this is a multi-value dictionary (see M_HASH_DICT_MULTI_VALUE) and an entry already
 * exists under the given key, the new value is added onto the end of the list. Otherwise,
 * the new value replaces any previous value stored under the given key.
 *
 * \param[in,out] h     Hashtable being referenced.
 * \param[in]     key   Key to insert.
 *                      A NULL or empty string is explicity disallowed.
 * \param[in]     value Value to insert into hashtable. Value will be duplicated,
 *                      and case will be preserved. May be NULL.
 *
 * \return M_TRUE on success, or M_FALSE on failure.
 */
M_API M_bool M_hash_dict_insert(M_hash_dict_t *h, const char *key, const char *value);


/*! Remove an entry from the hashtable.
 *
 * \param[in,out] h   Hashtable being referenced.
 * \param[in]     key Key to remove from the hashtable.
 *                    A NULL or empty string is explicitly disallowed.
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_hash_dict_remove(M_hash_dict_t *h, const char *key);


/*! Retrieve the value for a key from the hashtable.
 *
 * \param[in] h      Hashtable being referenced.
 * \param[in] key    Key for value.
 *                   A NULL or empty string is explicitly disallowed.
 * \param[out] value Pointer to value stored in the hashtable. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_dict_get(const M_hash_dict_t *h, const char *key, const char **value);


/*! Retrieve the value for a key from the hashtable, and return it directly as the return value.
 *
 * This cannot be used if you need to differentiate between a key that doesn't exist vs a key with a NULL value.
 *
 * \param[in] h   Hashtable being referenced.
 * \param[in] key Key for value to retrieve from the hashtable.
 *                A NULL or empty string is explicitly disallowed.
 *
 * \return NULL if key doesn't exist or NULL value on file, otherwise the value.
 */
M_API const char *M_hash_dict_get_direct(const M_hash_dict_t *h, const char *key);


/*! Retrieve the value for a key from the hashtable, and return it directly as the return value.
 *
 * If the key does not exist or the value is NULL the provided default value will be returned.
 *
 * \param[in] h   Hashtable being referenced.
 * \param[in] key Key for value to retrieve from the hashtable.
 *                A NULL or empty string is explicitly disallowed.
 * \param[in] def A default value.
 *
 * \return def if key doesn't exist or NULL value on file, otherwise the value.
 */
M_API const char *M_hash_dict_get_direct_default(const M_hash_dict_t *h, const char *key, const char *def);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Wether the hashtable a multi value table.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return M_TRUE if a multi value hashtable.
 */
M_bool M_hash_dict_is_multi(const M_hash_dict_t *h);


/*! Get the number of values for a given key.
 *
 * \param[in]  h   Hashtable being referenced.
 * \param[in]  key Key for value to retrieve.
 * \param[out] len The number of values.
 *
 * \return M_TRUE if length is retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_dict_multi_len(const M_hash_dict_t *h, const char *key, size_t *len);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in]  h     Hashtable being referenced.
 * \param[in]  key   Key for value to retrieve.
 * \param[in]  idx   The index the value resides at.
 * \param[out] value Pointer to value stored. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist
 */
M_API M_bool M_hash_dict_multi_get(const M_hash_dict_t *h, const char *key, size_t idx, const char **value);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in] h   Hashtable being referenced.
 * \param[in] key Key for value to retrieve.
 * \param[in] idx The index the value resides at.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API const char *M_hash_dict_multi_get_direct(const M_hash_dict_t *h, const char *key, size_t idx);


/*! Remove a value from the hashtable when supporting muli-values.
 *
 * If all values have been removed then the key will be removed.
 *
 * \param[in,out] h   Hashtable being referenced
 * \param[in]     key Key for value to retrieve.
 * \param[in]     idx The index the value resides at.
 *
 * \return M_TRUE if the value was removed, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_dict_multi_remove(M_hash_dict_t *h, const char *key, size_t idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the current size (number of buckets/slots, not necessarily used).
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Size of the hashtable
 */
M_API M_uint32 M_hash_dict_size(const M_hash_dict_t *h);


/*! Retrieve the number of collisions for hashtable entries that has occurred since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Number of collisions.
 */
M_API size_t M_hash_dict_num_collisions(const M_hash_dict_t *h);


/*! Retrieve the number of expansions/rehashes since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of expansions/rehashes.
 */
M_API size_t M_hash_dict_num_expansions(const M_hash_dict_t *h);


/*! Retrieve the number of entries in the hashtable.
 *
 * This is the number of keys stored.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of entries in the hashtable.
 */
M_API size_t M_hash_dict_num_keys(const M_hash_dict_t *h);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start an enumeration of the keys within a hashtable.
 *
 * \param[in]  h        Hashtable being referenced.
 * \param[out] hashenum Outputs an initialized state variable for starting an enumeration.
 *
 * \return Number of items in the hashtable
 *
 * \see M_hash_dict_enumerate_free
 */
M_API size_t M_hash_dict_enumerate(const M_hash_dict_t *h, M_hash_dict_enum_t **hashenum);


/*! Retrieve the next item from a hashtable enumeration.
 *
 * If multi-value, keys will appear multiple times as each value will be
 * retrieved individually.
 *
 * \param[in]     h        Hashtable being referenced.
 * \param[in,out] hashenum State variable for tracking the enumeration process.
 * \param[out]    key      Value of next enumerated key. Optional, pass NULL if not needed.
 * \param[out]    value    Value of next enumerated value. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if enumeration succeeded, M_FALSE if no more keys.
 */
M_API M_bool M_hash_dict_enumerate_next(const M_hash_dict_t *h, M_hash_dict_enum_t *hashenum, const char **key, const char **value);


/*! Destroy an enumeration state.
 *
 * \param[in] hashenum Enumeration to destroy.
 */
M_API void M_hash_dict_enumerate_free(M_hash_dict_enum_t *hashenum);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Merge two hashtables together.
 *
 * The second (src) hashtable will be destroyed automatically upon completion of this function. Any key/value
 * pointers for the hashtable will be directly copied over to the destination hashtable, they will not be
 * duplicated. Any keys which exist in 'dest' that also exist in 'src' will be overwritten by the 'src' value.
 *
 * If dest and src are multi-value, all values from src will be copied into dest and the values from
 * dest will not be removed. If dest is not multi-value and src is, then only the last value in src will
 * be present in dest. If dest is multi-value and src is not, then the value from src will be added to dest.
 *
 * \param[in,out] dest Pointer by reference to the hashtable receiving the key/value pairs.
 *                     if dest is NULL, the src address will simply be copied to dest.
 * \param[in,out] src  Pointer to the hashtable giving up its key/value pairs.
 */
M_API void M_hash_dict_merge(M_hash_dict_t **dest, M_hash_dict_t *src) M_FREE(2);


/*! Duplicate an existing hashtable.
 *
 * Copying all keys and values.
 *
 * \param[in] h Hashtable to be copied.
 *
 * \return Duplicated hashtable.
 */
M_API M_hash_dict_t *M_hash_dict_duplicate(const M_hash_dict_t *h) M_MALLOC;


/*! Possible flags for M_hash_dict_serialize() */
typedef enum {
	M_HASH_DICT_SER_FLAG_NONE          = 0,      /*!< Default flags */
	M_HASH_DICT_SER_FLAG_ALWAYS_QUOTE  = 1 << 0, /*!< Always quote the value even if not necesary. Cannot be used with M_HASH_DICT_SER_FLAG_ALWAYS_QUOTE */
	M_HASH_DICT_SER_FLAG_QUOTE_NON_ANS = 1 << 1  /*!< Quote any string that contains non Alpha-numeric or single space (0x20).  Cannot be used with M_HASH_DICT_SER_FLAG_ALWAYS_QUOTE */
} M_hash_dict_ser_flag_t;


/*! Serialize a dictionary and write it to the provided M_buf_t.
 *
 * \param[in] dict     Dictionary to serialize
 * \param[in] buf      Destination buffer to write to, already initialized.
 * \param[in] delim    Delimiter between key/value pairs (recommended ';')
 * \param[in] kv_delim Delimiter between the key and value (recommended '=')
 * \param[in] quote    Quote character (recommended '"')
 * \param[in] escape   Escape character (recommended '\' or '"')
 * \param[in] flags    Bitmap of possible M_hash_dict_ser_flag_t flags
 * \return M_TRUE on success or M_FALSE on failure.
 */
M_API M_bool M_hash_dict_serialize_buf(M_hash_dict_t *dict, M_buf_t *buf, char delim, char kv_delim, char quote, char escape, M_uint32 flags);


/*! Serialize a dictionary into a string as per the definition.
 *
 * \param[in] dict     Dictionary to serialize
 * \param[in] delim    Delimiter between key/value pairs (recommended ';')
 * \param[in] kv_delim Delimiter between the key and value (recommended '=')
 * \param[in] quote    Quote character (recommended '"')
 * \param[in] escape   Escape character (recommended '\' or '"')
 * \param[in] flags    Bitmap of possible M_hash_dict_ser_flag_t flags
 * \return String of serialized data, or NULL on failure.
 */
M_API char *M_hash_dict_serialize(M_hash_dict_t *dict, char delim, char kv_delim, char quote, char escape, M_uint32 flags);


/*! Deserialize a string into a hashtable as per the definition.
 *
 * \param[in] str      String to deserialize
 * \param[in] delim    Delimiter between key/value pairs (recommended ';')
 * \param[in] kv_delim Delimiter between the key and value (recommended '=')
 * \param[in] quote    Quote character (recommended '"')
 * \param[in] escape   Escape character (recommended '\' or '"')
 * \param[in] flags    Bitmap of possible M_hash_dict_flags_t flags
 * \return Dictionary of key/value pairs, or NULL on failure to parse.
 */
M_API M_hash_dict_t *M_hash_dict_deserialize(const char *str, char delim, char kv_delim, char quote, char escape, M_uint32 flags);


/*! @} */

__END_DECLS

#endif /* __M_HASH_DICT_H__ */
