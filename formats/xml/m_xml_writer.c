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
#include <mstdlib/mstdlib_formats.h>
#include "xml/m_xml_entities.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_xml_write_node(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_xml_write_node_indent(M_buf_t *buf, M_uint32 flags, size_t depth)
{
	if (depth == 0)
		return;

	if (flags & M_XML_WRITER_PRETTYPRINT_SPACE) {
		M_buf_add_fill(buf, ' ', depth*2);
	} else if (flags & M_XML_WRITER_PRETTYPRINT_TAB) {
		M_buf_add_fill(buf, '\t', depth);
	}
}

static void M_xml_write_node_newline(M_buf_t *buf, M_uint32 flags)
{
	if (!(flags & (M_XML_WRITER_PRETTYPRINT_SPACE|M_XML_WRITER_PRETTYPRINT_TAB)))
		return;
	M_buf_add_byte(buf, '\n');
}

/* Write the text for a text node. */
static M_bool M_xml_write_node_text(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	M_xml_node_t *parent;
	const char   *const_temp;
	char         *temp       = NULL;
	
	if (type != M_XML_NODE_TYPE_TEXT)
		return M_TRUE;

	parent = M_xml_node_parent(node);
	if (M_xml_node_num_children(parent) > 1 &&
		M_xml_node_type(M_xml_node_sibling(node, M_FALSE)) != M_XML_NODE_TYPE_TEXT)
	{
		M_xml_write_node_indent(buf, flags, depth);
	}

	const_temp = M_xml_node_text(node);
	if (flags & M_XML_WRITER_DONT_ENCODE_TEXT) {
		M_buf_add_str(buf, const_temp);
	} else {
		temp = M_xml_entities_encode(const_temp, M_str_len(const_temp));
		M_buf_add_str(buf, temp);
		M_free(temp);
	}

	if (M_xml_node_num_children(parent) > 1 &&
		M_xml_node_type(M_xml_node_sibling(node, M_TRUE)) != M_XML_NODE_TYPE_TEXT)
	{
		M_xml_write_node_newline(buf, flags);
	}

	return M_TRUE;
}

/* Write the opening characters for the tag, E.g. <, <?, <!, <!-- */
static M_bool M_xml_write_node_tag_open_start(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	(void)node;

	if (!(type == M_XML_NODE_TYPE_ELEMENT || type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION || type == M_XML_NODE_TYPE_DECLARATION || type == M_XML_NODE_TYPE_COMMENT))
		return M_TRUE;

	M_xml_write_node_indent(buf, flags, depth);

	M_buf_add_byte(buf, '<');
	if (type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION) {
		M_buf_add_byte(buf, '?');
	} else if (type == M_XML_NODE_TYPE_DECLARATION) {
		M_buf_add_byte(buf, '!');
	} else if (type == M_XML_NODE_TYPE_COMMENT) {
		M_buf_add_bytes(buf, "!--", 3);
	}

	return M_TRUE;
}

/* Write the tag name. */
static M_bool M_xml_write_node_tag_name(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	const char *const_temp;
	char       *temp;

	(void)depth;

	if (!(type == M_XML_NODE_TYPE_ELEMENT || type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION || type == M_XML_NODE_TYPE_DECLARATION))
		return M_TRUE;

	const_temp = M_xml_node_name(node);
	if (flags &  M_XML_WRITER_LOWER_TAGS) {
		temp = M_strdup_lower(const_temp);
		M_buf_add_str(buf, temp);
		M_free(temp);
	} else {
		M_buf_add_str(buf, const_temp);
	}

	return M_TRUE;
}

/* These are the three attributes that are part of the xml declaration.
 * They are in the order they should appear in the declaration. */
static struct {
	const char *key;
} M_xml_write_declaration_attributes[] = {
	{ "version"    },
	{ "encoding"   },
	{ "standalone" },
	{ NULL }
};

