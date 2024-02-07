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

#include "m_config.h"

#include <mstdlib/mstdlib_thread.h>
#include "m_thread_int.h"

/* Implementation notes:
 *    Globals:
 *        key_id    - counter for key ids that have been assigned, system-wide
 *        keys      - hashtable of keys with associated destructor function, to
 *                    test if a key really is valid as well as store the destructor
 *                    so that setspecific() doesn't need a destructor.
 *        storepool - pool of thread-specific hashtables that store the key:value
 *                    mapping.
 *        key_mutex - Protects the global keys and threadpool hashtables from
 *                     concurrent access.
 *    Thread specific:
 *        tls_store - hashtable retrieved from tls_storepool specific to the thread
 *                    currently being operated on.  Key is the global key, where the
 *                    value is a simple structure containing the user-supplied value
 *                    from setspecific() as well as the registered destructor from
 *                    key_create().
 */

static M_uint64          M_thread_tls_key_id    = 0;
static M_thread_mutex_t *M_thread_tls_key_mutex = NULL;
static M_hash_u64vp_t   *M_thread_tls_keys      = NULL;
static M_hash_u64vp_t   *M_thread_tls_storepool = NULL;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct {
    void (*destructor)(void *val);
    void *value;
} M_thread_tls_value;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_thread_tls_destroy_store(void *tls_store)
{
    M_hash_u64vp_destroy(tls_store, M_TRUE);
}

static void M_thread_tls_destroy_thread_key(void *value)
{
    M_thread_tls_value *tls_value = value;
    if (tls_value->destructor != NULL)
        tls_value->destructor(tls_value->value);
    M_free(tls_value);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Initialize the TLS subsystem, should be called as part of the global thread system
 *  initialization. */
void M_thread_tls_init(void)
{
    M_thread_tls_key_mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
    M_thread_tls_keys      = M_hash_u64vp_create(16, 75, M_HASH_U64VP_NONE, NULL);
    M_thread_tls_storepool = M_hash_u64vp_create(16, 75, M_HASH_U64VP_NONE, M_thread_tls_destroy_store);
}

/*! Destroy the TLS subsystem, should be called as part of the global thread system
 *  destruction if there is such a thing. */
void M_thread_tls_deinit(void)
{
    M_thread_mutex_destroy(M_thread_tls_key_mutex);
    M_hash_u64vp_destroy(M_thread_tls_keys, M_FALSE);
    M_hash_u64vp_destroy(M_thread_tls_storepool, M_TRUE);
    M_thread_tls_key_mutex = NULL;
    M_thread_tls_keys      = NULL;
    M_thread_tls_storepool = NULL;
    M_thread_tls_key_id    = 0;
}

/*! Destroy the current thread's TLS subsystem.  This is meant to be called internally
 *  by the threading system upon thread completion to destroy any memory associated
 *  with TLS for the thread.  M_thread_self() must still return a valid value. */
void M_thread_tls_purge_thread(void)
{
    M_hash_u64vp_t      *tls_store;
    M_hash_u64vp_enum_t *hashenum;
    M_uint64             key;
    M_list_u64_t        *keylist;
    size_t               i;
    size_t               len;

    M_thread_mutex_lock(M_thread_tls_key_mutex);
    tls_store = M_hash_u64vp_get_direct(M_thread_tls_storepool, M_thread_self());
    M_thread_mutex_unlock(M_thread_tls_key_mutex);

    /* No thread data */
    if (tls_store == NULL)
        return;

    /* We don't want to just remove the entry as it will iterate across all
     * destructors while holding a GLOBAL lock.  Who knows how long that could
     * take and cause the entire application to stall.  Instead, we are going
     * to enumerate the hashtable, store the keys in a list, then iterate
     * over the list and destroy each value ... OUTSIDE of that GLOBAL lock.
     */
    keylist = M_list_u64_create(M_LIST_U64_NONE);
    M_hash_u64vp_enumerate(tls_store, &hashenum);
    while (M_hash_u64vp_enumerate_next(tls_store, hashenum, &key, NULL)) {
        M_list_u64_insert(keylist, key);
    }
    M_hash_u64vp_enumerate_free(hashenum);

    /* Iterate across the keys (remember, can't enumerate and remove from a
     * hashtable) */
    len = M_list_u64_len(keylist);
    for (i=0; i<len; i++) {
        M_hash_u64vp_remove(tls_store, M_list_u64_at(keylist, i), M_TRUE);
    }
    M_list_u64_destroy(keylist);

    /* Now we can re-lock that global mutex and remove the thread specific hash */
    M_thread_mutex_lock(M_thread_tls_key_mutex);
    M_hash_u64vp_remove(M_thread_tls_storepool, M_thread_self(), M_TRUE);
    M_thread_mutex_unlock(M_thread_tls_key_mutex);
}

M_thread_tls_key_t M_thread_tls_key_create(void (*destructor)(void *))
{
    M_uint64 key_id;

    /* atomic_inc returns the prior value, but key id 0 is bad */
    key_id = M_atomic_inc_u64(&M_thread_tls_key_id) + 1;
    M_thread_mutex_lock(M_thread_tls_key_mutex);
    /* We don't want to require a destructor on each setspecific() so store it here */
    M_hash_u64vp_insert(M_thread_tls_keys, key_id, (void *)destructor);
    M_thread_mutex_unlock(M_thread_tls_key_mutex);
    return key_id;
}

M_bool M_thread_tls_setspecific(M_thread_tls_key_t key, const void *value)
{
    void              (*destructor)(void *) = NULL;
    M_hash_u64vp_t     *tls_store           = NULL;
    M_thread_tls_value *tls_value           = NULL;

    M_thread_mutex_lock(M_thread_tls_key_mutex);

    /* Validate key id was really assigned, and grab the destructor */
    if (!M_hash_u64vp_get(M_thread_tls_keys, key, (void **)&destructor)) {
        M_thread_mutex_unlock(M_thread_tls_key_mutex);
        return M_FALSE;
    }

    tls_store = M_hash_u64vp_get_direct(M_thread_tls_storepool, M_thread_self());

    if (tls_store == NULL) {
        tls_store = M_hash_u64vp_create(16, 75, M_HASH_U64VP_NONE, M_thread_tls_destroy_thread_key);
        M_hash_u64vp_insert(M_thread_tls_storepool, M_thread_self(), tls_store);
    }
    M_thread_mutex_unlock(M_thread_tls_key_mutex);

    /* If NULL is specified, we're clearing the value */
    if (value == NULL)
        M_hash_u64vp_remove(tls_store, key, M_TRUE);

    /* No longer in global context, operating on our own thread-specific pool.
     * Don't need to hold the lock any longer */
    tls_value             = M_malloc_zero(sizeof(*tls_value));
    tls_value->destructor = destructor;
    tls_value->value      = (void *)((M_uintptr)value);
    M_hash_u64vp_insert(tls_store, key, tls_value);

    return M_TRUE;
}

void *M_thread_tls_getspecific(M_thread_tls_key_t key)
{
    M_hash_u64vp_t     *tls_store;
    M_thread_tls_value *tls_value;

    /* Hold lock for as short a time as possible */
    M_thread_mutex_lock(M_thread_tls_key_mutex);
    tls_store = M_hash_u64vp_get_direct(M_thread_tls_storepool, M_thread_self());
    M_thread_mutex_unlock(M_thread_tls_key_mutex);

    if (tls_store == NULL)
        return NULL;

    tls_value = M_hash_u64vp_get_direct(tls_store, key);
    if (tls_value == NULL)
        return NULL;

    return tls_value->value;
}
