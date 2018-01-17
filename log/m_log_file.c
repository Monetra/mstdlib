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

/* Implementation of file system logging module.
 *
 */
#include "m_config.h"
#include <m_log_int.h>

/* TODO: make these config parameters? */
#define M_FILE_RETRY_DELAY  1000 /* (ms) Amount of time to wait after file access failure before we try reopening
                                  *      the file stream. This is intended to limit the rate at which we trigger
                                  *      open requests after there's an I/O error or during a suspend.
                                  */
#define M_POPEN_CLOSE_DELAY 15   /* (ms) Amount of time to wait for a popen call to finish when we're not
                                  *      allowed to block. Should be very short. (resolution is only about 15 ms)
                                  */

/* ---- PRIVATE: callbacks for internal async_writer object. ---- */

typedef struct {
	char             *log_file_path;
	char             *log_file_name;
	char             *log_file_pattern; /* Globbing pattern for old log files (doesn't include directory part). */
	char             *log_file_dir;
	M_time_t          log_file_create_time; /* File creation time (seconds) */
	M_uint64          log_file_size;
	M_fs_file_t      *fstream;
	M_uint64          num_to_keep;
	M_uint64          autorotate_size;
	M_uint64          autorotate_time;
	char             *archive_cmd;
	char             *archive_file_ext;
	M_popen_handle_t *archive_process;
	M_bool            in_err;
	const char       *line_end_str;
	M_bool            suspended;
} writer_thunk_t;


static void writer_thunk_destroy(void *ptr)
{
	writer_thunk_t *wdata = ptr;

	M_free(wdata->log_file_path);
	M_free(wdata->log_file_name);
	M_free(wdata->log_file_pattern);
	M_free(wdata->log_file_dir);
	M_fs_file_close(wdata->fstream);
	M_free(wdata->archive_cmd);
	M_free(wdata->archive_file_ext);

	/* If internal archive process exists and hasn't been closed yet, try to close it.
	 * If the process isn't ready to close in M_POPEN_CLOSE_DELAY seconds, force kill it and free resources.
	 * (Note: this delay should be very short, on the order of a few milliseconds.)
	 */
	M_popen_close_ex(wdata->archive_process, NULL, NULL, NULL, NULL, NULL, M_POPEN_CLOSE_DELAY);

	M_free(wdata);
}


/* Open the head logfile, update file creation time. */
static M_fs_error_t open_head_logfile(writer_thunk_t *wdata, M_bool is_rotate)
{
	M_fs_error_t err;
	size_t       buf_size;

	if (wdata == NULL) {
		return M_FS_ERROR_INVALID;
	}

	/* TODO: do we need the extra buf? It's currently turned off. Need to performance test. */
	buf_size = 0;

	err = M_fs_file_open(&wdata->fstream, wdata->log_file_path, buf_size,
		M_FS_FILE_MODE_WRITE | M_FS_FILE_MODE_APPEND, NULL);

	if (is_rotate) {
		/* We know the file has to be new, so don't bother checking the filesystem for creation time and size.
		 * NOTE: this is an attempted workaround for a windows logging issue we can't reproduce (one line per file)
		 */
		wdata->log_file_create_time = M_time();
		wdata->log_file_size        = 0;
	} else if (err == M_FS_ERROR_SUCCESS) {
		/* If we aren't creating a new file after a rotate, we want to try and get the creation time and size from
		 * the filesystem, in case we just opened an existing file.
		 */
		M_fs_info_t *info;

		M_fs_info(&info, wdata->log_file_path, M_FS_PATH_INFO_FLAGS_BASIC);
		wdata->log_file_create_time = M_fs_info_get_btime(info);
		wdata->log_file_size        = M_fs_info_get_size(info);

		/* If we couldn't get a file creation time from the filesystem, just use the current time. */
		if (wdata->log_file_create_time <= 0) {
			wdata->log_file_create_time = M_time();
		}

		M_fs_info_destroy(info);
	}

	return err;
}


