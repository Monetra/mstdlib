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

/* Implementation for nslog logging module.
 *
 */
#include "m_config.h"
#include <m_log_int.h>


#ifndef HAVE_NSLOG_SYS /* If platform doesn't provide NSLog: */
M_log_error_t M_log_module_add_nslog(M_log_t *log, size_t max_queue_bytes, M_log_module_t **out_mod)
{
	(void)log; (void)max_queue_bytes;
	if (out_mod != NULL) {
		*out_mod = NULL;
	}
	return M_LOG_MODULE_UNSUPPORTED;
}

#else /* If platform does provide NSLog: */
#include "m_log_nslog_sys.h"

/* Internal writer object doesn't need a thunk. */

/* Thunk for log write callback is just the internal writer. */


/* ---- PRIVATE: callbacks for internal async_writer object. ---- */

static M_bool writer_write_cb(char *msg, M_uint64 cmd, void *thunk)
{
	(void)cmd;
	(void)thunk;

	if (!M_str_isempty(msg)) {
		M_log_nslog_sys(msg);
	}

	return M_TRUE;
}



/* ---- PRIVATE: callbacks for log module object. ---- */

static void log_write_cb(M_log_module_t *mod, M_buf_t *msg, M_uint64 tag)
{
	M_async_writer_t *writer;

	(void)tag;

	if (msg == NULL || mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	writer = mod->module_thunk;

	M_async_writer_write(writer, M_buf_peek(msg));
}


static M_log_error_t log_suspend_cb(M_log_module_t *module)
{
	M_async_writer_t *writer;

	if (module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	writer = module->module_thunk;

	/* End the internal worker thread (message queue will still be intact and accepting messages). */
	M_async_writer_stop(writer); /* BLOCKING */

	return M_LOG_SUCCESS;
}


static M_log_error_t log_resume_cb(M_log_module_t *module, M_event_t *event)
{
	M_async_writer_t *writer;

	(void)event;

	if (module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	writer = module->module_thunk;

	/* Start a new internal worker thread. */
	M_async_writer_start(writer);

	return M_LOG_SUCCESS;
}


static void log_emergency_cb(M_log_module_t *mod, const char *msg)
{
	/* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
	 *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
	 *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
	 */

	(void)mod;
	M_log_nslog_sys(msg);
}


static void log_destroy_cb(void *thunk, M_bool flush)
{
	M_async_writer_destroy((M_async_writer_t *)thunk, flush);
}


static M_bool log_destroy_blocking_cb(void *thunk, M_bool flush, M_uint64 timeout_ms)
{
	return M_async_writer_destroy_blocking((M_async_writer_t *)thunk, flush, timeout_ms);
}



/* ---- PUBLIC: nslog-specific module functions ---- */

M_log_error_t M_log_module_add_nslog(M_log_t *log, size_t max_queue_bytes, M_log_module_t **out_mod)
{
	M_async_writer_t *writer;
	M_log_module_t   *mod;

	if (out_mod != NULL) {
		*out_mod = NULL;
	}

	if (log == NULL || max_queue_bytes == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (log->suspended) {
		return M_LOG_SUSPENDED;
	}

	/* Set up thunk for nslog module. */
	writer = M_async_writer_create(max_queue_bytes, writer_write_cb, NULL, NULL, NULL, log->line_end_writer_mode);

	/* General module settings. */
	mod                                   = M_malloc_zero(sizeof(*mod));
	mod->type                             = M_LOG_MODULE_NSLOG;
	mod->flush_on_destroy                 = log->flush_on_destroy;
	mod->module_thunk                     = writer;
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
	M_async_writer_start(writer);

	/* Add the module to the log. */
	M_thread_mutex_lock(log->lock);
	M_llist_insert(log->modules, mod);
	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}

#endif
