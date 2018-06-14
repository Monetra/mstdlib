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

#include "m_backtrace_int.h"

#include <windows.h>
#include <Dbghelp.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Backtrace references:
 *   - https://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
 *   - http://theorangeduck.com/page/printing-stack-trace-mingw 
 *   - https://www.bountysource.com/issues/36936327-bring-back-stack-tracing 
 */

static void win32_output_function(HANDLE mfile, size_t idx, DWORD64 frameOffset)
{
	/* name up to 256 chars This struct is of variable length though so
	 *  it must be declared as a raw byte buffer.
	 */
	static char        symbolBuffer[ sizeof(SYMBOL_INFO) + (256 * sizeof(TCHAR))];
	PSYMBOL_INFO       symbol = (PSYMBOL_INFO)symbolBuffer;
	IMAGEHLP_MODULE64  module;
	IMAGEHLP_LINE64    line;
	/* The displacement from the beginning of the symbol is stored here: pretty useless */
	DWORD64            displacement64 = 0;
	DWORD              displacement = 0;
	char               buf[1024];
	BOOL               gotModuleInfo;
	BOOL               gotSymbolInfo;
	BOOL               gotLineInfo;
	size_t             len;

	module.SizeOfStruct = sizeof(module);
	gotModuleInfo       = SymGetModuleInfo64(GetCurrentProcess(), frameOffset, &module);

	line.SizeOfStruct   = sizeof(line);
	gotLineInfo         = SymGetLineFromAddr64(GetCurrentProcess(), frameOffset, &displacement, &line);

	memset(symbolBuffer, 0, sizeof(symbolBuffer));

	/* Need to set the first two fields of this symbol before obtaining name info */
	symbol->SizeOfStruct  = sizeof(*symbol);
	symbol->MaxNameLen    = 255;

	/* Get the symbol information from the address of the instruction pointer register */
	gotSymbolInfo = SymFromAddr(
			GetCurrentProcess(),    /* Process to get symbol information for */
			frameOffset,            /* Address to get symbol for: instruction pointer register */
			&displacement64,        /* Displacement from the beginning of the symbol: whats this for ? */
			symbol                  /* Where to save the symbol */
			);

	len = M_snprintf(buf, sizeof(buf), "%zu - ", idx);

	if (gotModuleInfo && sizeof(buf)-len > 0) {
		len += M_snprintf(buf+len, sizeof(buf)-len, "%s!", module.ImageName);
	}

	if (sizeof(buf)-len > 0) {
		len += M_snprintf(buf+len, sizeof(buf)-len, "[0x%08llx]", (M_uint64)frameOffset);
	}

	if (gotSymbolInfo) {
		len += M_snprintf(buf+len, sizeof(buf)-len, " %s()", symbol->Name);
	}

	if (gotLineInfo && sizeof(buf)-len > 0) {
		len += M_snprintf(buf+len, sizeof(buf)-len, " at %s line %d", line.FileName, (int)line.LineNumber);
	}

	if (M_backtrace_flags & M_BACKTRACE_WRITE_FILE) {
		/* Write the data. */
		WriteFile(mfile, buf, len, NULL, NULL);

		/* Write new lines. */
		len = M_snprintf(buf, sizeof(buf), "\r\n");
		WriteFile(mfile, buf, len, NULL, NULL);

		/* Flush to force what we have to disk. */
		FlushFileBuffers(mfile);
	} else {
		/* Log to log function. */
		M_backtrace_cbs.crash_data((unsigned char *)buf, len);
	}
}

