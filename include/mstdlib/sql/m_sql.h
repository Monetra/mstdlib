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

#ifndef __M_SQL_H__
#define __M_SQL_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sql_drivers SQL Drivers
 *  \ingroup m_sql
 *
 * SQL Drivers and Configuration Options
 */

/*! \addtogroup m_sql_driver_sqlite SQLite Driver
 *  \ingroup m_sql_drivers
 *
 * SQLite Driver
 *
 * Driver Name: sqlite
 *
 * Driver Connection String Options:
 *  - path: Required. Filesystem path to SQLite database.
 *  - journal_mode: Optional. Defaults to "WAL" if not specified, other
 *    options include "DELETE".
 *  - analyze: Optional. Defaults to "TRUE" if not specified.  On first connect,
 *    automatically runs an analyze to update index statistics if set to "TRUE".
 *  - integrity_check: Optional. Defaults to "FALSE" if not specified.  On first
 *    connect, automatically runs an integrity check to verify the database
 *    integrity if set to "TRUE".
 *  - shared_cache: Optional. Defaults to "TRUE" if not specified.  Enables
 *    shared cache mode for multiple connections to the same database.
 *  - autocreate: Optional. Defaults to "TRUE" if not specified. The default is
 *    to auto-create the database if not found, set this to "FALSE" to error if
 *    the database does not exist.
 */

/*! \addtogroup m_sql_driver_mysql MySQL/MariaDB Driver
 *  \ingroup m_sql_drivers
 *
 * MySQL/MariaDB Driver
 *
 * Driver Name: mysql
 *
 * Driver Connection String Options:
 *  - db: Required. Database Name.
 *  - socketpath: Conditional. If using Unix Domain Sockets to connect to MySQL,
 *    this is the path to the Unix Domain Socket.  Use the keyword of 'search'
 *    to search for the socket based on standard known paths.  Cannot be used 
 *    with host.
 *  - host: Conditional. If using IP or SSL/TLS to connect to MySQL, this is the
 *    hostname or IP address of the server.  If not using the default port of 
 *    3306, may append a ":port#" to the end of the host.  For specifying multiple
 *    hosts in a pool, hosts should be comma delimited. Cannot be used with socketpath.
 *    \code  host=10.40.30.2,10.50.30.2:13306  \endcode
 *  - ssl: Optional. Defaults to false, if true enables SSL/TLS to the server.
 *  - mysql_engine: Optional.  Used during table creation, defaults to INNODB.  The default
 *    data storage engine to use with mysql.  Typically it is recommended to leave
 *    this at the default.
 *  - mysql_charset: Optional.  Used during table creation, defaults to utfmb4.
 *  - max_isolation: Optional. Sets the maximum isolation level used for transactions.
 *    This is used to overwrite requests for SERIALIZABLE isolation levels,
 *    useful with Galera-based clusters that do not truly support Serializable
 *    isolation.  Should use "SELECT ... FOR UPDATE" type syntax for row locking.
 *    Available settings: "REPEATABLE READ", READ COMMITTED"
 */

/*! \addtogroup m_sql_driver_postgresql PostgreSQL Driver
 *  \ingroup m_sql_drivers
 *
 * PostgreSQL Driver
 *
 * Driver Name: postgresql
 *
 * Driver Connection String Options:
 *  - db: Required. Database Name.
 *  - host: Required. This is the hostname or IP address of the server.  If not using
 *    the default port of 5432, may append a ":port#" to the end of the host.  For specifying multiple
 *    hosts in a pool, hosts should be comma delimited. Cannot be used with socketpath.
 *    \code  host=10.40.30.2,10.50.30.2:15432  \endcode
 *  - application_name: Optional.  Application name to register with the server for debugging
 *    purposes.
 */

