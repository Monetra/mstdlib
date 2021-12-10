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
#include "sqlite3.h"


typedef struct {
	char   path[1024];
	char   journal_mode[32];
	M_bool analyze;
	M_bool integrity_check;
	M_bool shared_cache;
	M_bool autocreate;
} sqlite_connpool_data_t;

struct M_sql_driver_connpool {
	sqlite_connpool_data_t primary;
	sqlite_connpool_data_t readonly;
};

struct M_sql_driver_conn {
	sqlite3 *conn;
	char     version[128];
};

struct M_sql_driver_stmt {
	sqlite3_stmt *stmt;
	M_bool        is_commit;
};

struct sqlite3_mutex {
	M_thread_mutex_t *mutex;
};

M_hash_u64vp_t   *sqlite_static_mutexes = NULL;
M_thread_mutex_t *sqlite_global_lock    = NULL;


static void sqlite_mutex_free(sqlite3_mutex *mutex)
{
	if (mutex == NULL)
		return;
	M_thread_mutex_destroy(mutex->mutex);
	M_free(mutex);
}

static void sqlite_destroy_mutex(void *arg)
{
	sqlite_mutex_free(arg);
}


static int sqlite_mutex_init(void)
{
	if (sqlite_static_mutexes != NULL) {
		return SQLITE_OK;
	}

	sqlite_global_lock    = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
	sqlite_static_mutexes = M_hash_u64vp_create(8, 75, M_HASH_U64VP_NONE, sqlite_destroy_mutex);

	return SQLITE_OK;
}


static int sqlite_mutex_finish(void)
{
	M_thread_mutex_destroy(sqlite_global_lock);
	M_hash_u64vp_destroy(sqlite_static_mutexes, M_TRUE);
	sqlite_global_lock    = NULL;
	sqlite_static_mutexes = NULL;

	return SQLITE_OK;
}


static sqlite3_mutex *sqlite_mutex_alloc(int iType)
{
	sqlite3_mutex *mutex = NULL;

	switch(iType) {
		case SQLITE_MUTEX_FAST:
			mutex = M_malloc_zero(sizeof(*mutex));
			mutex->mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_NONE);
			return mutex;
		case SQLITE_MUTEX_RECURSIVE:
			mutex = M_malloc_zero(sizeof(*mutex));
			mutex->mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_RECURSIVE);
			return mutex;
	}


	/* Must be a static mutex, see if we already created one, if not,
	 * create a new one and insert it */
	M_thread_mutex_lock(sqlite_global_lock);
	mutex = M_hash_u64vp_get_direct(sqlite_static_mutexes, (M_uint64)M_ABS(iType));

	/* Doesn't exist, create it */
	if (mutex == NULL) {
		mutex = M_malloc_zero(sizeof(*mutex));
		mutex->mutex = M_thread_mutex_create(M_THREAD_MUTEXATTR_RECURSIVE /* Docs dont say, err on side of caution */);
		M_hash_u64vp_insert(sqlite_static_mutexes, (M_uint64)M_ABS(iType), mutex);
	}

	M_thread_mutex_unlock(sqlite_global_lock);
	return mutex;
}


static void sqlite_mutex_enter(sqlite3_mutex *mutex)
{
	M_thread_mutex_lock(mutex->mutex);
}


static int sqlite_mutex_try(sqlite3_mutex *mutex)
{
	if (!M_thread_mutex_trylock(mutex->mutex))
		return SQLITE_BUSY;
	return SQLITE_OK;
}

static void sqlite_mutex_leave(sqlite3_mutex *mutex)
{
	M_thread_mutex_unlock(mutex->mutex);
}


static M_bool sqlite_cb_init(char *error, size_t error_size)
{
	struct sqlite3_mutex_methods methods = {
		sqlite_mutex_init,
		sqlite_mutex_finish,
		sqlite_mutex_alloc,
		sqlite_mutex_free,
		sqlite_mutex_enter,
		sqlite_mutex_try,
		sqlite_mutex_leave,
		NULL, /* held */
		NULL  /* not held */
	};

	/* Register mutex callbacks */
	if (sqlite3_config(SQLITE_CONFIG_MUTEX, &methods) != SQLITE_OK) {
		M_snprintf(error, error_size, "sqlite3_config(MUTEX) returned error");
		return M_FALSE;
	}

	if (!sqlite3_threadsafe()) {
		M_snprintf(error, error_size, "sqlite3_threadsafe() returned false");
		return M_FALSE;
	}

	/* Use multithread, less strict than Serialized, but still threadsafe.  We
	 * serialize access to the connection object and prepared statement handles
	 * ourselves so the less strict mode makes sense.  */
	if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD, NULL) != SQLITE_OK) {
		M_snprintf(error, error_size, "sqlite3_config(MULTITHREAD) returned error");
		return M_FALSE;
	}

	if (sqlite3_initialize() != SQLITE_OK) {
		M_snprintf(error, error_size, "sqlite3_initialize()) returned error");
		return M_FALSE;
	}

	return M_TRUE;
}


