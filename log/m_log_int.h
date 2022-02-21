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

/* Internal types and definitions that are shared between M_log and the various modules.
 *
 */
#ifndef M_LOG_INT_H
#define M_LOG_INT_H

#include <mstdlib/mstdlib_log.h>

#define M_LOG_SUSPEND_DELAY   200 /* (ms) Delay used to keep us from busy-waiting during a suspend. */

#define M_SYSLOG_MAX_CHARS    1024
#define M_SYSLOG_DEFAULT_PRI  M_SYSLOG_INFO
#define M_SYSLOG_TAB_REPLACE  "    "

#define M_ANDROID_DEFAULT_PRI M_ANDROID_LOG_INFO


/* Module-specific callback to check whether or not module is still valid.
 * Invalid modules are automatically removed on a future write.
 *
 * \return M_TRUE if module is OK, M_FALSE if module should be purged from the logger.
 */
typedef M_bool (*M_log_check_cb)(M_log_module_t *mod);


/* Module-specific callback to accept a filtered and prefixed message. */
typedef void (*M_log_write_cb)(M_log_module_t *mod, const char *msg, M_uint64 tag);


/* Module-specific callback that asks the module to reopen any internal resources (file stream, tcp connection, etc.) */
typedef M_log_error_t (*M_log_reopen_cb)(M_log_module_t *mod);


/* Module-specific callback that asks the module to close any internal resources and not write anything from the
 * message queue until further notice.
 *
 * If event-based modules are in use, the caller will destroy the event loop after the suspend. So,
 * you should set any internal references to the log's event loop to NULL inside the suspend callback.
 */
typedef M_log_error_t (*M_log_suspend_cb)(M_log_module_t *mod);


/* Module-specific callback that asks the module to open any internal resources and start writing messages again.
 *
 * If you're using modules that rely on an event loop, the new event loop must be passed too. If not, this
 * can just be NULL.
 */
typedef M_log_error_t (*M_log_resume_cb)(M_log_module_t *mod, M_event_t *event);


/* Module-specific callback that writes a static message immediately, without any locking, malloc'ing or hand-offs
 * to other threads.
 *
 * Only intended to be called during a crash by a signal handler, it's EXTREMELY DANGEROUS AND NOT THREAD SAFE AT ALL.
 */
typedef void (*M_log_emergency_cb)(M_log_module_t *mod, const char *msg);


/* Destructor with flush option (non-blocking). */
typedef void (*M_log_destroy_flush_cb)(void *thunk, M_bool flush);


/* Destructor with flush option that blocks until destruction is done, or a timeout is reached.
 *
 * Returns M_FALSE if the timeout expired without the destructor finishing its cleanup. Module will continue
 * trying to clean itself up in an orphaned thread until the program stops.
 */
typedef M_bool (*M_log_destroy_flush_blocking_cb)(void *thunk, M_bool flush, M_uint64 timeout_ms);



struct M_log {
	M_llist_t                      *modules;
	M_async_writer_line_end_mode_t  line_end_writer_mode;
	M_bool                          flush_on_destroy;     /* Flush message queue (if any) when destroying a module? */
	const char                     *line_end_str;
	char                           *time_format;          /* Will get passed to M_time_to_str(). */
	M_hash_u64str_t                *tag_to_name;
	M_hash_multi_t                 *name_to_tag;
	M_thread_mutex_t               *lock;                 /* Lock for list of modules, and per-module settings. */
	size_t                          max_name_width;       /* Keeps track of length of longest loaded tag name. */
	M_bool                          pad_names;            /* If true, tag names will be padded out to constant width. */
	M_event_t                      *event;                /* Event loop to use for event-based modules. */
	M_bool                          suspended;

	M_log_prefix_cb                 prefix_cb;
	void                           *prefix_thunk;
	M_log_destroy_cb                destroy_prefix_thunk_cb;
} /* M_log_t */;


struct M_log_module {
	/* General module options (set by general option calls). */
	M_log_filter_cb   filter_cb;
	void             *filter_thunk;
	M_log_destroy_cb  destroy_filter_thunk_cb;

	M_uint64          accepted_tags;

	/* Module specific stuff. */
	M_log_module_type_t              type;
	M_bool                           flush_on_destroy;
	void                            *module_thunk;
	M_log_check_cb                   module_check_cb;
	M_log_write_cb                   module_write_cb;
	M_log_reopen_cb                  module_reopen_cb;
	M_log_suspend_cb                 module_suspend_cb;
	M_log_resume_cb                  module_resume_cb;
	M_log_emergency_cb               module_emergency_cb;
	M_log_destroy_flush_cb           destroy_module_thunk_cb;
	M_log_destroy_flush_blocking_cb  destroy_module_thunk_blocking_cb;

	M_log_expire_cb                  module_expire_cb;
	void                            *module_expire_thunk; /* Not owned by module. */

} /* M_log_module_t */;


/* Internal helper that checks to see if module hasn't been removed. Assumes you've already locked the log.
 *
 * Implemented in m_log_common.c
 */
M_bool module_present_locked(M_log_t *log, M_log_module_t *module);


/* Internal helper that removes a module. Assumes you've already locked the log.
 *
 * Implemented in m_log_common.c
 */
void module_remove_locked(M_log_t *log, M_log_module_t *module);


/* Master list of commands that may be passed internally to m_async_writer.
 *
 * Must be composable, so these should only be powers of two.
 *
 * Put commands for all modules in same enum, as an extra safeguard against
 * applying commands meant for one module type to another module type.
 */
typedef enum {
	/* General */
	M_LOG_CMD_SUSPEND           = 1 << 0,
	M_LOG_CMD_RESUME            = 1 << 1,
	/* Module-specific */
	M_LOG_CMD_FILE_REOPEN       = 1 << 2,
	M_LOG_CMD_FILE_ROTATE       = 1 << 3,
	M_LOG_CMD_SYSLOG_REOPEN     = 1 << 4,
	M_LOG_CMD_TCP_SYSLOG_REOPEN = 1 << 5
} M_log_commands_t;


#endif /* M_LOG_INT_H */
