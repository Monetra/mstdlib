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
#include <mysql.h>
#include "mysql_shared.h"

/* Don't use m_defs_int.h, since we need to be able to build this as an external plugin. */
#ifndef M_CAST_OFF_CONST
#define M_CAST_OFF_CONST(type, var) ((type)((M_uintptr)var))
#endif


typedef struct {
	char              db[64];
	char              socketpath[1024];
	M_sql_hostport_t *hosts;
	size_t            num_hosts;
	M_bool            ssl;
	M_sql_isolation_t max_isolation;
	M_hash_dict_t    *settings;
} mysql_connpool_data_t;

struct M_sql_driver_connpool {
	mysql_connpool_data_t primary;
	mysql_connpool_data_t readonly;
};

struct M_sql_driver_conn {
	MYSQL *conn;
};


typedef struct {
	MYSQL_BIND    *bind;
	unsigned long *col_length;
	my_bool       *col_isnull;
	my_bool       *col_error;

	size_t         num_cols;
} M_sql_driver_stmt_resultcols_t;


struct M_sql_driver_stmt {
	MYSQL_STMT                    *stmt;         /*!< MySQL Statement handle  */
	MYSQL_BIND                    *bind_params;  /*!< Parameters bound to statement handle */

	M_sql_driver_stmt_resultcols_t res;          /*!< Result metadata - buffers to hold result data, if any. */
};


/* On windows, mysql_thread_end() might be an stdcall instead of cdecl function,
 * so wrap it since we can't pass an stdcall function as a pointer to a cdecl
 * callback */
static void mysql_thread_end_wrapper(void)
{
	mysql_thread_end();
}


static M_bool mysql_cb_init(char *error, size_t error_size)
{
	if (mysql_library_init(0, NULL, NULL) != 0) {
		M_snprintf(error, error_size, "Could not initialize mysql library");
		return M_FALSE;
	}

	/* XXX: Need M_thread_initializer_insert to call mysql_thread_init() */
	M_thread_destructor_insert(mysql_thread_end_wrapper);
	return M_TRUE;
}


static void mysql_cb_destroy(void)
{
	/* XXX: Need M_thread_initializer_remove to remove call to mysql_thread_init() */

	M_thread_destructor_remove(mysql_thread_end_wrapper);
	mysql_library_end();
}


static const char * const mysql_unix_socket_paths[] =
{ 
	"/tmp/mysql.sock",              /* Default       */
	"/var/mysql/mysql.sock",        /* MacOSX        */
	"/var/lib/mysql/mysql.sock",    /* RedHat        */
	"/var/lib/mysql/mysqld.sock",   /* ??            */
	"/var/run/mysqld/mysqld.sock",  /* Debian/Ubuntu */
	"/var/run/mysqld/mysql.sock",   /* ??            */
	NULL
};


static const char *mysql_find_unix_socket(void)
{
	size_t i;
	for (i=0; mysql_unix_socket_paths[i] != NULL; i++) {
		if (M_fs_perms_can_access(mysql_unix_socket_paths[i], M_FS_FILE_MODE_WRITE|M_FS_FILE_MODE_READ) == M_FS_ERROR_SUCCESS)
			return mysql_unix_socket_paths[i];
	}
	return NULL;
}


