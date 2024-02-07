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

#ifndef __M_SQL_TRANS_H__
#define __M_SQL_TRANS_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/sql/m_sql.h>
#include <mstdlib/sql/m_sql_stmt.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS



/*! \addtogroup m_sql_trans SQL Transaction Handling
 *  \ingroup m_sql
 *
 * SQL Transaction Handling
 *
 * @{
 */

struct M_sql_trans;
/*! Object holding the state for an active transaction */
typedef struct M_sql_trans M_sql_trans_t;

/*! Transaction isolation levels */
typedef enum {
    M_SQL_ISOLATION_UNKNOWN         = 0, /*!< Unknown, used for error conditions, never set */
    M_SQL_ISOLATION_READUNCOMMITTED = 1, /*!< Read Uncommitted */
    M_SQL_ISOLATION_READCOMMITTED   = 2, /*!< Read Committed */
    M_SQL_ISOLATION_REPEATABLEREAD  = 3, /*!< Repeatable Read */
    M_SQL_ISOLATION_SNAPSHOT        = 4, /*!< Snapshot */
    M_SQL_ISOLATION_SERIALIZABLE    = 5  /*!< Serializable */
} M_sql_isolation_t;


/*! Begin a new SQL transaction at the requested isolation level.
 *
 *  Beginning a new transaction will reserve an SQL connection from the pool
 *  until either a rollback or commit is performed.  Callers in most cases
 *  should not start more than one SQL transaction per thread as it could lead
 *  to deadlocks waiting on a connection to become available if insufficient
 *  connections are available in the pool.
 *
 *  In order to clean up the returned transaction handle, a caller must call
 *  either M_sql_trans_commit() or M_sql_trans_rollback() as appropriate.
 *
 *  \note It is recommended to use the M_sql_trans_process() helper rather than calling
 *        M_sql_trans_begin(), M_sql_trans_rollback() or M_sql_trans_commit() yourself.
 *
 *  \param[out] trans      Returns initialized transaction handle to be used for queries.
 *  \param[in]  pool       Initialized #M_sql_connpool_t object
 *  \param[in]  isolation  Requested isolation level.  The database may choose the closest match
 *                         if the isolation level requested is not supported.
 *  \param[out] error      User-supplied buffer to hold error message.
 *  \param[in]  error_size Size of User-supplied buffer.
 *  \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t results on failure.
 */
M_API M_sql_error_t M_sql_trans_begin(M_sql_trans_t **trans, M_sql_connpool_t *pool, M_sql_isolation_t isolation, char *error, size_t error_size);

/*! Rollback an SQL transaction.
 *
 *  This function should be called if the caller needs to cancel the transaction, or must be
 *  called to clean up the #M_sql_trans_t handle when an unrecoverable error has occurred such
 *  as a server disconnect or deadlock.
 *
 *  The passed in trans handle will be destroyed regardless if this function returns success
 *  or fail.
 *
 *  \note It is recommended to use the M_sql_trans_process() helper rather than calling
 *        M_sql_trans_begin(), M_sql_trans_rollback() or M_sql_trans_commit() yourself.
 *
 *  \param[in]  trans      Initialized transaction handle that will be used to rollback the
 *                         pending transaction, and will be will be destroyed automatically
 *                         upon return of this function.
 *  \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t results on failure.
 */
M_API M_sql_error_t M_sql_trans_rollback(M_sql_trans_t *trans);


/*! Commit a pending SQL transaction.
 *
 *  Any statements executed against the transaction handle will not be applied to the
 *  database until this command is called.
 *
 *  The associated transaction handle will be automatically destroyed regardless if
 *  this function returns success or fail.  If a failure occurs, the caller must assume
 *  the transaction was NOT applied (e.g. rolled back).
 *
 *  \note It is recommended to use the M_sql_trans_process() helper rather than calling
 *        M_sql_trans_begin(), M_sql_trans_rollback() or M_sql_trans_commit() yourself.
 *
 *  \param[in]  trans      Initialized transaction handle that will be used to commit the
 *                         pending transaction, and will be will be destroyed automatically
 *                         upon return of this function.
 *  \param[out] error      User-supplied buffer to hold error message.
 *  \param[in]  error_size Size of User-supplied buffer.
 *  \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t results on failure.
 */
