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

#ifndef __M_DEFS_H__
#define __M_DEFS_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Required headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> /* __VA_ARGS__ */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef __FUNCTION__
#  define __FUNCTION__                         __func__
#endif

#ifdef __GNUC__
#  define GCC_VERSION                          (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#  ifndef __inline__
#    define __inline__                         __inline
#  endif
#endif


#if defined(__clang__)
#  ifndef __has_attribute
#    define  __has_attribute(x)                0
#  endif
#  ifndef __has_feature
#    define  __has_feature(x)                  0
#  endif
#  ifndef __has_extension
#    define  __has_extension                   __has_feature
#  endif
#  define    M_COMPILER_SUPPORTS(ATTR, VER)    (__has_attribute(ATTR) || __has_extension(ATTR))
#elif defined(__GNUC__)
#  define    M_COMPILER_SUPPORTS(ATTR, VER)    (GCC_VERSION >= VER)
#else
#  define    M_COMPILER_SUPPORTS(ATTR, VER)    0
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Prototype Wrapping- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#undef __BEGIN_DECLS
#undef __END_DECLS
#if defined(__cplusplus)
#  define    __BEGIN_DECLS                     extern "C" {
#  define    __END_DECLS                       }
#else
#  define    __BEGIN_DECLS                     /* empty */
#  define    __END_DECLS                       /* empty */
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Function Keywords - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Dynamic shared object and dynamic-link library support */
#ifdef _WIN32 /* Both MinGW and Visual Studio support declspec. */
#  define    M_DLL_IMPORT                      __declspec(dllimport)
#  define    M_DLL_EXPORT                      __declspec(dllexport)
#elif M_COMPILER_SUPPORTS(visibility, 40000)
#  define    M_DLL_IMPORT                      __attribute__((visibility ("default")))
#  define    M_DLL_EXPORT                      __attribute__((visibility ("default")))
#else
#  define    M_DLL_IMPORT                      /* empty */
#  define    M_DLL_EXPORT                      /* empty */
#endif

#ifdef M_API
#  undef M_API
#endif
#if   defined(MSTDLIB_STATIC)
#  define    M_API
#elif defined(M_DLL)
#  define    M_API                             M_DLL_EXPORT
#else
#  define    M_API                             M_DLL_IMPORT
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Attributes  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if M_COMPILER_SUPPORTS(malloc, 0)
/*! Tell compiler that the function may be treated as if any non-NULL pointer
 * returned cannot alias any other pointer valid when function returns and the
 * memory contents are undefined. */
#  define    M_MALLOC                          __attribute__((malloc))
#  define    M_MALLOC_ALIASED
#else
#  define    M_MALLOC
#  define    M_MALLOC_ALIASED
#endif

#define      M_FREE(arg)

#if M_COMPILER_SUPPORTS(__deprecated__, 0)
   /*! Warn about the usage of a function. */
#  define    M_DEPRECATED(proto)               proto __attribute__((__deprecated__))
#else
#  define    M_DEPRECATED(proto)               proto
#endif

#if M_COMPILER_SUPPORTS(__format__, 0) && !defined(_WIN32)
   /*! Warn about format strings that are inconsistent with given arguments */
#  define    M_PRINTF(fmt_idx, arg_idx)        __attribute__((__format__(__printf__,fmt_idx,arg_idx)))
#else
#  define    M_PRINTF(fmtarg, firstvararg)     /* empty */
#endif


/* Warn about caller ignoring the result of this function.
 */
#if M_COMPILER_SUPPORTS(warn_unused_result, 30400)
#  define    M_WARN_UNUSED_RESULT              __attribute__((warn_unused_result))
#else
#  define    M_WARN_UNUSED_RESULT
#endif

/* Warn about function arguments that are passed as NULL.
 *
 * This must not be set when building the library itself because GCC will
 * optimize out NULL checks that are in place to prevent crashes due to misuse.
 */
#if !defined(MSTDLIB_INTERNAL) && M_COMPILER_SUPPORTS(nonnull, 30400)
#  define    M_WARN_NONNULL(arg)               __attribute__((nonnull(arg)))
#else
/* Other compilers might not support VA_ARGS */
#  define    M_WARN_NONNULL(arg)
#endif

/*! Tell compiler that the function return value points to memory where the
 * size is given by one or two of the functions parameters.
 */
#if M_COMPILER_SUPPORTS(alloc_size, 40300)
#  define    M_ALLOC_SIZE(size)                __attribute__((alloc_size(size)))
#else
#  define    M_ALLOC_SIZE(size)
#endif

/*! When debugging, produce a unit for an inline.
 */
#if defined(__GNUC__) && !defined(M_BLACKLIST_FUNC)
#  define    M_BLACKLIST_FUNC                  1
#endif

