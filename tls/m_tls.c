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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_tls.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#ifndef OPENSSL_NO_ENGINE
#  include <openssl/engine.h>
#endif
#include <openssl/conf.h>
#include "base/m_defs_int.h"
#include "m_tls_clientctx_int.h"
#include "m_tls_serverctx_int.h"
#include "m_tls_hostvalidate.h"

typedef enum {
	M_TLS_STATE_INIT         = 0,
	M_TLS_STATE_CONNECTING   = 1,
	M_TLS_STATE_ACCEPTING    = 2,
	M_TLS_STATE_CONNECTED    = 3,
	M_TLS_STATE_SHUTDOWN     = 4,
	M_TLS_STATE_DISCONNECTED = 5,
	M_TLS_STATE_ERROR        = 6
} M_tls_state_t;

typedef enum {
	M_TLS_STATEFLAG_READ_WANT_WRITE = 1 << 0,
	M_TLS_STATEFLAG_WRITE_WANT_READ = 1 << 1
} M_tls_stateflags_t;


struct M_io_handle {
	M_tls_clientctx_t *clientctx;
	M_tls_serverctx_t *serverctx;
	char              *hostname;
	SSL               *ssl;
	BIO               *bio_glue;
	M_tls_state_t      state;
	M_tls_stateflags_t state_flags;
	M_bool             is_client;
	M_event_timer_t   *timer;
	M_io_error_t       last_io_err;
	M_timeval_t        negotiation_start;
	M_uint64           negotiation_time;
	char               error[256];
};


static M_tls_init_t       M_tls_initialized       = 0;
static BIO_METHOD        *M_tls_bio_method        = NULL;

static void M_tls_bio_method_new(void);

static M_uint64 M_tls_get_negotiation_timeout_ms(M_io_handle_t *handle)
{
	if (handle->is_client)
		return handle->clientctx->negotiation_timeout_ms;
	return handle->serverctx->negotiation_timeout_ms;
}

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
static M_thread_mutex_t **M_tls_openssl_locks     = NULL;
static size_t             M_tls_openssl_locks_num = 0;

static void M_tls_openssl_thread_free(void)
{
	ERR_remove_thread_state(NULL);
}

static void M_tls_openssl_staticlocks_cb(int mode, int n, const char *file, int line)
{
	(void)file;
	(void)line;

	if (M_tls_openssl_locks == NULL) {
		return;
	}
	if ((size_t)n >= M_tls_openssl_locks_num) {
		return;
	}

	if (mode & CRYPTO_LOCK)	{
		M_thread_mutex_lock(M_tls_openssl_locks[n]);
	} else {
		M_thread_mutex_unlock(M_tls_openssl_locks[n]);
	}
}

/* Have to define this struct, oddly enough */
struct CRYPTO_dynlock_value {
	M_thread_mutex_t *mutex;
};

static struct CRYPTO_dynlock_value *M_tls_openssl_dynlock_create_cb(const char *file, int line)
{
	struct CRYPTO_dynlock_value *ret = NULL;
	(void)file;
	(void)line;

	ret        = M_malloc_zero(sizeof(*ret));
	ret->mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	return ret;
}

static void M_tls_openssl_dynlock_cb(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	(void)file;
	(void)line;

	if (mode & CRYPTO_LOCK) {
		M_thread_mutex_lock(l->mutex);
	} else {
		M_thread_mutex_unlock(l->mutex);
	}
}

static void M_tls_openssl_dynlock_destroy_cb(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	(void)file;
	(void)line;

	M_thread_mutex_destroy(l->mutex);
	M_free(l);
}

#endif

M_thread_once_t M_tls_init_once = M_THREAD_ONCE_STATIC_INITIALIZER;

static void M_tls_destroy(void *arg)
{
#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	size_t i;
#endif

	(void)arg;

	if (!M_thread_once_reset(&M_tls_init_once) || M_tls_initialized == M_TLS_INIT_EXTERNAL || M_tls_initialized == 0) {
		M_tls_initialized = 0;
		return;
	}

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	M_free(M_tls_bio_method);
#else
	BIO_meth_free(M_tls_bio_method);
#endif
	M_tls_bio_method = NULL;

//M_printf("%s(): destroying\n", __FUNCTION__);
	/* Ok, why doesn't OpenSSL have a SSL_library_free() ?? */
#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	ERR_remove_state(0);
#endif

#ifndef OPENSSL_NO_ENGINE
	ENGINE_cleanup();
#endif
	CONF_modules_unload(1);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
	SSL_COMP_free_compression_methods();
#endif

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	for (i=0; i<M_tls_openssl_locks_num; i++) {
		M_thread_mutex_destroy(M_tls_openssl_locks[i]);
	}
	M_free(M_tls_openssl_locks);
	M_tls_openssl_locks = NULL;
	M_thread_destructor_remove(M_tls_openssl_thread_free);
#endif

	M_tls_initialized = 0;
}


#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
static unsigned long M_tls_crypto_threadid_cb(void)
{
	M_threadid_t id = M_thread_self();
	return (unsigned long)((M_uintptr)id);
}
#endif


static void M_tls_init_routine(M_uint64 flags)
{
	M_tls_init_t type = (M_tls_init_t)flags;
#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	size_t       i;
#endif

	M_tls_initialized = type;

	if (type == M_TLS_INIT_EXTERNAL)
		return;
//M_printf("%s(): Initializing...\n", __FUNCTION__);
	SSL_load_error_strings();
	SSL_library_init();

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
	M_tls_openssl_locks_num = (size_t)CRYPTO_num_locks();
	M_tls_openssl_locks     = M_malloc_zero(M_tls_openssl_locks_num * sizeof(*M_tls_openssl_locks));

	for (i=0; i<M_tls_openssl_locks_num; i++) {
		M_tls_openssl_locks[i] = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	}

	M_thread_destructor_insert(M_tls_openssl_thread_free);


	CRYPTO_set_id_callback(M_tls_crypto_threadid_cb);

	CRYPTO_set_locking_callback(M_tls_openssl_staticlocks_cb);

	CRYPTO_set_dynlock_create_callback(M_tls_openssl_dynlock_create_cb);
	CRYPTO_set_dynlock_lock_callback(M_tls_openssl_dynlock_cb);
	CRYPTO_set_dynlock_destroy_callback(M_tls_openssl_dynlock_destroy_cb);
#endif

	M_tls_bio_method_new();

	M_library_cleanup_register(M_tls_destroy, NULL);
//M_printf("%s(): init done...\n", __FUNCTION__);

}



