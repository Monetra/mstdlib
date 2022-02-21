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

#include "m_config.h"
#include <m_log_int.h>

#ifndef __ANDROID__ /* If platform doesn't provide android log: */
M_log_error_t M_log_module_add_android(M_log_t *log, const char *product, size_t max_queue_bytes,
	M_log_module_t **out_mod)
{
	(void)log; (void)product; (void)max_queue_bytes;
	if (out_mod != NULL) {
		*out_mod = NULL;
	}
	return M_LOG_MODULE_UNSUPPORTED;
}
M_log_error_t M_log_module_android_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_android_log_priority_t priority)
{
	(void)log; (void)module; (void)tags; (void)priority;
	return M_LOG_MODULE_UNSUPPORTED;
}

#else /* If platform does provide android: */
#include <android/log.h>


/* Thunk for internal writer object is just the product name string. */


/* Thunk for m_log write callback. */
typedef struct {
	M_async_writer_t         *writer;
	M_android_log_priority_t  tag_to_priority[64]; /* tag_idx = M_uint64_log2(tag) --> will be in range [0, 63] */
} module_thunk_t;



/* ---- PRIVATE: misc. helper functions ---- */

static android_LogPriority to_native_priority(M_android_log_priority_t priority)
{
	switch (priority) {
		case M_ANDROID_LOG_VERBOSE: return ANDROID_LOG_VERBOSE;
		case M_ANDROID_LOG_DEBUG:   return ANDROID_LOG_DEBUG;
		case M_ANDROID_LOG_INFO:    return ANDROID_LOG_INFO;
		case M_ANDROID_LOG_WARN:    return ANDROID_LOG_WARN;
		case M_ANDROID_LOG_ERROR:   return ANDROID_LOG_ERROR;
		case M_ANDROID_LOG_FATAL:   return ANDROID_LOG_FATAL;
	}
	/* Shouldn't ever reach this, just here to silence a warning. */
	return ANDROID_LOG_INFO;
}


static char priority_to_char(M_android_log_priority_t p_val)
{
	return (char)(p_val + '0');
}


static M_android_log_priority_t char_to_priority(char c_val)
{
	return (M_android_log_priority_t)(c_val - '0');
}



/* ---- PRIVATE: callbacks for internal async_writer object. ---- */

static M_bool writer_write_cb(char *msg, M_uint64 cmd, void *thunk)
{
	const char *product  = thunk;
	size_t      msg_len  = M_str_len(msg);
	int         priority;

	(void)cmd;

	if (msg_len == 0) {
		return M_TRUE;
	}

	/* Parser priority byte off of end of message. */
	priority         = char_to_priority(msg[msg_len - 1]);
	msg[msg_len - 1] = '\0';

	/* Trim off any trailing whitespace (android logger will add newline on our behalf). */
	M_str_trim(msg);

	/* Send message to android log. */
	if (!M_str_isempty(msg)) {
		__android_log_write(to_native_priority(priority), product, msg);
	}

	return M_TRUE;
}

/* writer thunk destroy callback is M_free. */



/* ---- PRIVATE: callbacks for log module object. ---- */

static void log_write_cb(M_log_module_t *mod, const char *msg, M_uint64 tag)
{
	module_thunk_t           *mdata;
	M_android_log_priority_t  priority;

	if (msg == NULL || mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	mdata = mod->module_thunk;

	/* Add priority byte to end of string as an extra character. Will be parsed back off in async write callback. */
	priority = mdata->tag_to_priority[M_uint64_log2(tag)];
	M_buf_add_char(msg, priority_to_char(priority));

	M_async_writer_write(mdata->writer, msg);
}


static M_log_error_t log_suspend_cb(M_log_module_t *module)
{
	module_thunk_t *mdata;

	if (module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	mdata = module->module_thunk;

	/* End the internal worker thread (message queue will still be intact and accepting messages). */
	M_async_writer_stop(mdata->writer); /* BLOCKING */

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

	/* Start a new internal worker thread. */
	M_async_writer_start(mdata->writer);

	return M_LOG_SUCCESS;
}


static void log_emergency_cb(M_log_module_t *mod, const char *msg)
{
	/* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
	 *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
	 *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
	 */

	const char *product = NULL;

	/* Try to get product name, but don't cancel sending the message if we can't find it. */
	if (mod != NULL && mod->module_thunk != NULL) {
		module_thunk_t *mdata = mod->module_thunk;

		product = M_async_writer_get_thunk(mdata->writer);
	}

	__android_log_write(ANDROID_LOG_FATAL, product, msg);
}


static void log_destroy_cb(void *thunk, M_bool flush)
{
	module_thunk_t *mdata = thunk;

	if (mdata == NULL) {
		return;
	}

	M_async_writer_destroy(mdata->writer, flush); /* non-blocking - will orphan worker thread and free itself */
	M_free(mdata);
}


static M_bool log_destroy_blocking_cb(void *thunk, M_bool flush, M_uint64 timeout_ms)
{
	module_thunk_t *mdata = thunk;
	M_bool          done;

	if (mdata == NULL) {
		return M_TRUE;
	}

	/* Ask internal thread to destroy itself, then block until it's done. */
	done = M_async_writer_destroy_blocking(mdata->writer, flush, timeout_ms);
	M_free(mdata);

	return M_TRUE;
}



/* ---- PUBLIC: android-specific module functions ---- */

M_log_error_t M_log_module_add_android(M_log_t *log, const char *product, size_t max_queue_bytes,
	M_log_module_t **out_mod)
{
	char           *writer_product = NULL;
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

	/* Note: if passed in product name is NULL, want to preserve that when passing it to the android log API. */
	if (product != NULL) {
		writer_product = M_strdup(product);
	}

	/* Set up thunk for syslog module. */
	mdata = M_malloc_zero(sizeof(*mdata));
	mdata->writer = M_async_writer_create(max_queue_bytes, writer_write_cb, writer_product, NULL, M_free,
		M_ASYNC_WRITER_LINE_END_UNIX);

	/* Initialize tag->priority mapping to default value (INFO). */
	for (i=0; i<64; i++) {
		mdata->tag_to_priority[i] = M_ANDROID_DEFAULT_PRI;
	}

	/* General module settings. */
	mod                                   = M_malloc_zero(sizeof(*mod));
	mod->type                             = M_LOG_MODULE_ANDROID;
	mod->flush_on_destroy                 = log->flush_on_destroy;
	mod->module_thunk                     = mdata;
	mod->module_write_cb                  = log_write_cb;
	mod->module_suspend_cb                = log_suspend_cb;
	mod->module_resume_cb                 = log_resume_cb;
	mod->module_emergency_cb              = log_emergency_cb;
	mod->destroy_module_thunk_cb          = log_destroy_cb;
	mod->destroy_module_thunk_blocking_cb = log_destroy_blocking_cb;

	if (out_mod != NULL) {
		*out_mod = mod;
	}

	/* Start the internal writer's worker thread. */
	M_async_writer_start(mdata->writer);

	/* Add the module to the log. */
	M_thread_rwlock_lock(log->rwlock, M_THREAD_RWLOCK_TYPE_WRITE);
	M_llist_insert(log->modules, mod);
	M_thread_rwlock_unlock(log->rwlock);

	return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_android_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_android_log_priority_t priority)
{
	module_thunk_t *mdata;

	if (log == NULL || module == NULL || module->module_thunk == NULL || tags == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (module->type != M_LOG_MODULE_ANDROID) {
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
