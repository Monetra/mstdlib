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

#ifndef __M_SQL_DRIVER_H__
#define __M_SQL_DRIVER_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/mstdlib_sql.h>
/* Needed for M_module_handle_t */
#include <mstdlib/sql/m_module.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


/*! \addtogroup m_sql_driver SQL Module/Driver
 *  \ingroup m_sql
 *
 * SQL Module/Driver definitions and helpers.
 *
 * These functions are used only by the internal implementation and custom
 * loaded drivers.  Typically an integrator would never use these unless they
 * are developing their own custom SQL driver.  If so, they would
 * \code{.c}
 *   #include <mstdlib/sql/m_sql_driver.h>
 * \endcode
 * To gain access to these functions.
 *
 * @{
 */

/*! Current subsystem versioning for module compatibility tracking */
#define M_SQL_DRIVER_VERSION 0x0100

/*! Private connection object structure from pool */
struct M_sql_conn;
/*! Private connection object */
typedef struct M_sql_conn M_sql_conn_t;

/*! Driver-defined private storage for connection pool */
struct M_sql_driver_connpool;

/*! Driver-defined private storage for connection pool (typedef) */
typedef struct M_sql_driver_connpool M_sql_driver_connpool_t;

/*! Driver-defined private storage for connection object */
struct M_sql_driver_conn;

/*! Driver-defined private storage for connection object (typedef) */
typedef struct M_sql_driver_conn M_sql_driver_conn_t;

/*! Driver-defined private storage for a statement handle */
struct M_sql_driver_stmt;

/*! Driver-defined private storage for a statement handle (typedef) */
typedef struct M_sql_driver_stmt M_sql_driver_stmt_t;



/*! Callback called when the module is loaded.  If there is any global environment that needs
 *  to be set up, it should be called here.  This is guaranteed to only be called once.
 *
 *  \param[out] error      User-supplied buffer to hold an error message
 *  \param[in]  error_size Size of user-supplied error buffer.
 *  \return M_TRUE on success, M_FALSE on failure
 */
typedef M_bool (*M_sql_driver_cb_init_t)(char *error, size_t error_size);

/*! Callback called when the module is unloaded.  If there is any global environment that needs
 *  to be destroyed, it should be called here.  This is guaranteed to only be called once
 *  and only after a successful M_sql_driver_cb_init_t.
 */
typedef void (*M_sql_driver_cb_destroy_t)(void);

/*! Callback called when a pool is created or updated with a read-only pool.  A dictionary of
 *  configuration is passed for the connection pool type (primary vs readonly).
 *
 *  Any parameters needed should be saved into the private handle returned.  The dictionaries
 *  passed in should be strictly validated using, at a minimum, M_sql_driver_validate_connstr().
 *
 *  \param[in,out] dpool        Driver-specific pool handle.  Should be initailized if passed in as
 *                              NULL.  Currently the only time that occurs is when is_readonly is M_FALSE.
 *                              Only a single driver-specific pool is created for all pool types
 *                              (primary, readonly), so if an initialized object is passed in, the
 *                              additional configuration data needs to be appended to the current object.
 *  \param[in]     pool         Partially initialized pool, mostly used for getting other metadata for
 *                              verification (e.g. username/password)
 *  \param[in]     is_readonly  M_TRUE if the pool being initialized is readonly, M_FALSE if primary.
 *  \param[in]     conndict     Configuration dictionary of parameters
 *  \param[out]    num_hosts    The number of hosts contained within the configuration for load balancing or failover purposes.
 *  \param[out]    error        User-supplied buffer to output an error message.
 *  \param[in]     error_size   Size of user-supplied error buffer.
 *  \return M_TRUE on success, M_FALSE on failure
 */
typedef M_bool (*M_sql_driver_cb_createpool_t)(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size);

/*! Callback called when the pool is destroyed to free the driver-specific pool object
 *
 *  \param[in] dpool  Pool object to be destroyed
 */
typedef void (*M_sql_driver_cb_destroypool_t)(M_sql_driver_connpool_t *dpool);

/*! Callback called to initialize a new connection to the database.
 *  \param[out] conn             Initialized private connection object is returned on success.
 *  \param[in]  pool             Pool handle, use M_sql_driver_pool_get_dpool() to get driver-specific pool handle.
 *  \param[in]  is_readonly_pool M_TRUE if the connection references a read-only pool, or M_FALSE if the primary pool.
 *  \param[in]  host_idx         Host index to use (if multiple hosts configured and returned by #M_sql_driver_cb_createpool_t), 0 based.
 *  \param[out] error            User-supplied buffer to output an error message.
 *  \param[in]  error_size       Size of user-supplied error buffer.
 *  \return M_SQL_ERROR_SUCCESS on success, or one of the M_sql_error_t errors
 */
