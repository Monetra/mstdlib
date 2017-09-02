/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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
#include <mstdlib/mstdlib_formats.h>
#include "json/m_json_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_json_node_t *M_json_read_value(M_parser_t *parser, M_uint32 flags, M_json_error_t *error);

/*! Eat comments.
 * Supports C and C++ style / * and / / (no spaces between the two characters) comments.
 *
 * There is debate whether comments are really allowed by JSON. The spec doesn't support them
 * but the creator of JSON (who wrote the spec) Douglas Crockford says:
 * "JSON does not have comments. A JSON encoder MUST NOT output comments.
 * A JSON decoder MAY accept and ignore comments." (http://tech.groups.yahoo.com/group/json/message/152)
 *
 * Since JSON is "JavaScript Object Notation" we only support comments supported by Javascript.
 */
static M_bool M_json_eat_comment(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	const unsigned char *s;
	M_bool               isblock = M_FALSE;;

	if (flags & M_JSON_READER_DISALLOW_COMMENTS)
		return M_TRUE;

	/* Check if we have a comment. */
	s = M_parser_peek(parser);
	if (M_parser_len(parser) < 2 || *s != '/')
		return M_TRUE;

	/* Determine if the comment is a block or line comment. */
	isblock = (*(s+1) == '*');
	if (isblock) {
		/* Move past the opening of the comment */
		M_parser_mark(parser);
		M_parser_consume(parser, 2);
		if (!M_parser_consume_until(parser, (const unsigned char *)"*/", 2, M_TRUE)) {
			M_parser_mark_rewind(parser);
			*error = M_JSON_ERROR_MISSING_COMMENT_CLOSE;
			return M_FALSE;
		}
		M_parser_mark_clear(parser);
	} else if (*(s+1) == '/') {
		M_parser_consume_eol(parser);
	} else {
		*error = M_JSON_ERROR_UNEXPECTED_COMMENT_START;
		return M_FALSE;
	}

	return M_TRUE;
}

static M_bool M_json_eat_whitespace(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	(void)flags;
	(void)error;
	M_parser_consume_whitespace(parser, 0);
	return M_TRUE;
}

static struct {
	M_bool (*eater)(M_parser_t *parser, M_uint32 flags, M_json_error_t *error);
} M_json_eaters[] = {
	{ M_json_eat_whitespace },
	{ M_json_eat_comment    },
	{ M_json_eat_whitespace },
	{ NULL }
};

static M_bool M_json_eat_ignored(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	unsigned char c;
	size_t        i;
	size_t        len = 0;

	/* Short cut if we don't allow comments just eat any whitespace. */
	if (flags & M_JSON_READER_DISALLOW_COMMENTS) {
		M_json_eat_whitespace(parser, flags, error);
		return M_TRUE;
	}

	/* Loop though all of our eaters until we have nothing to eat.
	 *  We'll eat any white space and then comments, then any whitespace after the comment.
 	 *  Keep doing this until we've run out of comments. */
	do {
		len = M_parser_len(parser); 
		for (i=0; M_json_eaters[i].eater!=NULL; i++) {
			if (!M_json_eaters[i].eater(parser, flags, error)) {
				return M_FALSE;
			}
			if (M_parser_len(parser) == 0) {
				break;
			}
		}
	} while (!(flags & M_JSON_READER_DISALLOW_COMMENTS) && M_parser_peek_byte(parser, &c) && c == '/' && len != M_parser_len(parser));

	return M_TRUE;
}

