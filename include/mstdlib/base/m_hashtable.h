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

#ifndef __M_HASHTABLE_H__
#define __M_HASHTABLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_mem.h>
#include <mstdlib/base/m_math.h>
#include <mstdlib/base/m_sort.h>
#include <mstdlib/base/m_llist.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/* TODO:
 * Additional functions:
 *  + M_list_t *get_keys();
 *  + M_bool is_muli_value();
 */

/*! \defgroup m_hashtable Hashtable
 *  \ingroup m_datastructures
 *  Hashtable, meant for storing key/value pairs with O(1) insertion, deletion, and search time
 */

/*! \addtogroup m_hashtable_generic Hashtable generic/base implementation
 *  \ingroup m_hashtable
 * 
 * Hashtable, meant for storing key/value pairs.
 *
 * This should not be used directly. It is a base implementation that should
 * be used by a type safe wrapper. For example: M_hash_dict.
 *
 * The h can uses a set of callback functions to determine behavior. Such
 * as if it should duplicate or free values.
 *
 * An optional hash algorithm can be specified when creating a type safe wrapper.
 * It is highly recommended to provide a hash algorithm. The default algorithm
 * is an FNV1a variant using the pointer of the key.
 *
 * The currently provided wrappers (str and u64) use an FNV1a variant. Multiple
 * hashing algorithms were considered but FNV1a was ultimately chosen because testing
 * with real world data sets it was found to provide the best performance.
 *
 * The following hash functions were evaluated:
 * - FNV1
 * - FNV1a
 * - Lookup2
 * - Qt4's hash function
 * - djb2
 *
 * Overall performance was tested. We looked at time to generate the hash, 
 * time for insert, and lookup time. The insert and lookup are
 * specific to see how chaining due to increased collisions impacted overall
 * performance.
 *
 * FNV1a had average collision performance and average hash time. Some hash
 * functions had fewer collisions but the time it took to generate the hash
 * far exceeded the chaining time. Others had very fast generation time but had
 * so many collisions that the chaining time exceeded the benefit of being quick.
 *
 * FNV1a was found to have few enough collisions to keep any chains sort and the
 * combined hash generation and chaining time (when chaining happened) was overall
 * faster than the other hash's times.
 * 
 * In order to prevent denial of service attacks by an attacker causing generation
 * of extremely large chains FNV1a was modified. A random hash seed that is unique
 * per hashtable object (each hashtable created using _create(...)) is used as
 * the offset bias for the algorithm.
 *
 * According to draft-eastlake-fnv-09
 * at https://tools.ietf.org/html/draft-eastlake-fnv-09#section-2.2 .
 * "In the general case, almost any offset_basis will serve so long as it is non-zero."
 * This information can also be found on Noll's website
 * http://isthe.com/chongo/tech/comp/fnv/index.html in the section,
 * "Parameters of the FNV-1/FNV-1a hash". 
 *
 * In our variation care has been taken to ensure the bias is never 0.
 *
 * The random seed is created using M_rand. While M_rand is not a secure random
 * number generator the random seed for M_rand is created from unlikely to be known
 * data such as stack and heap memory addresses at the time the hashtable is created.
 * It is unlikely an attacker would be able to determine the random seed to be able to
 * get the hash seed. Nor is it likely for an attacker to be able to determine the
 * hash seed. Testing using a random hash seed was found to alleviate chaining attacks.
 *
 * @{
 */

/* Hashes are M_uint32 meaning we can only have that many buckets. We can
 * have more than that many items due to chaining where a bucket will have
 * multiple items chained together. */
#define M_HASHTABLE_MAX_BUCKETS (1U<<24)

/* ------- Semi-Public types ------- */

struct M_hashtable;
typedef struct M_hashtable M_hashtable_t;


/*! State tracking object for enumerating a Hashtable.  This is explicitly
 * not hidden so it doesn't require a malloc() */
struct M_hashtable_enum {
	union {
		struct {
			M_uint32 hash;     /*!< Hash of last processed entry */
			size_t   chainid;  /*!< 1-based offset within linked list of clashes of last
	                                processed entry.  This value is 1-based specifically
	                                so when starting an enumeration, a 0,0 value would
	                                indicate this */
		} unordered;
		struct {
			M_llist_node_t *keynode; /*!< When ordered keys are in use this is the node of the key
			                              currently being processed. */
		} ordered;
	} entry;
	size_t valueidx;           /*!< When multi-value is in use which index of next value. */
};
typedef struct M_hashtable_enum M_hashtable_enum_t;

/*! Function definition for callback to hash a key */
typedef M_uint32 (*M_hashtable_hash_func)(const void *, M_uint32);

/*! Function definition to duplicate a key or value */
typedef void *(*M_hashtable_duplicate_func)(const void *);

/*! Function definition to free a key or value */
typedef void (*M_hashtable_free_func)(void *);