/*! Warn about the usage of a function with a message.
 *
 * Suppress deprecation warnings on Windows, and in C++ code that's using Qt.
 *
 * Clang only allows you to add the deprecation attribute when you initially declare the function, so you
 * can't use it to mark functions from other libraries as deprecated. So, we can't use the deprecated
 * attribute with clang.
 */
#if defined(__GNUC__) && !defined(__clang__) && (GCC_VERSION >= 40500) && !defined(QT_VERSION) && !defined(_WIN32)
#  define    M_DEPRECATED_MSG(msg,proto)       proto __attribute__((deprecated(msg)));
#else
#  define    M_DEPRECATED_MSG(msg,proto)
#endif

/*! Warn about the usage of a function and recommend an alternative.
 */
#define      M_DEPRECATED_FOR(f,proto)         M_DEPRECATED_MSG("Use " #f " instead", proto)

#if defined(__clang__)
#  define    M_BEGIN_IGNORE_DEPRECATIONS       _Pragma ("clang diagnostic push") \
                                               _Pragma ("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#  define    M_END_IGNORE_DEPRECATIONS         _Pragma ("clang diagnostic pop")
#  define    M_BEGIN_IGNORE_REDECLARATIONS     _Pragma ("clang diagnostic push") \
                                               _Pragma ("clang diagnostic ignored \"-Wredundant-decls\"")
#  define    M_END_IGNORE_REDECLARATIONS       _Pragma ("clang diagnostic pop")
#elif defined(__GNUC__) && GCC_VERSION >= 40600
#  define    M_BEGIN_IGNORE_DEPRECATIONS       _Pragma ("GCC diagnostic push") \
                                               _Pragma ("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define    M_END_IGNORE_DEPRECATIONS         _Pragma ("GCC diagnostic pop")
#  define    M_BEGIN_IGNORE_REDECLARATIONS     _Pragma ("GCC diagnostic push") \
                                               _Pragma ("GCC diagnostic ignored \"-Wredundant-decls\"")
#  define    M_END_IGNORE_REDECLARATIONS       _Pragma ("GCC diagnostic pop")
#else
#  define    M_BEGIN_IGNORE_DEPRECATIONS
#  define    M_END_IGNORE_DEPRECATIONS
#  define    M_BEGIN_IGNORE_REDECLARATIONS
#  define    M_END_IGNORE_REDECLARATIONS
#  undef     m_blacklist_func
#  define    m_blacklist_func                  0
#  define    m_begin_ignore_redeclarations
#  define    m_end_ignore_redeclarations
#endif


#define M_WARNING_STR(s) #s
#define M_WARNING_JOINSTR(x,y) M_WARNING_STR(x ## y)
#ifdef _MSC_VER
#  define M_WARNING_PRAGMA(x) __pragma (warning(x))
#elif defined(__clang__)
#  define M_WARNING_PRAGMA(x) _Pragma (M_WARNING_STR(clang diagnostic x))
#elif defined(__GNUC__)
#  define M_WARNING_PRAGMA(x) _Pragma (M_WARNING_STR(GCC diagnostic x))
#else
#  define M_WARNING_PRAGMA(x)
#endif
#define M_WARNING_CANCEL() M_WARNING_PRAGMA(pop)
#if defined(__clang__)
#  define M_WARNING_DISABLE(gcc_unused,clang_option,msvc_unused)    M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(ignored M_WARNING_JOINSTR(-W,clang_option))
#  define M_WARNING_ENABLE(gcc_unused,clang_option,msvc_unused)     M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(M_WARNING_JOINSTR(-W,clang_option))
#elif defined(_MSC_VER)
#  define M_WARNING_DISABLE(gcc_unused,clang_unused,msvc_errorcode) M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(disable: msvc_errorcode)
#  define M_WARNING_ENABLE(gcc_unused,clang_unused,msvc_errorcode)  M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(msvc_errorcode)
#elif defined(__GNUC__)
#  define M_WARNING_DISABLE(gcc_option,clang_unused,msvc_unused)    M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(ignored M_WARNING_JOINSTR(-W,gcc_option))
#  define M_WARNING_ENABLE(gcc_option,clang_unused,msvc_unused)     M_WARNING_PRAGMA(push) M_WARNING_PRAGMA(warning M_WARNING_JOINSTR(-W,gcc_option))
#else
#  define M_WARNING_DISABLE(gcc_option,clang_unused,msvc_unsued)
#  define M_WARNING_ENABLE(gcc_option,clang_unused,msvc_unsued)
#endif


/*! Used to safely cast off a 'const', used only when it is known
 *  to be okay to do so */
#define M_CAST_OFF_CONST(type, var) ((type)((M_uintptr)var))


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_DEFS_H__ */
