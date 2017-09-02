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

#ifndef M_ASYNC_WRITER_H
#define M_ASYNC_WRITER_H

#include <mstdlib/mstdlib.h>

__BEGIN_DECLS

/*! \addtogroup m_async_writer Asynchronous Writer
 *  \ingroup m_log
 *
 * Helper class that manages an internal worker thread and message queue for asynchrnous writes.
 *
 * Used internally in various logging modules.
 *
 * @{
 */


/*! Control what type of line endings get automatically appended to error messages. */
typedef enum {
	M_ASYNC_WRITER_LINE_END_NATIVE, /*!< \c '\\n' if running on Unix, \c '\\r\\n' if running on Windows */
	M_ASYNC_WRITER_LINE_END_UNIX,   /*!< always use \c '\\n' */
	M_ASYNC_WRITER_LINE_END_WINDOWS /*!< always use \c '\\r\\n' */
} M_async_writer_line_end_mode_t;


/*! Callback that will be called to write messages.
 *
 * If your program modifies the "thunk" object outside the callback while the writer is running,
 * you'll need to add your own locks inside the callback to make this reentrant.
 *
 * The command flag allows you to pass one-off notifications to the callback. These notifications
 * will be processed lazily (i.e., the next time the internal thread tries to write something).
 * Only the next write is affected; after the command flag is used once, it's reset to zero.
 *
 * It is possible for the write callback to be called with a NULL msg and a non-zero command. This
 * happens when the user sets a command with the force flag set to M_TRUE, but the message queue is empty.
 * In this case, the callback should process the command, but it shouldn't write the empty message.
 *
 * \param[in] msg   message that needs to be written. Can be modified in-place. May be NULL, for command-only calls.
 * \param[in] thunk object passed into \a write_thunk parameter of M_async_writer_create().
 * \param[in] cmd   command flag passed into M_async_writer_set_command(). May be 0, if no command sent.
 * \return          M_TRUE if message was consumed, M_FALSE if message should be returned to queue (if possible).
 */
typedef M_bool (*M_async_write_cb_t)(char *msg, M_uint64 cmd, void *thunk);


/* Callback that will be used to stop any asynchronous operations owned by the write thunk.
 *
 * This is an optional extra callback. Only use this if you have an extra async operation running
 * that's managed by the thunk - for example, if your callback uses M_popen(), you'd call blocking
 * close in here.
 */
typedef void (*M_async_thunk_stop_cb_t)(void *thunk);


/* Callback that will be used to destroy the write thunk.
 *
 * If provided, will be called when writer is destroyed.
 *
 * Note: this function must not block, it is called for both synchronous and asynchronous destroys.
 *       Blocking destroys will call M_async_thunk_stop_cb_t first, do any optional blocking there.
 *
 * \param[in] thunk object passed into \a write_thunk parameter of M_async_writer_create().
 */
typedef void (*M_async_thunk_destroy_cb_t)(void *thunk);


/*! Opaque struct that manages state for the writer. */
struct M_async_writer;
typedef struct M_async_writer M_async_writer_t;


/*! Create a writer object.
 *
 * The writer does not automatically start running, you must call M_async_writer_start().
 *
 * \param[in] max_bytes   maximum bytes that can be queued before messages start getting dropped
 * \param[in] write_cb    callback that will be called by an internal thread to write messages
 * \param[in] write_thunk object that can be used to preserve callback state between writes
 * \param[in] stop_cb     optional callback that will be called during a stop request
 * \param[in] destroy_cb  callback that will be used to destroy the thunk when writer is destroyed
 * \param[in] mode        line-end mode for internally generated error messages
 */
M_API M_async_writer_t *M_async_writer_create(size_t max_bytes, M_async_write_cb_t write_cb,
	void *write_thunk, M_async_thunk_stop_cb_t stop_cb, M_async_thunk_destroy_cb_t destroy_cb,
	M_async_writer_line_end_mode_t mode);


