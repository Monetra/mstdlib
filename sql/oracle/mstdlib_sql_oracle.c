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

/* Work around OCI bug */
#if defined(_WIN32) && defined(__GNUC__)
#  define _int64 long long
#endif

#include <oci.h>
#include "oracle_shared.h"

/* Documentation:
 *   https://docs.oracle.com/cd/B28359_01/appdev.111/b28395/toc.htm
 */

/* Don't use m_defs_int.h, since we need to be able to build this as an external plugin. */
#ifndef M_CAST_OFF_CONST
#define M_CAST_OFF_CONST(type, var) ((type)((M_uintptr)var))
#endif


typedef struct {
	OCIDefine        *define;    /*!< Oracle result column Define handle */
	size_t            idx;       /*!< Column Index */
	M_sql_stmt_t     *stmt;      /*!< Pointer back to parent statement handle */
	sb2               ind;       /*!< Indicator value, -1 = NULL, 0 is ok */
	M_sql_data_type_t type;      /*!< Data type */
	size_t            type_size; /*!< Data type size */

	union {
		struct {
			ub2    len;
			M_bool data;
		} b; /*!< Boolean */
		struct {
			ub2     len;
			M_int16 data;
		} i16; /*!< M_int16 */
		struct {
			ub2     len;
			M_int32 data;
		} i32; /*!< M_int32 */
		struct {
			ub2     len;
			M_int64 data;
		} i64; /*!< M_int64 */
		struct {
			ub2     len;
			char    data[1024];
		} smalltext; /*!< Text <= 1024 bytes */
		struct {
			ub2     len;
			M_uint8 data[1024];
		} smallbinary; /*!< Binary <= 1024 bytes */
		struct {
			ub4      last_len;
			size_t   data_alloc;
			size_t   written_len;
			char    *data;
		} text; /*!< Arbitrarily large text */
		struct {
			ub4      last_len;
			size_t   data_alloc;
			size_t   written_len;
			M_uint8 *data;
		} binary; /*!< Arbitrarily large binary data */
	} d;
} oracle_result_data_t;


typedef struct {
	OCIBind          *bind;     /*!< Oracle Column BIND handle */
	size_t            idx;      /*!< Column index */
	M_sql_stmt_t     *stmt;     /*!< Pointer back to parent statement handle */
	sb2               ind;      /*!< Indicator value, -1 = NULL, 0 is ok */
	M_sql_data_type_t type;     /*!< Data type as currently known (for tracking if we need to re-bind) */
	size_t            max_size; /*!< Maximum data size for this column */
} oracle_bind_data_t;


struct M_sql_driver_stmt {
	OCIStmt              *stmt;        /*!< Oracle Statement handle */
	oracle_bind_data_t   *bind;        /*!< Bind handle per column */
	oracle_result_data_t *result;      /*!< Define handle per result column */
	size_t                result_cols; /*!< Count of columns in result handle */
	M_sql_conn_t         *conn;        /*!< Connection handle associated with statement */
	M_bool                is_query;    /*!< Needed to be able to set 'iters' for OCIStmtExecute appropriately */
};


typedef struct {
	char              dsn[2048];
	M_sql_hostport_t *hosts;
	size_t            num_hosts;
	char              service_name[128];
} oracle_connpool_data_t;


struct M_sql_driver_connpool {
	oracle_connpool_data_t primary;
	oracle_connpool_data_t readonly;
};


struct M_sql_driver_conn {
	OCIError   *err_handle;
	OCISvcCtx  *svc_handle;
	M_bool      is_connected;
	char        version[256];
};


static OCIEnv *oracle_env_handle = NULL;

/* Ugly and hackish, but it shuts the compiler up since it is
 * the _right_ way to do it for oracle */
#define CAST_DVOIDPP(a) ((dvoid **)((M_uintptr)a))


static void oracle_sanitize_error(char *error)
{
	if (M_str_isempty(error))
		return;

	M_str_replace_chr(error, '\n', ' ');
	M_str_replace_chr(error, '\r', ' ');
	M_str_replace_chr(error, '\t', ' ');
}


/*! Format an Oracle Error Message and return a more specific error code if available
 *
 *  \param[in]  msg_prefix Prefix to prepend to the front of a formatted error message
 *  \param[in]  dconn      Pointer to driver connection handle.  If NULL, just error message
 *                         prefix provided will be used.
 *  \param[in]  rv         Return value as passed from command that generated the error.
 *  \param[out] error      User-supplied buffer to hold error message.
 *  \param[in]  error_size Size of user-supplied error buffer.
 *  \return One of the !M_sql_error_t error conditions as mapped from the error code
 */
static M_sql_error_t oracle_format_error(const char *msg_prefix, M_sql_driver_conn_t *dconn, sword rv, char *error, size_t error_size)
{
	char          myerr[256];
	int           errcode = 0;
	M_sql_error_t err;

	M_mem_set(error, 0, error_size);
	M_mem_set(myerr, 0, sizeof(myerr));

	if (dconn == NULL || dconn->err_handle == NULL) {
		M_snprintf(error, error_size, "%s: rv=%d", msg_prefix, (int)rv);
		oracle_sanitize_error(error);
		return M_SQL_ERROR_CONN_FAILED;
	}

	OCIErrorGet((dvoid *)dconn->err_handle,
	            (ub4)1,
	            (OraText *)NULL,
	            &errcode,
	            (OraText *)myerr,
	            (ub4)sizeof(myerr),
	            OCI_HTYPE_ERROR);

	M_snprintf(error, error_size, "%s: rv=%d errcode=%d: %s", msg_prefix, (int)rv, errcode, myerr);
	oracle_sanitize_error(error);

	err = oracle_resolve_error(NULL, errcode);

	if (!dconn->is_connected && err == M_SQL_ERROR_QUERY_FAILURE) {
		/* Rewrite generic failure condition to connection failure if not connected */
		err = M_SQL_ERROR_CONN_FAILED;
	}

	return err;
}


static M_bool oracle_cb_init(char *error, size_t error_size)
{
	sword rv;

#define OCI_NLS_NCHARSET_ID_UT8     871               /* UTF8 charset id - Legacy */
#define OCI_NLS_NCHARSET_ID_AL32UT8 873               /* AL32UTF8 charset id - Current recommended */

	rv = OCIEnvNlsCreate(&oracle_env_handle,          /* env_hpp */
	                     OCI_OBJECT | OCI_THREADED,   /* mode */
	                     NULL,                        /* ctxp - for memory callbacks */
	                     NULL,                        /* mallocfp */
	                     NULL,                        /* ralocfp */
	                     NULL,                        /* mfreefp */
	                     0,                           /* xtramemsz */
	                     NULL,                        /* usrmempp */
	                     OCI_NLS_NCHARSET_ID_AL32UT8, /* charset */
	                     OCI_NLS_NCHARSET_ID_AL32UT8  /* ncharset */
	                     );

	if (rv != OCI_SUCCESS) {
		oracle_format_error("OCIEnvNlsCreate failed", NULL, rv, error, error_size);
		return M_FALSE;
	}

	return M_TRUE;
}


