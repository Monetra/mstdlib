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

#ifndef __M_TLS_CLIENTCTX_INT_H__
#define __M_TLS_CLIENTCTX_INT_H__

#include "m_tls_ctx_common.h"

struct M_tls_clientctx {
	M_thread_mutex_t    *lock;                   /*!< Mutex to protect concurrent access                                 */
	SSL_CTX             *ctx;                    /*!< OpenSSL's context                                                  */
	size_t               ref_cnt;                /*!< Reference count to prevent destroy of CTX while connections active */
	M_hash_strvp_t      *sessions;               /*!< Storage of session handles for future renegotiation                */
	M_tls_verify_level_t verify_level;           /*!< Certificate verification level                                     */
	M_bool               sessions_enabled;       /*!< Whether or not session resumption is desired                       */
	M_uint64             negotiation_timeout_ms; /*!< Amount of time negotiation can take                                */
};

#endif