static int sort_log_files_cb(const void *arg1, const void *arg2, void *thunk)
{
	const size_t *skip_len = thunk;
	const char   *str1     = *(char * const *)arg1;
	const char   *str2     = *(char * const *)arg2;
	M_parser_t   *p1;
	M_parser_t   *p2;
	M_uint64      num1;
	M_uint64      num2;
	int           ret;

	p1 = M_parser_create_const((const unsigned char *)str1, M_str_len(str1), M_PARSER_FLAG_NONE);
	p2 = M_parser_create_const((const unsigned char *)str2, M_str_len(str2), M_PARSER_FLAG_NONE);

	/* Skip past log file name and dot. */
	if (!M_parser_consume(p1, *skip_len) || !M_parser_consume(p2, *skip_len)) {
		ret = M_str_cmpsort(str1, str2);
		goto done;
	}

	/* Read number part of each log file name. */
	if (!M_parser_read_uint(p1, M_PARSER_INTEGER_ASCII, 0, 10, &num1)
		|| !M_parser_read_uint(p2, M_PARSER_INTEGER_ASCII, 0, 10, &num2))
	{
		ret = M_str_cmpsort(str1, str2);
		goto done;
	}

	/* If we were able to parse the number part, sort on that. Want to sort in descending order (highest first)*/
	if (num1 > num2) {
		ret = -1;
	} else if (num1 < num2) {
		ret = 1;
	} else {
		ret = 0;
	}

done:
	M_parser_destroy(p1);
	M_parser_destroy(p2);
	return ret;
}


/* Return list of old log files (<log file name>.<number>[<archive file ext>]) on disk. */
static M_llist_str_t *writer_thunk_get_log_file_names(writer_thunk_t *wdata)
{
	M_list_str_t  *glob_files;
	M_llist_str_t *sorted_files;
	size_t         i;
	size_t         min_len;

	if (wdata == NULL) {
		return NULL;
	}

	/* Glob files in dir to get a first cut at the list of files we need:
	 *    <log file name>.*[<archive_file_ext>]
	 *
	 * Note that this glob pattern will still include some files that don't match the exact pattern we're looking for,
	 * so we'll need to filter it further.
	 */
	glob_files = M_fs_dir_walk_strs(wdata->log_file_dir, wdata->log_file_pattern, M_FS_DIR_WALK_FILTER_FILE);

	if (M_list_str_len(glob_files) == 0) {
		M_list_str_destroy(glob_files);
		return NULL;
	}

	/* Length of log file name + dot char. */
	min_len = M_str_len(wdata->log_file_name) + 1;

	/* Files will be sorted in descending order (highest number --> lowest number). */
	sorted_files = M_llist_str_create(M_LLIST_STR_SORTDESC); /* Must create with sorting enabled, for change sorting to work. */
	M_llist_str_change_sorting(sorted_files, sort_log_files_cb, &min_len);

	/* Store and sort every file that matches our exact pattern (<log file name>.[number]<archive file ext>). */
	for (i=0; i<M_list_str_len(glob_files); i++) {
		const char *name;
		M_parser_t *parser;
		size_t      ndigits;

		name   = M_list_str_at(glob_files, i);
		parser = M_parser_create_const((const unsigned char *)name, M_str_len(name), M_PARSER_FLAG_NONE);

		/* Skip past log file name and dot. */
		if (!M_parser_consume(parser, min_len)) {
			M_parser_destroy(parser);
			continue;
		}

		/* Skip past digits, make sure we read at least one. */
		ndigits = M_parser_consume_str_charset(parser, "0123456789");
		if (ndigits == 0) {
			M_parser_destroy(parser);
			continue;
		}

		if (M_str_isempty(wdata->archive_file_ext)) {
			/* If there's no archive extension, make sure we've reached the end of the filename. */
			if (M_parser_len(parser) > 0) {
				M_parser_destroy(parser);
				continue;
			}
		} else if (!M_parser_compare_str(parser, wdata->archive_file_ext, 0, M_FALSE)){
			/* Make sure that filename ends with archive file extension, if one was provided. */
			M_parser_destroy(parser);
			continue;
		}

		/* If we get to here, the filename matches our exact requirements. */
		M_llist_str_insert(sorted_files, name);
		M_parser_destroy(parser);
	}

	M_list_str_destroy(glob_files);
	return sorted_files;
}


