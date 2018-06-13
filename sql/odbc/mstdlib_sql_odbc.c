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
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>

#if defined(ODBC_IBMDB2_PASE)
#  include <sqlcli.h>
#  define SQLLEN SQLINTEGER
#  define SQLULEN SQLUINTEGER
#elif defined(ODBC_IBMDB2)
#  include <sqlcli1.h>
#else
#  include <sql.h>
#  include <sqlext.h>
#  include <sqltypes.h>
#endif

#include "odbc_mssql.h"
#include "odbc_db2.h"
#include <postgresql_shared.h>
#include <oracle_shared.h>
#include <mysql_shared.h>

/* Don't use m_defs_int.h, since we need to be able to build this as an external plugin. */
#ifndef M_CAST_OFF_CONST
#define M_CAST_OFF_CONST(type, var) ((type)((M_uintptr)var))
#endif

#ifndef SQL_IS_INTEGER
#  define SQL_IS_INTEGER 0
#endif

/* DB2 Mappings */
#if !defined(SQL_C_SSHORT) && defined(SQL_C_SHORT)
#  define SQL_C_SSHORT SQL_C_SHORT
#endif
#if !defined(SQL_C_SBIGINT) && defined(SQL_C_BIGINT)
#  define SQL_C_SBIGINT SQL_C_BIGINT
#endif

typedef struct {
	SQLLEN            *lens;
	union {
		M_int8     *i8vals;
		M_int16    *i16vals;
		M_int32    *i32vals;
		M_int64    *i64vals;
		M_uint8    *pvalues;
	} data;
} odbc_bind_cols_t;

typedef enum {
	ODBC_CLEAR_NONE   = 0,
	ODBC_CLEAR_CURSOR = 1 << 0,
	ODBC_CLEAR_PARAMS = 1 << 1
} odbc_clear_type_t;

struct M_sql_driver_stmt {
	SQLHSTMT              stmt;                  /*!< ODBC Statement handle */
	odbc_clear_type_t     needs_clear;           /*!< if ODBC Statement Handle needs to be cleared before reuse */
	M_sql_driver_conn_t  *dconn;                 /*!< Pointer back to parent connection handle */

	odbc_bind_cols_t     *bind_cols;             /*!< Array binding columns/rows         */
	size_t                bind_cols_cnt;         /*!< Count of columns                   */
	SQLUSMALLINT         *bind_cols_status;      /*!< Array of status's, one per row     */
	SQLULEN               bind_params_processed; /*!< Output of how many param sets (rows) were processed */
	SQLLEN               *bind_flat_lens;        /*!< Array of lens for flat or comma-delimited style binding */
};


typedef M_sql_error_t (*M_sql_driver_cb_resolve_error_t)(const char *sqlstate, M_int32 errorcode);
typedef void (*M_sql_driver_cb_createtable_suffix_odbc_t)(M_sql_connpool_t *pool, M_hash_dict_t *settings, M_buf_t *query);


typedef struct {
	const char                               *name;                  /*!< SQL Server Name, used for matching (uses substring matching) */
	M_bool                                    is_multival_insert_cd; /*!< Uses comma-delimited multi-value insertion */
	size_t                                    max_insert_records;    /*!< Maximum number of records that can be inserted at once. 0=unlimited */
	size_t                                    unknown_size_ind;      /*!< Some DBs (PostgreSQL) use a length value to indicate a max or unknown size for results like 255 */

	M_sql_driver_cb_resolve_error_t           cb_resolve_error;      /*!< Required. Dereference server-specific error code */
	M_sql_driver_cb_connect_runonce_t         cb_connect_runonce;    /*!< Optional. Commands to execute on connect */
	M_sql_driver_cb_datatype_t                cb_datatype;           /*!< Required. Server-specific data types supported */
	M_sql_driver_cb_createtable_suffix_odbc_t cb_createtable_suffix; /*!< Optional. Append CREATE TABLE suffix */
	M_sql_driver_cb_append_updlock_t          cb_append_updlock;     /*!< Optional. Callback used to append row-level locking data */
	M_sql_driver_cb_append_bitop_t            cb_append_bitop;       /*!< Required. Callback used to append a bit operation */
	M_sql_driver_cb_rewrite_indexname_t       cb_rewrite_indexname;  /*!< Optional. Callback used to rewrite an index name to comply with DB requirements */
} odbc_server_profile_t;


typedef struct {
	char                       **dsns;
	size_t                       num_dsns;
	M_hash_dict_t               *settings;
	const odbc_server_profile_t *profile;
} odbc_connpool_data_t;


struct M_sql_driver_connpool {
	odbc_connpool_data_t primary;
	odbc_connpool_data_t readonly;
};


struct M_sql_driver_conn {
	odbc_connpool_data_t        *pool_data;
	SQLHDBC                      dbc_handle;
	char                         dbms_name[256];
	char                         dbms_ver[256];
	char                         version[256]; /* dbms_name || dbms_ver */
};


static SQLHENV odbc_env_handle         = SQL_NULL_HENV;