static void oracle_cb_destroy(void)
{
	if (oracle_env_handle)
		OCIHandleFree(oracle_env_handle, OCI_HTYPE_ENV);
	oracle_env_handle = NULL;
}


static M_bool oracle_connpool_readconf(oracle_connpool_data_t *data, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	M_sql_connstr_params_t params[] = {
		{ "dsn",          M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,  2048 },
		{ "host",         M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,  1024 },
		{ "service_name", M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,  128  },

		{ NULL, 0, M_FALSE, 0, 0}
	};
	const char *dsn;
	const char *host;
	const char *service_name;

	if (!M_sql_driver_validate_connstr(conndict, params, error, error_size)) {
		return M_FALSE;
	}

	dsn          = M_hash_dict_get_direct(conndict, "dsn");
	host         = M_hash_dict_get_direct(conndict, "host");
	service_name = M_hash_dict_get_direct(conndict, "service_name");

	if (!M_str_isempty(dsn) && (!M_str_isempty(host) || !M_str_isempty(service_name))) {
		M_snprintf(error, error_size, "cannot specify dsn with host or service_name");
		return M_FALSE;
	}

	if (M_str_isempty(dsn) && M_str_isempty(host)) {
		M_snprintf(error, error_size, "must specify either dsn or host and service_name");
		return M_FALSE;
	}

	if (!M_str_isempty(host) && M_str_isempty(service_name)) {
		M_snprintf(error, error_size, "must specify service_name with host");
		return M_FALSE;
	}

	/* dsn */
	M_str_cpy(data->dsn, sizeof(data->dsn), dsn);

	/* service_name */
	M_str_cpy(data->service_name, sizeof(data->service_name), service_name);

	/* hosts */
	if (!M_str_isempty(host)) {
		data->hosts = M_sql_driver_parse_hostport(host, 1521, &data->num_hosts, error, error_size);
		if (data->hosts == NULL)
			return M_FALSE;
	}

	*num_hosts = data->num_hosts;

	/* Must be using dsn instead */
	if (*num_hosts == 0)
		*num_hosts = 1;

	return M_TRUE;
}


static M_bool oracle_cb_createpool(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	oracle_connpool_data_t  *data;

	if (M_str_isempty(M_sql_driver_pool_get_username(pool))) {
		M_snprintf(error, error_size, "Username cannot be blank");
		return M_FALSE;
	}

	if (M_str_isempty(M_sql_driver_pool_get_password(pool))) {
		M_snprintf(error, error_size, "Password cannot be blank");
		return M_FALSE;
	}

	if (*dpool == NULL) {
		*dpool = M_malloc_zero(sizeof(**dpool));
	}

	data = is_readonly?&(*dpool)->readonly:&(*dpool)->primary;

	return oracle_connpool_readconf(data, conndict, num_hosts, error, error_size);
}


static void oracle_cb_destroypool(M_sql_driver_connpool_t *dpool)
{
	if (dpool == NULL)
		return;

	M_free(dpool->primary.hosts);
	M_free(dpool->readonly.hosts);
	M_free(dpool);
}


static void oracle_cb_disconnect(M_sql_driver_conn_t *conn)
{
	if (conn == NULL)
		return;

	/* We ignore errors */
	if (conn->is_connected) {
		OCILogoff(conn->svc_handle, conn->err_handle);
	}

	if (conn->svc_handle) {
		OCIHandleFree(conn->svc_handle, OCI_HTYPE_SVCCTX);
	}

	if (conn->err_handle) {
		OCIHandleFree(conn->err_handle, OCI_HTYPE_ERROR);
	}

	M_free(conn);
}


static M_sql_error_t oracle_cb_connect(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size)
{
	M_sql_driver_connpool_t *dpool      = M_sql_driver_pool_get_dpool(pool);
	oracle_connpool_data_t  *data       = is_readonly_pool?&dpool->readonly:&dpool->primary;
	M_sql_error_t            err        = M_SQL_ERROR_SUCCESS;
	sword                    rv;
	const char              *username;
	const char              *password;
	M_buf_t                 *dsn        = NULL;
	ub4                      ver;

	*conn = M_malloc_zero(sizeof(**conn));

	/* Initialize Error Handle */
	rv = OCIHandleAlloc(oracle_env_handle,                  /* parenth */
	                    CAST_DVOIDPP(&(*conn)->err_handle), /* hndlpp */
	                    OCI_HTYPE_ERROR,                    /* type */
	                    0,                                  /* xtramem_sz */
	                    NULL                                /* usrmempp */
	                   );
	if (rv != OCI_SUCCESS) {
		err = oracle_format_error("OCIHandleAlloc OCI_HTYPE_ERROR failed", *conn, rv, error, error_size);
		goto done;
	}

	/* Initialize Service Handle */
	rv = OCIHandleAlloc(oracle_env_handle,                  /* parenth */
	                    CAST_DVOIDPP(&(*conn)->svc_handle), /* hndlpp */
	                    OCI_HTYPE_SVCCTX,                   /* type */
	                    0,                                  /* xtramem_sz */
	                    NULL                                /* usrmempp */
	                   );
	if (rv != OCI_SUCCESS) {
		err = oracle_format_error("OCIHandleAlloc OCI_HTYPE_SVCCTX failed", *conn, rv, error, error_size);
		goto done;
	}

	username = M_sql_driver_pool_get_username(pool);
	password = M_sql_driver_pool_get_password(pool);
	dsn      = M_buf_create();
	if (data->num_hosts == 0) {
		M_buf_add_str(dsn, data->dsn);
	} else {
		M_buf_add_str(dsn, "(DESCRIPTION=(ENABLE=BROKEN)(ADDRESS=(PROTOCOL=tcp)(HOST=");
		M_buf_add_str(dsn, data->hosts[host_idx].host);
		M_buf_add_str(dsn, ")(PORT=");
		M_buf_add_int(dsn, data->hosts[host_idx].port);
		M_buf_add_str(dsn, "))");
		M_buf_add_str(dsn, "(CONNECT_DATA=(SERVICE_NAME=");
		M_buf_add_str(dsn, data->service_name);
		M_buf_add_str(dsn, ")))");
	}

	rv = OCILogon2(oracle_env_handle,                /* envhp */
	               (*conn)->err_handle,              /* errhp */
	               &(*conn)->svc_handle,             /* svchpp */
	               (const OraText *)username,        /* username */
	               (ub4)M_str_len(username),         /* uname_len */
	               (const OraText *)password,        /* password */
	               (ub4)M_str_len(password),         /* passwd_len */
	               (const OraText *)M_buf_peek(dsn), /* dbname */
	               (ub4)M_buf_len(dsn),              /* dbname_len */
	               OCI_DEFAULT                       /* mode. We manage our own client-side statement cache, no need to do it here */
	              );
	if (rv != OCI_SUCCESS) {
		char msg[256];
		M_snprintf(msg, sizeof(msg), "OCILogon2(username='%s', ..., dbname='%s') failed", username, M_buf_peek(dsn));
		err = oracle_format_error(msg, *conn, rv, error, error_size);
		goto done;
	}

	(*conn)->is_connected = M_TRUE;

	/* Get version (XXX: should we call OCIServerVersion instead?) */
	rv = OCIServerRelease((*conn)->svc_handle,         /* hndlp */
	                      (*conn)->err_handle,         /* errhp */
	                      (OraText *)(*conn)->version, /* bufp */
	                      sizeof((*conn)->version),    /* bufsz */
	                      OCI_HTYPE_SVCCTX,            /* hndltype */
	                      &ver                         /* version */
	                     );
	if (rv != OCI_SUCCESS) {
		err = oracle_format_error("OCIServerRelease failed", *conn, rv, error, error_size);
		goto done;
	}

done:
	M_buf_cancel(dsn);
	if (err != M_SQL_ERROR_SUCCESS) {
		oracle_cb_disconnect(*conn);
		*conn = NULL;
		return err;
	}

	return M_SQL_ERROR_SUCCESS;
}