/*! Structure of callbacks that can be registered to override default
 * behavior for h implementation.
 *
 * This allows a great deal of flexibility.  For instance, you may want
 * the HashTable to take ownership of the 'value' passed to it and clean
 * up when the entry is replaced, removed, or the h is destroyed.
 * In this implementation, you could use NULL for 'value_duplicate' so
 * the pointer passed in is used directly, but register an appropriate
 * 'value_free' to auto-cleanup.
 *
 * Note that there are two duplicate callbacks for keys and values.
 * There are two times a key or value can be duplicated. When it is
 * first inserted into the h and when the h itself
 * is duplicated.
 *
 * In some cases the key or value needs to be duplicated by the h
 * wrapper instead of by the base itself. For example storing unbounded
 * binary data as a value. To prevent extra allocations and additional
 * wrapping the value is duplicated by the wrapper and the length is
 * prepended. This duplicate needs the length in order to work where
 * the other duplicate (copy of h) will get the length from
 * the fist few bytes of the value itself.
 */
struct M_hashtable_callbacks {
	M_hashtable_duplicate_func key_duplicate_insert;   /*!< Callback to duplicate a key on insert. Default if
	                                                    *   NULL is pass-thru pointer */
	M_hashtable_duplicate_func key_duplicate_copy;     /*!< Callback to duplicate a key on copy. Default if
	                                                    *   NULL is pass-thru pointer */
	M_hashtable_free_func      key_free;               /*!< Callback to free a key. Default if NULL
	                                                    *   is no-op */
	M_hashtable_duplicate_func value_duplicate_insert; /*!< Callback to duplicate a value on insert. Default
	                                                    *   if NULL is pass-thru pointer */
	M_hashtable_duplicate_func value_duplicate_copy;   /*!< Callback to duplicate a value on copy. Default
	                                                    *   if NULL is pass-thru pointer */
	M_sort_compar_t            value_equality;         /*!< Callback used to determine if two values are equal.
	                                                        Primarily used for sorting muli-values stores. Default
	                                                        is all values are equal. */
	M_hashtable_free_func      value_free;             /*!< Callback to free a value. Default if
	                                                    *   NULL is a no-op */
};

/*! Flags for controlling the behavior of the hash */
typedef enum {
	M_HASHTABLE_NONE          = 0,      /*!< Case sensitive single value (new values replace). */
	M_HASHTABLE_KEYS_ORDERED  = 1 << 0, /*!< Keys should be ordered. Default is insertion order unless the
	                                         sorted option is specified. */
	M_HASHTABLE_KEYS_SORTED   = 1 << 1, /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASHTABLE_MULTI_VALUE   = 1 << 2, /*!< Allow keys to contain multiple values.
	                                          Sorted in insertion order another sorting is specified. */
	M_HASHTABLE_MULTI_SORTED  = 1 << 3, /*!< Allow keys to contain multiple values sorted in ascending order */
	M_HASHTABLE_MULTI_GETLAST = 1 << 4, /*!< When using the get function will get the last value from the list
	                                         when allowing multiple values. The default is to get the first value. */
	M_HASHTABLE_STATIC_SEED   = 1 << 5  /*!< Use a static seed for hash function initialization. This greatly reduces
	                                         the security of the hashtable and removes collision attack protections.
	                                         This should only be used as a performance optimization when creating
	                                         millions of hashtables with static data specifically for quick look up.
	                                         DO _NOT_ use this flag with any hashtable that could store user
	                                         generated data! Be very careful about duplicating a hashtable that
	                                         was created with this flag. All duplicates will use the static seed. */
} M_hashtable_flags_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new h.
 *
 * The h will pre-allocate an array of buckets based on the rounded up size specified. Any hash collisions
 * will result in those collisions being chained together via a linked list. The h will auto-expand by a
 * power of 2 when the fill percentage specified is reached. All key entries are compared in a case-insensitive
 * fashion, and are duplicated internally. Values are duplicated. Case is preserved for both keys and values.
 *
 * \param[in] size         Size of the hash table. If not specified as a power of 2, will
 *                         be rounded up to the nearest power of 2.
 * \param[in] fillpct      The maximum fill percentage before the hash table is expanded. If
 *                         0 is specified, the h will never expand, otherwise the
 *                         value must be between 1 and 99 (recommended: 75).
 * \param[in] key_hash     The function to use for hashing a key.  If not specified will use
 *                         the pointer address as the key and use FNV1a.
 * \param[in] key_equality The function to use to determine if two keys are equal.  If not 
 *                         specified, will compare pointer addresses.
 * \param[in] flags        M_hash_strvp_flags_t flags for modifying behavior.
 * \param[in] callbacks    Register callbacks for overriding default behavior.
 *
 * \return Allocated h.
 *
 * \see M_hashtable_destroy
 */
M_API M_hashtable_t *M_hashtable_create(size_t size, M_uint8 fillpct,
		M_hashtable_hash_func key_hash, M_sort_compar_t key_equality,
		M_uint32 flags, const struct M_hashtable_callbacks *callbacks) M_MALLOC;


/*! Destroy the h.
 *
 * \param[in] h            Hashtable to destroy
 * \param[in] destroy_vals M_TRUE if the values held by the h should be destroyed.
 *                         This will almost always be M_TRUE. This should only be set to M_FALSE
 *                         when all values held by the h are being managed externally.
 */
