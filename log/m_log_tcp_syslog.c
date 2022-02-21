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

/* Implementation for remote (TCP) syslog logging module.
 *
 * TODO: most of the code in here should be refactored out into an m_event_writer_t class, because it
 *       will be common to any event-based logging module. Wait to do this until we have more than
 *       one event-based module.
 */
#include "m_config.h"
#include <m_log_int.h>


/* Default TCP connection settings. */
#define M_TCP_SYSLOG_RETRY_DELAY      1000 /* ms to wait after disconnect or error before recreating connection */

#define DEFAULT_CONNECT_TIMEOUT       5    /* seconds */
#define DEFAULT_KEEPALIVE_IDLE_TIME   4    /* seconds */
#define DEFAULT_KEEPALIVE_RETRY_TIME  15   /* seconds */
#define DEFAULT_KEEPALIVE_RETRY_COUNT 3


/* Thunk for m_log callbacks. */
typedef struct {
	char                *product;
	M_syslog_facility_t  facility;

	char                *src_host;
	char                *dest_host;
	M_uint16             port;
	M_event_t           *event;
	M_dns_t             *dns;          /* passed in by caller, not owned by log module */
	M_io_t              *io;
	M_event_trigger_t   *trigger;
	const char          *line_end_str;

	M_uint64             connect_timeout_ms;
	M_uint64             keepalive_idle_time_s;
	M_uint64             keepalive_retry_time_s;
	M_uint64             keepalive_retry_count;

	M_syslog_priority_t  tag_to_priority[64];

	/* All the stuff in this last group should be access-controlled using the lock. */
	M_thread_mutex_t    *msg_lock;
	M_llist_str_t       *msgs;
	M_uint64             max_bytes;
	M_uint64             stored_bytes;
	M_uint64             num_dropped;
	M_buf_t             *msg_buf;      /* queue holding bytes of message currently being written to tcp stream */
	M_bool               stop_flag;    /* set to true to disconnect after current message finishes being sent */
	M_bool               flush_flag;   /* should next stop flush the entire message queue first? */
	M_bool               suspend_flag; /* set to true to not reconnect after next disconnect, and to wait
	                                    * to try reconnecting until we've resumed.
	                                    */
	M_bool               exit_flag;    /* set to true to not reconnect after next disconnect, and to destroy
	                                    * the module thunk.
	                                    */
} module_thunk_t;


static void timer_reconnect_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *thunk);

static void io_event_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *thunk);


static module_thunk_t *module_thunk_create(const char *product, M_syslog_facility_t facility, const char *host,
	M_uint16 port, M_event_t *event, M_dns_t *dns, M_uint64 max_bytes, const char *line_end_str)
{
	module_thunk_t *mdata = M_malloc_zero(sizeof(*mdata));
	int             i;

	if (M_str_isempty(product)) {
		product = "-"; /* NILVALUE from RFC 5424 - indicates that no product name was set */
	}

	mdata->product      = M_strdup(product);
	mdata->facility     = facility;
	mdata->src_host     = M_io_net_get_fqdn(); /* get hostname of device we're running on */
	mdata->dest_host    = M_strdup(host);
	mdata->port         = port;
	mdata->event        = event;
	mdata->dns          = dns;
	mdata->line_end_str = line_end_str;

	mdata->connect_timeout_ms     = DEFAULT_CONNECT_TIMEOUT;
	mdata->keepalive_idle_time_s  = DEFAULT_KEEPALIVE_IDLE_TIME;
	mdata->keepalive_retry_time_s = DEFAULT_KEEPALIVE_RETRY_TIME;
	mdata->keepalive_retry_count  = DEFAULT_KEEPALIVE_RETRY_COUNT;

	mdata->msg_lock  = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	mdata->msgs      = M_llist_str_create(M_LLIST_STR_NONE);
	mdata->max_bytes = max_bytes;
	mdata->msg_buf   = M_buf_create();

	/* Initialize tag->priority mapping to default value (INFO). */
	for (i=0; i<64; i++) {
		mdata->tag_to_priority[i] = M_SYSLOG_DEFAULT_PRI;
	}

	return mdata;
}


