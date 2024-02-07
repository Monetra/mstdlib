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

#ifdef _WIN32
#include <windows.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN32
static M_bool M_settings_read_registry_key(HKEY hkey, const char *location, const char *group, M_hash_dict_t *dict)
{
    M_buf_t *buf;
    char    *subkey;
    char    *name;
    char    *val;
    HKEY     shkey;
    size_t   len;
    size_t   i;
    DWORD    key_type;
    DWORD    j;
    DWORD    num_keys         = 0;
    DWORD    num_vals         = 0;
    DWORD    subname_max      = 0;
    DWORD    temp_subname_max = 0;
    DWORD    valname_max      = 0;
    DWORD    temp_valname_max = 0;
    DWORD    valdata_max      = 0;
    DWORD    temp_valdata_max = 0;
    LONG     ret;

    /* Put our groups onto the end of location so we have the full location. */
    if (group != NULL) {
        buf = M_buf_create();
        M_buf_add_str(buf, location);
        M_buf_add_byte(buf, '\\');
        M_buf_add_str(buf, group);
        subkey = M_buf_finish_str(buf, NULL);
    } else {
        subkey = M_strdup(location);
    }
    /* Change / into \ because this is Windows. */
    len = M_str_len(subkey);
    for (i=0; i<len; i++) {
        if (subkey[i] == '/') {
            subkey[i] = '\\';
        }
    }

    /* Open the location. */
    ret = RegOpenKeyEx(hkey, subkey, 0, KEY_READ, &shkey);
    M_free(subkey);
    /* If the location doesn't exist were done. No error. */
    if (ret == ERROR_FILE_NOT_FOUND)
        return M_TRUE;
    /* If we can't open the location (and it exists) we have an error. */
    if (ret != ERROR_SUCCESS)
        return M_FALSE;

    /* Get info about sub keys. */
    ret = RegQueryInfoKey(shkey, NULL, 0, NULL, &num_keys, &subname_max, NULL, &num_vals, &valname_max, &valdata_max, NULL, NULL);
    if (ret != ERROR_SUCCESS) {
        RegCloseKey(shkey);
        return M_FALSE;
    }

    /* Go though all sub keys and read their keys and values. */
    name = M_malloc(subname_max+1);
    for (j=0; j<num_keys; j++) {
        temp_subname_max = subname_max+1;
        name[0]          = '\0';
        ret              = RegEnumKeyEx(shkey, j, (LPTSTR)name, &temp_subname_max, NULL, NULL, NULL, NULL);
        if (ret != ERROR_SUCCESS) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }

        subkey = M_settings_full_key(group, name);
        if (!M_settings_read_registry_key(hkey, location, subkey, dict)) {
            M_free(subkey);
            M_free(name);
            return M_FALSE;
        }
        M_free(subkey);
    }
    M_free(name);

    /* Go though all values for this location and store them in the dict. */
    name = M_malloc_zero(valname_max+1);
    val  = M_malloc_zero(valdata_max+1);
    for (j=0; j<num_vals; j++) {
        temp_valname_max = valname_max+1;
        temp_valdata_max = valdata_max+1;
        name[0]          = '\0';
        val[0]           = '\0';
        ret              = RegEnumValue(shkey, j, (LPTSTR)name, &temp_valname_max, NULL, &key_type, (LPBYTE)val, &temp_valdata_max);
        if (ret != ERROR_SUCCESS) {
            M_free(name);
            M_free(val);
            RegCloseKey(shkey);
            return M_FALSE;
        }
        /* We only care about string keys. */
        if (key_type != REG_SZ) {
            continue;
        }

        subkey = M_settings_full_key(group, name);
        M_hash_dict_insert(dict, subkey, val);
        M_free(subkey);
    }
    M_free(name);
    M_free(val);

    RegCloseKey(shkey);
    return M_TRUE;
}

static M_bool M_settings_read_registry(const M_settings_t *settings, M_hash_dict_t *dict)
{
    HKEY hkey = HKEY_CURRENT_USER;

    if (M_settings_scope(settings) == M_SETTINGS_SCOPE_SYSTEM)
        hkey = HKEY_LOCAL_MACHINE;

    return M_settings_read_registry_key(hkey, M_settings_filename(settings), NULL, dict);
}
#endif

