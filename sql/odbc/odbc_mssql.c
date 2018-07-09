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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "odbc_mssql.h"


M_sql_error_t mssql_resolve_error(const char *sqlstate, M_int32 errorcode)
{
	(void)sqlstate;
	switch (errorcode) {
		/* From http://technet.microsoft.com/en-us/library/cc917589.aspx */
		case 6001: /* SHUTDOWN is waiting for %d process(es) to complete. */
		case 6002: /* SHUTDOWN is in progress. Log off. */
		case 6004: /* SHUTDOWN can only be used by members of the sysadmin role. */
		case 6005: /* SHUTDOWN is in progress. */
		case 6006: /* Server shut down by request. */
		case 8179: /* Could not find prepared statement with handle %d -- Force reconnect as we have no other means to handle this */
			return M_SQL_ERROR_CONN_LOST;
		case 1204: /* SQL Server has run out of LOCKS. Rerun your statement when there are fewer active users, or ask the system administrator to reconfigure SQL Server with more LOCKS. */
		case 1205: /* Your transaction (process ID #%d) was deadlocked with another process and has been chosen as the deadlock victim. Rerun your transaction. */
		case 1211: /* Process ID %d was chosen as the deadlock victim with P_BACKOUT bit set. */
		case 1222: /* Lock request time out period exceeded. */
			return M_SQL_ERROR_QUERY_DEADLOCK;
	}
	return M_SQL_ERROR_QUERY_FAILURE;
}


M_sql_error_t mssql_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t err;
	M_sql_stmt_t *stmt;

	(void)dpool;
	(void)is_first_in_pool;
	(void)is_readonly;

	/* We want ANSI mode, which makes the server act in a more standard way */
	stmt = M_sql_conn_execute_simple(conn, "SET ANSI_DEFAULTS ON", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET ANSI_DEFAULTS ON failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return M_SQL_ERROR_SUCCESS;
}


M_bool mssql_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len)
{
	(void)pool;

	if (max_len == 0) {
		max_len = SIZE_MAX;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			M_buf_add_str(buf, "TINYINT");
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT16:
			M_buf_add_str(buf, "SMALLINT");
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT32:
			M_buf_add_str(buf, "INTEGER");
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT64:
			M_buf_add_str(buf, "BIGINT");
			return M_TRUE;
		case M_SQL_DATA_TYPE_TEXT:
			if (max_len <= 8000) {
				M_buf_add_str(buf, "VARCHAR(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "VARCHAR(max)");
			}
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			if (max_len <= 8000) {
				M_buf_add_str(buf, "VARBINARY(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "VARBINARY(max)");
			}
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


void mssql_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type)
{
	(void)pool;
	M_sql_driver_append_updlock(M_SQL_DRIVER_UPDLOCK_CAP_MSSQL, query, type);
}


M_bool mssql_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_OP_CAST_BIGINT, query, op, exp1, exp2);
}
