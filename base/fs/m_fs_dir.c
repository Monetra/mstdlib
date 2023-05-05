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
#  include <Sddl.h>
#else
#  include <errno.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * M_FS_ERROR_DNE
 * M_FS_ERROR_FILE_EXISTS
 * M_FS_ERROR_ISDIR
 */
static M_fs_error_t M_fs_dir_mkdir_dir_status(const char *path)
{
	M_fs_info_t *info = NULL;

	if (M_fs_info(&info, path, M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS|M_FS_PATH_INFO_FLAGS_BASIC) != M_FS_ERROR_SUCCESS) {
		return M_FS_ERROR_DNE;
	}

	if (M_fs_info_get_type(info) != M_FS_TYPE_DIR) {
		M_fs_info_destroy(info);
		return M_FS_ERROR_FILE_EXISTS;
	}

	M_fs_info_destroy(info);
	return M_FS_ERROR_ISDIR;
}

#ifdef _WIN32
static M_fs_error_t M_fs_dir_mkdir_sys(const char *path, M_fs_perms_t *perms)
{
	PSID                everyone_sid = NULL;
	PACL                acl          = NULL;
	M_bool              sa_set       = M_FALSE;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;
	M_fs_error_t        res;

	if (perms != NULL) {
		if (!ConvertStringSidToSid("S-1-1-0", &everyone_sid)) {
			everyone_sid = NULL;
		}
		res = M_fs_perms_to_security_attributes(perms, everyone_sid, &acl, &sa, &sd);
		if (res != M_FS_ERROR_SUCCESS) {
			LocalFree(everyone_sid);
			return res;
		}
		sa_set = M_TRUE;
	}

	res = M_FS_ERROR_SUCCESS;
	if (!CreateDirectory(path, sa_set?&sa:NULL)) {
		res = M_fs_error_from_syserr(GetLastError());
	}

	LocalFree(everyone_sid);
	LocalFree(acl);
	return res;
}
#else
static M_fs_error_t M_fs_dir_mkdir_sys(const char *path, const M_fs_perms_t *perms)
{
	mode_t mode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH;

	if (perms != NULL) {
		mode = M_fs_perms_to_mode(perms, M_FALSE);
	}

	if (mkdir(path, mode) != 0) {
		return M_fs_error_from_syserr(errno);
	}

	return M_FS_ERROR_SUCCESS;
}
#endif

M_fs_error_t M_fs_dir_mkdir(const char *path, M_bool create_parents, M_fs_perms_t *perms)
{
	char         *norm_path;
	char         *base_dir;
	M_list_str_t *dirs;
	M_fs_error_t  res;

	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_HOME|M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		return res;
	}

	dirs = M_fs_path_componentize_path(norm_path, M_FS_SYSTEM_AUTO);
	M_list_str_remove_at(dirs, M_list_str_len(dirs)-1);
	base_dir = M_fs_path_join_parts(dirs, M_FS_SYSTEM_AUTO);
	M_list_str_destroy(dirs);

	if (base_dir == NULL || *base_dir == '\0') {
		M_free(base_dir);
		M_free(norm_path);
		return M_FS_ERROR_GENERIC;
	}

	res = M_fs_dir_mkdir_dir_status(norm_path);
	if (res != M_FS_ERROR_DNE) {
		M_free(base_dir);
		M_free(norm_path);
		return res;
	}

	res = M_fs_dir_mkdir_dir_status(base_dir);
	if (res == M_FS_ERROR_DNE && create_parents) {
		res = M_fs_dir_mkdir(base_dir, create_parents, perms);
	}
	if (res == M_FS_ERROR_ISDIR || res == M_FS_ERROR_SUCCESS) {
		res = M_fs_dir_mkdir_sys(norm_path, perms);
	}

	M_free(base_dir);
	M_free(norm_path);
	return res;
}
