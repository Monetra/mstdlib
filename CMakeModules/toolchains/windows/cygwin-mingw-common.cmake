# Common settings that are shared between the 32-bit and 64-bit cygwin-mingw toolchains.
#
# Note: CMAKE_TOOLCHAIN_PREFIX must be set before including this file.

# Target platform:
set(CMAKE_SYSTEM_NAME    Windows)
set(CMAKE_SYSTEM_VERSION 5.1) # Windows XP SP3

# Set paths to toolchain executables.
set(CMAKE_C_COMPILER   ${CMAKE_TOOLCHAIN_PREFIX}-gcc     CACHE FILEPATH "Toolchain C compiler")
set(CMAKE_CXX_COMPILER ${CMAKE_TOOLCHAIN_PREFIX}-g++     CACHE FILEPATH "Toolchain C++ compiler")
set(CMAKE_RC_COMPILER  ${CMAKE_TOOLCHAIN_PREFIX}-windres CACHE FILEPATH "Toolchain resource compiler")
set(CMAKE_ASM_COMPILER ${CMAKE_TOOLCHAIN_PREFIX}-as      CACHE FILEPATH "Toolchain assembler")
set(CMAKE_AR           ${CMAKE_TOOLCHAIN_PREFIX}-ar      CACHE FILEPATH "Toolchain static archiver")

# Set paths to force find_package() to find target system libs instead of host system ones.
set(CMAKE_SYSROOT                     /usr/${CMAKE_TOOLCHAIN_PREFIX}/sys-root)
set(CMAKE_FIND_ROOT_PATH              ${CMAKE_SYSROOT}/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH) # For exe's, search for target system ones first, then host system.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY) # Only search for libraries that are for the target system.
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY) # Only search for includes that are for the target system.

# Need to set CMAKE_CROSSCOMPILING to FALSE, so that CMake will let us use built targets in custom build
# rules. This isn't truly cross-compiling, only makes sense for Cygwin.
set(CMAKE_CROSSCOMPILING FALSE)
list(APPEND CMAKE_PREFIX_PATH
	"${CMAKE_FIND_ROOT_PATH}/usr/local/ssl"
	"${CMAKE_FIND_ROOT_PATH}"
	/usr/${CMAKE_TOOLCHAIN_PREFIX} # Workaround for find_package bug, only needed when CMAKE_CROSSCOMPILING is false.
)

# Fix for bug where compiler complains that it can't find stdlib.h while compiling Qt 5 programs.
# See the following:
#    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70129
#    https://bugs.webkit.org/show_bug.cgi?id=161697#c8
set(CMAKE_NO_SYSTEM_FROM_IMPORTED TRUE)

# NOTE: if we ever change CMAKE_CROSSCOMPILING back to true, we'll also need to uncomment the code below.
#
# Since we're cross-compiling, we can't actually run anything on the target machine. Thus, we have to hardcode
# any settings that are detected by running code (try_run()) during configure.
#set(HAVE_LARGE_FILE_SUPPORT_NATIVE            TRUE)  # See LargeFiles.cmake. Note that mingw-w64 supports large
#set(HAVE_LARGE_FILE_SUPPORT_FILE_OFFSET_BITS  FALSE) # files natively for both 32-bit and 64-bit targets.
#set(HAVE_LARGE_FILE_SUPPORT_LARGE_FILES       FALSE)
#set(HAVE_LARGE_FILE_SUPPORT_LARGE_FILE_SOURCE FALSE)
