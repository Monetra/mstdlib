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
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#include <mstdlib/mstdlib.h>
#include "fs/m_fs_int.h"
#include "fs/m_fs_int_unx.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Inline functions for repetitive tasks.
 *
 * A number of operations need to be performed multiple times where the logic is the
 * same but the parameters are different. For example, modifying user/group/other perms
 * and dealing with directory override user/group/other perms.
 *
 * Instead of having the same logic upwards of 6 times in a single function where the
 * variables are the only changes we have generic inline functions which take the pointers
 * to the variables to operate on. So the logic is reused and the caller just passes in
 * what to apply the logic to.
 */

/* Note:
 *    All ~ operations are cast to mode_t because without the cast we get:
 *    warning: negative integer implicitly converted to unsigned type [-Wsign-conversion] */
static __inline__ mode_t M_fs_perms_update_mode_from_perm_part(mode_t mode, const M_bool *isdir,
		const M_bool *p_set, const M_bool *p_dir_set,
		const M_fs_perms_mode_t *p_mode, const M_fs_perms_mode_t *p_dir_mode,
		const M_fs_perms_type_t *p_type, const M_fs_perms_type_t *p_dir_type,
		mode_t s_read, mode_t s_write, mode_t s_exec)
{
	const M_fs_perms_mode_t *mymode;
	const M_fs_perms_type_t *mytype;
	M_bool                isset = M_FALSE;

	if (*isdir && *p_dir_set) {
		isset  = M_TRUE;
		mymode = p_dir_mode;
		mytype = p_dir_type;
	} else if (*p_set) {
		isset  = M_TRUE;
		mymode = p_mode;
		mytype = p_type;
	}

	if (!isset) {
		return mode;
	}

	switch (*mytype) {
		case M_FS_PERMS_TYPE_EXACT:
			mode &= ~(mode_t)(s_read|s_write|s_exec);
		case M_FS_PERMS_TYPE_ADD:
			if (*mymode & M_FS_PERMS_MODE_READ)
				mode |= s_read;
			if (*mymode & M_FS_PERMS_MODE_WRITE)
				mode |= s_write;
			if (*mymode & M_FS_PERMS_MODE_EXEC)
				mode |= s_exec;
			break;
		case M_FS_PERMS_TYPE_REMOVE:
			if (*mymode & M_FS_PERMS_MODE_READ)
				mode &= ~(mode_t)s_read;
			if (*mymode & M_FS_PERMS_MODE_WRITE)
				mode &= ~(mode_t)s_write;
			if (*mymode & M_FS_PERMS_MODE_EXEC)
				mode &= ~(mode_t)s_exec;
			break;
	}

	return mode;
}