static const char *oracle_cb_serverversion(M_sql_driver_conn_t *conn)
{
	return conn->version;
}


static char *oracle_cb_queryformat(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	(void)conn;
	return M_sql_driver_queryformat(query, M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_COLON,
	                                num_params, num_rows,
	                                error, error_size);
}


static size_t oracle_cb_queryrowcnt(M_sql_conn_t *conn, size_t num_params_per_row, size_t num_rows)
{
	(void)conn;
	(void)num_params_per_row;
	return num_rows;
}


static void oracle_clear_driver_stmt(M_sql_driver_stmt_t *dstmt)
{
	size_t i;
	/* NOTE: se don't want to free dstmt->bind since we can reuse it */

	/* Clear result columns */
	for (i=0; i<dstmt->result_cols; i++) {
		switch (dstmt->result[i].type) {
			case M_SQL_DATA_TYPE_TEXT:
				/* Must be dynamic, free it */
				if (dstmt->result[i].type_size == 0 || dstmt->result[i].type_size > sizeof(dstmt->result[i].d.smalltext.data)) {
					M_free(dstmt->result[i].d.text.data); 
				}
				break;
			case M_SQL_DATA_TYPE_BINARY:
				/* Must be dynamic, free it */
				if (dstmt->result[i].type_size == 0 || dstmt->result[i].type_size > sizeof(dstmt->result[i].d.smallbinary.data)) {
					M_free(dstmt->result[i].d.binary.data); 
				}
				break;
			default:
				/* Nothing to free */
				break;
		}
	}
	M_free(dstmt->result);
	dstmt->result      = NULL;
	dstmt->result_cols = 0;
}


static void oracle_cb_prepare_destroy(M_sql_driver_stmt_t *dstmt)
{
	sword                rv;
	M_sql_conn_t        *conn;
	M_sql_driver_conn_t *dconn;

	if (dstmt == NULL)
		return;

	conn  = dstmt->conn;
	dconn = M_sql_driver_conn_get_conn(conn);

	rv = OCIStmtRelease(dstmt->stmt,       /* stmthp */
	                    dconn->err_handle, /* errhp */
	                    NULL,              /* key */
	                    0,                 /* keylen */
	                    OCI_DEFAULT        /* mode */
	                   );

	if (rv != OCI_SUCCESS) {
		char          error[256];
		M_sql_error_t err;
		err = oracle_format_error("OCIStmtRelease failed", dconn, rv, error, sizeof(error));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
	}

	oracle_clear_driver_stmt(dstmt);
	M_free(dstmt->bind); /* oracle_clear_driver_stmt() doesn't do this */
	M_free(dstmt);
}


static ub2 oracle_get_datatype(M_sql_data_type_t type)
{
	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
		case M_SQL_DATA_TYPE_INT16:
		case M_SQL_DATA_TYPE_INT32:
		case M_SQL_DATA_TYPE_INT64:
			/* XXX: Int64 is really only supported as of Oracle 11.2
			 *      need to detect server version and use SQLT_VNU
			 *      instead. */
			return SQLT_INT;

		case M_SQL_DATA_TYPE_TEXT:
			/* NOTE: Do not use SQLT_CHR or SQLT_STR, as it will right-trim whitespace.
			 *       This is very important for fields where we want to maintain trailing
			 *       spaces! */
			return SQLT_AFC;
		case M_SQL_DATA_TYPE_BINARY:
			return SQLT_LBI;
		case M_SQL_DATA_TYPE_UNKNOWN:
			break; /* WTF? */
	}
	return 0;
}