typedef M_sql_error_t (*M_sql_driver_cb_connect_t)(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size);

/*! Callback called to get the server version string/information
 *  \param[in] conn Private connection object.
 *  \return String indicating server name and version in an implementation-defined manner
 */
typedef const char *(*M_sql_driver_cb_serverversion_t)(M_sql_driver_conn_t *conn);

/*! Callback called after each connection is successfully established.  The is_first_in_pool can be
 *  used to key off of to ensure if the action only needs to be performed once for the lifetime of
 *  the pool after connectivity is established, it can be done there.
 *
 *  Examples of use for this callback include setting SQLite journal mode, performing an SQLite
 *  analyze or integrity check.  For other databases, this may be where custom store procedures
 *  are created, or default transaction isolation levels are set.
 *
 *  \param[in]  conn             Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                               private connection handle.
 *  \param[in]  dpool            Driver-specific pool handle returned from #M_sql_driver_cb_createpool_t
 *  \param[in]  is_first_in_pool M_TRUE if first connection in a pool to be established, M_FALSE if secondary connection.
 *  \param[in]  is_readonly      M_TRUE if this is referencing the readonly pool, M_FALSE if the normal pool
 *  \param[out] error            User-supplied buffer to output an error message.
 *  \param[in]  error_size       Size of user-supplied error buffer.
 *  \return M_SQL_ERROR_SUCCESS on success, or one of the M_sql_error_t errors
 */
typedef M_sql_error_t (*M_sql_driver_cb_connect_runonce_t)(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size);


/*! Callback called to disconnect and destroy all metadata associated with a connection.
 * \param[in] conn  Private driver-specific connection handle to be disconnected and destroyed
 */
typedef void (*M_sql_driver_cb_disconnect_t)(M_sql_driver_conn_t *conn);

/*! Rewrite the user-provided query string to one more easily consumed by the database
 *  backend.
 *
 *  It is suggested implementors use M_sql_driver_queryformat() if possible instead of
 *  writing this from scratch.
 *
 *  \param[in] conn        Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                         private connection handle.
 *  \param[in] query       User-provided query string
 *  \param[in] num_params  Number of bound parameters (per row)
 *  \param[in] num_rows    For insert statements, number of rows of bound parameters
 *  \param[in] error       User-supplied error message buffer
 *  \param[in] error_size  Size of user-supplied error message buffer
 *  \return Allocated buffer containing a rewritten query string or NULL on failure
 */
typedef char *(*M_sql_driver_cb_queryformat_t)(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size);

/*! Return number of rows that will be worked on for the current execution.
 *
 *  \param[in] conn        Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                         private connection handle.
 *  \param[in] num_params  Number of bound parameters (per row)
 *  \param[in] num_rows    For insert statements, number of rows of bound parameters
 *  \return Row count
 */
typedef size_t (*M_sql_driver_cb_queryrowcnt_t)(M_sql_conn_t *conn, size_t num_params, size_t num_rows);

/*! Prepare the provided query for execution.
 *
 * \param[in,out] driver_stmt Driver-specific statement handle.  If executing based on a cached
 *                            prepared statement handle, may pass in existing handle.  Handle used
 *                            will always be returned (may or may not be identical to passed in handle)
 * \param[in]     conn        Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                            private connection handle.
 * \param[in]     stmt        Statement handle containing all the details necessary for preparation
 * \param[in]     error       User-supplied error message buffer
 * \param[in]     error_size  Size of user-supplied error message buffer
 * \return M_SQL_ERROR_SUCCESS on success, or one of the M_sql_error_t errors on failure
 */
typedef M_sql_error_t (*M_sql_driver_cb_prepare_t)(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size);

/*! Destroy the driver-specific prepared statement handle.
 *
 * \param[in]     stmt    Driver-specific statement handle to be destroyed.
 */
typedef void (*M_sql_driver_cb_prepare_destroy_t)(M_sql_driver_stmt_t *stmt);

/*! Execute the query.
 *
 * \param[in]  conn          Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                           private connection handle.
 * \param[in]  stmt          Driver-specific statement handle to be executed as returned by M_sql_driver_cb_prepare_t
 * \param[out] rows_executed For drivers that support multiple rows being inserted in a single query, this is how
 *                           many bind parameter rows were actually inserted by the query.  This value may be up to
 *                           M_sql_driver_stmt_bind_rows() in size.  Execute will be called in a loop if not all rows
 *                           were executed in a single query until complete (with each iteration decrementing the
 *                           visible M_sql_driver_stmt_bind_rows()).
 * \param[in]  error         User-supplied error message buffer
 * \param[in]  error_size    Size of user-supplied error message buffer
 * \return one of the M_sql_error_t conditions
 */
