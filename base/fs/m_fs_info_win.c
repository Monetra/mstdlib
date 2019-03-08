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
#include "time/m_time_int.h"
#include "platform/m_platform.h"

#include <Aclapi.h>
#include <AccCtrl.h>
#include <Authz.h>
#include <Lmcons.h>
#include <Sddl.h>


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Get the user/group and their sids from a security descriptor.
 * \param sd The security descriptor to read the user/group/sids from.
 * \param user[out] The user for the sd. This will return alloced memory that must be freed.
 * \param user_sid[out] A buffer to hold the user's sid.
 * \param user_sid_len The length of the user sid buffer.
 * \param group[out] The group for the sid. This will return alloced memory that must be freed.
 * \param group_sid[out] A buffer to hold the group's sid.
 * \param group_sid_len The length of the group sid buffer.
 * \return Result.
 */
static M_fs_error_t M_fs_info_get_file_user_group(PSECURITY_DESCRIPTOR sd, char **user, PSID user_sid, DWORD user_sid_len, char **group, PSID group_sid, DWORD group_sid_len)
{
	PSID          myuser_sid;
	PSID          mygroup_sid;
	DWORD         user_len   = 0;
	DWORD         group_len  = 0;
	/* We don't care about the domin but the LookupAccountSid requires a buffer for the domain */
	char          domain[DNLEN+1];
	DWORD         domain_len;
	SID_NAME_USE  sid_use;
	BOOL          defaulted;

	if (user != NULL) {
		*user  = NULL;
	}
	if (group != NULL) {
		*group = NULL;
	}

	if (sd == NULL || user == NULL || user_sid == NULL || user_sid_len == 0 || group == NULL || group_sid == NULL || group_sid_len == 0) {
		return M_FS_ERROR_INVALID;
	}

	/* Get the user and group sids */
	if (!GetSecurityDescriptorOwner(sd, &myuser_sid, &defaulted)) {
		return M_fs_error_from_syserr(GetLastError());
	}
	if (!GetSecurityDescriptorGroup(sd, &mygroup_sid, &defaulted)) {
		return M_fs_error_from_syserr(GetLastError());
	}

	/* First get the lengths of the user and group so we know how much memory needs to be allocated to hold them */
	/* Find out the length of the user name */
	domain_len = sizeof(domain);
	LookupAccountSid(NULL, myuser_sid, NULL, &user_len, domain, &domain_len, &sid_use);
	if (user_len == 0) {
		return M_fs_error_from_syserr(GetLastError());
	}
	/* Find out the length of the group name */
	domain_len = sizeof(domain);
	LookupAccountSid(NULL, mygroup_sid, NULL, &group_len, domain, &domain_len, &sid_use);

	/* Allocate the memory and call the lookup function again to fill in the user and group names */
	if (IsValidSid(myuser_sid)) {
		*user = M_malloc(sizeof(**user)*user_len);
		domain_len = sizeof(domain);
		if (!LookupAccountSid(NULL, myuser_sid, *user, &user_len, domain, &domain_len, &sid_use)) {
			M_free(*user);
			*user = NULL;
			return M_fs_error_from_syserr(GetLastError());
		}
		/* Verify it looked up the proper type */
		if (sid_use != SidTypeUser && sid_use != SidTypeAlias && sid_use != SidTypeDeletedAccount) {
			M_free(*user);
			*user = NULL;
			return M_FS_ERROR_INVALID;
		}
		CopySid(user_sid_len, user_sid, myuser_sid);
	} else {
		return M_FS_ERROR_INVALID;
	}
	if (group_len != 0 && IsValidSid(mygroup_sid)) {
		*group = M_malloc(sizeof(**group)*group_len);
		domain_len = sizeof(domain);
		if (LookupAccountSid(NULL, mygroup_sid, *group, &group_len, domain, &domain_len, &sid_use) && (sid_use == SidTypeGroup || sid_use == SidTypeWellKnownGroup)) {
			CopySid(group_sid_len, group_sid, mygroup_sid);
		} else {
			M_free(*group);
			*group = NULL;
		}
	}

	return M_FS_ERROR_SUCCESS;
}