static sb4 oracle_bind_cb(dvoid *ictxp, OCIBind *bindp, ub4 iter, ub4 index, void **bufpp, ub4 *alenp, ub1 *piecep, void **indpp)
{
	oracle_bind_data_t  *data  = ictxp;
	M_sql_stmt_t        *stmt  = data->stmt;
	size_t               col   = data->idx;
	size_t               row   = iter;
	const char          *c8;
	const M_uint8       *cu8;

	(void)bindp;
	/* NOTE: Docs indicate index is the row, but testing seems to show iter is */
	(void)index;

	*piecep = OCI_ONE_PIECE;
	*indpp  = &data->ind;
	data->ind = 0; /* Not NULL */

	/* Handle NULL */
	if (M_sql_driver_stmt_bind_isnull(stmt, row, col)) {
		*bufpp    = NULL;
		*alenp    = 0;
		data->ind = -1; /* Indicate NULL */
		return OCI_CONTINUE;
	}

	switch (M_sql_driver_stmt_bind_get_type(stmt, row, col)) {
		case M_SQL_DATA_TYPE_BOOL:
			*bufpp = M_sql_driver_stmt_bind_get_bool_addr(stmt, row, col);
			*alenp = sizeof(M_bool);
			break;

		case M_SQL_DATA_TYPE_INT16:
			*bufpp = M_sql_driver_stmt_bind_get_int16_addr(stmt, row, col);
			*alenp = sizeof(M_int16);
			break;

		case M_SQL_DATA_TYPE_INT32:
			*bufpp = M_sql_driver_stmt_bind_get_int32_addr(stmt, row, col);
			*alenp = sizeof(M_int32);
			break;

		case M_SQL_DATA_TYPE_INT64:
			/* XXX: Int64 is really only supported as of Oracle 11.2
			 *      need to detect server version and use SQLT_VNU
			 *      instead. */
			*bufpp = M_sql_driver_stmt_bind_get_int64_addr(stmt, row, col);
			*alenp = sizeof(M_int64);
			break;

		case M_SQL_DATA_TYPE_TEXT:
			/* NOTE: Do not use SQLT_CHR or SQLT_STR, as it will right-trim whitespace.
			 *       This is very important for fields where we want to maintain trailing
			 *       spaces! */
			c8     = M_sql_driver_stmt_bind_get_text(stmt, row, col);
			*bufpp = M_CAST_OFF_CONST(char *, c8);
			*alenp = (ub4)M_sql_driver_stmt_bind_get_text_len(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_BINARY:
			cu8    = M_sql_driver_stmt_bind_get_binary(stmt, row, col);
			*bufpp = M_CAST_OFF_CONST(M_uint8 *, cu8);
			*alenp = (ub4)M_sql_driver_stmt_bind_get_binary_len(stmt, row, col);
			break;

		case M_SQL_DATA_TYPE_UNKNOWN: /* Silence warning, should never get this */
			break;

	}

	return OCI_CONTINUE;
}

static sb4 oracle_bind_noop_cb(dvoid *octxp, OCIBind *bindp, ub4 iter, ub4 index, void **bufpp, ub4 **alenpp, ub1 *piecep, dvoid **indpp, ub2 **rcodepp)
{
	(void)octxp;
	(void)bindp;
	(void)iter;
	(void)index;
	(void)bufpp;
	(void)alenpp;
	(void)piecep;
	(void)indpp;
	(void)rcodepp;
	return OCI_CONTINUE;
}


static M_sql_error_t oracle_bind_params(M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_error_t        err      = M_SQL_ERROR_SUCCESS;
	size_t               num_cols = M_sql_driver_stmt_bind_cnt(stmt);
	M_sql_driver_conn_t *dconn    = M_sql_driver_conn_get_conn(dstmt->conn);
	size_t               i;
	sword                rv;

	if (num_cols == 0)
		return M_SQL_ERROR_SUCCESS;

	if (dstmt->bind == NULL) {
		dstmt->bind = M_malloc_zero(sizeof(*dstmt->bind) * num_cols);
	}

	for (i = 0; i < num_cols; i++) {
		size_t            max_col_size = M_sql_driver_stmt_bind_get_max_col_size(stmt, i);
		M_sql_data_type_t type         = M_sql_driver_stmt_bind_get_col_type(stmt, i);
		ub2               oci_type     = oracle_get_datatype(type);

		if (oci_type == 0) {
			err = M_SQL_ERROR_PREPARE_INVALID;
			M_snprintf(error, error_size, "unable to dereference oracle datatype for col %zu", i);
			goto done;
		}

		/* For fixed-width types, overwrite the maximum size */
		switch (type) {
			case M_SQL_DATA_TYPE_BOOL:
				max_col_size = 1;
				break;
			case M_SQL_DATA_TYPE_INT16:
				max_col_size = 2;
				break;
			case M_SQL_DATA_TYPE_INT32:
				max_col_size = 4;
				break;
			case M_SQL_DATA_TYPE_INT64:
				max_col_size = 8;
				break;
			default:
				break;
		}

		/* The non-driver statement handle changes between calls, always re-set this */
		dstmt->bind[i].stmt     = stmt;

		/* If the statement has been bound before, we can reuse it if the type is the same and the
		 * previous reported max size is greater than or equal to the current calculated max column size */
		if (dstmt->bind[i].bind != NULL && dstmt->bind[i].type == type &&
		    max_col_size <= dstmt->bind[i].max_size) {
			continue;
		}

		dstmt->bind[i].type     = type;
		dstmt->bind[i].idx      = i;
		dstmt->bind[i].max_size = max_col_size;

		/* NOTE: we need to support data with lenghts > 64k, so we have to use
		 *       OCIBindDynamic as the length for OCIBindByPos (alenp) is 2 bytes */
		rv = OCIBindByPos(dstmt->stmt,                /* OCIStmt *stmtp   */
		                  &dstmt->bind[i].bind,       /* OCIBind **bindpp */
		                  dconn->err_handle,          /* OCIError *errhp  */
		                  (ub4)i+1,                   /* ub4 position     */
		                  NULL,                       /* dvoid *valuep - NULL on OCI_DATA_AT_EXEC */
		                  (sb4)max_col_size,          /* sb4 value_sz  - Maximum length (any row) */
		                  oci_type,                   /* ub2 dty          */
		                  NULL,                       /* dvoid *indp   - NULL on OCI_DATA_AT_EXEC */
		                  NULL,                       /* ub2 *alenp    - NULL on OCI_DATA_AT_EXEC */
		                  NULL,                       /* ub2 *rcodep   - NULL on OCI_DATA_AT_EXEC */
		                  (ub4)0,                     /* ub4 maxarr_len   */
		                  NULL,                       /* ub4 *curelep     */
		                  OCI_DATA_AT_EXEC            /* ub4 mode      - means we need to call OCIBindDynamic */
		                 );
		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIStmtPrepare2 failed", dconn, rv, error, error_size);
			goto done;
		}

		rv = OCIBindDynamic(dstmt->bind[i].bind,     /* bindp */
		                    dconn->err_handle,       /* errhp */
		                    (void *)&dstmt->bind[i], /* ictxp - in bind context  */
		                    oracle_bind_cb,          /* icbfp - in bind callback */
		                    (void *)&dstmt->bind[i], /* octxp - out bind context - not used */
		                    oracle_bind_noop_cb      /* ocbfp - out bind callback - not used - must be dummy */
		                   );
		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIBindDynamic failed", dconn, rv, error, error_size);
			goto done;
		}

	}

done:
	return err;
}


