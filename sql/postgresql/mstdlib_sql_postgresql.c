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
#include <libpq-fe.h>
#include "postgresql_shared.h"

typedef struct {
	char              db[64];
	M_sql_hostport_t *hosts;
	size_t            num_hosts;
	char              application_name[65];
} pgsql_connpool_data_t;


struct M_sql_driver_connpool {
	pgsql_connpool_data_t primary;
	pgsql_connpool_data_t readonly;
};


struct M_sql_driver_conn {
	PGconn *conn;         /*!< PostgreSQL connection handle */
	char    version[32];  /*!< Cached server version */
	size_t  stmt_id;      /*!< Prepared statements require a key/name, we'll use an integer counter */
};


typedef union {
	M_uint8              i8;
	M_int16              i16;
	M_int32              i32;
	M_int64              i64;
	const char          *text;
	const unsigned char *binary;
} pgsql_stmtdata_t;


typedef struct {
	pgsql_stmtdata_t *data;
	Oid              *oids;
	const char      **values;
	int              *lengths;
	int              *formats;
	size_t            cnt;
} pgsql_stmtbind_t;


struct M_sql_driver_stmt {
	size_t           id;    /*!< Server-side id of prepared statement */
	M_sql_conn_t    *conn;  /*!< Connection object associated with statement handle */
	pgsql_stmtbind_t bind;  /*!< Bound parameter pointers */
	PGresult        *res;   /*!< Result handle, may contain row response data */
};


static M_thread_mutex_t *pgsql_lock         = NULL;
static pgthreadlock_t    pgsql_prior_lockfn = NULL;


static void pgsql_threadlock(int acquire)
{
	if (acquire) {
		M_thread_mutex_lock(pgsql_lock);
	} else {
		M_thread_mutex_unlock(pgsql_lock);
	}
}


static M_bool pgsql_cb_init(char *error, size_t error_size)
{
	(void)error;
	(void)error_size;

	pgsql_lock         = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	pgsql_prior_lockfn = PQregisterThreadLock(pgsql_threadlock);

	return M_TRUE;
}


static void pgsql_cb_destroy(void)
{
	PQregisterThreadLock(pgsql_prior_lockfn);
	M_thread_mutex_destroy(pgsql_lock);
}


static void pgsql_sanitize_error(char *error)
{
	if (M_str_isempty(error))
		return;

	M_str_replace_chr(error, '\n', ' ');
	M_str_replace_chr(error, '\r', ' ');
	M_str_replace_chr(error, '\t', ' ');
}


static M_bool pgsql_connpool_readconf(pgsql_connpool_data_t *data, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	M_sql_connstr_params_t params[] = {
		{ "db",               M_SQL_CONNSTR_TYPE_ANY,      M_TRUE,   1,    31 },
		{ "host",             M_SQL_CONNSTR_TYPE_ANY,      M_TRUE,   1,  1024 },
		{ "application_name", M_SQL_CONNSTR_TYPE_ANY,      M_FALSE,  1,    64 },
		{ NULL, 0, M_FALSE, 0, 0}
	};

	const char *const_temp;

	if (!M_sql_driver_validate_connstr(conndict, params, error, error_size)) {
		return M_FALSE;
	}

	/* db */
	M_str_cpy(data->db, sizeof(data->db), M_hash_dict_get_direct(conndict, "db"));

	/* host */
	const_temp = M_hash_dict_get_direct(conndict, "host");
	if (!M_str_isempty(const_temp)) {
		data->hosts = M_sql_driver_parse_hostport(const_temp, 5432, &data->num_hosts, error, error_size);
		if (data->hosts == NULL)
			return M_FALSE;
	}

	/* application name */
	const_temp = M_hash_dict_get_direct(conndict, "application_name");
	if (!M_str_isempty(const_temp)) {
		M_str_cpy(data->application_name, sizeof(data->application_name), const_temp);
	}

	*num_hosts = data->num_hosts;

	return M_TRUE;
}


static M_bool pgsql_cb_createpool(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	pgsql_connpool_data_t  *data;

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

	return pgsql_connpool_readconf(data, conndict, num_hosts, error, error_size);
}