static M_io_error_t module_thunk_reconnect(module_thunk_t *mdata)
{
	M_io_error_t ret;

	if (mdata == NULL) {
		return M_IO_ERROR_INVALID;
	}

	mdata->trigger = M_event_trigger_add(mdata->event, io_event_cb, mdata);

	mdata->io = NULL;

	ret = M_io_net_client_create(&mdata->io, mdata->dns, mdata->dest_host, mdata->port, M_IO_NET_ANY);

	if (ret == M_IO_ERROR_SUCCESS) {
		M_io_net_set_connect_timeout_ms(mdata->io, mdata->connect_timeout_ms * 1000);
		M_io_net_set_keepalives(mdata->io, mdata->keepalive_idle_time_s, mdata->keepalive_retry_time_s,
			mdata->keepalive_retry_count);
	}

	return ret;
}


static void module_thunk_destroy(module_thunk_t *mdata)
{
	if (mdata == NULL) {
		return;
	}

	M_event_trigger_remove(mdata->trigger);
	mdata->trigger = NULL;

	M_io_destroy(mdata->io);

	M_free(mdata->product);
	M_free(mdata->src_host);
	M_free(mdata->dest_host);
	M_thread_mutex_destroy(mdata->msg_lock);
	M_llist_str_destroy(mdata->msgs);
	M_buf_cancel(mdata->msg_buf);

	M_free(mdata);
}


/* TODO: replace this with improved datestring formatting from M_time, once we get around to implementing it. */
static void add_syslog_header(M_buf_t *buf, module_thunk_t *mdata, M_syslog_priority_t priority)
{
	M_time_localtm_t     now;
	const char          *month_str = "";

	if (buf == NULL || mdata == NULL) {
		return;
	}

	M_time_tolocal(M_time(), &now, NULL);

	switch (now.month) {
		case 1:  month_str = "Jan"; break;
		case 2:  month_str = "Feb"; break;
		case 3:  month_str = "Mar"; break;
		case 4:  month_str = "Apr"; break;
		case 5:  month_str = "May"; break;
		case 6:  month_str = "Jun"; break;
		case 7:  month_str = "Jul"; break;
		case 8:  month_str = "Aug"; break;
		case 9:  month_str = "Sep"; break;
		case 10: month_str = "Oct"; break;
		case 11: month_str = "Nov"; break;
		case 12: month_str = "Dec"; break;
	}

	/* NOTE: must not contain any tabs or newline chars */

	/* WARNING: the formatting here is very strict, in accordance with RFC 3164, pages 7-10. Don't change it. */

	M_bprintf(buf, "<%d>%s %2lld %02lld:%02lld:%02lld %s %.32s: ", (int)((int)mdata->facility | (int)priority), month_str,
		now.day, now.hour, now.min, now.sec, mdata->src_host, mdata->product);
}


/* Add octet-counting framing and syslog header to given message, then add result to given buf. */
static void add_framed_message(M_buf_t *buf, const char *msg, module_thunk_t *mdata, M_syslog_priority_t priority)
{
	M_buf_t *payload;

	payload = M_buf_create();

	add_syslog_header(payload, mdata, priority);

	/* Add bytes from msg to payload, replace tabs while we do so. */
	M_buf_add_str_replace(payload, msg, "\t", M_SYSLOG_TAB_REPLACE);

	/* Truncate payload if message greater than syslog limit. Make sure we still end with the line ending sequence. */
	if (M_buf_len(payload) > M_SYSLOG_MAX_CHARS) {
		M_buf_truncate(payload, M_SYSLOG_MAX_CHARS - M_str_len(mdata->line_end_str));
		M_buf_add_str(payload, mdata->line_end_str);
	}

	/* Add octet-counting framing around message (see RFC 6587). */
	M_buf_add_uint(buf, M_buf_len(payload)); /* number of bytes in payload */
	M_buf_add_byte(buf, ' ');
	M_buf_merge(buf, payload); /* merge payload onto end of buf (destroys payload) */
}


