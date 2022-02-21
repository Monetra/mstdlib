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

#ifndef M_LOG_H
#define M_LOG_H

#include <mstdlib/mstdlib_io.h>

__BEGIN_DECLS



/*! \addtogroup m_log_tag_ranges Tag Ranges
 *  \ingroup m_log
 *
 * Helpers for constructing ranges of power-of-two tags.
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Helpers for constructing ranges of power-of-two tags */

/*! Convenience constant - when passed to a function that accepts multiple tags, indicates ALL tags should be used.
 *
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 */
#define M_LOG_ALL_TAGS M_UINT64_MAX


/*! Return all tags less than the given power-of-two tag, OR'd together.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \param[in] tag single power-of-two tag value
 * \return        all tags < given tag
 */
M_uint64 M_log_all_tags_lt(M_uint64 tag);


/*! Return all tags less than or equal to the given power-of-two tag, OR'd together.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \param[in] tag single power-of-two tag value
 * \return        all tags <= given tag
 */
M_uint64 M_log_all_tags_lte(M_uint64 tag);


/*! Return all tags greater than the given power-of-two tag, OR'd together.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gte
 *
 * \param[in] tag single power of two tag value
 * \return        all tags > given tag
 */
M_uint64 M_log_all_tags_gt(M_uint64 tag);


/*! Return all tags greater than or equal to the given power-of-two tag, OR'd together.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 *
 * \param[in] tag single power of two tag value
 * \return        all tags >= given tag
 */
M_uint64 M_log_all_tags_gte(M_uint64 tag);

/*! @} */ /* End of Tag Ranges group */




/*! \addtogroup m_log_common Common
 *  \ingroup m_log
 *
 * Common logging functions.
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* ---- Types ---- */

/*! Opaque struct that maintains state for the logging system. */
typedef struct M_log M_log_t;


/*! Opaque handle used to refer to individual log modules. DON'T FREE THIS. */
typedef struct M_log_module M_log_module_t;


/*! Function type for per-module prefix callbacks.
 *
 * This will be called every time a log message is sent to the module. It allows you to add a custom prefix
 * after the timestamp string, by letting you append to the message buffer before the log message is added:
 *
 * \verbatim 03-02-2012 08:05:32<your prefix here>... log message ...<line end char> \endverbatim
 *
 * Note that no spaces or separator chars are automatically added between the timestamp string, your prefix,
 * and the log message.
 *
 * If no prefix callback is provided, the default prefix \c ": " will be added instead.
 *
 * \warning
 * Do not call any M_log_* or M_log_module_* functions from inside a prefix callback, it will cause a deadlock.
 *
 * \warning
 * Do not write line-end characters in a prefix. Some logging modules don't handle multiple-line messages very well,
 * and line-end chars are not automatically removed from custom prefixes.
 *
 * \warning
 * May be called simultaneously from multiple threads. If the content of prefix_thunk may be modified after
 * it's passed to a module, you should include your own locks inside the thunk and use them inside the callback.
 *
 * \see M_log_set_prefix()
 *
 * \param[in] prefix_thunk thunk passed to M_log_set_prefix()
 * \param[in] msg_thunk    thunk passed to M_printf() or M_vprintf()
 * \param[in] msg_buf      message buffer (append prefix to this)
 * \param[in] tag          tag of message we're prefixing
 */
typedef void (*M_log_prefix_cb)(M_buf_t *msg_buf, M_uint64 tag, void *prefix_thunk, void *msg_thunk);


/*! Function type for per-module filtering callbacks.
 *
 * This will be called every time a log message is sent to the module. If the function returns M_FALSE,
 * the message will be ignored by the module.
 *
 * \warning
 * Do not call any M_log_* or M_log_module_* functions from inside a filter callback, it will cause a deadlock.
 *
 * \warning
 * May be called simultaneously from multiple threads. If the content of filter_thunk may be modified after
 * it's passed to a module, you should include your own locks inside the thunk and use them inside the callback.
 *
 * \see M_log_module_set_filter()
 *
 * \param[in] filter_thunk thunk passed to M_log_module_set_filter()
 * \param[in] msg_thunk    thunk passed to M_log_printf() or M_log_vprintf()
 * \param[in] tag          tag of message we're filtering
 * \return                 M_TRUE if this module will accept the given tag, M_FALSE otherwise
 */
typedef M_bool (*M_log_filter_cb)(M_uint64 tag, void *filter_thunk, void *msg_thunk);


/*! Function type for callback that's called when a module expires.
 *
 * This will be called by the log whenever a module expires and is automatically removed. Currently, only the
 * membuf module has an automatic expiration date. The callback is not called when a module is removed by normal,
 * user-initiated means like M_log_module_remove() or M_log_module_take_membuf().
 *
 * Note that the callback is called AFTER the module is removed from the log. So, at the time the callback
 * is called, the module handle is already invalid.
 *
 * \warning
 * This callback may be called simultaneously by multiple internal threads. If the callback modifies some shared
 * datastructure, you MUST use your own locks to prevent concurrent access to that datastructure.
 *
 * \see M_log_module_add_membuf()
 *
 * \param[in] module handle of membuf module that was just removed (invalid)
 * \param[in] thunk  thunk passed to M_log_module_add_membuf()
 */
typedef void (*M_log_expire_cb)(M_log_module_t *module, void *thunk);


/*! Function type for destructors. */
typedef void (*M_log_destroy_cb)(void *thunk);


