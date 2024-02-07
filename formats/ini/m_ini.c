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

static char M_ini_whitespace_chars[] = {
    ' ',
    '_',
    '-',
    '\t',
    0
};

char *M_ini_delete_whitespace(char *s)
{
    size_t  len;
    size_t  i;
    size_t  j;

    if (s == NULL || *s == '\0') {
        return s;
    }

    len = M_str_len(s);
    for (i=0; i<len; i++) {
        /* Check if we've encountered a whitespace character. */
        for (j=0; M_ini_whitespace_chars[j] != 0; j++) {
            if (s[i] == M_ini_whitespace_chars[j]) {
                M_mem_move(s+i, s+i+1, len-i);
                break;
            }
        }
    }
    s[i] = '\0';
    return s;
}

char *M_ini_internal_key(char *s, M_bool ignore_whitespace)
{
    if (s == NULL)
        return NULL;
    if (ignore_whitespace)
        M_ini_delete_whitespace(s);
    return s;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_ini_full_key(const char *section, const char *key)
{
    M_buf_t *buf;

    if (section == NULL && key == NULL) {
        return NULL;
    }

    buf = M_buf_create();
    if (section != NULL) {
        M_buf_add_str(buf, section);
        M_buf_add_byte(buf, '/');
    }
    if (key != NULL) {
        M_buf_add_str(buf, key);
    }

    return M_buf_finish_str(buf, NULL);
}

void M_ini_split_key(const char *s, char **section, char **key)
{
    char *d;
    char *p;

    if (section)
        *section = NULL;
    if (key)
        *key = NULL;
    if (s == NULL || *s == '\0')
        return;

    d = M_strdup(s);
    p = M_str_chr(d, '/');
    /* A '/' means we have a section. Otherwise we only have a key. */
    if (p == NULL) {
        if (key) {
            *key = d;
        } else {
            M_free(d);
        }
        return;
    }

    *p = '\0';
    if (section)
        *section = M_strdup(d);
    if (key && *(p+1) != '\0')
        *key = M_strdup(p+1);
    M_free(d);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Thin wrapper to conform to prototype for M_hash_strvp free val callback. */
static void M_ini_destroy_elements_vp(void *elems)
{
    M_ini_elements_destroy(elems);
}

M_ini_t *M_ini_create(M_bool ignore_whitespace)
{
    M_ini_t *ini;

    ini                    = M_malloc(sizeof(*ini));
    ini->elements          = M_ini_elements_create();
    ini->sections          = M_hash_strvp_create(16, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_CASECMP, M_ini_destroy_elements_vp);
    ini->kvs               = M_ini_kvs_create();
    ini->key_lookup        = M_hash_dict_create(8, 75, M_HASH_DICT_KEYS_ORDERED|M_HASH_DICT_CASECMP);
    ini->ignore_whitespace = ignore_whitespace;

    return ini;
}

M_ini_t *M_ini_duplicate(const M_ini_t *ini)
{
    M_ini_t             *dup_ini;
    M_hash_strvp_enum_t *hashenum;
    const char          *key;
    void                *val;

    if (ini == NULL)
        return NULL;

    dup_ini           = M_malloc_zero(sizeof(*ini));
    dup_ini->elements = M_ini_elements_duplicate(ini->elements);

    dup_ini->sections = M_hash_strvp_create(16, 75, M_HASH_STRVP_KEYS_ORDERED|M_HASH_STRVP_CASECMP, M_ini_destroy_elements_vp);
    /* ini object sections are pass though pointers so we have to manually duplicate the sections. */
    M_hash_strvp_enumerate(ini->sections, &hashenum);
    while (M_hash_strvp_enumerate_next(ini->sections, hashenum, &key, &val)) {
        M_hash_strvp_insert(dup_ini->sections, key, M_ini_elements_duplicate((M_ini_elements_t *)val));
    }
    M_hash_strvp_enumerate_free(hashenum);

    dup_ini->kvs               = M_ini_kvs_duplicate(ini->kvs);
    dup_ini->key_lookup        = M_hash_dict_duplicate(ini->key_lookup);
    dup_ini->ignore_whitespace = ini->ignore_whitespace;

    return dup_ini;
}

void M_ini_destroy(M_ini_t *ini)
{
    if (ini == NULL) {
        return;
    }

    M_hash_dict_destroy(ini->key_lookup);
    ini->key_lookup = NULL;

    M_hash_strvp_destroy(ini->sections, M_TRUE);
    ini->sections   = NULL;

    M_ini_kvs_destroy(ini->kvs);
    ini->kvs        = NULL;

    M_ini_elements_destroy(ini->elements);
    ini->elements   = NULL;

    M_free(ini);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_ini_kv_has_key(const M_ini_t *ini, const char *key)
{
    char   *int_key;
    M_bool  ret;

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    ret     = M_ini_kv_get(ini, int_key, 0, NULL);
    M_free(int_key);

    return ret;
}

M_list_str_t *M_ini_kv_keys(const M_ini_t *ini)
{
    M_hash_dict_enum_t *dictenum;
    const char         *val;
    const char         *key;
    char               *sect;
    char               *split_sect;
    char               *full_sect;
    char               *full_key;
    M_list_str_t       *keys;

    if (ini == NULL)
        return NULL;

    if (M_hash_dict_enumerate(ini->key_lookup, &dictenum) == 0)
        return NULL;

    keys = M_list_str_create(M_LIST_STR_CASECMP);
    while (M_hash_dict_enumerate_next(ini->key_lookup, dictenum, &key, &val)) {
        /* Check if this is a section or a kv. Sections are ignored. */
        if (key[M_str_len(key)-1] == '/') {
            continue;
        }

        /* For kv we have to build it properly based on the pretty section name. */
        M_ini_split_key(key, &split_sect, NULL);
        full_sect = M_ini_full_key(split_sect, NULL);
        sect = M_strdup(M_hash_dict_get_direct(ini->key_lookup, full_sect));
        full_key = M_ini_full_key(sect, val);

        M_list_str_insert(keys, full_key);

        M_free(split_sect);
        M_free(full_sect);
        M_free(sect);
        M_free(full_key);
    }
    M_hash_dict_enumerate_free(dictenum);

    return keys;
}

M_list_str_t *M_ini_kv_sections(const M_ini_t *ini)
{
    M_hash_dict_enum_t *dictenum;
    const char         *key;
    const char         *val;
    M_list_str_t       *sections;

    if (ini == NULL)
        return NULL;

    if (M_hash_dict_enumerate(ini->key_lookup, &dictenum) == 0)
        return NULL;

    sections = M_list_str_create(M_LIST_STR_CASECMP);
    while (M_hash_dict_enumerate_next(ini->key_lookup, dictenum, &key, &val)) {
        /* Check if this is a section or a kv. */
        if (key[M_str_len(key)-1] == '/') {
            M_list_str_insert(sections, val);
        }
    }
    M_hash_dict_enumerate_free(dictenum);

    return sections;
}

/* Renaming a section is a bit different than just renaming a key. We need to rename every
 * key that is part of the section. old_key in this case is an int_key. */
static M_bool M_ini_kv_rename_section(M_ini_t *ini, const char *int_old_section, const char *int_new_section, const char *new_section)
{
    char         *int_full_key;
    char         *int_full_key_new;
    char         *int_key;
    char         *int_old_sec;
    char         *int_new_sec;
    char         *int_cur_sec;
    char         *new_sec;
    char         *cur_sec;
    char         *cur_key;
    const char   *full_key;
    const char   *val;
    M_list_str_t *all_keys;
    size_t        len;
    size_t        i;

    /* We've already renamed the section in the ini key_lookup so we only need to worry about
     * the individual kvs. We also don't have to worry that any of the keys we'll be renaming
     * to already exist because we did that check earlier. */

    /* Trim off / at the end of the sections. */
    int_old_sec = M_strdup(int_old_section);
    len = M_str_len(int_old_sec);
    int_old_sec[len-1] = '\0';
    int_new_sec = M_strdup(int_new_section);
    len = M_str_len(int_new_sec);
    int_new_sec[len-1] = '\0';
    new_sec = M_strdup(new_section);
    len = M_str_len(new_sec);
    new_sec[len-1] = '\0';

    /* We can't enumerate the ini->kvs or ini->key_lookup because we're going to modify the
     * values as we enumerate. Loop though a pre computed list of all keys to determine which
     * ones to update and update them as we go. */
    all_keys = M_ini_kv_keys(ini);
    len = M_list_str_len(all_keys);
    for (i=0; i<len; i++) {
        /* M_ini_kv_keys returns the pretty keys so we need to convert it into our internal format. */
        full_key = M_list_str_at(all_keys, i);
        int_key  = M_ini_internal_key(M_strdup(full_key), ini->ignore_whitespace);
        M_ini_split_key(int_key, &cur_sec, &cur_key);
        int_cur_sec = M_ini_internal_key(M_strdup(cur_sec), ini->ignore_whitespace);
        if (int_cur_sec != NULL && M_str_caseeq(int_cur_sec, int_old_sec)) {
            /* Update the kv. */
            M_ini_internal_key(cur_key, ini->ignore_whitespace);
            int_full_key = M_ini_full_key(int_new_sec, cur_key);
            M_ini_kvs_rename(ini->kvs, int_key, int_full_key);

            /* Update the lookup. */
            val = M_hash_dict_get_direct(ini->key_lookup, int_key);
            int_full_key_new = M_ini_internal_key(M_ini_full_key(new_sec, val), ini->ignore_whitespace);
            M_hash_dict_remove(ini->key_lookup, int_key);
            M_hash_dict_insert(ini->key_lookup, int_full_key_new, val);

            M_free(int_full_key_new);
            M_free(int_full_key);
        }
        M_free(int_cur_sec);
        M_free(cur_sec);
        M_free(cur_key);
        M_free(int_key);
    }

    M_list_str_destroy(all_keys);
    M_free(new_sec);
    M_free(int_new_sec);
    M_free(int_old_sec);

    return M_TRUE;
}

M_bool M_ini_kv_rename(M_ini_t *ini, const char *key, const char *new_key)
{
    char   *int_key;
    char   *int_new_key;
    char   *stored_key;
    M_bool  ret;

    if (ini == NULL || M_str_isempty(key) || M_str_isempty(new_key) ||
        /* Sections need to be renamed as sections and keys as keys. Cannot rename a key to a section
         * or a section to a key. */
        (key[M_str_len(key)-1] == '/' && new_key[M_str_len(key)-1] != '/') ||
        (key[M_str_len(key)-1] != '/' && new_key[M_str_len(key)-1] == '/') ||
        M_str_eq(key, new_key))
    {
        return M_FALSE;
    }

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    /* Check if the key actually exists. */
    if (!M_hash_dict_get(ini->key_lookup, int_key, NULL)) {
        M_free(int_key);
        return M_FALSE;
    }

    int_new_key = M_ini_internal_key(M_strdup(new_key), ini->ignore_whitespace);

    /* We can't rename a key of it already exists and it's not changing the pretty name. */
    if (!M_str_caseeq(int_key, int_new_key) && M_hash_dict_get(ini->key_lookup, int_new_key, NULL)) {
        M_free(int_new_key);
        M_free(int_key);
        return M_FALSE;
    }

    /* Replace/add the pretty name for the key in the lookup. */
    if (int_key[M_str_len(int_key)-1] == '/') {
        /* Sections are straight replacements. */
        M_hash_dict_insert(ini->key_lookup, int_new_key, new_key);
    } else {
        /* Kv are a little different because we have to pull the key out of the full new_key. */
        M_ini_split_key(new_key, NULL, &stored_key);
        M_hash_dict_insert(ini->key_lookup, int_new_key, stored_key);
        M_free(stored_key);
    }
    /* If only the pretty name is changing then we only need to update the lookup. */
    if (M_str_caseeq(int_key, int_new_key)) {
        M_free(int_new_key);
        M_free(int_key);
        return M_TRUE;
    }

    /* Since the int keys don't match we're doing a full rename and not just a pretty name rename.
     * We need to remove the old int key from the lookup. */
    M_hash_dict_remove(ini->key_lookup, int_key);

    /* Are we renaming a key or a section? */
    if (M_ini_kvs_has_key(ini->kvs, int_key)) {
        ret = M_ini_kvs_rename(ini->kvs, int_key, int_new_key);
    } else {
        ret = M_ini_kv_rename_section(ini, int_key, int_new_key, new_key);
    }

    M_free(int_new_key);
    M_free(int_key);
    return ret;
}

typedef enum {
    M_INI_KV_INSERT_TYPE_SET,
    M_INI_KV_INSERT_TYPE_INSERT,
    M_INI_KV_INSERT_TYPE_ADD_KEY
} M_ini_kv_insert_type_t;

static M_bool M_ini_kv_insert_int(M_ini_t *ini, const char *key, const char *val, M_ini_kv_insert_type_t type)
{
    char   *int_sect;
    char   *int_key;
    char   *split_key;
    char   *split_sect;
    char   *full_sect;
    M_bool  ret       = M_FALSE;

    if (ini == NULL || key == NULL || *key == '\0' || key[M_str_len(key)-1] == '/')
        return M_FALSE;

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    switch (type) {
        case M_INI_KV_INSERT_TYPE_SET:
            ret = M_ini_kvs_val_set(ini->kvs, int_key, val);
            break;
        case M_INI_KV_INSERT_TYPE_INSERT:
            ret = M_ini_kvs_val_insert(ini->kvs, int_key, val);
            break;
        case M_INI_KV_INSERT_TYPE_ADD_KEY:
            ret = M_ini_kvs_val_add_key(ini->kvs, int_key);
            break;
    }
    if (ret && !M_hash_dict_get(ini->key_lookup, int_key, NULL)) {
        /* Split into key and section. Insert the key if we have one (othwerise this is a section) and the section
         * it the section doesn't exist in the lookup. */
        M_ini_split_key(key, &split_sect, &split_key);
        if (split_key != NULL && *split_key != '\0') {
            M_hash_dict_insert(ini->key_lookup, int_key, split_key);
        }
        full_sect = M_ini_full_key(split_sect, NULL);
        if (split_sect != NULL && *split_sect != '\0' && !M_hash_dict_get(ini->key_lookup, full_sect, NULL)) {
            int_sect = M_ini_internal_key(M_strdup(full_sect), ini->ignore_whitespace);
            M_hash_dict_insert(ini->key_lookup, int_sect, split_sect);
            M_free(int_sect);
        }

        M_free(split_key);
        M_free(split_sect);
        M_free(full_sect);
    }
    M_free(int_key);

    return ret;
}

M_bool M_ini_kv_add_key(M_ini_t *ini, const char *key)
{
    return M_ini_kv_insert_int(ini, key, NULL, M_INI_KV_INSERT_TYPE_ADD_KEY);
}

M_bool M_ini_kv_set(M_ini_t *ini, const char *key, const char *val)
{
    return M_ini_kv_insert_int(ini, key, val, M_INI_KV_INSERT_TYPE_SET);
}

M_bool M_ini_kv_insert(M_ini_t *ini, const char *key, const char *val)
{
    return M_ini_kv_insert_int(ini, key, val, M_INI_KV_INSERT_TYPE_INSERT);
}

typedef enum {
    M_INI_KV_REMOVE_TYPE_KEY,
    M_INI_KV_REMOVE_TYPE_VAL,
    M_INI_KV_REMOVE_TYPE_IDX
} M_ini_kv_remove_type_t;

static M_bool M_ini_kv_remove_int(M_ini_t *ini, const char *key, M_ini_kv_remove_type_t type, size_t idx)
{
    char   *int_key;
    M_bool  ret     = M_FALSE;

    if (ini == NULL || key == NULL || *key == '\0' || key[M_str_len(key)-1] == '/')
        return M_FALSE;

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    switch (type) {
        case M_INI_KV_REMOVE_TYPE_KEY:
            ret = M_ini_kvs_remove(ini->kvs, int_key);
            if (ret) {
                M_hash_dict_remove(ini->key_lookup, int_key);
            }
            break;
        case M_INI_KV_REMOVE_TYPE_VAL:
            ret = M_ini_kvs_val_remove_all(ini->kvs, int_key);
            break;
        case M_INI_KV_REMOVE_TYPE_IDX:
            ret = M_ini_kvs_val_remove_at(ini->kvs, int_key, idx);
            break;
    }
    M_free(int_key);

    return ret;
}

M_bool M_ini_kv_remove(M_ini_t *ini, const char *key)
{
    return M_ini_kv_remove_int(ini, key, M_INI_KV_REMOVE_TYPE_KEY, 0);
}

M_bool M_ini_kv_remove_vals(M_ini_t *ini, const char *key)
{
    return M_ini_kv_remove_int(ini, key, M_INI_KV_REMOVE_TYPE_VAL, 0);
}

M_bool M_ini_kv_remove_val_at(M_ini_t *ini, const char *key, size_t idx)
{
    return M_ini_kv_remove_int(ini, key, M_INI_KV_REMOVE_TYPE_IDX, idx);
}

size_t M_ini_kv_len(const M_ini_t *ini, const char *key)
{
    char   *int_key;
    size_t  len;

    if (ini == NULL) {
        return 0;
    }

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    if (!M_ini_kvs_has_key(ini->kvs, int_key)) {
        M_free(int_key);
        return 0;
    }
    len = M_ini_kvs_val_len(ini->kvs, int_key);
    M_free(int_key);
    return len;
}

M_bool M_ini_kv_get(const M_ini_t *ini, const char *key, size_t idx, const char **val)
{
    char   *int_key;
    M_bool  ret;

    if (ini == NULL)
        return M_FALSE;

    int_key = M_ini_internal_key(M_strdup(key), ini->ignore_whitespace);
    ret = M_ini_kvs_val_get(ini->kvs, int_key, idx, val);
    M_free(int_key);

    return ret;
}

const char *M_ini_kv_get_direct(const M_ini_t *ini, const char *key, size_t idx)
{
    const char *val;
    if (!M_ini_kv_get(ini, key, idx, &val)) {
        return NULL;
    }
    return val;
}

M_list_str_t *M_ini_kv_get_vals(const M_ini_t *ini, const char *key)
{
    M_list_str_t *vals = NULL;
    size_t        len;
    size_t        i;

    if (ini == NULL || key == NULL || *key == '\0')
        return NULL;

    vals = M_list_str_create(M_LIST_STR_NONE);
    len  = M_ini_kv_len(ini, key);
    for (i=0; i<len; i++) {
        M_list_str_insert(vals, M_ini_kv_get_direct(ini, key, i));
    }

    return vals;
}

M_bool M_ini_section_insert(M_ini_t *ini, const char *name)
{
    char *int_full_key;
    char *int_key;

    if (ini == NULL || name == NULL || *name == '\0')
        return M_FALSE;

    /* Ensure we have a '/' at the end of the name to denote this is a section. */
    int_key      = M_ini_internal_key(M_strdup(name), ini->ignore_whitespace);
    int_full_key = M_ini_internal_key(M_ini_full_key(name, NULL), ini->ignore_whitespace);

    /* Already exists so we don't need to add anything */
    if (M_hash_strvp_get(ini->sections, int_key, NULL)) {
        M_free(int_key);
        return M_TRUE;
    }

    M_hash_strvp_insert(ini->sections, int_key, M_ini_elements_create());
    M_hash_dict_insert(ini->key_lookup, int_full_key, name);

    M_free(int_full_key);
    M_free(int_key);
    return M_TRUE;
}

M_bool M_ini_section_get(M_ini_t *ini, const char *name, M_ini_elements_t **section)
{
    char   *int_key;
    M_bool  ret;
    void   *temp_section;

    if (ini == NULL)
        return M_FALSE;

    if (section)
        *section = NULL;
    if (name == NULL || *name == '\0' || M_str_eq(name, "/")) {
        if (section)
            *section = ini->elements;
        return M_TRUE;
    }

    int_key = M_ini_internal_key(M_strdup(name), ini->ignore_whitespace);
    ret = M_hash_strvp_get(ini->sections, int_key, &temp_section);
    if (section)
        *section = temp_section;

    M_free(int_key);

    return ret;
}

M_ini_elements_t *M_ini_section_get_direct(M_ini_t *ini, const char *name)
{
    M_ini_elements_t *section;

    if (M_ini_section_get(ini, name, &section)) {
        return section;
    }
    return NULL;
}

M_bool M_ini_section_remove(M_ini_t *ini, const char *name)
{
    char         *int_key;
    char         *int_full_key;
    const char   *s_full_key;
    char         *s_sect;
    M_list_str_t *kvs_keys;
    size_t        len;
    size_t        i;

    if (ini == NULL || name == NULL || *name == '\0')
        return M_FALSE;

    int_key = M_ini_internal_key(M_strdup(name), ini->ignore_whitespace);

    /* If the section doesn't exist then we don't have anything to remove. */
    if (!M_hash_strvp_get(ini->sections, int_key, NULL)) {
        M_free(int_key);
        return M_FALSE;
    }

    int_full_key = M_ini_full_key(int_key, NULL);

    /* Remove the section from the section list killing all elements it holds. */
    M_hash_strvp_remove(ini->sections, int_key, M_TRUE);
    /* Remove the section from the key_lookup. */
    M_hash_dict_remove(ini->key_lookup, int_full_key);

    /* Go though the KVS and remove every key under the section and remove them all form the key_lookup. */
    kvs_keys = M_ini_kvs_keys(ini->kvs);
    len      = M_list_str_len(kvs_keys);
    for (i=0; i<len; i++) {
        s_full_key = M_list_str_at(kvs_keys, i);
        M_ini_split_key(s_full_key, &s_sect, NULL);
        if (M_str_caseeq(int_key, s_sect)) {
            M_ini_kvs_remove(ini->kvs, s_full_key);
            M_hash_dict_remove(ini->key_lookup, s_full_key);
        }
    }
    M_list_str_destroy(kvs_keys);

    M_free(int_full_key);
    M_free(int_key);
    return M_TRUE;
}