/*! Destroy the writer (non-blocking).
 *
 * This is a non-blocking operation - the worker thread is commanded to destroy itself, then immediately orphaned. The
 * orphaned thread will still try to delete itself, if it has enough time to do so before the process ends. If the
 * program exits before it has time to do this, it will show up as a memory leak (even though it's not).
 *
 * This call asks the internal thread to stop running at the next opportunity and then destroy the writer object once
 * stopped. If the internal thread has already been stopped, the object is destroyed by the calling thread.
 *
 * If you set \a flush to M_TRUE, the internal thread will output all messages in the queue before it destroys itself.
 * Otherwise, the thread will stop itself right after it finishes the current message it's working on, and will output
 * a message describing the number of dropped messages left in the queue before destroying itself.
 *
 * If the internal thead is frozen, this is effectively a memory leak - the writer object won't be destroyed until
 * the process exits. But the calling thread won't freeze, so this is probably preferable.
 *
 * \see M_async_writer_destroy_blocking
 *
 * \param[in] writer object we're operating on
 * \param[in] flush  if M_TRUE, output all messages in queue before destroying
 */
M_API void M_async_writer_destroy(M_async_writer_t *writer, M_bool flush);


/*! Destroy the writer (blocking, with timeout).
 *
 * \warning
 * This is a BLOCKING operation, it will wait for the worker thread to finish before returning, or for the given
 * timeout to expire (whichever comes first).
 *
 * This call asks the internal thread to stop running at the next opportunity and then destroy the writer object once
 * stopped. If the internal thread has already been stopped, the object is destroyed by the calling thread.
 *
 * If you set \a flush to M_TRUE, the internal thread will output all messages in the queue before it destroys itself.
 * Otherwise, the thread will stop itself right after it finishes the current message it's working on, and will output
 * a message describing the number of dropped messages left in the queue before destroying itself.
 *
 * If the timeout expires before the worker thread is done, the worker thread will be orphaned and control will return
 * to the caller (just like in M_async_writer_destroy()). The orphaned thread will still try to delete itself, if it
 * has enough time to do so before the process ends. If the program exits before it has time to do this, it will show
 * up as a memory leak (even though it's not).
 *
 * \see M_async_writer_destroy
 * \see M_async_writer_stop
 *
 * \param[in] writer     object we're operating on
 * \param[in] flush      if M_TRUE, output all messages in queue before destroying
 * \param[in] timeout_ms length of time (in milliseconds) to wait until orphaning the worker thread, or 0 for no timeout
 * \return               M_TRUE if worker thread finished within timeout, false if it did not and was orphaned
 */
M_API M_bool M_async_writer_destroy_blocking(M_async_writer_t *writer, M_bool flush, M_uint64 timeout_ms);


/*! Start writing messages from the queue.
 *
 * This starts an internal worker thread that pulls messages off of the message queue and writes them.
 *
 * You can stop the worker thread with M_async_writer_stop() and then restart it with this function, and messages
 * will still be accepted into the message queue the entire time. Start and stop only affect whether messages are
 * being pulled off of the queue and written.
 *
 * \see M_async_writer_stop
 *
 * \param[in] writer object we're operating on
 */
M_API M_bool M_async_writer_start(M_async_writer_t *writer);


/*! Check to see if writer has been started and is accepting messages.
 *
 * This is non-blocking, we're just checking whether the writer has been started
 * and not stopped. If you need to check if a running writer is frozen or not,
 * use M_async_writer_is_alive().
 *
 * \param writer object we're checking
 * \return M_TRUE if writer has been started, M_FALSE if writer is stopped
 */
M_API M_bool M_async_writer_is_running(M_async_writer_t *writer);


/*! Check to see if writer is frozen or not (blocking).
 *
 * Blocks until either the internal worker thread responds, or the timeout is reached.
 *
 * If you just want to check if the writer has been started or not, use M_async_writer_is_running() instead, it's
 * non-blocking.
 *
 * Thread should respond after it finishes with the message that it's currently working on. So, the timeout
 * should be chosen based on the time it takes for the write_cb to execute once (worst case).
 *
 * \param[in] writer object we're checking
 * \param[in] timeout_ms amount of time to wait for thread to respond (in milliseconds)
 * \return M_TRUE if thread responded to liveness check, M_FALSE if thread didn't respond within timeout
 */
M_API M_bool M_async_writer_is_alive(M_async_writer_t *writer, M_uint64 timeout_ms);


/*! Stop internal worker thread.
 *
 * \warning
 * This is a BLOCKING operation, it will wait for the worker thread to finish before returning. The worker thread
 * will stop immediately after it finishes the current message it's working on (if any), so it shouldn't block for
 * long.
 *
 * This is used when you need to stop the internal worker thread temporarily, and then restart it with a new
 * thread later. Messages are still accepted into the message queue while the writer is stopped, it just doesn't
 * write anything until M_async_writer_start() is called again.
 *
 * \see M_async_writer_start
 * \see M_async_writer_destroy_blocking
 *
 * \param[in] writer object we're operating on
 */
