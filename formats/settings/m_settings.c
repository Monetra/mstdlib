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
#include "platform/m_platform.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_settings {
	char               *organization;
	char               *application;
	char               *filename;
	M_uint32            readflags;
	M_settings_scope_t  scope;
	M_settings_type_t   type;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_settings_type_t M_settings_determine_type(M_settings_type_t type)
{
	if (type == M_SETTINGS_TYPE_NATIVE) {
#if defined(_WIN32)
		type = M_SETTINGS_TYPE_REGISTRY;
#elif defined(__APPLE__)
		type = M_SETTINGS_TYPE_JSON;
#elif !defined(_WIN32)
		type = M_SETTINGS_TYPE_INI;
#endif
	}

	return type;
}

static const char *M_settings_determine_type_extension(M_settings_type_t type)
{
	const char *ext = "";

	switch (M_settings_determine_type(type)) {
		/* NATIVE shouldn't ever be here becuase we've already determined the actual type.
 		 * But we want to have it listed so we don't get a compiler warning. We don't
		 * want to use 'default' because we'd lose warnings if we add a new type and
		 * forget to add it to this switch. */
		case M_SETTINGS_TYPE_NATIVE:
#ifdef _WIN32
		case M_SETTINGS_TYPE_REGISTRY:
#endif
			/* We're going to add an extension for the registry even though it doens't
 			 * actually need/use an extension. This is because when writing we first
			 * delete all keys then write the settings. We don't want a situation
			 * where one app uses "Org/App" but another only uses "Org". The "Org"
			 * entry will be mixed with "Org/App" since it's a tree based hierarchy. */
			ext = ".cfg";
			break;
		case M_SETTINGS_TYPE_INI:
			ext = ".ini";
			break;
		case M_SETTINGS_TYPE_JSON:
			ext = ".json";
			break;
	}

	return ext;
}

static M_settings_t *M_settings_create_int(const char *organization, const char *application, const char *filename, M_settings_scope_t scope, M_settings_type_t type, M_uint32 flags)
{
	M_settings_t *settings;

	type = M_settings_determine_type(type);

	settings = M_malloc_zero(sizeof(*settings));
	settings->organization = M_strdup(organization);
	settings->application  = M_strdup(application);
	settings->filename     = M_strdup(filename);
	settings->scope        = scope;
	settings->type         = type;
	settings->readflags    = flags;

	return settings;
}