static void pgsql_cb_destroypool(M_sql_driver_connpool_t *dpool)
{
	if (dpool == NULL)
		return;

	M_free(dpool->primary.hosts);
	M_free(dpool->readonly.hosts);
	M_free(dpool);
}


static size_t pgsql_dict_to_kvarrays_nullterm(const M_hash_dict_t *dict, char ***keys, char ***vals)
{
	size_t              num_keys;
	M_hash_dict_enum_t *hashenum = NULL;
	const char         *key      = NULL;
	const char         *val      = NULL;
	size_t              idx      = 0;

	if (keys == NULL || vals == NULL)
		return 0;

	*keys = NULL;
	*vals = NULL;

	num_keys = M_hash_dict_num_keys(dict);

	if (num_keys == 0)
		return 0;

	*keys = M_malloc_zero(sizeof(**keys) * (num_keys+1));
	*vals = M_malloc_zero(sizeof(**vals) * (num_keys+1));

	M_hash_dict_enumerate(dict, &hashenum);
	while (M_hash_dict_enumerate_next(dict, hashenum, &key, &val)) {
		if (key == NULL)
			continue;

		(*keys)[idx] = M_strdup(key);
		(*vals)[idx] = M_strdup(val);
		idx++;
	}
	M_hash_dict_enumerate_free(hashenum);

	return idx;
}


static void pgsql_kvarray_free(char **keys, char **vals)
{
	size_t i;

	if (keys == NULL || vals == NULL)
		return;

	for (i=0; keys[i] != NULL; i++) {
		M_free(keys[i]);
		M_free(vals[i]);
	}
	M_free(keys);
	M_free(vals);
}


static M_sql_error_t pgsql_cb_connect(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size)
{
	M_sql_driver_connpool_t *dpool      = M_sql_driver_pool_get_dpool(pool);
	pgsql_connpool_data_t   *data       = is_readonly_pool?&dpool->readonly:&dpool->primary;
	M_sql_error_t            err        = M_SQL_ERROR_SUCCESS;
	M_hash_dict_t           *conn_opts  = M_hash_dict_create(8, 75, M_HASH_DICT_NONE);
	char                   **keys       = NULL;
	char                   **vals       = NULL;
	char                     temp[256];
	int                      ver;

	*conn         = M_malloc_zero(sizeof(**conn));

	/* Create options for connection */
	M_hash_dict_insert(conn_opts, "host",            data->hosts[host_idx].host);
	M_snprintf(temp, sizeof(temp), "%u", (unsigned int)data->hosts[host_idx].port);
	M_hash_dict_insert(conn_opts, "port",            temp);
	M_hash_dict_insert(conn_opts, "dbname",          data->db);
	M_hash_dict_insert(conn_opts, "user",            M_sql_driver_pool_get_username(pool));
	M_hash_dict_insert(conn_opts, "password",        M_sql_driver_pool_get_password(pool));
	M_hash_dict_insert(conn_opts, "connect_timeout", "5");
	if (!M_str_isempty(data->application_name))
		M_hash_dict_insert(conn_opts, "application_name", data->application_name);

	/* XXX: sslmode, sslcert, sslkey, sslrootcert, sslcrl */

	/* Convert options into NULL-terminated arrays for PQconnectdbParams() */
	if (pgsql_dict_to_kvarrays_nullterm(conn_opts, &keys, &vals) == 0) {
		goto done;
	}

	(*conn)->conn = PQconnectdbParams((const char * const *)keys, (const char * const *)vals, 0);

	if ((*conn)->conn == NULL || PQstatus((*conn)->conn) != CONNECTION_OK) {
		err = M_SQL_ERROR_CONN_FAILED;
		M_snprintf(error, error_size, "failed to connect: %s", PQerrorMessage((*conn)->conn));
		pgsql_sanitize_error(error);
		goto done;
	}

	ver = PQserverVersion((*conn)->conn);

	M_snprintf((*conn)->version, sizeof((*conn)->version), "%d.%d.%d", ver / 10000, (ver % 10000) / 100, ver % 100);

done:
	M_hash_dict_destroy(conn_opts);
	pgsql_kvarray_free(keys, vals);

	if (err != M_SQL_ERROR_SUCCESS && (*conn)->conn != NULL) {
		PQfinish((*conn)->conn);
		M_free(*conn);
		*conn = NULL;
	}

	return err;
}


