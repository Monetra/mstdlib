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

#include "m_config.h"
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include <mstdlib/mstdlib_formats.h>
#include <mstdlib/mstdlib_io.h>
#include "base/m_defs_int.h"
#include "m_sql_int.h"

static M_bool M_sql_chr_nospaceneeded(char c)
{
	if (M_chr_isspace(c))
		return M_TRUE;

	switch (c) {
		/* SQL92 BNF doesn't require spaces for any of these */
		case '(':
		case ')':
		case ',':
		case '=':
		case '+':
		case '-':
		case '%':
		case '&':
		case '|':
		case '^':
		case '/':
		case '>':
		case '<':
		case '!':
		/* case '*':  -- our own readability improvement, because of SELECT * FROM would otherwise become SELECT*FROM which is valid but not very readable */
			return M_TRUE;
	}

	return M_FALSE;
}

char *M_sql_driver_queryformat(const char *query, M_uint32 flags, size_t num_params, size_t num_rows, char *error, size_t error_size)
{
	char  *minimized_query;
	size_t len;
	size_t i;
	size_t offset  = 0;
	M_bool success = M_FALSE;

	/* Trim leading and trailing whitespace */
	minimized_query = M_strdup_trim(query);
	len             = M_str_len(minimized_query);

	if (len == 0) {
		M_snprintf(error, error_size, "empty query");
		goto fail;
	}

	/* Strip extra whitespace in the query itself, if the character is whitespace,
	 * check the next character and if its not alpha-numeric, it should be safe
	 * to strip the whitespace.  This makes it easier to rewrite the query if necessary */
	for (i=0; i<len; i++) {
		if (M_chr_isspace(minimized_query[i]) && (
		      M_sql_chr_nospaceneeded(minimized_query[i+1]) ||
		      (i-offset > 0 && M_sql_chr_nospaceneeded(minimized_query[i-offset-1]))
		    )
		   ) {
			offset++;
			continue;
		}

		if (M_chr_isspace(minimized_query[i])) {
			/* Make sure a space is only a space not something like a new line */
			minimized_query[i-offset] = ' ';
		} else {
			minimized_query[i-offset] = minimized_query[i];
		}
	}
	len -= offset;
	minimized_query[len] = '\0';

	/* Rewrite the query as a comma delimited values list if there are multiple rows.
	 * E.g.    INSERT INTO foo VALUES(?,?,?) with 3 rows ->
	 *         INSERT INTO foo VALUES(?,?,?),(?,?,?),(?,?,?)
	 */
	if (flags & M_SQL_DRIVER_QUERYFORMAT_MULITVALUEINSERT_CD && num_rows > 1) {
		const char *values_start = M_str_casestr(minimized_query, "VALUES(");
		const char *values_end   = NULL;
		M_buf_t    *buf          = NULL;
		size_t      paren_cnt;

		if (values_start == NULL) {
			M_snprintf(error, error_size, "no VALUES() string found in statement");
			goto fail;
		}

		values_start += M_str_len("VALUES");
		values_end    = values_start + 1; /* Start after opening ( */
		paren_cnt     = 1;

		/* Find valid closing paren, though we need to track parens in case
		 * there are internal parens */
		for ( ; paren_cnt && *values_end != '\0'; values_end++) {
			if (*values_end == '(')
				paren_cnt++;
			if (*values_end == ')')
				paren_cnt--;
		}

		if (paren_cnt != 0) {
			M_snprintf(error, error_size, "no end to VALUES() found in statement");
			goto fail;
		}

		buf = M_buf_create();
		M_buf_add_bytes(buf, minimized_query, (size_t)(values_start - minimized_query));
		for (i=0; i<num_rows; i++) {
			if (i != 0)
				M_buf_add_byte(buf, ',');
			M_buf_add_bytes(buf, values_start, (size_t)(values_end - values_start));
		}
		M_buf_add_str(buf, values_end);

		M_free(minimized_query);
		minimized_query = M_buf_finish_str(buf, &len);
	}

	/* Rewrite parameters from ? to $N or :N as appropriate */
	if (flags & (M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_DOLLAR|M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_COLON) && num_params) {
		M_buf_t *buf = M_buf_create();
		size_t   id  = 1;
		for (i=0; i<len; i++) {
			if (minimized_query[i] == '?') {
				if (flags & M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_DOLLAR) {
					M_buf_add_byte(buf, '$');
				} else { /* M_SQL_DRIVER_QUERYFORMAT_ENUMPARAM_COLON */
					M_buf_add_byte(buf, ':');
				}
				M_buf_add_uint(buf, id++);
			} else {
				M_buf_add_byte(buf, (unsigned char)minimized_query[i]);
			}
		}
		M_free(minimized_query);
		minimized_query = M_buf_finish_str(buf, &len);
	}

	/* Add terminator if necessary */
	if (flags & M_SQL_DRIVER_QUERYFORMAT_TERMINATOR) {
		char *temp      = NULL;
		len             = M_asprintf(&temp, "%s;", minimized_query);
		M_free(minimized_query);
		minimized_query = temp;
	}

	success = M_TRUE;

fail:
	if (!success) {
		M_free(minimized_query);
		return NULL;
	}

	return minimized_query;
}


