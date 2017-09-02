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
#include "ini/m_ini_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* An element. Can define multiple types but a single element can only be one type. */
struct M_ini_element {
	M_ini_element_type_t type;
	/* Save space. */
	union {
		struct {
			char *val;
		} comment;
		struct {
			char *name;
		} section;
		struct {
			char *key;
			char *val;
			char *comment;
		} kv;
	} data;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_element_t *M_ini_element_create(M_ini_element_type_t type)
{
	M_ini_element_t *elem;
	elem = M_malloc_zero(sizeof(*elem));
	elem->type = type;
	return elem;
}

static M_bool M_ini_element_destroy_comment(M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_COMMENT) {
		return M_FALSE;
	}

	M_free(elem->data.comment.val);
	elem->data.comment.val = NULL;
	
	return M_TRUE;
}

static M_bool M_ini_element_destroy_section(M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_SECTION) {
		return M_FALSE;
	}

	M_free(elem->data.section.name);
	elem->data.section.name = NULL;

	return M_TRUE;
}

static M_bool M_ini_element_destroy_kv(M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV) {
		return M_FALSE;
	}

	M_free(elem->data.kv.key);
	elem->data.kv.key = NULL;
	M_free(elem->data.kv.val);
	elem->data.kv.val = NULL;
	M_free(elem->data.kv.comment);
	elem->data.kv.comment = NULL;

	return M_TRUE;
}

static struct {
	M_bool (*destroy)(M_ini_element_t *elem);
} M_ini_element_destroy_types[] = {
	/* Ordered based on how often a type should appear in an ini file. Putting types
 	 * that appear most often at the top. */
	{ M_ini_element_destroy_kv },
	{ M_ini_element_destroy_comment },
	{ M_ini_element_destroy_section },
	{ NULL }
};

void M_ini_element_destroy(M_ini_element_t *elem)
{
	size_t i;

	if (elem == NULL) {
		return;
	}

	/* We could do an if statement or a switch but this makes it easier to
 	 * add new types. */
	for (i=0; M_ini_element_destroy_types[i].destroy != NULL; i++) {
		if (M_ini_element_destroy_types[i].destroy(elem)) {
			break;
		}
	}
	elem->type = M_INI_ELEMENT_TYPE_UNKNOWN;

	M_free(elem);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_element_type_t M_ini_element_get_type(const M_ini_element_t *elem)
{
	if (elem == NULL)
		return M_INI_ELEMENT_TYPE_UNKNOWN;
	return elem->type;
}

const char *M_ini_element_comment_get_val(const M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_COMMENT)
		return NULL;
	return elem->data.comment.val;
}

const char *M_ini_element_section_get_name(const M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_SECTION)
		return NULL;
	return elem->data.section.name;
}

const char *M_ini_element_kv_get_key(const M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return NULL;
	return elem->data.kv.key;
}

const char *M_ini_element_kv_get_val(const M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return NULL;
	return elem->data.kv.val;
}

const char *M_ini_element_kv_get_comment(const M_ini_element_t *elem)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return NULL;
	return elem->data.kv.comment;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_ini_element_comment_set_val(M_ini_element_t *elem, const char *val)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_COMMENT)
		return M_FALSE;
	M_free(elem->data.comment.val);
	elem->data.comment.val = M_strdup(val);
	return M_TRUE;
}

M_bool M_ini_element_section_set_name(M_ini_element_t *elem, const char *name)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_SECTION)
		return M_FALSE;
	M_free(elem->data.section.name);
	elem->data.section.name = M_strdup(name);
	return M_TRUE;
}

M_bool M_ini_element_kv_set_key(M_ini_element_t *elem, const char *key)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return M_FALSE;
	M_free(elem->data.kv.key);
	elem->data.kv.key = M_strdup(key);
	return M_TRUE;
}

M_bool M_ini_element_kv_set_val(M_ini_element_t *elem, const char *val)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return M_FALSE;
	M_free(elem->data.kv.val);
	elem->data.kv.val = M_strdup(val);
	return M_TRUE;
}

M_bool M_ini_element_kv_set_comment(M_ini_element_t *elem, const char *comment)
{
	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV)
		return M_FALSE;
	M_free(elem->data.kv.comment);
	elem->data.kv.comment = M_strdup(comment);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_ini_element_kv_to_comment(M_ini_element_t *elem, const M_ini_settings_t *info)
{
	M_buf_t *buf;

	if (elem == NULL || elem->type != M_INI_ELEMENT_TYPE_KV) {
		return;
	}

	buf = M_buf_create();
	M_ini_writer_write_element_kv(elem, M_ini_element_kv_get_key(elem), info, buf);
	/* Remove the delim from the end that was added. */
	M_buf_truncate(buf, M_buf_len(buf)-1);

	/* We're change the element so we only want to destroy the data it holds and replace it with the new data. We
 	 * don't want to destroy the entire element. */
	M_ini_element_destroy_kv(elem);
	elem->type             = M_INI_ELEMENT_TYPE_COMMENT;
	elem->data.comment.val = M_buf_finish_str(buf, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_ini_element_t *M_ini_element_duplicate(const M_ini_element_t *elem)
{
	M_ini_element_t *dup_elem;

	if (elem == NULL)
		return NULL;

	dup_elem = M_ini_element_create(elem->type);
	switch (elem->type) {
		case M_INI_ELEMENT_TYPE_COMMENT:
			M_ini_element_comment_set_val(dup_elem, M_ini_element_comment_get_val(elem));
			break;
		case M_INI_ELEMENT_TYPE_SECTION:
			M_ini_element_section_set_name(dup_elem, M_ini_element_section_get_name(elem));
			break;
		case M_INI_ELEMENT_TYPE_KV:
			M_ini_element_kv_set_key(dup_elem, M_ini_element_kv_get_key(elem));
			M_ini_element_kv_set_val(dup_elem, M_ini_element_kv_get_val(elem));
			M_ini_element_kv_set_comment(dup_elem, M_ini_element_kv_get_comment(elem));
			break;
		case M_INI_ELEMENT_TYPE_EMPTY_LINE:
		case M_INI_ELEMENT_TYPE_UNKNOWN:
			break;
	}

	return dup_elem;
}