M_API M_sql_error_t M_sql_trans_commit(M_sql_trans_t *trans, char *error, size_t error_size);


/*! Execute a query against the database that is part of an open transaction.  This
 *  request will not automatically commit and must be manually committed via M_sql_trans_commit().
 *
 *  Must call M_sql_stmt_prepare() or M_sql_stmt_prepare_buf() prior to execution.
 *  Must also bind any parameters using \link m_sql_stmt_bind M_sql_stmt_bind_*() \endlink series of functions.
 *
 *  This function will NOT destroy the passed in #M_sql_trans_t object, it is kept open
 *  so additional statements can be executed within the same transaction.  If NOT using
 *  the M_sql_trans_process() helper, it is the caller's responsibility to call
 *  M_sql_trans_commit() or M_sql_trans_rollback() as appropriate.
 *
 *  \param[in]  trans      Initialized #M_sql_trans_t object.
 *  \param[in]  stmt       Initialized and prepared #M_sql_stmt_t object
 *  \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_trans_execute(M_sql_trans_t *trans, M_sql_stmt_t *stmt);


/*! Function prototype called by M_sql_trans_process().
 *
 *  Inside the function created, the integrator should perform each step of the SQL
 *  transaction, and if an error occurs, return the appropriate error condition, whether
 *  it is an error condition as returned by M_sql_trans_execute(), which should be passed
 *  through unmodified, or an internally generated error condition if internal logic fails.
 *  For user-logic generated errors, special error conditions of #M_SQL_ERROR_USER_SUCCESS,
 *  #M_SQL_ERROR_USER_RETRY and #M_SQL_ERROR_USER_FAILURE exist to more accurately identify
 *  the condition rather than attempting to map to the generic SQL subsystem condtions.
 *
 *  \note The function should expect to be called potentially multiple times, so state tracking
 *        must be reset on entry to this user-specified function.  If a rollback or connectivity
 *        failure condition is met, it will automatically be called again.
 *
 *  \warning This function should NEVER call M_sql_trans_commit() or M_sql_trans_rollback() as that
 *           is handled internally by the helper M_sql_trans_process().
 *
 *  \param[in]  trans      Pointer to initialized transaction object to use to execute the transaction.
 *  \param[in]  arg        User-specified argument used for storing metadata about the flow/process.
 *  \param[out] error      User-supplied error buffer to output error message.
 *  \param[in]  error_size Size of user-supplied error buffer.
 *  \return #M_SQL_ERROR_SUCCESS or #M_SQL_ERROR_USER_SUCCESS on successful completion, or one of the #M_sql_error_t error conditions.
 */
typedef M_sql_error_t (*M_sql_trans_commands_t)(M_sql_trans_t *trans, void *arg, char *error, size_t error_size);


