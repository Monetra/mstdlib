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

#ifdef HAVE_EXECINFO_H
#  include <execinfo.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#endif
#include <signal.h>
#include <unistd.h>

static void nonfatal_sighandler(int sig)
{
	if (M_backtrace_cbs.got_nonfatal != NULL)
		M_backtrace_cbs.got_nonfatal(sig);
}

static void ignore_sighandler(int sig)
{
	signal(sig, SIG_IGN);
}

static void crash_sighandler(int sig)
{
	const char *message;
#ifdef HAVE_EXECINFO_H
#  define BTSIZE 100
	void  *buffer[BTSIZE];
	int    nptrs;
	void **bufptr;
	char **lines;
	int    nbufptrs;
	int    fd     = -1;
	char   fname[255];
	int    i;

	/* Grab backtrace. */
	nptrs    = backtrace(buffer, BTSIZE);
	bufptr   = buffer;
	nbufptrs = nptrs;

	/* Remove the first entry off the stack, we don't need info on our own
	 * signal handler */
	if (nptrs > 1) {
		bufptr++;
		nbufptrs--;
	}

	if (M_backtrace_flags & M_BACKTRACE_WRITE_FILE) {
		/* Try to log to a file. */
		M_backtrace_cbs.get_filename(fname, sizeof(fname));
		if (!M_str_isempty(fname)) {
			fd = open(fname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
			if (fd != -1) {
				backtrace_symbols_fd(bufptr, nbufptrs, fd);
				close(fd);
			}
		}
	} else {
		/* Log to log function. */
		lines = backtrace_symbols(bufptr, nbufptrs);
		for (i=0; i<nbufptrs; i++) {
			M_backtrace_cbs.crash_data((unsigned char *)lines[i], M_str_len(lines[i]));
		}
		M_free(lines);
	}
#endif

	switch (sig) {
		case SIGSEGV:
			message = "SEGFAULT DETECTED, IMMEDIATE SHUTDOWN";
			break;
		case SIGILL:
			message = "Illegal Instruction caught";
			break;
		case SIGFPE:
			message = "Floating Point Exception caught";
			break;
		case SIGBUS:
			message = "Bus Error";
			break;
		default:
			message = "Unknown fatal error";
			break;
	}
	if (M_backtrace_cbs.log_emergency != NULL)
		M_backtrace_cbs.log_emergency(sig, message);

	signal(sig, SIG_IGN);

	if (M_backtrace_cbs.got_crash != NULL)
		M_backtrace_cbs.got_crash(sig);

	exit(1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_bool M_backtrace_setup_handling(M_backtrace_type_t type, const M_list_u64_t *nonfatal, const M_list_u64_t *ignore)
{
	struct sigaction act;
	int              sig;
	size_t           len;
	size_t           i;

	/* Only backtrace supported. */
	(void)type;

	/* Setup nonfatal. */
	act.sa_handler = nonfatal_sighandler;
	act.sa_flags   = 0;
	sigemptyset(&act.sa_mask);

	len = M_list_u64_len(nonfatal);
	for (i=0; i<len; i++) {
		sig = (int)M_list_u64_at(nonfatal, i);
		sigaction(sig, &act, NULL);
	}

	/* Setup ignore. */
	act.sa_handler = ignore_sighandler;
	act.sa_flags   = 0;
	sigemptyset(&act.sa_mask);

	len = M_list_u64_len(ignore);
	for (i=0; i<len; i++) {
		sig = (int)M_list_u64_at(ignore, i);
		sigaction(sig, &act, NULL);
	}

	/* Setup crash. */
	act.sa_handler = crash_sighandler;
	act.sa_flags   = 0;
	sigemptyset(&act.sa_mask);

	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGFPE, &act, NULL);

	return M_TRUE;
}
