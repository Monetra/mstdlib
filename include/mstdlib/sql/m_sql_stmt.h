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

#ifndef __M_SQL_STMT_H__
#define __M_SQL_STMT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/sql/m_sql.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sql_stmt SQL Statement Management
 *  \ingroup m_sql
 *
 * SQL Statement Management
 *
 * @{
 */
struct M_sql_stmt;
/*! Data type for prepared SQL statements */
typedef struct M_sql_stmt M_sql_stmt_t;

/*! Create a prepared statement object for executing queries.
 *
 *  The statement object holds both request data as well as response data from the server.
 *
 *  Use the \link m_sql_stmt_bind M_sql_stmt_bind_*() \endlink series of functions to bind data to the statement handle
 *  matching the number of bound parameters referenced in the query.  When binding parameters,
 *  they must be bound in order of appearance in the query.
 * 
 *  \return Initialized #M_sql_stmt_t object
 */
M_API M_sql_stmt_t *M_sql_stmt_create(void);

/*! Destroy a prepared statement object 
 * 
 *  \param[in] stmt Initialized #M_sql_stmt_t object
 */
M_API void M_sql_stmt_destroy(M_sql_stmt_t *stmt);


/*! Prepare statement for execution.
 *
 *  This process will perform basic preprocessing and transformation on the statement query.
 *  Parameters for the query may be bound either before or after this call.  A placeholder
 *  of a question mark (?) will be used for each bound parameter in the statement.
 *
 *  No inline text is allowed in a prepared statement, callers must ensure they bind any
 *  text values.
 *
 *  Only a single query per statement execution is allowed.
 *
 * \param[in] stmt  Initialized #M_sql_stmt_t object
 * \param[in] query Query to be prepared
 * \return #M_SQL_ERROR_SUCCESS on sucess, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_prepare(M_sql_stmt_t *stmt, const char *query);

/*! Prepare statement for execution from an #M_buf_t object that will be automatically free'd.
 *
 *  This process will perform basic preprocessing and transformation on the statement query.
 *  Parameters for the query may be bound either before or after this call.  A placeholder
 *  of a question mark (?) will be used for each bound parameter in the statement.
 *
 *  No inline text is allowed in a prepared statement, callers must ensure they bind any
 *  text values.
 *
 *  Only a single query per statement execution is allowed.
 *
 * \param[in] stmt  Initialized #M_sql_stmt_t object
 * \param[in] query Query to be prepared held in an #M_buf_t object.  The #M_buf_t passed
 *                  to this function will be automatically destroyed so must not be used
 *                  after a call to this function.
 * \return #M_SQL_ERROR_SUCCESS on sucess, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_prepare_buf(M_sql_stmt_t *stmt, M_buf_t *query);


/*! Create a "grouped" SQL statement for optimizing server round-trips and commits
 *  for "like" INSERT statements.
 *
 *  When multiple threads are performing similar actions, such as during transaction
 *  processing, it is very likely that those multiple threads might need to perform
 *  essentially the same insert action on the same table with the same number of
 *  bound parameters.  Instead of sending these insertions individually, it is more
 *  efficient to group them together which could result in a single round trip and
 *  transaction instead of dozens or even hundreds.
 *
 *  This implementation will generate a hashtable entry in the pool with the query 
 *  as the key and the statement handle as the value.  If the entry already exists,
 *  it will use the existing statement handle and simply prepare it to take a 
 *  new row then once M_sql_stmt_execute() is called, it wait on a thread conditional
 *  rather than trying to directly execute the statement, which will wake when a
 *  result is ready.  If the entry is not already in the hashtable, it will add
 *  it, then on M_sql_stmt_execute() it will temporarily M_thread_yield() in order
 *  to allow other threads to obtain a lock and append additional rows, then finally
 *  execute and trigger the other threads waiting on the conditional that a result
 *  is ready.
 *
 *  All threads must still call M_sql_stmt_destroy() as it becomes reference counted
 *  when this function is used.  All normal M_sql_stmt_*() functions, except 
 *  M_sql_stmt_prepare() and M_sql_stmt_prepare_buf() may be called.  It should be
 *  advised that M_sql_stmt_result_affected_rows() may not return an expected count
 *  since it would reflect the overall count of grouped rows.  Also, if an error
 *  such as #M_SQL_ERROR_QUERY_CONSTRAINT is returned, the error maybe for another
 *  row, so it is advisable to simply re-run the query without using 
 *  M_sql_stmt_groupinsert_prepare() so you know if the record being inserted is
 *  the culprit or not.
 *
 *  \note At a minimum, one of the M_sql_stmt_bind_*() functions should be called,
 *        as well as M_sql_stmt_execute() and M_sql_stmt_destroy().
 *
 *  \warning If an error is triggered, such as #M_SQL_ERROR_QUERY_CONSTRAINT, the
 *           caller must re-try the transaction using normal M_sql_stmt_create()
 *           and M_sql_stmt_prepare() rather than M_sql_stmt_groupinsert_prepare()
 *           to recover.
 *
 *  \param[in] pool   Initialized connection pool.
 *  \param[in] query  Query string to execute
 *  \return #M_sql_stmt_t handle to bind parameters and execute.
 */
M_API M_sql_stmt_t *M_sql_stmt_groupinsert_prepare(M_sql_connpool_t *pool, const char *query);


/*! Create a "grouped" SQL statement for optimizing server round-trips and commits
 *  for "like" INSERT statements using an M_buf_t as the query string.
 * 
 *  See M_sql_stmt_groupinsert_prepare() for additional information.
 *  \param[in] pool   Initialized connection pool.
 *  \param[in] query  Query string to execute
 *  \return #M_sql_stmt_t handle to bind parameters and execute.
 */
M_API M_sql_stmt_t *M_sql_stmt_groupinsert_prepare_buf(M_sql_connpool_t *pool, M_buf_t *query);


/*! Execute a single query against the database and auto-commit if appropriate.
 *
 *  Must call M_sql_stmt_prepare() or M_sql_stmt_prepare_buf() prior to execution.
 *  Must also bind any parameters using \link m_sql_stmt_bind M_sql_stmt_bind_*() \endlink
 *  series of functions.
 *
 *  If executing as part of a transaction, use M_sql_trans_execute() instead.
 *
 * \param[in]  pool       Initialized #M_sql_connpool_t object
 * \param[in]  stmt       Initialized and prepared #M_sql_stmt_t object
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_execute(M_sql_connpool_t *pool, M_sql_stmt_t *stmt);


/*! Set the maximum number of rows to fetch/cache in the statement handle.
 *
 *  By default, all available rows are cached, if this is called, only
 *  up to this number of rows will be cached client-side.  The M_sql_stmt_fetch()
 *  function must be called until there are no remaining rows server-side.
 *
 *  It is recommended that users use partial fetching for extremely large result
 *  sets (either by number of rows, or for extremely large rows).
 *
 * \warning If a user chooses not to call this function, and the dataset is very
 *          large (especially if it contains BLOBs), then the user risks running
 *          out of memory.  However, if the user sets this value too low for small
 *          row sizes, it could significantly increase the query time on some
 *          servers (like Oracle).
 *
 * \param[in] stmt Initialized, but not yet executed #M_sql_stmt_t object.
 * \param[in] num  Number of rows to cache at a time.
 * \return M_TRUE on success, M_FALSE on failure (misuse, already executed).
 */
M_API M_bool M_sql_stmt_set_max_fetch_rows(M_sql_stmt_t *stmt, size_t num);


/*! Enforce the selection of the master pool, not the read-only pool for this
 *  statement.
 *
 *  Queries will, by default, be routed to the read-only pool.  In some instances,
 *  this may not be desirable if it is known that the query must be as fresh
 *  as possible and thus route to the read/write pool.
 *
 *  Another work around is simply to wrap the read request in a transaction,
 *  but if not performing other tasks, that may be overkill and this function
 *  simplifies that logic.
 *
 * \param[in] stmt       Initialized and not yet executed #M_sql_stmt_t object
 * \return M_TRUE on success, M_FALSE on failure (misuse, already executed).
 */
M_API M_bool M_sql_stmt_set_master_only(M_sql_stmt_t *stmt);


/*! Retrieve whether there are still remaining rows on the server yet to be
 *  fetched by the client.
 *
 *  If there are remaining rows, the client must call M_sql_stmt_fetch() to
 *  cache the next result set.
 *
 *  \param[in] stmt Initialized and executed #M_sql_stmt_t object.
 *  \return M_TRUE if there are remaining rows on the server that can be fetched,
 *          M_FALSE otherwise.
 */
M_API M_bool M_sql_stmt_has_remaining_rows(M_sql_stmt_t *stmt);


/*! Fetch additional rows from the server.
 *
 *  Any existing cached rows will be cleared.
 *
 *  \param[in] stmt Initialized and executed M_sql_stmt_t object.
 *  \return #M_SQL_ERROR_SUCCESS_ROW on success when there may be additional
 *          remaining rows, or #M_SQL_ERROR_SUCCESS if there
 *          are no remaining rows (if #M_SQL_ERROR_SUCCESS is returned, it is
 *          guaranteed no additional rows can be fetched using M_sql_stmt_fetch()).
 *          However, there may still be additional rows in the buffer that
 *          need to be processed, please check with M_sql_stmt_result_num_rows().
 *          Otherwise one of the #M_sql_error_t error conditions will be returned.
 */
M_API M_sql_error_t M_sql_stmt_fetch(M_sql_stmt_t *stmt);


/*! Retrieve the last recorded error.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return last recorded #M_sql_error_t for statement handle.
 */
M_API M_sql_error_t M_sql_stmt_get_error(M_sql_stmt_t *stmt);

/*! Retrieve the last recorded error message in string form.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return last recorded error string, or NULL if none
 */
M_API const char *M_sql_stmt_get_error_string(M_sql_stmt_t *stmt);

/*! @} */


/*! \addtogroup m_sql_stmt_bind SQL Statement Request Parameter Binding
 *  \ingroup m_sql_stmt
 *
 * SQL Statement Parameter Binding
 *
 * @{
 */

/*! Clear bound parameters from a prepared statement object.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 */
M_API void M_sql_stmt_bind_clear(M_sql_stmt_t *stmt);

/*! Increment to next row for statement binding.
 *
 *  Callers can bind multiple rows for insert statements to reduce server round
 *  trips during bulk data insertion.  This function creates a new row and resets
 *  the current column index for binding a new row worth of columns.
 *
 *  All rows must contain the same number of columns consisting of the same data
 *  types (with the exception that NULL may be used) or it is considered a failure.
 *
 *  TODO: It may not be possible to determine which row caused a failure, such as a key
 *  conflict.  Figure this out.
 *
 *  \param[in] stmt Initialized #M_sql_stmt_t object
 */
 M_API void M_sql_stmt_bind_new_row(M_sql_stmt_t *stmt);

/*! Bind M_bool as next column to prepared statement handle
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \param[in] val  Boolean value to bind.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_bool(M_sql_stmt_t *stmt, M_bool val);

/*! Bind M_bool NULL column to prepared statement handle
 *
 * Due to quirks with ODBC, you must know the data type of the bound parameter when
 * binding NULL values.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_bool_null(M_sql_stmt_t *stmt);

/*! Bind M_int16 as next column to prepared statement handle
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \param[in] val  16bit signed integer value to bind.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int16(M_sql_stmt_t *stmt, M_int16 val);

/*! Bind M_int16 NULL column to prepared statement handle
 *
 * Due to quirks with ODBC, you must know the data type of the bound parameter when
 * binding NULL values.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int16_null(M_sql_stmt_t *stmt);

/*! Bind M_int32 as next column to prepared statement handle
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \param[in] val  32bit signed integer value to bind.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int32(M_sql_stmt_t *stmt, M_int32 val);

/*! Bind M_int32 NULL column to prepared statement handle
 *
 * Due to quirks with ODBC, you must know the data type of the bound parameter when
 * binding NULL values.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int32_null(M_sql_stmt_t *stmt);

/*! Bind M_int64 as next column to prepared statement handle
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \param[in] val  64bit signed integer value to bind.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int64(M_sql_stmt_t *stmt, M_int64 val);

/*! Bind M_int64 NULL column to prepared statement handle
 *
 * Due to quirks with ODBC, you must know the data type of the bound parameter when
 * binding NULL values.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_int64_null(M_sql_stmt_t *stmt);

/*! Bind a const string/text as next column to prepared statement handle
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] text    Constant string, that is guaranteed to be available until the statement is executed, to bind to
 *                    statement.  Pass NULL for a NULL value.
 * \param[in] max_len Maximum length of text/string value to use, use 0 for no maximum.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_text_const(M_sql_stmt_t *stmt, const char *text, size_t max_len);

/*! Bind string/text as next column to prepared statement handle that will be automatically freed upon statement destruction.
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] text    Pointer to text/string to bind to statement.  Must not be free'd by caller as destruction of
 *                    the prepared statement handle will automatically free the value.
 * \param[in] max_len Maximum length of text/string value to use, use 0 for no maximum.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_text_own(M_sql_stmt_t *stmt, char *text, size_t max_len);

/*! Bind string/text as next column to prepared statement handle that will be duplicated internally as the caller cannot
 *  guarantee the pointer will survive after execution of this bind call.
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] text    Pointer to text/string that will be duplicated and bound to the statement handle.
 * \param[in] max_len Maximum length of text/string value to use, use 0 for no maximum.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_text_dup(M_sql_stmt_t *stmt, const char *text, size_t max_len);

/*! Bind a const binary buffer as next column to prepared statement handle
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] bin     Constant binary data, that is guaranteed to be available until the statement is executed, to bind to
 *                    statement. Pass NULL for a NULL value.
 * \param[in] bin_len Length of binary value to use.  Only values up to 64k are allowed.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_binary_const(M_sql_stmt_t *stmt, const M_uint8 *bin, size_t bin_len);

/*! Bind binary buffer as next column to prepared statement handle that will be automatically freed upon statement destruction.
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] bin     Pointer to binary data to bind to statement.  Must not be free'd by caller as destruction of
 *                    the prepared statement handle will automatically free the value.
 * \param[in] bin_len Length of binary value to use.  Only values up to 64k are allowed.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_binary_own(M_sql_stmt_t *stmt, M_uint8 *bin, size_t bin_len);

/*! Bind binary data as next column to prepared statement handle that will be duplicated internally as the caller cannot
 *  guarantee the pointer will survive after execution of this bind call.
 *
 * \param[in] stmt    Initialized #M_sql_stmt_t object
 * \param[in] bin     Pointer to binary data that will be duplicated and bound to the statement handle.
 * \param[in] bin_len Length of binary value to use.  Only values up to 64k are allowed.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t values on failure.
 */
M_API M_sql_error_t M_sql_stmt_bind_binary_dup(M_sql_stmt_t *stmt, const M_uint8 *bin, size_t bin_len);

/*! @} */


/*! \addtogroup m_sql_stmt_result SQL Statement Results
 *  \ingroup m_sql_stmt
 *
 * SQL Statement Result Processing
 *
 * @{
 */

/*! Retrieve the number of rows affected by the executed statement.
 *
 *  Does not apply to SELECT statements, used often on UPDATE and DELETE 
 *  statements to reflect how many rows were affected.
 *
 *  NOTE: On update, if updating a row in the database, and the passed in
 *  fields being updated are the same as the database already contains, depending
 *  on the backend database driver, that row may or may not be included.  
 *  Developers should not rely on this behavior.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return affected rows.
 */
M_API size_t M_sql_stmt_result_affected_rows(M_sql_stmt_t *stmt);


/*! Retrieve the number of cached rows in statement handle.   
 *
 *  There may be additional rows yet to be fetched if not retrieving all rows
 *  at once.  This function will return only the number of cached rows client-side,
 *  each time M_sql_stmt_fetch() is called, previous cached rows are cleared.  This
 *  result is NOT cumulative.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return row count
 */
M_API size_t M_sql_stmt_result_num_rows(M_sql_stmt_t *stmt);


/*! Retrieve the number of total rows that have been fetched so far.  
 *
 *  This number may be greater than or equal to M_sql_stmt_result_num_rows()
 *  as each call to M_sql_stmt_fetch() will clear the current cached rows
 *  (and count), but this value will continue to increment.
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return total row count fetched so far.
 */
M_API size_t M_sql_stmt_result_total_rows(M_sql_stmt_t *stmt);


/*! Retrieve the number of columns returned from the server in response to a query.  
 *
 * \param[in] stmt Initialized #M_sql_stmt_t object.
 * \return column count.
 */
M_API size_t M_sql_stmt_result_num_cols(M_sql_stmt_t *stmt);

/*! Retrieve the name of a returned column.
 *
 * \param[in] stmt Initialized and executed #M_sql_stmt_t object.
 * \param[in] col  Index of column.
 * \return column name or NULL on failure.
 */
M_API const char *M_sql_stmt_result_col_name(M_sql_stmt_t *stmt, size_t col);


/*! Retrieve the data type of the returned column.
 *
 * \param[in]  stmt      Initialized and executed #M_sql_stmt_t object.
 * \param[in]  col       Index of column.
 * \param[out] type_size Optional, pass NULL if not desired.  For TEXT and BINARY types, the column
 *                       definition may indicate a possible size (or maximum size).  If the value is 0,
 *                       it means the column width is bounded by the maximums of the SQL server.
 * \return Column type for referenced column.
 */
M_API M_sql_data_type_t M_sql_stmt_result_col_type(M_sql_stmt_t *stmt, size_t col, size_t *type_size);


/*! Retrieve the data type of the returned column.
 *
 * \param[in]  stmt      Initialized and executed #M_sql_stmt_t object.
 * \param[in]  col       Name of column.
 * \param[out] type_size Optional, pass NULL if not desired.  For TEXT and BINARY types, the column
 *                       definition may indicate a possible size (or maximum size).  If the value is 0,
 *                       it means the column width is bounded by the maximums of the SQL server.
 * \return Column type for referenced column or M_SQL_DATA_TYPE_UNKNOWN if column not found.
 */
M_API M_sql_data_type_t M_sql_stmt_result_col_type_byname(M_sql_stmt_t *stmt, const char *col, size_t *type_size);


/*! Retrieve the column index by name
 *
 * \param[in]  stmt Initialized and executed #M_sql_stmt_t object.
 * \param[in]  col  Column name, case insensitive.
 * \param[out] idx  Index of column.
 * \return M_TRUE on success, M_FALSE if column not found.
 */
M_API M_bool M_sql_stmt_result_col_idx(M_sql_stmt_t *stmt, const char *col, size_t *idx);


/*! Retrieve if a cell is NULL.
 *
 * \param[in]  stmt    Initialized #M_sql_stmt_t object.
 * \param[in]  row     Row index to retrieve
 * \param[in]  col     Column index to retrieve
 * \param[out] is_null Return whether column is NULL or not.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_isnull(M_sql_stmt_t *stmt, size_t row, size_t col, M_bool *is_null);

/*! Retrieve a textual cell from the resultset.
 *
 * All cell types may be retrieved in their textual form.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \param[out] text Output constant pointer to cell data.  May be NULL if a NULL
 *                  column.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_text(M_sql_stmt_t *stmt, size_t row, size_t col, const char **text);

/*! Retrieve a bool value from the resultset.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \param[out] val  Output boolean value.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_bool(M_sql_stmt_t *stmt, size_t row, size_t col, M_bool *val);

/*! Retrieve a signed 16bit Integer cell from the resultset.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \param[out] val  Output integer data.  If NULL, outputs 0.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int16(M_sql_stmt_t *stmt, size_t row, size_t col, M_int16 *val);


/*! Retrieve a signed 32bit Integer cell from the resultset.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \param[out] val  Output integer data.  If NULL, outputs 0.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int32(M_sql_stmt_t *stmt, size_t row, size_t col, M_int32 *val);

/*! Retrieve a signed 64bit Integer cell from the resultset.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \param[out] val  Output integer data.  If NULL, outputs 0.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int64(M_sql_stmt_t *stmt, size_t row, size_t col, M_int64 *val);

/*! Retrieve a binary cell from the resultset.
 *
 * \param[in]  stmt     Initialized #M_sql_stmt_t object.
 * \param[in]  row      Row index to retrieve
 * \param[in]  col      Column index to retrieve
 * \param[out] bin      Output constant pointer to cell data.  May be NULL if a NULL
 *                      column.
 * \param[out] bin_size Size of binary data returned.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_binary(M_sql_stmt_t *stmt, size_t row, size_t col, const M_uint8 **bin, size_t *bin_size);


/*! Retrieve if a cell is NULL, directly, ignoring errors.
 *
 * \param[in]  stmt    Initialized #M_sql_stmt_t object.
 * \param[in]  row     Row index to retrieve
 * \param[in]  col     Column index to retrieve
 * \return M_TRUE on usage failure, or if NULL cell, otherwise M_FALSE
 */
M_API M_bool M_sql_stmt_result_isnull_direct(M_sql_stmt_t *stmt, size_t row, size_t col);

/*! Retrieve a textual cell from the resultset, directly, ignoring errors.
 *
 * All cell types may be retrieved in their textual form.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \return String result on success, or NUL on failure.
 */
M_API const char *M_sql_stmt_result_text_direct(M_sql_stmt_t *stmt, size_t row, size_t col);

/*! Retrieve a bool value from the resultset, directly, ignoring errors.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \return Return M_TRUE if data is a boolean value resulting in truth, or M_FALSE for all other cases
 */
M_API M_bool M_sql_stmt_result_bool_direct(M_sql_stmt_t *stmt, size_t row, size_t col);

/*! Retrieve a signed 16bit Integer cell from the resultset, directly, ignoring errors.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int16 M_sql_stmt_result_int16_direct(M_sql_stmt_t *stmt, size_t row, size_t col);


/*! Retrieve a signed 32bit Integer cell from the resultset, directly, ignoring errors.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int32 M_sql_stmt_result_int32_direct(M_sql_stmt_t *stmt, size_t row, size_t col);

/*! Retrieve a signed 64bit Integer cell from the resultset, directly, ignoring errors.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column index to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int64 M_sql_stmt_result_int64_direct(M_sql_stmt_t *stmt, size_t row, size_t col);

/*! Retrieve a binary cell from the resultset, directly, ignoring errors.
 *
 * \param[in]  stmt     Initialized #M_sql_stmt_t object.
 * \param[in]  row      Row index to retrieve
 * \param[in]  col      Column index to retrieve
 * \param[out] bin_size Size of binary data returned.
 * \return NULL on error (not right data type), or error
 */
M_API const M_uint8 *M_sql_stmt_result_binary_direct(M_sql_stmt_t *stmt, size_t row, size_t col, size_t *bin_size);



/*! Retrieve if a cell is NULL (by column name).
 *
 * \param[in]  stmt    Initialized #M_sql_stmt_t object.
 * \param[in]  row     Row index to retrieve
 * \param[in]  col     Column nameto retrieve
 * \param[out] is_null Return whether column is NULL or not.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_isnull_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_bool *is_null);

/*! Retrieve a textual cell from the resultset (by column name).
 *
 * All cell types may be retrieved in their textual form.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \param[out] text Output constant pointer to cell data.  May be NULL if a NULL
 *                  column.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_text_byname(M_sql_stmt_t *stmt, size_t row, const char *col, const char **text);

/*! Retrieve a bool value from the resultset (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \param[out] val  Output boolean value.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_bool_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_bool *val);

/*! Retrieve a signed 16bit Integer cell from the resultset (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \param[out] val  Output integer data.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int16_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int16 *val);


/*! Retrieve a signed 32bit Integer cell from the resultset (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \param[out] val  Output integer data.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int32_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int32 *val);

/*! Retrieve a signed 64bit Integer cell from the resultset (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \param[out] val  Output integer data.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_int64_byname(M_sql_stmt_t *stmt, size_t row, const char *col, M_int64 *val);

/*! Retrieve a binary cell from the resultset (by column name).
 *
 * \param[in]  stmt     Initialized #M_sql_stmt_t object.
 * \param[in]  row      Row index to retrieve
 * \param[in]  col      Column index to retrieve
 * \param[out] bin      Output constant pointer to cell data.  May be NULL if a NULL
 *                      column.
 * \param[out] bin_size Size of binary data returned, maximum is 64k.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of #M_sql_error_t errors on failure.
 */
M_API M_sql_error_t M_sql_stmt_result_binary_byname(M_sql_stmt_t *stmt, size_t row, const char *col, const M_uint8 **bin, size_t *bin_size);

/*! Retrieve if a cell is NULL, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt    Initialized #M_sql_stmt_t object.
 * \param[in]  row     Row index to retrieve
 * \param[in]  col     Column name to retrieve
 * \return M_TRUE on usage failure, or if NULL cell, otherwise M_FALSE
 */
M_API M_bool M_sql_stmt_result_isnull_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);