/* Write the tag attributes. */
static M_bool M_xml_write_node_tag_open_attributes(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	const M_hash_dict_t *attributes;
	M_hash_dict_enum_t  *hashenum;
	const char          *key;
	const char          *val;
	char                *temp;
	size_t               i;
	M_bool               is_dec = M_FALSE; /* Is the element an xml declaration? */

	(void)depth;
	
	if (!(type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION || type == M_XML_NODE_TYPE_ELEMENT))
		return M_TRUE;

	/* XML declaration has specific requirements for attribute order. It must be "version encoding standalone". */
	if (type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION && M_str_eq(M_xml_node_name(node), "xml")) {
		for (i=0; M_xml_write_declaration_attributes[i].key!=NULL; i++) {
			key = M_xml_write_declaration_attributes[i].key;
			val = M_xml_node_attribute(node, key);
			if (val == NULL) {
				continue;
			}

			M_buf_add_byte(buf, ' ');

			/* We should use the case stored in the hashtable but that's not really proper so we're going
 			 * to force the lowered version. */
			M_buf_add_str(buf, key);

			M_buf_add_bytes(buf, "=\"", 2);
			if (flags & M_XML_WRITER_DONT_ENCODE_ATTRS) {
				M_buf_add_str(buf, val);
			} else {
				temp = M_xml_attribute_encode(val, M_str_len(val));
				M_buf_add_str(buf, temp);
				M_free(temp);
			}
			M_buf_add_byte(buf, '"');
		}
		is_dec = M_TRUE;
	}

	/* Write out all attributes. */
	attributes = M_xml_node_attributes(node);
	M_hash_dict_enumerate(attributes, &hashenum);
	while (M_hash_dict_enumerate_next(attributes, hashenum, &key, &val)) {
		/* When the element is an xml declaration we need to skip some attributes because they were written earlier. */
		if (is_dec &&
			(M_str_caseeq(key, "version") || M_str_caseeq(key, "encoding") || M_str_caseeq(key, "standalone")))
		{
			continue;
		}

		M_buf_add_byte(buf, ' ');

		if (flags & M_XML_WRITER_LOWER_ATTRS) {
			temp = M_strdup_lower(key);
			M_buf_add_str(buf, temp);
			M_free(temp);
		} else {
			M_buf_add_str(buf, key);
		}

		M_buf_add_bytes(buf, "=\"", 2);
		if (flags & M_XML_WRITER_DONT_ENCODE_ATTRS) {
			M_buf_add_str(buf, val);
		} else {
			temp = M_xml_attribute_encode(val, M_str_len(val));
			M_buf_add_str(buf, temp);
			M_free(temp);
		}
		M_buf_add_byte(buf, '"');
	}
	M_hash_dict_enumerate_free(hashenum);

	return M_TRUE;
}

/* Write tag data. Data within the tag that is not the name or attributes. E.g. Comment text. */
static M_bool M_xml_write_node_tag_open_tag_data(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	const char *const_temp;

	(void)flags;
	(void)depth;

	if (!(type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION || type == M_XML_NODE_TYPE_DECLARATION || type == M_XML_NODE_TYPE_COMMENT))
		return M_TRUE;

	const_temp = M_xml_node_tag_data(node);
	if (const_temp == NULL || *const_temp == '\0')
		return M_TRUE;

	M_buf_add_byte(buf, ' ');
	M_buf_add_str(buf, const_temp);

	return M_TRUE;
}

/* Write the end part of the opening tag. E.g. >, /> */
static M_bool M_xml_write_node_tag_open_end(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	size_t num_children;

	(void)depth;

	if (!(type == M_XML_NODE_TYPE_ELEMENT || type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION || type == M_XML_NODE_TYPE_DECLARATION || type == M_XML_NODE_TYPE_COMMENT))
		return M_TRUE;

	if (type == M_XML_NODE_TYPE_PROCESSING_INSTRUCTION) {
		M_buf_add_byte(buf, '?');
	} else if (type == M_XML_NODE_TYPE_COMMENT) {
		M_buf_add_bytes(buf, " --", 3);
	} else if (type != M_XML_NODE_TYPE_DECLARATION && M_xml_node_num_children(node) == 0) {
		if (flags & M_XML_WRITER_SELFCLOSE_SPACE) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_byte(buf, '/');
	}
	M_buf_add_byte(buf, '>');

	num_children = M_xml_node_num_children(node);
	if (num_children != 1 || (num_children != 0 && M_xml_node_type(M_xml_node_child(node, 0)) != M_XML_NODE_TYPE_TEXT))
		M_xml_write_node_newline(buf, flags);

	return M_TRUE;
}

