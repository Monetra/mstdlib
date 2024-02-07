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
#include "platform/m_platform.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Ensure the full path for writing a file exists. */
static M_bool M_settings_write_create_path(const M_settings_t *settings)
{
    char *dirname;

    /* Get the dirname from the path */
    dirname = M_fs_path_dirname(M_settings_filename(settings), M_FS_SYSTEM_AUTO);
    if (dirname == NULL)
        return M_FALSE;

    /* Check if the path exists. */
    if (M_fs_perms_can_access(dirname, 0) == M_FS_ERROR_SUCCESS) {
        M_free(dirname);
        return M_TRUE;
    }

    /* Create the path. */
    if (M_fs_dir_mkdir(dirname, M_TRUE, NULL) != M_FS_ERROR_SUCCESS) {
        M_free(dirname);
        return M_FALSE;
    }

    M_free(dirname);
    return M_TRUE;
}

#ifdef _WIN32
static char *M_settings_reg_path(const char *location)
{
    char   *path;
    size_t  len;
    size_t  i;

    path = M_strdup(location);
    /* Change / into \ because this is Windows. */
    len = M_str_len(path);
    for (i=0; i<len; i++) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }

    return path;
}

/* This function recursivly goes though every key, deletes each value and then the key
 * itself. RegDeleteTree function could be used instead but it requires "Vista" as
 * the minimum OS version. */
static M_bool M_settings_write_registry_clear(HKEY hkey, const char *location)
{
    char   *subkey;
    char   *name;
    HKEY    shkey;
    DWORD   subname_max      = 0;
    DWORD   temp_subname_max = 0;
    DWORD   valname_max      = 0;
    DWORD   temp_valname_max = 0;
    LONG    ret;

    subkey = M_settings_reg_path(location);

    /* Open the location. */
    ret = RegOpenKeyEx(hkey, subkey, 0, KEY_READ|KEY_WRITE, &shkey);
    M_free(subkey);
    /* If the location doesn't exist were done. No error. */
    if (ret == ERROR_FILE_NOT_FOUND)
        return M_TRUE;
    /* If we can't open the location (and it exists) we have an error. */
    if (ret != ERROR_SUCCESS)
        return M_FALSE;

    /* Get info about sub keys.
     *
     * Note:
     *
     * RegQueryInfoKey has lpcSubKeys and lpcValues parameters which according to MSDN are,
     * "A pointer to a variable that receives the number of subkeys that are contained by the specified key." and
     * "A pointer to a variable that receives the number of values that are associated with the key.".
     *
     * RegEnumKey's MSDN docs say, "To retrieve the index of the last subkey, use the RegQueryInfoKey function."
     *
     * Instead of using these parameters because it's unclear how to use them and
     * because using them simply does not work. We're going to loop until we get ERROR_NO_MORE_ITEMS.
     */
    ret = RegQueryInfoKey(shkey, NULL, 0, NULL, NULL, &subname_max, NULL, NULL, &valname_max, NULL, NULL, NULL);
    if (ret != ERROR_SUCCESS) {
        RegCloseKey(shkey);
        return M_FALSE;
    }

    /* Go though all values for this location delete them. */
    name = M_malloc_zero(valname_max+1);
    while (1) {
        temp_valname_max = valname_max+1;
        name[0]          = '\0';
        ret              = RegEnumValue(shkey, 0, (LPTSTR)name, &temp_valname_max, NULL, NULL, NULL, NULL);
        if (ret == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (ret != ERROR_SUCCESS) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }

        ret = RegDeleteValue(shkey, name);
        if (ret != ERROR_SUCCESS) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }
    }
    M_free(name);

    /* Go though all sub keys and delete them. */
    name = M_malloc(subname_max+1);
    while (1) {
        temp_subname_max = subname_max+1;
        name[0]          = '\0';
        ret              = RegEnumKeyEx(shkey, 0, (LPTSTR)name, &temp_subname_max, NULL, NULL, NULL, NULL);
        if (ret == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (ret != ERROR_SUCCESS) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }

        if (!M_settings_write_registry_clear(shkey, name)) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }

        /* Delete the key now that all of its sub keys and values have been deleted. */
        ret = RegDeleteKey(shkey, name);
        if (ret != ERROR_SUCCESS) {
            M_free(name);
            RegCloseKey(shkey);
            return M_FALSE;
        }
    }
    M_free(name);

    RegCloseKey(shkey);
    return M_TRUE;
}