/*! Error codes for the logging system. */
typedef enum {
	M_LOG_SUCCESS = 0,         /*!< Operation succeeded. */

	M_LOG_INVALID_PARAMS,      /*!< Given parameters invalid (usually a NULL pointer) */
	M_LOG_INVALID_PATH,        /*!< Given filesystem path couldn't be normalized */
	M_LOG_INVALID_TAG,         /*!< Single tags must be non-zero and a power of two */
	M_LOG_NO_EVENT_LOOP,       /*!< No event loop specified for log, can't use event-based modules. */
	M_LOG_SUSPENDED,           /*!< Log has been suspended, can't take the requested action until resume is called */
	M_LOG_DUPLICATE_TAG_NAME,  /*!< Given name has already been assigned to a different tag */
	M_LOG_UNREACHABLE,         /*!< Requested resource unreachable (can't connect to host, can't open file on disk) */
	M_LOG_INVALID_TIME_FORMAT, /*!< Given time format string is invalid (can't be parsed) */
	M_LOG_MODULE_UNSUPPORTED,  /*!< The given module type is not supported on this OS */
	M_LOG_MODULE_NOT_FOUND,    /*!< The requested module has already been removed from the logger */
	M_LOG_WRONG_MODULE,        /*!< Module-specific function was run on the wrong module */
	M_LOG_GENERIC_FAIL         /*!< Generic internal module failure occurred (usually an IO error) */
} M_log_error_t;


/*! Logging module types. */
typedef enum {
	M_LOG_MODULE_NULL   = 0, /*!< Type that represents invalid or unset module type*/
	M_LOG_MODULE_STREAM,     /*!< Module that outputs to stdout or stderr */
	M_LOG_MODULE_NSLOG,      /*!< Module that outputs to macOS/iOS logging system (NSLog) */
	M_LOG_MODULE_ANDROID,    /*!< Module that outputs to android logging system */
	M_LOG_MODULE_FILE,       /*!< Module that outputs to a set of files on the filesystem */
	M_LOG_MODULE_SYSLOG,     /*!< Module that outputs directly to a local syslog daemon */
	M_LOG_MODULE_TSYSLOG,    /*!< Module that outputs to a remove syslog daemon using TCP */
	M_LOG_MODULE_MEMBUF      /*!< Module that outputs to a temporary memory buffer */
} M_log_module_type_t;


/*! Control what type of line endings get automatically appended to log messages. */
typedef enum {
	M_LOG_LINE_END_NATIVE, /*!< \c '\\n' if running on Unix, \c '\\r\\n' if running on Windows */
	M_LOG_LINE_END_UNIX,   /*!< always use \c '\\n' */
	M_LOG_LINE_END_WINDOWS /*!< always use \c '\\r\\n' */
} M_log_line_end_mode_t;


/*! Types of output streams that can be used for the stream module. */
typedef enum {
	M_STREAM_STDOUT, /*!< Output log messages to \c stdout */
	M_STREAM_STDERR  /*!< Output log messages to \c stderr */
} M_stream_type_t;


/*! Standard facility types for syslog and tcp_syslog modules. */
typedef enum {
	M_SYSLOG_FACILITY_USER   =  1 << 3,
	M_SYSLOG_FACILITY_DAEMON =  3 << 3,
	M_SYSLOG_FACILITY_LOCAL0 = 16 << 3,
	M_SYSLOG_FACILITY_LOCAL1 = 17 << 3,
	M_SYSLOG_FACILITY_LOCAL2 = 18 << 3,
	M_SYSLOG_FACILITY_LOCAL3 = 19 << 3,
	M_SYSLOG_FACILITY_LOCAL4 = 20 << 3,
	M_SYSLOG_FACILITY_LOCAL5 = 21 << 3,
	M_SYSLOG_FACILITY_LOCAL6 = 22 << 3,
	M_SYSLOG_FACILITY_LOCAL7 = 23 << 3
} M_syslog_facility_t;


/*! Standard log priority types for syslog and tcp_syslog modules.
 *
 * Listed in enum in order of descending priority (highest priority --> lowest priority).
 *
 * It is up to the caller to define the mapping between their own logging tags and the syslog
 * priority levels. These mappings are defined on a per-module basis.
 *
 * \see M_log_module_syslog_set_tag_priority
 * \see M_log_module_tcp_syslog_set_tag_priority
 */
typedef enum {
	M_SYSLOG_EMERG   = 0,
	M_SYSLOG_ALERT   = 1,
	M_SYSLOG_CRIT    = 2,
	M_SYSLOG_ERR     = 3,
	M_SYSLOG_WARNING = 4,
	M_SYSLOG_NOTICE  = 5,
	M_SYSLOG_INFO    = 6,
	M_SYSLOG_DEBUG   = 7
} M_syslog_priority_t; /* Note: enum values can't exceed 78, since we append them to msg as a writable char */


/*! Standard log priority types for android log module.
 *
 * Listed in enum in order of descending priority (highest priority --> lowest priority).
 *
 * \see M_log_module_android_set_tag_priority
 */
typedef enum {
	M_ANDROID_LOG_FATAL   = 0,
	M_ANDROID_LOG_ERROR   = 1,
	M_ANDROID_LOG_WARN    = 2,
	M_ANDROID_LOG_INFO    = 3,
	M_ANDROID_LOG_DEBUG   = 4,
	M_ANDROID_LOG_VERBOSE = 5
} M_android_log_priority_t; /* Note: enum values can't exceed 78, since we append them to msg as a writable char */



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* ---- General log functions. ---- */