/*! \addtogroup m_sql_driver_oracle Oracle Driver
 *  \ingroup m_sql_drivers
 *
 * Oracle Driver
 *
 * Driver Name: oracle
 *
 * Driver Connection String Options:
 *  - dsn: Conditional. Data Source Name as specified in tnsnames.ora, or a fully
 *    qualified connection string.  If not specified, both host and service_name must both
 *    be specified and a connection string will be dynamically generated.  Use of this
 *    parameter negates the ability to use mstdlib's load balancing and failover logic,
 *    but facilitates the use of Oracle's equivalent functionality.
 *    An example of a fully qualified connection string would be:
 *    \code
 *      (DESCRIPTION =
 *        (ADDRESS = (PROTOCOL = TCP)(Host = 10.100.10.168)(Port = 1521))
 *        (CONNECT_DATA = (SERVICE_NAME = orcl))
 *      )
 *    \endcode
 *  - host: Conditional. If dsn is not specified, this parameter must be specified along
 *    with the service_name parameter.
 *    This is the hostname or IP address of the server.  If not using the default port
 *    of 1521, may append a ":port#" to the end of the host.  For specifying multiple
 *    hosts in a pool, hosts should be comma delimited.  Cannot be used with dsn.
 *    \code  host=10.40.30.2,10.50.30.2:11521  \endcode
 *  - service_name: Conditional. If dsn is not specified, this parameter must be specified
 *    along with the host parameter.  Cannot be used with dsn.
 *    \code service_name=orcl \endcode
 *
 *  Example with dsn:
 *  \code
 *  dsn='(DESCRIPTION =
 *        (ADDRESS = (PROTOCOL = TCP)(Host = 10.100.10.168)(Port = 1521))
 *        (CONNECT_DATA = (SERVICE_NAME = orcl))
 *      )'
 *  \endcode
 *
 *  Example without dsn:
 *  \code
 *  host=10.100.10.168;service_name=orcl
 *  \endcode
 */

/*! \addtogroup m_sql_driver_odbc ODBC and DB2 Driver
 *  \ingroup m_sql_drivers
 *
 * ODBC and DB2 Driver
 *
 * Driver Name(s):
 *   - odbc (for Microsoft Windows, iODBC, or UnixODBC)
 *   - db2 (for direct DB2 connectivity)
 *   - db2pase (for direct DB2 connectivity on OS/400 PASE)
 *
 * Driver Connection String Options:
 *  - dsn: Required. Data Source Name
 *  - mysql_engine: Optional.  Used during table creation when the destination
 *    database is MySQL, defaults to INNODB.  The default data storage engine
 *    to use with mysql.  Typically it is recommended to leave this at the
 *    default.
 *  - mysql_charset: Optional.  Used during table creation when the destination
 *    database is MySQL, defaults to utf8mb4.
 */

/*! \addtogroup m_sql_error SQL Error handling functions
 *  \ingroup m_sql
 *
 * SQL Error handling
 *
 * @{
 */

/*! Possible error conditions */
typedef enum {
	M_SQL_ERROR_SUCCESS              = 0,    /*!< No error, success. If returned by M_sql_stmt_fetch(), there
	                                          *   are guaranteed to not be any rows in the result set.  However,
	                                          *   for an M_sql_stmt_execute() or M_sql_trans_execute() if
	                                          *   M_sql_stmt_set_max_fetch_rows() was not set, there may be
	                                          *   rows available. */
	M_SQL_ERROR_SUCCESS_ROW          = 1,    /*!< No error, success, rows may be available to be fetched */

	/* Connectivity failures */
	M_SQL_ERROR_CONN_NODRIVER        = 100,  /*!< Driver not found for specified driver name. */
	M_SQL_ERROR_CONN_DRIVERLOAD      = 101,  /*!< Failed to dynamically load driver module. */
	M_SQL_ERROR_CONN_DRIVERVER       = 102,  /*!< Driver version invalid */
	M_SQL_ERROR_CONN_PARAMS          = 103,  /*!< Connection string parameter validation failed */
	M_SQL_ERROR_CONN_FAILED          = 104,  /*!< Failed to establish connection to server. */
	M_SQL_ERROR_CONN_BADAUTH         = 105,  /*!< Failed to authenticate against server. */
	M_SQL_ERROR_CONN_LOST            = 106,  /*!< Connection to server has been lost (remote disconnect). */

	/* Prepare errors */
	M_SQL_ERROR_PREPARE_INVALID      = 200,  /*!< Invalid query format */
	M_SQL_ERROR_PREPARE_STRNOTBOUND  = 201,  /*!< A string was detected in the query that was not bound */
	M_SQL_ERROR_PREPARE_NOMULITQUERY = 202,  /*!< Multiple requests in a single query are not allowed */

	/* Execute query */
	M_SQL_ERROR_QUERY_NOTPREPARED    = 300,  /*!< Can't execute query as statement hasn't been prepared */
	M_SQL_ERROR_QUERY_WRONGNUMPARAMS = 301,  /*!< Wrong number of bound parameters provided for query */
	M_SQL_ERROR_QUERY_PREPARE        = 302,  /*!< DB Driver failed to prepare the query for execution */

	/* Other errors */
	M_SQL_ERROR_QUERY_DEADLOCK       = 400,  /*!< Deadlock (must rollback), cannot continue. */
	M_SQL_ERROR_QUERY_CONSTRAINT     = 410,  /*!< Constraint failed (e.g. Unique key or primary key conflict) */
	M_SQL_ERROR_QUERY_FAILURE        = 499,  /*!< Failure (uncategorized) */



	/* Failure options for Disconnect */
	M_SQL_ERROR_INUSE                = 500,  /*!< Resource in use, invalid action */
	/* Generic Failures               */
	M_SQL_ERROR_INVALID_USE          = 600,  /*!< Invalid use */
	M_SQL_ERROR_INVALID_TYPE         = 601,  /*!< Invalid Data Type for conversion */

	/* User-generated errors or conditions via M_sql_trans_process() */
	M_SQL_ERROR_USER_SUCCESS         = 700,  /*!< Return code a User can generate in M_sql_trans_process() to 
	                                          *   Indicate the operation is complete and the system can
	                                          *   commit any pending data.  This is equivalent to #M_SQL_ERROR_SUCCESS
	                                          *   but can be used in its place if a user needs to have the
	                                          *   ability to differentiate how M_sql_trans_process() returned
	                                          *   success. */
	M_SQL_ERROR_USER_RETRY           = 701,  /*!< Return code a User can generate in M_sql_trans_process() to
	                                          *   request the system to rollback and retry the entire sequence
	                                          *   of events.  This is equivalent to #M_SQL_ERROR_QUERY_DEADLOCK
	                                          *   but more accurately indicates the failure was due to user-logic
	                                          *   rather than a condition triggered internally to the SQL system */
	M_SQL_ERROR_USER_FAILURE         = 702,  /*!< Return code a User can generate in M_sql_trans_process() to
	                                          *   request the system to rollback and return the error to the
	                                          *   caller.  This is equivalent to #M_SQL_ERROR_QUERY_FAILURE
	                                          *   but more accurately indicates the failure was due to user-logic
	                                          *   rather than a condition triggered internally to the SQL system */

	M_SQL_ERROR_UNSET                = 999   /*!< Error message not set. Internal use only. */
} M_sql_error_t;