typedef M_sql_error_t (*M_sql_driver_cb_execute_t)(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size);

/*! Fetch rows from server
 *
 * \param[in] conn        Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                        private connection handle.
 * \param[in] stmt        System statement object, use M_sql_driver_stmt_get_stmt() to fetch driver-specific statement handle.
 * \param[in] error       User-supplied error message buffer
 * \param[in] error_size  Size of user-supplied error message buffer
 * \return one of the M_sql_error_t conditions
 */
typedef M_sql_error_t (*M_sql_driver_cb_fetch_t)(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size);

/*! Begin a transaction on the server with the specified isolation level.
 *
 *  If the isolation level is not supported by the server, the closet match should be chosen.
 *
 *  \param[in]  conn       Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                         private connection handle.
 *  \param[in]  isolation  Requested isolation level
 *  \param[out] error      User-supplied error message buffer
 *  \param[in]  error_size Size of user-supplied error message buffer
 *  \return one of the M_sql_error_t conditions
 */
typedef M_sql_error_t (*M_sql_driver_cb_begin_t)(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size);

/*! Rollback a transaction.
 *
 *  The connection object should retain enough metadata to know if there is a current open transaction
 *  or not, so that if the transaction was already implicitly closed by a failed previous request,
 *  this should be a no-op.
 *
 *  If the rollback fails when it is expected to succeed, the driver should probably return a code
 *  to indicate a critical connectivity failure has occurred to kill the connection.
 *
 *  \param[in]  conn       Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                         private connection handle.
 *  \return one of the M_sql_error_t conditions
 */
typedef M_sql_error_t (*M_sql_driver_cb_rollback_t)(M_sql_conn_t *conn);

/*! Commit a transaction.
 *
 *  If a commit fails, the transaction must be automatically rolled back by the driver.
 *
 *  \param[in]  conn       Initialized connection object, use M_sql_driver_conn_get_conn() to get driver-specific
 *                         private connection handle.
 *  \param[out] error      User-supplied error message buffer
 *  \param[in]  error_size Size of user-supplied error message buffer
 *  \return one of the M_sql_error_t conditions
 */
typedef M_sql_error_t (*M_sql_driver_cb_commit_t)(M_sql_conn_t *conn, char *error, size_t error_size);

/*! Output the SQL-driver specific data type to the supplied buffer based on the input type and
 *  maximum length.
 *
 *  \param[in]     pool     Pointer to connection pool object
 *  \param[in,out] buf      Buffer to write sql-server-specific data type into.
 *  \param[in]     type     mstdlib sql data type
 *  \param[in]     max_len  Maximum length of data type. Meaningful for Text and Binary types only,
 *                          or use 0 for maximum supported server size.
 *  \param[in]     is_cast  Used to convert to data type for server.
 *  \return M_TRUE on success, M_FALSE on error such as misuse
 */
typedef M_bool (*M_sql_driver_cb_datatype_t)(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len, M_bool is_cast);

/*! Append an SQL-driver specific suffix to the end of the provided CREATE TABLE query.
 *
 *  Some servers like MySQL append things like " ENGINE=InnoDB CHARSET=utf8"
 *
 * \param[in]     pool  SQL Server pool, use M_sql_driver_pool_get_dpool() to get driver-specific
 *                      pool metadata.  Create Table is always executed against the primary subpool.
 * \param[in,out] query Query string to append suffix
 */
typedef void (*M_sql_driver_cb_createtable_suffix_t)(M_sql_connpool_t *pool, M_buf_t *query);


/*! Output the SQL-driver-specific update lock as needed.
 *
 * See M_sql_query_append_updlock() for more information.
 *
 *  \param[in]     pool       Pointer to connection pool object
 *  \param[in,out] query      Buffer to write sql-server-specific lock information into.
 *  \param[in]     type       mstdlib sql updlock type
 *  \param[in]     table_name Table name for "FOR UPDATE OF" style locks
 */
typedef void (*M_sql_driver_cb_append_updlock_t)(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name);


/*! Output the SQL-driver-specific bit operation formatted as needed.
 *
 * See M_sql_query_append_bitop() for more information.
 *
 *  \param[in]     pool       Pointer to connection pool object
 *  \param[in,out] query      Buffer to write sql-server-specific bitop into
 *  \param[in]     op         Bitwise operation to perform.
 *  \param[in]     exp1       Left-hand side of SQL expression.
 *  \param[in]     exp2       Right-hande size of SQL expression.
 *  \return M_TRUE on success, M_FALSE on misuse.
 */
