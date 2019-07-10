/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
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
#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"
#include "m_parser_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

enum M_PARSER_MARKED_TYPE {
	M_PARSER_MARKED_USER = 1 << 0,
	M_PARSER_MARKED_INT  = 1 << 1
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Return the minimum marked position start, or current buffer position if no mark */
static const unsigned char *M_parser_marked_buffer_start(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type, size_t *len_marked)
{
	ssize_t              min_mark = -1;
	const unsigned char *ptr;

	if (parser == NULL || len_marked == NULL)
		return NULL;

	if (type & M_PARSER_MARKED_USER && parser->mark_user != -1)
		min_mark = parser->mark_user;

	if (type & M_PARSER_MARKED_INT && parser->mark_int != -1 && (min_mark == -1 || parser->mark_int < min_mark))
		min_mark = parser->mark_int;

	if (min_mark == -1) {
		*len_marked = 0;
		return parser->data;
	}

	if (parser->data_dyn) {
		ptr = parser->data_dyn + min_mark;
	} else {
		ptr = parser->data_const + min_mark;
	}

	*len_marked = (size_t)(parser->data - ptr);
	return ptr;
}


static void M_parser_mark_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type)
{
	size_t               pos;
	const unsigned char *ptr;

	if (parser == NULL)
		return;

	if (parser->data_const) {
		ptr = parser->data_const;
	} else {
		ptr = parser->data_dyn;
	}

	if (ptr == NULL) {
		pos = 0;
	} else {
		pos = (size_t)(parser->data - ptr);
	}

	if (type & M_PARSER_MARKED_USER)
		parser->mark_user = (ssize_t)pos;
	if (type & M_PARSER_MARKED_INT)
		parser->mark_int  = (ssize_t)pos;
}


static void M_parser_mark_clear_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type)
{
	unsigned char *ptr        = NULL;
	size_t         len_marked = 0;

	if (parser == NULL)
		return;

	/* Get the length of all marked data and pointer if a dynamic buffer so we can
	 * potentially secure it later */
	if (parser->data_dyn) {
		const unsigned char *temp = M_parser_marked_buffer_start(parser, M_PARSER_MARKED_USER|M_PARSER_MARKED_INT, &len_marked);
		ptr = M_CAST_OFF_CONST(unsigned char *, temp);
	}

	if (type & M_PARSER_MARKED_USER)
		parser->mark_user = -1;
	if (type & M_PARSER_MARKED_INT)
		parser->mark_int  = -1;

	/* Secure data since marks have been cleared */
	if (ptr && len_marked && parser->mark_int == -1 && parser->mark_user == -1) {
		M_mem_set(ptr, 0xFF, len_marked);
	}
}


static size_t M_parser_read_bytes_mark_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type, unsigned char *buf, size_t buf_len)
{
	const unsigned char *ptr;
	size_t               len = 0;

	if (parser == NULL || buf == NULL || buf_len == 0)
		return 0;

	ptr = M_parser_marked_buffer_start(parser, type, &len);

	if (ptr == NULL || len == 0)
		return 0;

	if (len > buf_len)
		return 0;

	M_mem_copy(buf, ptr, len);

	/* Clear mark! */
	M_parser_mark_clear_int(parser, type);
	return len;
}

static size_t M_parser_read_buf_mark_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type, M_buf_t *buf)
{
	const unsigned char *ptr;
	size_t               len = 0;

	if (parser == NULL || buf == NULL)
		return 0;

	ptr = M_parser_marked_buffer_start(parser, type, &len);

	if (ptr == NULL || len == 0)
		return 0;

	M_buf_add_bytes(buf, ptr, len);

	/* Clear mark! */
	M_parser_mark_clear_int(parser, type);
	return len;
}


static size_t M_parser_consume_charset_int(M_parser_t *parser, const unsigned char *charset, size_t charset_len, M_bool inclusion)
{
	size_t len;
	size_t i;

	if (parser == NULL || charset == NULL || charset_len == 0)
		return 0;

	for (len=0; len < parser->data_len; len++) {
		for (i=0; i<charset_len; i++) {
			if (parser->data[len] == charset[i])
				break;
		}
		if (inclusion && i == charset_len)
			break;
		if (!inclusion && i != charset_len)
			break;
	}

	M_parser_consume(parser, len);
	return len;
}

