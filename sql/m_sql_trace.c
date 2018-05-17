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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"


struct M_sql_trace_data {
	M_sql_trace_t  type;
	M_sql_conn_t  *conn;
	M_sql_trans_t *trans;
	M_sql_stmt_t  *stmt;
	M_sql_error_t  err;
	const char    *error_msg;
};


static void M_sql_trace_message(M_sql_trace_t type, M_sql_connpool_t *pool, M_sql_conn_t *conn, M_sql_trans_t *trans, M_sql_stmt_t *stmt, M_sql_error_t err, const char *error)
{
	void              *cb_arg = NULL;
	M_sql_trace_cb_t   cb;
	M_sql_trace_data_t data;  

	if (trans == NULL) {
		if (stmt != NULL)
			trans = M_sql_stmt_get_trans(stmt);
	}

	if (conn == NULL) {
		if (trans != NULL) {
			conn = M_sql_trans_get_conn(trans);
		} else if (stmt != NULL) {
			conn = M_sql_stmt_get_conn(stmt);
		}
	}

	if (pool == NULL)
		pool = M_sql_driver_conn_get_pool(conn);

	if (stmt == NULL)
		stmt = M_sql_conn_get_curr_stmt(conn);

	/* We must have a pool handle */
	if (pool == NULL)
		return;

	cb   = M_sql_connpool_get_cb(pool, &cb_arg);
	if (cb == NULL)
		return;

	if (stmt != NULL && err == M_SQL_ERROR_UNSET)
		err = M_sql_stmt_get_error(stmt);

	if (stmt != NULL && M_str_isempty(error))
		error = M_sql_stmt_get_error_string(stmt);

	data.type      = type;
	data.conn      = conn;
	data.trans     = trans;
	data.stmt      = stmt;
	data.err       = err;
	data.error_msg = error;

	cb(type, &data, cb_arg);

	if (err == M_SQL_ERROR_UNSET)
		return;

	if (type == M_SQL_TRACE_TRANFAIL || type == M_SQL_TRACE_CONNFAIL || type == M_SQL_TRACE_CONNECT_FAILED)
		return;

	/* Log TRANFAIL and CONNFAIL as appropriate */
	if (M_sql_error_is_fatal(err) && (stmt == NULL || !stmt->ignore_tranfail))
		M_sql_trace_message(M_SQL_TRACE_TRANFAIL, pool, conn, trans, stmt, err, error);

	if (M_sql_error_is_disconnect(err))
		M_sql_trace_message(M_SQL_TRACE_CONNFAIL, pool, conn, trans, stmt, err, error);
}


void M_sql_driver_trace_message(M_bool is_debug, M_sql_connpool_t *pool, M_sql_conn_t *conn, M_sql_error_t err, const char *msg)
{
	M_sql_trace_message(is_debug?M_SQL_TRACE_DRIVER_DEBUG:M_SQL_TRACE_DRIVER_ERROR, pool, conn, NULL, NULL, err, msg);
}


void M_sql_trace_message_conn(M_sql_trace_t type, M_sql_conn_t *conn, M_sql_error_t err, const char *error)
{
	M_sql_trace_message(type, NULL, conn, NULL, NULL, err, error);
}


void M_sql_trace_message_trans(M_sql_trace_t type, M_sql_trans_t *trans, M_sql_error_t err, const char *error)
{
	M_sql_trace_message(type, NULL, NULL, trans, NULL, err, error);
}


void M_sql_trace_message_stmt(M_sql_trace_t type, M_sql_stmt_t *stmt)
{
	M_sql_trace_message(type, NULL, NULL, NULL, stmt, M_SQL_ERROR_UNSET, NULL);
}



const char *M_sql_trace_get_error_string(const M_sql_trace_data_t *data)
{
	if (data == NULL)
		return NULL;
	return data->error_msg;
}


M_sql_error_t M_sql_trace_get_error(const M_sql_trace_data_t *data)
{
	if (data == NULL)
		return M_SQL_ERROR_INVALID_USE;
	return data->err;
}