static M_bool mysql_connpool_readconf(mysql_connpool_data_t *data, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	M_sql_connstr_params_t params[] = {
		{ "db",              M_SQL_CONNSTR_TYPE_ANY,      M_TRUE,   1,    31 },
		{ "socketpath",      M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,  1024 },
		{ "host",            M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,  1024 },
		{ "ssl",             M_SQL_CONNSTR_TYPE_BOOL,     M_FALSE,  0,     0 },
		{ "mysql_engine",    M_SQL_CONNSTR_TYPE_ALPHA,    M_FALSE,  1,    31 },
		{ "mysql_charset",   M_SQL_CONNSTR_TYPE_ALPHANUM, M_FALSE,  1,    31 },
		{ "max_isolation",   M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,    31 },

		{ NULL, 0, M_FALSE, 0, 0}
	};

	const char *const_temp;

	if (!M_sql_driver_validate_connstr(conndict, params, error, error_size)) {
		return M_FALSE;
	}

	if (M_str_isempty(M_hash_dict_get_direct(conndict, "socketpath")) && M_str_isempty(M_hash_dict_get_direct(conndict, "host"))) {
		M_snprintf(error, error_size, "must specify socketpath or host");
		return M_FALSE;
	}

	if (!M_str_isempty(M_hash_dict_get_direct(conndict, "socketpath")) && !M_str_isempty(M_hash_dict_get_direct(conndict, "host"))) {
		M_snprintf(error, error_size, "must specify only one of socketpath or host");
		return M_FALSE;
	}

	/* db */
	M_str_cpy(data->db, sizeof(data->db), M_hash_dict_get_direct(conndict, "db"));

	/* socketpath */
	const_temp = M_hash_dict_get_direct(conndict, "socketpath");
	if (!M_str_isempty(const_temp)) {
		if (M_str_caseeq(const_temp, "search")) {
			const_temp = mysql_find_unix_socket();
			if (const_temp == NULL) {
				M_snprintf(error, error_size, "unable to find unix socket path");
				return M_FALSE;
			}

			M_str_cpy(data->socketpath, sizeof(data->socketpath), const_temp);
		} else {
			char *temp;
			if (M_fs_path_norm(&temp, const_temp, M_FS_PATH_NORM_ABSOLUTE|M_FS_PATH_NORM_HOME, M_FS_SYSTEM_AUTO) != M_FS_ERROR_SUCCESS) {
				M_snprintf(error, error_size, "failed path normalization for '%s'", const_temp);
				return M_FALSE;
			}
			M_str_cpy(data->socketpath, sizeof(data->socketpath), temp);
			M_free(temp);
		}
	}

	/* host */
	const_temp = M_hash_dict_get_direct(conndict, "host");
	if (!M_str_isempty(const_temp)) {
		data->hosts = M_sql_driver_parse_hostport(const_temp, 3306, &data->num_hosts, error, error_size);
		if (data->hosts == NULL)
			return M_FALSE;
	}

	/* ssl - defaults to off */
	/* XXX - use me */
	const_temp = M_hash_dict_get_direct(conndict, "ssl");
	data->ssl  = M_str_istrue(const_temp);

	/* max_isolation - defaults to serializable */
	data->max_isolation = M_SQL_ISOLATION_SERIALIZABLE;
	const_temp = M_hash_dict_get_direct(conndict, "max_isolation");
	if (!M_str_isempty(const_temp)) {
		data->max_isolation = M_sql_driver_str2isolation(const_temp);
		if (data->max_isolation == M_SQL_ISOLATION_UNKNOWN) {
			M_snprintf(error, sizeof(error), "Unrecognized max_isolation '%s'", const_temp);
			return M_FALSE;
		}
	}
	if (data->max_isolation == M_SQL_ISOLATION_SNAPSHOT)
		data->max_isolation = M_SQL_ISOLATION_SERIALIZABLE;

	data->settings = M_hash_dict_duplicate(conndict);

	if (!data->hosts) {
		/* Unix socket */
		*num_hosts = 1;
	} else {
		*num_hosts = data->num_hosts;
	}

	return M_TRUE;
}


static M_bool mysql_cb_createpool(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	mysql_connpool_data_t  *data;

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

	return mysql_connpool_readconf(data, conndict, num_hosts, error, error_size);
}


static void mysql_cb_destroypool(M_sql_driver_connpool_t *dpool)
{
	if (dpool == NULL)
		return;

	M_free(dpool->primary.hosts);
	M_free(dpool->readonly.hosts);
	M_hash_dict_destroy(dpool->primary.settings);
	M_hash_dict_destroy(dpool->readonly.settings);
	M_free(dpool);
}


static mysql_connpool_data_t *mysql_get_driverpool_data(M_sql_conn_t *conn)
{
	M_sql_driver_connpool_t *dpool       = M_sql_driver_conn_get_dpool(conn);
	M_bool                   is_readonly = M_sql_driver_conn_is_readonly(conn);

	if (dpool == NULL)
		return NULL;

	if (is_readonly)
		return &dpool->readonly;
	return &dpool->primary;
}