void M_tls_init(M_tls_init_t type)
{
	M_thread_once(&M_tls_init_once, M_tls_init_routine, (M_uint64)type);
}


static void M_tls_op_timeout_cb(M_event_t *event, M_event_type_t type, M_io_t *io_dummy, void *cb_data)
{
	M_io_layer_t  *layer  = cb_data;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	(void)io_dummy;
	(void)event;
	(void)type;

	if (handle->state == M_TLS_STATE_CONNECTING || handle->state == M_TLS_STATE_ACCEPTING) {
		M_snprintf(handle->error, sizeof(handle->error), "TLS %s timeout negotiating connection", (handle->state == M_TLS_STATE_CONNECTING)?"client":"server");
		handle->state            = M_TLS_STATE_ERROR;
		handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_ERROR);
		return;
	}

	if (handle->state == M_TLS_STATE_SHUTDOWN) {
		/* Ignore error */
		handle->state = M_TLS_STATE_DISCONNECTED;
		M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_DISCONNECTED);
		return;
	}

}

static M_bool M_io_tls_process_state_init(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);

	if (*type == M_EVENT_TYPE_CONNECTED) {
		handle->timer = M_event_timer_oneshot(event, M_tls_get_negotiation_timeout_ms(handle), M_FALSE, M_tls_op_timeout_cb, layer);
		M_time_elapsed_start(&handle->negotiation_start);
		if (handle->is_client) {
			handle->state = M_TLS_STATE_CONNECTING;
		} else {
			handle->state = M_TLS_STATE_ACCEPTING;
		}
	}
	return M_FALSE;
}


static void M_io_tls_error_string(int sslerr, char *error, size_t errlen)
{
	unsigned long e;
	switch (sslerr) {
		case SSL_ERROR_ZERO_RETURN:
			M_snprintf(error, errlen, "TLS connection has been closed via protocol");
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			M_snprintf(error, errlen, "TLS READ/WRITE Operation did not complete");
			break;
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			M_snprintf(error, errlen, "TLS CONNECT/ACCEPT Operation did not complete");
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			M_snprintf(error, errlen, "TLS WANT_X509_LOOKUP");
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0) {
				M_snprintf(error, errlen, "TLS SYSCALL ERROR");
			} else {
				M_snprintf(error, errlen, "TLS SYSCALL ERROR: %s", ERR_reason_error_string(e));
			}
			break;
		case SSL_ERROR_SSL:
		default:
			e = ERR_get_error();
			M_snprintf(error, errlen, "TLS ERROR (%lu): %s", e, ERR_reason_error_string(e));
			break;
	}
}

static M_bool M_io_tls_process_state_connecting(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	int            rv;
	int            err;

	switch (*type) {
		case M_EVENT_TYPE_CONNECTED:
		case M_EVENT_TYPE_READ:
		case M_EVENT_TYPE_WRITE:
			rv = SSL_connect(handle->ssl);
			if (rv == 1) {
				/* Verify peer */
				if (handle->clientctx->verify_level != M_TLS_VERIFY_NONE) {
					X509 *x509 = SSL_get_peer_certificate(handle->ssl); /* Incs reference count, must X509_free() */
					long  ret;
					if (x509 == NULL) {
						M_snprintf(handle->error, sizeof(handle->error), "TLS client did not receive peer/server certificate");
						goto cert_err;
					}
					ret = SSL_get_verify_result(handle->ssl);
					if (ret != X509_V_OK) {
						M_snprintf(handle->error, sizeof(handle->error), "TLS certificate verification failed: %s", X509_verify_cert_error_string(ret));
						X509_free(x509);
						goto cert_err;
					}
					if (handle->clientctx->verify_level == M_TLS_VERIFY_CERT_FUZZY || handle->clientctx->verify_level == M_TLS_VERIFY_FULL) {
						M_tls_verify_host_flags_t flags = M_TLS_VERIFY_HOST_FLAG_NORMAL;
						if (handle->clientctx->verify_level == M_TLS_VERIFY_CERT_FUZZY) {
							flags |= M_TLS_VERIFY_HOST_FLAG_FUZZY_BASE_DOMAIN;
						}
						if (!M_tls_verify_host(x509, handle->hostname, flags)) {
							M_snprintf(handle->error, sizeof(handle->error), "TLS hostname verification failed");
							X509_free(x509);
							goto cert_err;
						}
					}
//					M_printf("SSL certificate verification successful\n");
					X509_free(x509);
				}
//M_printf("SSL_connect() successful\n");
				handle->state            = M_TLS_STATE_CONNECTED;
				*type                    = M_EVENT_TYPE_CONNECTED;
				M_event_timer_remove(handle->timer);
				handle->timer            = NULL;
				handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);

				return M_FALSE; /* Not consumed, relay rewritten connect message */

cert_err:
				handle->state            = M_TLS_STATE_ERROR;
				*type                    = M_EVENT_TYPE_ERROR;
				handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);
				return M_FALSE; /* Not consumed, relay rewritten error message */
			}
			err = SSL_get_error(handle->ssl, rv);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
//M_printf("SSL_connect() WANT_%s\n", err == SSL_ERROR_WANT_READ?"READ":"WRITE");
				return M_TRUE; /* Internally consumed i/o */
			}
//M_printf("SSL_connect() rv = %d, err = %d\n", rv, err);
			handle->state            = M_TLS_STATE_ERROR;
			*type                    = M_EVENT_TYPE_ERROR;
			handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);
			M_io_tls_error_string(err, handle->error, sizeof(handle->error));
			return M_FALSE; /* Not consumed, relay rewritten error message */
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			handle->state            = M_TLS_STATE_DISCONNECTED; /* An error from a different layer isn't really an error for us */
			handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);
			return M_FALSE;
		default:
			break;
	}

	return M_TRUE; /* eat anything unknown */
}