static const odbc_server_profile_t odbc_server_profiles[] = {
	{ 
		"Microsoft SQL Server",       /* name                  */
		M_FALSE,                      /* is_multival_insert_cd */
		0,                            /* max_insert_records    */
		0,                            /* unknown_size_ind      */
		mssql_resolve_error,          /* cb_resolve_error      */
		mssql_cb_connect_runonce,     /* cb_connect_runonce    */
		mssql_cb_datatype,            /* cb_datatype           */
		NULL,                         /* cb_createtable_suffix */
		mssql_cb_append_updlock,      /* cb_append_updlock     */
		mssql_cb_append_bitop,        /* cb_append_bitop       */
		NULL                          /* cb_rewrite_indexname  */
	},

	{ 
		"DB2",                        /* name                  */
		M_FALSE,                      /* is_multival_insert_cd */
		0,                            /* max_insert_records    */
		0,                            /* unknown_size_ind      */
		db2_resolve_error,            /* cb_resolve_error      */
		NULL,                         /* cb_connect_runonce    */
		db2_cb_datatype,              /* cb_datatype           */
		NULL,                         /* cb_createtable_suffix */
		db2_cb_append_updlock,        /* cb_append_updlock     */
		db2_cb_append_bitop,          /* cb_append_bitop       */
		NULL                          /* cb_rewrite_indexname  */
	},

	{ 
		"ORACLE",                     /* name                  */
		M_FALSE,                      /* is_multival_insert_cd */
		0,                            /* max_insert_records    */
		0,                            /* unknown_size_ind      */
		oracle_resolve_error,         /* cb_resolve_error      */
		oracle_cb_connect_runonce,    /* cb_connect_runonce    */
		oracle_cb_datatype,           /* cb_datatype           */
		NULL,                         /* cb_createtable_suffix */
		oracle_cb_append_updlock,     /* cb_append_updlock     */
		oracle_cb_append_bitop,       /* cb_append_bitop       */
		oracle_cb_rewrite_indexname   /* cb_rewrite_indexname  */
	},

	{ 
		"MYSQL",                      /* name                  */
		M_TRUE,                       /* is_multival_insert_cd */
		100,                          /* max_insert_records    */
		0,                            /* unknown_size_ind      */
		mysql_resolve_error,          /* cb_resolve_error      */
		mysql_cb_connect_runonce,     /* cb_connect_runonce    */
		mysql_cb_datatype,            /* cb_datatype           */
		mysql_createtable_suffix,     /* cb_createtable_suffix */
		mysql_cb_append_updlock,      /* cb_append_updlock     */
		mysql_cb_append_bitop,        /* cb_append_bitop       */
		NULL                          /* cb_rewrite_indexname  */
	},

	{ 
		"MariaDB",                    /* name                  */
		M_TRUE,                       /* is_multival_insert_cd */
		100,                          /* max_insert_records    */
		0,                            /* unknown_size_ind      */
		mysql_resolve_error,          /* cb_resolve_error      */
		mysql_cb_connect_runonce,     /* cb_connect_runonce    */
		mysql_cb_datatype,            /* cb_datatype           */
		mysql_createtable_suffix,     /* cb_createtable_suffix */
		mysql_cb_append_updlock,      /* cb_append_updlock     */
		mysql_cb_append_bitop,        /* cb_append_bitop       */
		NULL                          /* cb_rewrite_indexname  */
	},

	{ 
		"PostgreSQL",                 /* name                  */
		M_TRUE,                       /* is_multival_insert_cd */
		100,                          /* max_insert_records    */
		255,                          /* unknown_size_ind      */
		pgsql_resolve_error,          /* cb_resolve_error      */
		pgsql_cb_connect_runonce,     /* cb_connect_runonce    */
		pgsql_cb_datatype,            /* cb_datatype           */
		NULL,                         /* cb_createtable_suffix */
		pgsql_cb_append_updlock,      /* cb_append_updlock     */
		pgsql_cb_append_bitop,        /* cb_append_bitop       */
		NULL                          /* cb_rewrite_indexname  */
	},

	{ NULL, M_FALSE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};


static const char *odbc_rc2str(int rc)
{
	switch (rc) {
		case SQL_SUCCESS:
			return "SUCCESS";
		case SQL_SUCCESS_WITH_INFO:
			return "SUCCESS_WITH_INFO";
		case SQL_NEED_DATA:
			return "SQL_NEED_DATA";
		case SQL_STILL_EXECUTING:
			return "SQL_STILL_EXECUTING";
		case SQL_ERROR:
			return "SQL_ERROR";
		case SQL_NO_DATA:
			return "SQL_NO_DATA";
		case SQL_INVALID_HANDLE:
			return "SQL_INVALID_HANDLE";
	}
	return "UKNOWN_RETURN_CODE";
}


static M_sql_error_t odbc_err_to_error(M_sql_driver_conn_t *dconn, SQLINTEGER errorcode, const char *sqlstate)
{
	size_t i;
	const struct {
		const char   *sqlstate;
		M_sql_error_t err;
	} sqlstate_map[] = {
		{ "08S01", M_SQL_ERROR_CONN_LOST        },
		{ "08007", M_SQL_ERROR_CONN_LOST        },
		{ "HYT00", M_SQL_ERROR_CONN_LOST        }, /* Transaction Timeout */
		{ "HYT01", M_SQL_ERROR_CONN_LOST        }, /* Connection Timeout */
		{ "40000", M_SQL_ERROR_QUERY_DEADLOCK   }, /* Transaction Rollback */
		{ "40001", M_SQL_ERROR_QUERY_DEADLOCK   }, /* Serialization Failure */
		{ "23000", M_SQL_ERROR_QUERY_CONSTRAINT }, /* Integrity constraint violation */
		{ "40002", M_SQL_ERROR_QUERY_CONSTRAINT }, /* TRANSACTION INTEGRITY CONSTRAINT VIOLATION */
		{ "40003", M_SQL_ERROR_QUERY_DEADLOCK   }, /* STATEMENT COMPLETION UNKNOWN */
		{ "40P01", M_SQL_ERROR_QUERY_DEADLOCK   }, /* DEADLOCK DETECTED */
		{ "25S03", M_SQL_ERROR_QUERY_DEADLOCK   }, /* Transaction is rolled back */
		{ NULL, 0 }
	};

	for (i=0; sqlstate_map[i].sqlstate != NULL; i++) {
		if (M_str_caseeq(sqlstate_map[i].sqlstate, sqlstate))
			return sqlstate_map[i].err;
	}

	if (dconn == NULL)
		return M_SQL_ERROR_CONN_FAILED;

	if (dconn != NULL && dconn->pool_data->profile != NULL && dconn->pool_data->profile->cb_resolve_error != NULL)
		return dconn->pool_data->profile->cb_resolve_error(sqlstate, errorcode);

	return M_SQL_ERROR_QUERY_FAILURE;
}


static void odbc_sanitize_error(char *error)
{
	if (M_str_isempty(error))
		return;

	M_str_replace_chr(error, '\n', ' ');
	M_str_replace_chr(error, '\r', ' ');
	M_str_replace_chr(error, '\t', ' ');
}


/*! Format an ODBC Error Message and return a more specific error code if available
 *
 *  \param[in]  msg_prefix Prefix to prepend to the front of a formatted error message
 *  \param[in]  dconn      Pointer to driver connection handle.  If NULL, will attempt
 *                         to use error from environment handle.
 *  \param[in]  dstmt      Pointer to driver statement handle.  If NULL, will attempt to
 *                         use error from connection or environment handle.
 *  \param[in]  rc         Return code as passed from command that generated the error.
 *  \param[out] error      User-supplied buffer to hold error message.
 *  \param[in]  error_size Size of user-supplied error buffer.
 *  \return One of the !M_sql_error_t error conditions as mapped from the error code
 */
static M_sql_error_t odbc_format_error(const char *msg_prefix, M_sql_driver_conn_t *dconn, M_sql_driver_stmt_t *dstmt, SQLRETURN rc, char *error, size_t error_size)
{
	M_sql_error_t err      = M_SQL_ERROR_CONN_DRIVERLOAD;
	SQLHANDLE     hnd      = odbc_env_handle;
	SQLSMALLINT   hnd_type = SQL_HANDLE_ENV;
	int           err_rc   = SQL_SUCCESS;
	char         *msg;
	M_buf_t      *buf      = NULL;
	size_t        i;

	M_mem_set(error, 0, error_size);

	if (dstmt != NULL) {
		dconn    = dstmt->dconn;
		hnd      = dstmt->stmt;
		hnd_type = SQL_HANDLE_STMT;
		err      = M_SQL_ERROR_QUERY_FAILURE; /* Generic */
	} else if (dconn != NULL) {
		hnd      = dconn->dbc_handle;
		hnd_type = SQL_HANDLE_DBC;
		err      = M_SQL_ERROR_CONN_FAILED; /* Generic failed to connect */
	}

	buf = M_buf_create();
	for (i=1; hnd != SQL_NULL_HENV; i++) {
		char           sqlstate[6];
		SQLINTEGER     errorcode     = 0;
		char           errortext[256];
		SQLSMALLINT    errortext_len = 0;
		M_sql_error_t  myerr;

		M_mem_set(errortext, 0, sizeof(errortext));
		M_mem_set(sqlstate,  0, sizeof(sqlstate));

		err_rc = SQLGetDiagRec(hnd_type, hnd, (SQLSMALLINT)i, (unsigned char *)sqlstate,
		                       &errorcode, (SQLCHAR *)errortext,
		                       (SQLSMALLINT)(sizeof(errortext)-1), &errortext_len);
		if (err_rc != SQL_SUCCESS && err_rc != SQL_SUCCESS_WITH_INFO)
			break;

		/* Get rid of characters we don't want and kill leading/trailing whitespace from message */
		odbc_sanitize_error(errortext);
		M_str_trim(errortext);

		/* If already have some error text, make sure it ends with ". " */
		if (M_buf_len(buf)) {
			if (M_buf_peek(buf)[M_buf_len(buf)-1] != '.') {
				M_buf_add_byte(buf, '.');
			}
			M_buf_add_byte(buf, ' ');
		}

		/* Append to error buffer */
		M_bprintf(buf, "%s(%d): %s", sqlstate, (int)errorcode, errortext);

		myerr = odbc_err_to_error(dconn, errorcode, sqlstate);

		/* First loop around, always use error */
		if (i == 1) {
			err = myerr;
		} else {
			/* Now we're looking for a "better" error condition, as we've
			 * seen sometimes there are multiple diag records and the one
			 * that indicates the real cause was a connectivity failure isn't
			 * output until later.
			 */
			switch (myerr) {
				/* These are clearly bogus if we're in an error condition */
				case M_SQL_ERROR_SUCCESS:
				case M_SQL_ERROR_SUCCESS_ROW:
					break;

				/* Errors considered too generic */
				case M_SQL_ERROR_QUERY_FAILURE:
					break;

				/* Most likely this is a more specific error we want */
				default:
					err = myerr;
					break;
			}
		}

	}

	msg = M_buf_finish_str(buf, NULL);
	if (msg == NULL) {
		M_snprintf(error, error_size, "%s: %s(%d)", msg_prefix, odbc_rc2str(rc), rc);
	} else {
		M_snprintf(error, error_size, "%s: %s(%d): %s", msg_prefix, odbc_rc2str(rc), rc, msg);
		M_free(msg);
	}

	return err;
}


static void odbc_cb_destroy(void)
{
	if (odbc_env_handle != SQL_NULL_HENV)
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_env_handle);
	odbc_env_handle = SQL_NULL_HENV;
}


