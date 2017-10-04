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
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_tls.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "base/m_defs_int.h"
#include "m_tls_clientctx_int.h"


/*! Used when destroying the hashtable */
static void M_tls_clientctx_session_destroy(void *session)
{
	if (session != NULL)
		SSL_SESSION_free(session);
}


M_tls_clientctx_t *M_tls_clientctx_create(void)
{
	M_tls_clientctx_t *ctx;

	M_tls_init(M_TLS_INIT_NORMAL);

	ctx                         = M_malloc_zero(sizeof(*ctx));
	ctx->ctx                    = M_tls_ctx_init(M_FALSE);
	if (ctx->ctx == NULL) {
		M_free(ctx);
		return NULL;
	}
	ctx->lock                   = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);

	/* Session support */
	ctx->sessions               = M_hash_strvp_create(16, 75, M_HASH_STRVP_MULTI_VALUE, M_tls_clientctx_session_destroy);

	ctx->verify_level           = M_TLS_VERIFY_FULL;

	ctx->ref_cnt                = 1;
	ctx->negotiation_timeout_ms = 10000;

	/* XXX: Default certificate trust list loading? Import from Windows? Search on Unix? */
	return ctx;
}


M_bool M_tls_clientctx_upref(M_tls_clientctx_t *ctx)
{
	if (ctx == NULL)
		return M_FALSE;
	M_thread_mutex_lock(ctx->lock);
	ctx->ref_cnt++;
	M_thread_mutex_unlock(ctx->lock);
	return M_TRUE;
}


static void M_tls_clientctx_destroy_real(M_tls_clientctx_t *ctx)
{
	M_hash_strvp_destroy(ctx->sessions, M_TRUE);
	M_tls_ctx_destroy(ctx->ctx);
	/* Locked when we entered */
	M_thread_mutex_unlock(ctx->lock);
	M_thread_mutex_destroy(ctx->lock);
	M_free(ctx);
}

void M_tls_clientctx_destroy(M_tls_clientctx_t *ctx)
{
	if (ctx == NULL)
		return;

	M_thread_mutex_lock(ctx->lock);
	if (ctx->ref_cnt > 0) {
		ctx->ref_cnt--;
	}
	if (ctx->ref_cnt != 0) {
		M_thread_mutex_unlock(ctx->lock);
		return;
	}

	M_tls_clientctx_destroy_real(ctx);
}


M_bool M_tls_clientctx_set_protocols(M_tls_clientctx_t *ctx, int protocols /* M_tls_protocols_t bitmap */)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_protocols(ctx->ctx, protocols);
	M_thread_mutex_unlock(ctx->lock);
	return retval;
}


M_bool M_tls_clientctx_set_ciphers(M_tls_clientctx_t *ctx, const char *ciphers)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_ciphers(ctx->ctx, ciphers);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_cert(M_tls_clientctx_t *ctx, const unsigned char *key, size_t key_len, const unsigned char *crt, size_t crt_len, const unsigned char *intermediate, size_t intermediate_len)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_cert(ctx->ctx, key, key_len, crt, crt_len, intermediate, intermediate_len, NULL);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_cert_files(M_tls_clientctx_t *ctx, const char *keypath, const char *crtpath, const char *intermediatepath)
{
	M_bool         retval;
	unsigned char *crt              = NULL;
	unsigned char *key              = NULL;
	unsigned char *intermediate     = NULL;
	size_t         crt_len          = 0;
	size_t         key_len          = 0;
	size_t         intermediate_len = 0;

	if (ctx == NULL || keypath == NULL || crtpath == NULL)
		return M_FALSE;

	if (M_fs_file_read_bytes(crtpath, 0, &crt, &crt_len) != M_FS_ERROR_SUCCESS)
		return M_FALSE;

	if (M_fs_file_read_bytes(keypath, 0, &key, &key_len) != M_FS_ERROR_SUCCESS) {
		M_free(crt);
		return M_FALSE;
	}

	if (!M_str_isempty(intermediatepath)) {
		if (M_fs_file_read_bytes(intermediatepath, 0, &intermediate, &intermediate_len) != M_FS_ERROR_SUCCESS) {
			M_free(crt);
			M_free(key);
			return M_FALSE;
		}
	}

	retval = M_tls_clientctx_set_cert(ctx, key, key_len, crt, crt_len, intermediate, intermediate_len);
	M_free(intermediate);
	M_free(crt);
	M_free(key);
	return retval;
}


