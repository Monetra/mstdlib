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
#include "base/m_defs_int.h"

const char *M_sql_error_string(M_sql_error_t err)
{
	switch (err) {
		case M_SQL_ERROR_SUCCESS:
			return "Success";
		case M_SQL_ERROR_SUCCESS_ROW:
			return "Success, rows may be available to fetch";
		case M_SQL_ERROR_CONN_NODRIVER:
			return "Driver not found for specified driver name";
		case M_SQL_ERROR_CONN_DRIVERLOAD:
			return "Failed to dynamically load driver module";
		case M_SQL_ERROR_CONN_DRIVERVER:
			return "Driver version invalid";
		case M_SQL_ERROR_CONN_PARAMS:
			return "Driver connection string parameter validation failed";
		case M_SQL_ERROR_CONN_FAILED:
			return "Failed to establish connection to server";
		case M_SQL_ERROR_CONN_BADAUTH:
			return "Failed to authenticate against server";
		case M_SQL_ERROR_CONN_LOST:
			return "Connection to server has been lost";
		case M_SQL_ERROR_PREPARE_INVALID:
			return "Invalid query format";
		case M_SQL_ERROR_PREPARE_STRNOTBOUND:
			return "A string was detected in the query, all strings must be bound";
		case M_SQL_ERROR_PREPARE_NOMULITQUERY:
			return "Multiple requests in a single query are not allowed";
		case M_SQL_ERROR_QUERY_NOTPREPARED:
			return "Can't execute query as statement hasn't been prepared";
		case M_SQL_ERROR_QUERY_WRONGNUMPARAMS:
			return "Wrong number of bound parameters provided for query";
		case M_SQL_ERROR_QUERY_PREPARE:
			return "DB Driver failed to prepare the query for execution";
		case M_SQL_ERROR_QUERY_DEADLOCK:
			return "Deadlock";
		case M_SQL_ERROR_QUERY_CONSTRAINT:
			return "Constraint failed";
		case M_SQL_ERROR_QUERY_FAILURE:
			return "Failure (uncategorized)";
		case M_SQL_ERROR_USER_SUCCESS:
			return "Success - User Notification";
		case M_SQL_ERROR_USER_RETRY:
			return "Retry - User Notification";
		case M_SQL_ERROR_USER_FAILURE:
			return "Failure - User Notification";
		case M_SQL_ERROR_INUSE:
			return "Resource in use, invalid action";
		case M_SQL_ERROR_INVALID_USE:
			return "Invalid use";
		case M_SQL_ERROR_INVALID_TYPE:
			return "Invalid Data Type Conversion";
		case M_SQL_ERROR_UNSET:
			return "UNSET. INTERNAL ONLY.";
	}
	return "UNKNOWN";
}


M_bool M_sql_error_is_error(M_sql_error_t err)
{
	if (err != M_SQL_ERROR_SUCCESS && err != M_SQL_ERROR_SUCCESS_ROW && err != M_SQL_ERROR_USER_SUCCESS)
		return M_TRUE;
	return M_FALSE;
}


M_bool M_sql_error_is_rollback(M_sql_error_t err)
{
	if (err == M_SQL_ERROR_QUERY_DEADLOCK || err == M_SQL_ERROR_USER_RETRY)
		return M_TRUE;
	return M_FALSE;
}


M_bool M_sql_error_is_fatal(M_sql_error_t err)
{
	return (M_sql_error_is_error(err) && !M_sql_error_is_rollback(err) && !M_sql_error_is_disconnect(err));
}


M_bool M_sql_error_is_disconnect(M_sql_error_t err)
{
	switch (err) {
		case M_SQL_ERROR_CONN_NODRIVER:
		case M_SQL_ERROR_CONN_DRIVERLOAD:
		case M_SQL_ERROR_CONN_DRIVERVER:
		case M_SQL_ERROR_CONN_PARAMS:
		case M_SQL_ERROR_CONN_FAILED:
		case M_SQL_ERROR_CONN_BADAUTH:
		case M_SQL_ERROR_CONN_LOST:
			return M_TRUE;
		default:
			break;
	}
	return M_FALSE;
}