static M_json_node_t *M_json_read_object(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	M_json_node_t *key_node;
	M_json_node_t *val_node;
	M_json_node_t *node    = NULL;
	unsigned char  c;

	/* Move past the opening '{'. */
	M_parser_consume(parser, 1);
	node = M_json_node_create(M_JSON_TYPE_OBJECT);

	while (M_parser_peek_byte(parser, &c) && c != '}') {
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_node_destroy(node);
			return NULL;
		}

		/* An empty object is okay and valid. */
		if (!M_parser_peek_byte(parser, &c) || c == '}') {
			break;
		}

		/* Check that the key part of the pair is a string. 
 		 * We're going to advance and check for '"' instead of
		 * relying on the type returned by M_json_read_value because
		 * if we have a list (for example) M_json_read_value will parse
		 * the list. We don't want to potentially parse a lot of data
		 * we will ignore because it's not a string. */
		if (c != '"') {
			M_json_node_destroy(node);
			*error = M_JSON_ERROR_INVALID_PAIR_START;
			return NULL;
		}

		/* Read the key part of the pair. */
		key_node = M_json_read_value(parser, flags, error);
		if (key_node == NULL) {
			M_json_node_destroy(node);
			return NULL;
		}

		/* Check if the key is unique (if it matters). */
		if (flags & M_JSON_READER_OBJECT_UNIQUE_KEYS &&
			M_json_object_value(node, M_json_get_string(key_node)) != NULL)
		{
			*error = M_JSON_ERROR_DUPLICATE_KEY;
			M_json_node_destroy(key_node);
			M_json_node_destroy(node);
			return NULL;
		}

		/* Check for the ':' separator. */
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_node_destroy(key_node);
			M_json_node_destroy(node);
			return NULL;
		}
		if (!M_parser_peek_byte(parser, &c) || c != ':') {
			*error = M_JSON_ERROR_MISSING_PAIR_SEPARATOR;
			M_json_node_destroy(key_node);
			M_json_node_destroy(node);
			return NULL;
		}
		/* Move past the ':' separator. */
		M_parser_consume(parser, 1);

		/* Read the value part of the pair. */
		val_node = M_json_read_value(parser, flags, error);
		if (val_node == NULL) {
			M_json_node_destroy(key_node);
			M_json_node_destroy(node);
			return NULL;
		}

		/* Add the value to the hashtable. */
		M_json_object_insert(node, M_json_get_string(key_node), val_node);
		M_json_node_destroy(key_node);

		/* Check for a member separator and advance if necessary */
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_node_destroy(node);
			return NULL;
		}

		if (!M_parser_peek_byte(parser, &c) && c != ',' && c != '}') {
			M_json_node_destroy(node);
			*error = M_JSON_ERROR_OBJECT_UNEXPECTED_CHAR;
			return NULL;
		}
		if (c == ',') {
			M_parser_consume(parser, 1);
			/* Check if we have an object end '}' after the separator. */
			if (!M_json_eat_ignored(parser, flags, error)) {
				M_json_node_destroy(node);
				return NULL;
			}
			if (!M_parser_peek_byte(parser, &c) || c == '}') {
				*error = M_JSON_ERROR_EXPECTED_VALUE;
				M_json_node_destroy(node);
				return NULL;
			}
		}
	}

	/* Check the object is closed. */
	if (!M_parser_peek_byte(parser, &c) || c != '}') {
		*error = M_JSON_ERROR_UNCLOSED_OBJECT;
		M_json_node_destroy(node);
		return NULL;
	}

	/* Advance past the closing }. */
	M_parser_consume(parser, 1);

	return node;
}

