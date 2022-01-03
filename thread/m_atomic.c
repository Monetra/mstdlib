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

#define ATOMIC_OP_GCC_BUILTIN 1  /* GCC builtin                      */
#define ATOMIC_OP_MSC_BUILTIN 2  /* MS Compiler builtin              */
#define ATOMIC_OP_SUN         3  /* SunOS/Solaris OS atomic function */
#define ATOMIC_OP_APPLE       4  /* Apple OS atomic function         */
#define ATOMIC_OP_AIX         5  /* AIX OS atomic function           */  
#define ATOMIC_OP_FREEBSD     6  /* FreeBSD OS atomic function       */
#define ATOMIC_OP_ASM         7  /* Inline ASM                       */
#define ATOMIC_OP_SPINLOCK    8  /* Emulation via spinlock           */
#define ATOMIC_OP_CAS32       9  /* Emulation via CAS32              */
#define ATOMIC_OP_CAS64       10 /* Emulation via CAS64              */
#define ATOMIC_OP_STDATOMIC   11 /* stdatomic from C11               */

/* Defines that will get created:
 * ATOMIC_CAS32 - Compare and Set atomic operation method
 * ATOMIC_CAS64 - Compare and Set atomic operation method
 * ATOMIC_INC32 - 32bit Integer increment operation method
 * ATOMIC_INC64 - 64bit Integer increment operation method
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if defined(_AIX) && defined(__GNUC__) && defined(_ARCH_PPC32)
#  define AIX_GCC_32 1
#endif

/* Though stdatomic exists on aix, and works for the xlc compiler, when using gcc,
 * it only works for 64bit and not 32bit */
#if defined(HAVE_STDATOMIC_H) && !defined(AIX_GCC_32)
/* Our use of stdatomic isn't totally proper, we basically do like
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4013.html
 * At some point, I guess we should introduce our own atomic type so we can
 * be more compliant. */
#  include <stdatomic.h>
#  define ATOMIC_CAS32 ATOMIC_OP_STDATOMIC
#  define ATOMIC_CAS64 ATOMIC_OP_STDATOMIC
#  define ATOMIC_INC32 ATOMIC_OP_STDATOMIC
#  define ATOMIC_INC64 ATOMIC_OP_STDATOMIC
#endif

#if defined(__sun__) && defined(HAVE_ATOMIC_H)
#  if !defined(ATOMIC_CAS32) || !defined(ATOMIC_CAS64) || !defined(ATOMIC_INC32) || !defined(ATOMIC_INC64)
#    include <atomic.h>
#  endif
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_SUN
#  endif
#  if !defined(ATOMIC_CAS64)
#    define ATOMIC_CAS64 ATOMIC_OP_SUN
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_SUN
#  endif
#  if !defined(ATOMIC_INC64)
#    define ATOMIC_INC64 ATOMIC_OP_SUN
#  endif
#endif

/* Use GCC's __sync built-in functions when they are known to exist and work.
 * This requires GCC 4.1.1+ for 32bit operations, and for 64bit, these are known to work:
 *  - Any amd64 OS
 *  - AIX PPC 64bit
 *  - Sun Solaris SPARC64
 * Exception: Solaris9 Sparc32 doesn't seem to actually work, prefer sun's atomic.h
 */
#if defined(__sun__) && defined(__sparc__) && !defined(__arch64__)
#  define SUN_SPARC32
#endif

#if (defined(__GNUC__) && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40101) && !defined(SUN_SPARC32)
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_GCC_BUILTIN
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_GCC_BUILTIN
#  endif
#  if defined(__amd64__) || \
      (defined(_AIX) && defined(_ARCH_PPC64)) || \
      (defined(__sun__) && defined(__sparc__) && defined(__arch64__))
#    if !defined(ATOMIC_CAS64)
#      define ATOMIC_CAS64 ATOMIC_OP_GCC_BUILTIN
#    endif
#    if !defined(ATOMIC_INC64)
#      define ATOMIC_INC64 ATOMIC_OP_GCC_BUILTIN
#    endif
#  endif
#endif

