#include "m_config.h"
#include <stdlib.h>
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/mstdlib_formats.h>

#define DEBUG 1
#define INSERT_ROWS 10000

static const char *coltype2str(M_sql_data_type_t type)
{
	switch (type) {
		case M_SQL_DATA_TYPE_UNKNOWN:
			return "UNKNOWN";
		case M_SQL_DATA_TYPE_BOOL:
			return "BOOL";
		case M_SQL_DATA_TYPE_INT16:
			return "INT16";
		case M_SQL_DATA_TYPE_INT32:
			return "INT32";
		case M_SQL_DATA_TYPE_INT64:
			return "INT64";
		case M_SQL_DATA_TYPE_TEXT:
			return "TEXT";
		case M_SQL_DATA_TYPE_BINARY:
			return "BINARY";
	}
	return "WTF";
}

static const char *sql_trace_type(M_sql_trace_t event_type)
{
	switch (event_type) {
		case M_SQL_TRACE_CONNECTING:
			return "CONNECTING";
		case M_SQL_TRACE_CONNECTED:
			return "CONNECTED";
		case M_SQL_TRACE_CONNECT_FAILED:
			return "CONNECT_FAILED";
		case M_SQL_TRACE_DISCONNECTING:
			return "DISCONNECTING";
		case M_SQL_TRACE_DISCONNECTED:
			return "DISCONNECTED";
		case M_SQL_TRACE_BEGIN_START:
			return "BEGIN_START";
		case M_SQL_TRACE_BEGIN_FINISH:
			return "BEGIN_FINISH";
		case M_SQL_TRACE_ROLLBACK_START:
			return "ROLLBACK_START";
		case M_SQL_TRACE_ROLLBACK_FINISH:
			return "ROLLBACK_FINISH";
		case M_SQL_TRACE_COMMIT_START:
			return "COMMIT_START";
		case M_SQL_TRACE_COMMIT_FINISH:
			return "COMMIT_FINISH";
		case M_SQL_TRACE_EXECUTE_START:
			return "EXECUTE_START";
		case M_SQL_TRACE_EXECUTE_FINISH:
			return "EXECUTE_FINISH";
		case M_SQL_TRACE_FETCH_START:
			return "FETCH_START";
		case M_SQL_TRACE_FETCH_FINISH:
			return "FETCH_FINISH";
		case M_SQL_TRACE_CONNFAIL:
			return "CONNFAIL";
		case M_SQL_TRACE_TRANFAIL:
			return "TRANFAIL";
		case M_SQL_TRACE_DRIVER_DEBUG:
			return "DRIVER_DEBUG";
		case M_SQL_TRACE_DRIVER_ERROR:
			return "DRIVER_ERROR";
		case M_SQL_TRACE_STALL_QUERY:
			return "STALL_QUERY";
		case M_SQL_TRACE_STALL_TRANS_IDLE:
			return "STALL_TRANS_IDLE";
		case M_SQL_TRACE_STALL_TRANS_LONG:
			return "STALL_TRANS_LONG";
	}
	return NULL;
}


static const char *sql_conn_type(M_sql_conn_type_t type)
{
	switch (type) {
		case M_SQL_CONN_TYPE_PRIMARY:
			return "RW";
		case M_SQL_CONN_TYPE_READONLY:
			return "RO";
		case M_SQL_CONN_TYPE_UNKNOWN:
			break;
	}
	return "UN";
}


