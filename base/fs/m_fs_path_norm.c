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
#include "fs/m_fs_int.h"
#include "platform/m_platform.h"

#ifdef _WIN32
#  include <Shlobj.h>
#else
#  include <errno.h>
#  include <sys/types.h>
#  include <pwd.h>
#  include <unistd.h>
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static const size_t MAX_REDIRECTS = 25;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_fs_error_t M_fs_path_norm_symlink(char **out, M_list_str_t **base, const M_list_str_t *parts, M_fs_path_norm_t flags, M_fs_system_t sys_type, M_hash_strvp_t *seen);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Normalize path separators in a path to the system separators. This will turn
 * / into \\ on Windows. */
static char *M_fs_path_norm_sep(const char *path, M_fs_system_t sys_type)
{
    M_buf_t *buf;
    size_t   len;
    size_t   i;
    char     sep;

    if (path == NULL) {
        return NULL;
    }

    sys_type = M_fs_path_get_system_type(sys_type);
    sep      = M_fs_path_get_system_sep(sys_type);
    len      = M_str_len(path);
    if (len >= M_fs_path_get_path_max(sys_type)) {
        return NULL;
    }

    buf    = M_buf_create();

    /* Replace // or \\ with / or \ depending on the OS. */
    for (i=0; i<len; i++) {
        if (sys_type == M_FS_SYSTEM_WINDOWS && path[i] == '/') {
            M_buf_add_byte(buf, (unsigned char)sep);
        } else {
            M_buf_add_byte(buf, (unsigned char)path[i]);
        }
    }

    return M_buf_finish_str(buf, NULL);
}

static M_bool M_fs_path_norm_expand_env_vars(M_list_str_t **dirs)
{
    M_list_str_t *temp_dirs;
    char         *platform_env_var;
    char         *retpath = NULL;
    const char   *env_var;
    size_t        len;
    size_t        dir_len;
    size_t        i;
    M_bool        have_env_var = M_FALSE;
#ifdef _WIN32
    DWORD       retpath_len;
    DWORD       retpath_final_len;
#else
    char       *retpath_tmp;
#endif

    if (dirs == NULL || *dirs == NULL) {
        return M_FALSE;
    }
    dir_len = M_list_str_len(*dirs);
    if (dir_len < 1) {
        return M_FALSE;
    }

    /* First go though all parts and check if we have any env vars. */
    for (i=0; i<dir_len; i++) {
        env_var = M_list_str_at(*dirs, i);
        if (*env_var == '$' || *env_var == '%') {
            have_env_var = M_TRUE;
            break;
        }
    }

    /* No env vars then nothing to expand. */
    if (!have_env_var) {
        return M_TRUE;
    }

    temp_dirs = M_list_str_create(M_LIST_STR_NONE);
    for (i=0; i<dir_len; i++) {
        env_var = M_list_str_at(*dirs, i);
        len     = M_str_len(env_var);

        /* Rewrite the env_var to the platform format if necessary. */
#ifdef _WIN32
        if (len >= 1 && M_str_eq_max(env_var, "$", 1)) {
            platform_env_var = M_malloc_zero(len);
            M_mem_copy(platform_env_var, env_var+1, len-1);
            platform_env_var[len-1] = '\0';
        } else if (len >= 2 && M_str_eq_max(env_var, "%", 1) && M_str_eq_max(env_var+len-1, "%", 1)) {
            platform_env_var = M_malloc_zero(len-1);
            M_mem_copy(platform_env_var, env_var+1, len-2);
            platform_env_var[len-2] = '\0';
        } else {
            M_list_str_insert(temp_dirs, env_var);
            continue;
        }
#else
        if (len >= 1 && M_str_eq_max(env_var, "$", 1)) {
            platform_env_var = M_malloc_zero(len);
            M_mem_copy(platform_env_var, env_var+1, len-1);
            platform_env_var[len-1] = '\0';
        } else if (len >=2 && M_str_eq_max(env_var, "%", 1) && M_str_eq_max(env_var+len-1, "%", 1)) {
            platform_env_var = M_malloc_zero(len-1);
            M_mem_copy(platform_env_var, env_var+1, len-2);
            platform_env_var[len-2] = '\0';
        } else {
            M_list_str_insert(temp_dirs, env_var);
            continue;
        }
#endif

        /* Check if the platform_env_var is empty because we had '$' or '%%'. */
        if (*platform_env_var == '\0') {
            M_free(platform_env_var);
            continue;
        }

        /* Expand the env var. */
#ifdef _WIN32
        retpath_len = GetEnvironmentVariable(platform_env_var, NULL, 0);
        if (retpath_len == 0) {
            M_free(platform_env_var);
            M_list_str_destroy(temp_dirs);
            return M_FALSE;
        }

        retpath = M_malloc_zero(retpath_len);
        retpath_final_len = GetEnvironmentVariable(platform_env_var, retpath, retpath_len);
        /* On failure the '\0' is included in the len. On sucess it is not. */
        if (retpath_final_len == 0 || retpath_len-1 != retpath_final_len) {
            M_free(retpath);
            M_free(platform_env_var);
            M_list_str_destroy(temp_dirs);
            return M_FALSE;
        }
#else
#  ifdef HAVE_SECURE_GETENV
        retpath_tmp = secure_getenv(platform_env_var);
#  else
        retpath_tmp = getenv(platform_env_var);
#  endif
        if (retpath_tmp == NULL) {
            M_free(platform_env_var);
            M_list_str_destroy(temp_dirs);
            return M_FALSE;
        }
        retpath = M_strdup(retpath_tmp);
#endif
        M_free(platform_env_var);

        /* Put the expanded env var into the result dir */
        if (*retpath != '\0') {
            M_list_str_insert(temp_dirs, retpath);
        }
        M_free(retpath);
    }

    M_list_str_destroy(*dirs);
    *dirs = temp_dirs;
    return M_TRUE;
}

