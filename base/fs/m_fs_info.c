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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_fs_info {
	char        *user;
	char        *group;

	M_fs_type_t  type;
	M_bool       hidden;

	M_uint64     size;
	M_time_t     atime;
	M_time_t     mtime;
	M_time_t     ctime;
	M_time_t     btime; /* Not available on all platforms, will be set to 0 if not found. */

	M_fs_perms_t     *perms;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_info_t *M_fs_info_create(void)
{
	M_fs_info_t *info;
	info = M_malloc_zero(sizeof(*info));
	return info;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_fs_info_destroy(M_fs_info_t *info)
{
	if (info == NULL) {
		return;
	}

	M_free(info->user);
	info->user = NULL;
	M_free(info->group);
	info->group = NULL;

	M_fs_perms_destroy(info->perms);
	info->perms = NULL;

	M_free(info);
}

const char *M_fs_info_get_user(const M_fs_info_t *info)
{
	if (info == NULL) {
		return NULL;
	}
	return info->user;
}

const char *M_fs_info_get_group(const M_fs_info_t *info)
{
	if (info == NULL) {
		return NULL;
	}
	return info->group;
}

M_fs_type_t M_fs_info_get_type(const M_fs_info_t *info)
{
	if (info == NULL)
		return M_FS_TYPE_UNKNOWN;
	return info->type;
}

M_bool M_fs_info_get_ishidden(const M_fs_info_t *info)
{
	if (info == NULL)
		return M_FALSE;
	return info->hidden;
}

M_uint64 M_fs_info_get_size(const M_fs_info_t *info)
{
	if (info == NULL) {
		return 0;
	}
	return info->size;
}

M_time_t M_fs_info_get_atime(const M_fs_info_t *info)
{
	if (info == NULL) {
		return 0;
	}
	return info->atime;
}

M_time_t M_fs_info_get_mtime(const M_fs_info_t *info)
{
	if (info == NULL) {
		return 0;
	}
	return info->mtime;
}

M_time_t M_fs_info_get_ctime(const M_fs_info_t *info)
{
	if (info == NULL) {
		return 0;
	}
	return info->ctime;
}

M_time_t M_fs_info_get_btime(const M_fs_info_t *info)
{
	if (info == NULL) {
		return 0;
	}
	return info->btime;
}

const M_fs_perms_t *M_fs_info_get_perms(const M_fs_info_t *info)
{
	if (info == NULL) {
		return NULL;
	}
	return info->perms;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_fs_info_set_user(M_fs_info_t *info, const char *val)
{
	if (info == NULL) {
		return;
	}
	M_free(info->user);
	info->user = M_strdup(val);
}

void M_fs_info_set_group(M_fs_info_t *info, const char *val)
{
	if (info == NULL) {
		return;
	}
	M_free(info->group);
	info->group = M_strdup(val);
}

void M_fs_info_set_type(M_fs_info_t *info, M_fs_type_t val)
{
	if (info == NULL) {
		return;
	}
	info->type = val;
}

void M_fs_info_set_hidden(M_fs_info_t *info, M_bool val)
{
	if (info == NULL) {
		return;
	}
	info->hidden = val;
}

void M_fs_info_set_size(M_fs_info_t *info, M_uint64 val)
{
	if (info == NULL) {
		return;
	}
	info->size = val;
}

void M_fs_info_set_atime(M_fs_info_t *info, M_time_t val)
{
	if (info == NULL) {
		return;
	}
	info->atime = val;
}

void M_fs_info_set_mtime(M_fs_info_t *info, M_time_t val)
{
	if (info == NULL) {
		return;
	}
	info->mtime = val;
}

void M_fs_info_set_ctime(M_fs_info_t *info, M_time_t val)
{
	if (info == NULL) {
		return;
	}
	info->ctime = val;
}

void M_fs_info_set_btime(M_fs_info_t *info, M_time_t val)
{
	if (info == NULL) {
		return;
	}
	info->btime = val;
}

void M_fs_info_set_perms(M_fs_info_t *info, M_fs_perms_t *val)
{
	if (info == NULL) {
		return;
	}
	M_fs_perms_destroy(info->perms);
	info->perms = val;
}
