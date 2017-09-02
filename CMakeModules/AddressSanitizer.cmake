# Check to see if the chosen compiler supports ASAN. If so, provide an option (M_ASAN) to enable it.
#

include(CheckCCompilerFlag)
#include(CheckCXXCompilerFlag) #Uncommenting C++ stuff enables ASAN for Qt code, might not work right.

set(asan_flags "-fsanitize=address -fno-omit-frame-pointer")

function(check_asan_flags)
	set(CMAKE_REQUIRED_LIBRARIES "${asan_flags}")
	check_c_compiler_flag("${asan_flags}" HAVE_ASAN_FLAGS)
	#check_cxx_compiler_flag("${asan_flags}" HAVE_ASAN_FLAGS_CXX)
endfunction()

check_asan_flags()
if (HAVE_ASAN_FLAGS)
	option(M_ASAN "Enable address sanitizer" FALSE)
elseif (DEFINED M_ASAN)
	# This prevents the M_ASAN option from staying visible in the GUI after the user tried to enable
	# ASAN on a platform that doesn't support it.
	unset(M_ASAN CACHE)
	unset(M_ASAN)
endif ()

if (M_ASAN AND NOT _internal_address_sanitizer_flags_added)
	set(_internal_address_sanitizer_flags_added TRUE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${asan_flags}")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${asan_flags}")
	link_libraries(${asan_flags}) # Add as linker flags to every library and executable.
endif ()
