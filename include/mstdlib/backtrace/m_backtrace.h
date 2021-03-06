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

#ifndef __M_BACKTRACE_H__
#define __M_BACKTRACE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_list_u64.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_backtrace_trace Backtrace
 *  \ingroup m_backtrace
 *
 * Generates a back trace when certain signal are singled. This is primarily
 * used for capturing information about a crash.
 *
 * # Use
 *
 * Backtrace data can either be written directly to a file or passed to
 * a configured callback but not both. When writing to a file the
 * M_backtrace_filename_func callback is required to be implemented.
 * When capturing trace data, the M_backtrace_trace_data_func is required
 * to be implemented. All other callbacks are optional.
 *
 * Windows has the ability to generate backtrace as well as a mini dump file.
 * Minidump requires writing directly to a file.
 *
 * Other OS cannot use dump output and only support backtrace output.
 *
 * # Events
 *
 * There are three types of events:
 * - Fatal     - Generate backtrace and exit the application.
 * - Non-Fatal - Informational events that allow application to take an action.
 * - Ignored   - Events that are silenced and ignored.
 *
 * ## Windows
 * 
 * Only fatal events are supported on Windows.
 *
 * Crash events are any events captured by SetUnhandledExceptionFilter.
 *
 * Primary events (other could trigger):
 * - EXCEPTION_ACCESS_VIOLATION
 * - EXCEPTION_ARRAY_BOUNDS_EXCEEDED
 * - EXCEPTION_BREAKPOINT
 * - EXCEPTION_DATATYPE_MISALIGNMENT
 * - EXCEPTION_FLT_DENORMAL_OPERAND
 * - EXCEPTION_FLT_DIVIDE_BY_ZERO
 * - EXCEPTION_FLT_INEXACT_RESULT
 * - EXCEPTION_FLT_INVALID_OPERATION
 * - EXCEPTION_FLT_OVERFLOW
 * - EXCEPTION_FLT_STACK_CHECK
 * - EXCEPTION_FLT_UNDERFLOW
 * - EXCEPTION_ILLEGAL_INSTRUCTION
 * - EXCEPTION_IN_PAGE_ERROR
 * - EXCEPTION_INT_DIVIDE_BY_ZERO
 * - EXCEPTION_INT_OVERFLOW
 * - EXCEPTION_INVALID_DISPOSITION
 * - EXCEPTION_NONCONTINUABLE_EXCEPTION
 * - EXCEPTION_PRIV_INSTRUCTION
 * - EXCEPTION_SINGLE_STEP
 * - EXCEPTION_STACK_OVERFLOW
 *
 * ## Other OS
 *
 * Events are signals from singled.h.
 *
 * \note Signals are global and their signal handler can be changed outside of this library.
 *       A backtrace will not be generated if the signal handler is changed.
 *
 * Fatal signals (default captured):
 * - SIGPIPE
 * - SIGSEGV
 * - SIGBUS
 * - SIGILL
 * - SIGFPE
 *
 * Non-fatal signals (default captured):
 * - SIGINT
 * - SIGQUIT
 * - SIGTERM
 * - SIGXFSZ
 *
 * Ignored signals (default captured):
 * - SIGCHLD
 * - SIGUSR1
 * - SIGUSR2
 *
 * Fatal signals are always enabled. Non-fatal and ignore will only be set during setup
 * when the M_BACKTRACE_CAPTURE_NONCRASH flag is set. When not set only the default
 * fatal signals are set to be captured.
 *
 * Signals can be changed from one type to another and additional signals can be added
 * to a given type using the M_backtrace_set_* functions. For example, some applications
 * may want to treat SIGPIPE as non-fatal or ignore it altogether.
 *
 * # Platform notes
 *
 * ## macOS
 *
 * macOS will not return filename or line numbers. This is a limitation of
 * the OS. Additional tools need to be used to evaluate the trace data.
 * Given this trace data.
 *
 *     0   libsystem_platform.dylib            0x00007fff70c3bf5a _sigtramp + 26
 *     1   APP_NAME                            0x0000000101bc37af my_read + 95
 *     2   APP_NAME                            0x0000000101bff381 mm_listen_connection + 545
 *
 * lldb can be used to get the source file and line from the function and offset.
 *
 *     (lldb) so l -a my_read+95
 *
 * The trace may not show the exact line that caused the crash but only the
 * generate area where it occurred. For example, my_read+95 may point to line within
 * my_read where function my_crashing_function is called. The crash may be somewhere
 * within my_crashing_function.
 *
 * ## Windows
 *
 * Example trace data when PDB is distributed with application (not typical outside of development):
 *
 *     EXCEPTION_ACCESS_VIOLATION at address 0x7ff614eb5e4a Invalid operation: write at address 0x00000000
 *     0 - C:\Program Files\MY_APP\lib\mylib.dll + 21024 my_dt() at \cygdrive\c\build\my_file.c line 692
 *     1 - C:\Program Files\MY_APP\lib\mylib.dll + 4202 my_do_thing() at \cygdrive\c\build\my_file.c line 1180
 *     2 - C:\Program Files\MY_APP\lib\mylib.dll + 45 my_start() at \cygdrive\c\build\main.c line 316
 *
 * Example trace data when PDB is _NOT_ distributed with application (typical production deployment):
 *
 *     EXCEPTION_ACCESS_VIOLATION at address 0x7ff614eb5e4a Invalid operation: write at address 0x00000000
 *     0 - C:\Program Files\MY_APP\app.exe + 292671
 *     0 - C:\Program Files\MY_APP\app.exe + 286282
 *
 * We have experienced the function name in the output being incorrect while the
 * filename and line point to the correct location. For example my_dt() may not be correct and my_file.c line 692
 * is actually my_check_valid_dt. This is due to compiler optimizations such as inlining.
 *
 * The location of '+ #' is the offset from the module base address. When using a PDB file for debugging you
 * must get the base address from the debugger and add the offset to find the faulting address. PDB files use
 * a virtual address that does not correspond to the address within the binary. Hence, an offset from the
 * base address is provided by the backtrace.
 *
 * If using Mingw PDB files can be generated by building with debug symbols, running cv2pdb, then stripping
 * the binary.
 *
 * ### Using Visual Studio to Lookup Addresses
 *
 * Lookup by address is important when the application is not distributed withe PDB files. Which is
 * typical for pretty much every application. When this is the case the backtrace will only list modules
 * and offsets. It will not include file names or line numbers. Instead we need to look this up using
 * the module and offset within the module recorded in the backtrace.
 *
 * 1. Open Visual Studio
 * 2. Choose File -> Open -> Project/Solution
 * 3. Choose the exe (app.exe in above example). This will create a temporary/pseudo project to work in.
 * 4. Choose Debug -> Options
 * 5. Go to Debugging -> Symbols
 * 6. Add the location of the PDB files
 * 7. Right click the solution in the solution explorer and choose Properties
 * 8. Go to Common Properties -> Debug Source Files
 * 9. Add the location of the source files
 * 10. Choose Debug -> Step Into. This will start the debugger and break on main
 * 11. Chose Debug -> Windows -> Modules
 * 12. Find the entry for the application or DLL referenced in the backtrace you're interested in
 * 13. The Address column will include the start and end addresses for the module. Get the start address
 * 14. Add the offset to the start address for the entry in the backtrace you're interested in
 * 15. Choose Debug -> Windows -> Disassembly
 * 16. In the Address entry put in the address you've calculated using the start and offset
 * 17. The code will be inline with the disassemtly
 * 18. Right click on the line corresponding to the address and choose Go To Source
 *
 * You are now at the line captured in the backtrace.
 *
 * @{
 */


/*! Callback to get file name to write data to.
 *
 * If the filename cannot be opened or written to the data
 * will be lost. There is no recovery in this instance so it
 * is very important the location can be used. The file will
 * be created if it does not exist.
 *
 * \param[in] fname     Buffer to store the filename.
 * \param[in] fname_len Length of buffer.
 */
typedef void (*M_backtrace_filename_func)(char *fname, size_t fname_len);


/*! Callback to receive backtrace data.
 *
 * This can be called multiple times. For example, every line describing
 * the trace could call this function.
 *
 * Memory allocation or any function that could generate a signal should
 * not be used within this callback. If writing to a file, it should be
 * flushed after every write to reduce the risk of data loss.
 *
 * \param[in] data Data describing the backtrace.
 * \param[in] len  Length of data.
 */
typedef void (*M_backtrace_trace_data_func)(const unsigned char *data, size_t len);


/*! Callback to receive information about a backtrace event.
 *
 * Information about an event suitable for logging. This is general information
 * such as "segfault detected" and does not contain backtrace information.
 *
 * \param[in] sig     Signal or exception that generated this event.
 * \param[in] message Message.
 */
typedef void (*M_backtrace_log_emergency_func)(int sig, const char *message);


/*! Callback to handle non-fatal events.
 *
 * \note UNIX only.
 *
 * Non-fatal events do not generate a backtrace and are informational
 * so the receiver can take additional action. For example, capturing
 * the CTRL+C event to write to a log before exiting.
 *
 * \param[in] sig Signal or exception that generated this event.
 */
typedef void (*M_backtrace_got_nonfatal_func)(int sig);


/*! Callback to signal a fatal even occurred and the application will exit.
 *
 * This will be called after all trace_data and log_emergency calls.
 *
 * \param[in] sig Signal or exception that generated this event.
 */
typedef void (*M_backtrace_got_fatal_func)(int sig);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Type of backtrae that should be generated. */
typedef enum {
	M_BACKTRACE_TYPE_BACKTRACE = 0, /*!< Stack trace. */
	M_BACKTRACE_TYPE_DUMP,          /*!< Binary dump of trace data. Windows only and generates a minidump.
	                                     On all other OS will be treated as BACKTRACE. */
} M_backtrace_type_t;


/*! Flags controlling behavior. */
typedef enum {
	M_BACKTRACE_NONE             = 0,      /*!< Normal behavior. */
	M_BACKTRACE_WRITE_FILE       = 1 << 0, /*!< Write data to a file using the get_filename callback. */
	M_BACKTRACE_EXTENDED_DUMP    = 1 << 1, /*!< When using dump type capture additional information about the event.
	                                            This will produce a much larger dump file and can capture sensitive
	                                            data in memory. For example, encryption keys. */
	M_BACKTRACE_CAPTURE_NONCRASH = 1 << 2  /*!< Setup default non-fatal and ignore signal handling on non-Windows OS. */
} M_backtrace_flags_t;


/*! Callbacks. */
struct M_backtrace_callbacks {
	M_backtrace_filename_func      get_filename;  /*!< Get a filename when writing to file is enabled. Cannot be
	                                                   NULL if writing to a file. */
	M_backtrace_trace_data_func    trace_data;    /*!< Backtrace data. Cannot be NULL when not writing to a file. */
	M_backtrace_log_emergency_func log_emergency; /*!< Emergency log function with information about a crash event. */
	M_backtrace_got_nonfatal_func  got_nonfatal;  /*!< Non fatal event captured. */
	M_backtrace_got_fatal_func     got_fatal;     /*!< A fatal event occurred and the application is about to exit.
	                                                   Informational only to allow last logging. */
};


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Enable backtracing in the application.
 *
 * Cannot be called multiple times and cannot be disabled. Once enabled it is enabled.
 *
 * \param[in] type  Type of tracing that should happen.
 * \param[in] cbs   Callbacks to handle events.
 * \param[in] flags M_backtrace_flags_t Flags controlling behavior.
 */
M_API M_bool M_backtrace_enable(M_backtrace_type_t type, struct M_backtrace_callbacks *cbs, M_uint32 flags);


/*! Ignore the signal.
 *
 * This does _not_ set SIG_IGN. Instead it sets a no-op function as
 * the handler. If SIG_IGN is required it must be set using `signal(sig, SIG_IGN)`
 * directly.
 *
 * A no-op is used because of how SIG_IGN interacts with child
 * processes. Per POSIX.1-2001, wait/waitpid will return ECHILD
 * if SIGCHLD is set to SIG_IGN. If that's the case then children
 * may not become zombies and wait/waitpid will block until all
 * children have exited and fail with ECHILD. If using WNOHANG you'll
 * get ECHILD immediately instead of being informed the child has exited.
 *
 * \note Unix only. 
 *
 * \param[in] sig Signal.
 */
M_API void M_backtrace_set_ignore_signal(int sig);


/*! Consider the siginal as non-fatal
 *
 * Will call the M_backtrace_got_fatal_func callback. If the callback not set the signal will be ignored.
 *
 * \note Unix only. 
 *
 * \param[in] sig Signal.
 */
M_API void M_backtrace_set_nonfatal_signal(int sig);


/*! Consider the signal fatal.
 *
 * \note Unix only. 
 *
 * \param[in] sig Signal.
 */
M_API void M_backtrace_set_fatal_signal(int sig);


/*! Use the default sign handler.
 *
 * Default is what is set by SIG_DFL
 *
 * \note Unix only. 
 *
 * \param[in] sig Signal.
 */
M_API void M_backtrace_signal_use_default_handler(int sig);


/*! @} */

__END_DECLS

#endif /* __M_BACKTRACE_H__ */