typedef M_bool (*M_sql_driver_cb_append_bitop_t)(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2);


/*! Rewrite index identifier name to comply with database limitations.
 *
 *  For instance, Oracle versions prior to 12c R2 has a limitation of 30 characters
 *  for identifiers.  We need to rewrite the index name if the database version
 *  matches.
 *
 *  \param[in]     pool       Pointer to connection pool object
 *  \param[in]     index_name Desired index name
 *  \return NULL if no change necessary, otherwise rewritten index name.
 */
typedef char *(*M_sql_driver_cb_rewrite_indexname_t)(M_sql_connpool_t *pool, const char *index_name);


/*! Structure to be implemented by SQL drivers with information about the database in use */
typedef struct  {
	M_uint16                      driver_sys_version; /*!< Driver/Module subsystem version, use M_SQL_DRIVER_VERSION */
	const char                   *name;               /*!< Short name of module */
	const char                   *display_name;       /*!< Display name of module */
	const char                   *version;            /*!< Internal module version */

	/* NOTE: All callbacks are required to be registered by all drivers */
	M_sql_driver_cb_init_t               cb_init;               /*!< Required. Callback used for module initialization. */
	M_sql_driver_cb_destroy_t            cb_destroy;            /*!< Required. Callback used for module destruction/unloading. */
	M_sql_driver_cb_createpool_t         cb_createpool;         /*!< Required. Callback used for pool creation */
	M_sql_driver_cb_destroypool_t        cb_destroypool;        /*!< Required. Callback used for pool destruction */
	M_sql_driver_cb_connect_t            cb_connect;            /*!< Required. Callback used for connecting to the db */
	M_sql_driver_cb_serverversion_t      cb_serverversion;      /*!< Required. Callback used to get the server name/version string */
	M_sql_driver_cb_connect_runonce_t    cb_connect_runonce;    /*!< Optional. Callback used after connection is established, but before first query to set run-once options. */
	M_sql_driver_cb_disconnect_t         cb_disconnect;         /*!< Required. Callback used to disconnect from the db */
	M_sql_driver_cb_queryformat_t        cb_queryformat;        /*!< Required. Callback used for reformatting a query to the sql db requirements */
	M_sql_driver_cb_queryrowcnt_t        cb_queryrowcnt;        /*!< Required. Callback used for determining how many rows will be processed by the current execution (chunking rows) */
	M_sql_driver_cb_prepare_t            cb_prepare;            /*!< Required. Callback used for preparing a query for execution */
	M_sql_driver_cb_prepare_destroy_t    cb_prepare_destroy;    /*!< Required. Callback used to destroy the driver-specific prepared statement handle */
	M_sql_driver_cb_execute_t            cb_execute;            /*!< Required. Callback used for executing a prepared query */
	M_sql_driver_cb_fetch_t              cb_fetch;              /*!< Required. Callback used to fetch result data/rows from server */
	M_sql_driver_cb_begin_t              cb_begin;              /*!< Required. Callback used to begin a transaction */
	M_sql_driver_cb_rollback_t           cb_rollback;           /*!< Required. Callback used to rollback a transaction */
	M_sql_driver_cb_commit_t             cb_commit;             /*!< Required. Callback used to commit a transaction */
	M_sql_driver_cb_datatype_t           cb_datatype;           /*!< Required. Callback used to convert to data type for server */
	M_sql_driver_cb_createtable_suffix_t cb_createtable_suffix; /*!< Optional. Callback used to append additional data to the Create Table query string */
	M_sql_driver_cb_append_updlock_t     cb_append_updlock;     /*!< Optional. Callback used to append row-level locking data */
	M_sql_driver_cb_append_bitop_t       cb_append_bitop;       /*!< Required. Callback used to append a bit operation */
	M_sql_driver_cb_rewrite_indexname_t  cb_rewrite_indexname;  /*!< Optional. Callback used to rewrite an index name to comply with DB requirements */
	M_module_handle_t                    handle;                /*!< Handle for loaded driver - must be initialized to NULL in the driver structure */
} M_sql_driver_t;