static M_json_node_t *M_json_read_array(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	M_json_node_t *sub_node;
	M_json_node_t *node    = NULL;
	unsigned char  c;
	size_t         len     = 0;

	/* Move past the opening '['. */
	M_parser_consume(parser, 1);
	node = M_json_node_create(M_JSON_TYPE_ARRAY);

	if (!M_json_eat_ignored(parser, flags, error)) {
		M_json_node_destroy(node);
		return NULL;
	}

	while (len != M_parser_len(parser) && M_parser_peek_byte(parser, &c) && c != ']') {
		len = M_parser_len(parser);
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_node_destroy(node);
			return NULL;
		}

		/* An empty array is okay and valid. */
		if (!M_parser_peek_byte(parser, &c) || c == ']') {
			break;
		}

		/* Read the value from the list*/
		sub_node = M_json_read_value(parser, flags, error);
		if (sub_node == NULL) {
			M_json_node_destroy(node);
			return NULL;
		}

		/* Add the value to the list. */
		M_json_array_insert(node, sub_node);

		/* Validate we have a value separator and advance if necessary */
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_node_destroy(node);
			return NULL;
		}

		if (!M_parser_peek_byte(parser, &c) && c != ',' && c != ']') {
			M_json_node_destroy(node);
			*error = M_JSON_ERROR_ARRAY_UNEXPECTED_CHAR;
			return NULL;
		}
		if (c == ',') {
			M_parser_consume(parser, 1);
			/* Check if we have an array end ']' after the separator. */
			if (!M_json_eat_ignored(parser, flags, error)) {
				M_json_node_destroy(node);
				return NULL;
			}
			if (!M_parser_peek_byte(parser, &c) || c == ']') {
				*error = M_JSON_ERROR_EXPECTED_VALUE;
				M_json_node_destroy(node);
				return NULL;
			}
		}
	}

	if (!M_parser_peek_byte(parser, &c) || c != ']') {
		*error = M_JSON_ERROR_UNCLOSED_ARRAY;
		M_json_node_destroy(node);
		return NULL;
	}

	/* Advance past the closing ]. */
	M_parser_consume(parser, 1);

	return node;
}

static M_json_node_t *M_json_read_string(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	M_json_node_t       *node;
	M_buf_t             *buf;
	char                *out;
	const unsigned char *s;
	unsigned char        c;
	unsigned char        ce;
	unsigned char        cp = 0;

	(void)flags;

	/* Skip past the '"' that starts the string. */
	M_parser_consume(parser, 1);

	buf = M_buf_create();
	while (M_parser_peek_byte(parser, &c) && c != '"' && cp != '\\') {
		/* Control character. */
		if (c < 32) {
			if (c == '\n') {
				*error = M_JSON_ERROR_UNEXPECTED_NEWLINE;
			} else {
				*error = M_JSON_ERROR_UNEXPECTED_CONTROL_CHAR;
			}
			M_buf_cancel(buf);
			return NULL;
		} else if (c > 127) {
			/* XXX: Support this */
			M_buf_add_byte(buf, '?');
		/* Escape. */
		} else if (c == '\\') {
			/* Set ce to something invalid so the default case will pick it up when we don't have enough bytes left. */
			ce = 0;
			if (M_parser_len(parser) >= 2) {
				s = M_parser_peek(parser)+1;
				ce = *s;
			}
			switch (ce) {
				case '"':
				case '/':
					M_buf_add_byte(buf, ce);
					break;
				case '\\':
					M_buf_add_byte(buf, ce);
					/* We have \\ which is an escape for \. We don't want to set cp = \ later because
 					 * it will look like we are starting an escape instead of ending one. */
					ce = 0;
					break;
				case 'b':
					M_buf_add_byte(buf, '\b');
					break;
				case 'f':
					M_buf_add_byte(buf, '\f');
					break;
				case 'n':
					M_buf_add_byte(buf, '\n');
					break;
				case 'r':
					M_buf_add_byte(buf, '\r');
					break;
				case 't':
					M_buf_add_byte(buf, '\t');
					break;
				case 'u':
					/* XXX: Handle unicode escapes */
					if (M_parser_len(parser) < 6   ||
						!M_chr_ishex((char)*(s+1)) ||
						!M_chr_ishex((char)*(s+2)) ||
						!M_chr_ishex((char)*(s+3)) ||
						!M_chr_ishex((char)*(s+4)))
					{
						M_buf_cancel(buf);
						*error = M_JSON_ERROR_INVALID_UNICODE_ESACPE;
						return NULL;
					}
					M_buf_add_byte(buf, '?');
					M_parser_consume(parser, 4);
					break;
				default:
					M_buf_cancel(buf);
					*error = M_JSON_ERROR_UNEXPECTED_ESCAPE;
					return NULL;
			}
			cp = ce;
			M_parser_consume(parser, 2);
			continue;
		}

		cp = c;
		M_buf_add_byte(buf, c);
		M_parser_consume(parser, 1);
	}

	if (!M_parser_peek_byte(parser, &c) || c != '"') {
		M_buf_cancel(buf);
		*error = M_JSON_ERROR_UNCLOSED_STRING;
		return NULL;
	}
	M_parser_consume(parser, 1);

	out  = M_buf_finish_str(buf, NULL);
	node = M_json_node_create(M_JSON_TYPE_STRING);
	M_json_set_string(node, out);
	M_free(out);

	return node;
}

