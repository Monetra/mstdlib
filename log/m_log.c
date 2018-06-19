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

/* Implementations of high-level log functions.
 */
#include "m_config.h"
#include <m_log_int.h>



/* ---- PRIVATE: helper functions ---- */

/* Get line ending string for the given line ending mode. */
static const char *line_end_to_str(M_log_line_end_mode_t mode)
{
	switch (mode) {
		case M_LOG_LINE_END_WINDOWS:
			return "\r\n";
		case M_LOG_LINE_END_UNIX:
			return "\n";
		case M_LOG_LINE_END_NATIVE:
#ifdef _WIN32
			return "\r\n";
#else
			return "\n";
#endif
	}
	/* Silences warning message, shouldn't ever actually reach here. */
	return "\n";
}


/* Convert a log line-end enum value into an async writer line end enum value. */
static M_async_writer_line_end_mode_t line_end_to_writer_enum(M_log_line_end_mode_t mode)
{
	M_async_writer_line_end_mode_t ret = M_ASYNC_WRITER_LINE_END_NATIVE;
	switch (mode) {
		case M_LOG_LINE_END_WINDOWS:
			ret = M_ASYNC_WRITER_LINE_END_WINDOWS;
			break;
		case M_LOG_LINE_END_UNIX:
			ret = M_ASYNC_WRITER_LINE_END_UNIX;
			break;
		case M_LOG_LINE_END_NATIVE:
			ret = M_ASYNC_WRITER_LINE_END_NATIVE;
			break;
	}
	return ret;
}


/* Trim whitespace from end of buffer. */
static void buf_trim_end(M_buf_t *buf)
{
	size_t      len = M_buf_len(buf);
	const char *ptr = M_buf_peek(buf);

	if (len == 0) {
		return;
	}

	while (len > 0 && M_chr_isspace(*(ptr + len - 1))) {
		len--;
	}
	M_buf_truncate(buf, len);
}


static void log_module_destroy(void *modptr)
{
	M_log_module_t *mod = modptr;

	if (mod == NULL) {
		return;
	}

	if (mod->destroy_prefix_thunk_cb != NULL && mod->prefix_thunk != NULL) {
		mod->destroy_prefix_thunk_cb(mod->prefix_thunk);
	}
	if (mod->destroy_filter_thunk_cb != NULL && mod->filter_thunk != NULL) {
		mod->destroy_filter_thunk_cb(mod->filter_thunk);
	}
	if (mod->destroy_module_thunk_cb != NULL && mod->module_thunk != NULL) {
		mod->destroy_module_thunk_cb(mod->module_thunk, mod->flush_on_destroy);
	}

	M_free(mod);
}


/* Returns NULL if the given time format was invalid.
 *
 * TODO: update M_time_to_str() to provide the functionality we need for this (will need to support useconds)
 */