M_bool M_sql_driver_validate_connstr(const M_hash_dict_t *conndict, const M_sql_connstr_params_t *params, char *error, size_t error_size)
{
	size_t              i;
	M_hash_dict_enum_t *hashenum = NULL;
	M_bool              rv       = M_FALSE;
	const char         *key      = NULL;
	const char         *val      = NULL;

	/* Validate parameters that are passed */
	M_hash_dict_enumerate(conndict, &hashenum);
	while (M_hash_dict_enumerate_next(conndict, hashenum, &key, &val)) {
		for (i=0; params[i].name != NULL; i++) {
			if (M_str_caseeq(key, params[i].name))
				break;
		}
		if (params[i].name == NULL) {
			M_snprintf(error, error_size, "unrecognized param %s", key);
			goto done;
		}

		switch (params[i].type) {
			case M_SQL_CONNSTR_TYPE_BOOL:
				if (!M_str_caseeq(val, "1") && !M_str_caseeq(val, "0") &&
				    !M_str_caseeq(val, "y") && !M_str_caseeq(val, "n") &&
				    !M_str_caseeq(val, "yes") && !M_str_caseeq(val, "no") &&
				    !M_str_caseeq(val, "true") && !M_str_caseeq(val, "false") &&
				    !M_str_caseeq(val, "on") && !M_str_caseeq(val, "off")) {
					M_snprintf(error, error_size, "param %s not boolean", key);
					goto done;
				}
				break;
			case M_SQL_CONNSTR_TYPE_NUM:
				if (!M_str_isnum(val)) {
					M_snprintf(error, error_size, "param %s not numeric", key);
					goto done;
				}
				break;
			case M_SQL_CONNSTR_TYPE_ALPHA:
				if (!M_str_isalpha(val)) {
					M_snprintf(error, error_size, "param %s not alpha-only", key);
					goto done;
				}
				break;
			case M_SQL_CONNSTR_TYPE_ALPHANUM:
				if (!M_str_isalnum(val)) {
					M_snprintf(error, error_size, "param %s not alpha-numeric", key);
					goto done;
				}
				break;
			case M_SQL_CONNSTR_TYPE_ANY:
				break;
		}

		if (params[i].type != M_SQL_CONNSTR_TYPE_BOOL) {
			size_t len = M_str_len(val);
			if (len < params[i].min_len || len > params[i].max_len) {
				M_snprintf(error, error_size, "param %s not between %zu and %zu in length", key, params[i].min_len, params[i].max_len);
			}
		}
	}

	/* Make sure we have all required params */
	for (i=0; params[i].name != NULL; i++) {
		if (!params[i].required)
			continue;
		if (!M_hash_dict_get(conndict, params[i].name, NULL)) {
			M_snprintf(error, error_size, "missing param %s", params[i].name);
			goto done;
		}
	}

	rv = M_TRUE;

done:
	M_hash_dict_enumerate_free(hashenum);
	return rv;
}