static M_bool odbc_cb_init(char *error, size_t error_size)
{
	SQLRETURN rc;

	rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc_env_handle);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		odbc_format_error("SQLAllocHandle(SQL_HANDLE_ENV) failed", NULL, NULL, rc, error, error_size);
		return M_FALSE;
	}


#ifdef SQL_ATTR_ODBC_VERSION
	rc = SQLSetEnvAttr(odbc_env_handle, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		odbc_format_error("SQLSetEnvAttr(SQL_ATTR_ODBC_VERSION) failed", NULL, NULL, rc, error, error_size);
		odbc_cb_destroy();
		return M_FALSE;
	}
#else
	/* SQL driver does not support setting SQL_ATTR_ODBC_VERSION */
#endif

	return M_TRUE;
}


static M_bool odbc_connpool_readconf(odbc_connpool_data_t *data, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	M_sql_connstr_params_t params[] = {
		{ "dsn",             M_SQL_CONNSTR_TYPE_ANY,      M_TRUE,   1,  2048 },
		{ "mysql_engine",    M_SQL_CONNSTR_TYPE_ALPHA,    M_FALSE,  1,    31 },
		{ "mysql_charset",   M_SQL_CONNSTR_TYPE_ALPHANUM, M_FALSE,  1,    31 },
		{ NULL, 0, M_FALSE, 0, 0}
	};
	const char *dsn;

	if (!M_sql_driver_validate_connstr(conndict, params, error, error_size)) {
		return M_FALSE;
	}

	dsn         = M_hash_dict_get_direct(conndict, "dsn");
	data->dsns  = M_str_explode_str(',', dsn, &data->num_dsns);
	if (data->dsns == NULL || data->num_dsns == 0 || M_str_isempty(data->dsns[0]))
		return M_FALSE;

	*num_hosts     = data->num_dsns;
	data->settings = M_hash_dict_duplicate(conndict);

	return M_TRUE;
}


static M_bool odbc_cb_createpool(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	odbc_connpool_data_t  *data;
	(void)pool;

	if (*dpool == NULL) {
		*dpool = M_malloc_zero(sizeof(**dpool));
	}

	data = is_readonly?&(*dpool)->readonly:&(*dpool)->primary;

	return odbc_connpool_readconf(data, conndict, num_hosts, error, error_size);
}


static void odbc_cb_destroypool(M_sql_driver_connpool_t *dpool)
{
	if (dpool == NULL)
		return;

	M_str_explode_free(dpool->primary.dsns, dpool->primary.num_dsns);
	M_str_explode_free(dpool->readonly.dsns, dpool->readonly.num_dsns);
	M_hash_dict_destroy(dpool->primary.settings);
	M_hash_dict_destroy(dpool->readonly.settings);
	M_free(dpool);
}


static void odbc_cb_disconnect(M_sql_driver_conn_t *conn)
{
	if (conn == NULL)
		return;

	if (conn->dbc_handle != SQL_NULL_HDBC) {
		SQLDisconnect(conn->dbc_handle);
		SQLFreeHandle(SQL_HANDLE_DBC, conn->dbc_handle);
	}

	M_free(conn);
}


static M_sql_error_t odbc_cb_connect(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size)
{
	M_sql_driver_connpool_t     *dpool        = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t        *data         = is_readonly_pool?&dpool->readonly:&dpool->primary;
	const odbc_server_profile_t *profile      = NULL;
	M_sql_error_t                err          = M_SQL_ERROR_SUCCESS;
	SQLRETURN                    rc;
	const char                  *username;
	const char                  *password;
	M_buf_t                     *dsn          = NULL;
	char                        *connstr;
	SQLSMALLINT                  outlen; /* Junk value */
	size_t                       i;
#if 0
	SQLINTEGER                   auto_pop_ipd = SQL_FALSE;
#endif

	*conn              = M_malloc_zero(sizeof(**conn));
	(*conn)->pool_data = data;

	/* Initialize DBC Handle */
	rc = SQLAllocHandle(SQL_HANDLE_DBC, odbc_env_handle, &(*conn)->dbc_handle);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLAllocHandle(SQL_HANDLE_DBC) failed", NULL, NULL, rc, error, error_size);
		goto done;
	}

#ifdef SQL_ATTR_ODBC_CURSORS /* DB2 PASE doesn't appear to have this */
	/* Only use cursors if needed - must be set before connect */
	rc = SQLSetConnectAttr((*conn)->dbc_handle, SQL_ATTR_ODBC_CURSORS, (SQLPOINTER)SQL_CUR_USE_IF_NEEDED, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetConnectAttr(SQL_ATTR_ODBC_CURSORS=SQL_CUR_USE_IF_NEEDED) failed", *conn, NULL, rc, error, error_size);
		goto done;
	}
#endif

	/* Generate connection string */
	username = M_sql_driver_pool_get_username(pool);
	password = M_sql_driver_pool_get_password(pool);

	dsn      = M_buf_create();
	M_bprintf(dsn, "DSN=%s;", data->dsns[host_idx]);

	if (!M_str_isempty(username)) {
		M_bprintf(dsn, "UID=%s;", username);
	}

	if (!M_str_isempty(password)) {
		M_bprintf(dsn, "PWD=%s;", password);
	}

	connstr = M_buf_finish_str(dsn, NULL);

	/* Connect */
	rc = SQLDriverConnect((*conn)->dbc_handle, NULL, (SQLCHAR *)connstr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	M_free(connstr);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		char prefix[256];
		M_snprintf(prefix, sizeof(prefix), "SQLDriverConnect(DSN=%s) failed", data->dsns[host_idx]);
		err = odbc_format_error(prefix, *conn, NULL, rc, error, error_size);
		goto done;
	}

	/* Make sure its started with autocommit on, since we don't start out in a transaction */
	rc = SQLSetConnectAttr((*conn)->dbc_handle, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT=SQL_AUTOCOMMIT_ON) failed", *conn, NULL, rc, error, error_size);
		goto done;
	}

	/* Default to read-committed transactions, not very restrictive */
	rc = SQLSetConnectAttr((*conn)->dbc_handle, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_READ_COMMITTED, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetConnectAttr(SQL_ATTR_TXN_ISOLATION=SQL_TXN_READ_COMMITTED) failed", *conn, NULL, rc, error, error_size);
		goto done;
	}

/* We no longer need to derive the data type as we aren't doing NULL insertion trickery anymore */
#if 0
	/* Determine if the driver supports automatic IPD (ability to get parameter data types) population */
	rc = SQLGetConnectAttr((*conn)->dbc_handle, SQL_ATTR_AUTO_IPD, (SQLPOINTER)&auto_pop_ipd, SQL_IS_UINTEGER, NULL);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		/* NOTE: FreeTDS returns an error trying to retrieve this attribute, just assume not supported
		 * err = odbc_format_error("SQLGetConnectAttr(SQL_ATTR_AUTO_IPD) failed", *conn, NULL, rc, error, error_size);
		 * goto done;
		 */
		auto_pop_ipd = SQL_FALSE;
	}

	if (auto_pop_ipd == SQL_TRUE) {
#ifdef SQL_ATTR_ENABLE_AUTO_IPD /* DB2 PASE doesn't appear to have this */
		/* Driver supports automatic IPD population, enable it! */
		rc = SQLSetConnectAttr((*conn)->dbc_handle, SQL_ATTR_ENABLE_AUTO_IPD, (SQLPOINTER)SQL_TRUE, SQL_IS_UINTEGER);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
			M_sql_error_t myerr;
			char          myerror[256];
			myerr = odbc_format_error("SQLSetConnectAttr(SQL_ATTR_ENABLE_AUTO_IPD=SQL_TRUE) failed", *conn, NULL, rc, myerror, sizeof(myerror));
			M_sql_driver_trace_message(M_FALSE /* Not debug */, pool, NULL, myerr, myerror);

			/* Lets not treat this as a hard failure
			 * goto done;
			 */
		}