static char *get_current_time_str(const char *time_format, size_t *out_len)
{
	static const char *days_of_week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *months_of_year[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
		"Aug", "Sep", "Oct", "Nov", "Dec" };

	M_timeval_t       tv;
	M_time_localtm_t  ltime;
	M_int64           abs_gmtoff;
	M_buf_t          *buf;
	size_t            i;
	size_t            fmt_len;

	if (out_len != NULL) {
		*out_len = 0;
	}

	if (M_str_isempty(time_format)) {
		return NULL;
	}

	fmt_len = M_str_len(time_format);
	buf     = M_buf_create();

	/* Get current time. Use gettimeofday so we have access to microseconds. */
	M_mem_set(&tv, 0, sizeof(tv));
	M_time_gettimeofday(&tv);
	M_time_tolocal(tv.tv_sec, &ltime, NULL);

	abs_gmtoff = M_ABS(ltime.gmtoff);

	for (i=0; i<fmt_len; i++) {
		if (time_format[i] != '%') {
			M_buf_add_char(buf, time_format[i]);
			continue;
		}
		i++;
		switch(time_format[i]) {
			case 't': /* Unix timestamp */
				M_buf_add_int(buf, tv.tv_sec);
				break;
			case 'M': /* Month (2-digit) */
				M_buf_add_int_just(buf, ltime.month, 2);
				break;
			case 'a': /* Month (abbreviated string) */
				M_buf_add_str(buf, months_of_year[ltime.month - 1]);
				break;
			case 'D': /* Day of Month (2-digit) */
				M_buf_add_int_just(buf, ltime.day, 2);
				break;
			case 'd': /* Day of Week (abbreviated string) */
				M_buf_add_str(buf, days_of_week[ltime.wday]);
				break;
			case 'Y': /* Year (4-digit) */
				M_buf_add_int_just(buf, ltime.year, 4);
				break;
			case 'y': /* Year (2-digit) */
				M_buf_add_int_just(buf, ltime.year2, 2);
				break;
			case 'H': /* Hour (2-digit) */
				M_buf_add_int_just(buf, ltime.hour, 2);
				break;
			case 'm': /* Minute (2-digit) */
				M_buf_add_int_just(buf, ltime.min, 2);
				break;
			case 's': /* Second (2-digit) */
				M_buf_add_int_just(buf, ltime.sec, 2);
				break;
			case 'u': /* Microsecond (6-digit) */
				M_buf_add_int_just(buf, tv.tv_usec, 6);
				break;
			case 'z': /* Timezone offset */
				M_buf_add_char(buf, (ltime.gmtoff > 0)? '+' : '-');
				M_buf_add_int_just(buf, abs_gmtoff / (60 * 60), 2);
				M_buf_add_int_just(buf, (abs_gmtoff / 60) % 60, 2);
				break;
			case '%':  /* Escaped percent sign ('%%' --> '%') */
			case '\0': /* String ended in the middle of a format field (hanging %). */
				M_buf_add_char(buf, '%');
				break;
			default:   /* Unrecognized format field identifier - print the identifier, instead of replacing it. */
				M_buf_add_char(buf, '%');
				M_buf_add_char(buf, time_format[i]);
				break;
		}
	}

	return M_buf_finish_str(buf, out_len);
}


/* ---- PUBLIC: tag list helpers ---- */

M_uint64 M_log_all_tags_lt(M_uint64 tag)
{
	tag = M_uint64_round_down_to_power_of_two(tag);
	return (tag == 0)? 0 : (tag - 1);
}


M_uint64 M_log_all_tags_lte(M_uint64 tag)
{
	tag = M_uint64_round_down_to_power_of_two(tag);
	return (tag == 0)? 0 : ((tag - 1) | tag);
}


M_uint64 M_log_all_tags_gt(M_uint64 tag)
{
	return ~M_log_all_tags_lte(tag);
}


M_uint64 M_log_all_tags_gte(M_uint64 tag)
{
	return ~M_log_all_tags_lt(tag);
}



/* ---- PUBLIC: General log functions ---- */

const char *M_log_err_to_str(M_log_error_t err)
{
	switch(err) {
		case M_LOG_SUCCESS:
			return "";
		case M_LOG_INVALID_PARAMS:
			return "invalid params";
		case M_LOG_INVALID_PATH:
			return "given filesystem path couldn't be normalized";
		case M_LOG_INVALID_TAG:
			return "expected a single, non-zero, power-of-two value for the tag field";
		case M_LOG_NO_EVENT_LOOP:
			return "no event loop specified for log, can't use event-based modules";
		case M_LOG_SUSPENDED:
			return "log has been suspended, can't take the requested action until after resume is called";
		case M_LOG_DUPLICATE_TAG_NAME:
			return "given name has already been assigned to a different tag";
		case M_LOG_UNREACHABLE:
			return "requested I/O resource could not be accessed";
		case M_LOG_INVALID_TIME_FORMAT:
			return "given time format string is invalid (can't be parsed)";
		case M_LOG_MODULE_UNSUPPORTED:
			return "requested module type not supported by this platform or configuration";
		case M_LOG_MODULE_NOT_FOUND:
			return "requested module has already been removed";
		case M_LOG_WRONG_MODULE:
			return "module-specific function was run on the wrong module";
		case M_LOG_GENERIC_FAIL:
			return "internal error";
	}
	return "unknown";
}