#if defined(_MSC_VER)
#  if !defined(ATOMIC_CAS32) || !defined(ATOMIC_CAS64) || !defined(ATOMIC_INC32) || !defined(ATOMIC_INC64)
#    include <intrin.h>
#    pragma intrinsic(_InterlockedCompareExchange)
#    pragma intrinsic(_InterlockedExchangeAdd)
#    ifdef _WIN64
#      pragma intrinsic(_InterlockedCompareExchange64)
#      pragma intrinsic(_InterlockedExchangeAdd64)
#    endif
#  endif
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_MSC_BUILTIN
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_MSC_BUILTIN
#  endif
#  if defined(_WIN64)
#    if !defined(ATOMIC_CAS64)
#      define ATOMIC_CAS64 ATOMIC_OP_MSC_BUILTIN
#    endif
#    if !defined(ATOMIC_INC64)
#      define ATOMIC_INC64 ATOMIC_OP_MSC_BUILTIN
#    endif
#  else
#    if !defined(ATOMIC_CAS64)
#      define ATOMIC_CAS64 ATOMIC_OP_ASM
#    endif
#  endif
#endif

#if defined(__SCO_VERSION__)
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_ASM
#  endif
#  if !defined(ATOMIC_CAS64)
#    define ATOMIC_CAS64 ATOMIC_OP_ASM
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_ASM
#  endif
#endif

#if defined(__FreeBSD__)
#  if !defined(ATOMIC_CAS32) || !defined(ATOMIC_CAS64) || !defined(ATOMIC_INC32) || !defined(ATOMIC_INC64)
#    include <sys/types.h>
#    include <machine/atomic.h>
#  endif
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_FREEBSD
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_FREEBSD
#  endif
#  if defined(__amd64__)
#    if !defined(ATOMIC_CAS64)
#      define ATOMIC_CAS64 ATOMIC_OP_FREEBSD
#    endif
#    if !defined(ATOMIC_INC64)
#      define ATOMIC_INC64 ATOMIC_OP_FREEBSD
#    endif
#  endif
#endif

#if defined(__APPLE__)
#  if !defined(ATOMIC_CAS32) || !defined(ATOMIC_CAS64) || !defined(ATOMIC_INC32) || !defined(ATOMIC_INC64)
#    include <libkern/OSAtomic.h>
#  endif
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_APPLE
#  endif
#  if !defined(ATOMIC_CAS64)
#    define ATOMIC_CAS64 ATOMIC_OP_APPLE
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_APPLE
#  endif
#  if !defined(ATOMIC_INC64)
#    define ATOMIC_INC64 ATOMIC_OP_APPLE
#  endif
#endif

#if defined(_AIX)
#  if !defined(ATOMIC_CAS32) || !defined(ATOMIC_CAS64) || !defined(ATOMIC_INC32) || !defined(ATOMIC_INC64)
#    include <sys/atomic_op.h>
#  endif
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_AIX
#  endif
#  if !defined(ATOMIC_CAS64) && defined(_ARCH_PPC64)
#    define ATOMIC_CAS64 ATOMIC_OP_AIX
#  endif
/* 
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_AIX
#  endif
#  if !defined(ATOMIC_INC64)
#    define ATOMIC_INC64 ATOMIC_OP_AIX
#  endif
*/
#endif


#if defined(__GNUC__) && defined(__i386__)
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_ASM
#  endif
#  if !defined(ATOMIC_CAS64)
#    define ATOMIC_CAS64 ATOMIC_OP_ASM
#  endif
#  if !defined(ATOMIC_INC32)
#    define ATOMIC_INC32 ATOMIC_OP_ASM
#  endif
#endif