#endif
	}
#endif

	/* Grab database name */
	rc = SQLGetInfo((*conn)->dbc_handle, SQL_DBMS_NAME, (*conn)->dbms_name, sizeof((*conn)->dbms_name)-1, &outlen);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLGetInfo(SQL_DBMS_NAME) failed", *conn, NULL, rc, error, error_size);
		goto done;
	}

	/* Grab database version */
	rc = SQLGetInfo((*conn)->dbc_handle, SQL_DBMS_VER, (*conn)->dbms_ver, sizeof((*conn)->dbms_ver)-1, &outlen);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLGetInfo(SQL_DBMS_VER) failed", *conn, NULL, rc, error, error_size);
		goto done;
	}

	/* Concatenate them for a real version */
	M_snprintf((*conn)->version, sizeof((*conn)->version), "%s %s", (*conn)->dbms_name, (*conn)->dbms_ver);

	for (i=0; odbc_server_profiles[i].name != NULL; i++) {
		if (M_str_casestr((*conn)->dbms_name, odbc_server_profiles[i].name) != NULL ||
			M_str_casestr((*conn)->dbms_ver,  odbc_server_profiles[i].name) != NULL /* iODBC sometimes puts the database name in the version */
			) {
			profile = &odbc_server_profiles[i];
			break;
		}
	}

	if (profile == NULL) {
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		M_snprintf(error, error_size, "No matching profile for server type (unsupported): %s", (*conn)->version);
		goto done;
	}

	/* No lock necessary as first connection is brought up syncronously */
	if (data->profile == NULL) {
		data->profile = profile;
	} else if (data->profile != profile) {
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		M_snprintf(error, error_size, "profile of server doesn't match prior profile");
		goto done;
	}

	/* Sanity check for read-only pool bring-up.  Must use same server type */
	if (is_readonly_pool && dpool->primary.profile != profile) {
		err = M_SQL_ERROR_CONN_DRIVERLOAD;
		M_snprintf(error, error_size, "profile of readonly pool doesn't match that of primary pool");
		goto done;
	}

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		odbc_cb_disconnect(*conn);
		*conn = NULL;
		return err;
	}

	return M_SQL_ERROR_SUCCESS;
}


static const char *odbc_cb_serverversion(M_sql_driver_conn_t *conn)
{
	return conn->version;
}


static M_sql_error_t odbc_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	if (!dconn->pool_data->profile->cb_connect_runonce)
		return M_SQL_ERROR_SUCCESS;

	return dconn->pool_data->profile->cb_connect_runonce(conn, dpool, is_first_in_pool, is_readonly, error, error_size);
}


static char *odbc_cb_queryformat(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	M_sql_driver_conn_t             *dconn = M_sql_driver_conn_get_conn(conn);
	M_sql_driver_queryformat_flags_t flags = M_SQL_DRIVER_QUERYFORMAT_NORMAL;

	if (dconn->pool_data->profile->is_multival_insert_cd)
		flags |= M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD;

	if (dconn->pool_data->profile->max_insert_records)
		num_rows = M_MIN(num_rows, dconn->pool_data->profile->max_insert_records);

	return M_sql_driver_queryformat(query, flags, num_params, num_rows, error, error_size);
}


static void odbc_clear_driver_stmt(M_sql_driver_stmt_t *dstmt)
{
	size_t i;

	if (dstmt->bind_cols) {
		for (i=0; i<dstmt->bind_cols_cnt; i++) {
			M_free(dstmt->bind_cols[i].lens);
			M_free(dstmt->bind_cols[i].data.pvalues); /* Pointer base address is same for all */
		}
		M_free(dstmt->bind_cols);
	}
	M_free(dstmt->bind_cols_status);
	M_free(dstmt->bind_flat_lens);

	dstmt->bind_cols             = NULL;
	dstmt->bind_cols_cnt         = 0;
	dstmt->bind_cols_status      = NULL;
	dstmt->bind_params_processed = 0;
	dstmt->bind_flat_lens        = NULL;

	/* Prepare for re-use */
	if (dstmt->needs_clear & ODBC_CLEAR_CURSOR)
		SQLFreeStmt(dstmt->stmt, SQL_CLOSE);
	if (dstmt->needs_clear & ODBC_CLEAR_PARAMS)
		SQLFreeStmt(dstmt->stmt, SQL_RESET_PARAMS);

	dstmt->needs_clear      = ODBC_CLEAR_NONE;
}


static void odbc_cb_prepare_destroy(M_sql_driver_stmt_t *dstmt)
{
	if (dstmt == NULL)
		return;

	odbc_clear_driver_stmt(dstmt);

	SQLFreeHandle(SQL_HANDLE_STMT, dstmt->stmt);

	M_free(dstmt);
}


static M_bool odbc_bind_set_type(M_sql_data_type_t type, SQLSMALLINT *ValueType, SQLSMALLINT *ParameterType)
{
	/* Uninitialized warning suppress (won't ever actually be used). */
	*ValueType     = 0;
	*ParameterType = 0;

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			*ValueType                = SQL_C_STINYINT;
			*ParameterType            = SQL_TINYINT;
			break;

		case M_SQL_DATA_TYPE_INT16:
			*ValueType                = SQL_C_SSHORT;
			*ParameterType            = SQL_SMALLINT;
			break;

		case M_SQL_DATA_TYPE_INT32:
			*ValueType                = SQL_C_SLONG;
			*ParameterType            = SQL_INTEGER;
			break;

		case M_SQL_DATA_TYPE_INT64:
			/* XXX: Int64 is really only supported as of Oracle 11.2.
			 *      Should convert to SQL_C_NUMERIC on older versions. */
			*ValueType                = SQL_C_SBIGINT;
			*ParameterType            = SQL_BIGINT;
			break;

		case M_SQL_DATA_TYPE_TEXT:
			*ValueType                = SQL_C_CHAR;
			*ParameterType            = SQL_VARCHAR;
			break;

		case M_SQL_DATA_TYPE_BINARY:
			*ValueType                = SQL_C_BINARY;
			*ParameterType            = SQL_LONGVARBINARY;
			break;

		case M_SQL_DATA_TYPE_UNKNOWN:
			return M_FALSE;
	}
	return M_TRUE;
}