static M_bool M_io_tls_process_state_accepting(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	int            rv;
	int            err;
//M_printf("SSL_accept(%p) enter\n", M_io_layer_get_io(layer));
	switch (*type) {
		case M_EVENT_TYPE_CONNECTED:
		case M_EVENT_TYPE_READ:
		case M_EVENT_TYPE_WRITE:
			ERR_clear_error();
			rv = SSL_accept(handle->ssl);
			if (rv == 1) {
//M_printf("SSL_accept(%p) successful\n", M_io_layer_get_io(layer));
				handle->state = M_TLS_STATE_CONNECTED;
				*type         = M_EVENT_TYPE_CONNECTED;
/* XXX: Verify peer */
				M_event_timer_remove(handle->timer);
				handle->timer            = NULL;
				handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);

				return M_FALSE; /* Not consumed, relay rewritten connect message */
			}
			err = SSL_get_error(handle->ssl, rv);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
//M_printf("SSL_accept(%p) WANT_%s\n", M_io_layer_get_io(layer), err == SSL_ERROR_WANT_READ?"READ":"WRITE");
				return M_TRUE; /* Internally consumed i/o */
			}
//M_printf("SSL_accept(%p) rv = %d, err = %d\n", M_io_layer_get_io(layer), rv, err);
			handle->state            = M_TLS_STATE_ERROR;
			*type                    = M_EVENT_TYPE_ERROR;
			handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);

			M_io_tls_error_string(err, handle->error, sizeof(handle->error));
			return M_FALSE; /* Not consumed, relay rewritten error message */
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			handle->state            = M_TLS_STATE_DISCONNECTED; /* An error from a different layer isn't really an error for us */
			handle->negotiation_time = M_time_elapsed(&handle->negotiation_start);
			return M_FALSE;
		default:
			break;
	}

	return M_TRUE; /* eat anything unknown */
}


static M_bool M_io_tls_process_state_connected(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);

	/* NOTE: SSL_read() may return WANT_WRITE, and SSL_write() may return
	 *       WANT_READ, so we need to handle tracking such states so we
	 *       can send additional soft events in such situations */
	switch (*type) {
		case M_EVENT_TYPE_CONNECTED:
			/* Relay */
			return M_FALSE;
		case M_EVENT_TYPE_DISCONNECTED:
		case M_EVENT_TYPE_ERROR:
			handle->state = M_TLS_STATE_DISCONNECTED;
			return M_FALSE;
		case M_EVENT_TYPE_READ:
			if (handle->state_flags & M_TLS_STATEFLAG_WRITE_WANT_READ) {
				/* Prefer rewriting this event to a "write" which will get processed
				 * immediately as a higher priority, but still trigger a "read" event
				 * too as a secondary event in case we're also waiting on this */
				*type = M_EVENT_TYPE_WRITE;
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_READ);
			}
			return M_FALSE;
		case M_EVENT_TYPE_WRITE:
			if (handle->state_flags & M_TLS_STATEFLAG_READ_WANT_WRITE) {
				/* Prefer rewriting this event to a "read" which will get processed
				 * immediately as a higher priority, but still trigger a "write" event
				 * too as a secondary event in case we're also waiting on this */
				*type = M_EVENT_TYPE_READ;
				M_io_layer_softevent_add(layer, M_TRUE, M_EVENT_TYPE_WRITE);
			}
			return M_FALSE;
		default:
			break;
	}

	return M_TRUE; /* eat anything unknown */
}


static M_bool M_io_tls_process_state_shutdown(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	int            rv;
	int            err;

	switch (*type) {
		case M_EVENT_TYPE_CONNECTED:
		case M_EVENT_TYPE_READ:
		case M_EVENT_TYPE_WRITE:
			ERR_clear_error();
			rv = SSL_shutdown(handle->ssl);
			if (rv == 1) {
//M_printf("SSL_shutdown() successful\n");
				handle->state = M_TLS_STATE_DISCONNECTED;
				*type         = M_EVENT_TYPE_DISCONNECTED;
				M_event_timer_remove(handle->timer);
				handle->timer = NULL;
				return M_FALSE; /* Not consumed, relay rewritten connect message */
			}
			err = SSL_get_error(handle->ssl, rv);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_SYSCALL) {
//M_printf("SSL_shutdown() WANT_%s\n", err == SSL_ERROR_WANT_READ?"READ":((err == SSL_ERROR_WANT_WRITE)?"WRITE":"SYSCALL"));
				return M_TRUE; /* Internally consumed i/o */
			}
//M_printf("SSL_shutdown() rv = %d, err = %d\n", rv, err);
			/* Ignore error during shutdown */
			handle->state = M_TLS_STATE_DISCONNECTED;
			*type         = M_EVENT_TYPE_DISCONNECTED;
			return M_FALSE; /* Not consumed, relay rewritten error message */
		case M_EVENT_TYPE_DISCONNECTED:
			handle->state = M_TLS_STATE_DISCONNECTED;
			return M_FALSE;
		case M_EVENT_TYPE_ERROR:
			handle->state = M_TLS_STATE_DISCONNECTED; /* Don't consider a shutdown error a real error */
			return M_FALSE;
		default:
			break;
	}

	return M_TRUE; /* eat anything unknown */
}


static M_bool M_io_tls_process_cb(M_io_layer_t *layer, M_event_type_t *type)
{
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	M_bool         consumed = M_FALSE;

//M_printf("%s(): entering %p event %d state %d\n", __FUNCTION__, M_io_layer_get_io(layer), (int)*type, (int)handle->state);
	/* NOTE: This is not a switch statement as a state transition could occur that requires
	 *       processing.  It is ordered in the way state transitions can occur */
	if (!consumed && handle->state == M_TLS_STATE_INIT) {
		consumed = M_io_tls_process_state_init(layer, type);
	}

	if (!consumed && handle->state == M_TLS_STATE_CONNECTING) {
		consumed = M_io_tls_process_state_connecting(layer, type);
	}

	if (!consumed && handle->state == M_TLS_STATE_ACCEPTING) {
		consumed = M_io_tls_process_state_accepting(layer, type);
	}

	if (!consumed && handle->state == M_TLS_STATE_CONNECTED) {
		consumed = M_io_tls_process_state_connected(layer, type);
	}

	if (!consumed && handle->state == M_TLS_STATE_SHUTDOWN) {
		consumed = M_io_tls_process_state_shutdown(layer, type);
	}
//M_printf("%s(): exiting %p event %d state %d - consumed %d\n", __FUNCTION__, M_io_layer_get_io(layer), (int)*type, (int)handle->state, (int)consumed);
	return consumed;
}