static M_sql_error_t oracle_cb_prepare(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_conn_t *dconn    = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err      = M_SQL_ERROR_SUCCESS;
	const char          *query    = M_sql_driver_stmt_get_query(stmt);
	sword                rv;
	M_bool               new_stmt = (*driver_stmt) == NULL?M_TRUE:M_FALSE;

//M_printf("Query |%s|\n", query);

	if (*driver_stmt == NULL) {
		ub2 stmttype;

		/* Allocate a new handle */
		*driver_stmt         = M_malloc_zero(sizeof(**driver_stmt));
		(*driver_stmt)->conn = conn;

		rv = OCIStmtPrepare2(dconn->svc_handle,      /* svchp */
		                     &(*driver_stmt)->stmt,  /* stmthp */
		                     dconn->err_handle,      /* errhp */
		                     (const OraText *)query, /* stmttext */
		                     (ub4)M_str_len(query),  /* stmt_len */
		                     NULL,                   /* key */
		                     0,                      /* key_len */
		                     OCI_NTV_SYNTAX,         /* language */
		                     OCI_DEFAULT             /* mode */
		                    );

		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIStmtPrepare2 failed", dconn, rv, error, error_size);
			goto done;
		}

		rv = OCIAttrGet((*driver_stmt)->stmt, OCI_HTYPE_STMT, &stmttype, (ub4 *)0, OCI_ATTR_STMT_TYPE, dconn->err_handle);
 		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIAttrGet OCI_HTYPE_STMT failed", dconn, rv, error, error_size);
			goto done;
		}

		(*driver_stmt)->is_query = (stmttype == OCI_STMT_SELECT)?M_TRUE:M_FALSE;
	} else {
		/* Clear any existing data so we can reuse the handle */
		oracle_clear_driver_stmt(*driver_stmt);
	}

	/* Bind parameters */
	err = oracle_bind_params(*driver_stmt, stmt, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS)
		goto done;

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		if (*driver_stmt != NULL && new_stmt) {
			oracle_cb_prepare_destroy(*driver_stmt);
			*driver_stmt = NULL;
		}
	}

	return err;
}


static M_sql_error_t oracle_type_to_mtype(M_sql_driver_conn_t *dconn, OCIParam *colhnd, M_sql_data_type_t *type, size_t *max_len, char *error, size_t error_size)
{
	sword rv;
	ub2   datatype  = 0;
	sb2   precision = 0; /*!< Implicit describe via OCIStmtExecute() uses sb2 instead of ub1 */
	sb1   scale     = 0;
	ub2   max_width = 0;

	/* Get Datatype */
	rv = OCIAttrGet(colhnd,             /* trgthndlp */
	                OCI_DTYPE_PARAM,    /* trghndltyp */
	                &datatype,          /* attributep */
	                NULL,               /* sizep */
	                OCI_ATTR_DATA_TYPE, /* attrtype */
	                dconn->err_handle   /* errhp */
	               );
	if (rv != OCI_SUCCESS) {
		return oracle_format_error("OCIAttrGet OCI_ATTR_DATA_TYPE failed", dconn, rv, error, error_size);
	}

	switch (datatype) {
		case SQLT_INT:
		case SQLT_VNU:
		case SQLT_NUM:
			/* Get Precision */
			rv = OCIAttrGet(colhnd,             /* trgthndlp */
			                OCI_DTYPE_PARAM,    /* trghndltyp */
			                &precision,         /* attributep */
			                NULL,               /* sizep */
			                OCI_ATTR_PRECISION, /* attrtype */
			                dconn->err_handle   /* errhp */
			               );
			if (rv != OCI_SUCCESS) {
				return oracle_format_error("OCIAttrGet OCI_ATTR_PRECISION failed", dconn, rv, error, error_size);
			}

			/* Get Scale */
			rv = OCIAttrGet(colhnd,             /* trgthndlp */
			                OCI_DTYPE_PARAM,    /* trghndltyp */
			                &scale,             /* attributep */
			                NULL,               /* sizep */
			                OCI_ATTR_SCALE,     /* attrtype */
			                dconn->err_handle   /* errhp */
			               );
			if (rv != OCI_SUCCESS) {
				return oracle_format_error("OCIAttrGet OCI_ATTR_SCALE failed", dconn, rv, error, error_size);
			}

			/* Unknown, use text */
			if (scale != 0) {
				*type    = M_SQL_DATA_TYPE_TEXT;
				*max_len = 128;
				break;
			}

			if (precision == 1) {
				*type = M_SQL_DATA_TYPE_BOOL;
			} else if (precision == 5) {
				*type = M_SQL_DATA_TYPE_INT16;
			} else if (precision == 10) {
				*type = M_SQL_DATA_TYPE_INT32;
			} else {
				*type = M_SQL_DATA_TYPE_INT64;
			}
			break;

		case SQLT_CHR:
		case SQLT_STR:
			/* Get Max Width */
			rv = OCIAttrGet(colhnd,             /* trgthndlp */
			                OCI_DTYPE_PARAM,    /* trghndltyp */
			                &max_width,         /* attributep */
			                NULL,               /* sizep */
			                OCI_ATTR_DATA_SIZE, /* attrtype */
			                dconn->err_handle   /* errhp */
			               );
			if (rv != OCI_SUCCESS) {
				return oracle_format_error("OCIAttrGet OCI_ATTR_DATA_SIZE failed", dconn, rv, error, error_size);
			}

			if (max_width > 4000) {
				*max_len = 0;
			} else {
				*max_len = max_width;
			}
			*type = M_SQL_DATA_TYPE_TEXT;
			break;

		case SQLT_BIN:
		case SQLT_LVB:
		case SQLT_LBI:
		case SQLT_BLOB:
			/* Get Max Width */
			rv = OCIAttrGet(colhnd,             /* trgthndlp */
			                OCI_DTYPE_PARAM,    /* trghndltyp */
			                &max_width,         /* attributep */
			                NULL,               /* sizep */
			                OCI_ATTR_DATA_SIZE, /* attrtype */
			                dconn->err_handle   /* errhp */
			               );
			if (rv != OCI_SUCCESS) {
				return oracle_format_error("OCIAttrGet OCI_ATTR_DATA_SIZE failed", dconn, rv, error, error_size);
			}
			if (max_width > 2000) {
				*max_len = 0;
			} else {
				*max_len = max_width;
			}
			*type = M_SQL_DATA_TYPE_BINARY;
			break;

		default:
			/* Convert all others to text, with max size = 128 */
			*max_len = 128;
			*type = M_SQL_DATA_TYPE_TEXT;
			break;
	}

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t oracle_fetch_result_metadata(M_sql_driver_conn_t *dconn, M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	sword         rv;
	ub4           num_cols;
	ub4           i;
	M_sql_error_t err    = M_SQL_ERROR_SUCCESS;
	OCIParam     *colhnd = NULL;

	/* Get Column Count */
	rv  = OCIAttrGet(dstmt->stmt,          /* trgthndlp */
	                 OCI_HTYPE_STMT,       /* trghndltyp */
	                 &num_cols,            /* attributep */
	                 NULL,                 /* sizep */
	                 OCI_ATTR_PARAM_COUNT, /* attrtype */
	                 dconn->err_handle     /* errhp */
	                );
	if (rv != OCI_SUCCESS && rv != OCI_NO_DATA) {
		err = oracle_format_error("OCIAttrGet OCI_ATTR_PARAM_COUNT failed", dconn, rv, error, error_size);
		goto done;
	}

	M_sql_driver_stmt_result_set_num_cols(stmt, num_cols);
	if (num_cols == 0)
		goto done;

	for (i=0; i<num_cols; i++) {
		size_t            max_len  = 0;
		OraText          *name     = NULL;
		ub4               name_len = 0;
		char              name_str[256];
		M_sql_data_type_t mtype    = 0;

		/* Get column parameter handle */
		rv = OCIParamGet(dstmt->stmt            /* hndlp */,
		                 OCI_HTYPE_STMT,        /* htype */
		                 dconn->err_handle,     /* errhp */
		                 CAST_DVOIDPP(&colhnd), /* parmdpp */
		                 i+1 /* pos */
		                );
		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIParamGet failed", dconn, rv, error, error_size);
			goto done;
		}

		/* Get Name */
		rv = OCIAttrGet(colhnd,             /* trgthndlp */
		                OCI_DTYPE_PARAM,    /* trghndltyp */
		                &name,              /* attributep */
		                &name_len,          /* sizep */
		                OCI_ATTR_NAME,      /* attrtype */
		                dconn->err_handle   /* errhp */
		               );
		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIAttrGet OCI_ATTR_NAME failed", dconn, rv, error, error_size);
			goto done;
		}

		err = oracle_type_to_mtype(dconn, colhnd, &mtype, &max_len, error, error_size);
		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		/* The Name may not be null-terminated, copy it to a buffer and terminate it */
		if (name_len >= sizeof(name_str))
			name_len = sizeof(name_str)-1;
		M_mem_copy(name_str, name, name_len);
		name_str[name_len] = 0;

		M_sql_driver_stmt_result_set_col_name(stmt, i, name_str);
		M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, max_len);

		OCIDescriptorFree(colhnd, OCI_DTYPE_PARAM);
		colhnd = NULL;
	}

