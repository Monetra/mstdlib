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

static M_bool M_json_write_node(const M_json_node_t *node, M_buf_t *buf, size_t *depth, M_uint32 flags);

static void M_json_write_depth(M_buf_t *buf, size_t *depth, M_uint32 flags)
{
	if (flags & M_JSON_WRITER_PRETTYPRINT_SPACE) {
		M_buf_add_fill(buf, ' ', (*depth)*2);
	} else if (flags & M_JSON_WRITER_PRETTYPRINT_TAB) {
		M_buf_add_fill(buf, '\t', *depth);
	}
}

static void M_json_write_newline(M_buf_t *buf, M_uint32 flags)
{
	if (!(flags & (M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_TAB))) {
		return;
	}

	if (flags & M_JSON_WRITER_PRETTYPRINT_WINLINEEND) {
		M_buf_add_str(buf, "\r\n");
	} else {
		M_buf_add_byte(buf, '\n');
	}
}

static M_bool M_json_write_node_object(const M_json_node_t *node, M_buf_t *buf, size_t *depth, M_uint32 flags)
{
	M_hash_strvp_enum_t *hashenum;
	const char          *key;
	void                *value;
	size_t               len;

	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_OBJECT)
		return M_FALSE;

	M_buf_add_byte(buf, '{');
	M_json_write_newline(buf, flags);
	(*depth)++;

	len = M_hash_strvp_enumerate(node->data.json_object, &hashenum);
	while (M_hash_strvp_enumerate_next(node->data.json_object, hashenum, &key, &value)) {
		M_json_write_depth(buf, depth, flags);
		M_buf_add_byte(buf, '"');
		M_buf_add_str(buf, key); 
		M_buf_add_byte(buf, '"');

		if (flags & (M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_TAB))
			M_buf_add_byte(buf, ' ');
		M_buf_add_byte(buf, ':');
		if (flags & (M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_TAB))
			M_buf_add_byte(buf, ' ');

		M_json_write_node((const M_json_node_t *)value, buf, depth, flags);

		len--;
		if (len > 0) {
			M_buf_add_byte(buf, ',');
		}
		M_json_write_newline(buf, flags);
	}
	M_hash_strvp_enumerate_free(hashenum);

	(*depth)--;
	M_json_write_depth(buf, depth, flags);
	M_buf_add_byte(buf, '}');

	return M_TRUE;
}

static M_bool M_json_write_node_array(const M_json_node_t *node, M_buf_t *buf, size_t *depth, M_uint32 flags)
{
	size_t len;
	size_t i;

	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_ARRAY)
		return M_FALSE;

	M_buf_add_byte(buf, '[');
	M_json_write_newline(buf, flags);
	(*depth)++;

	len = M_json_array_len(node);
	for (i=0; i<len; i++) {
		M_json_write_depth(buf, depth, flags);
		M_json_write_node(M_json_array_at(node, i), buf, depth, flags);
		if (i != len-1) {
			M_buf_add_byte(buf, ',');
		}
		M_json_write_newline(buf, flags);
	}

	(*depth)--;
	M_json_write_depth(buf, depth, flags);
	M_buf_add_byte(buf, ']');

	return M_TRUE;
}

