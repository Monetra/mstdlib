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

/* Implementation of local stream logging module.
 *
 */
#include "m_config.h"
#include <m_log_int.h>


#ifdef __ANDROID__ /* If platform doesn't support console output: */
M_log_error_t M_log_module_add_stream(M_log_t *log, M_stream_type_t type, size_t max_queue_bytes,
	M_log_module_t **out_mod)
{
	(void)log; (void)type; (void)max_queue_bytes;
	if (out_mod != NULL) {
		*out_mod = NULL;
	}
	return M_LOG_MODULE_UNSUPPORTED;
}
#else /* If platform does support console output: */

/* ---- PRIVATE: callbacks for internal async_writer object. ---- */

static M_bool writer_write_cb(char *msg, M_uint64 cmd, void *thunk)
{
	FILE   *iostream = thunk;
	size_t  msg_len  = M_str_len(msg);

	(void)cmd;

	if (msg_len == 0) {
		return M_TRUE;
	}

	if (iostream == NULL) {
		return M_FALSE;
	}

	M_fprintf(iostream, "%s", msg);

	return M_TRUE;
}



/* ---- PRIVATE: callbacks for log module object. ---- */

static void log_write_cb(M_log_module_t *mod, const char *msg, M_uint64 tag)
{
	M_async_writer_t *writer;

	(void)tag;

	if (msg == NULL || mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	writer = mod->module_thunk;

	M_async_writer_write(writer, msg);
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

	M_async_writer_t *writer;
	FILE             *iostream;

	if (mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	writer   = mod->module_thunk;
	iostream = M_async_writer_get_thunk(writer);

	M_fprintf(iostream, "%s%s", msg, M_async_writer_get_line_end(writer));
}


static void log_destroy_cb(void *ptr, M_bool flush)
{
	M_async_writer_destroy((M_async_writer_t *)ptr, flush);
}


static M_bool log_destroy_blocking_cb(void *ptr, M_bool flush, M_uint64 timeout_ms)
{
	return M_async_writer_destroy_blocking((M_async_writer_t *)ptr, flush, timeout_ms);
}



/* ---- PUBLIC: stream-specific module functions. ---- */

M_log_error_t M_log_module_add_stream(M_log_t *log, M_stream_type_t type, size_t max_queue_bytes,
	M_log_module_t **out_mod)
{
	M_log_module_t *mod;
	FILE           *iostream;

	if (out_mod != NULL) {
		*out_mod = NULL;
	}

	if (log == NULL || max_queue_bytes == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (log->suspended) {
		return M_LOG_SUSPENDED;
	}

	iostream = NULL;
	switch (type) {
		case M_STREAM_STDOUT: iostream = stdout; break;
		case M_STREAM_STDERR: iostream = stderr; break;
	}
	if (iostream == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	mod                                   = M_malloc_zero(sizeof(*mod));
	mod->type                             = M_LOG_MODULE_STREAM;
	mod->flush_on_destroy                 = log->flush_on_destroy;
	mod->allow_tag_padding                = M_TRUE;
	mod->module_thunk                     = M_async_writer_create(max_queue_bytes, writer_write_cb, iostream, NULL,
		NULL, log->line_end_writer_mode);
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
	M_async_writer_start(mod->module_thunk);

	/* Add the module to the log. */
	M_thread_mutex_lock(log->lock);
	M_llist_insert(log->modules, mod);
	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}

#endif