done:
	if (colhnd != NULL) {
		OCIDescriptorFree(colhnd, OCI_DTYPE_PARAM);
	}
	return err;
}


static sb4 oracle_fetch_oversized(dvoid *octxp, OCIDefine *defnp, ub4 iter, dvoid **bufpp, ub4 **alenpp, ub1 *piecep, dvoid **indpp, ub2 **rcodep)
{
	oracle_result_data_t *data = octxp;

/* NOTE: we tried using a minimum size of 256 bytes, but it appeared to trigger an issue
 *       inside of the OCI module that would actually corrupt the data.  Any size >= 4096
 *       appeared to be ok, however */
#define ORACLE_MIN_BLOB (16 * 1024)

	(void)defnp;
	(void)iter;
	(void)piecep;

	*rcodep = NULL;
	*indpp  = &data->ind;

	if (*piecep == OCI_FIRST_PIECE || *piecep == OCI_ONE_PIECE) {
		if (data->type == M_SQL_DATA_TYPE_TEXT) {
			data->d.text.written_len = 0;
			data->d.text.last_len    = 0;
		} else {
			data->d.binary.written_len = 0;
			data->d.binary.last_len    = 0;
		}
	}

	if (data->type == M_SQL_DATA_TYPE_TEXT) {
		data->d.text.written_len += data->d.text.last_len;

		if (data->d.text.written_len == data->d.text.data_alloc) {
			data->d.text.data_alloc   = M_size_t_round_up_to_power_of_two(M_MAX(ORACLE_MIN_BLOB, data->d.text.data_alloc+1));
			data->d.text.data         = M_realloc(data->d.text.data, data->d.text.data_alloc);
		}

		data->d.text.last_len     = (ub4)(data->d.text.data_alloc - data->d.text.written_len);
		*bufpp                    = data->d.text.data + data->d.text.written_len;
		*alenpp                   = &data->d.text.last_len;
	} else {
		data->d.binary.written_len += data->d.binary.last_len;

		if (data->d.binary.written_len == data->d.binary.data_alloc) {
			data->d.binary.data_alloc   = M_size_t_round_up_to_power_of_two(M_MAX(ORACLE_MIN_BLOB, data->d.binary.data_alloc+1));
			data->d.binary.data         = M_realloc(data->d.binary.data, data->d.binary.data_alloc);
		}

		data->d.binary.last_len     = (ub4)(data->d.binary.data_alloc - data->d.binary.written_len);
		*bufpp                      = data->d.binary.data + data->d.binary.written_len;
		*alenpp                     = &data->d.binary.last_len;
	}

	return OCI_CONTINUE;
}