static void writer_thunk_rotate_log_files(writer_thunk_t *wdata)
{
	M_llist_str_t      *existing_files; /* Will contain list of existing log files, sorted in descending order. */
	M_llist_str_node_t *node;
	M_buf_t            *new_path;
	size_t              min_len;
	M_fs_error_t        res;

	/* Only allow rotate if head log is open (not in error state). */
	if (wdata == NULL || wdata->fstream == NULL) {
		return;
	}

	new_path       = M_buf_create();

	existing_files = writer_thunk_get_log_file_names(wdata);

	/* Loop over each existing file from highest log number to lowest, bump up each log file's number by 1.
	 * Any extra logs past num_to_keep will be deleted.
	 */
	node    = M_llist_str_first(existing_files);
	min_len = M_str_len(wdata->log_file_name) + 1;
	while (node != NULL) {
		const char *name;
		M_parser_t *parser;
		M_uint64    log_num = 0;
		char       *old_path;

		name   = M_llist_str_node_val(node);
		parser = M_parser_create_const((const unsigned char *)name, M_str_len(name), M_PARSER_FLAG_NONE);

		old_path = M_fs_path_join(wdata->log_file_dir, name, M_FS_SYSTEM_AUTO);

		/* Parse log number out of input filename, then increment it. */
		M_parser_consume(parser, min_len);
		M_parser_read_uint(parser, M_PARSER_INTEGER_ASCII, 0, 10, &log_num);
		log_num++;

		/* NOTE: log numbers start at 1, not 0. */

		if (log_num <= wdata->num_to_keep) {
			/* If new log number is in bounds, rename the file to use the new number. */
			M_buf_truncate(new_path, 0); /* Clean out any leftovers from previous use of buffer. */

			M_buf_add_str(new_path, wdata->log_file_path);
			M_buf_add_byte(new_path, '.');
			M_buf_add_uint(new_path, log_num);
			M_buf_add_str(new_path, wdata->archive_file_ext);

			M_fs_move(old_path, M_buf_peek(new_path), M_FS_FILE_MODE_OVERWRITE, NULL, M_FS_PROGRESS_NOEXTRA);
		} else {
			/* If new log number exceeds the number we want to keep, delete the log. */
			M_fs_delete(old_path, M_FALSE, NULL, M_FS_PROGRESS_NOEXTRA);
		}

		M_free(old_path);
		M_parser_destroy(parser);
		node = M_llist_str_node_next(node);
	}

	/* Wait for archive command from previous rotate to finish, if it hasn't already. */
	M_popen_close(wdata->archive_process, NULL);

	/* Close the head log file, rename it to log #1. */
	M_fs_file_close(wdata->fstream);
	wdata->fstream = NULL;

	if (wdata->num_to_keep == 0) {
		/* If we're not keeping any old files, just delete the main logfile. */
		M_fs_delete(wdata->log_file_path, M_FALSE, NULL, M_FS_PROGRESS_NOEXTRA);
	} else {
		/* If we are keeping old files, rename main logfile with ".1" extension, then compress if requested. */
		M_buf_truncate(new_path, 0); /* Clean out any leftovers from previous use of buffer. */
		M_buf_add_str(new_path, wdata->log_file_path);
		M_buf_add_str(new_path, ".1");
		res = M_fs_move(wdata->log_file_path, M_buf_peek(new_path), M_FS_FILE_MODE_OVERWRITE, NULL,
			M_FS_PROGRESS_NOEXTRA);

		/* Handle any required compression in a separate process (only if move was successful). */
		if (res == M_FS_ERROR_SUCCESS && !M_str_isempty(wdata->archive_file_ext)) {
			M_buf_t *cmd = M_buf_create();

			/* cmd: <archive cmd> <logfilename.1> */
			M_buf_add_str(cmd, wdata->archive_cmd);
			M_buf_add_str(cmd, " \"");
			M_buf_add_str(cmd, M_buf_peek(new_path));
			M_buf_add_byte(cmd, '\"');

			wdata->archive_process = M_popen(M_buf_peek(cmd), NULL);

			M_buf_cancel(cmd);
		}
	}

	/* Open a new head log file. */
	open_head_logfile(wdata, M_TRUE);

	M_buf_cancel(new_path);

	M_llist_str_destroy(existing_files);
}


