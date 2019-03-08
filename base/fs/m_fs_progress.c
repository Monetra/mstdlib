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

struct M_fs_progress {
	char         *path;                  /*!< The path. */
	M_fs_type_t   type;                  /*!< File type. */
	M_fs_error_t  result;                /*!< Result. */
	/* M_FS_PROGRESS_COUNT */
	M_uint64      count_total;           /*!< The total number of entries we're processing. */
	M_uint64      count;                 /*!< The index of the total we are processing. */
	/* M_FS_PROGRESS_SIZE_TOTAL */
	M_uint64      size_total;            /*!< The total size of all entries. */
	M_uint64      size_total_progess;    /*!< The total size we have processed. */
	/* M_FS_PROGRESS_SIZE_CUR */
	M_uint64      size_current;          /*!< The size of the current entry we are processing. */
	M_uint64      size_current_progress; /*!< The number of bytes of the current entry we have processed. */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 * Public */

const char *M_fs_progress_get_path(const M_fs_progress_t *p)
{
	if (p == NULL)
		return NULL;
	return p->path;
}

M_fs_type_t M_fs_progress_get_type(const M_fs_progress_t *p)
{
	if (p == NULL)
		return M_FS_TYPE_UNKNOWN;
	return p->type;
}

M_fs_error_t M_fs_progress_get_result(const M_fs_progress_t *p)
{
	if (p == NULL)
		return M_FS_ERROR_INVALID;
	return p->result;
}

M_uint64 M_fs_progress_get_count_total(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->count_total;
}

M_uint64 M_fs_progress_get_count(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->count;
}

M_uint64 M_fs_progress_get_size_total(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->size_total;
}

M_uint64 M_fs_progress_get_size_total_progess(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->size_total_progess;
}

M_uint64 M_fs_progress_get_size_current(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->size_current;
}

M_uint64 M_fs_progress_get_size_current_progress(const M_fs_progress_t *p)
{
	if (p == NULL)
		return 0;
	return p->size_current_progress;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 * Internal */

M_fs_progress_t *M_fs_progress_create(void)
{
	M_fs_progress_t *p;
	p = M_malloc_zero(sizeof(*p));
	return p;
}

void M_fs_progress_destroy(M_fs_progress_t *p)
{
	if (p == NULL)
		return;

	M_free(p->path);
	p->path = NULL;

	M_free(p);
}

void M_fs_progress_clear(M_fs_progress_t *p)
{
	if (p == NULL)
		return;

	M_free(p->path);
	/* Reset everything. */
	M_mem_set(p, 0, sizeof(*p));
}

void M_fs_progress_set_path(M_fs_progress_t *p, const char *val)
{
	if (p == NULL)
		return;
	M_free(p->path);
	p->path = M_strdup(val);
}

void M_fs_progress_set_type(M_fs_progress_t *p, M_fs_type_t type)
{
	if (p == NULL)
		return;
	p->type = type;
}

void M_fs_progress_set_result(M_fs_progress_t *p, M_fs_error_t val)
{
	if (p == NULL)
		return;
	p->result = val;
}

void M_fs_progress_set_count_total(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->count_total = val;
}

void M_fs_progress_set_count(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->count = val;
}

void M_fs_progress_set_size_total(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->size_total = val;
}

void M_fs_progress_set_size_total_progess(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->size_total_progess = val;
}

void M_fs_progress_set_size_current(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->size_current = val;
}

void M_fs_progress_set_size_current_progress(M_fs_progress_t *p, M_uint64 val)
{
	if (p == NULL)
		return;
	p->size_current_progress = val;
}
