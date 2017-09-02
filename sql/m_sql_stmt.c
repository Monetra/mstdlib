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


M_sql_stmt_t *M_sql_stmt_create(void)
{
	M_sql_stmt_t *stmt = M_malloc_zero(sizeof(*stmt));
	stmt->last_error   = M_SQL_ERROR_SUCCESS;
	return stmt;
}


void M_sql_stmt_destroy(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return;

	/* If group lock exists, it is reference counted */
	if (stmt->group_lock) {
		size_t cnt;
		M_thread_mutex_lock(stmt->group_lock);
		stmt->group_cnt--;
		cnt = stmt->group_cnt;
		M_thread_mutex_unlock(stmt->group_lock);
		if (cnt != 0)
			return;
	}

/* XXX: If stmt->last_error == M_SQL_ERROR_SUCCESS_ROW, cancel fetch! */
	M_sql_stmt_bind_clear(stmt);
	M_sql_stmt_result_clear(stmt);

	M_free(stmt->query_user);
	M_free(stmt->query_prepared);
	M_thread_mutex_destroy(stmt->group_lock);
	M_thread_cond_destroy(stmt->group_cond);
	M_free(stmt);
}


static M_sql_error_t M_sql_stmt_prepare_query(M_sql_stmt_t *stmt, const char *query, M_bool skip_sanity_checks)
{
	size_t len             = M_str_len(query);
	size_t i;
	size_t param_cnt       = 0;
	M_bool is_create_table = M_FALSE;

	if (stmt == NULL)
		return M_SQL_ERROR_INVALID_USE;

	if (len == 0) {
		M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "Blank query");
		return M_SQL_ERROR_PREPARE_INVALID;
	}

	/* Create table statements are allowed to have single quotes as it is necessary
	 * for things like default values */
	if (M_str_caseeq_max(query, "CREATE TABLE", 12))
		is_create_table = M_TRUE;

	for (i=0; i<len; i++) {
		switch(query[i]) {
			case '\'':
				if (!is_create_table && !skip_sanity_checks) {
					M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "pos %zu: quote found", i);
					return M_SQL_ERROR_PREPARE_STRNOTBOUND;
				}
				break;
			case ';':
				if (i != len-1 && !skip_sanity_checks) {
					M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "pos %zu: statement terminator found", i);
					return M_SQL_ERROR_PREPARE_NOMULITQUERY;
				}
				break;
			case '?':
				param_cnt++;
				break;
			default:
				if (!M_chr_isprint(query[i]) && !skip_sanity_checks) {
					M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "pos %zu: non-printable character", i);
					return M_SQL_ERROR_PREPARE_INVALID;
				}
				break;
		}
	}

	stmt->query_param_cnt = param_cnt;
	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t M_sql_stmt_prepare_int(M_sql_stmt_t *stmt, const char *query, M_bool skip_sanity_checks)
{
	if (stmt == NULL || M_str_isempty(query))
		return M_SQL_ERROR_INVALID_USE;

	/* If the group lock exists, invalid use, cannot re-prepare */
	if (stmt->group_lock)
		return M_SQL_ERROR_INVALID_USE;

	if (stmt->query_user != NULL)
		M_free(stmt->query_user);

	stmt->query_user = M_strdup_trim(query);
	stmt->last_error = M_sql_stmt_prepare_query(stmt, stmt->query_user, skip_sanity_checks);

	return stmt->last_error;
}


M_sql_error_t M_sql_stmt_prepare(M_sql_stmt_t *stmt, const char *query)
{
	return M_sql_stmt_prepare_int(stmt, query, M_FALSE /* Users calling this are never allowed to skip sanity checks */);
}


M_sql_error_t M_sql_stmt_prepare_buf(M_sql_stmt_t *stmt, M_buf_t *query)
{
	M_sql_error_t err;

	err = M_sql_stmt_prepare(stmt, M_buf_peek(query));
	M_buf_cancel(query);

	return err;
}