static const char *pgsql_cb_serverversion(M_sql_driver_conn_t *conn)
{
	return conn->version;
}


static void pgsql_cb_disconnect(M_sql_driver_conn_t *conn)
{
	if (conn == NULL)
		return;
	if (conn->conn != NULL)
		PQfinish(conn->conn);
	M_free(conn);
}


static size_t pgsql_num_process_rows(size_t num_rows)
{
#define PGSQL_MAX_PROCESS_ROWS 100
	return M_MIN(num_rows, PGSQL_MAX_PROCESS_ROWS);
}


static char *pgsql_cb_queryformat(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	(void)conn;
	return M_sql_driver_queryformat(query, M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD|M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_DOLLAR,
	                                num_params, pgsql_num_process_rows(num_rows),
	                                error, error_size);
}


static void pgsql_free_stmt(M_sql_driver_stmt_t *stmt)
{
	if (stmt == NULL)
		return;

	M_free(stmt->bind.data);
	M_free(stmt->bind.oids);
	M_free(stmt->bind.values);
	M_free(stmt->bind.lengths);
	M_free(stmt->bind.formats);
	if (stmt->res != NULL)
		PQclear(stmt->res);
	M_free(stmt);
}


static void pgsql_cb_prepare_destroy(M_sql_driver_stmt_t *stmt)
{
	M_sql_conn_t          *conn  = stmt->conn;
	M_sql_driver_conn_t   *dconn = M_sql_driver_conn_get_conn(conn);
	if (M_sql_conn_get_state(conn) != M_SQL_CONN_STATE_FAILED) {
		char                   query[256];
		PGresult              *res;

		/* There is no built-in routine for this, instead we actually have to execute
		 * a server-side request to clear a prepared statement handle.  However, we
		 * CANNOT use M_sql_conn_execute_simple() or similar because we don't want to
		 * cache the deallocate query itself!  We're going to ignore failures. */
		M_snprintf(query, sizeof(query), "DEALLOCATE PREPARE ps%zu", stmt->id);

		res = PQexec(dconn->conn, query);
		if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
			char msg[256];
			M_snprintf(msg, sizeof(msg), "DEALLOCATE PREPARE ps%zu failed: %s", stmt->id, PQerrorMessage(dconn->conn));
			M_sql_driver_trace_message(M_FALSE, NULL, conn, M_SQL_ERROR_QUERY_FAILURE, msg);
		} else {
			M_sql_driver_trace_message(M_TRUE, NULL, conn, M_SQL_ERROR_SUCCESS, query);
		}
		if (res != NULL)
			PQclear(res);
	}
	pgsql_free_stmt(stmt);
}


/* From src/include/catalog/pg_type.h */
typedef enum {
	BOOLOID    =   16,  /*!< Boolean -- may not be converted to integer */
	BYTEAOID   =   17,  /*!< Binary data */
	CHAROID    =   18,  /*!< 1 Character -- may not be converted to integer */
	INT8OID    =   20,  /*!< 64bit integer */
	INT2OID    =   21,  /*!< 16bit integer */
	INT4OID    =   23,  /*!< 32bit integer */
	TEXTOID    =   25,  /*!< Text/String */
	FLOAT4OID  =  700,  /*!< 32bit Float */
	FLOAT8OID  =  701,  /*!< 64bit Float */
	VARCHAROID = 1043   /*!< Text/String VarChar */
} pgsql_oids;


static Oid pgsql_datatype_to_oid(M_sql_data_type_t type)
{
	switch (type) {
		case M_SQL_DATA_TYPE_BOOL: /* Can't use BOOLOID as you cannot bind an integer 0/1 to it which we require */
		case M_SQL_DATA_TYPE_INT16:
			return INT2OID;

		case M_SQL_DATA_TYPE_INT32:
			return INT4OID;

		case M_SQL_DATA_TYPE_INT64:
			return INT8OID;

		case M_SQL_DATA_TYPE_TEXT:
			return TEXTOID;

		case M_SQL_DATA_TYPE_BINARY:
			return BYTEAOID;

		case M_SQL_DATA_TYPE_NULL:
			return 0; /* Server is supposed to "infer".  Otherwise we'll actually get an error if we default to
			           * TEXTOID because TEXTOID may not match the real column type */

		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}

	/* Default to text */
	return TEXTOID;
}


