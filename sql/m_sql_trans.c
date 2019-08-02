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
#include <mstdlib/mstdlib_sql.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"

struct M_sql_trans {
	M_timeval_t   start_tv;
	M_timeval_t   last_tv;
	M_sql_conn_t *conn;
	char          error[512];
};


static void M_sql_trans_release(M_sql_trans_t *trans)
{
	if (trans == NULL)
		return;

	if (trans->conn)
		M_sql_connpool_release_conn(trans->conn);

	M_free(trans);
}


M_sql_error_t M_sql_trans_begin(M_sql_trans_t **trans, M_sql_connpool_t *pool, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_error_t         err;
	const M_sql_driver_t *driver;
	M_sql_conn_t         *conn;
	char                  myerror[512];
	M_bool                rollback = M_FALSE;

	/* We need a real error buffer */
	if (error == NULL || error_size == 0) {
		error      = myerror;
		error_size = sizeof(myerror);
	}

	if (trans == NULL || pool == NULL) {
		M_snprintf(error, error_size, "bad pointers passed in");
		return M_SQL_ERROR_INVALID_USE;
	}

	do {
		if (rollback) {
			M_thread_sleep(M_sql_rollback_delay_ms(pool) * 1000);
			rollback = M_FALSE;
		}

		conn           = M_sql_connpool_acquire_conn(pool, M_FALSE /* Transactions are never going to use the readonly pool */, M_TRUE /* Uhh yeah, in a transaction */);
		if (conn == NULL) {
			M_snprintf(error, error_size, "pool not started");
			return M_SQL_ERROR_INVALID_USE;
		}

		*trans         = M_malloc_zero(sizeof(**trans));
		(*trans)->conn = conn;
		driver         = M_sql_conn_get_driver((*trans)->conn);

		M_time_elapsed_start(&(*trans)->start_tv);
		M_time_elapsed_start(&(*trans)->last_tv);

		M_sql_trace_message_trans(M_SQL_TRACE_BEGIN_START, *trans, M_SQL_ERROR_SUCCESS, NULL);

		err            = driver->cb_begin((*trans)->conn, isolation, error, error_size);

		M_sql_trace_message_trans(M_SQL_TRACE_BEGIN_FINISH, *trans, err, error);

		/* Catch a connectivity or rollback error */
		M_sql_conn_set_state_from_error((*trans)->conn, err);

		if (err != M_SQL_ERROR_SUCCESS) {
			M_sql_trans_release(*trans);
			*trans = NULL;
		}

		/* Check for automatic retry */
		if (M_sql_error_is_error(err)) {
			if (!M_sql_error_is_fatal(err) &&
			    !(M_sql_connpool_flags(pool) & M_SQL_CONNPOOL_FLAG_NO_AUTORETRY_QUERY)) {
				rollback = M_TRUE;
			}
		}
	} while(rollback);

	return err;
}

/* XXX: protect against statement handle still pending (for fetching rows) */