static __inline__ mode_t M_fs_perms_to_mode_part(const M_bool *isdir,
		const M_bool *p_set, const M_bool *p_dir_set,
		const M_fs_perms_mode_t *p_mode, const M_fs_perms_mode_t *p_dir_mode,
		const M_fs_perms_type_t *p_type, const M_fs_perms_type_t *p_dir_type,
		mode_t s_read, mode_t s_write, mode_t s_exec)
{
	const M_fs_perms_mode_t *mymode;
	const M_fs_perms_type_t *mytype;
	mode_t                mode  = 0;
	M_bool                isset = M_FALSE;

	if (*isdir && *p_dir_set) {
		isset  = M_TRUE;
		mymode = p_dir_mode;
		mytype = p_dir_type;
	} else if (*p_set) {
		isset  = M_TRUE;
		mymode = p_mode;
		mytype = p_type;
	}

	if (isset && (*mytype == M_FS_PERMS_TYPE_EXACT || *mytype == M_FS_PERMS_TYPE_ADD)) {
		if (*mymode & M_FS_PERMS_MODE_READ)
			mode |= s_read;
		if (*mymode & M_FS_PERMS_MODE_WRITE)
			mode |= s_write;
		if (*mymode & M_FS_PERMS_MODE_EXEC)
			mode |= s_exec;
	}

	return mode;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static mode_t M_fs_perms_update_mode_from_perms(mode_t mode, const M_fs_perms_t *perms, M_bool isdir)
{
	mode_t mymode = 0;

	mymode |= M_fs_perms_update_mode_from_perm_part(mode, &isdir,
		&(perms->user_set), &(perms->dir_user_set),
		&(perms->user_mode), &(perms->dir_user_mode),
		&(perms->user_type), &(perms->dir_user_type),
		S_IRUSR, S_IWUSR, S_IXUSR);
	mymode |= M_fs_perms_update_mode_from_perm_part(mode, &isdir,
		&(perms->group_set), &(perms->dir_group_set),
		&(perms->group_mode), &(perms->dir_group_mode),
		&(perms->group_type), &(perms->dir_group_type),
		S_IRGRP, S_IWGRP, S_IXGRP);
	mymode |= M_fs_perms_update_mode_from_perm_part(mode, &isdir,
		&(perms->other_set), &(perms->dir_other_set),
		&(perms->other_mode), &(perms->dir_other_mode),
		&(perms->other_type), &(perms->dir_other_type),
		S_IROTH, S_IWOTH, S_IXOTH);
	return mymode;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * m_perms_unx.h */

mode_t M_fs_perms_to_mode(const M_fs_perms_t *perms, M_bool isdir)
{
	mode_t mode = 0;

	mode |= M_fs_perms_to_mode_part(&isdir,
		&(perms->user_set), &(perms->dir_user_set),
		&(perms->user_mode), &(perms->dir_user_mode),
		&(perms->user_type), &(perms->dir_user_type),
		S_IRUSR, S_IWUSR, S_IXUSR);
	mode |= M_fs_perms_to_mode_part(&isdir,
		&(perms->group_set), &(perms->dir_group_set),
		&(perms->group_mode), &(perms->dir_group_mode),
		&(perms->group_type), &(perms->dir_group_type),
		S_IRGRP, S_IWGRP, S_IXGRP);
	mode |= M_fs_perms_to_mode_part(&isdir,
		&(perms->other_set), &(perms->dir_other_set),
		&(perms->other_mode), &(perms->dir_other_mode),
		&(perms->other_type), &(perms->dir_other_type),
		S_IROTH, S_IWOTH, S_IXOTH);

	return mode;
}

M_fs_error_t M_fs_perms_set_user_int(M_fs_perms_t *perms, const char *user, uid_t uid)
{
	M_free(perms->user);
	perms->user = M_strdup(user);
	perms->uid  = uid;
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_group_int(M_fs_perms_t *perms, const char *group, gid_t gid)
{
	M_free(perms->group);
	perms->group = M_strdup(group);
	perms->gid   = gid;
	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * m_perms.h */

/* NOTE:
 *   chown takes -1 for uid and gid to signify do not change. However, uid_t and gid_t are
 *   unsigned type so we get a complier warning which is why the -1 is cast. */
M_fs_error_t M_fs_perms_set_perms(const M_fs_perms_t *perms, const char *path)
{
	M_fs_info_t  *info;
	char         *norm_path;
	M_fs_error_t  res;
	mode_t        mode = 0;

	if (perms == NULL || path == NULL) {
		return M_FS_ERROR_INVALID;
	}

	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}
	res = M_fs_info(&info, norm_path, M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	mode = M_fs_perms_to_mode(M_fs_info_get_perms(info), M_FALSE);
	mode = M_fs_perms_update_mode_from_perms(mode, perms, (M_fs_info_get_type(info)==M_FS_TYPE_DIR)?M_TRUE:M_FALSE);
	if (chmod(norm_path, mode) == -1) {
		M_fs_info_destroy(info);
		M_free(norm_path);
		return M_fs_error_from_syserr(errno);
	}

	if (perms->user != NULL || perms->group != NULL) {
		if (chown(norm_path,
				(perms->user != NULL)?perms->uid:(uid_t)-1,
				(perms->group != NULL)?perms->gid:(gid_t)-1)
			== -1)
		{
			M_fs_info_destroy(info);
			M_free(norm_path);
			return M_fs_error_from_syserr(errno);
		}
	}

	M_fs_info_destroy(info);
	M_free(norm_path);
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_perms_file(const M_fs_perms_t *perms, M_fs_file_t *fd)
{
	M_fs_info_t  *info;
	M_fs_error_t  res;
	mode_t        mode = 0;

	if (perms == NULL || fd == NULL) {
		return M_FS_ERROR_INVALID;
	}

	res = M_fs_info_file(&info, fd, M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS);
	if (res != M_FS_ERROR_SUCCESS) {
		return res;
	}

	mode = M_fs_perms_to_mode(M_fs_info_get_perms(info), M_FALSE);
	mode = M_fs_perms_update_mode_from_perms(mode, perms, (M_fs_info_get_type(info)==M_FS_TYPE_DIR)?M_TRUE:M_FALSE);
	if (fchmod(fd->fd, mode) == -1) {
		M_fs_info_destroy(info);
		return M_fs_error_from_syserr(errno);
	}

	if (perms->user != NULL || perms->group != NULL) {
		if (fchown(fd->fd,
				(perms->user != NULL)?perms->uid:(uid_t)-1,
				(perms->group != NULL)?perms->gid:(gid_t)-1)
			== -1)
		{
			M_fs_info_destroy(info);
			return M_fs_error_from_syserr(errno);
		}
	}

	M_fs_info_destroy(info);
	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_perms_can_access(const char *path, M_uint32 mode)
{
	char         *norm_path;
	M_fs_error_t  res;
	int           access_mode = 0;

	if (path == NULL) {
		return M_FS_ERROR_INVALID;
	}

	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	if (mode == 0)
		access_mode = F_OK;
	if (mode & M_FS_PERMS_MODE_READ)
		access_mode |= R_OK;
	if (mode & M_FS_PERMS_MODE_WRITE)
		access_mode |= W_OK;
	if (mode & M_FS_PERMS_MODE_EXEC)
		access_mode |= X_OK;

	if (access(norm_path, access_mode) == -1) {
		M_free(norm_path);
		return M_fs_error_from_syserr(errno);
	}
	M_free(norm_path);

	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_perms_set_user(M_fs_perms_t *perms, const char *user)
{
	struct passwd  p;
	struct passwd *p_result;
	char          *buf;
	size_t         buf_len;
	M_fs_error_t   res;
	int            ret;

	if (perms == NULL) {
		return M_FS_ERROR_INVALID;
	}

	if (user == NULL || *user == '\0') {
		M_free(perms->user);
		perms->user = NULL;
		return M_FS_ERROR_SUCCESS;
	}

#if defined(HAVE_GETPWNAM_5)
	buf_len = M_fs_unx_getpw_r_size();
	buf     = M_malloc(buf_len);
	ret     = getpwnam_r(user, &p, buf, buf_len, &p_result);
	if (ret != 0 || p_result == NULL) {
		M_free(buf);
		return M_FS_ERROR_INVALID;
	}
	res = M_fs_perms_set_user_int(perms, user, p.pw_uid);
	M_free(buf);
	return res;
#elif defined(HAVE_GETPWNAM_4)
	(void)ret;
	buf_len  = M_fs_unx_getpw_r_size();
	buf      = M_malloc(buf_len);
	p_result = getpwnam_r(user, &p, buf, buf_len);
	if (p_result == NULL) {
		M_free(buf);
		return M_FS_ERROR_INVALID;
	}
	res = M_fs_perms_set_user_int(perms, user, p.pw_uid);
	M_free(buf);
	return res;
#else
	(void)buf_len;
	(void)buf;
	(void)p;
	(void)res;
	(void)ret;
	p_result = getpwnam(user);
	if (p_result == NULL) {
		return M_FS_ERROR_INVALID;
	}
	return M_fs_perms_set_user_int(perms, user, p_result->pw_uid);
#endif
}

M_fs_error_t M_fs_perms_set_group(M_fs_perms_t *perms, const char *group)
{
	struct group  g;
	struct group *g_result;
	char          *buf;
	size_t         buf_len;
	M_fs_error_t   res;
	int            ret;

	if (perms == NULL) {
		return M_FS_ERROR_INVALID;
	}

	if (group == NULL) {
		M_free(perms->group);
		perms->group = NULL;
		return M_FS_ERROR_SUCCESS;
	}

#if defined(HAVE_GETGRNAM_5)
	buf_len = M_fs_unx_getgr_r_size();
	buf     = M_malloc(buf_len);
	ret     = getgrnam_r(group, &g, buf, buf_len, &g_result);
	if (ret != 0 || g_result == NULL) {
		M_free(buf);
		return M_FS_ERROR_INVALID;
	}
	res = M_fs_perms_set_group_int(perms, group, g.gr_gid);
	M_free(buf);
	return res;
#elif defined(HAVE_GETGRNAM_4)
	(void)ret;
	buf_len  = M_fs_unx_getgr_r_size();
	buf      = M_malloc(buf_len);
	g_result = getgrnam_r(group, &g, buf, buf_len);
	if (g_result == NULL) {
		M_free(buf);
		return M_FS_ERROR_INVALID;
	}
	res = M_fs_perms_set_group_int(perms, group, g.gr_gid);
	M_free(buf);
	return res;
#else
	(void)buf_len;
	(void)buf;
	(void)g;
	(void)res;
	(void)ret;
	g_result = getgrnam(group);
	if (g_result == NULL) {
		return M_FS_ERROR_INVALID;
	}
	return M_fs_perms_set_group_int(perms, group, g_result->gr_gid);
#endif
}
