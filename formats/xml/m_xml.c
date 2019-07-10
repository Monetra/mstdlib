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
#include "m_defs_int.h"
#include "xml/m_xml_entities.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_xml_node {
	M_xml_node_type_t      type;       /*!< Type of node. */
	M_xml_node_t          *parent;     /*!< Parent this node belongs to. */

	union {
		struct {
			M_list_t      *children;   /*!< List of child nodes. */
		} doc;
		struct {
			char          *name;       /*!< Tag name. */
			M_list_t      *children;   /*!< List of child nodes. */
			M_hash_dict_t *attributes; /*!< Dictionary holding attributes. */
		} element;
		struct {
			char          *name;       /*!< Tag name. */
			M_hash_dict_t *attributes; /*!< Dictionary holding attributes. */
			char          *tag_data;   /*!< Textual data within the tag. */
		} processing_instruction;
		struct {
			char          *name;       /*!< Tag name. */
			char          *tag_data;   /*!< Textual data within the tag. */
		} declaration;
		struct {
			char          *text;       /*!< Textual data. */
		} text;
		struct {
			char          *tag_data;   /*!< Textual data within the tag. */
		} comment;
	} d;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_list_t *M_xml_get_childen(const M_xml_node_t *node)
{
	if (node == NULL)
		return NULL;

	switch (M_xml_node_type(node)) {
		case M_XML_NODE_TYPE_DOC:
			return node->d.doc.children;
		case M_XML_NODE_TYPE_ELEMENT:
			return node->d.element.children;
		default:
			return NULL;
	}

	return NULL;
}

static M_hash_dict_t *M_xml_get_attributes(const M_xml_node_t *node)
{
	if (node == NULL)
		return NULL;

	switch (M_xml_node_type(node)) {
		case M_XML_NODE_TYPE_ELEMENT:
			return node->d.element.attributes;
		case M_XML_NODE_TYPE_PROCESSING_INSTRUCTION:
			return node->d.processing_instruction.attributes;
		default:
			return NULL;
	}

	return NULL;
}

static char **M_xml_get_name(const M_xml_node_t *node)
{
	M_xml_node_t *n;

	if (node == NULL)
		return NULL;

	n = M_CAST_OFF_CONST(M_xml_node_t *, node);
	switch (M_xml_node_type(n)) {
		case M_XML_NODE_TYPE_ELEMENT:
			return &(n->d.element.name);
			break;
		case M_XML_NODE_TYPE_PROCESSING_INSTRUCTION:
			return &(n->d.processing_instruction.name);
		case M_XML_NODE_TYPE_DECLARATION:
			return &(n->d.declaration.name);
		default:
			return NULL;
	}

	return NULL;
}

static char **M_xml_get_tag_data(const M_xml_node_t *node)
{
	M_xml_node_t *n;

	if (node == NULL)
		return NULL;

	n = M_CAST_OFF_CONST(M_xml_node_t *, node);
	switch (M_xml_node_type(n)) {
		case M_XML_NODE_TYPE_PROCESSING_INSTRUCTION:
			return &(n->d.processing_instruction.tag_data);
		case M_XML_NODE_TYPE_DECLARATION:
			return &(n->d.declaration.tag_data);
		case M_XML_NODE_TYPE_COMMENT:
			return &(n->d.comment.tag_data);
		default:
			return NULL;
	}

	return NULL;
}

static void M_xml_node_destroy_int(M_xml_node_t *node)
{
	M_xml_node_type_t type;

	if (node == NULL)
		return;

	type = M_xml_node_type(node);
	switch (type) {
		case M_XML_NODE_TYPE_UNKNOWN:
			break;
		case M_XML_NODE_TYPE_DOC:
			M_list_destroy(node->d.doc.children, M_TRUE);
			break;
		case M_XML_NODE_TYPE_ELEMENT:
			M_free(node->d.element.name);
			M_list_destroy(node->d.element.children, M_TRUE);
			M_hash_dict_destroy(node->d.element.attributes);
			break;
		case M_XML_NODE_TYPE_PROCESSING_INSTRUCTION:
			M_free(node->d.processing_instruction.name);
			M_hash_dict_destroy(node->d.processing_instruction.attributes);
			break;
		case M_XML_NODE_TYPE_DECLARATION:
			M_free(node->d.declaration.name);
			M_free(node->d.declaration.tag_data);
			break;
		case M_XML_NODE_TYPE_TEXT:
			M_free(node->d.text.text);
			break;
		case M_XML_NODE_TYPE_COMMENT:
			M_free(node->d.comment.tag_data);
			break;

	}

	node->type = M_XML_NODE_TYPE_UNKNOWN;
	M_free(node);
}

