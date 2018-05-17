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

#ifndef __M_SQL_INT_H__
#define __M_SQL_INT_H__

#include <mstdlib/sql/m_sql_driver.h>


/*! Reserve a connection from the pool.
 * 
 *  Acquire a connection, will wait until a connection is available.  This
 *  is a blocking operation if no connections are available.
 * 
 *  \param[in] pool     Pointer to initialized pool object.
 *  \param[in] readonly Acquire a readonly connection or primary connection.  Advisory, if readonly
 *                      is not configured, then will obtain a primary.
 *  \return connection on success that must be released with M_sql_connpool_release_conn(), or NULL on error
 */
M_sql_conn_t *M_sql_connpool_acquire_conn(M_sql_connpool_t *pool, M_bool readonly, M_bool for_trans);

/*! Release a connection reserved with M_sql_connpool_acquire_conn()
 * 
 *  \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 */
void M_sql_connpool_release_conn(M_sql_conn_t *conn);

/*! Retrieve the initialization flags for the connection pool.
 *
 * \param[in] pool Pointer to initialized pool object.
 * \return bitmap of flags
 */
M_sql_connpool_flags_t M_sql_connpool_flags(M_sql_connpool_t *pool);

const M_sql_driver_t *M_sql_connpool_get_driver(M_sql_connpool_t *pool);

/*! Retrieve a handle to the driver structure from a connection.  This can be used
 *  to obtain the callbacks, etc.
 * 
 *  \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 */
const M_sql_driver_t *M_sql_conn_get_driver(M_sql_conn_t *conn);

M_uint64 M_sql_conn_duration_start_ms(M_sql_conn_t *conn);
M_uint64 M_sql_conn_duration_last_ms(M_sql_conn_t *conn);

void M_sql_conn_use_stmt(M_sql_conn_t *conn, M_sql_stmt_t *stmt);
void M_sql_conn_release_stmt(M_sql_conn_t *conn);


/*! Set the current connection state.
 *
 * \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 * \param[in] state connection state to set
 */
void M_sql_conn_set_state(M_sql_conn_t *conn, M_sql_conn_state_t state);

/*! Set the connection state based on an error code.
 *
 * In general, if M_sql_error_is_disconnect() returns M_TRUE, will set
 * #M_SQL_CONN_STATE_FAILED.  If M_sql_error_is_rollback(), will set
 * #M_SQL_CONN_STATE_ROLLBACK.
 *
 * \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 * \param[in] err  One of the M_sql_error_t error conditions returned.
 */
void M_sql_conn_set_state_from_error(M_sql_conn_t *conn, M_sql_error_t err);

/*! Retrieve a cached prepared statement handle from the connection object.  Client-side
 *  prepared statement handle caching is used as an optimization as it may reduce
 *  load due to query string parsing, or server round trips by being able to reference
 *  a handle already available on the server-side.
 * 
 *  \param[in] conn  Connection acquired with M_sql_connpool_acquireconn()
 *  \param[in] query Query string already pre-processed by the DB driver backend.
 *  \return Pointer to record in cache, or NULL if no statement handle is cached
 */
M_sql_driver_stmt_t *M_sql_conn_get_stmt_cache(M_sql_conn_t *conn, const char *query);

/*! Insert a prepared statement handle to be cached into the connection object.
 * 
 *  If the passed in statement handle matches the existing statement handle, this is
 *  a no-op.  Otherwise if it does not match, it will free the original handle and
 *  replace it with the new handle.
 * 
 *  \param[in] conn  Connection acquired with M_sql_connpool_acquireconn()
 *  \param[in] query Query string already pre-processed by the DB driver backend.
 *  \param[in] stmt  Statement handle associated with query, or NULL if requesting to
 *                   remove an existing association.
 */
void M_sql_conn_set_stmt_cache(M_sql_conn_t *conn, const char *query, M_sql_driver_stmt_t *stmt);

M_bool M_sql_stmt_result_clear(M_sql_stmt_t *stmt);
M_bool M_sql_stmt_result_clear_data(M_sql_stmt_t *stmt);

