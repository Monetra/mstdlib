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

#include "m_config.h"
#include <mstdlib/mstdlib_log.h>


typedef enum {
	M_ASYNC_WRITER_STOPPED             = 0,
	M_ASYNC_WRITER_RUNNING,
	M_ASYNC_WRITER_FLUSHING_TO_STOP,       /* Flushing message queue before stopping the writer. */
	M_ASYNC_WRITER_FLUSHING_TO_DESTROY,    /* Flushing message queue before destroying the writer. */
	M_ASYNC_WRITER_DESTROYING              /* Destroying the writer. */
} writer_state_t;

struct M_async_writer {
	/* Set once per create, or on explicit function call. */
	size_t              max_bytes;     /* maximum number of text bytes allowed in queue (does not include overhead). */
	M_uint64            write_command; /* 0 indicates no command, valid commands must be non-zero. */
	M_bool              force_command; /* execute write callback on next received command, even if message queue is empty */
	const char         *line_end;      /* set once on writer creation, never modified after that */

	M_async_write_cb_t          write_cb;
	void                       *write_thunk; /* thunk that gets passed to write_cb. */
	M_async_thunk_stop_cb_t     stop_cb;
	M_async_thunk_destroy_cb_t  destroy_cb;  /* destructor for thunk (may be NULL). */

	M_thread_mutex_t   *lock;
	M_thread_mutex_t   *block_cmd_lock; /* used to serialize access to the set_command_blocking() function. */
	M_thread_cond_t    *cond_updated;   /* when triggered, indicates that the queue has been updated or stopped. */
	M_thread_cond_t    *cond_done;      /* when triggered, indicates that the internal thread has finished a command, or exited. */
	M_thread_cond_t    *cond_alive;     /* when triggered, indicates that the internal thread is still alive. */

	/* Reset on start. */
	M_llist_str_t      *msgs;
	size_t              stored_bytes;  /* current number of text bytes stored in queue (does not include overhead). */
	M_uint64            num_dropped;   /* number of messages that have been dropped since last call to pop(). */

	/* Reset only by explicit function call. */
	writer_state_t      state;
	M_bool              command_done;  /* used to indicate a command has completed, if the user sent a blocking command */
	M_bool              thread_done;
	M_bool              thread_alive;

} /* typedef'd as M_async_writer_t in header. */;


static void destroy_int(M_async_writer_t *writer)
{
	if (writer->destroy_cb != NULL) {
		writer->destroy_cb(writer->write_thunk);
	}

	M_thread_mutex_destroy(writer->lock);
	M_thread_mutex_destroy(writer->block_cmd_lock);
	M_thread_cond_destroy(writer->cond_updated);
	M_thread_cond_destroy(writer->cond_done);
	M_thread_cond_destroy(writer->cond_alive);

	M_llist_str_destroy(writer->msgs);

	M_free(writer);
}



/* --------- PRIVATE HELPERS ------------ */

static M_bool in_flush(M_async_writer_t *writer)
{
	if (writer->state == M_ASYNC_WRITER_FLUSHING_TO_STOP || writer->state == M_ASYNC_WRITER_FLUSHING_TO_DESTROY) {
		return M_TRUE;
	}
	return M_FALSE;
}

/* Pull the oldest message off the queue. If no messages in queue, wait until there is one.
 *
 * If num_dropped isn't NULL, it will be set to the number of dropped messages since the last call to
 * pop().
 *
 * If this method returns NULL and sets cmd to 0, it means that we've received a stop request.
 */