/*! Helper function for processing a sequence of SQL commands as a single atomic operation, while
 *  automatically handling things like rollback and connectivity failure situations.
 *
 *  \warning The user-supplied function being called should expect to be called, potentially, multiple
 *           times when errors occur.  State MUST NOT be maintained from call to call or risk having
 *           inconstent data.
 *
 *  Usage Example:
 *  \code{.c}
 *  typedef struct {
 *     M_int64 id;
 *     M_int64 inc;
 *     M_int64 result;
 *  } my_counter_metadata_t;
 *
 *  // Table: CREATE TABLE counters (id INTEGER, val INTEGER, PRIMARY KEY(id))
 *  // Increment requested id by requested amount
 *  static M_sql_error_t my_counter_inc(M_sql_trans_t *trans, void *arg, char *error, size_t error_size)
 *  {
 *    my_counter_metadata_t *data     = arg;
 *    M_sql_stmt_t          *stmt;
 *    M_sql_error_t          err;
 *    M_int64                curr_val = 0;
 *    M_buf_t               *query;
 *
 *    M_mem_set(error, 0, error_size);
 *
 *    // Retrieve current value for id - don't forget to use update locks!
 *    stmt  = M_sql_stmt_create();
 *    query = M_buf_create();
 *    M_buf_add_str(query, "SELECT \"val\" FROM \"counters\"");
 *    M_sql_query_append_updlock(M_sql_trans_get_pool(trans), query, M_SQL_QUERY_UPDLOCK_TABLE);
 *    M_buf_add_str(query, " WHERE \"id\" = ?");
 *    M_sql_query_append_updlock(M_sql_trans_get_pool(trans), query, M_SQL_QUERY_UPDLOCK_QUERYEND);
 *    M_sql_stmt_prepare_buf(stmt, query);
 *    M_sql_stmt_bind_int64(stmt, data->id);
 *    err  = M_sql_trans_execute(trans, stmt);
 *    if (err != M_SQL_ERROR_SUCCESS)
 *      goto done;
 *
 *    if (M_sql_stmt_result_int64(stmt, 0, 0, &curr_val) != M_SQL_ERROR_SUCCESS) {
 *      M_snprintf(error, error_size, "id %lld not found", data->id);
 *      err = M_SQL_ERROR_QUERY_FAILED;
 *      goto done;
 *    }
 *    M_sql_stmt_destroy(stmt);
 *
 *    // Increment the value for the id
 *    data->result = curr_val + data->inc;
 *    stmt = M_sql_stmt_create();
 *    M_sql_stmt_prepare(stmt, "UPDATE \"counters\" SET \"val\" = ? WHERE \"id\" = ?");
 *    M_sql_stmt_bind_int64(stmt, data->result);
 *    M_sql_stmt_bind_int64(stmt, data->id);
 *    err  = M_sql_trans_execute(trans, stmt);
 *    if (err != M_SQL_ERROR_SUCCESS)
 *      goto done;
 *
 *  done:
 *    if (err != M_SQL_ERROR_SUCCESS && M_str_isempty(error)) {
 *      M_snprintf(error, error_size, "%s", M_sql_stmt_get_error_string(stmt));
 *    }
 *    M_sql_stmt_destroy(stmt);
 *
 *    return err;
 *  }
 *
 *  static void run_txn(M_sql_connpool_t *pool)
 *  {
 *    my_counter_metadata_t data;
 *    M_sql_error_t         err;
 *    char                  msg[256];
 *
 *    data.id     = 5;
 *    data.inc    = 25;
 *    data.result = 0;
 *
 *    err = M_sql_trans_process(pool, M_SQL_ISOLATION_SERIALIZABLE, my_counter_inc, &data, msg, sizeof(msg));
 *    if (err != M_SQL_ERROR_SUCCESS) {
 *      M_printf("Error: %s: %s\n", M_sql_error_string(err), msg);
 *      return;
 *    }
 *    M_printf("Success! Final result: %lld\n", data.result);
 *  }
 *  \endcode
 *
 *  \param[in]  pool       Initialized and started pool object.
 *  \param[in]  isolation  Requested isolation level.  The database may choose the closest match
 *                         if the isolation level requested is not supported.
 *  \param[in]  cmd        User-specified function to call to step through the sequence of SQL commands
 *                         to run as part of the transaction.
 *  \param[in]  cmd_arg    Argument to pass to User-specified function for metadata about the command(s)
 *                         being executed.
 *  \param[out] error      User-supplied error buffer to output error message.
 *  \param[in]  error_size Size of user-supplied error buffer.
 *  \return #M_SQL_ERROR_SUCCESS if executed to completion, or one of the #M_sql_error_t fatal errors on
 *          failure (but never #M_SQL_ERROR_QUERY_DEADLOCK or #M_SQL_ERROR_CONN_LOST as those are
 *          automatic retry events)
 */
M_API M_sql_error_t M_sql_trans_process(M_sql_connpool_t *pool, M_sql_isolation_t isolation, M_sql_trans_commands_t cmd, void *cmd_arg, char *error, size_t error_size);

/*! Retrieve the #M_sql_connpool_t object from a transaction handle typically used within M_sql_trans_process()
 *  for using the SQL helpers like M_sql_query_append_updlock() and M_sql_query_append_bitop().
 *
 *  \param[in] trans   Transaction object
 *  \return #M_sql_connpool_t object
 */
M_API M_sql_connpool_t *M_sql_trans_get_pool(M_sql_trans_t *trans);

/*! @} */

__END_DECLS

#endif /* __M_SQL_TRANS_H__ */