static M_bool M_settings_read_ini(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_ini_t          *ini;
    M_ini_settings_t *info;
    M_list_str_t     *keys;
    const char       *key;
    size_t            len;
    size_t            i;

    info = M_ini_settings_create();
    M_ini_settings_set_element_delim_char(info, '\n');
    M_ini_settings_set_quote_char(info, '"');
    M_ini_settings_set_escape_char(info, '"');
    M_ini_settings_set_comment_char(info, '#');
    M_ini_settings_set_kv_delim_char(info, '=');
    M_ini_settings_reader_set_dupkvs_handling(info, M_INI_DUPKVS_REMOVE_PREV);

    ini = M_ini_read_file(M_settings_filename(settings), info, M_TRUE, NULL, 0);
    if (ini == NULL) {
        M_ini_settings_destroy(info);
        return M_FALSE;
    }

    keys = M_ini_kv_keys(ini);
    len  = M_list_str_len(keys);
    for (i=0; i<len; i++) {
        key = M_list_str_at(keys, i);
        M_hash_dict_insert(dict, key, M_ini_kv_get_direct(ini, key, 0));
    }
    M_list_str_destroy(keys);

    M_ini_destroy(ini);
    M_ini_settings_destroy(info);
    return M_TRUE;
}

static M_bool M_settings_read_json_node(const M_json_node_t *node, const char *group, M_hash_dict_t *dict)
{
    M_list_str_t *keys;
    const char   *key;
    char         *s_group;
    char         *val;
    size_t        len;
    size_t        i;
    M_bool        ret;

    switch (M_json_node_type(node)) {
        case M_JSON_TYPE_ARRAY:
            return M_FALSE;
        case M_JSON_TYPE_OBJECT:
            keys = M_json_object_keys(node);
            len  = M_list_str_len(keys);
            for (i=0; i<len; i++) {
                key     = M_list_str_at(keys, i);
                s_group = M_settings_full_key(group, key);
                ret     = M_settings_read_json_node(M_json_object_value(node, key), s_group, dict);
                M_free(s_group);
                if (!ret) {
                    M_list_str_destroy(keys);
                    return M_FALSE;
                }
            }
            M_list_str_destroy(keys);
            break;
        default:
            val = M_json_get_value_dup(node);
            M_hash_dict_insert(dict, group, val);
            M_free(val);
    }

    return M_TRUE;
}

static M_bool M_settings_read_json(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_json_node_t *json;
    M_bool         ret;

    json = M_json_read_file(M_settings_filename(settings), M_JSON_READER_NONE, 0, NULL, NULL, NULL);
    if (json == NULL)
        return M_FALSE;

    ret = M_settings_read_json_node(json, NULL, dict);
    M_json_node_destroy(json);
    return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


M_bool M_settings_read(const M_settings_t *settings, M_hash_dict_t **dict)
{
    M_bool ret = M_FALSE;

    if (settings == NULL || dict == NULL)
        return M_FALSE;

    *dict = M_settings_create_dict(settings);

    /* Verify the file exists and we can read it. */
#ifdef _WIN32
    if (M_settings_type(settings) != M_SETTINGS_TYPE_REGISTRY) {
#endif
        /* Not an error if the file doesn't exist. */
        if (M_fs_perms_can_access(M_settings_filename(settings), 0) != M_FS_ERROR_SUCCESS) {
            return M_TRUE;
        }
        /* If the file exists but we don't have access to read it then we can't get the settings from it.
         * We don't use the settings access check because that will also check the containing directory.
         * We want to know about the file itself.
         *
         * Checking access then opening can be a security hole. If the file is manipulated between checking
         * and opening this could be an issue. However, these are settings so does it really matter if the
         * user can access the file and it's modified. If it can be modified then the settings will still
         * be read. */
        if (M_fs_perms_can_access(M_settings_filename(settings), M_FS_PERMS_MODE_READ) != M_FS_ERROR_SUCCESS) {
            M_hash_dict_destroy(*dict);
            *dict = NULL;
            return M_FALSE;
        }
#ifdef _WIN32
    }
#endif

    switch (M_settings_type(settings)) {
#ifdef _WIN32
        case M_SETTINGS_TYPE_REGISTRY:
            ret = M_settings_read_registry(settings, *dict);
            break;
#endif
        case M_SETTINGS_TYPE_INI:
            ret = M_settings_read_ini(settings, *dict);
            break;
        case M_SETTINGS_TYPE_JSON:
            ret = M_settings_read_json(settings, *dict);
            break;
        case M_SETTINGS_TYPE_NATIVE:
            ret = M_FALSE;
            break;
    }

    if (!ret) {
        M_hash_dict_destroy(*dict);
        *dict = NULL;
    }
    return ret;
}
