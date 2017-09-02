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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_error_from_syserr(DWORD err)
{
	switch (err) {
		case ERROR_SUCCESS:
			return M_FS_ERROR_SUCCESS;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return M_FS_ERROR_DNE;
		case ERROR_TOO_MANY_OPEN_FILES:
		/* case ERROR_TOO_MANY_DESCRIPTORS: */
		case ERROR_NO_MORE_FILES:
			return M_FS_ERROR_FILE_2MANY;
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
			return M_FS_ERROR_PERMISSION;
		case ERROR_SEEK:
			return M_FS_ERROR_SEEK;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			return M_FS_ERROR_FILE_EXISTS;
		case ERROR_INVALID_PARAMETER:
			return M_FS_ERROR_INVALID;
		case ERROR_DIR_NOT_EMPTY:
			return M_FS_ERROR_DIR_NOTEMPTY;
		case ERROR_FILENAME_EXCED_RANGE:
			return M_FS_ERROR_NAMETOOLONG;
#if 0
		case ERROR_FILE_TOO_LARGE:
			return M_FS_ERROR_FILE_2BIG;
		case ERROR_DIRECTORY_NOT_SUPPORTED:
			return M_FS_ERROR_ISDIR;
#endif
		case ERROR_DIRECTORY:
			return M_FS_ERROR_NOTDIR;
		case ERROR_NOT_SUPPORTED:
			return M_FS_ERROR_NOT_SUPPORTED;
		case ERROR_READ_FAULT:
		case ERROR_WRITE_FAULT:
			return M_FS_ERROR_IO;
		case ERROR_WRITE_PROTECT:
			return M_FS_ERROR_READONLY;
		case ERROR_NOT_SAME_DEVICE:
			return M_FS_ERROR_NOT_SAMEDEV;
	}
	return M_FS_ERROR_GENERIC;
}