static void win32_output_stacktrace(HANDLE mfile, CONTEXT *context)
{
	STACKFRAME64 frame = { 0 };
	DWORD        MachineType;
	size_t       idx   = 0;

	SymInitialize(GetCurrentProcess(), 0, TRUE);

	/* setup initial stack frame */
#ifdef _M_IX86
	MachineType            = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset    = context->Eip;
	frame.AddrPC.Mode      = AddrModeFlat;
	frame.AddrFrame.Offset = context->Ebp;
	frame.AddrFrame.Mode   = AddrModeFlat;
	frame.AddrStack.Offset = context->Esp;
	frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_X64
	MachineType            = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset    = context->Rip;
	frame.AddrPC.Mode      = AddrModeFlat;
	frame.AddrFrame.Offset = context->Rsp;
	frame.AddrFrame.Mode   = AddrModeFlat;
	frame.AddrStack.Offset = context->Rsp;
	frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_IA64
	MachineType            = IMAGE_FILE_MACHINE_IA64;
	frame.AddrPC.Offset    = context->StIIP;
	frame.AddrPC.Mode      = AddrModeFlat;
	frame.AddrFrame.Offset = context->IntSp;
	frame.AddrFrame.Mode   = AddrModeFlat;
	frame.AddrBStore.Offset= context->RsBSP;
	frame.AddrBStore.Mode  = AddrModeFlat;
	frame.AddrStack.Offset = context->IntSp;
	frame.AddrStack.Mode   = AddrModeFlat;
#else
#error "Unsupported platform"
#endif

	while (StackWalk64(
				MachineType,
				GetCurrentProcess(),
				GetCurrentThread(),
				&frame,
				context,
				0,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				0
				)
		  ) {
		win32_output_function(mfile, idx++, frame.AddrPC.Offset);
		/* Prevent dumping too many due to recursion or bad stack */
		if (idx > 25)
			break;
	}
	SymCleanup(GetCurrentProcess());
}

static const char *win32_opdesc(const ULONG opcode)
{
	switch( opcode ) {
		case 0:
			return "read";
		case 1:
			return "write";
		case 8:
			return "DEP-violation";
		default: 
			break;
	}
	return "unknown";
}

