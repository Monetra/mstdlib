# - Check for large file support.
# Once done this will set the following if found:
#  _LARGE_FILES
#  _LARGE_FILE_SOURCE
#  _FILE_OFFSET_BITS=X

include(CheckCSourceRuns)

set(_found FALSE)

set(large_file_src "
	#include <sys/types.h>
	#define LARGE_OFF_T (((off_t) 1 << 62) - 1 + ((off_t) 1 << 62))
	int main(int argc, char **argv) {
		int off_t_is_large[(LARGE_OFF_T % 2147483629 == 721 && LARGE_OFF_T % 2147483647 == 1) ? 1 : -1];
		return 0;
	}
	"
)

set(large_file_extra_src "
	#include <sys/types.h>
	int main(int argc, char **argv) {
		return sizeof(off_t) == 8 ? 0 : 1;
	}
	"
)

# Check if off_t is 64 bit and we don't need to set anything.
check_c_source_runs("${large_file_src}" HAVE_LARGE_FILE_SUPPORT_NATIVE)
if (HAVE_LARGE_FILE_SUPPORT_NATIVE)
	set(OFF_TYPE "native")
	set(_found    TRUE)
endif ()

# Check if we need to set _FILE_OFFSET_BITS=64.
if (NOT _found)
	check_c_source_runs("#define _FILE_OFFSET_BITS 64\n${large_file_extra_src}" HAVE_LARGE_FILE_SUPPORT_FILE_OFFSET_BITS)
	if (HAVE_LARGE_FILE_SUPPORT_FILE_OFFSET_BITS)
		set(OFF_TYPE          "_FILE_OFFSET_BITS=64")
		set(_FILE_OFFSET_BITS 64)
		set(_found            TRUE)
	endif ()
endif ()

# Check if we need to set _LARGE_FILES
if (NOT _found)
	check_c_source_runs("#define _LARGE_FILES\n${large_file_extra_src}" HAVE_LARGE_FILE_SUPPORT_LARGE_FILES)
	if (HAVE_LARGE_FILE_SUPPORT_LARGE_FILES)
		set(OFF_TYPE     "_LARGE_FILES")
		set(_LARGE_FILES 1)
		set(_found       TRUE)
	endif ()
endif ()

# Check if we need to set _LARGE_FILE_SOURCE
if (NOT _found)
	check_c_source_runs("#define _LARGE_FILE_SOURCE\n${large_file_extra_src}" HAVE_LARGE_FILE_SUPPORT_LARGE_FILE_SOURCE)
	if (HAVE_LARGE_FILE_SUPPORT_LARGE_FILE_SOURCE)
		set(OFF_TYPE           "_LARGE_FILE_SOURCE")
		set(_LARGE_FILE_SOURCE 1)
		set(_found             TRUE)
	endif ()
endif ()

# Let the caller know what happened.
if (_found)
	message(STATUS "64-bit off_t - present with ${OFF_TYPE}")
elseif ()
	message(STATUS "64-bit off_t - not present")
endif ()
