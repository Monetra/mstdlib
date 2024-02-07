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

/* Implementation for local syslog logging module.
 *
 */
#include "m_config.h"
#include <m_log_int.h>

#ifdef _WIN32 /* If platform doesn't provide syslog: */
M_log_error_t M_log_module_add_syslog(M_log_t *log, const char *product, M_syslog_facility_t facility,
    size_t max_queue_bytes, M_log_module_t **out_mod)
{
    (void)log; (void)product; (void)facility; (void)max_queue_bytes;
    if (out_mod != NULL) {
        *out_mod = NULL;
    }
    return M_LOG_MODULE_UNSUPPORTED;
}
M_log_error_t M_log_module_syslog_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
    M_syslog_priority_t priority)
{
    (void)log; (void)module; (void)tags; (void)priority;
    return M_LOG_MODULE_UNSUPPORTED;
}
#else /* If platform does provide syslog: */
#include <syslog.h>


/* Thunk for internal writer object. */
typedef struct {
    M_syslog_facility_t  facility;
    char                *product;
    M_bool               suspended;
} writer_thunk_t;


/* Thunk for m_log write callback. */
typedef struct {
    M_async_writer_t    *writer;
    M_syslog_priority_t  tag_to_priority[64]; /* tag_idx = M_uint64_log2(tag) --> will be in range [0, 63] */
    const char          *line_end_str;
} module_thunk_t;



/* ---- PRIVATE: misc. helper functions ---- */

/* The only reason this is a helper function is to keep the openlog settings in one place. */
static void open_syslog(writer_thunk_t *wdata)
{
    openlog(wdata->product, LOG_CONS|LOG_NOWAIT, (int)wdata->facility);
}


static char priority_to_char(M_syslog_priority_t p_val)
{
    return (char)(p_val + '0');
}


static M_syslog_priority_t char_to_priority(char c_val)
{
    return (M_syslog_priority_t)(c_val - '0');
}



/* ---- PRIVATE: callbacks for internal async_writer object. ---- */

static M_bool writer_write_cb(char *msg, M_uint64 cmd, void *thunk)
{
    writer_thunk_t *wdata    = thunk;
    size_t          msg_len  = M_str_len(msg);
    int             priority;

    if (wdata == NULL) {
        return M_FALSE;
    }

    /* If we just received a resume command, update suspended flag and reopen the log. */
    if ((cmd & M_LOG_CMD_RESUME) != 0) {
        open_syslog(wdata);
        wdata->suspended = M_FALSE;
    }

    /* If we're currently suspended, return write failure. Message will be placed back on queue (if possible). */
    if (wdata->suspended) {
        /* Sleep, so the worker thread doesn't busy-wait the whole time it's suspended. */
        M_thread_sleep(M_LOG_SUSPEND_DELAY * 1000); /* expects microseconds, not milliseconds */
        return M_FALSE;
    }

    /* If we received a reopen request, close+open the syslog before we send the next message. */
    if ((cmd & M_LOG_CMD_SYSLOG_REOPEN) != 0) {
        closelog();
        open_syslog(wdata);
    }

    /* If suspend was requested (and we didn't receive a resume at the same time), update the suspend
     * flag, close the file stream, and skip writing the current message (will be added back onto queue).
     *
     * This should be the LAST command we process, otherwise we'll lose any commands that are in flight.
     */
    if ((cmd & M_LOG_CMD_SUSPEND) != 0 && (cmd & M_LOG_CMD_RESUME) == 0) {
        closelog();
        wdata->suspended = M_TRUE;
        return M_FALSE;
    }

    if (msg_len == 0) {
        return M_TRUE;
    }

    /* Parse priority byte off of end of message. */
    priority         = (int)(char_to_priority(msg[msg_len - 1]));
    msg[msg_len - 1] = '\0';

    /* Send message to syslog. */
    syslog(priority | (int)wdata->facility, "%s", msg);

    return M_TRUE;
}


static void writer_destroy_cb(void *thunk)
{
    writer_thunk_t *wdata = thunk;

    if (wdata != NULL) {
        M_free(wdata->product);
        M_free(wdata);
    }

    closelog();
}



