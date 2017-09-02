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
#include "fs/m_fs_int_win.h"
#include "platform/m_platform.h"

#include <Aclapi.h>
#include <AccCtrl.h>
#include <Shlwapi.h>
#include <Sddl.h>


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Given a string name set the user or group for the perms. This will lookup the sid for
 * the string name and only set both if the sid lookup succeeds.
 * \param perms The perms to modify.
 * \param name The name to be set.
 * \param isuser Are we setting the user or group.
 * \return Result.
 */
static M_fs_error_t M_fs_perms_set_name(M_fs_perms_t *perms, const char *name, M_bool isuser)
{
	SID_NAME_USE sid_use;
	DWORD        sid_len;
	/* We don't care about the domain but LookupAccountName requires a domain buffer */
	char         domain[DNLEN+1];
	DWORD        domain_len;

	if (perms == NULL) {
		return M_FS_ERROR_INVALID;
	}

	/* Clear the user or group */
	if (name == NULL || *name == '\0') {
		if (isuser) {
			M_free(perms->user);
			perms->user        = NULL;
		} else {
			M_free(perms->group);
			perms->group        = NULL;
		}
		return M_FS_ERROR_SUCCESS;
	}

	/* Look up the named account */
	sid_len    = (isuser?sizeof(perms->user_sid):sizeof(perms->group_sid))/(isuser?sizeof(*(perms->user_sid)):sizeof(*(perms->group_sid)));
	domain_len = sizeof(domain);
	if (!LookupAccountName(NULL, name, isuser?perms->user_sid:perms->group_sid, &sid_len, domain, &domain_len, &sid_use)) {
		return M_fs_error_from_syserr(GetLastError());
	}
	/* Check that the lookup returned the correct type */
	if ((isuser && sid_use != SidTypeUser) || (!isuser && sid_use != SidTypeGroup)) {
		return M_FS_ERROR_INVALID;
	}

	/* Set the values */
	if (isuser) {
		M_free(perms->user);
		perms->user = M_strdup(name);
	} else {
		M_free(perms->group);
		perms->group = M_strdup(name);
	}
	return M_FS_ERROR_SUCCESS;
}

/* Converts a perms entry to a DACL entry.
 * Return true if the access was filled otherwise false. */
static __inline__ M_bool M_fs_perms_to_dacl_entry(const M_bool *isdir,
	const M_bool *p_set, const M_bool *p_dir_set,
	const M_fs_perms_mode_t *p_mode, const M_fs_perms_mode_t *p_dir_mode,
	const M_fs_perms_type_t *p_type, const M_fs_perms_type_t *p_dir_type,
	PEXPLICIT_ACCESS access, PSID sid, M_bool isuser, M_bool isowner)
{
	M_fs_perms_mode_t mymode = M_FS_PERMS_MODE_NONE;
	M_fs_perms_type_t mytype = M_FS_PERMS_TYPE_EXACT;
	M_bool            isset  = M_FALSE;
	DWORD             perms  = 0;

	if (access == NULL || sid == NULL) {
		return M_FALSE;
	}

	if (*isdir && *p_dir_set) {
		isset  = M_TRUE;
		mymode = *p_dir_mode;
		mytype = *p_dir_type;
	} else if (*p_set) {
		isset  = M_TRUE;
		mymode = *p_mode;
		mytype = *p_type;
	}

	if (!isset) {
		return M_FALSE;
	}

	if (mytype != M_FS_PERMS_TYPE_EXACT && mytype != M_FS_PERMS_TYPE_ADD) {
		return M_FALSE;
	}

	/* Set perms that the owner of a file should alway have */
	if (isowner) {
		perms |= WRITE_DAC|READ_CONTROL|WRITE_OWNER|DELETE;
	}

	/* Calculate the perms that need to be allowed */
	if (mymode & M_FS_PERMS_MODE_READ) {
		perms |= GENERIC_READ|FILE_GENERIC_READ;
	}
	if (mymode & M_FS_PERMS_MODE_WRITE) {
		perms |= GENERIC_WRITE|FILE_GENERIC_WRITE;
	}
	if (mymode & M_FS_PERMS_MODE_EXEC) {
		perms |= GENERIC_EXECUTE|FILE_GENERIC_EXECUTE;
	}

	/* Set the perms */
	access->grfAccessMode                     = SET_ACCESS;
	access->grfAccessPermissions              = perms;
	access->grfInheritance                    = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	access->Trustee.pMultipleTrustee          = NULL;
	access->Trustee.MultipleTrusteeOperation  = NO_MULTIPLE_TRUSTEE;
	access->Trustee.TrusteeForm               = TRUSTEE_IS_SID;
	access->Trustee.TrusteeType               = isuser ? TRUSTEE_IS_USER : TRUSTEE_IS_GROUP;
	access->Trustee.ptstrName                 = sid;

	return M_TRUE;
}

