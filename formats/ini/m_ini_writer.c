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
#include "ini/m_ini_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void M_ini_writer_write_line_ending(const M_ini_settings_t *info, M_buf_t *buf)
{
	const char *line_ending;

	line_ending = M_ini_settings_writer_get_line_ending(info);

	if (line_ending == NULL) {
		M_buf_add_byte(buf, M_ini_settings_get_element_delim_char(info));
	} else {
		M_buf_add_str(buf, line_ending);
	}
}

/* Return true if the entire section should be dropped otherwise false if it should be kept. */
static M_bool M_ini_writer_tree_prune_section_has_kv(M_ini_elements_t *section)
{
	size_t           i;
	size_t           len;
	M_ini_element_t *elem;

	len = M_ini_elements_len(section);
	for (i=0; i<len; i++) {
		elem = M_ini_elements_at(section, i);
		if (M_ini_element_get_type(elem) == M_INI_ELEMENT_TYPE_KV) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}

static void M_ini_writer_tree_prune_section_kvs(const char *sect_name, M_ini_elements_t *section, M_ini_kvs_t *kvs)
{
	M_ini_element_t *elem;
	M_list_u64_t    *remove_idx;
	char            *int_key;
	size_t           len;
	size_t           i;

	remove_idx = M_list_u64_create(M_LIST_U64_SORTDESC);

	/* Determine which indexes need to be removed. */
	len = M_ini_elements_len(section);
	for (i=0; i<len; i++) {
		elem = M_ini_elements_at(section, i);
		if (M_ini_element_get_type(elem) != M_INI_ELEMENT_TYPE_KV) {
			continue;
		}
		int_key = M_ini_full_key(sect_name, M_ini_element_kv_get_key(elem));
		if (!M_ini_kvs_has_key(kvs, int_key)) {
			M_list_u64_insert(remove_idx, i);
		}
		M_free(int_key);
	}

	/* Remove the indexes. */
	len = M_list_u64_len(remove_idx);
	for (i=0; i<len; i++) {
		M_ini_elements_remove_at(section, (size_t)M_list_u64_at(remove_idx, i));
	}

	M_list_u64_destroy(remove_idx);
}

static void M_ini_writer_tree_prune(M_ini_t *ini)
{
	size_t               i;
	size_t               len;
	void                *section;
	const char          *sect_name;
	M_hash_strvp_enum_t *sectenum;
	M_list_str_t        *prune_sections;

	prune_sections = M_list_str_create(M_LIST_STR_NONE);

	/* KV in root. */
	M_ini_writer_tree_prune_section_kvs(NULL, ini->elements, ini->kvs);

	/* KV in sections. */
	M_hash_strvp_enumerate(ini->sections, &sectenum);
	while (M_hash_strvp_enumerate_next(ini->sections, sectenum, &sect_name, &section)) {
		M_ini_writer_tree_prune_section_kvs(sect_name, section, ini->kvs);
		/* Check if the section has any kv elements. If not we should remove it. */
		if (M_ini_writer_tree_prune_section_has_kv(section)) {
			M_list_str_insert(prune_sections, sect_name);
		}
	}
	M_hash_strvp_enumerate_free(sectenum);

	/* Remove sections that have no kv. */
	len = M_list_str_len(prune_sections);
	for (i=0; i<len; i++) {
		M_hash_strvp_remove(ini->sections, M_list_str_at(prune_sections, i), M_TRUE);
	}

	M_list_str_destroy(prune_sections);
}

static void M_ini_writer_tree_update_kv_vals(M_ini_t *ini, M_ini_kvs_t *kvs, const M_ini_settings_t *info)
{
	void                *section;
	const char          *sect_name = NULL;
	const char          *val;
	M_hash_strvp_enum_t *sectenum;
	M_ini_element_t     *elem;
	M_list_u64_t        *remove_idx;
	char                *int_key;
	M_ini_multivals_t    multi_flag;
	M_bool               exists;
	size_t               val_len;
	size_t               len;
	size_t               i;
	size_t               j;

	multi_flag = M_ini_settings_writer_get_multivals_handling(info);
	section    = ini->elements;
	M_hash_strvp_enumerate(ini->sections, &sectenum);
	do {
		remove_idx = M_list_u64_create(M_LIST_U64_SORTDESC);

		len = M_ini_elements_len(section);
		for (i=0; i<len; i++) {
			elem = M_ini_elements_at(section, i);
			if (M_ini_element_get_type(elem) != M_INI_ELEMENT_TYPE_KV) {
				continue;
			}
			int_key = M_ini_full_key(sect_name, M_ini_element_kv_get_key(elem));

			val_len = M_ini_kvs_val_len(kvs, int_key);
			/* There are no values so this must be a multi-value key and we've already updated
 			 * all of the keys. We're going to remove this element since there is no
			 * value to update. */
			if (val_len == 0) {
				M_list_u64_insert(remove_idx, i);
				M_ini_kvs_remove(kvs, int_key);
			/* Either there is only one value so we're going to update it or */
			/* we have multiple values. If the value exists in the list of values we'll
 			 * leave this element alone and remove the value from the list. If the value
			 * of the element isn't in the list we'll remove the element. */
			} else {
				/* Single value so update. We check the original kvs because we want to know if
 				 * this really is a single or multi value key. */
				if (M_ini_kvs_val_len(ini->kvs, int_key) == 1) {
					M_ini_element_kv_set_val(elem, M_ini_kvs_val_get_direct(kvs, int_key, 0));
					M_ini_kvs_val_remove_at(kvs, int_key, 0);
				/* We have multiple values and we need to handle them accordingly. */
				} else {
					/* Multi-values are not supported. Use either the first or last value and remove all others. */
					if (multi_flag == M_INI_MULTIVALS_USE_LAST || multi_flag == M_INI_MULTIVALS_USE_FIRST) {
						if (multi_flag == M_INI_MULTIVALS_USE_LAST) {
							val = M_ini_kvs_val_get_direct(kvs, int_key, val_len-1);
						} else {
							val = M_ini_kvs_val_get_direct(kvs, int_key, 0);
						}
						M_ini_element_kv_set_val(elem, val);
						M_ini_kvs_val_remove_all(kvs, int_key);
					/* Keep existing values as they are. */
					} else if (multi_flag == M_INI_MULTIVALS_KEEP_EXISTING) {
						exists = M_FALSE;
						for (j=0; j<val_len; j++) {
							/* Check if the element has an existing value. */
							if (M_str_eq(M_ini_element_kv_get_val(elem), M_ini_kvs_val_get_direct(kvs, int_key, j))) {
								M_ini_kvs_val_remove_at(kvs, int_key, j);
								exists = M_TRUE;
								break;
							}
						}
						/* Do we need to remove the element? */
						if (!exists) {
							M_list_u64_insert(remove_idx, i);
						}
					/* M_INI_MULTIVALS_MAINTAIN_ORDER. Everything gets removed and added back later in the order
 					 * it's in the value list. */
					} else {
						M_list_u64_insert(remove_idx, i);
					}
				}
				/* If there are no other vlaues then we'll remove the key. */
				if (M_ini_kvs_val_len(kvs, int_key) == 0) {
					M_ini_kvs_remove(kvs, int_key);
				}
			}

			M_free(int_key);
		}

		/* Remove the elements that need to be removed. */
		len = M_list_u64_len(remove_idx);
		for (i=0; i<len; i++) {
			M_ini_elements_remove_at(section, (size_t)M_list_u64_at(remove_idx, i));
		}
		M_list_u64_destroy(remove_idx);
	} while (M_hash_strvp_enumerate_next(ini->sections, sectenum, &sect_name, &section));
	M_hash_strvp_enumerate_free(sectenum);
}

static void M_ini_writer_tree_add_kv(M_ini_t *ini, M_ini_kvs_t *kvs)
{
	void             *section;
	M_ini_kvs_enum_t *kvsenum;
	M_ini_element_t  *elem;
	M_ini_element_t  *section_elem;
	const char       *int_full_key;
	const char       *val;
	char             *section_name;
	char             *key;
	size_t            len;
	size_t            i;
	size_t            first_sect_idx  = 0;
	M_bool            find_first_sect = M_TRUE;

	M_ini_kvs_enumerate(kvs, &kvsenum);
	while (M_ini_kvs_enumerate_next(kvs, kvsenum, &int_full_key, NULL, &val)) {
		M_ini_split_key(int_full_key, &section_name, &key);
		elem = M_ini_element_create(M_INI_ELEMENT_TYPE_KV);
		M_ini_element_kv_set_key(elem, key);
		M_ini_element_kv_set_val(elem, val);
		
		/* Items not in a section need to be inserted before the first section otherwise they'll be considered
 		 * part of the last section if they were append to the end. */
		if (section_name == NULL) {
			section = ini->elements;
			if (find_first_sect) {
				len = M_ini_elements_len(section);
				for (i=0; i<len; i++) { 
					if (M_ini_element_get_type(M_ini_elements_at(section, i)) == M_INI_ELEMENT_TYPE_SECTION) {
						break;
					}
				}
				first_sect_idx  = i;
				find_first_sect = M_FALSE;
			}
			M_ini_elements_insert_at(section, elem, first_sect_idx++);
		/* Sections just need to have the element inserted at the end. */
		} else {
			section = M_ini_section_get_direct(ini, section_name);
			if (section == NULL) {
				M_ini_section_insert(ini, section_name);
				/* Add the section to the element list. */
				section_elem = M_ini_element_create(M_INI_ELEMENT_TYPE_SECTION);
				M_ini_element_section_set_name(section_elem, section_name);
				M_ini_elements_insert(ini->elements, section_elem);
				/* Setup the section as a list of elements. */
				section = M_ini_section_get_direct(ini, section_name);
			}
			M_ini_elements_insert(section, elem);
		}

		M_free(key);
		M_free(section_name);
	}

	M_ini_kvs_enumerate_free(kvsenum);
}

static void M_ini_writer_update_tree(M_ini_t *ini, const M_ini_settings_t *info)
{
	M_ini_kvs_t *kvs;

	/* duplicate the kvs because we're going to modify it to track what needs to be updated. */
	kvs = M_ini_kvs_duplicate(ini->kvs);

 	/* 1. Update the tree with kvs values and remove the kv from the kvs. */
	M_ini_writer_tree_update_kv_vals(ini, kvs, info);
	/* 2. Add The remaining kv from the kvs to the tree. */
	M_ini_writer_tree_add_kv(ini, kvs);
	/* 3. Remove all kv from the tree that do not appear in the kvs. */
	M_ini_writer_tree_prune(ini);

	M_ini_kvs_destroy(kvs);
}

static void M_ini_writer_write_element_comment(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf)
{
	const char *const_temp;

	(void)key;

	const_temp = M_ini_element_comment_get_val(elem);

	M_buf_add_byte(buf, M_ini_settings_get_comment_char(info));
	if (M_ini_settings_get_padding(info) & M_INI_PADDING_AFTER_COMMENT_CHAR && (const_temp != NULL && !M_chr_isspace(*const_temp)))
		M_buf_add_byte(buf, ' ');

	M_buf_add_str(buf, const_temp);
	M_ini_writer_write_line_ending(info, buf);
}

static void M_ini_writer_write_element_empty_line(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf)
{
	(void)elem;
	(void)key;

	M_ini_writer_write_line_ending(info, buf);
}

static void M_ini_writer_write_element_section(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf)
{
	const char      *val;
	M_ini_padding_t  padding_flags;

	if (M_str_isempty(key))
		return;

	padding_flags = M_ini_settings_get_padding(info);

	M_buf_add_byte(buf, '[');
	M_buf_add_str(buf, key);
	M_buf_add_byte(buf, ']');

	val = M_ini_element_section_get_comment(elem);
	if (val != NULL) {
		if (padding_flags & M_INI_PADDING_AFTER_KV_VAL) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_byte(buf, M_ini_settings_get_comment_char(info));
		if (padding_flags & M_INI_PADDING_AFTER_COMMENT_CHAR && (val != NULL && !M_chr_isspace(*val))) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_str(buf, val);
	}

	M_ini_writer_write_line_ending(info, buf);
}

static void M_ini_writer_write_element(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf)
{
	switch (M_ini_element_get_type(elem)) {
		case M_INI_ELEMENT_TYPE_COMMENT:
			M_ini_writer_write_element_comment(elem, key, info, buf);
			break;
		case M_INI_ELEMENT_TYPE_EMPTY_LINE:
			M_ini_writer_write_element_empty_line(elem, key, info, buf);
			break;
		case M_INI_ELEMENT_TYPE_SECTION:
			M_ini_writer_write_element_section(elem, key, info, buf);
			break;
		case M_INI_ELEMENT_TYPE_KV:
			M_ini_writer_write_element_kv(elem, key, info, buf);
			break;
		case M_INI_ELEMENT_TYPE_UNKNOWN:
			break;
	}
}

static void M_ini_writer_write_section(const char *sect_name, const M_ini_elements_t *section, const M_ini_settings_t *info, M_hash_dict_t *key_lookup, M_buf_t *buf)
{
	M_ini_element_t *elem;
	char            *int_key;
	const char      *key;
	size_t           len;
	size_t           i;

	len = M_ini_elements_len(section);
	for (i=0; i<len; i++) {
		elem        = M_ini_elements_at(section, i);
		int_key     = NULL;
		key         = NULL;
		if (M_ini_element_get_type(elem) == M_INI_ELEMENT_TYPE_KV) {
			int_key = M_ini_full_key(sect_name, M_ini_element_kv_get_key(elem));
			key     = M_hash_dict_get_direct(key_lookup, int_key);
		}
		M_ini_writer_write_element(elem, key, info, buf);
		M_free(int_key);
	}
}

static char *M_ini_writer_tree_to_string(const M_ini_t *ini, const M_ini_settings_t *info)
{
	M_buf_t              *buf;
	M_ini_element_t      *elem;
	const char           *sect_name = NULL;
	const char           *kv_name;
	char                 *int_key;
	const char           *key;
	size_t                len;
	size_t                i;
	M_ini_element_type_t  type;

	buf = M_buf_create();

	len = M_ini_elements_len(ini->elements);
	for (i=0; i<len; i++) {
		elem = M_ini_elements_at(ini->elements, i);
		type = M_ini_element_get_type(elem);

		kv_name = NULL;
		int_key = NULL;
		key     = NULL;

		if (type == M_INI_ELEMENT_TYPE_SECTION) {
			sect_name = M_ini_element_section_get_name(elem);
		} else if (type == M_INI_ELEMENT_TYPE_KV) {
			kv_name   = M_ini_element_kv_get_key(elem);
		}
		if (type == M_INI_ELEMENT_TYPE_SECTION || type == M_INI_ELEMENT_TYPE_KV) {
			int_key = M_ini_full_key(sect_name, kv_name);
			key     = M_hash_dict_get_direct(ini->key_lookup, int_key);
		}

		/* If we have anything not in a section we write it. Once we hit a section this
 		 * will always have selections as the type and write_element will write the section
		 * name and write_section will write all elements in the section. */
		M_ini_writer_write_element(elem, key, info, buf);
		if (type == M_INI_ELEMENT_TYPE_SECTION) {
			M_ini_writer_write_section(sect_name, M_hash_strvp_get_direct(ini->sections, sect_name), info, ini->key_lookup, buf);
		}

		M_free(int_key);
	}

	return M_buf_finish_str(buf, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_ini_writer_write_element_kv(const M_ini_element_t *elem, const char *key, const M_ini_settings_t *info, M_buf_t *buf)
{
	const char      *val;
	char            *temp;
	M_ini_padding_t  padding_flags;

	if (M_str_isempty(key))
		return;

	padding_flags = M_ini_settings_get_padding(info);
	M_buf_add_str(buf, key);

	val = M_ini_element_kv_get_val(elem);
	if (val != NULL) {
		if (padding_flags & M_INI_PADDING_BEFORE_KV_DELIM) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_byte(buf, M_ini_settings_get_kv_delim_char(info));
		if (padding_flags & M_INI_PADDING_AFTER_KV_DELIM) {
			M_buf_add_byte(buf, ' ');
		}
		if (!M_str_quote_if_necessary(&temp, val, M_ini_settings_get_quote_char(info), M_ini_settings_get_escape_char(info), M_ini_settings_get_element_delim_char(info))) {
			temp = NULL;
		}
		M_buf_add_str(buf, temp?temp:val);
		M_free(temp);
	}

	val = M_ini_element_kv_get_comment(elem);
	if (val != NULL) {
		if (padding_flags & M_INI_PADDING_AFTER_KV_VAL) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_byte(buf, M_ini_settings_get_comment_char(info));
		if (padding_flags & M_INI_PADDING_AFTER_COMMENT_CHAR && (val != NULL && !M_chr_isspace(*val))) {
			M_buf_add_byte(buf, ' ');
		}
		M_buf_add_str(buf, val);
	}

	M_ini_writer_write_line_ending(info, buf);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_ini_write(M_ini_t *ini, const M_ini_settings_t *info) 
{
	char *out;

	if (ini == NULL)
		return NULL;

	/* Update the tree with any modifications. */
	M_ini_writer_update_tree(ini, info);

	/* Write the tree to a string. */
	out = M_ini_writer_tree_to_string(ini, info);
	return out;
}

M_fs_error_t M_ini_write_file(M_ini_t *ini, const char *path, const M_ini_settings_t *info)
{
	char         *out;
	M_fs_error_t  res;

	out = M_ini_write(ini, info);
	if (out == NULL)
		return M_FS_ERROR_INVALID;

	res = M_fs_file_write_bytes(path, (unsigned char *)out, 0, M_FS_FILE_MODE_OVERWRITE, NULL);

	M_free(out);
	return res;
}