static M_sql_error_t M_sql_stmt_fetch_int(M_sql_stmt_t *stmt, M_bool is_execute_fetchall)
{
	M_sql_conn_t         *conn;
	const M_sql_driver_t *driver;

	if (stmt == NULL || stmt->conn == NULL || stmt->dstmt == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	conn   = stmt->conn;
	driver = M_sql_conn_get_driver(conn);

	/* Clear prior results if a user is calling fetch */
	if (!is_execute_fetchall)
		M_sql_stmt_result_clear_data(stmt);

	if (stmt->last_error != M_SQL_ERROR_SUCCESS_ROW) {
		return stmt->last_error;
	}

	if (stmt->result == NULL || stmt->result->total_rows == 0) {
		M_sql_trace_message_stmt(M_SQL_TRACE_FETCH_START, stmt);
	}

	do {
		stmt->last_error = driver->cb_fetch(conn, stmt, stmt->error_msg, sizeof(stmt->error_msg));
	} while (stmt->last_error == M_SQL_ERROR_SUCCESS_ROW && (is_execute_fetchall || M_sql_stmt_result_num_rows(stmt) < stmt->max_fetch_rows));

	/* Don't clean up handles if more rows are left to be fetched! */
	if (stmt->last_error == M_SQL_ERROR_SUCCESS_ROW)
		return M_SQL_ERROR_SUCCESS_ROW;

	/* Done retrieving rows, success or error */
	M_sql_trace_message_stmt(M_SQL_TRACE_FETCH_FINISH, stmt);

	/* If the driver returned more rows when they returned M_SQL_ERROR_SUCESS, rewrite the
	 * condition to M_SQL_ERROR_SUCCESS_ROW instead of returning the real result. */
	if (!is_execute_fetchall && stmt->last_error == M_SQL_ERROR_SUCCESS && M_sql_stmt_result_num_rows(stmt))
		return M_SQL_ERROR_SUCCESS_ROW;

	return stmt->last_error;
}


/*! Perform actual execution in a loop if there are multiple bound rows while
 *  the prior grouping was successful and there are more rows remaining
 */
static M_sql_error_t M_sql_conn_execute_rows(M_sql_conn_t *conn, M_sql_stmt_t *stmt)
{
	size_t                rows_executed; /*! For multiple-insert queries, this may be a value > 1 */
	const M_sql_driver_t *driver = M_sql_conn_get_driver(conn);
	M_sql_error_t         err    = M_SQL_ERROR_SUCCESS;

	/* Make sure we start at offset 0 */
	stmt->bind_row_offset = 0;

	/* Number of rows might be 0 if there are no bound parameters, so we want
	 * to account for this possibility by making it a do { } while */
	do {
		/* Call query format callback (clear existing format *first* as it may be invalid) */
		M_free(stmt->query_prepared);
		stmt->query_prepared = driver->cb_queryformat(conn, stmt->query_user, stmt->query_param_cnt, M_sql_driver_stmt_bind_rows(stmt), stmt->error_msg, sizeof(stmt->error_msg));
		if (stmt->query_prepared == NULL) {
			err = M_SQL_ERROR_QUERY_PREPARE;
			goto done;
		}

		/* Lookup existing driver statement handle for formatted query from connection */
		stmt->dstmt = M_sql_conn_get_stmt_cache(conn, stmt->query_prepared);

		/* Call the sql driver's prepare callback, passing in existing statement handle if any.
		 * Add returned handle to cache (even if NULL, which will clear the cached handle). */
		err = driver->cb_prepare(&stmt->dstmt, conn, stmt, stmt->error_msg, sizeof(stmt->error_msg));
		M_sql_conn_set_stmt_cache(conn, stmt->query_prepared, stmt->dstmt);

		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		/* Execute the query */
		rows_executed = 1; /* Assume, driver may update */
		err = driver->cb_execute(conn, stmt, &rows_executed, stmt->error_msg, sizeof(stmt->error_msg));
		if (err != M_SQL_ERROR_SUCCESS && err != M_SQL_ERROR_SUCCESS_ROW) {
			/* If there is a generic failure, invalidate the prepared statement handle as it could
			 * be invalid to reuse */
			if (err == M_SQL_ERROR_QUERY_FAILURE) {
				M_sql_conn_set_stmt_cache(conn, stmt->query_prepared, NULL);
			}
			goto done;
		}

		/* Update offset */
		stmt->bind_row_offset += rows_executed;

	} while (M_sql_driver_stmt_bind_rows(stmt));

done:
	return err;
}


M_sql_error_t M_sql_conn_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt)
{
	M_sql_error_t         err = M_SQL_ERROR_SUCCESS;

	if (conn == NULL || stmt == NULL) {
		return M_SQL_ERROR_INVALID_USE;
	}

	/* Cache connection handle, mostly for M_sql_stmt_fetch() */
	stmt->conn       = conn;
	stmt->last_error = M_SQL_ERROR_SUCCESS;
	M_mem_set(stmt->error_msg, 0, sizeof(stmt->error_msg));

	/* Start timer so we know how long it is taking */
	M_time_elapsed_start(&stmt->start_tv);

	/* If the last bound row is blank, user probably called M_sql_stmt_bind_new_row() at the end of a loop and
	 * didn't mean to, lets auto-fix this situation */
	if (stmt->bind_row_cnt > 0 && stmt->bind_rows[stmt->bind_row_cnt-1].col_cnt == 0) {
		stmt->bind_row_cnt--;
	}

	M_sql_trace_message_stmt(M_SQL_TRACE_EXECUTE_START, stmt);

	if (M_str_isempty(stmt->query_user)) {
		M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "Query not prepared");
		err = M_SQL_ERROR_QUERY_NOTPREPARED;
		goto done;
	}

	/* If no parameters bound, but expected some, error out */
	if (stmt->query_param_cnt && stmt->bind_row_cnt == 0) {
		M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "No parameters bound, expected %zu", stmt->query_param_cnt);
		err = M_SQL_ERROR_QUERY_WRONGNUMPARAMS;
		goto done;
	}

	/* If parameters bound, but doesn't match the expected count, error out */
	if (stmt->bind_row_cnt != 0 && stmt->query_param_cnt != stmt->bind_rows[0].col_cnt) {
		M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "Expected %zu params, have %zu", stmt->query_param_cnt, stmt->bind_rows[0].col_cnt);
		err = M_SQL_ERROR_QUERY_WRONGNUMPARAMS;
		goto done;
	}

	/* Validate all rows have the same count of parameters and that they don't have different types */
	if (stmt->bind_row_cnt > 1) {
		size_t i;
		for (i=1; i<stmt->bind_row_cnt; i++) {
			if (stmt->bind_rows[i].col_cnt != stmt->bind_rows[0].col_cnt) {
				M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "Row %zu has %zu params, expected %zu", i, stmt->bind_rows[i].col_cnt, stmt->bind_rows[0].col_cnt);
				err = M_SQL_ERROR_QUERY_WRONGNUMPARAMS;
				goto done;
			}
		}
		for (i=0; i<stmt->bind_rows[0].col_cnt; i++) {
			M_sql_data_type_t type = M_SQL_DATA_TYPE_NULL;
			size_t            j;

			for (j=0; j<stmt->bind_row_cnt; j++) {
				M_sql_data_type_t mytype = stmt->bind_rows[j].cols[i].type;
				if (mytype != M_SQL_DATA_TYPE_NULL) {
					if (type != M_SQL_DATA_TYPE_NULL && mytype != type) {
						M_snprintf(stmt->error_msg, sizeof(stmt->error_msg), "Row %zu column %zu has type %u, expected %u", j, i, mytype, type);
						err = M_SQL_ERROR_PREPARE_INVALID;
						goto done;
					}
					type = mytype; /* Cache for future checks */
				}
			}
		}
	}

	/* Clear any existing results */
	M_sql_stmt_result_clear(stmt);

	/* Make sure if there are rows of bound paramters, and the SQL server can't handle
	 * all rows in one execution that they are executed back to back until complete or
	 * error. */
	err = M_sql_conn_execute_rows(conn, stmt);
	if (err != M_SQL_ERROR_SUCCESS && err != M_SQL_ERROR_SUCCESS_ROW)
		goto done;

	/* Mark time before rows are fetched */
	M_time_elapsed_start(&stmt->last_tv);