static void get_next_message(module_thunk_t *mdata)
{
	char   *msg;
	size_t  msg_len;

	if (mdata->num_dropped > 0) {
		char drop_msg[128] = {0};
		M_snprintf(drop_msg, sizeof(drop_msg) - 1, "%llu messages were dropped (buffer full)\n",
			(unsigned long long)mdata->num_dropped);
		add_framed_message(mdata->msg_buf, drop_msg, mdata, M_SYSLOG_WARNING);
		mdata->num_dropped = 0;
	}

	msg     = M_llist_str_take_node(M_llist_str_last(mdata->msgs));
	msg_len = M_str_len(msg);

	mdata->stored_bytes -= msg_len;

	M_buf_add_bytes(mdata->msg_buf, msg, msg_len);

	M_free(msg);
}


/* ---- PRIVATE: callbacks for internal IO object. ---- */

static void io_event_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *thunk)
{
	M_io_error_t    err   = M_IO_ERROR_SUCCESS;
	module_thunk_t *mdata = thunk;

	(void)io;

	/* Note: will get TYPE_OTHER event if manually triggered by log_write_cb. */
	if (type == M_EVENT_TYPE_WRITE || type == M_EVENT_TYPE_OTHER || type == M_EVENT_TYPE_CONNECTED) {

		M_thread_mutex_lock(mdata->msg_lock);

		while (err == M_IO_ERROR_SUCCESS) {
			/* If we've finished writing the currrent log message, grab the next message from the message queue
			 * and stick it in the msg buf.
			 */
			if (M_buf_len(mdata->msg_buf) == 0) {
				/* stop_flag being set means that somebody requested a clean disconnect while we were in the middle of
				 * sending a message. Now we're done sending the message, though, so go ahead and register the
				 * disconnect.
				 *
				 * If flush flag is set, don't stop until we've written every message in the queue.
				 */
				if (mdata->stop_flag && (!mdata->flush_flag || M_llist_str_len(mdata->msgs) == 0)) {
					M_io_disconnect(mdata->io);
					mdata->flush_flag = M_FALSE;
					M_thread_mutex_unlock(mdata->msg_lock);
					return;
				}

				if (M_llist_str_len(mdata->msgs) == 0) {
					M_thread_mutex_unlock(mdata->msg_lock);
					return;
				}

				get_next_message(mdata);
			}

			/* Ask TCP layer to send as much of the message as it can. */
			err = M_io_write_from_buf(mdata->io, mdata->msg_buf);
		}

		M_thread_mutex_unlock(mdata->msg_lock);

	} else if (type == M_EVENT_TYPE_DISCONNECTED || type == M_EVENT_TYPE_ERROR) {
		M_thread_mutex_lock(mdata->msg_lock);
		mdata->stop_flag = M_FALSE;
		if (mdata->exit_flag) {
			M_thread_mutex_unlock(mdata->msg_lock);
			/* If the exit flag is set, this is a pure disconnect, not a reconnect - destroy everything and exit. */
			module_thunk_destroy(mdata);
		} else if (mdata->suspend_flag) {
			M_thread_mutex_unlock(mdata->msg_lock);
			/* If the suspend flag is set, disconnect and wait for a call to resume to reconnect. */
			M_io_destroy(mdata->io);
			mdata->io = NULL;
		} else {
			M_thread_mutex_unlock(mdata->msg_lock);
#			if 0
			/* Enqueue a message describing why we're reconnecting. */
			char msg[320] = {0};
			M_str_cpy(msg, sizeof(msg), "Reconnecting due to: ");
			if (type == M_EVENT_TYPE_ERROR) {
				char error[256];
				M_io_get_error_string(mdata->io, error, sizeof(error));
				if (M_str_isempty(error)) {
					M_str_cat(msg, sizeof(msg), "unknown error");
				} else {
					M_str_cat(msg, sizeof(msg), error);
				}
			} else {
				M_str_cat(msg, sizeof(msg), "reopen request");
			}
			/* TODO: figure out what to do with error message (output to another module?) */
			M_fprintf(stderr, "%s\n", msg); /*DEBUG_161*/
#			endif
			/* Destroy the connection, wait a while, then recreate it. */
			M_io_destroy(mdata->io);
			mdata->io = NULL;
			M_event_timer_oneshot(event, M_TCP_SYSLOG_RETRY_DELAY, M_TRUE, timer_reconnect_cb, mdata);
		}
	}
}