static char *M_settings_determine_filename(const char *organization, const char *application, M_settings_scope_t scope, M_settings_type_t type)
{
	M_list_str_t *parts;
	M_buf_t      *buf;
	const char   *ext;
	char         *out;
	char         *filename;
	M_fs_error_t  res;
#ifdef _WIN32
	DWORD         path_max;
#endif

	type  = M_settings_determine_type(type);
	ext   = M_settings_determine_type_extension(type);
	parts = M_list_str_create(M_LIST_STR_NONE);

	/* Native type is allowed on Windows which denotes the registry. */
#ifdef _WIN32
	if (type == M_SETTINGS_TYPE_REGISTRY) {
		M_list_str_insert(parts, "Software");
	}
#endif

	/* If we don't have any parts then we're not on Windows or we are but we're not using the registry. */
	if (M_list_str_len(parts) == 0) {
		if (scope == M_SETTINGS_SCOPE_USER) {
#ifdef _WIN32
			res = M_fs_path_norm(&out, "%APPDATA%", M_FS_PATH_NORM_NONE, M_FS_SYSTEM_WINDOWS);
#else
			res = M_fs_path_norm(&out, "~", M_FS_PATH_NORM_HOME, M_FS_SYSTEM_AUTO);
#endif
			if (res != M_FS_ERROR_SUCCESS) {
				M_list_str_destroy(parts);
				return NULL;
			}
			M_list_str_insert(parts, out);
#if defined(__APPLE__)
			M_list_str_insert(parts, "Library/Preferences");
#elif !defined(_WIN32)
			M_list_str_insert(parts, ".config");
#endif
		} else {
#if defined(_WIN32)
			out    = M_malloc(M_fs_path_get_path_max(M_FS_SYSTEM_WINDOWS)+1);
			out[0] = '\0';
			if (!M_win32_size_t_to_dword(M_fs_path_get_path_max(M_FS_SYSTEM_WINDOWS)+1, &path_max)) {
				M_free(out);
				return NULL;
			}
			if (GetModuleFileName(GetModuleHandle(NULL), out, path_max) != ERROR_SUCCESS) {
				M_free(out);
				return NULL;
			}
			M_list_str_insert(parts, out);
			M_free(out);
#elif defined(__APPLE)
			M_list_str_insert(parts, "/Library/Preferences");
#else
			M_list_str_insert(parts, "/etc");
#endif
		}
	}

	/* Now that we have the base location add the config file info locations. */
	buf = M_buf_create();
	if (!M_str_isempty(organization) && !M_str_isempty(application)) {
		M_list_str_insert(parts, organization);
		M_buf_add_str(buf, application);
		M_buf_add_str(buf, ext);
	} else if (M_str_isempty(organization) && !M_str_isempty(application)) {
		M_buf_add_str(buf, application);
		M_buf_add_str(buf, ext);
	} else if (!M_str_isempty(organization) && M_str_isempty(application)) {
		M_buf_add_str(buf, organization);
		M_buf_add_str(buf, ext);
	} else {
		/* Neither organization nor application were set. */
		M_list_str_destroy(parts);
		M_buf_cancel(buf);
		return NULL;
	}
	out = M_buf_finish_str(buf, NULL);
	M_list_str_insert(parts, out);
	M_free(out);

	/* Join all the parts and create our filename. */
	out = M_fs_path_join_parts(parts, M_FS_SYSTEM_AUTO);
	M_list_str_destroy(parts);
#ifdef _WIN32
	if (type == M_SETTINGS_TYPE_REGISTRY) {
		filename = M_strdup(out);
		res      = M_FS_ERROR_SUCCESS;
	} else {
#endif
		res = M_fs_path_norm(&filename, out, M_FS_PATH_NORM_NONE, M_FS_SYSTEM_AUTO);
#ifdef _WIN32
	}
#endif
	M_free(out);
	if (res != M_FS_ERROR_SUCCESS)
		return NULL;
	return filename;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_settings_t *M_settings_create(const char *organization, const char *application, M_settings_scope_t scope, M_settings_type_t type, M_uint32 flags)
{
	M_settings_t *settings;
	char         *filename;

	if (M_str_isempty(organization) && M_str_isempty(application))
		return NULL;

	filename = M_settings_determine_filename(organization, application, scope, type);
	settings = M_settings_create_int(organization, application, filename, scope, type, flags);
	M_free(filename);

	return settings;
}

M_settings_t *M_settings_create_file(const char *filename, M_settings_type_t type, M_uint32 flags)
{
	char         *norm_filename;
	M_settings_t *settings;
	M_fs_error_t  res;

	if (M_str_isempty(filename))
		return NULL;

	type = M_settings_determine_type(type);
#ifdef _WIN32
	if (type == M_SETTINGS_TYPE_REGISTRY) {
		norm_filename = M_strdup(filename);
	} else {
#endif
		res = M_fs_path_norm(&norm_filename, filename, M_FS_PATH_NORM_NONE, M_FS_SYSTEM_AUTO);
		if (res != M_FS_ERROR_SUCCESS) {
			return NULL;
		}
#ifdef _WIN32
	}
#endif

	settings = M_settings_create_int(NULL, NULL, norm_filename, M_SETTINGS_SCOPE_USER, type, flags);
	M_free(norm_filename);
	return settings;
}

void M_settings_destroy(M_settings_t *settings)
{
	if (settings == NULL)
		return;

	M_free(settings->organization);
	settings->organization = NULL;
	M_free(settings->application);
	settings->application = NULL;
	M_free(settings->filename);
	settings->filename = NULL;

	M_free(settings);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_settings_access_t M_settings_access(const M_settings_t *settings)
{
	char                *name;
	M_settings_access_t  access = M_SETTINGS_ACCESS_NONE;

	if (settings == NULL)
		return M_SETTINGS_ACCESS_NONE;

	name = M_strdup(settings->filename);

	if (M_fs_perms_can_access(name, 0) == M_FS_ERROR_SUCCESS)
		access |= M_SETTINGS_ACCESS_EXISTS;

	/* Check if we can read. We only care about reading the file itself and not
 	 * the directory. */
	if (M_fs_perms_can_access(name, M_FS_FILE_MODE_READ) == M_FS_ERROR_SUCCESS)
		access |= M_SETTINGS_ACCESS_READ;

	/* We need to find what part of the path actually exists to determine if we can write. */
	while (!M_str_isempty(name) && *name != '.' && M_fs_perms_can_access(name, 0) != M_FS_ERROR_SUCCESS) {
		char *dname = M_fs_path_dirname(name, M_FS_SYSTEM_AUTO);
		M_free(name);
		name = dname;
	}
	/* Now that we have a path that exists, lets see if it is writable */
	if (M_fs_perms_can_access(name, M_FS_FILE_MODE_WRITE) == M_FS_ERROR_SUCCESS) {
		access |= M_SETTINGS_ACCESS_WRITE;
	}
	M_free(name);

	return access;
}

const char *M_settings_filename(const M_settings_t *settings)
{
	if (settings == NULL)
		return NULL;
	return settings->filename;
}

M_settings_scope_t M_settings_scope(const M_settings_t *settings)
{
	if (settings == NULL)
		return M_SETTINGS_SCOPE_USER;
	return settings->scope;
}

M_settings_type_t M_settings_type(const M_settings_t *settings)
{
	if (settings == NULL)
		return M_SETTINGS_TYPE_NATIVE;
	return settings->type;
}

M_hash_dict_t *M_settings_create_dict(const M_settings_t *settings)
{
	M_uint32 dict_flags = M_HASH_DICT_KEYS_ORDERED;   

	if (settings == NULL)
		return NULL;

	if (settings->readflags & M_SETTINGS_READER_CASECMP)
		dict_flags |= M_HASH_DICT_CASECMP;
	return M_hash_dict_create(16, 8, dict_flags);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

char *M_settings_full_key(const char *group, const char *key)
{
	M_buf_t *buf;

	if (key == NULL || *key == '\0')
		return NULL;

	buf = M_buf_create();
	if (!M_str_isempty(group)) {
		M_buf_add_str(buf, group);
		M_buf_add_byte(buf, '/');
	}
	M_buf_add_str(buf, key);

	return M_buf_finish_str(buf, NULL);
}

void M_settings_split_key(const char *s, char **group, char **key)
{
	char *d;
	char *p;

	if (group)
		*group = NULL;
	if (key)
		*key = NULL;
	if (M_str_isempty(s))
		return;

	d = M_strdup(s);
	p = M_str_rchr(d, '/');
	/* A '/' means we have a group. Otherwise we only have a key. */
	if (p == NULL) {
		if (key) {
			*key = d;
		} else {
			M_free(d);
		}
		return;
	}

	*p = '\0';
	if (group)
		*group = M_strdup(d);
	if (key && *(p+1) != '\0')
		*key = M_strdup(p+1);
	M_free(d);
}

void M_settings_set_value(M_hash_dict_t *dict, const char *group, const char *key, const char *value)
{
	char *mykey;

	if (dict == NULL || M_str_isempty(key))
		return;

	mykey = M_settings_full_key(group, key);
	if (M_str_isempty(value)) {
		M_hash_dict_remove(dict, mykey);
	} else {
		M_hash_dict_insert(dict, mykey, value);
	}

	M_free(mykey);
}

const char *M_settings_value(M_hash_dict_t *dict, const char *group, const char *key)
{
	char       *mykey;
	const char *val;

	if (dict == NULL || key == NULL || *key == '\0')
		return NULL;

	mykey = M_settings_full_key(group, key);
	val   = M_hash_dict_get_direct(dict, mykey);
	M_free(mykey);
	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_list_str_t *M_settings_groups(M_hash_dict_t *dict, const char *group)
{
	char               **parts;
	M_hash_dict_enum_t  *dictenum;
	const char          *key;
	M_list_str_t        *groups;
	char                *s_group;
	char                *s_key;
	size_t               num_parts;
	size_t               len;
	size_t               slen;
	size_t               i;

	if (dict == NULL)
		return NULL;

	groups = M_list_str_create(M_LIST_STR_SORTASC|M_LIST_STR_SET);
	len    = M_str_len(group);

	M_hash_dict_enumerate(dict, &dictenum);
	while (M_hash_dict_enumerate_next(dict, dictenum, &key, NULL)) {
		/* Get the full group for this key. */
		M_settings_split_key(key, &s_group, &s_key);
		if (M_str_isempty(group) || M_str_eq_max(group, s_group, len))
		{
			/* If we have a group remove the given group from the key's group. */
			if (!M_str_isempty(group) && !M_str_isempty(s_group)) {
				slen = M_str_len(s_group);
				M_mem_move(s_group, s_group+len, slen-len);
				s_group[slen-len] = '\0';
			}

			/* There is no sub group. */
			if (M_str_isempty(s_group)) {
				M_free(s_group);
				M_free(s_key);
				continue;
			}

			/* The group can be given as "group" or "group/".
 			 * This means that when we removed the provided group from the key's group we could have
			 * "subgroup" or "/subgroup". Instead of trimming we'll just explode and skip any empty
			 * parts. Once we find a non-empty part we know that's the next sub group. */
			parts = M_str_explode_str('/', s_group, &num_parts);
			for (i=0; i<num_parts; i++) {
				if (parts[i] == NULL || *parts[i] == '\0') {
					continue;
				}
				M_list_str_insert(groups, parts[i]);
				break;
			}
			M_str_explode_free(parts, num_parts);
		}
		M_free(s_group);
		M_free(s_key);
	}
	M_hash_dict_enumerate_free(dictenum);

	return groups;
}

M_list_str_t *M_settings_group_keys(M_hash_dict_t *dict, const char *group)
{
	M_hash_dict_enum_t *dictenum;
	const char         *key;
	M_list_str_t       *keys;
	char               *g_group;
	char               *s_group;
	char               *s_key;
	size_t              len;

	keys = M_list_str_create(M_LIST_STR_NONE);
	
	/* Strip trailing / from the provided group.
 	 * The group could have been provided as "group" or "group/". */
	g_group = M_strdup(group);
	len     = M_str_len(g_group);
	while (len > 0 && g_group[len-1] == '/') { 
		g_group[len-1] = '\0';
		len--;
	}

	M_hash_dict_enumerate(dict, &dictenum);
	while (M_hash_dict_enumerate_next(dict, dictenum, &key, NULL)) {
		M_settings_split_key(key, &s_group, &s_key);
		if (M_str_eq(g_group, s_group)) {
			M_list_str_insert(keys, s_key);
		}
		M_free(s_group);
		M_free(s_key);
	}
	M_hash_dict_enumerate_free(dictenum);

	M_free(g_group);
	return keys;
}