M_API void M_async_writer_stop(M_async_writer_t *writer);


/*! Set a command flag that will be passed to the write callback the next time it's called.
 *
 * This can be used to notify the write callback of a condition change (like a request to rotate logs, etc.).
 *
 * The command will be passed on the next call to the write callback, then reset immediately afterwards.
 *
 * If multiple calls to this command occur before the next write, the commands will be OR'd together into one value.
 *
 * You can force the write callback to always be called after the command is set by setting the \a force flag to
 * M_TRUE. If not set, the command will be processed the next time the internal worker thread pulls a message off
 * the queue (which might not be until the next call to M_async_writer_write(), if the queue is currently empty).
 *
 * \param[in] writer        object we're operating on
 * \param[in] write_command flag to pass to write_callback on next write (must be a power of two, or several flags OR'd together)
 * \param[in] force         if M_TRUE, wakes up writer thread even if message queue is empty.
 * \return                  M_FALSE if command rejected due to writer being in the middle of a flush-stop.
 */
M_API M_bool M_async_writer_set_command(M_async_writer_t *writer, M_uint64 write_command, M_bool force);


/*! Set a command flag, and block until that command is processed.
 *
 * Same as M_async_writer_set_command(), except that it blocks until the internal worker thread is done processing
 * the command.
 *
 * Note that this function always sets the force flag for the command - even if the message queue is empty, the
 * internal worker thread will be awakened and the command will be processed.  If the message queue is not empty,
 * the command will be processed when the next message is pulled off of the queue.
 *
 * \warning
 * If this function is called from multiple threads on the same async_writer object, execution of the requested
 * commands will be serialized - the command from the second thread won't even start until after the command from
 * the first thread has finished.
 *
 * \param[in] writer        object we're operating on
 * \param[in] write_command flag to pass to write_callback on next write (must be a power of two, or several flags OR'd together)
 * \return                  M_FALSE if command rejected due to writer being in the middle of a flush-stop.
 */
M_API M_bool M_async_writer_set_command_block(M_async_writer_t *writer, M_uint64 write_command);


/*! Change the maximum buffer size.
 *
 * If the writer is running, the new maximum buffer size will not actually be enforced until the
 * next time a message is written to the writer.
 *
 * \param[in] writer    object we're operating on
 * \param[in] max_bytes new maximum number of bytes that can be queued before messages start getting dropped
 */
M_API void M_async_writer_set_max_bytes(M_async_writer_t *writer, size_t max_bytes);


/*! Write a message to the writer (non-blocking).
 *
 * The message will be added to a work queue, to be passed later to write_callback by an internal worker thread.
 *
 * If the message can't be added (empty message, message itself is larger than queue, alloc error, or writer is
 * in the middle of a flush-stop) the message is dropped without modifying the internal queue.
 *
 * If the queue doesn't have enough empty space to fit the message, messages in the queue are dropped until
 * there is enough room. The oldest message is dropped first, then the next oldest, and so on, until there's
 * enough room in the queue to add the new message.
 *
 * Note that an async writer will still accept messages passed with this function when stopped - it will just
 * add them to the message queue, and wait until the writer is started again to write them.
 *
 * \param[in] writer object we're operating on
 * \param[in] msg    message to add to write queue
 * \return           M_TRUE if message was added to queue, M_FALSE if it couldn't be added
 */
M_API M_bool M_async_writer_write(M_async_writer_t *writer, const char *msg);


/*! Return the internal writer callback thunk.
 *
 * \warning
 * If the writer is running, don't modify the thunk from external code unless you've implemented your own
 * locking scheme between the writer callback and the external code, using locks stored in the thunk.
 *
 * \warning
 * Ownership of the returned thunk remains with the M_async_writer_t object, so this pointer is only valid
 * until M_async_writer_destroy() is called.
 *
 * \param[in] writer object we're operating on
 * \return           pointer to internal
 */
M_API void *M_async_writer_get_thunk(M_async_writer_t *writer);


/*! @} */

__END_DECLS

#endif /* M_ASYNC_WRITER_H */
