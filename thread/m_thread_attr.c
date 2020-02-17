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

#include <mstdlib/mstdlib_thread.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_thread_attr {
	M_bool  create_joinable;
	size_t  stack_size;
	M_uint8 priority;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_thread_attr_t *M_thread_attr_create(void)
{
	M_thread_attr_t *attr;
	attr = M_malloc_zero(sizeof(*attr));
	attr->priority = M_THREAD_PRIORITY_NORMAL; /* Defaults to normal */
	return attr;
}

void M_thread_attr_destroy(M_thread_attr_t *attr)
{
	if (attr == NULL)
		return;
	M_free(attr);
}

M_bool M_thread_attr_get_create_joinable(const M_thread_attr_t *attr)
{
	if (attr == NULL)
		return M_FALSE;
	return attr->create_joinable;
}

size_t M_thread_attr_get_stack_size(const M_thread_attr_t *attr)
{
	if (attr == NULL)
		return 0;
	return attr->stack_size;
}

M_uint8 M_thread_attr_get_priority(const M_thread_attr_t *attr)
{
	if (attr == NULL)
		return 0;
	return attr->priority;
}

void M_thread_attr_set_create_joinable(M_thread_attr_t *attr, M_bool val)
{
	if (attr == NULL)
		return;
	attr->create_joinable = val;
}

void M_thread_attr_set_stack_size(M_thread_attr_t *attr, size_t val)
{
	if (attr == NULL)
		return;
	attr->stack_size = val;
}

M_bool M_thread_attr_set_priority(M_thread_attr_t *attr, M_uint8 priority)
{
	if (attr == NULL)
		return M_FALSE;
	if (priority < M_THREAD_PRIORITY_MIN || priority > M_THREAD_PRIORITY_MAX)
		return M_FALSE;
	attr->priority = priority;
	return M_TRUE;
}
