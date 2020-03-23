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

#ifndef __M_FS_H__
#define __M_FS_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_time.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_fs File System
 *  \ingroup mstdlib_base
 *
 * File sytem routines.
 *
 * Example (check if a file exists):
 *
 * \code{.c}
 *     if (M_fs_perms_can_access("/file.txt", 0) == M_FS_ERROR_SUCCESS) {
 *         M_printf("path exists\n");
 *     } else {
 *         M_printf("path does not exist\n");
 *     } 
 * \endcode
 *
 * Example (information about a file or directory):
 *
 * \code{.c}
 *     M_fs_info_t *info = NULL;
 *
 *     if (M_fs_info(&info, "/file.txt", M_FS_PATH_INFO_FLAGS_BASIC) == M_FS_ERROR_SUCCESS) {
 *         M_printf("user='%s'\n", M_fs_info_get_user(info));
 *     } else {
 *         M_printf("Failed to get file information\n");
 *     } 
 *     M_fs_info_destroy(info);
 * \endcode
 *
 * Example (normalize path):
 *
 * \code{.c}
 *     const char *p1  = "./abc def/../xyz/./1 2 3/./xyr/.";
 *     const char *n1  = "xyz/1 2 3/xyr";
 *     const char *p2  = "C:\\\\var\\log\\.\\mysql\\\\\\5.1\\..\\..\\mysql.log";
 *     const char *n2  = "C:\\var\\log\\mysql.log";
 *     char       *out = NULL;
 *
 *     if (M_fs_path_norm(&out, p1, M_FS_PATH_NORM_NONE, M_FS_SYSTEM_UNIX) == M_FS_ERROR_SUCCESS) {
 *         if (M_str_eq(out, n1)) {
 *             M_printf("p1 normalized correctly\n")
 *         } else {
 *             M_printf("p1 did not normalize correctly\n");
 *         }
 *     } else {
 *         M_printf("failed to normalize p1\n");
 *     }
 *     M_free(out);
 *     
 *     if (M_fs_path_norm(&out, p2, M_FS_PATH_NORM_ABSOLUTE, M_FS_SYSTEM_WINDOWS) == M_FS_ERROR_SUCCESS) {
 *         if (M_str_eq(out, n2)) {
 *             M_printf("p2 normalized correctly\n")
 *         } else {
 *             M_printf("p2 did not normalize correctly\n");
 *         }
 *     } else {
 *         M_printf("failed to normalize p2\n");
 *     } 
 *     M_free(out);
 * \endcode
 *
 * Example (listing files in a directory):
 *
 * \code{.c}
 *     M_list_str_t *l  = NULL;
 *     size_t        len;
 *     size_t        i;
 *
 *     l = M_fs_dir_walk_strs("~", "*.txt", M_FS_DIR_WALK_FILTER_FILE|M_FS_DIR_WALK_FILTER_READ_INFO_BASIC);
 *     len = M_list_str_len(l);
 *     for (i=0; i<len; i++) {
 *         M_printf("%s\n", M_list_str_at(l, i));
 *     }
 *     M_list_str_destroy(l);
 * \endcode
 */

/*! \addtogroup m_fs_common Common
 *  \ingroup m_fs
 *
 * @{
 */

/*! Permissions. */
struct M_fs_perms;
typedef struct M_fs_perms M_fs_perms_t;


/*! Information. */
struct M_fs_info;
typedef struct M_fs_info M_fs_info_t;


/*! An open file. */
struct M_fs_file;
typedef struct M_fs_file M_fs_file_t;


/*! An entry in a directory. */
struct M_fs_dir_entry;
typedef struct M_fs_dir_entry M_fs_dir_entry_t;


/*! A list of directory entries. */
struct M_fs_dir_entries;
typedef struct M_fs_dir_entries M_fs_dir_entries_t;


/*! File operation progress information. */
struct M_fs_progress;
typedef struct M_fs_progress M_fs_progress_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Error codes. */

typedef enum {
	M_FS_ERROR_SUCCESS = 0,   /*!< Operation completed successfully */
	M_FS_ERROR_GENERIC,       /*!< Generic, uncategorized error */
	M_FS_ERROR_INVALID,       /*!< Invalid argument */
	M_FS_ERROR_PERMISSION,    /*!< Operation not permitted */
	M_FS_ERROR_NOT_SUPPORTED, /*!< Operation not supported */
	M_FS_ERROR_IO,            /*!< Input/output error */
	M_FS_ERROR_SEEK,          /*!< Invalid seek */
	M_FS_ERROR_READONLY,      /*!< Read-only file system */
	M_FS_ERROR_QUOTA,         /*!< Disk quota exceeded */
	M_FS_ERROR_DNE,           /*!< No such file or directory */
	M_FS_ERROR_NAMETOOLONG,   /*!< Filename too long */
	M_FS_ERROR_FILE_EXISTS,   /*!< File exists */
	M_FS_ERROR_FILE_2BIG,     /*!< File too large */
	M_FS_ERROR_FILE_2MANY,    /*!< Too many open files */
	M_FS_ERROR_ISDIR,         /*!< Is a directory */
	M_FS_ERROR_NOTDIR,        /*!< Not a directory */
	M_FS_ERROR_DIR_NOTEMPTY,  /*!< Directory not empty */
	M_FS_ERROR_LINK_LOOP,     /*!< Too many levels of symbolic links */
	M_FS_ERROR_LINK_2MANY,    /*!< Too many links */
	M_FS_ERROR_NOT_SAMEDEV,   /*!< Cannot move across mount points. */
	M_FS_ERROR_CANCELED       /*!< The operation was canceled (typically by user interaction). */
} M_fs_error_t;


/*! Standard streams for input and output. */
typedef enum {
	M_FS_IOSTREAM_IN = 0,
	M_FS_IOSTREAM_OUT,
	M_FS_IOSTREAM_ERR
} M_fs_iostream_t;


/*! File permissions. Based on POSIX file permissions. */
typedef enum {
	M_FS_PERMS_MODE_NONE  = 0,      /*!< No perms. */
	M_FS_PERMS_MODE_READ  = 1 << 0, /*!< Read. */
	M_FS_PERMS_MODE_WRITE = 1 << 1, /*!< Write. */
	M_FS_PERMS_MODE_EXEC  = 1 << 2  /*!< Execute. */
} M_fs_perms_mode_t;


/*! How should the perms be modified. */
typedef enum {
	M_FS_PERMS_TYPE_EXACT = 0, /*!< Perms are exactly what is set. */
	M_FS_PERMS_TYPE_ADD,       /*!< Perms will be added to existing perms. */
	M_FS_PERMS_TYPE_REMOVE     /*!< Perms will be removed from existing perms. */
} M_fs_perms_type_t;