static M_sql_error_t pgsql_bind_params(M_sql_driver_stmt_t *driver_stmt, M_sql_stmt_t *stmt, M_bool rebind, char *error, size_t error_size)
{
	size_t        num_rows = pgsql_num_process_rows(M_sql_driver_stmt_bind_rows(stmt));
	size_t        num_cols = M_sql_driver_stmt_bind_cnt(stmt);
	size_t        num_bind = num_rows * num_cols;
	size_t        row;
	size_t        i;
	M_bool        tbool;

	if (rebind && num_bind != driver_stmt->bind.cnt) {
		M_snprintf(error, error_size, "original bind had %zu cols, new bind has %zu", driver_stmt->bind.cnt, num_bind);
		return M_SQL_ERROR_PREPARE_INVALID;
	}

	if (!rebind) {
		driver_stmt->bind.cnt     = num_bind;
		driver_stmt->bind.data    = M_malloc_zero(sizeof(*driver_stmt->bind.data)   * num_bind);
		driver_stmt->bind.oids    = M_malloc_zero(sizeof(*driver_stmt->bind.oids)   * num_bind);
		driver_stmt->bind.values  = M_malloc_zero(sizeof(*driver_stmt->bind.values) * num_bind);
		driver_stmt->bind.lengths = M_malloc_zero(sizeof(*driver_stmt->bind.lengths)* num_bind);
		driver_stmt->bind.formats = M_malloc_zero(sizeof(*driver_stmt->bind.formats)* num_bind);
	}

	for (row = 0; row < num_rows; row++) {
		for (i = 0; i < num_cols; i++) {
			size_t         paramid = ((row * num_cols) + i);
			Oid            oid     = pgsql_datatype_to_oid(M_sql_driver_stmt_bind_get_type(stmt, row, i));

			if (rebind && oid != driver_stmt->bind.oids[paramid]) {
				M_snprintf(error, error_size, "original bind row %zu col %zu has Oid %d, new Oid %d", row, i, driver_stmt->bind.oids[paramid], oid);
				return M_SQL_ERROR_PREPARE_INVALID;
			}
			driver_stmt->bind.oids[paramid]    = oid;
			if (oid != 0) {
				/* If oid isn't "inferred", we can use binary */
				driver_stmt->bind.formats[paramid] = 1;  /* Prefer binary */
			}
			switch (M_sql_driver_stmt_bind_get_type(stmt, row, i)) {
				/* NOTE: PostgreSQL wants all binary data in their "native" form, this mainly means that all
				 *       Integer values must be in NetworkByteOrder */
				case M_SQL_DATA_TYPE_BOOL:
					tbool                                  = M_sql_driver_stmt_bind_get_bool(stmt, row, i);
					driver_stmt->bind.data[paramid].i16    = (M_int16)M_hton16((M_uint16)tbool);
					driver_stmt->bind.values[paramid]      = (const char *)&driver_stmt->bind.data[paramid].i16;
					driver_stmt->bind.lengths[paramid]     = sizeof(driver_stmt->bind.data[paramid].i16);
					break;
				case M_SQL_DATA_TYPE_INT16:
					driver_stmt->bind.data[paramid].i16    = (M_int16)M_hton16((M_uint16)M_sql_driver_stmt_bind_get_int16(stmt, row, i));
					driver_stmt->bind.values[paramid]      = (const char *)&driver_stmt->bind.data[paramid].i16;
					driver_stmt->bind.lengths[paramid]     = sizeof(driver_stmt->bind.data[paramid].i16);
					break;
				case M_SQL_DATA_TYPE_INT32:
					driver_stmt->bind.data[paramid].i32    = (M_int32)M_hton32((M_uint32)M_sql_driver_stmt_bind_get_int32(stmt, row, i));
					driver_stmt->bind.values[paramid]      = (const char *)&driver_stmt->bind.data[paramid].i32;
					driver_stmt->bind.lengths[paramid]     = sizeof(driver_stmt->bind.data[paramid].i32);
					break;
				case M_SQL_DATA_TYPE_INT64:
					driver_stmt->bind.data[paramid].i64    = (M_int64)M_hton64((M_uint64)M_sql_driver_stmt_bind_get_int64(stmt, row, i));
					driver_stmt->bind.values[paramid]      = (const char *)&driver_stmt->bind.data[paramid].i64;
					driver_stmt->bind.lengths[paramid]     = sizeof(driver_stmt->bind.data[paramid].i64);
					break;
				case M_SQL_DATA_TYPE_TEXT:
					driver_stmt->bind.data[paramid].text   = M_sql_driver_stmt_bind_get_text(stmt, row, i);
					driver_stmt->bind.values[paramid]      = driver_stmt->bind.data[paramid].text;
					driver_stmt->bind.lengths[paramid]     = (int)M_sql_driver_stmt_bind_get_text_len(stmt, row, i);
					break;
				case M_SQL_DATA_TYPE_BINARY:
					driver_stmt->bind.data[paramid].binary = M_sql_driver_stmt_bind_get_binary(stmt, row, i);
					driver_stmt->bind.values[paramid]      = (const char *)driver_stmt->bind.data[paramid].binary;
					driver_stmt->bind.lengths[paramid]     = (int)M_sql_driver_stmt_bind_get_binary_len(stmt, row, i);
					break;
				case M_SQL_DATA_TYPE_NULL:
					driver_stmt->bind.values[paramid]      = NULL;
					driver_stmt->bind.lengths[paramid]     = 0;
					break;
				default:
					M_snprintf(error, error_size, "Unknown parameter type for row %zu, col %zu", row, i);
					return M_SQL_ERROR_INVALID_USE;
			}
		}
	}

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t pgsql_cb_prepare(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_conn_t   *dconn = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t          err;
	PGresult              *res   = NULL;
	char                   psid[32];

	if (*driver_stmt != NULL) {
		if (pgsql_bind_params(*driver_stmt, stmt, M_TRUE /* ReBind */, error, error_size) == M_SQL_ERROR_SUCCESS)
			return M_SQL_ERROR_SUCCESS;

		/* Failure, probably argument data types changed, lets re-prepare.  This should be fairly rare. */
		M_mem_set(error, 0, error_size);
		*driver_stmt = NULL;
	}

	*driver_stmt         = M_malloc_zero(sizeof(**driver_stmt));
	(*driver_stmt)->id   = dconn->stmt_id++;
	(*driver_stmt)->conn = conn;

	err = pgsql_bind_params(*driver_stmt, stmt, M_FALSE /* New */, error, error_size);
	if (err != M_SQL_ERROR_SUCCESS) {
		goto done;
	}

	M_snprintf(psid, sizeof(psid), "ps%zu", (*driver_stmt)->id);
	res = PQprepare(dconn->conn, psid, M_sql_driver_stmt_get_query(stmt), (int)(*driver_stmt)->bind.cnt, (*driver_stmt)->bind.oids);
	if (res == NULL) {
		err = M_SQL_ERROR_PREPARE_INVALID;
		M_snprintf(error, error_size, "PQprepare failed - NULL: %s", PQerrorMessage(dconn->conn));
		pgsql_sanitize_error(error);
		goto done;
	}

	if (PQresultStatus(res) == PGRES_COMMAND_OK) {
		err = M_SQL_ERROR_SUCCESS;
	} else {
		err = pgsql_resolve_error(PQresultErrorField(res, PG_DIAG_SQLSTATE), 0);
		if (err != M_SQL_ERROR_SUCCESS) {
			M_snprintf(error, error_size, "PQprepare failed: %s: %s", PQresultErrorField(res, PG_DIAG_SQLSTATE), PQresultErrorMessage(res));
			pgsql_sanitize_error(error);
			goto done;
		}
	}

done:
	if (err != M_SQL_ERROR_SUCCESS) {
		pgsql_free_stmt(*driver_stmt);
		*driver_stmt = NULL;
	}
	if (res != NULL)
		PQclear(res);

	return err;
}


static M_sql_data_type_t pgsql_get_mtype(PGresult *res, size_t col, size_t *max_len)
{
	*max_len = 0;

	switch (PQftype(res, (int)col)) {
		case BOOLOID:
		case CHAROID:
			return M_SQL_DATA_TYPE_BOOL;
		case INT8OID:
			return M_SQL_DATA_TYPE_INT64;
		case INT4OID:
			return M_SQL_DATA_TYPE_INT32;
		case INT2OID:
			return M_SQL_DATA_TYPE_INT16;
		case TEXTOID:
		case VARCHAROID:
			*max_len = (size_t)PQfmod(res, (int)col);
			/* Weird 4-byte added to varchar lengths */
			if (*max_len > 4)
				*max_len -= 4;
			if (*max_len >= 64 * 1024)
				*max_len = 0;
			return M_SQL_DATA_TYPE_TEXT;
		case BYTEAOID:
			return M_SQL_DATA_TYPE_BINARY;
		default:
			break;
	}
	return M_SQL_DATA_TYPE_TEXT;
}


static void pgsql_fetch_result_metadata(M_sql_driver_stmt_t *dstmt, M_sql_stmt_t *stmt)
{
	size_t num_cols = (size_t)PQnfields(dstmt->res);
	size_t i;

	M_sql_driver_stmt_result_set_num_cols(stmt, num_cols);
	if (num_cols == 0)
		return;

	for (i=0; i<num_cols; i++) {
		size_t            max_len = 0;
		M_sql_data_type_t mtype   = pgsql_get_mtype(dstmt->res, i, &max_len);

		M_sql_driver_stmt_result_set_col_name(stmt, i, PQfname(dstmt->res, (int)i));
		M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, max_len);
	}
}