/*! Retrieve generic error string associated with error code.
 *
 * Often the error message returned by the calling function or M_sql_stmt_get_error_string()
 * is more useful for human display purposes.
 *
 * \param[in] err Error to evaluate
 * \return string representation of error message.
 */
M_API const char *M_sql_error_string(M_sql_error_t err);

/*! Returns if error code is a failure or not.
 * 
 *  Currently this returns true if the error condition is any error other than
 *  #M_SQL_ERROR_SUCCESS or #M_SQL_ERROR_SUCCESS_ROW.
 *
 *  \param[in] err Error to evaluate
 *  \return M_TRUE if error, M_FALSE if not.
 */
M_API M_bool M_sql_error_is_error(M_sql_error_t err);

/*! Returns if the error code is due to a fatal communications error.
 *  If this occurs, the connection will be automatically destroyed and
 *  next use will try to establish a new connection
 *
 *  \param[in] err Error to evaluate.
 *  \return M_TRUE if connectivity failure, M_FALSE if not.
 */
M_API M_bool M_sql_error_is_disconnect(M_sql_error_t err);

/*! Returns if the error code represents a rollback condition.
 *
 *  There may be multiple types of failures that are rollback conditions such
 *  as unexpected disconnects from the database, deadlocks, and consistency
 *  failures.  This function checks for all known conditions where a rollback
 *  should be performed.
 *
 *  \param[in] err Error to evaluate.
 *  \return M_TRUE if rollback condition, M_FALSE if not.
 */
M_API M_bool M_sql_error_is_rollback(M_sql_error_t err);

/*! Returns if the error code represents a fatal error returned from the server
 *  that is unlikely to succeed if simply re-attempted.  Often this is the 
 *  result of a poorly formed query that can't be parsed or prepared.
 *
 *  Currently this is equivelent to:
 *  \code{.c}
 *    (M_sql_error_is_error(err) && !M_sql_error_is_rollback(err) && !M_sql_error_is_disconnect(err))
 *  \endcode
 *
 *  \param[in] err Error to evaluate.
 *  \return M_TRUE if fatal error, M_FALSE if not.
 */
M_API M_bool M_sql_error_is_fatal(M_sql_error_t err);


/*! @} */

/*! \addtogroup m_sql_conn SQL Connection Management
 *  \ingroup m_sql
 * 
 * SQL Connection Management
 *
 * @{
 */

struct M_sql_connpool;
/*! Connection pool object */
typedef struct M_sql_connpool M_sql_connpool_t;