/*! Return human-readable string describing the given error code.
 *
 * \param[in] err error code
 * \return        human-readable descriptive string
 */
M_API const char *M_log_err_to_str(M_log_error_t err);


/*! Create a new log manager.
 *
 * When first created, the log manager will accept messages, but won't output anything. The desired
 * output modules must be added and configured after the logger is created.  You can add any number
 * or combination of the output modules.
 *
 * To free the logger object, you must call M_log_destroy() or M_log_destroy_blocking().
 *
 * If \a flush_on_destroy is set to M_TRUE, log modules won't be destroyed until all messages in their queue (if any)
 * are written. Otherwise, log modules will be destroyed immediately after the message currently being written is done.
 *
 * If you don't plan on adding any event-based modules (like tcp_syslog), you can pass \c NULL for
 * the event pointer.
 *
 * \see M_log_module_add_stream
 * \see M_log_module_add_file
 * \see M_log_module_add_syslog
 * \see M_log_module_add_tcp_syslog
 * \see M_log_module_add_membuf
 * \see M_log_destroy
 * \see M_log_destroy_blocking
 *
 * \param[in] mode             line-ending mode
 * \param[in] flush_on_destroy if M_TRUE, wait until message queue is empty when destroying a module
 * \param[in] event            external event loop to use for event-based modules (not owned by logger)
 *
 * \return new logger object
 */
M_API M_log_t *M_log_create(M_log_line_end_mode_t mode, M_bool flush_on_destroy, M_event_t *event);


/*! Destroy the logger (non-blocking).
 *
 * Sends a message to each module requesting that it stop at the next opportunity and destroy itself,
 * then immediately destroys the logger.
 *
 * Worker threads will try to clean themselves up gracefully, after this function returns (if the process
 * doesn't end before they have a chance to).
 *
 * \see M_log_destroy_blocking
 *
 * \param[in] log logger object
 */
M_API void M_log_destroy(M_log_t *log);


/*! Destroy the logger (blocking).
 *
 * Sends a message to each module requesting that it stop at the next opportunity. Once all internal
 * worker threads have stopped, destroys all the modules and the logger.
 *
 * If the timeout is reached before all modules have stopped, non-blocking destroys will be triggered for
 * the remaining modules.
 *
 * In order to avoid blocking the event loop, backend modules like TCP syslog which are event-based (no
 * worker threads) will not block when this function is called. Instead, they will execute a normal
 * non-blocking destroy. To give these backends time to exit cleanly, make sure to call
 * M_event_done_with_disconnect() after this function, and pass non-zero values to both timeouts.
 *
 * Example:
 * \code
 * M_event_t  event_loop = ...;
 * M_log_t   *log        = M_log_create(..., event_loop, ...);
 * ...
 * // Block up to five seconds while threaded backends try to exit cleanly.
 * M_log_destroy_blocking(log, 5000);
 *
 * // Wait up to five seconds for top-level handlers added by event-based log backends to close their
 * // own I/O objects. Then, wait up to 1 second to force clean disconnects on any I/O objects
 * // that are still open.
 * M_event_done_with_disconnect(event_loop, 5000, 1000);
 *
 * M_event_destroy(event_loop);
 * \endcode
 *
 * \see M_log_destroy
 *
 * \param[in] log        logger object
 * \param[in] timeout_ms amount of time to wait for modules to finish before giving up, or 0 to never give up
 */
M_API void M_log_destroy_blocking(M_log_t *log, M_uint64 timeout_ms);


/*! Set timestamp format for all future log messages.
 *
 * If not set, the default timestamp format \c "%Y-%M-%DT%H:%m:%s.%l%Z" (ISO8601) will be used.
 *
 * If the given time format string is empty or invalid, an error will be returned
 * and the old time format string will be preserved.
 *
 * Specifiers accepted in time format string:
 *   - %%t -- Unix timestamp
 *   - %%M -- 2 digit month
 *   - %%a -- abbreviated month (Jan/Feb/Mar, etc)
 *   - %%D -- 2 digit day of month
 *   - %%d -- abbreviated day of week (Sun/Mon/Tue, etc)
 *   - %%Y -- 4 digit year
 *   - %%y -- 2 digit year
 *   - %%H -- 2 digit hour
 *   - %%m -- 2 digit minute
 *   - %%s -- 2 digit second
 *   - %%l -- 3 digit millisecond
 *   - %%u -- 6 digit microsecond
 *   - %%z -- timezone offset (without colon)
 *   - %%Z -- timezone offset (with colon)
 *
 * For example "[%D/%a/%Y:%H:%m:%s.%l %z]" might give a prefix like:
 *	[11/Jan/2008:09:19:11.654 -0500]
 *
 * \param[in] log logger object
 * \param[in] fmt time format string
 * \return        error code
 */
M_API M_log_error_t M_log_set_time_format(M_log_t *log, const char *fmt);


/*! Associate a name with the given tag.
 *
 * If a name is specified for the given tag, it will be added to the message prefix, in between the
 * timestamp and the custom prefix.
 *
 * Alternatively, since the tag gets passed into the custom prefix callback, you can add the tag name
 * there instead of using this.
 *
 * Tag names must be unique (case-insensitive). If you try to assign the same name to two tags, you'll
 * get an error code on the second (M_LOG_DUPLICATE_NAME).
 *
 * \param  log  logger object
 * \param  tag  user-defined tag (must be a single power-of-two tag)
 * \param  name name to associate with this tag (NULL or empty string to remove an existing name association)
 * \return      error code
 */