static void pgsql_clear_remaining_data(M_sql_conn_t *conn)
{
	M_sql_driver_conn_t   *dconn = M_sql_driver_conn_get_conn(conn);
	PGresult              *res;

	/* Doc's say to call PQgetResult() until it returns NULL, always */
	while ((res = PQgetResult(dconn->conn)) != NULL) {
		PQclear(res);
	}
}


static M_sql_error_t pgsql_cb_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size)
{
	M_sql_driver_conn_t   *dconn = M_sql_driver_conn_get_conn(conn);
	M_sql_driver_stmt_t   *dstmt = M_sql_driver_stmt_get_stmt(stmt);
	char                   psid[32];
	M_sql_error_t          err;

	M_snprintf(psid, sizeof(psid), "ps%zu", dstmt->id);

	/* https://www.postgresql.org/message-id/20160331195656.17bc0e3b%40slate.meme.com */

	if (!PQsendQueryPrepared(dconn->conn, psid, (int)dstmt->bind.cnt, dstmt->bind.values,
	    dstmt->bind.lengths, dstmt->bind.formats, 0 /* Always text response, we can't handle every OID otherwise */)) {
		M_snprintf(error, error_size, "PQsendQueryPrepared failed: %s", PQerrorMessage(dconn->conn));
		pgsql_sanitize_error(error);
		return M_SQL_ERROR_CONN_LOST;
	}

	if (!PQsetSingleRowMode(dconn->conn)) {
		M_snprintf(error, error_size, "PQsetSingleRowMode failed: %s", PQerrorMessage(dconn->conn));
		pgsql_sanitize_error(error);
		return M_SQL_ERROR_CONN_LOST;
	}

	dstmt->res = PQgetResult(dconn->conn);
	if (dstmt->res == NULL) {
		M_snprintf(error, error_size, "PQgetResult failed: %s", PQerrorMessage(dconn->conn));
		pgsql_sanitize_error(error);
		return M_SQL_ERROR_CONN_LOST;
	}

	switch (PQresultStatus(dstmt->res)) {
		case PGRES_COMMAND_OK:
			err = M_SQL_ERROR_SUCCESS;
			M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)M_str_to_uint32(PQcmdTuples(dstmt->res)));

			/* Rewrite to M_SQL_ERROR_SUCCESS_ROW if there were columns defined in the result set */
			if (PQnfields(dstmt->res)) {
				err = M_SQL_ERROR_SUCCESS_ROW;
			}
			break;
		case PGRES_TUPLES_OK:
		case PGRES_SINGLE_TUPLE:
			err = M_SQL_ERROR_SUCCESS_ROW;
			break;
		default:
			err = pgsql_resolve_error(PQresultErrorField(dstmt->res, PG_DIAG_SQLSTATE), 0);
			M_snprintf(error, error_size, "%s: %s", PQresultErrorField(dstmt->res, PG_DIAG_SQLSTATE), PQresultErrorMessage(dstmt->res));
			break;
	}

	/* We need to get metadata here for result output (column definitions) */
	if (err == M_SQL_ERROR_SUCCESS_ROW) {
		pgsql_fetch_result_metadata(dstmt, stmt);
	}

	if (err != M_SQL_ERROR_SUCCESS_ROW) {
		PQclear(dstmt->res);
		dstmt->res = NULL;
		pgsql_clear_remaining_data(conn);
	}

	/* Get number of rows that are processed at once, supports
	 * comma-delimited values for inserting multiple rows. */
	*rows_executed = pgsql_num_process_rows(M_sql_driver_stmt_bind_rows(stmt));

	return err;
}


