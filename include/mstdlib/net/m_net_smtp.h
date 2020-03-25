/* The MIT License (MIT)
 * 
 * Copyright (c) 2020 Monetra Technologies, LLC.
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

#ifndef __M_NET_SMTP_H__
#define __M_NET_SMTP_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_tls.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_net_smtp SMTP mailer
 *  \ingroup m_net
 * 
 * SMTP mailer
 *
 * Defaults to 3 send attempts.
 *
 * Will start running processing queued messages soon as an endpoint is added.
 *
 * @{
 *
 */

struct M_net_smtp;
typedef struct M_net_smtp M_net_smtp_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Current processing status. */
typedef enum {
	M_NET_SMTP_STATUS_IDLE = 0,    /*!< Currently up and able to process. */
	M_NET_SMTP_STATUS_PROCESSING,  /*!< Currently processing. */
	M_NET_SMTP_STATUS_STOPPED,     /*!< Not processing. */
	M_NET_SMTP_STATUS_NOENDPOINTS, /*!< Not processing due to no endpoints configured. */
	M_NET_SMTP_STATUS_STOPPING     /*!< In the process of stopping. Messages will not continue to
	                                    be sent but current messages that are processing will
	                                    process until finished. */
} M_net_smtp_status_t;


/*! Pool operation mode. */
typedef enum {
	M_NET_SMTP_MODE_FAILOVER = 0, /*!< Only one endpoint should be used and others should
	                                   be used when the current endpoint has a failure. */
	M_NET_SMTP_MODE_ROUNDROBIN    /*!< Connections should rotate across all endpoints. */
} M_net_smtp_mode_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Callback when connected.
 *
 * Only used by TCP endpoints.
 *
 * \param[in] address Address of the server. This is the same address passed
 *                    to M_net_smtp_add_endpoint_tcp.
 * \param[in] port    Port connected to. This is the same port passed
 *                    to M_net_smtp_add_endpoint_tcp.
 * \param[in] thunk   Thunk parameter provided during create.
 */
typedef void (*M_net_smtp_connect_cb)(const char *address, M_uint16 port, void *think);


/*! Callback when a connection to a server fails.
 *
 * Only used by TCP endpoints.
 *
 * \param[in] address   Address of the server. This is the same address passed
 *                      to M_net_smtp_add_endpoint_tcp.
 * \param[in] port      Port connected to. This is the same port passed
 *                      to M_net_smtp_add_endpoint_tcp.
 * \param[in] net_error Indicates where there was a network problem of some type or
 *                      if the network operation succeeded.
 * \param[in] error     Error message.
 * \param[in] thunk     Thunk parameter provided during create.
 *
 * \return M_FALSE if the endpoint should be be removed from the pool. M_TRUE to allow
 *         the server to be retried later.
 */
typedef M_bool (*M_net_smtp_connect_fail_cb)(const char *address, M_uint16 port, M_net_error_t net_err, const char *error, void *thunk);


/*! Callback when the connection to the server disconnects.
 *
 * Only used by TCP endpoints.
 *
 * This does not represent an error. Server connections will be establish and disconnected
 * periodically as part of normal processing.
 *
 * \param[in] address Address of the server. This is the same address passed
 *                    to M_net_smtp_add_endpoint_tcp.
 * \param[in] port    Port connected to. This is the same port passed
 *                    to M_net_smtp_add_endpoint_tcp.
 * \param[in] thunk   Thunk parameter provided during create.
 */
typedef void (*M_net_smtp_disconnect_cb)(const char *address, M_uint16 port, void *thunk);


/*! Callback when a process endpoint fails.
 *
 * Only used by process endpoints.
 *
 * \param[in] command     Command executed. Same as passed to M_net_smtp_add_endpoint_process.
 * \param[in] result_code Exit code of the process.
 * \param[in] proc_stdout Output of the process.
 * \param[in] proc_errout Error output of the process.
 * \param[in] thunk       Thunk parameter provided during create.
 *
 * \return M_FALSE if the endpoint should be be removed from the pool. M_TRUE to allow
 *         the server to be retried later.
 */
typedef M_bool (*M_net_smtp_process_fail_cb)(const char *command, int result_code, const char *proc_stdout, const char *proc_stderror, void *thunk);


/*! Callback when all endpoints have failed.
 *
 * \param[in] no_endpoints M_TRUE if processing was halted due to no endpoints configured.
 * \param[in] thunk        Thunk parameter provided during create.
 *
 * \return The number of seconds to wait before retrying to process. Use 0 to stop
 *         automatic reconnect attempts. When 0, M_net_smtp_resume must be called to
 *         restart processing. The return value is ignored if no endpoints are configured.
 */
typedef M_uint64 (*M_net_smtp_processing_halted_cb)(M_bool no_endpoints, void *thunk);


/*! Callback when a message was sent successfully.
 *
 * \param[in] headers Message headers provided as metadata to identify
 *                    the message that was sent.
 * \param[in] thunk   Thunk parameter provided during create.
 */