M_log_t *M_log_create(M_log_line_end_mode_t mode, M_bool flush_on_destroy, M_event_t *event)
{
	M_log_t                  *log = M_malloc_zero(sizeof(*log));
	struct M_llist_callbacks  cbs = {M_sort_compar_vp, NULL, NULL, log_module_destroy};

	log->modules              = M_llist_create(&cbs, M_LLIST_SORTED);
	log->line_end_writer_mode = line_end_to_writer_enum(mode);
	log->flush_on_destroy     = flush_on_destroy;
	log->line_end_str         = line_end_to_str(mode);
	log->time_format          = M_strdup("%a %D %H:%m:%s.%u %z");
	log->lock                 = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	log->event                = event;

	return log;
}


void M_log_destroy(M_log_t *log)
{
	if (log == NULL) {
		return;
	}

	M_llist_destroy(log->modules, M_TRUE); /* calls log_module_destroy() on each module */
	M_free(log->time_format);
	M_thread_mutex_destroy(log->lock);
	M_hash_u64str_destroy(log->tag_to_name);
	M_hash_multi_destroy(log->name_to_tag);

	M_free(log);
}


void M_log_destroy_blocking(M_log_t *log, M_uint64 timeout_ms)
{
	M_timeval_t     t;
	M_llist_node_t *node = NULL;

	if (log == NULL) {
		return;
	}

	M_time_elapsed_start(&t);

	M_thread_mutex_lock(log->lock);

	/* Destroy each log module's thunk (blocking, if possible). */
	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_uint64        elapsed;
		M_log_module_t *mod;

		elapsed = M_time_elapsed(&t);

		mod = M_llist_node_val(node);

		if (mod != NULL && mod->module_thunk != NULL) {
			if (mod->destroy_module_thunk_blocking_cb != NULL && (timeout_ms == 0 || elapsed < timeout_ms)) {
				M_uint64 next_timeout = (timeout_ms == 0)? 0 : timeout_ms - elapsed;
				mod->destroy_module_thunk_blocking_cb(mod->module_thunk, mod->flush_on_destroy, next_timeout);
			} else if (mod->destroy_module_thunk_cb != NULL) {
				/* Use async destructor if blocking destructor not provided, or if timeout has expired. */
				mod->destroy_module_thunk_cb(mod->module_thunk, mod->flush_on_destroy);
			}
			/* Set module_thunk to NULL so nothing else tries to delete it. */
			mod->module_thunk = NULL;
		}

		node = M_llist_node_next(node);
	}

	M_thread_mutex_unlock(log->lock);

	/* Destroy the log and all modules, after we've already launched a destroy on each module's thunk. */
	M_log_destroy(log);

	/* Give modules a little extra time to wrap up. */
	M_thread_sleep(500);
}