static writer_thunk_t *writer_thunk_create(const char *log_file_path, M_uint64 num_to_keep, M_uint64 autorotate_size,
	M_uint64 autorotate_time, const char *archive_cmd, const char *archive_file_ext, const char *line_end_str)
{
	writer_thunk_t *wdata         = M_malloc_zero(sizeof(*wdata));
	M_buf_t        *buf           = NULL;
	M_fs_error_t    err;

	/* Normalize the path - subs in environment variable values, converts to absolute, resolves '~', etc. */
	err = M_fs_path_norm(&wdata->log_file_path, log_file_path, M_FS_PATH_NORM_ABSOLUTE | M_FS_PATH_NORM_HOME,
		M_FS_SYSTEM_AUTO);
	if (err != M_FS_ERROR_SUCCESS) {
		M_free(wdata);
		return NULL;
	}

	/* Construct globbing pattern for extra log files. */
	wdata->log_file_name = M_fs_path_basename(wdata->log_file_path, M_FS_SYSTEM_AUTO);
	buf = M_buf_create();
	M_buf_add_str(buf, wdata->log_file_name);
	M_buf_add_str(buf, ".*");
	if (!M_str_isempty(archive_file_ext)) {
		M_buf_add_str(buf, archive_file_ext);
	}
	wdata->log_file_pattern = M_strdup(M_buf_peek(buf));
	M_buf_cancel(buf);

	/* Set other parameters. */
	wdata->log_file_dir     = M_fs_path_dirname(wdata->log_file_path, M_FS_SYSTEM_AUTO);
	wdata->num_to_keep      = num_to_keep;
	wdata->autorotate_size  = autorotate_size;
	wdata->autorotate_time  = autorotate_time;
	wdata->archive_cmd      = M_strdup(archive_cmd);
	wdata->archive_file_ext = M_strdup(archive_file_ext);
	wdata->line_end_str     = line_end_str;

	return wdata;
}


static void writer_thunk_stop(void *ptr)
{
	writer_thunk_t *wdata = ptr;
	/* Block until internal archive process finishes (if one was started). */
	M_popen_close(wdata->archive_process, NULL);
	/* Set to NULL so that close won't be called again on destroy. */
	wdata->archive_process = NULL;
}


