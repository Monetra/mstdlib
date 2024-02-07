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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Inline functions for repetitive tasks.
 *
 * A number of operations need to be performed multiple times where the logic is the
 * same but the parameters are different. For example, modifying user/group/other perms
 * and dealing with directory override user/group/other perms.
 *
 * Instead of having the same logic upwards of 6 times in a single function where the
 * variables are the only changes we have generic inline functions which take the pointers
 * to the variables to operate on. So the logic is reused and the caller just passes in
 * what to apply the logic to.
 */

static __inline__ void M_fs_perms_merge_part(M_bool *dest_set, M_fs_perms_mode_t *dest_mode, M_fs_perms_type_t *dest_type,
        M_bool *src_set, M_fs_perms_mode_t *src_mode, M_fs_perms_type_t *src_type)
{
    if (!*src_set) {
        return;
    }
    if (*dest_set && *dest_type == M_FS_PERMS_TYPE_EXACT && *src_type != M_FS_PERMS_TYPE_EXACT) {
        if (*src_type == M_FS_PERMS_TYPE_ADD) {
            *dest_mode |= *src_mode;
        } else {
            *dest_mode &= ~(*src_mode);
        }
    } else {
        *dest_mode = *src_mode;
        *dest_type = *src_type;
    }
    *dest_set = M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_perms_t *M_fs_perms_create(void)
{
    M_fs_perms_t *perms = NULL;

    /* Will set exact and no mode. */
    perms = M_malloc_zero(sizeof(*perms));

#ifdef _WIN32
    /* Cache the size for an sid here. We don't necessary want to have UNLEN+1 scattered though out the code
     * and (sizeof(p->user_sid)/sizeof(p->user_sid)) is cumbersome. */
    perms->sid_len = UNLEN+1;
#endif

    return perms;
}

M_fs_perms_t *M_fs_perms_dup(const M_fs_perms_t *perms)
{
    M_fs_perms_t *dup_perms;

    if (perms == NULL) {
        return NULL;
    }

    dup_perms = M_fs_perms_create();

    dup_perms->user  = M_strdup(perms->user);
    dup_perms->group = M_strdup(perms->group);
#ifdef _WIN32
    CopySid(dup_perms->sid_len, dup_perms->user_sid, M_CAST_OFF_CONST(SID *, perms->user_sid));
    CopySid(dup_perms->sid_len, dup_perms->group_sid, M_CAST_OFF_CONST(SID *, perms->group_sid));
#else
    dup_perms->uid   = perms->uid;
    dup_perms->gid   = perms->gid;
#endif

    dup_perms->user_set   = perms->user_set;
    dup_perms->user_mode  = perms->user_mode;
    dup_perms->user_type  = perms->user_type;
    dup_perms->group_set  = perms->group_set;
    dup_perms->group_mode = perms->group_mode;
    dup_perms->group_mode = perms->group_mode;
    dup_perms->other_set  = perms->other_set;
    dup_perms->other_mode = perms->other_mode;
    dup_perms->other_type = perms->other_type;

    dup_perms->dir_user_set   = perms->dir_user_set;
    dup_perms->dir_user_mode  = perms->dir_user_mode;
    dup_perms->dir_user_type  = perms->dir_user_type;
    dup_perms->dir_group_set  = perms->dir_group_set;
    dup_perms->dir_group_mode = perms->dir_group_mode;
    dup_perms->dir_group_type = perms->dir_group_type;
    dup_perms->dir_other_set  = perms->dir_other_set;
    dup_perms->dir_other_mode = perms->dir_other_mode;
    dup_perms->dir_other_type = perms->dir_other_type;

    return dup_perms;
}

void M_fs_perms_merge(M_fs_perms_t **dest, M_fs_perms_t *src)
{
    if (dest == NULL || src == NULL) {
        return;
    }
    if (*dest == NULL) {
        *dest = src;
        return;
    }

    /* user and group. */
    if (src->user != NULL) {
#ifdef _WIN32
        CopySid((*dest)->sid_len, (*dest)->user_sid, M_CAST_OFF_CONST(SID *, src->user_sid));
#else
        (*dest)->uid = src->uid;
#endif
    }
    if (src->group != NULL) {
#ifdef _WIN32
        CopySid((*dest)->sid_len, (*dest)->group_sid, M_CAST_OFF_CONST(SID *, src->group_sid));
#else
        (*dest)->gid = src->gid;
#endif
    }

    /* perms. */
    M_fs_perms_merge_part(&((*dest)->user_set), &((*dest)->user_mode), &((*dest)->user_type),
        &(src->user_set), &(src->user_mode), &(src->user_type));
    M_fs_perms_merge_part(&((*dest)->group_set), &((*dest)->group_mode), &((*dest)->group_type),
        &(src->group_set), &(src->group_mode), &(src->group_type));
    M_fs_perms_merge_part(&((*dest)->other_set), &((*dest)->other_mode), &((*dest)->other_type),
        &(src->other_set), &(src->other_mode), &(src->other_type));
    /* dir override perms */
    M_fs_perms_merge_part(&((*dest)->dir_user_set), &((*dest)->dir_user_mode), &((*dest)->dir_user_type),
        &(src->dir_user_set), &(src->dir_user_mode), &(src->dir_user_type));
    M_fs_perms_merge_part(&((*dest)->dir_group_set), &((*dest)->dir_group_mode), &((*dest)->dir_group_type),
        &(src->dir_group_set), &(src->dir_group_mode), &(src->dir_group_type));
    M_fs_perms_merge_part(&((*dest)->dir_other_set), &((*dest)->dir_other_mode), &((*dest)->dir_other_type),
        &(src->dir_other_set), &(src->dir_other_mode), &(src->dir_other_type));
}

void M_fs_perms_destroy(M_fs_perms_t *perms)
{
    if (perms == NULL) {
        return;
    }

    M_free(perms->user);
    M_free(perms->group);
    M_free(perms);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

const char *M_fs_perms_get_user(const M_fs_perms_t *perms)
{
    if (perms == NULL) {
        return NULL;
    }
    return perms->user;
}

const char *M_fs_perms_get_group(const M_fs_perms_t *perms)
{
    if (perms == NULL) {
        return NULL;
    }
    return perms->group;
}


M_uint32 M_fs_perms_get_mode(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FS_PERMS_MODE_NONE;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->user_mode;
        case M_FS_PERMS_WHO_GROUP:
            return perms->group_mode;
        case M_FS_PERMS_WHO_OTHER:
            return perms->other_mode;
    }
    return M_FS_PERMS_MODE_NONE;
}

M_fs_perms_type_t M_fs_perms_get_type(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FS_PERMS_TYPE_EXACT;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->user_type;
        case M_FS_PERMS_WHO_GROUP:
            return perms->group_type;
        case M_FS_PERMS_WHO_OTHER:
            return perms->other_type;
    }
    return M_FS_PERMS_TYPE_EXACT;
}

