#define STDC_HEADERS

#define HAVE_IO_H
#define HAVE_STDLIB_H
#define HAVE_STRING_H
#define HAVE_SYS_STAT_H
#define HAVE_WINDOWS_H
#define HAVE_ERRNO_H

#ifndef _WIN32_WCE
#  define HAVE_SYS_TYPES_H
#endif

/* Don't complain about sprintf usage */
#ifndef _CRT_SECURE_NO_DEPRECATE
#  ifdef NDEBUG
#    define _CRT_SECURE_NO_DEPRECATE
#  endif
#endif

/* move away from this kind of thing */
#ifndef _WIN32
#  define _WIN32
#endif

#define HAVE__STRICMP
#define HAVE__STRNICMP
#define HAVE_SOCKADDR_STORAGE

/* Note: As of Visual Studio 2013 (v12)
 * va_copy is supported. Change to HAVE_VA_COPY */
#define VA_LIST_IS_ARRAY_TYPE                                                                                                                                                                             