M_API M_log_error_t M_log_set_tag_name(M_log_t *log, M_uint64 tag, const char *name);


/*! Get the name associated with the given tag.
 *
 * \warning
 * DO NOT call this method from a prefix or filter callback, it will cause a deadlock.
 *
 * \param log logger object
 * \param tag user-defined tag (must be a single power-of-two tag)
 * \return    name of tag, or NULL if there's no name stored or some other error occurred
 */
M_API const char *M_log_get_tag_name(M_log_t *log, M_uint64 tag);


/*! Get the tag associated with the given name (case-insensitive).
 *
 * \warning
 * DO NOT call this method from a prefix or filter callback, it will cause a deadlock.
 *
 * \param log  logger object
 * \param name name that has been previously associated with a tag (case doesn't matter)
 * \return     tag associated with the given name, or 0 if there's no tag with this name or some other error occurred
 */
M_API M_uint64 M_log_get_tag(M_log_t *log, const char *name);


/*! Control whether the log should pad names out to a common width or not.
 *
 * By default, tag names are unpadded. However, if you use this function to enable padding,
 * all tag names will be padded with spaces on the right out to the width of the longest
 * name added to the log so far.
 *
 * \param log    logger object
 * \param padded M_TRUE if tag names should be padded on output, M_FALSE if not (the default)
 * \return       error code
 */
M_API M_log_error_t M_log_set_tag_names_padded(M_log_t *log, M_bool padded);


/*! Write a formatted message to the log.
 *
 * Multi-line messages will be split into a separate log message for each line.
 *
 * A timestamp prefix will be automatically added to the start of each line, formatted according to the string set in
 * M_log_set_time_format(). All the lines from a single call to M_printf() will be given identical timestamps.
 *
 * The message will then be sent to each individual logging module for further processing, if the given message tag is
 * in the list of accepted tags for the module (as set by M_log_module_set_accepted_tags()). A given module may also
 * be skipped if a filter callback was set for the module and it rejected the message.
 *
 * For each module that accepts the message, a prefix callback is called (if set) that appends additional text to each
 * line's prefix, immediately following the timestamp string. The formatted message itself is then added, followed by
 * the line-end characters corresponding to the current line-ending mode. The finished message is then sent on to the
 * module.
 *
 * Note that the per-message thunk only needs to be valid until M_log_printf() returns - you can put non-const data
 * in here. This thunk is caller-managed - it's not freed or otherwise kept track of internally.
 *
 * \param[in] log       logger object
 * \param[in] tag       user-defined tag attached to this message (must be a single power-of-two tag)
 * \param[in] msg_thunk per-message thunk to pass to filter and prefix callbacks (only needs to be valid until function returns)
 * \param[in] fmt       format string, accepts same tags as M_printf()
 * \return              error code
 */
M_API M_log_error_t M_log_printf(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *fmt, ...) M_PRINTF(4,5) M_WARN_NONNULL(4);


/*! Write a formatted message to the log (var arg).
 *
 * Same as M_log_printf(), but accepts a variable argument list explicitly as a va_list. This is inteded to allow
 * the user to define their own wrapper functions that take variable argument lists (...) and call M_log_vprintf()
 * internally.
 *
 * \param[in] log       logger object
 * \param[in] tag       user-defined tag attached to this message (must be a single power-of-two tag)
 * \param[in] msg_thunk per-message thunk to pass to filter and prefix callbacks (only needs to be valid until function returns)
 * \param[in] fmt       format string, accepts same tags as M_printf()
 * \param[in] ap        list of arguments passed in from the calling vararg function
 * \return              error code
 */
M_API M_log_error_t M_log_vprintf(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *fmt, va_list ap);


/*! Write a message directly to the log.
 *
 * Same as M_log_printf(), but it takes a message directly, instead of a format string and a list of arguments.
 *
 * \param[in] log       logger object
 * \param[in] tag       user-defined tag attached to this message (must be a single power-of-two tag)
 * \param[in] msg_thunk per-message thunk to pass to filter and prefix callbacks (only needs to be valid until function returns)
 * \param[in] msg       message string
 */
M_API M_log_error_t M_log_write(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *msg);


/*! Perform an emergency message write, to all modules that allow such writes.
 *
 * \warning
 * This function is EXTREMELY dangerous. It's meant to be called in a signal handler as the program is crashing,
 * so it doesn't acquire any mutex locks, and it tries not to malloc anything. DON'T USE THIS FUNCTION IN NORMAL
 * USAGE, IT IS NOT SAFE.
 *
 * \param[in] log logger object
 * \param[in] msg emergency message to output
 */
M_API void M_log_emergency(M_log_t *log, const char *msg);


/*! Reopen/refresh connections for all modules in the logger.
 *
 * This closes then re-opens existing file streams, syslog, TCP connections, etc. for every
 * loaded module.
 *
 * Modules that don't open or close anything (membuf, stream, Android log, etc.) are unaffected by this command.
 *
 * \see M_log_module_reopen (if you only want to reopen a specific module)
 *
 * \param[in] log logger object
 */
M_API void M_log_reopen_all(M_log_t *log);


