# - Check for various system extensions.
# Once done this will set the following if found:
#  _GNU_SOURCE              - Extensions for GNU/Linux.
#  _MINIX                   - Identify Minix platform.
#  _POSIX_1_SOURCE          - Additional Posix functions for Minix.
#  _POSIX_SOURCE            - Posix founctions for Minix.
#  __USE_MINGW_ANSI_STDIO   - MinGW
# Once done set the following CFLAGS if found:
#  _XOPEN_SOURCE=500        - Enables some additional functions.
#  _REENTRANT               - Needed on UnixWare for gethostbyname_r.

if (_internal_system_extensions_already_run)
	return()
endif ()
set(_internal_system_extensions_already_run TRUE)

if (    CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(_new_flags "-D_XOPEN_SOURCE=500 -D_DARWIN_C_SOURCE")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(_new_flags "-D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=600")
elseif (CMAKE_SYSTEM_NAME MATCHES "Android")
	set(_new_flags "-D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=600")
elseif (CMAKE_SYSTEM_NAME MATCHES "UnixWare")
	set(_new_flags "-D_REENTRANT")
elseif (MINGW)
	set(_new_flags "-D__USE_MINGW_ANSI_STDIO")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_new_flags}")