M_bool M_fs_perms_get_isset(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FALSE;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->user_set;
        case M_FS_PERMS_WHO_GROUP:
            return perms->group_set;
        case M_FS_PERMS_WHO_OTHER:
            return perms->other_set;
    }
    return M_FALSE;
}

M_uint32 M_fs_perms_get_dir_mode(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FS_PERMS_MODE_NONE;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->dir_user_mode;
        case M_FS_PERMS_WHO_GROUP:
            return perms->dir_group_mode;
        case M_FS_PERMS_WHO_OTHER:
            return perms->dir_other_mode;
    }
    return M_FS_PERMS_MODE_NONE;
}

M_fs_perms_type_t M_fs_perms_get_dir_type(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FS_PERMS_TYPE_EXACT;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->dir_user_type;
        case M_FS_PERMS_WHO_GROUP:
            return perms->dir_group_type;
        case M_FS_PERMS_WHO_OTHER:
            return perms->dir_other_type;
    }
    return M_FS_PERMS_TYPE_EXACT;
}

M_bool M_fs_perms_get_dir_isset(const M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return M_FALSE;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            return perms->dir_user_set;
        case M_FS_PERMS_WHO_GROUP:
            return perms->dir_group_set;
        case M_FS_PERMS_WHO_OTHER:
            return perms->dir_other_set;
    }
    return M_FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_fs_perms_set_mode(M_fs_perms_t *perms, M_uint32 mode, M_fs_perms_who_t who, M_fs_perms_type_t type)
{
    if (perms == NULL) {
        return;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            perms->user_set  = M_TRUE;
            perms->user_mode = mode;
            perms->user_type = type;
            break;
        case M_FS_PERMS_WHO_GROUP:
            perms->group_set  = M_TRUE;
            perms->group_mode = mode;
            perms->group_type = type;
            break;
        case M_FS_PERMS_WHO_OTHER:
            perms->other_set  = M_TRUE;
            perms->other_mode = mode;
            perms->other_type = type;
            break;
    }
}

