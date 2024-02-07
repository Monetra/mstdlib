/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

/* Implementation of membuf logging module.
 *
 */
#include "m_config.h"
#include <m_log_int.h>

/* Thunk for m_log write callback. */
typedef struct {
    M_buf_t          *buf;
    size_t            max_size;    /* maximum size to store, after we pass this no new messages are added to the buf. */
    M_uint64          max_time_ms; /* amount of elapsed time allowed before membuf is purged. */
    M_timeval_t       create_time; /* time at which membuf was created. */
    M_thread_mutex_t *lock;        /* Only a global read lock is held when writing, we need an exclusive lock on our own object */
} module_thunk_t;


static module_thunk_t *thunk_create(size_t max_size, M_uint64 max_time_ms)
{
    module_thunk_t *thunk = M_malloc_zero(sizeof(*thunk));

    thunk->buf         = M_buf_create();
    thunk->max_size    = max_size;
    thunk->max_time_ms = max_time_ms;
    thunk->lock        = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

    /* Use M_time_elapsed_start instead of M_time_gettimeofday, so that we use the monotonic clock (if available). */
    M_time_elapsed_start(&thunk->create_time);

    return thunk;
}


static void log_write_cb(M_log_module_t *mod, const char *msg, M_uint64 tag)
{
    module_thunk_t *mdata;
    size_t          msg_len = M_str_len(msg);

    (void)tag;

    if (msg_len == 0 || mod == NULL || mod->module_thunk == NULL) {
        return;
    }

    mdata = mod->module_thunk;

    M_thread_mutex_lock(mdata->lock);

    if (M_buf_len(mdata->buf) > mdata->max_size) {
    /* If buffer is full, don't add the message.
     *
     * Note that we're intentionally allowing the max_size to be exceeded by
     * a single message, instead of truncating it. We figure a no-truncation
     * guarantee is more useful than a strict membuf size limit.
     */
    } else {
        M_buf_add_bytes(mdata->buf, msg, msg_len);
    }
    M_thread_mutex_unlock(mdata->lock);
}


static M_bool log_check_cb(M_log_module_t *mod)
{
    /* Return M_FALSE, if we've exceeded our max time and the module needs to be purged. */
    module_thunk_t *mdata;
    M_uint64        elapsed_ms;

    if (mod == NULL || mod->module_thunk == NULL) {
        return M_FALSE;
    }

    mdata      = mod->module_thunk;
    elapsed_ms = M_time_elapsed(&mdata->create_time);

    if (elapsed_ms > mdata->max_time_ms) {
        return M_FALSE;
    }

    return M_TRUE;
}


static void log_destroy_cb(void *ptr, M_bool flush)
{
    module_thunk_t *thunk = ptr;

    (void)flush;

    if (thunk == NULL) {
        return;
    }

    M_thread_mutex_destroy(thunk->lock);
    M_buf_cancel(thunk->buf);
    M_free(thunk);
}



/* ---- PUBLIC: membuf-specific module functions. ---- */

M_log_error_t M_log_module_add_membuf(M_log_t *log, size_t buf_size, M_uint64 buf_time_s,
    M_log_expire_cb expire_cb, void *expire_thunk, M_log_module_t **out_mod)
{
    module_thunk_t *mdata;
    M_log_module_t *mod;

    if (out_mod != NULL) {
        *out_mod = NULL;
    }

    if (log == NULL) {
        return M_LOG_INVALID_PARAMS;
    }

    /* Set up thunk for membuf module. */
    if (buf_time_s > (M_UINT64_MAX / 1000)) {
        buf_time_s = (M_UINT64_MAX / 1000);
    }
    mdata = thunk_create(buf_size, buf_time_s * 1000); /* convert to ms */

    /* General module settings. */
    mod                          = M_malloc_zero(sizeof(*mod));
    mod->type                    = M_LOG_MODULE_MEMBUF;
    mod->flush_on_destroy        = log->flush_on_destroy;
    mod->module_thunk            = mdata;
    mod->module_write_cb         = log_write_cb;
    mod->module_check_cb         = log_check_cb;
    mod->destroy_module_thunk_cb = log_destroy_cb;
    mod->module_expire_cb        = expire_cb;
    mod->module_expire_thunk     = expire_thunk;

    if (out_mod != NULL) {
        *out_mod = mod;
    }

    /* Add the module to the log. */
    M_thread_rwlock_lock(log->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
    M_llist_insert(log->modules, mod);
    M_thread_rwlock_unlock(log->rwlock);

    return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_take_membuf(M_log_t *log, M_log_module_t *module, M_buf_t **out_buf)
{
    if (out_buf != NULL) {
        *out_buf = NULL;
    }

    if (log == NULL || module == NULL) {
        return M_LOG_INVALID_PARAMS;
    }

    if (module->type != M_LOG_MODULE_MEMBUF) {
        return M_LOG_WRONG_MODULE;
    }

    M_thread_rwlock_lock(log->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);

    if (out_buf != NULL) {
        module_thunk_t *mdata = module->module_thunk;
        if (mdata != NULL) {
            *out_buf = mdata->buf;
            /* Store NULL to internal buffer pointer, so that the buffer won't be
             * destroyed when the module is removed.
             */
            mdata->buf = NULL;
        }
    }

    module_remove_locked(log, module);

    M_thread_rwlock_unlock(log->rwlock);

    return M_LOG_SUCCESS;
}
