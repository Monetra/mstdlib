# Toolchain for cross-compiling to 32-bit Windows from Cygwin using mingw64.
set(CMAKE_TOOLCHAIN_PREFIX i686-w64-mingw32)
include("${CMAKE_CURRENT_LIST_DIR}/cygwin-mingw-common.cmake")