/*! Flags controlling behavior of the connection pool */
typedef enum {
	M_SQL_CONNPOOL_FLAG_NONE               = 0,       /*!< No special pool flags */
	M_SQL_CONNPOOL_FLAG_PRESPAWN_ALL       = 1 << 0,  /*!< Pre-spawn all connections, not just the first. 
	                                                   *   Without this, the remaining connections are on-demand */
	M_SQL_CONNPOOL_FLAG_NO_AUTORETRY_QUERY = 1 << 1,  /*!< If a non-transactional query is rolled back due to a deadlock 
                                                       *   or connectivity failure, the default behavior is to automatically
                                                       *   retry the query, indefinitely.  For queries executed as part of
                                                       *   a transaction, rollbacks must be handled by the caller as they
                                                       *   may be dependent on prior queries in the transaction.  This flag
                                                       *   will turn off the auto-retry logic */
	M_SQL_CONNPOOL_FLAG_LOAD_BALANCE       = 1 << 2,  /*!< If there are multiple servers specified for the connection string,
	                                                   *   this will load balance requests across the servers instead of using
	                                                   *   them for failover. */
} M_sql_connpool_flags_t;


/*! Create an SQL connection pool.
 * 
 *  A connection pool is required to be able to run SQL transactions.  An internal
 *  connection is automatically claimed for a transaction or statement, or will
 *  wait on an available connection.
 *
 *  \note The pool is not started untile M_sql_connpool_start() is called, which must
 *  occur before the pool can be used by M_sql_stmt_execute() or M_sql_trans_begin().
 *
 *  \warning Pool modifications such as M_sql_connpool_add_readonly_pool() and M_sql_connpool_add_trace()
 *  must be called prior to M_sql_connpool_start().
 * 
 *  \param[out] pool               Newly initialized pool object
 *  \param[in]  driver             Name of driver to use. If the driver is not already loaded, will attempt
 *                                 to load the driver module automatically.  Driver modules are named
 *                                 mstdlib_sql_$driver.dll or mstdlib_sql_$driver.so as appropriate.
 *  \param[in]  conn_str           A driver-specific connection string or DSN.  This string often configures
 *                                 the host/port, and available options for the driver in use.  The connection
 *                                 strings are a set of key/value pairs, with keys seperated from the values
 *                                 with an equal sign (=), and values separated by a semi-colon (;).  If quoting
 *                                 is in use, a single-quote (') is recognized, and an escape character of a
 *                                 single quote (') can be used such that to use a real single quote, you would
 *                                 use two single quotes.  E.g. :
 *                                 \code host=10.130.40.5:3306;ssl=yes \endcode
 *                                 Please see the documentation for your driver for available configuration options.
 *  \param[in]  username           Connection username.
 *  \param[in]  password           Connection password.
 *  \param[in]  max_conns          Maximum number of SQL connections to attempt to create.  Valid range 1-1000.
 *  \param[in]  flags              Bitmap of #M_sql_connpool_flags_t options to configure.
 *  \param[out] error              Buffer to hold error message.
 *  \param[in]  error_size         Size of error buffer passed in.
 *  \return #M_SQL_ERROR_SUCCESS on successful pool creation, otherwise one of the #M_sql_error_t errors.
 */
M_API M_sql_error_t M_sql_connpool_create(M_sql_connpool_t **pool, const char *driver, const char *conn_str, const char *username, const char *password, size_t max_conns, M_uint32 flags, char *error, size_t error_size);


/*! Create a read-only pool attached to our already-created pool.
 * 
 *  The read-only pool will automatically route SELECT transactions, not part of a 
 *  transaction (e.g. not within a M_sql_trans_begin() ... M_sql_trans_commit() block)
 *  to the read-only pool.  This can be useful for report generation, where the data
 *  is coming from an asyncronous replication pool for reducing load on the master.
 *
 *  \note The caller can optionally use M_sql_stmt_set_master_only() to enforce routing of
 *        SELECT transactions to the read/write pool instead.
 *
 *  The read-only pool must share the same driver, username, password, and usage flags as
 *  specified via M_sql_connpool_create(), and must be called before M_sql_connpool_start().
 *
 *  Only a single read-only pool per pool object is allowed, repeated calls to this function
 *  will result in a failure.
 *
 *  \param[in]  pool               Initialized pool object by M_sql_connpool_create().
 *  \param[in]  conn_str           A driver-specific connection string or DSN.  This string often configures
 *                                 the host/port, and available options for the driver in use.  The connection
 *                                 strings are a set of key/value pairs, with keys seperated from the values
 *                                 with an equal sign (=), and values separated by a semi-colon (;).  If quoting
 *                                 is in use, a single-quote (') is recognized, and an escape character of a
 *                                 single quote (') can be used such that to use a real single quote, you would
 *                                 use two single quotes.  E.g. :
 *                                 \code host=10.130.40.5:3306;ssl=yes \endcode
 *                                 Please see the documentation for your driver for available configuration options.
 *  \param[in]  max_conns          Maximum number of SQL connections to attempt to create.  Valid range 1-1000.
 *  \param[out] error              Buffer to hold error message.
 *  \param[in]  error_size         Size of error buffer passed in.
 *  \return #M_SQL_ERROR_SUCCESS on successful readonly pool creation, otherwise one of the #M_sql_error_t errors.
 */