typedef void (*M_net_smtp_sent_cb)(const M_hash_dict_t *headers, void *thunk);


/*! Callback when sending a message failed.
 *
 * \param[in] headers     Message headers provided as metadata to identify
 *                        the message that failed.
 * \param[in] error       Error message.
 * \param[in] attempt_num Current attempt number to send this message. Will be 0 when 
 *                        using an external queue. Otherwise, >= 1.
 * \param[in] can_requeue M_TRUE when the message can be requed to try again. Will be
 *                        M_FALSE if the message has reached the maximum send attempts
 *                        when using the internal queue. Or when an external queue is
 *                        in use.
 * \param[in] thunk       Thunk parameter provided during create.
 *
 * \return M_TRUE to requeue the message. Ignored if using an external queue.
 */
typedef M_bool (*M_net_smtp_send_failed_cb)(const M_hash_dict_t *headers, const char *error, size_t attempt_num, M_bool can_requeue, void *thunk);


/*! Callback when a message needs to be requeued.
 *
 * Only called when an external queue is used. Will be called when a message that was dequeued
 * failed to send.
 *
 * \param[in] msg      Raw email message.
 * \param[in] wait_sec Number of seconds the queue should hold the message before attempting
 *                     to allow the message to resend. Typically set due to gray listing.
 * \param[in] thunk    Thunk parameter provided during create.
 */
typedef void (*M_net_smtp_reschedule_cb)(const char *msg, M_uint64 wait_sec, void *thunk);


/*! Callback to set additional I/O layers on the internal network request I/O object.
 *
 * The primary use for this callback is to add tracing or bandwidth shaping. TLS
 * should not be added here because it is handled internally.
 *
 * Due to connections being in a dynamic pool, the callback may be called multiple times.
 *
 * \param[in] io     The base I/O object to add layers on top of.
 * \param[in] error  Error buffer to set a textual error message when returning a failure response.
 * \param[in] errlen Size of error buffer.
 * \param[in] thunk  Thunk parameter provided during create.
 *
 * \return M_TRUE on success. M_FALSE if setting up the I/O object failed and the operation should abort.
 */
