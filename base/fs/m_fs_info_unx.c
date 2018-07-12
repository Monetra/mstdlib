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

#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include <mstdlib/mstdlib.h>
#include "fs/m_fs_int.h"
#include "fs/m_fs_int_unx.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Convert a unix mode (file perm) into a M_fs_perms_t. */
static M_fs_perms_t *M_fs_info_mode_to_perms(mode_t mode)
{
	M_fs_perms_t      *perms;
	M_fs_perms_mode_t  perms_mode;

	perms = M_fs_perms_create();

	/* user. */
	perms_mode = M_FS_PERMS_MODE_NONE;
	if (mode & S_IRUSR)
		perms_mode |= M_FS_PERMS_MODE_READ;
	if (mode & S_IWUSR)
		perms_mode |= M_FS_PERMS_MODE_WRITE;
	if (mode & S_IXUSR)
		perms_mode |= M_FS_PERMS_MODE_EXEC;
	M_fs_perms_set_mode(perms, perms_mode, M_FS_PERMS_WHO_USER, M_FS_PERMS_TYPE_EXACT);

	/* group. */
	perms_mode = M_FS_PERMS_MODE_NONE;
	if (mode & S_IRGRP)
		perms_mode |= M_FS_PERMS_MODE_READ;
	if (mode & S_IWGRP)
		perms_mode |= M_FS_PERMS_MODE_WRITE;
	if (mode & S_IXGRP)
		perms_mode |= M_FS_PERMS_MODE_EXEC;
	M_fs_perms_set_mode(perms, perms_mode, M_FS_PERMS_WHO_GROUP, M_FS_PERMS_TYPE_EXACT);

	/* other. */
	perms_mode = M_FS_PERMS_MODE_NONE;
	if (mode & S_IROTH)
		perms_mode |= M_FS_PERMS_MODE_READ;
	if (mode & S_IWOTH)
		perms_mode |= M_FS_PERMS_MODE_WRITE;
	if (mode & S_IXOTH)
		perms_mode |= M_FS_PERMS_MODE_EXEC;
	M_fs_perms_set_mode(perms, perms_mode, M_FS_PERMS_WHO_OTHER, M_FS_PERMS_TYPE_EXACT);

	return perms;
}