/*! Flags for the helper query string format rewrite function M_sql_driver_queryformat() */
typedef enum {
	M_SQL_DRIVER_QUERYFORMAT_NORMAL                      = 0,      /*!< Normal, strips any query terminator otherwise unmodified */
	M_SQL_DRIVER_QUERYFORMAT_TERMINATOR                  = 1 << 0, /*!< Query terminator (;) is required */
	M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_DOLLAR            = 1 << 1, /*!< Instead of using ? for each bound parameter, parameters
	                                                                    take the form of $1, $2, ... $N  (used by PostgreSQL) */
	M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_COLON             = 1 << 2, /*!< Instead of using ? for each bound parameter, parameters
	                                                                    take the form of :1, :2, ... :N  (used by Oracle) */
	M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD         = 1 << 3, /*!< Multiple-value/row insertions are not sent to the server using
	                                                                    rows of bound parameters, but instead by comma-delimiting the
	                                                                    values in the insert statement.  This will rewrite an INSERT
	                                                                    statement from "INSERT INTO foo VALUES (?, ?, ?)"  into something
	                                                                    like "INSERT INTO foo VALUES (?, ?, ?), (?, ?, ?), ..., (?, ?, ?)" */
	M_SQL_DRIVER_QUERYFORMAT_INSERT_ONCONFLICT_DONOTHING = 1 << 4, /*!< Some databases may choose to abort the entire transaction on a 
	                                                                *   conflict, but there are times we explicitly want to take action
	                                                                *   on such a case without rolling back.  This clause will cause it to
	                                                                *   skip the insert of that record.  PostgreSQL is known to behave this
	                                                                *   way.  However, this means the result will not return said conflict
	                                                                *   so we must check to see if the return count is expected, and if
	                                                                *   not, rewrite the code to assume it is a conflict */
} M_sql_driver_queryformat_flags_t;


/*! Rewrite the user-provided query string to one more easily consumed by the database
 *  backend based on a series of flags.
 *
 *  This is a helper function to reduce code duplication in database implementations and
 *  is exclusively called by the drivers.  If the implementation here is insufficient for
 *  the requirements of the SQL server, then it is up to the driver to implement their own
 *  routine
 *
 *  \param[in] query      User-provided query string
 *  \param[in] flags      Bitmap of M_sql_driver_queryformat_flags_t Flags controlling behavior of processor
 *  \param[in] num_params Number of bound parameters (per row)
 *  \param[in] num_rows   For insert statements, number of rows of bound parameters
 *  \param[in] error      User-supplied error message buffer
 *  \param[in] error_size Size of user-supplied error message buffer
 *  \return Allocated buffer containing a rewritten query string or NULL on failure
 */
M_API char *M_sql_driver_queryformat(const char *query, M_uint32 flags, size_t num_params, size_t num_rows, char *error, size_t error_size);

/*! Connection string argument value allowed */
typedef enum {
	M_SQL_CONNSTR_TYPE_BOOL     = 1,
	M_SQL_CONNSTR_TYPE_NUM      = 2,
	M_SQL_CONNSTR_TYPE_ALPHA    = 3,
	M_SQL_CONNSTR_TYPE_ALPHANUM = 4,
	M_SQL_CONNSTR_TYPE_ANY      = 5
} M_sql_connstr_type_t;


/*! Structure defining possible connection string parameters to be passed to
 *  M_sql_driver_validate_connstr() to notify callers of possible typos */
struct M_sql_connstr_params {
	const char          *name;      /*!< Parameter name (case-insensitive) */
	M_sql_connstr_type_t type;      /*!< Data type of parameter */
	M_bool               required;  /*!< Whether or not the parameter is required */
	size_t               min_len;   /*!< Minimum length of parameter when present */
	size_t               max_len;   /*!< Maximum length of parameter when present */
};

/*! Typedef for struct M_sql_connstr_params */
typedef struct M_sql_connstr_params M_sql_connstr_params_t;

/*! Host/port used with M_sql_driver_parse_hostport() */
typedef struct {
	char     host[256];
	M_uint16 port;
} M_sql_hostport_t;


/*! Connection state tracking */
typedef enum {
	M_SQL_CONN_STATE_OK       = 1, /*!< Connection state is good */
	M_SQL_CONN_STATE_ROLLBACK = 2, /*!< A rollback condition has been hit, must be returned to the pool to be cleared */
	M_SQL_CONN_STATE_FAILED   = 3  /*!< The connection has failed, must be destroyed (return to the pool will do this) */
} M_sql_conn_state_t;

/*! Get the current connection state.
 *
 * \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 * \return connection state
 */
M_API M_sql_conn_state_t M_sql_conn_get_state(M_sql_conn_t *conn);


/*! Base helper used to execute a statement on a connection handle.
 *
 *  This helper is called by M_sql_stmt_execute() and M_sql_trans_execute()
 *
 *  \param[in] conn  Connection acquired with M_sql_connpool_acquireconn()
 *  \param[in] stmt  Prepared statement object to be executed
 *  \return one of the M_sql_error_t codes
 */
M_API M_sql_error_t M_sql_conn_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt);