static int M_tls_bio_create(BIO *b)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	BIO_set_data(b, NULL);
	BIO_set_init(b, 1);
	BIO_clear_flags(b, INT_MAX);
#else
	b->ptr   = NULL;
	b->num   = 0;
	b->init  = 1;
	b->flags = 0;
#endif
	return 1;
}


static int M_tls_bio_destroy(BIO *b)
{
	if (b == NULL)
		return 0;

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	BIO_set_data(b, NULL);
	BIO_set_init(b, 0);
	BIO_clear_flags(b, INT_MAX);
#else
	b->ptr   = NULL;
	b->init  = 0;
	b->flags = 0;
#endif

	return 1;
}


static int M_tls_bio_read(BIO *b, char *buf, int len)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	M_io_layer_t  *layer  = BIO_get_data(b);
#else
	M_io_layer_t  *layer  = b->ptr;
#endif
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_error_t   err;
	size_t         read_len;

	if (buf == NULL || len <= 0 || layer == NULL)
		return 0;

	read_len = (size_t)len;
	err      = M_io_layer_read(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, (unsigned char *)buf, &read_len, NULL);
	handle->last_io_err = err;
	BIO_clear_retry_flags(b);

	if (err != M_IO_ERROR_SUCCESS) {
		if (err == M_IO_ERROR_WOULDBLOCK) {
			BIO_set_retry_read(b);
			return -1;
		} else if (err == M_IO_ERROR_DISCONNECT) {
			return 0;
		}
		/* Error */
		return -1;
	}

	return (int)read_len;
}


static int M_tls_bio_write(BIO *b, const char *buf, int len)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	M_io_layer_t  *layer = BIO_get_data(b);
#else
	M_io_layer_t  *layer = b->ptr;
#endif
	M_io_error_t   err;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	size_t         write_len;

	if (buf == NULL || len <= 0 || layer == NULL)
		return 0;

	write_len = (size_t)len;
	err       = M_io_layer_write(M_io_layer_get_io(layer), M_io_layer_get_index(layer)-1, (const unsigned char *)buf, &write_len, NULL);
	handle->last_io_err = err;
	BIO_clear_retry_flags(b);

	if (err != M_IO_ERROR_SUCCESS) {
		if (err == M_IO_ERROR_WOULDBLOCK) {
			BIO_set_retry_write(b);
			return -1;
		} else if (err == M_IO_ERROR_DISCONNECT) {
			return 0;
		}
		/* Error */
		return -1;
	}

	return (int)write_len;
}


static long M_tls_bio_ctrl(BIO *b, int cmd, long num, void *ptr)
{
	(void)b;
	(void)num;
	(void)ptr;
	switch (cmd) {
		case BIO_CTRL_FLUSH:
			/* Required internally by OpenSSL, no-op though */
			return 1;
	}
	return 0;
}


static int M_tls_bio_puts(BIO *b, const char *str)
{
	return M_tls_bio_write(b, str, (int)M_str_len(str));
}


static void M_tls_bio_method_new(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	M_tls_bio_method = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "openssl mstdlib io glue");

	BIO_meth_set_write(M_tls_bio_method, M_tls_bio_write);
	BIO_meth_set_read(M_tls_bio_method, M_tls_bio_read);
	BIO_meth_set_puts(M_tls_bio_method, M_tls_bio_puts);
	BIO_meth_set_ctrl(M_tls_bio_method, M_tls_bio_ctrl);
	BIO_meth_set_create(M_tls_bio_method, M_tls_bio_create);
	BIO_meth_set_destroy(M_tls_bio_method, M_tls_bio_destroy);
#else
	M_tls_bio_method          = M_malloc_zero(sizeof(*M_tls_bio_method));
	M_tls_bio_method->type    = ( 0x0033 | BIO_TYPE_SOURCE_SINK );  /* Ooo, magic numbers! I think 0x00-0xFF is the ID of the method,
	                                                                 * and 0x0100 - 0xFF00 are the flags, BIO_TYPE_SOURCE_SINK is 0x0400 */
	M_tls_bio_method->name    = "openssl mstdlib io glue";
	M_tls_bio_method->bwrite  = M_tls_bio_write;
	M_tls_bio_method->bread   = M_tls_bio_read;
	M_tls_bio_method->bputs   = M_tls_bio_puts;
	M_tls_bio_method->ctrl    = M_tls_bio_ctrl;
	M_tls_bio_method->create  = M_tls_bio_create;
	M_tls_bio_method->destroy = M_tls_bio_destroy;
#endif
}


static BIO *M_tls_bio_new(M_io_layer_t *layer)
{
	BIO *bio;

	if (layer == NULL)
		return NULL;

	bio      = BIO_new(M_tls_bio_method);
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL && !defined(LIBRESSL_VERSION_NUMBER)
	BIO_set_data(bio, layer);
#else
	bio->ptr = layer;
#endif

	return bio;
}


static M_bool M_io_tls_init_cb(M_io_layer_t *layer)
{
	(void)layer;

	return M_TRUE;
}