static M_bool M_settings_write_registry_key(HKEY hkey, const char *location, const char *key, const char *val)
{
    char  *temp;
    char  *sgroup;
    char  *skey;
    HKEY   shkey;
    DWORD  dlen;
    LONG   ret;

    temp   = M_settings_full_key(location, key);
    M_settings_split_key(temp, &sgroup, &skey);
    M_free(temp);
    temp   = sgroup;
    sgroup = M_settings_reg_path(temp);
    M_free(temp);
    ret    = RegCreateKeyEx(hkey, (LPTSTR)sgroup, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ|KEY_WRITE, NULL, &shkey, NULL);
    M_free(sgroup);
    if (ret != ERROR_SUCCESS) {
        M_free(skey);
        return M_FALSE;
    }

    if (!M_win32_size_t_to_dword(M_str_len(val)+1, &dlen)) {
        M_free(skey);
        return M_FALSE;
    }

    ret = RegSetValueEx(shkey, (LPTSTR)skey, 0, REG_SZ, (const BYTE *)val, dlen);
    if (ret != ERROR_SUCCESS) {
        M_free(skey);
        return M_FALSE;
    }

    M_free(skey);
    return M_TRUE;
}

static M_bool M_settings_clear_registry(const M_settings_t *settings)
{
    HKEY hkey = HKEY_CURRENT_USER;

    if (M_settings_scope(settings) == M_SETTINGS_SCOPE_SYSTEM)
        hkey = HKEY_LOCAL_MACHINE;

    if (!M_settings_write_registry_clear(hkey, M_settings_filename(settings)))
        return M_FALSE;

    return M_TRUE;
}

static M_bool M_settings_write_registry(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_hash_dict_enum_t *dictenum;
    const char         *key;
    const char         *val;
    HKEY hkey = HKEY_CURRENT_USER;

    if (M_settings_scope(settings) == M_SETTINGS_SCOPE_SYSTEM)
        hkey = HKEY_LOCAL_MACHINE;

    if (!M_settings_clear_registry(settings))
        return M_FALSE;

    M_hash_dict_enumerate(dict, &dictenum);
    while (M_hash_dict_enumerate_next(dict, dictenum, &key, &val)) {
        if (!M_settings_write_registry_key(hkey, M_settings_filename(settings), key, val)) {
            M_hash_dict_enumerate_free(dictenum);
            return M_FALSE;
        }
    }
    M_hash_dict_enumerate_free(dictenum);

    return M_TRUE;
}
#endif

static M_bool M_settings_clear_file(const M_settings_t *settings)
{
    M_fs_file_t *fd;

    if (M_fs_file_open(&fd, M_settings_filename(settings), M_FS_BUF_SIZE, M_FS_FILE_MODE_WRITE|M_FS_FILE_MODE_OVERWRITE, NULL) != M_FS_ERROR_SUCCESS)
        return M_FALSE;
    M_fs_file_close(fd);
    return M_TRUE;
}

static M_bool M_settings_write_ini(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_ini_t            *ini;
    M_ini_settings_t   *info;
    M_hash_dict_enum_t *dictenum;
    const char         *key;
    const char         *val;
    M_bool              ret = M_TRUE;

    ini  = M_ini_create(M_FALSE);
    info = M_ini_settings_create();
    M_ini_settings_set_element_delim_char(info, '\n');
    M_ini_settings_set_quote_char(info, '"');
    M_ini_settings_set_escape_char(info, '"');
    M_ini_settings_set_comment_char(info, '#');
    M_ini_settings_set_kv_delim_char(info, '=');
    M_ini_settings_writer_set_multivals_handling(info, M_INI_MULTIVALS_USE_LAST);

    M_hash_dict_enumerate(dict, &dictenum);
    while (M_hash_dict_enumerate_next(dict, dictenum, &key, &val)) {
        M_ini_kv_set(ini, key, val);
    }
    M_hash_dict_enumerate_free(dictenum);

    if (M_ini_write_file(ini, M_settings_filename(settings), info) != M_FS_ERROR_SUCCESS)
        ret = M_FALSE;

    M_ini_settings_destroy(info);
    M_ini_destroy(ini);

    return ret;
}