/* Turn ~ into a path. */
static M_bool M_fs_path_norm_home(M_list_str_t **dirs, M_fs_system_t sys_type)
{
    char          *home = NULL;
    M_list_str_t  *temp;
#if !defined(_WIN32) && !defined(__APPLE__)
    struct passwd *pwd_result = NULL;
    struct passwd  pwd;
    char          *env_home;
    char          *pbuf;
    size_t         pbuf_len;
    int            ret;
#endif

    if (dirs == NULL || *dirs == NULL) {
        return M_FALSE;
    }
    if (M_list_str_len(*dirs) < 1) {
        return M_FALSE;
    }

    if (M_str_eq(M_list_str_at(*dirs, 0), "~")) {
#ifdef _WIN32
        home = M_malloc(M_fs_path_get_path_max(sys_type)+1);
        /* We are using SHGetFolderPath for XP support. SHGetKnownFolderPath with FOLDERID_Profile should
         * be used instead once XP support is dropped. */
        if (SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, home) != S_OK) {
            M_free(home);
            home = NULL;
        }
#elif defined(__APPLE__) || defined(IOS)
        home = M_fs_path_mac_home();
#else
        /* Get the dir pointed to by the env and fall back to the pwd db entry
         * if the env var hasn't been set. */
#  ifdef HAVE_SECURE_GETENV
        env_home = secure_getenv("HOME");
#  else
        env_home = getenv("HOME");
#  endif
        if (env_home == NULL) {
#  if defined(HAVE_GETPWUID_5)
            pbuf_len = M_fs_unx_getpw_r_size();
            pbuf     = M_malloc(pbuf_len);
            ret      = getpwuid_r(getuid(), &pwd, pbuf, pbuf_len, &pwd_result);
            if (ret == 0 && pwd_result != NULL) {
                home = M_strdup(pwd.pw_dir);
            }
            M_free(pbuf);
#  elif defined(HAVE_GETPWUID_4)
            (void)ret;
            pbuf_len   = M_fs_unx_getpw_r_size();
            pbuf       = M_malloc(pbuf_len);
            pwd_result = getpwuid_r(getuid(), &pwd, pbuf, pbuf_len);
            if (pwd_result != NULL) {
                home = M_strdup(pwd.pw_dir);
            }
            M_free(pbuf);
#  else
            (void)pbuf_len;
            (void)pbuf;
            (void)pwd;
            (void)ret;
            pwd_result = getpwuid(getuid());
            if (pwd_result != NULL) {
                home = M_strdup(pwd_result->pw_dir);
            }
#  endif
        } else {
            home = M_strdup(env_home);
        }
#endif
        if (home == NULL) {
            return M_FALSE;
        }

        M_list_str_remove_at(*dirs, 0);
        temp = M_fs_path_componentize_path(home, sys_type);
        M_list_str_merge(&temp, *dirs, M_TRUE);
        *dirs = temp;
        M_free(home);
    }

    return M_TRUE;
}