static char *pop_one(M_async_writer_t *writer, M_uint64 *num_dropped, M_uint64 *cmd)
{
	char *ret;

	if (writer == NULL || cmd == NULL) {
		return NULL;
	}

	M_thread_mutex_lock(writer->lock);

	/* If there's a pending thread_alive request initially, tell everybody we're still here. */
	if (!writer->thread_alive) {
		writer->thread_alive = M_TRUE;
		M_thread_cond_broadcast(writer->cond_alive);
	}

	/* Wait on cond_updated signal, until one of the following happens:
	 *   (1) The message queue isn't empty.
	 *   (2) A stop or destroy request has been received.
	 *   (3) A write command has been set, and force_command is true.
	 */
	while (M_llist_str_len(writer->msgs) == 0 && writer->state == M_ASYNC_WRITER_RUNNING
		&& (!writer->force_command || writer->write_command == 0)) {
		M_thread_cond_wait(writer->cond_updated, writer->lock);

		/* If there's a pending thread_alive request when we wake up again, tell everybody we're still here. */
		if (!writer->thread_alive) {
			writer->thread_alive = M_TRUE;
			M_thread_cond_broadcast(writer->cond_alive);
		}
	}

	if (writer->state == M_ASYNC_WRITER_DESTROYING || writer->state == M_ASYNC_WRITER_STOPPED
		|| (in_flush(writer) && M_llist_str_len(writer->msgs) == 0)) {
		if (num_dropped != NULL) {
			if (writer->state == M_ASYNC_WRITER_STOPPED) {
				/* If we're not destroying the writer, just leave number of dropped messages in writer. They can be
				 * output once the writer is started up again.
				 */
				*num_dropped = 0;
			} else {
				/* When exiting, include messages left in queue in number of dropped messages reported to caller. */
				*num_dropped = writer->num_dropped + M_llist_str_len(writer->msgs);
			}
		}
		M_thread_mutex_unlock(writer->lock);
		/* Returning NULL and setting cmd to 0 tells the worker thread that we need to stop executing. */
		*cmd = 0;
		return NULL;
	}

	if (M_llist_str_len(writer->msgs) > 0) {
		ret = M_llist_str_take_node(M_llist_str_last(writer->msgs));
		writer->stored_bytes -= M_str_len(ret);

		/* Report number of dropped messages to caller, then reset the drop counter. */
		if (num_dropped != NULL) {
			*num_dropped = writer->num_dropped;
		}
		writer->num_dropped = 0;
	} else {
		ret = NULL;
	}

	/* Transfer any commands received by the message queue to the caller. */
	*cmd = writer->write_command;
	writer->write_command = 0;

	M_thread_mutex_unlock(writer->lock);
	return ret;
}


/* If we popped a message, but then failed to write it, use this to add it back onto the back end of the queue,
 * and update the number of dropped messages accordingly.
 */
static void replace_one(M_async_writer_t *writer, char *msg, M_uint64 num_dropped)
{
	size_t msg_len = M_str_len(msg);

	if (writer == NULL || msg_len == 0) {
		return;
	}

	M_thread_mutex_lock(writer->lock);

	if (writer->num_dropped == 0 && writer->stored_bytes + msg_len <= writer->max_bytes) {
		/* If no newer messages have been dropped in the time since we tried to write the old message,
		 * and if we have room to add the old message back onto the tail end of the buffer:
		 */
		M_llist_str_insert_after(M_llist_str_last(writer->msgs), msg);
		writer->stored_bytes += msg_len;
	} else {
		/* If newer messages have been dropped, or if the old message won't fit in the queue,
		 * just drop the old message.
		 */
		writer->num_dropped++;
	}

	/* Add back the number of dropped messages present when we first tried to write the old message. */
	writer->num_dropped += num_dropped;

	M_thread_mutex_unlock(writer->lock);
}