/* Write the node's children. */
static M_bool M_xml_write_node_children(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	M_xml_node_t *child;
	size_t        len;
	size_t        i;
	size_t        mydepth;

	if (!(type == M_XML_NODE_TYPE_DOC || type == M_XML_NODE_TYPE_ELEMENT))
		return M_TRUE;

	mydepth = depth;
	if (type != M_XML_NODE_TYPE_DOC)
		mydepth = depth+1;

	len = M_xml_node_num_children(node);
	for (i=0; i<len; i++) {
		child = M_xml_node_child(node, i);
		if (!M_xml_write_node(buf, flags, mydepth, child, M_xml_node_type(child))) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}

/* Write the close tag is necessary. E.g. </name> */
static M_bool M_xml_write_node_tag_close(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	const char *const_temp;
	char       *temp;
	size_t      num_children;

	num_children = M_xml_node_num_children(node);
	if (type != M_XML_NODE_TYPE_ELEMENT || num_children == 0)
		return M_TRUE;

	/* Only append tab to the depth if we didn't simply output text */
	if (num_children != 1 || M_xml_node_type(M_xml_node_child(node, 0)) != M_XML_NODE_TYPE_TEXT)
		M_xml_write_node_indent(buf, flags, depth);

	M_buf_add_bytes(buf, "</", 2);

	const_temp = M_xml_node_name(node);
	if (flags &  M_XML_WRITER_LOWER_TAGS) {
		temp = M_strdup_lower(const_temp);
		M_buf_add_str(buf, temp);
		M_free(temp);
	} else {
		M_buf_add_str(buf, const_temp);
	}

	M_buf_add_byte(buf, '>');
	if (M_xml_node_parent(node) != NULL && M_xml_node_type(M_xml_node_parent(node)) != M_XML_NODE_TYPE_DOC)
		M_xml_write_node_newline(buf, flags);

	return M_TRUE;
}

static struct {
	M_bool (*func)(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type);
} M_xml_write_node_funcs[] = {
	{ M_xml_write_node_text                },
	{ M_xml_write_node_tag_open_start      },
	{ M_xml_write_node_tag_name            },
	{ M_xml_write_node_tag_open_attributes },
	{ M_xml_write_node_tag_open_tag_data   },
	{ M_xml_write_node_tag_open_end        },
	{ M_xml_write_node_children            },
	{ M_xml_write_node_tag_close           },
	{ NULL },
};

static M_bool M_xml_write_node(M_buf_t *buf, M_uint32 flags, size_t depth, const M_xml_node_t *node, M_xml_node_type_t type)
{
	size_t i;

	if (flags & M_XML_WRITER_IGNORE_COMMENTS && type == M_XML_NODE_TYPE_COMMENT)
		return M_TRUE;

	for (i=0; M_xml_write_node_funcs[i].func!=NULL; i++) {
		if (!M_xml_write_node_funcs[i].func(buf, flags, depth, node, type)) {
			return M_FALSE;
		}
	}
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_xml_write(const M_xml_node_t *node, M_uint32 flags, size_t *len)
{
	M_buf_t *buf;

	if (len != NULL)
		*len = 0;

	if (node == NULL)
		return NULL;

	buf = M_buf_create();
	if (!M_xml_write_buf(buf, node, flags)) {
		M_buf_cancel(buf);
		return NULL;
	}

	return M_buf_finish_str(buf, len);
}

M_bool M_xml_write_buf(M_buf_t *buf, const M_xml_node_t *node, M_uint32 flags)
{
	size_t start_len = M_buf_len(buf);

	if (buf == NULL || node == NULL) {
		return M_FALSE;
	}

	if (!M_xml_write_node(buf, flags, 0, node, M_xml_node_type(node))) {
		M_buf_truncate(buf, start_len);
		return M_FALSE;
	}

	return M_TRUE;
}

M_fs_error_t M_xml_write_file(const M_xml_node_t *node, const char *path, M_uint32 flags)
{
	char         *out;
	M_fs_error_t  res;

	out = M_xml_write(node, flags, NULL);
	if (out == NULL)
		return M_FS_ERROR_INVALID;

	res = M_fs_file_write_bytes(path, (unsigned char *)out, 0, M_FS_FILE_MODE_OVERWRITE, NULL);

	M_free(out);
	return res;
}
