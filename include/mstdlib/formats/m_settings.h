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

#ifndef __M_SETTINGS_H__
#define __M_SETTINGS_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_hash_dict.h>
#include <mstdlib/base/m_list_str.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_settings Settings
 *  \ingroup m_formats
 *
 * Platform independent settings storage and retrieval.
 *
 * Settings a are a series of string based key, value pairs. The settings themselves
 * are stored/represented by a M_hash_dict_t. The M_settings_t object handles storing
 * and retrieving the M_hash_dict_t data.
 *
 * Multi-Value M_hash_dict_t's are not currently supported.
 *
 * M_settings_t only handles the storage aspect of settings. It handles determining
 * the OS specific location and format for settings. Though the location and format
 * can be overridden.
 *
 * Settings can be stored in groups by using the '/' character to separate groups,
 * sub groups, and keys.
 * E.g.: group1/group2/key=value
 *
 * Limitation of using the Registry for Windows users:
 * - Key names (includes the full path) cannot exceed 255 characters.
 * - Values cannot exceed 16,383 characters.
 * - It's recommended not to use the registry to store values exceeding 2,048 characters.
 *   Instead a file (such as INI) should be used.
 * - Only 512 sub groups (full path) are supported.
 * - Only 32 sub groups can be created at one given time using M_settings. Meaning you shouldn't
 *   use more than approximately 29 sub groups.
 *
 * Example:
 *
 * \code{.c}
 *     M_settings_t  *s;
 *     M_hash_dict_t *h;
 *
 *     s = M_settings_create("org", "app", M_SETTINGS_SCOPE_USER, M_SETTINGS_TYPE_NATIVE, M_SETTINGS_READER_NONE);
 *     if (!M_settings_read(s, &h)) {
 *         h = M_settings_create_dict(s);
 *     }
 *
 *     M_hash_dict_insert(h, "key", "val");
 *     M_settings_write(s, h);
 *
 *     M_hash_dict_destroy(h);
 *     M_settings_destroy(s);
 * \endcode
 *
 * @{
 */

struct M_settings;
typedef struct M_settings M_settings_t;


/*! The visibility of the settings. */
typedef enum {
    M_SETTINGS_SCOPE_USER = 0, /*!< The settings are local to the current user.
                                    - Windows:
                                      - HKEY_CURRENT_USER -- When type is registry.
                                      - "$HOME\Application Data\" -- Any other type.
                                    - Apple's OS X
                                      - $HOME/Library/Preferences/
                                    - Other OS (Unix/Linux):
                                      - $HOME/.config/ */
    M_SETTINGS_SCOPE_SYSTEM    /*!< The settings are global or system level.
                                    - Windows:
                                      - HKEY_LOCAL_MACHINE -- When type is registry.
                                      - Directory where the running process is located -- Any other type.
                                    - Apple's OS X
                                      - /Library/Preferences/
                                    - Other OS (Unix/Linux):
                                      - /etc/ */
} M_settings_scope_t;


/*! The format the settings should be stored on disk in.
 *
 * NATIVE is the recommended format as m_settings abstracts the underlying format
 * for the given OS.
 *
 * That said it is possible to select a specific format. For example
 * always using a portable format such as INI or JSON means that settings can shared
 * between OS's. However, sharing settings between OS's is dependant on the data itself
 * being cross platform as well.
 * */
typedef enum {
    M_SETTINGS_TYPE_NATIVE = 0, /*!< The OS preferred format.
                                     - Windows:
                                       - Registry.
                                     - Apple's OS X
                                       - JSON
                                     - Other OS (Unix/Linux):
                                       - INI */
    M_SETTINGS_TYPE_INI,        /*!< INI file. */
    /*!< JSON file. */
    M_SETTINGS_TYPE_JSON
#ifdef _WIN32
    ,
    M_SETTINGS_TYPE_REGISTRY    /*!< The Windows Registry. This is only valid and available on Windows. */
#endif
} M_settings_type_t;


/*! Access permissions for a settings. */
typedef enum {
    M_SETTINGS_ACCESS_NONE   = 0,      /*!< Cannot read or write. */
    M_SETTINGS_ACCESS_EXISTS = 1 << 0, /*!< File exists. */
    M_SETTINGS_ACCESS_READ   = 1 << 1, /*!< Can read. */
    M_SETTINGS_ACCESS_WRITE  = 1 << 2  /*!< Can write. */
} M_settings_access_t;


/*! Flags to control the behavior of the settings reader. */
typedef enum {
    M_SETTINGS_READER_NONE    = 0,      /*!< Normal operation. */
    M_SETTINGS_READER_CASECMP = 1 << 0  /*!< Key compare is case insensitive. The dictionary returned by read
                                             will be created with case insensitive key compare. */
} M_settings_reader_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a settings object.
 *
 * \param[in] organization Organization information to store the settings under. This is recommended
 *                         for organizational purposes. It is recommended (for widest compatibility)
 *                         to use a domain name.
 *                         Optional and can be NULL if application is specified. If application is
 *                         not specified this will be used as the name stored on disk.
 * \param[in] application  The application name.
 *                         Optional and can be NULL if organization is specified.
 * \param[in] scope        The visibility of the configuration. User vs system level.
 * \param[in] type         The underlying data type the settings should be stored using.
 * \param[in] flags        M_settings_reader_flags_t flags controlling the reader behavior.
 *
 * \return Settings object on success. NULL on error.
 */
M_API M_settings_t *M_settings_create(const char *organization, const char *application, M_settings_scope_t scope, M_settings_type_t type, M_uint32 flags);


/*! Create a settings object at a specific location.
 *
 * Instead of using the default system paths and constructing the filename from the given information
 * a use the specified filename (including path).
 *
 * \param[in] filename The filename the settings file should be created using.
 *                     If the type is REGISTRY then this will be under HKEY_CURRENT_USER.
 * \param[in] type     The underlying data type the settings should be stored using.
 * \param[in] flags    M_settings_reader_flags_t flags controlling the reader behavior.
 *
 * \return Settings object on success. NULL on error.
 */
M_API M_settings_t *M_settings_create_file(const char *filename, M_settings_type_t type, M_uint32 flags);


/*! Destroy a settings object.
 *
 * \param[in] settings The settings object.
 */
M_API void M_settings_destroy(M_settings_t *settings);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Check what types of operations can be performed for the settings.
 *
 * \param[in] settings The settings object.
 *
 * \return The access permissions.
 */
M_API M_settings_access_t M_settings_access(const M_settings_t *settings);


/*! Get the filename (and path) for the settings.
 *
 * \param[in] settings The settings.
 *
 * \return The associated filename. If the type is registry the filename is
 *         the location under either HKEY_CURRENT_USER or HKEY_LOCAL_MACHINE.
 *         The HKEY itself will not be returned. The scope needs to be used
 *         to determine which HKEY would be used.
 */
M_API const char *M_settings_filename(const M_settings_t *settings);


/*! Get the scope for the settings.
 *
 * \param[in] settings The settings
 *
 * \return The scope.
 */
M_API M_settings_scope_t M_settings_scope(const M_settings_t *settings);


/*! Get the type for the settings.
 *
 * \param[in] settings The settings.
 *
 * \return The underlying type the settings will be stored on disk using. This
 *         is the actual underlying type. If the settings object was created with
 *         NATIVE type this will not return NATIVE but the type that is considered
 *         'native' for the OS.
 */
M_API M_settings_type_t M_settings_type(const M_settings_t *settings);


/*! Create an empty dictionary for storing settings.
 *
 * In cases where there is a parse errot this can be used to create
 * and emptry dictionary to overwrite and store new settings.
 *
 * \param[in] settings The settings
 *
 * \return An empty dict for storing settings.
 */
M_API M_hash_dict_t *M_settings_create_dict(const M_settings_t *settings);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read stored settings.
 *
 * M_settings_access should be used to determing if the parse error
 * was due to a permissions error. If access shows the file exists
 * and does not show it can be read from then it is a permission error.
 *
 * \param[in]  settings The settings.
 * \param[out] dict     A dict with the settings.
 *
 * \return M_TRUE on successful read or if the "file" does not exist. M_FALSE on parse error.
 *
 * \see M_settings_access
 */
M_API M_bool M_settings_read(const M_settings_t *settings, M_hash_dict_t **dict);


/*! Write settings to disk.
 *
 * This will overwrite any existing settings at the location represented by the settings object.
 *
 * \param[in] settings The settings.
 * \param[in] dict     The dict of key, value pairs that hold the setting data.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_settings_write(const M_settings_t *settings, M_hash_dict_t *dict);


/*! Clear settings in memory and on disk.
 *
 * This will clear all existing settings at the location represented by the settings object.
 *
 * \param[in]  settings The settings.
 * \param[out] dict     The dict of key, value pairs that hold the setting data.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_settings_clear(const M_settings_t *settings, M_hash_dict_t **dict);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Combine a group and key into a full key.
 *
 * \param[in] group The group.
 * \param[in] key   The key.
 *
 * \return The full key.
 */
M_API char *M_settings_full_key(const char *group, const char *key);


/*! Split a full key into group and key parts.
 *
 * \param[in]  s     Full key;
 * \param[out] group The group part.
 * \param[out] key   The key part.
 */
M_API void M_settings_split_key(const char *s, char **group, char **key);


/*! Set a settings value.
 *
 * This is a convince function that handles combining the group and key. Otherwise it
 * is no different than adding the value to the dict directly.
 *
 * \param[in,out] dict  The dict to store the value in.
 * \param[in]     group The group. Optional, can be NULL if not storing into a group.
 * \param[in]     key   The key to store under.
 * \param[in]     value The value to store.
 */
M_API void M_settings_set_value(M_hash_dict_t *dict, const char *group, const char *key, const char *value);


/*! Get a settings value.
 *
 * This is a convince function that handles combining the group and key. Otherwise it
 * is no different than accessing the dict directly.
 *
 * \param[in] dict  The dict to read from.
 * \param[in] group The group. Optional, can be NULL if not storing into a group.
 * \param[in] key   The key to store under.
 *
 * \return The value or NULL if the key under group does not exist.
 */
M_API const char *M_settings_value(M_hash_dict_t *dict, const char *group, const char *key);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get a list of sub groups under a given group.
 *
 * This only returns direct sub group.
 * E.g. Full key is "g1/g2/g3/k1"
 *  - Searching NULL -> "g1"
 *  - Searching "g1" -> "g2"
 *  - Searching "g1/g2" -> "g2"
 *  - Searching "g2 -> Nothing because there is no top level g2 group.
 *
 * \param[in] dict  The dict to read from.
 * \param[in] group The group to filer using. Optional, can be NULL to list all top level groups.
 *
 * \return A list of sub groups.
 */
M_API M_list_str_t *M_settings_groups(M_hash_dict_t *dict, const char *group);


/*! Get a list of keys under a given group.
 *
 * This only returns keys under the given group.
 * E.g. Full key is "g1/g2/g3/k1"
 *  - Searching "g1/g2/g3" -> "k1"
 *  - Searching "g1" -> Nothing because there are no keys directly under this group.
 *  - Searching "g1/g2" -> Nothing because there are no keys directly under this group.
 *
 * \param[in] dict  The dict to read from.
 * \param[in] group The group to filer using. Optional, can be NULL to list all top level groups.
 *
 * \return A list of keys.
 */
M_API M_list_str_t *M_settings_group_keys(M_hash_dict_t *dict, const char *group);

/*! @} */

__END_DECLS

#endif /* __M_SETTINGS_H__ */
