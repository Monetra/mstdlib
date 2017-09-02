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

#ifndef __M_CACHE_H__
#define __M_CACHE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_hashtable.h>
#include <mstdlib/base/m_sort.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \defgroup m_cache Cache
 *  \ingroup m_datastructures
 * 
 *  Cache (Hot)
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_cache_generic Cache
 *  \ingroup m_cache
 *
 * Hot cache.
 *
 * @{
 */

struct M_cache;
typedef struct M_cache M_cache_t;


/*! Function definition to duplicate a value. */
typedef void *(*M_cache_duplicate_func)(const void *);


/*! Function definition to free a value. */
typedef void (*M_cache_free_func)(void *);


/*! Flags for controlling the behavior of the hash */
typedef enum {
	M_CACHE_NONE = 0, /*!< Default. */
} M_cache_flags_t;


/*! Structure of callbacks that can be registered to override default
 *  behavior for implementation. */
struct M_cache_callbacks {
	M_cache_duplicate_func key_duplicate;   /*!< Callback to duplicate a key. Default if
	                                         *   NULL is pass-thru pointer */
	M_cache_free_func      key_free;        /*!< Callback to free a key. Default if NULL
	                                         *   is no-op */
	M_cache_duplicate_func value_duplicate; /*!< Callback to duplicate a value. Default
	                                         *   if NULL is pass-thru pointer */
	M_cache_free_func      value_free;      /*!< Callback to free a value. Default if
	                                         *   NULL is a no-op */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a cache.
 *
 * \param[in] max_size     Maximum number of entries in the cache.
 * \param[in] key_hash     The function to use for hashing a key.  If not specified will use
 *                         the pointer address as the key.
 * \param[in] key_equality The function to use to determine if two keys are equal.  If not 
 *                         specified, will compare pointer addresses.
 * \param[in] flags        M_hash_strvp_flags_t flags for modifying behavior.
 * \param[in] callbacks    Register callbacks for overriding default behavior.
 *
 * \return Allocated cache.
 *
 * \see M_cache_destroy
 */
M_API M_cache_t *M_cache_create(size_t max_size,
		M_hashtable_hash_func key_hash, M_sort_compar_t key_equality,
		M_uint32 flags, const struct M_cache_callbacks *callbacks) M_MALLOC;


/*! Destroy the cache.
 *
 * \param[in] c Cache to destroy
 */
M_API void M_cache_destroy(M_cache_t *c);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert an entry into the cache.
 *
 * \param[in] c     Cache being referenced.
 * \param[in] key   Key to insert.
 * \param[in] value Value to insert into h.
 *                  The c will take ownership of the value. Maybe NULL.
 *
 * \return M_TRUE on success, or M_FALSE on failure.
 */
M_API M_bool M_cache_insert(M_cache_t *c, const void *key, const void *value);


/*! Remove an entry from the cache.
 *
 * \param[in] c   Cache being referenced.
 * \param[in] key Key to remove from the h.
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_cache_remove(M_cache_t *c, const void *key);


/*! Retrieve the value for a key from the cache. 
 *
 * \param[in]  c      Cache being referenced.
 * \param[in]  key    Key for value.
 * \param[out] value  Pointer to value stored in the h. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_cache_get(const M_cache_t *c, const void *key, void **value);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of items in the cache.
 *
 * \param[in] c Cache being referenced.
 *
 * \return Count.
 */
M_API size_t M_cache_size(const M_cache_t *c);


/*! Get the maximum number of items allowed in the cache.
 *
 * \param[in] c Cache being referenced.
 *
 * \return Max.
 */
M_API size_t M_cache_max_size(const M_cache_t *c);


/*! Set the maximum number of items allowed in the cache.
 *
 * This can be used to increase or decrease the maximum size of the cache.
 * If the max size is smaller than the number of items in the cache, older
 * items will be removed.
 *
 * \param[in] c        Cache being referenced.
 * \param[in] max_size Maximum size.
 *
 * \return M_TRUE if the max size was changed, otherwise M_FALSE on error.
 */
M_API M_bool M_cache_set_max_size(M_cache_t *c, size_t max_size);

/*! @} */

__END_DECLS

#endif /* __M_CACHE_H__ */