static M_sql_error_t mysql_cb_connect(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size)
{
	M_sql_driver_connpool_t *dpool      = M_sql_driver_pool_get_dpool(pool);
	mysql_connpool_data_t   *data       = is_readonly_pool?&dpool->readonly:&dpool->primary;
	unsigned int             arg_u32;
	my_bool                  arg_b;
	const char              *socketpath = NULL;
	const char              *host       = NULL;
	unsigned short           port       = 0;
	M_sql_error_t            err        = M_SQL_ERROR_SUCCESS;

	*conn         = M_malloc_zero(sizeof(**conn));
	(*conn)->conn = mysql_init(NULL);

	if (!data->hosts) {
		socketpath = data->socketpath;

		arg_u32    = MYSQL_PROTOCOL_SOCKET;
		mysql_options((*conn)->conn, MYSQL_OPT_PROTOCOL, &arg_u32);
	} else {
		host       = data->hosts[host_idx].host;
		port       = data->hosts[host_idx].port;

		arg_u32    = MYSQL_PROTOCOL_TCP;
		mysql_options((*conn)->conn, MYSQL_OPT_PROTOCOL, &arg_u32);
	}

	/* Connect timeout, 5s */
	arg_u32 = 5;
	mysql_options((*conn)->conn, MYSQL_OPT_CONNECT_TIMEOUT, &arg_u32);

	/* Disable automatic reconnects since we set some connection options, we handle
	 * reconnects ourself */
	arg_b = 0;
	mysql_options((*conn)->conn, MYSQL_OPT_RECONNECT, &arg_b);

	/* XXX:
	 * MYSQL_OPT_SSL_CA
	 * MYSQL_OPT_SSL_CAPATH
	 * MYSQL_OPT_SSL_CERT
	 * MYSQL_OPT_SSL_CIPHER
	 * MYSQL_OPT_SSL_CRL
	 * MYSQL_OPT_SSL_CRLPATH
	 * MYSQL_OPT_SSL_KEY
	 * MYSQL_OPT_SSL_MODE
	 * MYSQL_OPT_TLS_VERSION
	 */

	if (mysql_real_connect((*conn)->conn, host,
	                         M_sql_driver_pool_get_username(pool),
	                         M_sql_driver_pool_get_password(pool),
	                         data->db, port,
	                         socketpath, 0) == NULL) {
		err = M_SQL_ERROR_CONN_FAILED;
		M_snprintf(error, error_size, "failed to connect: (%u) %s", mysql_errno((*conn)->conn), mysql_error((*conn)->conn));
		goto done;
	}

	/* Enable auto-commit, will disable it in transactions */
	if (mysql_autocommit((*conn)->conn, 1) != 0) {
		unsigned int merr = mysql_errno((*conn)->conn);
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(error, error_size, "failed to enable autocommit: (%u) %s", merr, mysql_error((*conn)->conn));
		goto done;
	}

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		mysql_close((*conn)->conn);
		M_free(*conn);
		*conn = NULL;
	}

	return err;
}


static const char *mysql_cb_serverversion(M_sql_driver_conn_t *conn)
{
	return mysql_get_server_info(conn->conn);
}


static void mysql_cb_disconnect(M_sql_driver_conn_t *conn)
{
	if (conn == NULL)
		return;
	if (conn->conn != NULL)
		mysql_close(conn->conn);
	M_free(conn);
}


static size_t mysql_num_process_rows(size_t num_rows)
{
#define MYSQL_MAX_PROCESS_ROWS 100
	return M_MIN(num_rows, MYSQL_MAX_PROCESS_ROWS);
}


static char *mysql_cb_queryformat(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	(void)conn;
	return M_sql_driver_queryformat(query, M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD,
	                                num_params, mysql_num_process_rows(num_rows),
	                                error, error_size);
}


static void mysql_clear_driver_stmt_resultmeta(M_sql_driver_stmt_t *driver_stmt)
{
	size_t i;

	if (driver_stmt == NULL || driver_stmt->res.bind == NULL)
		return;

	for (i=0; i<driver_stmt->res.num_cols; i++) {
		M_free(driver_stmt->res.bind[i].buffer);
	}
	M_free(driver_stmt->res.bind);
	M_free(driver_stmt->res.col_length);
	M_free(driver_stmt->res.col_isnull);
	M_free(driver_stmt->res.col_error);
	M_mem_set(&driver_stmt->res, 0, sizeof(driver_stmt->res));
}


static void mysql_clear_driver_stmt(M_sql_driver_stmt_t *driver_stmt)
{
	if (driver_stmt == NULL)
		return;
	M_free(driver_stmt->bind_params);
	driver_stmt->bind_params = NULL;
	mysql_clear_driver_stmt_resultmeta(driver_stmt);
}


static void mysql_cb_prepare_destroy(M_sql_driver_stmt_t *stmt)
{
	if (stmt == NULL)
		return;

	if (stmt->stmt != NULL)
		mysql_stmt_close(stmt->stmt);

	mysql_clear_driver_stmt(stmt);

	M_free(stmt);
}