#if defined(__GNUC__) && defined(__sparc__) && !defined(__arch64__)
#  if !defined(ATOMIC_CAS32)
#    define ATOMIC_CAS32 ATOMIC_OP_ASM
#  endif
#endif


#if !defined(ATOMIC_CAS32)
#  error unable to determine how to implement compare and set
#endif

/* Fallbacks */

#if !defined(ATOMIC_CAS64)
#  define ATOMIC_CAS64 ATOMIC_OP_SPINLOCK
#endif

#if !defined(ATOMIC_INC32)
#  define ATOMIC_INC32 ATOMIC_OP_CAS32
#endif

#if !defined(ATOMIC_INC64)
#  define ATOMIC_INC64 ATOMIC_OP_SPINLOCK
#endif


/* Spinlock helpers */
#if ATOMIC_CAS32 == ATOMIC_OP_SPINLOCK || ATOMIC_INC32 == ATOMIC_OP_SPINLOCK || ATOMIC_INC64 == ATOMIC_OP_SPINLOCK
static volatile M_uint32 M_atomic_lock = 0;
#  define M_atomic_spin_lock()   \
	while (!M_atomic_cas32(&M_atomic_lock, 0, 1)) \
		;
#  define M_atomic_spin_unlock() \
	while (!M_atomic_cas32(&M_atomic_lock, 1, 0)) \
		;
#endif


/* -------------------------------------------------------------------------------------
 * M_atomic_cas32
 * ------------------------------------------------------------------------------------- */


#if ATOMIC_CAS32 == ATOMIC_OP_ASM
#  if defined(__SCO_VERSION__) /* SCO 6 */

/* Adds and returns original value before value */
asm M_uint32 M_atomic_cas32_asm(volatile M_uint32 *dest, M_uint32 expected, M_uint32 newval)
{
/* dest, expected, newval are in memory or register */
%mem dest; mem expected; mem newval

/* Back up registers */
pushl %ebx
pushl %edx

/* cmpxchg compares EAX with first operand (destination), if two values */
/* are equal, second operand (source) is loaded into the destination operand. */

/* Copy the destination pointer to the edx register */
movl dest, %edx

/* Copy the expected value to the eax register */
movl expected, %eax

/* Copy new value to ebx register */
movl newval, %ebx

lock
cmpxchgl %ebx, (%edx)

/* restore registers */
popl %edx
popl %ebx

/* Zero flag should indicate success/fail and be returned, eax holds return value */
movl  $0,%eax
setz  %al
}

#  elif defined(__GNUC__) && defined(__i386__)

static __inline__ unsigned int M_atomic_cas32_asm(volatile M_uint32 *dest, M_uint32 expected, M_uint32 newval)
{
	unsigned char success;

	__asm__ __volatile__ (
			"lock\n"
			"cmpxchgl %3,%1\n"
			"sete %0\n"
			: "=q" (success),
			  "+m" (*dest),
			  "+a" (expected)
			: "r" (newval)
			: "memory", "cc");
    return success;
}

#  elif defined(__GNUC__) && defined(__sparc__)

static __inline__ unsigned int M_atomic_cas32_asm(volatile M_uint32 *dest, M_uint32 expected, M_uint32 newval)
{
	unsigned char success;

	__asm__ __volatile__ (
			"membar #StoreLoad | #LoadLoad\n"
			"cas [%1],%2,%3\n"
			"membar #StoreLoad | #LoadLoad\n"
			"cmp %2,%3\n"
			"be,a 1f\n"
			"mov 1,%0\n"
			"mov 0,%0\n"
			"1:"
			: "=r" (success)
			: "r" (dest), "r" (expected), "r" (newval));
	return success;
}

#  else
#    error unexpected ATOMIC_CAS32 value
#  endif
#endif