M_sql_hostport_t *M_sql_driver_parse_hostport(const char *hostport, M_uint16 default_port, size_t *out_len, char *error, size_t error_size)
{
	M_parser_t       *parser      = M_parser_create_const((const unsigned char *)hostport, M_str_len(hostport), M_PARSER_FLAG_NONE);
	size_t            num_entries = 0;
	M_parser_t      **entries     = M_parser_split(parser, ',', 0, M_PARSER_SPLIT_FLAG_NONE, &num_entries);
	M_bool            rv          = M_FALSE;
	size_t            i;
	M_sql_hostport_t *out         = NULL;
	size_t            num_hosts   = 0;

	*out_len = 0;

	if (num_entries == 0)
		goto done;

	out = M_malloc_zero(sizeof(*out) * num_entries);

	for (i=0; i<num_entries; i++) {
		unsigned char ip_bin[16];
		size_t        ip_bin_len = 0;

		/* Trim */
		M_parser_truncate_whitespace(entries[i], M_PARSER_WHITESPACE_NONE);
		M_parser_consume_whitespace(entries[i], M_PARSER_WHITESPACE_NONE);
		/* Skip */
		if (M_parser_len(entries[i]) == 0)
			continue;

		out[num_hosts].port = default_port;
		if (M_parser_read_str_until(entries[i], out[num_hosts].host, sizeof(out[num_hosts].host), ":", M_FALSE) != 0) {
			M_uint64 i64;

			/* Had a colon, process */
			M_parser_consume(entries[i], 1); /* Eat Colon */
			/* Trim */
			M_parser_consume_whitespace(entries[i], M_PARSER_WHITESPACE_NONE);
			if (!M_parser_read_uint(entries[i], M_PARSER_INTEGER_ASCII, M_parser_len(entries[i]), 10, &i64) || i64 >= (1 << 16) || i64 == 0) {
				M_snprintf(error, error_size, "Invalid port configuration for host %zu", i+1);
				goto done;
			}
			out[num_hosts].port = (M_uint16)(i64 & 0xFFFF);
		} else {
			M_parser_read_str_max(entries[i], sizeof(out[num_hosts].host)-1, out[num_hosts].host, sizeof(out[num_hosts].host));
		}

		M_str_trim(out[num_hosts].host);
		if (!M_verify_domain(out[num_hosts].host) &&
		    !M_io_net_ipaddr_to_bin(ip_bin, sizeof(ip_bin), out[num_hosts].host, &ip_bin_len)) {
			M_snprintf(error, error_size, "Host name validation failed for entry %zu '%s'", i+1, out[num_hosts].host);
			goto done;
		}
		num_hosts++;
	}

	rv = M_TRUE;

done:
	M_parser_split_free(entries, num_entries);
	M_parser_destroy(parser);

	if (!rv) {
		M_free(out);
		return NULL;
	}

	*out_len = num_hosts;
	return out;
}


M_sql_driver_stmt_t *M_sql_driver_stmt_get_stmt(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return NULL;
	return stmt->dstmt;
}


const char *M_sql_driver_stmt_get_query(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return NULL;
	return stmt->query_prepared;
}


size_t M_sql_driver_stmt_get_requested_row_cnt(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return 0;

	return stmt->max_fetch_rows;
}


size_t M_sql_driver_stmt_bind_rows(M_sql_stmt_t *stmt)
{
	if (stmt == NULL)
		return 0;

	/* Possible if there were no bound parameters, but there was a statement
	 * execution */
	if (stmt->bind_row_offset > stmt->bind_row_cnt)
		return 0;

	return stmt->bind_row_cnt - stmt->bind_row_offset;
}


size_t M_sql_driver_stmt_bind_cnt(M_sql_stmt_t *stmt)
{
	if (stmt == NULL || stmt->bind_row_cnt == 0)
		return 0;
	return stmt->bind_rows[0].col_cnt;
}


M_sql_data_type_t M_sql_driver_stmt_bind_get_type(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) || idx >= M_sql_driver_stmt_bind_cnt(stmt))
		return 0;

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].type;
}


M_sql_data_type_t M_sql_driver_stmt_bind_get_col_type(M_sql_stmt_t *stmt, size_t idx)
{
	size_t row;
	size_t num_rows        = M_sql_driver_stmt_bind_rows(stmt);
	M_sql_data_type_t type = M_SQL_DATA_TYPE_UNKNOWN;

	/* Iterate across incase someone decided to use something weird for null binding */
	for (row = 0; row < num_rows; row++) {
		type = M_sql_driver_stmt_bind_get_type(stmt, row, idx);

		if (!M_sql_driver_stmt_bind_isnull(stmt, row, idx))
			break;
	}

	return type;
}