/*! Who do the given perms apply to. Based on POSIX file permissions. */
typedef enum {
	M_FS_PERMS_WHO_USER = 0, /*!< User/owner. */
	M_FS_PERMS_WHO_GROUP,    /*!< Group. */
	M_FS_PERMS_WHO_OTHER     /*!< Other. */
} M_fs_perms_who_t;


/*! How should the path be normalized. */
typedef enum {
	M_FS_PATH_NORM_NONE                 = 0,
	M_FS_PATH_NORM_ABSOLUTE             = 1 << 0, /*!< Use the current working directory to determine absolute
	                                                   path if provided path is relative. */
	M_FS_PATH_NORM_FOLLOWSYMLINKS       = 1 << 1, /*!< Follow sym links. This will succeed if even if the path
	                                                   pointed by by the symlink does not exist. */
	M_FS_PATH_NORM_SYMLINKS_FAILDNE     = 1 << 2, /*!< Follow sym links. Fail if the location pointed to by the
	                                                   link does not exist excluding the last location in the path. */
	M_FS_PATH_NORM_SYMLINKS_FAILDNELAST = 1 << 3, /*!< Follow sym links. Fail if only the last location pointed
	                                                   to by the link does not exist. */
	M_FS_PATH_NORM_HOME                 = 1 << 4, /*!< Normalize ~/ to $HOME. */
	M_FS_PATH_NORM_NOPARENT             = 1 << 5  /*!< Do NOT Normalize ../ paths. */
} M_fs_path_norm_t;

/* Default/common flags */
#define M_FS_PATH_NORM_RESDIR M_FS_PATH_NORM_HOME|M_FS_PATH_NORM_FOLLOWSYMLINKS|M_FS_PATH_NORM_SYMLINKS_FAILDNE
#define M_FS_PATH_NORM_RESALL M_FS_PATH_NORM_HOME|M_FS_PATH_NORM_FOLLOWSYMLINKS|M_FS_PATH_NORM_SYMLINKS_FAILDNE|M_FS_PATH_NORM_SYMLINKS_FAILDNELAST


/*! How should a path's info be read. */
typedef enum {
	M_FS_PATH_INFO_FLAGS_NONE            = 0,      /*!< Normal operation. Get all info for the given location. */
	M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS = 1 << 0, /*!< If the location is symlink get the info for the location pointed
	                                                    to by the link and not the link itself. */
	M_FS_PATH_INFO_FLAGS_BASIC           = 1 << 1  /*!< Get basic info only.
	                                                    Excludes:
	                                                      - User and group.
	                                                      - Permissions. */
} M_fs_info_flags_t;


/*! File interaction. */
typedef enum {
	M_FS_FILE_MODE_NONE           = 0,      /*!< No mode specified. */
	M_FS_FILE_MODE_READ           = 1 << 0, /*!< Read. */
	M_FS_FILE_MODE_WRITE          = 1 << 1, /*!< Write. */
	M_FS_FILE_MODE_NOCREATE       = 1 << 2, /*!< Do not create the file if it does not exist. */
	M_FS_FILE_MODE_APPEND         = 1 << 3, /*!< Only write at the end of the file. */
	M_FS_FILE_MODE_OVERWRITE      = 1 << 4, /*!< Overwrite the file (truncate) if it exists. */
	M_FS_FILE_MODE_PRESERVE_PERMS = 1 << 5, /*!< Move/Copy use the perms from the original file.
	                                             This only preserves permissions that can be expressed
	                                             by an M_fs_perms_t object. ACLs for example will not be
	                                             persevered. */
	M_FS_FILE_MODE_NOCLOSEEXEC    = 1 << 6  /*!< Allow sharing of file descriptors with fork executed processes. */
} M_fs_file_mode_t;


/*! Read / Write behavior */
typedef enum {
	M_FS_FILE_RW_NORMAL  = 0,      /*!< Normal operation */
	M_FS_FILE_RW_FULLBUF = 1 << 0  /*!< Read until the given buffer is full or until there is no more data to read.	
	                                    Write all data in the buffer. Normal operation is to return after the system 
	                                    reads/writes what it can. This will cause the read/write to retry until the
	                                    given all data is read/written. */
} M_fs_file_read_write_t;


/*! Seeking within a file. */
typedef enum {
	M_FS_FILE_SEEK_BEGIN = 0, /*!< Seek relative to the beginning of the file. */
	M_FS_FILE_SEEK_END,       /*!< Seek relative to the end of the file .*/
	M_FS_FILE_SEEK_CUR        /*!< Seek relative to the current location */
} M_fs_file_seek_t;


/*! How should data be synced to disk. */
typedef enum {
	M_FS_FILE_SYNC_NONE   = 0,      /*!< No sync. */
	M_FS_FILE_SYNC_BUFFER = 1 << 0, /*!< Internal write buffer should be synced (fflush) */
	M_FS_FILE_SYNC_OS     = 1 << 1  /*!< OS buffer should be synced (fsync) */
} M_fs_file_sync_t;

/*! Controls the behavior of walk. Specifies how the walk should be performed and what should be stored in the
 * result of the walk.
 */
typedef enum {
	M_FS_DIR_WALK_FILTER_NONE            = 0,      /*!< No filters. */
	/* Types. */
	M_FS_DIR_WALK_FILTER_FILE            = 1 << 0, /*!< Include files in the list of entries. 
	                                                    Anything that is not another type is considered a file. */
	M_FS_DIR_WALK_FILTER_DIR             = 1 << 1, /*!< Include directories in the list of entries. */
	M_FS_DIR_WALK_FILTER_PIPE            = 1 << 2, /*!< Include pipes in the list of entries. */
	M_FS_DIR_WALK_FILTER_SYMLINK         = 1 << 3, /*!< Include symlinks in the list of entries. */
	/* Attributes. */
	M_FS_DIR_WALK_FILTER_HIDDEN          = 1 << 4, /*!< Include hidden locations in the list of entries. */
	/* Behaviors. */
	M_FS_DIR_WALK_FILTER_RECURSE         = 1 << 5, /*!< Recurse into directories and include their contents.
	                                                    File system loops (infinite redirects due to symlinks) will be
	                                                    ignored. */
	M_FS_DIR_WALK_FILTER_FOLLOWSYMLINK   = 1 << 6, /*!< Should symlinks be followed. */
	M_FS_DIR_WALK_FILTER_JAIL_FAIL       = 1 << 7, /*!< Fail walk if redirection outside of base path. */
	M_FS_DIR_WALK_FILTER_JAIL_SKIP       = 1 << 8, /*!< Skip entry if redirection outside of base path. */
	M_FS_DIR_WALK_FILTER_AS_SET          = 1 << 9, /*!< Only include a given entry once. Symlinks could cause a file
	                                                    or directory to show up multiple times in a walk this will
	                                                    exclude the additional entries. Also, only one symlink to
	                                                    a given entry will be included. For example, if there are two
	                                                    symlinks to the same file one symlink will be ingored. */
	/* Read and store the file info in each entry.
	 * The info is specific to the type. Meaning if the type if a symlink then the info will for the symlink not what
	 * the symlink points to. Depending on the other options you could have two entires in the list one for the symlink
	 * and one for the file. The path will be the same but the type and the info will be different. If READ_INFO is not
	 * set this doesn't guarantee the info won't be read (some cases and options it is necessary) but even if it is
	 * read it won't be set in the entry. Assume that if not set the info won't be available. */
	M_FS_DIR_WALK_FILTER_READ_INFO_BASIC = 1 << 10, /*!< Read/store basic info about the entry.
	                                                     Specifically:
	                                                       - Is dir.
	                                                       - Is hidden.
	                                                       - File size.
	                                                       - Access time.
	                                                       - Last modification time.
	                                                       - Creation time. */
	M_FS_DIR_WALK_FILTER_READ_INFO_FULL  = 1 << 11, /*!< Read/Store all info about the entry.
	                                                     Specifically:
	                                                       - All basic info.
	                                                       - User and Group.
	                                                       - Permissions. */
	M_FS_DIR_WALK_FILTER_CASECMP         = 1 << 12  /*!< The pattern matching should be compared to the path in
	                                                     a case insensitive manner. */
} M_fs_dir_walk_filter_t;