M_bool M_atomic_cas32(volatile M_uint32 *ptr, M_uint32 expected, M_uint32 newval)
{
#if ATOMIC_CAS32 == ATOMIC_OP_AIX
	int res;
	res = compare_and_swap((atomic_p)ptr, &expected, newval);
	if (res)
		__asm__("isync");
	return res?M_TRUE:M_FALSE;
#elif ATOMIC_CAS32 == ATOMIC_OP_MSC_BUILTIN
	if (_InterlockedCompareExchange((long *)ptr, newval, expected) != expected)
		return M_FALSE;
	return M_TRUE;
#elif ATOMIC_CAS32 == ATOMIC_OP_SUN
	if (atomic_cas_32(ptr, expected, newval) != expected)
		return M_FALSE;
	return M_TRUE;
#elif ATOMIC_CAS32 == ATOMIC_OP_APPLE
	return OSAtomicCompareAndSwap32Barrier((int32_t)expected, (int32_t)newval, (volatile int32_t *)ptr)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS32 == ATOMIC_OP_FREEBSD
	return atomic_cmpset_int(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS32 == ATOMIC_OP_GCC_BUILTIN
	return __sync_bool_compare_and_swap(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS32 == ATOMIC_OP_ASM
	return M_atomic_cas32_asm(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS32 == ATOMIC_OP_STDATOMIC
	return atomic_compare_exchange_strong_explicit((_Atomic M_uint32 *)ptr, &expected, newval, memory_order_relaxed, memory_order_relaxed)?M_TRUE:M_FALSE;
#else
#  error missing cas32 implementation
#endif
}

/* -------------------------------------------------------------------------------------
 * M_atomic_cas64
 * ------------------------------------------------------------------------------------- */
#if ATOMIC_CAS64 == ATOMIC_OP_ASM

#  if defined(__SCO_VERSION__) /* SCO 6 */

/* Need this wrapper as we cannot use leal to load the address of the variables
 * as there appears to be a bug in relation to the way that works with inline asm */
asm unsigned int M_atomic_cas64_asmref(volatile M_uint64 *dest, M_uint64 *compare, M_uint64 *exchange)
{
%mem dest; mem compare; mem exchange

/* back up registers */
pushal

/* current compare value is edx(high):eax(low), copy them */
movl compare, %edi
movl 4(%edi), %edx
movl (%edi), %eax

/* current values to exchange are ecx(high):ebx(low), copy them */
movl exchange, %edi
movl 4(%edi), %ecx
movl (%edi), %ebx

/* copy the destination to a temporary register, edi */
movl dest, %edi

/* Prevent concurrent access and execute the exchange and compare */
lock
cmpxchg8b (%edi)

/* Restore registers to previous state */
popal

/* Zero flag should indicate success/fail and be returned, eax holds return value */
movl  $0,%eax
setz  %al
}


static inline unsigned int M_atomic_cas64_asm(volatile M_uint64 *dest, M_uint64 compare, M_uint64 exchange)
{
	return M_atomic_cas64_asmref(dest, &compare, &exchange);
}

#  elif defined(__GNUC__) && defined(__i386__)

static __inline__ unsigned int M_atomic_cas64_asm(volatile M_uint64 *dest, M_uint64 compare, M_uint64 exchange)
{
	unsigned char success;
	__asm__ __volatile__(
	                     /* Backup ebx, needed for -fPIC */
	                     "pushl %%ebx\n\t"
	                     /* Copy the dest data - 64bit int (in edi) - to ecx:ebx */
	                     "movl (%3), %%ebx\n\t"
	                     "movl 4(%3), %%ecx\n\t"
	                     /* Compare edx:eax to value in edi */
	                     "lock; cmpxchg8b (%1)\n\t"
	                     /* Restore ebx */
	                     "popl %%ebx\n\t"
	                     /* Copy zero flag into output */
	                     "setz %0\n\t"

	                     /* output */
	                     /* %0 : =a says to store the contents of the zero flag into success on exit */
	                     : "=a" (success)

	                     /* input */
	                     /* %1: "D" says to copy the dest pointer into edi */
	                     : "D" (dest),
	                     /* %2: "A" says to copy the compare value to edx:eax */
	                     "A" (compare),
	                     /* %3: "S" says to use esi to store the pointer to the exchange value */
	                     "S" (&exchange)

	                     /* clobbered */
	                     : "flags", "memory", "%ecx"
	                    );
	return success;
}

#  elif defined(_MSC_VER) && !defined(_WIN64)

static __inline__ unsigned int M_atomic_cas64_asm(volatile M_uint64 *dest, M_uint64 compare, M_uint64 exchange)
{
	__asm {
		/* back up registers */
		pushad

		/* copy the destination to a temporary register, edi */
		mov edi, dest

		/* current compare value is edx(high):eax(low), copy them */
		lea esi, compare
		mov edx, 4[esi]
		mov eax, [esi]

		/* current values to exchange are ecx(high):ebx(low), copy them */
		lea esi, exchange
		mov ecx, 4[esi]
		mov ebx, [esi]

		/* Prevent concurrent access and execute the exchange and compare */
		lock cmpxchg8b [edi]

		/* Restore registers to previous state */
		popad

		/* Zero flag should indicate success/fail and be returned, eax holds return value */
		mov eax, 0
		setz al
	}
	/* Return with result in EAX */
}
#  else
#    error unexpected ATOMIC_CAS64 value
#  endif
#endif


M_bool M_atomic_cas64(volatile M_uint64 *ptr, M_uint64 expected, M_uint64 newval)
{
#if ATOMIC_CAS64 == ATOMIC_OP_AIX
	int res;
	res = compare_and_swaplp((atomic_l)ptr, &expected, newval);
	if (res)
		__asm__("isync");
	return res?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_MSC_BUILTIN
	if (_InterlockedCompareExchange64(ptr, newval, expected) != expected)
		return M_FALSE;
	return M_TRUE;
#elif ATOMIC_CAS64 == ATOMIC_OP_SUN
	if (atomic_cas_64(ptr, expected, newval) != expected)
		return M_FALSE;
	return M_TRUE;
#elif ATOMIC_CAS64 == ATOMIC_OP_APPLE
	return OSAtomicCompareAndSwap64Barrier((int64_t)expected, (int64_t)newval, (volatile int64_t *)ptr)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_FREEBSD
	return atomic_cmpset_64(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_GCC_BUILTIN
	return __sync_bool_compare_and_swap(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_ASM
	return M_atomic_cas64_asm(ptr, expected, newval)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_SPINLOCK
	M_uint64 val;
	M_atomic_spin_lock();
	val = *ptr;
	if (val == expected) {
		*ptr = newval;
	}
	M_atomic_spin_unlock();
	return (val == expected)?M_TRUE:M_FALSE;
#elif ATOMIC_CAS64 == ATOMIC_OP_STDATOMIC
	return atomic_compare_exchange_strong_explicit((_Atomic M_uint64 *)ptr, &expected, newval, memory_order_relaxed, memory_order_relaxed)?M_TRUE:M_FALSE;
#else
#  error missing cas64 implementation
#endif
}



/* -------------------------------------------------------------------------------------
 * M_atomic_add_u32
 * ------------------------------------------------------------------------------------- */

#if ATOMIC_INC32 == ATOMIC_OP_ASM
#  if defined(__GNUC__) && defined(__i386__)

static __inline__ M_uint32 M_atomic_add_u32_asm(volatile M_uint32 *dest, M_uint32 inc)
{
	M_uint32 temp = inc;
	__asm__ __volatile__("lock; xaddl %0,%1"
	                     /* output */
	                     : "+r" (temp),
	                       "+m" (*dest)
	                     /* input */
	                     :
	                     /* clobbered */
	                     : "memory");

	/* temp should contain the previous value on exit since
	 * xaddl will copy the original value into the source */
	return temp;
}

#  elif defined(__SCO_VERSION__) /* SCO 6 */

/* Adds and returns original value before value */
asm M_uint32 M_atomic_add_u32_asm(volatile M_uint32 *dest, M_uint32 inc)
{
/* dest and inc are both in memory or register */
%mem dest; mem inc

/* Back up registers used (ebx) */
pushl %ebx

/* Copy the destination pointer to the ebx register */
movl dest, %ebx

/* Copy the increment to the eax register */
movl inc, %eax

/* Prevent concurrent access and add. xaddl will copy  */
/* the original destination value back into the source */
/* as well so that it can be returned                  */
lock
xaddl %eax, (%ebx)

/* restore registers */
popl %ebx

/* eax will contain the result */
}

#  elif defined(_MSC_VER) && !defined(_WIN64)

/* Adds and returns original value before value */
static __inline__ M_uint32 M_atomic_add_u32_asm(volatile M_uint32 *dest, M_uint32 incr)
{
	__asm {
		/* Back up registers used (ebx) */
		push ebx

		/* Copy the destination pointer to the ebx register */
		mov ebx, dest

		/* Copy the increment to the eax register */
		mov eax, incr

		/* Prevent concurrent access and add. xaddl will copy  */
		/* the original destination value back into the source */
		/* as well so that it can be returned  */
		lock xadd [ebx], eax

		/* restore registers */
		pop ebx
	}
	/* Return with result in EAX */
}

#  else
#    error undefined atomic inc32 asm
#  endif
#endif


M_uint32 M_atomic_add_u32(volatile M_uint32 *ptr, M_uint32 val)
{
#if ATOMIC_INC32 == ATOMIC_OP_GCC_BUILTIN
	return __sync_fetch_and_add(ptr, val);
#elif ATOMIC_INC32 == ATOMIC_OP_MSC_BUILTIN
	return _InterlockedExchangeAdd(ptr, val);
#elif ATOMIC_INC32 == ATOMIC_OP_APPLE
	return (M_uint32)OSAtomicAdd32Barrier(val, (M_int32 *)ptr) - val;
#elif ATOMIC_INC32 == ATOMIC_OP_FREEBSD
	return atomic_fetchadd_32(ptr, val);
#elif ATOMIC_INC32 == ATOMIC_OP_SUN
	return atomic_add_32_nv(ptr, val) - val;
#elif ATOMIC_INC32 == ATOMIC_OP_ASM
	return M_atomic_add_u32_asm(ptr, val);
#elif ATOMIC_INC32 == ATOMIC_OP_CAS32
	M_uint32 compare;
	M_uint32 exchange;
	do {
		compare  = *ptr;
		exchange = compare + val;
	} while (!M_atomic_cas32(ptr, compare, exchange));

	return compare;
#elif ATOMIC_INC32 == ATOMIC_OP_SPINLOCK
	M_uint32 oldval;
	
	M_atomic_spin_lock();

	oldval = *ptr;
	*ptr += val;

	M_atomic_spin_unlock();

	return oldval;
#elif ATOMIC_INC32 == ATOMIC_OP_STDATOMIC
	return atomic_fetch_add_explicit((_Atomic M_uint32 *)ptr, val, memory_order_relaxed);
#else
#  error unhandled M_atomic_add_u32
#endif
}


/* -------------------------------------------------------------------------------------
 * M_atomic_add_u64
 * ------------------------------------------------------------------------------------- */

#if ATOMIC_INC64 == ATOMIC_OP_ASM
#  error no ATOMIC_INC64 ASM implementation defined 
#endif


M_uint64 M_atomic_add_u64(volatile M_uint64 *ptr, M_uint64 val)
{
#if ATOMIC_INC64 == ATOMIC_OP_GCC_BUILTIN
	return __sync_fetch_and_add(ptr, val);
#elif ATOMIC_INC64 == ATOMIC_OP_MSC_BUILTIN
	return _InterlockedExchangeAdd64(ptr, val);
#elif ATOMIC_INC64 == ATOMIC_OP_APPLE
	return (M_uint64)OSAtomicAdd64Barrier(val, (M_int64 *)ptr) - val;
#elif ATOMIC_INC64 == ATOMIC_OP_FREEBSD
	return atomic_fetchadd_64(ptr, val);
#elif ATOMIC_INC64 == ATOMIC_OP_SUN
	return atomic_add_64_nv(ptr, val) - val;
#elif ATOMIC_INC64 == ATOMIC_OP_ASM
	return M_atomic_add_u64_asm(ptr, val);
#elif ATOMIC_INC64 == ATOMIC_OP_CAS64
	M_uint64 compare;
	M_uint64 exchange;
	do {
		compare  = *ptr;
		exchange = compare + val;
	} while (!M_atomic_cas64(ptr, compare, exchange));

	return compare;
#elif ATOMIC_INC64 == ATOMIC_OP_SPINLOCK
	M_uint64 oldval;
	
	M_atomic_spin_lock();

	oldval = *ptr;
	*ptr  += val;

	M_atomic_spin_unlock();

	return oldval;
#elif ATOMIC_INC64 == ATOMIC_OP_STDATOMIC
	return atomic_fetch_add_explicit((_Atomic M_uint64 *)ptr, val, memory_order_relaxed);
#else
#  error unhandled M_atomic_add_u64
#endif
}


/* -------------------------------------------------------------------------------------
 * M_atomic_inc_u32
 * ------------------------------------------------------------------------------------- */

M_uint32 M_atomic_inc_u32(volatile M_uint32 *ptr)
{
	return M_atomic_add_u32(ptr, 1);
}

/* -------------------------------------------------------------------------------------
 * M_atomic_inc_u64
 * ------------------------------------------------------------------------------------- */

M_uint64 M_atomic_inc_u64(volatile M_uint64 *ptr)
{
	return M_atomic_add_u64(ptr, 1);
}

/* -------------------------------------------------------------------------------------
 * M_atomic_sub_u32
 * ------------------------------------------------------------------------------------- */

M_uint32 M_atomic_sub_u32(volatile M_uint32 *ptr, M_uint32 val)
{
#if ATOMIC_INC32 == ATOMIC_OP_GCC_BUILTIN
	return __sync_fetch_and_sub(ptr, val);
#elif ATOMIC_INC32 == ATOMIC_OP_STDATOMIC
	return atomic_fetch_sub_explicit((_Atomic M_uint32 *)ptr, val, memory_order_relaxed);
#else
	/* No other implemention provides an explicit subtraction */
	return M_atomic_add_u32(ptr, (M_uint32)((M_int32)val * -1));
#endif
}

/* -------------------------------------------------------------------------------------
 * M_atomic_sub_u64
 * ------------------------------------------------------------------------------------- */
M_uint64 M_atomic_sub_u64(volatile M_uint64 *ptr, M_uint64 val)
{
#if ATOMIC_INC64 == ATOMIC_OP_GCC_BUILTIN
	return __sync_fetch_and_sub(ptr, val);
#elif ATOMIC_INC64 == ATOMIC_OP_STDATOMIC
	return atomic_fetch_sub_explicit((_Atomic M_uint64 *)ptr, val, memory_order_relaxed);
#else
	/* No other implemention provides an explicit subtraction */
	return M_atomic_add_u64(ptr, (M_uint64)((M_int64)val * -1));
#endif
}

/* -------------------------------------------------------------------------------------
 * M_atomic_dec_u32
 * ------------------------------------------------------------------------------------- */

M_uint32 M_atomic_dec_u32(volatile M_uint32 *ptr)
{
	return M_atomic_sub_u32(ptr, 1);
}

/* -------------------------------------------------------------------------------------
 * M_atomic_dec_u64
 * ------------------------------------------------------------------------------------- */
M_uint64 M_atomic_dec_u64(volatile M_uint64 *ptr)
{
	return M_atomic_sub_u64(ptr, 1);
}