/*! Suspend connections for all modules in the logger (BLOCKING).
 *
 * This closes existing file streams, syslog, TCP connections, etc. for every loaded module.
 *
 * For modules that have resources closed, messages will accumulate without any being written while suspended.
 *
 * Modules that don't open or close anything (membuf, stream, Android log, etc.) are unaffected by this command.
 *
 * \warning
 * This call will block until every module that can be suspended reports that it is finished suspending itself.
 * At worst, this means that we'll be blocked until whatever message is in the process of being written is done.
 *
 * \warning
 * If you have modules that depend on an external event loop (like tcp_syslog), you must wait for the event
 * loop to finish after calling suspend, and then destroy it. You then create a new event loop once you're
 * ready to resume, and pass it into M_log_resume().
 *
 * \see M_log_resume
 *
 * \param[in] log logger object
 */
M_API void M_log_suspend(M_log_t *log);


/*! Resume connections for all modules in the logger.
 *
 * This opens file streams, syslog, TCP connections, etc. that were closed by M_log_suspend().
 *
 * Modules that were previously suspended will now start pulling messages of the queue and writing them again.
 *
 * If you have any modules that use an external event loop, you must pass the new loop to use into this
 * function (since you should have destroyed the old one after calling suspend). If you're not using
 * event-based modules, you can just set this to NULL.
 *
 * \see M_log_suspend
 *
 * \param[in] log   logger object
 * \param[in] event external event loop to use for event-based modules (not owned by logger)
 */
M_API void M_log_resume(M_log_t *log, M_event_t *event);


/*! Return list of handles of all currently loaded modules.
 *
 * The returned list is a snapshot of what modules were loaded when the function was called. Other threads may
 * modify the internal list of modules after you get a copy, so the list of modules may not be accurate.
 *
 * The caller is responsible for freeing the returned list with M_list_destroy().
 *
 * \param[in] log logger object
 * \return        list of M_log_module_t pointers (module handles)
 */
M_API M_list_t *M_log_all_modules(M_log_t *log);



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Common module functions (see m_log_common.c). */

/*! Check to see if the given module handle is still loaded in the logger.
 *
 * \see M_log_module_remove
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to operate on
 * \return           M_TRUE if module hasn't been removed from the log yet, M_FALSE otherwise
 */
M_API M_bool M_log_module_present(M_log_t *log, M_log_module_t *module);


/*! Return the type of the given module (file, stream, etc.).
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to operate on
 * \return           type of module, or M_LOG_MODULE_NULL if module has already been removed
 */
M_API M_log_module_type_t M_log_module_type(M_log_t *log, M_log_module_t *module);


/*! Associate the given user-defined tag(s) with the given module handle.
 *
 * If you don't associate any tags with a module, nothing will get written to it.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \see M_log_module_get_accepted_tags
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to operate on
 * \param[in] tags   user-defined power-of-two tag (or multiple power-of-two tags, OR'd together)
 * \return           error code
 */
M_API M_log_error_t M_log_module_set_accepted_tags(M_log_t *log, M_log_module_t *module, M_uint64 tags);


/*! Return a snapshot of the user-defined tag(s) currently associated with the given module handle.
 *
 * \see M_log_module_set_accepted_tags
 *
 * \param[in]  log      logger object
 * \param[in]  module   handle of module to operate on
 * \param[out] out_tags tag snapshot will be placed here, or 0 if no tags are associated with this module
 * \return              error code
 */
M_API M_log_error_t M_log_module_get_accepted_tags(M_log_t *log, M_log_module_t *module, M_uint64 *out_tags);


/*! Associate a prefix callback with the given module handle.
 *
 * This exists only for legacy compatibility.  Do Not use.  It does not associate with a module.
 *
 * \see M_log_set_prefix
 *
 *
 * \param[in] log              logger object
 * \param[in] module           handle of module to operate on
 * \param[in] prefix_cb        prefix callback (function pointer)
 * \param[in] prefix_thunk     any state used by prefix callback
 * \param[in] thunk_destroy_cb function to call when destroying the prefix thunk
 * \return                     error code
 */
M_API M_log_error_t M_log_module_set_prefix(M_log_t *log, M_log_module_t *module, M_log_prefix_cb prefix_cb,
	void *prefix_thunk, M_log_destroy_cb thunk_destroy_cb);


/* Associate a prefix callback with the log system.
 *
 * The prefix callback allows the user to add a string between the timestamp and the body of the log message.
 * If no prefix callback is provided, the default prefix of ": " will be used.  See \link M_log_prefix_cb \endlink
 * for more details.
 *
 * \see M_log_prefix_cb
 * \see M_log_set_time_format
 *
 * \param[in] log              logger object
 * \param[in] prefix_cb        prefix callback (function pointer)
 * \param[in] prefix_thunk     any state used by prefix callback
 * \param[in] thunk_destroy_cb function to call when destroying the prefix thunk
 * \return                     error code
 */
M_API M_log_error_t M_log_set_prefix(M_log_t *log, M_log_prefix_cb prefix_cb,
    void *prefix_thunk, M_log_destroy_cb thunk_destroy_cb);

/*! Associate a filter callback with the given module handle.
 *
 * The filter callback allows the user to reject additional log messages. It is applied after the messages are
 * filtered according to the list of accepted tags set by M_log_module_set_accepted_tags(). If no filter callback
 * is provided, no additional filtering beyond the list of accepted tags will be performed. See
 * \link M_log_filter_cb \endlink for more details.
 *
 * \see M_log_filter_cb
 * \see M_log_module_set_accepted_tags
 *
 * \param[in] log              logger object
 * \param[in] module           handle of module to operate on
 * \param[in] filter_cb        filter callback (function pointer)
 * \param[in] filter_thunk     any state used by filter callback
 * \param[in] thunk_destroy_cb function to call when destroying the filter thunk
 * \return                     error code
 */