static M_io_error_t M_io_tls_read_cb(M_io_layer_t *layer, unsigned char *buf, size_t *read_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle     = M_io_layer_get_handle(layer);
	int            rv;
	int            err;
	size_t         request_len = *read_len;
	M_io_error_t   ioerr;

	(void)meta;

	if (layer == NULL || handle == NULL || handle->ssl == NULL)
		return M_IO_ERROR_INVALID;

	/* Clear READ_WANT_WRITE flag */
	handle->state_flags &= (M_tls_stateflags_t)~(M_TLS_STATEFLAG_READ_WANT_WRITE);

	*read_len = 0;

	/* We need to consume all data, SSL_read() may not do this, so since we need to
	 * act in a edge-triggered manner, do this in a loop */
	while (1) {
		ERR_clear_error();
		rv = SSL_read(handle->ssl, buf+(*read_len), (int)(request_len - *read_len));
		if (rv <= 0)
			break;

		*read_len += (size_t)rv;

		if (request_len == *read_len)
			return M_IO_ERROR_SUCCESS;
	}

	ioerr = M_IO_ERROR_ERROR;
	err   = SSL_get_error(handle->ssl, rv);
	if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
		if (err == SSL_ERROR_WANT_WRITE)
			handle->state_flags |= M_TLS_STATEFLAG_READ_WANT_WRITE;

		ioerr = M_IO_ERROR_WOULDBLOCK;
	} else {
		/* OpenSSL doesn't appear to relay disconnect vs error up the chain for syscalls, manage that ourselves */
		if (err == SSL_ERROR_ZERO_RETURN || (err == SSL_ERROR_SYSCALL && handle->last_io_err == M_IO_ERROR_DISCONNECT)) {
//M_printf("%s(): err == SSL_ERROR_ZERO_RETURN\n", __FUNCTION__);
			handle->state = M_TLS_STATE_DISCONNECTED;
			ioerr         = M_IO_ERROR_DISCONNECT;
		} else {
			handle->state = M_TLS_STATE_ERROR;
		}
		M_io_tls_error_string(err, handle->error, sizeof(handle->error));
	}

	/* Overwrite error condition if we have data */
	if (*read_len != 0) {
		/* Send signal if we obscured a critical error */
		if (ioerr != M_IO_ERROR_WOULDBLOCK) {
			M_io_layer_softevent_add(layer, M_TRUE, ioerr == M_IO_ERROR_DISCONNECT?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR);
		}
		ioerr = M_IO_ERROR_SUCCESS;
	}
//M_printf("%s(): ioerr = %d (%s) - read_len = %zu\n", __FUNCTION__, ioerr, M_io_error_string(ioerr), *read_len);
	return ioerr;
}


static M_io_error_t M_io_tls_write_cb(M_io_layer_t *layer, const unsigned char *buf, size_t *write_len, M_io_meta_t *meta)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	int            rv;
	int            err;
	size_t         request_len = *write_len;
	M_io_error_t   ioerr;

	(void)meta;

	if (layer == NULL || handle == NULL || handle->ssl == NULL)
		return M_IO_ERROR_INVALID;

	/* Clear WRITE_WANT_READ flag */
	handle->state_flags &= (M_tls_stateflags_t)~(M_TLS_STATEFLAG_WRITE_WANT_READ);

	*write_len = 0;

	/* We need to write as much data as possible, SSL_write() may not do this, so since
	 * we need to act in a edge-triggered manner, do this in a loop */
	while (1) {
		ERR_clear_error();
		rv = SSL_write(handle->ssl, buf+(*write_len), (int)(request_len - *write_len));
		if (rv <= 0)
			break;

		*write_len += (size_t)rv;

		if (request_len == *write_len)
			return M_IO_ERROR_SUCCESS;
	}

	ioerr = M_IO_ERROR_ERROR;
	err   = SSL_get_error(handle->ssl, rv);
	if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
		if (err == SSL_ERROR_WANT_READ)
			handle->state_flags |= M_TLS_STATEFLAG_WRITE_WANT_READ;

		ioerr = M_IO_ERROR_WOULDBLOCK;
	} else {
		/* OpenSSL doesn't appear to relay disconnect vs error up the chain for syscalls, manage that ourselves */
		if (err == SSL_ERROR_ZERO_RETURN || (err == SSL_ERROR_SYSCALL && handle->last_io_err == M_IO_ERROR_DISCONNECT)) {
//M_printf("%s(): err == SSL_ERROR_ZERO_RETURN\n", __FUNCTION__);
			handle->state = M_TLS_STATE_DISCONNECTED;
			ioerr         = M_IO_ERROR_DISCONNECT;
		} else {
			handle->state = M_TLS_STATE_ERROR;
		}
		M_io_tls_error_string(err, handle->error, sizeof(handle->error));
	}

	/* Overwrite error condition if we wrote data */
	if (*write_len != 0) {
		/* Send signal if we obscured a critical error */
		if (ioerr != M_IO_ERROR_WOULDBLOCK) {
			M_io_layer_softevent_add(layer, M_TRUE, ioerr == M_IO_ERROR_DISCONNECT?M_EVENT_TYPE_DISCONNECTED:M_EVENT_TYPE_ERROR);
		}
		ioerr = M_IO_ERROR_SUCCESS;
	}

	return ioerr;
}


static M_bool M_io_tls_disconnect_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);
	M_event_t     *event  = M_io_get_event(io);
	int            rv;
	int            err;

	if (handle->state != M_TLS_STATE_CONNECTED) {
		/* Already started shutdown, ignore */
		if (handle->state == M_TLS_STATE_SHUTDOWN)
			return M_FALSE;
		return M_TRUE;
	}

	handle->state = M_TLS_STATE_SHUTDOWN;

	ERR_clear_error();
	rv = SSL_shutdown(handle->ssl);
//M_printf("SSL_shutdown returned %d\n", rv);
	if (rv == 1) {
		handle->state = M_TLS_STATE_DISCONNECTED;
		return M_TRUE; /* Go to next layer */
	}

	err = SSL_get_error(handle->ssl, rv);
//M_printf("SSL_shutdown err %d\n", err);
	if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_SYSCALL) {
		handle->state = M_TLS_STATE_DISCONNECTED;
		return M_TRUE; /* Go to next layer, even though this is an error */
	}
//M_printf("SSL_shutdown started\n");
	/* Ok, we've initiated a disconnect sequence, start a timer so this can
	 * be canceled if it takes too long */
	handle->timer = M_event_timer_oneshot(event, 5000 /* 5s max */, M_FALSE, M_tls_op_timeout_cb, layer);

	/* Don't go to next layer, processing disconnect */
	return M_FALSE;
}