/* Converts perms to DACL */
static void M_fs_perms_to_dacl_entries(const M_fs_perms_t *perms, PSID everyone_sid, PEXPLICIT_ACCESS access, size_t access_size, ULONG *access_cnt, M_bool isdir)
{
	/* Must contain at least 3 EXPLICIT_ACCESS because we want to set for user/group/other permissions */
	if (perms == NULL || access == NULL || access_size < 3) {
		return;
	}
	*access_cnt = 0;

	/* user */
	if (perms->user != NULL && M_fs_perms_to_dacl_entry(&isdir,
			&(perms->user_set), &(perms->dir_user_set),
			&(perms->user_mode), &(perms->dir_user_mode),
			&(perms->user_type), &(perms->dir_user_type),
			&(access[*access_cnt]), M_CAST_OFF_CONST(PSID, perms->user_sid),
			M_TRUE, M_TRUE)
		)
	{
		(*access_cnt)++;
	}
	/* group */
	if (perms->group != NULL && M_fs_perms_to_dacl_entry(&isdir,
			&(perms->group_set), &(perms->dir_group_set),
			&(perms->group_mode), &(perms->dir_group_mode),
			&(perms->group_type), &(perms->dir_group_type),
			&(access[*access_cnt]), M_CAST_OFF_CONST(PSID, perms->group_sid),
			M_FALSE, M_FALSE)
		)
	{
		(*access_cnt)++;
	}
	/* other */
	if (everyone_sid != NULL && M_fs_perms_to_dacl_entry(&isdir,
			&(perms->other_set), &(perms->dir_other_set),
			&(perms->other_mode), &(perms->dir_other_mode),
			&(perms->other_type), &(perms->dir_other_type),
			&(access[*access_cnt]), everyone_sid, M_FALSE, M_FALSE)
		)
	{
		(*access_cnt)++;
	}
}