static size_t M_parser_read_bytes_charset_int(M_parser_t *parser, const unsigned char *charset, size_t charset_len, unsigned char *buf, size_t buf_len, M_bool inclusion)
{
	size_t cnt;

	if (parser == NULL || buf == NULL || buf_len == 0 || charset == NULL || charset_len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	if (inclusion) {
		cnt = M_parser_consume_charset_int(parser, charset, charset_len, M_TRUE);
	} else {
		cnt = M_parser_consume_charset_int(parser, charset, charset_len, M_FALSE);
	}
	if (cnt == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, buf, buf_len);
}

static size_t M_parser_read_str_charset_int(M_parser_t *parser, const char *charset, char *buf, size_t buf_len, M_bool inclusion)
{
	size_t len;

	if (buf == NULL || buf_len == 0)
		return 0;

	if (inclusion) {
		len = M_parser_read_bytes_charset(parser, (const unsigned char *)charset, M_str_len(charset), (unsigned char *)buf, buf_len-1);
	} else {
		len = M_parser_read_bytes_not_charset(parser, (const unsigned char *)charset, M_str_len(charset), (unsigned char *)buf, buf_len-1);
	}

	if (len == 0)
		return 0;

	/* NULL terminate */
	buf[len]=0;
	return len;
}

static char *M_parser_read_strdup_charset_int(M_parser_t *parser, const char *charset, M_bool inclusion)
{
	size_t len;
	char  *out = NULL;

	if (parser == NULL || charset == NULL || M_str_len(charset) == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	if (inclusion) {
		len = M_parser_consume_str_charset(parser, charset);
	} else {
		len = M_parser_consume_str_not_charset(parser, charset);
	}
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	out = M_malloc(len+1);

	/* Output the data from the marked position, this will also clear the mark */
	if (M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, (unsigned char *)out, len) != len) {
		M_free(out);
		return NULL;
	}

	out[len] = 0;
	return out;
}

static size_t M_parser_read_buf_charset_int(M_parser_t *parser, M_buf_t *buf, const unsigned char *charset, size_t charset_len, M_bool inclusion)
{
	if (parser == NULL || buf == NULL || charset == NULL || charset_len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	if (M_parser_consume_charset_int(parser, charset, charset_len, inclusion) == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_buf_mark_int(parser, M_PARSER_MARKED_INT, buf);
}

static M_parser_t *M_parser_read_parser_mark_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type)
{
	M_parser_t          *p;
	const unsigned char *ptr;
	size_t               ptrlen;

	if (parser == NULL)
		return NULL;

	ptr = M_parser_marked_buffer_start(parser, type, &ptrlen);
	if (ptr == NULL || ptrlen == 0) {
		M_parser_mark_clear_int(parser, type);
		return NULL;
	}

	p = M_parser_create(parser->flags);
	M_parser_append(p, ptr, ptrlen);

	M_parser_mark_clear_int(parser, type);
	return p;
}


static M_bool M_parser_read_int_binary(M_parser_t *parser, size_t len, M_bool is_bigendian, M_int64 *integer)
{
	size_t i;

	if (parser == NULL || integer == NULL || len == 0 || len > 8 || len > parser->data_len)
		return M_FALSE;

	*integer = 0;
	for (i=0; i<len; i++) {
		*integer <<= 8;
		*integer  |= parser->data[is_bigendian?i:len-(i+1)];
	}

	M_parser_consume(parser, len);
	return M_TRUE;
}


static M_bool M_parser_read_int_ascii(M_parser_t *parser, size_t len, unsigned char base, M_int64 *integer)
{
	M_str_int_retval_t  rv;
	const char         *end = NULL;

	if (parser == NULL || integer == NULL || len > parser->data_len || parser->data_len == 0)
		return M_FALSE;

	if (len == 0)
		len = parser->data_len;

	rv = M_str_to_int64_ex((const char *)parser->data, len, base, integer, &end);
	if (rv != M_STR_INT_SUCCESS)
		return M_FALSE;

	M_parser_consume(parser, (size_t)(((const unsigned char *)end) - parser->data));
	return M_TRUE;
}


static M_bool M_parser_read_uint_ascii(M_parser_t *parser, size_t len, unsigned char base, M_uint64 *integer)
{
	M_str_int_retval_t  rv;
	const char         *end = NULL;

	if (parser == NULL || integer == NULL || len > parser->data_len || parser->data_len == 0)
		return M_FALSE;

	if (len == 0)
		len = parser->data_len;

	rv = M_str_to_uint64_ex((const char *)parser->data, len, base, integer, &end);
	if (rv != M_STR_INT_SUCCESS)
		return M_FALSE;

	M_parser_consume(parser, (size_t)(((const unsigned char *)end) - parser->data));
	return M_TRUE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_parser_init(M_parser_t *parser, const unsigned char *buf, size_t len, M_uint32 flags)
{
	M_mem_set(parser, 0, sizeof(*parser));

	if (buf != NULL) {
		parser->data_const = buf;
		parser->data_len   = len;
		parser->data       = parser->data_const;
	}

	parser->flags      = flags;
	parser->mark_user  = -1;
	parser->mark_int   = -1;
}

M_parser_t *M_parser_create_const(const unsigned char *buf, size_t len, M_uint32 flags)
{
	M_parser_t *ret;

	if (buf == NULL)
		return NULL;

	ret = M_malloc_zero(sizeof(*ret));
	M_parser_init(ret, buf, len, flags);
	return ret;
}


M_parser_t *M_parser_create(M_uint32 flags)
{
	M_parser_t *ret;

	ret = M_malloc_zero(sizeof(*ret));
	M_parser_init(ret, NULL, 0, flags);
	return ret;
}


void M_parser_destroy(M_parser_t *parser)
{
	if (parser == NULL)
		return;
	M_free(parser->data_dyn);
	M_free(parser);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_parser_ensure_space(M_parser_t *parser, size_t len)
{
	size_t len_marked = 0; /*! Length of marked data until start of consumed data pointer */
	size_t keep_len   = 0; /*! Length of data to keep (non-consumed data and marked data) */

	/* Lets chop off anything consumed on the left, only if buffer was populated at all */
	if (parser->data_dyn != NULL) {
		const unsigned char *mark_start = M_parser_marked_buffer_start(parser, M_PARSER_MARKED_USER|M_PARSER_MARKED_INT, &len_marked);

		keep_len   = parser->data_len + len_marked;

		/* Make sure we don't move to same position where we are, and we are actually moving
		 * data at all */
		if (parser->data_dyn != mark_start && keep_len != 0) {
			/* Update mark offsets.  Remember, they can be different, so just
			 * do math instead of setting them to 0 */
			if (parser->mark_int != -1)
				parser->mark_int -= (mark_start - parser->data_dyn);
			if (parser->mark_user != -1)
				parser->mark_user -= (mark_start - parser->data_dyn);

			/* Move the data into position */
			M_mem_move(parser->data_dyn, mark_start, keep_len);
		}

		parser->data = parser->data_dyn + len_marked;
	}

	/* Append of 0 just chops */
	if (len == 0)
		return;

	/* Lets see if there is enough room in the current buffer, if not,
	 * expand to the next closest power of 2 */
	if (parser->data_dyn == NULL || len > parser->data_dyn_size - keep_len) {
		parser->data_dyn_size = M_size_t_round_up_to_power_of_two(keep_len + len);
		parser->data_dyn      = M_realloc(parser->data_dyn, parser->data_dyn_size);
		parser->data          = parser->data_dyn + len_marked;
	}
}


M_bool M_parser_append(M_parser_t *parser, const unsigned char *data, size_t len)
{
	/* Either bad object, or is a constant buffer */
	if (parser == NULL || parser->data_const != NULL)
		return M_FALSE;

	if (data == NULL) {
		if (len != 0)
			return M_FALSE;
		return M_TRUE; /* Ok, we let them append nothing */
	}

	M_parser_ensure_space(parser, len);

	/* Copy new data on to the end of the buffer */
	M_mem_copy(M_CAST_OFF_CONST(unsigned char *, parser->data) + parser->data_len, data, len);
	parser->data_len += len;

	return M_TRUE;
}


unsigned char *M_parser_direct_write_start(M_parser_t *parser, size_t *len)
{
	if (parser == NULL || len == NULL || *len == 0 || parser->data_const != NULL)
		return NULL;

	M_parser_ensure_space(parser, *len);
	*len = parser->data_dyn_size - (size_t)(parser->data - parser->data_dyn) - parser->data_len;

	return M_CAST_OFF_CONST(unsigned char *, parser->data) + parser->data_len;
}


void M_parser_direct_write_end(M_parser_t *parser, size_t len)
{
	if (parser == NULL || parser->data_const != NULL)
		return;
	parser->data_len += len;
}



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_parser_len(M_parser_t *parser)
{
	if (parser == NULL)
		return 0;
	return parser->data_len;
}


size_t M_parser_current_offset(M_parser_t *parser)
{
	if (parser == NULL)
		return 0;
	return parser->consumed;
}


size_t M_parser_current_line(M_parser_t *parser)
{
	if (parser == NULL || !(parser->flags & M_PARSER_FLAG_TRACKLINES))
		return 0;
	return parser->curr_line+1;
}


size_t M_parser_current_column(M_parser_t *parser)
{
	if (parser == NULL || !(parser->flags & M_PARSER_FLAG_TRACKLINES))
		return 0;
	return parser->curr_col+1;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_compare(M_parser_t *parser, const unsigned char *data, size_t data_len)
{
	if (parser == NULL || data == NULL || data_len == 0 || data_len > parser->data_len)
		return M_FALSE;

	return M_mem_eq(parser->data, data, data_len);
}


M_bool M_parser_compare_str(M_parser_t *parser, const char *str, size_t max_len, M_bool casecmp)
{
	size_t str_len;
	size_t cmp_len;

	if (parser == NULL || str == NULL)
		return M_FALSE;

	str_len = M_str_len(str);
	if (max_len != 0 && max_len < str_len)
		str_len = max_len;

	if (str_len == 0 || str_len > parser->data_len)
		return M_FALSE;

	if (max_len > str_len)
		max_len = str_len;

	if (max_len == 0) {
		cmp_len = parser->data_len;
	} else {
		cmp_len = max_len;
	}

	if (casecmp)
		return M_str_caseeq_max((const char *)parser->data, str, cmp_len);

	return M_str_eq_max((const char *)parser->data, str, cmp_len);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_parser_mark(M_parser_t *parser)
{
	M_parser_mark_int(parser, M_PARSER_MARKED_USER);
}


void M_parser_mark_clear(M_parser_t *parser)
{
	M_parser_mark_clear_int(parser, M_PARSER_MARKED_USER);
}


size_t M_parser_mark_len(M_parser_t *parser)
{
	size_t len_marked = 0;
	M_parser_marked_buffer_start(parser, M_PARSER_MARKED_USER, &len_marked);
	return len_marked;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_parser_rewind_int(M_parser_t *parser, size_t len)
{
	size_t               len_available;
	const unsigned char *start;
	
	if (parser == NULL || len == 0)
		return 0;

	if (parser->data_const) {
		len_available = (size_t)(parser->data - parser->data_const);
		start = parser->data_const;
	} else {
		len_available = (size_t)(parser->data - parser->data_dyn);
		start = parser->data_dyn;
	}

	if (len > len_available)
		return M_FALSE;

	/* Move pointer backwards */
	parser->data      -= len;
	parser->data_len  += len;
	parser->consumed  -= len;

	/* Handle invalidating marks */
	if (parser->mark_int >= 0 && start + parser->mark_int > parser->data) {
		parser->mark_int = -1;
	}
	if (parser->mark_user >= 0 && start + parser->mark_user > parser->data) {
		parser->mark_user = -1;
	}

	if (parser->flags & M_PARSER_FLAG_TRACKLINES) {
		size_t               i;
		const unsigned char *ptr;
		
		/* Subtract off lines passed */
		for (i=0; i<len; i++) {
			if (parser->data[i] == '\n')
				parser->curr_line--;
		}

		/* Do best guess on column count. We have to scan backwards until the
		 * prior newline.  During append, some could have been truncated off
		 * the beginning making this inaccurate */
		parser->curr_col = 0;
		for (ptr = parser->data - 1; ptr >= start; ptr--) {
			if (*ptr == '\n')
				break;
			parser->curr_col++;
		}
	}

	return M_TRUE;
}


static size_t M_parser_mark_rewind_int(M_parser_t *parser, enum M_PARSER_MARKED_TYPE type)
{
	size_t len = 0;

	if (type & M_PARSER_MARKED_INT) {
		M_parser_marked_buffer_start(parser, M_PARSER_MARKED_INT, &len);
	} else {
		len = M_parser_mark_len(parser);
	}
	
	if (len != 0)
		M_parser_rewind_int(parser, len);
	return len;
}


size_t M_parser_mark_rewind(M_parser_t *parser)
{
	return M_parser_mark_rewind_int(parser, M_PARSER_MARKED_USER);
}


size_t M_parser_reset(M_parser_t *parser)
{
	size_t len;

	/* Not allowed for dynamic data */
	if (parser->data_dyn)
		return 0;

	len = M_parser_current_offset(parser);
	if (M_parser_rewind_int(parser, len))
		return len;
	return 0;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const unsigned char *M_parser_peek(M_parser_t *parser)
{
	if (parser == NULL)
		return NULL;

	return parser->data;
}


const unsigned char *M_parser_peek_mark(M_parser_t *parser, size_t *len)
{
	if (parser == NULL || len == NULL)
		return NULL;

	return M_parser_marked_buffer_start(parser, M_PARSER_MARKED_USER, len);
}


M_bool M_parser_peek_byte(M_parser_t *parser, unsigned char *byte)
{
	if (parser == NULL || byte == NULL || parser->data_len < 1)
		return M_FALSE;

	*byte = *parser->data;
	return M_TRUE;
}


M_bool M_parser_peek_bytes(M_parser_t *parser, size_t len, unsigned char *buf)
{
	if (parser == NULL || parser->data_len < len || buf == NULL || len == 0)
		return M_FALSE;

	M_mem_copy(buf, parser->data, len);
	return M_TRUE;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_truncate(M_parser_t *parser, size_t len)
{
	if (parser == NULL)
		return M_FALSE;

	if (parser->data_len < len)
		return M_FALSE;

	/* If dynamic data, make sure we secure it */
	if (parser->data_dyn) {
		M_mem_set(M_CAST_OFF_CONST(unsigned char *, parser->data + len), 0xFF, parser->data_len - len);
	}

	parser->data_len = len;

	return M_TRUE;
}


size_t M_parser_truncate_whitespace(M_parser_t *parser, M_uint32 flags)
{
	size_t consumed;
	size_t dlen;

	if (parser == NULL)
		return 0;

	dlen = parser->data_len;

	/* If we start on a newline, go ahead and consume that even if we're consuming
	 * whitespace to the new line */
	if (flags & M_PARSER_WHITESPACE_TO_NEWLINE && parser->data[dlen-1] == '\n')
		dlen--;

	for ( ; dlen != 0; dlen--) {
		unsigned char c = parser->data[dlen-1];

		if (flags & M_PARSER_WHITESPACE_TO_NEWLINE && c == '\n') {
			break;
		} else if (flags & M_PARSER_WHITESPACE_SPACEONLY && c != ' ') {
			break;
		} else if (!M_chr_isspace((char)c)) {
			break;
		}
	}

	consumed = parser->data_len - dlen;
	if (consumed)
		M_parser_truncate(parser, dlen);
	return consumed;
}


size_t M_parser_truncate_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat)
{
	const unsigned char *ptr;
	size_t               consumed_len;

	if (parser == NULL || pat == NULL || len == 0)
		return 0;

	ptr = M_mem_rmem(parser->data, parser->data_len, pat, len);
	if (ptr == NULL)
		return 0;

	consumed_len = (size_t)(ptr - parser->data);
	if (!eat_pat)
		consumed_len -= len;
	M_parser_truncate(parser, parser->data_len-consumed_len);
	return consumed_len;
}


size_t M_parser_truncate_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len)
{
	size_t i;
	size_t j;

	if (parser == NULL || charset == NULL || charset_len == 0)
		return 0;

	for (i=parser->data_len; i-->0; ) {
		for (j=0; j<charset_len; j++) {
			if (parser->data[i] == charset[j])
				break;
		}
		if (j == charset_len)
			break;
	}

	if (i == parser->data_len)
		return 0;
	M_parser_truncate(parser, parser->data_len - i);
	return i;
}


size_t M_parser_truncate_predicate_max(M_parser_t *parser, M_parser_predicate_func func, size_t max)
{
	size_t i;

	if (parser == NULL || func == NULL || max == 0)
		return 0;

	if (max > parser->data_len)
		max = parser->data_len;

	for (i=max; i>0; i--) {
		if (!func(parser->data[i-1])) {
			break;
		}
	}

	if (i == parser->data_len)
		return 0;
	M_parser_truncate(parser, i);
	return i;
}

size_t M_parser_truncate_predicate(M_parser_t *parser, M_parser_predicate_func func)
{
	return M_parser_truncate_predicate_max(parser, func, SIZE_MAX);
}


size_t M_parser_truncate_chr_predicate(M_parser_t *parser, M_chr_predicate_func func)
{
	return M_parser_truncate_predicate(parser, (M_parser_predicate_func)func);
}

size_t M_parser_truncate_chr_predicate_max(M_parser_t *parser, M_chr_predicate_func func, size_t max)
{
	return M_parser_truncate_predicate_max(parser, (M_parser_predicate_func)func, max);
}


size_t M_parser_truncate_str_until(M_parser_t *parser, const char *pat, M_bool eat_pat)
{
	return M_parser_truncate_until(parser, (const unsigned char *)pat, M_str_len(pat), eat_pat);
}


size_t M_parser_truncate_str_charset(M_parser_t *parser, const char *charset)
{
	return M_parser_truncate_charset(parser, (const unsigned char *)charset, M_str_len(charset));
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_consume(M_parser_t *parser, size_t len)
{
	if (parser == NULL)
		return M_FALSE;

	if (len > parser->data_len)
		return M_FALSE;

	if (parser->flags & M_PARSER_FLAG_TRACKLINES) {
		size_t i;

		/* Perform calculation for line/col */
		for (i=0; i<len; i++) {
			if (parser->data[i] == '\n') {
				parser->curr_line++;
				parser->curr_col=0;
			} else {
				parser->curr_col++;
			}
		}
	}

	/* Secure the data being consumed */
	if (parser->data_dyn && parser->mark_user == -1 && parser->mark_int == -1) {
		M_mem_set(M_CAST_OFF_CONST(unsigned char *, parser->data), 0xFF, len);
	}

	/* Update offsets */
	parser->data     += len;
	parser->data_len -= len;
	parser->consumed += len;

	return M_TRUE;
}


size_t M_parser_consume_whitespace(M_parser_t *parser, M_uint32 flags)
{
	size_t i;

	if (parser == NULL)
		return 0;

	for (i=0; i<parser->data_len; i++) {
		if (flags & M_PARSER_WHITESPACE_TO_NEWLINE && parser->data[i] == '\n') {
			i++;
			break;
		}

		if (flags & M_PARSER_WHITESPACE_SPACEONLY) {
			if (parser->data[i] != ' ')
				break;
		} else {
			if (!M_chr_isspace((char)parser->data[i]))
				break;
		}
	}

	M_parser_consume(parser, i);
	return i;
}


size_t M_parser_consume_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat)
{
	const unsigned char *ptr;
	size_t               consumed_len;

	if (parser == NULL || pat == NULL || len == 0)
		return 0;

	ptr = M_mem_mem(parser->data, parser->data_len, pat, len);
	if (ptr == NULL)
		return 0;

	/* Skip past end of pattern */
	if (eat_pat)
		ptr += len;

	consumed_len = (size_t)(ptr - parser->data);
	M_parser_consume(parser, consumed_len);

	return consumed_len;
}


size_t M_parser_consume_boundary(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat, M_bool *found)
{
	size_t consumed_len = 0;
	M_bool myfound;
	size_t i            = 0;
	size_t j            = 0;

	if (found == NULL)
		found = &myfound;
	*found = M_FALSE;

	if (parser == NULL || pat == NULL || len == 0)
		return 0;

	do {
		M_bool eq = (parser->data[i] == pat[j]);
		i++;
		if (eq) {
			j++;
			if (j >= len) {
				*found = M_TRUE;
			}
		} else {
			consumed_len = i;
			j            = 0;
		}
	} while (i < parser->data_len && !(*found));

	/* Skip past end of boundry if it was fully found. */
	if (*found && eat_pat)
		consumed_len += len;

	M_parser_consume(parser, consumed_len);
	return consumed_len;
}


size_t M_parser_consume_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len)
{
	return M_parser_consume_charset_int(parser, charset, charset_len, M_TRUE);
}


size_t M_parser_consume_predicate_max(M_parser_t *parser, M_parser_predicate_func func, size_t max)
{
	size_t len;

	if (parser == NULL || func == NULL || max == 0)
		return 0;

	if (max > parser->data_len)
		max = parser->data_len;

	for (len=0; len<max; len++) {
		if (!func(parser->data[len])) {
			break;
		}
	}

	M_parser_consume(parser, len);
	return len;
}

size_t M_parser_consume_predicate(M_parser_t *parser, M_parser_predicate_func func)
{
	return M_parser_consume_predicate_max(parser, func, SIZE_MAX);
}

size_t M_parser_consume_chr_predicate_max(M_parser_t *parser, M_chr_predicate_func func, size_t max)
{
	return M_parser_consume_predicate_max(parser, (M_parser_predicate_func)func, max);
}

size_t M_parser_consume_chr_predicate(M_parser_t *parser, M_chr_predicate_func func)
{
	return M_parser_consume_predicate(parser, (M_parser_predicate_func)func);
}


size_t M_parser_consume_str_until(M_parser_t *parser, const char *pat, M_bool eat_pat)
{
	return M_parser_consume_until(parser, (const unsigned char *)pat, M_str_len(pat), eat_pat);
}

size_t M_parser_consume_str_boundary(M_parser_t *parser, const char *pat, M_bool eat_pat, M_bool *found)
{
	return M_parser_consume_boundary(parser, (const unsigned char *)pat, M_str_len(pat), eat_pat, found);
}

size_t M_parser_consume_not_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len)
{
	return M_parser_consume_charset_int(parser, (const unsigned char *)charset, charset_len, M_FALSE);
}


size_t M_parser_consume_str_charset(M_parser_t *parser, const char *charset)
{
	return M_parser_consume_charset_int(parser, (const unsigned char *)charset, M_str_len(charset), M_TRUE);
}


size_t M_parser_consume_str_not_charset(M_parser_t *parser, const char *charset)
{
	return M_parser_consume_not_charset(parser, (const unsigned char *)charset, M_str_len(charset));
}


size_t M_parser_consume_eol(M_parser_t *parser)
{
	size_t i;

	if (parser == NULL)
		return 0;

	for (i=0; i<parser->data_len; i++) {
		if (parser->data[i] == '\n') {
			i++;
			break;
		}
	}

	M_parser_consume(parser, i);
	return i;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_read_int(M_parser_t *parser, enum M_PARSER_INTEGER_TYPE type, size_t len, unsigned char base, M_int64 *integer)
{
	switch (type) {
		case M_PARSER_INTEGER_BIGENDIAN:
		case M_PARSER_INTEGER_LITTLEENDIAN:
			return M_parser_read_int_binary(parser, len, type == M_PARSER_INTEGER_BIGENDIAN?M_TRUE:M_FALSE, integer);
		case M_PARSER_INTEGER_ASCII:
			return M_parser_read_int_ascii(parser, len, base, integer);
		default:
			break;
	}

	return M_FALSE;
}


M_bool M_parser_read_uint(M_parser_t *parser, enum M_PARSER_INTEGER_TYPE type, size_t len, unsigned char base, M_uint64 *integer)
{
	switch (type) {
		case M_PARSER_INTEGER_BIGENDIAN:
		case M_PARSER_INTEGER_LITTLEENDIAN:
			return M_parser_read_int_binary(parser, len, type == M_PARSER_INTEGER_BIGENDIAN?M_TRUE:M_FALSE, (M_int64 *)integer);
		case M_PARSER_INTEGER_ASCII:
			return M_parser_read_uint_ascii(parser, len, base, integer);
		default:
			break;
	}

	return M_FALSE;
}


M_bool M_parser_read_uint_bcd(M_parser_t *parser, size_t len, M_uint64 *integer)
{
	size_t pos = 1;
	size_t i;

	if (parser == NULL || integer == NULL || len == 0 || len > 10 || len > parser->data_len)
		return M_FALSE;

	*integer = 0;
	for (i=len; i-->0; ) {
		*integer += (parser->data[i] & 0x0F) * pos;
		pos *= 10;
		*integer += (parser->data[i] >> 4) * pos;
		pos *= 10;
	}

	M_parser_consume(parser, len);
	return M_TRUE;
}


enum M_DECIMAL_RETVAL M_parser_read_decimal(M_parser_t *parser, size_t len, M_bool truncate_fail, M_decimal_t *decimal)
{
	enum M_DECIMAL_RETVAL rv;
	const char           *end = NULL;

	if (parser == NULL || decimal == NULL || len > parser->data_len || parser->data_len == 0)
		return M_DECIMAL_INVALID;

	if (len == 0)
		len = parser->data_len;

	rv = M_decimal_from_str((const char *)parser->data, len, decimal, &end);

	if (rv == M_DECIMAL_OVERFLOW || rv == M_DECIMAL_INVALID)
		return rv;

	if (rv == M_DECIMAL_TRUNCATION && truncate_fail)
		return rv;

	M_parser_consume(parser, (size_t)(((const unsigned char *)end) - parser->data));
	return rv;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_read_byte(M_parser_t *parser, unsigned char *byte)
{
	if (parser == NULL || byte == NULL || parser->data_len < 1)
		return M_FALSE;

	*byte = *parser->data;
	M_parser_consume(parser, 1);
	return M_TRUE;
}


M_bool M_parser_read_bytes(M_parser_t *parser, size_t len, unsigned char *buf)
{
	if (parser == NULL || parser->data_len < len || buf == NULL || len == 0)
		return M_FALSE;

	M_mem_copy(buf, parser->data, len);
	M_parser_consume(parser, len);
	return M_TRUE;
}


size_t M_parser_read_bytes_max(M_parser_t *parser, size_t len, unsigned char *buf, size_t buf_len)
{
	if (parser == NULL || buf == NULL || len > buf_len)
		return M_FALSE;

	if (len > parser->data_len)
		len = parser->data_len;

	if (len == 0)
		return M_FALSE;

	M_mem_copy(buf, parser->data, len);
	M_parser_consume(parser, len);
	return len;
}


size_t M_parser_read_bytes_until(M_parser_t *parser, unsigned char *buf, size_t buf_len, const unsigned char *pat, size_t pat_len, M_bool eat_pat)
{
	size_t rlen;

	if (parser == NULL || buf == NULL || buf_len == 0 || pat == NULL || pat_len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_until(parser, pat, pat_len, eat_pat);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, buf, buf_len);
}


size_t M_parser_read_bytes_boundary(M_parser_t *parser, unsigned char *buf, size_t buf_len, const unsigned char *pat, size_t len, M_bool eat_pat, M_bool *found)
{
	size_t rlen;

	if (parser == NULL || buf == NULL || buf_len == 0 || pat == NULL || len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_boundary(parser, pat, len, eat_pat, found);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, buf, buf_len);

}


size_t M_parser_read_bytes_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len, unsigned char *buf, size_t buf_len)
{
	return M_parser_read_bytes_charset_int(parser, charset, charset_len, buf, buf_len, M_TRUE);
}


size_t M_parser_read_bytes_not_charset(M_parser_t *parser, const unsigned char *charset, size_t charset_len, unsigned char *buf, size_t buf_len)
{
	return M_parser_read_bytes_charset_int(parser, charset, charset_len, buf, buf_len, M_TRUE);
}


size_t M_parser_read_bytes_predicate(M_parser_t *parser, M_parser_predicate_func func, unsigned char *buf, size_t buf_len)
{
	if (parser == NULL || buf == NULL || buf_len == 0 || func == NULL)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	if (M_parser_consume_predicate_max(parser, func, buf_len) == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, buf, buf_len);
}


size_t M_parser_read_bytes_chr_predicate(M_parser_t *parser, M_chr_predicate_func func, unsigned char *buf, size_t buf_len)
{
	return M_parser_read_bytes_predicate(parser, (M_parser_predicate_func)func, buf, buf_len);
}


size_t M_parser_read_bytes_mark(M_parser_t *parser, unsigned char *buf, size_t buf_len)
{
	return M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_USER, buf, buf_len);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_read_str(M_parser_t *parser, size_t len, char *buf, size_t buf_len)
{
	if (parser == NULL || buf == NULL || buf_len <= len)
		return M_FALSE;

	if (!M_parser_read_bytes(parser, len, (unsigned char *)buf))
		return M_FALSE;

	/* Don't forget to NULL term */
	buf[len] = 0;
	return M_TRUE;
}


size_t M_parser_read_str_max(M_parser_t *parser, size_t len, char *buf, size_t buf_len)
{
	size_t cnt;

	if (parser == NULL || buf == NULL || buf_len <= len)
		return 0;

	cnt = M_parser_read_bytes_max(parser, len, (unsigned char *)buf, buf_len-1);

	/* Don't forget to NULL term */
	if (buf_len)
		buf[cnt] = 0;
	return cnt;
}


size_t M_parser_read_str_until(M_parser_t *parser, char *buf, size_t buf_len, const char *pat, M_bool eat_pat)
{
	size_t len;

	if (parser == NULL || buf == NULL || buf_len == 0 || pat == NULL || *pat == '\0')
		return 0;

	len = M_parser_read_bytes_until(parser, (unsigned char *)buf, buf_len-1, (const unsigned char *)pat, M_str_len(pat), eat_pat);

	/* NULL terminate */
	if (buf_len)
		buf[len] = 0;
	return len;
}


size_t M_parser_read_str_boundary(M_parser_t *parser, char *buf, size_t buf_len, const char *pat, M_bool eat_pat, M_bool *found)
{
	size_t len;

	if (parser == NULL || buf == NULL || buf_len == 0 || M_str_isempty(pat))
		return 0;

	len = M_parser_read_bytes_boundary(parser, (unsigned char *)buf, buf_len-1, (const unsigned char *)pat, M_str_len(pat), eat_pat, found);

	/* NULL terminate */
	if (buf_len)
		buf[len] = 0;
	return len;
}

size_t M_parser_read_str_charset(M_parser_t *parser, const char *charset, char *buf, size_t buf_len)
{
	return M_parser_read_str_charset_int(parser, charset, buf, buf_len, M_TRUE);
}


size_t M_parser_read_str_not_charset(M_parser_t *parser, const char *charset, char *buf, size_t buf_len)
{
	return M_parser_read_str_charset_int(parser, charset, buf, buf_len, M_FALSE);
}


size_t M_parser_read_str_predicate(M_parser_t *parser, M_parser_predicate_func func, char *buf, size_t buf_len)
{
	size_t len;

	if (buf == NULL || buf_len == 0)
		return 0;

	len = M_parser_read_bytes_predicate(parser, func, (unsigned char *)buf, buf_len-1);

	if (len == 0)
		return 0;

	/* NULL terminate */
	buf[len]=0;
	return len;
}


size_t M_parser_read_str_chr_predicate(M_parser_t *parser, M_chr_predicate_func func, char *buf, size_t buf_len)
{
	return M_parser_read_str_predicate(parser, (M_parser_predicate_func)func, buf, buf_len);
}


size_t M_parser_read_str_mark(M_parser_t *parser, char *buf, size_t buf_len)
{
	size_t len;

	if (parser == NULL || buf == NULL || buf_len == 0)
		return 0;

	len = M_parser_read_bytes_mark(parser, (unsigned char *)buf, buf_len-1);
	buf[len] = 0;
	return len;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_parser_read_strdup(M_parser_t *parser, size_t len)
{
	char *out;

	if (parser == NULL || len == 0)
		return NULL;

	out = M_malloc(len + 1);
	if (!M_parser_read_str(parser, len, out, len + 1)) {
		M_free(out);
		return NULL;
	}

	return out;
}


char *M_parser_read_strdup_hex(M_parser_t *parser, size_t len)
{
	char *hex;

	if (parser == NULL || len == 0 || parser->data_len < len) {
		return NULL;
	}

	hex = M_bincodec_encode_alloc(M_parser_peek(parser), parser->data_len, 0, M_BINCODEC_HEX);
	M_parser_consume(parser, parser->data_len);
	return hex;
}


char *M_parser_read_strdup_until(M_parser_t *parser, const char *pat, M_bool eat_pat)
{
	size_t len;
	char  *out = NULL;

	if (parser == NULL || pat == NULL || *pat == '\0')
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	len = M_parser_consume_str_until(parser, pat, eat_pat);
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	out = M_malloc(len+1);
	
	/* Output the data from the marked position, this will also clear the mark */
	if (M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, (unsigned char *)out, len) != len) {
		M_free(out);
		return NULL;
	}

	out[len] = 0;
	return out;
}


char *M_parser_read_strdup_boundary(M_parser_t *parser, const char *pat, M_bool eat_pat, M_bool *found)
{
	size_t len;
	char  *out = NULL;

	if (parser == NULL || pat == NULL || *pat == '\0')
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	len = M_parser_consume_str_boundary(parser, pat, eat_pat, found);
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	out = M_malloc(len+1);

	/* Output the data from the marked position, this will also clear the mark */
	if (M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, (unsigned char *)out, len) != len) {
		M_free(out);
		return NULL;
	}

	out[len] = 0;
	return out;
}


char *M_parser_read_strdup_charset(M_parser_t *parser, const char *charset)
{
	return M_parser_read_strdup_charset_int(parser, charset, M_TRUE);
}


char *M_parser_read_strdup_not_charset(M_parser_t *parser, const char *charset)
{
	return M_parser_read_strdup_charset_int(parser, charset, M_FALSE);
}


char *M_parser_read_strdup_predicate_max(M_parser_t *parser, M_parser_predicate_func func, size_t max)
{
	size_t len;
	char  *out = NULL;

	if (parser == NULL || func == NULL || max == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	len = M_parser_consume_predicate_max(parser, func, max);
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	out = M_malloc(len+1);
	
	/* Output the data from the marked position, this will also clear the mark */
	if (M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, (unsigned char *)out, len) != len) {
		M_free(out);
		return NULL;
	}

	out[len] = 0;
	return out;
}

char *M_parser_read_strdup_predicate(M_parser_t *parser, M_parser_predicate_func func)
{
	return M_parser_read_strdup_predicate_max(parser, func, SIZE_MAX);
}


char *M_parser_read_strdup_chr_predicate_max(M_parser_t *parser, M_chr_predicate_func func, size_t max)
{
	return M_parser_read_strdup_predicate_max(parser, (M_parser_predicate_func)func, max);
}

char *M_parser_read_strdup_chr_predicate(M_parser_t *parser, M_chr_predicate_func func)
{
	return M_parser_read_strdup_predicate(parser, (M_parser_predicate_func)func);
}


char *M_parser_read_strdup_mark(M_parser_t *parser)
{
	size_t len;
	char  *out;

	len = M_parser_mark_len(parser);
	if (len == 0)
		return NULL;

	out = M_malloc(len+1);
	M_parser_read_str_mark(parser, out, len+1);
	return out;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_parser_t *M_parser_read_parser(M_parser_t *parser, size_t len)
{
	if (parser == NULL || len == 0)
		return NULL;

	M_parser_mark_int(parser, M_PARSER_MARKED_INT);
	M_parser_consume(parser, len);
	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_INT);
}


M_parser_t *M_parser_read_parser_until(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat)
{
	size_t rlen;

	if (parser == NULL || pat == NULL || len == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_until(parser, pat, len, eat_pat);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_INT);
}


M_parser_t *M_parser_read_parser_boundary(M_parser_t *parser, const unsigned char *pat, size_t len, M_bool eat_pat, M_bool *found)
{
	size_t rlen;

	if (parser == NULL || pat == NULL || len == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_boundary(parser, pat, len, eat_pat, found);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_INT);
}


M_parser_t *M_parser_read_parser_charset(M_parser_t *parser, unsigned const char *charset, size_t charset_len)
{
	size_t len;

	if (parser == NULL || charset == NULL || charset_len == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	len = M_parser_consume_charset_int(parser, charset, charset_len, M_TRUE);
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_INT);
}


M_parser_t *M_parser_read_parser_predicate_max(M_parser_t *parser, M_parser_predicate_func func, size_t max)
{
	size_t len;

	if (parser == NULL || func == NULL || max == 0)
		return NULL;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	len = M_parser_consume_predicate_max(parser, func, max);
	if (len == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return NULL;
	}

	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_INT);
}

M_parser_t *M_parser_read_parser_predicate(M_parser_t *parser, M_parser_predicate_func func)
{
	return M_parser_read_parser_predicate_max(parser, func, SIZE_MAX);
}


M_parser_t *M_parser_read_parser_chr_predicate(M_parser_t *parser, M_parser_predicate_func func)
{
	return M_parser_read_parser_predicate(parser, (M_parser_predicate_func)func);
}

M_parser_t *M_parser_read_parser_chr_predicate_max(M_parser_t *parser, M_parser_predicate_func func, size_t max)
{
	return M_parser_read_parser_predicate_max(parser, (M_parser_predicate_func)func, max);
}


M_parser_t *M_parser_read_parser_mark(M_parser_t *parser)
{
	return M_parser_read_parser_mark_int(parser, M_PARSER_MARKED_USER);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_parser_t **M_parser_split(M_parser_t *parser, unsigned char delim, size_t maxcnt, M_uint32 flags, size_t *num_output)
{
	return M_parser_split_pat(parser, &delim, 1, maxcnt, flags, num_output);
}

M_parser_t **M_parser_split_pat(M_parser_t *parser, const unsigned char *pat, size_t pat_len, size_t maxcnt, M_uint32 flags, size_t *num_output)
{
	size_t               cnt = 0;
	M_parser_t         **parsers;
	const unsigned char *ptr;
	size_t               ptrlen;
	*num_output          = 0;

	if (parser == NULL)
		return NULL;

	/* Count number of delimiters to get number of output sections */
	cnt = 1;
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);
	while (M_parser_consume_until(parser, pat, pat_len, M_TRUE) != 0) {
		cnt++;
		if (maxcnt != 0 && cnt == maxcnt) {
			break;
		}
	}
	M_parser_mark_rewind_int(parser, M_PARSER_MARKED_INT);

	if (cnt == 1 && flags & M_PARSER_SPLIT_FLAG_NODELIM_ERROR)
		return NULL;

	parsers = M_malloc_zero(cnt * sizeof(*parsers));
	cnt     = 0;

	while (M_parser_len(parser)) {
		size_t curr_col;
		size_t curr_line;
		M_bool trim_delimiter = M_FALSE;

		/* Mark start position */
		M_parser_mark_int(parser, M_PARSER_MARKED_INT);
		curr_col  = parser->curr_col;
		curr_line = parser->curr_line;

		if (maxcnt != 0 && cnt == maxcnt - 1) {
			/* At the max count, everything goes into this last entry */
			M_parser_consume(parser, parser->data_len);
		} else {
			/* If we can't find the delimiter, just consume the rest of the input */
			if (M_parser_consume_until(parser, pat, pat_len, M_TRUE) == 0) {
				M_parser_consume(parser, parser->data_len);
			} else {
				trim_delimiter = M_TRUE;
			}
		}
		ptr = M_parser_marked_buffer_start(parser, M_PARSER_MARKED_INT, &ptrlen);

		/* M_parser_consume_until also consumes the specified pat so trim
		 * that if that is the function we called */
		if (trim_delimiter && ptrlen)
			ptrlen -= pat_len;

		parsers[cnt] = M_parser_create(parser->flags);
		M_parser_append(parsers[cnt], ptr, ptrlen);

		/* Silence clang, this should not be possible */
		if (parsers[cnt] != NULL) {
			/* Preserve col/line numbers in children*/
			parsers[cnt]->curr_col  = curr_col;
			parsers[cnt]->curr_line = curr_line;
		}

		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		cnt++;
	}

	*num_output = cnt;
	return parsers;
}

M_parser_t **M_parser_split_str_pat(M_parser_t *parser, const char *pat, size_t maxcnt, M_uint32 flags, size_t *num_output)
{
	return M_parser_split_pat(parser, (const unsigned char *)pat, M_str_len(pat), maxcnt, flags, num_output);
}

void M_parser_split_free(M_parser_t **parsers, size_t cnt)
{
	size_t i;
	if (parsers == NULL || cnt == 0)
		return;

	for (i=0; i<cnt; i++)
		M_parser_destroy(parsers[i]);

	M_free(parsers);
}

M_PARSER_FRAME_ERROR M_parser_read_stxetxlrc_message(M_parser_t *parser, M_parser_t **out, M_uint32 lrc_frame_chars)
{
	unsigned char *data;
	unsigned char  byte;
	unsigned char  msg_lrc;
	size_t         rlen;
	size_t         offset;
	size_t         clen;

	if (parser == NULL || out == NULL || M_parser_len(parser) < 4)
		return M_PARSER_FRAME_ERROR_INVALID;

	if (!M_parser_peek_byte(parser, &byte) || byte != 0x02)
		return M_PARSER_FRAME_ERROR_NO_STX;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the message up to and including the ETX */
	rlen = M_parser_consume_until(parser, (const unsigned char *)"\x03", 1, M_TRUE);
	/* No ETX or not enough bytes to include the LRC */
	if (rlen == 0) {
		M_parser_mark_rewind_int(parser, M_PARSER_MARKED_INT);
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return M_PARSER_FRAME_ERROR_NO_ETX;
	} else if (M_parser_len(parser) == 0) {
		M_parser_mark_rewind_int(parser, M_PARSER_MARKED_INT);
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return M_PARSER_FRAME_ERROR_NO_LRC;
	}

	/* Pull out the marked data STX-EXT. Will clear mark. */
	data = M_malloc(rlen);
	rlen = M_parser_read_bytes_mark_int(parser, M_PARSER_MARKED_INT, data, rlen);

	/* Add the message to the output parser */
	*out = M_parser_create(parser->flags);
	M_parser_append(*out, data+1, rlen-2);

	/* Determine which (if any) framing characters will be included in the
 	 * LRC calculation. */
	offset = 0;
	clen   = rlen;
	if (!(lrc_frame_chars & M_PARSER_FRAME_STX)) {
		offset++;
		clen--;
	}
	if (!(lrc_frame_chars & M_PARSER_FRAME_ETX)) {
		clen--;
	}

	/* Calculate the LRC for the message. */
	msg_lrc = M_mem_calc_lrc(data+offset, clen);

	/* Free the data since it was already added to the out parser. */
	M_free(data);

	/* Get the LRC from the input parser that was left on after pulling
 	 * off the framed messasge. */
	M_parser_read_byte(parser, &byte);

	/* Verify the LRC. */
	if (msg_lrc != byte)
		return M_PARSER_FRAME_ERROR_LRC_CALC_FAILED;
	return M_PARSER_FRAME_ERROR_SUCCESS;
}


M_bool M_parser_read_buf(M_parser_t *parser, M_buf_t *buf, size_t len)
{
	if (parser == NULL || buf == NULL || parser->data_len < len)
		return M_FALSE;

	M_buf_add_bytes(buf, parser->data, len);
	M_parser_consume(parser, len);
	return M_TRUE;
}


M_bool M_parser_read_buf_hex(M_parser_t *parser, M_buf_t *buf, size_t len)
{
	size_t         encode_size;
	unsigned char *direct;

	if (parser == NULL || buf == NULL || parser->data_len < len) {
		return M_FALSE;
	}

	encode_size = M_bincodec_encode_size(len, 0, M_BINCODEC_HEX);
	direct      = M_buf_direct_write_start(buf, &encode_size);
	encode_size = M_bincodec_encode((char *)direct, encode_size, M_parser_peek(parser), len, 0, M_BINCODEC_HEX);
	M_buf_direct_write_end(buf, encode_size);

	M_parser_consume(parser, len);

	return M_TRUE;
}


size_t M_parser_read_buf_max(M_parser_t *parser, M_buf_t *buf, size_t len)
{
	if (parser == NULL || buf == NULL)
		return M_FALSE;

	if (len > parser->data_len)
		len = parser->data_len;

	if (len == 0)
		return M_FALSE;

	M_buf_add_bytes(buf, parser->data, len);
	M_parser_consume(parser, len);
	return M_TRUE;
}

size_t M_parser_read_buf_until(M_parser_t *parser, M_buf_t *buf, const unsigned char *pat, size_t pat_len, M_bool eat_pat)
{
	size_t rlen;

	if (parser == NULL || buf == NULL || pat == NULL || pat_len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_until(parser, pat, pat_len, eat_pat);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_buf_mark_int(parser, M_PARSER_MARKED_INT, buf);
}

size_t M_parser_read_buf_boundary(M_parser_t *parser, M_buf_t *buf, const unsigned char *pat, size_t len, M_bool eat_pat, M_bool *found)
{
	size_t rlen;

	if (parser == NULL || buf == NULL || pat == NULL || len == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	rlen = M_parser_consume_boundary(parser, pat, len, eat_pat, found);
	if (rlen == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_buf_mark_int(parser, M_PARSER_MARKED_INT, buf);
}

size_t M_parser_read_buf_charset(M_parser_t *parser, M_buf_t *buf, const unsigned char *charset, size_t charset_len)
{
	return M_parser_read_buf_charset_int(parser, buf, charset, charset_len, M_TRUE);
}

size_t M_parser_read_buf_not_charset(M_parser_t *parser, M_buf_t *buf, const unsigned char *charset, size_t charset_len)
{
	return M_parser_read_buf_charset_int(parser, buf, charset, charset_len, M_FALSE);
}

size_t M_parser_read_buf_predicate_max(M_parser_t *parser, M_buf_t *buf, M_parser_predicate_func func, size_t max)
{
	if (parser == NULL || buf == NULL || func == NULL || max == 0)
		return 0;

	/* Mark internal */
	M_parser_mark_int(parser, M_PARSER_MARKED_INT);

	/* Consume the charset */
	if (M_parser_consume_predicate_max(parser, func, max) == 0) {
		M_parser_mark_clear_int(parser, M_PARSER_MARKED_INT);
		return 0;
	}

	/* Output the data from the marked position, this will also clear the mark */
	return M_parser_read_buf_mark_int(parser, M_PARSER_MARKED_INT, buf);
}

size_t M_parser_read_buf_predicate(M_parser_t *parser, M_buf_t *buf, M_parser_predicate_func func)
{
	return M_parser_read_buf_predicate_max(parser, buf, func, SIZE_MAX);
}

size_t M_parser_read_buf_chr_predicate(M_parser_t *parser, M_buf_t *buf, M_chr_predicate_func func)
{
	return M_parser_read_buf_predicate(parser, buf, (M_parser_predicate_func)func);
}

size_t M_parser_read_buf_chr_predicate_max(M_parser_t *parser, M_buf_t *buf, M_chr_predicate_func func, size_t max)
{
	return M_parser_read_buf_predicate_max(parser, buf, (M_parser_predicate_func)func, max);
}

size_t M_parser_read_buf_mark(M_parser_t *parser, M_buf_t *buf)
{
	return M_parser_read_buf_mark_int(parser, M_PARSER_MARKED_USER, buf);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_parser_is_predicate(M_parser_t *parser, size_t len, M_parser_predicate_func func)
{
	size_t i;

	if (parser == NULL || len == 0 || func == NULL)
		return M_FALSE;

	if (len > parser->data_len)
		len = parser->data_len;

	for (i=0; i<len; i++) {
		if (!func(parser->data[i])) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}

M_bool M_parser_is_chr_predicate(M_parser_t *parser, size_t len, M_chr_predicate_func func)
{
	return M_parser_is_predicate(parser, len, (M_parser_predicate_func)func);
}

M_bool M_parser_is_charset(M_parser_t *parser, size_t len, const unsigned char *charset, size_t charset_len)
{
	size_t i;
	size_t j;

	if (parser == NULL || len == 0 || charset == NULL || charset_len == 0)
		return M_FALSE;

	if (len > parser->data_len)
		len = parser->data_len;

	for (i=0; i<len; i++) {
		for (j=0; j<charset_len; j++) {
			if (parser->data[i] == charset[j]) {
				break;
			}
		}
		if (j == charset_len) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}

M_bool M_parser_is_str_charset(M_parser_t *parser, size_t len, const char *charset)
{
	return M_parser_is_charset(parser, len, (const unsigned char *)charset, M_str_len(charset));
}

M_bool M_parser_is_not_predicate(M_parser_t *parser, size_t len, M_parser_predicate_func func)
{
	return !M_parser_is_predicate(parser, len, func);
}

M_bool M_parser_is_not_chr_predicate(M_parser_t *parser, size_t len, M_chr_predicate_func func)
{
	return !M_parser_is_chr_predicate(parser, len, func);
}

M_bool M_parser_is_not_charset(M_parser_t *parser, size_t len, const unsigned char *charset, size_t charset_len)
{
	return !M_parser_is_charset(parser, len, charset, charset_len);
}

M_bool M_parser_is_not_str_charset(M_parser_t *parser, size_t len, const char *charset)
{
	return !M_parser_is_str_charset(parser, len, charset);
}