done:
	stmt->last_error = err;

	M_sql_trace_message_stmt(M_SQL_TRACE_EXECUTE_FINISH, stmt);

	/* Only prefetch rows if max_fetch_rows is 0 */
	if (err == M_SQL_ERROR_SUCCESS_ROW && stmt->max_fetch_rows == 0) {
		err = M_sql_stmt_fetch_int(stmt, M_TRUE);
	}

	/* NOTE: since this can be called from within the driver, we aren't going to
	 *       update tracking here.  We'll delegate that to whoever calls us
	 */

	/* If there's still rows to be fetched, don't release the statement or connection handles */
	if (!M_sql_stmt_has_remaining_rows(stmt)) {
		stmt->dstmt = NULL;
		stmt->conn  = NULL;
		stmt->trans = NULL;
	}

	return err;
}


M_sql_stmt_t *M_sql_conn_execute_simple(M_sql_conn_t *conn, const char *query, M_bool skip_sanity_checks)
{
	M_sql_stmt_t *stmt = M_sql_stmt_create();
	M_sql_error_t err;

	err = M_sql_stmt_prepare_int(stmt, query, skip_sanity_checks);
	if (err != M_SQL_ERROR_SUCCESS) {
		goto fail;
	}

	err = M_sql_conn_execute(conn, stmt);
	if (err != M_SQL_ERROR_SUCCESS) {
		goto fail;
	}

fail:
	return stmt;
}