static void sqlite_cb_destroy(void)
{
	if (sqlite3_temp_directory != NULL) {
		sqlite3_free(sqlite3_temp_directory);
		sqlite3_temp_directory = NULL;
	}
	sqlite3_shutdown();
}


static M_bool sqlite_connpool_readconf(sqlite_connpool_data_t *data, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	M_sql_connstr_params_t params[] = {
		{ "path",            M_SQL_CONNSTR_TYPE_ANY,   M_TRUE,  1, 1024 },
		{ "journal_mode",    M_SQL_CONNSTR_TYPE_ALPHA, M_FALSE, 1,   32 },
		{ "analyze",         M_SQL_CONNSTR_TYPE_BOOL,  M_FALSE, 0,    0 },
		{ "integrity_check", M_SQL_CONNSTR_TYPE_BOOL,  M_FALSE, 0,    0 },
		{ "shared_cache",    M_SQL_CONNSTR_TYPE_BOOL,  M_FALSE, 0,    0 },
		{ "autocreate",      M_SQL_CONNSTR_TYPE_BOOL,  M_FALSE, 0,    0 },

		{ NULL, 0, M_FALSE, 0, 0}
	};
	const char              *config_path;
	char                    *db_path     = NULL;
	const char              *const_temp;

	/* NOTE: Why would we possibly support the ro_conndict version?  Not really
	 *       feasible with SQLite, right ? */

	if (!M_sql_driver_validate_connstr(conndict, params, error, error_size)) {
		return M_FALSE;
	}

	/* Normalize the provided path */
	config_path = M_hash_dict_get_direct(conndict, "path");
	if (M_fs_path_norm(&db_path, config_path, M_FS_PATH_NORM_ABSOLUTE|M_FS_PATH_NORM_HOME, M_FS_SYSTEM_AUTO) != M_FS_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "failed path normalization for '%s'", config_path);
		return M_FALSE;
	}
	M_str_cpy(data->path, sizeof(data->path), db_path);
	M_free(db_path);

	/* Analyze defaults to on */
	const_temp = M_hash_dict_get_direct(conndict, "analyze");
	if (M_str_isempty(const_temp) || M_str_istrue(const_temp)) {
		data->analyze = M_TRUE;
	}

	/* Integrity check defaults to off */
	const_temp = M_hash_dict_get_direct(conndict, "integrity_check");
	if (M_str_istrue(const_temp)) {
		data->integrity_check = M_TRUE;
	}

	/* Shared Cache defaults to on */
	const_temp = M_hash_dict_get_direct(conndict, "shared_cache");
	if (M_str_isempty(const_temp) || M_str_istrue(const_temp)) {
		data->shared_cache = M_TRUE;
	}

	/* Journal Mode defaults to WAL */
	const_temp = M_hash_dict_get_direct(conndict, "journal_mode");
	if (M_str_isempty(const_temp))
		const_temp = "WAL";
	M_str_cpy(data->journal_mode, sizeof(data->journal_mode), const_temp);

	/* Autocreate defaults to on */
	const_temp = M_hash_dict_get_direct(conndict, "autocreate");
	if (M_str_isempty(const_temp) || M_str_istrue(const_temp)) {
		data->autocreate = M_TRUE;
	}

	*num_hosts = 1;

	return M_TRUE;
}


static M_bool sqlite_cb_createpool(M_sql_driver_connpool_t **dpool, M_sql_connpool_t *pool, M_bool is_readonly, const M_hash_dict_t *conndict, size_t *num_hosts, char *error, size_t error_size)
{
	sqlite_connpool_data_t  *data;

	(void)pool;

	if (*dpool == NULL) {
		*dpool = M_malloc_zero(sizeof(**dpool));
	}

	data = is_readonly?&(*dpool)->readonly:&(*dpool)->primary;

	return sqlite_connpool_readconf(data, conndict, num_hosts, error, error_size);
}


static void sqlite_cb_destroypool(M_sql_driver_connpool_t *dpool)
{
	M_free(dpool);
}