size_t M_sql_driver_stmt_bind_get_max_col_size(M_sql_stmt_t *stmt, size_t idx)
{
	size_t            row;
	size_t            num_rows = M_sql_driver_stmt_bind_rows(stmt);
	M_sql_data_type_t type     = M_sql_driver_stmt_bind_get_col_type(stmt, idx);
	size_t            max_size = 0;

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			return sizeof(M_bool);

		case M_SQL_DATA_TYPE_INT16:
			return sizeof(M_int16);

		case M_SQL_DATA_TYPE_INT32:
			return sizeof(M_int32);

		case M_SQL_DATA_TYPE_INT64:
			return sizeof(M_int64);

		case M_SQL_DATA_TYPE_TEXT:
		case M_SQL_DATA_TYPE_BINARY:
			for (row = 0; row < num_rows; row++) {
				size_t len;
				if (type == M_SQL_DATA_TYPE_TEXT) {
					len = M_sql_driver_stmt_bind_get_text_len(stmt, row, idx);
				} else {
					len = M_sql_driver_stmt_bind_get_binary_len(stmt, row, idx);
				}
				if (len > max_size)
					max_size = len;
			}
			return max_size;

		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}

	return 0;
}


size_t M_sql_driver_stmt_bind_get_curr_col_size(M_sql_stmt_t *stmt, size_t row, size_t col)
{
	M_sql_data_type_t type = M_sql_driver_stmt_bind_get_col_type(stmt, col);

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			return sizeof(M_bool);

		case M_SQL_DATA_TYPE_INT16:
			return sizeof(M_int16);

		case M_SQL_DATA_TYPE_INT32:
			return sizeof(M_int32);

		case M_SQL_DATA_TYPE_INT64:
			return sizeof(M_int64);

		case M_SQL_DATA_TYPE_TEXT:
			return M_sql_driver_stmt_bind_get_text_len(stmt, row, col);

		case M_SQL_DATA_TYPE_BINARY:
			return M_sql_driver_stmt_bind_get_binary_len(stmt, row, col);

		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}

	return 0;
}


M_bool M_sql_driver_stmt_bind_isnull(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt)) {
		return M_TRUE;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */
	return stmt->bind_rows[row].cols[idx].isnull;
}


M_bool *M_sql_driver_stmt_bind_get_bool_addr(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_BOOL) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */
	return &stmt->bind_rows[row].cols[idx].v.b;
}


M_int16 *M_sql_driver_stmt_bind_get_int16_addr(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT16) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */
	return &stmt->bind_rows[row].cols[idx].v.i16;
}


M_int32 *M_sql_driver_stmt_bind_get_int32_addr(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT32) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */
	return &stmt->bind_rows[row].cols[idx].v.i32;
}


M_int64 *M_sql_driver_stmt_bind_get_int64_addr(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT64) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */
	return &stmt->bind_rows[row].cols[idx].v.i64;
}


M_bool M_sql_driver_stmt_bind_get_bool(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_BOOL) {
		return M_FALSE;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.b;
}

M_int16 M_sql_driver_stmt_bind_get_int16(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT16) {
		return 0;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.i16;
}

M_int32 M_sql_driver_stmt_bind_get_int32(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT32) {
		return 0;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.i32;
}

M_int64 M_sql_driver_stmt_bind_get_int64(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_INT64) {
		return 0;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.i64;
}

const char *M_sql_driver_stmt_bind_get_text(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_TEXT) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.text.data;
}

size_t M_sql_driver_stmt_bind_get_text_len(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_TEXT) {
		return 0;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	if (stmt->bind_rows[row].cols[idx].isnull)
		return 0;

	return stmt->bind_rows[row].cols[idx].v.text.max_len;
}

const M_uint8 *M_sql_driver_stmt_bind_get_binary(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_BINARY) {
		return NULL;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	return stmt->bind_rows[row].cols[idx].v.binary.data;
}

