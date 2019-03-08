/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_MODULE_H__
#define __M_MODULE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

#ifdef _WIN32
#include <windows.h> /* Needed for HMODULE below */
#endif
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS


/*! \addtogroup m_module Dynamic Module Loading Subsystem
 *  \ingroup m_sql
 *
 * Dynamic Module Loading Subsystem
 *
 * @{
 */

/*! Type for returned handle for M_module_load() */
#ifdef _WIN32
typedef HMODULE M_module_handle_t;
#else
typedef void * M_module_handle_t;
#endif

/*! Value for an invalid module handle */
#define M_MODULE_INVALID_HANDLE NULL

/*! Load a module
 * 
 *  The module should be a dynamic loadable module for the operating system.
 *  The subsystem will attempt to search for the module in multiple paths and
 *  multiple file name extensions.
 * 
 *  There is no need to supply a suffix like .so, .dll, or .dylib.
 *
 *  \param[in]  module_name module name to load, no suffix necessary.
 *  \param[out] error       user-supplied error message buffer
 *  \param[in]  error_size  size of user-supplied error message buffer
 *  \return M_MODULE_INVALID_HANDLE on failure, otherwise a valid handle.
 */
M_API M_module_handle_t M_module_load(const char *module_name, char *error, size_t error_size);


/*! Retrieve a pointer to a symbol in the module 
 * 
 * \param[in]  handle       Loaded module handle from M_module_load(), or M_MODULE_INVALID_HANDLE to attempt
 *                          to load a symbol from the current process.
 * \param[in]  symbol_name  Name of symbol to retrieve.
 * \return Pointer to symbol on success, NULL on failure.
 */  
M_API void *M_module_symbol(M_module_handle_t handle, const char *symbol_name);


/*! Unload a module loaded by M_module_load().
 *  \param[in]  handle       Loaded module handle from M_module_load()
 */
M_API void M_module_unload(M_module_handle_t handle);

/*! @} */

__END_DECLS

#endif /* __M_MODULE_H__ */