static M_sql_error_t sqlite_cb_connect(M_sql_driver_conn_t **conn, M_sql_connpool_t *pool, M_bool is_readonly_pool, size_t host_idx, char *error, size_t error_size)
{
	char                    *db_dir   = NULL;
	M_sql_error_t            err      = M_SQL_ERROR_SUCCESS;
	int                      rc;
	M_sql_driver_connpool_t *dpool    = M_sql_driver_pool_get_dpool(pool);
	sqlite_connpool_data_t  *data     = is_readonly_pool?&dpool->readonly:&dpool->primary;
	int                      flags    = SQLITE_OPEN_READWRITE;

	/* SQLite doesn't support the concept of multiple hosts, ignore */
	(void)host_idx;

	*conn = NULL;

	/* SQLite can store some temporary files in a system-specific temp location. 
	 * This has caused issues for at least one customer who's temp path filled up
	 * as they were not expecting this sort of behavior.  This must be called prior
	 * to the first sqlite3_open().
	 * References:
	 *   http://www.sqlite.org/c3ref/temp_directory.html
	 *   http://www.sqlite.org/compile.html#temp_store
	 *   http://www.sqlite.org/tempfiles.html
	 */
	db_dir = M_fs_path_dirname(data->path, M_FS_SYSTEM_AUTO);
	if (db_dir == NULL)
		db_dir = M_strdup(".");
	M_thread_mutex_lock(sqlite_global_lock);
	if (!M_str_caseeq(db_dir, sqlite3_temp_directory)) {
		if (sqlite3_temp_directory != NULL)
			sqlite3_free(sqlite3_temp_directory);
		sqlite3_temp_directory = sqlite3_mprintf("%s", db_dir);
	}
	M_thread_mutex_unlock(sqlite_global_lock);

	*conn = M_malloc_zero(sizeof(**conn));

	M_snprintf((*conn)->version, sizeof((*conn)->version), "SQLite %s", SQLITE_VERSION);

	if (data->autocreate)
		flags |= SQLITE_OPEN_CREATE;

	if (data->shared_cache)
		flags |= SQLITE_OPEN_SHAREDCACHE;

	rc    = sqlite3_open_v2(data->path, &(*conn)->conn, flags, NULL);
	if (rc) {
		M_snprintf(error, error_size, "SQLite failed to connect (%d): %s", rc, sqlite3_errmsg((*conn)->conn));
		err = M_SQL_ERROR_CONN_FAILED;
		goto fail;
	}

	/* Enable extended result codes */
	sqlite3_extended_result_codes((*conn)->conn, 1);

	/* Set busy timeout so it doesn't return immediately if it can't obtain
	 * a lock.  Wait up to 1/4 second */
	sqlite3_busy_timeout((*conn)->conn, 250);

	err = M_SQL_ERROR_SUCCESS;

fail:
	M_free(db_dir);
	if (err != M_SQL_ERROR_SUCCESS) {
		if (*conn) {
			if ((*conn)->conn) {
				sqlite3_close((*conn)->conn);
			}
			M_free(*conn);
			*conn = NULL;
		}
	}

	return err;
}


static const char *sqlite_cb_serverversion(M_sql_driver_conn_t *conn)
{
	return conn->version;
}


static M_bool sqlite_verify_integrity(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_stmt_t   *stmt   = M_sql_conn_execute_simple(conn, "PRAGMA integrity_check", M_FALSE);
	M_sql_error_t   err    = M_SQL_ERROR_SUCCESS;
	M_sql_report_t *report = M_sql_report_create(M_SQL_REPORT_FLAG_OMIT_HEADERS|M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED);
	char           *csv    = NULL;
	char            temp[256];

	err = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "integrity_check failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		goto done;
	}

	err = M_sql_report_process(report, stmt, NULL, &csv, NULL, temp, sizeof(temp));
	if (err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "integrity_check failed to generate report: %s: %s", M_sql_error_string(err), temp);
		goto done;
	}

	if (!M_str_eq_max(csv, "ok\r\n", 3)) {
		M_snprintf(error, error_size, "integrity_check returned inconsistencies, database is corrupt.");
		err = M_SQL_ERROR_QUERY_FAILURE;
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, csv);
		goto done;
	}

done:
	M_sql_stmt_destroy(stmt);
	M_sql_report_destroy(report);
	M_free(csv);
	return (err == M_SQL_ERROR_SUCCESS)?M_TRUE:M_FALSE;
}


static M_bool sqlite_analyze(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_stmt_t *stmt = M_sql_conn_execute_simple(conn, "ANALYZE", M_FALSE);
	M_sql_error_t err;

	err = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "analyze failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		goto done;
	}

done:
	M_sql_stmt_destroy(stmt);
	return (err == M_SQL_ERROR_SUCCESS)?M_TRUE:M_FALSE;
}


static M_bool sqlite_set_journal_mode(M_sql_conn_t *conn, const char *mode, char *error, size_t error_size)
{
	M_sql_stmt_t   *stmt   = NULL;
	char            query[256];
	M_sql_error_t   err    = M_SQL_ERROR_SUCCESS;
	M_sql_report_t *report = M_sql_report_create(M_SQL_REPORT_FLAG_OMIT_HEADERS|M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED);
	char           *csv    = NULL;
	char            temp[256];

	M_snprintf(query, sizeof(query), "PRAGMA journal_mode=%s", mode);
	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);

	err   = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "journal_mode failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		goto done;
	}

	err = M_sql_report_process(report, stmt, NULL, &csv, NULL, temp, sizeof(temp));
	if (err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "journal_mode failed to generate report: %s: %s", M_sql_error_string(err), temp);
		goto done;
	}

	/* Remove any leading/trailing whitespace */
	M_str_trim(csv);

	if (!M_str_caseeq(csv, mode)) {
		M_snprintf(error, error_size, "journal mode does not match what was requested (requested %s, received %s)", mode, csv);
		err = M_SQL_ERROR_QUERY_FAILURE;
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, error);
		M_sql_driver_trace_message(M_FALSE, NULL, conn, err, csv);
		goto done;
	}