M_API M_log_error_t M_log_module_set_filter(M_log_t *log, M_log_module_t *module, M_log_filter_cb filter_cb,
	void *filter_thunk, M_log_destroy_cb thunk_destroy_cb);


/*! Trigger a disconnect/reconnect of the given module's internal resource.
 *
 * The exact action taken by this command depends on the module. For example, the file module will close and reopen
 * the internal connection to the main log file.  Modules without closeable resources (like membuf and stream) treat
 * this as a no-op.
 *
 * \see M_log_reopen_all (if you want to reopen all modules in the logger)
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to operate on
 * \return           error code
 */
M_API M_log_error_t M_log_module_reopen(M_log_t *log, M_log_module_t *module);


/*! Remove a module from the running log and destroy it.
 *
 * This function does not block. If the module is busy writing a message, it is removed from the list of active
 * modules, and then it destroys itself asynchronously when the module is finished with its current chunk of work.
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to destroy
 * \return           error code
 */
M_API M_log_error_t M_log_module_remove(M_log_t *log, M_log_module_t *module);

/*! @} */ /* End of Common group */




/*! \addtogroup m_log_stream Stream Module
 *  \ingroup m_log
 *
 * Functions to enable logging to streams (stdout, stderr).
 *
 * Not available when building for Android platform.
 *
 * @{
 */

/*! Add a module to output to a standard stream (stdout, stderr).
 *
 * If the library was compiled on a platform that doesn't allow console output (e.g., Android), this function
 * will return \link M_LOG_MODULE_UNSUPPORTED \endlink when called, and no module will be added to the logger.
 *
 * \warning
 * Normally, you should only add at most one stream output module to a given M_log_t object. This is because
 * having multiple backends write to the same console stream may cause the output from each thread to become
 * intermingled and unreadable. Plus, there's only one global thing you're writing too, it doesn't make any
 * sense to output to it from a bunch of separate worker threads. Just use one backend per destination.
 *
 * \param[in] log             logger object
 * \param[in] type            what stream to output to (stdout, stderr)
 * \param[in] max_queue_bytes max size of queue used to buffer asynchronous writes to the stream
 * \param[in] out_mod         handle for created module, or \c NULL if there was an error
 * \return                    error code
 */
M_API M_log_error_t M_log_module_add_stream(M_log_t *log, M_stream_type_t type, size_t max_queue_bytes,
	M_log_module_t **out_mod);

/*! @} */ /* End of Stream group */




/*! \addtogroup m_log_nslog NSLog Module
 *  \ingroup    m_log
 *
 * Functions to enable logging to macOS/iOS logging subsytem (NSLog).
 *
 * Only available when building for Apple platforms.
 *
 * @{
 */

/*! Add a module to output to macOS/iOS logging subsystem (if we're building for an Apple platform).
 *
 * If the library wasn't compiled for an Apple device, this function will return \link M_LOG_MODULE_UNSUPPORTED \endlink
 * when called, and no module will be added to the logger.
 *
 * \param[in] log             logger object
 * \param[in] max_queue_bytes max size of queue used to buffer asynchronous writer to NSLog
 * \param[in] out_mod         handle for created module, or \c NULL if there was an error
 * \return                    error code
 */
M_API M_log_error_t M_log_module_add_nslog(M_log_t *log, size_t max_queue_bytes, M_log_module_t **out_mod);

/*! @} */ /* End of NSLog group */




/*! \addtogroup m_log_android Android Module
 *  \ingroup m_log
 *
 * Functions to enable logging to Android logging subsystem (__android_log_write).
 *
 * Only available when building for the Android platform.
 *
 * @{
 */

/*! Add a module to output to Android logging subsystem (if we're building for Android).
 *
 * Note: Android logging messages may be truncated by the subsystem to an implementation-specific
 *       line length limit (usually 1023 chars).
 *
 * Android logging allows passing a \c NULL or empty string for the product name - in this case, the "global"
 * product name is used, not the name of the program like in syslog.
 *
 * If the library wasn't compiled for Android, this function will return \link M_LOG_MODULE_UNSUPPORTED \endlink
 * when called, and no module will be added to the logger.
 *
 * \param[in]  log             logger object
 * \param[in]  product         short tag string: name of program, or \c NULL to use "global" tag
 * \param[in]  max_queue_bytes max size of queue used to buffer asynchronous writes to android log
 * \param[out] out_mod         handle for created module, or \c NULL if there was an error
 * \return                     error code
 */
M_API M_log_error_t M_log_module_add_android(M_log_t *log, const char *product, size_t max_queue_bytes,
	M_log_module_t **out_mod);


/*! Associate the given user-defined tag(s) with an Android log priority.
 *
 * If you don't associate a tag with an Android log priority, the default priority of \link M_ANDROID_LOG_INFO \endlink
 * will be used for that tag.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \param[in] log      logger object
 * \param[in] module   handle of module to operate on
 * \param[in] tags     user-defined power-of-two tag (or multiple power-of-two tags, OR'd together)
 * \param[in] priority single priority value to associate with the given tags
 * \return             error code
 */
M_API M_log_error_t M_log_module_android_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_android_log_priority_t priority);

/*! @} */ /* End of Android group */




/*! \addtogroup m_log_file Filesystem Module
 *  \ingroup m_log
 *
 * Functions to enable logging to a group of files on disk. Includes support for log rotation and compression.
 *
 * @{
 */