/* Set the user or group when the sid is known. */
static M_fs_error_t M_fs_perms_set_ug_int(const char *name, PSID sid, char **set_name, PSID set_sid, DWORD set_sid_len)
{
	M_free(*set_name);
	if (name == NULL || !IsValidSid(sid)) {
		*set_name = NULL;
		return M_FS_ERROR_SUCCESS;
	}

	*set_name = M_strdup(name);
	if (!CopySid(set_sid_len, set_sid, sid)) {
		M_free(*set_name);
		*set_name = NULL;
		return M_fs_error_from_syserr(GetLastError());
	}

	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * m_perms_win.h */

M_fs_error_t M_fs_perms_to_dacl(const M_fs_perms_t *perms, PSID everyone_sid, PACL *acl, M_bool isdir)
{
	EXPLICIT_ACCESS access[3];
	ULONG           access_cnt = 0;
	DWORD           ret;
	DWORD           acl_len;

	if (acl == NULL) {
		return M_FS_ERROR_INVALID;
	}
	*acl = NULL;

	if (perms == NULL) {
		return M_FS_ERROR_SUCCESS;
	}

	M_mem_set(access, 0, sizeof(access));
	M_fs_perms_to_dacl_entries(perms, everyone_sid, access, sizeof(access)/sizeof(*access), &access_cnt, isdir);
	if (access_cnt == 0) {
		/* Create an empty DACL. An empty DACL gives no permissions. Unlike NULL which gives all permissions. */
		acl_len = (sizeof(*(*acl))+(sizeof(DWORD)-1))&0xfffffffc;
		*acl = LocalAlloc(LPTR, acl_len);
		if (!InitializeAcl(*acl, acl_len, ACL_REVISION)) {
			LocalFree(*acl);
			*acl = NULL;
			return M_fs_error_from_syserr(GetLastError());
		}
	} else {
		/* Set the specific calculated permissions. */
		ret = SetEntriesInAcl(access_cnt, access, NULL, acl);
		if (ret != ERROR_SUCCESS) {
			return M_fs_error_from_syserr(ret);
		}
	}
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_sd_user(const M_fs_perms_t *perms, PSECURITY_DESCRIPTOR sd)
{
	if (perms->user == NULL) {
		return M_FS_ERROR_SUCCESS;
	}
	if (!SetSecurityDescriptorOwner(sd, M_CAST_OFF_CONST(PSID, perms->user_sid), FALSE)) {
		return M_fs_error_from_syserr(GetLastError());
	}
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_sd_group(const M_fs_perms_t *perms, PSECURITY_DESCRIPTOR sd)
{
	if (perms->group == NULL) {
		return M_FS_ERROR_SUCCESS;
	}
	if (!SetSecurityDescriptorGroup(sd, M_CAST_OFF_CONST(PSID, perms->group_sid), FALSE)) {
		return M_fs_error_from_syserr(GetLastError());
	}
	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_user_int(M_fs_perms_t *perms, const char *user, PSID sid)
{
	if (perms == NULL) {
		return M_FS_ERROR_INVALID;
	}
	return M_fs_perms_set_ug_int(user, sid, &(perms->user), perms->user_sid, sizeof(perms->user_sid)/sizeof(*(perms->user_sid)));
}

M_fs_error_t M_fs_perms_set_group_int(M_fs_perms_t *perms, const char *group, PSID sid)
{
	if (perms == NULL) {
		return M_FS_ERROR_INVALID;
	}
	return M_fs_perms_set_ug_int(group, sid, &(perms->group), perms->group_sid, sizeof(perms->group_sid)/sizeof(*(perms->group_sid)));
}

/* The everyone sid needs to remain valid for the life of the sa. */
M_fs_error_t M_fs_perms_to_security_attributes(M_fs_perms_t *perms, PSID everyone_sid, PACL *acl, PSECURITY_ATTRIBUTES sa, PSECURITY_DESCRIPTOR sd)
{
	char          proc_username[UNLEN+1];
	DWORD         proc_username_len;
	M_fs_error_t  res;

	if (perms == NULL || acl == NULL || sa == NULL || sd == NULL) {
		return M_FS_ERROR_INVALID;
	}

	/* Primary user for the perms is not set so we are going to use the user for the
	 * process. A user has to be set. */
	if (M_fs_perms_get_user(perms) == NULL) {
		proc_username_len = sizeof(proc_username);
		if (!GetUserName(proc_username, &proc_username_len)) {
			return M_fs_error_from_syserr(GetLastError());
		}
		res = M_fs_perms_set_user(perms, proc_username);
		if (res != M_FS_ERROR_SUCCESS) {
			return res;
		}
	}

	/* Generate a acl from the perms. */
	res = M_fs_perms_to_dacl(perms, everyone_sid, acl, M_FALSE);
	if (res != M_FS_ERROR_SUCCESS)
		return res;
	/* Create a security descripitor to add the perms and user and group to */
	if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)) {
		LocalFree(*acl);
		return res;
	}
	if (!SetSecurityDescriptorDacl(sd, TRUE, *acl, FALSE)) {
		LocalFree(*acl);
		return res;
	}
	res = M_fs_perms_set_sd_user(perms, sd);
	if (res != M_FS_ERROR_SUCCESS) {
		LocalFree(*acl);
		return res;
	}
	res = M_fs_perms_set_sd_group(perms, sd);
	if (res != M_FS_ERROR_SUCCESS) {
		LocalFree(*acl);
		return res;
	}
	/* Add the security descriptor to the security attributes */
	M_mem_set(sa, 0, sizeof(*sa));
	sa->nLength              = sizeof(*sa);
	sa->lpSecurityDescriptor = sd;
	sa->bInheritHandle       = FALSE;

	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * m_perms.h */

M_fs_error_t M_fs_perms_set_perms(const M_fs_perms_t *perms, const char *path)
{
	M_fs_info_t          *info;
	char                 *norm_path;
	M_fs_error_t          res;
	DWORD                 ret;
	M_fs_perms_t         *myperms;
	M_bool                isdir;
	PACL                  acl          = NULL;
	PSID                  user_sid     = NULL;
	PSID                  group_sid    = NULL;
	PSID                  everyone_sid = NULL;
	/* Set PROTECTED_DACL_SECURITY_INFORMATION so that perms are not inherited from the container */
	SECURITY_INFORMATION  sec_info     = DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION;


	if (perms == NULL || path == NULL) {
		return M_FS_ERROR_INVALID;
	}

	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}
	res = M_fs_info(&info, norm_path, M_FS_PATH_INFO_FLAGS_FOLLOW_SYMLINKS);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	/* Get the original perms and combine it with the perms we want to set */
	myperms = M_fs_perms_dup(M_fs_info_get_perms(info));
	isdir   = (M_fs_info_get_type(info) == M_FS_TYPE_DIR)?M_TRUE:M_FALSE;
	M_fs_info_destroy(info);
	M_fs_perms_merge(&myperms, M_fs_perms_dup(perms));

	/* Get the everyone SID. This needs to remain valid until after SetNamedSecurityInfo is called */
	if (!ConvertStringSidToSid("S-1-1-0", &everyone_sid)) {
		everyone_sid = NULL;
	}

	/* Convert the perms to a dacl. */
	res = M_fs_perms_to_dacl(myperms, everyone_sid, &acl, isdir);
	if (res != M_FS_ERROR_SUCCESS) {
		LocalFree(everyone_sid);
		M_free(norm_path);
		return res;
	}

	/* Get the user and group we're setting on the file */
	if (perms->user != NULL) {
		sec_info |= OWNER_SECURITY_INFORMATION;
		user_sid  = M_CAST_OFF_CONST(PSID, perms->user_sid);
	}
	if (perms->group != NULL) {
		sec_info |= GROUP_SECURITY_INFORMATION;
		group_sid = M_CAST_OFF_CONST(PSID, perms->group_sid);
	}

	/* Apply the perms to the file */
	ret = SetNamedSecurityInfo(norm_path, SE_FILE_OBJECT, sec_info, user_sid, group_sid, acl, NULL);
	if (ret != ERROR_SUCCESS) {
		LocalFree(acl);
		LocalFree(everyone_sid);
		M_free(norm_path);
		return M_fs_error_from_syserr(ret);
	}
	LocalFree(everyone_sid);
	LocalFree(acl);
	M_free(norm_path);

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_can_access(const char *path, M_uint32 mode)
{
	char         *norm_path;
	M_fs_error_t  res;
	DWORD         access_mode = 0;
	HANDLE        fd;
	M_bool        ret;

	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	/* Check for existence. */
	if (mode == 0) {
		ret = PathFileExists(norm_path);
		M_free(norm_path);
		if (ret)
			return M_FS_ERROR_SUCCESS;
		return M_fs_error_from_syserr(GetLastError());
	}

	/* Check specific perms. */
	if (mode & M_FS_PERMS_MODE_READ)
		access_mode |= GENERIC_READ;
	if (mode & M_FS_PERMS_MODE_WRITE)
		access_mode |= GENERIC_WRITE|DELETE;
	if (mode & M_FS_PERMS_MODE_EXEC)
		access_mode |= GENERIC_EXECUTE;

	/* Check by opening the file with the specific perms requested. */
	fd = CreateFile(norm_path, access_mode, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS, NULL);
	M_free(norm_path);
	if (fd == INVALID_HANDLE_VALUE) {
		return M_fs_error_from_syserr(GetLastError());
	}
	CloseHandle(fd);

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_perms_set_user(M_fs_perms_t *perms, const char *user)
{
	return M_fs_perms_set_name(perms, user, M_TRUE);
}

M_fs_error_t M_fs_perms_set_group(M_fs_perms_t *perms, const char *group)
{
	return M_fs_perms_set_name(perms, group, M_FALSE);
}
