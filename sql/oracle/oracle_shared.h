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
#ifndef __ORACLE_SHARED_H__
#define __ORACLE_SHARED_H__

#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>

M_sql_error_t oracle_resolve_error(const char *sqlstate, M_int32 errorcode);
M_sql_error_t oracle_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size);
M_bool oracle_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len, M_bool is_cast);
void oracle_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name);
M_bool oracle_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2);
char *oracle_cb_rewrite_indexname(M_sql_connpool_t *pool, const char *index_name);

#endif