/*! Add a module to output to a rotating list of files on disk.
 *
 * When archiving a file, the uncompressed file name will be appended directly onto whatever archive command is
 * supplied by the user, then executed in its own process. In order for rotation to work correctly, the output
 * file produced by the command must be exactly equal to <tt>[uncompressed file][archive_file_ext]</tt>.
 *
 * The automatic file rotation parameters (\a autorotate_size and \a autorotate_time_s) can be disabled by setting
 * them to a value of 0. If both are disabled, rotations will only happen when M_log_module_file_rotate() is explicitly
 * called by the user.
 *
 * The behavior of time-based autorotate is platform dependent. On platforms that allow you to query file creation time
 * (e.g., Windows, macOS, BSD), the age of the file is calculated from its creation date. On platforms that don't keep
 * track of file creation time (e.g., Linux), the age of the file is calculated using an internal timer that starts
 * when the file is first opened. On such platforms, the age of the head log file is effectively reset to zero whenever
 * you restart the process.
 *
 * \param[in]  log               logger object
 * \param[in]  log_file_path     main log file to output (if relative path, interpreted relative to current working dir)
 * \param[in]  num_to_keep       number of older rotated log files to keep on disk, or 0 to keep no old files
 * \param[in]  autorotate_size   size (in bytes) after which the main log file is rotated, or 0 to disable size autorotate
 * \param[in]  autorotate_time_s time (in seconds) after which the main log file is rotated, or 0 to disable time autorotate
 * \param[in]  max_queue_bytes   max size of queue used to buffer asynchronous writes to the log file
 * \param[in]  archive_cmd       command used to compress old log files (e.g., "bzip2 -f"). If NULL, compression is disabled.
 * \param[in]  archive_file_ext  extension added to files by \a archive_cmd (e.g., ".bz2"). If NULL, compression is disabled.
 * \param[out] out_mod           handle for created module, or \c NULL if there was an error.
 * \return                       error code
 */
M_API M_log_error_t M_log_module_add_file(M_log_t *log, const char *log_file_path, size_t num_to_keep,
	M_uint64 autorotate_size, M_uint64 autorotate_time_s, size_t max_queue_bytes, const char *archive_cmd,
	const char *archive_file_ext, M_log_module_t **out_mod);


/*! Manually trigger a file rotation.
 *
 * This can be used to rotate the main log file on some other condition than size - like receiving SIGHUP, or on
 * some sort of timer. If the internal message queue is empty, the rotation will happen immediately. If not, the
 * rotation will happen after the internal worker thread finishes writing the message it's currently working on.
 *
 * \param[in] log    logger object
 * \param[in] module handle of module to operate on
 * \return           error code
 */
M_API M_log_error_t M_log_module_file_rotate(M_log_t *log, M_log_module_t *module);

/*! @} */ /* End of file group */




/*! \addtogroup m_log_syslog Syslog Module
 *  \ingroup m_log
 *
 * Functions to enable logging to a local syslog server.
 *
 * Only available on some platforms.
 *
 * Note: syslog messages are limited to 1024 chars / line. Lines longer than this will be truncated.
 *
 * @{
 */

/*! Add a module to output to syslog (if supported by this platform).
 *
 * Note: syslog messages are limited to 1024 chars / line. Lines longer than this will be truncated.
 *
 * If the library wasn't compiled with syslog support, this function will return \link M_LOG_MODULE_UNSUPPORTED \endlink
 * when called, and no module will be added to the logger.
 *
 * \param[in]  log             logger object
 * \param[in]  product         short tag string passed to syslog: if \c NULL, will be set to program name
 * \param[in]  facility        facility type to associate with all log messages, gets passed directly to syslog
 * \param[in]  max_queue_bytes max size of queue used to buffer asynchronous writes to syslog
 * \param[out] out_mod         handle for created module, or \c NULL if there was an error
 * \return                     error code
 */
M_API M_log_error_t M_log_module_add_syslog(M_log_t *log, const char *product, M_syslog_facility_t facility,
	size_t max_queue_bytes, M_log_module_t **out_mod);


/*! Associate the given user-defined tag(s) with a syslog priority.
 *
 * If you don't associate a tag with a syslog priority, the default priority of \link M_SYSLOG_INFO \endlink will
 * be used for that tag.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \param[in] log      logger object
 * \param[in] module   handle of module to operate on
 * \param[in] tags     user-defined power-of-two tag (or multiple power-of-two tags, OR'd together)
 * \param[in] priority single priority value to associate with the given tags
 * \return             error code
 */
M_API M_log_error_t M_log_module_syslog_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_syslog_priority_t priority);

/*! @} */ /* End of syslog group */




/*! \addtogroup m_log_tcp_syslog TCP Syslog Module
 *  \ingroup m_log
 *
 * Functions to enable logging to a remote syslog server over TCP.
 *
 * This module formats messages according to the legacy BSD syslog format from RFC 3164, and adds
 * octet-counting framing as described in RFC 6587 for transmission over TCP. This legacy format was
 * chosen to ensure compatibility across a wide range of syslog servers.
 *
 * Messages are written asynchronously, as part of an existing event loop owned by the caller.
 *
 * Note: syslog messages are limited to 1024 chars / line. Lines longer than this will be truncated.
 *
 * @{
 */

