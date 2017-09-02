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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_fs_dir_entry
{
	char          *name;
	char          *resolved_name;
	M_fs_info_t *info;
	M_fs_type_t  type;
	M_bool         hidden;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Internal */

M_fs_dir_entry_t *M_fs_dir_entry_create(void)
{
	M_fs_dir_entry_t *entry;
	entry = M_malloc_zero(sizeof(*entry));
	return entry;
}

void M_fs_dir_entry_set_type(M_fs_dir_entry_t *entry, M_fs_type_t type)
{
	if (entry == NULL)
		return;
	entry->type = type;
}

void M_fs_dir_entry_set_hidden(M_fs_dir_entry_t *entry, M_bool hidden)
{
	if (entry == NULL)
		return;
	entry->hidden = hidden;
}

void M_fs_dir_entry_set_name(M_fs_dir_entry_t *entry, const char *name)
{
	if (entry == NULL)
		return;
	M_free(entry->name);
	entry->name = M_strdup(name);
}

void M_fs_dir_entry_set_resolved_name(M_fs_dir_entry_t *entry, const char *name)
{
	if (entry == NULL)
		return;
	M_free(entry->resolved_name);
	entry->resolved_name = M_strdup(name);
}

void M_fs_dir_entry_set_info(M_fs_dir_entry_t *entry, M_fs_info_t *info)
{
	if (entry == NULL)
		return;
	M_fs_info_destroy(entry->info);
	entry->info = info;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Public */

void M_fs_dir_entry_destroy(M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return;

	M_free(entry->name);
	entry->name = NULL;
	M_free(entry->resolved_name);
	entry->resolved_name = NULL;
	M_fs_info_destroy(entry->info);
	entry->info = NULL;
	entry->type = M_FS_TYPE_UNKNOWN;
	entry->hidden = M_FALSE;
	M_free(entry);
}

M_fs_type_t M_fs_dir_entry_get_type(const M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return M_FS_TYPE_UNKNOWN;
	return entry->type;
}

M_bool M_fs_dir_entry_get_ishidden(const M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return M_FALSE;
	return entry->hidden;
}

const char *M_fs_dir_entry_get_name(const M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return NULL;
	return entry->name;
}

const char *M_fs_dir_entry_get_resolved_name(const M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return NULL;
	return entry->resolved_name;
}

const M_fs_info_t *M_fs_dir_entry_get_info(const M_fs_dir_entry_t *entry)
{
	if (entry == NULL)
		return NULL;
	return entry->info;
}
