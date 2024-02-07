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

#ifndef __M_FS_UNX_H__
#define __M_FS_UNX_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

#include <sys/stat.h>
#include <unistd.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/* Convert permissions (accounting for file vs dir override perms) to unix mode. */
M_API mode_t M_fs_perms_to_mode(const M_fs_perms_t *perms, M_bool isdir);
/* Set the user and group based when given both the string name and id. */
M_API M_fs_error_t M_fs_perms_set_user_int(M_fs_perms_t *perms, const char *user, uid_t uid);
M_API M_fs_error_t M_fs_perms_set_group_int(M_fs_perms_t *perms, const char *group, gid_t gid);

M_API size_t M_fs_unx_getpw_r_size(void);
M_API size_t M_fs_unx_getgr_r_size(void);

__END_DECLS

#endif /* __M_FS_UNX_H__ */