static void sql_trace(M_sql_trace_t event_type, const M_sql_trace_data_t *data, void *arg)
{
	M_buf_t      *buf = M_buf_create();
	const char   *msg;

	(void)arg;

	M_buf_add_str(buf, "(CONN ");
	M_buf_add_str(buf, sql_conn_type(M_sql_trace_get_conntype(data)));
	M_buf_add_str(buf, "#");
	M_buf_add_uint(buf, M_sql_trace_get_conn_id(data));
	M_buf_add_str(buf, ") ");
	M_buf_add_str(buf, "[");
	M_buf_add_str(buf, sql_trace_type(event_type));
	M_buf_add_str(buf, "] ");

	M_buf_add_str(buf, M_sql_error_string(M_sql_trace_get_error(data)));

	msg = M_sql_trace_get_error_string(data);
	if (!M_str_isempty(msg)) {
		M_buf_add_str(buf, " - ");
		M_buf_add_str(buf, msg);
	}

	switch (event_type) {
		case M_SQL_TRACE_CONNECTED:
		case M_SQL_TRACE_CONNECT_FAILED:
		case M_SQL_TRACE_DISCONNECTING:
		case M_SQL_TRACE_CONNFAIL:
		case M_SQL_TRACE_DISCONNECTED:
		case M_SQL_TRACE_BEGIN_FINISH:
		case M_SQL_TRACE_ROLLBACK_FINISH:
		case M_SQL_TRACE_COMMIT_FINISH:
		case M_SQL_TRACE_EXECUTE_FINISH:
		case M_SQL_TRACE_TRANFAIL:
		case M_SQL_TRACE_FETCH_FINISH:
		case M_SQL_TRACE_STALL_QUERY:
		case M_SQL_TRACE_STALL_TRANS_IDLE:
		case M_SQL_TRACE_STALL_TRANS_LONG:
			M_buf_add_str(buf, " (");
			M_buf_add_uint(buf, M_sql_trace_get_duration_ms(data));
			M_buf_add_str(buf, "ms)");
			break;
		default:
			break;
	}

	switch (event_type) {
		case M_SQL_TRACE_FETCH_FINISH:
		case M_SQL_TRACE_DISCONNECTED:
		case M_SQL_TRACE_STALL_TRANS_IDLE:
			M_buf_add_str(buf, " (overall ");
			M_buf_add_uint(buf, M_sql_trace_get_total_duration_ms(data));
			M_buf_add_str(buf, "ms)");
			break;
		default:
			break;
	}

	switch (event_type) {
		case M_SQL_TRACE_EXECUTE_START:
		case M_SQL_TRACE_EXECUTE_FINISH:
		case M_SQL_TRACE_FETCH_START:
		case M_SQL_TRACE_FETCH_FINISH:
		case M_SQL_TRACE_TRANFAIL:
		case M_SQL_TRACE_STALL_QUERY:
		case M_SQL_TRACE_STALL_TRANS_LONG:
			if (M_sql_trace_get_query_user(data) != NULL) {
				M_buf_add_str(buf, " UserQuery='");
				M_buf_add_str(buf, M_sql_trace_get_query_user(data));
				M_buf_add_str(buf, "'");
			}
			break;
		default:
			break;
	}

	switch (event_type) {
		case M_SQL_TRACE_EXECUTE_FINISH:
		case M_SQL_TRACE_FETCH_START:
		case M_SQL_TRACE_FETCH_FINISH:
		case M_SQL_TRACE_TRANFAIL:
		case M_SQL_TRACE_STALL_QUERY:
		case M_SQL_TRACE_STALL_TRANS_LONG:
			if (M_sql_trace_get_query_prepared(data) != NULL) {
				M_buf_add_str(buf, " PreparedQuery='");
				M_buf_add_str(buf, M_sql_trace_get_query_prepared(data));
				M_buf_add_str(buf, "'");
			}
			break;
		default:
			break;
	}

	switch (event_type) {
		case M_SQL_TRACE_EXECUTE_START:
		case M_SQL_TRACE_EXECUTE_FINISH:
		case M_SQL_TRACE_FETCH_START:
		case M_SQL_TRACE_FETCH_FINISH:
		case M_SQL_TRACE_TRANFAIL:
		case M_SQL_TRACE_STALL_QUERY:
		case M_SQL_TRACE_STALL_TRANS_LONG:
			if (M_sql_trace_get_bind_rows(data) > 0) {
				M_buf_add_str(buf, " bind_rows=");
				M_buf_add_uint(buf, M_sql_trace_get_bind_rows(data));
				M_buf_add_str(buf, " bind_cols=");
				M_buf_add_uint(buf, M_sql_trace_get_bind_cols(data));
			}
			break;
		default:
			break;
	}

	switch (event_type) {
		case M_SQL_TRACE_EXECUTE_FINISH:
			if (M_sql_trace_get_affected_rows(data)) {
				M_buf_add_str(buf, " affected_rows=");
				M_buf_add_uint(buf, M_sql_trace_get_affected_rows(data));
			}
			break;
		case M_SQL_TRACE_FETCH_FINISH:
			if (M_sql_trace_get_has_result_rows(data)) {
				M_buf_add_str(buf, " result_rows=");
				M_buf_add_uint(buf, M_sql_trace_get_result_row_count(data));
			}
		default:
			break;
	}

#if defined(DEBUG) && DEBUG >= 1
	M_printf("%s\n", M_buf_peek(buf));
#endif
	M_buf_cancel(buf);
}