/* Create an absolute path from a relative path using the cwd. */
static M_bool M_fs_path_norm_abs(M_list_str_t **dirs, M_fs_system_t sys_type)
{
    M_list_str_t *temp_l;
    char         *temp;
    const char   *part;
    size_t        path_max;
#ifdef _WIN32
    DWORD         dpath_max;
#endif

    if (dirs == NULL || *dirs == NULL) {
        return M_FALSE;
    }
    if (M_list_str_len(*dirs) == 0) {
        return M_FALSE;
    }

    sys_type = M_fs_path_get_system_type(sys_type);
    path_max = M_fs_path_get_path_max(sys_type);
    temp     = M_malloc(path_max);

    /* If the path starts with an empty then this is already an abs path so we
     * don't need to do anything */
    part = M_list_str_at(*dirs, 0);
    if ((part != NULL && *part == '\0') || (sys_type == M_FS_SYSTEM_WINDOWS && M_fs_path_isabs(part, sys_type))) {
        M_free(temp);
        return M_TRUE;
    }

    /* Try to get the cwd. */
#ifdef _WIN32
    if (!M_win32_size_t_to_dword(path_max, &dpath_max)) {
        M_free(temp);
        return M_FALSE;
    }
    if (GetCurrentDirectory(dpath_max-1, temp) == 0) {
#else
    if (!getcwd(temp, path_max-1)) {
#endif
        M_free(temp);
        return M_FALSE;
    }
    /* Put the componitized cwd in front of the list of dirs */
    temp_l = M_fs_path_componentize_path(temp, sys_type);
    M_free(temp);
    M_list_str_merge(&temp_l, *dirs, M_TRUE);
    *dirs = temp_l;

    return M_TRUE;
}

/* Remove a parent directory from the list of dirs. */
static void M_fs_path_norm_remove_parent(M_list_str_t **dirs, M_fs_system_t sys_type)
{
    const char *last;
    size_t      len;

    if (dirs == NULL || *dirs == NULL) {
        return;
    }

    len = M_list_str_len(*dirs);
    /* We don't have anyting before to remove so it's a relative path. Add ..
     * to the dirs because we don't want to lose that we need to move up. */
    if (len == 0) {
        M_list_str_insert(*dirs, "..");
        return;
    }

    last = M_list_str_at(*dirs, len-1);

    /* We don't want to remove the root if this already an abs path. */
    if ((sys_type == M_FS_SYSTEM_WINDOWS && M_fs_path_isabs(last, sys_type)) || (*last == '\0')) {
        return;
    }

    /* Add .. if we already have .. or remove the last path. */
    if (M_str_eq(last, "..")) {
        M_list_str_insert(*dirs, "..");
    } else {
        M_list_str_remove_at(*dirs, len-1);
    }
}

static M_fs_error_t M_fs_path_norm_int(char **out, const char *path, M_uint32 flags, M_fs_system_t sys_type, M_hash_strvp_t *seen)
{
    M_list_str_t *base = NULL;
    M_list_str_t *parts;
    char         *str_p;
    char         *temp;
    size_t        len;
    M_fs_error_t  ret;

    *out = NULL;

    /* Can't normalize nothing ... */
    if (path == NULL || *path == '\0')
        return M_FS_ERROR_INVALID;

    /* Treat an empty path as cwd */
    if (*path == '\0')
        path = ".";

    /* Deal with redirect (symlink) loops. */
    if (M_hash_strvp_num_keys(seen) >= MAX_REDIRECTS || M_hash_strvp_get(seen, path, NULL))
        return M_FS_ERROR_LINK_LOOP;
    M_hash_strvp_insert(seen, path, NULL);

    /* Figure out which separator we should use and which kind of logic we
     * should follow */
    sys_type = M_fs_path_get_system_type(sys_type);

    /* Normalize the separators. */
    temp = M_fs_path_norm_sep(path, sys_type);
    if (temp == NULL) {
        return M_FS_ERROR_NAMETOOLONG;
    }

    /* we need to support UNC path names, it's ok to
     * start a path with 2 slashes, it indicates that
     * it is a UNC path. We'll add the \s back later. */
    if (sys_type == M_FS_SYSTEM_WINDOWS && M_fs_path_isunc(temp)) {
        /* Cannot follow symlinks on UNC paths. */
        flags &= ~((M_fs_path_norm_t)M_FS_PATH_NORM_FOLLOWSYMLINKS);
    }

    parts = M_fs_path_componentize_path(temp, sys_type);
    M_free(temp);

    if (!M_fs_path_norm_expand_env_vars(&parts)) {
        M_list_str_destroy(parts);
        return M_FS_ERROR_GENERIC;
    }

    if (flags & M_FS_PATH_NORM_HOME && !M_fs_path_norm_home(&parts, sys_type)) {
        M_list_str_destroy(parts);
        return M_FS_ERROR_GENERIC;
    }

    if (flags & M_FS_PATH_NORM_ABSOLUTE && !M_fs_path_norm_abs(&parts, sys_type)) {
        M_list_str_destroy(parts);
        return M_FS_ERROR_GENERIC;
    }

    /* We're going to move the parts into base for processing. */
    base = M_list_str_create(M_LIST_STR_NONE);

    while (M_list_str_len(parts) > 0) {
        str_p = M_list_str_take_at(parts, 0);
        if (!(flags & M_FS_PATH_NORM_NOPARENT) && M_str_eq(str_p, "..")) {
            /* handle '..' is we should */
            M_fs_path_norm_remove_parent(&base, sys_type);
        } else if (!M_str_eq(str_p, ".")) {
            /* Hanlde anything other than '.' */
            M_list_str_insert(base, str_p);
            if (flags & M_FS_PATH_NORM_FOLLOWSYMLINKS) {
                ret = M_fs_path_norm_symlink(out, &base, parts, flags, sys_type, seen);
                /* We had an error or we followed the symlink and fully
                 * normalized the path. */
                if (ret != M_FS_ERROR_SUCCESS || (ret == M_FS_ERROR_SUCCESS && *out != NULL)) {
                    M_free(str_p);
                    M_list_str_destroy(base);
                    M_list_str_destroy(parts);
                    return ret;
                }
                /* Othwerwise we were sucessful but it wasn't a symlink so we're
                 * going to continue */
            }
        }
        M_free(str_p);
    }
    /* Everything should have been moved into base */
    M_list_str_destroy(parts);

    /* Handle special cases */
    len = M_list_str_len(base);
    /* Everything was removed so it must be a relative path. We can't return
     * nothing to return a . since we're looking at the current dir. */
    if (len == 0) {
        M_list_str_destroy(base);
        *out = M_strdup(".");
        return M_FS_ERROR_SUCCESS;
    }

    *out = M_fs_path_join_parts(base, sys_type);
    M_list_str_destroy(base);
    return M_FS_ERROR_SUCCESS;

}

/* Try to follow a path if it was an environment variable. Rewrite the dirs approperately. */
/* Try to follow a path as if it was a symlink. If it is a symlink rewrite the
 * dirs appropriatly.
 */
static M_fs_error_t M_fs_path_norm_symlink(char **out, M_list_str_t **base, const M_list_str_t *parts, M_fs_path_norm_t flags, M_fs_system_t sys_type, M_hash_strvp_t *seen)
{
    char         *path;
    char         *retpath = NULL;
    M_fs_error_t  ret;

    if (out == NULL || base == NULL || *base == NULL || M_list_str_len(*base) == 0 || parts == NULL) {
        return M_FS_ERROR_GENERIC;
    }
    *out = NULL;

    /* Turn our path components into a path. */
    path = M_fs_path_join_parts(*base, sys_type);
    if (path == NULL) {
        return M_FS_ERROR_INVALID;
    }

    ret = M_fs_path_readlink_int(&retpath, path, M_list_str_len(parts) == 0 ? M_TRUE : M_FALSE, flags, sys_type);
    M_free(path);

    /* Either we have a failure or a success where no symlink was followed. */
    if (ret != M_FS_ERROR_SUCCESS || (ret == M_FS_ERROR_SUCCESS && retpath == NULL)) {
        M_free(retpath);
        return ret;
    }

    /* Otherwise we followed a symlink and we have a new path. */

    /* Remove everything if the new path is an abs path. */
    if (M_fs_path_isabs(retpath, sys_type)) {
        M_list_str_remove_range(*base, 0, M_list_str_len(*base));
    /* Only remove the last part if the new path is relative because it's
     * relative to the last part. */
    } else {
        M_list_str_remove_at(*base, M_list_str_len(*base)-1);
    }
    /* Put our new path into the list of parts. */
    M_list_str_merge(base, M_fs_path_componentize_path(retpath, sys_type), M_TRUE);
    M_free(retpath);

    /* Merge our base with our remaining parts and normalize our new path.
     * We duplicate parts because the caller needs to handle destroying it. */
    M_list_str_merge(base, M_list_str_duplicate(parts), M_TRUE);
    path = M_fs_path_join_parts(*base, sys_type);
    /* Remove everything from base because we don't need it anymore. but we shouldn't destory things we don't own. */
    M_list_str_remove_range(*base, 0, M_list_str_len(*base));
    ret = M_fs_path_norm_int(out, path, flags, sys_type, seen);
    M_free(path);
    return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_path_norm(char **out, const char *path, M_uint32 flags, M_fs_system_t sys_type)
{
    M_hash_strvp_t *seen;
    M_fs_error_t    res;

    seen = M_hash_strvp_create(25, 75, M_HASH_STRVP_NONE, NULL);
    res = M_fs_path_norm_int(out, path, flags, sys_type, seen);
    M_hash_strvp_destroy(seen, M_FALSE);
    return res;
}