static M_sql_error_t oracle_define_results(M_sql_driver_conn_t *dconn, M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	size_t num_cols = M_sql_stmt_result_num_cols(stmt);
	size_t i;


	dstmt->result      = M_malloc_zero(sizeof(*dstmt->result) * num_cols);
	dstmt->result_cols = num_cols;
	for (i=0; i<num_cols; i++) {
		void             *valuep    = NULL; /* Pointer to buffer to store result */
		sb4               value_sz  = 0;    /* Maximum size of result (buffer size) */
		ub2               dty       = 0;    /* Data type to retrieve as */
		ub4               mode      = OCI_DEFAULT; /* Mode, OCI_DEFAULT or OCI_DYNAMIC_FETCH for huge values */
		ub2              *rlenp     = NULL; /* Pointer to output length variable */
		sword             rv;

		dstmt->result[i].idx  = i;
		dstmt->result[i].stmt = stmt;
		dstmt->result[i].type = M_sql_stmt_result_col_type(stmt, i, &dstmt->result[i].type_size);

		switch (dstmt->result[i].type) {
			case M_SQL_DATA_TYPE_BOOL:
				valuep   = &dstmt->result[i].d.b.data;
				value_sz = sizeof(dstmt->result[i].d.b.data);
				dty      = SQLT_INT;
				rlenp    = &dstmt->result[i].d.b.len;
				break;

			case M_SQL_DATA_TYPE_INT16:
				valuep   = &dstmt->result[i].d.i16.data;
				value_sz = sizeof(dstmt->result[i].d.i16.data);
				dty      = SQLT_INT;
				rlenp    = &dstmt->result[i].d.i16.len;
				break;

			case M_SQL_DATA_TYPE_INT32:
				valuep   = &dstmt->result[i].d.i32.data;
				value_sz = sizeof(dstmt->result[i].d.i32.data);
				dty      = SQLT_INT;
				rlenp    = &dstmt->result[i].d.i32.len;
				break;

			case M_SQL_DATA_TYPE_INT64:
				valuep   = &dstmt->result[i].d.i64.data;
				value_sz = sizeof(dstmt->result[i].d.i64.data);
				dty      = SQLT_INT;
				rlenp    = &dstmt->result[i].d.i64.len;
				break;

			case M_SQL_DATA_TYPE_TEXT:
				dty      = SQLT_STR;
				if (dstmt->result[i].type_size != 0 && dstmt->result[i].type_size <= sizeof(dstmt->result[i].d.smalltext.data)) {
					valuep   = &dstmt->result[i].d.smalltext.data;
					value_sz = sizeof(dstmt->result[i].d.smalltext.data);
					rlenp    = &dstmt->result[i].d.smalltext.len;
					break;
				}
				value_sz = MINSB4MAXVAL;
				mode     = OCI_DYNAMIC_FETCH;
				break;

			case M_SQL_DATA_TYPE_BINARY:
				dty      = SQLT_LBI;
				if (dstmt->result[i].type_size != 0 && dstmt->result[i].type_size <= sizeof(dstmt->result[i].d.smallbinary.data)) {
					valuep   = &dstmt->result[i].d.smallbinary.data;
					value_sz = sizeof(dstmt->result[i].d.smallbinary.data);
					rlenp    = &dstmt->result[i].d.smallbinary.len;
					break;
				}
				value_sz = MINSB4MAXVAL;
				mode     = OCI_DYNAMIC_FETCH;
				break;

			default:
				M_snprintf(error, error_size, "Result column %zu unrecognized data type", i);
				return M_SQL_ERROR_QUERY_FAILURE;
		}

		rv = OCIDefineByPos(dstmt->stmt,              /* stmtp    */
		                    &dstmt->result[i].define, /* defnpp   */
		                    dconn->err_handle,        /* errhp    */
		                    (ub4)(i+1),               /* position */
		                    valuep,                   /* valuep   */
		                    value_sz,                 /* value_sz */
		                    dty,                      /* dty      */
		                    &dstmt->result[i].ind,    /* indp     */
		                    rlenp,                    /* rlenp    */
		                    NULL,                     /* rcodep   */
		                    mode);                    /* mode     */
		if (rv != OCI_SUCCESS) {
			return oracle_format_error("OCIDefineByPos failed", dconn, rv, error, error_size);
		}

		if (mode == OCI_DYNAMIC_FETCH) {
			rv = OCIDefineDynamic(dstmt->result[i].define, /* defnp */
			                      dconn->err_handle,       /* errhp */
			                      &dstmt->result[i],       /* octxp */
			                      oracle_fetch_oversized   /* ocbfp */
			                     );
			if (rv != OCI_SUCCESS) {
				return oracle_format_error("OCIDefineDynamic failed", dconn, rv, error, error_size);
			}
		}
	}

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t oracle_cb_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size)
{
	M_sql_driver_stmt_t *dstmt  = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t *dconn  = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err    = M_SQL_ERROR_SUCCESS;
	ub4                  iters;
	sword                rv;

	/* We'll try to execute all that are bound */
	*rows_executed = M_sql_driver_stmt_bind_rows(stmt);

	if (dstmt->is_query) {
		iters = 0;
	} else {
		iters = (ub4)M_MAX(*rows_executed, 1);
	}

	rv = OCIStmtExecute(dconn->svc_handle,
	                    dstmt->stmt,
	                    dconn->err_handle,
	                    (ub4)iters,
	                    (ub4)0,
	                    (CONST OCISnapshot *)NULL,
	                    (OCISnapshot *)NULL,
	                    M_sql_driver_conn_in_trans(conn)?OCI_DEFAULT:OCI_COMMIT_ON_SUCCESS);

	if (rv != OCI_SUCCESS && rv != OCI_NO_DATA) {
		err = oracle_format_error("OCIStmtExecute failed", dconn, rv, error, error_size);
		goto done;
	}

	if (*rows_executed > 1) {
		/* It is not clear from the docs if you could get a 'partial' insert that returns success.
		 * So we are going to sanity check this, and assume an error is a constraint violation which
		 * the caller knows means they need to split and repeat. */
		ub4 num_errs = 0;
		OCIAttrGet(dstmt->stmt, OCI_HTYPE_STMT, &num_errs, 0, OCI_ATTR_NUM_DML_ERRORS, dconn->err_handle);
		if (num_errs) {
			M_snprintf(error, error_size, "OCI array operation had one or more row failures");
			err = M_SQL_ERROR_QUERY_CONSTRAINT;
			goto done;
		}
	}

	if (dstmt->is_query) {
		/* Get column count, names, types */
		err = oracle_fetch_result_metadata(dconn, dstmt, stmt, error, error_size);
		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		/* Define output parameters for storing results */
		err = oracle_define_results(dconn, dstmt, stmt, error, error_size);
		if (err != M_SQL_ERROR_SUCCESS)
			goto done;

		/* Tell Oracle to prefetch rows to reduce network round-trips. Should improve performance of large queries */
		iters = (ub4)M_sql_driver_stmt_get_requested_row_cnt(stmt);
		if (iters == 0) /* User didn't specify, set sane default */
			iters = 1000;
		rv    = OCIAttrSet(dstmt->stmt,
		                   OCI_HTYPE_STMT,
		                   (dvoid *)&iters,
		                   (ub4)sizeof(iters),
		                   OCI_ATTR_PREFETCH_ROWS,
		                   dconn->err_handle
		                  );
		if (rv != OCI_SUCCESS) {
			err = oracle_format_error("OCIAttrSet OCI_ATTR_PREFECH_ROWS failed", dconn, rv, error, error_size);
			goto done;
		}

		/* We need to call fetch at least once */
		err = M_SQL_ERROR_SUCCESS_ROW;
	} else {
		/* Retrieve the affected row count */
		iters = 0;
		rv    = OCIAttrGet(dstmt->stmt,         /* trgthndlp */
		                   OCI_HTYPE_STMT,      /* trghndltyp */
		                   &iters,              /* attributep */
		                   NULL,                /* sizep */
		                   OCI_ATTR_ROW_COUNT,  /* attrtype */
		                   dconn->err_handle    /* errhp */
		                  );
		if (rv != OCI_SUCCESS && rv != OCI_NO_DATA) {
			err = oracle_format_error("OCIAttrGet OCI_ATTR_ROW_COUNT failed", dconn, rv, error, error_size);
			goto done;
		}
		M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)iters);
	}

done:
	/* No need for result metadata if all has been fetched or an error has occurred */
	if (err != M_SQL_ERROR_SUCCESS_ROW)
		oracle_clear_driver_stmt(dstmt);
	return err;
}


/* XXX: Fetch Cancel ? */