static void odbc_bind_set_value_array(M_sql_stmt_t *stmt, M_sql_data_type_t type, size_t row, size_t col, size_t col_size, odbc_bind_cols_t *bcol)
{
	const unsigned char *data;

	if (M_sql_driver_stmt_bind_isnull(stmt, row, col)) {
		bcol->lens[row] = SQL_NULL_DATA;
		return;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			bcol->data.i8vals[row]  = (M_int8)(M_sql_driver_stmt_bind_get_bool(stmt, row, col));
			break;

		case M_SQL_DATA_TYPE_INT16:
			bcol->data.i16vals[row] = M_sql_driver_stmt_bind_get_int16(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_INT32:
			bcol->data.i32vals[row] = M_sql_driver_stmt_bind_get_int32(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_INT64:
			bcol->data.i64vals[row] = M_sql_driver_stmt_bind_get_int64(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_TEXT:
			data                    = (const unsigned char *)M_sql_driver_stmt_bind_get_text(stmt, row, col);
			bcol->lens[row]         = (SQLLEN)M_sql_driver_stmt_bind_get_text_len(stmt, row, col);
			M_mem_copy(bcol->data.pvalues + (row * col_size), data, (size_t)bcol->lens[row]);
			break;

		case M_SQL_DATA_TYPE_BINARY:
			data                    = M_sql_driver_stmt_bind_get_binary(stmt, row, col);
			bcol->lens[row]         = (SQLLEN)M_sql_driver_stmt_bind_get_binary_len(stmt, row, col);
			M_mem_copy(bcol->data.pvalues + (row * col_size), data, (size_t)bcol->lens[row]);
			break;

		case M_SQL_DATA_TYPE_UNKNOWN: /* Silence warning, should never get this */
			break;
	}
}


static M_sql_error_t odbc_bind_params_array(M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, size_t num_rows, char *error, size_t error_size)
{
	M_sql_error_t        err      = M_SQL_ERROR_SUCCESS;
	size_t               num_cols = M_sql_driver_stmt_bind_cnt(stmt);
	size_t               i;
	size_t               row;
	SQLRETURN            rc;

	/* Array binding reference:
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/using-arrays-of-parameters
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/binding-arrays-of-parameters
	 *   SQLSetStmtAttr:
	 *      SQL_ATTR_PARAMSET_SIZE        - SQLULEN                - num rows - required
	 *      SQL_ATTR_PARAMS_PROCESSED_PTR - SQLULEN *              - ignore   - optional. may set to NULL
	 *      SQL_ATTR_PARAM_STATUS_PTR     - SQLUSMALLINT[num_rows] - ignore   - required
	 *   SQLBindParameter:
	 *      ParameterValue   - SQL_DESC_DATA_PTR         - SQLPOINTER[num_rows] - otherwise SQLPOINTER
	 *      StrLen_or_IndPtr - SQL_DESC_INDICATOR_PTR    - SQLLEN[num_rows]     - otherwise &SQLLEN
	 *      BufferLength     - SQL_DESC_OCTET_LENGTH_PTR - SQLLEN(sizeof(*ptr)) - otherwise sizeof buffer - 0 ok?
	 *      ColumnSize       -                           - SQLULEN              - maximum column size
	 */

	if (num_cols == 0)
		return M_SQL_ERROR_SUCCESS;

	dstmt->bind_cols        = M_malloc_zero(sizeof(*dstmt->bind_cols) * num_cols);
	dstmt->bind_cols_cnt    = num_cols;
	dstmt->bind_cols_status = M_malloc_zero(sizeof(*dstmt->bind_cols_status) * num_rows);

	/* Specify use of column-wise binding, should be default */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* Specify the number of elements in each parameter array.  */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)((SQLULEN)num_rows), 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAMSET_SIZE, num_rows)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* Specify an array in which to return the status of each set of parameters */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAM_STATUS_PTR, dstmt->bind_cols_status, 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAM_STATUS_PTR)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* Specify a variable to indicate how many param sets (rows) were actually processed */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &dstmt->bind_params_processed, 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAMS_PROCESSED_PTR)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	for (i = 0; i < num_cols; i++) {
		SQLSMALLINT          ValueType     = 0;
		SQLSMALLINT          ParameterType = 0;
		SQLULEN              ColumnSize    = M_sql_driver_stmt_bind_get_max_col_size(stmt, i);
		M_sql_data_type_t    type          = M_sql_driver_stmt_bind_get_col_type(stmt, i);
		SQLPOINTER           ParameterValue;

		/* SQL2000 doesn't like 0 on NULL params */
		if (ColumnSize == 0)
			ColumnSize = 1;

		dstmt->bind_cols[i].lens   = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].lens))   * num_rows);
		switch (type) {
			case M_SQL_DATA_TYPE_BOOL:
				dstmt->bind_cols[i].data.i8vals  = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].data.i8vals)) * num_rows);
				ParameterValue                   = dstmt->bind_cols[i].data.i8vals;
				break;
			case M_SQL_DATA_TYPE_INT16:
				dstmt->bind_cols[i].data.i16vals = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].data.i16vals)) * num_rows);
				ParameterValue                   = dstmt->bind_cols[i].data.i16vals;
				break;
			case M_SQL_DATA_TYPE_INT32:
				dstmt->bind_cols[i].data.i32vals = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].data.i32vals)) * num_rows);
				ParameterValue                   = dstmt->bind_cols[i].data.i32vals;
				break;
			case M_SQL_DATA_TYPE_INT64:
				dstmt->bind_cols[i].data.i64vals = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].data.i64vals)) * num_rows);
				ParameterValue                   = dstmt->bind_cols[i].data.i64vals;
				break;
			default:
				/* Increment ColumnSize by 1 to account for NULL termination */
				if (type != M_SQL_DATA_TYPE_BINARY)
					ColumnSize++;

				dstmt->bind_cols[i].data.pvalues = M_malloc_zero(sizeof(*(dstmt->bind_cols[i].data.pvalues)) * ColumnSize * num_rows);
				ParameterValue                   = dstmt->bind_cols[i].data.pvalues;
				break;
		}

		if (!odbc_bind_set_type(type, &ValueType, &ParameterType)) {
			M_snprintf(error, error_size, "Failed to determine data type col %zu", i);
			goto done;
		}

		for (row = 0; row < num_rows; row++) {
			odbc_bind_set_value_array(stmt, type, row, i, (size_t)ColumnSize, &dstmt->bind_cols[i]);
		}

		rc = SQLBindParameter(
			dstmt->stmt,                       /* SQLHSTMT StatementHandle */
			(SQLUSMALLINT)(i+1),               /* SQLUSMALLINT ParameterNumber */
			SQL_PARAM_INPUT,                   /* SQLSMALLINT InputOutputType */
			ValueType,                         /* SQLSMALLINT ValueType */
			ParameterType,                     /* SQLSMALLINT ParameterType */
			ColumnSize,                        /* SQLULEN ColumnSize */
			(SQLSMALLINT)0,                    /* SQLSMALLINT DecimalDigits */
			ParameterValue,                    /* SQLPOINTER ParameterValuePtr - union base addr is same for all types */
			(SQLLEN)ColumnSize,                /* SQLLEN BufferLength */
			dstmt->bind_cols[i].lens           /* SQLLEN *StrLen_or_IndPtr */
		);

		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
			char prefix[256];
			M_snprintf(prefix, sizeof(prefix), "SQLBindParameter(row: %zu, col: %zu) failed", row, i);
			err = odbc_format_error(prefix, NULL, dstmt, rc, error, error_size);
			goto done;
		}
	}

done:
	return err;
}