static void M_io_tls_save_session(M_io_handle_t *handle, unsigned int port)
{
	char        *hostport = NULL;
	SSL_SESSION *session;

	if (!handle->is_client || !handle->clientctx->sessions_enabled)
		return;

	if (M_str_isempty(handle->hostname))
		return;

	session = SSL_get1_session(handle->ssl);
#if OPENSSL_VERSION_NUMBER >= 0x1010100fL && !defined(LIBRESSL_VERSION_NUMBER)
	/* If it's not resumable we won't store it and
 	 * we want to remove any stored sessions from the
	 * cache for this host and port because it shouldn't
	 * be resumable either. */
	if (session != NULL && !SSL_SESSION_is_resumable(session))
		session = NULL;
#endif

	M_asprintf(&hostport, "%s:%u", handle->hostname, port);

	M_thread_mutex_lock(handle->clientctx->lock);
	if (session == NULL) {
		M_hash_strvp_remove(handle->clientctx->sessions, hostport, M_TRUE);
	} else {
		M_hash_strvp_insert(handle->clientctx->sessions, hostport, session);
	}
	M_thread_mutex_unlock(handle->clientctx->lock);

	M_free(hostport);
}


static M_bool M_io_tls_reset_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);
	M_io_t        *io     = M_io_layer_get_io(layer);

	if (handle == NULL)
		return M_FALSE;

	/* Save session */
	if (handle->state == M_TLS_STATE_CONNECTED || handle->state == M_TLS_STATE_SHUTDOWN || handle->state == M_TLS_STATE_DISCONNECTED) {
		/* Tell OpenSSL that shutdown was successful otherwise it may not mark the session as resumable */
		SSL_set_shutdown(handle->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
	}

	/* If client connection, we have additional work to do to save the session */
	M_io_tls_save_session(handle, (unsigned int)M_io_net_get_port(io));

	if (handle->ssl != NULL) {
		SSL_free(handle->ssl);
	}

	handle->ssl              = NULL;
	/* SSL_free() auto-frees the bio BIO_free(handle->bio_glue); */
	handle->bio_glue         = NULL;
	M_event_timer_remove(handle->timer);
	handle->timer            = NULL;
	handle->state            = M_TLS_STATE_INIT;
	handle->state_flags      = 0;
	handle->last_io_err      = M_IO_ERROR_SUCCESS;
	M_mem_set(&handle->negotiation_start, 0, sizeof(handle->negotiation_start));
	handle->negotiation_time = 0;
	*(handle->error)         = '\0';

	return M_TRUE;
}


static void M_io_tls_destroy_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle == NULL)
		return;

	/* reset_cb() will be called to clean up most things */

	if (handle->is_client) {
		M_tls_clientctx_destroy(handle->clientctx);
		handle->clientctx = NULL;
	} else {
		M_tls_serverctx_destroy(handle->serverctx);
		handle->serverctx = NULL;
	}

	M_free(handle->hostname);
	handle->hostname = NULL;

	M_free(handle);
}


static M_io_state_t M_io_tls_state_cb(M_io_layer_t *layer)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	switch (handle->state) {
		case M_TLS_STATE_INIT:
			return M_IO_STATE_INIT;
		case M_TLS_STATE_CONNECTING:
			return M_IO_STATE_CONNECTING;
		case M_TLS_STATE_ACCEPTING:
			return M_IO_STATE_CONNECTING;
		case M_TLS_STATE_CONNECTED:
			return M_IO_STATE_CONNECTED;
		case M_TLS_STATE_SHUTDOWN:
			return M_IO_STATE_DISCONNECTING;
		case M_TLS_STATE_DISCONNECTED:
			return M_IO_STATE_DISCONNECTED;
		case M_TLS_STATE_ERROR:
		default:
			break;
	};

	return M_IO_STATE_ERROR;
}

static M_bool M_io_tls_errormsg_cb(M_io_layer_t *layer, char *error, size_t err_len)
{
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (handle->state != M_TLS_STATE_ERROR)
		return M_FALSE;

	if (M_str_isempty(handle->error))
		return M_FALSE;

	M_str_cpy(error, err_len, handle->error);
	return M_TRUE;
}


M_io_error_t M_io_tls_client_add(M_io_t *io, M_tls_clientctx_t *ctx, const char *hostname, size_t *layer_id)
{
	M_io_handle_t    *handle;
	M_io_layer_t     *layer;
	M_io_callbacks_t *callbacks;
	SSL_SESSION      *session;

	if (io == NULL || ctx == NULL)
		return M_IO_ERROR_INVALID;

	M_tls_clientctx_upref(ctx);

	handle              = M_malloc_zero(sizeof(*handle));
	handle->is_client   = M_TRUE; /* To know if we are accepting (server) or connecting (client) */
	handle->clientctx   = ctx;

	/* If a hostname wasn't provided, see if the underlying object is a network connection, if so,
	 * get the hostname from there */
	if (M_str_isempty(hostname)) {
		hostname = M_io_net_get_host(io);
	}

	handle->hostname    = M_strdup(hostname);
	handle->ssl         = SSL_new(handle->clientctx->ctx);

	/* Attempt to look up session object to use */
	if (!M_str_isempty(hostname) && ctx->sessions_enabled) {
		char *hostport = NULL;
		M_asprintf(&hostport, "%s:%u", handle->hostname, (unsigned int)M_io_net_get_port(io));

		/* Attempt to resume session */
		M_thread_mutex_lock(ctx->lock);
		session = M_hash_strvp_multi_get_direct(ctx->sessions, hostport, 0);
		if (session) {
			SSL_set_session(handle->ssl, session);
			/* This will also reduce the reference count on thet session */
			M_hash_strvp_multi_remove(ctx->sessions, hostport, 0, M_TRUE);
		}
		M_thread_mutex_unlock(ctx->lock);
		M_free(hostport);
	}

	if (!M_str_isempty(handle->hostname)) {
		/* support SNI */
		SSL_set_tlsext_host_name(handle->ssl, handle->hostname);
	}

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_tls_init_cb);
	M_io_callbacks_reg_read(callbacks, M_io_tls_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_tls_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_tls_process_cb);
	//M_io_callbacks_reg_unregister(callbacks, M_io_tls_unregister_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_tls_disconnect_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_tls_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_tls_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_tls_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_tls_errormsg_cb);
	layer = M_io_layer_add(io, "TLS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);

	/* Set the layer as the 'thunk' data for the custom bio */
	handle->bio_glue = M_tls_bio_new(layer);
	SSL_set_bio(handle->ssl, handle->bio_glue, handle->bio_glue);

	return M_IO_ERROR_SUCCESS;
}