static void timer_reconnect_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *thunk)
{
	module_thunk_t *mdata = thunk;

	(void)event;
	(void)io;

	if (type == M_EVENT_TYPE_OTHER) {
		module_thunk_reconnect(mdata);
		M_event_add(event, mdata->io, io_event_cb, mdata);
	}
}



/* ---- PRIVATE: callbacks for log module object. ---- */

static void log_write_cb(M_log_module_t *mod, const char *msg, M_uint64 tag)
{
	module_thunk_t *mdata;
	M_buf_t        *buf;

	if (msg == NULL || mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	mdata = mod->module_thunk;

	buf   = M_buf_create();

	add_framed_message(buf, msg, mdata, mdata->tag_to_priority[M_uint64_log2(tag)]);

	/* Add message to output queue. */

	M_thread_mutex_lock(mdata->msg_lock);

	/* If we're currently flushing the message queue before a destroy, don't let any new messages get added to it.
	 *
	 * This avoids a race condition where we could flush forever in one thread, while another thread keeps adding
	 * new messages to the queue.
	 */
	if (mdata->flush_flag) {
		M_thread_mutex_unlock(mdata->msg_lock);
		return;
	}

	/* If the message is too big to fit in the queue, drop it without wiping out the existing contents of the queue. */
	if (M_buf_len(buf) > mdata->max_bytes) {
		if (mdata->num_dropped < M_UINT64_MAX) {
			mdata->num_dropped++;
		}
		M_thread_mutex_unlock(mdata->msg_lock);
		return;
	}

	/* Insert message into queue. */
	M_llist_str_insert_first(mdata->msgs, M_buf_peek(buf));
	mdata->stored_bytes += M_buf_len(buf);
	M_buf_cancel(buf); /* M_llist_str_insert_first() duplicated the data, we no longer need the buf */

	/* If adding the new message will exceed our queue size limit, drop oldest messages until we have room. */
	while (mdata->stored_bytes > mdata->max_bytes) {
		char   *old;
		size_t  old_len;

		old     = M_llist_str_take_node(M_llist_str_last(mdata->msgs));
		old_len = M_str_len(old);
		M_free(old);

		mdata->stored_bytes -= old_len;
		if (mdata->num_dropped < M_UINT64_MAX) {
			mdata->num_dropped++;
		}
	}

	/* Trigger an event to notify the worker that we've added a new message to the queue (if we're not suspended). */
	if (!mdata->suspend_flag && !mdata->stop_flag) {
		M_event_trigger_signal(mdata->trigger);
	}

	M_thread_mutex_unlock(mdata->msg_lock);
}


static M_log_error_t log_reopen_cb(M_log_module_t *mod)
{
	module_thunk_t *mdata;

	if (mod == NULL || mod->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	mdata = mod->module_thunk;

	M_thread_mutex_lock(mdata->msg_lock);

	/* Don't allow reopens if we're currently suspended. */
	if (mdata->suspend_flag) {
		M_thread_mutex_unlock(mdata->msg_lock);
		return M_LOG_GENERIC_FAIL;
	}

	mdata->stop_flag = M_TRUE;

	/* If we don't have a partial message pending, go ahead and queue up a disconnect event.
	 * Otherwise, the stop_flag will ensure that a disconnect gets queued after the partial message
	 * is fully sent.
	 */
	if (M_buf_len(mdata->msg_buf) == 0) {
		M_io_disconnect(mdata->io);
	}

	M_thread_mutex_unlock(mdata->msg_lock);

	return M_LOG_SUCCESS;
}


static M_log_error_t log_suspend_cb(M_log_module_t *mod)
{
	module_thunk_t *mdata;

	if (mod == NULL || mod->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	mdata = mod->module_thunk;

	M_thread_mutex_lock(mdata->msg_lock);

	mdata->suspend_flag = M_TRUE;

	/* This will be destroyed when the event loop is destroyed but let's be explicit. */
	M_event_trigger_remove(mdata->trigger);

	M_thread_mutex_unlock(mdata->msg_lock);

	/* Issue non-graceful disconnect, caller will need to wait until done, then destroy the event loop. */
	M_io_destroy(mdata->io);
	mdata->io      = NULL;
	mdata->event   = NULL;
	mdata->trigger = NULL;

	return M_LOG_SUCCESS;
}


static M_log_error_t log_resume_cb(M_log_module_t *mod, M_event_t *event)
{
	module_thunk_t *mdata;

	if (mod == NULL || mod->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	mdata = mod->module_thunk;

	M_thread_mutex_lock(mdata->msg_lock);

	mdata->suspend_flag = M_FALSE;
	mdata->event        = event;

	module_thunk_reconnect(mdata);
	M_event_add(mdata->event, mdata->io, io_event_cb, mdata);

	M_thread_mutex_unlock(mdata->msg_lock);

	return M_LOG_SUCCESS;
}


static void log_emergency_cb(M_log_module_t *mod, const char *msg)
{
	/* NOTE: this is an emergency method, intended to be called from a signal handler as a last-gasp
	 *       attempt to get out a message before crashing. So, we don't want any mutex locks or mallocs
	 *       in here. HORRIBLY DANGEROUS, MAY RESULT IN WEIRD ISSUES DUE TO THREAD CONFLICTS.
	 */

	module_thunk_t *mdata;
	size_t          msg_len;
	size_t          len_written;
	M_io_error_t    err         = M_IO_ERROR_SUCCESS;

	if (mod == NULL || mod->module_thunk == NULL) {
		return;
	}

	mdata = mod->module_thunk;

	msg_len     = M_str_len(msg);
	len_written = 0;

	/* Try direct-writing to io layer. May or may not work, depending on exact composition of layer. */
	while (err == M_IO_ERROR_SUCCESS && len_written < msg_len) {
		size_t next_write;
		err = M_io_write(mdata->io, (const unsigned char *)(msg + len_written), msg_len - len_written, &next_write);
		len_written += next_write;
	}
}


static void log_destroy_cb(void *thunk, M_bool flush)
{
	module_thunk_t *mdata = thunk;

	if (mdata == NULL) {
		return;
	}

	if (mdata->io == NULL) {
		/* If io object is already destroyed (due to active suspend, or error), just kill the whole module thunk. */
		module_thunk_destroy(mdata);
	} else {

		M_thread_mutex_lock(mdata->msg_lock);

		mdata->stop_flag  = M_TRUE;

		mdata->flush_flag = flush;

		/* The exit flag tells module thunk to destroy itself after the disconnect finishes, instead of reconnecting. */
		mdata->exit_flag  = M_TRUE;

		/* If we don't have any messages left we need to write, go ahead and queue up a disconnect event.
		 * Otherwise, the stop_flag will ensure that a disconnect gets queued by the event handler when
		 * we're ready to disconnect.
		 */
		if (M_buf_len(mdata->msg_buf) == 0 && (!flush || M_llist_str_len(mdata->msgs) == 0)) {
			M_io_disconnect(mdata->io);
		} else {
			M_event_trigger_signal(mdata->trigger);
		}

		M_thread_mutex_unlock(mdata->msg_lock);
	}
}



/* ---- PUBLIC: tcp_syslog-specific module functions ---- */

M_log_error_t M_log_module_add_tcp_syslog(M_log_t *log, const char *product, M_syslog_facility_t facility,
	const char *host, M_uint16 port, M_dns_t *dns, size_t max_queue_bytes, M_log_module_t **out_mod)
{
	module_thunk_t *mdata;
	M_log_module_t *mod;


	if (out_mod != NULL) {
		*out_mod = NULL;
	}

	if (log == NULL || M_str_isempty(host) || dns == NULL || max_queue_bytes == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (log->suspended) {
		return M_LOG_SUSPENDED;
	}

	if (log->event == NULL) {
		return M_LOG_NO_EVENT_LOOP;
	}

	/* Internal thunk settings for write callback. */
	mdata = module_thunk_create(product, facility, host, port, log->event, dns, max_queue_bytes, log->line_end_str);

	/* Create IO object. */
	module_thunk_reconnect(mdata);

	/* Add io object to event loop, set callback. */
	if (!M_event_add(mdata->event, mdata->io, io_event_cb, mdata)) {
		M_io_destroy(mdata->io);
		module_thunk_destroy(mdata);
		return M_LOG_GENERIC_FAIL;
	}

	/* General module settings. */
	mod                          = M_malloc_zero(sizeof(*mod));
	mod->type                    = M_LOG_MODULE_TSYSLOG;
	mod->flush_on_destroy        = log->flush_on_destroy;
	mod->module_thunk            = mdata;
	mod->module_write_cb         = log_write_cb;
	mod->module_reopen_cb        = log_reopen_cb;
	mod->module_suspend_cb       = log_suspend_cb;
	mod->module_resume_cb        = log_resume_cb;
	mod->module_emergency_cb     = log_emergency_cb;
	mod->destroy_module_thunk_cb = log_destroy_cb;

	if (out_mod != NULL) {
		*out_mod = mod;
	}

	/* Add the module to the log. */
	M_thread_mutex_lock(log->lock);
	M_llist_insert(log->modules, mod);
	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_tcp_syslog_set_connect_timeout_ms(M_log_t *log, M_log_module_t *module, M_uint64 timeout_ms)
{
	module_thunk_t *mdata;

	if (log == NULL || module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	if (module->type != M_LOG_MODULE_TSYSLOG) {
		return M_LOG_WRONG_MODULE;
	}

	M_thread_mutex_lock(log->lock);

	if (!module_present_locked(log, module)) {
		M_thread_mutex_unlock(log->lock);
		return M_LOG_MODULE_NOT_FOUND;
	}

	mdata = module->module_thunk;

	if (!M_io_net_set_connect_timeout_ms(mdata->io, timeout_ms)) {
		M_thread_mutex_unlock(log->lock);
		return M_LOG_GENERIC_FAIL;
	}

	mdata->connect_timeout_ms = timeout_ms;

	M_thread_mutex_unlock(log->lock);
	return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_tcp_syslog_set_keepalives(M_log_t *log, M_log_module_t *module, M_uint64 idle_time_s,
	M_uint64 retry_time_s, M_uint64 retry_count)
{
	module_thunk_t *mdata;

	if (log == NULL || module == NULL || module->module_thunk == NULL) {
		return M_LOG_INVALID_PARAMS;
	}

	if (module->type != M_LOG_MODULE_TSYSLOG) {
		return M_LOG_WRONG_MODULE;
	}

	M_thread_mutex_lock(log->lock);

	if (!module_present_locked(log, module)) {
		M_thread_mutex_unlock(log->lock);
		return M_LOG_MODULE_NOT_FOUND;
	}

	mdata = module->module_thunk;

	if (!M_io_net_set_keepalives(mdata->io, idle_time_s, retry_time_s, retry_count)) {
		M_thread_mutex_unlock(log->lock);
		return M_LOG_GENERIC_FAIL;
	}
	mdata->keepalive_idle_time_s  = idle_time_s;
	mdata->keepalive_retry_time_s = retry_time_s;
	mdata->keepalive_retry_count  = retry_count;

	M_thread_mutex_unlock(log->lock);
	return M_LOG_SUCCESS;
}


M_log_error_t M_log_module_tcp_syslog_set_tag_priority(M_log_t *log, M_log_module_t *module, M_uint64 tags,
	M_syslog_priority_t priority)
{
	module_thunk_t *mdata;

	if (log == NULL || module == NULL || module->module_thunk == NULL || tags == 0) {
		return M_LOG_INVALID_PARAMS;
	}

	if (module->type != M_LOG_MODULE_TSYSLOG) {
		return M_LOG_WRONG_MODULE;
	}

	M_thread_mutex_lock(log->lock);

	if (!module_present_locked(log, module)) {
		M_thread_mutex_unlock(log->lock);
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

	M_thread_mutex_unlock(log->lock);

	return M_LOG_SUCCESS;
}
