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

#ifndef __M_FS_INT_H__
#define __M_FS_INT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>
#include "m_defs_int.h"
#include "platform/m_platform.h"

#ifdef _WIN32
#  include <Lmcons.h>
#  include "fs/m_fs_int_win.h"
#else
#  include "fs/m_fs_int_unx.h"
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_fs_file {
#ifdef _WIN32
	HANDLE    fd;
#else
	int       fd;
#endif
	size_t    buf_size;   /* Configured buffer size for buffered read/write */
	M_buf_t  *read_buf;   /* Read buffer. Store read ahead data. */
	M_buf_t  *write_buf;  /* Write buffer. Store data to be written which will be written at a later time as
	                         one large block instead of many small ones. */
	M_int64  read_offset; /* Read offset from where the caller expects a read to have put the location vs where
	                         it really is. Read buffering will read more than requested and advance in the file
	                         further than expected. This is used to move the offset back to the correct location
	                         for seek and write operations. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Perms */

struct M_fs_perms {
	/* user and group. If these are NULL then they were not set. */
	char           *user;
	char           *group;

	/* Cache the system specific values so we don't have to do multiple lookups.
 	 * This also means we can verify the account info when set instead of when
	 * trying to write the perms to the file. */
#ifdef _WIN32
	SID             user_sid[UNLEN+1];
	SID             group_sid[UNLEN+1];
	/* This will be set to UNLEN+1 in M_fs_perms_create. */
	DWORD           sid_len;
#else
	uid_t           uid;
	gid_t           gid;
#endif

	/* user/group/other permissions */
	M_bool            user_set;
	M_fs_perms_mode_t user_mode;
	M_fs_perms_type_t user_type;
	M_bool            group_set;
	M_fs_perms_mode_t group_mode;
	M_fs_perms_type_t group_type;
	M_bool            other_set;
	M_fs_perms_mode_t other_mode;
	M_fs_perms_type_t other_type;

	/* directory override user/group/other permissions */
	M_bool            dir_user_set;
	M_fs_perms_mode_t dir_user_mode;
	M_fs_perms_type_t dir_user_type;
	M_bool            dir_group_set;
	M_fs_perms_mode_t dir_group_mode;
	M_fs_perms_type_t dir_group_type;
	M_bool            dir_other_set;
	M_fs_perms_mode_t dir_other_mode;
	M_fs_perms_type_t dir_other_type;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Error */

/*! Convert an errno to an result.
 * \param err The errno.
 * \return A result.
 */
#ifdef _WIN32
M_API M_fs_error_t M_fs_error_from_syserr(DWORD err);
#else
M_API M_fs_error_t M_fs_error_from_syserr(int err);
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Path */

/*! Determine the type of system logic that should be used.
 * When auto the compiled system type will be used. Otherwise if an explicit system type is given that will be used.
 * \param sys_type The system type.
 * \return The system type.
 */
M_fs_system_t M_fs_path_get_system_type(M_fs_system_t sys_type);

/*! Get the directory separator for the given system type.
 * \param sys_type The system type.
 * \return The directory separator character.
 */
char M_fs_path_get_system_sep(M_fs_system_t sys_type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Resolve a symlink.
 * Reads the value pointed to by a symlink.
 * \param out An allocated string with the normalized path.
 * \param path The path to resolve.
 * \param last Is this a full path.
 * \param flags How should failures to resovle be handled.
 * \param sys_type The system type.
 * \return Result.
 */
M_fs_error_t M_fs_path_readlink_int(char **out, const char *path, M_bool last, M_fs_path_norm_t flags, M_fs_system_t sys_type);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef _WIN32
/*! Get information about a given path.
 * This is an internal helper that takes the WIN32_FIND_DATA which has some of the info already.
 * \param info A point to the info object to be created.
 * \param path The path.
 * \param flags M_fs_info_flags_t defining behavior.
 * \param file_data The find data. Typically from a call to FindFirstFile or FindNextFile.
 * \return Result.
 */
M_fs_error_t M_fs_info_int(M_fs_info_t **info, const char *path, M_fs_info_flags_t flags, WIN32_FIND_DATA *file_data);
#endif

/*! Create a new empty path info object.
 * \return The path info object to be filled.
 */
M_fs_info_t *M_fs_info_create(void) M_MALLOC;

/*! Set the user for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_user(M_fs_info_t *info, const char *val);

/*! Set the group for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_group(M_fs_info_t *info, const char *val);

/*! Set the type for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_type(M_fs_info_t *info, M_fs_type_t val);

/*! Set whether the path is considered hidden for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_hidden(M_fs_info_t *info, M_bool val);

/*! Set the size of the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_size(M_fs_info_t *info, M_uint64 val);

/*! Set the last access time for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_atime(M_fs_info_t *info, M_time_t val);

/*! Set the last modification time for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_mtime(M_fs_info_t *info, M_time_t val);

/*! Set the file status changed time for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_ctime(M_fs_info_t *info, M_time_t val);

/*! Set the file created time for the path info.
 * \param info object created by M_fs_info()
 * \param val  file birth/creation time
 */
void M_fs_info_set_btime(M_fs_info_t *info, M_time_t val);

/*! Set the perms for the path info.
 * \param info The info.
 * \param val  The val to set.
 */
void M_fs_info_set_perms(M_fs_info_t *info, M_fs_perms_t *val);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * file */


/*! Open a file.
 *
 * \param[out] fd    The file object created upon success.
 * \param[in]  path  The path to open.
 * \param[in]  mode  M_fs_file_mode_t open mode.
 * \param[in]  perms Additional perms to apply to the file if it does not exist and is created.
                     If perms is NULL a default perms of rw-rw-r-- & ~umask is used.
 *
 * \return Result.
 */
M_fs_error_t M_fs_file_open_sys(M_fs_file_t **fd, const char *path, M_uint32 mode, const M_fs_perms_t *perms);

void M_fs_file_close_sys(M_fs_file_t *fd);


/*! Read from a file.
 * \param fd The file object.
 * \param buf A buffer to put the read data into.
 * \param buf_len The size of the buffer.
 * \param read_len How much data was read into buf.
 * \return Result.
 */
M_fs_error_t M_fs_file_read_sys(M_fs_file_t *fd, unsigned char *buf, size_t buf_len, size_t *read_len);

/*! Write data to a file.
 * \param fd The file object.
 * \param buf The data to write.
 * \param count The length of the data to write.
 * \param wrote_len The amount of data written to the file.
 * \return Result.
 */
M_fs_error_t M_fs_file_write_sys(M_fs_file_t *fd, const unsigned char *buf, size_t count, size_t *wrote_len);

M_fs_error_t M_fs_file_seek_sys(M_fs_file_t *fd, M_int64 offset, M_fs_file_seek_t from);

M_fs_error_t M_fs_file_fsync_sys(M_fs_file_t *fd);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * dir_walk */

/*! Create and fill an entry.
 * \param full_path The ull path to the location.
 * \param rel_path The relative path to the search path. This is what will be stored in the entry.
 * \param type The type of the entry. Can be Unknown.
 * \param info The entry info. Can be NULL. If info is passed the entry will take ownership or it will be
 *             destroyed if we are not storing info.
 * \param[in] filter M_fs_dir_walk_filter_t flags controlling the behavior of the walk.
 * \return A dir entry.
 */
M_fs_dir_entry_t *M_fs_dir_walk_fill_entry(const char *full_path, const char *rel_path, M_fs_type_t type, M_fs_info_t *info, M_fs_dir_walk_filter_t filter);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * dir_entry */

/*! Create a directory entry.
 */
M_fs_dir_entry_t *M_fs_dir_entry_create(void) M_MALLOC;

/*! Set the type for a directory entry.
 * \param entry The entry.
 * \param type The type.
 */
void M_fs_dir_entry_set_type(M_fs_dir_entry_t *entry, M_fs_type_t type);

/*! Set whether this entry is hidden.
 * \param entry The entry.
 * \param hidden Whether this entry is considere hidden by the OS.
 */
void M_fs_dir_entry_set_hidden(M_fs_dir_entry_t *entry, M_bool hidden);

/*! Set the filename of the entry.
 * This should be relative to the walked path.
 * \param entry The entry.
 * \param name The filename.
 */
void M_fs_dir_entry_set_name(M_fs_dir_entry_t *entry, const char *name);

/*! Set the resolved filename of the entry.
 * This should be relative to the filename and only applies to symlinks. It is the path a symlink points to.
 * \param entry The entry.
 * \param name The resolved filename.
 */
void M_fs_dir_entry_set_resolved_name(M_fs_dir_entry_t *entry, const char *name);

/*! Set the file info for the entry.
 * \param entry The entry.
 * \param info The file info. The entry takes ownership of the info.
 */
void M_fs_dir_entry_set_info(M_fs_dir_entry_t *entry, M_fs_info_t *info);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * dir_entries */

/*! Create a list of directory entires.
 */
M_fs_dir_entries_t *M_fs_dir_entries_create(void) M_MALLOC;

/*! Insert an entry into a list of entries.
 * \param d The list.
 * \param val The entry to add.
 */
M_bool M_fs_dir_entries_insert(M_fs_dir_entries_t *d, M_fs_dir_entry_t *val);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Progress */

M_fs_progress_t *M_fs_progress_create(void);
void M_fs_progress_destroy(M_fs_progress_t *p);
void M_fs_progress_clear(M_fs_progress_t *p);
void M_fs_progress_set_path(M_fs_progress_t *p, const char *val);
void M_fs_progress_set_type(M_fs_progress_t *p, M_fs_type_t type);
void M_fs_progress_set_result(M_fs_progress_t *p, M_fs_error_t val);
void M_fs_progress_set_count_total(M_fs_progress_t *p, M_uint64 val);
void M_fs_progress_set_count(M_fs_progress_t *p, M_uint64 val);
void M_fs_progress_set_size_total(M_fs_progress_t *p, M_uint64 val);
void M_fs_progress_set_size_total_progess(M_fs_progress_t *p, M_uint64 val);
void M_fs_progress_set_size_current(M_fs_progress_t *p, M_uint64 val);
void M_fs_progress_set_size_current_progress(M_fs_progress_t *p, M_uint64 val);

__END_DECLS

#endif /* __M_FS_INT_H__ */