static M_io_error_t M_io_tls_accept_cb(M_io_t *io, M_io_layer_t *orig_layer)
{
	M_io_error_t   err;
	size_t         layer_id;
	M_io_layer_t  *layer;
	M_io_handle_t *handle;
	M_io_handle_t *orig_handle = M_io_layer_get_handle(orig_layer);

	/* Add a new layer into the new io object with the same settings as we have */
	err = M_io_tls_server_add(io, orig_handle->serverctx, &layer_id);
	if (err != M_IO_ERROR_SUCCESS)
		return err;

	layer  = M_io_layer_acquire(io, layer_id, "TLS");
	if (layer == NULL)
		return M_IO_ERROR_ERROR;

	handle = M_io_layer_get_handle(layer);

	/* Initialize SSL handle */
	handle->ssl         = SSL_new(handle->serverctx->ctx);

	/* If DHE negotiation is enabled, set it up now */
	if (handle->serverctx->dh) 
		SSL_set_tmp_dh(handle->ssl, handle->serverctx->dh);

	/* Set the layer as the 'thunk' data for the custom bio */
	handle->bio_glue    = M_tls_bio_new(layer);
	SSL_set_bio(handle->ssl, handle->bio_glue, handle->bio_glue);

	M_io_layer_release(layer);
	return M_IO_ERROR_SUCCESS;
}


M_io_error_t M_io_tls_server_add(M_io_t *io, M_tls_serverctx_t *ctx, size_t *layer_id)
{
	M_io_handle_t    *handle;
	M_io_layer_t     *layer;
	M_io_callbacks_t *callbacks;

	if (io == NULL || ctx == NULL)
		return M_IO_ERROR_INVALID;

	/* XXX: Verify cert is added to ctx otherwise fail */

	M_tls_serverctx_upref(ctx);

	handle              = M_malloc_zero(sizeof(*handle));
	handle->is_client   = M_FALSE; /* To know if we are accepting (server) or connecting (client) */
	handle->serverctx   = ctx;

	callbacks = M_io_callbacks_create();
	M_io_callbacks_reg_init(callbacks, M_io_tls_init_cb);
	M_io_callbacks_reg_accept(callbacks, M_io_tls_accept_cb);
	M_io_callbacks_reg_read(callbacks, M_io_tls_read_cb);
	M_io_callbacks_reg_write(callbacks, M_io_tls_write_cb);
	M_io_callbacks_reg_processevent(callbacks, M_io_tls_process_cb);
	//M_io_callbacks_reg_unregister(callbacks, M_io_tls_unregister_cb);
	M_io_callbacks_reg_disconnect(callbacks, M_io_tls_disconnect_cb);
	M_io_callbacks_reg_reset(callbacks, M_io_tls_reset_cb);
	M_io_callbacks_reg_destroy(callbacks, M_io_tls_destroy_cb);
	M_io_callbacks_reg_state(callbacks, M_io_tls_state_cb);
	M_io_callbacks_reg_errormsg(callbacks, M_io_tls_errormsg_cb);
	layer = M_io_layer_add(io, "TLS", handle, callbacks);
	M_io_callbacks_destroy(callbacks);

	if (layer_id != NULL)
		*layer_id = M_io_layer_get_index(layer);

	return M_IO_ERROR_SUCCESS;
}


M_tls_protocols_t M_tls_get_protocol(M_io_t *io, size_t id)
{

	M_io_layer_t      *layer  = M_io_layer_acquire(io, id, "TLS");
	M_tls_protocols_t  ret    = M_TLS_PROTOCOL_INVALID;
	M_io_handle_t     *handle = M_io_layer_get_handle(layer);
	const struct {
		const char        *name;
		M_tls_protocols_t  protocol;
	} versions[] = {
		{ "TLSv1",   M_TLS_PROTOCOL_TLSv1_0 },
		{ "TLSv1.1", M_TLS_PROTOCOL_TLSv1_1 },
		{ "TLSv1.2", M_TLS_PROTOCOL_TLSv1_2 },
		{ "TLSv1.3", M_TLS_PROTOCOL_TLSv1_3 },
		{ NULL, 0 }
	};

	if (layer == NULL)
		return ret;

	if (handle->ssl != NULL) {
		const char *const_temp = SSL_get_version(handle->ssl);
		size_t      i;
		for (i=0; versions[i].name != NULL; i++) {
			if (M_str_caseeq(versions[i].name, const_temp)) {
				ret = versions[i].protocol;
				break;
			}
		}
	}

	M_io_layer_release(layer);

	return ret;
}


M_bool M_tls_get_sessionreused(M_io_t *io, size_t id)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, id, "TLS");
	M_bool         ret    = M_FALSE;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL)
		return M_FALSE;

	if (handle->ssl != NULL)
		ret = SSL_session_reused(handle->ssl)?M_TRUE:M_FALSE;

	M_io_layer_release(layer);

	return ret;
}


const char *M_tls_get_cipher(M_io_t *io, size_t id)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, id, "TLS");
	const char    *ret    = NULL;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL)
		return NULL;

	if (handle->ssl != NULL)
		ret = SSL_get_cipher_name(handle->ssl);

	M_io_layer_release(layer);

	return ret;
}


const char *M_tls_protocols_to_str(M_tls_protocols_t protocol)
{
	switch (protocol) {
		case M_TLS_PROTOCOL_TLSv1_0:
			return "TLSv1.0";
		case M_TLS_PROTOCOL_TLSv1_1:
			return "TLSv1.1";
		case M_TLS_PROTOCOL_TLSv1_2:
			return "TLSv1.2";
		case M_TLS_PROTOCOL_TLSv1_3:
			return "TLSv1.3";
		default:
			break;
	}
	return "Unknown";
}



