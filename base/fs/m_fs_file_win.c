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
#include "time/m_time_int.h"
#include "platform/m_platform.h"

#include <Lmcons.h>
#include <Sddl.h>


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_file_open_sys(M_fs_file_t **fd, const char *path, M_uint32 mode, const M_fs_perms_t *perms)
{
	M_fs_file_t          *myfd           = NULL;
	M_fs_perms_t         *eperms         = NULL;
	DWORD                 desired_access = 0;
	DWORD                 creation       = 0;
	char                 *norm_path      = NULL;
	PSID                  everyone_sid   = NULL;
	PACL                  acl            = NULL;
	M_bool                sa_set         = M_FALSE;
	SECURITY_ATTRIBUTES   sa;
	SECURITY_DESCRIPTOR   sd;
	M_fs_error_t          res;
	FILETIME              ft;
	M_bool                set_ft         = M_FALSE;

	if (fd == NULL || path == NULL || *path == '\0') {
		return M_FS_ERROR_INVALID;
	}

	*fd  = NULL;

	/* open flags */
	/* Mode */
	if (!(mode & (M_FS_FILE_MODE_READ|M_FS_FILE_MODE_WRITE))) {
		return M_FS_ERROR_INVALID;
	}
   	if (mode & M_FS_FILE_MODE_READ) {
		desired_access |= GENERIC_READ;
	}
	if (mode & M_FS_FILE_MODE_APPEND) {
		desired_access |= FILE_APPEND_DATA;
	} else if (mode & M_FS_FILE_MODE_WRITE) {
		desired_access |= GENERIC_WRITE;
	}
	/* Behavior modifiers */
	if ((mode & (M_FS_FILE_MODE_OVERWRITE|M_FS_FILE_MODE_NOCREATE)) == (M_FS_FILE_MODE_OVERWRITE|M_FS_FILE_MODE_NOCREATE)) {
		creation |= TRUNCATE_EXISTING;
	} else if (mode & M_FS_FILE_MODE_OVERWRITE && !(mode & M_FS_FILE_MODE_NOCREATE)) {
		creation |= CREATE_ALWAYS;
	} else if (!(mode & M_FS_FILE_MODE_OVERWRITE) && mode & M_FS_FILE_MODE_NOCREATE) {
		creation |= OPEN_EXISTING;
	} else {
		creation |= OPEN_ALWAYS;
	}

	/* Normalize the path following the resolution process outlined in path_resolution(7). */
	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESDIR, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	/* Set the permission information. */
	if (perms != NULL && creation & (OPEN_ALWAYS|CREATE_ALWAYS)) {
		/* Get the everyone SID. This needs to remain valid until after CreateFile is called */
		if (!ConvertStringSidToSid("S-1-1-0", &everyone_sid)) {
			everyone_sid = NULL;
		}
		/* sid read from perms needs to persist until after CreateFile is called
 		 * because the sid from the perms which is loaded into the sd is referenced
		 * not copied. */
		eperms = M_fs_perms_dup(perms);
		res = M_fs_perms_to_security_attributes(eperms, everyone_sid, &acl, &sa, &sd);
		if (res != M_FS_ERROR_SUCCESS) {
			M_fs_perms_destroy(eperms);
			LocalFree(everyone_sid);
			M_free(norm_path);
			return res;
		}
		sa_set = M_TRUE;
	}

	/* Windows uses something called "file system tunneling" when creating files.
	 * When you delete or rename a file then create a new file with the old name
	 * the creation time and a few other attributes will be retained from the old file.
	 *
	 * The rational is for an old file save paradigm:
	 * 1. Save the data to a new file.
	 * 2. Delete the current file.
	 * 3. Rename the new file to the current file name.
	 *
	 * This paradigm was widely used before the advent of journaling file systems. A crash
	 * While saving to an existing file could result in data loss. This minimized the loss
	 * because the old data was retained until the new data was written to disk. A crash
	 * while writing would only lose the changes. If there was a crash during delete the
	 * new data was still present on disk and the application could recover. A rename even
	 * on those older file systems was atomic and should never result in data loss. This
	 * pattern is unnecessary today because this behavior happens internally to the filesystem.
	 *
	 * Tunneling prevents the creation time from changing with every save. Making
	 * the process seamless and appear like a file is being saved instead of created,
	 * deleted, renamed.
	 *
	 * This causes significant issues when doing log rotation because the creation time never
	 * changes. If you're rotating on a 7 day period, for example, every time you check
	 * the file would appear older than 7 days even if it was rotated seconds ago.
	 *
	 * Since this behavior doesn't exist on other OS's we're going to "disable" tunneling.
	 * Unfortunately, the only way to do so is a registry setting, which we won't touch.
	 * CreateFile won't tell us if a file was created or opened. The only way we can
	 * do this is by checking if the file exists when we have flags telling CreateFile to
	 * create if the file does exist. If it doesn't exist we'll call SetFileTime to the
	 * current time to override the tunneling behavior.
	 *
	 * Since there are two steps it's possible the file is created by something else
	 * between check and create. Since this time is so close setting the file time
	 * shouldn't impact anything.
	 */
	if (creation & CREATE_ALWAYS && !M_fs_perms_can_access(norm_path, 0)) {
		set_ft = M_TRUE;
	}

	/* Try to open/create the file. */
	myfd     = M_malloc_zero(sizeof(*myfd));
	myfd->fd = CreateFile(norm_path, desired_access, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, sa_set?&sa:NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);

	M_fs_perms_destroy(eperms);
	LocalFree(everyone_sid);
	LocalFree(acl);

	if (myfd->fd == INVALID_HANDLE_VALUE) {
		M_free(norm_path);
		M_free(myfd);
		return M_fs_error_from_syserr(GetLastError());
	}

	/* File was created. Update the time to disable tunneling. */
	if (set_ft) {
		M_time_to_filetime(M_time(), &ft);
		SetFileTime(myfd->fd, &ft, &ft, &ft);
	}

	M_free(norm_path);
	*fd = myfd;
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_open_iostream(M_fs_file_t **fd, M_fs_iostream_t stream)
{
	M_fs_file_t *myfd;

	if (fd == NULL)
		return M_FS_ERROR_INVALID;

	*fd  = NULL;
	myfd = M_malloc_zero(sizeof(*myfd));

	switch (stream) {
		case M_FS_IOSTREAM_IN:
			myfd->fd = GetStdHandle(STD_INPUT_HANDLE);
			break;
		case M_FS_IOSTREAM_OUT:
			myfd->fd = GetStdHandle(STD_OUTPUT_HANDLE);
			break;
		case M_FS_IOSTREAM_ERR:
			myfd->fd = GetStdHandle(STD_ERROR_HANDLE);
			break;
		default:
			M_free(myfd);
			return M_FS_ERROR_INVALID;
	}

	if (myfd->fd == INVALID_HANDLE_VALUE) {
		M_free(myfd);
		return M_fs_error_from_syserr(GetLastError());
	}

	*fd = myfd;
	return M_FS_ERROR_SUCCESS;
}

void M_fs_file_close_sys(M_fs_file_t *fd)
{
	if (fd == NULL) {
		return;
	}

	if (fd->fd != INVALID_HANDLE_VALUE) {
		CloseHandle(fd->fd);
		fd->fd = INVALID_HANDLE_VALUE;
	}
	M_free(fd);
}

M_fs_error_t M_fs_file_read_sys(M_fs_file_t *fd, unsigned char *buf, size_t buf_len, size_t *read_len)
{
	DWORD dread_len;
	DWORD dbuf_len;

	if (fd == NULL || fd->fd == INVALID_HANDLE_VALUE || buf == NULL || buf_len == 0 || read_len == NULL) {
		return M_FS_ERROR_INVALID;
	}

	if (!M_win32_size_t_to_dword(buf_len, &dbuf_len))
		return M_FS_ERROR_INVALID;

	if (!ReadFile(fd->fd, buf, dbuf_len, &dread_len, NULL)) {
		*read_len = 0;
		return M_fs_error_from_syserr(GetLastError());
	}
	*read_len = dread_len;

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_write_sys(M_fs_file_t *fd, const unsigned char *buf, size_t count, size_t *wrote_len)
{
	DWORD dwrote_len;
	DWORD dcount;

	if (fd == NULL || fd->fd == INVALID_HANDLE_VALUE || buf == NULL || count == 0 || wrote_len == NULL) {
		return M_FS_ERROR_INVALID;
	}

	if (!M_win32_size_t_to_dword(count, &dcount))
		return M_FS_ERROR_INVALID;

	if (!WriteFile(fd->fd, buf, dcount, &dwrote_len, NULL)) {
		*wrote_len = 0;
		return M_fs_error_from_syserr(GetLastError());
	}
	*wrote_len = dwrote_len;

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_seek_sys(M_fs_file_t *fd, M_int64 offset, M_fs_file_seek_t from)
{
	LARGE_INTEGER move;
	DWORD         method;

	if (fd == NULL || fd->fd == INVALID_HANDLE_VALUE) { 
		return M_FS_ERROR_INVALID;
	}

	if (from == M_FS_FILE_SEEK_BEGIN) {
		method = FILE_BEGIN;
	} else if (from == M_FS_FILE_SEEK_CUR) {
		method = FILE_CURRENT;
	} else if (from == M_FS_FILE_SEEK_END) {
		method = FILE_END;
	} else {
		return M_FS_ERROR_INVALID;
	}

	move.QuadPart = offset;
	if (!SetFilePointerEx(fd->fd, move, NULL, method)) {
		return M_fs_error_from_syserr(GetLastError());
	}

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_fsync_sys(M_fs_file_t *fd)
{
	if (fd == NULL || fd->fd == INVALID_HANDLE_VALUE) { 
		return M_FS_ERROR_INVALID;
	}

	if (!FlushFileBuffers(fd->fd)) {
		return M_fs_error_from_syserr(GetLastError());
	}
	return M_FS_ERROR_SUCCESS;	
}