M_bool M_tls_clientctx_set_default_trust(M_tls_clientctx_t *ctx)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_load_os_trust(ctx->ctx);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_trust_ca(M_tls_clientctx_t *ctx, const unsigned char *ca, size_t len)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_trust_ca(ctx->ctx, NULL, ca, len);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_trust_ca_file(M_tls_clientctx_t *ctx, const char *path)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_trust_ca_file(ctx->ctx, NULL, path);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_trust_cert(M_tls_clientctx_t *ctx, const unsigned char *crt, size_t len)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_trust_cert(ctx->ctx, NULL, crt, len);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_trust_cert_file(M_tls_clientctx_t *ctx, const char *path)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_trust_cert_file(ctx->ctx, NULL, path);
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_trust_ca_dir(M_tls_clientctx_t *ctx, const char *path)
{
	M_bool retval;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	retval = M_tls_ctx_set_trust_ca_dir(ctx->ctx, NULL, path, "*.pem");
	M_thread_mutex_unlock(ctx->lock);

	return retval;
}


M_bool M_tls_clientctx_set_verify_level(M_tls_clientctx_t *ctx, M_tls_verify_level_t level)
{
	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	ctx->verify_level = level;
	M_thread_mutex_unlock(ctx->lock);

	return M_TRUE;
}


/*! ALPN support */
M_bool M_tls_clientctx_set_applications(M_tls_clientctx_t *ctx, M_list_str_t *applications)
{
#if OPENSSL_VERSION_NUMBER < 0x1000200fL
	(void)ctx;
	(void)applications;
	return M_FALSE;
#else
	unsigned char *apps     = NULL;
	size_t         apps_len = 0;
	M_bool         retval   = M_TRUE;

	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);

	apps = M_tls_alpn_list(applications, &apps_len);

	/* docs say this returns 0 on success, unlike most other functions */
	if (SSL_CTX_set_alpn_protos(ctx->ctx, apps, (unsigned int)apps_len) != 0)
		retval = M_FALSE;

	M_free(apps);

	M_thread_mutex_unlock(ctx->lock);

	return retval;
#endif
}

M_bool M_tls_clientctx_set_negotiation_timeout_ms(M_tls_clientctx_t *ctx, M_uint64 timeout_ms)
{
	if (ctx == NULL)
		return M_FALSE;

	if (timeout_ms == 0)
		timeout_ms = 10000;

	M_thread_mutex_lock(ctx->lock);
	ctx->negotiation_timeout_ms = timeout_ms;
	M_thread_mutex_unlock(ctx->lock);
	return M_TRUE;
}

M_bool M_tls_clientctx_set_session_resumption(M_tls_clientctx_t *ctx, M_bool enable)
{
	if (ctx == NULL)
		return M_FALSE;

	M_thread_mutex_lock(ctx->lock);
	ctx->sessions_enabled = enable;
	M_thread_mutex_unlock(ctx->lock);
	return M_TRUE;
}

char *M_tls_clientctx_get_cipherlist(M_tls_clientctx_t *ctx)
{
	char *ret = NULL;

	if (ctx == NULL)
		return NULL;

	M_thread_mutex_lock(ctx->lock);
	ret = M_tls_ctx_get_cipherlist(ctx->ctx);
	M_thread_mutex_unlock(ctx->lock);

	return ret;
}