static M_fs_error_t M_fs_info_int(M_fs_info_t **info, struct stat *stbuf, M_uint32 flags)
{
	M_fs_perms_t  *perms;
	struct passwd  pwd;
	struct passwd *pwd_result;
	struct group   grp;
	struct group  *grp_result;
	char          *pg_buf;
	M_fs_error_t   res;
	size_t         pg_len;
	int            ret;

	/* If info was sent in as NULL then the we are only checking that the path exists. */
	if (info == NULL)
		return M_FS_ERROR_SUCCESS;
	*info = NULL;

	/* Fill in our M_fs_info_t. */
	*info = M_fs_info_create();

	/* Type. */
	if (S_ISDIR(stbuf->st_mode)) {
		M_fs_info_set_type(*info, M_FS_TYPE_DIR);
	} else if (S_ISLNK(stbuf->st_mode)) {
		M_fs_info_set_type(*info, M_FS_TYPE_SYMLINK);
	} else if (S_ISFIFO(stbuf->st_mode)) {
		M_fs_info_set_type(*info, M_FS_TYPE_PIPE);
	} else {
		M_fs_info_set_type(*info, M_FS_TYPE_FILE);
	}

	/* Basic info. */
	M_fs_info_set_size(*info, (stbuf->st_size >= 0) ? (M_uint64)stbuf->st_size : 0);
	M_fs_info_set_atime(*info, (M_time_t)stbuf->st_atime);
	M_fs_info_set_mtime(*info, (M_time_t)stbuf->st_mtime);
	M_fs_info_set_ctime(*info, (M_time_t)stbuf->st_ctime);

#	if defined(HAVE_ST_BIRTHTIME)
	M_fs_info_set_btime(*info, (M_time_t)stbuf->st_birthtime);
#	endif


	if (flags & M_FS_PATH_INFO_FLAGS_BASIC) {
		return M_FS_ERROR_SUCCESS;
	}

	/* User. */
#if defined(HAVE_GETPWUID_5)
	pg_len = M_fs_unx_getpw_r_size();
	pg_buf = M_malloc(pg_len);
	ret = getpwuid_r(stbuf->st_uid, &pwd, pg_buf, pg_len, &pwd_result);
	if (ret != 0 || pwd_result == NULL) {
		M_free(pg_buf);
		M_fs_info_destroy(*info);
		*info = NULL;
		if (ret != 0) {
			return M_fs_error_from_syserr(errno);
		}
		return M_FS_ERROR_GENERIC;
	}
	M_fs_info_set_user(*info, pwd.pw_name);
	M_free(pg_buf);
#elif defined(HAVE_GETPWUID_4)
	pg_len     = M_fs_unx_getpw_r_size();
	pg_buf     = M_malloc(pg_len);
	pwd_result = getpwuid_r(stbuf->st_uid, &pwd, pg_buf, pg_len);
	if (pwd_result == NULL) {
		M_free(pg_buf);
		M_fs_info_destroy(*info);
		*info = NULL;
		return M_fs_error_from_syserr(errno);
	}
	M_fs_info_set_user(*info, pwd.pw_name);
	M_free(pg_buf);
#else
	(void)pg_len;
	(void)pg_buf;
	(void)pwd;
	pwd_result = getpwuid(stbuf->st_uid);
	if (pwd_result == NULL) {
		M_fs_info_destroy(*info);
		*info = NULL;
		return M_fs_error_from_syserr(errno);
	}
	M_fs_info_set_user(*info, pwd_result->pw_name);
#endif

	/* Group. */
#if defined(HAVE_GETGRGID_5)
	pg_len = M_fs_unx_getgr_r_size();
	pg_buf = M_malloc(pg_len);
	ret = getgrgid_r(stbuf->st_gid, &grp, pg_buf, pg_len, &grp_result);
	if (ret != 0 || grp_result == NULL) {
		M_free(pg_buf);
		M_fs_info_destroy(*info);
		*info = NULL;
		if (ret != 0) {
			return M_fs_error_from_syserr(errno);
		}
		return M_FS_ERROR_GENERIC;
	}
	M_fs_info_set_group(*info, grp.gr_name);
	M_free(pg_buf);
#elif defined(HAVE_GETGRGID_4)
	pg_len     = M_fs_unx_getgr_r_size();
	pg_buf     = M_malloc(pg_len);
	grp_result = getgrgid_r(stbuf->st_gid, &grp, pg_buf, pg_len);
	if (grp_result == NULL) {
		M_free(pg_buf);
		M_fs_info_destroy(*info);
		*info = NULL;
		if (ret != 0) {
			return M_fs_error_from_syserr(errno);
		}
		return M_FS_ERROR_GENERIC;
	}
	M_fs_info_set_group(*info, grp.gr_name);
	M_free(pg_buf);
#else
	(void)pg_len;
	(void)pg_buf;
	(void)grp;
	grp_result = getgrgid(stbuf->st_gid);
	if (grp_result == NULL) {
		M_fs_info_destroy(*info);
		*info = NULL;
		return M_fs_error_from_syserr(errno);
	}
	M_fs_info_set_group(*info, grp_result->gr_name);
#endif

	/* Perms. */
	perms = M_fs_info_mode_to_perms((mode_t)stbuf->st_mode);
	if (perms == NULL) {
		M_fs_info_destroy(*info);
		*info = NULL;
		return M_FS_ERROR_GENERIC;
	}
	if ((res = M_fs_perms_set_user_int(perms, M_fs_info_get_user(*info), stbuf->st_uid)) != M_FS_ERROR_SUCCESS ||
		(res = M_fs_perms_set_group_int(perms, M_fs_info_get_group(*info), stbuf->st_gid)) != M_FS_ERROR_SUCCESS)
	{
		M_fs_perms_destroy(perms);
		M_fs_info_destroy(*info);
		*info = NULL;
		return res;
	}
	M_fs_info_set_perms(*info, perms);

	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_info(M_fs_info_t **info, const char *path, M_uint32 flags)
{
	char         *norm_path;
	struct stat   stbuf;
	M_fs_error_t  res;
	int           ret;
	M_bool        is_hidden;

	if (info != NULL)
		*info = NULL;

	if (path == NULL)
		return M_FS_ERROR_INVALID;

	/* Normalize the path. */
	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_HOME, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}
	is_hidden = M_fs_path_ishidden(norm_path, NULL);

	/* stat the path to get the info. */
	M_mem_set(&stbuf, 0, sizeof(stbuf));
	if (flags & M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS) {
		ret = stat(norm_path, &stbuf);
	} else {
		ret = lstat(norm_path, &stbuf);
	}
	M_free(norm_path);
	if (ret == -1) {
		return M_fs_error_from_syserr(errno);
	}

	res = M_fs_info_int(info, &stbuf, flags);

	if (res == M_FS_ERROR_SUCCESS && info != NULL)
		M_fs_info_set_hidden(*info, is_hidden);

	return res;
}

M_fs_error_t M_fs_info_file(M_fs_info_t **info, M_fs_file_t *fd, M_uint32 flags)
{
	struct stat stbuf;
	int         ret;

	if (info != NULL)
		*info = NULL;

	if (fd == NULL)
		return M_FS_ERROR_INVALID;

	if (info == NULL)
		return M_FS_ERROR_SUCCESS;

	M_mem_set(&stbuf, 0, sizeof(stbuf));
	ret = fstat(fd->fd, &stbuf);
	if (ret == -1)
		return M_fs_error_from_syserr(errno);

	return M_fs_info_int(info, &stbuf, flags);
}
