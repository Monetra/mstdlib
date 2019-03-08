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
#include "odbc_db2.h"

M_sql_error_t db2_resolve_error(const char *sqlstate, M_int32 errorcode)
{
	const struct {
		const char   *state_prefix;  /*!< Prefix of sqlstate to match */
		M_sql_error_t err;           /*!< Mapped error code */
	} statemap[] = {
		/* https://www.ibm.com/support/knowledgecenter/SSEPEK_10.0.0/codes/src/tpc/db2z_sqlstatevalues.html */
		{ "00",    M_SQL_ERROR_SUCCESS          }, /*!< Success */
		{ "08",    M_SQL_ERROR_CONN_LOST        }, /*!< Connection Exception */
		{ "23",    M_SQL_ERROR_QUERY_CONSTRAINT }, /*!< Integrity Constraint Violation */
		{ "40",    M_SQL_ERROR_QUERY_DEADLOCK   }, /*!< Transaction Rollback */
		{ "42505", M_SQL_ERROR_CONN_BADAUTH     }, /*!< Connection authorization failure occurred. */
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


M_bool db2_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len)
{
	(void)pool;

	if (max_len == 0) {
		max_len = SIZE_MAX;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
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
			if (max_len <= 16 * 1024) {
				M_buf_add_str(buf, "VARCHAR(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "CLOB");
			}
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			if (max_len <= 16 * 1024) {
				M_buf_add_str(buf, "VARBINARY(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "BLOB");
			}
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


void db2_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name)
{
	(void)pool;
	M_sql_driver_append_updlock(M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATE, query, type, table_name);
}


M_bool db2_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_FUNC, query, op, exp1, exp2);
}