static void *write_thread(void *arg)
{
	M_async_writer_t *writer      = arg;
	M_uint64          num_dropped = 0;
	M_bool            destroying;

	while (M_TRUE) {
		char     *msg;
		M_uint64  cmd          = 0;
		M_bool    msg_consumed = M_TRUE;

		/* Wait until at least one message is available, then pop the oldest one from the queue.
		 *
		 * Returns the number of dropped messages before this message, and resets the internal dropped message
		 * counter to zero.
		 */
		num_dropped = 0;
		msg         = pop_one(writer, &num_dropped, &cmd);

		/* If any messages were dropped, write a message about it. Do this before exit check so that we
		 * can report any remaining messages in queue as dropped on exit.
		 */
		if (num_dropped > 0) {
			char tmp[128];
			M_snprintf(tmp, sizeof(tmp), "%llu messages were dropped (%s)%s", num_dropped,
				(msg == NULL && cmd == 0)? "log shutdown" : "buffer full", writer->line_end);
			msg_consumed = writer->write_cb(tmp, 0, writer->write_thunk);
		}

		/* NULL message and 0 command indicates that message queue wants us to stop processing. */
		if (msg == NULL && cmd == 0) {
			break;
		}

		/* Pass the next message to the writer. If we got a NULL message with an attached command, we need
		 * to call the writer in that case too.
		 *
		 * If we already tried sending a drop message and it wasn't accepted, don't bother trying to send
		 * a message again.
		 */
		if (msg_consumed) {
			msg_consumed = writer->write_cb(msg, cmd, writer->write_thunk);
			/* If a command was set, signal that it's done (in case anyone is blocking on it). */
			if (cmd != 0) {
				M_thread_mutex_lock(writer->lock);
				writer->command_done = M_TRUE;
				M_thread_cond_broadcast(writer->cond_done);
				M_thread_mutex_unlock(writer->lock);
			}
		}

		/* If either the drop message wasn't accepted, or the main message wasn't accepted, replace the
		 * message on the queue and correct the number of dropped messages.
		 */
		if (!msg_consumed && msg != NULL) {
			replace_one(writer, msg, num_dropped);
		}

		M_free(msg);
	}

	/* Set flag and notify any listening threads that the internal thread has finished. */
	if (writer->stop_cb != NULL) {
		/* Call stop callback in worker thread, so it doesn't block the main thread on a stall. */
		writer->stop_cb(writer->write_thunk);
	}
	M_thread_mutex_lock(writer->lock);
	writer->command_done = M_TRUE; /* Make sure any thread blocking on a command stops when writer is destroyed. */
	writer->thread_done  = M_TRUE;
	/* At this point, we've finished flushing the message queue. Update flush states to final state. */
	if (writer->state == M_ASYNC_WRITER_FLUSHING_TO_DESTROY) {
		writer->state = M_ASYNC_WRITER_DESTROYING;
	} else if(writer->state == M_ASYNC_WRITER_FLUSHING_TO_STOP) {
		writer->state = M_ASYNC_WRITER_STOPPED;
	}
	destroying = (writer->state == M_ASYNC_WRITER_DESTROYING)? M_TRUE : M_FALSE;
	M_thread_cond_broadcast(writer->cond_done); /* DO THIS LAST, right before final unlock */
	M_thread_mutex_unlock(writer->lock);

	/* If we're asynchronously destroying the thread, the main thread has already oprhaned us. Go ahead and destroy. */
	if (destroying) {
		destroy_int(writer);
	}
	/* Otherwise, the main thread has just requested a blocking stop. Destruction will be handled by the main thread. */

	return NULL;
}



/* --------- PUBLIC API ------------ */

M_async_writer_t *M_async_writer_create(size_t max_bytes, M_async_write_cb_t write_cb,
	void *write_thunk, M_async_thunk_stop_cb_t stop_cb, M_async_thunk_destroy_cb_t destroy_cb,
	M_async_writer_line_end_mode_t mode)
{
	M_async_writer_t *writer;

	if (write_cb == NULL) {
		return NULL;
	}

	writer = M_malloc_zero(sizeof(*writer));

	writer->max_bytes      = max_bytes;
	writer->lock           = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	writer->block_cmd_lock = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	writer->cond_updated   = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	writer->cond_done      = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	writer->cond_alive     = M_thread_cond_create(M_THREAD_CONDATTR_NONE);
	writer->write_cb       = write_cb;
	writer->write_thunk    = write_thunk;
	writer->stop_cb        = stop_cb;
	writer->destroy_cb     = destroy_cb;
	writer->state          = M_ASYNC_WRITER_STOPPED;
	writer->command_done   = M_TRUE;

	writer->msgs = M_llist_str_create(M_LLIST_STR_NONE);

	switch(mode) {
		case M_LOG_LINE_END_WINDOWS:
			writer->line_end = "\r\n";
			break;
		case M_LOG_LINE_END_UNIX:
			writer->line_end = "\n";
			break;
		case M_LOG_LINE_END_NATIVE:
#ifdef _WIN32
			writer->line_end = "\r\n";
#else
			writer->line_end = "\n";
#endif
			break;
	}

	return writer;
}


void M_async_writer_destroy(M_async_writer_t *writer, M_bool flush)
{
	if (writer == NULL) {
		return;
	}

	M_thread_mutex_lock(writer->lock);

	/* If we've already initiated a destroy operation, return without doing anything. */
	if (writer->state == M_ASYNC_WRITER_FLUSHING_TO_DESTROY || writer->state == M_ASYNC_WRITER_DESTROYING) {
		M_thread_mutex_unlock(writer->lock);
		return;
	}

	/* If the internal thread is stopped, destroy the object in the main thread and return. */
	if (writer->state == M_ASYNC_WRITER_STOPPED) {
		M_thread_mutex_unlock(writer->lock);
		destroy_int(writer);
		return;
	}

	/* Notify internal thread that it needs to stop at the next opportunity, and to destroy the object when it does. */
	writer->state = (flush)? M_ASYNC_WRITER_FLUSHING_TO_DESTROY : M_ASYNC_WRITER_DESTROYING;
	M_thread_cond_broadcast(writer->cond_updated);

	M_thread_mutex_unlock(writer->lock);
}


