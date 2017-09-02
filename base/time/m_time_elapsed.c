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
#include "platform/m_platform.h"
#include "time/m_time_int.h"

#include <time.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef __APPLE__
#  include <mach/mach_time.h>
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

M_int64 M_time_timeval_diff(const M_timeval_t *start_time, const M_timeval_t *end_time)
{
	M_int64 ret;

	ret  = (((M_int64)end_time->tv_sec) - ((M_int64)start_time->tv_sec)) * 1000;
	ret += ((M_int64)end_time->tv_usec/1000) - ((M_int64)start_time->tv_usec/1000);

	return ret;
}


#define WIN32_QPC 1

void M_time_elapsed_start(M_timeval_t *start_tv)
{
#if defined(_WIN32) && defined(WIN32_QPC)
	/* Use Windows Query Performance Counters */
	LARGE_INTEGER counter;
	LARGE_INTEGER freq;
	M_uint64      microsecs;

	/* NOTE: we really should cache 'freq' once as it will never change. */
	QueryPerformanceFrequency(&freq);

	QueryPerformanceCounter(&counter);

	/* NOTE: due to the high likelihood of an overflow if we used integer math,
	 *       we need to use long doubles (80bit) instead.  Hopefully we won't lose
	 *       enough precision to matter. */
	microsecs = (M_uint64)((long double)counter.QuadPart / ((long double)freq.QuadPart / 1000000.0));

	start_tv->tv_sec  = (M_time_t)(microsecs / 1000000);
	start_tv->tv_usec = (M_time_t)(microsecs % 1000000);
#elif defined(_WIN32)
	M_uint32 mtime    = timeGetTime();
	start_tv->tv_sec  = mtime / 1000;
	start_tv->tv_usec = (mtime % 1000) * 1000;
#elif defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
	struct timespec ts;

	/* clock_gettime can return -1 if the system doesn't actually support
	 * a monotonic clock even though the compiler flags say it does.  Fallback
	 * to the slower M_time_gettimeofday().
	 * NOTE: MacOSX/iOS needs a runtime check for clock_gettime() since it is a weak
	 *       symbol that was introduced in MacOS 10.12 and iOS10, we need to check for
	 *       NULL. */
	if (clock_gettime == NULL || clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		M_time_gettimeofday(start_tv);
		return;
	}

	/* Successful if here */
	start_tv->tv_sec  = ts.tv_sec;
	start_tv->tv_usec = ts.tv_nsec/1000;
#elif defined(__APPLE__)
	M_uint64                  start_abs;
	M_uint64                  start_conv;
	mach_timebase_info_data_t time_base;

	start_abs  = mach_absolute_time();
	mach_timebase_info(&time_base);
	start_conv = start_abs * time_base.numer / time_base.denom;

	start_tv->tv_sec  = (M_time_t)(start_conv/1000000000);
	start_tv->tv_usec = (M_suseconds_t)((start_conv%1000000000)/1000);
#else

#  if !defined(_WIN32) && !defined(__SCO_VERSION__)
#    warning PLATFORM DOES NOT SUPPORT MONOTONIC CLOCK. FALLING BACK TO GETTIMEOFDAY.
#  endif
	M_time_gettimeofday(start_tv);
#endif
}

M_uint64 M_time_elapsed(const M_timeval_t *start_tv)
{
	M_int64        ms       = 0;
#if defined(_WIN32) && !defined(WIN32_QPC)
	M_uint32       start_ms = 0;
	M_uint32       curr_ms  = 0;
	M_uint32       mag_a    = 0;
#else
	M_timeval_t curr_tv;
#endif

#if defined(_WIN32) && !defined(WIN32_QPC)
	curr_ms  = timeGetTime();
	start_ms = (M_uint32)((start_tv->tv_sec * 1000) + (start_tv->tv_usec / 1000));
	/* Check if wrapping or time went backwards
	 *
	 * timeGetTime returns an unsigned 32bit integer (DWORD). This means that
	 * the return value wraps around to 0 every 2^32 milliseconds which is
	 * about 49.71 days. We can run into a situation where the start and 
	 * elapsed times are in this situation.
	 *
	 * In both cases end will be smaller than start. The magnitude between
	 * start and end on both sides will determine if time wrapped or went backwards.
	 *
	 * Wrap:
	 * |-b--------a-|
	 * 0 to b + a to max will be smaller than the distance from b to a.
	 *
	 * Backward:
	 * |-----b-a----|
	 * 0 to b + a to max will be larger than the distance from b to a.
	 */
	if (curr_ms < start_ms) {
		mag_a = UINT_MAX - start_ms + curr_ms;
		if (mag_a < start_ms - curr_ms) {
			/* Wrap */
			ms = mag_a;
		} else {
			/* Backwards */
			ms = 0;
		}
	} else {
		ms = curr_ms - start_ms;
	}

	return ms;
#else
	/* We use the same logic to get time here as M_time_elapsed_start() so just
	 * use it instead */
	M_time_elapsed_start(&curr_tv);

	ms = M_time_timeval_diff(start_tv, &curr_tv);
	if (ms < 0) {
		ms = 0;
	}

	return (M_uint64)ms;
#endif
}
