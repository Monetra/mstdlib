/* The MIT License (MIT)
 * 
 * Copyright (h) 2016 Main Street Softworks, Inc.
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

#ifndef __M_HASH_MULTI_H__
#define __M_HASH_MULTI_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_hashtable.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_hash_multi Hashtable - Multi
 *  \ingroup m_hashtable
 *
 * Hashtable, meant for storing a variety of key and value types.
 *
 * All data except void pointers will be duplicated.
 *
 * @{
 */

struct M_hash_multi;
typedef struct M_hash_multi M_hash_multi_t;

/*! Flags for controlling the behavior of the hash_multi. */
typedef enum {
	M_HASH_MULTI_NONE        = 0,     /*!< String key compare is case sensitive. */
	M_HASH_MULTI_STR_CASECMP = 1 << 0 /*!< String key compare is case insensitive. */
} M_hash_multi_flags_t;


/* Types of data that can be uses as values. */
typedef enum {
	M_HASH_MULTI_VAL_TYPE_UNKNOWN = 0, /*!< Unknown. */
	M_HASH_MULTI_VAL_TYPE_BOOL,        /*!< Boolean. */
	M_HASH_MULTI_VAL_TYPE_INT,         /*!< Integer. */
	M_HASH_MULTI_VAL_TYPE_STR,         /*!< String. */
	M_HASH_MULTI_VAL_TYPE_BIN,         /*!< Binary. */
	M_HASH_MULTI_VAL_TYPE_VP           /*!< Void pointer. Could be any data. */
} M_hash_multi_val_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Callback for freeing void pointer data. */
typedef void (*M_hash_multi_free_func)(void *);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new multi hashtable.
 * 
 * \param[in] flags M_hash_multi_flags_t flags for modifying behavior.
 *
 * return multi table.
 */
M_API M_hash_multi_t *M_hash_multi_create(M_uint32 flags) M_MALLOC;


/*! Destroy the hashtable.
 *
 * \param[in] h Hashtable to destroy.
 */
M_API void M_hash_multi_destroy(M_hash_multi_t *h) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert bool with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val Boolean value.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_bool(M_hash_multi_t *h, M_uint64 key, M_bool val);


/*! Insert signed integer with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val Signed integer val.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_int(M_hash_multi_t *h, M_uint64 key, M_int64 val);


/*! Insert unsigned integer with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val Unsigned integer val.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_uint(M_hash_multi_t *h, M_uint64 key, M_uint64 val);


/*! Insert string with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val NULL terminated string.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_str(M_hash_multi_t *h, M_uint64 key, const char *val);


/*! Insert binary data with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val Binary data.
 * \param[in]     len Length of binary data.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_bin(M_hash_multi_t *h, M_uint64 key, const unsigned char *val, size_t len);


/*! Insert a void pointer with an integer key.
 *
 * This will not duplicate the value. It only stores the memory address of the data.
 * The if a value exists at the given key it will be destroyed if it was inserted with a
 * value free function.
 *
 * \param[in,out] h        Hashtable.
 * \param[in]     key      Integer key.
 * \param[in]     val      The memory location of the data.
 * \param[in]     val_free Callback for freeing the data.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_insert_vp(M_hash_multi_t *h, M_uint64 key, void *val, M_hash_multi_free_func val_free);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a bool value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_bool(M_hash_multi_t *h, M_uint64 key, M_bool *val);


/*! Get a signed integer value with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_int(M_hash_multi_t *h, M_uint64 key, M_int64 *val);


/*! Get an unsigned integer value with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_uint(M_hash_multi_t *h, M_uint64 key, M_uint64 *val);


/*! Get a string value with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_str(M_hash_multi_t *h, M_uint64 key, const char **val);


/*! Get binary data with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 * \param[in,out] len The value length.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_bin(M_hash_multi_t *h, M_uint64 key, const unsigned char **val, size_t *len);


/*! Get a void pointer value with an integer key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_get_vp(M_hash_multi_t *h, M_uint64 key, void **val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Remove a value with an integer key.
 *
 * \param[in,out] h          Hashtable.
 * \param[in]     key        Integer key.
 * \param[in]     destroy_vp If the value is a void pointer M_TRUE if the associated (if set)
 *                           value free callback should be called.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_u64_remove(M_hash_multi_t *h, M_uint64 key, M_bool destroy_vp);


/*! Get the type of data stored in with an integer key.
 *
 * \param[in] h   Hashtable.
 * \param[in] key Integer key.
 *
 * \return The type.
 */
M_API M_hash_multi_val_type_t M_hash_multi_u64_type(const M_hash_multi_t *h, M_uint64 key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert bool with an string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in]     val Boolean value.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_bool(M_hash_multi_t *h, const char *key, M_bool val);


/*! Insert signed integer with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in]     val Signed integer val
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_int(M_hash_multi_t *h, const char *key, M_int64 val);


/*! Insert unsigned integer with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in]     val Unsigned integer val.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_uint(M_hash_multi_t *h, const char *key, M_uint64 val);


/*! Insert string with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in]     val NULL terminated string.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_str(M_hash_multi_t *h, const char *key, const char *val);


/*! Insert binary data with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in]     val Binary data.
 * \param[in]      len Length of binary data.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_bin(M_hash_multi_t *h, const char *key, const unsigned char *val, size_t len);


/*! Insert a void pointer with a string key.
 *
 * This will not duplicate the value. It only stores the memory address of the data.
 * The if a value exists at the given key it will be destroyed if it was inserted with a
 * value free function.
 *
 * \param[in,out] h        Hashtable.
 * \param[in]     key      String key.
 * \param[in]     val      The memory location of the data.
 * \param[in]     val_free Callback for freeing the data.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_insert_vp(M_hash_multi_t *h, const char *key, void *val, M_hash_multi_free_func val_free);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a bool value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_bool(M_hash_multi_t *h, const char *key, M_bool *val);


/*! Get a signed integer value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key Integer key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_int(M_hash_multi_t *h, const char *key, M_int64 *val);


/*! Get an unsigned integer value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_uint(M_hash_multi_t *h, const char *key, M_uint64 *val);


/*! Get a string value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_str(M_hash_multi_t *h, const char *key, const char **val);


/*! Get binary data with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in,out] val The value to get.
 * \param[in,out] len The value length.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_bin(M_hash_multi_t *h, const char *key, const unsigned char **val, size_t *len);


/*! Get a void pointer value with a string key.
 *
 * \param[in,out] h   Hashtable.
 * \param[in]     key String key.
 * \param[in,out] val The value to get.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_get_vp(M_hash_multi_t *h, const char *key, void **val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Remove a value with a string key.
 *
 * \param[in,out] h          Hashtable.
 * \param[in]     key        String key.
 * \param[in]     destroy_vp If the value is a void pointer M_TRUE if the associated (if set)
 *                           value free callback should be called.
 *
 * \return M_TRUE if insert was successful. Otherwise M_FALSE.
 */
M_API M_bool M_hash_multi_str_remove(M_hash_multi_t *h, const char *key, M_bool destroy_vp);


/*! Get the type of data stored in with a string key.
 *
 * \param[in] h   Hashtable.
 * \param[in] key String key.
 *
 * \return The type.
 */
M_API M_hash_multi_val_type_t M_hash_multi_str_type(const M_hash_multi_t *h, const char *key);

/*! @} */

__END_DECLS

#endif /* __M_HASH_MULTI_H__ */

