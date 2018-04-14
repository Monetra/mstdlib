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
#include "mysql_shared.h"


M_sql_error_t mysql_resolve_error(const char *sqlstate, M_int32 errorcode)
{
	(void)sqlstate;

	/* https://dev.mysql.com/doc/refman/5.7/en/error-messages-client.html
	 * https://dev.mysql.com/doc/refman/5.5/en/error-messages-server.html */
	switch (errorcode) {
		case 2001: /* CR_SOCKET_CREATE_ERROR */
		case 2002: /* CR_CONNECTION_ERROR */
		case 2003: /* CR_CONN_HOST_ERROR */
		case 2005: /* CR_UNKNOWN_HOST */
		case 2007: /* CR_VERSION_ERROR */
		case 2012: /* CR_SERVER_HANDSHAKE_ERR */
		case 2026: /* CR_SSL_CONNECTION_ERROR */
			return M_SQL_ERROR_CONN_FAILED;

		case 1044: /* ER_DBACCESS_DENIED_ERROR */
		case 1045: /* ER_ACCESS_DENIED_ERROR */
			return M_SQL_ERROR_CONN_BADAUTH;

		case 2006: /* CR_SERVER_GONE_ERROR */
		case 2013: /* CR_SERVER_LOST */
		case 2055: /* CR_SERVER_LOST_EXTENDED */
		case 1053: /* ER_SERVER_SHUTDOWN */
		case 1077: /* ER_NORMAL_SHUTDOWN */
		case 1079: /* ER_SHUTDOWN_COMPLETE */
		case 1152: /* ER_ABORTING_CONNECTION */

		/* These events mean the node is non-primary, should try to reconnect to another host */
		case 1290: /* ERROR 1290 (HY000): The MySQL server is running with the --read-only option so it cannot execute this statement */
		case 1792: /* ERROR 1792 (HY000): Cannot execute statement in a READ ONLY transaction. */
		case 1047: /* WSREP has not yet prepared node for application use - Galera */
			return M_SQL_ERROR_CONN_LOST;

		case 1021: /* ER_DISK_FULL */
		case 1205: /* Lock wait timeout */
		case 1206: /* Lock table full */
		case 1213: /* Deadlock found */
		case 1317: /* query execution was interrupted -- triggered by Galera */
			return M_SQL_ERROR_QUERY_DEADLOCK;

		case 1022: /* ER_DUP_KEY */
		case 1062: /* ER_DUP_ENTRY */
		case 1169: /* ER_DUP_UNIQUE */
		case 1451: /* ER_ROW_IS_REFERENCED_2 - Cannot delete or update a parent row: a foreign key constraint fails (%s) */
		case 1452: /* ER_NO_REFERENCED_ROW_2 - Cannot add or update a child row: a foreign key constraint fails (%s) */
		case 1557: /* ER_FOREIGN_DUPLICATE_KEY */
			return M_SQL_ERROR_QUERY_CONSTRAINT;

		default:
			break;
	}

	return M_SQL_ERROR_QUERY_FAILURE;
}


M_sql_error_t mysql_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t           err   = M_SQL_ERROR_SUCCESS;
	M_sql_stmt_t           *stmt;

	(void)is_first_in_pool;  /* We do these always */
	(void)dpool;
	(void)is_readonly;

	/* Set default isolation mode */
	stmt = M_sql_conn_execute_simple(conn, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET ISOLATION READ COMMITTED failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	stmt = M_sql_stmt_create();
	M_sql_stmt_prepare(stmt, "SET SESSION sql_mode = ?");
	M_sql_stmt_bind_text_const(stmt, "ANSI", 0);
	M_sql_conn_execute(conn, stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET SESSION sql_mode = ANSI failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return err;
}


M_bool mysql_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len)
{
	(void)pool;

	if (max_len == 0) {
		max_len = SIZE_MAX;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			M_buf_add_str(buf, "TINYINT"); /* 8 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT16:
			M_buf_add_str(buf, "SMALLINT"); /* 16 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT32:
			M_buf_add_str(buf, "INTEGER"); /* 32 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT64:
			M_buf_add_str(buf, "BIGINT"); /* 64 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_TEXT:
			if (max_len < 16 * 1024) {
				M_buf_add_str(buf, "VARCHAR(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else if (max_len < (1 << 23)) {
				M_buf_add_str(buf, "MEDIUMTEXT");
			} else {
				M_buf_add_str(buf, "LONGTEXT");
			}
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			if (max_len < 16 * 1024) {
				/* Use VARBINARY with a length instead of TINYBLOB or BLOB
				 * as it is more likely to be stored inline in the row for
				 * small sizes */
				M_buf_add_str(buf, "VARBINARY(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else if (max_len < (1 << 15)) {
				M_buf_add_str(buf, "BLOB");
			} else if (max_len < (1 << 23)) {
				M_buf_add_str(buf, "MEDIUMBLOB");
			} else {
				M_buf_add_str(buf, "LONGBLOB");
			}
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


void mysql_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type)
{
	(void)pool;
	M_sql_driver_append_updlock(M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATE, query, type);
}


M_bool mysql_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_OP, query, op, exp1, exp2);
}


void mysql_createtable_suffix(M_sql_connpool_t *pool, M_hash_dict_t *settings, M_buf_t *query)
{
	const char *const_temp;
	(void) pool;

	M_buf_add_str(query, " ENGINE=");
	const_temp = M_hash_dict_get_direct(settings, "mysql_engine");
	if (M_str_isempty(const_temp))
		const_temp = "INNODB";
	M_buf_add_str(query, const_temp);

	M_buf_add_str(query, " CHARSET=");
	const_temp = M_hash_dict_get_direct(settings, "mysql_charset");
	if (M_str_isempty(const_temp))
		const_temp = "UTF8";
	M_buf_add_str(query, const_temp);
}