M_sql_error_t M_sql_stmt_execute(M_sql_connpool_t *pool, M_sql_stmt_t *stmt)
{
	M_sql_conn_t  *conn        = NULL;
	M_sql_error_t  err         = M_SQL_ERROR_SUCCESS;
	M_sql_trans_t *trans       = NULL;
	M_bool         is_readonly = M_str_caseeq_max(stmt->query_user, "SELECT", 6) && !stmt->master_only;
	M_bool         rollback    = M_FALSE;


	/* If doing a group insert, handle this scenario */
	if (stmt->group_lock) {
		/* Caught attempted re-run of stmt */
		if (stmt->group_state != M_SQL_GROUPINSERT_NEW)
			return M_SQL_ERROR_INVALID_USE;

		/* If the group_cnt is not 1 when we get here, we're a subsequent
		 * thread, so we just need to wait on a signal from a conditional
		 * and return the result. */
		if (stmt->group_cnt != 1) {
			/* Loop in case of spurious wake-up */
			while (stmt->group_state != M_SQL_GROUPINSERT_FINISHED) {
				M_thread_cond_wait(stmt->group_cond, stmt->group_lock);
			}

			/* No need to continue holding a lock */
			M_thread_mutex_unlock(stmt->group_lock);
			return M_sql_stmt_get_error(stmt);
		}

		/* We must be the master, so we are going to temporarily release our
		 * hold on the mutex and yeild to potentially allow others to add
		 * additional rows onto our statement handle before even attempting
		 * to pull a connection handle */
		M_thread_mutex_unlock(stmt->group_lock);
		M_thread_yield(M_TRUE); /* Forcibly yield */
	}


	do {
		if (rollback) {
			M_thread_sleep(M_sql_rollback_delay_ms(pool) * 1000);
			rollback = M_FALSE;
		}

		/* Either begin transaction or acquire connection */
		if (M_sql_driver_stmt_bind_rows(stmt) > 1 || stmt->group_lock) {
			err = M_sql_trans_begin(&trans, pool, M_SQL_ISOLATION_READCOMMITTED, stmt->error_msg, sizeof(stmt->error_msg));
			if (M_sql_error_is_error(err)) {
				if (M_sql_error_is_rollback(err)) {
					M_sql_trans_rollback(trans); /* Ignore error, what else could we do? */
					rollback = M_TRUE;
					continue;
				}
				/* Must be a fatal error - but if begin failed, no need to rollback */
				goto done;
			}

			/* We have acquired a connection, time to close off the ability to
			 * add more rows */
			if (stmt->group_lock && stmt->group_state == M_SQL_GROUPINSERT_NEW) {
				M_thread_mutex_lock(stmt->group_lock);
				M_sql_connpool_remove_groupinsert(pool, stmt->query_user, stmt);
				stmt->group_state = M_SQL_GROUPINSERT_PENDING;
				/* Statement handle is still locked */
			}

		} else {
			conn = M_sql_connpool_acquire_conn(pool, is_readonly, M_FALSE /* Not in a transaction */);
			if (conn == NULL) {
				err = M_SQL_ERROR_INVALID_USE;
				goto done;
			}
		}

		/* Make sure we start at offset when making decisions */
		stmt->bind_row_offset = 0;

		if (!trans) {
			err = M_sql_conn_execute(conn, stmt);

			/* Catch a connectivity or rollback error */
			M_sql_conn_set_state_from_error(conn, err);

			if (M_sql_error_is_error(err)) {
				if (M_sql_error_is_rollback(err) &&
				    !(M_sql_connpool_flags(pool) & M_SQL_CONNPOOL_FLAG_NO_AUTORETRY_QUERY)) {
					rollback = M_TRUE;
					M_sql_connpool_release_conn(conn);
					conn = NULL;
				}
			}
			/* Will break if !rollback */
			continue;
		}

		/* In transaction, execute */
		err = M_sql_trans_execute(trans, stmt);
		if (M_sql_error_is_error(err)) {
			M_sql_trans_rollback(trans); /* Ignore error, what else could we do? */

			if (M_sql_error_is_rollback(err)) {
				rollback = M_TRUE;
				continue;
			}

			/* Must be a fatal error, exit (is rolled back) */
			goto done;
		}

		/* Commit the transaction */
		err = M_sql_trans_commit(trans, stmt->error_msg, sizeof(stmt->error_msg));
		if (M_sql_error_is_error(err)) {
			if (M_sql_error_is_rollback(err)) {
				/* Don't try to actually execute the rollback since commit guarantees if it fails it is rolled back */
				rollback = M_TRUE;
				continue;
			}

			/* Must be a fatal error, and commit guarantees it is rolled back already */
			goto done;
		}

	} while (rollback);

done:
	if (conn) {
		/* Release connection if we're not going to fetch */
		if (!M_sql_stmt_has_remaining_rows(stmt)) {
			M_sql_connpool_release_conn(conn);
		}
	}

	/* Group insert, time to let the other waiters know they can process the
	 * result */
	if (stmt->group_lock) {
		/* We're still holding a lock */
		stmt->group_state = M_SQL_GROUPINSERT_FINISHED;
		M_thread_cond_broadcast(stmt->group_cond);
		M_thread_mutex_unlock(stmt->group_lock);
	}

	return err;
}