done:
	M_sql_stmt_destroy(stmt);
	M_sql_report_destroy(report);
	M_free(csv);
	return (err == M_SQL_ERROR_SUCCESS)?M_TRUE:M_FALSE;
}


static M_sql_error_t sqlite_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t           err  = M_SQL_ERROR_SUCCESS;
	sqlite_connpool_data_t *data = is_readonly?&dpool->readonly:&dpool->primary;

	if (!is_first_in_pool)
		return M_SQL_ERROR_SUCCESS;

	if (data->integrity_check &&
	    !sqlite_verify_integrity(conn, error, error_size)) {
		err = M_SQL_ERROR_CONN_FAILED;
		goto fail;
	}

	if (data->analyze && !sqlite_analyze(conn, error, error_size)) {
		err = M_SQL_ERROR_CONN_FAILED;
		goto fail;
	}

	if (!sqlite_set_journal_mode(conn, data->journal_mode, error, error_size)) {
		err = M_SQL_ERROR_CONN_FAILED;
		goto fail;
	}

fail:
	return err;
}


static void sqlite_cb_disconnect(M_sql_driver_conn_t *conn)
{
	if (conn == NULL)
		return;
	if (conn->conn != NULL)
		sqlite3_close(conn->conn);
	M_free(conn);
}


static size_t sqlite_num_process_rows(M_sql_driver_conn_t *dconn, size_t num_params_per_row, size_t num_rows)
{
	int    max_params;
	int    max_compound;
	size_t max_rows;

	if (num_rows == 1)
		return num_rows;

	if (num_params_per_row == 0)
		return 1;

	/* Maximum number of bound variables */
	max_params = sqlite3_limit(dconn->conn, SQLITE_LIMIT_VARIABLE_NUMBER, -1);
	if (max_params <= 0)
		return 1;

	/* Maximum limit on compound select, on some versions of SQLite this appears to
	 * also apply per row on insert */
	max_compound =  sqlite3_limit(dconn->conn, SQLITE_LIMIT_COMPOUND_SELECT, -1);
	if (max_compound <= 0)
		return 1;

	/* Get max rows based on total maximum parameters compared to params per row */
	max_rows = ((size_t)max_params) / num_params_per_row;
	if (max_rows == 0)
		return 1;

	/* Reduce maximum rows to compound limit, if applicable */
	max_rows = M_MIN((size_t)max_compound, max_rows);

	/* Reduce maximum rows to actual number of rows provided, if applicable */
	max_rows = M_MIN(num_rows, max_rows);

	return max_rows;
}


static char *sqlite_cb_queryformat(M_sql_conn_t *conn, const char *query, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	return M_sql_driver_queryformat(query, M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD,
	                                num_params, sqlite_num_process_rows(M_sql_driver_conn_get_conn(conn), num_params, num_rows),
	                                error, error_size);
}


static size_t sqlite_cb_queryrowcnt(M_sql_conn_t *conn, size_t num_params_per_row, size_t num_rows)
{
	return sqlite_num_process_rows(M_sql_driver_conn_get_conn(conn), num_params_per_row, num_rows);
}


static void sqlite_cb_prepare_destroy(M_sql_driver_stmt_t *stmt)
{
	if (stmt == NULL)
		return;
	if (stmt->stmt != NULL)
		sqlite3_finalize(stmt->stmt);
	M_free(stmt);
}

static M_sql_error_t sqlite_rc_to_error(int rc)
{
	switch (rc & 0xFF) {
		case SQLITE_ABORT:
		case SQLITE_BUSY:
		case SQLITE_FULL:
		case SQLITE_LOCKED: /* if breaking out of retries, return deadlock so everything rolls back */
			return M_SQL_ERROR_QUERY_DEADLOCK;
		case SQLITE_OK:
		case SQLITE_DONE:
			return M_SQL_ERROR_SUCCESS;
		case SQLITE_ROW:
			return M_SQL_ERROR_SUCCESS_ROW;
		case SQLITE_CONSTRAINT:
			return M_SQL_ERROR_QUERY_CONSTRAINT;
		case SQLITE_IOERR:
		case SQLITE_CANTOPEN:
		case SQLITE_READONLY:
		case SQLITE_CORRUPT:
			return M_SQL_ERROR_CONN_LOST;
		default:
			break;
	}
	return M_SQL_ERROR_QUERY_FAILURE;
}