static M_bool M_json_write_node_string(const M_json_node_t *node, M_buf_t *buf, M_uint32 flags)
{
	const char *p;
	char        uchr[8];
	char        c;
	M_uint32    cp;
	size_t      len;
	size_t      i;

	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_STRING)
		return M_FALSE;

	M_buf_add_byte(buf, '"');
	len = M_str_len(node->data.json_string);
	for (i=0; i<len; i++) {
		c = node->data.json_string[i];
		switch (c) {
			case '\b':
				M_buf_add_str(buf, "\\b");
				break;
			case '\f':
				M_buf_add_str(buf, "\\f");
				break;
			case '\n':
				M_buf_add_str(buf, "\\n");
				break;
			case '\r':
				M_buf_add_str(buf, "\\r");
				break;
			case '\t':
				M_buf_add_str(buf, "\\t");
				break;
			case '/':
				M_buf_add_str(buf, "\\/");
				break;
			case '"':
				/* fall-thru */
				/* We have a " in the middle of a string we're quoting.
				 * This will end up writing \" to escape it. */
			case '\\':
				M_buf_add_byte(buf, '\\');
				/* fall-thru */
				/* We have a \ in the middle of a string we're quoting.
				 * This will end up writing \\ to escape it. */
			default:
				if ((unsigned char)c < 32) {
					/* Control character. */
					if (flags & M_JSON_WRITER_REPLACE_BAD_CHARS) {
						M_buf_add_byte(buf, '?');
					} else {
						return M_FALSE;
					}
				} else if ((unsigned char)c > 127) {
					if (M_utf8_get_cp(node->data.json_string+i, &cp, &p) != M_UTF8_ERROR_SUCCESS) {
						if (flags & M_JSON_WRITER_REPLACE_BAD_CHARS) {
							M_buf_add_byte(buf, '?');
						} else {
							return M_FALSE;
						}
						break;
					}

					if (flags & M_JSON_WRITER_DONT_ENCODE_UNICODE) {
						M_buf_add_bytes(buf, node->data.json_string+i, (size_t)(p - (node->data.json_string+i)));
					} else {
						M_buf_add_str(buf, "\\u");
						M_snprintf(uchr, sizeof(uchr), "%04X", cp);
						M_buf_add_str(buf, uchr);
					}
					/* advance i to the end of the bytes read. Back off 1 because
					 * when we come back around to the start of the loop it will
					 * move one forward. This needs to be the byte before the next
					 * one that will be processed. */
					i += (size_t)(p - (node->data.json_string+i)-1);
				} else {
					M_buf_add_byte(buf, (unsigned char)c);
				}
		}
	}

	M_buf_add_byte(buf, '"');
	return M_TRUE;
}

static M_bool M_json_write_node_integer(const M_json_node_t *node, M_buf_t *buf)
{
	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_INTEGER)
		return M_FALSE;
	M_buf_add_int(buf, node->data.json_integer);
	return M_TRUE;
}

static M_bool M_json_write_node_decimal(const M_json_node_t *node, M_buf_t *buf) 
{
	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_DECIMAL)
		return M_FALSE;
	return M_buf_add_decimal(buf, &(node->data.json_decimal), M_FALSE, -1, 0);
}

static M_bool M_json_write_node_bool(const M_json_node_t *node, M_buf_t *buf)
{
	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_BOOL)
		return M_FALSE;

	if (node->data.json_bool) {
		M_buf_add_str(buf, "true");
	} else {
		M_buf_add_str(buf, "false");
	}
	return M_TRUE;
}

static M_bool M_json_write_node_null(const M_json_node_t *node, M_buf_t *buf)
{
	if (buf == NULL || node == NULL || node->type != M_JSON_TYPE_NULL)
		return M_FALSE;

	M_buf_add_str(buf, "null");
	return M_TRUE;
}

static M_bool M_json_write_node(const M_json_node_t *node, M_buf_t *buf, size_t *depth, M_uint32 flags)
{
	if (buf == NULL || node == NULL)
		return M_FALSE;

	switch (node->type) {
		case M_JSON_TYPE_OBJECT:
			return M_json_write_node_object(node, buf, depth, flags);
		case M_JSON_TYPE_ARRAY:
			return M_json_write_node_array(node, buf, depth, flags);
		case M_JSON_TYPE_STRING:
			return M_json_write_node_string(node, buf, flags);
		case M_JSON_TYPE_INTEGER:
			return M_json_write_node_integer(node, buf);
		case M_JSON_TYPE_DECIMAL:
			return M_json_write_node_decimal(node, buf);
		case M_JSON_TYPE_BOOL:
			return M_json_write_node_bool(node, buf);
		case M_JSON_TYPE_NULL:
			return M_json_write_node_null(node, buf);
		default:
			return M_FALSE;
	}

	return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_json_write(const M_json_node_t *node, M_uint32 flags, size_t *len)
{
	M_buf_t *buf;
	size_t   depth = 0;

	if (len != NULL)
		*len = 0;
	
	if (node == NULL)
		return NULL;

	buf = M_buf_create();
	if (!M_json_write_node(node, buf, &depth, flags)) {
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish_str(buf, len);
}

M_fs_error_t M_json_write_file(const M_json_node_t *node, const char *path, M_uint32 flags)
{
	char         *out;
	M_fs_error_t  res;

	out = M_json_write(node, flags, NULL);
	if (out == NULL)
		return M_FS_ERROR_INVALID;

	res = M_fs_file_write_bytes(path, (unsigned char *)out, 0, M_FS_FILE_MODE_OVERWRITE, NULL);

	M_free(out);
	return res;
}
