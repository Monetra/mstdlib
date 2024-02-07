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

#include "m_config.h"
#include <mstdlib/mstdlib.h>
#include <mstdlib/sql/m_module.h>
#include "base/m_defs_int.h"
#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#ifdef _WIN32
static M_bool M_module_win32_error(DWORD err, char *error, size_t err_len)
{
    LPSTR errString = NULL;
    if (!FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                         0,
                         err,
                         0,
                         (LPSTR)&errString,
                         0,
                         0 ))
        return M_FALSE;

    M_snprintf(error, err_len, "%s", errString);
    LocalFree(errString);

    return M_TRUE;
}
#endif


static M_module_handle_t M_module_load_int(const char *name, char *error, size_t error_size)
{
    M_module_handle_t handle;

#ifdef _WIN32
    handle = LoadLibraryA(name);
    if (handle == M_MODULE_INVALID_HANDLE) {
        M_module_win32_error(GetLastError(), error, error_size);
    }
#else
    handle = dlopen(name, RTLD_NOW|RTLD_LOCAL);
    if (handle == M_MODULE_INVALID_HANDLE) {
        /* Strip off module name prefix if there was one */
        const char *temp = dlerror();
        if (temp != NULL) {
            char prefix[256];
            M_snprintf(prefix, sizeof(prefix), "%s: ", name);

            if (M_str_caseeq_max(temp, prefix, M_str_len(prefix)))
                temp += M_str_len(prefix);
        }
        M_snprintf(error, error_size, "%s", temp);
    }
#endif

    return handle;
}


M_module_handle_t M_module_load(const char *module_name, char *error, size_t error_size)
{
#ifdef _WIN32
    const char *suffixes[] = { ".dll", "", NULL };
#elif defined(__APPLE__)
    const char *suffixes[] = { ".so", ".dylib", "", NULL };
#else
    const char *suffixes[] = { ".so", "", NULL };
#endif
    const char *prefixes[] = { "", "lib", NULL };

    size_t            i;
    size_t            j;
    char              name[1024];
    char              temp[256];
    M_module_handle_t handle = M_MODULE_INVALID_HANDLE;

    M_mem_set(error, 0, error_size);

    for (i=0; suffixes[i] != NULL; i++) {
        for (j=0; prefixes[j] != NULL; j++) {
            M_snprintf(name, sizeof(name), "%s%s%s", prefixes[j], module_name, suffixes[i]);
            *temp  = 0;
            handle = M_module_load_int(name, temp, sizeof(temp));
            if (handle != M_MODULE_INVALID_HANDLE) {
                M_mem_set(error, 0, error_size);
                return handle;
            }

            /* Lets save the *longest* error message.  We're, of course, assuming the
             * error message is the most relevant ... we hope.  For Linux, we also want
             * to exclude a known message that is long that may hide a more useful
             * error. */
#define HIDE_LINUX_ERROR "cannot open shared object file: No such file or directory"
            if (M_str_len(temp) > M_str_len(error) || M_str_caseeq(error, HIDE_LINUX_ERROR)) {
                /* Only ever write the linux error we want to hide if the current error
                 * string is empty */
                if (!M_str_caseeq(temp, HIDE_LINUX_ERROR) || M_str_isempty(error)) {
                    M_str_cpy(error, error_size, temp);
                }
            }
        }
    }

    return M_MODULE_INVALID_HANDLE;
}


void *M_module_symbol(M_module_handle_t handle, const char *symbol_name)
{
    const char *prefixes[] = { "", "_", "__", NULL };
    size_t      i;
    char        name[256];
    void       *sym = NULL;

    if (handle == NULL)
        return NULL;

    for (i=0; prefixes[i] != NULL && sym == NULL; i++) {
        M_snprintf(name, sizeof(name), "%s%s", prefixes[i], symbol_name);
#ifdef _WIN32
        sym = GetProcAddress(handle, name);
#else
        sym = dlsym(handle, name);
#endif
    }
    return sym;
}


void M_module_unload(M_module_handle_t handle)
{
    if (handle == NULL)
        return;
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}