M_bool M_async_writer_destroy_blocking(M_async_writer_t *writer, M_bool flush, M_uint64 timeout_ms)
{
	M_bool      done;
	M_timeval_t t;

	if (writer == NULL) {
		return M_TRUE;
	}

	M_thread_mutex_lock(writer->lock);

	/* If we've already initiated a destroy or destroy_blocking operation, return without doing anything. */
	if (writer->state == M_ASYNC_WRITER_DESTROYING || in_flush(writer)) {
		M_thread_mutex_unlock(writer->lock);
		return M_TRUE;
	}

	/* If the internal thread is stopped, destroy the object in the main thread and return. */
	if (writer->state == M_ASYNC_WRITER_STOPPED) {
		M_thread_mutex_unlock(writer->lock);
		destroy_int(writer);
		return M_TRUE;
	}

	/* Notify internal thread that it needs to stop at the next opportunity. */
	writer->state = (flush)? M_ASYNC_WRITER_FLUSHING_TO_STOP : M_ASYNC_WRITER_STOPPED;
	M_thread_cond_broadcast(writer->cond_updated);

	/* BLOCKING: Wait for internal thread to finish any leftover processing and stop. */
	M_time_elapsed_start(&t);
	while (!writer->thread_done) {
		if (timeout_ms == 0) {
			M_thread_cond_wait(writer->cond_done, writer->lock);
		} else {
			M_uint64 elapsed = M_time_elapsed(&t);
			if (elapsed >= timeout_ms) {
				break;
			}
			M_thread_cond_timedwait(writer->cond_done, writer->lock, timeout_ms - elapsed);
		}
	}

	done = writer->thread_done;

	if (done) {
		/* If worker thread successfully stopped, it's safe to destroy it from here. */
		M_thread_mutex_unlock(writer->lock);
		destroy_int(writer);
	} else {
		/* If we timed out while waiting for worker thread to stop, tell worker to destroy itself asynchronously at
		 * the first opportunity.
		 */
		writer->state = M_ASYNC_WRITER_DESTROYING;
		M_thread_cond_broadcast(writer->cond_updated);
		M_thread_mutex_unlock(writer->lock);
	}

	return done;
}


M_bool M_async_writer_start(M_async_writer_t *writer)
{
	if (writer == NULL) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->lock);

	if (writer->state != M_ASYNC_WRITER_STOPPED) {
		M_thread_mutex_unlock(writer->lock);
		/* Return M_TRUE if already running, M_FALSE if we're in the middle of a destroy. */
		return (writer->state == M_ASYNC_WRITER_RUNNING)? M_TRUE : M_FALSE;
	}

	/* Reset to fresh state. I'm intentionally not resetting write_command. */
	writer->thread_done  = M_FALSE;
	writer->thread_alive = M_TRUE;
	writer->command_done = M_TRUE;

	/* Start write thread. */
	if (M_thread_create(NULL, write_thread, writer) == 0) {
		return M_FALSE;
	}

	writer->state = M_ASYNC_WRITER_RUNNING;

	M_thread_mutex_unlock(writer->lock);
	return M_TRUE;
}


M_bool M_async_writer_is_running(M_async_writer_t *writer)
{
	writer_state_t state;

	if (writer == NULL) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->lock);

	state = writer->state;

	M_thread_mutex_unlock(writer->lock);

	return (state == M_ASYNC_WRITER_RUNNING)? M_TRUE : M_FALSE;
}


M_bool M_async_writer_is_alive(M_async_writer_t *writer, M_uint64 timeout_ms)
{
	M_bool ret;

	if (writer == NULL) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->lock);

	if (writer->state != M_ASYNC_WRITER_RUNNING) {
		M_thread_mutex_unlock(writer->lock);
		return M_FALSE;
	}

	writer->thread_alive = M_FALSE;
	M_thread_cond_broadcast(writer->cond_updated);

	while (!writer->thread_alive) {
		if (!M_thread_cond_timedwait(writer->cond_alive, writer->lock, timeout_ms)) {
			break;
		}
	}

	ret = writer->thread_alive;

	M_thread_mutex_unlock(writer->lock);

	return ret;
}