static void M_xml_node_destroy_vp(void *node)
{
	M_xml_node_destroy_int(node);
}

/*! Create an empty node of a given type. */
static M_xml_node_t *M_xml_node_create(M_xml_node_type_t type, M_xml_node_t *parent)
{
	M_xml_node_t            *node;
	struct M_list_callbacks  list_callbacks;

	if (parent != NULL) {
		/* Check that we can add children to the parent. */
		switch (M_xml_node_type(parent)) {
			case M_XML_NODE_TYPE_DOC:
			case M_XML_NODE_TYPE_ELEMENT:
				break;
			default:
				return NULL;
		}
	}

	M_mem_set(&list_callbacks, 0, sizeof(list_callbacks));
	list_callbacks.value_free = M_xml_node_destroy_vp;

	node       = M_malloc_zero(sizeof(*node));
	node->type = type;

	switch (type) {
		case M_XML_NODE_TYPE_DOC:
			node->d.doc.children = M_list_create(&list_callbacks, M_LIST_NONE);
			break;
		case M_XML_NODE_TYPE_ELEMENT:
			node->d.element.children   = M_list_create(&list_callbacks, M_LIST_NONE);
			node->d.element.attributes = M_hash_dict_create(4, 75, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_CASECMP);
			break;
		case M_XML_NODE_TYPE_PROCESSING_INSTRUCTION:
			node->d.processing_instruction.attributes = M_hash_dict_create(4, 75, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_CASECMP);
			break;
		case M_XML_NODE_TYPE_DECLARATION:
		case M_XML_NODE_TYPE_TEXT:
		case M_XML_NODE_TYPE_COMMENT:
			break;
		default:
			M_free(node);
			return NULL;
	}

	if (parent != NULL) {
		if (!M_xml_node_insert_node(parent, node)) {
			M_xml_node_destroy(node);
			return NULL;
		}
	}

	return node;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_xml_node_t *M_xml_create_doc(void)
{
	return M_xml_node_create(M_XML_NODE_TYPE_DOC, NULL);
}

M_xml_node_t *M_xml_create_element(const char *name, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_ELEMENT, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_name(node, name)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_element_with_text(const char *name, const char *text, size_t max_len, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_create_element(name, parent);
	if (node == NULL)
		return NULL;

	if (M_xml_create_text(text, max_len, node) == NULL) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_element_with_num(const char *name, M_int64 num, size_t max_len, M_xml_node_t *parent)
{
	char num_str[64];

	M_snprintf(num_str, sizeof(num_str), "%lld", num);

	return M_xml_create_element_with_text(name, num_str, max_len, parent);
}

M_xml_node_t *M_xml_create_text(const char *text, size_t max_len, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_TEXT, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_text(node, text, max_len)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_xml_declaration(const char *encoding, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_PROCESSING_INSTRUCTION, parent);
	if (node == NULL)
		return NULL;

	if (encoding == NULL || *encoding == '\0') {
		encoding = "UTF-8";
	}

	if (!M_xml_node_set_name(node, "xml")                                    ||
		!M_xml_node_insert_attribute(node, "version", "1.0", 0, M_FALSE)     ||
		!M_xml_node_insert_attribute(node, "encoding", encoding, 0, M_FALSE))
	{
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_declaration(const char *name, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_DECLARATION, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_name(node, name)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_declaration_with_tag_data(const char *name, const char *data, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_create_declaration(name, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_tag_data(node, data)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_processing_instruction(const char *name, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_PROCESSING_INSTRUCTION, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_name(node, name)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

M_xml_node_t *M_xml_create_comment(const char *comment, M_xml_node_t *parent)
{
	M_xml_node_t *node;

	node = M_xml_node_create(M_XML_NODE_TYPE_COMMENT, parent);
	if (node == NULL)
		return NULL;

	if (!M_xml_node_set_tag_data(node, comment)) {
		M_xml_node_destroy(node);
		return NULL;
	}

	return node;
}

void M_xml_node_destroy(M_xml_node_t *node)
{
	if (node == NULL)
		return;
	M_xml_take_from_parent(node);
	M_xml_node_destroy_int(node);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define ERRCASE(x) case x: ret = #x; break

const char *M_xml_errcode_to_str(M_xml_error_t err)
{
	const char *ret = "unknown";

	switch (err) {
		ERRCASE(M_XML_ERROR_SUCCESS);
		ERRCASE(M_XML_ERROR_GENERIC);
		ERRCASE(M_XML_ERROR_MISUSE);
		ERRCASE(M_XML_ERROR_ATTR_EXISTS);
		ERRCASE(M_XML_ERROR_NO_ELEMENTS);
		ERRCASE(M_XML_ERROR_INVALID_START_TAG);
		ERRCASE(M_XML_ERROR_INVALID_CHAR_IN_START_TAG);
		ERRCASE(M_XML_ERROR_EMPTY_START_TAG);
		ERRCASE(M_XML_ERROR_MISSING_DECLARATION_NAME);
		ERRCASE(M_XML_ERROR_INELIGIBLE_FOR_CLOSE);
		ERRCASE(M_XML_ERROR_UNEXPECTED_CLOSE);
		ERRCASE(M_XML_ERROR_MISSING_CLOSE_TAG);
		ERRCASE(M_XML_ERROR_MISSING_PROCESSING_INSTRUCTION_END);
		ERRCASE(M_XML_ERROR_EXPECTED_END);
	}

	return ret;
}

M_xml_node_type_t M_xml_node_type(const M_xml_node_t *node)
{
	if (node == NULL)
		return M_XML_NODE_TYPE_UNKNOWN;
	return node->type;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_xml_node_t *M_xml_node_parent(const M_xml_node_t *node)
{
	if (node == NULL)
		return NULL;
	return node->parent;
}

void M_xml_take_from_parent(M_xml_node_t *node)
{
	M_list_t *children;
	size_t    idx;

	if (node == NULL || node->parent == NULL)
		return;

	children = M_xml_get_childen(node->parent);
	if (children == NULL)
		return;

	/* Remove the node from it's parent. */
	if (M_list_index_of(children, node, M_LIST_MATCH_PTR, &idx))
		M_list_take_at(children, idx);
	node->parent = NULL;
}

M_bool M_xml_node_insert_node(M_xml_node_t *parent, M_xml_node_t *child)
{
	return M_xml_node_insert_node_at(parent, child, M_xml_node_num_children(parent));
}

M_bool M_xml_node_insert_node_at(M_xml_node_t *parent, M_xml_node_t *child, size_t idx)
{
	M_list_t *children;

	if (parent == NULL || child == NULL || M_xml_node_type(child) == M_XML_NODE_TYPE_DOC || M_xml_node_parent(child) != NULL)
		return M_FALSE;

	children = M_xml_get_childen(parent);
	if (children == NULL)
		return M_FALSE;

	if (!M_list_insert_at(children, child, idx))
		return M_FALSE;

	child->parent = parent;
	return M_TRUE;
}

size_t M_xml_node_num_children(const M_xml_node_t *node)
{
	M_list_t *children;

	if (node == NULL)
		return 0;

	children = M_xml_get_childen(node);
	if (children == NULL)
		return 0;
	return M_list_len(children);
}

M_xml_node_t *M_xml_node_child(const M_xml_node_t *node, size_t idx)
{
	M_list_t   *children;
	const void *value;

	if (node == NULL)
		return NULL;

	children = M_xml_get_childen(node);
	if (children == NULL)
		return NULL;

	value = M_list_at(children, idx);
	return M_CAST_OFF_CONST(M_xml_node_t *, value);
}

M_xml_node_t *M_xml_node_sibling(const M_xml_node_t *node, M_bool after)
{
	M_xml_node_t *parent;
	size_t        num_children;
	size_t        i;

	if (node == NULL)
		return NULL;

	parent = M_xml_node_parent(node);
	if (parent == NULL)
		return NULL;

	/* Figure out the position of the node within the parent. */
	num_children = M_xml_node_num_children(parent);
	for (i=0; i<num_children; i++) {
		if (M_xml_node_child(parent, i) == node) {
			break;
		}
	}

	/* If we're looking before or after we can't be at an index on the end for the
 	 * direction of the sibling we want. */
	if (after && i < num_children-1)
		return M_xml_node_child(parent, i+1);
	if (!after && i > 0)
		return M_xml_node_child(parent, i-1);

	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_xml_node_set_name(M_xml_node_t *node, const char *name)
{
	char **node_name;

	if (node == NULL || name == NULL || *name == '\0')
		return M_FALSE;

	node_name = M_xml_get_name(node);
	if (node_name == NULL)
		return M_FALSE;

	/* XXX: Validate the name is valid format. */

	M_free(*node_name);
	*node_name = M_strdup(name);

	return M_TRUE;
}

const char *M_xml_node_name(const M_xml_node_t *node)
{
	char **node_name;

	if (node == NULL)
		return NULL;

	node_name = M_xml_get_name(node);
	if (node_name == NULL)
		return NULL;

	return *node_name;
}

M_bool M_xml_node_set_text(M_xml_node_t *node, const char *text, size_t max_len)
{
	char *temp;

	if (node == NULL)
		return M_FALSE;
	
	if (M_xml_node_type(node) != M_XML_NODE_TYPE_TEXT)
		return M_FALSE;

	if (max_len != 0) {
		temp = M_xml_entities_encode(text, M_str_len(text));
		if (M_str_len(temp) > max_len) {
			M_free(temp);
			return M_FALSE;
		}
		M_free(temp);
	}

	M_free(node->d.text.text);
	node->d.text.text = M_strdup(text);

	return M_TRUE;
}

const char *M_xml_node_text(const M_xml_node_t *node)
{
	if (node == NULL)
		return NULL;

	if (M_xml_node_type(node) != M_XML_NODE_TYPE_TEXT)
		return NULL;
	return node->d.text.text;
}

M_bool M_xml_node_set_tag_data(M_xml_node_t *node, const char *data)
{
	char **tag_data;

	if (node == NULL)
		return M_FALSE;

	tag_data = M_xml_get_tag_data(node);
	if (tag_data == NULL)
		return M_FALSE;
	
	M_free(*tag_data);
	*tag_data = M_strdup(data);

	return M_TRUE;
}

const char *M_xml_node_tag_data(const M_xml_node_t *node)
{
	char **tag_data;

	if (node == NULL) {
		return NULL;
	}

	tag_data = M_xml_get_tag_data(node);
	if (tag_data == NULL)
		return NULL;
	return *tag_data;
}

M_bool M_xml_node_insert_attribute(M_xml_node_t *node, const char *key, const char *val, size_t max_len, M_bool overwrite)
{
	M_hash_dict_t *attributes;
	char          *temp;

	if (node == NULL || key == NULL || *key == '\0' || val == NULL)
		return M_FALSE;

	attributes = M_xml_get_attributes(node);
	if (attributes == NULL)
		return M_FALSE;

	/* XXX: Validate the key is valid format. */

	if (!overwrite && M_hash_dict_get(attributes, key, NULL))
		return M_FALSE;

	if (max_len != 0) {
		temp = M_xml_attribute_encode(val, M_str_len(val));
		if (M_str_len(temp) > max_len) {
			M_free(temp);
			return M_FALSE;
		}
		M_free(temp);
	}

	M_hash_dict_insert(attributes, key, val);
	return M_TRUE;
}

M_bool M_xml_node_remove_attribute(M_xml_node_t *node, const char *key)
{
	M_hash_dict_t *attributes;

	if (node == NULL || key == NULL || *key == '\0')
		return M_FALSE;

	attributes = M_xml_get_attributes(node);
	if (attributes == NULL)
		return M_FALSE;

	return M_hash_dict_remove(attributes, key);
}

M_list_str_t *M_xml_node_attribute_keys(const M_xml_node_t *node)
{
	M_hash_dict_t      *attributes;
	M_hash_dict_enum_t *dictenum;
	M_list_str_t       *keys      = NULL;
	const char         *key;

	if (node == NULL)
		return NULL;

	attributes = M_xml_get_attributes(node);
	if (attributes == NULL)
		return NULL;

	keys = M_list_str_create(M_LIST_STR_NONE);
	M_hash_dict_enumerate(attributes, &dictenum);
	while (M_hash_dict_enumerate_next(attributes, dictenum, &key, NULL)) {
		M_list_str_insert(keys, key);
	}
	M_hash_dict_enumerate_free(dictenum);

	return keys;
}

const M_hash_dict_t *M_xml_node_attributes(const M_xml_node_t *node)
{
	if (node == NULL)
		return NULL;
	return M_xml_get_attributes(node);
}

const char *M_xml_node_attribute(const M_xml_node_t *node, const char *key)
{
	M_hash_dict_t *attributes;

	if (node == NULL || key == NULL || *key == '\0')
		return NULL;

	attributes = M_xml_get_attributes(node);
	if (attributes == NULL)
		return NULL;

	return M_hash_dict_get_direct(attributes, key);
}