static M_sql_error_t sqlite_bind_params(M_sql_driver_conn_t *conn, M_sql_driver_stmt_t *driver_stmt, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	size_t i;
	size_t row;
	size_t num_rows;
	size_t num_cols;
	int    rc;

	num_rows = sqlite_num_process_rows(conn, M_sql_driver_stmt_bind_cnt(stmt), M_sql_driver_stmt_bind_rows(stmt));
	num_cols = M_sql_driver_stmt_bind_cnt(stmt);

	for (row = 0; row < num_rows; row++) {
		for (i = 0; i < num_cols; i++) {
			int paramid = (int)((row * num_cols) + i + 1);
			if (M_sql_driver_stmt_bind_isnull(stmt, row, i)) {
				rc = sqlite3_bind_null(driver_stmt->stmt, paramid);
			} else {
				switch (M_sql_driver_stmt_bind_get_type(stmt, row, i)) {
					case M_SQL_DATA_TYPE_BOOL:
						rc = sqlite3_bind_int(driver_stmt->stmt, paramid, M_sql_driver_stmt_bind_get_bool(stmt, row, i)?1:0);
						break;
					case M_SQL_DATA_TYPE_INT16:
						rc = sqlite3_bind_int(driver_stmt->stmt, paramid, (int)M_sql_driver_stmt_bind_get_int16(stmt, row, i));
						break;
					case M_SQL_DATA_TYPE_INT32:
						rc = sqlite3_bind_int(driver_stmt->stmt, paramid, (int)M_sql_driver_stmt_bind_get_int32(stmt, row, i));
						break;
					case M_SQL_DATA_TYPE_INT64:
						rc = sqlite3_bind_int64(driver_stmt->stmt, paramid, (sqlite_int64)M_sql_driver_stmt_bind_get_int64(stmt, row, i));
						break;
					case M_SQL_DATA_TYPE_TEXT:
						rc = sqlite3_bind_text(driver_stmt->stmt, paramid, M_sql_driver_stmt_bind_get_text(stmt, row, i),
						                       (int)M_sql_driver_stmt_bind_get_text_len(stmt, row, i), SQLITE_TRANSIENT);
						break;
					case M_SQL_DATA_TYPE_BINARY:
						rc = sqlite3_bind_blob(driver_stmt->stmt, paramid, M_sql_driver_stmt_bind_get_binary(stmt, row, i),
						                       (int)M_sql_driver_stmt_bind_get_binary_len(stmt, row, i), SQLITE_TRANSIENT);
						break;
					default:
						rc = SQLITE_MISUSE;
						break;
				}
			}
			if (rc != SQLITE_OK) {
				M_snprintf(error, error_size, "Failed to bind parameter %d:%d - id %d (%d): %s", (int)row+1, (int)i+1, paramid, rc, sqlite3_errmsg(conn->conn));
				return sqlite_rc_to_error(rc);
			}
		}
	}

	return M_SQL_ERROR_SUCCESS;
}


static M_sql_error_t sqlite_cb_prepare(M_sql_driver_stmt_t **driver_stmt, M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	int                  rc;
	M_sql_driver_conn_t *driver_conn = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err         = M_SQL_ERROR_SUCCESS;
	const char          *query       = M_sql_driver_stmt_get_query(stmt);
	M_bool               new_stmt    = M_FALSE;

	if (*driver_stmt != NULL) {
		if (sqlite3_reset((*driver_stmt)->stmt) != SQLITE_OK) {
			/* Can't reset it, so we need to create a new statement handle
			 * instead.  The caller will notice the statement handle was
			 * changed and should call the prepare_destroy() on the old handle
			 * to free it automatically */
			*driver_stmt = NULL;
		} else {
			sqlite3_clear_bindings((*driver_stmt)->stmt);
		}
	}

//M_printf("Query |%s|\n", query);

	if (*driver_stmt == NULL) {
		size_t retry_cnt = 0;
		new_stmt         = M_TRUE;
		*driver_stmt     = M_malloc_zero(sizeof(**driver_stmt));

		do {
			rc  = sqlite3_prepare_v2(driver_conn->conn, query, (int)M_str_len(query), &(*driver_stmt)->stmt, NULL);
			err = sqlite_rc_to_error(rc);

			/* Need to track if this is a commit for different retry logic */
			if (M_str_eq_max(query, "COMMIT", 5))
				(*driver_stmt)->is_commit = M_TRUE;

			if ((rc & 0xFF) == SQLITE_LOCKED) {
				char temp[256];
				M_snprintf(temp, sizeof(temp), "sqlite3_prepare_v2() returned locked, retry (%zu).", retry_cnt);
				M_sql_driver_trace_message(M_FALSE, NULL, conn, M_SQL_ERROR_UNSET, temp);
				if (retry_cnt >= 10) {
					break;
				}
				M_thread_sleep(M_sql_rollback_delay_ms(M_sql_driver_conn_get_pool(conn)) * 1000);
			}
			retry_cnt++;
		} while ((rc & 0xFF) == SQLITE_LOCKED);

		if (err != M_SQL_ERROR_SUCCESS) {
			M_snprintf(error, error_size, "%s", sqlite3_errmsg(driver_conn->conn));
			goto fail;
		}
	} else {
		sqlite3_reset((*driver_stmt)->stmt);
		sqlite3_clear_bindings((*driver_stmt)->stmt);
	}

	err = sqlite_bind_params(driver_conn, *driver_stmt, stmt, error, error_size);

fail:
	if (err != M_SQL_ERROR_SUCCESS) {
		if (*driver_stmt != NULL && new_stmt) {
			sqlite_cb_prepare_destroy(*driver_stmt);
			*driver_stmt = NULL;
		}
	}
	return err;
}