static M_sql_error_t oracle_cb_fetch(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_stmt_t *dstmt = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	size_t               num_cols;
	size_t               i;
	sword                rv;

	rv = OCIStmtFetch2(dstmt->stmt,      /* stmthp */
	                  dconn->err_handle, /* errhp */
	                  1,                 /* nrows */
	                  OCI_FETCH_NEXT,    /* orientation */
	                  0,                 /* fetchOffset */
	                  OCI_DEFAULT        /* mode */
	                 );

	/* Fetch is complete */
	if (rv == OCI_NO_DATA) {
		oracle_clear_driver_stmt(dstmt);
		return M_SQL_ERROR_SUCCESS;
	}

	/* Failure */
	if (rv != OCI_SUCCESS) {
		oracle_clear_driver_stmt(dstmt);
		return oracle_format_error("OCIStmtFetch2 failed", dconn, rv, error, error_size);
	}

	num_cols = M_sql_stmt_result_num_cols(stmt);
	for (i=0; i<num_cols; i++) {
		M_buf_t *buf = M_sql_driver_stmt_result_col_start(stmt);

		/* Is NULL */
		if (dstmt->result[i].ind == -1)
			continue;

		switch (dstmt->result[i].type) {
			case M_SQL_DATA_TYPE_BOOL:
				M_buf_add_int(buf, dstmt->result[i].d.b.data);
				break;
			case M_SQL_DATA_TYPE_INT16:
				M_buf_add_int(buf, dstmt->result[i].d.i16.data);
				break;
			case M_SQL_DATA_TYPE_INT32:
				M_buf_add_int(buf, dstmt->result[i].d.i32.data);
				break;
			case M_SQL_DATA_TYPE_INT64:
				M_buf_add_int(buf, dstmt->result[i].d.i64.data);
				break;
			case M_SQL_DATA_TYPE_TEXT:
				if (dstmt->result[i].type_size == 0 || dstmt->result[i].type_size > sizeof(dstmt->result[i].d.smalltext.data)) {
					dstmt->result[i].d.text.written_len += dstmt->result[i].d.text.last_len;
					M_buf_add_bytes(buf, (M_uint8 *)dstmt->result[i].d.text.data, dstmt->result[i].d.text.written_len);
					break;
				}
				M_buf_add_bytes(buf, (M_uint8 *)dstmt->result[i].d.smalltext.data, dstmt->result[i].d.smalltext.len);
				break;
			case M_SQL_DATA_TYPE_BINARY:
				if (dstmt->result[i].type_size == 0 || dstmt->result[i].type_size > sizeof(dstmt->result[i].d.smallbinary.data)) {
					dstmt->result[i].d.binary.written_len += dstmt->result[i].d.binary.last_len;
					M_buf_add_bytes(buf, (M_uint8 *)dstmt->result[i].d.binary.data, dstmt->result[i].d.binary.written_len);
					break;
				}
				M_buf_add_bytes(buf, (M_uint8 *)dstmt->result[i].d.smallbinary.data, dstmt->result[i].d.smallbinary.len);
				break;
			default:
				M_snprintf(error, error_size, "unhandled column %zu", i);
				return M_SQL_ERROR_QUERY_FAILURE;
		}

		/* All columns with data require NULL termination, even binary.  Otherwise its considered a NULL column. */
		M_buf_add_byte(buf, 0); /* Manually add NULL terminator */
	}
	M_sql_driver_stmt_result_row_finish(stmt);

	return M_SQL_ERROR_SUCCESS_ROW;
}


static M_sql_error_t oracle_cb_begin(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_stmt_t          *stmt;
	const char            *iso;
	char                   query[256];
	M_sql_error_t          err   = M_SQL_ERROR_QUERY_FAILURE;

	if (isolation == M_SQL_ISOLATION_SNAPSHOT)
		isolation = M_SQL_ISOLATION_SERIALIZABLE;
	if (isolation == M_SQL_ISOLATION_READUNCOMMITTED)
		isolation = M_SQL_ISOLATION_READCOMMITTED;

	iso = M_sql_driver_isolation2str(isolation);

	M_snprintf(query, sizeof(query), "SET TRANSACTION ISOLATION LEVEL %s", iso);

	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET ISOLATION %s failed: %s: %s", iso, M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	}
	M_sql_stmt_destroy(stmt);

	/* Implicitly begins */

	return err;
}


static M_sql_error_t oracle_cb_rollback(M_sql_conn_t *conn)
{
	M_sql_error_t        err   = M_SQL_ERROR_SUCCESS;
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	sword                rv;

	rv = OCITransRollback(dconn->svc_handle, dconn->err_handle, OCI_DEFAULT);
	if (rv != OCI_SUCCESS) {
		char error[256];
		err = oracle_format_error("OCITransRollback failed", dconn, rv, error, sizeof(error));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
	}

	return err;
}


static M_sql_error_t oracle_cb_commit(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_error_t        err   = M_SQL_ERROR_SUCCESS;
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);
	sword                rv;

	rv = OCITransCommit(dconn->svc_handle, dconn->err_handle, OCI_DEFAULT);
	if (rv != OCI_SUCCESS) {
		err = oracle_format_error("OCITransCommit failed", dconn, rv, error, error_size);
	}

	return err;
}


static M_sql_driver_t M_sql_oracle = {
	M_SQL_DRIVER_VERSION,         /* Driver/Module subsystem version */
	"oracle",                     /* Short name of module */
	"Oracle/OCI driver for mstdlib",  /* Display name of module */
	"1.0.0",                      /* Internal module version */

	NULL,                          /* Callback used for getting connection-specific flags */
	oracle_cb_init,                /* Callback used for module initialization. */
	oracle_cb_destroy,             /* Callback used for module destruction/unloading. */
	oracle_cb_createpool,          /* Callback used for pool creation */
	oracle_cb_destroypool,         /* Callback used for pool destruction */
	oracle_cb_connect,             /* Callback used for connecting to the db */
	oracle_cb_serverversion,       /* Callback used to get the server name/version string */
	oracle_cb_connect_runonce,     /* Callback used after connection is established, but before first query to set run-once options. */
	oracle_cb_disconnect,          /* Callback used to disconnect from the db */
	oracle_cb_queryformat,         /* Callback used for reformatting a query to the sql db requirements */
	oracle_cb_queryrowcnt,         /* Callback used for determining how many rows will be processed by the current execution (chunking rows) */
	oracle_cb_prepare,             /* Callback used for preparing a query for execution */
	oracle_cb_prepare_destroy,     /* Callback used to destroy the driver-specific prepared statement handle */
	oracle_cb_execute,             /* Callback used for executing a prepared query */
	oracle_cb_fetch,               /* Callback used to fetch result data/rows from server */
	oracle_cb_begin,               /* Callback used to begin a transaction */
	oracle_cb_rollback,            /* Callback used to rollback a transaction */
	oracle_cb_commit,              /* Callback used to commit a transaction */
	oracle_cb_datatype,            /* Callback used to convert to data type for server */
	NULL,                          /* Callback used to append additional data to the Create Table query string */
	oracle_cb_append_updlock,      /* Callback used to append row-level locking data */
	oracle_cb_append_bitop,        /* Callback used to append a bit operation */
	oracle_cb_rewrite_indexname,   /* Callback used to rewrite an index name to comply with DB requirements */

	NULL,                          /* Handle for loaded driver - must be initialized to NULL */
};

/*! Defines function that references M_sql_driver_t M_sql_##name for module loading */
M_SQL_DRIVER(oracle)