/*! Base helper used to execute a simple query (no bound parameters) on a connection handle.
 *
 *  This internally generates a statement handle and destroys it upon completion.
 *
 *  \param[in]  conn               Connection acquired with M_sql_connpool_acquireconn()
 *  \param[in]  query              Direct query to be executed.
 *  \param[in]  skip_sanity_checks Skip sanity checks that may otherwise fail.  Usually used for injecting a stored procedure at db init.
 *  \return one of the M_sql_error_t codes
 */
M_API M_sql_stmt_t *M_sql_conn_execute_simple(M_sql_conn_t *conn, const char *query, M_bool skip_sanity_checks);

/*! Helper for SQL drivers to validate the connection strings provided.
 *
 * \param[in]  conndict   Dictionary of key/value pairs passed to driver
 * \param[in]  params     NULL-terminated structure of parameters to validate.
 * \param[out] error      User-supplied error buffer to output error message.
 * \param[in]  error_size Size of user-supplied error buffer.
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_sql_driver_validate_connstr(const M_hash_dict_t *conndict, const M_sql_connstr_params_t *params, char *error, size_t error_size);

M_API M_sql_hostport_t *M_sql_driver_parse_hostport(const char *hostport, M_uint16 default_port, size_t *out_len, char *error, size_t error_size);

/*! Retrieve a handle to the driver-specific connection object.
 *
 *  \param[in] conn Connection acquired with M_sql_connpool_acquireconn()
 *
 * \return handle to driver-specific connection object
 */
M_API M_sql_driver_conn_t *M_sql_driver_conn_get_conn(M_sql_conn_t *conn);
M_API M_sql_connpool_t *M_sql_driver_conn_get_pool(M_sql_conn_t *conn);
M_API const char *M_sql_driver_pool_get_username(M_sql_connpool_t *pool);
M_API const char *M_sql_driver_pool_get_password(M_sql_connpool_t *pool);
M_API const char *M_sql_driver_conn_get_username(M_sql_conn_t *conn);
M_API const char *M_sql_driver_conn_get_password(M_sql_conn_t *conn);
M_API M_sql_driver_connpool_t *M_sql_driver_pool_get_dpool(M_sql_connpool_t *pool);
M_API M_sql_driver_connpool_t *M_sql_driver_conn_get_dpool(M_sql_conn_t *conn);
M_API M_bool M_sql_driver_conn_is_readonly(M_sql_conn_t *conn);

/*! Return whether or not the connection is used within an SQL
 *  transaction, or simply a single standalone query.
 *
 * \param[in] conn Initialized #M_sql_conn_t
 * \return M_TRUE if used within a transaction, M_FALSE for single standalone query
 */
M_API M_bool M_sql_driver_conn_in_trans(M_sql_conn_t *conn);

M_API size_t M_sql_driver_conn_get_id(M_sql_conn_t *conn);
M_API const char *M_sql_driver_stmt_get_query(M_sql_stmt_t *stmt);
M_API M_sql_driver_stmt_t *M_sql_driver_stmt_get_stmt(M_sql_stmt_t *stmt);

/*! Retrieve remaining unprocessed row count.
 *
 *  Rows returned, and rows passed in may not accurately reflect the number
 *  of rows of parameters actually bound.  Some SQL servers do not support
 *  multiple rows being inserted in a single query, or may have a limit on
 *  how many rows can be inserted at once ... so this is the current 'view'
 *  that adjusts for rows that have already been processed so the sql driver
 *  doesn't have to track it
 *
 *  \param[in] stmt Initialized statement handle
 *  \return unprocessed rows remaining
 */
M_API size_t M_sql_driver_stmt_bind_rows(M_sql_stmt_t *stmt);

/*! Retrieve column count per row, or if a single row or a query that does not
 *  support multiple rows (e.g. select), the entire number of parameters bound.
 *
 *  This is NOT the number of bound params for multi-row binding, this is just
 *  the number of columns in a single row.
 *
 *  \param[in] stmt Initialized statement handle
 *  \return bind column count
 */
M_API size_t M_sql_driver_stmt_bind_cnt(M_sql_stmt_t *stmt);


M_API M_sql_data_type_t M_sql_driver_stmt_bind_get_type(M_sql_stmt_t *stmt, size_t row, size_t idx);

/*! Some columns with multiple rows might have a NULL data type bound with the wrong type,
 *  this searches for the "real" datatype, first non-null */
M_API M_sql_data_type_t M_sql_driver_stmt_bind_get_col_type(M_sql_stmt_t *stmt, size_t idx);