M_log_error_t M_log_set_time_format(M_log_t *log, const char *fmt)
{
	char *test_str = NULL;

	if (log == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	/* Only let the time format be changed if the new format is valid. */
	test_str = get_current_time_str(fmt, NULL);
	if (test_str == NULL) {
		return M_LOG_INVALID_TIME_FORMAT;
	}
	M_free(test_str);

	/* Copy new time format to log. */
	M_thread_mutex_lock(log->lock);

	M_free(log->time_format);
	log->time_format = M_strdup(fmt);

	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}


M_log_error_t M_log_set_tag_name(M_log_t *log, M_uint64 tag, const char *name)
{
	const char *old_name  = NULL;
	M_uint64    other_tag;

	if (log == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	if (!M_uint64_is_power_of_two(tag)) {
		return M_LOG_INVALID_TAG;
	}

	M_thread_mutex_lock(log->lock);

	if (log->tag_to_name == NULL) {
		log->tag_to_name = M_hash_u64str_create(16, 75, M_HASH_U64STR_NONE);
	}
	if (log->name_to_tag == NULL) {
		log->name_to_tag = M_hash_multi_create(M_HASH_MULTI_STR_CASECMP);
	}

	/* Check to see if new name has already been assigned to another tag. */
	if (M_hash_multi_str_get_uint(log->name_to_tag, name, &other_tag) && other_tag != tag) {
		return M_LOG_DUPLICATE_TAG_NAME;
	}

	/* Remove old name-to-tag mapping. */
	if (M_hash_u64str_get(log->tag_to_name, tag, &old_name)) {
		M_hash_multi_str_remove(log->name_to_tag, old_name, M_TRUE);
	}

	if (M_str_isempty(name)) {
		/* If name is empty, remove the tag-to-name mapping, too. */
		M_hash_u64str_remove(log->tag_to_name, tag);

		/* If removing the old name may reduce the max_name_width, need to recalculate it. */
		if (M_str_len(old_name) >= log->max_name_width) {
			size_t                new_max;
			const char           *iter_name;
			M_hash_u64str_enum_t *hashenum;
			M_hash_u64str_enumerate(log->tag_to_name, &hashenum);

			new_max = 0;
			while (M_hash_u64str_enumerate_next(log->tag_to_name, hashenum, NULL, &iter_name)) {
				size_t name_len = M_str_len(iter_name);
				if (name_len > new_max) {
					new_max = name_len;
				}
			}
			log->max_name_width = new_max;

			M_hash_u64str_enumerate_free(hashenum);
		}
	} else {
		size_t name_len;

		/* Otherwise, add new tag-to-name and name-to-tag mappings. */
		M_hash_u64str_insert(log->tag_to_name, tag, name);
		M_hash_multi_str_insert_uint(log->name_to_tag, name, tag);

		/* Update variable that keeps track of longest name we've seen so far. */
		name_len = M_str_len(name);
		if (name_len > log->max_name_width) {
			log->max_name_width = name_len;
		}
	}

	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}


const char *M_log_get_tag_name(M_log_t *log, M_uint64 tag)
{
	const char *ret = NULL;

	if (log == NULL || !M_uint64_is_power_of_two(tag)) {
		return NULL;
	}

	M_thread_mutex_lock(log->lock);

	if (!M_hash_u64str_get(log->tag_to_name, tag, &ret)) {
		ret = NULL;
	}

	M_thread_mutex_unlock(log->lock);

	return ret;
}


M_uint64 M_log_get_tag(M_log_t *log, const char *name)
{
	M_uint64 ret = 0;

	if (log == NULL || M_str_isempty(name)) {
		return 0;
	}

	M_thread_mutex_lock(log->lock);

	if (!M_hash_multi_str_get_uint(log->name_to_tag, name, &ret)) {
		ret = 0;
	}

	M_thread_mutex_unlock(log->lock);

	return ret;
}


M_log_error_t M_log_set_tag_names_padded(M_log_t *log, M_bool padded)
{
	if (log == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	M_thread_mutex_lock(log->lock);

	log->pad_names = padded;

	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}


M_log_error_t M_log_printf(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *fmt, ...)
{
	M_log_error_t ret;
	va_list       ap;

	va_start(ap, fmt);
	ret = M_log_vprintf(log, tag, msg_thunk, fmt, ap);
	va_end(ap);

	return ret;
}


M_log_error_t M_log_vprintf(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *fmt, va_list ap)
{
	M_log_error_t  ret;
	char          *msg;

	/* Expand message string for this log entry from the format string. */
	M_vasprintf(&msg, fmt, ap);

	ret = M_log_write(log, tag, msg_thunk, msg);

	M_free(msg);

	return ret;
}


M_log_error_t M_log_write(M_log_t *log, M_uint64 tag, void *msg_thunk, const char *msg)
{
	M_log_error_t   ret          = M_LOG_SUCCESS;
	M_llist_node_t *node         = NULL;
	M_buf_t        *buf          = NULL;
	char           *time_str     = NULL;
	size_t          time_str_len = 0;
	const char     *name_str     = NULL;
	size_t          name_str_len = 0;
	const char     *line_start   = NULL;
	M_bool          tag_used;

	M_llist_t      *expired_mods = NULL;


	if (log == NULL || msg == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	if (!M_uint64_is_power_of_two(tag)) {
		return M_LOG_INVALID_TAG;
	}

	M_thread_mutex_lock(log->lock);

	/* If this tag is disabled for all modules, skip it. This is an optimization, worth it since this
	 * case happens a lot.
	 */
	tag_used = M_FALSE;
	node     = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);
		if ((mod->accepted_tags & tag) != 0) {
			tag_used = M_TRUE;
			break;
		}
		node = M_llist_node_next(node);
	}
	if (!tag_used) {
		goto done;
	}


	/* Construct time string for this log message (log must be locked when we do this, format string can change). */
	time_str = get_current_time_str(log->time_format, &time_str_len);
	if (time_str == NULL) {
		ret = M_LOG_INVALID_TIME_FORMAT;
		goto done;
	}

	/* Get tag name (if any). */
	M_hash_u64str_get(log->tag_to_name, tag, &name_str);
	name_str_len = M_str_len(name_str);

	/* Loop over each line of log message. */
	line_start = msg;
	buf        = M_buf_create();
	while (!M_str_isempty(line_start)) {
		const char *line_end;
		size_t      line_len;

		/* Parse out current line of log message (not including line end chars). */
		line_end = M_str_find_first_from_charset(line_start, "\r\n");
		line_len = (line_end == NULL)? M_str_len(line_start) : (size_t)(line_end - line_start);

		/* Loop over every running module, output current line to it. */
		node = M_llist_first(log->modules);
		while (node != NULL) {
			M_llist_node_t *curr;
			M_log_module_t *mod;

			curr = node;
			mod  = M_llist_node_val(curr);
			node = M_llist_node_next(curr);

			if (mod->module_write_cb == NULL) {
				continue;
			}

			/* If this module doesn't accept messages with this tag, skip it. */
			if ((mod->accepted_tags & tag) == 0) {
				continue;
			}

			/* If module has become invalid, remove from list of nodes and skip it. */
			if (mod->module_check_cb != NULL && !mod->module_check_cb(mod)) {
				/* Remove fom log without deleting module, add module to list of expired modules. */
				mod = M_llist_take_node(curr);

				/* Add removed module to list of expired modules. */
				if (expired_mods == NULL) {
					expired_mods = M_llist_create(NULL, M_LLIST_NONE);
				}
				M_llist_insert(expired_mods, mod);

				continue;
			}

			/* If the module's custom filter rejects it, skip it. */
			if (mod->filter_cb != NULL && !mod->filter_cb(tag, mod->filter_thunk, msg_thunk)) {
				continue;
			}

			/* Clear out old contents of buffer. */
			M_buf_truncate(buf, 0);

			/* Time string. */
			M_buf_add_bytes(buf, time_str, time_str_len);

			/* Tag name. */
			if (name_str_len > 0) {
				M_buf_add_str(buf, " [");
				M_buf_add_bytes(buf, name_str, name_str_len);
				M_buf_add_str(buf, "]");
				if (log->pad_names && mod->allow_tag_padding && name_str_len < log->max_name_width) {
					M_buf_add_fill(buf, ' ', log->max_name_width - name_str_len);
				}
			}

			/* Prefix */
			if (mod->prefix_cb == NULL) {
				M_buf_add_str(buf, ": ");
			} else {
				mod->prefix_cb(buf, tag, mod->prefix_thunk, msg_thunk);
			}

			/* Current line of message. */
			M_buf_add_bytes(buf, line_start, line_len);
			buf_trim_end(buf);

			/* Line ending. */
			M_buf_add_str(buf, log->line_end_str);

			/* Ask module to write the message. Module is allowed to modify buffer contents, but it can't destroy the
			 * buffer object itself.
			 */
			mod->module_write_cb(mod, buf, tag);
		} /* END loop over modules */

		/* Get start of next line for next iteration of loop (or NULL, if no more lines). */
		line_start = M_str_find_first_not_from_charset(line_end, "\r\n");
	} /* END loop over lines */

done:
	M_thread_mutex_unlock(log->lock);

	M_buf_cancel(buf);
	M_free(time_str);

	/* Clean up any expired modules. */
	if (expired_mods != NULL) {
		node = M_llist_first(expired_mods);
		while (node != NULL) {
			M_log_module_t *mod = M_llist_node_val(node);
			/* Call the expire callback. */
			if (mod != NULL && mod->module_expire_cb != NULL) {
				mod->module_expire_cb(mod, mod->module_expire_thunk);
			}
			/* Destroy the module. */
			log_module_destroy(mod);

			node = M_llist_node_next(node);
		}
		M_llist_destroy(expired_mods, M_FALSE);
	}

	return ret;
}


void M_log_emergency(M_log_t *log, const char *msg)
{
	/* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
	 *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
	 *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
	 */

	M_llist_node_t *node = NULL;

	/* First, write to all modules BESIDES TCP SYSLOG that advertise an emergency callback. */
	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);

		if (mod->type != M_LOG_MODULE_TSYSLOG && mod->module_emergency_cb != NULL) {
			mod->module_emergency_cb(mod, msg);
		}

		node = M_llist_node_next(node);
	}

	/* Only after those are done, try calling the TCP syslog emergency callbacks (they're somewhat riskier). */
	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);

		if (mod->type == M_LOG_MODULE_TSYSLOG && mod->module_emergency_cb != NULL) {
			mod->module_emergency_cb(mod, msg);
		}

		node = M_llist_node_next(node);
	}
}