void M_fs_perms_set_dir_mode(M_fs_perms_t *perms, M_uint32 mode, M_fs_perms_who_t who, M_fs_perms_type_t type)
{
    if (perms == NULL) {
        return;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            perms->dir_user_set  = M_TRUE;
            perms->dir_user_mode = mode;
            perms->dir_user_type = type;
            break;
        case M_FS_PERMS_WHO_GROUP:
            perms->dir_group_set  = M_TRUE;
            perms->dir_group_mode = mode;
            perms->dir_group_type = type;
            break;
        case M_FS_PERMS_WHO_OTHER:
            perms->dir_other_set  = M_TRUE;
            perms->dir_other_mode = mode;
            perms->dir_other_type = type;
            break;
    }
}

void M_fs_perms_unset_mode(M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return;
    }
    switch (who) {
        case M_FS_PERMS_WHO_USER:
            perms->user_set  = M_FALSE;
            perms->user_mode = M_FS_PERMS_MODE_NONE;
            perms->user_type = M_FS_PERMS_TYPE_EXACT;
            break;
        case M_FS_PERMS_WHO_GROUP:
            perms->group_set  = M_FALSE;
            perms->group_mode = M_FS_PERMS_MODE_NONE;
            perms->group_type = M_FS_PERMS_TYPE_EXACT;
            break;
        case M_FS_PERMS_WHO_OTHER:
            perms->other_set  = M_FALSE;
            perms->other_mode = M_FS_PERMS_MODE_NONE;
            perms->other_type = M_FS_PERMS_TYPE_EXACT;
            break;
    }
    M_fs_perms_unset_dir_mode(perms, who);
}

void M_fs_perms_unset_dir_mode(M_fs_perms_t *perms, M_fs_perms_who_t who)
{
    if (perms == NULL) {
        return;
    }
    switch(who) {
        case M_FS_PERMS_WHO_USER:
            perms->dir_user_set  = M_FALSE;
            perms->dir_user_mode = M_FS_PERMS_MODE_NONE;
            perms->dir_user_type = M_FS_PERMS_TYPE_EXACT;
            break;
        case M_FS_PERMS_WHO_GROUP:
            perms->dir_group_set  = M_FALSE;
            perms->dir_group_mode = M_FS_PERMS_MODE_NONE;
            perms->dir_group_type = M_FS_PERMS_TYPE_EXACT;
            break;
        case M_FS_PERMS_WHO_OTHER:
            perms->dir_other_set  = M_FALSE;
            perms->dir_other_mode = M_FS_PERMS_MODE_NONE;
            perms->dir_other_type = M_FS_PERMS_TYPE_EXACT;
            break;
    }
}