/* Using Authz api instead of GetEffectiveRightsFromAcl because GetEffectiveRightsFromAcl "... may be altered or
 * unavailable in subsequent [OS] versions". Authz is the recommend way to determine a user or groups perimissions in
 * regard to a file. */
static unsigned int M_fs_info_sid_perms(PSECURITY_DESCRIPTOR sd, PSID sid)
{
	AUTHZ_ACCESS_REQUEST          access_request;
	AUTHZ_ACCESS_REPLY            access_reply;
	AUTHZ_CLIENT_CONTEXT_HANDLE   authz_client  = NULL;
	AUTHZ_RESOURCE_MANAGER_HANDLE authz_manager = NULL;
	ACCESS_MASK                   access_mask   = 0;
	DWORD                         access_error  = 0;
	LUID                          id;
	int                           myperms       = 0;

	/* Can determine permissions for a valid sid */
	if (!IsValidSid(sid)) {
		return 0;
	}

	if (!AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, NULL, &authz_manager)) {
		return 0;
	}

	M_mem_set(&id, 0, sizeof(id));
	if (!AuthzInitializeContextFromSid(0, sid, authz_manager, NULL, id, NULL, &authz_client)) {
		AuthzFreeResourceManager(authz_manager);
		return 0;
	}

	M_mem_set(&access_request, 0, sizeof(access_request));
	access_request.DesiredAccess        = MAXIMUM_ALLOWED;
	access_request.PrincipalSelfSid     = sid;
	access_request.ObjectTypeList       = NULL;
	access_request.ObjectTypeListLength = 0;
	access_request.OptionalArguments    = NULL;

	M_mem_set(&access_reply, 0, sizeof(access_reply));
	access_reply.ResultListLength  = 1;
	access_reply.GrantedAccessMask = &access_mask;
	access_reply.Error             = &access_error;

	if (!AuthzAccessCheck(0, authz_client, &access_request, NULL, sd, NULL, 0, &access_reply, NULL) || *(access_reply.Error) != ERROR_SUCCESS) {
		AuthzFreeContext(authz_client);
		AuthzFreeResourceManager(authz_manager);
		return 0;
	}

	if ((*(access_reply.GrantedAccessMask) & GENERIC_ALL) == GENERIC_ALL || (*(access_reply.GrantedAccessMask) & FILE_ALL_ACCESS) == FILE_ALL_ACCESS)
	{
		AuthzFreeContext(authz_client);
		AuthzFreeResourceManager(authz_manager);
		return M_FS_PERMS_MODE_READ|M_FS_PERMS_MODE_WRITE|M_FS_PERMS_MODE_EXEC;
	}

	if ((*(access_reply.GrantedAccessMask) & GENERIC_READ) == GENERIC_READ || (*(access_reply.GrantedAccessMask) & FILE_GENERIC_READ) == FILE_GENERIC_READ) {
		myperms |= M_FS_PERMS_MODE_READ;
	}
	if ((*(access_reply.GrantedAccessMask) & GENERIC_WRITE) == GENERIC_WRITE || (*(access_reply.GrantedAccessMask) & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE) {
		myperms |= M_FS_PERMS_MODE_WRITE;
	}
	if ((*(access_reply.GrantedAccessMask) & GENERIC_EXECUTE) == GENERIC_EXECUTE || (*(access_reply.GrantedAccessMask) & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE) {
		myperms |= M_FS_PERMS_MODE_EXEC;
	}

	AuthzFreeContext(authz_client);
	AuthzFreeResourceManager(authz_manager);
	return (unsigned int)myperms;
}

/* Maps the DACL permissions to our M_fs_perms_mode_t (posix like perms). */
static M_fs_perms_t *M_fs_info_security_info_to_perms(PSECURITY_DESCRIPTOR sd, PSID user_sid, PSID group_sid)
{
	M_fs_perms_t            *perms;
	PACL                  acl = NULL;
	PSID                  everyone_sid;
	BOOL                  acl_present;
	BOOL                  defaulted;

	perms = M_fs_perms_create();

	if (!GetSecurityDescriptorDacl(sd, &acl_present, &acl, &defaulted)) {
		M_fs_perms_destroy(perms);
		return NULL;
	}

	/* NULL acl means all perms are granted. A NULL acl is different than an empty acl which grants no permissions. */
	if (!acl_present || acl == NULL) {
		M_fs_perms_set_mode(perms, M_FS_PERMS_MODE_READ|M_FS_PERMS_MODE_WRITE|M_FS_PERMS_MODE_EXEC, M_FS_PERMS_WHO_USER, M_FS_PERMS_TYPE_EXACT);
		M_fs_perms_set_mode(perms, M_FS_PERMS_MODE_READ|M_FS_PERMS_MODE_WRITE|M_FS_PERMS_MODE_EXEC, M_FS_PERMS_WHO_GROUP, M_FS_PERMS_TYPE_EXACT);
		M_fs_perms_set_mode(perms, M_FS_PERMS_MODE_READ|M_FS_PERMS_MODE_WRITE|M_FS_PERMS_MODE_EXEC, M_FS_PERMS_WHO_OTHER, M_FS_PERMS_TYPE_EXACT);
		return perms;
	}

	/* User rights */
	M_fs_perms_set_mode(perms, M_fs_info_sid_perms(sd, user_sid), M_FS_PERMS_WHO_USER, M_FS_PERMS_TYPE_EXACT);

	/* Group rights */
	M_fs_perms_set_mode(perms, M_fs_info_sid_perms(sd, group_sid), M_FS_PERMS_WHO_GROUP, M_FS_PERMS_TYPE_EXACT);

	/* Other rights */
	if (ConvertStringSidToSid("S-1-1-0", &everyone_sid)) {
		M_fs_perms_set_mode(perms, M_fs_info_sid_perms(sd, everyone_sid), M_FS_PERMS_WHO_OTHER, M_FS_PERMS_TYPE_EXACT);
		LocalFree(everyone_sid);
	} else {
		M_fs_perms_set_mode(perms, M_FS_PERMS_MODE_NONE, M_FS_PERMS_WHO_OTHER, M_FS_PERMS_TYPE_EXACT);
	}

	return perms;
}

/* This is nearly identical to M_fs_info_int but because we have an fd not a path and the
 * file data type is different we need two different functions. */
static M_fs_error_t M_fs_info_file_int(M_fs_info_t **info, M_fs_file_t *fd, M_fs_info_flags_t flags, BY_HANDLE_FILE_INFORMATION *file_data)
{
	char                 *user;
	char                 *group;
	M_fs_perms_t         *perms;
	M_fs_error_t          res;
	DWORD                 ret;
	ULARGE_INTEGER        large_val;
	SID                   user_sid[UNLEN+1];
	SID                   group_sid[UNLEN+1];
	PSECURITY_DESCRIPTOR  sd;

	if (info == NULL || file_data == NULL) {
		return M_FS_ERROR_INVALID;
	}

	/* Fill in our M_fs_info_t. */
	*info = M_fs_info_create();

	/* Basic info. */
	M_fs_info_set_type(*info, (file_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? M_FS_TYPE_DIR : M_FS_TYPE_FILE);
	M_fs_info_set_hidden(*info, (file_data->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? M_TRUE : M_FALSE);

	large_val.LowPart  = file_data->nFileSizeLow;
	large_val.HighPart = file_data->nFileSizeHigh;
	M_fs_info_set_size(*info, large_val.QuadPart);

	/* If the system can't pull these times (I'm looking at you ftCreationTime) the
 	 * value will be 0 (the Windows Epoc of Jan 1, 1601). It's not possibly for a file
	 * to be created at that time because Windows didn't exist back then. If this is 0
	 * we set the time to 0 because we are stating in the docs that if these aren't
	 * avaliable they'll be set to 0. */
	if (M_time_filetime_to_int64(&(file_data->ftLastAccessTime)) == 0) {
		M_fs_info_set_atime(*info, 0);
	} else {
		M_fs_info_set_atime(*info, M_time_from_filetime(&(file_data->ftLastAccessTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftLastWriteTime)) == 0) {
		M_fs_info_set_mtime(*info, 0);
	} else {
		M_fs_info_set_mtime(*info, M_time_from_filetime(&(file_data->ftLastWriteTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftCreationTime)) == 0) {
		M_fs_info_set_ctime(*info, 0);
	} else {
		M_fs_info_set_ctime(*info, M_time_from_filetime(&(file_data->ftCreationTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftCreationTime)) == 0) {
		M_fs_info_set_btime(*info, 0);
	} else {
		M_fs_info_set_btime(*info, M_time_from_filetime(&(file_data->ftCreationTime)));
	}

	if (flags & M_FS_PATH_INFO_FLAGS_BASIC) {
		return M_FS_ERROR_SUCCESS;
	}

	/* The following data is every expensive and slow to pull */

	/* Get the security descriptor so we can get info about the file */
	ret = GetSecurityInfo(fd->fd, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sd);
	if (ret != ERROR_SUCCESS) {
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return M_fs_error_from_syserr(ret);
	}

	/* User and group. */
	res = M_fs_info_get_file_user_group(sd, &user, user_sid, sizeof(user_sid), &group, group_sid, sizeof(group_sid));
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(user);
		M_free(group);
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return res;
	}
	M_fs_info_set_user(*info, user);
	M_fs_info_set_group(*info, group);
	M_free(user);
	M_free(group);
	if (M_fs_info_get_group(*info) == NULL) {
		M_mem_set(group_sid, 0, sizeof(group_sid));
	}

	/* Perms. */
	perms = M_fs_info_security_info_to_perms(sd, user_sid, group_sid);
	if (perms == NULL) {
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return M_FS_ERROR_GENERIC;
	}
	LocalFree(sd);
	if ((res = M_fs_perms_set_user_int(perms, M_fs_info_get_user(*info), user_sid)) != M_FS_ERROR_SUCCESS ||
		(res = M_fs_perms_set_group_int(perms, M_fs_info_get_group(*info), group_sid)) != M_FS_ERROR_SUCCESS)
	{
		M_fs_perms_destroy(perms);
		M_fs_info_destroy(*info);
		*info = NULL;
		return res;
	}
	M_fs_info_set_perms(*info, perms);

	return M_FS_ERROR_SUCCESS;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_fs_error_t M_fs_info_int(M_fs_info_t **info, const char *path, M_fs_info_flags_t flags, WIN32_FIND_DATA *file_data)
{
	char                 *user;
	char                 *group;
	M_fs_perms_t         *perms;
	M_fs_error_t          res;
	DWORD                 ret;
	ULARGE_INTEGER        large_val;
	SID                   user_sid[UNLEN+1];
	SID                   group_sid[UNLEN+1];
	PSECURITY_DESCRIPTOR  sd;

	if (info == NULL || file_data == NULL || (!(flags & M_FS_PATH_INFO_FLAGS_BASIC) && (path == NULL || *path == '\0'))) {
		return M_FS_ERROR_INVALID;
	}

	/* Fill in our M_fs_info_t. */
	*info = M_fs_info_create();

	/* Basic info. */
	M_fs_info_set_type(*info, (file_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? M_FS_TYPE_DIR : M_FS_TYPE_FILE);
	M_fs_info_set_hidden(*info, (file_data->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? M_TRUE : M_FALSE);

	large_val.LowPart  = file_data->nFileSizeLow;
	large_val.HighPart = file_data->nFileSizeHigh;
	M_fs_info_set_size(*info, large_val.QuadPart);

	/* If the system can't pull these times (I'm looking at you ftCreationTime) the
 	 * value will be 0 (the Windows Epoc of Jan 1, 1601). It's not possibly for a file
	 * to be created at that time because Windows didn't exist back then. If this is 0
	 * we set the time to 0 because we are stating in the docs that if these aren't
	 * avaliable they'll be set to 0. */
	if (M_time_filetime_to_int64(&(file_data->ftLastAccessTime)) == 0) {
		M_fs_info_set_atime(*info, 0);
	} else {
		M_fs_info_set_atime(*info, M_time_from_filetime(&(file_data->ftLastAccessTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftLastWriteTime)) == 0) {
		M_fs_info_set_mtime(*info, 0);
	} else {
		M_fs_info_set_mtime(*info, M_time_from_filetime(&(file_data->ftLastWriteTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftCreationTime)) == 0) {
		M_fs_info_set_ctime(*info, 0);
	} else {
		M_fs_info_set_ctime(*info, M_time_from_filetime(&(file_data->ftCreationTime)));
	}
	if (M_time_filetime_to_int64(&(file_data->ftCreationTime)) == 0) {
		M_fs_info_set_btime(*info, 0);
	} else {
		M_fs_info_set_btime(*info, M_time_from_filetime(&(file_data->ftCreationTime)));
	}

	if (flags & M_FS_PATH_INFO_FLAGS_BASIC) {
		return M_FS_ERROR_SUCCESS;
	}

	/* The following data is every expensive and slow to pull */

	/* Get the security descriptor so we can get info about the file */
	ret = GetNamedSecurityInfo(M_CAST_OFF_CONST(char *, path), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sd);
	if (ret != ERROR_SUCCESS) {
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return M_fs_error_from_syserr(ret);
	}

	/* User and group. */
	res = M_fs_info_get_file_user_group(sd, &user, user_sid, sizeof(user_sid), &group, group_sid, sizeof(group_sid));
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(user);
		M_free(group);
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return res;
	}
	M_fs_info_set_user(*info, user);
	M_fs_info_set_group(*info, group);
	M_free(user);
	M_free(group);
	if (M_fs_info_get_group(*info) == NULL) {
		M_mem_set(group_sid, 0, sizeof(group_sid));
	}

	/* Perms. */
	perms = M_fs_info_security_info_to_perms(sd, user_sid, group_sid);
	if (perms == NULL) {
		M_fs_info_destroy(*info);
		*info = NULL;
		LocalFree(sd);
		return M_FS_ERROR_GENERIC;
	}
	LocalFree(sd);
	if ((res = M_fs_perms_set_user_int(perms, M_fs_info_get_user(*info), user_sid)) != M_FS_ERROR_SUCCESS ||
		(res = M_fs_perms_set_group_int(perms, M_fs_info_get_group(*info), group_sid)) != M_FS_ERROR_SUCCESS)
	{
		M_fs_perms_destroy(perms);
		M_fs_info_destroy(*info);
		*info = NULL;
		return res;
	}
	M_fs_info_set_perms(*info, perms);

	return M_FS_ERROR_SUCCESS;
}

M_fs_error_t M_fs_info(M_fs_info_t **info, const char *path, M_uint32 flags)
{
	char            *norm_path;
	M_fs_error_t     res;
	WIN32_FIND_DATA  file_data;
	HANDLE           find;

	if (info != NULL) {
		*info = NULL;
	}

	if (path == NULL) {
		return M_FS_ERROR_INVALID;
	}

	/* Normalize the path. */
	res = M_fs_path_norm(&norm_path, path, M_FS_PATH_NORM_RESALL, M_FS_SYSTEM_AUTO);
	if (res != M_FS_ERROR_SUCCESS) {
		M_free(norm_path);
		return res;
	}

	/* Using FindFirstFile to get the specific file we want to get info about. The file_data will contain most
 	 * of the information we want to know about the file. This is more convenient than using multiple calls
	 * to get the same info. */
	find = FindFirstFile(norm_path, &file_data);
	if (find == INVALID_HANDLE_VALUE) {
		M_free(norm_path);
		return M_fs_error_from_syserr(GetLastError());
	}

	/* If info was sent in as NULL then the we are only checking that the path exists. */
	if (info == NULL) {
		FindClose(find);
		return M_FS_ERROR_SUCCESS;
	}

	res = M_fs_info_int(info, norm_path, flags, &file_data);

	FindClose(find);
	M_free(norm_path);

	return res;
}

M_fs_error_t M_fs_info_file(M_fs_info_t **info, M_fs_file_t *fd, M_uint32 flags)
{
	BY_HANDLE_FILE_INFORMATION file_data;

	if (info != NULL)
		*info = NULL;

	if (fd == NULL)
		return M_FS_ERROR_INVALID;

	if (info == NULL)
		return M_FS_ERROR_SUCCESS;

	if (!GetFileInformationByHandle(fd->fd, &file_data))
		return M_fs_error_from_syserr(GetLastError());

	return M_fs_info_file_int(info, fd, flags, &file_data);
}