static M_json_node_t *M_json_read_bool(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	M_json_node_t       *node;
	const unsigned char *s;
	size_t               len;
	M_bool               istrue;

	(void)flags;

	len = M_parser_len(parser);
	s   = M_parser_peek(parser);
	if (s == NULL                                                             ||
		(*s == 't' && (len < 4 || !M_str_eq_max((const char *)s, "true", 4))) ||
		(*s == 'f' && (len < 5 || !M_str_eq_max((const char *)s, "false", 5))))
	{
		*error = M_JSON_ERROR_INVALID_BOOL;
		return NULL;
	}
	
	istrue = *s=='t'?M_TRUE:M_FALSE;

	node = M_json_node_create(M_JSON_TYPE_BOOL);
	M_json_set_bool(node, istrue);
	M_parser_consume(parser, istrue?4:5);

	return node;
}

static M_json_node_t *M_json_read_null(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	(void)flags;

	if (M_parser_len(parser) < 4 || !M_str_eq_max((const char *)M_parser_peek(parser), "null", 4)) {
		*error = M_JSON_ERROR_INVALID_NULL;
		return NULL;
	}

	M_parser_consume(parser, 4);
	return M_json_node_create(M_JSON_TYPE_NULL);
}

static M_json_node_t *M_json_read_number(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	M_json_node_t         *node;
	M_decimal_t            decimal;
	enum M_DECIMAL_RETVAL  rv;

	rv = M_parser_read_decimal(parser, 0, !(flags & M_JSON_READER_ALLOW_DECIMAL_TRUNCATION), &decimal);
	if ((!(flags & M_JSON_READER_ALLOW_DECIMAL_TRUNCATION) && rv != M_DECIMAL_SUCCESS) ||
		((flags & M_JSON_READER_ALLOW_DECIMAL_TRUNCATION) && rv != M_DECIMAL_SUCCESS && rv != M_DECIMAL_TRUNCATION))
	{
		*error = M_JSON_ERROR_INVALID_NUMBER;
		return NULL;
	}

	if (M_decimal_num_decimals(&decimal) == 0) {
		node = M_json_node_create(M_JSON_TYPE_INTEGER);
		M_json_set_int(node, M_decimal_to_int(&decimal, 0));
	} else {
		node = M_json_node_create(M_JSON_TYPE_DECIMAL);
		M_json_set_decimal(node, &decimal);
	}

	return node;
}

static M_json_node_t *M_json_read_value(M_parser_t *parser, M_uint32 flags, M_json_error_t *error)
{
	unsigned char c;

	if (!M_json_eat_ignored(parser, flags, error)) {
		return NULL;
	}

	if (M_parser_len(parser) > 0) {
		c = *M_parser_peek(parser);
		switch (c) {
			case '{':
				return M_json_read_object(parser, flags, error);
			case '[':
				return M_json_read_array(parser, flags, error);
			case '"':
				return M_json_read_string(parser, flags, error);
			case 't':
			case 'f':
				return M_json_read_bool(parser, flags, error);
			case 'n':
				return M_json_read_null(parser, flags, error);
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				return M_json_read_number(parser, flags, error);
			case '\0':
				*error = M_JSON_ERROR_UNEXPECTED_TERMINATION;
				return NULL;
			default:
				*error = M_JSON_ERROR_INVALID_IDENTIFIER;
				return NULL;
		}
	}

	*error = M_JSON_ERROR_UNEXPECTED_END;
	return NULL;
}