/* Include all "files" in a walk. */
#define M_FS_DIR_WALK_FILTER_ALL M_FS_DIR_WALK_FILTER_FILE|M_FS_DIR_WALK_FILTER_DIR|M_FS_DIR_WALK_FILTER_SYMLINK|M_FS_DIR_WALK_FILTER_HIDDEN


/*! Sorting methods. Some of these methods require the file info. If the file info was not retrieved (walk did not
 * have a M_FS_DIR_WALK_FILTER_READ_INFO_* filter set) all files are considered equal.
 */
typedef enum {
	M_FS_DIR_SORT_NAME_CASECMP = 0, /*!< Sort by name case insensitive. */
	M_FS_DIR_SORT_NAME_CMP,         /*!< Sort by name case sensitive. */
	M_FS_DIR_SORT_ISDIR,            /*!< Sort by is directory. */
	M_FS_DIR_SORT_ISHIDDEN,         /*!< Sort by hidden status. */
	M_FS_DIR_SORT_NONE,             /*!< Don't sort. This is an option because sorting can have primary and secondary.
	                                  This allows only a primary sort to be applied. */ 
	/* Requires info. */
	M_FS_DIR_SORT_SIZE,             /*!< Sort by file size. */
	M_FS_DIR_SORT_ATIME,            /*!< Sort by last access time. */
	M_FS_DIR_SORT_MTIME,            /*!< Sort by last modification time. */
	M_FS_DIR_SORT_CTIME             /*!< Sort by create time. */
} M_fs_dir_sort_t;


/*! Determines what progress information should be reported to the progress callback. Size reporting will
 * increase the amount of time required for processing due to needing to get and calculate totals. */
typedef enum {
	M_FS_PROGRESS_NOEXTRA    = 0,      /*!< Don't provide optional reporting. Will be overridden by other flags. */
	M_FS_PROGRESS_COUNT      = 1 << 0, /*!< Report on number of operations total and completed. */
	M_FS_PROGRESS_SIZE_TOTAL = 1 << 1, /*!< Report the total size for all file operations and the total completed. */
	M_FS_PROGRESS_SIZE_CUR   = 1 << 2  /*!< Report the total size for the current file being processed and the total
	                                        size of the file completed. */
} M_fs_progress_flags_t;


/*! Controls how path should be constructed. */
typedef enum {
	M_FS_SYSTEM_AUTO = 0, /*!< Automatically set based on current system. */
	M_FS_SYSTEM_WINDOWS,  /*!< Forcibly use windows logic. */
	M_FS_SYSTEM_UNIX      /*!< Forcibly use Unix logic. */
} M_fs_system_t;


/*! Types of file objects. */
typedef enum {
	M_FS_TYPE_UNKNOWN = 0, /*!< The location is an unknown type. Typically this means it was not read. */
	M_FS_TYPE_FILE,        /*!< The location is a regular file. */
	M_FS_TYPE_DIR,         /*!< The location is a directory. */
	M_FS_TYPE_PIPE,        /*!< The location is a fifo (pipe). */
	M_FS_TYPE_SYMLINK      /*!< The location is a symbolic link. */
} M_fs_type_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* 1 KB default buffer size. */
#define M_FS_BUF_SIZE 1024

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* @} */


/*! Walk callback function prototype.
 *
 * \param[in] path  The path passed into walk.
 * \param[in] entry The entry created for the location. The cb will have ownership of the entry. It is up to the
 *                  cb to save or destroy the entry.
 * \param[in] res   The status of the entry. A success should be treat the entry as a good entry for the purpose
 *                  of the callback. Any other result should be treated as an error condition and it is up to the
 *                  callback as to how it should be handled. For example and infinite recursion loop due to circular
 *                  symlinks will have an entry denoting which link causes the loop and a result of M_FS_ERROR_LINK_LOOP.
 * \param[in] thunk Additional data passed to walk for use in this callback.
 *
 * \return M_TRUE if walk should continue. M_FALSE if the walk should be cancelled.
 */
typedef M_bool (*M_fs_dir_walk_cb_t)(const char *path, M_fs_dir_entry_t *entry, M_fs_error_t res, void *thunk);


/*! File operation progress callback function prototype.
 *
 * Many file and directory operations (move, copy, delete...) can report their progress as the operation is run.
 *
 * \param p The progress object. Contains information about the status of the operation. The object is only valid
 *          until the callback returns; it should not be stored.
 *
 * \return M_TRUE if the operation should continue. M_FALSE if the operation should be cancelled.
 */
