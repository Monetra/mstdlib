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
#include <fcntl.h>
#include <unistd.h>

#include <mstdlib/mstdlib.h>
#include "fs/m_fs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_file_open_sys(M_fs_file_t **fd, const char *path, M_uint32 mode, const M_fs_perms_t *perms)
{
	M_fs_file_t  *myfd      = NULL;
	int           o_flags = 0;
	mode_t        o_mode  = 0;
	char         *norm_path = NULL;
	M_fs_error_t  res;

	if (fd == NULL || path == NULL || *path == '\0') {
		return M_FS_ERROR_INVALID;
	}

	*fd  = NULL;

	/* open flags */
	/* Mode */
	if ((mode & (M_FS_FILE_MODE_READ|M_FS_FILE_MODE_WRITE)) == (M_FS_FILE_MODE_READ|M_FS_FILE_MODE_WRITE)) {
		o_flags = O_RDWR;
	} else if (mode & M_FS_FILE_MODE_WRITE) {
		o_flags = O_WRONLY;
	} else if (mode & M_FS_FILE_MODE_READ) {
		o_flags = O_RDONLY;
	} else {
		return M_FS_ERROR_INVALID;
	}
	/* Behavior modifiers */
	if (!(mode & M_FS_FILE_MODE_NOCREATE)) {
		o_flags |= O_CREAT;
		if (perms == NULL) {
			o_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
		} else {
			o_mode = M_fs_perms_to_mode(perms, M_FALSE);
		}
	}
	if (mode & M_FS_FILE_MODE_APPEND)
		o_flags |= O_APPEND;
	if (mode & M_FS_FILE_MODE_OVERWRITE)
		o_flags |= O_TRUNC;
#ifdef O_CLOEXEC
	if (!(mode & M_FS_FILE_MODE_NOCLOSEEXEC))
		o_flags |= O_CLOEXEC;
#endif

	/* Normalize the path following the resolution process outlined in path_resolution(7). */
	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESDIR, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	myfd     = M_malloc_zero(sizeof(*myfd));
	myfd->fd = open(norm_path, o_flags, o_mode);
	if (myfd->fd == -1) {
		M_free(norm_path);
		M_free(myfd);
		return M_fs_error_from_syserr(errno);
	}
	M_free(norm_path);

#ifndef O_CLOEXEC
	if (!(mode & M_FS_FILE_MODE_NOCLOSEEXEC)) {
		int flags = fcntl(myfd->fd, F_GETFD);
		if (flags != -1) {
			fcntl(myfd->fd, F_SETFD, flags | FD_CLOEXEC);
		}
	}
#endif

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
			myfd->fd = STDIN_FILENO;
			break;
		case M_FS_IOSTREAM_OUT:
			myfd->fd = STDOUT_FILENO;
			break;
		case M_FS_IOSTREAM_ERR:
			myfd->fd = STDERR_FILENO;
			break;
		default:
			M_free(myfd);
			return M_FS_ERROR_INVALID;
	}

	*fd = myfd;
	return M_FS_ERROR_SUCCESS;
}

void M_fs_file_close_sys(M_fs_file_t *fd)
{
	if (fd == NULL) {
		return;
	}

	if (fd->fd != -1) {
		close(fd->fd);
		fd->fd = -1;
	}
	M_free(fd);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_file_read_sys(M_fs_file_t *fd, unsigned char *buf, size_t buf_len, size_t *read_len)
{
	ssize_t ret;

	if (buf == NULL || buf_len == 0 || read_len == NULL || fd == NULL || fd->fd == -1) {
		return M_FS_ERROR_INVALID;
	}
	*read_len = 0;

	ret = read(fd->fd, buf, buf_len);
	if (ret == -1) {
		return M_fs_error_from_syserr(errno);
	}
	*read_len = (size_t)ret;

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_write_sys(M_fs_file_t *fd, const unsigned char *buf, size_t count, size_t *wrote_len)
{
	ssize_t ret;

	if (buf == NULL || count == 0 || wrote_len == NULL || fd == NULL || fd->fd == -1) {
		return M_FS_ERROR_INVALID;
	}
	*wrote_len = 0;

	ret = write(fd->fd, buf, count);
	if (ret == -1) {
		return M_fs_error_from_syserr(errno);
	}
	*wrote_len = (size_t)ret;

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_seek_sys(M_fs_file_t *fd, M_int64 offset, M_fs_file_seek_t from)
{
	off_t ret;
	int   whence;

	if (fd == NULL || fd->fd == -1)
		return M_FS_ERROR_INVALID;

	/* XXX: Instead of checking that we're not larger for all seek types we could split the value and run
 	 * seek multiple times. */
	if (sizeof(off_t) == 4 && (offset > M_INT32_MAX || offset < M_INT32_MIN))
		return M_FS_ERROR_SEEK;
	
	if (from == M_FS_FILE_SEEK_END) {
		whence = SEEK_END;
	} else if (from == M_FS_FILE_SEEK_CUR) {
		whence = SEEK_CUR;
	} else {
		whence = SEEK_SET;
	}

	ret = lseek(fd->fd, (off_t)offset, whence);
	if (ret == -1) {
		return M_fs_error_from_syserr(errno);
	}

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_file_fsync_sys(M_fs_file_t *fd)
{
	if (fd == NULL || fd->fd == -1)
		return M_FS_ERROR_INVALID;

	if (fsync(fd->fd) == -1)
		return M_fs_error_from_syserr(errno);
	return M_FS_ERROR_SUCCESS;	
}