static void odbc_bind_set_value_flat(M_sql_stmt_t *stmt, size_t row, size_t col, SQLPOINTER *value, SQLLEN *len)
{
	M_sql_data_type_t type = M_sql_driver_stmt_bind_get_type(stmt, row, col);
	const unsigned char *data;

	if (M_sql_driver_stmt_bind_isnull(stmt, row, col)) {
		*len   = SQL_NULL_DATA;
		return;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			*value = M_sql_driver_stmt_bind_get_bool_addr(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_INT16:
			*value = M_sql_driver_stmt_bind_get_int16_addr(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_INT32:
			*value = M_sql_driver_stmt_bind_get_int32_addr(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_INT64:
			*value = M_sql_driver_stmt_bind_get_int64_addr(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_TEXT:
			data   = (const unsigned char *)M_sql_driver_stmt_bind_get_text(stmt, row, col);
			*value = M_CAST_OFF_CONST(SQLPOINTER, data);
			*len   = (SQLLEN)M_sql_driver_stmt_bind_get_text_len(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_BINARY:
			data   = M_sql_driver_stmt_bind_get_binary(stmt, row, col);
			*value = M_CAST_OFF_CONST(SQLPOINTER, data);
			*len   = (SQLLEN)M_sql_driver_stmt_bind_get_binary_len(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_UNKNOWN: /* Silence warning, should never get this */
			break;
	}
}


static M_sql_error_t odbc_bind_params_flat(M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, size_t num_rows, char *error, size_t error_size)
{
	M_sql_error_t        err      = M_SQL_ERROR_SUCCESS;
	size_t               num_cols = M_sql_driver_stmt_bind_cnt(stmt);
	size_t               i;
	size_t               row;
	SQLRETURN            rc;

	if (num_cols == 0)
		return M_SQL_ERROR_SUCCESS;

	dstmt->bind_flat_lens = M_malloc_zero(sizeof(*dstmt->bind_flat_lens) * num_cols * num_rows);

	/* Specify the number of elements in each parameter array.  */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)((SQLULEN)1), 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAMSET_SIZE)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* UNSET array in which to return the status of each set of parameters */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAM_STATUS_PTR, NULL, 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAM_STATUS_PTR=NULL)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* UNSET value in which to return the number of parameter sets processed */
	rc = SQLSetStmtAttr(dstmt->stmt, SQL_ATTR_PARAMS_PROCESSED_PTR, NULL, 0);  
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		err = odbc_format_error("SQLSetStmtAttr(SQL_ATTR_PARAMS_PROCESSED_PTR=NULL)", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	for (row = 0; row < num_rows; row++) {
		for (i = 0; i < num_cols; i++) {
			SQLSMALLINT          ValueType      = 0;
			SQLSMALLINT          ParameterType  = 0;
			SQLULEN              ColumnSize     = 0;
			SQLPOINTER           ParameterValue = NULL;
			M_sql_data_type_t    type           = M_sql_driver_stmt_bind_get_type(stmt, row, i);
			size_t               idx            = (row * num_cols) + i;

			if (!odbc_bind_set_type(type, &ValueType, &ParameterType)) {
				M_snprintf(error, error_size, "Failed to determine data type for rows %zu col %zu", row, i);
				goto done;
			}
			odbc_bind_set_value_flat(stmt, row, i, &ParameterValue, &dstmt->bind_flat_lens[idx]);

			if (ParameterValue != NULL) {
				ColumnSize = (SQLULEN)dstmt->bind_flat_lens[idx];
			}

			/* SQL2000 can't handle a 0 column size on NULL values */
			if (ColumnSize == 0)
				ColumnSize = 1;

			rc = SQLBindParameter(
				dstmt->stmt,                       /* SQLHSTMT StatementHandle */
				(SQLUSMALLINT)(idx+1),             /* SQLUSMALLINT ParameterNumber */
				SQL_PARAM_INPUT,                   /* SQLSMALLINT InputOutputType */
				ValueType,                         /* SQLSMALLINT ValueType */
				ParameterType,                     /* SQLSMALLINT ParameterType */
				ColumnSize,                        /* SQLULEN ColumnSize */
				(SQLSMALLINT)0,                    /* SQLSMALLINT DecimalDigits */
				ParameterValue,                    /* SQLPOINTER ParameterValuePtr */
				0,                                 /* SQLLEN BufferLength */
				&dstmt->bind_flat_lens[idx]        /* SQLLEN *StrLen_or_IndPtr */
			);

			if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
				char prefix[256];
				M_snprintf(prefix, sizeof(prefix), "SQLBindParameter(row: %zu, col: %zu) failed", row, i);
				err = odbc_format_error(prefix, NULL, dstmt, rc, error, error_size);
				goto done;
			}
		}
	}

done:
	return err;
}


static size_t odbc_num_bind_rows(M_sql_conn_t *conn, M_sql_stmt_t *stmt) 
{
	M_sql_driver_conn_t *dconn    = M_sql_driver_conn_get_conn(conn);
	size_t               num_rows = M_sql_driver_stmt_bind_rows(stmt);

	if (dconn->pool_data->profile->max_insert_records) 
		num_rows = M_MIN(dconn->pool_data->profile->max_insert_records, num_rows);

	return num_rows;
}


static M_sql_error_t odbc_bind_params(M_sql_conn_t *conn, M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_conn_t *dconn    = M_sql_driver_conn_get_conn(conn);
	size_t               num_rows = odbc_num_bind_rows(conn, stmt);

	if (M_sql_driver_stmt_bind_cnt(stmt) == 0 || num_rows == 0)
		return M_SQL_ERROR_SUCCESS;

	/* We will definitely be binding params here, so need to mark that it needs to be cleared before reuse */
	dstmt->needs_clear |= ODBC_CLEAR_PARAMS;

	if (num_rows == 1 || dconn->pool_data->profile->is_multival_insert_cd)
		return odbc_bind_params_flat(dstmt, stmt, num_rows, error, error_size);

	return odbc_bind_params_array(dstmt, stmt, num_rows, error, error_size);
}


static M_sql_error_t odbc_cb_prepare(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_conn_t *dconn    = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err      = M_SQL_ERROR_SUCCESS;
	const char          *query    = M_sql_driver_stmt_get_query(stmt);
	SQLRETURN            rc;
	M_bool               new_stmt = (*driver_stmt) == NULL?M_TRUE:M_FALSE;

//M_printf("Query |%s|\n", query);

	if (*driver_stmt == NULL) {
		/* Allocate a new handle */
		*driver_stmt          = M_malloc_zero(sizeof(**driver_stmt));
		(*driver_stmt)->dconn = dconn;
		rc                    = SQLAllocHandle(SQL_HANDLE_STMT, dconn->dbc_handle, &(*driver_stmt)->stmt);
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
			err = odbc_format_error("SQLAllocHandle(SQL_HANDLE_STMT) failed", dconn, NULL, rc, error, error_size);
			goto done;
		}

		rc = SQLPrepare((*driver_stmt)->stmt, M_CAST_OFF_CONST(SQLCHAR *, query), (SQLINTEGER)M_str_len(query));
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
			err = odbc_format_error("SQLPrepare failed", NULL, *driver_stmt, rc, error, error_size);
			goto done;
		}
	} else {
		/* Clear any existing data so we can reuse the handle */
		odbc_clear_driver_stmt(*driver_stmt);
	}

	/* Bind parameters */
	err = odbc_bind_params(conn, *driver_stmt, stmt, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		if (*driver_stmt != NULL && new_stmt) {
			odbc_cb_prepare_destroy(*driver_stmt);
			*driver_stmt = NULL;
		}
	}

	return err;
}


static M_sql_data_type_t odbc_type_to_mtype(M_sql_conn_t *conn, SQLSMALLINT DataType, SQLULEN ColumnSize, size_t *max_len)
{
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	M_sql_data_type_t    type;

	*max_len = 0;

	switch (DataType) {
		case SQL_TINYINT:
			type     = M_SQL_DATA_TYPE_BOOL;
			break;

		case SQL_SMALLINT:
			type     = M_SQL_DATA_TYPE_INT16;
			break;

		case SQL_INTEGER:
			type     = M_SQL_DATA_TYPE_INT32;
			break;

		case SQL_BIGINT:
		case SQL_NUMERIC:
			type     = M_SQL_DATA_TYPE_INT64;
			break;

		case SQL_CHAR:
		case SQL_VARCHAR:
#if SQL_VARCHAR != SQL_LONGVARCHAR
		case SQL_LONGVARCHAR:
#endif
			type     = M_SQL_DATA_TYPE_TEXT;
			*max_len = ColumnSize;
			break;

		case SQL_BINARY:
		case SQL_VARBINARY:
#if SQL_VARBINARY != SQL_LONGVARBINARY
		case SQL_LONGVARBINARY:
#endif
#ifdef SQL_BLOB
		case SQL_BLOB:
#endif
			type     = M_SQL_DATA_TYPE_BINARY;
			*max_len = ColumnSize;
			break;

		default:
			type     = M_SQL_DATA_TYPE_TEXT;
			break;
	}

	/* Some DBs use a weird indicator for unknown size.  PostgresSQL uses "255"
	 * for some reason and this causes major issues if we take that at face value */
	if (*max_len == dconn->pool_data->profile->unknown_size_ind)
		*max_len = 0;

	return type;
}


static M_sql_error_t odbc_fetch_result_metadata(M_sql_conn_t *conn, M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, size_t num_cols, char *error, size_t error_size)
{
	size_t              i;

	M_sql_driver_stmt_result_set_num_cols(stmt, num_cols);

	for (i=0; i<num_cols; i++) {
		SQLRETURN         rc;
		size_t            max_len          = 0;
		M_sql_data_type_t mtype;
		/* Column description variables */
		SQLSMALLINT       NameLength    = 0;
		SQLSMALLINT       DataType      = 0;
		SQLSMALLINT       DecimalDigits = 0;
		SQLSMALLINT       Nullable      = 0;
		SQLULEN           ColumnSize    = 0;
		SQLCHAR           ColumnName[256];

		M_mem_set(ColumnName, 0, sizeof(ColumnName));

		rc = SQLDescribeCol(
			dstmt->stmt,           /* SQLHSTMT       StatementHandle            */
			(SQLUSMALLINT)(i+1),   /* SQLUSMALLINT   ColumnNumber (1->num_cols) */
			ColumnName,            /* SQLCHAR       *ColumnName                 */
			sizeof(ColumnName)-1,  /* SQLSMALLINT    BufferLength               */
			&NameLength,           /* SQLSMALLINT   *NameLengthPtr (ignore)     */
			&DataType,             /* SQLSMALLINT   *DataTypePtr                */
			&ColumnSize,           /* SQLULEN       *ColumnSizePtr              */
			&DecimalDigits,        /* SQLSMALLINT   *DecimalDigitsPtr (ignore)  */
			&Nullable              /* SQLSMALLINT   *NullablePtr (ignore)       */
		);

		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
			char prefix[256];
			M_snprintf(prefix, sizeof(prefix), "SQLDescribeCol(%zu) failed", i);
			return odbc_format_error(prefix, NULL, dstmt, rc, error, error_size);
		}

		mtype = odbc_type_to_mtype(conn, DataType, ColumnSize, &max_len);
		M_sql_driver_stmt_result_set_col_name(stmt, i, (const char *)ColumnName);
		M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, max_len);

		/* XXX:  Its possible we should use SQL_DATA_AT_EXEC with SQLBindCol() for 
		 *       large columns and fetch them with SQLGetData, but for reasonably sized
		 *       ones (<=2k?) , use the bound fields */
	}

	return M_SQL_ERROR_SUCCESS;
}