static M_bool writer_write_cb(char *msg, M_uint64 cmd, void *thunk)
{
	writer_thunk_t *wdata    = thunk;
	size_t          msg_len  = M_str_len(msg);
	M_fs_error_t    res;
	M_bool          ret      = M_TRUE;
	M_bool          dorotate = M_FALSE;

	if (wdata == NULL) {
		return M_FALSE;
	}

	/* If we just received a resume command, update the suspended flag. */
	if ((cmd & M_LOG_CMD_RESUME) != 0) {
		wdata->suspended = M_FALSE;
	}

	/* If we're currently suspended, return write failure. Message will be placed back on queue (if possible). */
	if (wdata->suspended) {
		/* Sleep, so the worker thread doesn't busy-wait the whole time it's suspended. */
		M_thread_sleep(M_LOG_SUSPEND_DELAY * 1000); /* function expects microseconds, not milliseconds */
		return M_FALSE;
	}

	/* Reopen the file if the stream was closed due to a previous error, or if explicitly requested by user.
	 *
	 * Note: must do this before the file rotate conditions checks are done below, because open_head_logfile()
	 *       also updates our internal file size counter to match size of file on disk.
	 */
	if (wdata->fstream == NULL || (cmd & M_LOG_CMD_FILE_REOPEN) != 0) {
		M_fs_file_close(wdata->fstream);
		open_head_logfile(wdata, M_FALSE);
	}

	/* Detect conditions that require a file rotate. */
	if ((cmd & M_LOG_CMD_FILE_ROTATE) != 0 && wdata->log_file_size > 0) {
		/* Rotate if rotate command received, and current head logfile isn't empty. */
		dorotate = M_TRUE;
	} else if (wdata->autorotate_time > 0 && M_time() > (wdata->log_file_create_time + (M_time_t)wdata->autorotate_time)) {
		/* Rotate if head logfile exceeds our age limit (if set) */
		dorotate = M_TRUE;
	} else if (wdata->autorotate_size > 0 && wdata->log_file_size > wdata->autorotate_size) {
		/* Rotate if head logfile exceeds our size limit (if set) */
		dorotate = M_TRUE;
	}

	if (dorotate) {
		writer_thunk_rotate_log_files(wdata);
	}

	/* If a suspend was requested (and we didn't receive a resume at the same time), update the suspend
	 * flag, close the file stream, and skip writing the current message (will be added back onto queue).
	 *
	 * This should be the LAST command we process, otherwise we'll lose any commands that are in flight.
	 */
	if ((cmd & M_LOG_CMD_SUSPEND) != 0 && (cmd & M_LOG_CMD_RESUME) == 0) {
		M_fs_file_close(wdata->fstream);
		wdata->fstream   = NULL;
		wdata->suspended = M_TRUE;
		return M_FALSE;
	}

	/* Write out the current message (if it's not empty). */
	if (msg_len != 0) {
		unsigned char *to_write     = (unsigned char *)msg;
		size_t         to_write_len = msg_len;

		/* If we just recovered from an error, prepend log line with a separate line documenting this. */
		if (wdata->in_err) {
			M_buf_t *buf = M_buf_create();
			M_buf_add_str(buf, "Log file stream reopened due to I/O error.");
			M_buf_add_str(buf, wdata->line_end_str);
			M_buf_add_bytes(buf, msg, msg_len);
			to_write = M_buf_finish(buf, &to_write_len);
		}

		res = M_fs_file_write(wdata->fstream, to_write, to_write_len, NULL, M_FS_FILE_RW_FULLBUF);
		wdata->log_file_size += to_write_len;

		/* Free message buffer, if we sent an error message. */
		if (wdata->in_err) {
			M_free(to_write);
		}

		if (res == M_FS_ERROR_SUCCESS) {
			/* If write succeeded, clear error indicator. */
			wdata->in_err = M_FALSE;
		} else {
			/* If we failed to write to the stream, need to push message back onto queue, and try to reopen
			 * resource on next write.
			 *
			 * Note: don't need to update file size here, will be refreshed by checking the disk on reopen.
			 */
			M_fs_file_close(wdata->fstream);

			wdata->in_err  = M_TRUE;
			wdata->fstream = NULL;
			ret            = M_FALSE;

			M_thread_sleep(M_FILE_RETRY_DELAY * 1000); /* function expects microseconds, not milliseconds */
		}
	}

	return ret;
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


static M_log_error_t log_reopen_cb(M_log_module_t *module)
{
	M_async_writer_t *writer;

	if (module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	writer = module->module_thunk;

	M_async_writer_set_command(writer, M_LOG_CMD_FILE_REOPEN, M_FALSE);

	return M_LOG_SUCCESS;
}


static M_log_error_t log_suspend_cb(M_log_module_t *module)
{
	M_async_writer_t *writer;

	if (module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	writer = module->module_thunk;

	if (M_async_writer_is_running(writer)) {
		/* Notify worker to close its resources and suspend write operations. */
		M_async_writer_set_command_block(writer, M_LOG_CMD_SUSPEND); /* BLOCKING */

		/* Stop the internal worker thread (message queue will still be intact and accepting messages). */
		M_async_writer_stop(writer); /* BLOCKING */
	}

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

	if (!M_async_writer_is_running(writer)) {
 		/* Start a new internal worker thread. */
 		M_async_writer_start(writer);

		/* Notify the internal worker to reopen resources and resume write operations. */
		M_async_writer_set_command(writer, M_LOG_CMD_RESUME, M_TRUE);
	}

	return M_LOG_SUCCESS;
}


static void log_emergency_cb(M_log_module_t *mod, const char *msg)
{
	/* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
	 *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
	 *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
	 */

	M_async_writer_t *writer;
	writer_thunk_t   *wdata;
	size_t            msg_len;

	if (mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	writer  = mod->module_thunk;

	wdata   = M_async_writer_get_thunk(writer);
	msg_len = M_str_len(msg);

	M_fs_file_write(wdata->fstream, (const unsigned char *)msg, msg_len, NULL, M_FS_FILE_RW_FULLBUF);
	wdata->log_file_size += msg_len;
}


static void log_destroy_cb(void *ptr, M_bool flush)
{
	M_async_writer_destroy((M_async_writer_t *)ptr, flush);
}


static M_bool log_destroy_blocking_cb(void *ptr, M_bool flush, M_uint64 timeout_ms)
{
	return M_async_writer_destroy_blocking((M_async_writer_t *)ptr, flush, timeout_ms);
}



/* ---- PUBLIC: file-specific module functions ---- */

M_log_error_t M_log_module_add_file(M_log_t *log, const char *log_file_path, size_t num_to_keep,
	M_uint64 autorotate_size, M_uint64 autorotate_time_s, size_t max_queue_bytes, const char *archive_cmd,
	const char *archive_file_ext, M_log_module_t **out_mod)
{
	writer_thunk_t   *writer_thunk; /* Don't free, ownership will be passed to writer. */
	M_async_writer_t *writer;       /* Don't free, ownership will be passed to mod. */
	M_log_module_t   *mod;          /* Don't free, ownership will be passed to log. */

	if (out_mod != NULL) {
		*out_mod = NULL;
	}

	if (log == NULL || M_str_isempty(log_file_path) || max_queue_bytes == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (log->suspended) {
		return M_LOG_SUSPENDED;
	}

	/* archive_cmd and archive_ext must either both be empty, or both set. */
	if (M_str_isempty(archive_cmd) != M_str_isempty(archive_file_ext)) {
		return M_LOG_INVALID_PARAMS;
	}

	/* Create the writer and its thunk. */
	writer_thunk = writer_thunk_create(log_file_path, num_to_keep, autorotate_size, autorotate_time_s,
		archive_cmd, archive_file_ext, log->line_end_str);
	if (writer_thunk == NULL) {
		return M_LOG_INVALID_PATH;
	}

	if (open_head_logfile(writer_thunk, M_FALSE) != M_FS_ERROR_SUCCESS) {
		/* This early open allows most I/O errors to be caught before logging starts. */
		return M_LOG_UNREACHABLE;
	}

	writer = M_async_writer_create(max_queue_bytes, writer_write_cb, writer_thunk,
		writer_thunk_stop, writer_thunk_destroy, log->line_end_writer_mode);

	/* Create the module, pass the writer to it as its thunk. */
	mod                                   = M_malloc_zero(sizeof(*mod));
	mod->type                             = M_LOG_MODULE_FILE;
	mod->flush_on_destroy                 = log->flush_on_destroy;
	mod->allow_tag_padding                = M_TRUE;
	mod->module_thunk                     = writer;
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

	/* Start the internal writer's worker thread. */
	M_async_writer_start(mod->module_thunk);

	/* Add the module to the log. */
	M_thread_mutex_lock(log->lock);
	M_llist_insert(log->modules, mod);
	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_file_rotate(M_log_t *log, M_log_module_t *module)
{
	M_async_writer_t *writer;

	if (log == NULL || module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	if (module->type != M_LOG_MODULE_FILE) {
		return M_LOG_WRONG_MODULE;
	}

	M_thread_mutex_lock(log->lock);

	if (!module_present_locked(log, module)) {
		M_thread_mutex_unlock(log->lock);
		return M_LOG_MODULE_NOT_FOUND;
	}

	writer = module->module_thunk;
	M_async_writer_set_command(writer, M_LOG_CMD_FILE_ROTATE, M_TRUE);

	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}