M_bool M_sql_stmt_set_max_fetch_rows(M_sql_stmt_t *stmt, size_t num)
{
	if (stmt == NULL)
		return M_FALSE;
	stmt->max_fetch_rows = num;
	return M_TRUE;
}


M_bool M_sql_stmt_set_master_only(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return M_FALSE;
	stmt->master_only = M_TRUE;
	return M_TRUE;
}


M_bool M_sql_stmt_has_remaining_rows(M_sql_stmt_t *stmt)
{
	if (stmt && stmt->last_error == M_SQL_ERROR_SUCCESS_ROW)
		return M_TRUE;
	return M_FALSE;
}


M_sql_error_t M_sql_stmt_fetch(M_sql_stmt_t *stmt)
{
	M_sql_error_t err    = stmt->last_error;
	M_bool        append = M_FALSE;

	err = M_sql_stmt_fetch_int(stmt, append);

	/* Catch a connectivity or rollback error */
	M_sql_conn_set_state_from_error(stmt->conn, err);

	if (err != M_SQL_ERROR_SUCCESS_ROW) {
		M_sql_conn_t *conn = stmt->conn;
		stmt->conn         = NULL;
		stmt->dstmt        = NULL;

		/* If the statement is not part of a transaction, and also not part of an execute
		 * that is fetching all rows, release the connection object since we're done */
		if (!stmt->trans) {
			M_sql_connpool_release_conn(conn);
		} else {
			stmt->trans = NULL;
		}
	}

	return err;
}



M_sql_error_t M_sql_stmt_get_error(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return M_SQL_ERROR_INVALID_USE;
	return stmt->last_error;
}


const char *M_sql_stmt_get_error_string(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return NULL;
	return stmt->error_msg;
}

M_uint64 M_sql_stmt_duration_start_ms(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return 0;

	return M_time_elapsed(&stmt->start_tv);
}


M_uint64 M_sql_stmt_duration_last_ms(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return 0;

	return M_time_elapsed(&stmt->last_tv);
}

M_sql_trans_t *M_sql_stmt_get_trans(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return NULL;
	return stmt->trans;
}


M_sql_conn_t *M_sql_stmt_get_conn(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return NULL;
	return stmt->conn;
}


M_sql_stmt_t *M_sql_stmt_groupinsert_prepare(M_sql_connpool_t *pool, const char *query)
{
	M_sql_stmt_t *stmt;

	if (pool == NULL || M_str_isempty(query))
		return NULL;

	stmt = M_sql_connpool_get_groupinsert(pool, query);

	/* If no statement was found, pool handle is returned locked, so we need
	 * to quickly create a new statement handle and insert it */
	if (stmt == NULL) {
		stmt             = M_sql_stmt_create();
		M_sql_stmt_prepare(stmt, query);

		/* Must be initialized after calling prepare() */
		stmt->group_lock  = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
		stmt->group_cnt   = 0;
		stmt->group_state = M_SQL_GROUPINSERT_NEW;
		stmt->group_cond  = M_thread_cond_create(M_THREAD_CONDATTR_NONE);

		/* Docs say stmt handle should be locked on entry */
		M_thread_mutex_lock(stmt->group_lock);

		/* This will release the pool lock on exit */
		M_sql_connpool_set_groupinsert(pool, query, stmt);
	}

	/* Statement handle is locked here, either by M_sql_connpool_get_groupinsert()
	 * or during creation.  Lets make sure we add a new bind row and bump up our
	 * reference count.  We even call the new row even if this is brand new as
	 * someone might have gained the lock before M_sql_connpool_set_groupinsert()
	 * returns and have added a row */
	stmt->group_cnt++;
	M_sql_stmt_bind_new_row(stmt);

	return stmt;
}


M_sql_stmt_t *M_sql_stmt_groupinsert_prepare_buf(M_sql_connpool_t *pool, M_buf_t *query)
{
	M_sql_stmt_t *stmt = M_sql_stmt_groupinsert_prepare(pool, M_buf_peek(query));
	if (stmt != NULL)
		M_buf_cancel(query);
	return stmt;
}