M_tls_protocols_t M_tls_protocols_from_str(const char *protocols_str)
{
	M_tls_protocols_t   protocols = M_TLS_PROTOCOL_INVALID;
	char              **parts;
	size_t              num_parts = 0;
	size_t              i;
	size_t              j;
	static struct {
		const char        *name;
		M_tls_protocols_t  protocols;
	} protcol_flags[] = {
		{ "tlsv1",    M_TLS_PROTOCOL_TLSv1_0|M_TLS_PROTOCOL_TLSv1_1|M_TLS_PROTOCOL_TLSv1_2|M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1+",   M_TLS_PROTOCOL_TLSv1_0|M_TLS_PROTOCOL_TLSv1_1|M_TLS_PROTOCOL_TLSv1_2|M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1.0",  M_TLS_PROTOCOL_TLSv1_0 },
		{ "tlsv1.0+", M_TLS_PROTOCOL_TLSv1_0|M_TLS_PROTOCOL_TLSv1_1|M_TLS_PROTOCOL_TLSv1_2|M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1.1",  M_TLS_PROTOCOL_TLSv1_1 },
		{ "tlsv1.1+", M_TLS_PROTOCOL_TLSv1_1|M_TLS_PROTOCOL_TLSv1_2|M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1.2",  M_TLS_PROTOCOL_TLSv1_2 },
		{ "tlsv1.2+", M_TLS_PROTOCOL_TLSv1_2|M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1.3",  M_TLS_PROTOCOL_TLSv1_3 },
		{ "tlsv1.3+", M_TLS_PROTOCOL_TLSv1_3 },
		{ NULL, 0 }
	};

	if (M_str_isempty(protocols_str))
		return M_TLS_PROTOCOL_INVALID;

	parts = M_str_explode_str(' ', protocols_str, &num_parts);
	if (parts == NULL || num_parts == 0)
		return M_TLS_PROTOCOL_INVALID;

	for (i=0; i<num_parts; i++) {
		if (M_str_isempty(parts[i])) {
			continue;
		}

		for (j=0; protcol_flags[j].name!=NULL; j++) {
			if (M_str_caseeq(parts[i], protcol_flags[j].name)) {
				protocols |= protcol_flags[j].protocols;
			}
		}
	}
	M_str_explode_free(parts, num_parts);

#if OPENSSL_VERSION_NUMBER < 0x1010100fL || defined(LIBRESSL_VERSION_NUMBER)
	protocols &= ~M_TLS_PROTOCOL_TLSv1_3;
#endif

	return protocols;
}


const char *M_tls_server_get_hostname(M_io_t *io, size_t id)
{
	M_io_layer_t  *layer  = M_io_layer_acquire(io, id, "TLS");
	const char    *ret    = NULL;
	M_io_handle_t *handle = M_io_layer_get_handle(layer);

	if (layer == NULL)
		return NULL;

	ret = SSL_get_servername(handle->ssl, TLSEXT_NAMETYPE_host_name);

	M_io_layer_release(layer);

	return ret;
}


char *M_tls_get_peer_cert(M_io_t *io, size_t id)
{
	M_io_layer_t  *layer    = M_io_layer_acquire(io, id, "TLS");
	char          *buf      = NULL;
	size_t         buf_size = 0;
	M_io_handle_t *handle   = M_io_layer_get_handle(layer);
	X509          *x509     = NULL;
	BIO           *bio      = NULL;

	if (layer == NULL)
		return NULL;

	if (handle->ssl == NULL)
		goto done;

	/* Get Peer */
	x509 = SSL_get_peer_certificate(handle->ssl);
	if (x509 == NULL)
		goto done;

	/* Write as PEM encoded cert */
	bio = BIO_new(BIO_s_mem());
	if (bio == NULL) {
		goto done;
	}

	if (!PEM_write_bio_X509(bio, x509)) {
		goto done;
	}

	buf_size = (size_t)BIO_ctrl_pending(bio);
	buf      = M_malloc_zero(buf_size+1);
	if (buf == NULL) {
		goto done;
	}

	if ((size_t)BIO_read(bio, buf, (int)buf_size) != buf_size) {
		M_free(buf);
		buf = NULL;
		goto done;
	}

done:
	if (x509 != NULL)
		X509_free(x509);

	if (bio != NULL)
		BIO_free(bio);

	M_io_layer_release(layer);
	return buf;
}


/*! Returns NULL if there is no ALPN support or on error, or will return the application. */
char *M_tls_get_application(M_io_t *io, size_t id)
{
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
	(void)io;
	(void)id;
	return NULL;
#else
	M_io_layer_t        *layer  = M_io_layer_acquire(io, id, "TLS");
	M_io_handle_t       *handle = M_io_layer_get_handle(layer);
	const unsigned char *app    = NULL;
	unsigned int         len    = 0;
	char                *ret    = NULL;

	if (layer == NULL)
		return NULL;

	if (handle->ssl) {
		SSL_get0_alpn_selected(handle->ssl, &app, &len);
		/* Docs don't say, but this does NOT have a length prefix so it isn't
		 * in the same format as everything else */
		if (app && len >= 1) {
			/* Skip over length indicator, and it isn't NULL terminated */
			ret = M_strdup_max((const char *)app, len);
		}
	}

	M_io_layer_release(layer);

	return ret;
#endif
}

M_uint64 M_tls_get_negotiation_time_ms(M_io_t *io, size_t id)
{
	M_io_layer_t        *layer  = M_io_layer_acquire(io, id, "TLS");
	M_io_handle_t       *handle = M_io_layer_get_handle(layer);
	M_uint64             ret    = 0;

	if (layer == NULL)
		return 0;

	if (handle->state == M_TLS_STATE_CONNECTING || handle->state == M_TLS_STATE_ACCEPTING) {
		/* Return current elapsed time */
		ret = M_time_elapsed(&handle->negotiation_start);
	} else {
		/* Return cached time */
		ret = handle->negotiation_time;
	}

	M_io_layer_release(layer);

	return ret;
}