static const char insert_query[] = "INSERT INTO \"foo\" (\"key\", \"name\", \"i16col\", \"i32col\", \"boolcol\", \"bincol\", \"hugebincol\") VALUES (?, ?, ?, ?, ?, ?, ?)";

static M_sql_error_t check_sql_trans(M_sql_trans_t *trans, void *arg, char *error, size_t error_size)
{
	M_sql_stmt_t  *stmt;
	M_sql_error_t  err;
	size_t         i;
	char           temp[32];
	(void)arg;

	/* Insert Record */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, insert_query);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(INSERT) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	M_sql_stmt_bind_int32(stmt, 1);
	M_sql_stmt_bind_text_const(stmt, "Hello World", 0);
	M_sql_stmt_bind_int32(stmt, (M_int16)(0 & 0xFFFF)); /* Insert int32 into int16 */
	M_sql_stmt_bind_int16(stmt, (M_int32)0);            /* Insert int16 into int32 */
	M_sql_stmt_bind_int16(stmt, (M_int16)(0 % 2));      /* Bind int16 to bool */
	M_sql_stmt_bind_binary_const(stmt, (const M_uint8 *)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16);
	M_sql_stmt_bind_binary_const(stmt, NULL, 0);
	err  = M_sql_trans_execute(trans, stmt);
	if (err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "M_sql_stmt_execute(INSERT) failed: %s", M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	/* Insert Another Record */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, insert_query);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(INSERT) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	M_sql_stmt_bind_int32(stmt, 2);
	M_sql_stmt_bind_text_const(stmt, "GoodBye", 0);
	M_sql_stmt_bind_bool(stmt, (M_bool)(1 % 2));   /* Bind bool to int16 */
	M_sql_stmt_bind_bool(stmt, (M_bool)(1 % 2));   /* Bind bool to int32 */
	M_sql_stmt_bind_int32(stmt, (M_int32)(1 % 2)); /* Bind int32 to bool */
	M_sql_stmt_bind_binary_const(stmt, (const M_uint8 *)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16);
	M_sql_stmt_bind_binary_const(stmt, NULL, 0);
	err  = M_sql_trans_execute(trans, stmt);
	if (err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "M_sql_stmt_execute(INSERT) failed: %s", M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	/* Insert 30 more records using row binding */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, insert_query);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(INSERT 30) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	for (i=0; i<INSERT_ROWS; i++) {
		M_sql_stmt_bind_int32(stmt, (M_int32)(3+i));
		M_snprintf(temp, sizeof(temp), "Row%zu", i+1);
		M_sql_stmt_bind_text_dup(stmt, temp, 0);
		M_sql_stmt_bind_int16(stmt, (M_int16)(i & 0xFFFF));
		M_sql_stmt_bind_int32(stmt, (M_int32)i);
		M_sql_stmt_bind_bool(stmt, (M_bool)i % 2);
		M_sql_stmt_bind_binary_const(stmt, NULL, 0);
		M_sql_stmt_bind_binary_const(stmt, NULL, 0);
		M_sql_stmt_bind_new_row(stmt);
	}
	err  = M_sql_trans_execute(trans, stmt);
	if (err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "M_sql_stmt_execute(INSERT 30) failed: %s", M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return err;
}