static M_sql_error_t mysql_bind_params(M_sql_driver_stmt_t *driver_stmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_error_t err      = M_SQL_ERROR_SUCCESS;
	unsigned int  merr;
	size_t        num_rows = mysql_num_process_rows(M_sql_driver_stmt_bind_rows(stmt));
	size_t        num_cols = M_sql_driver_stmt_bind_cnt(stmt);
	size_t        row;
	size_t        i;

	if (num_rows == 0)
		return M_SQL_ERROR_SUCCESS;

	driver_stmt->bind_params = M_malloc_zero(sizeof(*driver_stmt->bind_params) * num_cols * num_rows);

	for (row = 0; row < num_rows; row++) {
		for (i = 0; i < num_cols; i++) {
			size_t         paramid = ((row * num_cols) + i);
			const M_uint8 *p8u;
			const char    *p8;

			driver_stmt->bind_params[paramid].error       = (my_bool *)0;
			driver_stmt->bind_params[paramid].length      = &driver_stmt->bind_params[paramid].buffer_length;
			driver_stmt->bind_params[paramid].is_unsigned = 0;
			driver_stmt->bind_params[paramid].is_null     = (my_bool *)0;

			switch (M_sql_driver_stmt_bind_get_type(stmt, row, i)) {
				case M_SQL_DATA_TYPE_BOOL:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_TINY;
					driver_stmt->bind_params[paramid].buffer        = M_sql_driver_stmt_bind_get_bool_addr(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer_length = sizeof(M_bool);
					break;
				case M_SQL_DATA_TYPE_INT16:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_SHORT;
					driver_stmt->bind_params[paramid].buffer        = M_sql_driver_stmt_bind_get_int16_addr(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer_length = sizeof(M_int16);
					break;
				case M_SQL_DATA_TYPE_INT32:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_LONG;
					driver_stmt->bind_params[paramid].buffer        = M_sql_driver_stmt_bind_get_int32_addr(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer_length = sizeof(M_int32);
					break;
				case M_SQL_DATA_TYPE_INT64:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_LONGLONG;
					driver_stmt->bind_params[paramid].buffer        = M_sql_driver_stmt_bind_get_int64_addr(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer_length = sizeof(M_int64);
					break;
				case M_SQL_DATA_TYPE_TEXT:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_STRING;
					p8 = M_sql_driver_stmt_bind_get_text(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer        = M_CAST_OFF_CONST(char *, p8);
					driver_stmt->bind_params[paramid].buffer_length = M_sql_driver_stmt_bind_get_text_len(stmt, row, i);
					break;
				case M_SQL_DATA_TYPE_BINARY:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_BLOB;
					p8u = M_sql_driver_stmt_bind_get_binary(stmt, row, i);
					driver_stmt->bind_params[paramid].buffer        = M_CAST_OFF_CONST(M_uint8 *, p8u);
					driver_stmt->bind_params[paramid].buffer_length = M_sql_driver_stmt_bind_get_binary_len(stmt, row, i);
					driver_stmt->bind_params[paramid].is_unsigned = 1;
					break;
				case M_SQL_DATA_TYPE_NULL:
					driver_stmt->bind_params[paramid].buffer_type   = MYSQL_TYPE_NULL;
					driver_stmt->bind_params[paramid].buffer        = NULL;
					driver_stmt->bind_params[paramid].buffer_length = 0;
					break;
				default:
					err = M_SQL_ERROR_INVALID_USE;
					M_snprintf(error, error_size, "Unknown parameter type for row %zu, col %zu", row, i);
					goto fail;
			}
		}
	}

	if (mysql_stmt_bind_param(driver_stmt->stmt, driver_stmt->bind_params) != 0) {
		merr = mysql_stmt_errno(driver_stmt->stmt);
		M_snprintf(error, error_size, "stmt bind failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
		err = mysql_resolve_error(NULL, (M_int32)merr);
		goto fail;
	}

fail:
	return err;
}


static M_sql_data_type_t mysql_type_to_mtype(const MYSQL_FIELD *field, size_t *max_len)
{
	*max_len = 0;

	switch (field->type) {
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		/* MySQL returns the wrong datatype (blob instead of string), but charsetnr
		 * returns 63 for binary data as per bug http://bugs.mysql.com/bug.php?id=11974
		 * and charsetnr doc: http://dev.mysql.com/doc/refman/5.0/en/c-api-datatypes.html
		 */
			*max_len = field->length;
			if (field->charsetnr != 63)
				return M_SQL_DATA_TYPE_TEXT;
			return M_SQL_DATA_TYPE_BINARY;

		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
			*max_len = field->length;
			return M_SQL_DATA_TYPE_TEXT;

		case MYSQL_TYPE_TINY:
			return M_SQL_DATA_TYPE_BOOL;
		case MYSQL_TYPE_SHORT:
			return M_SQL_DATA_TYPE_INT16;
		case MYSQL_TYPE_LONG:
			return M_SQL_DATA_TYPE_INT32;
		case MYSQL_TYPE_LONGLONG:
			return M_SQL_DATA_TYPE_INT64;
		default:
			break;
	}
	return M_SQL_DATA_TYPE_TEXT;
}


static M_sql_error_t mysql_fetch_result_metadata(M_sql_driver_stmt_t *driver_stmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	MYSQL_RES                      *res    = mysql_stmt_result_metadata(driver_stmt->stmt);
	M_sql_driver_stmt_resultcols_t *result = &driver_stmt->res;
	size_t                          i;
	const MYSQL_FIELD              *fields;
	M_sql_error_t                   err    = M_SQL_ERROR_SUCCESS;

	/* NULL is not an error, it simply means the query won't return these kind of results */
	if (res == NULL)
		goto done;

	result->num_cols   = mysql_num_fields(res);
	M_sql_driver_stmt_result_set_num_cols(stmt, result->num_cols);
	if (result->num_cols == 0)
		goto done;

	fields             = mysql_fetch_fields(res);
	result->bind       = M_malloc_zero(sizeof(*result->bind)       * result->num_cols);
	result->col_length = M_malloc_zero(sizeof(*result->col_length) * result->num_cols);
	result->col_isnull = M_malloc_zero(sizeof(*result->col_isnull) * result->num_cols);
	result->col_error  = M_malloc_zero(sizeof(*result->col_error)  * result->num_cols);

	for (i=0; i<result->num_cols; i++) {
		size_t            max_len = 0;
		M_sql_data_type_t mtype   = mysql_type_to_mtype(&fields[i], &max_len);

		M_sql_driver_stmt_result_set_col_name(stmt, i, fields[i].name);
		M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, max_len);

		/* Length of 0 is unknown, assume virtually unlimited */
		if (max_len == 0 && (mtype == M_SQL_DATA_TYPE_TEXT || mtype == M_SQL_DATA_TYPE_BINARY))
			max_len = SIZE_MAX;

		/* When we allocate the initial data buffer, we want to make sure we have
		 * enough room */
		if (mtype == M_SQL_DATA_TYPE_TEXT)
			max_len = M_MIN(max_len, 2048);

		if (mtype == M_SQL_DATA_TYPE_BINARY)
			max_len = M_MIN(max_len, 16384);

		/* Try to store as a native type where we support it */
		switch (fields[i].type) {
			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_TINY_BLOB:
			case MYSQL_TYPE_MEDIUM_BLOB:
			case MYSQL_TYPE_LONG_BLOB:
			case MYSQL_TYPE_STRING:
				result->bind[i].buffer_type   = (mtype == M_SQL_DATA_TYPE_TEXT)?MYSQL_TYPE_STRING:fields[i].type;
				result->bind[i].buffer_length = max_len;
				break;
			case MYSQL_TYPE_TINY:
				result->bind[i].buffer_type   = fields[i].type;
				result->bind[i].buffer_length = sizeof(M_bool);
				break;
			case MYSQL_TYPE_SHORT:
				result->bind[i].buffer_type   = fields[i].type;
				result->bind[i].buffer_length = sizeof(M_int16);
				break;
			case MYSQL_TYPE_LONG:
				result->bind[i].buffer_type   = fields[i].type;
				result->bind[i].buffer_length = sizeof(M_int32);
				break;
			case MYSQL_TYPE_LONGLONG:
				result->bind[i].buffer_type   = fields[i].type;
				result->bind[i].buffer_length = sizeof(M_int64);
				break;
			default:
				/* Data type is not directly supported, force conversion to a string */
				result->bind[i].buffer_type   = MYSQL_TYPE_STRING;
				if (max_len == 0 || max_len > 16394)
					max_len = 128;
				result->bind[i].buffer_length = max_len;
				break;
		}

		result->bind[i].buffer  = M_malloc_zero(result->bind[i].buffer_length);
		result->bind[i].is_null	= &result->col_isnull[i];
		result->bind[i].length  = &result->col_length[i];
		result->bind[i].error   = &result->col_error[i];
	}

	if (mysql_stmt_bind_result(driver_stmt->stmt, result->bind) != 0) {
		unsigned int merr = mysql_stmt_errno(driver_stmt->stmt);
		M_snprintf(error, error_size, "stmt bind result failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
		err = mysql_resolve_error(NULL, (M_int32)merr);
		goto done;
	}

done:
	if (res != NULL)
		mysql_free_result(res);

	return err;
}



static M_sql_error_t mysql_cb_prepare(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_conn_t *driver_conn = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err         = M_SQL_ERROR_SUCCESS;
	const char          *query       = M_sql_driver_stmt_get_query(stmt);
	unsigned int         merr;
	M_bool               new_stmt    = M_FALSE;

//M_printf("Query |%s|\n", query);

	if (*driver_stmt != NULL) {
		if (mysql_stmt_reset((*driver_stmt)->stmt) != 0) {
			merr = mysql_stmt_errno((*driver_stmt)->stmt);
			M_snprintf(error, error_size, "stmt reset failed: %u: %s", merr, mysql_stmt_error((*driver_stmt)->stmt));
			err = mysql_resolve_error(NULL, (M_int32)merr);
			goto fail;
		}

		mysql_clear_driver_stmt(*driver_stmt);
	}


	if (*driver_stmt == NULL) {
		new_stmt             = M_TRUE;
		*driver_stmt         = M_malloc_zero(sizeof(**driver_stmt));
		(*driver_stmt)->stmt = mysql_stmt_init(driver_conn->conn);

		if ((*driver_stmt)->stmt == NULL) {
			merr = mysql_errno(driver_conn->conn);
			M_snprintf(error, error_size, "stmt init failed: %u: %s", merr, mysql_error(driver_conn->conn));
			err = mysql_resolve_error(NULL, (M_int32)merr);
			goto fail;
		}

		if (mysql_stmt_prepare((*driver_stmt)->stmt, query, M_str_len(query)) != 0) {
			merr = mysql_stmt_errno((*driver_stmt)->stmt);
			M_snprintf(error, error_size, "stmt prepare failed: %u: %s", merr, mysql_stmt_error((*driver_stmt)->stmt));
			err = mysql_resolve_error(NULL, (M_int32)merr);
			goto fail;
		}

		/* Prepare succeeded, so its ok to cache this */
		new_stmt             = M_FALSE;
	}

	/* Bind parameters */
	err = mysql_bind_params(*driver_stmt, stmt, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS)
		goto fail;

	/* Get result metadata */
	err = mysql_fetch_result_metadata(*driver_stmt, stmt, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS)
		goto fail;

/* XXX: It appears STMT_ATTR_PREFETCH_ROWS may require a cursor, but that means
 * a server-side temporary table is created.  We need to check the communication
 * to see if the client buffers multiple rows at once or not */
#if 0 
	/* If there are columns, that means it is a query */
	if (M_sql_stmt_result_num_cols(stmt)) {
		unsigned long arg;
		arg = (unsigned long) CURSOR_TYPE_NO_CURSOR; /* CURSOR_TYPE_READ_ONLY; */
		if (mysql_stmt_attr_set((*driver_stmt)->stmt, STMT_ATTR_CURSOR_TYPE, &arg) != 0) {
			merr = mysql_stmt_errno((*driver_stmt)->stmt);
			M_snprintf(error, error_size, "stmt attr set CURSOR_TYPE=NO_CURSOR failed: %u: %s", merr, mysql_stmt_error((*driver_stmt)->stmt));
			err = mysql_rc_to_error(merr);
			goto fail;
		}
		arg = 1000; /* XXX: Get me from user-requested size! */
		if (mysql_stmt_attr_set(conn->stmt, STMT_ATTR_PREFETCH_ROWS, &arg) != 0) {
			merr = mysql_stmt_errno((*driver_stmt)->stmt);
			M_snprintf(error, error_size, "stmt attr set PREFETCH_ROWS=%lu failed: %u: %s", arg, merr, mysql_stmt_error((*driver_stmt)->stmt));
			err = mysql_rc_to_error(merr);
			goto fail;
		}
	}
#endif

fail:
	if (err != M_SQL_ERROR_SUCCESS) {
		if (*driver_stmt != NULL && new_stmt) {
			mysql_cb_prepare_destroy(*driver_stmt);
		}
		/* Set to NULL to let subsystem know to destroy the statement handle */
		*driver_stmt = NULL;
	}

	return err;
}


static M_sql_error_t mysql_cb_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size)
{
	M_sql_driver_stmt_t *driver_stmt = M_sql_driver_stmt_get_stmt(stmt);

	(void)conn;

	/* Get number of rows that are processed at once, supports
	 * comma-delimited values for inserting multiple rows. */
	*rows_executed = mysql_num_process_rows(M_sql_driver_stmt_bind_rows(stmt));

	if (mysql_stmt_execute(driver_stmt->stmt) != 0) {
		unsigned int merr = mysql_stmt_errno(driver_stmt->stmt);
		M_snprintf(error, error_size, "stmt execute failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
		return mysql_resolve_error(NULL, (M_int32)merr);
	}

	/* If we know there should be a result set, lets return ROW so it calls fetch */
	if (M_sql_stmt_result_num_cols(stmt) > 0)
		return M_SQL_ERROR_SUCCESS_ROW;

	M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)mysql_stmt_affected_rows(driver_stmt->stmt));

	return M_SQL_ERROR_SUCCESS;
}


/* XXX: Fetch Cancel ? */

static M_sql_error_t mysql_cb_fetch(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_stmt_t            *driver_stmt = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_stmt_resultcols_t *result      = &driver_stmt->res;
	M_sql_error_t                   err         = M_SQL_ERROR_SUCCESS_ROW;
	size_t                          i;
	int                             rv;
	M_bool                          rebind      = M_FALSE;

	(void)conn;

	/* NOTE: There is no reason to call this in a loop up to stmt->max_fetch_rows as when the
	 *       SQL subsystem realizes we returned less rows than that value, it will simply keep
	 *       calling us. */

	rv = mysql_stmt_fetch(driver_stmt->stmt);

	/* No more rows */
	if (rv == MYSQL_NO_DATA)
		return M_SQL_ERROR_SUCCESS;

	/* Error */
	if (rv == 1) {
		unsigned int merr = mysql_stmt_errno(driver_stmt->stmt);
		M_snprintf(error, error_size, "stmt fetch failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
		return mysql_resolve_error(NULL, (M_int32)merr);
	}

	/* Otherwise it is 0, or MYSQL_DATA_TRUNCATED, which we'll check as we fetch each column */

	/* Output the current row of data */
	for (i=0; i<result->num_cols; i++) {
		M_buf_t *buf = M_sql_driver_stmt_result_col_start(stmt);

		/* NULL column encountered, record nothing */
		if (result->col_isnull[i])
			continue;

		/* Buffer was too small, expand and re-fetch column ... and rebind so future rows can use
		 * that new size.
		 * NOTE: DO NOT add (rv == MYSQL_DATA_TRUNCATED && result->col_error[i]) to this check!
		 *       There are buggy MySQL client versions that do not set those indicators properly,
		 *       really according to the docs, the length check alone is pretty much guaranteed
		 *       to work. */
		if (result->col_length[i] > result->bind[i].buffer_length) {
			result->bind[i].buffer_length = result->col_length[i];
			result->bind[i].buffer        = M_realloc(result->bind[i].buffer, result->bind[i].buffer_length);
			rebind                        = M_TRUE;
			if (mysql_stmt_fetch_column(driver_stmt->stmt, &result->bind[i], (unsigned int)i, 0) != 0) {
				unsigned int merr = mysql_stmt_errno(driver_stmt->stmt);
				M_snprintf(error, error_size, "stmt fetch (oversized) column failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
				return mysql_resolve_error(NULL, (M_int32)merr);
			}
		}

		switch (result->bind[i].buffer_type) {
			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_TINY_BLOB:
			case MYSQL_TYPE_MEDIUM_BLOB:
			case MYSQL_TYPE_LONG_BLOB:
				M_buf_add_bytes(buf, (const M_uint8 *)result->bind[i].buffer, result->col_length[i]);
				break;
			case MYSQL_TYPE_STRING:
				M_buf_add_bytes(buf, (const char *)result->bind[i].buffer, result->col_length[i]);
				break;
			case MYSQL_TYPE_TINY:
				M_buf_add_int(buf, *((M_int8 *)result->bind[i].buffer));
				break;
			case MYSQL_TYPE_SHORT:
				M_buf_add_int(buf, *((M_int16 *)result->bind[i].buffer));
				break;
			case MYSQL_TYPE_LONG:
				M_buf_add_int(buf, *((M_int32 *)result->bind[i].buffer));
				break;
			case MYSQL_TYPE_LONGLONG:
				M_buf_add_int(buf, *((M_int64 *)result->bind[i].buffer));
				break;
			default:
				M_snprintf(error, error_size, "column %zu unrecognized data type: %d", i, (int)result->bind[i].buffer_type);
				return M_SQL_ERROR_INVALID_USE;
		}

		/* All columns with data require NULL termination, even binary.  Otherwise its considered a NULL column. */
		M_buf_add_byte(buf, 0); /* Manually add NULL terminator */

	}
	M_sql_driver_stmt_result_row_finish(stmt);

	/* We extended a column, lets re-bind it so it can use the extended size for the rest of the query */
	if (rebind) {
		if (mysql_stmt_bind_result(driver_stmt->stmt, result->bind) != 0) {
			unsigned int merr = mysql_stmt_errno(driver_stmt->stmt);
			M_snprintf(error, error_size, "stmt bind result failed: %u: %s", merr, mysql_stmt_error(driver_stmt->stmt));
			return mysql_resolve_error(NULL, (M_int32)merr);
		}
	}

	return err;
}


static M_sql_error_t mysql_cb_begin(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_stmt_t          *stmt;
	const char            *iso;
	char                   query[256];
	M_sql_error_t          err;
	mysql_connpool_data_t *data  = mysql_get_driverpool_data(conn); 
	M_sql_driver_conn_t   *dconn = M_sql_driver_conn_get_conn(conn);

	if (isolation > data->max_isolation)
		isolation = data->max_isolation;

	if (isolation == M_SQL_ISOLATION_SNAPSHOT)
		isolation = M_SQL_ISOLATION_SERIALIZABLE;

	iso = M_sql_driver_isolation2str(isolation);

	M_snprintf(query, sizeof(query), "SET TRANSACTION ISOLATION LEVEL %s", iso);

	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET ISOLATION %s failed: %s: %s", iso, M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	if (mysql_autocommit(dconn->conn, 0) != 0) {
		unsigned int merr = mysql_errno(dconn->conn);
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(error, error_size, "failed to disable autocommit: (%u) %s", merr, mysql_error(dconn->conn));
		return err;
	}

	return err;
}


static M_sql_error_t mysql_cb_rollback(M_sql_conn_t *conn)
{
	M_sql_stmt_t        *stmt;
	M_sql_error_t        err;
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);

	if (mysql_rollback(dconn->conn) != 0) {
		unsigned int merr = mysql_errno(dconn->conn);
		char         msg[256];
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(msg, sizeof(msg), "Rollback Failed: %s", mysql_error(dconn->conn));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, msg);

		return err;
	}

	/* Enable auto-commit, will disable it in transactions */
	if (mysql_autocommit(dconn->conn, 1) != 0) {
		unsigned int merr = mysql_errno(dconn->conn);
		char         error[256];
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(error, sizeof(error), "failed to enable autocommit, forcing disconnect: (%u) %s", merr, mysql_error(dconn->conn));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
		return M_SQL_ERROR_CONN_LOST;
	}

	stmt = M_sql_conn_execute_simple(conn, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", M_FALSE);
	if (stmt == NULL || M_sql_stmt_get_error(stmt) != M_SQL_ERROR_SUCCESS) {
		char msg[256];
		M_snprintf(msg, sizeof(msg), "Set Isolation Read Committed Failed: %s", M_sql_stmt_get_error_string(stmt));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, M_sql_stmt_get_error(stmt), msg);
	}
	M_sql_stmt_destroy(stmt);

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t mysql_cb_commit(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_stmt_t *stmt;
	M_sql_error_t err;
	M_sql_driver_conn_t *dconn = M_sql_driver_conn_get_conn(conn);

	if (mysql_commit(dconn->conn) != 0) {
		unsigned int merr = mysql_errno(dconn->conn);
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(error, error_size, "COMMIT failed (%u): %s", merr, mysql_error(dconn->conn));

		/* On failure to commit, attempt a rollback */
		mysql_cb_rollback(conn);
		return err;
	}

	/* Enable auto-commit, will disable it in transactions */
	if (mysql_autocommit(dconn->conn, 1) != 0) {
		unsigned int merr = mysql_errno(dconn->conn);
		err               = mysql_resolve_error(NULL, (M_int32)merr);
		M_snprintf(error, error_size, "failed to enable autocommit, forcing disconnect: (%u) %s", merr, mysql_error(dconn->conn));
		return M_SQL_ERROR_CONN_LOST;
	}

	/* Restore isolation level */
	stmt = M_sql_conn_execute_simple(conn, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		char msg[256];
		M_snprintf(msg, sizeof(msg), "Set Isolation Read Committed Failed: %s", M_sql_stmt_get_error_string(stmt));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, M_sql_stmt_get_error(stmt), msg);
	}
	M_sql_stmt_destroy(stmt);

	/* Ignore failure setting isolation level */

	return M_SQL_ERROR_SUCCESS;
}


static void mysql_cb_createtable_suffix(M_sql_connpool_t *pool, M_buf_t *query)
{
	M_sql_driver_connpool_t *dpool = M_sql_driver_pool_get_dpool(pool);
	mysql_createtable_suffix(pool, dpool->primary.settings, query);
}


static M_sql_driver_t M_sql_mysql = {
	M_SQL_DRIVER_VERSION,         /* Driver/Module subsystem version */
	"mysql",                      /* Short name of module */
	"MySQL/MariaDB driver for mstdlib",  /* Display name of module */
	"1.0.0",                      /* Internal module version */

	mysql_cb_init,                /* Callback used for module initialization. */
	mysql_cb_destroy,             /* Callback used for module destruction/unloading. */
	mysql_cb_createpool,          /* Callback used for pool creation */
	mysql_cb_destroypool,         /* Callback used for pool destruction */
	mysql_cb_connect,             /* Callback used for connecting to the db */
	mysql_cb_serverversion,       /* Callback used to get the server name/version string */
	mysql_cb_connect_runonce,     /* Callback used after connection is established, but before first query to set run-once options. */
	mysql_cb_disconnect,          /* Callback used to disconnect from the db */
	mysql_cb_queryformat,         /* Callback used for reformatting a query to the sql db requirements */
	mysql_cb_prepare,             /* Callback used for preparing a query for execution */
	mysql_cb_prepare_destroy,     /* Callback used to destroy the driver-specific prepared statement handle */
	mysql_cb_execute,             /* Callback used for executing a prepared query */
	mysql_cb_fetch,               /* Callback used to fetch result data/rows from server */
	mysql_cb_begin,               /* Callback used to begin a transaction */
	mysql_cb_rollback,            /* Callback used to rollback a transaction */
	mysql_cb_commit,              /* Callback used to commit a transaction */
	mysql_cb_datatype,            /* Callback used to convert to data type for server */
	mysql_cb_createtable_suffix,  /* Callback used to append additional data to the Create Table query string */
	mysql_cb_append_updlock,      /* Callback used to append row-level locking data */
	mysql_cb_append_bitop,        /* Callback used to append a bit operation */

	NULL,                         /* Handle for loaded driver - must be initialized to NULL */
};

/*! Defines function that references M_sql_driver_t M_sql_##name for module loading */
M_SQL_DRIVER(mysql)