static LONG WINAPI win32_exception_handler(EXCEPTION_POINTERS *ExceptionInfo)
{
	const char *error = NULL;
	char        fname[MAX_PATH];
	HANDLE      mfile = NULL;
	char        msg[512];
	size_t      len;

	switch(ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		case EXCEPTION_ACCESS_VIOLATION:
			error = "EXCEPTION_ACCESS_VIOLATION";
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			error = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
			break;
		case EXCEPTION_BREAKPOINT:
			error = "EXCEPTION_BREAKPOINT";
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			error = "EXCEPTION_DATATYPE_MISALIGNMENT";
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			error = "EXCEPTION_FLT_DENORMAL_OPERAND";
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			error = "EXCEPTION_FLT_DIVIDE_BY_ZERO";
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			error = "EXCEPTION_FLT_INEXACT_RESULT";
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			error = "EXCEPTION_FLT_INVALID_OPERATION";
			break;
		case EXCEPTION_FLT_OVERFLOW:
			error = "EXCEPTION_FLT_OVERFLOW";
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			error = "EXCEPTION_FLT_STACK_CHECK";
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			error = "EXCEPTION_FLT_UNDERFLOW";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			error = "EXCEPTION_ILLEGAL_INSTRUCTION";
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			error = "EXCEPTION_IN_PAGE_ERROR";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			error = "EXCEPTION_INT_DIVIDE_BY_ZERO";
			break;
		case EXCEPTION_INT_OVERFLOW:
			error = "EXCEPTION_INT_OVERFLOW";
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			error = "EXCEPTION_INVALID_DISPOSITION";
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			error = "EXCEPTION_NONCONTINUABLE_EXCEPTION";
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			error = "EXCEPTION_PRIV_INSTRUCTION";
			break;
		case EXCEPTION_SINGLE_STEP:
			error = "EXCEPTION_SINGLE_STEP";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			error = "EXCEPTION_STACK_OVERFLOW";
			break;
		default:
			error = "UNRECOGNIZED EXCEPTION";
			break;
	}

	len = M_snprintf(msg, sizeof(msg), "%s at address 0x%08llx", error, (M_uint64)ExceptionInfo->ExceptionRecord->ExceptionAddress);
	if ((ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
				ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
			sizeof(msg)-len > 0) {
		len += M_snprintf(msg+len, sizeof(msg)-len, " Invalid operation: %s at address 0x%08llx",
				win32_opdesc(ExceptionInfo->ExceptionRecord->ExceptionInformation[0]),
				(M_uint64)ExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
	}
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && sizeof(msg)-len > 0) {
		len += M_snprintf(msg-len, sizeof(msg)-len, " NTSTATUS code that resulted in the exception: %ld",  (long)ExceptionInfo->ExceptionRecord->ExceptionInformation[2]);
	}
	if (M_backtrace_cbs.log_emergency != NULL)
		M_backtrace_cbs.log_emergency(ExceptionInfo->ExceptionRecord->ExceptionCode, msg);

	if (M_backtrace_flags & M_BACKTRACE_WRITE_FILE) {
		M_backtrace_cbs.get_filename(fname, sizeof(fname));
		mfile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (mfile == INVALID_HANDLE_VALUE) {
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	/* If this is a stack overflow then we can't walk the stack, so just show
	   where the error happened */
	if (EXCEPTION_STACK_OVERFLOW != ExceptionInfo->ExceptionRecord->ExceptionCode) {
		win32_output_stacktrace(mfile, ExceptionInfo->ContextRecord);
	} else {
		win32_output_function(mfile, 0,
#ifdef _M_IX86
				ExceptionInfo->ContextRecord->Eip
#else
				ExceptionInfo->ContextRecord->Rip
#endif
				);
	}

	if (M_backtrace_flags & M_BACKTRACE_WRITE_FILE)
		CloseHandle(mfile);

	if (M_backtrace_cbs.got_crash != NULL)
		M_backtrace_cbs.got_crash(ExceptionInfo->ExceptionRecord->ExceptionCode);

	return EXCEPTION_EXECUTE_HANDLER;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static LONG WINAPI win32_make_minidump(EXCEPTION_POINTERS *e)
{
	char                           fname[MAX_PATH];
	HANDLE                         mfile;
	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	MINIDUMP_TYPE                  dtype;

	M_backtrace_cbs.get_filename(fname, sizeof(fname));
	if (M_str_isempty(fname))
		return EXCEPTION_EXECUTE_HANDLER;

	mfile = CreateFile(fname, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (mfile == INVALID_HANDLE_VALUE)
		return EXCEPTION_EXECUTE_HANDLER;

	exceptionInfo.ThreadId          = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = e;
	exceptionInfo.ClientPointers    = FALSE;

	if (M_backtrace_flags & M_BACKTRACE_EXTENDED_DUMP) {
		dtype = (MINIDUMP_TYPE)(MiniDumpWithDataSegs|MiniDumpWithFullMemory|MiniDumpWithHandleData|MiniDumpScanMemory|
				MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithProcessThreadData|MiniDumpWithPrivateReadWriteMemory);
	} else {
		dtype = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory|MiniDumpScanMemory|MiniDumpWithDataSegs);
	}

	MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			mfile,
			dtype,
			e ? &exceptionInfo : NULL,
			NULL,
			NULL);

	FlushFileBuffers(mfile);
	CloseHandle(mfile);

	if (M_backtrace_cbs.got_crash != NULL)
		M_backtrace_cbs.got_crash(e ? e->ExceptionRecord->ExceptionCode : -1);

	return EXCEPTION_CONTINUE_SEARCH;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_backtrace_setup_handling(M_backtrace_type_t type)
{
	switch (type) {
		case M_BACKTRACE_TYPE_BACKTRACE:
			SetUnhandledExceptionFilter(win32_exception_handler);
			break;
		case M_BACKTRACE_TYPE_DUMP:
			SetUnhandledExceptionFilter(win32_make_minidump);
			break;
	}

	return M_TRUE;
}

void M_backtrace_set_ignore_signal(int sig)
{
	(void)sig;
}

void M_backtrace_set_nonfatal_signal(int sig)
{
	(void)sig;
}

void M_backtrace_set_crash_signal(int sig)
{
	(void)sig;
}