static M_sql_error_t odbc_cb_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size)
{
	M_sql_driver_stmt_t *dstmt         = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t *dconn         = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err           = M_SQL_ERROR_SUCCESS;
	SQLRETURN            rc;
	SQLRETURN            exec_rc;
	SQLSMALLINT          num_cols      = 0;

	/* Calc number of rows we'll try to insert at once */
	*rows_executed = odbc_num_bind_rows(conn, stmt); 

	exec_rc = SQLExecute(dstmt->stmt);
	if (exec_rc != SQL_SUCCESS && exec_rc != SQL_SUCCESS_WITH_INFO && exec_rc != SQL_NO_DATA) {
		err = odbc_format_error("SQLExecute failed", NULL, dstmt, exec_rc, error, error_size);
		goto done;
	}

	if (*rows_executed > 1 && !dconn->pool_data->profile->is_multival_insert_cd) {
		size_t i;

		/* Validate */
		if (dstmt->bind_params_processed != *rows_executed) {
			M_snprintf(error, error_size, "SQLExecute expected to process %zu rows, only processed %zu", *rows_executed, (size_t)dstmt->bind_params_processed);
			err = M_SQL_ERROR_QUERY_FAILURE;
			goto done;
		}

		for (i=0; i<*rows_executed; i++) {
			if (dstmt->bind_cols_status[i] != SQL_PARAM_SUCCESS && dstmt->bind_cols_status[i] != SQL_PARAM_SUCCESS_WITH_INFO) {
				const char *reason = "UNKNOWN";
				if (dstmt->bind_cols_status[i] == SQL_PARAM_ERROR)
					reason = "ERROR";
				if (dstmt->bind_cols_status[i] == SQL_PARAM_UNUSED)
					reason = "UNUSED";
				M_snprintf(error, error_size, "SQLExecute row %zu of %zu failure: %s", i, *rows_executed, reason);
				err = M_SQL_ERROR_QUERY_FAILURE;
				goto done;
			}
		}
	}


	rc = SQLNumResultCols(dstmt->stmt, &num_cols);
	/* NOTE: SQLExecute() may return SQL_NO_DATA and this return an error */
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO && exec_rc != SQL_NO_DATA) {
		err = odbc_format_error("SQLNumResultCols failed", NULL, dstmt, rc, error, error_size);
		goto done;
	}

	/* Statement doesn't return results, get the affected row count and exit */
	if (num_cols == 0) {
		SQLLEN affected_rows = 0;
		rc = SQLRowCount(dstmt->stmt, &affected_rows);
		/* NOTE: SQLExecute() may return SQL_NO_DATA and this return an error */
		if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO && exec_rc != SQL_NO_DATA) {
			err = odbc_format_error("SQLRowCount failed", NULL, dstmt, rc, error, error_size);
			goto done;
		}

		/* We're good! */
		if (affected_rows > 0) {
			M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)affected_rows);
		}
		err = M_SQL_ERROR_SUCCESS;
		goto done;
	}

	/* Statement returns results, so it may have a cursor, mark that it may need to be cleared */
	dstmt->needs_clear |= ODBC_CLEAR_CURSOR;

	err = odbc_fetch_result_metadata(conn, dstmt, stmt, (size_t)num_cols, error, error_size);
	if (M_sql_error_is_error(err))
		goto done;


	/* Success, lets assume we probably have rows */
	err = M_SQL_ERROR_SUCCESS_ROW;

done:
	/* No need for result metadata if all has been fetched or an error has occurred */
	if (err != M_SQL_ERROR_SUCCESS_ROW)
		odbc_clear_driver_stmt(dstmt);
	return err;
}


/* XXX: Fetch Cancel ? */

static M_sql_error_t odbc_cb_fetch(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_stmt_t *dstmt    = M_sql_driver_stmt_get_stmt(stmt);
	size_t               num_cols = M_sql_stmt_result_num_cols(stmt);
	size_t               i;
	SQLRETURN            rc;

	(void)conn;

	rc = SQLFetch(dstmt->stmt);

	/* Fetch complete */
	if (rc == SQL_NO_DATA) {
		odbc_clear_driver_stmt(dstmt);
		return M_SQL_ERROR_SUCCESS;
	}

	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		return odbc_format_error("SQLFetch failed", NULL, dstmt, rc, error, error_size);
	}

	for (i=0; i<num_cols; i++) {
		size_t            max_size     = 0;
		M_sql_data_type_t type         = M_sql_stmt_result_col_type(stmt, i, &max_size);
		M_buf_t          *buf          = M_sql_driver_stmt_result_col_start(stmt);

		/* Parameters for SQLGetData */
		SQLSMALLINT       TargetType;
		SQLPOINTER        TargetValue;
		SQLLEN            BufferLength = 0;
		SQLLEN            StrLen       = 0;

		M_int8            i8;
		M_int16           i16;
		M_int32           i32;
		M_int64           i64;
		unsigned char    *data         = NULL;
		size_t            data_size    = 0;

		switch (type) {
			case M_SQL_DATA_TYPE_BOOL:
				TargetType  = SQL_C_STINYINT;
				TargetValue = &i8;
				break;

			case M_SQL_DATA_TYPE_INT16:
				TargetType  = SQL_C_SSHORT;
				TargetValue = &i16;
				break;

			case M_SQL_DATA_TYPE_INT32:
				TargetType  = SQL_C_SLONG;
				TargetValue = &i32;
				break;

			case M_SQL_DATA_TYPE_INT64:
				TargetType  = SQL_C_SBIGINT;
				TargetValue = &i64;
				break;

			case M_SQL_DATA_TYPE_BINARY:
			case M_SQL_DATA_TYPE_TEXT:
			default: /* Should never be used */
				if (type == M_SQL_DATA_TYPE_BINARY) {
					TargetType   = SQL_C_BINARY;
				} else {
					TargetType   = SQL_C_CHAR;
				}

				/* Get a direct writable buffer for small unknown size data sets */
				if (max_size != 0 && max_size <= 1024) {
					data_size  = max_size;
				} else {
					data_size  = 1024;
				}

				data_size   += 1; /* Additional size for NULL terminator */
				data         = M_buf_direct_write_start(buf, &data_size);
				BufferLength = (SQLLEN)data_size;

				TargetValue  = data;
				break;
		}

		/* Read in result data to pointers */
		do {
			rc = SQLGetData(dstmt->stmt, (SQLUSMALLINT)(i+1), TargetType, TargetValue, BufferLength, &StrLen);
			if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
				char prefix[256];
				M_snprintf(prefix, sizeof(prefix), "SQLGetData(%zu) failed", i);
				return odbc_format_error(prefix, NULL, dstmt, rc, error, error_size);
			}

			if (rc == SQL_SUCCESS_WITH_INFO && data != NULL) {
				M_buf_direct_write_end(buf, (size_t)((TargetType == SQL_C_CHAR)?BufferLength-1:BufferLength)); /* Ignore Null Term on Character data */

				/* StrLen might be unavailable */
				if (StrLen == SQL_NO_TOTAL)
					StrLen = BufferLength * 2;

				data_size    = (size_t)(StrLen - BufferLength + 1);
				data         = M_buf_direct_write_start(buf, &data_size);
				TargetValue  = data;
				BufferLength = (SQLLEN)data_size;
				StrLen       = 0;
			}
		} while(rc == SQL_SUCCESS_WITH_INFO && data != NULL);

		/* Write pointers to column data */

		/* Result is NULL, go to next column, don't write NULL terminator */
		if (StrLen == SQL_NULL_DATA) {
			if (data != NULL)
				M_buf_direct_write_end(buf, 0);
			continue;
		}

		switch (type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_buf_add_int(buf, i8);
				break;

			case M_SQL_DATA_TYPE_INT16:
				M_buf_add_int(buf, i16);
				break;

			case M_SQL_DATA_TYPE_INT32:
				M_buf_add_int(buf, i32);
				break;

			case M_SQL_DATA_TYPE_INT64:
				M_buf_add_int(buf, i64);
				break;

			case M_SQL_DATA_TYPE_BINARY:
			case M_SQL_DATA_TYPE_TEXT:
			default:
				M_buf_direct_write_end(buf, (size_t)StrLen);
				break;
		}

		/* All columns with data require NULL termination, even binary.  Otherwise its considered a NULL column. */
		M_buf_add_byte(buf, 0); /* Manually add NULL terminator */
	}

	M_sql_driver_stmt_result_row_finish(stmt);

	return M_SQL_ERROR_SUCCESS_ROW;
}


