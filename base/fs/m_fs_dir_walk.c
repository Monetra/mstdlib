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
#include "fs/m_fs_int.h"
#include "platform/m_platform.h"

#ifndef _WIN32
#  include <dirent.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_FS_DIR_WALK_SEEN_SUCCESS = 0,
	M_FS_DIR_WALK_SEEN_FAIL,
	M_FS_DIR_WALK_SEEN_IN_SET
} M_fs_dir_walk_seen_type_t;

typedef struct {
	M_hash_strvp_t *seen; /* Paths (all types) that have been previously processed. */
	M_hash_strvp_t *traversed; /* Symlink traversal so we don't go down the same path multiple times. */
} M_fs_dir_walk_seens_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Declared here because M_fs_dir_walk_create_entry call M_fs_dir_walk_int which calls M_fs_dir_walk_create_entry...
 * We have a call loop so one has to be declared before. */ 
static M_fs_dir_walk_seen_type_t M_fs_dir_walk_int(const char *base_path, const char *path, const char *prefix, const char *pat,
	M_fs_dir_walk_filter_t filter, M_fs_dir_walk_cb_t cb, void *thunk, M_fs_dir_walk_seens_t *seen);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_fs_dir_walk_seens_t *M_fs_dir_walk_seens_create(void)
{
	M_fs_dir_walk_seens_t *s;
	s            = M_malloc(sizeof(*s));
	s->seen      = M_hash_strvp_create(32, 75, M_HASH_STRVP_NONE, NULL);
	s->traversed = M_hash_strvp_create(32, 75, M_HASH_STRVP_NONE, NULL);
	return s;
}

