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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "postgresql_shared.h"

M_sql_error_t pgsql_resolve_error(const char *sqlstate, M_int32 errorcode)
{
	const struct {
		const char   *state_prefix;  /*!< Prefix of sqlstate to match */
		M_sql_error_t err;           /*!< Mapped error code */
	} statemap[] = {
		{ "HYT00", M_SQL_ERROR_CONN_LOST        }, /*!< timeout on transaction */
		{ "HYT01", M_SQL_ERROR_CONN_LOST        }, /*!< timeout on connection */
		/* https://www.postgresql.org/docs/9.6/static/errcodes-appendix.html */
		{ "00",    M_SQL_ERROR_SUCCESS          }, /*!< Success */
		{ "08",    M_SQL_ERROR_CONN_LOST        }, /*!< Connection Exception */
		{ "23",    M_SQL_ERROR_QUERY_CONSTRAINT }, /*!< Integrity Constraint Violation */
		{ "40",    M_SQL_ERROR_QUERY_DEADLOCK   }, /*!< Transaction Rollback */
		{ "53100", M_SQL_ERROR_QUERY_DEADLOCK   }, /*!< Disk Full */
		{ "53",    M_SQL_ERROR_CONN_LOST        }, /*!< Other insufficient resources, disconnect */
		{ "57P",   M_SQL_ERROR_CONN_LOST        }, /*!< ADMIN shutdown or similar */
		{ NULL, M_SQL_ERROR_UNSET }
	};
	size_t i;

	(void)errorcode;

	for (i=0; statemap[i].state_prefix != NULL; i++) {
		size_t prefix_len = M_str_len(statemap[i].state_prefix);
		if (M_str_caseeq_max(statemap[i].state_prefix, sqlstate, prefix_len))
			return statemap[i].err;
	}

	/* Anything else is a generic query failure */
	return M_SQL_ERROR_QUERY_FAILURE;
}


M_sql_error_t pgsql_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t           err   = M_SQL_ERROR_SUCCESS;
	M_sql_stmt_t           *stmt;

	(void)is_first_in_pool;  /* We do these always */
	(void)dpool;
	(void)is_readonly;

	/* Set default isolation mode */
	stmt = M_sql_conn_execute_simple(conn, "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET ISOLATION READ COMMITTED failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return err;
}


M_bool pgsql_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len, M_bool is_cast)
{
	(void)pool;
	(void)is_cast;

	if (max_len == 0) {
		max_len = SIZE_MAX;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL: /* The boolean type in postgresql isn't considered an integer type, we require integer handling of booleans */
		case M_SQL_DATA_TYPE_INT16:
			M_buf_add_str(buf, "SMALLINT"); /* 16 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT32:
			M_buf_add_str(buf, "INTEGER");  /* 32 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT64:
			M_buf_add_str(buf, "BIGINT");   /* 64 bit */
			return M_TRUE;
		case M_SQL_DATA_TYPE_TEXT:
			if (max_len <= 64 * 1024) {
				M_buf_add_str(buf, "VARCHAR(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "TEXT");
			}
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			M_buf_add_str(buf, "BYTEA");
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


void pgsql_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name)
{
	(void)pool;
	M_sql_driver_append_updlock(M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATEOF, query, type, table_name);
}


M_bool pgsql_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_OP, query, op, exp1, exp2);
}