M_uint64 M_sql_stmt_duration_start_ms(M_sql_stmt_t *stmt);
M_uint64 M_sql_stmt_duration_last_ms(M_sql_stmt_t *stmt);

M_uint64 M_sql_trans_duration_start_ms(M_sql_trans_t *trans);
M_uint64 M_sql_trans_duration_last_ms(M_sql_trans_t *trans);

M_sql_trans_t *M_sql_stmt_get_trans(M_sql_stmt_t *stmt);
M_sql_conn_t *M_sql_stmt_get_conn(M_sql_stmt_t *stmt);
M_sql_conn_t *M_sql_trans_get_conn(M_sql_trans_t *trans);
void M_sql_trace_message_conn(M_sql_trace_t type, M_sql_conn_t *conn, M_sql_error_t err, const char *error);
void M_sql_trace_message_trans(M_sql_trace_t type, M_sql_trans_t *trans, M_sql_error_t err, const char *error);
void M_sql_trace_message_stmt(M_sql_trace_t type, M_sql_stmt_t *stmt);
M_sql_trace_cb_t M_sql_connpool_get_cb(M_sql_connpool_t *pool, void **cb_arg);

/*! Retrieve open statement handle from the pool for the same query to append more rows.
 *  \note returns LOCKED statement handle if one was found, otherwise returns LOCKED
 *        pool handle.
 */
M_sql_stmt_t *M_sql_connpool_get_groupinsert(M_sql_connpool_t *pool, const char *query);

/*! Insert statement handle for query.
 *  \note pool handle MUST be LOCKED on entry, it will be returned UNLOCKED. Statement
 *        handle should be LOCKED on entry, will be returned as LOCKED.
 */
void M_sql_connpool_set_groupinsert(M_sql_connpool_t *pool, const char *query, M_sql_stmt_t *stmt);

/*! Remove the statement handle from the pool as it is about to be execute.
 *  \note the statement handle MUST be LOCKED on entry, it will be returned LOCKED.
 */
void M_sql_connpool_remove_groupinsert(M_sql_connpool_t *pool, const char *query, M_sql_stmt_t *stmt);

/* ----- Statement Info ------ */

/*! Definition for statement bind column */
typedef struct {
	M_sql_data_type_t type;    /*!< Data type of column */
	M_bool            isnull;  /*!< Column data is NULL */
	union {
		M_bool   b;            /*!< Used when type == M_SQL_DATA_TYPE_BOOL */
		M_int16  i16;          /*!< Used when type == M_SQL_DATA_TYPE_INT16 */
		M_int32  i32;          /*!< Used when type == M_SQL_DATA_TYPE_INT32 */
		M_int64  i64;          /*!< Used when type == M_SQL_DATA_TYPE_INT64 */
		struct {
			char   *data;      /*!< Pointer to data for string */
			size_t  max_len;   /*!< Maximum length of string to pass to SQL server */
			M_bool  is_const;  /*!< Whether or not this is const data, if not, then will be free'd on destruction */
		}        text;         /*!< Used when type == M_SQL_DATA_TYPE_TEXT */
		struct {
			M_uint8 *data;     /*!< Pointer to binary data */
			size_t   len;      /*!< Length of binary data */
			M_bool   is_const; /*!< Whether or not this is const data, if not, then will be free'd on destruction */
		}        binary;       /*!< Used when type == M_SQL_DATA_TYPE_BINARY */
	} v;                       /*!< Union holding possible values based on data type */
} M_sql_stmt_bind_col_t;

/*! Definition for statement bind rows */
typedef struct {
	M_sql_stmt_bind_col_t *cols;    /*!< Array of bound columns */
	size_t                 col_cnt; /*!< Count of columns */
} M_sql_stmt_bind_row_t;


/*! Definition for Column Descriptors */
typedef struct {
	M_sql_data_type_t type;      /*!< Column type */
	char              name[128]; /*!< Column name */
	size_t            max_size;  /*!< For TEXT/BLOB types, maximum size, if known, or 0. */
} M_sql_stmt_result_coldef_t;