static void sqlite_createtable_suffix(M_sql_connpool_t *pool, M_buf_t *query)
{
	(void) pool;

	/* Prefer strict data type conversions.  Error if it can't be done.  Added in 3.37.0.
	 * Otherwise in an integer column, if you pass xyz it will store xyz instead of erroring
	 * which would mean someone developing against sqlite might not realize every other
	 * database will break */
	if (SQLITE_VERSION_NUMBER >= 3037000)
		M_buf_add_str(query, " STRICT");
}


static M_sql_data_type_t sqlite_type_to_mtype(int type, const char *decltype, size_t *type_size)
{
	*type_size = 0;

	if (M_str_caseeq(decltype, "TINYINT")) {
		return M_SQL_DATA_TYPE_BOOL;
	} else if (M_str_caseeq(decltype, "SMALLINT")) {
		return M_SQL_DATA_TYPE_INT16;
	} else if (M_str_caseeq(decltype, "INT")) {
		return M_SQL_DATA_TYPE_INT32;
	} else if (M_str_caseeq(decltype, "INTEGER") || M_str_caseeq(decltype, "BIGINT")) {
		return M_SQL_DATA_TYPE_INT64;
	} else if (M_str_caseeq(decltype, "BLOB")) {
		return M_SQL_DATA_TYPE_BINARY;
	} else if (M_str_caseeq(decltype, "TEXT")) {
		return M_SQL_DATA_TYPE_TEXT;
	} else if (M_str_caseeq_max(decltype, "BLOB(", 5)) {
		*type_size = M_str_to_uint32(decltype+5);
		return M_SQL_DATA_TYPE_BINARY;
	} else if (M_str_caseeq_max(decltype, "VARCHAR(", 8)) {
		*type_size = M_str_to_uint32(decltype+8);
		return M_SQL_DATA_TYPE_TEXT;
	}

	switch (type) {
		case SQLITE_INTEGER:
			return M_SQL_DATA_TYPE_INT64;

		case SQLITE_BLOB:
			return M_SQL_DATA_TYPE_BINARY;

		case SQLITE_NULL:
			return M_SQL_DATA_TYPE_UNKNOWN;
		case SQLITE_TEXT:
		case SQLITE_FLOAT:
		default:
			break;
	}

	return M_SQL_DATA_TYPE_TEXT;
}


static void sqlite_fetch_result_metadata(M_sql_driver_conn_t *conn, M_sql_driver_stmt_t *driver_stmt, M_sql_stmt_t *stmt)
{
	size_t col_cnt = (size_t)sqlite3_column_count(driver_stmt->stmt);
	size_t i;

	if (!col_cnt) {
#if SQLITE_VERSION_NUMBER >= 3037000
		/* sqlite3_changes64() for large changesets.  Unlikely to happen with sqlite, but
		 * better to use this function always if available */
		M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)sqlite3_changes64(conn->conn));
#else
		M_sql_driver_stmt_result_set_affected_rows(stmt, (size_t)sqlite3_changes(conn->conn));
#endif
		return;
	}

	M_sql_driver_stmt_result_set_num_cols(stmt, col_cnt);
	for (i=0; i<col_cnt; i++) {
		int               type       = sqlite3_column_type(driver_stmt->stmt, (int)i);
		const char       *decltype   = sqlite3_column_decltype(driver_stmt->stmt, (int)i);
		size_t            mtype_size = 0;
		M_sql_data_type_t mtype      = sqlite_type_to_mtype(type, decltype, &mtype_size);

		M_sql_driver_stmt_result_set_col_name(stmt, i, sqlite3_column_name(driver_stmt->stmt, (int)i));
		/* NOTE: SQLite might actually set a column type to NULL, because they are talking about
		 * the specific cell, not the definition.  So we have to update the data types later as we
		 * get in more cells for the specific column ... ugh */
		M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, mtype_size);
	}
}


static M_sql_error_t sqlite_cb_execute(M_sql_conn_t *conn, M_sql_stmt_t *stmt, size_t *rows_executed, char *error, size_t error_size)
{
	int                  real_rc;
	int                  rc;
	M_sql_error_t        err;
	size_t               retry_cnt   = 0;
	M_sql_driver_stmt_t *driver_stmt = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t *driver_conn = M_sql_driver_conn_get_conn(conn);

	/* Get number of rows that are processed at once, SQLite supports the mysql-style
	 * comma-delimited values for inserting multiple rows. */
	*rows_executed = sqlite_num_process_rows(driver_conn, M_sql_driver_stmt_bind_cnt(stmt), M_sql_driver_stmt_bind_rows(stmt));

	while (1) {
		real_rc = sqlite3_step(driver_stmt->stmt);

		/* We're using extended error codes, so we only want the first 8
		 * bits to check the original non-extended codes ... but keep the
		 * extended codes for debugging purposes */
		rc      = real_rc & 0xFF;
		err     = sqlite_rc_to_error(real_rc);

		if (err == M_SQL_ERROR_SUCCESS || err == M_SQL_ERROR_SUCCESS_ROW) {
			sqlite_fetch_result_metadata(driver_conn, driver_stmt, stmt);
			break;
		}

		/* Seems to cause deadlocks */
#if 0
		if (rc == SQLITE_BUSY && driver_stmt->is_commit) {
			/* Docs say we should retry on commit */
		} else
#endif
		if (rc == SQLITE_LOCKED) {
			char temp[256];

			/* Retry */
			M_snprintf(temp, sizeof(temp), "sqlite3_step (execute) returned locked, retry (%zu).", retry_cnt);
			M_sql_driver_trace_message(M_FALSE, NULL, conn, M_SQL_ERROR_UNSET, temp);
			if (retry_cnt >= 10) {
				M_snprintf(error, error_size, "Rollback (%d), max retry count: %s", real_rc, sqlite3_errmsg(driver_conn->conn));
				break;
			}
		} else {
			M_snprintf(error, error_size, "Query Failed (%d): %s", real_rc, sqlite3_errmsg(driver_conn->conn));
			break;
		}

		/* On retry events, should call sqlite3 reset before retrying */
		sqlite3_reset(driver_stmt->stmt);

		/* Sleep a the retry is probably due to some other caller */
		M_thread_sleep(M_sql_rollback_delay_ms(M_sql_driver_conn_get_pool(conn)) * 1000);

		retry_cnt++;
	}

	return err;
}