M_API M_sql_error_t M_sql_connpool_add_readonly_pool(M_sql_connpool_t *pool, const char *conn_str, size_t max_conns, char *error, size_t error_size);


/*! Set timeouts for connections on the pool.  Timeouts can be used to prevent
 *  stale connections from being used if known firewall timers expire, or to
 *  force reconnects to possibly rebalance connections across multiple servers.
 *
 *  Typically these should be set before M_sql_connpool_start() however it is 
 *  safe to change these on an active pool.
 *
 *  \param[in] pool              Initialized connection pool object
 *  \param[in] reconnect_time_s  How many seconds to allow a connection to be used before a disconnection
 *                               is forced.  The connection will be terminated even if not idle, termination
 *                               will occur when a connection is returned to the pool instead of prior to use
 *                               to prevent unexpected delays.  This can be used to either redistribute load
 *                               after a node failure when load balancing, or to fall back to a prior host.
 *                               Set to 0 for infinite, set to -1 to not change the current value.  Default is 0.
 *  \param[in] max_idle_time_s   Maximum amount of time a connection can have been idle to be used.  Some
 *                               firewalls may lose connection state after a given duration, so it may be
 *                               advisable to set this to below that threshold so the connection will be
 *                               forcibly terminated rather than use.  The connection will be terminated
 *                               before use and the consumer will attempt to grab a different connection
 *                               from the pool, or start a new one if none are available.  Set to 0 for infinite, 
 *                               set to -1 to not change the current value.  Default is 0.
 *  \param[in] fallback_s        Number of seconds when a connection error occurs to a host before it is
 *                               eligible for "fallback".  If this isn't set, the only time the first host
 *                               will be re-used is if the secondary host(s) also fail.  This should be used
 *                               in conjunction with reconnect_time_s. Set to 0 to never fallback, or -1 to
 *                               not change the current value.  Not relevant for load balancing, the host will
 *                               always be in the attempt pool. Default is 0.
 */
M_API void M_sql_connpool_set_timeouts(M_sql_connpool_t *pool, M_time_t reconnect_time_s, M_time_t max_idle_time_s, M_time_t fallback_s);


/*! Start the connection pool and make it ready for use.
 *
 *  At least one connection from the primary pool, and optionally the read-only pool will be
 *  started, controlled via the #M_SQL_CONNPOOL_FLAG_PRESPAWN_ALL flag.
 *
 *  If this returns a failure, either it can be attempted to be started again, or should be
 *  destroyed with M_sql_connpool_destroy().  No other functions are eligible for use after a failed start.
 *
 *  \note This must be called once prior to being able to use M_sql_stmt_execute() or M_sql_trans_begin(),
 *        but must be called after M_sql_connpool_add_readonly_pool() or M_sql_connpool_add_trace().
 *
 *  \param[in]  pool        Initialized pool object by M_sql_connpool_create().
 *  \param[out] error       Buffer to hold error message.
 *  \param[in]  error_size  Size of error buffer passed in.
 *  \return #M_SQL_ERROR_SUCCESS on successful readonly pool creation, otherwise one of the #M_sql_error_t errors.
 */
M_API M_sql_error_t M_sql_connpool_start(M_sql_connpool_t *pool, char *error, size_t error_size);


/*! Destroy the SQL connection pool and close all open connections.
 * 
 *  All connections must be idle/unused or will return a failure.
 *
 *  \param[in] pool  Pool object to be destroyed
 *  \return #M_SQL_ERROR_SUCCESS on successful pool destruction, otherwise one of the #M_sql_error_t errors.
 */
M_API M_sql_error_t M_sql_connpool_destroy(M_sql_connpool_t *pool);


/*! Count of active/connected SQL connections (but not ones that are in process of being brought online).
 * 
 *  \param[in] pool     Initialized pool object
 *  \param[in] readonly M_TRUE if querying for readonly connections, M_FALSE for primary
 * 
 *  \return count of active/connected SQL connections.
 */