/* XXX: Additional needed checks:
 *      - multiple blobs per row
 *      - make sure strings aren't truncated on the right when they have trailing spaces
 *      - key conflicts return right codes
 *      - largish blobs are handled properly.
 *      - rollbacks/deadlocks work (multithreaded test?)
 */

START_TEST(check_sql)
{
	M_sql_error_t     err;
	M_sql_connpool_t *pool    = NULL;
	char              error[256];
	M_sql_stmt_t     *stmt;
	M_sql_report_t   *report  = M_sql_report_create(M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED);
	M_sql_table_t    *table   = NULL;
	char             *out     = NULL;
	size_t            out_len = 0;
	M_csv_t          *csv     = NULL;
	char              temp[64];
	size_t            i;
	const char       *driver;
	const char       *conn_str;
	const char       *sql_conns;
	const char       *username;
	const char       *password;
	M_uint8          *hugedata;
	M_int64           hugedataid;
	const M_uint8    *outbincol;
	size_t            outbincol_size;


	driver    = getenv("SQL_DRIVER");
	conn_str  = getenv("SQL_CONN_STR");
	sql_conns = getenv("SQL_CONNS");
	username  = getenv("SQL_USERNAME");
	password  = getenv("SQL_PASSWORD");

	if (M_str_isempty(driver)) {
		driver = "sqlite";
		M_printf("SQL_DRIVER env empty, using default %s\n", driver);
	}
	if (M_str_isempty(conn_str)) {
		conn_str = "path=./test.sqlite;integrity_check=yes";
		M_printf("SQL_CONN_STR env empty, using default '%s'\n", conn_str);
	}
	if (M_str_isempty(sql_conns)) {
		sql_conns = "2";
		M_printf("SQL_CONNS env empty, using default %s\n", sql_conns);
	}
	if (M_str_isempty(username)) {
		username = "";
		M_printf("SQL_USERNAME env empty, using default '%s'\n", username);
	}
	if (M_str_isempty(password)) {
		password = "";
		M_printf("SQL_PASSWORD env empty, using default '%s'\n", password);
	}

	/* Connect */
	err = M_sql_connpool_create(&pool, driver, conn_str, username, password, M_str_to_uint32(sql_conns), M_SQL_CONNPOOL_FLAG_PRESPAWN_ALL, error, sizeof(error));
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_connpool_create failed: %s: %s", M_sql_error_string(err), error);

	M_printf("SQL Driver        : %s (%s) v%s\n", M_sql_connpool_driver_display_name(pool), M_sql_connpool_driver_name(pool), M_sql_connpool_driver_version(pool));

	ck_assert_msg(M_sql_connpool_add_trace(pool, sql_trace, NULL) == M_TRUE, "M_sql_connpool_add_trace() failed");

	err = M_sql_connpool_start(pool, error, sizeof(error));
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_connpool_start failed: %s: %s", M_sql_error_string(err), error);

	M_printf("SQL Server Version: %s\n", M_sql_connpool_server_version(pool));

	if (M_sql_table_exists(pool, "foo")) {
		stmt = M_sql_stmt_create();
		err  = M_sql_stmt_prepare(stmt, "DROP TABLE \"foo\"");
		ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(DROP TABLE) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		err  = M_sql_stmt_execute(pool, stmt);
		ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_trans_execute(DROP TABLE) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
	}

	/* Create Schema */
	table = M_sql_table_create("foo");
	ck_assert_msg(table != NULL, "M_sql_table_create() failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NOTNULL, "key",        M_SQL_DATA_TYPE_INT64,     0,              NULL), "M_sql_table_add_col(id) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NONE,    "name",       M_SQL_DATA_TYPE_TEXT,     32,              NULL), "M_sql_table_add_col(name) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NONE,    "defaultval", M_SQL_DATA_TYPE_TEXT,     32, "'default value'"), "M_sql_table_add_col(defaultval) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NOTNULL, "i16col",     M_SQL_DATA_TYPE_INT16,     0,              NULL), "M_sql_table_add_col(i16col) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NONE,    "i32col",     M_SQL_DATA_TYPE_INT32,     0,              NULL), "M_sql_table_add_col(i32col) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NOTNULL, "boolcol",    M_SQL_DATA_TYPE_BOOL,      0,              NULL), "M_sql_table_add_col(boolcol) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NONE,    "bincol",     M_SQL_DATA_TYPE_BINARY, 1024,              NULL), "M_sql_table_add_col(bincol) failed");
	ck_assert_msg(M_sql_table_add_col(table, M_SQL_TABLE_COL_FLAG_NONE,    "hugebincol", M_SQL_DATA_TYPE_BINARY,    0,              NULL), "M_sql_table_add_col(hugebincol) failed");
	ck_assert_msg(M_sql_table_add_pk_col(table, "key"), "M_sql_table_add_pk_col(id) failed");
	ck_assert_msg(M_sql_table_add_index_str(table, M_SQL_INDEX_FLAG_NONE, "blah", "name,defaultval"), "M_sql_table_add_index_str(name,defaultval) failed");
	err = M_sql_table_execute(pool, table, error, sizeof(error));
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_table_execute() failed: %s", error);
	M_sql_table_destroy(table);


	/* Insert records in a transaction */
	err = M_sql_trans_process(pool, M_SQL_ISOLATION_READCOMMITTED, check_sql_trans, NULL, error, sizeof(error));
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_trans_process() failed: %s", error);


	/* Insert 2 more records using row binding, but without being in a txn (should start an implicit one) */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, insert_query);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(INSERT 2) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	for (i=0; i<2; i++) {
		M_sql_stmt_bind_int32(stmt, (M_int32)((3+INSERT_ROWS+i) & 0xFFFFFFFF));
		M_snprintf(temp, sizeof(temp), "Row%zu", INSERT_ROWS+i+1);
		M_sql_stmt_bind_text_dup(stmt, temp, 0);
		M_sql_stmt_bind_int16(stmt, (M_int16)((3+INSERT_ROWS+i) & 0xFFFF));
		M_sql_stmt_bind_int32(stmt, (M_int32)((3+INSERT_ROWS+i) & 0xFFFFFFFF));
		M_sql_stmt_bind_bool(stmt, (M_bool)(3+INSERT_ROWS+i) % 2);
		M_sql_stmt_bind_binary_const(stmt, (const unsigned char *)"0", 2);
		M_sql_stmt_bind_binary_const(stmt, NULL, 0);
		M_sql_stmt_bind_new_row(stmt);
	}
	err  = M_sql_stmt_execute(pool, stmt);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_execute(INSERT 2) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	M_sql_stmt_destroy(stmt);

	/* Insert huge binary column */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, insert_query);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(INSERT hugedata) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