typedef M_bool (*M_fs_progress_cb_t)(const M_fs_progress_t *p);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_perms Permissions
 *  \ingroup m_fs
 *
 * @{
 */

/*! Create a perms object.
 *
 * \return A perms object.
 */
M_API M_fs_perms_t *M_fs_perms_create(void) M_MALLOC;


/*! Duplicate a perms object.
 *
 * \param[in] perms The perms object to duplicate.
 *
 * \return A new perms object with the same information as the original.
 */
M_API M_fs_perms_t *M_fs_perms_dup(const M_fs_perms_t *perms) M_MALLOC;


/*! Merge two perms objects together.
 *
 * The second (src) perms will be destroyed automatically upon completion of this function. 
 *
 * This is intended for dest to hold exact permissions. In this case, when src is exact then
 * src will replace the permissions in dest. If src is an add or remove it will modify dest
 * accordingly.
 *
 * When the perms in dest are not set then the permissions from src will be used.
 *
 * When dest is a modifier (add or remove) then the permissions from src will replace the
 * permission in dest. This happens regardless of the permissions in src being exact or
 * a modifier.
 *
 * When the permissions in src are not set then dest will not be modified.
 *
 * \param[in,out] dest Pointer by reference to the perms receiving the values.
 *                     if this is NULL, the pointer will simply be switched out for src.
 * \param[in,out] src  Pointer to the perms giving up its values.
 */
M_API void M_fs_perms_merge(M_fs_perms_t **dest, M_fs_perms_t *src) M_FREE(2);


/*! Destroy a perms object.
 *
 * \param[in] perms The perms.
 */
M_API void M_fs_perms_destroy(M_fs_perms_t *perms) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Can the process access the path with the given perms.
 *
 * \warning using this function incorrectly can lead to security issues. This is an
 * implementation of the POSIX access() function and the security considerations
 * apply.
 *
 * This function should not be used to make access control decisions due to
 * Time-of-check Time-of-use (TOCTOU) race condition attacks.
 *
 * \param[in] path The path to access.
 * \param[in] mode M_fs_perms_mode_t permissions to should be checked. Optional, pass
 *                 0 if only checking if the path exists.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_perms_can_access(const char *path, M_uint32 mode);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Apply perms to a path.
 *
 * This will set/change/modify the perms on a path.
 *
 * \param[in] perms The perms.
 * \param[in] path  The path.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_perms_set_perms(const M_fs_perms_t *perms, const char *path);


/*! Apply perms to open file.
 *
 * This will set/change/modify the perms on a file.
 *
 * \param[in] perms The perms.
 * \param[in] fd    The file.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_perms_set_perms_file(const M_fs_perms_t *perms, M_fs_file_t *fd);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the user associated with the perms.
 *
 * \param[in] perms The perms.
 *
 * \return The user. NULL if no user is set.
 */
M_API const char *M_fs_perms_get_user(const M_fs_perms_t *perms);


/*! Get the group associated with the perms.
 *
 * \param[in] perms The perms.
 *
 * \return The group. NULL if not group is set.
 */
M_API const char *M_fs_perms_get_group(const M_fs_perms_t *perms);


/*! Get the mode associated with the perms for the given permission.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return A bit map of M_fs_perms_mode_t values which are the permissions that are set.
 */
M_API M_uint32 M_fs_perms_get_mode(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Get the type (exact/add/remove) associated with the perms for the given permission.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return The permission type.
 */
M_API M_fs_perms_type_t M_fs_perms_get_type(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Check if a given permission is set.
 *
 * If not set the permission will be ignored during merge, set and other operation that use the permissions.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return M_TRUE if the permission are set. Otherwise M_FALSE.
 */
M_API M_bool M_fs_perms_get_isset(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Get the directory override mode associated with the perms for the given permission.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return A bit map of M_fs_perms_mode_t values which are the permissions that are set.
 */
M_API M_uint32 M_fs_perms_get_dir_mode(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Get the directory override type (exact/add/remove) associated with the perms for the given permission.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return The permission type.
 */
M_API M_fs_perms_type_t M_fs_perms_get_dir_type(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Check if a given directory override permission is set.
 *
 * If not set the permission will be ignored during merge, set and other operation that use the permissions.
 *
 * \param[in] perms The perms.
 * \param[in] who   The permissions this applies to.
 *
 * \return M_TRUE if the permission are set. Otherwise M_FALSE.
 */
M_API M_bool M_fs_perms_get_dir_isset(const M_fs_perms_t *perms, M_fs_perms_who_t who);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set the user.
 *
 * \param[in] perms The perms.
 * \param[in] user  The user.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_perms_set_user(M_fs_perms_t *perms, const char *user);


/*! Set the group.
 *
 * \param[in] perms The perms.
 * \param[in] group The group.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_perms_set_group(M_fs_perms_t *perms, const char *group);


/*! Set the mode for the perms.
 *
 * \param[in] perms The perms.
 * \param[in] mode  M_fs_file_mode_t modes.
 * \param[in] who   Who this applies to.
 * \param[in] type   The type permissions being set.
 */
M_API void M_fs_perms_set_mode(M_fs_perms_t *perms, M_uint32 mode, M_fs_perms_who_t who, M_fs_perms_type_t type);


/*! Set the directory override mode for the perms.
 *
 * \param[in] perms The perms.
 * \param[in] mode  M_fs_file_mode_t modes.
 * \param[in] who   Who this applies to.
 * \param[in] type  The type permissions being set.
 */
M_API void M_fs_perms_set_dir_mode(M_fs_perms_t *perms, M_uint32 mode, M_fs_perms_who_t who, M_fs_perms_type_t type);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Unset permissions.
 *
 * This is different than setting _no_ permissions.
 *
 * This will also unset the equivalent directory override permissions.
 *
 * \param[in] perms The perms.
 * \param[in] who   Who this applies to.
 */
M_API void M_fs_perms_unset_mode(M_fs_perms_t *perms, M_fs_perms_who_t who);


/*! Unset directory override permissions.
 *
 * This is different than setting _no_ permissions.
 *
 * \param[in] perms The perms.
 * \param[in] who   Who this applies to.
 */
M_API void M_fs_perms_unset_dir_mode(M_fs_perms_t *perms, M_fs_perms_who_t who);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_path Path
 *  \ingroup m_fs
 *
 * @{
 */

/*! Determine the max path length for the system.
 *
 * \param[in] sys_type The system type used to determine the maximum path length.
 *
 * \return The maximum path length.
 */
M_API size_t M_fs_path_get_path_max(M_fs_system_t sys_type);


/*! Check if a path is an absolute path.
 *
 * A path is absolute if it's Unix and starts with /. Or Windows and starts with \\\\ (UNC) or a drive letter.
 *
 * \param[in] p        The path.
 * \param[in] sys_type The system type.
 *
 * \return M_TRUE if an absolute path. Otherwise M_FALSE.
 */
M_API M_bool M_fs_path_isabs(const char *p, M_fs_system_t sys_type);


/*! Check if a path is a UNC path.
 *
 * A path is UNC if it's Windows and starts with "\\\\".
 *
 * \param[in] p The path.
 *
 * \return M_TRUE if an UNC path. Otherwise M_FALSE.
 */
M_API M_bool M_fs_path_isunc(const char *p);


/*! Check if the path is considered hidden by the OS.
 *
 * Either the path or info parameters can be NULL. Both cannot be NULL.
 *
 * \param[in] path The path.
 * \param[in] info The info.
 *
 * \return Whether the path is considered hidden.
 */
M_API M_bool M_fs_path_ishidden(const char *path, M_fs_info_t *info);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Take a path and split it into components.
 *
 * This will remove empty parts. An absolute path (Unix) starting with / will have the / replaced with an empty to
 * start the list. The same is true for UNC paths. An empty at the start of the path list should be treated as an
 * absolute path.
 *
 * \param[in] path     The path.
 * \param[in] sys_type The system type.
 *
 * \return A list of path parts.
 */
M_API M_list_str_t *M_fs_path_componentize_path(const char *path, M_fs_system_t sys_type);


/*! Join two parts into one path.
 *
 * If either part is empty the separator won't be added. Unlike M_fs_path_join_parts this does not have special
 * handling (using an empty string) for absolute paths. This is a convenience function to write the appropriate system
 * separator between two paths.
 *
 * \param[in] p1 First part.
 * \param[in] p2 Second part.
 * \param[in] sys_type The system type.
 *
 *
 * \return The path as a single string.
 */
M_API char *M_fs_path_join(const char *p1, const char *p2, M_fs_system_t sys_type);


/*! Take a list of path components and join them into a string separated by the system path separator.
 *
 * Empty parts (except the first on Unix and UNC) will be ignored. An empty part at the start is used on Unix and UNC to
 * denote an absolute path.
 *
 * \param[in] path The path.
 * \param[in] sys_type The system type.
 *
 * \return The path as a single string.
 */
M_API char *M_fs_path_join_parts(const M_list_str_t *path, M_fs_system_t sys_type);


/*! Take a list of path components and join them into a string separated by the system path separator.
 *
 * Empty parts (except the first on Unix and UNC) will be ignored. An empty part at the start is used on Unix and UNC to
 * denote an absolute path.
 *
 * \param[in] sys_type The system type.
 * \param[in] num      The number of parts.
 * \param[in] ...      char * parts to be joined.
 *
 * \return The path as a single string.
 */
M_API char *M_fs_path_join_vparts(M_fs_system_t sys_type, size_t num, ...);

/*! Join a base path, the name and the resolved name into the full resolved path.
 *
 * This is a helper for dealing with M_fs_dir_walk in order to determine the resolved path
 * when the entry returned by the callback is a symlink.
 *
 * We have three parts: path, entry_name, resolved_name.
 * The entry_name needs to have the last part removed because it is a symlink. Then
 * we need to put path and resolved_name on either size to get the real name.
 *
 * For example:
 * path          = /usr/share/zoneinfo/America
 * part          = Indiana/Indianapolis
 * resolved_name = ../../posix/America/Indiana/Indianapolis
 *
 * We need:
 * /usr/share/zoneinfo/America/Indiana/../../posix/America/Indiana/Indianapolis
 *
 * \param[in] path          The base path.
 * \param[in] part          The path component under the base.
 * \param[in] resolved_name The resolved path for a symlink that needs to be combined with the base and part.
 * \param[in] sys_type      The system type.
 * \return The resolved path.
 */
M_API char *M_fs_path_join_resolved(const char *path, const char *part, const char *resolved_name, M_fs_system_t sys_type);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Strip last component from a filename.
 *
 * Remove last full non-slash component. Output will not include trailing slashes.
 * E.g: /usr/bin/ -> /usr
 *
 * A path without a dir component will output a '.' (current dir.).
 * E.g: bin -> . (meaning the current directory).
 *
 * \param[in] path     The path.
 * \param[in] sys_type The system path logic and separator to use.
 *
 * \return The path component from a filename.
 */
M_API char *M_fs_path_dirname(const char *path, M_fs_system_t sys_type);


/*! Strip all but the last component from a filename.
 *
 * Remove all but the last full non-slash component. Output will not include trailing slashes.
 *
 * E.g: /usr/bin/ -> bin
 *
 * E.g: bin -> bin
 *
 * \param[in] path     The path.
 * \param[in] sys_type The system path logic and separator to use.
 *
 * \return The path last component from a filename.
 */
M_API char *M_fs_path_basename(const char *path, M_fs_system_t sys_type);


/*! The user's configuration directory.
 *
 * This is a user level _not_ system level directory.
 * This is the OS standard directory for application
 * configuration files.
 *
 * \param[in] sys_type The system path logic and separator to use.
 *
 * \return The path to the config dir, otherwise NULL on error.
 */
M_API char *M_fs_path_user_confdir(M_fs_system_t sys_type);



/*! Temporary directory set by the system that the application can use for temporary storage.
 *
 * \warning This is _NOT_ a secure location.
 *
 * Other processes on the system can share this directory.
 * It's recommend to create an applications specific subdirectory to use for temporary
 * files. Again, this is _NOT_ intended to be used for secure files or when secure files
 * are necessary.
 *
 * This should only be used for temporary storage
 * of files being manipulated. For example, unpacking a compressed archive then moving
 * the files to the destination. Or saving to a temporary file then using M_fs_move to
 * ensure an atomic write.
 *
 * \param[in] sys_type The system path logic and separator to use.
 *
 * \return The path to a temporary dir, otherwise NULL on error.
 */
M_API char *M_fs_path_tmpdir(M_fs_system_t sys_type);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the current working directory for the calling process.
 *
 * \param[in] cwd An allocated string with the cwd.
 *
 * \return result.
 */
M_API M_fs_error_t M_fs_path_get_cwd(char **cwd);


/*! Set the current working directory for the calling process.
 *
 * \param[in] path The path to set as the cwd.
 *
 * \return result.
 */
M_API M_fs_error_t M_fs_path_set_cwd(const char *path);


/*! Resolve a symlink.
 *
 * Reads the value pointed to by a symlink.
 *
 * \param[out] out  An allocated string with the normalized path.
 * \param[in]  path The path to resolve.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_path_readlink(char **out, const char *path);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Normalize a path.
 *
 * This typically does not need to be called because all functions that take a path (file) will
 * call this internally using the appropriate parameters. This is provided as a convenience for
 * displaying paths to a user.
 *
 * Supported features on all OS's;
 *   - Home dir (~) expansion.
 *   - Environment variable expansion (both $var and \%var\%).
 *
 * Supported feature Unix only:
 *   - Symlink resolution.
 *
 * \param[out] out      An allocated string with the normalized path.
 * \param[in]  path     The path to normalize.
 * \param[in]  flags    M_fs_path_norm_t flags to control the normalization behavior.
 * \param[in]  sys_type The system path format to the path is in. This denotes the
 *                      path type and how it should be normalized. For example, a Windows
 *                      path with "C:\..." passed with the UNIX type will do strange things
 *                      because it is not a Unix formatted path. The purpose of this argument
 *                      is to specify the path type if known. Allows a Windows path on a Unix
 *                      system to be parsed properly even though it's not the standard path
 *                      type for the system. Note that if the path is not the same as the
 *                      system standard type the M_FS_PATH_NORM_ABSOLUTE my give unexpected
 *                      results for non-absolute paths. For example this relative path specified
 *                      as a Windows path run on a Unix system:
 *                      ".\\abc.\\\\\\..\\xyz\\\\.\\123\\.\\xyr\\." may result in something like
 *                      May give a result like:
 *                      "home\jschember\svn\mstdlib-trunk\build\xyz\123\xyr"
 *                      Notice there is no '\' or drive letter because they are not technically
 *                      valid. However, the path was properly converted to an absolute path.
 *
 *                      
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_path_norm(char **out, const char *path, M_uint32 flags, M_fs_system_t sys_type);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_info Info
 *  \ingroup m_fs
 *
 * @{
 */

/*! Get information about a given path.
 *
 * \param[out] info Allocated info object with the info about the path. If passed as NULL then
 *                  this only verifies that a path exists. However, M_fs_perms_can_access is 
 *                  more useful for checking for file existence.
 * \param[in] path  The path.
 * \param[in] flags M_fs_info_flags_t defining behavior of how and what info to read.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_info(M_fs_info_t **info, const char *path, M_uint32 flags);


/*! Get information about an open file.
 *
 * \param[out] info Allocated info object with the info about the path. If passed as NULL then
 *                  this only verifies that a path exists. However, the file is already open
 *                  so it exists!
 * \param[in] fd    The file.
 * \param[in] flags M_fs_info_flags_t defining behavior of how and what info to read.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_info_file(M_fs_info_t **info, M_fs_file_t *fd, M_uint32 flags);


/*! Destroy an info object.
 *
 * \param[in] info The info object.
 */
M_API void M_fs_info_destroy(M_fs_info_t *info) M_FREE(1);


/*! Get the user from a path info.
 *
 * \param[in] info The info object.
 *
 * \return The user/owner.
 */
M_API const char *M_fs_info_get_user(const M_fs_info_t *info);


/*! Get the group from a path info.
 *
 * \param[in] info The info object.
 *
 * \return The group.
 */
M_API const char *M_fs_info_get_group(const M_fs_info_t *info);


/*! Location type.
 *
 * \param[in] info The info object.
 *
 * \return The path type.
 */
M_API M_fs_type_t M_fs_info_get_type(const M_fs_info_t *info);


/*! Is this a hidden file?
 *
 * \param[in] info The info object.
 *
 * \return M_TRUE if this is a hidden file. Otherwise M_FALSE.
 */
M_API M_bool M_fs_info_get_ishidden(const M_fs_info_t *info);


/*! The size of the path.
 *
 * \param[in] info The info object.
 *
 * \return The size.
 */
M_API M_uint64 M_fs_info_get_size(const M_fs_info_t *info);


/*! The last access time.
 *
 * \param[in] info The info object.
 *
 * \return The time.
 */
M_API M_time_t M_fs_info_get_atime(const M_fs_info_t *info);


/*! The last modify time.
 *
 * \param[in] info The info object.
 *
 * \return The time.
 */
M_API M_time_t M_fs_info_get_mtime(const M_fs_info_t *info);


/*! The last status change time.
 *
 * \param[in] info The info object.
 *
 * \return The time.
 */
M_API M_time_t M_fs_info_get_ctime(const M_fs_info_t *info);


/*! The file birth/creation time.
 *
 * This time is not updated after append operations. In Linux terms, it's the time the inode was created.
 *
 * Note that birth/creation times aren't available on all platforms - if you're on one of those platforms,
 * this method will always return 0.
 *
 * \param[in] info object created by M_fs_info()
 *
 * \return Time when file was created, or 0 if time couldn't be retrieved
 */
M_API M_time_t M_fs_info_get_btime(const M_fs_info_t *info);


/*! Get the permissions associated with the path.
 *
 * \param[in] info The info object.
 *
 * \return A perms object belonging to the info object. The perms object will be
 *         destroyed when the info object is destroyed.
 */
M_API const M_fs_perms_t *M_fs_info_get_perms(const M_fs_info_t *info);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_file File
 *  \ingroup m_fs
 *
 * @{
 */

/*! Open a file.
 *
 * The set of flags you pass to \a mode must include M_FS_FILE_MODE_READ and/or M_FS_FILE_MODE_WRITE.
 * System umask is honored when creating a file.
 *
 * The other M_fs_file_mode_t flags can be used as well, they just need to be OR'd with M_FS_FILE_MODE_READ and/or
 * M_FS_FILE_MODE_WRITE.
 *
 * \param[out] fd       The file object created upon success. Will be set to \c NULL if there was an error.
 * \param[in]  path     The path to open.
 * \param[in]  buf_size Set a buffer size to enable buffered read and write. Use 0 to disable buffering.
 * \param[in]  mode     M_fs_file_mode_t open mode.
 * \param[in]  perms    Additional perms to apply to the file if it does not exist and is created.
                        Umake is honored when perms are set. E.g. perms & ~umask is used.
                        If perms is NULL a default of rw-rw-r-- & ~umask is used.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_open(M_fs_file_t **fd, const char *path, size_t buf_size, M_uint32 mode, const M_fs_perms_t *perms);


/*! Open a standard IO stream.
 *
 * \param[out] fd     The file object created upon success. Will be set to \c NULL if there was an error.
 * \param[in]  stream The stream to open.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_open_iostream(M_fs_file_t **fd, M_fs_iostream_t stream);


/*! Close an open file.
 *
 * \param[in] fd The file object.
 */
M_API void M_fs_file_close(M_fs_file_t *fd);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read from a file. 
 *
 * \param[in]  fd       The file object.
 * \param[out] buf      A buffer to put the read data into.
 * \param[in]  buf_len  The size of the buffer.
 * \param[out] read_len How much data was read into buf.
 * \param[in]  flags    M_fs_file_read_write_t flags to control the read.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_read(M_fs_file_t *fd, unsigned char *buf, size_t buf_len, size_t *read_len, M_uint32 flags);


/*! Write data to a file.
 *
 * \param[in]  fd        The file object.
 * \param[in]  buf       The data to write.
 * \param[in]  count     The length of the data to write.
 * \param[out] wrote_len The amount of data written to the file.
 * \param[in]  flags     M_fs_file_read_write_t flags to control the write.
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_write(M_fs_file_t *fd, const unsigned char *buf, size_t count, size_t *wrote_len, M_uint32 flags);


/*! Move/Set the read/write offset within an file.
 *
 * \param[in] fd     The file object.
 * \param[in] offset How much to move the offset relative to from. Can be negative to move backwards.
 * \param[in] from   Where the offset is relative to.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_seek(M_fs_file_t *fd, M_int64 offset, M_fs_file_seek_t from);


/*! Flush file buffer to disk.
 *
 * \param[in] fd   The file object.
 * \param[in] type M_fs_file_sync_t type of sync to perform.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_sync(M_fs_file_t *fd, M_uint32 type);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Read a file into a buffer as a str.
 *
 * \param[in]  path       The path to read from.
 * \param[in]  max_read   A maximum of bytes to read. 0 for no maximum.
 * \param[out] buf        A buffer that will be allocated and contain the file contents. It will be NULL terminated
 *                        on success.
 * \param[out] bytes_read The number of bytes read and contained in the buffer excluding the NULL terminator.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_read_bytes(const char *path, size_t max_read, unsigned char **buf, size_t *bytes_read);


/*! Write a str to a file.
 *
 * \param[in]  path          The path of the file to write into.
 * \param[in]  buf           Buffer containing the data to write into the file.
 * \param[in]  write_len     The number of bytes from buf to write. Optional, pass 0 to use M_str_len to determine
 *                           length of a NULL terminated buffer to write.
 * \param[in]  mode          M_fs_file_mode_t mode. Only supports APPEND. Used to control appending vs overwriting.
 *                           The default it to overwrite the file.
 * \param[out] bytes_written The number of bytes from buf written to the file. Optional, pass NULL if not needed.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_file_write_bytes(const char *path, const unsigned char *buf, size_t write_len, M_uint32 mode, size_t *bytes_written);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_dir Directory
 *  \ingroup m_fs
 *
 * @{
 */

/*! Destroy a directory entry.
 *
 * \param[in] entry The entry to destroy.
 */
M_API void M_fs_dir_entry_destroy(M_fs_dir_entry_t *entry) M_FREE(1);


/*! Get the type of the entry.
 *
 * \param[in] entry The entry.
 *
 * \return The type.
 */
M_API M_fs_type_t M_fs_dir_entry_get_type(const M_fs_dir_entry_t *entry);


/*! Get whether this entry is considered hidden by the OS.
 *
 * \param[in] entry The entry.
 *
 * \return Whether this entry is considered hidden.
 */
M_API M_bool M_fs_dir_entry_get_ishidden(const M_fs_dir_entry_t *entry);


/*! Get the filename of the entry.
 *
 * The path/filename is relative to the directory that was walked.
 *
 * \param[in] entry The entry.
 *
 * \return The name.
 */
M_API const char *M_fs_dir_entry_get_name(const M_fs_dir_entry_t *entry);


/*! Get the resolved filename.
 *
 * This only applies if the entry is a symlink. The resolved name is the path that the symlink points to. This is
 * relative to the filename.
 *
 * \param[in] entry The entry.
 *
 * \return The resolved name.
 */
M_API const char *M_fs_dir_entry_get_resolved_name(const M_fs_dir_entry_t *entry);


/*! Get the file information about the entry.
 *
 * This may be NULL if reading file info was not requested during walk.
 *
 * \param[in] entry The entry.
 *
 * \return The file info.
 */
M_API const M_fs_info_t *M_fs_dir_entry_get_info(const M_fs_dir_entry_t *entry);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Destroy a list of directory entries.
 *
 * \param[in] d The entry list to destroy.
 */
M_API void M_fs_dir_entries_destroy(M_fs_dir_entries_t *d) M_FREE(1);


/*! Sort a list of directory entries.
 *
 * This does an in place sort and does not keep list sorted for subsequent insertions.
 *
 * \param[in] d              The entry list.
 * \param[in] primary_sort   Primary sort method.
 * \param[in] primary_asc    Should the primary sorting be ascending.
 * \param[in] secondary_sort The secondary sort method that should be used when entries are considered equal according
 *                           to the primary_sort method.
 * \param[in] secondary_asc  Should the secondary sorting be ascending.
 */
M_API void M_fs_dir_entries_sort(M_fs_dir_entries_t *d, M_fs_dir_sort_t primary_sort, M_bool primary_asc, M_fs_dir_sort_t secondary_sort, M_bool secondary_asc);


/*! Get the number of entries in the list.
 *
 * \param[in] d The entry list.
 *
 * \return The length of the list.
 */
M_API size_t M_fs_dir_entries_len(const M_fs_dir_entries_t *d);


/*! Get the entry at at the specified index.
 *
 * The entry remains part of the list.
 *
 * \param[in] d   The entry list.
 * \param[in] idx The index.
 *
 * \return The entry.
 */
M_API const M_fs_dir_entry_t *M_fs_dir_entries_at(const M_fs_dir_entries_t *d, size_t idx);


/*! Take the entry from the list.
 *
 * The entry will be removed from the list. It is up to the caller to free the entry.
 *
 * \param[in] d   The entry list.
 * \param[in] idx The index.
 *
 * \return The entry.
 *
 * \see M_fs_dir_entry_destroy
 */
M_API M_fs_dir_entry_t *M_fs_dir_entries_take_at(M_fs_dir_entries_t *d, size_t idx);


/*! Remove and destroy the entry at the given index.
 *
 * \param[in] d   The entry list.
 * \param[in] idx The index.
 *
 * \return M_TRUE if the entry was destroyed. Otherwise M_FALSE.
 */
M_API M_bool M_fs_dir_entries_remove_at(M_fs_dir_entries_t *d, size_t idx);


/*! Remove and destroy all entries in a given range.
 *
 * \param[in] d     The entry list.
 * \param[in] start The starting index. Inclusive.
 * \param[in] end   The ending index. Inclusive.
 *
 * \return M_TRUE if the entry was destroyed. Otherwise M_FALSE.
 */
M_API M_bool M_fs_dir_entries_remove_range(M_fs_dir_entries_t *d, size_t start, size_t end);


/*! Merge two directory entry lists together.
 *
 * The second (src) list will be destroyed automatically upon completion of this function. Any value pointers for the
 * list will be directly copied over to the destination list, they will not be duplicated.
 *
 * \param[in,out] dest Pointer by reference to the list receiving the values.
 *                     if this is NULL, the pointer will simply be switched out for src.
 * \param[in,out] src  Pointer to the list giving up its values.
 */
M_API void M_fs_dir_entries_merge(M_fs_dir_entries_t **dest, M_fs_dir_entries_t *src) M_FREE(2);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! List the contents of a directory by walking the tree.
 *
 * The tree will be walked depth first. When searching for both directory and file contents, the directory entry
 * will come after entries for the directories contents. Support for modifying while walking is OS and filesystem
 * dependent. Thus, behavior while modifying the contents of a directory during a walk is undefined.
 *
 * \param[in] path   The path to walk.
 * \param[in] pat    Glob style pattern to filter entries in the tree. Only entries matching the pattern will
 *                   be included in the output. NULL, "", and "*" will match all entries.
 * \param[in] filter M_fs_dir_walk_filter_t flags controlling the behavior of the walk.
 * \param[in] cb     Callback for entries.
 * \param[in] thunk  Additional data passed to the callback.
 */
M_API void M_fs_dir_walk(const char *path, const char *pat, M_uint32 filter, M_fs_dir_walk_cb_t cb, void *thunk);


/*! List the contents of a directory by walking the tree.
 *
 * \param[in] path   The path to walk.
 * \param[in] pat    Glob style pattern to filter entries in the tree. Only entries matching the pattern will
 *                   be included in the output. NULL, "", and "*" will match all entries.
 * \param[in] filter M_fs_dir_walk_filter_t flags controlling the behavior of the walk.
 *
 * \return A list of entries in the dir. The entries are relative to the specified path.
 */
M_API M_fs_dir_entries_t *M_fs_dir_walk_entries(const char *path, const char *pat, M_uint32 filter);


/*! List the contents of a directory as a list of string paths by walking the tree.
 *
 * \param[in] path   The path to walk.
 * \param[in] pat    Glob style pattern to filter entries in the tree. Only entries matching the pattern will
 *                   be included in the output. NULL, "", and "*" will match all entries.
 * \param[in] filter M_fs_dir_walk_filter_t flags controlling the behavior of the walk.
 *
 * \return A list of string paths that are the contents of the dir. The entries are relative to the specified
 *         path. Directory entries in the output list will end with the OS path separator.
 */
M_API M_list_str_t *M_fs_dir_walk_strs(const char *path, const char *pat, M_uint32 filter);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a directory.
 *
 * \param[in] path           The directory to create.
 * \param[in] create_parents When M_TRUE create the any parents of the last directory if they do not exist instead of
 *                           erroring.
 * \param[in]  perms         Additional perms to apply to the created directory.
                             If perms is NULL a default perms of rw-rw-r-- & ~umask is used.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_dir_mkdir(const char *path, M_bool create_parents, M_fs_perms_t *perms);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_progress Progress
 *  \ingroup m_fs
 *
 * @{
 */

/*! Get the path.
 *
 * \param[in] p Progress.
 *
 * \return The path.
 */
M_API const char *M_fs_progress_get_path(const M_fs_progress_t *p);


/*! Get file type.
 *
 * \param[in] p Progress.
 *
 * \return The type.
 */
M_API M_fs_type_t M_fs_progress_get_type(const M_fs_progress_t *p);


/*! Get result of the operation at this stage for the current file being processed.
 *
 * \param[in] p Progress.
 *
 * \return The result.
 */
M_API M_fs_error_t M_fs_progress_get_result(const M_fs_progress_t *p);


/*! Get total number of files to process.
 *
 * \param[in] p Progress.
 *
 * \return The total number of files.
 */
M_API M_uint64 M_fs_progress_get_count_total(const M_fs_progress_t *p);


/*! Get current number being processing.
 *
 * \param[in] p Progress.
 *
 * \return The current number being processed.
 */
M_API M_uint64 M_fs_progress_get_count(const M_fs_progress_t *p);


/*! Get the total size of all files.
 *
 * \param[in] p Progress.
 *
 * \return The total size.
 */
M_API M_uint64 M_fs_progress_get_size_total(const M_fs_progress_t *p);


/*! Get total number of bytes that have been processed.
 *
 * \param[in] p Progress.
 *
 * \return The number of bytes processed.
 */
M_API M_uint64 M_fs_progress_get_size_total_progess(const M_fs_progress_t *p);


/*! Get size of the current file.
 *
 * \param[in] p Progress.
 *
 * \return The size of the current file.
 */
M_API M_uint64 M_fs_progress_get_size_current(const M_fs_progress_t *p);


/*! Get number of bytes of the current file that have been processed.
 *
 * \param[in] p Progress.
 *
 * \return The number of bytes processed for the current file.
 */
M_API M_uint64 M_fs_progress_get_size_current_progress(const M_fs_progress_t *p);

/*! @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_fs_operations File System Operations
 *  \ingroup m_fs
 *
 * @{
 */

/*! Create a soft link.
 *
 * \param[in] target    The target to link.
 * \param[in] link_name The link to create.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_symlink(const char *target, const char *link_name);


/*! Move a file or directory from one location to another.
 *
 * If moving a file to an existing directory the file will be copied into the directory with the same name. 
 *
 * \param[in] path_old       The file to move.
 * \param[in] path_new       The location the file should be moved to.
 * \param[in] mode           M_fs_file_mode_t mode. Only supports OVERWRITE. If overwrite is set the move will
 *                           overwrite the file if it exists. Without this set the move operation will fail if
 *                           the file exists.
 * \param[in] cb             Progress callback that should be called.
 * \param[in] progress_flags M_fs_progress_flags_t flags to control what data should be set in the progress callback.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_move(const char *path_old, const char *path_new, M_uint32 mode, M_fs_progress_cb_t cb, M_uint32 progress_flags);


/*! Copy a file or directory to a new location.
 *
 * If copying a file to an existing directory the file will be copied into the directory with the same name. 
 *
 * \param[in] path_old       The file to move.
 * \param[in] path_new       The location the file should be copied to.
 * \param[in] mode           M_fs_file_mode_t mode. Only supports OVERWRITE. If overwrite is set the move will
 *                           overwrite the file if it exists. Without this set the move operation will fail if
 *                           the file exists.
 * \param[in] cb             Progress callback that should be called.
 * \param[in] progress_flags M_fs_progress_flags_t flags to control what data should be set in the progress callback.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_copy(const char *path_old, const char *path_new, M_uint32 mode, M_fs_progress_cb_t cb, M_uint32 progress_flags);


/*! Delete a file or directory.
 *
 * \param[in] path            The file to delete.
 * \param[in] remove_children Only applies to directories. If M_TRUE all contents of the directory will be removed in
 *                            addition to the directory itself. If M_FALSE and the directory is not empty an error
 *                            will be returned.
 * \param[in] cb              Progress callback function. Most useful when deleting a directory with children
 *                            and remove_children is M_TRUE. Will be called after a delete action is completed (each
 *                            child is deleted and the passed path itself is deleted).
 * \param[in] progress_flags  M_fs_progress_flags_t flags to control what data should be set in the progress callback.
 *
 * \return Result.
 */
M_API M_fs_error_t M_fs_delete(const char *path, M_bool remove_children, M_fs_progress_cb_t cb, M_uint32 progress_flags);

/*! @} */

__END_DECLS

#endif /* __M_FS_H__ */