M_API size_t M_sql_connpool_active_conns(M_sql_connpool_t *pool, M_bool readonly);

/*! SQL server name and version
 * 
 *  \param[in] pool  Initialized pool object
 * 
 *  \return SQL server name and version
 */
M_API const char *M_sql_connpool_server_version(M_sql_connpool_t *pool);

/*! SQL driver display (pretty) name
 *
 *  \param[in] pool  Initialized pool object
 * 
 *  \return SQL driver pretty name
 */
M_API const char *M_sql_connpool_driver_display_name(M_sql_connpool_t *pool);

/*! SQL driver internal/short name
 *
 *  \param[in] pool  Initialized pool object
 * 
 *  \return SQL driver internal/short name
 */
M_API const char *M_sql_connpool_driver_name(M_sql_connpool_t *pool);

/*! SQL driver version (not db version)
 *
 *  \param[in] pool  Initialized pool object
 * 
 *  \return SQL driver version (not db version)
 */
M_API const char *M_sql_connpool_driver_version(M_sql_connpool_t *pool);

/*! @} */


/*! \addtogroup m_sql_helpers SQL Helpers
 *  \ingroup m_sql
 * 
 * SQL Helpers for various situations
 *
 * @{
 */

/*! Generate a time-based + random unique id suitable for primary key use rather
 *  than using an auto-increment column. 
 *
 *  It is not recommended to use auto-increment columns for portability reasons,
 *  therefore a suitable unique id needs to be chosen that has a low probability
 *  of conflict.  There is no guarantee this key is unique, so integrators
 *  should handle conflicts by regenerating the key and re-attempting the
 *  operation (even though this may be an extremely low probability).
 *
 *  The generated key is a combination of the current timestamp in UTC and
 *  a random suffix.  The reason for a timestamp prefix is some databases 
 *  cannot handle purely random numbers as they cause index splits that cause
 *  exponential slowdown as the number of rows increase.  This can be observed
 *  with MySQL in particular.
 *
 *  Where possible, a 64bit (signed) column should be used for the unique id,
 *  which has a maximum length of 18 digits where all digits can contain any
 *  value.  A 32bit signed integer has a maximum of 9 digits where all digits
 *  can contain any value.  32bit integers are strongly discouraged as the possibility for
 *  conflict is much higher and would limit the total number of possible rows
 *  considerably (max 99,000 rows per day, but conflicts will be highly probable
 *  over a couple of thousand rows per day).
 *
 *  The current formats based on length are listed below:
 *    - 17-18+ digits : YYYJJJSSSSS{6-7R}
 *    - 16 digits     : YYJJJSSSSS{6R}
 *    - 14-15 digits  : YJJJSSSSS{5-6R}
 *    - 13 digits     : YJJJSSSS{6R}
 *    - 11-12 digits  : YJJJSS{6-7R}
 *    - 9-10 digits   : YJJJ{5-6R}
 *    - <9            : {1-8R}
 *
 *  Where:
 *    - Y = last digit of year
 *    - YY = last 2 digits of year
 *    - YYY = last 3 digits of year
 *    - JJJ = Julian day of year (0-365)
 *    - SSSSS = Second of day (0-86399)
 *    - SSSS  = Second of day divided by 10 (0-8639) (more fine grained than alternative of HHMM 0-2359)
 *    - SS    = Second of day divided by 1000 (0-86) (more fine grained than alternative of HH 0-23)
 *    - {\#R},{\#-\#R} = number of random digits
 *
 *  \note A time prefix is used solely for the purpose of reducing database load
 *        by making the values as incremental as possible, while still having a
 *        random portion to avoid conflicts.  These are not meant to be
 *        human-interpretable numbers, and formats may change in the future.  These
 *        should essentially appear completely random to a human.
 *
 *  \param[in] pool    Initialized pool object
 *  \param[in] max_len Length of unique id to generate in digits.  Valid range
 *                     is 9-18.  May return fewer digits only when the time
 *                     prefix begins with zero.
 *  \return 64bit signed integer representation of usable unique id, or 0 on
 *          misuse.
 */
M_API M_int64 M_sql_gen_timerand_id(M_sql_connpool_t *pool, size_t max_len);


/*! Random delay to use for a rollback to assist in preventing
 *  continual deadlocks and rollbacks.
 *
 *  A random delay is returned in milliseconds that can be used
 *  when a rollback condition is necessary to help break deadlock
 *  loops.
 *
 * \param[in] pool  Initialized pool object
 * \return delay to use in milliseconds.
 */
M_API M_uint64 M_sql_rollback_delay_ms(M_sql_connpool_t *pool);