#define HUGEDATA_SIZE (1024 * 1024)
	hugedata = M_malloc(HUGEDATA_SIZE);
	M_mem_set(hugedata, 0x0D, HUGEDATA_SIZE);
	hugedataid = (5+INSERT_ROWS);
	M_sql_stmt_bind_int64(stmt, hugedataid);
	M_snprintf(temp, sizeof(temp), "Row%zu", (size_t)INSERT_ROWS+5+1);
	M_sql_stmt_bind_text_dup(stmt, temp, 0);
	M_sql_stmt_bind_int16(stmt, (M_int16)((5+INSERT_ROWS) & 0xFFFF));
	M_sql_stmt_bind_int32(stmt, (M_int32)((5+INSERT_ROWS) & 0xFFFFFFFF));
	M_sql_stmt_bind_bool(stmt, (M_bool)(5+INSERT_ROWS) % 2);
	M_sql_stmt_bind_binary_const(stmt, NULL, 0);
	M_sql_stmt_bind_binary_own(stmt, hugedata, HUGEDATA_SIZE);
	err  = M_sql_stmt_execute(pool, stmt);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_execute(INSERT hugedata) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	M_sql_stmt_destroy(stmt);

	/* Insert another row using M_sql_stmt_groupinsert_prepare() - use some NULL values */
	stmt = M_sql_stmt_groupinsert_prepare(pool, insert_query);
	ck_assert_msg(stmt != NULL, "M_sql_stmt_groupinsert_prepare() failed");
	M_sql_stmt_bind_int32(stmt, (M_int32)((6+INSERT_ROWS) & 0xFFFFFFFF));
	M_sql_stmt_bind_text_const(stmt, NULL, 0); /* name */
	M_sql_stmt_bind_int16(stmt, (M_int16)((6+INSERT_ROWS) & 0xFFFF));
	M_sql_stmt_bind_int32_null(stmt); /* i32col */
	M_sql_stmt_bind_bool(stmt, (M_bool)(6+INSERT_ROWS) % 2);
	M_sql_stmt_bind_binary_const(stmt, NULL, 0);
	M_sql_stmt_bind_binary_const(stmt, NULL, 0);
	err  = M_sql_stmt_execute(pool, stmt);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_execute(groupinsert) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	M_sql_stmt_destroy(stmt);


	/* Query for the records we inserted */
	stmt = M_sql_stmt_create();
	M_sql_stmt_set_max_fetch_rows(stmt, 10000); /* Could be a large resultset, set the caching high */
	err  = M_sql_stmt_prepare(stmt, "SELECT * FROM \"foo\" ORDER BY \"key\"");
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(SELECT) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	err  = M_sql_stmt_execute(pool, stmt);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS_ROW, "M_sql_stmt_execute(SELECT) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));

	/* Validate column count */
	ck_assert_msg(M_sql_stmt_result_num_cols(stmt) == 8, "M_sql_stmt_result_num_cols() expected 8, got %zu", M_sql_stmt_result_num_cols(stmt));

	/* Validate column names */
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 0), "key"),        "col 0 name expected 'key' got '%s'",        M_sql_stmt_result_col_name(stmt, 0));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 1), "name"),       "col 1 name expected 'name' got '%s'",       M_sql_stmt_result_col_name(stmt, 1));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 2), "defaultval"), "col 2 name expected 'defaultval' got '%s'", M_sql_stmt_result_col_name(stmt, 2));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 3), "i16col"),     "col 3 name expected 'i16col' got '%s'",     M_sql_stmt_result_col_name(stmt, 3));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 4), "i32col"),     "col 4 name expected 'i32col' got '%s'",     M_sql_stmt_result_col_name(stmt, 4));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 5), "boolcol"),    "col 5 name expected 'boolcol' got '%s'",    M_sql_stmt_result_col_name(stmt, 5));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 6), "bincol"),     "col 6 name expected 'bincol' got '%s'",     M_sql_stmt_result_col_name(stmt, 6));
	ck_assert_msg(M_str_eq(M_sql_stmt_result_col_name(stmt, 7), "hugebincol"), "col 7 name expected 'hugebincol' got '%s'", M_sql_stmt_result_col_name(stmt, 7));