/* ---- PRIVATE: callbacks for log module object. ---- */

static void log_write_cb(M_log_module_t *mod, const char *msg, M_uint64 tag)
{
    module_thunk_t      *mdata;
    M_syslog_priority_t  priority;
    M_buf_t             *buf;

    if (msg == NULL || mod == NULL || mod->module_thunk == NULL) {
        return;
    }

    mdata = mod->module_thunk;

    /* Copy message bytes to buf, expand tabs during transfer. */
    buf = M_buf_create();
    M_buf_add_str_replace(buf, msg, "\t", M_SYSLOG_TAB_REPLACE);

    /* Truncate if message greater than syslog limit. Make sure we still end with the line ending sequence. */
    if (M_buf_len(buf) > M_SYSLOG_MAX_CHARS) {
        M_buf_truncate(buf, M_SYSLOG_MAX_CHARS - M_str_len(mdata->line_end_str));
        M_buf_add_str(buf, mdata->line_end_str);
    }

    priority = mdata->tag_to_priority[M_uint64_log2(tag)];

    M_buf_add_char(buf, priority_to_char(priority));

    M_async_writer_write(mdata->writer, M_buf_peek(buf));

    M_buf_cancel(buf);
}


static M_log_error_t log_reopen_cb(M_log_module_t *module)
{
    module_thunk_t *mdata;

    if (module == NULL || module->module_thunk == NULL) {
        return M_LOG_INVALID_PARAMS;
    }

    mdata = module->module_thunk;

    M_async_writer_set_command(mdata->writer, M_LOG_CMD_SYSLOG_REOPEN, M_FALSE);

    return M_LOG_SUCCESS;
}


static M_log_error_t log_suspend_cb(M_log_module_t *module)
{
    module_thunk_t *mdata;

    if (module == NULL || module->module_thunk == NULL) {
        return M_LOG_INVALID_PARAMS;
    }

    mdata = module->module_thunk;

    if (M_async_writer_is_running(mdata->writer)) {
        /* Ask internal writer to move to suspend mode, then block until it's done. */
        M_async_writer_set_command_block(mdata->writer, M_LOG_CMD_SUSPEND);

        /* Stop the internal worker thread (message queue will still be intact and accepting messages). */
        M_async_writer_stop(mdata->writer); /* BLOCKING */
    }

    return M_LOG_SUCCESS;
}


static M_log_error_t log_resume_cb(M_log_module_t *module, M_event_t *event)
{
    module_thunk_t *mdata;

    (void)event;

    if (module == NULL || module->module_thunk == NULL) {
        return M_LOG_INVALID_PARAMS;
    }

    mdata = module->module_thunk;

    if (!M_async_writer_is_running(mdata->writer)) {
        /* Start a new internal worker thread. */
        M_async_writer_start(mdata->writer);

        /* Ask internal writer to reopen resources and resume writer operations. */
        M_async_writer_set_command(mdata->writer, M_LOG_CMD_RESUME, M_TRUE);
    }

    return M_LOG_SUCCESS;
}


static void log_emergency_cb(M_log_module_t *module, const char *msg)
{
    /* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
     *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
     *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
     */

    M_syslog_facility_t fac = 0;

    /* Try to get facility, but don't cancel sending the message if we can't find it. */
    if (module != NULL && module->module_thunk != NULL) {
        module_thunk_t *mdata = module->module_thunk;
        writer_thunk_t *wdata = M_async_writer_get_thunk(mdata->writer);

        if (wdata != NULL) {
            fac = wdata->facility;
        }
    }

    /* Send message to syslog. */
    syslog(((int)fac) | M_SYSLOG_EMERG, "%s", msg);
}


static void log_destroy_cb(void *thunk, M_bool flush)
{
    module_thunk_t *mdata = thunk;

    if (mdata == NULL) {
        return;
    }

    M_async_writer_destroy(mdata->writer, flush); /* non-blocking - will free itself at next stopping point */
    M_free(mdata);
}


