#ifdef __M_CONFIG_H__
#  error m_config.h must not be included multiple times
#endif

#ifndef M_DLL
#  define M_DLL
#endif

/* note: to see builtins
 *   clang -x c /dev/null -dM -E
 *   gcc   -x c /dev/null -dM -E
 */

#define __M_CONFIG_H__

#if defined(_WIN32) && defined(NO_AUTOGEN_CONFIG_H)
#  include <config.w32.h>
#else
#  include <config.h>
#endif

/* We want to test handling NULL in our tests, so this is just noise.
 */
#if defined(TEST) && defined(__clang__)
#  pragma clang diagnostic ignored "-Wnonnull"
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compiler Output Control - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#  pragma  GCC diagnostic  warning "-Wwrite-strings"
#  pragma  GCC diagnostic  warning "-fipa-pure-const"
#  pragma  GCC diagnostic  warning "-Wsurprising"
#  pragma  GCC diagnostic  error   "-Wunused-result"
#endif

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 2
#  pragma  GCC diagnostic  ignored "-Wredundant-decls"
#  pragma  GCC diagnostic  warning "-Wswitch"
#endif

#define M_BLACKLIST_FUNC 1
