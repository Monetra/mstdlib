/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

#ifndef __M_BACKTRACE_H__
#define __M_BACKTRACE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_u64.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_backtrace_int Backtrace
 *  \ingroup m_backtrace
 *
 * @{
 */


typedef void (*M_backtrace_filanme_func)(char *fname, size_t fname_len);
typedef void (*M_backtrace_write_crash_data_func)(const unsigned char *data, size_t len);
typedef void (*M_backtrace_log_emergency_func)(int sig, const char *message);
typedef void (*M_backtrace_got_nonfatal_func)(int sig);
typedef void (*M_backtrace_got_crash_func)(int sig);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	M_BACKTRACE_TYPE_BACKTRACE = 0,
	M_BACKTRACE_TYPE_DUMP, /*!< Windows only. If set will use backtrace on other systems. */
} M_backtrace_type_t;


typedef enum {
	M_BACKTRACE_NONE = 0,
	M_BACKTRACE_WRITE_FILE    = 1 << 0,
	M_BACKTRACE_EXTENDED_DUMP = 1 << 1
} M_backtrace_flags_t;


struct M_backtrace_callbacks {
	M_backtrace_filanme_func          get_filename;
	M_backtrace_write_crash_data_func crash_data;
	M_backtrace_log_emergency_func    log_emergency;
	M_backtrace_got_nonfatal_func     got_nonfatal;
	M_backtrace_got_crash_func        got_crash;
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_API M_bool M_backtrace_enable(M_backtrace_type_t type, const M_list_u64_t *nonfatal, const M_list_u64_t *ignore, struct M_backtrace_callbacks *cbs, M_uint32 flags);

/*! @} */

__END_DECLS

#endif /* __M_BACKTRACE_H__ */
