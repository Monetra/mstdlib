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

#ifndef __M_HASH_U64U64_H__
#define __M_HASH_U64U64_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_hash_u64u64 Hashtable - uint64/uint64
 *  \ingroup m_hashtable
 * 
 * Hashtable, meant for storing uint64 keys and uint64 values.
 * This is useful for using an enum value as a key with an
 * integer value.
 *
 * References to the data will always be read-only.
 * All keys and values will be duplicated by the hashtable.
 *
 * @{
 */

struct M_hash_u64u64;
/* Currently a direct map to M_hashtable private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary.
 */
typedef struct M_hash_u64u64 M_hash_u64u64_t;

struct M_hash_u64u64_enum;
/* Used for enumerating a M_hash_u64bin. */
typedef struct M_hash_u64u64_enum M_hash_u64u64_enum_t;


/*! Flags for controlling the behavior of the hashtable */
typedef enum {
	M_HASH_U64U64_NONE           = 0,      /*!< Case sensitive single value (new values replace). */
	M_HASH_U64U64_KEYS_ORDERED   = 1 << 0, /*!< Keys should be ordered. Default is insertion order unless the
	                                            sorted option is specified. */
	M_HASH_U64U64_KEYS_SORTASC   = 1 << 1, /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_U64U64_KEYS_SORTDESC  = 1 << 2, /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_U64U64_MULTI_VALUE    = 1 << 3, /*!< Allow keys to contain multiple values.
	                                            Sorted in insertion order another sorting is specified. */
	M_HASH_U64U64_MULTI_SORTASC  = 1 << 4, /*!< Allow keys to contain multiple values sorted in ascending order */
	M_HASH_U64U64_MULTI_SORTDESC = 1 << 5, /*!< Allow keys to contain multiple values sorted in descending order */
	M_HASH_U64U64_MULTI_GETLAST  = 1 << 6  /*!< When using get and get_direct function get the last value from the list
	                                            when allowing multiple values. The default is to get the first value. */
} M_hash_u64u64_flags_t;


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
 * \param[in] flags    M_hash_u64u64_flags_t flags for modifying behavior.
 *
 * \return Allocated hashtable.
 *
 * \see M_hash_u64u64_destroy
 */
M_API M_hash_u64u64_t *M_hash_u64u64_create(size_t size, M_uint8 fillpct, M_uint32 flags) M_MALLOC;


/*! Destroy the hashtable.
 *
 * \param[in] h Hashtable to destroy
 */
M_API void M_hash_u64u64_destroy(M_hash_u64u64_t *h) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert an entry into the hashtable.
 *
 * \param[in,out] h     Hashtable being referenced.
 * \param[in]     key   Key to insert.
 * \param[in]     value Value to insert into hashtable. Value will be duplicated,
 *                      and case will be preserved. May be NULL.
 *
 * \return M_TRUE on success, or M_FALSE on failure.
 */
M_API M_bool M_hash_u64u64_insert(M_hash_u64u64_t *h, M_uint64 key, M_uint64 value);


/*! Remove an entry from the hashtable.
 *
 * \param[in,out] h   Hashtable being referenced.
 * \param[in]     key Key to remove from the hashtable. 
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64u64_remove(M_hash_u64u64_t *h, M_uint64 key);


/*! Retrieve the value for a key from the hashtable. 
 *
 * \param[in] h      Hashtable being referenced.
 * \param[in] key    Key for value.
 * \param[out] value Pointer to value stored in the hashtable. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64u64_get(const M_hash_u64u64_t *h, M_uint64 key, M_uint64 *value);


/*! Retrieve the value for a key from the hashtable, and return it directly as the return value.
 *
 * This cannot be used if you need to differentiate between a key that doesn't exist vs a key with a NULL value.
 *
 * \param[in] h   Hashtable being referenced.
 * \param[in] key Key for value to retrieve from the hashtable.
 *
 * \return NULL if key doesn't exist or NULL value on file, otherwise the value.
 */
M_API M_uint64 M_hash_u64u64_get_direct(const M_hash_u64u64_t *h, M_uint64 key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of values for a given key.
 *
 * \param[in]  h   Hashtable being referenced.
 * \param[in]  key Key for value to retrieve. 
 * \param[out] len The number of values.
 *
 * \return M_TRUE if length is retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64u64_multi_len(const M_hash_u64u64_t *h, M_uint64 key, size_t *len);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in]  h     Hashtable being referenced.
 * \param[in]  key   Key for value to retrieve.
 * \param[in]  idx   The index the value resides at.
 * \param[out] value Pointer to value stored. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist
 */
M_API M_bool M_hash_u64u64_multi_get(const M_hash_u64u64_t *h, M_uint64 key, size_t idx, M_uint64 *value);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in] h   Hashtable being referenced.
 * \param[in] key Key for value to retrieve.
 * \param[in] idx The index the value resides at.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_uint64 M_hash_u64u64_multi_get_direct(const M_hash_u64u64_t *h, M_uint64 key, size_t idx);


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
M_API M_bool M_hash_u64u64_multi_remove(M_hash_u64u64_t *h, M_uint64 key, size_t idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the current size (number of buckets/slots, not necessarily used).
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Size of the hashtable.
 */
M_API M_uint32 M_hash_u64u64_size(const M_hash_u64u64_t *h);


/*! Retrieve the number of collisions for hashtable entries that has occurred since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Number of collisions.
 */
M_API size_t M_hash_u64u64_num_collisions(const M_hash_u64u64_t *h);


/*! Retrieve the number of expansions/rehashes since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of expansions/rehashes.
 */
M_API size_t M_hash_u64u64_num_expansions(const M_hash_u64u64_t *h);


/*! Retrieve the number of entries in the hashtable.
 *
 * This is the number of keys stored.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of entries in the hashtable.
 */
M_API size_t M_hash_u64u64_num_keys(const M_hash_u64u64_t *h);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start an enumeration of the keys within the hashtable.
 *
 * \param[in] h         Hashtable being referenced.
 * \param[out] hashenum Outputs an initialized state variable for starting an enumeration.
 *
 * \return Number of values in the hashtable. 
 *
 * \see M_hash_u64u64_enumerate_free
 */
M_API size_t M_hash_u64u64_enumerate(const M_hash_u64u64_t *h, M_hash_u64u64_enum_t **hashenum);


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
M_API M_bool M_hash_u64u64_enumerate_next(const M_hash_u64u64_t *h, M_hash_u64u64_enum_t *hashenum, M_uint64 *key, M_uint64 *value);


/*! Destroy an enumeration state.
 *
 * \param[in] hashenum Enumeration to destroy.
 */
M_API void M_hash_u64u64_enumerate_free(M_hash_u64u64_enum_t *hashenum);


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
M_API void M_hash_u64u64_merge(M_hash_u64u64_t **dest, M_hash_u64u64_t *src) M_FREE(2);

/*! Duplicate an existing hashtable.
 *
 * Copying all keys and values.
 *
 * \param[in] h Hashtable to be copied.
 *
 * \return Duplicated hashtable.
 */
M_API M_hash_u64u64_t *M_hash_u64u64_duplicate(const M_hash_u64u64_t *h) M_MALLOC;

/*! @} */

__END_DECLS

#endif /* __M_HASH_U64U64_H__ */