static void M_fs_dir_walk_seens_destroy(M_fs_dir_walk_seens_t *s)
{
	if (s == NULL)
		return;

	M_hash_strvp_destroy(s->seen, M_FALSE);
	M_hash_strvp_destroy(s->traversed, M_FALSE);
	M_free(s);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_bool M_fs_dir_walk_was_seen(const char *path, const char *prefix, M_fs_dir_walk_seens_t *seen, M_bool traversed)
{
	char   *full_path;
	char   *norm_path;
	M_bool  ret;

	if (prefix != NULL) {
		full_path = M_fs_path_join(path, prefix, M_FS_SYSTEM_AUTO);
	} else {
		full_path = M_strdup(path);
	}

	if (M_fs_path_norm(&norm_path, full_path, M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) != M_FS_ERROR_SUCCESS) {
		/* Invalid path? Most likely an invalid symlink. We'll say we haven't seen it. */
		M_free(full_path);
		return M_FALSE;
	}
	M_free(full_path);

	ret = M_hash_strvp_get(traversed?seen->traversed:seen->seen, norm_path, NULL);
	if (!ret)
		M_hash_strvp_insert(traversed?seen->traversed:seen->seen, norm_path, NULL);

	M_free(norm_path);
	return ret;
}

/* Check that the name matches the pattern and isn't a name (. and ..) that should never be included. */
static M_bool M_fs_dir_walk_check_pattern(const char *name, const char *pat, M_fs_dir_walk_filter_t filter, M_fs_type_t type)
{
	M_bool casecmp;

	if (name == NULL || pat == NULL || type == M_FS_TYPE_UNKNOWN) {
		return M_FALSE;
	}

	/* Don't include . or .. */
	if (M_str_eq(name, ".") || M_str_eq(name, "..")) {
		return M_FALSE;
	}

	/* Check if we should apply the pattern match check. */
	if ((type == M_FS_TYPE_FILE    && (filter & M_FS_DIR_WALK_FILTER_FILE)) ||
		(type == M_FS_TYPE_DIR     && (filter & M_FS_DIR_WALK_FILTER_DIR))  ||
		(type == M_FS_TYPE_PIPE    && (filter & M_FS_DIR_WALK_FILTER_PIPE)) ||
		(type == M_FS_TYPE_SYMLINK && (filter & M_FS_DIR_WALK_FILTER_SYMLINK)))
	{
		/* Check that the entry matches the pattern. */
		casecmp = (filter & M_FS_DIR_WALK_FILTER_CASECMP) ? M_TRUE : M_FALSE;
		if ((!casecmp && !M_str_pattern_match(pat, name)) || (casecmp && !M_str_case_pattern_match(pat, name))) {
			return M_FALSE;
		}
	}

	return M_TRUE;
}

/* Read in the info for the path if necessary. */
static M_bool M_fs_dir_walk_read_path_info(const char *path, M_fs_type_t *type, M_fs_info_t **info, M_fs_dir_walk_filter_t filter)
{
	M_fs_info_flags_t info_flags = M_FS_PATH_INFO_FLAGS_NONE;

	if (path == NULL || type == NULL || info == NULL) {
		return M_FALSE;
	}

	/* We only want to read the path info if we don't know the file type or if reading info is explicitly requested.
	 * Note: Windows passes info filled into M_fs_dir_walk_create_entry so this will be skipped on Windows. */
	if (*info == NULL && (*type == M_FS_TYPE_UNKNOWN || (filter & (M_FS_DIR_WALK_FILTER_READ_INFO_BASIC|M_FS_DIR_WALK_FILTER_READ_INFO_FULL)))) {
		/* If we don't know what is or if we know it's a symlink we want the info of the location itself
		 * and not what it points to if it is a symlink. */
		if (*type != M_FS_TYPE_UNKNOWN && *type != M_FS_TYPE_SYMLINK) {
			info_flags |= M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS;
		}
		if (!(filter & M_FS_DIR_WALK_FILTER_READ_INFO_FULL)) {
			info_flags |= M_FS_PATH_INFO_FLAGS_BASIC;
		}
		/* Read the info for the location. */
		if (M_fs_info(info, path, info_flags) != M_FS_ERROR_SUCCESS) {
			return M_FALSE;
		}
	}
	/* Set the type if it's not set. */
	if (*type == M_FS_TYPE_UNKNOWN) {
		*type = M_fs_info_get_type(*info);
	}

	return M_TRUE;
}

/*! Read a location and turn it into an entry.
 * If the entry is a directory and we are following directories the directory will be followed and the entries from
 * that directory will added to the list of entries.
 * \param path The base path we are walking.
 * \param prefix The location under path we are at.
 * \param name The name of the file/dir/symlink we are creating an entry for.
 * \param pat The pattern to check if the name matches against.
 * \param filter How should operate.
 * \param cb Callback for entries.
 * \param thunk Additional data passed to the callback.
 * \param seen A list of entries that we have already operated on.
 * \param type The type of location name is.
 * \param info Information about location. Can be NULL.
 * \return True if the we should continue walking. Otherwise false.
 */
static M_bool M_fs_dir_walk_create_entry(const char *base_path, const char *path, const char *prefix, const char *name,
	const char *pat, M_fs_dir_walk_filter_t filter, M_fs_dir_walk_cb_t cb, void *thunk, M_fs_dir_walk_seens_t *seen,
	M_fs_type_t type, M_fs_info_t *info)
{
	M_fs_dir_entry_t          *entry;
	M_fs_info_t               *info_sym = NULL;
	M_list_str_t              *parts;
	char                      *full_path;
	char                      *name_path;
	char                      *name_prefix;
	char                      *norm_path;
	char                      *npath;
	char                      *nprefix;
	char                      *nname;
	M_fs_dir_walk_seen_type_t  seen_type;
	M_bool                     ret;

	if (path == NULL || name == NULL || pat == NULL) {
		return M_FALSE;
	}

	/* The full path for the location is path/prefix/name. */
	full_path   = M_fs_path_join(path, prefix, M_FS_SYSTEM_AUTO);
	name_path   = M_fs_path_join(full_path, name, M_FS_SYSTEM_AUTO);
	name_prefix = M_fs_path_join(prefix, name, M_FS_SYSTEM_AUTO);
	M_free(full_path);


	/* Handle jail. */
	if (M_fs_path_norm(&norm_path, name_path, M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) == M_FS_ERROR_SUCCESS) {
		if (filter & (M_FS_DIR_WALK_FILTER_JAIL_FAIL|M_FS_DIR_WALK_FILTER_JAIL_SKIP) && !M_str_eq_max(norm_path, base_path, M_str_len(base_path))) {
			M_free(norm_path);
			M_free(name_prefix);
			M_free(name_path);
			if (filter & (M_FS_DIR_WALK_FILTER_JAIL_FAIL))
				return M_FALSE;
			return M_TRUE;
		}
		M_free(norm_path); norm_path = NULL;
	}

	/* Get the file info if necessary. */
	if (!M_fs_dir_walk_read_path_info(name_path, &type, &info, filter)) {
		M_fs_info_destroy(info);
		M_free(name_prefix);
		M_free(name_path);
		return M_TRUE;
	}

	/* Can't do anything if we couldn't determine the file type. */
	if (type == M_FS_TYPE_UNKNOWN) {
		M_fs_info_destroy(info);
		M_free(name_prefix);
		M_free(name_path);
		return M_TRUE;
	}

	/* Check if the location is hidden. */
	if (M_fs_path_ishidden(name, info) && !(filter & M_FS_DIR_WALK_FILTER_HIDDEN)) {
		M_fs_info_destroy(info);
		M_free(name_prefix);
		M_free(name_path);
		return M_TRUE;
	}

	/* Does the name match the pattern? */
	if (!M_fs_dir_walk_check_pattern(name, pat, filter, type)) { 
		M_fs_info_destroy(info);
		M_free(name_prefix);
		M_free(name_path);
		return M_TRUE;
	}

	/* Should we follow the symlink? */
	if (type == M_FS_TYPE_SYMLINK && (filter & M_FS_DIR_WALK_FILTER_FOLLOWSYMLINK) &&
			M_fs_path_norm(&norm_path, name_path, M_FS_PATH_NORM_HOME|M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) == M_FS_ERROR_SUCCESS)
	{
		full_path = NULL;
		if (M_fs_path_readlink(&full_path, norm_path) == M_FS_ERROR_SUCCESS && full_path != NULL) {
			M_free(norm_path);
			if (M_fs_path_isabs(full_path, M_FS_SYSTEM_AUTO)) {
				norm_path = full_path;
			} else {
				norm_path = M_fs_path_join_vparts(M_FS_SYSTEM_AUTO, 3, path, prefix, full_path);
				M_free(full_path);
			}
			if (M_fs_path_norm(&full_path, norm_path, M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) == M_FS_ERROR_SUCCESS) {
				/* Not seen then we should try to traverse it. */
				if (!M_fs_dir_walk_was_seen(full_path, NULL, seen, M_TRUE)) {
					/* Split the path into parts so it can be walked. Use the symlink after it was followed so
					 * what's pointed to not the link is added. */
					parts   = M_fs_path_componentize_path(full_path, M_FS_SYSTEM_AUTO);
					nname   = M_list_str_take_last(parts);

					npath   = M_fs_path_join_parts(parts, M_FS_SYSTEM_AUTO);
					if (M_str_eq(npath, path)) {
						nprefix = NULL;
					} else {
						M_free(npath);
						nprefix = M_list_str_take_last(parts);
						npath   = M_fs_path_join_parts(parts, M_FS_SYSTEM_AUTO);
					}

					M_list_str_destroy(parts);

					ret = M_fs_dir_walk_create_entry(base_path, npath, nprefix, nname, pat, filter, cb, thunk, seen, M_fs_info_get_type(info_sym), info_sym);

					M_free(nname);
					M_free(nprefix);;
					M_free(npath);
					if (!ret) {
						M_free(full_path);
						M_free(norm_path);
						M_fs_info_destroy(info);
						M_free(name_prefix);
						M_free(name_path);
						return M_FALSE;
					}
				}
			}
		}
		M_free(full_path);
		M_free(norm_path);
	}

	/* We have a directory and we want to recuse into it. So read the contents of the dir. */
	if (type == M_FS_TYPE_DIR && (filter & M_FS_DIR_WALK_FILTER_RECURSE)) {
		/* Read the entries from the directory and add them to the current list. */
		seen_type = M_fs_dir_walk_int(base_path, path, name_prefix, pat, filter, cb, thunk, seen);
		switch (seen_type) {
			case M_FS_DIR_WALK_SEEN_IN_SET:
				M_fs_info_destroy(info);
				M_free(name_prefix);
				M_free(name_path);
				return M_TRUE;
			case M_FS_DIR_WALK_SEEN_FAIL:
				M_fs_info_destroy(info);
				M_free(name_prefix);
				M_free(name_path);
				return M_FALSE;
			case M_FS_DIR_WALK_SEEN_SUCCESS:
				break;
		}
	}

	/* Check our filters. This needs to happen after recurse because FILE|RECURSE should recurse but
	 * only return a list of files. */
	if ((type == M_FS_TYPE_FILE    && !(filter & M_FS_DIR_WALK_FILTER_FILE)) ||
		(type == M_FS_TYPE_DIR     && !(filter & M_FS_DIR_WALK_FILTER_DIR))  ||
		(type == M_FS_TYPE_PIPE    && !(filter & M_FS_DIR_WALK_FILTER_PIPE)) ||
		(type == M_FS_TYPE_SYMLINK && !(filter & M_FS_DIR_WALK_FILTER_SYMLINK)))
	{
		M_fs_info_destroy(info);
		M_free(name_prefix);
		M_free(name_path);
		return M_TRUE;
	}

	/* Dir was handled during recursion. */
	if (type != M_FS_TYPE_DIR && filter & M_FS_DIR_WALK_FILTER_AS_SET) {
		if (M_fs_dir_walk_was_seen(path, name_prefix, seen, M_FALSE)) {
			M_fs_info_destroy(info);
			M_free(name_prefix);
			M_free(name_path);
			return M_TRUE;
		}
	}
	
	/* Create our entry. */
	entry = M_fs_dir_walk_fill_entry(name_path, name_prefix, type, info, filter);
	ret   = cb(path, entry, M_FS_ERROR_SUCCESS, thunk);

	M_free(name_prefix);
	M_free(name_path);

	return ret;
}

/*! Given a directory walk though each item it contains.
 * This is an internal version of the function that allows us to pass additional parameters.
 * \param path The base path we are walking.
 * \param The path under the base path we are walking. The full path of the location is path/prefix.
 * \param pat The pattern we should match against to determine if we should include the location or not.
 * \param filter How we should operate on the entry.
 * \param cb Callback for entries.
 * \param thunk Additional data passed to the callback.
 * \param seen A list of entries above this we have already traversed.
 * \return True if we should continue walking. Otherwise false.
 */
#ifdef _WIN32
static M_bool M_fs_dir_walk_int_sys(const char *base_path, const char *full_path, const char *path, const char *prefix,
	const char *pat, M_fs_dir_walk_filter_t filter, M_fs_dir_walk_cb_t cb, void *thunk, M_fs_dir_walk_seens_t *seen)
{
	char            *norm_path;
	M_fs_info_t     *info;
	WIN32_FIND_DATA  file_data;
	HANDLE           find;
	M_bool           ret;

	/* We need to add \* to the end of the path otherwise we would only get info about the path itself and not
 	 * a list of files under path. */
	norm_path = M_fs_path_join(full_path, "*", M_FS_SYSTEM_AUTO);
	find = FindFirstFile(norm_path, &file_data);
	if (find == INVALID_HANDLE_VALUE) {
		M_free(norm_path);
		return M_FALSE;
	}
	M_free(norm_path);

	do {
		norm_path = M_fs_path_join(full_path, file_data.cFileName, M_FS_SYSTEM_AUTO);
		if (M_fs_info_int(&info, norm_path, (filter&M_FS_DIR_WALK_FILTER_READ_INFO_FULL)?M_FS_PATH_INFO_FLAGS_NONE:M_FS_PATH_INFO_FLAGS_BASIC, &file_data) != M_FS_ERROR_SUCCESS) {
			M_free(norm_path);
			break;
		}
		M_free(norm_path);

		ret = M_fs_dir_walk_create_entry(base_path, path, prefix, file_data.cFileName, pat, filter, cb, thunk, seen, M_fs_info_get_type(info), info);
	} while (FindNextFile(find, &file_data) != 0 && ret == M_TRUE);

	FindClose(find);

	return ret;
}
#else
static M_bool M_fs_dir_walk_int_sys(const char *base_path, const char *full_path, const char *path, const char *prefix,
	const char *pat, M_fs_dir_walk_filter_t filter, M_fs_dir_walk_cb_t cb, void *thunk, M_fs_dir_walk_seens_t *seen)
{
	DIR           *dir;
	struct dirent *dir_entry = NULL;
	M_fs_type_t    type;
	M_bool         ret = M_FALSE;

	/* Read the contents of the dir. */
	dir = opendir(full_path);
	if (dir == NULL)
		return M_FALSE;

	/* We don't use readdir_r because it's deprecated on pretty much every modern platform.
 	 * readdir is reentrant these days and should be used instead. */
	while ((dir_entry = readdir(dir)) != NULL) {
		/* Try to determine the file type. This is a short cut (which prevents a needless (l)stat calls) if we don't
 		 * need to get the file info and the OS/filesystem supports giving us this information we'll use it. */
#ifdef HAVE_DIRENT_TYPE
		if (dir_entry->d_type == DT_DIR) {
			type = M_FS_TYPE_DIR;
		} else if (dir_entry->d_type == DT_FIFO) {
			type = M_FS_TYPE_PIPE;
		} else if (dir_entry->d_type == DT_LNK) {
			type = M_FS_TYPE_SYMLINK;
		} else if (dir_entry->d_type == DT_UNKNOWN) {
			type = M_FS_TYPE_UNKNOWN;
		} else {
			type = M_FS_TYPE_FILE;
		}
#else
		type     = M_FS_TYPE_UNKNOWN;
#endif
		ret = M_fs_dir_walk_create_entry(base_path, path, prefix, dir_entry->d_name, pat, filter, cb, thunk, seen, type, NULL);
		if (!ret) {
			break;
		}
	}

	closedir(dir);
	return ret;
}
#endif

static M_fs_dir_walk_seen_type_t M_fs_dir_walk_int(const char *base_path, const char *path, const char *prefix, const char *pat,
	M_fs_dir_walk_filter_t filter, M_fs_dir_walk_cb_t cb, void *thunk, M_fs_dir_walk_seens_t *seen)
{
	char             *full_path;
	char             *norm_path;
	M_fs_dir_entry_t *entry;
	M_bool            ret;

	/* Combine the prefix with the path so we have the real path we're traversing. */
	full_path = M_fs_path_join(path, prefix, M_FS_SYSTEM_AUTO);
	/* Get the full path (must exist) we are going to walk. */
	if (M_fs_path_norm(&norm_path, full_path, M_FS_PATH_NORM_RESALL|M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) != M_FS_ERROR_SUCCESS) {
		M_free(full_path);
		return M_FS_DIR_WALK_SEEN_FAIL;
	}
	M_free(full_path);

	if (M_fs_dir_walk_was_seen(norm_path, NULL, seen, M_FALSE)) {
		/* We're filtering out paths that have been seen so return this one is already in our set. */
		if (filter & M_FS_DIR_WALK_FILTER_AS_SET) {
			M_free(norm_path);
			return M_FS_DIR_WALK_SEEN_IN_SET;
		}

		/* We've see this path so we're in an infinite loop and need to stop processing. An
		 * infinite loop can result from a symlink in a dir pointing to it's parent.
		 * E.g /dir1
		 *     /dir1/sym1 -> ../dir1
		 */
		entry = M_fs_dir_walk_fill_entry(norm_path, prefix, M_FS_TYPE_DIR, NULL, filter);
		M_free(norm_path);
		if (entry == NULL)
			return M_FS_DIR_WALK_SEEN_FAIL;
		return cb(path, entry, M_FS_ERROR_LINK_LOOP, thunk)?M_FS_DIR_WALK_SEEN_SUCCESS:M_FS_DIR_WALK_SEEN_FAIL;
	}

	ret = M_fs_dir_walk_int_sys(base_path, norm_path, path, prefix, pat, filter, cb, thunk, seen);

	/* Only remove the directory if we're not filtering as a set.
 	 * As we go down dirs under path we add them to the list of seen dirs and as we come back out of each dir we
 	 * remove it from the list. We're checking for infinite loops and we don't care if a dir is included multiple
	 * times due to symlinks. We only care about getting stuck in an infinite loop. */
	if (!(filter & M_FS_DIR_WALK_FILTER_AS_SET))
		M_hash_strvp_remove(seen->seen, norm_path, M_FALSE);

	M_free(norm_path);
	return ret?M_FS_DIR_WALK_SEEN_SUCCESS:M_FS_DIR_WALK_SEEN_FAIL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_fs_dir_walk(const char *path, const char *pat, M_uint32 filter, M_fs_dir_walk_cb_t cb, void *thunk)
{
	char           *norm_path = NULL;
	M_fs_dir_walk_seens_t *seen;

	/* We have to have a path and a callback. Without a path we don't know what to walk. Without the callback
 	 * we don't have anything to do. */
	if (path == NULL || *path == '\0' || cb == NULL)
		return;

	/* Get the absolute path we're walking. We'll need this for jailing and properly following symlinks
 	 * that are relative paths. */
	if (M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL|M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO) != M_FS_ERROR_SUCCESS)
		return;
	path = norm_path;

	/* If a pattern is not set then we want to match everything. */
	if (pat == NULL || *pat == '\0')
		pat = "*";

	seen = M_fs_dir_walk_seens_create();
	M_fs_dir_walk_int(norm_path, path, NULL, pat, filter, cb, thunk, seen);
	M_fs_dir_walk_seens_destroy(seen);
	M_free(norm_path);
}

static M_bool M_fs_dir_walk_entries_appender(const char *path, M_fs_dir_entry_t *entry, M_fs_error_t res, void *thunk)
{
	M_fs_dir_entries_t *entries;

	(void)path;

	if (entry == NULL) {
		return M_FALSE;
	}
	if (thunk == NULL) {
		M_fs_dir_entry_destroy(entry);
		return M_FALSE;
	}

	if (res != M_FS_ERROR_SUCCESS) {
		M_fs_dir_entry_destroy(entry);
		return M_TRUE;
	}

	entries = (M_fs_dir_entries_t *)thunk;
	M_fs_dir_entries_insert(entries, entry);

	return M_TRUE;
}

M_fs_dir_entries_t *M_fs_dir_walk_entries(const char *path, const char *pat, M_uint32 filter)
{
	M_fs_dir_entries_t *entries;

	entries = M_fs_dir_entries_create();
	M_fs_dir_walk(path, pat, filter, M_fs_dir_walk_entries_appender, entries);

	if (M_fs_dir_entries_len(entries) == 0) {
		M_fs_dir_entries_destroy(entries);
		entries = NULL;
	}
	return entries;
}

static M_bool M_fs_dir_walk_strs_appender(const char *path, M_fs_dir_entry_t *entry, M_fs_error_t res, void *thunk)
{
	const char   *name;
	char         *out = NULL;
	M_list_str_t *entries;
	M_buf_t      *buf;

	(void)path;

	if (entry == NULL) {
		return M_FALSE;
	}
	if (thunk == NULL) {
		M_fs_dir_entry_destroy(entry);
		return M_FALSE;
	}

	if (res != M_FS_ERROR_SUCCESS) {
		M_fs_dir_entry_destroy(entry);
		return M_TRUE;
	}

	name = M_fs_dir_entry_get_name(entry);
	if (name == NULL || *name == '\0') {
		M_fs_dir_entry_destroy(entry);
		return M_TRUE;
	}

	entries = (M_list_str_t *)thunk;
	/* Add sep on the end of dirs. */
	if (M_fs_dir_entry_get_type(entry) == M_FS_TYPE_DIR) {
		buf = M_buf_create();
		M_buf_add_str(buf, name);
		M_buf_add_byte(buf, (unsigned char)M_fs_path_get_system_sep(M_FS_SYSTEM_AUTO));
		out = M_buf_finish_str(buf, NULL);
	}
	M_list_str_insert(entries, out!=NULL?out:name);
	M_free(out);
	M_fs_dir_entry_destroy(entry);

	return M_TRUE;
}

M_list_str_t *M_fs_dir_walk_strs(const char *path, const char *pat, M_uint32 filter)
{
	M_list_str_t *entries;

	entries = M_list_str_create(M_LIST_STR_NONE);
	M_fs_dir_walk(path, pat, filter, M_fs_dir_walk_strs_appender, entries);

	if (M_list_str_len(entries) == 0) {
		M_list_str_destroy(entries);
		entries = NULL;
	}
	return entries;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_dir_entry_t *M_fs_dir_walk_fill_entry(const char *full_path, const char *rel_path, M_fs_type_t type, M_fs_info_t *info, M_fs_dir_walk_filter_t filter)
{
	M_fs_dir_entry_t *entry;
	char             *norm_path;

	if (!M_fs_dir_walk_read_path_info(full_path, &type, &info, filter)) {
		return NULL;
	}

	/* Create our entry. */
	entry = M_fs_dir_entry_create();
	M_fs_dir_entry_set_type(entry, type);
	M_fs_dir_entry_set_hidden(entry, M_fs_path_ishidden(full_path, info));

	M_fs_dir_entry_set_name(entry, rel_path);

	/* If it's symlink we want to resolve what it points to. */
	if (type == M_FS_TYPE_SYMLINK) {
		if (M_fs_path_readlink(&norm_path, full_path) == M_FS_ERROR_SUCCESS) {
			M_fs_dir_entry_set_resolved_name(entry, norm_path);
			M_free(norm_path);
		}
	}

	/* Determine if we should store the info or throw it away (if it was even created). */
	if (filter & (M_FS_DIR_WALK_FILTER_READ_INFO_BASIC|M_FS_DIR_WALK_FILTER_READ_INFO_FULL)) {
		M_fs_dir_entry_set_info(entry, info);
	} else {
		M_fs_info_destroy(info);
	} 

	return entry;
}