/* XXX: Fetch Cancel ? */

static M_sql_error_t pgsql_cb_fetch(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	M_sql_driver_stmt_t            *dstmt   = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t            *dconn   = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t                   err     = M_SQL_ERROR_SUCCESS_ROW;
	size_t                          i;
	size_t                          row;
	ExecStatusType                  status;
	size_t                          num_cols;
	size_t                          num_rows;

	if (dstmt->res == NULL) {
		M_snprintf(error, error_size, "No active resultset");
		err = M_SQL_ERROR_INVALID_USE;
		goto done;
	}

	status = PQresultStatus(dstmt->res);

	/* No more data */
	if (status == PGRES_COMMAND_OK) {
		err = M_SQL_ERROR_SUCCESS;
		goto done;
	}

	num_cols = M_sql_stmt_result_num_cols(stmt);
	num_rows = (size_t)PQntuples(dstmt->res);

	/* Grab the result set */
	for (row = 0; row < num_rows; row++) {
		for (i=0; i < num_cols; i++) {
			M_buf_t       *buf = M_sql_driver_stmt_result_col_start(stmt);
			size_t         len = 0;
			unsigned char *binary = NULL;

			/* Don't write anything at all for NULL fields */
			if (PQgetisnull(dstmt->res, (int)row, (int)i))
				continue;

			/* Non-binary data is already in string form */
			if (M_sql_stmt_result_col_type(stmt, i, NULL) != M_SQL_DATA_TYPE_BINARY) {
				M_buf_add_str(buf, PQgetvalue(dstmt->res, (int)row, (int)i));
			} else {
				/* Binary Data */
				binary = PQunescapeBytea((const unsigned char *)PQgetvalue(dstmt->res, (int)row, (int)i), &len);
				M_buf_add_bytes(buf, binary, len);
				PQfreemem(binary);
			}
			/* All columns with data require NULL termination, even binary.  Otherwise its considered a NULL column. */
			M_buf_add_byte(buf, 0); /* Manually add NULL terminator */
		}
		M_sql_driver_stmt_result_row_finish(stmt);
	}

	/* Fetch next row */
	PQclear(dstmt->res);
	dstmt->res = PQgetResult(dconn->conn);
	if (dstmt->res == NULL) {
		/* We need to assume this really means we're done if we processed 0 rows and next attempt returns NULL */
		if (num_rows == 0) {
			err = M_SQL_ERROR_SUCCESS;
			goto done;
		}
		M_snprintf(error, error_size, "PQgetResult failed: %s", PQerrorMessage(dconn->conn));
		pgsql_sanitize_error(error);
		return M_SQL_ERROR_CONN_LOST;
	}

	switch (PQresultStatus(dstmt->res)) {
		case PGRES_COMMAND_OK:
			err = M_SQL_ERROR_SUCCESS;
			break;
		case PGRES_TUPLES_OK:
		case PGRES_SINGLE_TUPLE:
			err = M_SQL_ERROR_SUCCESS_ROW;
			break;
		default:
			err = pgsql_resolve_error(PQresultErrorField(dstmt->res, PG_DIAG_SQLSTATE), 0);
			M_snprintf(error, error_size, "%s: %s", PQresultErrorField(dstmt->res, PG_DIAG_SQLSTATE), PQresultErrorMessage(dstmt->res));
			break;
	}

done:
	if (err != M_SQL_ERROR_SUCCESS_ROW && dstmt->res != NULL) {
		PQclear(dstmt->res);
		dstmt->res = NULL;
		pgsql_clear_remaining_data(conn);
	}
	return err;
}