static void M_json_read_format_error_pos(M_parser_t *parser, size_t *error_line, size_t *error_pos)
{
	if (error_line == NULL && error_pos == NULL)
		return;

	if (error_line != NULL)
		*error_line = 1;
	if (error_pos != NULL)
		*error_pos = 1;

	if (parser == NULL)
		return;

	if (error_line == NULL) {
		*error_pos = M_parser_current_offset(parser);
		return;
	}

	*error_line = M_parser_current_line(parser);
	if (error_pos != NULL) {
		*error_pos = M_parser_current_column(parser);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_json_node_t *M_json_read(const char *data, size_t data_len, M_uint32 flags, size_t *processed_len, M_json_error_t *error, size_t *error_line, size_t *error_pos)
{
	M_json_node_t  *root;
	M_parser_t     *parser;
	M_json_error_t  myerror;
	size_t          myerror_line;
	size_t          myerror_pos;

	if (error == NULL)
		error = &myerror;
	*error = M_JSON_ERROR_SUCCESS;
	if (error_line == NULL)
		error_line = &myerror_line;
	*error_line = 0;
	if (error_pos == NULL)
		error_pos = &myerror_pos;
	*error_pos = 0;

	if (data == NULL || data_len == 0 || *data == '\0') {
		*error = M_JSON_ERROR_MISUSE;
		M_json_read_format_error_pos(NULL, error_line, error_pos);
		return NULL;
	}

	parser = M_parser_create_const((const unsigned char *)data, data_len, M_PARSER_FLAG_TRACKLINES);
	root   = M_json_read_value(parser, flags, error);
	if (root == NULL) {
		M_json_read_format_error_pos(parser, error_line, error_pos);
		M_json_node_destroy(root);
		M_parser_destroy(parser);
		return NULL;
	}

	if (M_json_node_type(root) != M_JSON_TYPE_OBJECT && M_json_node_type(root) != M_JSON_TYPE_ARRAY) {
		*error = M_JSON_ERROR_INVALID_START;
		M_json_read_format_error_pos(NULL, error_line, error_pos);
		M_json_node_destroy(root);
		M_parser_destroy(parser);
		return NULL;
	}

	/* Eat any whitespace after the data. */
	if (M_parser_len(parser) > 0) {
		if (!M_json_eat_ignored(parser, flags, error)) {
			M_json_read_format_error_pos(parser, error_line, error_pos);
			M_json_node_destroy(root);
			M_parser_destroy(parser);
			return NULL;
		}
	}

	if (processed_len) {
		*processed_len = data_len - M_parser_len(parser);
	} else if (M_parser_len(parser) > 0) {
		*error = M_JSON_ERROR_EXPECTED_END;
		M_json_read_format_error_pos(parser, error_line, error_pos);
		M_json_node_destroy(root);
		M_parser_destroy(parser);
		return NULL;
	}

	M_parser_destroy(parser);
	return root;
}

M_json_node_t *M_json_read_file(const char *path, M_uint32 flags, size_t max_read, M_json_error_t *error, size_t *error_line, size_t *error_pos)
{
	char          *buf = NULL;
	M_json_node_t *node;
	size_t         bytes_read;
	M_fs_error_t   res;
	
	res = M_fs_file_read_bytes(path, max_read, (unsigned char **)&buf, &bytes_read);
	if (res != M_FS_ERROR_SUCCESS || buf == NULL) {
		if (error != NULL) {
			*error = M_JSON_ERROR_GENERIC;
		}
		M_free(buf);
		return NULL;
	}

	node = M_json_read(buf, bytes_read, flags, NULL, error, error_line, error_pos);
	M_free(buf);
	return node;
}