static M_bool M_settings_write_json_node(M_json_node_t *json, const char *key, const char *val)
{
    char          **parts;
    M_json_node_t  *node;
    M_list_str_t   *keys;
    size_t          num_parts;
    size_t          len;
    size_t          i;
    size_t          j;
    M_bool          found;

    if (key[M_str_len(key)-1] == '/')
        return M_FALSE;

    parts = M_str_explode_str('/', key, &num_parts);
    for (i=0; i<num_parts; i++) {
        if (parts[i] == NULL || *parts[i] == '\0')
            continue;

        if (i == num_parts-1) {
            /* Last one if the key for the value. */
            node = M_json_node_create(M_JSON_TYPE_STRING);
            M_json_set_string(node, val);
            M_json_object_insert(json, parts[i], node);
        } else {
            /* All other parts are sections. */
            keys  = M_json_object_keys(json);
            len   = M_list_str_len(keys);
            found = M_FALSE;
            for (j=0; j<len; j++) {
                if (M_str_eq(M_list_str_at(keys, j), parts[i])) {
                    json = M_json_object_value(json, parts[i]);
                    if (M_json_node_type(json) != M_JSON_TYPE_OBJECT) {
                        M_str_explode_free(parts, num_parts);
                        M_list_str_destroy(keys);
                        return M_FALSE;
                    }
                    found = M_TRUE;
                    break;
                }
            }
            M_list_str_destroy(keys);
            if (!found) {
                node = M_json_node_create(M_JSON_TYPE_OBJECT);
                M_json_object_insert(json, parts[i], node);
                json = node;
            }
        }
    }

    M_str_explode_free(parts, num_parts);
    return M_TRUE;
}

static M_bool M_settings_write_json(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_json_node_t      *json;
    M_hash_dict_enum_t *dictenum;
    const char         *key;
    const char         *val;
    M_bool              ret = M_TRUE;

    json = M_json_node_create(M_JSON_TYPE_OBJECT);
    M_hash_dict_enumerate(dict, &dictenum);
    while (M_hash_dict_enumerate_next(dict, dictenum, &key, &val)) {
        if (!M_settings_write_json_node(json, key, val)) {
            M_hash_dict_enumerate_free(dictenum);
            return M_FALSE;
        }
    }
    M_hash_dict_enumerate_free(dictenum);

    if (M_json_write_file(json, M_settings_filename(settings), M_JSON_WRITER_PRETTYPRINT_SPACE) != M_FS_ERROR_SUCCESS)
        ret = M_FALSE;

    M_json_node_destroy(json);
    return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_settings_write(const M_settings_t *settings, M_hash_dict_t *dict)
{
    M_settings_type_t type;
    M_bool            ret = M_FALSE;

    if (settings == NULL || dict == NULL)
        return M_FALSE;

    type = M_settings_type(settings);
#ifdef _WIN32
    if (type != M_SETTINGS_TYPE_REGISTRY) {
#endif
        if (!M_settings_write_create_path(settings)) {
            return M_FALSE;
        }
#ifdef _WIN32
    }
#endif

    switch (type) {
#ifdef _WIN32
        case M_SETTINGS_TYPE_REGISTRY:
            ret = M_settings_write_registry(settings, dict);
            break;
#endif
        case M_SETTINGS_TYPE_INI:
            ret = M_settings_write_ini(settings, dict);
            break;
        case M_SETTINGS_TYPE_JSON:
            ret = M_settings_write_json(settings, dict);
            break;
        case M_SETTINGS_TYPE_NATIVE:
            ret = M_FALSE;
            break;
    }

    return ret;
}

M_bool M_settings_clear(const M_settings_t *settings, M_hash_dict_t **dict)
{
    M_hash_dict_t     *ndict;
    M_settings_type_t  type;
    M_bool             ret  = M_TRUE;

    if (settings == NULL)
        return M_FALSE;

    type = M_settings_type(settings);
#ifdef _WIN32
    if (type != M_SETTINGS_TYPE_REGISTRY) {
#endif
        if (!M_settings_write_create_path(settings)) {
            return M_FALSE;
        }
#ifdef _WIN32
    }
#endif

    switch (type) {
#ifdef _WIN32
        case M_SETTINGS_TYPE_REGISTRY:
            ret = M_settings_clear_registry(settings);
            break;
#endif
        case M_SETTINGS_TYPE_INI:
        case M_SETTINGS_TYPE_JSON:
        case M_SETTINGS_TYPE_NATIVE:
            ret = M_settings_clear_file(settings);
            break;
    }

    if (ret) {
        ndict = M_settings_create_dict(settings);
        if (dict != NULL) {
            if (*dict != NULL) {
                M_hash_dict_destroy(*dict);
            }
            *dict = ndict;
        } else {
            M_hash_dict_destroy(ndict);
        }
    }

    return ret;
}
