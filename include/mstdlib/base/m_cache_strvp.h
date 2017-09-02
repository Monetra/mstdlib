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

#ifndef __M_CACHE_STRVP_H__
#define __M_CACHE_STRVP_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_cache.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \defgroup m_cache_strvp Cache - String/Void Pointer
 *  \ingroup m_cache
 * 
 * Hot cache meant for storing string keys and void pointer values.
 *
 * @{
 */

struct M_cache_strvp;
typedef struct M_cache_strvp M_cache_strvp_t;


/*! Flags for controlling the behavior of the hash */
typedef enum {
	M_CACHE_STRVP_NONE    = 0,      /*!< Default. */
	M_CACHE_STRVP_CASECMP = 1 << 0, /*!< Compare keys case insensitive. */
} M_cache_strvp_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a cache.
 *
 * \param[in] max_size     Maximum number of entries in the cache.
 * \param[in] flags        M_hash_strvp_flags_t flags for modifying behavior.
 * \param[in] destroy_func The function to be called to destroy value when removed.
 *
 * \return Allocated cache.
 *
 * \see M_cache_strvp_destroy
 */
M_API M_cache_strvp_t *M_cache_strvp_create(size_t max_size, M_uint32 flags, void (*destroy_func)(void *)) M_MALLOC_ALIASED;


/*! Destroy the cache.
 *
 * \param[in] c Cache to destroy
 */
M_API void M_cache_strvp_destroy(M_cache_strvp_t *c);


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
M_API M_bool M_cache_strvp_insert(M_cache_strvp_t *c, const char *key, const void *value);


/*! Remove an entry from the cache.
 *
 * \param[in] c   Cache being referenced.
 * \param[in] key Key to remove from the h.
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_cache_strvp_remove(M_cache_strvp_t *c, const char *key);


/*! Retrieve the value for a key from the cache. 
 *
 * \param[in]  c      Cache being referenced.
 * \param[in]  key    Key for value.
 * \param[out] value  Pointer to value stored in the h. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_cache_strvp_get(const M_cache_strvp_t *c, const char *key, void **value);


/*! Retrieve the value for a key from the cache, and return it directly as the return value.
 *
 * This cannot be used if you need to differentiate between a key that doesn't exist vs a key with a NULL value.
 *
 * \param[in] c   Cache being referenced.
 * \param[in] key Key for value to retrieve from the hashtable.
 *                A NULL or empty string is explicitly disallowed.
 *
 * \return NULL if key doesn't exist or NULL value on file, otherwise the value.
 */
M_API void *M_cache_strvp_get_direct(const M_cache_strvp_t *c, const char *key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of items in the cache.
 *
 * \param[in] c Cache being referenced.
 *
 * \return Count.
 */
M_API size_t M_cache_strvp_size(const M_cache_strvp_t *c);


/*! Get the maximum number of items allowed in the cache.
 *
 * \param[in] c Cache being referenced.
 *
 * \return Max.
 */
M_API size_t M_cache_strvp_max_size(const M_cache_strvp_t *c);


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
M_API M_bool M_cache_strvp_set_max_size(M_cache_strvp_t *c, size_t max_size);

/*! @} */

__END_DECLS

#endif /* __M_CACHE_STRVP_H__ */