/*! Retrieve a textual cell from the resultset, directly, ignoring errors (by column name).
 *
 * All cell types may be retrieved in their textual form.
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \return String result on success, or NUL on failure.
 */
M_API const char *M_sql_stmt_result_text_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);

/*! Retrieve a bool value from the resultset, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \return Return M_TRUE if data is a boolean value resulting in truth, or M_FALSE for all other cases
 */
M_API M_bool M_sql_stmt_result_bool_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);

/*! Retrieve a signed 16bit Integer cell from the resultset, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int16 M_sql_stmt_result_int16_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);


/*! Retrieve a signed 32bit Integer cell from the resultset, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int32 M_sql_stmt_result_int32_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);

/*! Retrieve a signed 64bit Integer cell from the resultset, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt Initialized #M_sql_stmt_t object.
 * \param[in]  row  Row index to retrieve
 * \param[in]  col  Column name to retrieve
 * \return Return integer value represented in cell, or possibly 0 on error
 */
M_API M_int64 M_sql_stmt_result_int64_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col);

/*! Retrieve a binary cell from the resultset, directly, ignoring errors (by column name).
 *
 * \param[in]  stmt     Initialized #M_sql_stmt_t object.
 * \param[in]  row      Row index to retrieve
 * \param[in]  col      Column name to retrieve
 * \param[out] bin_size Size of binary data returned.
 * \return NULL on error (not right data type), or error
 */
M_API const M_uint8 *M_sql_stmt_result_binary_byname_direct(M_sql_stmt_t *stmt, size_t row, const char *col, size_t *bin_size);


/*! @} */

__END_DECLS

#endif /* __M_SQL_STMT_H__ */