M_sql_error_t M_sql_trans_rollback(M_sql_trans_t *trans)
{
	M_sql_error_t         err;
	const M_sql_driver_t *driver;

	if (trans == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	M_time_elapsed_start(&trans->last_tv);

	M_sql_trace_message_trans(M_SQL_TRACE_ROLLBACK_START, trans, M_SQL_ERROR_SUCCESS, NULL);

	/* No reason to ask the driver to rollback if the connection is down and
	 * will be destroyed when released */
	if (M_sql_conn_get_state(trans->conn) != M_SQL_CONN_STATE_FAILED) {
		driver = M_sql_conn_get_driver(trans->conn);
		err    = driver->cb_rollback(trans->conn);

		/* Catch a connectivity error */
		M_sql_conn_set_state_from_error(trans->conn, err);
	} else {
		err = M_SQL_ERROR_CONN_FAILED;
		/* XXX: Get error message from connection ? */
	}

	M_sql_trace_message_trans(M_SQL_TRACE_ROLLBACK_FINISH, trans, err, NULL);

	/* Regardless of the error, we still release the connection and free trans */
	M_sql_trans_release(trans);
	return err;
}


M_sql_error_t M_sql_trans_commit(M_sql_trans_t *trans, char *error, size_t error_size)
{
	M_sql_error_t         err;
	const M_sql_driver_t *driver;
	char                  myerror[512];

	/* Trace needs a real error buffer */
	if (error == NULL || error_size == 0) {
		error      = myerror;
		error_size = sizeof(myerror);
	}

	if (trans == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	M_time_elapsed_start(&trans->last_tv);

	/* Prevent attempting a commit if we must roll back, issue a rollback instead */
	if (M_sql_conn_get_state(trans->conn) != M_SQL_CONN_STATE_OK) {
		M_snprintf(error, error_size, "forced rollback");
		M_sql_trans_rollback(trans);
		if (M_sql_conn_get_state(trans->conn) == M_SQL_CONN_STATE_FAILED) {
			err = M_SQL_ERROR_CONN_FAILED;
		} else {
			err = M_SQL_ERROR_QUERY_DEADLOCK;
		}
		return err;
	}

	M_sql_trace_message_trans(M_SQL_TRACE_COMMIT_START, trans, M_SQL_ERROR_SUCCESS, NULL);

	driver = M_sql_conn_get_driver(trans->conn);
	err    = driver->cb_commit(trans->conn, error, error_size);

	M_sql_trace_message_trans(M_SQL_TRACE_COMMIT_FINISH, trans, err, error);

	/* Catch a commit/connectivity error */
	M_sql_conn_set_state_from_error(trans->conn, err);

	/* Regardless of the error, we still release the connection and free trans */
	M_sql_trans_release(trans);
	return err;
}


M_sql_error_t M_sql_trans_execute(M_sql_trans_t *trans, M_sql_stmt_t *stmt)
{
	M_sql_error_t err;

	if (trans == NULL || stmt == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	if (M_sql_conn_get_state(trans->conn) != M_SQL_CONN_STATE_OK) {
		M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "rollback required");
		stmt->last_error = M_SQL_ERROR_QUERY_DEADLOCK;
		return stmt->last_error;
	}

	M_time_elapsed_start(&trans->last_tv);

	err = M_sql_conn_execute(trans->conn, stmt);

	/* Capture error message to transaction handle so we can augment errors in
	 * M_sql_trans_process() automatically */
	if (M_sql_error_is_error(err)) {
		M_str_cpy(trans->error, sizeof(trans->error), M_sql_stmt_get_error_string(stmt));
	}

	/* Catch a connectivity or rollback error */
	M_sql_conn_set_state_from_error(trans->conn, err);

	return err;
}


M_uint64 M_sql_trans_duration_start_ms(M_sql_trans_t *trans)
{
	if (trans == NULL)
		return 0;

	return M_time_elapsed(&trans->start_tv);
}


M_uint64 M_sql_trans_duration_last_ms(M_sql_trans_t *trans)
{
	if (trans == NULL)
		return 0;

	return M_time_elapsed(&trans->last_tv);
}

M_sql_conn_t *M_sql_trans_get_conn(M_sql_trans_t *trans)
{
	if (trans == NULL)
		return NULL;

	return trans->conn;
}

M_sql_error_t M_sql_trans_process(M_sql_connpool_t *pool, M_sql_isolation_t isolation, M_sql_trans_commands_t cmd, void *cmd_arg, char *error, size_t error_size)
{
	M_sql_trans_t *trans    = NULL;
	M_bool         rollback = M_FALSE;
	M_sql_error_t  err;
	M_sql_error_t  cmd_err;

	if (pool == NULL || cmd == NULL) {
		M_snprintf(error, error_size, "missing pool or cmd");
		return M_SQL_ERROR_INVALID_USE;
	}

	do {
		if (rollback) {
			M_thread_sleep(M_sql_rollback_delay_ms(pool) * 1000);
			rollback = M_FALSE;
		}

		/* Clear error string in case one was already set */
		M_mem_set(error, 0, error_size);

		/* Begin a new transaction */
		err = M_sql_trans_begin(&trans, pool, isolation, error, error_size);
		if (M_sql_error_is_error(err)) {
			if (M_sql_error_is_rollback(err)) {
				M_sql_trans_rollback(trans); /* Ignore error, what else could we do? */
				rollback = M_TRUE;
				continue;
			}
			/* Must be a fatal error - but if begin failed, no need to rollback */
			return err;
		}

		/* Execute the user-command */
		err     = cmd(trans, cmd_arg, error, error_size);
		cmd_err = err; /* to preserve M_SQL_ERROR_USER_SUCCESS */
		if (M_sql_error_is_error(err)) {
			/* If an error occurred, but the error buffer is not filled in, attempt to
			 * pull the last error message from the 'trans' error buffer instead */
			if (error != NULL && M_str_isempty(error) && !M_str_isempty(trans->error)) {
				M_str_cpy(error, error_size, trans->error);
			}

			M_sql_trans_rollback(trans); /* Ignore error, what else could we do? */

			if (M_sql_error_is_rollback(err)) {
				rollback = M_TRUE;
				continue;
			}

			/* Must be a fatal error, exit (is rolled back) */
			return err;
		}

		/* Check for usage error */
		if (err == M_SQL_ERROR_SUCCESS_ROW) {
			M_snprintf(error, error_size, "M_sql_trans_commands_t user function returned SUCCESS_ROW, must not leave unprocessed results.");
			M_sql_trans_rollback(trans); /* Ignore error, what else could we do? */
			return M_SQL_ERROR_QUERY_FAILURE;
		}

		/* Commit the transaction */
		err = M_sql_trans_commit(trans, error, error_size);
		if (M_sql_error_is_error(err)) {
			if (M_sql_error_is_rollback(err)) {
				/* Don't try to actually execute the rollback since commit guarantees if it fails it is rolled back */
				rollback = M_TRUE;
				continue;
			}

			/* Must be a fatal error, and commit guarantees it is rolled back already */
			return err;
		} else {
			err = cmd_err; /* Restore return value from command executed, we don't want to possibly override a
			                * M_SQL_ERROR_USER_SUCCESS with M_SQL_ERROR_SUCCESS */
		}

	} while (rollback);

	return err;
}

M_sql_connpool_t *M_sql_trans_get_pool(M_sql_trans_t *trans)
{
	if (trans == NULL)
		return NULL;
	return M_sql_driver_conn_get_pool(trans->conn);
}