void M_log_reopen_all(M_log_t *log)
{
	M_llist_node_t *node = NULL;

	M_thread_mutex_lock(log->lock);

	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);

		if (mod->module_reopen_cb != NULL) {
			mod->module_reopen_cb(mod);
		}

		node = M_llist_node_next(node);
	}

	M_thread_mutex_unlock(log->lock);
}


void M_log_suspend(M_log_t *log)
{
	M_llist_node_t *node = NULL;

	M_thread_mutex_lock(log->lock);

	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);

		if (mod->module_suspend_cb != NULL) {
			mod->module_suspend_cb(mod); /* MAY BLOCK */
		}

		node = M_llist_node_next(node);
	}

	log->event     = NULL;
	log->suspended = M_TRUE;

	M_thread_mutex_unlock(log->lock);
}


void M_log_resume(M_log_t *log, M_event_t *event)
{
	M_llist_node_t *node = NULL;

	M_thread_mutex_lock(log->lock);

	if (!log->suspended) {
		M_thread_mutex_unlock(log->lock);
		return;
	}

	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_log_module_t *mod = M_llist_node_val(node);

		if (mod->module_resume_cb != NULL) {
			mod->module_resume_cb(mod, event);
		}

		node = M_llist_node_next(node);
	}

	log->event     = event;
	log->suspended = M_FALSE;

	M_thread_mutex_unlock(log->lock);
}


M_list_t *M_log_all_modules(M_log_t *log)
{
	M_list_t       *ret;
	M_llist_node_t *node;

	ret = M_list_create(NULL, M_LIST_NONE);

	M_thread_mutex_lock(log->lock);

	node = M_llist_first(log->modules);
	while (node != NULL) {
		M_list_insert(ret, M_llist_node_val(node));
		node = M_llist_node_next(node);
	}

	M_thread_mutex_unlock(log->lock);

	return ret;
}
