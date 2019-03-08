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

#include <errno.h>

#include <mstdlib/mstdlib.h>
#include "fs/m_fs_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static size_t M_fs_unx_getpwgr_r_size(int name)
{
	long   llen;
	size_t len;
	llen = sysconf(name);
	/* NOTE: CentOS 6.6 appears to return a length of 1024, which
	 * has actually proven to be insufficient as the getgrgid_r will
	 * return ERANGE.  Lets make sure the minimum length we return
	 * is 16384. */
	if (llen < 16384) {
		len = 16384;
	} else {
		len = (size_t)llen;
	}
	return len;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

size_t M_fs_unx_getpw_r_size(void)
{
#ifndef _SC_GETPW_R_SIZE_MAX
	return 16384;
#else
	return M_fs_unx_getpwgr_r_size(_SC_GETPW_R_SIZE_MAX);
#endif
}

size_t M_fs_unx_getgr_r_size(void)
{
#ifndef _SC_GETGR_R_SIZE_MAX
	return 16384;
#else
	return M_fs_unx_getpwgr_r_size(_SC_GETGR_R_SIZE_MAX);
#endif
}

M_fs_error_t M_fs_error_from_syserr(int err)
{
	switch (err) {
#ifdef E2BIG
		case E2BIG:
			return M_FS_ERROR_INVALID;
#endif
#ifdef EACCES
		case EACCES:
			return M_FS_ERROR_PERMISSION;
#endif
#ifdef EDQUOT
		case EDQUOT:
			return M_FS_ERROR_QUOTA;
#endif
#ifdef EEXIST
		case EEXIST:
			return M_FS_ERROR_FILE_EXISTS;
#endif
#ifdef EFBIG
		case EFBIG:
			return M_FS_ERROR_FILE_2BIG;
#endif
#ifdef EIO
		case EIO:
			return M_FS_ERROR_IO;
#endif
#ifdef EISDIR
		case EISDIR:
			return M_FS_ERROR_ISDIR;
#endif
#ifdef ELOOP
		case ELOOP:
			return M_FS_ERROR_LINK_LOOP;
#endif
#ifdef EMFILE
		case EMFILE:
			return M_FS_ERROR_FILE_2MANY;
#endif
#ifdef EMLINK
		case EMLINK:
			return M_FS_ERROR_LINK_2MANY;
#endif
#ifdef ENAMETOOLONG
		case ENAMETOOLONG:
			return M_FS_ERROR_NAMETOOLONG;
#endif
#ifdef ENFILE
		case ENFILE:
			return M_FS_ERROR_FILE_2MANY;
#endif
#ifdef ENOENT
		case ENOENT:
			return M_FS_ERROR_DNE;
#endif
#ifdef ENOSYS
		case ENOSYS:
			return M_FS_ERROR_INVALID;
#endif
#ifdef ENOTDIR
		case ENOTDIR:
			return M_FS_ERROR_NOTDIR;
#endif
/* AIX defined ENOTEMPTY and EEXIST to the same value. */
#if defined(ENOTEMPTY) && (defined(EEXIST) && ENOTEMPTY != EEXIST)
		case ENOTEMPTY:
			return M_FS_ERROR_DIR_NOTEMPTY;
#endif
#ifdef ENOTSUP
		case ENOTSUP:
			return M_FS_ERROR_NOT_SUPPORTED;
#endif
#ifdef EPERM
		case EPERM:
			return M_FS_ERROR_PERMISSION;
#endif
#ifdef EROFS
		case EROFS:
			return M_FS_ERROR_READONLY;
#endif
#ifdef ESPIPE
		case ESPIPE:
			return M_FS_ERROR_SEEK;
#endif
#ifdef EXDEV
		case EXDEV:
			return M_FS_ERROR_NOT_SAMEDEV;
#endif
	}
	return M_FS_ERROR_GENERIC;
}