size_t M_sql_driver_stmt_bind_get_binary_len(M_sql_stmt_t *stmt, size_t row, size_t idx)
{
	if (stmt == NULL || row >= M_sql_driver_stmt_bind_rows(stmt) ||
	    idx >= M_sql_driver_stmt_bind_cnt(stmt) ||
	    M_sql_driver_stmt_bind_get_type(stmt, row, idx) != M_SQL_DATA_TYPE_BINARY) {
		return 0;
	}

	row += stmt->bind_row_offset; /* Multi-row partial execution */

	if (stmt->bind_rows[row].cols[idx].isnull)
		return 0;

	return stmt->bind_rows[row].cols[idx].v.binary.len;
}



void M_sql_driver_append_updlock(M_sql_driver_updlock_caps_t caps, M_buf_t *query, M_sql_query_updlock_type_t type, const char *table_name)
{
	switch (caps) {
		case M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATE:
			if (type == M_SQL_QUERY_UPDLOCK_QUERYEND) {
				M_buf_add_str(query, " FOR UPDATE");
			}
			return;
		case M_SQL_DRIVER_UPDLOCK_CAP_MSSQL:
			if (type == M_SQL_QUERY_UPDLOCK_TABLE) {
				M_buf_add_str(query, " WITH (ROWLOCK, XLOCK, HOLDLOCK)");
			}
			return;
		case M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATEOF:
			if (type == M_SQL_QUERY_UPDLOCK_QUERYEND) {
				M_buf_add_str(query, " FOR UPDATE");
				if (!M_str_isempty(table_name)) {
					M_buf_add_str(query, " OF ");
					M_buf_add_str(query, table_name);
				}
			}
			return;
		default: /* NONE */
			break;
	}
}


M_bool M_sql_driver_append_bitop(M_sql_driver_bitop_caps_t caps, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	if (query == NULL || M_str_isempty(exp1) || M_str_isempty(exp2))
		return M_FALSE;

	switch (caps) {
		case M_SQL_DRIVER_BITOP_CAP_OP:
			M_buf_add_str(query, "(");
			M_buf_add_str(query, exp1);
			M_buf_add_str(query, (op == M_SQL_BITOP_AND)?" & ":" | ");
			M_buf_add_str(query, exp2);
			M_buf_add_str(query, ")");
			return M_TRUE;
		case M_SQL_DRIVER_BITOP_CAP_OP_CAST_BIGINT:
			M_buf_add_str(query, "(");
			M_buf_add_str(query, exp1);
			M_buf_add_str(query, (op == M_SQL_BITOP_AND)?" & ":" | ");
			M_buf_add_str(query, "CAST(");
			M_buf_add_str(query, exp2);
			M_buf_add_str(query, " AS BIGINT) ");
			M_buf_add_str(query, ")");
			return M_TRUE;
		case M_SQL_DRIVER_BITOP_CAP_FUNC:
			M_buf_add_str(query, (op == M_SQL_BITOP_AND)?" BITAND(":" BITOR(");
			M_buf_add_str(query, exp1);
			M_buf_add_str(query, ", ");
			M_buf_add_str(query, exp2);
			M_buf_add_str(query, ")");
			return M_TRUE;
		default:
			break;
	}
	return M_FALSE;
}


static const struct {
	const char       *name;
	M_sql_isolation_t type;
} M_sql_isolation_lookup[] = {
	{ "SERIALIZABLE",     M_SQL_ISOLATION_SERIALIZABLE    },
	{ "SNAPSHOT",         M_SQL_ISOLATION_SNAPSHOT        },
	{ "REPEATABLE READ",  M_SQL_ISOLATION_REPEATABLEREAD  },
	{ "READ COMMITTED",   M_SQL_ISOLATION_READCOMMITTED   },
	{ "READ UNCOMMITTED", M_SQL_ISOLATION_READUNCOMMITTED },
	{ NULL, 0 }
};


M_sql_isolation_t M_sql_driver_str2isolation(const char *str)
{
	size_t i;

	for (i=0; M_sql_isolation_lookup[i].name != NULL; i++) {
		if (M_str_caseeq(str, M_sql_isolation_lookup[i].name))
			return M_sql_isolation_lookup[i].type;
	}

	return M_SQL_ISOLATION_UNKNOWN;
}


const char *M_sql_driver_isolation2str(M_sql_isolation_t type)
{
	size_t i;

	for (i=0; M_sql_isolation_lookup[i].name != NULL; i++) {
		if (type == M_sql_isolation_lookup[i].type)
			return M_sql_isolation_lookup[i].name;
	}

	return NULL;
}