/*! @} */



/*! \addtogroup m_sql_query SQL Query Extension/Portability Helpers
 *  \ingroup m_sql
 *
 * SQL Query Extension/Portability Helpers are used to assist in ensuring queries are portable across
 * various database servers utilizing extensions they offer.
 *
 * @{
 */

/*! Row lock type to append to query to lock rows returned from a select statement
 *  for a future update with in a transaction.  All values must be used within a
 *  single query */
typedef enum {
	M_SQL_QUERY_UPDLOCK_TABLE    = 1, /*!< Apply SQL-specific lock to rows in the table being updated. 
	                                   *   This must be appended immediately after every referenced table
	                                   *   name when row locking is desired.  Must be used in conjunction
	                                   *   with a later call for #M_SQL_QUERY_UPDLOCK_QUERYEND */
	M_SQL_QUERY_UPDLOCK_QUERYEND = 2  /*!< Apply the SQL-specific lock to the rows referenced by query, this
	                                   *   must always be applied at the END of a query string.  Must be
	                                   *   used in conjunction with an earlier call for #M_SQL_QUERY_UPDLOCK_TABLE */
} M_sql_query_updlock_type_t;


/*! Append the SQL-server-specific row lock method to the proper point in the query
 *  to be updated by a later call within the same transaction.
 *
 *  Row locks are intended to block conflicting select statements until the current
 *  transaction has completed.  It is an optimization to assist in reducing deadlocks
 *  which force rollback and retry cycles.  For some database clustering solutions,
 *  like MySQL/MariaDB with Galera, it is necessary to use to prevent lost updates since
 *  updates cross-node lack the serializable isolation level guarantees.
 *
 *  Different databases utilize different row locking methods and the methods appear
 *  at different points in the query.  Due to the complexity of SQL queries, it is not
 *  viable to offer automatic rewrite ability for such queries, and instead we provide
 *  methods for simply inserting the locking statements in a DB-specific way into your
 *  query.
 *
 *  Locking is for the duration of an SQL transaction, so row locking can only occur
 *  within a transaction, please see M_sql_trans_begin().
 *
 *  An example query that you want to lock the rows might look like:
 *    \code SELECT * FROM "foo" WHERE "bar" = ? \endcode
 *  For a row lock for Microsoft SQL Server, the desired query with locks would look like:
 *    \code SELECT * FROM "foo" WITH (ROWLOCK, XLOCK, HOLDLOCK) WHERE "bar" = ? \endcode
 *  For the equivalent on MySQL, it would look like this:
 *    \code SELECT * FROM "foo" WHERE "bar" = ? FOR UPDATE \endcode
 *
 *  Clearly as the above example indicates, it would be undesirable to need to rewrite
 *  the query manually by detecting the database in use, using helpers makes this
 *  easier so you do not need to have SQL-server-specific logic in your own code.  Converting
 *  that query above using the helpers could be done as the below:
 *  \code{.c}
 *    M_sql_stmt_t *stmt  = M_sql_stmt_create();
 *    M_buf_t      *query = M_buf_create();
 *    M_sql_error_t err;
 *
 *    M_buf_add_str(query, "SELECT * FROM \"foo\"");
 *    M_sql_query_append_updlock(pool, query, M_SQL_QUERY_UPDLOCK_TABLE);
 *    M_buf_add_str(query, " WHERE \"bar\" = ?");
 *    M_sql_stmt_bind_int32(stmt, 1);
 *    M_sql_query_append_updlock(pool, query, M_SQL_QUERY_UPDLOCK_QUERYEND);
 *    M_sql_stmt_prepare_buf(stmt, query);
 *    err = M_sql_stmt_execute(pool, stmt);
 *    //...
 *    M_sql_stmt_destroy(stmt);
 *  \endcode
 *
 *  \note At least one  #M_SQL_QUERY_UPDLOCK_TABLE must be appended per query.  They will
 *        be appended immediately after each table reference (SELECT FROM ... table, or
 *        JOIN table).  For the same query, at the end of the query, #M_SQL_QUERY_UPDLOCK_QUERYEND
 *        must be appended.
 *
 *  \warning Not all databases support row-lock hints and instead rely on consistency
 *           guarantees by the underlying database for the isolation method in use.  If you
 *           need these guarantees, please ensure you are using the #M_SQL_ISOLATION_SERIALIZABLE
 *           isolation method as well.
 *
 *  \param[in]     pool       Initialized #M_sql_connpool_t object
 *  \param[in,out] query      A pointer to an already populated M_buf_t with a partial (or complete
 *                            for #M_SQL_QUERY_UPDLOCK_QUERYEND) request.
 *  \param[in]     type       Type of sql-specific lock to append to the query.
 *  \param[in]     table_name Optional. For databases that support "FOR UPDATE OF" this will specify the explicit
 *                            table name to use.  If NULL, then will not emit the "OF" clause.  This may be necessary
 *                            for left outer joins on PostgreSQL.
 */