static M_bool log_destroy_blocking_cb(void *thunk, M_bool flush, M_uint64 timeout_ms)
{
    module_thunk_t *mdata = thunk;
    M_bool          done;

    if (mdata == NULL) {
        return M_TRUE;
    }

    done = M_async_writer_destroy_blocking(mdata->writer, flush, timeout_ms);
    M_free(mdata);

    return done;
}



/* ---- PUBLIC: syslog-specific module functions ---- */

M_log_error_t M_log_module_add_syslog(M_log_t *log, const char *product, M_syslog_facility_t facility,
    size_t max_queue_bytes, M_log_module_t **out_mod)
{
    writer_thunk_t *wdata;
    module_thunk_t *mdata;
    M_log_module_t *mod;
    int             i;

    if (out_mod != NULL) {
        *out_mod = NULL;
    }

    if (log == NULL || max_queue_bytes == 0) {
        return M_LOG_INVALID_PARAMS;
    }

    if (log->suspended) {
        return M_LOG_SUSPENDED;
    }

    /* Set up thunk for internal writer. */
    wdata           = M_malloc_zero(sizeof(*wdata));
    wdata->facility = facility;
    if (product == NULL) {
        wdata->product = NULL;
    } else {
        wdata->product  = M_strdup(product);
    }

    /* Set up thunk for syslog module. */
    mdata               = M_malloc_zero(sizeof(*mdata));
    mdata->writer       = M_async_writer_create(max_queue_bytes, writer_write_cb, wdata, NULL, writer_destroy_cb,
        log->line_end_writer_mode);
    mdata->line_end_str = log->line_end_str;

    /* Initialize tag->priority mapping to default value (INFO). */
    for (i=0; i<64; i++) {
        mdata->tag_to_priority[i] = M_SYSLOG_DEFAULT_PRI;
    }

    /* General module settings. */
    mod                                   = M_malloc_zero(sizeof(*mod));
    mod->type                             = M_LOG_MODULE_SYSLOG;
    mod->flush_on_destroy                 = log->flush_on_destroy;
    mod->module_thunk                     = mdata;
    mod->module_write_cb                  = log_write_cb;
    mod->module_reopen_cb                 = log_reopen_cb;
    mod->module_suspend_cb                = log_suspend_cb;
    mod->module_resume_cb                 = log_resume_cb;
    mod->module_emergency_cb              = log_emergency_cb;
    mod->destroy_module_thunk_cb          = log_destroy_cb;
    mod->destroy_module_thunk_blocking_cb = log_destroy_blocking_cb;

    if (out_mod != NULL) {
        *out_mod = mod;
    }

    /* Initialize the module. */
    open_syslog(wdata);

    /* Start the internal writer's worker thread. */
    M_async_writer_start(mdata->writer);

    /* Add the module to the log. */
    M_thread_rwlock_lock(log->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
    M_llist_insert(log->modules, mod);
    M_thread_rwlock_unlock(log->rwlock);

    return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_syslog_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
    M_syslog_priority_t priority)
{
    module_thunk_t *mdata;

    if (log == NULL || module == NULL || module->module_thunk == NULL || tags == 0) {
        return M_LOG_INVALID_PARAMS;
    }

    if (module->type != M_LOG_MODULE_SYSLOG) {
        return M_LOG_WRONG_MODULE;
    }

    M_thread_rwlock_lock(log->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);

    if (!module_present_locked(log, module)) {
        M_thread_rwlock_unlock(log->rwlock);
        return M_LOG_MODULE_NOT_FOUND;
    }

    mdata = module->module_thunk;

    while (tags != 0) {
        M_uint8 tag_idx;

        /* Get index of highest set bit (range: 0,63). */
        tag_idx = M_uint64_log2(tags);

        /* Store priority in map at this index. */
        mdata->tag_to_priority[tag_idx] = priority;

        /* Turn off the flag we just processed. */
        tags = tags & ~((M_uint64)1 << tag_idx);
    }

    M_thread_rwlock_unlock(log->rwlock);

    return M_LOG_SUCCESS;
}

#endif