static M_sql_error_t pgsql_cb_begin(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_stmt_t          *stmt;
	const char            *iso;
	char                   query[256];
	M_sql_error_t          err;

	/* Snapshot not supported */
	if (isolation == M_SQL_ISOLATION_SNAPSHOT)
		isolation = M_SQL_ISOLATION_SERIALIZABLE;

	iso = M_sql_driver_isolation2str(isolation);

	M_snprintf(query, sizeof(query), "BEGIN TRANSACTION ISOLATION LEVEL %s", iso);

	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "BEGIN TRANSACTION ISOLATION LEVEL %s failed: %s: %s", iso, M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return err;
}


static M_sql_error_t pgsql_cb_rollback(M_sql_conn_t *conn)
{
	M_sql_stmt_t          *stmt;
	M_sql_error_t          err;

	stmt = M_sql_conn_execute_simple(conn, "ROLLBACK", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		char error[256];
		M_snprintf(error, sizeof(error), "ROLLBACK failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);
	return err;
}


static M_sql_error_t pgsql_cb_commit(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_stmt_t          *stmt;
	M_sql_error_t          err;

	stmt = M_sql_conn_execute_simple(conn, "COMMIT", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "COMMIT failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);
	return err;
}


static M_sql_driver_t M_sql_postgresql = {
	M_SQL_DRIVER_VERSION,         /* Driver/Module subsystem version */
	"postgresql",                 /* Short name of module */
	"PostgreSQL driver for mstdlib",  /* Display name of module */
	"1.0.0",                      /* Internal module version */

	pgsql_cb_init,                /* Callback used for module initialization. */
	pgsql_cb_destroy,             /* Callback used for module destruction/unloading. */
	pgsql_cb_createpool,          /* Callback used for pool creation */
	pgsql_cb_destroypool,         /* Callback used for pool destruction */
	pgsql_cb_connect,             /* Callback used for connecting to the db */
	pgsql_cb_serverversion,       /* Callback used to get the server name/version string */
	pgsql_cb_connect_runonce,     /* Callback used after connection is established, but before first query to set run-once options. */
	pgsql_cb_disconnect,          /* Callback used to disconnect from the db */
	pgsql_cb_queryformat,         /* Callback used for reformatting a query to the sql db requirements */
	pgsql_cb_prepare,             /* Callback used for preparing a query for execution */
	pgsql_cb_prepare_destroy,     /* Callback used to destroy the driver-specific prepared statement handle */
	pgsql_cb_execute,             /* Callback used for executing a prepared query */
	pgsql_cb_fetch,               /* Callback used to fetch result data/rows from server */
	pgsql_cb_begin,               /* Callback used to begin a transaction */
	pgsql_cb_rollback,            /* Callback used to rollback a transaction */
	pgsql_cb_commit,              /* Callback used to commit a transaction */
	pgsql_cb_datatype,            /* Callback used to convert to data type for server */
	NULL,                         /* Callback used to append additional data to the Create Table query string */
	pgsql_cb_append_updlock,      /* Callback used to append row-level locking data */
	pgsql_cb_append_bitop,        /* Callback used to append a bit operation */

	NULL,                         /* Handle for loaded driver - must be initialized to NULL */
};

/*! Defines function that references M_sql_driver_t M_sql_##name for module loading */
M_SQL_DRIVER(postgresql)