static M_sql_error_t odbc_cb_begin(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	SQLINTEGER           iso;
	SQLRETURN            rc;

	if (isolation == M_SQL_ISOLATION_SNAPSHOT)
		isolation = M_SQL_ISOLATION_SERIALIZABLE;

/* XXX: Support max isolation? */

	switch (isolation) {
		case M_SQL_ISOLATION_READCOMMITTED:
			iso = SQL_TXN_READ_COMMITTED;
			break;
		case M_SQL_ISOLATION_REPEATABLEREAD:
			iso = SQL_TXN_REPEATABLE_READ;
			break;
		case M_SQL_ISOLATION_READUNCOMMITTED:
			iso = SQL_TXN_READ_UNCOMMITTED;
			break;
		case M_SQL_ISOLATION_SNAPSHOT:
		case M_SQL_ISOLATION_SERIALIZABLE:
		default: /* Should not get this */
			iso = SQL_TXN_SERIALIZABLE;
			break;
	}

	rc = SQLSetConnectAttr(dconn->dbc_handle, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)((M_uintptr)iso), SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		return odbc_format_error("SQLSetConnectAttr(SQL_ATTR_TXN_ISOLATION) failed", dconn, NULL, rc, error, error_size);
	}

	rc = SQLSetConnectAttr(dconn->dbc_handle, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		return odbc_format_error("SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT=SQL_AUTOCOMMIT_OFF) failed", dconn, NULL, rc, error, error_size);
	}

	/* Implicitly begins */

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t odbc_end_tran(M_sql_conn_t *conn, M_bool is_rollback, char *error, size_t error_size)
{
	SQLRETURN            rc;
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);

	rc = SQLEndTran(SQL_HANDLE_DBC, dconn->dbc_handle, is_rollback?SQL_ROLLBACK:SQL_COMMIT);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		char prefix[256];
		M_snprintf(prefix, sizeof(prefix), "SQLEndTran(%s) failed", is_rollback?"SQL_ROLLBACK":"SQL_COMMIT");
		return odbc_format_error(prefix, dconn, NULL, rc, error, error_size);
	}

	/* Turn auto-commit on since we're no longer in a transaction */
	rc = SQLSetConnectAttr(dconn->dbc_handle, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		return odbc_format_error("SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT=SQL_AUTOCOMMIT_ON) failed", dconn, NULL, rc, error, error_size);
	}

	/* Default back to read-committed transactions, not very restrictive */
	rc = SQLSetConnectAttr(dconn->dbc_handle, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_READ_COMMITTED, SQL_IS_INTEGER);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
		return odbc_format_error("SQLSetConnectAttr(SQL_ATTR_TXN_ISOLATION=SQL_TXN_READ_COMMITTED) failed", dconn, NULL, rc, error, error_size);
	}

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t odbc_cb_rollback(M_sql_conn_t *conn)
{
	char           error[256];
	M_sql_error_t  err   = odbc_end_tran(conn, M_TRUE, error, sizeof(error));

	if (M_sql_error_is_error(err)) {
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
	}

	return err;
}


static M_sql_error_t odbc_cb_commit(M_sql_conn_t *conn, char *error, size_t error_size)
{
	return odbc_end_tran(conn, M_FALSE, error, error_size);
}


static M_bool odbc_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len)
{
	M_sql_driver_connpool_t  *dpool      = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t     *data       = &dpool->primary;

	return data->profile->cb_datatype(pool, buf, type, max_len);
}


static void odbc_cb_createtable_suffix(M_sql_connpool_t *pool, M_buf_t *query)
{
	M_sql_driver_connpool_t  *dpool      = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t     *data       = &dpool->primary;

	if (data->profile->cb_createtable_suffix)
		data->profile->cb_createtable_suffix(pool, data->settings, query);
}


static void odbc_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type)
{
	M_sql_driver_connpool_t  *dpool      = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t     *data       = &dpool->primary;

	if (data->profile->cb_append_updlock)
		data->profile->cb_append_updlock(pool, query, type);
}


static M_bool odbc_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	M_sql_driver_connpool_t  *dpool      = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t     *data       = &dpool->primary;

	return data->profile->cb_append_bitop(pool, query, op, exp1, exp2);
}


static char *odbc_cb_rewrite_indexname(M_sql_connpool_t *pool, const char *index_name)
{
	M_sql_driver_connpool_t  *dpool      = M_sql_driver_pool_get_dpool(pool);
	odbc_connpool_data_t     *data       = &dpool->primary;

	if (!data->profile->cb_rewrite_indexname)
		return NULL;

	return data->profile->cb_rewrite_indexname(pool, index_name);
}


static M_sql_driver_t M_sql_odbc = {
	M_SQL_DRIVER_VERSION,         /* Driver/Module subsystem version */
	"odbc",                       /* Short name of module */
	"ODBC driver for mstdlib",    /* Display name of module */
	"1.0.0",                      /* Internal module version */

	odbc_cb_init,                 /* Callback used for module initialization. */
	odbc_cb_destroy,              /* Callback used for module destruction/unloading. */
	odbc_cb_createpool,           /* Callback used for pool creation */
	odbc_cb_destroypool,          /* Callback used for pool destruction */
	odbc_cb_connect,              /* Callback used for connecting to the db */
	odbc_cb_serverversion,        /* Callback used to get the server name/version string */
	odbc_cb_connect_runonce,      /* Callback used after connection is established, but before first query to set run-once options. */
	odbc_cb_disconnect,           /* Callback used to disconnect from the db */
	odbc_cb_queryformat,          /* Callback used for reformatting a query to the sql db requirements */
	odbc_cb_prepare,              /* Callback used for preparing a query for execution */
	odbc_cb_prepare_destroy,      /* Callback used to destroy the driver-specific prepared statement handle */
	odbc_cb_execute,              /* Callback used for executing a prepared query */
	odbc_cb_fetch,                /* Callback used to fetch result data/rows from server */
	odbc_cb_begin,                /* Callback used to begin a transaction */
	odbc_cb_rollback,             /* Callback used to rollback a transaction */
	odbc_cb_commit,               /* Callback used to commit a transaction */
	odbc_cb_datatype,             /* Callback used to convert to data type for server */
	odbc_cb_createtable_suffix,   /* Callback used to append additional data to the Create Table query string */
	odbc_cb_append_updlock,       /* Callback used to append row-level locking data */
	odbc_cb_append_bitop,         /* Callback used to append a bit operation */
	odbc_cb_rewrite_indexname,    /* Callback used to rewrite an index name to comply with DB requirements */
	NULL,                         /* Handle for loaded driver - must be initialized to NULL */
};

/*! Defines function that references M_sql_driver_t M_sql_##name for module loading */
M_SQL_DRIVER(odbc)