/*! Definition for cell metadata */
typedef struct {
	size_t offset; /*!< Start offset in row buffer to cell. Always a multiple of the Alignment (M_SAFE_ALIGNMENT) */
	size_t length; /*!< Length of data, always in string form, INCLUDING NULL terminator, except for BLOBs.  A
	                *   length of 0 indicates a NULL column */
} M_sql_stmt_result_cellinfo_t;


/*! Result descriptor */
typedef struct {
	M_sql_stmt_result_coldef_t   *col_defs;      /*!< Array of Description/Definition of columns */
	M_hash_stridx_t              *col_name;      /*!< Column name to index hashtable */
	size_t                        num_cols;      /*!< Number of result columns */

	size_t                        num_rows;      /*!< Number of currently cached result rows */
	size_t                        alloc_rows;    /*!< Number of allocated results rows (allocated in powers of 2),
	                                              *   but all maybe cleared but not dealloc'd when fetching next
	                                              *   batch of rows */
	M_sql_stmt_result_cellinfo_t *cellinfo;      /*!< Array of cells with metadata about the specific cell (start offset, length)
	                                              *   The count of this array is (alloc_rows * num_cols), and the index to
	                                              *   a specific cell based on row and col is  (row * num_cols + col) */
	M_buf_t                     **rows;          /*!< Array of buffers to hold rows.  Multiple columns are stored in the buffer
	                                              *   at alignment offsets.  The size of the array is equal to alloc_rows */
	size_t                        curr_col;      /*!< State Tracking. Current column being added, 1-based */
	size_t                        total_rows;    /*!< Total number of rows fetched */
} M_sql_stmt_result_t;


typedef enum {
	M_SQL_GROUPINSERT_NEW      = 0, /*!< New, allowed to insert additional rows */
	M_SQL_GROUPINSERT_PENDING  = 1, /*!< Pending, no new rows allowed */
	M_SQL_GROUPINSERT_FINISHED = 2  /*!< Result complete */
} M_sql_groupinsert_t;

/*! Definition for SQL statment handle */
struct M_sql_stmt {
	/* Query information */
	char  *query_user;       /*!< User-supplied query */
	char  *query_prepared;   /*!< Preprocessed query */
	size_t query_param_cnt;  /*!< Count of parameters in supplied query */
	size_t max_fetch_rows;   /*!< User-specified maximum number of rows to cache/fetch at a time, per M_sql_stmt_fetch() */
	M_bool master_only;      /*!< Controlled by the caller to enforce routing to the read/write pool not the read-only pool */
	M_bool ignore_tranfail;  /*!< Do not emit #M_SQL_TRACE_TRANFAIL messages */

	M_timeval_t start_tv; /*!< Start of execution */
	M_timeval_t last_tv;  /*!< End of execution, but before row fetching */

	/* Row binding information */
	M_sql_stmt_bind_row_t *bind_rows;       /*!< Array of bound rows */
	size_t                 bind_row_cnt;    /*!< Count of rows */
	size_t                 bind_row_offset; /*!< Offset of starting position of next row of parameters */

	/* Result information */
	size_t                 affected_rows; /*!< Count of affected rows for INSERT/UPDATE/DELETE type operations */
	M_sql_stmt_result_t   *result; /*!< Pointer to result structure */

	/* Error handling */
	M_sql_error_t last_error;      /*!< Last recorded error */
	char          error_msg[256];  /*!< Last recorded error message */

	/* State Tracking */
	M_sql_driver_stmt_t *dstmt;    /*!< DB-driver specific statement handle used for execution */
	M_sql_conn_t        *conn;     /*!< Used to hang on to the connection for row fetching */
	M_sql_trans_t       *trans;    /*!< SQL transaction handle */

	/* Group Insert handling */
	M_thread_mutex_t    *group_lock;  /*!< Mutex only initialized for M_sql_stmt_groupinsert_prepare() */
	size_t               group_cnt;   /*!< Number of reference counts on statement handle */
	M_sql_groupinsert_t  group_state; /*!< Boolean indicating if group operation is done.  Useful to detect a
	                                   *   spurious wakeup from M_thread_cond_wait() */
	M_thread_cond_t     *group_cond;  /*!< Group conditional used to let other callers know a result is available */
};


#endif