M_API void M_hashtable_destroy(M_hashtable_t *h, M_bool destroy_vals) M_FREE(1);


/*! Insert an entry into the h.
 *
 * \param[in] h     Hashtable being referenced.
 * \param[in] key   Key to insert.
 * \param[in] value Value to insert into h. Value will not be duplicated.
 *                  The h will take ownership of the value. Maybe NULL.
 *
 * \return M_TRUE on success, or M_FALSE on failure.
 */
M_API M_bool M_hashtable_insert(M_hashtable_t *h, const void *key, const void *value);


/*! Remove an entry from the h.
 *
 * \param[in] h            Hashtable being referenced.
 * \param[in] key          Key to remove from the h.
 * \param[in] destroy_vals M_TRUE if the value held by the h should be destroyed.
 *                         This will almost always be M_TRUE. This should only be set to M_FALSE
 *                         when the value held by the h is being managed externally.
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_hashtable_remove(M_hashtable_t *h, const void *key, M_bool destroy_vals);


/*! Retrieve the value for a key from the h. 
 *
 * \param[in] h      Hashtable being referenced.
 * \param[in] key    Key for value.
 * \param[out] value Pointer to value stored in the h. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hashtable_get(const M_hashtable_t *h, const void *key, void **value);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of values for a given key.
 *
 * \param[in]  h   Hashtable being referenced.
 * \param[in]  key Key for value to retrieve. 
 * \param[out] len The number of values.
 *
 * \return M_TRUE if length is retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hashtable_multi_len(const M_hashtable_t *h, const void *key, size_t *len);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in]  h     Hashtable being referenced.
 * \param[in]  key   Key for value to retrieve.
 * \param[in]  idx   The index the value resides at.
 * \param[out] value Pointer to value stored. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist
 */
M_API M_bool M_hashtable_multi_get(const M_hashtable_t *h, const void *key, size_t idx, void **value);


/*! Remove a value from the h when supporting muli-values.
 *
 * If all values have been removed then the key will be removed.
 *
 * \param[in] h            Hashtable being referenced
 * \param[in] key          Key for value to retrieve.
 * \param[in] idx          The index the value resides at.
 * \param[in] destroy_vals M_TRUE if the value held by the h should be destroyed.
 *                         This will almost always be M_TRUE. This should only be set to M_FALSE
 *                         when the value held by the h is being managed externally.
 *
 * \return M_TRUE if the value was removed, M_FALSE if key does not exist.
 */
M_API M_bool M_hashtable_multi_remove(M_hashtable_t *h, const void *key, size_t idx, M_bool destroy_vals);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the current size (number of buckets/slots, not necessarily used).
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Size of the h
 */
M_API M_uint32 M_hashtable_size(const M_hashtable_t *h);


/*! Retrieve the number of collisions for h entries that has occurred since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Number of collisions.
 */
M_API size_t M_hashtable_num_collisions(const M_hashtable_t *h);


/*! Retrieve the number of expansions/rehashes since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of expansions/rehashes.
 */
M_API size_t M_hashtable_num_expansions(const M_hashtable_t *h);


/*! Retrieve the number of entries in the h.
 *
 * This is the number of keys stored.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of entries in the h.
 */
M_API size_t M_hashtable_num_keys(const M_hashtable_t *h);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start an enumeration of the keys within a h.
 *
 * \param[in]  h        Hashtable being referenced.
 * \param[out] hashenum Outputs an initialized state variable for starting an enumeration.
 *
 * \return Number of items in the h
 */
M_API size_t M_hashtable_enumerate(const M_hashtable_t *h, M_hashtable_enum_t *hashenum);


/*! Retrieve the next item from a h enumeration.
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
M_API M_bool M_hashtable_enumerate_next(const M_hashtable_t *h, M_hashtable_enum_t *hashenum, const void **key, const void **value);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Merge two hashtables together.
 *
 * The second (src) h will be destroyed automatically upon completion of this function. Any key/value
 * pointers for the h will be directly copied over to the destination h, they will not be
 * duplicated. Any keys which exist in 'dest' that also exist in 'src' will be overwritten by the 'src' value.
 *
 * If dest and src are multi-value, all values from src will be copied into dest and the values from
 * dest will not be removed. If dest is not multi-value and src is, then only the last value in src will
 * be present in dest. If dest is multi-value and src is not, then the value from src will be added to dest.
 * A value_equality function in dest is very important to ensure duplicate values are not present in a given
 * key with multiple values.
 *
 * \param[in,out] dest Pointer by reference to the h receiving the key/value pairs.
 *                     if dest is NULL, the src address will simply be copied to dest.
 * \param[in,out] src  Pointer to the h giving up its key/value pairs.
 */
M_API void M_hashtable_merge(M_hashtable_t **dest, M_hashtable_t *src);


/*! Duplicate an existing h.
 *
 * Copying all keys and values. As well as other elements such as callbacks.
 *
 * \param[in] h Hashtable to be copied.
 *
 * \return Duplicated h.
 */
M_API M_hashtable_t *M_hashtable_duplicate(const M_hashtable_t *h);

/* @} */

__END_DECLS

#endif