/* XXX: Fetch Cancel ? */

static M_sql_error_t sqlite_cb_fetch(M_sql_conn_t *conn, M_sql_stmt_t *stmt, char *error, size_t error_size)
{
	int                  real_rc;
	int                  rc;
	M_sql_driver_stmt_t *driver_stmt = M_sql_driver_stmt_get_stmt(stmt);
	M_sql_driver_conn_t *driver_conn = M_sql_driver_conn_get_conn(conn);
	M_sql_error_t        err;
	size_t               retry_cnt   = 0;

	while (1) {
		size_t i;

		/* Output the current row of data */
		for (i=0; i<(size_t)sqlite3_column_count(driver_stmt->stmt); i++) {
			M_buf_t *buf  = M_sql_driver_stmt_result_col_start(stmt);
			int      type = sqlite3_column_type(driver_stmt->stmt, (int)i);

			switch (type) {
				case SQLITE_INTEGER:
					M_buf_add_int(buf, sqlite3_column_int64(driver_stmt->stmt, (int)i));
					break;
				case SQLITE_BLOB:
					M_buf_add_bytes(buf, sqlite3_column_blob(driver_stmt->stmt, (int)i), (size_t)sqlite3_column_bytes(driver_stmt->stmt, (int)i));
					break;
				case SQLITE_NULL:
					/* Append nothing */
					break;
				default:
					M_buf_add_str(buf, (const char *)sqlite3_column_text(driver_stmt->stmt, (int)i));
					break;
			}

			if (type != SQLITE_NULL) {
				/* All columns with data require NULL termination, even binary.  Otherwise its considered a NULL column. */
				M_buf_add_byte(buf, 0); /* Manually add NULL terminator */
			}

			/* NOTE: Funky FixUp! */
			if (M_sql_stmt_result_col_type(stmt, i, NULL) == M_SQL_DATA_TYPE_UNKNOWN && type != SQLITE_NULL) {
				const char       *decltype   = sqlite3_column_decltype(driver_stmt->stmt, (int)i);
				size_t            mtype_size = 0;
				M_sql_data_type_t mtype      = sqlite_type_to_mtype(type, decltype, &mtype_size);
				M_sql_driver_stmt_result_set_col_type(stmt, i, mtype, mtype_size);
			}
		}
		M_sql_driver_stmt_result_row_finish(stmt);

		/* Attempt to fetch next row */
		real_rc = sqlite3_step(driver_stmt->stmt);

		/* We're using extended error codes, so we only want the first 8
		 * bits to check the original non-extended codes ... but keep the
		 * extended codes for debugging purposes */
		rc      = real_rc & 0xFF;
		err     = sqlite_rc_to_error(real_rc);

		/* Successfully either fetched the next row or there are no more results */
		if (err == M_SQL_ERROR_SUCCESS || err == M_SQL_ERROR_SUCCESS_ROW) {
			break;
		}

		if (rc == SQLITE_LOCKED) {
			/* Retry */
			M_sql_driver_trace_message(M_FALSE, NULL, conn, M_SQL_ERROR_UNSET, "sqlite3_step (fetch) returned locked, retry.");
			if (retry_cnt >= 10) {
				M_snprintf(error, error_size, "Rollback (%d), max retry count: %s", real_rc, sqlite3_errmsg(driver_conn->conn));
				break;
			}
		} else if (rc == SQLITE_BUSY) {
			/* If busy and not COMMIT statement, rollback */
			M_snprintf(error, error_size, "Rollback (%d): %s", real_rc, sqlite3_errmsg(driver_conn->conn));
			break;
		} else {
			M_snprintf(error, error_size, "Fetch Failed (%d): %s", real_rc, sqlite3_errmsg(driver_conn->conn));
			break;
		}

		/* On retry events, should call sqlite3 reset before retrying */
		sqlite3_reset(driver_stmt->stmt);

		/* Sleep a the retry is probably due to some other caller */
		M_thread_sleep(M_sql_rollback_delay_ms(M_sql_driver_conn_get_pool(conn)) * 1000);

		retry_cnt++;
	}

	return err;
}