typedef M_bool (*M_net_smtp_iocreate_cb)(M_io_t *io, char *error, size_t errlen, void *thunk);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Structure of callbacks to inform and control operating behavior. */
struct M_net_smtp_callbacks {
	M_net_smtp_connect_cb           connect_cb;
	M_net_smtp_connect_fail_cb      connect_fail_cb;
	M_net_smtp_disconnect_cb        disconnect_cb;
	M_net_smtp_process_fail_cb      process_fail_cb;
	M_net_smtp_processing_halted_cb processing_halted_cb;
	M_net_smtp_sent_cb              sent_cb;
	M_net_smtp_send_failed_cb       send_failed_cb;
	M_net_smtp_reschedule_cb        reschedule_cb;
	M_net_smtp_iocreate_cb          iocreate_cb;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an SMTP pool.
 *
 * \param[in] el    Event loop to operate on.
 * \param[in] cbs   Callbacks for getting information about state and controlling behavior.
 * \param[in] thunk Optional thunk passed to callbacks.
 *
 * \return SMTP network object.
 */
M_API M_net_smtp_t *M_net_smtp_create(M_event_t *el, const struct M_net_smtp_callbacks *cbs, void *thunk);


/*! Destroy an SMTP pool.
 *
 * \param[in] sp SMTP pool.
 */
M_API void M_net_smtp_destroy(M_net_smtp_t *sp);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Pause processing.
 *
 * \param[in] sp SMTP pool.
 */
M_API void M_net_smtp_pause(M_net_smtp_t *sp);


/*! Resume processing.
 *
 * \param[in] sp SMTP pool.
 *
 * \return M_TRUE if resumed. Otherwise M_FALSE. Can return M_FALSE for
 *         conditions, such as, no end points.
 */
M_API M_bool M_net_smtp_resume(M_net_smtp_t *sp);


/*! Get the status of the SMTP pool.
 *
 * \param[in] sp SMTP pool.
 *
 * \return Status.
 */
M_API M_net_smtp_status_t M_net_smtp_status(const M_net_smtp_t *sp);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Setup parameters for TCP endpoints.
 *
 * This must be called before any TCP end points can be added.
 *
 * It is highly recommend a TLS client context be provided even
 * It is possible for the server to request a TLS connection
 * due to START TLS. It is also required if a TLS only connection
 * endpoint is configured.
 *
 * \param[in] sp  SMTP pool.
 * \param[in] dns DNS object. Must be valid for the duration of this object's life.
 * \param[in] ctx The TLS client context. The context does not have to persist after being set here.
 */
M_API void M_net_smtp_setup_tcp(M_net_smtp_t *sp, M_dns_t *dns, M_tls_clientctx_t *ctx);


/*! Setup timeout parameters for TCP endpoints.
 *
 * \param[in] sp         SMTP pool.
 * \param[in] connect_ms Connect timeout in milliseconds. Will trigger when a connection
 *                       has not been established within this time.
 * \param[in] stall_ms   Stall timeout in milliseconds. Will trigger when the time between read
 *                       and write events has been exceeded. This helps prevent a server from causing
 *                       a denial of service by sending 1 byte at a time with a large internal between
 *                       each one.
 * \param[in] idle_ms    Overall time the connection can be idle before being closed. 0 will cause the
 *                       connection to be closed after a single message.
 */
M_API void M_net_smtp_setup_tcp_timeouts(M_net_smtp_t *sp, M_uint64 connect_ms, M_uint64 stall_ms, M_uint64 idle_ms);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add a TCP endpoint.
 *
 * \param[in] sp          SMTP pool.
 * \param[in] address     Address of server.
 * \param[in] port        Port to connect on. If 0 will use port 25.
 * \param[in] connect_tls Server requires a TLS connection and the connection.
 * \param[in] username    Authentication username.
 * \param[in] password    Authentication password.
 * \param[in] max_conns   Maximum connections to this server that should be opened.
 *                        Scales up to max based on the number of messages queued.
 *
 * \return M_TRUE if the end point was added. Otherwise M_FALSE. Will always return M_FALSE
 *         if M_net_smtp_setup_tcp was not called and provided with DNS or if M_net_smtp_setup_tcp
 *         was called without a TLS context and connect_tls is set.
 */
M_API M_bool M_net_smtp_add_endpoint_tcp(M_net_smtp_t *sp, const char *address, M_uint16 port, M_bool connect_tls, const char *username, const char *password, size_t max_conns);


/*! Add a process endpoint.
 *
 * \param[in] sp           SMTP pool.
 * \param[in] command      Command to send message using. Must accept message as STDIN.
 * \param[in]  args        Optional. List of arguments to pass to command.
 * \param[in]  env         Optional. List of environment variables to pass on to process.  Use NULL to pass current environment through.
 * \param[in]  timeout_ms  Optional. Maximum execution time of the process before it is forcibly terminated.  Use 0 for infinite.
 *
 * \return M_TRUE if the endpoint was added. Otherwise, M_FALSE.
 */
M_API M_bool M_net_smtp_add_endpoint_process(M_net_smtp_t *sp, const char *command, const M_list_str_t *args, const M_hash_dict_t *env, M_uint64 timeout_ms);


/*! Set how the pool should handle multiple endpoints.
 *
 * \param[in] sp   SMTP pool.
 * \param[in] mode Operation mode.
 */
M_API M_bool M_net_smtp_mode(M_net_smtp_t *sp, M_net_smtp_mode_t mode);


/*! Number of resend attempts allowed per message.
 *
 * Only applies to internal queue processing.
 *
 * \param[in] sp  SMTP pool.
 * \param[in] num Number of send attempts per message.
 */
M_API void M_net_smtp_set_num_attempts(M_net_smtp_t *sp, size_t num);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Remove all queued messages from the queue.
 *
 * \param[in] sp  SMTP pool.
 *
 * It is recommended to call M_net_smtp_pause
 * and wait until the status is stopped before calling
 * this function.
 *
 * \return List of messages that were queued. Will not
 *         include messages that are currently processing.
 */
M_API M_list_str_t *M_net_smtp_dump_queue(M_net_smtp_t *sp);


/*! Add an email object to the queue.
 *
 * \param[in] sp SMTP pool.
 * \param[in] e  Email message.
 *
 * \return M_TRUE if the message was added. Otherwise, M_FALSE.
 */
M_API M_bool M_net_smtp_queue_smtp(M_net_smtp_t *sp, const M_email_t *e);


/*! Add an email message as a string to the queue.
 *
 * \param[in] sp SMTP pool.
 * \param[in] e  Message.
 *
 * \return M_TRUE if the message was added. Otherwise, M_FALSE.
 */
M_API M_bool M_net_smtp_queue_message(M_net_smtp_t *sp, const char *e);


/*! Tell the pool to use an external queue.
 *
 * Can only be called when the queue is empty.
 * Once an external queue is setup, the internal queue cannot be used.
 *
 * \param[in] sp     SMTP pool.
 * \param[in] get_cb Callback used by the pool to get messages from the queue.
 *                   Callback should return NULL if no messages are available.
 *
 * \return M_TRUE if the external queue was set.
 */
M_API M_bool M_net_smtp_use_external_queue(M_net_smtp_t *sp, char *(*get_cb)(void));


/*! Tell the pool messages are available in the external queue.
 *
 * The pool will run though messages in the queue until no more messages are available
 * However, the pool does not know when messages have been added to the external
 * queue. It is up to the queue manager to inform the pool messages are available
 * to process. It is recommended this be called after one or more messages are
 * added.
 *
 * \param[in] sp SMTP pool.
 */
M_API void M_net_smtp_external_queue_have_messages(M_net_smtp_t *sp);

/*! @} */

__END_DECLS

#endif /* __M_NET_SMTP_H__ */