M_uint64 M_sql_trace_get_duration_ms(const M_sql_trace_data_t *data)
{
	if (data == NULL)
		return 0;

	switch (data->type) {
		case M_SQL_TRACE_CONNECTED:       /* Time to establish connection */
		case M_SQL_TRACE_CONNECT_FAILED:  /* Time it took for connection to fail. */
		case M_SQL_TRACE_DISCONNECTING:   /* Time connection was up before disconnect was attempted. */
		case M_SQL_TRACE_CONNFAIL:        /* Time connection was up before a failure was detected. */
			return M_sql_conn_duration_start_ms(data->conn);

		case M_SQL_TRACE_DISCONNECTED:    /* Time connection took to disconnect (from start of disconnect) */
			return M_sql_conn_duration_last_ms(data->conn);

		case M_SQL_TRACE_BEGIN_FINISH:    /* Time it took to begin a transaction. */
		case M_SQL_TRACE_ROLLBACK_FINISH: /* Time it took to rollback. */
		case M_SQL_TRACE_COMMIT_FINISH:   /* Time it took to commit a transaction. */
			return M_sql_trans_duration_last_ms(data->trans);

		case M_SQL_TRACE_EXECUTE_FINISH:  /* Time it took to execute the transaction. */
		case M_SQL_TRACE_TRANFAIL:        /* Time query execution took before failure was returned. */
			return M_sql_stmt_duration_start_ms(data->stmt);
		case M_SQL_TRACE_FETCH_FINISH:    /* Time it took to retrieve the rows after execution. */
			return M_sql_stmt_duration_last_ms(data->stmt);
		case M_SQL_TRACE_STALL_QUERY:
			return M_sql_conn_duration_query_ms(data->conn);
		case M_SQL_TRACE_STALL_TRANS_IDLE:
			return M_sql_conn_duration_trans_last_ms(data->conn);
		case M_SQL_TRACE_STALL_TRANS_LONG:
			return M_sql_conn_duration_trans_ms(data->conn);

		case M_SQL_TRACE_FETCH_START:
		case M_SQL_TRACE_EXECUTE_START:
		case M_SQL_TRACE_COMMIT_START:
		case M_SQL_TRACE_ROLLBACK_START:
		case M_SQL_TRACE_BEGIN_START:
		case M_SQL_TRACE_CONNECTING:
		case M_SQL_TRACE_DRIVER_DEBUG:
		case M_SQL_TRACE_DRIVER_ERROR:
			/* Not Supported */
			break;
	}
	return 0;
}


M_uint64 M_sql_trace_get_total_duration_ms(const M_sql_trace_data_t *data)
{
	if (data == NULL)
		return 0;

	switch (data->type) {
		case M_SQL_TRACE_FETCH_FINISH: /* Total time of query execution plus row fetch time. */
			return M_sql_stmt_duration_start_ms(data->stmt);

		case M_SQL_TRACE_DISCONNECTED: /* Total time from connection establishment to disconnect end. */
			return M_sql_conn_duration_start_ms(data->conn);

		case M_SQL_TRACE_STALL_TRANS_IDLE:
			return M_sql_conn_duration_trans_ms(data->conn);

		default:
			break;
	}

	return 0;
}


M_sql_conn_type_t M_sql_trace_get_conntype(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->conn == NULL)
		return M_SQL_CONN_TYPE_UNKNOWN;

	if (M_sql_driver_conn_is_readonly(data->conn))
		return M_SQL_CONN_TYPE_READONLY;

	return M_SQL_CONN_TYPE_PRIMARY;
}


size_t M_sql_trace_get_conn_id(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->conn == NULL)
		return 0;

	return M_sql_driver_conn_get_id(data->conn);
}


const char *M_sql_trace_get_query_user(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return NULL;
	return data->stmt->query_user;
}


const char *M_sql_trace_get_query_prepared(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return NULL;
	return M_sql_driver_stmt_get_query(data->stmt);
}


size_t M_sql_trace_get_bind_cols(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return 0;
	return M_sql_driver_stmt_bind_cnt(data->stmt);
}


size_t M_sql_trace_get_bind_rows(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return 0;
	return data->stmt->bind_row_cnt; /* Don't use helper as we want entire count, not subset */
}


M_bool M_sql_trace_get_has_result_rows(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return M_FALSE;
	return M_sql_stmt_result_num_cols(data->stmt) > 0?M_TRUE:M_FALSE;
}


size_t M_sql_trace_get_affected_rows(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL)
		return 0;
	return M_sql_stmt_result_affected_rows(data->stmt);
}


size_t M_sql_trace_get_result_row_count(const M_sql_trace_data_t *data)
{
	if (data == NULL || data->stmt == NULL || data->stmt->result == NULL)
		return 0;
	return data->stmt->result->total_rows;
}


void M_sql_trace_ignore_tranfail(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return;
	stmt->ignore_tranfail = M_TRUE;
}