/*! Get the maximum size of a column if there are multiple rows bound, taking into account things like integer sizes */
M_API size_t M_sql_driver_stmt_bind_get_max_col_size(M_sql_stmt_t *stmt, size_t idx);

/*! Get the current size of the row/column in bytes, taking into account things like integer sizes */
M_API size_t M_sql_driver_stmt_bind_get_curr_col_size(M_sql_stmt_t *stmt, size_t row, size_t col);


/*! Get the requested row count as requested by the user by M_sql_stmt_set_max_fetch_rows().
 *
 *  This value can be used to set a Prefetch Row setting for receiving rows from the
 *  server as an optimization.  If a value of 0 is returned, this means the customer
 *  did not request partial fetching (user wants all rows), so the server might want
 *  to choose an internal default size.
 *
 * \param[in] stmt Initialized statement handle
 * \return rows requested per fetch, or 0 if user wants all rows
 */
M_API size_t M_sql_driver_stmt_get_requested_row_cnt(M_sql_stmt_t *stmt);

M_API M_bool *M_sql_driver_stmt_bind_get_bool_addr(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int16 *M_sql_driver_stmt_bind_get_int16_addr(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int32 *M_sql_driver_stmt_bind_get_int32_addr(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int64 *M_sql_driver_stmt_bind_get_int64_addr(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_bool M_sql_driver_stmt_bind_get_bool(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int16 M_sql_driver_stmt_bind_get_int16(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int32 M_sql_driver_stmt_bind_get_int32(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_int64 M_sql_driver_stmt_bind_get_int64(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API M_bool M_sql_driver_stmt_bind_isnull(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API const char *M_sql_driver_stmt_bind_get_text(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API size_t M_sql_driver_stmt_bind_get_text_len(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API const M_uint8 *M_sql_driver_stmt_bind_get_binary(M_sql_stmt_t *stmt, size_t row, size_t idx);
M_API size_t M_sql_driver_stmt_bind_get_binary_len(M_sql_stmt_t *stmt, size_t row, size_t idx);

/*! Set the number of affected rows from things like UPDATE or DELETE
 *
 * \param[in] stmt Statement handle
 * \param[in] cnt  Count to set
 * \return M_TRUE on succes, M_FALSE on failure such as misuse
 */
M_API M_bool M_sql_driver_stmt_result_set_affected_rows(M_sql_stmt_t *stmt, size_t cnt);


/*! Set the column count for the row headers
 *
 * \param[in] stmt Statement handle
 * \param[in] cnt  Count to set
 * \return M_TRUE on succes, M_FALSE on failure such as misuse or column count has already been set
 */
M_API M_bool M_sql_driver_stmt_result_set_num_cols(M_sql_stmt_t *stmt, size_t cnt);

/*! Sets the column header name for the specified column
 *
 *  Must only be called after M_sql_driver_stmt_result_set_num_cols()
 *
 * \param[in] stmt Statement handle
 * \param[in] col  Column to modify
 * \param[in] name Name to set
 * \return M_TRUE on success, or M_FALSE on failure such as misuse
 */
M_API M_bool M_sql_driver_stmt_result_set_col_name(M_sql_stmt_t *stmt, size_t col, const char *name);

/*! Sets the column header name for the specified column
 *
 *  Must only be called after M_sql_driver_stmt_result_set_num_cols()
 *
 * \param[in] stmt     Statement handle
 * \param[in] col      Column to modify
 * \param[in] type     Column type to set
 * \param[in] max_size Maximum size of column (for text or binary data), if available. 0 otherwise.
 * \return M_TRUE on success, or M_FALSE on failure such as misuse
 */
M_API M_bool M_sql_driver_stmt_result_set_col_type(M_sql_stmt_t *stmt, size_t col, M_sql_data_type_t type, size_t max_size);

/*! Start a new data column, returning writable buffer to hold column
 *  data.
 *
 *  The data written to the buffer is the Text or Binary version of the data.
 *
 *  \note ALL data except NULL columns must write at least a NULL terminator,
 *        even binary data requires a NULL terminator even though it won't be
 *        indicated in the final length.  Any fields added without at least
 *        a NULL terminator will be considered NULL fields.
 *
 *  The text version is also used for Integer and Boolean values.  If the column
 *  is NULL, do not write any data, not even a NULL terminator.
 *
 *  Must only be called after M_sql_driver_stmt_result_set_num_cols(), and highly
 *  recommended to have previously called M_sql_driver_stmt_result_set_col_name()
 *  and M_sql_driver_stmt_result_set_col_type().
 *
 *  Binary data can only be written if M_sql_driver_stmt_result_set_col_type() is
 *  set to #M_SQL_DATA_TYPE_BINARY.
 *
 *  \param[in] stmt Statement handle
 *  \return Allocated #M_buf_t to write data.  Or NULL on failure such as no more
 *          eligible columns for row.
 */
M_API M_buf_t *M_sql_driver_stmt_result_col_start(M_sql_stmt_t *stmt);

/*! Finish a row worth of data.
 *
 *  This is required to be called after all the columns for a row are written using
 *  M_sql_driver_stmt_result_col_start().
 *
 * \param[in] stmt Statement handle
 * \return M_TRUE on success, or M_FALSE on error, such as not all columns written.
 */
M_API M_bool M_sql_driver_stmt_result_row_finish(M_sql_stmt_t *stmt);

/*! Capabilities driver can use for M_sql_driver_append_updlock() helper */
typedef enum {
	M_SQL_DRIVER_UPDLOCK_CAP_NONE        = 0, /*!< No row-level locks supported */
	M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATE   = 1, /*!< FOR UPDATE style locks */
	M_SQL_DRIVER_UPDLOCK_CAP_MSSQL       = 2, /*!< Microsoft SQL Server style locks */
	M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATEOF = 3 /*!< FOR UPDATE, and FOR UPDATE OF (PostgreSQL) style locks */
} M_sql_driver_updlock_caps_t;


/*! Helper for drivers to implement M_sql_driver_cb_append_updlock_t
 *
 *  \param[in]     caps       Capabilities of SQL server
 *  \param[in,out] query      Buffer to write sql-server-specific lock information into.
 *  \param[in]     type       mstdlib sql updlock type
 *  \param[in]     table_name Table name for M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATEOF
 */
M_API void M_sql_driver_append_updlock(M_sql_driver_updlock_caps_t caps, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name);


/*! Bit Operations capabilities/type used by SQL server */
typedef enum {
	M_SQL_DRIVER_BITOP_CAP_OP               = 1, /*!< SQL server supports direct operators */
	M_SQL_DRIVER_BITOP_CAP_FUNC             = 2, /*!< SQL server supports BITOR and BITAND functions */
	M_SQL_DRIVER_BITOP_CAP_OP_CAST_BIGINT   = 3  /*!< SQL server supports direct operators, but needs exp2 input cast as BIGINT */
} M_sql_driver_bitop_caps_t;


/*! Helper for drivers to implement M_sql_driver_cb_append_bitop_t
 *
 *  \param[in]     caps       Capabilities of SQL server
 *  \param[in,out] query      Buffer to write sql-server-specific bitop into
 *  \param[in]     op         Bitwise operation to perform.
 *  \param[in]     exp1       Left-hand side of SQL expression.
 *  \param[in]     exp2       Right-hande size of SQL expression.
 *  \return M_TRUE on success, M_FALSE on misuse
 */
M_API M_bool M_sql_driver_append_bitop(M_sql_driver_bitop_caps_t caps, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2);


M_API M_sql_isolation_t M_sql_driver_str2isolation(const char *str);
M_API const char *M_sql_driver_isolation2str(M_sql_isolation_t type);

/*! Generate a driver-trace message.
 *
 *  Must pass either the pool or the connection handle so the trace system can
 *  look up the registered callback
 *
 * \param[in] is_debug  If M_TRUE, #M_SQL_TRACE_DRIVER_DEBUG is used, if M_FALSE, #M_SQL_TRACE_DRIVER_ERROR is used.
 * \param[in] pool      Conditional. If conn is not provided, must be populated.  The initialized pool handle.
 * \param[in] conn      Conditional. If pool is not provided, must be populated.  The initialized connection handle.
 * \param[in] err       Error code, possibly #M_SQL_ERROR_SUCCESS if not an error but a debug message.
 * \param[in] msg       Message to send to the trace callback
 */
M_API void M_sql_driver_trace_message(M_bool is_debug, M_sql_connpool_t *pool, M_sql_conn_t *conn, M_sql_error_t err, const char *msg);


#ifdef MSTDLIB_SQL_STATIC_MODULE
#  define M_SQL_API
#else
#  define M_SQL_API M_DLL_EXPORT
#endif


/*! Use in sql driver source file to create entry point
 *  \param[in] name is the name of the module, a M_sql_driver_t structure
 *             must be defined named  M_sql_[name]  */
#define M_SQL_DRIVER(name) \
	M_SQL_API M_sql_driver_t *M_sql_get_driver_##name(void); \
	M_sql_driver_t *M_sql_get_driver_##name(void)        \
	{                                                    \
		return &M_sql_##name;                            \
	}

/*! @} */

__END_DECLS

#endif /* __M_SQL_DRIVER_H__ */