#if defined(DEBUG) && DEBUG >= 1
	M_printf("%zu cols (", M_sql_stmt_result_num_cols(stmt));
	for (i=0; i<M_sql_stmt_result_num_cols(stmt); i++) {
		size_t            size = 0;
		M_sql_data_type_t type;

		type = M_sql_stmt_result_col_type(stmt, i, &size);
		if (i != 0)
			M_printf(", ");
		M_printf("\"%s\" %s", M_sql_stmt_result_col_name(stmt, i), coltype2str(type));
		if (type == M_SQL_DATA_TYPE_TEXT || type == M_SQL_DATA_TYPE_BINARY) {
			M_printf("(%zu)", size);
		}
	}
	M_printf(")\n");
#endif

	/* Output a nice CSV report of the records */
	err = M_sql_report_process(report, stmt, NULL, &out, &out_len, error, sizeof(error));
	M_sql_report_destroy(report);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_report_process() failed: %s: %s", M_sql_error_string(err), error);
#if defined(DEBUG) && DEBUG >= 2
	M_printf("Query Output      :\n%s", out);
#endif
	ck_assert_msg(!M_str_isempty(out), "M_sql_report_process() failed to return result data");
	csv = M_csv_parse_inplace(out, out_len, ',', '"', M_CSV_FLAG_NONE);
	ck_assert_msg(csv != NULL, "Failed to parse CSV data");
	ck_assert_msg(M_csv_get_numrows(csv) == M_sql_stmt_result_total_rows(stmt), "mismatch between csv rows and sql rows: %zu vs %zu", M_csv_get_numrows(csv), M_sql_stmt_result_total_rows(stmt));
	ck_assert_msg(M_csv_get_numcols(csv) == M_sql_stmt_result_num_cols(stmt), "mismatch between csv cols and sql cols: %zu vs %zu", M_csv_get_numcols(csv), M_sql_stmt_result_num_cols(stmt));

	M_csv_destroy(csv); /* Auto-free's out */

	/* Validate Row Count */
	ck_assert_msg(M_sql_stmt_result_total_rows(stmt) == INSERT_ROWS + 2 + 2 + 1 + 1, "M_sql_stmt_result_total_rows() did not return the expected number of rows");

	M_sql_stmt_destroy(stmt);

	/* Validate the huge binary data row */
	stmt = M_sql_stmt_create();
	err  = M_sql_stmt_prepare(stmt, "SELECT * FROM \"foo\" WHERE \"key\" = ?");
	M_sql_stmt_bind_int64(stmt, hugedataid);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_prepare(SELECT hugedata) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	err  = M_sql_stmt_execute(pool, stmt);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_execute(SELECT hugedata) failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
	ck_assert_msg(M_sql_stmt_result_total_rows(stmt) == 1, "M_sql_stmt_result_total_rows(SELECT hugedata) did not return the expected number of rows");

	err = M_sql_stmt_result_binary_byname(stmt, 0, "hugebincol", &outbincol, &outbincol_size);
	ck_assert_msg(err == M_SQL_ERROR_SUCCESS, "M_sql_stmt_result_binary_byname(hugebincol) failed");

	ck_assert_msg(outbincol_size == HUGEDATA_SIZE, "Expected huge binary column to be %zu bytes, was %zu bytes", (size_t)HUGEDATA_SIZE, outbincol_size);
	for (i=0; i<outbincol_size; i++) {
		ck_assert_msg(outbincol[i] == 0x0D, "Binary data index %zu (0x%02X) does not match expected value of 0x0D", i, outbincol[i]);
	}
	M_sql_stmt_destroy(stmt);

	/* Close connections */
	ck_assert_msg(M_sql_connpool_destroy(pool) == M_SQL_ERROR_SUCCESS, "M_sql_connpool_destroy() failed");

	/* Free any internally allocated memory for mstdlib */
	M_library_cleanup();
/* XXX: Check boolean - including binding integer
 *      Check other data types too, like binary */
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *sql_suite(void)
{
	Suite *suite;
	TCase *tc;

	suite = suite_create("sql");

	tc = tcase_create("sql");
	tcase_set_timeout(tc, 30);
	tcase_add_test(tc, check_sql);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int      nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(sql_suite());
	srunner_set_log(sr, "check_sql.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