static M_sql_error_t sqlite_cb_begin(M_sql_conn_t *conn, M_sql_isolation_t isolation, char *error, size_t error_size)
{
	M_sql_stmt_t *stmt;
	const char   *query = "BEGIN TRANSACTION";
	M_sql_error_t err;

	if (isolation == M_SQL_ISOLATION_SERIALIZABLE) {
		query = "BEGIN IMMEDIATE TRANSACTION";
	}
	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "BEGIN failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	}

	M_sql_stmt_destroy(stmt);
	return err;
}


static M_sql_error_t sqlite_cb_rollback(M_sql_conn_t *conn)
{
	M_sql_stmt_t *stmt;
	const char   *query = "ROLLBACK TRANSACTION";
	M_sql_error_t err;

	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	/* Ignore failues as sqlite may sometimes implicitly rollback, its ok for it to fail. */
	M_sql_stmt_destroy(stmt);
	return err;
}


static M_sql_error_t sqlite_cb_commit(M_sql_conn_t *conn, char *error, size_t error_size)
{
	M_sql_stmt_t *stmt;
	const char   *query = "COMMIT TRANSACTION";
	M_sql_error_t err;

	stmt = M_sql_conn_execute_simple(conn, query, M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "COMMIT failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	}
	M_sql_stmt_destroy(stmt);

	/* If a commit fails for any reason, rollback as it is not re-tryable */
	if (err != M_SQL_ERROR_SUCCESS)
		sqlite_cb_rollback(conn);

	return err;
}


static M_bool sqlite_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len, M_bool is_cast)
{
	/* NOTE: SQLite really only supports TEXT, NUMERIC, INTEGER, REAL, BLOB.
	 *       So we are just mapping to these primitives.  It does support
	 *       passing other datatypes, but it really just translates them into
	 *       the primitive.  For instance, VARCHAR(32) by no means actually imposes
	 *       a 32 character limit as you'd expect. */
	(void)pool;
	(void)is_cast;
	(void)max_len; /* can't be honored by sqlite */
	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
		case M_SQL_DATA_TYPE_INT16:
		case M_SQL_DATA_TYPE_INT32:
		case M_SQL_DATA_TYPE_INT64:
			M_buf_add_str(buf, "INTEGER");
			return M_TRUE;
		case M_SQL_DATA_TYPE_TEXT:
			M_buf_add_str(buf, "TEXT");
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			M_buf_add_str(buf, "BLOB");
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


static M_bool sqlite_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_OP, query, op, exp1, exp2);
}


static M_sql_driver_t M_sql_sqlite = {
	M_SQL_DRIVER_VERSION,         /* Driver/Module subsystem version */
	"sqlite",                     /* Short name of module */
	"SQLite driver for mstdlib",  /* Display name of module */
	"1.0.1",                      /* Internal module version */

	sqlite_cb_init,               /* Callback used for module initialization. */
	sqlite_cb_destroy,            /* Callback used for module destruction/unloading. */
	sqlite_cb_createpool,         /* Callback used for pool creation */
	sqlite_cb_destroypool,        /* Callback used for pool destruction */
	sqlite_cb_connect,            /* Callback used for connecting to the db */
	sqlite_cb_serverversion,      /* Callback used to get the server name/version string */
	sqlite_cb_connect_runonce,    /* Callback used after connection is established, but before first query to set run-once options. */
	sqlite_cb_disconnect,         /* Callback used to disconnect from the db */
	sqlite_cb_queryformat,        /* Callback used for reformatting a query to the sql db requirements */
	sqlite_cb_queryrowcnt,        /* Callback used for determining how many rows will be processed by the current execution (chunking rows) */
	sqlite_cb_prepare,            /* Callback used for preparing a query for execution */
	sqlite_cb_prepare_destroy,    /* Callback used to destroy the driver-specific prepared statement handle */
	sqlite_cb_execute,            /* Callback used for executing a prepared query */
	sqlite_cb_fetch,              /* Callback used to fetch result data/rows from server */
	sqlite_cb_begin,              /* Callback used to begin a transaction */
	sqlite_cb_rollback,           /* Callback used to rollback a transaction */
	sqlite_cb_commit,             /* Callback used to commit a transaction */
	sqlite_cb_datatype,           /* Callback used to convert to data type for server */
	sqlite_createtable_suffix,    /* Callback used to append additional data to the Create Table query string */
	NULL,                         /* Callback used to append row-level locking data */
	sqlite_cb_append_bitop,       /* Callback used to append a bit operation */
	NULL,                         /* Callback used to rewrite an index name to comply with DB requirements */

	NULL,                         /* Handle for loaded driver - must be initialized to NULL */
};

/*! Defines function that references M_sql_driver_t M_sql_##name for module loading */
M_SQL_DRIVER(sqlite)
