/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Monetra Technologies, LLC.
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

#include "m_backtrace_int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_backtrace_type_t           M_backtrace_type    = M_BACKTRACE_TYPE_BACKTRACE;
M_uint32                     M_backtrace_flags   = M_BACKTRACE_NONE;
struct M_backtrace_callbacks M_backtrace_cbs;

static M_bool                M_backtrace_enabled = M_FALSE;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_backtrace_enable(M_backtrace_type_t type, struct M_backtrace_callbacks *cbs, M_uint32 flags)
{
	M_bool ret;

	if (cbs == NULL)
		return M_FALSE;

	if (M_backtrace_enabled)
		return M_FALSE;

	/* Ensure we don't have some arbitrary value passed in as the type. */
	switch (type) {
		default:
		case M_BACKTRACE_TYPE_BACKTRACE:
			type = M_BACKTRACE_TYPE_BACKTRACE;
			break;
		case M_BACKTRACE_TYPE_DUMP:
			/* Only Windows supports dump right now. */
#ifdef _WIN32
			type = M_BACKTRACE_TYPE_DUMP;
#else
			type = M_BACKTRACE_TYPE_BACKTRACE;
#endif
			break;
	}

	/* Not writing a file requires the trace data callback.
 	 * Writing a file or dump (can only write to a file) requires filename callback. */
	if ((!(flags & M_BACKTRACE_WRITE_FILE) && cbs->trace_data == NULL) ||
			((type == M_BACKTRACE_TYPE_DUMP || flags & M_BACKTRACE_WRITE_FILE) && cbs->get_filename == NULL))
	{
		return M_FALSE;
	}

	/* Type. */
	M_backtrace_type  = type;

	/* Flags. */
	M_backtrace_flags = flags;

	/* Callbacks. */
	M_backtrace_cbs.get_filename  = cbs->get_filename;
	M_backtrace_cbs.trace_data    = cbs->trace_data;
	M_backtrace_cbs.log_emergency = cbs->log_emergency;
	M_backtrace_cbs.got_nonfatal  = cbs->got_nonfatal;
	M_backtrace_cbs.got_fatal     = cbs->got_fatal;

	ret = M_backtrace_setup_handling(type);
	if (ret) {
		/* Mark backtracing as enabled. */
		M_backtrace_enabled = M_TRUE;
	} else {
		M_backtrace_type    = M_BACKTRACE_TYPE_BACKTRACE;
		M_backtrace_flags   = M_BACKTRACE_NONE;
		M_mem_set(&M_backtrace_cbs, 0, sizeof(M_backtrace_cbs));
	}
	return ret;
}