/*! Add a module to output to a remote syslog server over TCP.
 *
 * This module formats messages according to the legacy BSD syslog format from RFC 3164, and adds
 * octet-counting framing as described in RFC 6587 for transmission over TCP. This legacy format was
 * chosen to ensure compatibility across a wide range of syslog servers.
 *
 * Note: syslog messages are limited to 1024 chars / line. Lines longer than this will be truncated.
 *
 * \param[in]  log             logger object
 * \param[in]  product         short tag string for syslog, usually the name of the program sending log info
 * \param[in]  facility        facility type to associate with all log messages, gets passed directly to syslog
 * \param[in]  host            hostname of destination syslog server
 * \param[in]  port            port of destination syslog server (usually 514)
 * \param[in]  dns             external DNS resolver cache to use (not owned by logger)
 * \param[in]  max_queue_bytes max size of queue used to buffer asynchronous writes to syslog
 * \param[out] out_mod         handle for created module, or \c NULL if there was an error
 * \return                     error code
 */
M_API M_log_error_t M_log_module_add_tcp_syslog(M_log_t *log, const char *product, M_syslog_facility_t facility,
	const char *host, M_uint16 port, M_dns_t *dns, size_t max_queue_bytes, M_log_module_t **out_mod);


/*! Set TCP connection timeout.
 *
 * Note that, in addition to normal connection parameters, the TCP module will automatically try to reconnect on
 * disconnect or error after a short delay, no matter what you set here.
 *
 * \param[in] log        logger object
 * \param[in] module     handle of module to operate on
 * \param[in] timeout_ms connection timeout, in milliseconds
 * \return               error code
 */
M_API M_log_error_t M_log_module_tcp_syslog_set_connect_timeout_ms(M_log_t *log, M_log_module_t *module,
	M_uint64 timeout_ms);


/*! Set TCP keep alive parameters for the connection.
 *
 * If this function isn't called, the following default values are used:
 * \li <tt>idle_time_s = 4 seconds</tt>
 * \li <tt>retry_time_s = 15 seconds</tt>
 * \li <tt>retry_count = 3</tt>
 *
 * \param[in] log          logger object
 * \param[in] module       handle of module to operate on
 * \param[in] idle_time_s  tcp idle timeout, in seconds
 * \param[in] retry_time_s tcp retry time, in seconds
 * \param[in] retry_count  tcp allowed number of retries
 * \return                 error code
 */
M_API M_log_error_t M_log_module_tcp_syslog_set_keepalives(M_log_t *log, M_log_module_t *module, M_uint64 idle_time_s,
	M_uint64 retry_time_s, M_uint64 retry_count);


/*! Associate the given user-defined tag(s) with a syslog priority tag.
 *
 * If you don't associate a tag with a syslog priority, the default priority of \link M_SYSLOG_INFO \endlink will
 * be used for that tag.
 *
 * \see M_LOG_ALL_TAGS
 * \see M_log_all_tags_lt
 * \see M_log_all_tags_lte
 * \see M_log_all_tags_gt
 * \see M_log_all_tags_gte
 *
 * \param[in] log      logger object
 * \param[in] module   handle of module to operate on
 * \param[in] tags     user-defined power-of-two tag (or multiple power-of-two tags, OR'd together)
 * \param[in] priority single priority value to associate with the given tags
 * \return             error code
 */
M_API M_log_error_t M_log_module_tcp_syslog_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_syslog_priority_t priority);

/*! @} */ /* End of tcp_syslog group */




/*! \addtogroup m_log_membuf Memory Buffer Module
 *  \ingroup m_log
 *
 * Functions to enable logging sensitive data to a temporary memory buffer.
 *
 * Membuf modules only accept messages until they are full, then they stop accepting messages and hang out in memory
 * until they're either explicitly retrieved by the caller with M_log_module_take_membuf(), or they reach their
 * expiration time and are automatically destroyed.
 *
 * @{
 */

/*! Add a module to output to a buffer in memory.
 *
 * This is intended for temporary, in-memory storage of sensitive data that can't be stored long-term.
 *
 * Messages are accepted from the time the buffer is added, until the buffer is full. After that, no new messages
 * are stored, and the contents are preserved until either the module is removed, or the expiration time is reached.
 *
 * \param[in]  log           logger object
 * \param[in]  buf_size      max size (in bytes) of memory buffer
 * \param[in]  buf_time_s    max time (in seconds) to allow buffer to exist in memory before it's automatically deleted
 * \param[in]  expire_cb     callback to call when a memory buffer is automatically deleted
 * \param[in]  expire_thunk  any state used by the expire callback (not owned by log, you must delete it yourself)
 * \param[out] out_mod       handle for created module, or \c NULL if there was an error
 * \return                   error code
 */
M_API M_log_error_t M_log_module_add_membuf(M_log_t *log, size_t buf_size, M_uint64 buf_time_s,
	M_log_expire_cb expire_cb, void *expire_thunk, M_log_module_t **out_mod);


/*! Remove a membuf module from the log and return the internal memory store.
 *
 * This method should be used if you need to preserve the data stored in the buffer. If you just want
 * to remove the module and you don't care about the memory buffer contents, you should call the normal
 * M_log_module_remove() function instead.
 *
 * \see M_log_module_remove
 *
 * \param[in]  log     logger object
 * \param[in]  module  handle of module to operate on
 * \param[out] out_buf buffer containing membuf data, or \c NULL if there was an error
 * \return             error code
 */
M_API M_log_error_t M_log_module_take_membuf(M_log_t *log, M_log_module_t *module, M_buf_t **out_buf);

/*! @} */ /* End of membuf group */



__END_DECLS

#endif /* M_LOG_H */