M_API void M_sql_query_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name);


/*! Type of bitwise operation to perform. */
typedef enum {
	M_SQL_BITOP_AND = 1, /*!< Perform a bitwise AND (&) operation */
	M_SQL_BITOP_OR  = 2  /*!< Perform a bitwise OR (|) operation */
} M_sql_query_bitop_t;


/*! Perform a bitwise operation on the database using an SQL-server-specific format.
 *
 *  A classic bitwise operation checking to see if a bit is set may look like the below:
 *  \code (exp1 & exp2) != 0 \endcode
 *
 *  Or a bitwise operation, setting a bit or set of bits may look like:
 *  \code exp1 = exp1 | exp2 \endcode
 *
 *  Some database servers take the expressions listed above exactly, however, others
 *  may require functions like BITAND() and BITOR() to accomplish the same thing.
 *
 *  Taking an example of selecting all rows where "bar" has bit 4 (0x8) set:
 *    \code SELECT * FROM "foo" WHERE ("bar" & 8) <> 0 \endcode
 *  Might look like this:
 *    \code{.c}
 *      M_buf_t      *buf  = M_buf_create();
 *      M_sql_stmt_t *stmt = M_sql_stmt_create();
 *      M_sql_error_t err;
 *
 *      M_buf_add_str(buf, "SELECT * FROM \"foo\" WHERE (");
 *      M_sql_query_append_bitop(pool, buf, M_SQL_BITOP_AND, "\"bar\"", "?");
 *      M_sql_stmt_bind_int32(stmt, 8);
 *      M_buf_add_str(buf, ") <> 0");
 *      M_sql_stmt_prepare_buf(stmt, buf);
 *      err = M_sql_stmt_execute(pool, stmt);
 *      //...
 *      M_sql_stmt_destroy(stmt);
 *    \endcode
 *
 *  Of course, more complex things are possible as well, such as unsetting bits and
 *  setting others in a single request by embedding operations within eachother.  Take
 *  the below example that keeps bits 2 (0x2) and 3 (0x4) while clearing the rest and
 *  also sets bit 4 (0x8):
 *    \code UPDATE "foo" SET "bar" = ( "bar" & 6 ) | 8; \endcode
 *  Might look like this:
 *    \code{.c}
 *      M_buf_t      *buf  = M_buf_create();
 *      M_sql_stmt_t *stmt = M_sql_stmt_create();
 *      M_sql_error_t err;
 *
 *      M_buf_add_str(buf, "UPDATE \"foo\" SET \"bar\" = ");
 *
 *        // Do inner-first ( "bar" & 6 )
 *        M_buf_t *inner = M_buf_create();
 *        M_buf_add_str(inner, "( ");
 *        M_sql_query_append_bitop(pool, inner, M_SQL_BITOP_AND, "\"bar\"", "?");
 *        M_sql_stmt_bind_int32(stmt, 6);
 *        M_buf_add_str(inner, " )");
 *
 *      // Do outer, embedding inner
 *      M_sql_query_append_bitop(pool, buf, M_SQL_BITOP_OR, M_buf_peek(inner), "?");
 *      M_sql_stmt_bind_int32(stmt, 8);
 *      M_buf_cancel(inner);  // We peeked, throw it away
 *
 *      M_sql_stmt_prepare_buf(stmt, buf);
 *      err = M_sql_stmt_execute(pool, stmt);
 *      //...
 *      M_sql_stmt_destroy(stmt);
 *    \endcode
 *
 *  \warning Most databases do not allow bitwise operations to be used for 'truth' values
 *           (e.g as a boolean).  Instead, an integrator should compare the result to 0
 *           to turn it into a boolean operation if needed.
 *
 *  \param[in]     pool       Initialized #M_sql_connpool_t object
 *  \param[in,out] query      A pointer to an already populated M_buf_t with a partial request.
 *  \param[in]     op         Bitwise operation to perform.
 *  \param[in]     exp1       Left-hand side of SQL expression.
 *  \param[in]     exp2       Right-hande size of SQL expression.
 *  \return M_TRUE on success, M_FALSE on misuse
 */ 
M_API M_bool M_sql_query_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2);


/*! @} */

__END_DECLS

#endif /* __M_SQL_H__ */
