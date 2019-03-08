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

#ifndef __M_FS_WIN_H__
#define __M_FS_WIN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include "platform/m_platform.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/* Convert perms to a dacl */
M_API M_fs_error_t M_fs_perms_to_dacl(const M_fs_perms_t *perms, PSID everyone_sid, PACL *acl, M_bool isdir);
/* Set the user/group on an sd from perms */
M_API M_fs_error_t M_fs_perms_set_sd_user(const M_fs_perms_t *perms, PSECURITY_DESCRIPTOR sd);
M_API M_fs_error_t M_fs_perms_set_sd_group(const M_fs_perms_t *perms, PSECURITY_DESCRIPTOR sd);
/* Set the user/group when the name and sid is known. */
M_API M_fs_error_t M_fs_perms_set_user_int(M_fs_perms_t *perms, const char *user, PSID sid);
M_API M_fs_error_t M_fs_perms_set_group_int(M_fs_perms_t *perms, const char *group, PSID sid);
/* Fill in an acl, sd, sd */
M_fs_error_t M_fs_perms_to_security_attributes(M_fs_perms_t *perms, PSID everyone_sid, PACL *acl, PSECURITY_ATTRIBUTES sa, PSECURITY_DESCRIPTOR sd);

__END_DECLS

#endif /* __M_FS_WIN_H__ */