/* NOTE: this is a BLOCKING operation, it will wait for the worker thread to finish before returning. */
void M_async_writer_stop(M_async_writer_t *writer)
{
	if (writer == NULL) {
		return;
	}

	M_thread_mutex_lock(writer->lock);

	/* If this writer has already been stopped or is being destroyed, return without doing anything. */
	if (writer->state != M_ASYNC_WRITER_RUNNING) {
		M_thread_mutex_unlock(writer->lock);
		return;
	}

	/* Tell the internal thread to stop at the next opportunity. */
	writer->state = M_ASYNC_WRITER_STOPPED;
	M_thread_cond_broadcast(writer->cond_updated);

	/* BLOCKING: Wait for internal thread to finish any leftover processing and stop. */
	while (!writer->thread_done) {
		M_thread_cond_wait(writer->cond_done, writer->lock);
	}

	M_thread_mutex_unlock(writer->lock);
}


M_bool M_async_writer_set_command(M_async_writer_t *writer, M_uint64 write_command, M_bool force)
{
	if (writer == NULL) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->lock);

	/* Don't allow new commands or messages while flushing. */
	if (in_flush(writer)) {
		M_thread_mutex_unlock(writer->lock);
		return M_FALSE;
	}

	writer->write_command |= write_command;
	writer->force_command |= force;

	/* If force is set, this will wake up the worker thread, even if the queue is currently empty. */
	if (force) {
		M_thread_cond_broadcast(writer->cond_updated);
	}

	M_thread_mutex_unlock(writer->lock);

	return M_TRUE;
}


M_bool M_async_writer_set_command_block(M_async_writer_t *writer, M_uint64 write_command)
{
	if (writer == NULL) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->block_cmd_lock);
	M_thread_mutex_lock(writer->lock);

	/* Don't allow new commands or messages while flushing. */
	if (in_flush(writer)) {
		M_thread_mutex_unlock(writer->lock);
		M_thread_mutex_unlock(writer->block_cmd_lock);
		return M_FALSE;
	}

	writer->write_command |= write_command;
	writer->force_command  = M_TRUE; /* ALWAYS force commands that block */

	/* Wake up worker thread. */
	M_thread_cond_broadcast(writer->cond_updated);

	/* Block until command is processed. */
	writer->command_done = M_FALSE;
	while (!writer->command_done) {
		M_thread_cond_wait(writer->cond_done, writer->lock);
	}

	M_thread_mutex_unlock(writer->lock);
	M_thread_mutex_unlock(writer->block_cmd_lock);

	return M_TRUE;
}


void M_async_writer_set_max_bytes(M_async_writer_t *writer, size_t max_bytes)
{
	if (writer == NULL) {
		return;
	}

	M_thread_mutex_lock(writer->lock);

	writer->max_bytes = max_bytes;

	M_thread_mutex_unlock(writer->lock);
}


M_bool M_async_writer_write(M_async_writer_t *writer, const char *msg)
{
	M_bool msg_added = M_FALSE;
	size_t msg_len;

	msg_len = M_str_len(msg);

	/* If queue is not allocated, or if message is zero length, ignore the message completely. */
	if (writer == NULL || msg_len == 0) {
		return M_FALSE;
	}

	M_thread_mutex_lock(writer->lock);

	/* Don't allow new commands or messages while flushing. */
	if (in_flush(writer)) {
		goto done;
	}

	/* If the message itself is too big to fit in the queue, drop it without wiping out the existing contents
	 * of the queue.
	 */
	if (msg_len > writer->max_bytes) {
		if (writer->num_dropped < M_UINT64_MAX) {
			writer->num_dropped++;
		}
		goto done;
	}

	/* Insert message into queue. If insertion failed, drop the message. */
	if (M_llist_str_insert_first(writer->msgs, msg) == NULL) {
		if (writer->num_dropped < M_UINT64_MAX) {
			writer->num_dropped++;
		}
		goto done;
	}

	msg_added = M_TRUE;

	/* If adding the new message will exceed our queue size limit, drop oldest messages until we have room. */
	writer->stored_bytes += msg_len;
	while (writer->stored_bytes > writer->max_bytes) {
		char   *old;
		size_t  old_len;

		old     = M_llist_str_take_node(M_llist_str_last(writer->msgs));
		old_len = M_str_len(old);
		M_free(old);

		writer->stored_bytes -= old_len;
		if (writer->num_dropped < M_UINT64_MAX) {
			writer->num_dropped++;
		}
	};

	done:
	if (msg_added) {
		M_thread_cond_broadcast(writer->cond_updated);
	}
	M_thread_mutex_unlock(writer->lock);
	return msg_added;
}


void *M_async_writer_get_thunk(M_async_writer_t *writer)
{
	if (writer == NULL) {
		return NULL;
	}
	return writer->write_thunk;
}


const char *M_async_writer_get_line_end(M_async_writer_t *writer)
{
	if (writer == NULL) {
		return "\n";
	}
	return writer->line_end;
}
